#pragma once

#include <rendering/ResourceLedger.hpp>

namespace playground {
// CPU preparation estimates share the rendering ledger. Dependency-private
// allocations may differ; a reservation is admission, not a capped allocator.
class PreparationBudget {
  std::shared_ptr<rendering::ResourceLedger> _ledger;

public:
  struct Snapshot {
    std::size_t limit{}, used{}, peak{};
  };

  using Lease = rendering::ResourceLedger::Token;

  explicit PreparationBudget(std::size_t bytes)
      : _ledger{std::make_shared<rendering::ResourceLedger>(
            rendering::ResourceBudgetProps{.cpuBytes = bytes,
                                           .preparationBytes = bytes})} {}

  explicit PreparationBudget(std::shared_ptr<rendering::ResourceLedger> ledger)
      : _ledger{std::move(ledger)} {
    if (!_ledger)
      throw std::invalid_argument("Preparation budget requires a ledger");
  }

  const auto &resources() const noexcept { return _ledger; }

  Lease acquire(std::size_t bytes) const {
    if (!bytes)
      return {};
    return _ledger->reserve(rendering::MemoryClass::CPU,
                            rendering::ResourceKind::Preparation, bytes,
                            "CPU resource preparation");
  }

  Snapshot snapshot() const {
    const auto s = _ledger->snapshot();
    const auto &u =
        s.kinds[static_cast<std::size_t>(rendering::ResourceKind::Preparation)];
    return {s.budgets.preparationBytes, u.bytes, u.peak};
  }
};

inline PreparationBudget &resourcePreparationBudget() {
  static PreparationBudget budget{rendering::defaultResourceLedger()};
  return budget;
}
} // namespace playground
