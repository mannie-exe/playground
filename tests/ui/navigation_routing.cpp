#include <array>
#include <memory>
#include <utility>

#include <input/InputMap.hpp>
#include <support/Test.hpp>
#include <ui/TextEdit.hpp>
#include <ui/UIRoot.hpp>
#include <ui/containers/Stack.hpp>
#include <ui/controls/Button.hpp>
#include <ui/controls/Composite.hpp>

using namespace playground;

namespace {
class Editor final : public ui::Button, public ui::TextInputClient {
public:
  ui::TextInputState textInputState() const override { return {}; }
};
} // namespace

int main() {
  return test::run([] {
    ui::UIRoot root;
    root.setContent(std::make_unique<ui::VStack>());
    root.flushLayout({320, 240});
    input::InputMap actions;
    actions.addContext({.name = "camera", .stage = input::InputStage::AfterUI},
                       {{.action = "orbit", .code = 1}});
    const auto dispatch = [&](ui::Key key) {
      actions.setUIClaims(root.inputClaims());
      ui::UIEvent event{.type = ui::EventType::KeyDown, .logicalKey = key};
      input::routeInputEvent(actions, {{input::ControlKind::Key, 1}, 1}, false,
                             [&] {
                               root.dispatch(event);
                               actions.setUIClaims(root.inputClaims());
                               return event.handled || event.propagationStopped;
                             });
      const auto snapshot = actions.takeFrameSnapshot();
      input::routeInputEvent(actions, {{input::ControlKind::Key, 1}, 0}, false,
                             [] { return false; });
      actions.takeFrameSnapshot();
      return std::pair{event.handled, snapshot["orbit"].pressed};
    };
    for (const auto key : std::array{ui::Key::Left, ui::Key::Right, ui::Key::Up,
                                     ui::Key::Down, ui::Key::Tab})
      test::require(dispatch(key) == std::pair{false, true},
                    "decorative UI does not swallow application orbit keys");

    auto stack = std::make_unique<ui::VStack>();
    auto first = std::make_unique<ui::Button>();
    auto second = std::make_unique<ui::Button>();
    first->setBoxProps({.height = layout::SizeRule::fixed(30)});
    second->setBoxProps({.height = layout::SizeRule::fixed(30)});
    auto *a = first.get(), *b = second.get();
    stack->append(std::move(first));
    stack->append(std::move(second));
    root.setContent(std::move(stack));
    root.flushLayout({320, 240});
    test::require(dispatch(ui::Key::Down) == std::pair{true, false} &&
                      a->hasFocus(),
                  "first arrow focuses UI rather than activating camera");
    test::require(dispatch(ui::Key::Down) == std::pair{true, false} &&
                      b->hasFocus(),
                  "directional focus movement consumes application binding");
    test::require(dispatch(ui::Key::Down) == std::pair{true, false} &&
                      b->hasFocus(),
                  "focus boundary does not leak arrow to the camera");
    a->setEnabled(false);
    b->setEnabled(false);
    test::require(dispatch(ui::Key::Right) == std::pair{false, true},
                  "disabled controls cannot claim directional navigation");

    root.setContent(std::make_unique<ui::Dialog>(
        std::make_unique<ui::VStack>(),
        ui::DialogProps{.open = true, .name = "Blocking"}));
    root.flushLayout({320, 240});
    test::require(dispatch(ui::Key::Right) == std::pair{true, false},
                  "empty modal still prevents background camera navigation");
    test::require(
        !dispatch(ui::Key::Unknown).second,
        "modal reserves unhandled gameplay letters as well as arrows");
    auto editor = std::make_unique<Editor>();
    auto *editing = editor.get();
    root.setContent(std::move(editor));
    root.flushLayout({320, 240});
    root.requestFocus(editing->id());
    test::require(root.inputClaims().keyboard && !root.inputClaims().gamepad,
                  "editor reserves keyboard, not all devices");
    test::require(!dispatch(ui::Key::Unknown).second,
                  "ordinary typing never triggers AfterUI actions");
    editing->setEnabled(false);
    test::require(!root.inputClaims().keyboard,
                  "disabled editor releases ownership");
    editing->setEnabled(true);
    test::require(!root.inputClaims().keyboard,
                  "reenabling does not steal focus");
    root.requestFocus(editing->id());
    test::require(root.inputClaims().keyboard,
                  "enabled editor refreshes cached claims");
    editing->capturePointer(7);
    test::require(
        root.inputClaims().owns({input::ControlKind::MouseButton, 1, 7}),
        "pointer capture participates independently of cached focus");
    editing->releasePointer(7);
    test::require(
        !root.inputClaims().owns({input::ControlKind::MouseButton, 1, 7}),
        "released capture is not retained in cached claims");
    editing->setVisibility(ui::Visibility::Hidden);
    test::require(!root.inputClaims().keyboard, "hidden editor releases claim");
    root.setContent(std::make_unique<ui::VStack>());
    root.flushLayout({320, 240});
    test::require(!root.inputClaims().keyboard && !root.inputClaims().gamepad,
                  "replacement does not retain editor or modal reservations");
  });
}
