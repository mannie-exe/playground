#include <app/TTFGuard.hpp>
#include <support/AssetRegistry.hpp>
#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/content/Rectangle.hpp>
#include <ui/controls/ChoiceStepper.hpp>
#include <ui/controls/Form.hpp>
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

std::vector<ui::ChoiceItem> choices() {
  std::vector<ui::ChoiceItem> result;
  result.push_back({"low", "Low", content()});
  result.push_back({"disabled", "Disabled", content(), false});
  result.push_back({"high", "High", content()});
  return result;
}
} // namespace

int main() {
  return test::run([] {
    TTFGuard ttf;
    AssetRegistry assets;
    auto font = assets.getFont({.path = std::string{PLAYGROUND_SOURCE_DIR} +
                                        "/assets/fonts/LBRITE.TTF",
                                .style = {.size = 18}});
    ui::UIRoot root;
    auto editor = std::make_unique<ui::NumberField>(
        ui::TextFieldProps{.font = font, .name = "Scale"},
        ui::NumberFieldProps{.range = {50, 25, 100, 5}});
    auto *number = editor.get();
    root.setContent(std::move(editor));
    root.flushLayout({300, 60});
    number->requestFocus();
    number->setValue("72.5");
    test::require(number->commitDraft() && number->acceptedNumber() == 72.5,
                  "fractional input independent of step");
    number->adjustNumber(1,
                         {ui::ActionSource::Keyboard, ui::ChangeReason::Step});
    test::require(number->acceptedNumber() == 77.5,
                  "stepping uses accepted decimal");
    number->setValue("-");
    ui::Form form;
    auto registration = form.registerField("scale", number->handle());
    test::require(!form.commit() && number->acceptedNumber() == 77.5,
                  "incomplete form draft cannot publish");
    number->setValue("85");
    number->setNumberProps({.range = {60, 25, 100, 5}});
    test::require(number->model().value() == "85" && number->hasConflict(),
                  "refresh preserves draft and exposes conflict");
    number->revertDraft();
    test::require(number->model().value() == "60" && !number->draftDirty(),
                  "revert uses latest acknowledged number");
    number->setValidator([](double value) -> ui::ValidationResult {
      if (value > 80)
        return ui::ValidationIssue{"budget", "Budget is 80"};
      return {};
    });
    number->setValue("90");
    test::require(!number->commitDraft() && number->acceptedNumber() == 60,
                  "custom validation shared by commit");
    test::require(
        number->performAction(ui::SetValue{90}, ui::ActionSource::Assistive) ==
            ui::ActionResult::Unavailable,
        "assistive action uses custom validation");
    number->revertDraft();
    number->setValue("70");
    test::require(form.commit() && number->acceptedNumber() == 70,
                  "form validates and commits focused draft");
    test::require(ui::parseNumber("1e-").state == ui::ParseState::Incomplete &&
                      ui::parseNumber("garbagee").state ==
                          ui::ParseState::Invalid,
                  "incomplete exponent requires a valid numeric prefix");
    number->setCodec({.parse = ui::parseNumber, .format = [](double value) {
                        if (value == 75)
                          throw std::invalid_argument(
                              "unsupported display value");
                        return ui::formatNumber(value);
                      }});
    bool formatRejected{};
    try {
      number->performAction(ui::SetValue{75}, ui::ActionSource::Program);
    } catch (const std::invalid_argument &) {
      formatRejected = true;
    }
    test::require(formatRejected && number->acceptedNumber() == 70 &&
                      number->model().value() == "70",
                  "formatter failure preserves accepted value and draft");
    number->setCodec({});
    number->setValidator({}, ui::ValidationMode::OnSubmit);
    number->setValue("-");
    test::require(!number->commitDraft(
                      {ui::ActionSource::Keyboard, ui::ChangeReason::Blur}) &&
                      number->props().validationMessage.empty(),
                  "submit validation defers presentation, not rejection");
    test::require(!form.commit() && !number->props().validationMessage.empty(),
                  "submission presents deferred validation");
    number->setValue("-");
    ui::UIEvent escape{.type = ui::EventType::KeyDown,
                       .logicalKey = ui::Key::Escape};
    root.dispatch(escape);
    test::require(escape.handled && number->model().value() == "70",
                  "Escape reverts numeric edit");
    registration.disconnect();
    root.setContent(content());
    test::require(form.validate(),
                  "registration disconnect releases stale field");

    auto boundedEditor = std::make_unique<ui::NumberField>(
        ui::TextFieldProps{.font = font},
        ui::NumberFieldProps{.range = {100, 0, 100, 10}});
    auto *bounded = boundedEditor.get();
    auto numeric = std::make_unique<ui::NumberStepper>(
        std::move(boundedEditor), content(), content(),
        ui::NumberStepperProps{.value = 100, .maximum = 100, .step = 10});
    auto *numericPtr = numeric.get();
    root.setContent(std::move(numeric));
    root.flushLayout({300, 60});
    auto &increase = dynamic_cast<ui::Button &>(
        *numericPtr->children().front()->children()[2]);
    test::require(!increase.isEnabled(), "accepted bound disables adjustment");
    bounded->performAction(ui::TextSelection{0, 3}, ui::ActionSource::Keyboard);
    bounded->performAction(ui::ReplaceSelectedText{"50"},
                           ui::ActionSource::Keyboard);
    test::require(increase.isEnabled() && numericPtr->value() == 100,
                  "pending edit can adjust away from an accepted bound");
    increase.performAction(ui::Activate{}, ui::ActionSource::Assistive);
    test::require(numericPtr->value() == 60,
                  "button commits the pending number before stepping");
    numericPtr->applyPatch({.readOnly = Patch<bool>::set(true)});
    test::require(!increase.isEnabled() &&
                      bounded->performAction(ui::ReplaceSelectedText{"70"},
                                             ui::ActionSource::Assistive) ==
                          ui::ActionResult::Unavailable,
                  "composite read-only state reaches the actual editor");

    ui::ChoiceStepper choice{
        content(), choices(), {.selected = "low", .name = "Quality"}};
    unsigned changes{};
    auto changed = choice.onSelectionChanged([&](std::string key) {
      test::require(key == "high", "choice emits key");
      ++changes;
    });
    choice.stepBy(1);
    choice.stepBy(1);
    test::require(choice.selectionProps().selected == "high" && changes == 1,
                  "choice skips disabled and stops at bound");
    ui::MenuList menu{choices()};
    unsigned invoked{};
    auto invocation =
        menu.onInvoked([&](std::string key, ui::ActionSource source) {
          test::require(key == "low" && source == ui::ActionSource::Assistive,
                        "menu preserves command source");
          ++invoked;
        });
    menu.performAction(ui::SelectItem{"low"}, ui::ActionSource::Assistive);
    menu.performAction(ui::SelectItem{"low"}, ui::ActionSource::Assistive);
    test::require(invoked == 2 && !menu.selectionProps().selected,
                  "commands repeat without persistent selection");
    auto keyboardMenu = std::make_unique<ui::MenuList>(choices());
    auto *commands = keyboardMenu.get();
    std::string command;
    auto keyboardInvocation = commands->onInvoked(
        [&](std::string key, ui::ActionSource) { command = std::move(key); });
    root.setContent(std::move(keyboardMenu));
    root.flushLayout({300, 200});
    commands->requestFocus();
    ui::UIEvent enter{.type = ui::EventType::KeyDown,
                      .logicalKey = ui::Key::Enter};
    root.dispatch(enter);
    test::require(command == "low", "menu opens with first enabled command");
    ui::UIEvent down{.type = ui::EventType::KeyDown,
                     .logicalKey = ui::Key::Down};
    root.dispatch(down);
    commands->applySelectionPatch(
        {.name = Patch<std::string>::set("Commands")});
    enter.handled = enter.propagationStopped = enter.defaultPrevented = false;
    root.dispatch(enter);
    test::require(command == "high" && !commands->selectionProps().selected,
                  "menu retains active command across property refresh");
    test::require(dynamic_cast<ui::TextInputClient *>(commands) &&
                      root.inputClaims().keyboard,
                  "focused menu requests native text input and owns typing");
    ui::UIEvent preedit{.type = ui::EventType::TextEditing, .text = "lo"};
    root.dispatch(preedit);
    enter.handled = enter.propagationStopped = enter.defaultPrevented = false;
    command.clear();
    root.dispatch(enter);
    test::require(commands->textInputState().composing && command.empty(),
                  "IME preedit cannot invoke a command");
    ui::UIEvent typed{.type = ui::EventType::TextInput, .text = "lo"};
    root.dispatch(typed);
    enter.handled = enter.propagationStopped = enter.defaultPrevented = false;
    root.dispatch(enter);
    test::require(
        !commands->textInputState().composing && command == "low",
        "committed text navigates before explicit command invocation");
    ui::ToggleGroup toggles{choices(), {.selected = {"low"}, .required = true}};
    toggles.performAction(ui::SelectItem{"low"}, ui::ActionSource::Keyboard);
    test::require(toggles.props().selected == std::set<std::string>{"low"},
                  "required group retains last selection");
    toggles.performAction(ui::SelectItem{"high"}, ui::ActionSource::Keyboard);
    test::require(toggles.props().selected == std::set<std::string>{"high"},
                  "single group replaces selection");
    ui::CheckboxGroup checks{choices()};
    checks.performAction(ui::SetChecked{ui::CheckState::On},
                         ui::ActionSource::Program);
    test::require(checks.aggregate() == ui::CheckState::On &&
                      checks.props().selected.size() == 2,
                  "group aggregate excludes disabled options");
    checks.performAction(ui::SelectItem{"low"}, ui::ActionSource::Program);
    test::require(checks.aggregate() == ui::CheckState::Mixed,
                  "group exposes mixed state");
    std::vector<ui::AccordionItem> items;
    items.push_back({"a", content(), content()});
    items.push_back({"b", content(), content()});
    ui::Accordion accordion{std::move(items)};
    accordion.setExpanded({"a"});
    bool rejected{};
    try {
      accordion.setExpanded({"a", "b"});
    } catch (const std::invalid_argument &) {
      rejected = true;
    }
    test::require(rejected &&
                      accordion.expanded() == std::set<std::string>{"a"},
                  "accordion rejects invalid expansion atomically");
    ui::Meter meter{
        {.value = 120, .maximum = 100, .name = "Memory", .unit = "MiB"}};
    test::require(meter.semanticState().description.value->find("120") !=
                          std::string::npos &&
                      meter.semanticState().description.value->find(
                          "over limit") != std::string::npos,
                  "meter keeps actual over-limit reading");
    ui::ProgressBar progress{{.indeterminate = true}};
    test::require(!progress.semanticState().range,
                  "unknown completion is not zero progress");
    auto toolbar = std::make_unique<ui::Toolbar>("Tools");
    auto first = std::make_unique<ui::Button>(content());
    auto second = std::make_unique<ui::Button>(content());
    auto *firstPtr = first.get(), *secondPtr = second.get();
    toolbar->append(std::move(first));
    toolbar->append(std::move(second));
    root.setContent(std::move(toolbar));
    root.flushLayout({300, 60});
    root.focusNext();
    test::require(
        firstPtr->hasFocus() && secondPtr->isFocusable(),
        "toolbar enters first control without disabling other focus targets");
    ui::UIEvent right{.type = ui::EventType::KeyDown,
                      .logicalKey = ui::Key::Right};
    root.dispatch(right);
    test::require(secondPtr->hasFocus(), "toolbar arrows move internal focus");
    auto toasts = std::make_unique<ui::ToastHost>(
        [](const ui::ToastMessage &, std::function<void()>) {
          return content();
        },
        2);
    auto *host = toasts.get();
    root.setContent(std::move(toasts));
    root.flushLayout({300, 200});
    host->post({"a", "First", .2});
    host->post({"a", "Replacement", .2});
    host->post({"b", "Persistent", 0});
    host->post({"c", "Third", 0});
    test::require(host->size() == 2,
                  "notifications replace identities and remain bounded");
    root.update(.1);
    root.flushLayout({300, 200});
    host->dismiss("c");
    host->post({"timed", "Timed", .2});
    root.update(.1);
    root.update(.3);
    test::require(host->size() == 1,
                  "notification timeout uses root scheduler");
    auto focusToasts = std::make_unique<ui::ToastHost>(
        [](const ui::ToastMessage &, std::function<void()>) {
          return std::make_unique<ui::Button>(content());
        });
    auto *notifications = focusToasts.get();
    root.setContent(std::move(focusToasts));
    notifications->post({"focused", "Keep focus", .2});
    root.update(.1);
    root.flushLayout({300, 200});
    auto *retained = notifications->children().front().get();
    retained->requestFocus();
    notifications->post({"other", "Another notification", 0});
    root.update(.1);
    root.flushLayout({300, 200});
    root.update(.5);
    test::require(retained->hasFocus() && notifications->size() == 2,
                  "new notifications preserve focus and pause active timeouts");
    root.requestFocus({});
    root.update(.5);
    test::require(notifications->size() == 1,
                  "notification timeout resumes after interaction");

    auto nestedNumber = std::make_unique<ui::NumberField>(
        ui::TextFieldProps{.font = font},
        ui::NumberFieldProps{.range = {5, 0, 10, 1}});
    auto *nested = nestedNumber.get();
    auto dialog = std::make_unique<ui::Dialog>(std::move(nestedNumber),
                                               ui::DialogProps{.open = true});
    auto *dialogPtr = dialog.get();
    root.setContent(std::move(dialog));
    root.flushLayout({500, 300});
    nested->requestFocus();
    nested->setValue("-");
    ui::UIEvent cancel{.type = ui::EventType::KeyDown,
                       .logicalKey = ui::Key::Escape};
    root.dispatch(cancel);
    test::require(dialogPtr->props().open && !nested->draftDirty(),
                  "first Escape cancels edit inside modal");
    cancel.handled = cancel.propagationStopped = cancel.defaultPrevented =
        false;
    root.dispatch(cancel);
    test::require(!dialogPtr->props().open, "second Escape closes modal");
    auto owner = std::make_unique<ui::Button>(content());
    auto *ownerPtr = owner.get();
    auto help = std::make_unique<ui::TooltipTrigger>(
        std::move(owner), content(), "Help",
        ui::TooltipTiming{.showDelay = .2, .hideDelay = .1});
    auto *helpPtr = help.get();
    root.setContent(std::move(help));
    root.flushLayout({300, 100});
    ownerPtr->requestFocus();
    root.update(.25);
    root.flushLayout({300, 100});
    auto *tip = dynamic_cast<ui::Tooltip *>(helpPtr->children().back().get());
    test::require(tip && tip->isOpen(),
                  "tooltip opens on descendant keyboard focus after delay");
    cancel.handled = cancel.propagationStopped = cancel.defaultPrevented =
        false;
    root.dispatch(cancel);
    test::require(!tip->isOpen(), "Escape dismisses passive help");
  });
}
