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
#include <rendering/GPUTiming.hpp>
#include <rendering/RenderFailure.hpp>
#include <rendering/Submission.hpp>
#include <support/SDLError.hpp>

namespace playground::sdl {
GPUDevice::GPUDevice(GPUDeviceProps props) : _limits{props.limits} {
  _limits.validate();
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

GPUDevice::~GPUDevice() { invalidate(); }

void GPUDevice::invalidate() noexcept {
  _valid = false;
  for (auto &[commands, recording] : _recordings) {
    const bool canceled = SDL_CancelGPUCommandBuffer(commands);
    if (recording.timestamp) {
      if (canceled)
        _timestamps->cancel(*recording.timestamp);
      else
        _timestamps->abandon(*recording.timestamp);
    }
  }
  _recordings.clear();
  for (auto &pending : _pending)
    if (pending.fence)
      SDL_ReleaseGPUFence(get(), pending.fence);
  _pending.clear();
  _timings.clear();
  _completionLatencies.clear();
  _timestamps.reset();
}

SDL_GPUCommandBuffer *GPUDevice::acquireCommands(std::string_view label) {
  checkOwnerThread();
  pollCompletions();
  if (_recordings.size() + _pending.size() >= _limits.maxInFlightSubmissions)
    throw std::length_error("GPU submission capacity exhausted");
  auto *commands = SDL_AcquireGPUCommandBuffer(get());
  if (!commands)
    throw rendering::RenderFailure(
        std::string{"Cannot acquire GPU commands: "} + SDL_GetError());
  try {
    auto &recording = _recordings.emplace(commands, Recording{}).first->second;
    if (_profiling && _timestamps)
      recording.timestamp = _timestamps->begin(commands, label);
  } catch (...) {
    _recordings.erase(commands);
    if (!SDL_CancelGPUCommandBuffer(commands))
      _valid = false;
    throw;
  }
  return commands;
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
  pending.uses = std::move(recording->second.uses);
  _recordings.erase(recording);
  if (timestamp)
    pending.submittedAt = std::chrono::steady_clock::now();
  pending.fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
  if (!pending.fence) {
    if (timestamp)
      _timestamps->abandon(*timestamp);
    // SDL has consumed the command buffer, but completion is unknown. Keep its
    // leases and forbid any subsequent recording/reuse in this domain.
    _valid = false;
    throw rendering::RenderFailure(
        std::string{"GPU command submission failed: "} + SDL_GetError());
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
    if (_recordings.erase(commands))
      SDL_SubmitGPUCommandBuffer(commands);
    _valid = false;
  }
}

void GPUDevice::pollCompletions() {
  checkOwnerThread();
  while (!_pending.empty() &&
         SDL_QueryGPUFence(get(), _pending.front().fence)) {
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
      if (_timings.size() >= _limits.maxTimestampScopes)
        _timings.erase(_timings.begin());
      _timings.push_back(std::move(sample));
    }
  }
}

void GPUDevice::setProfilingEnabled(bool enabled) {
  checkOwnerThread();
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
  _profiling = enabled;
}

bool GPUDevice::supportsTimestamps() const noexcept {
  return _timestampSupported;
}

std::vector<rendering::GPUTimingSample> GPUDevice::takeGPUTimings() {
  pollCompletions();
  return std::exchange(_timings, {});
}

} // namespace playground::sdl
