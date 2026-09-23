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

  static const char *name(FramePhase phase);

public:
  explicit PerformanceMonitor(PerformanceConfig config = {}) {
    setConfig(config);
  }

  const PerformanceConfig &getConfig() const { return _config; }

  void setConfig(PerformanceConfig config);

  bool isEnabled() const { return _config.enabled; }

  void setEnabled(bool enabled);

  void toggleEnabled() { setEnabled(!isEnabled()); }

  void beginFrame() {
    if (_config.enabled)
      _starts[index(FramePhase::Total)] = SDL_GetPerformanceCounter();
  }

  void begin(FramePhase phase) {
    if (_config.enabled)
      _starts[index(phase)] = SDL_GetPerformanceCounter();
  }

  void end(FramePhase phase);

  void endFrame();

  void report();

  void resetStatistics() {
    _stats = {};
    _frameCount = 0;
  }

  bool handleHotkey(const SDL_KeyboardEvent &key);
};
