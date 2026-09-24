#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include <rendering/ResourceDomain.hpp>

namespace playground::rendering {

inline constexpr std::size_t maximumGPUTimingLabelBytes{128};

inline void validateGPUTimingLabel(std::string_view label) {
  if (label.empty() || label.size() > maximumGPUTimingLabelBytes)
    throw std::invalid_argument("GPU timing label must contain 1..128 bytes");
}

struct GPUTimingSample {
  std::uint64_t sequence{};
  std::string label;
  double milliseconds{};
  ResourceDomainId domain{};
  std::optional<double> completionLatencyMilliseconds;
};

} // namespace playground::rendering
