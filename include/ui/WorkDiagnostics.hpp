#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace playground::ui {

struct UIWorkStats {
  std::uint64_t measureRequests{};
  std::uint64_t measured{};
  std::uint64_t measureCacheHits{};
  std::uint64_t measureCacheEvictions{};
  std::uint64_t arrangeRequests{};
  std::uint64_t arranged{};
  std::uint64_t arrangeSkips{};
  std::uint64_t invalidationVisits{};
  std::uint64_t containerPlans{};
  std::uint64_t textLayouts{};
  std::uint64_t textLayoutHits{};
  std::uint64_t fontFitAttempts{};
  std::uint64_t prepared{};
  std::uint64_t painted{};
  std::uint64_t realized{};
  std::uint64_t scratchGrowths{};
  std::uint64_t scratchPeakBytes{};
  std::uint64_t publications{}, publicationHits{};
};

inline UIWorkStats workDelta(const UIWorkStats &now,
                             const UIWorkStats &before) noexcept {
  UIWorkStats result;
  result.measureRequests = now.measureRequests - before.measureRequests;
  result.measured = now.measured - before.measured;
  result.measureCacheHits = now.measureCacheHits - before.measureCacheHits;
  result.measureCacheEvictions =
      now.measureCacheEvictions - before.measureCacheEvictions;
  result.arrangeRequests = now.arrangeRequests - before.arrangeRequests;
  result.arranged = now.arranged - before.arranged;
  result.arrangeSkips = now.arrangeSkips - before.arrangeSkips;
  result.invalidationVisits =
      now.invalidationVisits - before.invalidationVisits;
  result.containerPlans = now.containerPlans - before.containerPlans;
  result.textLayouts = now.textLayouts - before.textLayouts;
  result.textLayoutHits = now.textLayoutHits - before.textLayoutHits;
  result.fontFitAttempts = now.fontFitAttempts - before.fontFitAttempts;
  result.prepared = now.prepared - before.prepared;
  result.painted = now.painted - before.painted;
  result.realized = now.realized - before.realized;
  result.scratchGrowths = now.scratchGrowths - before.scratchGrowths;
  result.scratchPeakBytes = now.scratchPeakBytes;
  result.publications = now.publications - before.publications;
  result.publicationHits = now.publicationHits - before.publicationHits;
  return result;
}

inline void accumulate(UIWorkStats &sum, const UIWorkStats &value) noexcept {
  sum.measureRequests += value.measureRequests;
  sum.measured += value.measured;
  sum.measureCacheHits += value.measureCacheHits;
  sum.measureCacheEvictions += value.measureCacheEvictions;
  sum.arrangeRequests += value.arrangeRequests;
  sum.arranged += value.arranged;
  sum.arrangeSkips += value.arrangeSkips;
  sum.invalidationVisits += value.invalidationVisits;
  sum.containerPlans += value.containerPlans;
  sum.textLayouts += value.textLayouts;
  sum.textLayoutHits += value.textLayoutHits;
  sum.fontFitAttempts += value.fontFitAttempts;
  sum.prepared += value.prepared;
  sum.painted += value.painted;
  sum.realized += value.realized;
  sum.scratchGrowths += value.scratchGrowths;
  sum.scratchPeakBytes = std::max(sum.scratchPeakBytes, value.scratchPeakBytes);
  sum.publications += value.publications;
  sum.publicationHits += value.publicationHits;
}

enum class UIWorkPhase : std::size_t {
  Layout,
  Prepare,
  Paint,
  Input,
  Completions,
  Publication,
  Count
};

struct UIWorkSample {
  std::uint64_t root{};
  UIWorkStats work;
  std::array<double, static_cast<std::size_t>(UIWorkPhase::Count)>
      milliseconds{};
};

class UIWorkTiming {
public:
  using Clock = double (*)() noexcept;

private:
  static double now() noexcept {
    return std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
  }

  Clock _clock{now};
  bool _enabled{};
  std::array<std::size_t, static_cast<std::size_t>(UIWorkPhase::Count)>
      _depth{};
  std::array<double, static_cast<std::size_t>(UIWorkPhase::Count)> _totals{};

public:
  void setEnabled(bool enabled) noexcept { _enabled = enabled; }

  void setClock(Clock clock) noexcept { _clock = clock ? clock : now; }

  const auto &totals() const noexcept { return _totals; }

  class Scope {
    UIWorkTiming &_owner;
    std::size_t _phase;
    double _start{};
    bool _active{}, _outer{};

  public:
    Scope(UIWorkTiming &owner, UIWorkPhase phase) noexcept
        : _owner{owner}, _phase{static_cast<std::size_t>(phase)},
          _active{owner._enabled && _phase < owner._depth.size()} {
      if (_active) {
        _outer = owner._depth[_phase]++ == 0;
        if (_outer)
          _start = owner._clock();
      }
    }

    ~Scope() {
      if (_active) {
        --_owner._depth[_phase];
        if (_outer)
          _owner._totals[_phase] += std::max(0.0, _owner._clock() - _start);
      }
    }

    Scope(const Scope &) = delete;
    Scope &operator=(const Scope &) = delete;
  };
};

inline std::uint64_t nextUIWorkId() noexcept {
  static std::atomic<std::uint64_t> next{1};
  return next.fetch_add(1, std::memory_order_relaxed);
}

} // namespace playground::ui
