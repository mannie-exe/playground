#include <cmath>

#include <input/ControlsSettings.hpp>
#include <input/ViewControlSession.hpp>
#include <support/Test.hpp>

using namespace playground;
using test::require;
using namespace input;

int main() {
  return test::run([] {
    InputMap map;
    map.addContext({.name = "view"}, {{.action = "move", .code = 1},
                                      {.action = "look",
                                       .kind = ActionKind::Delta,
                                       .control = ControlKind::PointerMotion,
                                       .contribution = {1, 1}}});
    ViewControlProps props;
    props.devices.push_back({DeviceKind::Gamepad, 7});
    props.actions[0] = {"move"};
    props.actions[1] = {"look"};
    ViewControlSession session{map, props};
    runtime::ActivationLifetime lifetime;
    lifetime.activate();
    int locks{};
    auto acquire = [&] {
      ++locks;
      return std::shared_ptr<void>{new int, [&](void *p) {
                                     delete static_cast<int *>(p);
                                     --locks;
                                   }};
    };
    ViewEngagement engage{
        lifetime.token(), {DeviceKind::Mouse, 0}, true, true, true, acquire};
    require(
        !session.accept(ViewChannel::Movement, {DeviceKind::Keyboard, 0}, 1),
        "hover cannot grant gameplay");
    engage.focused = false;
    require(!session.engage(engage) && locks == 0, "background cannot capture");
    engage.focused = true;
    require(session.engage(engage) && locks == 1,
            "explicit eligible engagement acquires lease");
    require(session.accept(ViewChannel::Movement, {DeviceKind::Keyboard, 0}, 1),
            "keyboard owns movement");
    require(session.accept(ViewChannel::Look, {DeviceKind::Gamepad, 7}, .8f),
            "gamepad look cooperates with keyboard movement");
    require(!session.accept(ViewChannel::Look, {DeviceKind::Mouse, 0}, .1f),
            "noise cannot take over");
    require(!session.accept(ViewChannel::Look, {DeviceKind::Mouse, 0}, 1, true,
                            false, true),
            "warp cannot take over");
    require(session.accept(ViewChannel::Look, {DeviceKind::Mouse, 0}, 1),
            "intentional motion takes over");
    require(!session.accept(ViewChannel::Look, {DeviceKind::Gamepad, 7}, .8f),
            "displaced losing stick cannot fight ownership");
    session.accept(ViewChannel::Look, {DeviceKind::Gamepad, 7}, 0);
    require(session.accept(ViewChannel::Look, {DeviceKind::Gamepad, 7}, .8f),
            "neutral stick rearms takeover");
    const auto old = session.state().generation;
    session.synchronize(false, {});
    require(locks == 0 && session.state().status == ControlStatus::Suspended,
            "focus loss releases native ownership");
    session.synchronize(true, {});
    require(
        !session.accept(ViewChannel::Movement, {DeviceKind::Keyboard, 0}, 1),
        "focus return does not reengage");
    require(session.engage(engage), "deliberate reengagement succeeds");
    session.release(old);
    require(locks == 1, "stale release cannot cancel newer engagement");
    require(
        !session.accept(ViewChannel::Movement, {DeviceKind::Keyboard, 0}, 1),
        "held movement suppressed through reengagement");
    session.accept(ViewChannel::Movement, {DeviceKind::Keyboard, 0}, 0);
    require(session.accept(ViewChannel::Movement, {DeviceKind::Keyboard, 0}, 1),
            "neutral input rearms");
    session.removeDevice({DeviceKind::Gamepad, 7});
    require(!session.accept(ViewChannel::Look, {DeviceKind::Gamepad, 8}, 1),
            "another controller is not auto assigned");
    session.release();
    require(locks == 0, "release retires lease");
    engage.acquirePointer = [] { return std::shared_ptr<void>{}; };
    require(!session.engage(engage) &&
                session.state().reason == ControlReason::Acquisition,
            "failed native acquisition is observable");
    engage.pointer = false;
    engage.device = {DeviceKind::Keyboard, 0};
    require(session.engage(engage), "nonmouse engagement needs no lease");
    lifetime.deactivate();
    session.synchronize(true, {});
    require(session.state().reason == ControlReason::App,
            "activation expiry revokes control");
    InputMap takeover;
    takeover.addContext({.name = "source"},
                        {{.action = "travel", .code = 1},
                         {.action = "travel",
                          .control = ControlKind::GamepadButton,
                          .code = 1,
                          .device = 7}});
    routeInputEvent(takeover, {{ControlKind::Key, 1, 0}, 1}, false,
                    [] { return false; });
    routeInputEvent(takeover, {{ControlKind::GamepadButton, 1, 7}, 1}, false,
                    [] { return false; });
    const std::array<std::string_view, 1> names{"travel"};
    takeover.cancelDeviceActions(names, ControlKind::Key, 0);
    require(takeover.takeFrameSnapshot()["travel"].held &&
                takeover.physicalValue({ControlKind::Key, 1, 0}) == 1,
            "takeover preserves winning holds and raw losing state for neutral "
            "rearm");
    routeInputEvent(takeover, {{ControlKind::Key, 1, 42}, 1}, false,
                    [] { return false; });
    require(takeover.physicalValue(ControlKind::Key, 1) == 1,
            "grouped keyboards retain nonzero native IDs");
    takeover.cancelDeviceActions(names, ControlKind::Key, std::nullopt);
    require(takeover.physicalValue({ControlKind::Key, 1, 42}) == 1,
            "grouped cancellation preserves physical rearm observation");
    InputMap teardown;
    teardown.addContext({.name = "teardown"}, {{.action = "move", .code = 1}});
    {
      ViewControlSession scoped(teardown, props);
      routeInputEvent(teardown, {{ControlKind::Key, 1, 0}, 1}, false,
                      [] { return false; });
    }
    require(!teardown.takeFrameSnapshot()["move"].held,
            "session destruction cancels its configured holds");
    ControlsSettings preferences;
    preferences.validate();
    require(stickResponse({.1f, .1f}, preferences) == math::Vec2f{},
            "paired stick has radial neutral zone");
    const auto diagonal = stickResponse({1, 1}, preferences);
    require(std::abs(std::hypot(diagonal.x, diagonal.y) - 1) < 1e-6 &&
                diagonal.x == diagonal.y,
            "radial response preserves diagonal direction and caps magnitude");
    preferences.stickSaturationThreshold = preferences.stickInnerDeadZone;
    test::rejects([&] { preferences.validate(); },
                  "contradictory radial limits rejected");
  });
}
