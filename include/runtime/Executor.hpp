#pragma once

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string_view>

#include <support/MoveOnlyFunction.hpp>

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
  std::shared_ptr<std::atomic<bool>> _retired;

  TaskTicket(std::stop_source stop, std::shared_ptr<std::atomic<bool>> retired)
      : _stop{std::move(stop)}, _retired{std::move(retired)} {}
  friend class Executor;

public:
  void cancel() noexcept { _stop.request_stop(); }

  bool isCanceled() const noexcept { return _stop.stop_requested(); }

  // Includes canceled queued work. Result readiness alone does not establish
  // that worker captures and their admission reservations have retired.
  bool retired() const noexcept {
    return _retired->load(std::memory_order_acquire);
  }
};

enum class TaskAdmission { Accepted, Busy, TooLarge, Closed };
std::string_view describe(TaskAdmission) noexcept;

struct TaskSubmission {
  TaskAdmission admission;
  std::optional<TaskTicket> ticket;
};

struct ExecutorStats {
  std::size_t outstanding{}, reservedBytes{};
  std::size_t maxReservedBytes{};
  bool closed{};
};

// Jobs must contain their own error/result channel. Destruction stops
// admission, requests cooperative cancellation, drops queued jobs and joins
// running jobs.
class Executor {
  struct Impl;
  std::unique_ptr<Impl> _impl;

public:
  using Job = support::MoveOnlyFunction<void(std::stop_token) noexcept>;
  explicit Executor(ExecutorProps props = {});
  ~Executor();
  Executor(const Executor &) = delete;
  Executor &operator=(const Executor &) = delete;
  TaskSubmission submit(Job, std::size_t reservedBytes);
  ExecutorStats stats() const;
  void close();
};
} // namespace playground::runtime
