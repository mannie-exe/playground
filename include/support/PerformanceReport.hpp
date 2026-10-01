#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <rendering/GPUTiming.hpp>
#include <rendering/RenderRuntime.hpp>
#include <rendering/PaintWork.hpp>
#include <ui/WorkDiagnostics.hpp>

using FramePhase = playground::rendering::CPUPhase;

// Counts are independent for execution duration and optional completion
// latency.
struct DurationStats {
  std::uint64_t count{};
  double totalMilliseconds{};
  double minimumMilliseconds{};
  double maximumMilliseconds{};

  void add(double milliseconds) {
    const auto total = totalMilliseconds + milliseconds;
    if (!std::isfinite(milliseconds) || milliseconds < 0 ||
        !std::isfinite(total))
      throw std::invalid_argument("Invalid performance duration");
    if (count == std::numeric_limits<std::uint64_t>::max())
      throw std::overflow_error("Performance sample count exhausted");
    if (!count || milliseconds < minimumMilliseconds)
      minimumMilliseconds = milliseconds;
    if (!count || milliseconds > maximumMilliseconds)
      maximumMilliseconds = milliseconds;
    totalMilliseconds = total;
    ++count;
  }

  std::optional<double> average() const noexcept {
    return count ? std::optional{totalMilliseconds / static_cast<double>(count)}
                 : std::nullopt;
  }
};

struct GPUGroupKey {
  playground::rendering::ResourceDomainId domain;
  std::uint64_t collectionGeneration{};
  std::string label;
  playground::rendering::GPUWorkContext context;

  bool operator==(const GPUGroupKey &) const = default;
};

struct GPUGroupReport {
  GPUGroupKey key;
  DurationStats duration;
  DurationStats completionLatency;
};

enum class GPUTimingStatus : std::uint8_t {
  Unsupported,
  Disabled,
  Pending,
  Measured,
};

struct PerformanceReport {
  std::uint64_t cpuFrames{};
  // Same order as FramePhase: poll, update, render, present, total.
  std::array<DurationStats, 5> cpu;
  playground::ui::UIWorkStats ui;
  std::vector<GPUGroupReport> gpu;

  GPUTimingStatus gpuTiming{GPUTimingStatus::Unsupported};
  std::optional<playground::rendering::GPUTimingCollection> collection;
  std::uint64_t gpuSamplesReceived{};
  std::uint64_t omittedGPUSamples{};
  std::uint64_t queryDrops{};
  std::uint64_t bufferDiscards{};
  DurationStats idleWait;
  playground::rendering::PaintWork paint;
  std::array<double,
             static_cast<std::size_t>(playground::ui::UIWorkPhase::Count)>
      uiMilliseconds{};
};
