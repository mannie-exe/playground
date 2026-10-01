#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace playground::rendering {

enum class MemoryClass { CPU, GPU, Count };
enum class ResourceKind {
  Target,
  Texture,
  Mesh,
  Upload,
  Stream,
  Surface,
  Asset,
  Preparation,
  Count
};
enum class AllocationState { Reserved, Owned, Retiring, Count };

struct ResourceBudgetProps {
  std::size_t cpuBytes{1024ULL * 1024 * 1024};
  std::size_t gpuBytes{2048ULL * 1024 * 1024};
  std::size_t targetBytes{1024ULL * 1024 * 1024};
  std::size_t preparationBytes{512ULL * 1024 * 1024};

  void validate() const {
    if (!cpuBytes || !gpuBytes || !targetBytes || !preparationBytes)
      throw std::invalid_argument("Resource budgets must be positive");
  }

  bool operator==(const ResourceBudgetProps &) const = default;
};

struct ResourceUsage {
  std::size_t bytes{}, peak{}, allocations{};
};

struct ResourceSnapshot {
  ResourceBudgetProps budgets;
  std::array<ResourceUsage, 2> memory{};
  std::array<ResourceUsage, 8> kinds{};
  std::array<std::size_t, 3> states{};
  std::uint64_t policyRevision{}, usageRevision{}, refusals{};
};

// A policy refusal is distinct from a failed native allocation/device loss.
class ResourcePressure : public std::length_error {
public:
  enum class Unit { Bytes, Slots };
  const std::size_t requested, used, limit;
  const Unit unit;

  ResourcePressure(std::string_view context, std::size_t request,
                   std::size_t current, std::size_t ceiling,
                   Unit measure = Unit::Bytes)
      : std::length_error(
            measure == Unit::Bytes
                ? std::format("{}: resource budget exhausted: requested {} "
                              "bytes, {} bytes in use, {} byte limit",
                              context, request, current, ceiling)
                : std::format("{}: work capacity exhausted: requested {} "
                              "slots, {} in use, {} slot limit",
                              context, request, current, ceiling)),
        requested{request}, used{current}, limit{ceiling}, unit{measure} {}
};

class ResourceAllocationFailure : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

class ResourceLedger {
  struct State {
    std::mutex mutex;
    ResourceSnapshot snapshot;
    std::uint64_t nextId{};
  };

  std::shared_ptr<State> _state{std::make_shared<State>()};

public:
  // Does not retain a renderer/device. Owners and submission leases share the
  // same token; sharing never adds another charge.
  class Allocation {
    friend class ResourceLedger;
    std::shared_ptr<State> _state;
    std::size_t _bytes{}, _memory{}, _kind{};
    AllocationState _phase{AllocationState::Reserved};
    std::uint64_t _id{};

  public:
    Allocation() = default;
    Allocation(const Allocation &) = delete;
    Allocation &operator=(const Allocation &) = delete;

    ~Allocation() {
      if (!_state)
        return;
      std::lock_guard lock{_state->mutex};
      auto &s = _state->snapshot;
      for (auto *usage : {&s.memory[_memory], &s.kinds[_kind]}) {
        usage->bytes -= _bytes;
        --usage->allocations;
      }
      s.states[static_cast<std::size_t>(_phase)] -= _bytes;
      ++s.usageRevision;
    }

    std::size_t bytes() const noexcept { return _bytes; }

    std::uint64_t id() const noexcept { return _id; }

    void setState(AllocationState phase) {
      if (!_state)
        throw std::logic_error("Unreserved allocation has no state");
      if (static_cast<std::size_t>(phase) >= 3)
        throw std::invalid_argument("Unknown allocation state");
      std::lock_guard lock{_state->mutex};
      auto &s = _state->snapshot;
      s.states[static_cast<std::size_t>(_phase)] -= _bytes;
      s.states[static_cast<std::size_t>(phase)] += _bytes;
      _phase = phase;
      ++s.usageRevision;
    }
  };

  using Token = std::shared_ptr<Allocation>;

  explicit ResourceLedger(ResourceBudgetProps props = {}) {
    props.validate();
    _state->snapshot.budgets = props;
  }

  Token reserve(MemoryClass memory, ResourceKind kind, std::size_t bytes,
                std::string_view context) {
    if (static_cast<std::size_t>(memory) >= 2 ||
        static_cast<std::size_t>(kind) >= 8 || !bytes)
      throw std::invalid_argument("Invalid resource reservation");
    auto result = std::make_shared<Allocation>();
    std::lock_guard lock{_state->mutex};
    auto &s = _state->snapshot;
    const auto mi = static_cast<std::size_t>(memory);
    const auto ki = static_cast<std::size_t>(kind);
    const auto check = [&](std::size_t used, std::size_t cap) {
      if (used > cap || bytes > cap - used) {
        ++s.refusals;
        throw ResourcePressure(context, bytes, used, cap);
      }
    };
    check(s.memory[mi].bytes,
          memory == MemoryClass::CPU ? s.budgets.cpuBytes : s.budgets.gpuBytes);
    if (kind == ResourceKind::Target)
      check(s.kinds[ki].bytes, s.budgets.targetBytes);
    if (kind == ResourceKind::Preparation)
      check(s.kinds[ki].bytes, s.budgets.preparationBytes);
    const auto maximum = std::numeric_limits<std::size_t>::max();
    if (s.memory[0].bytes > maximum - s.memory[1].bytes ||
        bytes > maximum - s.memory[0].bytes - s.memory[1].bytes ||
        _state->nextId == std::numeric_limits<std::uint64_t>::max())
      throw std::overflow_error("Resource accounting capacity exhausted");
    result->_bytes = bytes;
    result->_memory = mi;
    result->_kind = ki;
    result->_id = ++_state->nextId;
    result->_state = _state;
    for (auto *usage : {&s.memory[mi], &s.kinds[ki]}) {
      usage->bytes += bytes;
      usage->peak = std::max(usage->peak, usage->bytes);
      ++usage->allocations;
    }
    s.states[0] += bytes;
    ++s.usageRevision;
    return result;
  }

  // Partition an exclusive reservation without releasing/reacquiring capacity.
  // The parent retains any remaining allowance; total commitment is unchanged.
  Token splitReservation(const Token &parent, std::size_t bytes,
                         std::optional<ResourceKind> destination = {}) {
    if (!parent || parent.use_count() != 1 || !bytes)
      throw std::invalid_argument(
          "Splitting requires an exclusive reservation");
    auto result = std::make_shared<Allocation>();
    std::lock_guard lock{_state->mutex};
    if (parent->_state != _state ||
        parent->_phase != AllocationState::Reserved || bytes > parent->_bytes)
      throw std::invalid_argument("Invalid reservation partition");
    if (_state->nextId == std::numeric_limits<std::uint64_t>::max())
      throw std::overflow_error("Resource identity exhausted");
    const auto kind =
        destination ? static_cast<std::size_t>(*destination) : parent->_kind;
    if (kind >= 8)
      throw std::invalid_argument("Unknown reservation category");
    auto &s = _state->snapshot;
    if (kind != parent->_kind) {
      const auto cap =
          kind == static_cast<std::size_t>(ResourceKind::Target)
              ? s.budgets.targetBytes
          : kind == static_cast<std::size_t>(ResourceKind::Preparation)
              ? s.budgets.preparationBytes
              : std::numeric_limits<std::size_t>::max();
      if (s.kinds[kind].bytes > cap || bytes > cap - s.kinds[kind].bytes) {
        ++s.refusals;
        throw ResourcePressure("Reservation transfer", bytes,
                               s.kinds[kind].bytes, cap);
      }
      s.kinds[parent->_kind].bytes -= bytes;
      s.kinds[kind].bytes += bytes;
      s.kinds[kind].peak = std::max(s.kinds[kind].peak, s.kinds[kind].bytes);
    }
    result->_state = _state;
    result->_bytes = bytes;
    result->_memory = parent->_memory;
    result->_kind = kind;
    result->_id = ++_state->nextId;
    parent->_bytes -= bytes;
    ++s.memory[result->_memory].allocations;
    ++s.kinds[result->_kind].allocations;
    ++s.usageRevision;
    return result;
  }

  void validateReservation(const Token &token, MemoryClass memory,
                           ResourceKind kind, std::size_t bytes) const {
    std::lock_guard lock{_state->mutex};
    if (!token || token->_state != _state ||
        token->_phase != AllocationState::Reserved ||
        token->_memory != static_cast<std::size_t>(memory) ||
        token->_kind != static_cast<std::size_t>(kind) ||
        token->_bytes != bytes)
      throw std::invalid_argument(
          "Allocation does not match its reserved storage");
  }

  void setBudgets(ResourceBudgetProps props) {
    props.validate();
    std::lock_guard lock{_state->mutex};
    if (_state->snapshot.budgets == props)
      return;
    _state->snapshot.budgets = props;
    ++_state->snapshot.policyRevision;
  }

  ResourceSnapshot snapshot() const {
    std::lock_guard lock{_state->mutex};
    return _state->snapshot;
  }
};

// Shared process rendering account, also available to CPU asset workers and
// standalone adapters. Tests/embedded hosts can supply an isolated ledger.
inline std::shared_ptr<ResourceLedger> defaultResourceLedger() {
  static auto ledger = std::make_shared<ResourceLedger>();
  return ledger;
}

} // namespace playground::rendering
