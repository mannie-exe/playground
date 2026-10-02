#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_timer.h>

#include <rendering/GPUTiming.hpp>
#include <support/PerformanceReport.hpp>
#include <ui/WorkDiagnostics.hpp>

struct PerformanceConfig {
  bool enabled{false};
  double reportEverySeconds{1};
  std::uint32_t historySize{240};
  bool logSummary{true};
};

// CPU durations only. A missing phase is not a zero-duration measurement.
using PerformanceSample = playground::rendering::CPUSample;

struct PerformanceHistoryEntry {
  std::uint64_t frame{};
  PerformanceSample cpu;
  GPUTimingStatus gpuTiming{GPUTimingStatus::Unsupported};
  bool gpuSampleReceived{};
  std::vector<playground::ui::UIWorkSample> ui;
};

class PerformanceMonitor {
  PerformanceConfig _config;
  double _reportAt{};
  std::string _workload{"host"};
  std::array<DurationStats, 5> _stats;
  std::array<std::uint64_t, 5> _starts{};
  std::array<bool, 5> _active{};
  PerformanceSample _sample;
  std::vector<playground::ui::UIWorkSample> _uiWork;
  playground::ui::UIWorkStats _uiSummary;
  DurationStats _idleWait;
  playground::rendering::PaintWork _paint;
  std::array<double,
             static_cast<std::size_t>(playground::ui::UIWorkPhase::Count)>
      _uiMilliseconds{};
  std::deque<PerformanceHistoryEntry> _history;
  std::deque<playground::rendering::GPUTimingSample> _gpuHistory;
  std::vector<GPUGroupReport> _gpuGroups;
  std::optional<playground::rendering::GPUTimingCollection> _gpuCollection;
  std::uint64_t _gpuSamplesReceived{};
  std::uint64_t _omittedGPUSamples{};
  std::uint64_t _queryDrops{};
  std::uint64_t _bufferDiscards{};
  bool _gpuMeasured{};
  bool _gpuTimingAvailable{};
  bool _gpuSampleReceived{};
  std::uint64_t _frameCount{};
  std::uint64_t _framesSinceReport{};
  std::uint64_t _statisticsRevision{};
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
  static constexpr std::size_t maximumGPUGroups{64};

  explicit PerformanceMonitor(PerformanceConfig config = {}) {
    setConfig(config);
  }

  const PerformanceConfig &config() const { return _config; }

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
    _gpuMeasured = false;
    _gpuCollection.reset();
    _gpuSampleReceived = false;
  }

  GPUTimingStatus gpuTimingStatus() const noexcept {
    if (!_gpuTimingAvailable)
      return GPUTimingStatus::Unsupported;
    if (!_config.enabled || (_gpuCollection && !_gpuCollection->enabled))
      return GPUTimingStatus::Disabled;
    return _gpuMeasured ? GPUTimingStatus::Measured : GPUTimingStatus::Pending;
  }

  void setConfig(PerformanceConfig config);

  bool isEnabled() const { return _config.enabled; }

  // Reporting revision only. Runtime/native collection is independent.
  std::uint64_t statisticsRevision() const noexcept {
    return _statisticsRevision;
  }

  void setEnabled(bool enabled);

  void toggleEnabled() { setEnabled(!isEnabled()); }

  void beginFrame() {
    _uiWork.clear();
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

  void recordIdleWait(double milliseconds) {
    if (isEnabled())
      _idleWait.add(milliseconds);
  }

  void recordPaintWork(const playground::rendering::PaintWork &work) {
    if (isEnabled())
      _paint.add(work);
  }

  // Per-root deltas; root timings overlap CPU phases and are not added to
  // Total.
  void recordUI(const playground::ui::UIWorkSample &sample);
  // Only completed native timestamp queries; unavailable data stays absent.
  void recordGPU(const playground::rendering::GPUTimingSample &sample);
  // Record the backend snapshot before forwarding its completed sample batch.
  void recordGPUCollection(const playground::rendering::GPUTimingCollection &);

  PerformanceReport snapshotReport() const;
  void resetReportInterval() noexcept;
  void report();
  void reportIfDue(double monotonicSeconds);
  void setWorkload(std::string name);

  void resetStatistics();

  bool handleHotkey(const SDL_KeyboardEvent &key);
};
