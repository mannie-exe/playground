#pragma once

#include <cstddef>
#include <memory>
#include <stdexcept>

namespace playground::rendering {

// Owner-thread accounting of estimated bytes. A reservation outlives the cache
// if a native allocation is still in use; this is not a query of free VRAM.
class AllocationBudget {
  struct State {
    std::size_t bytes{};
  };
  struct Reservation {
    std::shared_ptr<State> state;
    std::size_t bytes;
    ~Reservation() { state->bytes -= bytes; }
  };
  std::shared_ptr<State> _state{std::make_shared<State>()};
  std::size_t _limit;

public:
  explicit AllocationBudget(std::size_t limit) : _limit{limit} {}
  std::size_t bytes() const noexcept { return _state->bytes; }
  std::shared_ptr<void> reserve(std::size_t bytes) {
    if (bytes > _limit || _state->bytes > _limit - bytes)
      throw std::length_error("Live render target allocation budget exhausted");
    auto result = std::make_shared<Reservation>(_state, bytes);
    _state->bytes += bytes;
    return result;
  }
};

} // namespace playground::rendering
