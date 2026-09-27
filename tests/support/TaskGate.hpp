#pragma once

#include <condition_variable>
#include <mutex>
#include <stop_token>

namespace playground::test {
class TaskGate {
  std::mutex _mutex;
  std::condition_variable_any _ready;
  bool _open{};

public:
  void wait(std::stop_token stop) {
    std::unique_lock lock{_mutex};
    _ready.wait(lock, stop, [&] { return _open; });
  }

  void open() {
    {
      std::lock_guard lock{_mutex};
      _open = true;
    }
    _ready.notify_all();
  }
};
} // namespace playground::test
