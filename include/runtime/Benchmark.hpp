#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string_view>
#include <vector>

#include <rendering/RenderRuntime.hpp>

namespace playground::runtime {
enum class BenchmarkPhase {
  Loading,
  Warmup,
  Measuring,
  Draining,
  Complete,
  Invalid
};

inline std::string_view toString(BenchmarkPhase phase) {
  switch (phase) {
  case BenchmarkPhase::Loading:
    return "loading";
  case BenchmarkPhase::Warmup:
    return "warmup";
  case BenchmarkPhase::Measuring:
    return "measuring";
  case BenchmarkPhase::Draining:
    return "draining";
  case BenchmarkPhase::Complete:
    return "complete";
  case BenchmarkPhase::Invalid:
    return "invalid";
  }
  return "invalid";
}

struct BenchmarkDistribution {
  static constexpr std::size_t capacity = 65536;
  std::uint64_t count{};
  double total{}, maximum{};
  std::vector<double> recent;

  void add(double value) {
    if (!std::isfinite(value) || value < 0)
      throw std::invalid_argument("Invalid benchmark sample");
    if (recent.size() < capacity)
      recent.push_back(value);
    else
      recent[count % capacity] = value;
    ++count;
    total += value;
    maximum = std::max(maximum, value);
  }

  double percentile(double fraction) const {
    if (!std::isfinite(fraction) || fraction < 0 || fraction > 1)
      throw std::invalid_argument("Invalid percentile");
    if (recent.empty())
      return 0;
    auto sorted = recent;
    std::sort(sorted.begin(), sorted.end());
    return sorted[std::size_t(std::ceil(fraction * (sorted.size() - 1)))];
  }
};

// Explicit monotonic seconds make lifecycle and sample admission deterministic.
class BenchmarkRun {
  double _duration, _warmup, _phaseAt{}, _last{}, _measured{};
  BenchmarkPhase _phase{BenchmarkPhase::Loading};

public:
  std::array<BenchmarkDistribution, 5> cpu;
  BenchmarkDistribution gpuScene;
  std::uint64_t firstFrame{}, lastFrame{}, omittedCPU{}, omittedGPU{};
  bool drainTimedOut{};

  explicit BenchmarkRun(double duration = 15, double warmup = 15)
      : _duration{duration}, _warmup{warmup} {
    if (!std::isfinite(duration) || duration < 0 || !std::isfinite(warmup) ||
        warmup < 0)
      throw std::invalid_argument("Invalid benchmark duration");
  }

  BenchmarkPhase phase() const { return _phase; }

  double duration() const { return _duration; }

  double measuredSeconds() const { return _measured; }

  double elapsed(double now) const { return std::max(0., now - _phaseAt); }

  bool traversing() const {
    return _phase == BenchmarkPhase::Warmup ||
           _phase == BenchmarkPhase::Measuring;
  }

  bool finished() const {
    return _phase == BenchmarkPhase::Complete ||
           _phase == BenchmarkPhase::Invalid;
  }

  void ready(double now) {
    if (_phase != BenchmarkPhase::Loading || !std::isfinite(now))
      throw std::logic_error("Invalid benchmark readiness");
    _phaseAt = _last = now;
    _phase = BenchmarkPhase::Warmup;
  }

  void advance(double now, std::uint64_t admitted, bool pending) {
    if (!std::isfinite(now) ||
        (_phase != BenchmarkPhase::Loading && now < _last))
      throw std::invalid_argument("Benchmark clock went backwards");
    _last = now;
    if (_phase == BenchmarkPhase::Warmup && now - _phaseAt >= _warmup) {
      _phaseAt = now;
      firstFrame = admitted + 1;
      _phase = BenchmarkPhase::Measuring;
    } else if (_phase == BenchmarkPhase::Measuring) {
      _measured = now - _phaseAt;
      if (_duration && _measured >= _duration) {
        lastFrame = admitted;
        _phase = BenchmarkPhase::Draining;
        _phaseAt = now;
      }
    } else if (_phase == BenchmarkPhase::Draining &&
               (!pending || now - _phaseAt >= 2)) {
      drainTimedOut = pending;
      _phase = BenchmarkPhase::Complete;
    }
  }

  void record(const rendering::CPUSample &sample) {
    if (_phase != BenchmarkPhase::Measuring)
      return;
    for (std::size_t i = 0; i < cpu.size(); ++i)
      if (sample.measured[i])
        cpu[i].add(sample.milliseconds[i]);
  }

  void record(const rendering::GPUTimingSample &sample) {
    if (finished() || !firstFrame || sample.context.frameId < firstFrame ||
        (lastFrame && sample.context.frameId > lastFrame))
      return;
    if (sample.label == "scene3d")
      gpuScene.add(sample.milliseconds);
  }

  void invalidate() {
    if (!finished())
      _phase = BenchmarkPhase::Invalid;
  }
};
} // namespace playground::runtime
