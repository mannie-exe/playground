#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <thread>
#include <utility>

#include <runtime/ActivationLifetime.hpp>

namespace playground::runtime {
namespace detail {
struct CompletionState;
}

struct CompletionQueueProps {
  std::size_t maxPending{4096};
  std::size_t maxPerDrain{256};
  void validate() const;
};

// Copyable weak delivery endpoint. Posting does not execute the callback.
class CompletionSink {
  std::weak_ptr<detail::CompletionState> _state;
  explicit CompletionSink(std::weak_ptr<detail::CompletionState> state);
  friend class CompletionQueue;

public:
  bool post(std::move_only_function<void()> callback) const;
  bool post(ActivationToken owner,
            std::move_only_function<void()> callback) const;
};

class ActivationSink {
  CompletionSink _sink;
  ActivationToken _owner;

public:
  ActivationSink(CompletionSink sink, ActivationToken owner)
      : _sink{std::move(sink)}, _owner{owner} {}

  bool post(std::move_only_function<void()> callback) const {
    return _sink.post(_owner, std::move(callback));
  }
};

// Many posting threads, one draining owner. Not a worker executor or GPU queue.
class CompletionQueue {
  std::shared_ptr<detail::CompletionState> _state;
  std::thread::id _owner;
  bool _draining{};

public:
  explicit CompletionQueue(CompletionQueueProps props = {});
  ~CompletionQueue();
  CompletionQueue(const CompletionQueue &) = delete;
  CompletionQueue &operator=(const CompletionQueue &) = delete;

  CompletionSink sink() const { return CompletionSink{_state}; }

  CompletionQueueProps props() const;
  std::size_t pending() const;
  void setWakeCallback(std::function<void()> callback);
  std::size_t drain();
  void close();
};

} // namespace playground::runtime
