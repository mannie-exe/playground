#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/content/Rectangle.hpp>
#include <ui/controls/Stepper.hpp>

using namespace playground;

int main() {
  return test::run([] {
    const auto content = [] {
      return std::make_unique<ui::Rectangle>(ui::RectangleProps{});
    };
    auto readout = content();
    auto decreaseLabel = content();
    auto increaseLabel = content();
    auto *readoutNode = readout.get();
    auto *decreaseNode = decreaseLabel.get();
    auto *increaseNode = increaseLabel.get();
    auto stepper = std::make_unique<ui::Stepper>(
        std::move(readout), std::move(decreaseLabel), std::move(increaseLabel),
        ui::StepperProps{
            .value = 1, .minimum = 0, .maximum = 2, .name = "Count"});
    auto *value = stepper.get();
    int notifications{};
    auto connection = value->onValueChanged([&](int next) {
      ++notifications;
      test::require(next == value->value(),
                    "state committed before notification");
    });
    ui::UIRoot root;
    root.setContent(std::move(stepper));
    root.flushLayout({240, 48});
    root.focusNext();
    auto &row = *value->children()[0];
    auto &decrease = dynamic_cast<ui::Button &>(*row.children()[0]);
    auto &increase = dynamic_cast<ui::Button &>(*row.children()[2]);
    test::require(row.children()[1].get() == readoutNode &&
                      decrease.children()[0].get() == decreaseNode &&
                      increase.children()[0].get() == increaseNode,
                  "constructor readout/decrease/increase arguments map to "
                  "center/left/right slots");
    test::require(decrease.hasFocus(),
                  "stepper starts with a usable tab target");
    ui::UIEvent left{.type = ui::EventType::KeyDown,
                     .logicalKey = ui::Key::Left};
    root.dispatch(left);
    test::require(value->value() == 0 && notifications == 1 && left.handled,
                  "arrow decrements and notifies once");
    test::require(!decrease.isEnabled() && increase.hasFocus(),
                  "limit preserves focus inside stepper");
    value->stepBy(-1);
    test::require(notifications == 1, "bound does not emit duplicate change");
    ui::UIEvent down{.type = ui::EventType::KeyDown,
                     .logicalKey = ui::Key::Enter};
    ui::UIEvent up{.type = ui::EventType::KeyUp, .logicalKey = ui::Key::Enter};
    root.dispatch(down);
    root.dispatch(up);
    test::require(value->value() == 1 && notifications == 2,
                  "button keyboard activation increments");
    value->applyPatch({.enabled = Patch<bool>::set(false)});
    value->stepBy(1);
    test::require(value->value() == 1 && !increase.isEnabled() &&
                      !decrease.isEnabled(),
                  "disabled is inert");
    test::require(value->semanticProps().value == "1",
                  "semantic value tracks control");
    const auto before = value->props();
    bool rejected{};
    try {
      value->applyPatch({.maximum = Patch<int>::set(0)});
    } catch (const std::invalid_argument &) {
      rejected = true;
    }
    test::require(rejected && value->props() == before,
                  "invalid patch does not mutate");
    value->setProps({.value = std::numeric_limits<int>::max() - 1,
                     .minimum = std::numeric_limits<int>::min(),
                     .maximum = std::numeric_limits<int>::max(),
                     .step = std::numeric_limits<int>::max()});
    test::require(notifications == 2,
                  "programmatic setters do not echo user actions");
    value->stepBy(1);
    test::require(value->value() == std::numeric_limits<int>::max(),
                  "large increment saturates without overflow");
    value->stepBy(-1);
    test::require(value->value() == 0,
                  "large decrement uses widened arithmetic");
    value->setProps({.value = 4, .minimum = 4, .maximum = 4});
    test::require(!decrease.isEnabled() && !increase.isEnabled(),
                  "single-value range disables both buttons");
    rejected = false;
    try {
      ui::Stepper invalid{content(), content(), content(), {.step = 0}};
    } catch (const std::invalid_argument &) {
      rejected = true;
    }
    test::require(rejected, "zero step rejected");
    auto throwing = value->onValueChanged(
        [](int) { throw std::runtime_error("observer"); });
    value->setProps({.value = 0, .maximum = 2});
    rejected = false;
    try {
      value->stepBy(1);
    } catch (const std::runtime_error &) {
      rejected = true;
    }
    test::require(rejected && value->value() == 1,
                  "throwing notification leaves committed valid state");
  });
}
