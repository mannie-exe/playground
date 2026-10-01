#include <chrono>

#include <platform/WindowTransition.hpp>
#include <support/Test.hpp>

using namespace playground;
using namespace std::chrono_literals;
using Transition = platform::WindowTransition;
using Action = platform::WindowTransitionAction;
using Outcome = platform::WindowTransitionOutcome;

int main() {
  return test::run([] {
    const auto wayland = platform::windowPlacementCapabilities("wayland");
    test::require(!wayland.requestsSupported && !wayland.globalPositionAvailable,
                  "Wayland does not advertise global placement or coordinates");
    for (const auto driver : {"cocoa", "windows", "x11"}) {
      const auto desktop = platform::windowPlacementCapabilities(driver);
      test::require(desktop.requestsSupported && desktop.globalPositionAvailable,
                    "desktop placement capability is separate from acceptance");
    }

    const Transition::TimePoint start{};
    Transition transition;
    test::require(transition.outcome() == Outcome::Idle && !transition.wakeAt() &&
                      transition.advance({}, start) == Action::None,
                  "idle transition schedules no work");
    transition.begin(true, start);
    test::require(transition.outcome() == Outcome::Pending &&
                      transition.wakeAt() == start,
                  "new transition is pending, not an observed native change");
    Transition::Observation observed{.fullscreen = true};
    test::require(transition.advance(observed, start) == Action::LeaveFullscreen,
                  "leave fullscreen is first native request");
    test::require(transition.advance(observed, start + 1ms) == Action::None &&
                      transition.advance(observed, start + 2ms) == Action::None,
                  "accepted leave request must be observed before restoration");
    observed.fullscreen = false;
    observed.maximized = true;
    test::require(transition.advance(observed, start + 3ms) == Action::Restore,
                  "restoration starts only after leaving fullscreen");
    test::require(transition.advance(observed, start + 4ms) == Action::None,
                  "geometry waits for maximization to clear");
    observed.maximized = false;
    observed.minimized = true;
    test::require(transition.advance(observed, start + 5ms) == Action::None,
                  "geometry also waits for minimization to clear");
    observed.minimized = false;
    test::require(transition.advance(observed, start + 6ms) == Action::ApplyGeometry,
                  "normal state unlocks geometry exactly once");
    test::require(transition.advance(observed, start + 7ms) == Action::None,
                  "geometry submission is not treated as geometry observation");
    observed.geometryObserved = true;
    test::require(transition.advance(observed, start + 8ms) == Action::ApplyMode,
                  "observed geometry unlocks target mode");
    test::require(transition.advance(observed, start + 9ms) == Action::None,
                  "fullscreen display routing waits for target mode observation");
    observed.modeObserved = true;
    test::require(transition.advance(observed, start + 10ms) ==
                      Action::RouteFullscreenDisplay,
                  "display routing occurs after desktop fullscreen is observed");
    test::require(transition.advance(observed, start + 11ms) == Action::None &&
                      transition.outcome() == Outcome::Pending,
                  "display routing request does not claim completed placement");
    observed.displayObserved = true;
    test::require(transition.advance(observed, start + 12ms) == Action::None &&
                      transition.outcome() == Outcome::Observed &&
                      !transition.wakeAt(),
                  "all observed requirements complete transition");
    test::require(transition.advance({}, start + 6s) == Action::None &&
                      transition.outcome() == Outcome::Observed,
                  "terminal success does not restart or expire");

    transition.begin(true, start);
    observed = {.geometryObserved = true};
    transition.advance(observed, start);
    transition.advance(observed, start);
    transition.advance(observed, start);
    transition.advance(observed, start);
    observed.modeObserved = true;
    observed.displayObserved = true;
    test::require(transition.advance(observed, start) == Action::None &&
                      transition.outcome() == Outcome::Observed,
                  "fullscreen already on its target display needs no placement request");

    // Compositors may clamp geometry. The final mode still gets its request,
    // but lack of matching observation must never become a false success.
    transition.begin(false, start);
    observed = {};
    test::require(transition.advance(observed, start) == Action::LeaveFullscreen &&
                      transition.advance(observed, start) == Action::Restore &&
                      transition.advance(observed, start) == Action::ApplyGeometry,
                  "normal windows use the same ordered request sequence");
    test::require(transition.advance(observed, start + 249ms) == Action::None &&
                      transition.advance(observed, start + 250ms) == Action::ApplyMode,
                  "geometry grace period bounds waiting before final mode");
    observed.modeObserved = true;
    observed.displayObserved = true;
    test::require(transition.advance(observed, start + 251ms) == Action::None &&
                      transition.outcome() == Outcome::Pending,
                  "non-routing transition never emits display route");
    transition.advance(observed, start + 4999ms);
    test::require(transition.wakeAt() == start + 5s,
                  "poll deadline never extends beyond overall deadline");
    test::require(transition.advance(observed, start + 5s) == Action::None &&
                      transition.outcome() == Outcome::Unconfirmed &&
                      !transition.wakeAt(),
                  "ignored geometry ends unconfirmed without blocking forever");

    transition.begin(false, start);
    observed = {};
    transition.advance(observed, start);
    transition.advance(observed, start);
    transition.advance(observed, start);
    test::require(transition.advance(observed, start + 250ms) == Action::ApplyMode,
                  "late geometry does not prevent final mode submission");
    observed.modeObserved = true;
    observed.displayObserved = true;
    transition.advance(observed, start + 300ms);
    observed.geometryObserved = true;
    test::require(transition.advance(observed, start + 500ms) == Action::None &&
                      transition.outcome() == Outcome::Observed &&
                      !transition.wakeAt(),
                  "late geometry observation after final mode still confirms request");

    transition.begin(false, start);
    observed = {.geometryObserved = true};
    transition.advance(observed, start);
    transition.advance(observed, start);
    transition.advance(observed, start);
    transition.advance(observed, start);
    observed.geometryObserved = false;
    observed.modeObserved = true;
    observed.displayObserved = true;
    transition.advance(observed, start + 10ms);
    test::require(transition.outcome() == Outcome::Pending,
                  "transient earlier geometry match does not confirm current mismatch");

    transition.begin(false, start);
    observed = {.fullscreen = true};
    transition.advance(observed, start);
    test::require(transition.advance(observed, start + 5s) == Action::None &&
                      transition.outcome() == Outcome::Unconfirmed,
                  "denied fullscreen exit prevents later geometry requests");
    transition.begin(false, start);
    transition.fail();
    test::require(transition.outcome() == Outcome::Failed &&
                      !transition.wakeAt() &&
                      transition.advance({}, start) == Action::None,
                  "native failure cancels all later transition work");
    transition.begin(false, start);
    transition.cancel();
    test::require(transition.outcome() == Outcome::Cancelled &&
                      !transition.wakeAt() &&
                      transition.advance({}, start) == Action::None,
                  "superseded transition cannot issue stale requests");
    transition.begin(false, start + 10s);
    test::require(transition.outcome() == Outcome::Pending &&
                      transition.advance({}, start + 10s) == Action::LeaveFullscreen,
                  "new request resets terminal state and overall deadline");
    transition.begin(true, start + 11s);
    transition.advance({}, start + 11s);
    transition.advance({}, start + 11s);
    transition.advance({}, start + 11s);
    transition.begin(false, start + 12s);
    observed = {.geometryObserved = true,
                .modeObserved = true,
                .displayObserved = true};
    test::require(
        transition.advance(observed, start + 12s) == Action::LeaveFullscreen &&
            transition.advance(observed, start + 12s) == Action::Restore &&
            transition.advance(observed, start + 12s) == Action::ApplyGeometry &&
            transition.advance(observed, start + 12s) == Action::ApplyMode &&
            transition.advance(observed, start + 12s) == Action::None &&
            transition.outcome() == Outcome::Observed,
        "superseding pending request restarts ordering and drops stale display route");
  });
}
