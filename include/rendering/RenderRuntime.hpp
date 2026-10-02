#pragma once
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <deque>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include <rendering/GPUTiming.hpp>
#include <rendering/ResourceLedger.hpp>
#include <rendering/SceneWork.hpp>

namespace playground::rendering {

using FrameId = std::uint64_t;
using RenderClock = std::chrono::steady_clock;
enum class CPUPhase : std::uint8_t { Poll, Update, Render, Present, Total };

struct CPUSample {
  std::array<double, 5> milliseconds{};
  std::array<bool, 5> measured{};
};

struct FrameWork {
  FrameId id;
  ResourceDomainId domain;
};

using FrameLease = std::shared_ptr<const FrameWork>;

struct FramePacingProps {
  std::optional<double> maximumFramesPerSecond;
  unsigned maxOutstandingFrames{2};

  void validate() const {
    if (maxOutstandingFrames < 1 || maxOutstandingFrames > 3)
      throw std::invalid_argument("Outstanding frames must be 1..3");
    if (maximumFramesPerSecond &&
        (!std::isfinite(*maximumFramesPerSecond) ||
         *maximumFramesPerSecond < 1 || *maximumFramesPerSecond > 1000))
      throw std::invalid_argument("Frame-rate cap must be 1..1000 Hz");
  }

  bool operator==(const FramePacingProps &) const = default;
};

struct RenderRuntimePatch {
  std::optional<ResourceBudgetProps> budgets;
  std::optional<FramePacingProps> pacing;
};

enum class FrameAdmissionStatus { Ready, Paced, Busy, Blocked };

struct FrameAdmission {
  FrameAdmissionStatus status;
  std::optional<RenderClock::time_point> wakeAt;
};

struct RenderTelemetrySnapshot {
  std::vector<CPUSample> cpu;
  std::vector<GPUTimingSample> gpu;
};

struct RenderRuntimeSnapshot {
  SceneWork sceneWork;
  ResourceSnapshot resources;
  FramePacingProps pacing;
  std::size_t outstandingFrames{};
  std::uint64_t admitted{}, submitted{}, skipped{}, abandoned{};
  FrameAdmissionStatus admission{FrameAdmissionStatus::Ready};
  std::string pressure;
  std::optional<CPUSample> cpu;
  GPUTimingCollection gpu;
  std::optional<GPUTimingSample> latestGPU;
  std::uint64_t gpuSamples{}, cpuSamples{};
  double idleMilliseconds{};
};

// Owner-thread coordinator. Only ledger reservations may run on workers.
// Native recordings retain FrameLease without retaining this coordinator.
class RenderRuntime {
  std::shared_ptr<ResourceLedger> _resources;
  FramePacingProps _pacing;
  ResourceDomainId _domain;
  std::vector<std::weak_ptr<const FrameWork>> _frames;
  RenderClock::time_point _nextFrame{};
  std::optional<RenderClock::time_point> _lastFrame;
  RenderRuntimeSnapshot _metrics;
  std::uint64_t _blockedUsage{}, _blockedPolicy{}, _blockedDemand{};
  bool _pressureBlocked{};
  std::array<RenderClock::time_point, 5> _starts;
  std::array<bool, 5> _active{};
  CPUSample _sample;
  std::deque<CPUSample> _cpuHistory;
  std::deque<GPUTimingSample> _gpuHistory;

  static std::size_t phaseIndex(CPUPhase phase) {
    const auto i = static_cast<std::size_t>(phase);
    if (i >= 5)
      throw std::invalid_argument("Unknown CPU phase");
    return i;
  }

  SceneWork _sceneWork;

public:
  void recordSceneWork(const SceneWork &work) { _sceneWork.add(work); }

  explicit RenderRuntime(
      std::shared_ptr<ResourceLedger> resources = defaultResourceLedger())
      : _resources{std::move(resources)} {
    if (!_resources)
      throw std::invalid_argument("Runtime requires a ledger");
    _frames.reserve(3);
  }

  void attachDomain(ResourceDomainId domain) {
    if (domain == _domain)
      return;
    // Native queues have independent frame credits. Old allocation charges
    // remain in the shared ledger until their owners/device actually retire.
    _domain = domain;
    _frames.clear();
    clearPressure();
    _metrics.gpu = {};
  }

  const FramePacingProps &pacing() const noexcept { return _pacing; }

  const auto &resources() const noexcept { return _resources; }

  const auto &cpuHistory() const noexcept { return _cpuHistory; }

  const auto &gpuHistory() const noexcept { return _gpuHistory; }

  void applyPatch(const RenderRuntimePatch &patch) {
    if (patch.budgets)
      patch.budgets->validate();
    if (patch.pacing)
      patch.pacing->validate();
    if (patch.budgets)
      _resources->setBudgets(*patch.budgets);
    if (patch.pacing) {
      _pacing = *patch.pacing;
      _nextFrame =
          _lastFrame && _pacing.maximumFramesPerSecond
              ? *_lastFrame + std::chrono::duration_cast<RenderClock::duration>(
                                  std::chrono::duration<double>{
                                      1 / *_pacing.maximumFramesPerSecond})
              : RenderClock::time_point{};
    }
    retryPressure();
  }

  FrameAdmission admission(RenderClock::time_point now, std::uint64_t demand) {
    std::erase_if(_frames, [](const auto &f) { return f.expired(); });
    if (_pressureBlocked) {
      const auto s = _resources->snapshot();
      if (s.usageRevision == _blockedUsage &&
          s.policyRevision == _blockedPolicy && demand == _blockedDemand) {
        _metrics.admission = FrameAdmissionStatus::Blocked;
        return {_metrics.admission, {}};
      }
      retryPressure();
    }
    if (_frames.size() >= _pacing.maxOutstandingFrames) {
      _metrics.admission = FrameAdmissionStatus::Busy;
      return {_metrics.admission, now + std::chrono::milliseconds{2}};
    }
    if (now < _nextFrame) {
      _metrics.admission = FrameAdmissionStatus::Paced;
      return {_metrics.admission, _nextFrame};
    }
    _metrics.admission = FrameAdmissionStatus::Ready;
    return {_metrics.admission, {}};
  }

  FrameLease beginFrame(RenderClock::time_point now, ResourceDomainId domain,
                        std::uint64_t demand) {
    if (admission(now, demand).status != FrameAdmissionStatus::Ready)
      return {};
    if (_metrics.admitted == std::numeric_limits<FrameId>::max())
      throw std::overflow_error("Frame identity exhausted");
    auto frame =
        std::make_shared<FrameWork>(FrameWork{_metrics.admitted + 1, domain});
    _frames.push_back(frame);
    ++_metrics.admitted;
    _lastFrame = now;
    _nextFrame = _pacing.maximumFramesPerSecond
                     ? now + std::chrono::duration_cast<RenderClock::duration>(
                                 std::chrono::duration<double>{
                                     1 / *_pacing.maximumFramesPerSecond})
                     : now;
    return frame;
  }

  void submitted() noexcept {
    ++_metrics.submitted;
    clearPressure();
  }

  void skipped() noexcept { ++_metrics.skipped; }

  void abandoned() noexcept { ++_metrics.abandoned; }

  void block(std::string reason, std::uint64_t demand) {
    const auto s = _resources->snapshot();
    _blockedUsage = s.usageRevision;
    _blockedPolicy = s.policyRevision;
    _blockedDemand = demand;
    _pressureBlocked = true;
    _metrics.pressure = std::move(reason);
    _metrics.admission = FrameAdmissionStatus::Blocked;
  }

  // A wake permits another attempt without hiding its previous diagnostic.
  void retryPressure() noexcept { _pressureBlocked = false; }

  void clearPressure() noexcept {
    _pressureBlocked = false;
    _metrics.pressure.clear();
  }

  void beginIteration() {
    _sample = {};
    _active = {};
    begin(CPUPhase::Total);
  }

  void begin(CPUPhase phase) {
    const auto i = phaseIndex(phase);
    _starts[i] = RenderClock::now();
    _active[i] = true;
  }

  void end(CPUPhase phase) {
    const auto i = phaseIndex(phase);
    if (!_active[i])
      return;
    _sample.milliseconds[i] += std::chrono::duration<double, std::milli>(
                                   RenderClock::now() - _starts[i])
                                   .count();
    _sample.measured[i] = true;
    _active[i] = false;
  }

  const CPUSample &endIteration() {
    end(CPUPhase::Total);
    _metrics.cpu = _sample;
    ++_metrics.cpuSamples;
    _cpuHistory.push_back(_sample);
    if (_cpuHistory.size() > 240)
      _cpuHistory.pop_front();
    return _sample;
  }

  void recordIdle(double ms) { _metrics.idleMilliseconds += ms; }

  void recordGPUCollection(GPUTimingCollection collection) {
    _metrics.gpu = collection;
  }

  void recordGPU(const GPUTimingSample &sample) {
    _gpuHistory.push_back(sample);
    if (_gpuHistory.size() > 128)
      _gpuHistory.pop_front();
    ++_metrics.gpuSamples;
  }

  RenderTelemetrySnapshot telemetrySnapshot() const {
    return {{_cpuHistory.begin(), _cpuHistory.end()},
            {_gpuHistory.begin(), _gpuHistory.end()}};
  }

  RenderRuntimeSnapshot snapshot() const {
    auto result = _metrics;
    result.sceneWork = _sceneWork;
    result.resources = _resources->snapshot();
    if (!_gpuHistory.empty() &&
        _gpuHistory.back().domain == _metrics.gpu.domain &&
        _gpuHistory.back().collectionGeneration == _metrics.gpu.generation)
      result.latestGPU = _gpuHistory.back();
    result.pacing = _pacing;
    result.outstandingFrames =
        std::count_if(_frames.begin(), _frames.end(),
                      [](const auto &f) { return !f.expired(); });
    return result;
  }
};

} // namespace playground::rendering
