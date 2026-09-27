#pragma once

#include <functional>
#include <utility>

#include <runtime/CompletionQueue.hpp>

namespace playground::runtime {

// Explicit owner-thread traversal boundary, not a second background executor.
class DeferredMutations {
  CompletionQueue _queue;

public:
  explicit DeferredMutations(CompletionQueueProps props = {}) : _queue{props} {}

  bool defer(std::move_only_function<void()> operation) {
    return _queue.sink().post(std::move(operation));
  }

  bool remove(ActivationLifetime &owner,
              std::move_only_function<void()> destroy) {
    if (!defer(std::move(destroy)))
      return false;
    owner.deactivate();
    return true;
  }

  std::size_t flush() { return _queue.drain(); }

  std::size_t pending() const { return _queue.pending(); }
};
} // namespace playground::runtime
