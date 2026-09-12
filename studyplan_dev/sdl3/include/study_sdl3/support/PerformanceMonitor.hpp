#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_timer.h>

struct PerformanceConfig {
  bool enabled{false};
  std::uint32_t sampleEveryFrames{60};
  std::uint32_t historySize{240};
  bool logSummary{true};
};

enum class FramePhase : std::uint8_t {
  Poll,
  Update,
  Render,
  Present,
  Total,
};

class PerformanceMonitor {
  struct PhaseStats {
    std::uint64_t count{};
    double totalMilliseconds{};
    double minimumMilliseconds{0.0};
    double maximumMilliseconds{};

    void add(double milliseconds) {
      if (count == 0)
        minimumMilliseconds = milliseconds;
      else if (milliseconds < minimumMilliseconds)
        minimumMilliseconds = milliseconds;

      if (milliseconds > maximumMilliseconds)
        maximumMilliseconds = milliseconds;
      totalMilliseconds += milliseconds;
      ++count;
    }

    double average() const {
      return count == 0 ? 0.0 : totalMilliseconds / count;
    }
  };

  PerformanceConfig _config;
  std::array<PhaseStats, 5> _stats;
  std::array<std::uint64_t, 5> _starts{};
  std::uint64_t _frameCount{};
  std::uint64_t _frequency{SDL_GetPerformanceFrequency()};

  static std::size_t index(FramePhase phase) {
    return static_cast<std::size_t>(phase);
  }

  double milliseconds(std::uint64_t ticks) const {
    return 1000.0 * static_cast<double>(ticks) /
           static_cast<double>(_frequency);
  }

  static const char *name(FramePhase phase) {
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

public:
  explicit PerformanceMonitor(PerformanceConfig config = {}) {
    setConfig(config);
  }

  const PerformanceConfig &getConfig() const { return _config; }

  void setConfig(PerformanceConfig config) {
    config.sampleEveryFrames =
        config.sampleEveryFrames == 0 ? 1 : config.sampleEveryFrames;
    _config = config;
  }

  bool isEnabled() const { return _config.enabled; }

  void setEnabled(bool enabled) {
    if (_config.enabled == enabled)
      return;
    _config.enabled = enabled;
    resetStatistics();
  }

  void toggleEnabled() { setEnabled(!isEnabled()); }

  void beginFrame() {
    if (_config.enabled)
      _starts[index(FramePhase::Total)] = SDL_GetPerformanceCounter();
  }

  void begin(FramePhase phase) {
    if (_config.enabled)
      _starts[index(phase)] = SDL_GetPerformanceCounter();
  }

  void end(FramePhase phase) {
    if (!_config.enabled)
      return;

    const std::uint64_t start{_starts[index(phase)]};
    _stats[index(phase)].add(milliseconds(SDL_GetPerformanceCounter() - start));
  }

  void endFrame() {
    if (!_config.enabled)
      return;

    end(FramePhase::Total);
    ++_frameCount;
    if (_config.logSummary && _frameCount % _config.sampleEveryFrames == 0)
      report();
  }

  void report() {
    if (!_config.enabled)
      return;

    std::string message{"Performance"};
    for (const FramePhase phase :
         {FramePhase::Poll, FramePhase::Update, FramePhase::Render,
          FramePhase::Present, FramePhase::Total}) {
      const PhaseStats &stats{_stats[index(phase)]};
      message +=
          std::format(" | {} avg={:.3f}ms min={:.3f}ms max={:.3f}ms",
                      name(phase), stats.average(), stats.minimumMilliseconds,
                      stats.maximumMilliseconds);
    }
    SDL_Log("%s", message.c_str());
    resetStatistics();
  }

  void resetStatistics() {
    _stats = {};
    _frameCount = 0;
  }

  bool handleHotkey(const SDL_KeyboardEvent &key) {
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
      SDL_Log("Performance monitoring %s",
              isEnabled() ? "enabled" : "disabled");
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
};
