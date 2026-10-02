#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <queue>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <support/MoveOnlyFunction.hpp>
#include <ui/NodeIdentity.hpp>
#include <ui/Theme.hpp>

namespace playground::ui {

class Connection {
  support::MoveOnlyFunction<void() noexcept> _disconnect;

public:
  Connection() = default;

  explicit Connection(support::MoveOnlyFunction<void() noexcept> disconnect)
      : _disconnect{std::move(disconnect)} {}

  ~Connection() { disconnect(); }

  Connection(Connection &&other) noexcept
      : _disconnect{std::exchange(other._disconnect, {})} {}

  Connection &operator=(Connection &&other) noexcept {
    if (this != &other) {
      disconnect();
      _disconnect = std::exchange(other._disconnect, {});
    }
    return *this;
  }

  Connection(const Connection &) = delete;
  Connection &operator=(const Connection &) = delete;

  void disconnect() noexcept {
    if (_disconnect) {
      auto callback = std::exchange(_disconnect, {});
      callback();
    }
  }
};

template <typename... Args> class Signal {
  struct Slot {
    bool active{true};
    support::MoveOnlyFunction<void(Args...)> callback;
    std::size_t executions{};

    void invoke(Args... args) {
      ++executions;

      struct Guard {
        Slot &slot;

        ~Guard() {
          if (--slot.executions == 0 && !slot.active)
            slot.callback = nullptr;
        }
      } guard{*this};

      callback(args...);
    }
  };

  std::vector<std::shared_ptr<Slot>> _slots;

public:
  Connection connect(support::MoveOnlyFunction<void(Args...)> callback) {
    if (!callback)
      throw std::invalid_argument("A subscription requires a callback");
    auto slot = std::make_shared<Slot>(Slot{true, std::move(callback)});
    std::erase_if(_slots, [](const auto &item) { return !item->active; });
    _slots.push_back(slot);
    return Connection{[weak = std::weak_ptr{slot}]() noexcept {
      if (auto value = weak.lock()) {
        value->active = false;
        // Self-disconnection must keep the executing callable alive.
        if (value->executions == 0)
          value->callback = nullptr;
      }
    }};
  }

  void emit(Args... args) {
    // A callback may disconnect itself or add another callback safely.
    const auto snapshot = _slots;
    for (const auto &slot : snapshot)
      if (slot->active)
        slot->invoke(args...);
  }
};

using TimerHandle = Connection;

class Scheduler {
  struct Timer {
    double deadline{};
    double interval{};
    std::uint64_t order{};
    bool active{true};
    support::MoveOnlyFunction<void()> callback;
    bool executing{};
  };

  struct Later {
    bool operator()(const std::shared_ptr<Timer> &a,
                    const std::shared_ptr<Timer> &b) const {
      return a->deadline == b->deadline ? a->order > b->order
                                        : a->deadline > b->deadline;
    }
  };

  std::priority_queue<std::shared_ptr<Timer>,
                      std::vector<std::shared_ptr<Timer>>, Later>
      _timers;
  double _now{};
  std::uint64_t _order{};
  bool _advancing{};

public:
  double now() const noexcept { return _now; }

  std::optional<double> nextDelay() {
    while (!_timers.empty() && !_timers.top()->active)
      _timers.pop();
    if (_timers.empty())
      return {};
    return std::max(0.0, _timers.top()->deadline - _now);
  }

  TimerHandle schedule(double delay, support::MoveOnlyFunction<void()> callback,
                       double interval = 0);

  void advance(double seconds);
};

struct UIServices {
  ResolvedTheme theme{defaultResolvedTheme()};
  std::function<std::string()> readClipboard;
  std::function<void(std::string_view)> writeClipboard;
  Scheduler *scheduler{};
  std::function<void(std::string)> diagnostic;
  std::function<void(support::MoveOnlyFunction<void()>)> defer;
  std::function<void(NodeId)> focusAfterLayout;

  struct CacheBudget {
    std::size_t limit{64 * 1024 * 1024};
    std::size_t used{};

    bool reserve(std::size_t bytes) noexcept {
      if (used > limit || bytes > limit - used)
        return false;
      used += bytes;
      return true;
    }
  };

  std::shared_ptr<CacheBudget> rasterBudget{std::make_shared<CacheBudget>()};
};

} // namespace playground::ui
