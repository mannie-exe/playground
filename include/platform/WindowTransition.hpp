#pragma once

#include <algorithm>
#include <chrono>
#include <optional>
#include <string_view>

namespace playground::platform {

struct WindowPlacementCapabilities {
  bool requestsSupported;
  bool globalPositionAvailable;
};

// A supported operation is still only a request to the window manager.
constexpr WindowPlacementCapabilities
windowPlacementCapabilities(std::string_view videoDriver) {
  const bool supported = videoDriver != "wayland";
  return {supported, supported};
}

enum class WindowRequestResult { Submitted, Unsupported, Failed };
enum class WindowTransitionOutcome {
  Idle, Pending, Observed, Unconfirmed, Failed, Cancelled
};
enum class WindowTransitionAction {
  None, LeaveFullscreen, Restore, ApplyGeometry, ApplyMode,
  RouteFullscreenDisplay
};

// Native-operation sequencing only. No SDL objects, callbacks, or app state.
// The adapter supplies observations; each action is emitted at most once.
class WindowTransition {
public:
  using Clock = std::chrono::steady_clock;
  using TimePoint = Clock::time_point;
  struct Observation {
    bool fullscreen{};
    bool maximized{};
    bool minimized{};
    bool geometryObserved{};
    bool modeObserved{};
    bool displayObserved{};
  };

  void begin(bool routeFullscreenDisplay, TimePoint now) {
    _phase = Phase::LeaveFullscreen;
    _routeDisplay = routeFullscreenDisplay;
    _outcome = WindowTransitionOutcome::Pending;
    _deadline = now + std::chrono::seconds{5};
    _pollAt = now;
  }

  WindowTransitionAction advance(Observation state, TimePoint now) {
    if (_outcome != WindowTransitionOutcome::Pending)
      return WindowTransitionAction::None;
    if (now >= _deadline) {
      _outcome = WindowTransitionOutcome::Unconfirmed;
      return WindowTransitionAction::None;
    }
    _pollAt = std::min(_deadline, now + std::chrono::milliseconds{16});
    for (;;) {
      switch (_phase) {
      case Phase::LeaveFullscreen:
        _phase = Phase::WaitFullscreen;
        return WindowTransitionAction::LeaveFullscreen;
      case Phase::WaitFullscreen:
        if (state.fullscreen)
          return WindowTransitionAction::None;
        _phase = Phase::Restore;
        break;
      case Phase::Restore:
        _phase = Phase::WaitNormal;
        return WindowTransitionAction::Restore;
      case Phase::WaitNormal:
        if (state.fullscreen || state.maximized || state.minimized)
          return WindowTransitionAction::None;
        _phase = Phase::Geometry;
        break;
      case Phase::Geometry:
        _phase = Phase::WaitGeometry;
        _geometryUntil = now + std::chrono::milliseconds{250};
        return WindowTransitionAction::ApplyGeometry;
      case Phase::WaitGeometry:
        // Clamped/ignored sizes must not indefinitely block the final mode.
        if (!state.geometryObserved && now < _geometryUntil)
          return WindowTransitionAction::None;
        _phase = Phase::Mode;
        break;
      case Phase::Mode:
        _phase = Phase::WaitMode;
        return WindowTransitionAction::ApplyMode;
      case Phase::WaitMode:
        if (!state.modeObserved)
          return WindowTransitionAction::None;
        _phase = _routeDisplay && !state.displayObserved ? Phase::RouteDisplay
                                                       : Phase::Confirm;
        break;
      case Phase::RouteDisplay:
        _phase = Phase::Confirm;
        return WindowTransitionAction::RouteFullscreenDisplay;
      case Phase::Confirm:
        if (state.geometryObserved && state.modeObserved &&
            state.displayObserved)
          _outcome = WindowTransitionOutcome::Observed;
        return WindowTransitionAction::None;
      }
    }
  }

  WindowTransitionOutcome outcome() const { return _outcome; }
  std::optional<TimePoint> wakeAt() const {
    return _outcome == WindowTransitionOutcome::Pending
               ? std::optional{_pollAt} : std::nullopt;
  }
  void fail() { _outcome = WindowTransitionOutcome::Failed; }
  void cancel() { _outcome = WindowTransitionOutcome::Cancelled; }

private:
  enum class Phase {
    LeaveFullscreen, WaitFullscreen, Restore, WaitNormal, Geometry,
    WaitGeometry, Mode, WaitMode, RouteDisplay, Confirm
  };
  Phase _phase{Phase::LeaveFullscreen};
  WindowTransitionOutcome _outcome{WindowTransitionOutcome::Idle};
  bool _routeDisplay{};
  TimePoint _deadline{}, _geometryUntil{}, _pollAt{};
};

} // namespace playground::platform
