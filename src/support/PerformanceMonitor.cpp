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
  config.sampleEveryFrames =
      config.sampleEveryFrames == 0 ? 1 : config.sampleEveryFrames;
  _config = config;
}

void PerformanceMonitor::setEnabled(bool enabled) {
  if (_config.enabled == enabled)
    return;
  _config.enabled = enabled;
  resetStatistics();
}

void PerformanceMonitor::end(FramePhase phase) {
  if (!_config.enabled)
    return;

  const std::uint64_t start{_starts[index(phase)]};
  _stats[index(phase)].add(milliseconds(SDL_GetPerformanceCounter() - start));
}

void PerformanceMonitor::endFrame() {
  if (!_config.enabled)
    return;

  end(FramePhase::Total);
  ++_frameCount;
  if (_config.logSummary && _frameCount % _config.sampleEveryFrames == 0)
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
  SDL_Log("%s", message.c_str());
  resetStatistics();
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
    setConfig({.sampleEveryFrames = static_cast<std::uint32_t>(
                   _config.sampleEveryFrames == 60 ? 300 : 60)});
    SDL_Log("Performance summary interval: %u frames",
            _config.sampleEveryFrames);
    return true;
  }

  return false;
}
