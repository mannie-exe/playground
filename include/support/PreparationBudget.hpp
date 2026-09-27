#pragma once

#include <cstddef>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace playground {
// Admission accounting, not an allocator: dependency-internal allocations may
// differ from estimates. Refusal is immediate; callers may explicitly retry.
class PreparationBudget {
  struct State {
    std::mutex mutex;
    std::size_t limit, used{}, peak{};

    explicit State(std::size_t bytes) : limit{bytes} {}
  };

  std::shared_ptr<State> _state;

public:
  struct Snapshot {
    std::size_t limit{}, used{}, peak{};
  };

  class Lease {
    std::shared_ptr<State> _state;
    std::size_t _bytes{};
    friend class PreparationBudget;

    Lease(std::shared_ptr<State> state, std::size_t bytes)
        : _state{std::move(state)}, _bytes{bytes} {}

    void release() noexcept {
      if (_state) {
        std::lock_guard lock{_state->mutex};
        _state->used -= _bytes;
      }
      _state.reset();
    }

  public:
    ~Lease() { release(); }

    Lease(const Lease &) = delete;
    Lease &operator=(const Lease &) = delete;

    Lease(Lease &&other) noexcept
        : _state{std::move(other._state)}, _bytes{other._bytes} {}

    Lease &operator=(Lease &&other) noexcept {
      if (this != &other) {
        release();
        _state = std::move(other._state);
        _bytes = other._bytes;
      }
      return *this;
    }
  };

  explicit PreparationBudget(std::size_t bytes)
      : _state{std::make_shared<State>(bytes)} {
    if (!bytes)
      throw std::invalid_argument("Preparation budget must be positive");
  }

  Lease acquire(std::size_t bytes) const {
    std::lock_guard lock{_state->mutex};
    if (bytes > _state->limit - _state->used)
      throw std::length_error("Preparation budget exhausted");
    _state->used += bytes;
    if (_state->used > _state->peak)
      _state->peak = _state->used;
    return {_state, bytes};
  }

  Snapshot snapshot() const {
    std::lock_guard lock{_state->mutex};
    return {_state->limit, _state->used, _state->peak};
  }
};

inline PreparationBudget &resourcePreparationBudget() {
  static PreparationBudget budget{512 * 1024 * 1024};
  return budget;
}
} // namespace playground
