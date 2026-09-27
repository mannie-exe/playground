#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/content/Rectangle.hpp>
#include <ui/controls/Composite.hpp>
#include <ui/controls/Slider.hpp>

using namespace playground;

namespace {
auto content() { return std::make_unique<ui::Rectangle>(ui::RectangleProps{}); }

std::vector<ui::ChoiceItem> choices() {
  std::vector<ui::ChoiceItem> v;
  v.push_back({"one", "One", content()});
  v.push_back({"two", "Two", content()});
  v.push_back({"off", "Off", content(), false});
  return v;
}
} // namespace

int main() {
  return test::run([] {
    ui::Checkbox check{content(), {.allowMixed = true, .name = "Check"}};
    int changes{};
    auto c = check.onValueChanged([&](auto) { ++changes; });
    check.performAction(ui::Activate{}, ui::ActionSource::Keyboard);
    test::require(check.props().checked == ui::CheckState::On && changes == 1,
                  "toggle activation");
    check.performAction(ui::SetChecked{ui::CheckState::Mixed},
                        ui::ActionSource::Assistive);
    test::require(check.semanticState().checked == ui::CheckState::Mixed,
                  "mixed state semantics");
    check.applyPatch(
        {.checked = Patch<ui::CheckState>::set(ui::CheckState::Off)});
    test::require(changes == 2, "programmatic toggle silent");
    ui::Switch sw{content()};
    test::require(sw.performAction(ui::SetChecked{ui::CheckState::Mixed},
                                   ui::ActionSource::Assistive) ==
                      ui::ActionResult::Unavailable,
                  "switch rejects mixed");
    ui::RadioGroup radios{choices(), {.selected = "one", .required = true}};
    test::require(radios.performAction(ui::SelectItem{"off"},
                                       ui::ActionSource::Assistive) ==
                      ui::ActionResult::Unavailable,
                  "disabled option refused");
    radios.performAction(ui::SelectItem{"two"}, ui::ActionSource::Keyboard);
    test::require(radios.selectionProps().selected == "two",
                  "single keyed selection");
    bool rejected{};
    try {
      radios.applySelectionPatch(
          {.selected = Patch<std::optional<std::string>>::set(std::nullopt)});
    } catch (const std::invalid_argument &) {
      rejected = true;
    }
    test::require(rejected && radios.selectionProps().selected == "two",
                  "required patch rejection atomic");
    ui::UIRoot root;
    auto select = std::make_unique<ui::Select>(
        content(), choices(), ui::SelectionProps{.selected = "one"});
    auto *selector = select.get();
    int selections{};
    auto selected =
        selector->onSelectionChanged([&](std::string) { ++selections; });
    root.setContent(std::move(select));
    root.flushLayout({300, 200});
    selector->performAction(ui::SetExpanded{true}, ui::ActionSource::Assistive);
    test::require(selector->isExpanded(), "select opens");
    root.flushLayout({300, 200});
    root.requestFocus(selector->children()[1]->id());
    ui::UIEvent confirm{.type = ui::EventType::KeyDown,
                        .logicalKey = ui::Key::Enter};
    root.dispatch(confirm);
    test::require(!selector->isExpanded() && selections == 0 &&
                      selector->children()[0]->hasFocus(),
                  "confirming unchanged selection closes and restores focus "
                  "without a false value-change notification");
    selector->setExpanded(true);
    selector->performAction(ui::SelectItem{"two"}, ui::ActionSource::Program);
    test::require(!selector->isExpanded() &&
                      selector->selectionProps().selected == "two",
                  "selection closes select");
    auto disclosure = std::make_unique<ui::Disclosure>(content(), content());
    auto *d = disclosure.get();
    root.setContent(std::move(disclosure));
    root.flushLayout({300, 200});
    d->performAction(ui::Activate{}, ui::ActionSource::Assistive);
    test::require(d->expansionProps().expanded &&
                      d->children()[1]->visibility() == ui::Visibility::Visible,
                  "disclosure retains expanded body");
    std::vector<ui::TabItem> items;
    items.push_back({"a", "A", content(), content()});
    items.push_back({"b", "B", content(), content()});
    auto tabs = std::make_unique<ui::Tabs>(std::move(items));
    auto *t = tabs.get();
    root.setContent(std::move(tabs));
    root.flushLayout({300, 200});
    t->performAction(ui::SelectItem{"b"}, ui::ActionSource::Keyboard);
    test::require(t->children()[1]->visibility() == ui::Visibility::Collapsed &&
                      t->children()[2]->visibility() == ui::Visibility::Visible,
                  "tabs retain only active panel layout");
    auto field = std::make_unique<ui::Field>(
        std::make_unique<ui::Button>(), content(), content(),
        ui::FieldProps{.label = "Name", .description = "Help"});
    auto *f = field.get();
    root.setContent(std::move(field));
    root.flushLayout({300, 200});
    auto snapshot = root.semanticSnapshot();
    bool named{};
    for (auto &n : snapshot.nodes)
      if (n.id == f->control().id()) {
        named = n.state.description.name == "Name" &&
                n.state.description.description == "Help";
      }
    test::require(named, "field relationships resolved after children attach");
    ui::Status status{content(), "Ready"};
    status.setMessage("Done");
    test::require(status.semanticState().description.role ==
                          ui::SemanticRole::Status &&
                      status.message() == "Done",
                  "status announcement state");
    ui::Tooltip tip{content(), "Help"};
    test::require(!tip.isOpen() && !tip.isFocusable(),
                  "tooltip passive and initially closed");
    tip.setOpen(true);
    test::require(tip.isOpen(), "tooltip author controlled visibility");
    ui::ProgressBar progress{{.range = {5, 0, 10, 1}}};
    test::require(progress.semanticState().range->value == 5 &&
                      progress.semanticState().readOnly,
                  "read-only progress");
  });
}
