#include <cmath>
#include <limits>
#include <stdexcept>

#include <input/InputMap.hpp>
#include <support/Test.hpp>

using namespace playground::input;
using playground::test::require;

namespace {
void send(InputMap &map, int key, float value, bool ui = false,
          bool repeat = false) {
  InputEvent e{{ControlKind::Key, key, 0}, value, repeat};
  routeInputEvent(map, e, false, [=] { return ui; });
}
} // namespace

int main() {
  return playground::test::run([] {
    InputMap map;
    const auto game =
        map.addContext({.name = "game"}, {{.action = "jump", .code = 1},
                                          {.action = "jump", .code = 2}});
    send(map, 1, 1);
    require(map.takeFrameSnapshot()["jump"].pressed, "frame press");
    require(!map.takeFrameSnapshot()["jump"].pressed, "frame edges consumed");
    require(map.takeTickSnapshot()["jump"].pressed,
            "tick independently latched");
    require(!map.takeTickSnapshot()["jump"].pressed,
            "catchup does not repeat edges");
    send(map, 2, 1);
    send(map, 1, 0);
    require(map.takeFrameSnapshot()["jump"].held,
            "second binding retains hold");
    send(map, 2, 0);
    require(map.takeFrameSnapshot()["jump"].released, "last binding releases");
    send(map, 1, 1, true);
    require(!map.takeFrameSnapshot()["jump"].held,
            "UI consumption blocks gameplay");
    send(map, 1, 1);
    require(!map.takeFrameSnapshot()["jump"].pressed,
            "blocked hold requires neutral");
    send(map, 1, 0);
    send(map, 1, 1);
    map.setEnabled(game, false);
    auto canceled = map.takeTickSnapshot()["jump"];
    require(canceled.canceled && !canceled.released && !canceled.pressed,
            "disable cancels pending press");
    map.setEnabled(game, true);
    send(map, 1, 1, false, true);
    require(!map.takeFrameSnapshot()["jump"].held,
            "enable does not synthesize held press");
    send(map, 1, 0);
    send(map, 1, 1);
    map.rebind(game, {{.action = "jump", .code = 1}});
    require(map.takeFrameSnapshot()["jump"].canceled,
            "rebind cancels old hold");
    send(map, 1, 0);
    const auto modal = map.addContext(
        {.name = "modal", .priority = 10, .stage = InputStage::BeforeUI},
        {{.action = "accept", .code = 1}});
    send(map, 1, 1);
    auto snapshot = map.takeFrameSnapshot();
    require(snapshot["accept"].pressed && !snapshot["jump"].held,
            "priority consumes");
    map.removeContext(modal);
    send(map, 1, 1);
    require(!map.takeFrameSnapshot()["jump"].held,
            "removing modal does not leak hold");
    send(map, 1, 0);
    send(map, 1, 1);
    send(map, 1, 0);
    snapshot = map.takeTickSnapshot();
    require(snapshot["jump"].pressed && snapshot["jump"].released &&
                !snapshot["jump"].held,
            "quick tap survives zero-tick frame");
    map.cancelAll();
    require(!map.takeTickSnapshot()["jump"].pressed,
            "focus cancellation clears edge backlog");

    InputMap axes;
    axes.addContext({.name = "axes"}, {{.action = "move",
                                        .kind = ActionKind::Vector,
                                        .code = 1,
                                        .contribution = {1, 0}},
                                       {.action = "move",
                                        .kind = ActionKind::Vector,
                                        .code = 2,
                                        .contribution = {0, 1}},
                                       {.action = "throttle",
                                        .kind = ActionKind::Axis,
                                        .control = ControlKind::GamepadAxis,
                                        .code = 0,
                                        .deadZone = 0.2f}});
    send(axes, 1, 1);
    send(axes, 2, 1);
    const auto diagonal = axes.takeFrameSnapshot()["move"].value;
    require(std::abs(std::hypot(diagonal.x, diagonal.y) - 1) < 1e-6,
            "diagonal normalized");
    InputEvent analog{{ControlKind::GamepadAxis, 0, 42}, 0.6f};
    axes.route(analog, InputStage::BeforeUI);
    axes.route(analog, InputStage::AfterUI);
    require(std::abs(axes.takeFrameSnapshot()["throttle"].value.x - 0.5f) <
                1e-6,
            "deadzone remaps active range");
    axes.cancelDevice(ControlKind::GamepadAxis, 42);
    require(axes.takeTickSnapshot()["throttle"].canceled,
            "device removal cancels");
    bool rejected{};
    try {
      axes.route({{}, std::numeric_limits<float>::infinity()},
                 InputStage::BeforeUI);
    } catch (const std::invalid_argument &) {
      rejected = true;
    }
    require(rejected, "nonfinite input rejected");
    rejected = false;
    try {
      axes.addContext({.name = "bad"},
                      {{.action = "move", .kind = ActionKind::Button}});
    } catch (const std::invalid_argument &) {
      rejected = true;
    }
    require(rejected, "action kinds cannot conflict");

    InputMap old, next;
    send(old, 9, 1);
    next.addContext({.name = "next"}, {{.action = "open", .code = 9}});
    next.inheritHeld(old);
    send(next, 9, 1);
    require(!next.takeTickSnapshot()["open"].held,
            "app switch suppresses inherited holds");
    send(next, 9, 0);
    send(next, 9, 1);
    require(next.takeTickSnapshot()["open"].pressed,
            "fresh press in next app works");
    send(next, 9, 0, true);
    auto consumedRelease = next.takeTickSnapshot()["open"];
    require(consumedRelease.canceled && !consumedRelease.released,
            "consumed release cancels instead of triggering release action");

    InputMap priorities;
    priorities.addContext(
        {.name = "first", .priority = 5, .stage = InputStage::BeforeUI},
        {{.action = "first", .code = 1}});
    const auto second = priorities.addContext(
        {.name = "second", .priority = 5, .stage = InputStage::BeforeUI},
        {{.action = "second", .code = 1}});
    bool routedUI{};
    routeInputEvent(priorities, {{ControlKind::Key, 1}, 1}, false, [&] {
      routedUI = true;
      return false;
    });
    auto selected = priorities.takeFrameSnapshot();
    require(selected["first"].pressed && !selected["second"].held && !routedUI,
            "equal priority preserves insertion order and skips UI");
    send(priorities, 1, 0);
    auto changed = priorities.contextProps(second);
    changed.priority = 6;
    priorities.setContextProps(second, changed);
    send(priorities, 1, 1);
    selected = priorities.takeFrameSnapshot();
    require(selected["second"].pressed && !selected["first"].held,
            "priority changes apply");
    require(priorities.bindings(second).front().action == "second",
            "bindings inspectable for rebinding");
  });
}
