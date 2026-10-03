#pragma once

#include <runtime/ResourceLedger.hpp>

// Existing renderer APIs retain source compatibility with the neutral account.
namespace playground::rendering {
using runtime::AllocationState;
using runtime::defaultResourceLedger;
using runtime::MemoryClass;
using runtime::OwnerUsage;
using runtime::ResourceAllocationFailure;
using runtime::ResourceBudgetProps;
using runtime::ResourceKind;
using runtime::ResourceLedger;
using runtime::ResourceOwner;
using runtime::ResourcePressure;
using runtime::ResourceSnapshot;
using runtime::ResourceUsage;
} // namespace playground::rendering
