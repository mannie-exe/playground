#include <memory>

#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/controls/Button.hpp>

using namespace playground;

int main() {
  return test::run([] {
    ui::UIRoot root;
    auto button = std::make_unique<ui::Button>();
    auto *b = button.get();
    int activations{};
    auto connection = b->onActivate([&] { ++activations; });
    root.setContent(std::move(button));
    root.flushLayout({100, 50});
    auto send = [&](ui::EventType type, math::Point2 point, int which = 1) {
      ui::UIEvent event{
          .type = type, .position = point, .pointer = 1, .button = which};
      root.dispatch(event);
    };
    send(ui::EventType::PointerDown, {5, 5}, 2);
    send(ui::EventType::PointerUp, {5, 5}, 2);
    test::require(!b->isPressed() && activations == 0,
                  "secondary click does not activate");
    send(ui::EventType::PointerDown, {5, 5});
    send(ui::EventType::PointerMove, {150, 5});
    test::require(b->isPressed() && !b->isHovered(),
                  "drag outside retains capture but clears hover");
    send(ui::EventType::PointerMove, {5, 5});
    send(ui::EventType::PointerUp, {5, 5});
    test::require(activations == 1 && !b->isPressed(),
                  "return inside then release activates once");
    send(ui::EventType::PointerDown, {5, 5});
    send(ui::EventType::PointerCancel, {5, 5});
    send(ui::EventType::PointerUp, {5, 5});
    test::require(activations == 1 && !b->isPressed(),
                  "cancel cannot activate");
    for (auto key : {ui::Key::Space, ui::Key::Enter}) {
      root.requestFocus(b->id());
      ui::UIEvent down{.type = ui::EventType::KeyDown, .logicalKey = key};
      root.dispatch(down);
      test::require(b->isPressed(), "keyboard down arms control");
      ui::UIEvent repeat{
          .type = ui::EventType::KeyDown, .logicalKey = key, .repeat = true};
      root.dispatch(repeat);
      const int before = activations;
      ui::UIEvent up{.type = ui::EventType::KeyUp, .logicalKey = key};
      root.dispatch(up);
      ui::UIEvent duplicate{.type = ui::EventType::KeyUp, .logicalKey = key};
      root.dispatch(duplicate);
      test::require(activations == before + 1 && !b->isPressed(),
                    "matching release activates only once despite repeat");
    }
    send(ui::EventType::PointerDown, {5, 5});
    b->setEnabled(false);
    test::require(!b->isPressed() && !b->hasFocus(),
                  "disable cancels press and focus");
    b->applyPatch({.enabled = ui::Patch<bool>::reset()});
    test::require(b->isEnabled(), "enabled Reset uses true baseline");
  });
}
