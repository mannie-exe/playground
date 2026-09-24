#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <rendering/ResourceDomain.hpp>

namespace playground::rendering {

struct GPUTimingSample {
  std::uint64_t sequence{};
  std::string label;
  double milliseconds{};
  ResourceDomainId domain{};
  std::optional<double> completionLatencyMilliseconds;
};

} // namespace playground::rendering
