#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <ratio>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_gpu_timestamps_playground.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_properties.h>

#include <platform/sdl/GPUDevice.hpp>
#include <platform/sdl/GPUTimestamps.hpp>
#include <platform/sdl/RenderError.hpp>
#include <rendering/GPUTiming.hpp>
#include <rendering/RenderFailure.hpp>
#include <rendering/Submission.hpp>
#include <support/SDLError.hpp>

namespace playground::sdl {
GPUDevice::GPUDevice(GPUDeviceProps props, GPUCommandAPI commands)
    : _limits{props.limits}, _commands{commands},
      _resources{std::move(props.resources)} {
  _limits.validate();
  if (!_resources)
    throw std::invalid_argument("GPU device requires a resource ledger");
  if (!_commands.submit)
    throw std::invalid_argument("GPU command API requires a submit function");
  if (!props.shaderFormats)
    throw std::invalid_argument("GPU device requires supported shader formats");
  if (props.driver && std::string_view{props.driver} != "vulkan")
    throw std::invalid_argument("Only the Vulkan GPU driver is supported");
  _device.reset(
      SDL_CreateGPUDevice(props.shaderFormats, props.debug, "vulkan"));
  if (!_device)
    throwSDLError("Failed to create GPU device");
  _timestampSupported =
      SDL_GetPointerProperty(SDL_GetGPUDeviceProperties(get()),
                             SDL_PROP_GPU_DEVICE_PLAYGROUND_TIMESTAMPS_POINTER,
                             nullptr) != nullptr;
}

void GPUDevice::checkOwnerThread() const {
  if (std::this_thread::get_id() != _owner)
    throw std::logic_error("GPU work must run on its renderer owner thread");
  if (!_valid)
    throw rendering::RenderFailure("GPU resource domain is no longer active");
}

GPUDevice::~GPUDevice() {
  invalidate();
  // SDL device teardown completes its own native cleanup before quarantined
  // charges/frame credits are released. Invalidation alone is not completion.
  _device.reset();
  _recordings.clear();
  _pending.clear();
}

void GPUDevice::invalidate() noexcept {
  _valid = false;
  _frame.reset();
  for (auto it = _recordings.begin(); it != _recordings.end();) {
    auto &recording = it->second;
    if (recording.consumed) {
      ++it;
      continue;
    }
    const bool canceled = SDL_CancelGPUCommandBuffer(it->first);
    if (recording.timestamp && _timestamps) {
      if (canceled)
        _timestamps->cancel(*recording.timestamp);
      else
        _timestamps->abandon(*recording.timestamp);
    }
    recording.timestamp.reset();
    if (canceled)
      it = _recordings.erase(it);
    else {
      recording.consumed = true;
      ++it;
    }
  }
  for (auto &pending : _pending)
    if (pending.fence) {
      SDL_ReleaseGPUFence(get(), pending.fence);
      pending.fence = nullptr;
    }
  _timings.clear();
  _completionLatencies.clear();
  _timestamps.reset();
}

bool GPUDevice::retireInvalidatedSubmissions() noexcept {
  if (_valid || _pending.empty())
    return !_valid;
  // Only backend replacement/recovery calls this blocking retirement path.
  // Native success establishes completion; failure keeps uncertain charges.
  if (!SDL_WaitForGPUIdle(get()))
    return false;
  _completed = _submitted;
  _pending.clear();
  return true;
}

SDL_GPUCommandBuffer *
GPUDevice::acquireCommands(std::string_view label,
                           rendering::GPUWorkContext context) {
  checkOwnerThread();
  rendering::validateGPUTimingLabel(label);
  rendering::validateGPUWorkContext(context);
  pollCompletions();
  if (_recordings.size() + _pending.size() >= _limits.maxInFlightSubmissions)
    throw rendering::ResourcePressure("GPU command batches", 1,
                                      _recordings.size() + _pending.size(),
                                      _limits.maxInFlightSubmissions,
                                      rendering::ResourcePressure::Unit::Slots);
  auto *commands = SDL_AcquireGPUCommandBuffer(get());
  if (!commands)
    throwRenderError("Cannot acquire GPU commands",
                     rendering::RenderOperation::Acquire);
  try {
    auto &recording = _recordings.emplace(commands, Recording{}).first->second;
    recording.frame = _frame;
    context.frameId = _frame ? _frame->id : 0;
    const bool coarse = label == "paint2d" || label == "scene3d" ||
                        label == "scene post-processing" ||
                        label == "custom offscreen" ||
                        label == "presentation composition";
    if (_profiling && _timestamps && (_detailedTiming || coarse))
      recording.timestamp =
          _timestamps->begin(commands, label, context, _collectionGeneration);
  } catch (...) {
    _recordings.erase(commands);
    if (!SDL_CancelGPUCommandBuffer(commands))
      _valid = false;
    throw;
  }
  return commands;
}

void GPUDevice::setTimingContext(SDL_GPUCommandBuffer *commands,
                                 rendering::GPUWorkContext context) {
  checkOwnerThread();
  rendering::validateGPUWorkContext(context);
  const auto &recording = _recordings.at(commands);
  context.frameId = recording.frame ? recording.frame->id : 0;
  if (recording.timestamp)
    _timestamps->setContext(*recording.timestamp, context);
}

void GPUDevice::recordUse(SDL_GPUCommandBuffer *commands,
                          rendering::ResourceLease use) {
  checkOwnerThread();
  if (!use || use->domain != _domain)
    throw std::invalid_argument(
        "GPU recording resource belongs to another domain");
  auto &uses = _recordings.at(commands).uses;
  if (std::ranges::find(uses, use) == uses.end())
    uses.push_back(std::move(use));
}

rendering::SubmissionId GPUDevice::submit(SDL_GPUCommandBuffer *commands) {
  checkOwnerThread();
  auto recording = _recordings.find(commands);
  if (recording == _recordings.end())
    throw std::invalid_argument(
        "GPU command buffer is not a tracked recording");
  if (_submitted == std::numeric_limits<rendering::SubmissionId>::max())
    throw std::overflow_error("GPU submission sequence exhausted");
  auto timestamp = recording->second.timestamp;
  if (timestamp)
    _timestamps->end(commands, *timestamp);
  // Allocate bookkeeping before consuming commands; allocation failure leaves
  // the recording cancelable by its owner.
  _pending.emplace_back();
  auto &pending = _pending.back();
  pending.frame = std::move(recording->second.frame);
  pending.uses = std::move(recording->second.uses);
  _recordings.erase(recording);
  if (timestamp)
    pending.submittedAt = std::chrono::steady_clock::now();
  pending.fence = _commands.submit(commands);
  if (!pending.fence) {
    if (timestamp)
      _timestamps->abandon(*timestamp);
    // SDL has consumed the command buffer, but completion is unknown. Keep its
    // leases and forbid any subsequent recording/reuse in this domain.
    _valid = false;
    throwRenderError("GPU command submission failed",
                     rendering::RenderOperation::Submit);
  }
  pending.id = ++_submitted;
  if (timestamp) {
    _timestamps->submitted(*timestamp, pending.id);
  }
  for (auto &use : pending.uses)
    use->lastSubmission = pending.id;
  return pending.id;
}

void GPUDevice::registerTexture(SDL_GPUTexture *texture,
                                const rendering::ResourceLease &use) {
  checkOwnerThread();
  _textures.insert_or_assign(texture, use);
}

void GPUDevice::recordTexture(SDL_GPUCommandBuffer *commands,
                              SDL_GPUTexture *texture) {
  checkOwnerThread();
  if (auto it = _textures.find(texture); it != _textures.end())
    if (auto use = it->second.lock())
      recordUse(commands, std::move(use));
}

void GPUDevice::cancel(SDL_GPUCommandBuffer *commands) noexcept {
  if (auto it = _recordings.find(commands); it != _recordings.end()) {
    const bool canceled = SDL_CancelGPUCommandBuffer(commands);
    if (it->second.timestamp) {
      if (canceled)
        _timestamps->cancel(*it->second.timestamp);
      else
        _timestamps->abandon(*it->second.timestamp);
    }
    if (!canceled) {
      _valid = false;
      return;
    }
    _recordings.erase(it);
  }
}

void GPUDevice::finishAbandonedPresentation(
    SDL_GPUCommandBuffer *commands) noexcept {
  if (!_recordings.contains(commands))
    return;
  try {
    submit(commands);
  } catch (...) {
    // Acquired swapchain commands cannot be canceled. On bookkeeping failure
    // submit for SDL cleanup, then retire the domain rather than reuse
    // resources whose completion could not be tracked.
    if (auto it = _recordings.find(commands); it != _recordings.end()) {
      it->second.consumed = true;
      SDL_SubmitGPUCommandBuffer(commands);
    }
    _valid = false;
  }
}

void GPUDevice::pollCompletions() {
  checkOwnerThread();
  while (!_pending.empty() &&
         SDL_QueryGPUFence(get(), _pending.front().fence)) {
    if (_completionLatencies.size() >= _limits.maxTimestampScopes)
      _completionLatencies.erase(_completionLatencies.begin());
    if (_pending.front().submittedAt)
      _completionLatencies.insert_or_assign(
          _pending.front().id,
          std::chrono::duration<double, std::milli>(
              std::chrono::steady_clock::now() - *_pending.front().submittedAt)
              .count());
    _completed = _pending.front().id;
    SDL_ReleaseGPUFence(get(), _pending.front().fence);
    _pending.pop_front();
  }
  std::erase_if(_textures,
                [](const auto &entry) { return entry.second.expired(); });
  if (_timestamps) {
    auto samples = _timestamps->poll(_completed);
    for (auto &sample : samples) {
      sample.domain = _domain;
      if (auto it = _completionLatencies.find(sample.sequence);
          it != _completionLatencies.end()) {
        sample.completionLatencyMilliseconds = it->second;
        _completionLatencies.erase(it);
      }
      if (!_profiling || sample.collectionGeneration != _collectionGeneration)
        continue;
      if (_timings.size() >= _limits.maxTimestampScopes) {
        _timings.erase(_timings.begin());
        if (_bufferDiscards != std::numeric_limits<std::uint64_t>::max())
          ++_bufferDiscards;
      }
      _timings.push_back(std::move(sample));
    }
  }
}

void GPUDevice::setProfilingEnabled(bool enabled, bool detailed) {
  checkOwnerThread();
  _detailedTiming = detailed;
  if (enabled == _profiling)
    return;
  if (enabled &&
      _collectionGeneration == std::numeric_limits<std::uint64_t>::max())
    throw std::overflow_error("GPU profiling generation exhausted");
  if (enabled && _timestampSupported && !_timestamps) {
    try {
      _timestamps = std::make_unique<GPUTimestampRing>(
          get(), GPUTimestampProps{_limits.maxTimestampScopes});
    } catch (const std::runtime_error &error) {
      _timestampSupported = false;
      SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "GPU timing unavailable: %s",
                  error.what());
    }
  }
  _timings.clear();
  if (enabled) {
    ++_collectionGeneration;
    _queryDropBaseline = _timestamps ? _timestamps->dropped() : 0;
    _bufferDiscards = 0;
  }
  _profiling = enabled;
}

bool GPUDevice::supportsTimestamps() const noexcept {
  return _timestampSupported;
}

std::vector<rendering::GPUTimingSample> GPUDevice::takeGPUTimings() {
  pollCompletions();
  return std::exchange(_timings, {});
}

rendering::GPUTimingCollection GPUDevice::gpuTimingCollection() const {
  checkOwnerThread();
  return {_domain,
          _collectionGeneration,
          _timestampSupported,
          _profiling,
          _timestamps ? _timestamps->pending(_collectionGeneration) : 0,
          _timestamps ? _timestamps->dropped() - _queryDropBaseline : 0,
          _bufferDiscards};
}

} // namespace playground::sdl
