#include <cmath>
#include <format>
#include <stdexcept>
#include <utility>

#include <SDL3/SDL_log.h>

#include <support/PerformanceMonitor.hpp>

const char *PerformanceMonitor::name(FramePhase phase) {
  switch (phase) {
  case FramePhase::Poll:
    return "poll";
  case FramePhase::Update:
    return "update";
  case FramePhase::Render:
    return "render";
  case FramePhase::Present:
    return "present";
  case FramePhase::Total:
    return "total";
  }
  return "unknown";
}

void PerformanceMonitor::setConfig(PerformanceConfig config) {
  if (config.historySize > maximumHistorySize)
    throw std::invalid_argument("Performance history exceeds entry limit");
  config.sampleEveryFrames =
      config.sampleEveryFrames == 0 ? 1 : config.sampleEveryFrames;
  if (_config.enabled != config.enabled)
    resetStatistics();
  _config = config;
  while (_history.size() > _config.historySize)
    _history.pop_front();
  while (_gpuHistory.size() > _config.historySize)
    _gpuHistory.pop_front();
}

void PerformanceMonitor::setEnabled(bool enabled) {
  if (_config.enabled == enabled)
    return;
  _config.enabled = enabled;
  resetStatistics();
}

void PerformanceMonitor::end(FramePhase phase) {
  if (!_config.enabled || !_active[index(phase)])
    return;

  const std::uint64_t start{_starts[index(phase)]};
  _active[index(phase)] = false;
  _sample.milliseconds[index(phase)] +=
      milliseconds(SDL_GetPerformanceCounter() - start);
  _sample.measured[index(phase)] = true;
}

void PerformanceMonitor::endFrame() {
  if (!_config.enabled || !_active[index(FramePhase::Total)])
    return;

  end(FramePhase::Total);
  recordFrame(_sample);
}

void PerformanceMonitor::recordFrame(const PerformanceSample &sample) {
  if (!_config.enabled)
    return;
  for (std::size_t i = 0; i < sample.measured.size(); ++i)
    if (sample.measured[i] &&
        (!std::isfinite(sample.milliseconds[i]) || sample.milliseconds[i] < 0))
      throw std::invalid_argument("Invalid CPU performance sample");
  const auto nextFrame = _frameCount + 1;
  if (_config.historySize) {
    _history.push_back(
        {nextFrame, sample, gpuTimingStatus(), _gpuSampleReceived});
    if (_history.size() > _config.historySize)
      _history.pop_front();
  }
  for (std::size_t i = 0; i < sample.measured.size(); ++i)
    if (sample.measured[i])
      _stats[i].add(sample.milliseconds[i]);
  _frameCount = nextFrame;
  _gpuSampleReceived = false;
  ++_framesSinceReport;
  if (_config.logSummary && _framesSinceReport >= _config.sampleEveryFrames)
    report();
}

void PerformanceMonitor::report() {
  if (!_config.enabled)
    return;

  std::string message{"Performance"};
  for (const FramePhase phase :
       {FramePhase::Poll, FramePhase::Update, FramePhase::Render,
        FramePhase::Present, FramePhase::Total}) {
    const PhaseStats &stats{_stats[index(phase)]};
    message += std::format(
        " | {} avg={:.3f}ms min={:.3f}ms max={:.3f}ms", name(phase),
        stats.average(), stats.minimumMilliseconds, stats.maximumMilliseconds);
  }
  if (_latestGPU) {
    message += std::format(" | GPU {} latest={:.3f}ms (completed sequence {})",
                           _latestGPU->label, _latestGPU->milliseconds,
                           _latestGPU->sequence);
    if (_latestGPU->completionLatencyMilliseconds)
      message += std::format(" completion latency={:.3f}ms",
                             *_latestGPU->completionLatencyMilliseconds);
  } else {
    const char *status = "unsupported";
    switch (gpuTimingStatus()) {
    case GPUTimingStatus::Unsupported:
      break;
    case GPUTimingStatus::Disabled:
      status = "disabled";
      break;
    case GPUTimingStatus::Pending:
      status = "enabled, pending/no new sample";
      break;
    case GPUTimingStatus::Measured:
      status = "measured";
      break;
    }
    message += std::format(" | GPU timing={}", status);
  }
  SDL_Log("%s", message.c_str());
  _stats = {};
  _latestGPU.reset();
  _framesSinceReport = 0;
}

void PerformanceMonitor::recordGPU(
    const playground::rendering::GPUTimingSample &sample) {
  if (!_config.enabled || !_gpuTimingAvailable)
    return;
  if (!sample.sequence || !sample.domain ||
      !std::isfinite(sample.milliseconds) || sample.milliseconds < 0 ||
      sample.label.empty() || sample.label.size() > 128 ||
      (sample.completionLatencyMilliseconds &&
       (!std::isfinite(*sample.completionLatencyMilliseconds) ||
        *sample.completionLatencyMilliseconds < 0)))
    throw std::invalid_argument("Invalid GPU performance sample");
  // Prepare the optional report value before publishing history, so a string
  // allocation failure cannot leave mismatched views of the newest sample.
  auto latest = sample;
  if (_config.historySize) {
    _gpuHistory.push_back(sample);
    if (_gpuHistory.size() > _config.historySize)
      _gpuHistory.pop_front();
  }
  _latestGPU = std::move(latest);
  _gpuSampleReceived = true;
}

bool PerformanceMonitor::handleHotkey(const SDL_KeyboardEvent &key) {
  if (key.type != SDL_EVENT_KEY_DOWN || key.repeat)
    return false;

  if (key.key == SDLK_F10 && (key.mod & SDL_KMOD_SHIFT)) {
    if (isEnabled())
      report();
    else
      SDL_Log("Performance monitoring is disabled");
    return true;
  }

  if (key.key == SDLK_F10) {
    toggleEnabled();
    SDL_Log("Performance monitoring %s", isEnabled() ? "enabled" : "disabled");
    return true;
  }

  if (key.key == SDLK_F11) {
    auto config = _config;
    config.sampleEveryFrames = _config.sampleEveryFrames == 60 ? 300 : 60;
    setConfig(config);
    SDL_Log("Performance summary interval: %u frames",
            _config.sampleEveryFrames);
    return true;
  }

  return false;
}
