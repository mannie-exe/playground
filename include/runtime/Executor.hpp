#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>

namespace playground::runtime {

struct ExecutorProps {
  std::size_t workers{1};
  std::size_t maxOutstanding{8};
  // Admission reservation, not a measurement or hard allocator limit.
  std::size_t maxReservedBytes{512 * 1024 * 1024};
  void validate() const;
};

class TaskTicket {
  std::stop_source _stop;

  explicit TaskTicket(std::stop_source stop) : _stop{std::move(stop)} {}
  friend class Executor;

public:
  void cancel() noexcept { _stop.request_stop(); }

  bool isCanceled() const noexcept { return _stop.stop_requested(); }
};

struct ExecutorStats {
  std::size_t outstanding{}, reservedBytes{};
};

// Jobs must contain their own error/result channel. Destruction stops
// admission, requests cooperative cancellation, drops queued jobs and joins
// running jobs.
class Executor {
  struct Impl;
  std::unique_ptr<Impl> _impl;

public:
  using Job = std::move_only_function<void(std::stop_token) noexcept>;
  explicit Executor(ExecutorProps props = {});
  ~Executor();
  Executor(const Executor &) = delete;
  Executor &operator=(const Executor &) = delete;
  std::optional<TaskTicket> submit(Job, std::size_t reservedBytes);
  ExecutorStats stats() const;
  void close();
};
} // namespace playground::runtime
