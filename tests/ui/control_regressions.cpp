#include <app/TTFGuard.hpp>
#include <support/AssetRegistry.hpp>
#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/content/Rectangle.hpp>
#include <ui/controls/ChoiceStepper.hpp>
#include <ui/controls/Groups.hpp>
#include <ui/controls/Meter.hpp>
#include <ui/controls/NumberStepper.hpp>
#include <ui/controls/Slider.hpp>
#include <ui/controls/Surfaces.hpp>
#include <ui/controls/TextField.hpp>
#include <ui/controls/ToastHost.hpp>

using namespace playground;

namespace {
auto content() {
  return std::make_unique<ui::Rectangle>(
      ui::RectangleProps{},
      layout::BoxProps{.width = layout::SizeRule::fixed(40),
                       .height = layout::SizeRule::fixed(30)});
}

auto choices() {
  std::vector<ui::ChoiceItem> items;
  items.push_back({"a", "A", content()});
  items.push_back({"b", "B", content()});
  return items;
}

void key(ui::UIRoot &root, ui::Key value) {
  ui::UIEvent e{.type = ui::EventType::KeyDown, .logicalKey = value};
  root.dispatch(e);
}

class FocusGroup : public ui::VStack {
public:
  unsigned entered{}, left{};

protected:
  void onDefaultEvent(ui::UIEvent &e) override {
    entered += e.type == ui::EventType::FocusWithinGained;
    left += e.type == ui::EventType::FocusWithinLost;
  }
};
} // namespace

int main() {
  return test::run([] {
    TTFGuard ttf;
    AssetRegistry assets;
    auto font = assets.getFont({.path = std::string{PLAYGROUND_SOURCE_DIR} +
                                        "/assets/fonts/LBRITE.TTF",
                                .style = {.size = 18}});
    unsigned failed{};
    const auto check = [&](const char *name, auto body) {
      std::cout << name << '\n';
      failed += test::run(body);
    };
    check("numeric publication", [&] {
      ui::NumberField field{{.font = font, .editing = {.maximumBytes = 1}},
                            {.range = {1, 0, 100, 1}}};
      unsigned notifications{};
      auto c = field.onNumberChanged([&](double) { ++notifications; });
      test::rejects<std::length_error>(
          [&] {
            field.performAction(ui::SetValue{10}, ui::ActionSource::Program);
          },
          "over-capacity number is rejected");
      test::require(field.acceptedNumber() == 1 &&
                        field.model().value() == "1" && notifications == 0 &&
                        !field.draftDirty(),
                    "failed numeric action must preserve all accepted state");
      test::rejects<std::length_error>(
          [&] { field.setNumberProps({.range = {10, 0, 100, 1}}); },
          "numeric props validate display capacity");
      test::require(field.acceptedNumber() == 1,
                    "failed props preserve number");
      test::rejects<std::invalid_argument>(
          [&] {
            field.setCodec(
                {.parse =
                     [](std::string_view) {
                       return ui::NumberParse{ui::ParseState::Valid, 1};
                     },
                 .format = [](double) { return std::string(1, char(0xff)); }});
          },
          "invalid UTF-8 formatter is rejected");
      field.performAction(ui::SetValue{2}, ui::ActionSource::Program);
      test::require(field.acceptedNumber() == 2 && field.model().value() == "2",
                    "failed codec must preserve previous codec");
    });
    check("dirty text refresh", [&] {
      ui::TextField field{{.font = font, .editing = {.maximumBytes = 3}},
                          "old"};
      field.performAction(ui::TextSelection{0, 3}, ui::ActionSource::Program);
      field.performAction(ui::ReplaceSelectedText{"new"},
                          ui::ActionSource::Keyboard);
      test::rejects<std::length_error>([&] { field.refreshValue("long"); },
                                       "dirty refresh must validate capacity");
      test::rejects<std::invalid_argument>(
          [&] { field.refreshValue(std::string(1, char(0xff))); },
          "dirty refresh must validate Unicode");
      test::require(field.model().value() == "new",
                    "failed refresh preserves draft");
      field.revertDraft();
      test::require(field.model().value() == "old",
                    "failed refresh preserves revert target");
    });
    check("select relationships", [&] {
      ui::UIRoot root;
      auto select = std::make_unique<ui::Select>(
          content(), choices(), ui::SelectionProps{.selected = "a"});
      auto *control = select.get();
      auto field =
          std::make_unique<ui::Field>(std::move(select), content(), content());
      root.setContent(std::move(field));
      root.flushLayout({300, 200});
      auto expected = control->focusTarget().semanticProps();
      auto state = control->focusTarget().semanticState();
      test::require(expected.labelledBy && expected.describedBy &&
                        state.description.labelledBy == expected.labelledBy &&
                        state.description.describedBy == expected.describedBy,
                    "Select semantic target must retain Field relationships");
      control->applySelectionPatch(
          {.selected = Patch<std::optional<std::string>>::set("b")});
      test::require(
          control->focusTarget().semanticState().description.labelledBy ==
              expected.labelledBy,
          "selection refresh preserves label relationship");
    });
    check("popover edit dismissal", [&] {
      ui::UIRoot root;
      auto editor =
          std::make_unique<ui::NumberField>(ui::TextFieldProps{.font = font});
      auto *value = editor.get();
      auto popover =
          std::make_unique<ui::Popover>(content(), std::move(editor));
      auto *surface = popover.get();
      root.setContent(std::move(popover));
      root.flushLayout({400, 300});
      surface->setOpen(true);
      root.flushLayout({400, 300});
      value->requestFocus();
      value->setValue("-");
      key(root, ui::Key::Escape);
      test::require(surface->isOpen() && !value->draftDirty(),
                    "Escape reverts an edit before closing nonmodal surface");
      key(root, ui::Key::Escape);
      test::require(!surface->isOpen(), "next Escape dismisses clean surface");
    });
    check("tooltip semantic focus target", [&] {
      auto select = std::make_unique<ui::Select>(content(), choices());
      ui::TooltipTrigger chooser{std::move(select), content(), "Choice help"};
      test::require(
          chooser.focusTarget().semanticState().description.description ==
              "Choice help",
          "tooltip description reaches Select trigger semantics");
      auto field =
          std::make_unique<ui::NumberField>(ui::TextFieldProps{.font = font});
      auto *editor = field.get();
      ui::TooltipTrigger numeric{std::move(field), content(), "Number help"};
      editor->showValidation(ui::ValidationIssue{"range", "Invalid number"});
      const auto description =
          numeric.focusTarget().semanticState().description.description;
      test::require(description.find("Number help") != std::string::npos &&
                        description.find("Invalid number") != std::string::npos,
                    "validation retains essential tooltip help");
    });
    check("toolbar composites", [&] {
      ui::UIRoot root;
      auto stack = std::make_unique<ui::VStack>();
      auto toolbar = std::make_unique<ui::Toolbar>();
      auto label = content();
      auto *labelPtr = label.get();
      toolbar->append(std::move(label));
      auto numeric =
          std::make_unique<ui::NumberStepper>(content(), content(), content());
      auto *number = numeric.get();
      toolbar->append(std::move(numeric));
      auto select = std::make_unique<ui::Select>(content(), choices());
      auto *selection = select.get();
      toolbar->append(std::move(select));
      auto disabled = std::make_unique<ui::Button>(content());
      disabled->setEnabled(false);
      toolbar->append(std::move(disabled));
      stack->append(std::move(toolbar));
      auto outside = std::make_unique<ui::Button>(content());
      auto *after = outside.get();
      stack->append(std::move(outside));
      root.setContent(std::move(stack));
      root.flushLayout({600, 200});
      root.focusNext();
      test::require(number->hasFocus() && !labelPtr->isFocusable(),
                    "toolbar skips decoration and enters logical control");
      root.focusNext();
      test::require(after->hasFocus(),
                    "Tab exits toolbar past composite children");
      selection->focusTarget().requestFocus();
      root.focusNext();
      test::require(after->hasFocus(),
                    "clicked logical entry remains one Tab stop");
    });
    check("toolbar hidden targets and modal scope", [&] {
      ui::UIRoot root;
      auto toolbar = std::make_unique<ui::Toolbar>();
      auto select = std::make_unique<ui::Select>(content(), choices());
      select->setVisibility(ui::Visibility::Collapsed);
      toolbar->append(
          std::make_unique<ui::Field>(std::move(select), content()));
      auto button = std::make_unique<ui::Button>(content());
      auto *available = button.get();
      toolbar->append(std::move(button));
      root.setContent(std::move(toolbar));
      root.flushLayout({400, 300});
      root.focusNext();
      test::require(available->hasFocus(),
                    "hidden composed targets do not block toolbar entry");

      auto owner = std::make_unique<ui::Toolbar>();
      owner->append(std::make_unique<ui::Button>(content()));
      auto panel = std::make_unique<ui::VStack>();
      auto a = std::make_unique<ui::Button>(content());
      auto *first = a.get();
      auto b = std::make_unique<ui::Button>(content());
      auto *second = b.get();
      panel->append(std::move(a));
      panel->append(std::move(b));
      owner->append(std::make_unique<ui::Dialog>(
          std::move(panel), ui::DialogProps{.open = true}));
      root.setContent(std::move(owner));
      root.flushLayout({400, 300});
      first->requestFocus();
      root.focusNext();
      test::require(second->hasFocus(),
                    "modal navigation excludes outer toolbar grouping");
    });
    check("typed control invariants", [&] {
      ui::AlertDialog alert{content()};
      ui::Dialog &dialog = alert;
      dialog.applyPatch({.modal = Patch<bool>::set(false)});
      test::require(dialog.props().modal,
                    "AlertDialog stays modal through base mutation");
      ui::CheckboxGroup checks{choices()};
      ui::ToggleGroup &group = checks;
      group.setProps({.selected = {"a"}, .multiple = false});
      group.performAction(ui::SelectItem{"b"}, ui::ActionSource::Keyboard);
      test::require(
          group.props().selected.size() == 2,
          "CheckboxGroup retains independent selection through base mutation");
    });
    check("positioned popup width", [&] {
      ui::ArrangeContext context;
      auto anchor = content();
      anchor->arrange(context, math::rect(10, 10, 100, 30));
      ui::Popup popup{content(),
                      {.open = true, .position = math::Point2{150, 80}}};
      popup.present(context, math::rect(0, 0, 400, 300), anchor.get());
      test::require(popup.bounds().w() == 100 && popup.bounds().x() == 150 &&
                        popup.bounds().y() == 84,
                    "explicit position retains anchor-matched width");
      popup.present(context, math::rect(0, 0, 80, 100), anchor.get());
      test::require(popup.bounds().w() == 64 && popup.bounds().x() >= 8 &&
                        popup.bounds().right() <= 72,
                    "anchor-matched width is clamped to usable viewport");
      popup.applyPopupPatch(
          {.width = Patch<ui::PopupWidth>::set(ui::PopupWidth::Content)});
      popup.present(context, math::rect(0, 0, 400, 300), anchor.get());
      test::require(popup.bounds().w() == 40,
                    "content width is independent of anchor width");
      popup.applyPopupPatch({.width = Patch<ui::PopupWidth>::reset()});
      popup.present(context, math::rect(0, 0, 400, 300), nullptr);
      test::require(popup.bounds().w() == 40 && popup.bounds().x() == 150,
                    "positioned popup without anchor uses content width");
    });
    check("toast replacement placement", [&] {
      ui::UIRoot root;
      auto host = std::make_unique<ui::ToastHost>(
          [](const auto &, auto) { return content(); });
      auto *toasts = host.get();
      root.setContent(std::move(host));
      toasts->post({"first", "First", 0});
      toasts->post({"retained", "Retained", 0});
      root.update(.01);
      const auto retained = toasts->children()[1]->id();
      const layout::StackPlacement placement{.margin = math::Insets::all(7),
                                             .crossAlignmentOverride =
                                                 layout::CrossAlignment::End};
      toasts->setPlacement(retained, placement);
      toasts->post({"first", "Replacement", 0});
      root.update(.01);
      test::require(toasts->children()[1]->id() == retained &&
                        toasts->children()[0]->semanticProps().name ==
                            "Replacement",
                    "replacement retains queue order and unaffected child");
      test::require(toasts->placementOf(retained) == placement &&
                        toasts->placementInParent(0) ==
                            layout::StackPlacement{},
                    "toast placement follows its child during reordering");
    });
    check("toast hover and focus", [&] {
      ui::UIRoot root;
      auto host = std::make_unique<ui::ToastHost>([](const auto &, auto) {
        return std::make_unique<ui::Button>(content());
      });
      auto *toasts = host.get();
      root.setContent(std::move(host));
      toasts->post({"one", "Message", .2});
      root.update(.01);
      root.flushLayout({200, 100});
      auto *button = toasts->children().front().get();
      button->requestFocus();
      ui::UIEvent move{.type = ui::EventType::PointerMove,
                       .position = {10, 10}};
      root.dispatch(move);
      root.requestFocus({});
      root.update(.5);
      test::require(toasts->size() == 1,
                    "focus departure must not clear stationary toast hover");
      move.position = {300, 200};
      root.dispatch(move);
      root.update(.5);
      test::require(toasts->size() == 0, "leaving hover resumes timeout");
    });
    check("cancellation source", [&] {
      ui::NumberField field{{.font = font}};
      ui::ChangeContext observed;
      auto c = field.onInteractionFinished(
          [&](ui::ChangeContext value) { observed = value; });
      field.setValue("-");
      field.performAction(ui::CancelEdit{}, ui::ActionSource::Assistive);
      test::require(observed.source == ui::ActionSource::Assistive &&
                        observed.reason == ui::ChangeReason::Cancel,
                    "assistive cancellation must preserve its source");
      field.setValue("-");
      field.revertDraft();
      test::require(observed.source == ui::ActionSource::Program,
                    "programmatic revert must not claim keyboard input");
    });
    check("choice and expansion sources", [&] {
      ui::ChoiceStepper stepper{content(), choices(), {.selected = "a"}};
      ui::ActionSource source{};
      auto c = stepper.onSelectionEdited(
          [&](std::string, ui::ActionSource s) { source = s; });
      stepper.stepBy(1, ui::ActionSource::Assistive);
      test::require(source == ui::ActionSource::Assistive,
                    "ChoiceStepper preserves selection source through Select");
      std::vector<ui::AccordionItem> items;
      items.push_back({"one", content(), content()});
      ui::Accordion accordion{std::move(items)};
      auto a = accordion.onExpandedEdited(
          [&](auto, ui::ActionSource s) { source = s; });
      accordion.children().front()->performAction(ui::SetExpanded{true},
                                                  ui::ActionSource::Pointer);
      test::require(source == ui::ActionSource::Pointer,
                    "Accordion preserves Disclosure interaction source");
    });
    check("slider terminal states", [&] {
      ui::UIRoot root;
      auto slider = std::make_unique<ui::Slider>(
          ui::SliderProps{.range = {0, 0, 100, 10}});
      auto *control = slider.get();
      ui::ChangeContext edited, finished;
      unsigned completions{};
      auto a = control->onValueEdited(
          [&](double, ui::ChangeContext c) { edited = c; });
      auto b = control->onInteractionFinished([&](ui::ChangeContext c) {
        finished = c;
        ++completions;
      });
      root.setContent(std::move(slider));
      root.flushLayout({100, 24});
      ui::UIEvent down{.type = ui::EventType::PointerDown,
                       .position = {8, 12},
                       .button = 1,
                       .source = ui::ActionSource::Pointer};
      root.dispatch(down);
      ui::UIEvent up{.type = ui::EventType::PointerUp,
                     .position = {50, 12},
                     .button = 1,
                     .source = ui::ActionSource::Pointer};
      root.dispatch(up);
      test::require(edited.reason == ui::ChangeReason::Drag &&
                        finished.reason == ui::ChangeReason::Drag &&
                        completions == 1,
                    "release-only value change retains drag provenance and "
                    "completes once");
      down.handled = false;
      root.dispatch(down);
      control->applyPatch({.readOnly = Patch<bool>::set(true)});
      test::require(!control->isDragging() &&
                        finished.reason == ui::ChangeReason::Cancel &&
                        completions == 2,
                    "read-only transition cancels an active drag explicitly");
      control->applyPatch({.readOnly = Patch<bool>::set(false),
                           .snapToStep = Patch<bool>::set(false)});
      down.position = {39, 12};
      down.handled = false;
      root.dispatch(down);
      test::require(control->props().range.value > 36 &&
                        control->props().range.value < 38,
                    "unsnapped pointer motion preserves continuous value");
    });
    check("meter threshold semantics", [&] {
      ui::Meter meter{
          {.value = 85, .maximum = 100, .warning = 80, .critical = 95}};
      test::require(meter.semanticState().description.value->find("warning") !=
                        std::string::npos,
                    "warning threshold must have a noncolor description");
      meter.setProps(
          {.value = 95, .maximum = 100, .warning = 80, .critical = 95});
      test::require(meter.semanticState().description.value->find("critical") !=
                        std::string::npos,
                    "critical boundary must have a distinct description");
    });
    check("focus within boundaries", [&] {
      ui::UIRoot root;
      auto group = std::make_unique<FocusGroup>();
      auto *container = group.get();
      auto a = std::make_unique<ui::Button>(content());
      auto *first = a.get();
      auto b = std::make_unique<ui::Button>(content());
      auto *second = b.get();
      group->append(std::move(a));
      group->append(std::move(b));
      root.setContent(std::move(group));
      root.flushLayout({200, 100});
      first->requestFocus();
      second->requestFocus();
      test::require(container->entered == 1 && container->left == 0,
                    "moving within a group does not cross its focus boundary");
      root.requestFocus({});
      test::require(container->left == 1,
                    "leaving group crosses boundary once");
    });
    test::require(!failed, "control review regression cases failed");
  });
}
