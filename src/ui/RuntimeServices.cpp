#include <cmath>
#include <limits>

#include <ui/RuntimeServices.hpp>

namespace playground::ui {

TimerHandle Scheduler::schedule(double delay,
                                std::move_only_function<void()> callback,
                                double interval) {
  if (!std::isfinite(delay) || delay < 0 || !std::isfinite(interval) ||
      interval < 0 || !callback || !std::isfinite(_now + delay))
    throw std::invalid_argument("Invalid timer deadline, interval or callback");
  auto timer = std::make_shared<Timer>(
      Timer{_now + delay, interval, ++_order, true, std::move(callback)});
  _timers.push(timer);
  return TimerHandle{[weak = std::weak_ptr{timer}]() noexcept {
    if (auto value = weak.lock()) {
      value->active = false;
      if (!value->executing)
        value->callback = nullptr;
    }
  }};
}

void Scheduler::advance(double seconds) {
  if (!std::isfinite(seconds) || seconds < 0 || !std::isfinite(_now + seconds))
    throw std::invalid_argument("Invalid scheduler time step");
  if (_advancing)
    throw std::logic_error("Cannot recursively advance UI timers");
  _advancing = true;
  struct Guard {
    bool &active;
    ~Guard() { active = false; }
  } guard{_advancing};
  _now += seconds;
  const auto boundary = _order;
  while (!_timers.empty()) {
    auto timer = _timers.top();
    if (!timer->active) {
      _timers.pop();
      continue;
    }
    if (timer->deadline > _now || timer->order > boundary)
      break;
    _timers.pop();
    // Repeating timers fire at most once per advance, not an unbounded
    // catch-up.
    if (timer->interval > 0) {
      timer->deadline = _now + timer->interval;
      if (timer->deadline == _now)
        timer->deadline =
            std::nextafter(_now, std::numeric_limits<double>::infinity());
      if (!std::isfinite(timer->deadline))
        throw std::overflow_error("Timer deadline overflow");
      _timers.push(timer);
    } else {
      timer->active = false;
    }
    timer->executing = true;
    struct InvocationGuard {
      Timer &timer;
      ~InvocationGuard() {
        timer.executing = false;
        if (!timer.active)
          timer.callback = nullptr;
      }
    } invocation{*timer};
    timer->callback();
  }
}

} // namespace playground::ui
