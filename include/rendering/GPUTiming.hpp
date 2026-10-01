#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include <math/Geometry2D.hpp>
#include <rendering/ResourceDomain.hpp>

namespace playground::rendering {

inline constexpr std::size_t maximumGPUTimingLabelBytes{128};

inline void validateGPUTimingLabel(std::string_view label) {
  if (label.empty() || label.size() > maximumGPUTimingLabelBytes)
    throw std::invalid_argument("GPU timing label must contain 1..128 bytes");
}

struct GPUWorkContext {
  std::optional<math::Vec2i> targetPixels;
  std::optional<math::Vec2i> sourcePixels;
  std::uint64_t frameId{}; // zero denotes preparation outside an admitted frame

  std::uint64_t workloadId{}, qualityRevision{};

  bool operator==(const GPUWorkContext &) const = default;
};

inline void validateGPUWorkContext(const GPUWorkContext &context) {
  for (const auto &extent : {context.targetPixels, context.sourcePixels})
    if (extent && (extent->x <= 0 || extent->y <= 0))
      throw std::invalid_argument("GPU work extents must be positive pixels");
}

struct GPUTimingSample {
  std::uint64_t sequence{};
  std::string label;
  double milliseconds{};
  ResourceDomainId domain{};
  std::optional<double> completionLatencyMilliseconds;
  GPUWorkContext context;
  std::uint64_t collectionGeneration{};
};

// Owner-thread snapshot. Counters are cumulative within domain/generation;
// pending counts only that generation's live recording/submitted queries.
struct GPUTimingCollection {
  ResourceDomainId domain;
  std::uint64_t generation{};
  bool supported{};
  bool enabled{};
  std::uint32_t pending{};
  std::uint64_t queryDrops{};
  std::uint64_t bufferDiscards{};
};

} // namespace playground::rendering
