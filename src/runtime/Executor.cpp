#include <condition_variable>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#include <runtime/Executor.hpp>

namespace playground::runtime {
void ExecutorProps::validate() const {
  if (!workers || workers > 64 || !maxOutstanding || !maxReservedBytes)
    throw std::invalid_argument("Invalid executor limits");
}

struct Executor::Impl {
  struct Task {
    Job run;
    std::stop_source stop;
    std::size_t bytes;
    std::shared_ptr<std::atomic<bool>> retired;

    Task(Job job, std::size_t size)
        : run{std::move(job)}, bytes{size},
          retired{std::make_shared<std::atomic<bool>>(false)} {}

    Task(Task &&) noexcept = default;
    Task &operator=(Task &&) = delete;

    ~Task() {
      run = nullptr;
      if (retired)
        retired->store(true, std::memory_order_release);
    }
  };

  ExecutorProps props;
  mutable std::mutex mutex;
  std::condition_variable_any ready;
  std::deque<Task> pending;
  std::vector<std::optional<std::stop_source>> active;
  ExecutorStats stats;
  bool closed{};
  // Threads are last, so construction failure joins before shared state dies.
  std::vector<std::jthread> threads;

  explicit Impl(ExecutorProps p) : props{p}, active(p.workers) {
    threads.reserve(p.workers);
    for (std::size_t i = 0; i < p.workers; ++i)
      threads.emplace_back([this, i](std::stop_token shutdown) {
        for (;;) {
          std::optional<Task> task;
          {
            std::unique_lock lock{mutex};
            ready.wait(lock, shutdown,
                       [&] { return closed || !pending.empty(); });
            if (closed || shutdown.stop_requested())
              return;
            task.emplace(std::move(pending.front()));
            pending.pop_front();
            active[i] = task->stop;
          }
          if (!task->stop.stop_requested())
            task->run(task->stop.get_token());
          // Release job captures before returning admission capacity.
          task->run = nullptr;
          {
            std::lock_guard lock{mutex};
            active[i].reset();
            --stats.outstanding;
            stats.reservedBytes -= task->bytes;
          }
        }
      });
  }
};

Executor::Executor(ExecutorProps props) {
  props.validate();
  _impl = std::make_unique<Impl>(props);
}

Executor::~Executor() { close(); }

std::optional<TaskTicket> Executor::submit(Job job, std::size_t bytes) {
  if (!job)
    throw std::invalid_argument("Executor requires a job");
  // Stage captures before locking: rejected admission/allocation failure must
  // not destroy caller-owned captures while holding the executor mutex.
  Impl::Task task{std::move(job), bytes};
  std::lock_guard lock{_impl->mutex};
  if (_impl->closed ||
      _impl->stats.outstanding >= _impl->props.maxOutstanding ||
      bytes > _impl->props.maxReservedBytes - _impl->stats.reservedBytes)
    return {};
  const auto stop = task.stop;
  const auto retired = task.retired;
  _impl->pending.push_back(std::move(task));
  ++_impl->stats.outstanding;
  _impl->stats.reservedBytes += bytes;
  _impl->ready.notify_one();
  return TaskTicket{stop, retired};
}

ExecutorStats Executor::stats() const {
  std::lock_guard lock{_impl->mutex};
  return _impl->stats;
}

void Executor::close() {
  {
    std::lock_guard lock{_impl->mutex};
    _impl->closed = true;
  }
  _impl->ready.notify_all();
  // Copy stop sources without allocating; never invoke callbacks under the
  // lock.
  for (std::size_t i = 0; i < _impl->active.size(); ++i) {
    std::optional<std::stop_source> stop;
    {
      std::lock_guard lock{_impl->mutex};
      stop = _impl->active[i];
    }
    if (stop)
      stop->request_stop();
  }
  for (;;) {
    std::optional<Impl::Task> discarded;
    {
      std::lock_guard lock{_impl->mutex};
      if (_impl->pending.empty())
        break;
      discarded.emplace(std::move(_impl->pending.front()));
      _impl->pending.pop_front();
      --_impl->stats.outstanding;
      _impl->stats.reservedBytes -= discarded->bytes;
    }
    discarded->stop.request_stop();
  }
}
} // namespace playground::runtime
