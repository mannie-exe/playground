#include <algorithm>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <utility>

#include <runtime/CompletionQueue.hpp>

namespace playground::runtime {
namespace detail {
struct CompletionState {
  const CompletionQueueProps props;
  std::mutex mutex;
  std::deque<std::move_only_function<void()>> pending;
  bool closed{};
  std::function<void()> wake;

  explicit CompletionState(CompletionQueueProps props) : props{props} {}
};
} // namespace detail

void CompletionQueueProps::validate() const {
  if (!maxPending || !maxPerDrain)
    throw std::invalid_argument("Completion queue limits must be positive");
}

CompletionSink::CompletionSink(std::weak_ptr<detail::CompletionState> state)
    : _state{std::move(state)} {}

bool CompletionSink::post(std::move_only_function<void()> callback) const {
  if (!callback)
    throw std::invalid_argument("Completion requires a callback");
  auto state = _state.lock();
  if (!state)
    return false;
  std::function<void()> wake;
  {
    std::lock_guard lock{state->mutex};
    if (state->closed || state->pending.size() >= state->props.maxPending)
      return false;
    wake = state->wake;
    state->pending.push_back(std::move(callback));
  }
  // Notification failure must not turn an accepted post into a reported
  // failure.
  if (wake)
    try {
      wake();
    } catch (...) {
    }
  return true;
}

CompletionQueue::CompletionQueue(CompletionQueueProps props)
    : _owner{std::this_thread::get_id()} {
  props.validate();
  _state = std::make_shared<detail::CompletionState>(props);
}

bool CompletionSink::post(ActivationToken owner,
                          std::move_only_function<void()> callback) const {
  if (!callback)
    throw std::invalid_argument("Completion requires a callback");
  if (!owner.isActive())
    return false;
  return post([owner, callback = std::move(callback)]() mutable {
    if (owner.isActive())
      callback();
  });
}

CompletionQueue::~CompletionQueue() { close(); }

CompletionQueueProps CompletionQueue::props() const { return _state->props; }

std::size_t CompletionQueue::pending() const {
  std::lock_guard lock{_state->mutex};
  return _state->pending.size();
}

void CompletionQueue::setWakeCallback(std::function<void()> callback) {
  std::function<void()> wake;
  {
    std::lock_guard lock{_state->mutex};
    _state->wake = std::move(callback);
    if (!_state->pending.empty())
      wake = _state->wake;
  }
  if (wake)
    try {
      wake();
    } catch (...) {
    }
}

void CompletionQueue::close() {
  std::deque<std::move_only_function<void()>> discarded;
  {
    std::lock_guard lock{_state->mutex};
    _state->closed = true;
    discarded.swap(_state->pending);
  }
  // Destruct captured resources outside the lock; they may themselves post.
}

std::size_t CompletionQueue::drain() {
  if (std::this_thread::get_id() != _owner || _draining)
    throw std::logic_error(
        "Completions require nonrecursive owner-thread draining");
  _draining = true;

  struct Guard {
    bool &active;

    ~Guard() { active = false; }
  } guard{_draining};

  std::size_t count;
  {
    std::lock_guard lock{_state->mutex};
    count = std::min(_state->pending.size(), _state->props.maxPerDrain);
  }
  std::size_t completed{};
  for (; completed < count; ++completed) {
    std::move_only_function<void()> callback;
    {
      std::lock_guard lock{_state->mutex};
      if (_state->closed || _state->pending.empty())
        break;
      callback = std::move(_state->pending.front());
      _state->pending.pop_front();
    }
    // Failed callbacks are attempted once; the untouched tail stays queued.
    callback();
  }
  return completed;
}
} // namespace playground::runtime
