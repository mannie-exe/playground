#pragma once

#include <rendering/ResourceLedger.hpp>

namespace playground::rendering {
// Compatibility facade for an isolated byte budget. Shared renderer resources
// reserve directly from their ResourceLedger instead of creating another pool.
class AllocationBudget {
  ResourceLedger _ledger;
  std::size_t _limit;

public:
  explicit AllocationBudget(std::size_t limit)
      : _ledger{
            ResourceBudgetProps{.cpuBytes = std::max(std::size_t{1}, limit)}},
        _limit{limit} {}

  std::size_t bytes() const { return _ledger.snapshot().memory[0].bytes; }

  ResourceLedger::Token reserve(std::size_t amount) {
    if (!_limit)
      throw ResourcePressure("Allocation", amount, bytes(), _limit);
    if (!amount)
      return {};
    return _ledger.reserve(MemoryClass::CPU, ResourceKind::Surface, amount,
                           "Allocation");
  }
};
} // namespace playground::rendering
