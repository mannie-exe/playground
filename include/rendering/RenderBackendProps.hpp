#pragma once

#include <rendering/AllocationLimits.hpp>
#include <rendering/ResourceLedger.hpp>

namespace playground::rendering {

// Immutable construction policy, not per-frame or persistent user settings.
// A host passes the same value during selection, rollback and recovery.
struct RenderBackendProps {
  AllocationLimits allocations;
  bool gpuDebug{};
  std::shared_ptr<ResourceLedger> resources{defaultResourceLedger()};

  void validate() const {
    allocations.validate();
    if (!resources)
      throw std::invalid_argument("Backend requires a resource ledger");
  }

  bool operator==(const RenderBackendProps &) const = default;
};

} // namespace playground::rendering
