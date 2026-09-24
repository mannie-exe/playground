#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <stdexcept>
#include <string>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_timer.h>

#include <rendering/GPUTiming.hpp>

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

// CPU durations only. A missing phase is not a zero-duration measurement.
struct PerformanceSample {
  std::array<double, 5> milliseconds{};
  std::array<bool, 5> measured{};
};

enum class GPUTimingStatus : std::uint8_t {
  Unsupported,
  Disabled,
  Pending,
  Measured,
};

struct PerformanceHistoryEntry {
  std::uint64_t frame{};
  PerformanceSample cpu;
  GPUTimingStatus gpuTiming{GPUTimingStatus::Unsupported};
  bool gpuSampleReceived{};
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
  std::array<bool, 5> _active{};
  PerformanceSample _sample;
  std::deque<PerformanceHistoryEntry> _history;
  std::deque<playground::rendering::GPUTimingSample> _gpuHistory;
  std::optional<playground::rendering::GPUTimingSample> _latestGPU;
  bool _gpuTimingAvailable{};
  bool _gpuSampleReceived{};
  std::uint64_t _frameCount{};
  std::uint64_t _framesSinceReport{};
  std::uint64_t _frequency{SDL_GetPerformanceFrequency()};

  static std::size_t index(FramePhase phase) {
    const auto result = static_cast<std::size_t>(phase);
    if (result >= 5)
      throw std::invalid_argument("Unknown performance phase");
    return result;
  }

  double milliseconds(std::uint64_t ticks) const {
    return 1000.0 * static_cast<double>(ticks) /
           static_cast<double>(_frequency);
  }

  static const char *name(FramePhase phase);

public:
  static constexpr std::uint32_t maximumHistorySize{65536};

  explicit PerformanceMonitor(PerformanceConfig config = {}) {
    setConfig(config);
  }

  const PerformanceConfig &getConfig() const { return _config; }
  const std::deque<PerformanceHistoryEntry> &history() const noexcept {
    return _history;
  }
  const std::deque<playground::rendering::GPUTimingSample> &
  gpuHistory() const noexcept {
    return _gpuHistory;
  }
  void setGPUTimingAvailable(bool available) noexcept {
    if (_gpuTimingAvailable == available)
      return;
    _gpuTimingAvailable = available;
    _latestGPU.reset();
    _gpuSampleReceived = false;
  }
  GPUTimingStatus gpuTimingStatus() const noexcept {
    if (!_gpuTimingAvailable)
      return GPUTimingStatus::Unsupported;
    if (!_config.enabled)
      return GPUTimingStatus::Disabled;
    return _latestGPU ? GPUTimingStatus::Measured : GPUTimingStatus::Pending;
  }

  void setConfig(PerformanceConfig config);

  bool isEnabled() const { return _config.enabled; }

  void setEnabled(bool enabled);

  void toggleEnabled() { setEnabled(!isEnabled()); }

  void beginFrame() {
    _sample = {};
    _active = {};
    begin(FramePhase::Total);
  }

  void begin(FramePhase phase) {
    if (_config.enabled) {
      _starts[index(phase)] = SDL_GetPerformanceCounter();
      _active[index(phase)] = true;
    }
  }

  void end(FramePhase phase);

  void endFrame();

  // Allows deterministic samples from another CPU profiler, not GPU timings.
  // Enabled collection validates finite, nonnegative measured durations.
  void recordFrame(const PerformanceSample &sample);
  // Only completed native timestamp queries; unavailable data stays absent.
  void recordGPU(const playground::rendering::GPUTimingSample &sample);

  void report();

  void resetStatistics() {
    _stats = {};
    _active = {};
    _sample = {};
    _history.clear();
    _gpuHistory.clear();
    _latestGPU.reset();
    _gpuSampleReceived = false;
    _frameCount = 0;
    _framesSinceReport = 0;
  }

  bool handleHotkey(const SDL_KeyboardEvent &key);
};
