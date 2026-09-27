#pragma once

#include <rendering/AllocationLimits.hpp>

namespace playground::rendering {

// Immutable construction policy, not per-frame or persistent user settings.
// A host passes the same value during selection, rollback and recovery.
struct RenderBackendProps {
  AllocationLimits allocations;
  bool gpuDebug{};

  void validate() const { allocations.validate(); }

  bool operator==(const RenderBackendProps &) const = default;
};

} // namespace playground::rendering
