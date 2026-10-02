#include <algorithm>
#include <format>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <demo2d/Demo2DUI.hpp>
#include <layout/Breakpoints.hpp>
#include <ui/collections/ScrollView.hpp>
#include <ui/containers/Flow.hpp>
#include <ui/content/Vector.hpp>
#include <ui/controls/ChoiceStepper.hpp>
#include <ui/controls/Composite.hpp>
#include <ui/controls/Groups.hpp>
#include <ui/controls/Meter.hpp>
#include <ui/controls/NumberStepper.hpp>
#include <ui/controls/Slider.hpp>
#include <ui/controls/Surfaces.hpp>
#include <ui/controls/TextField.hpp>

namespace playground::demo2d {
namespace {
class ControlsShowcase final : public ui::ZStack {
  AssetRegistry &_assets;
  FontHandle _font;

  const layout::BreakpointSet<bool> _sidePanel{
      {.rules = {{{.minimum = {800, 0}}, true, "wide"}}, .fallback = false}};

  ui::ScrollView *_controls{};
  ui::ScrollView *_information{};
  ui::Text *_statusText{};
  ui::Status *_status{};
  ui::Dialog *_dialog{};
  ui::Tooltip *_tooltip{};
  ui::Button *_tooltipTrigger{};
  std::vector<ui::Connection> _connections;

  std::unique_ptr<ui::Text> label(std::string value,
                                  ui::TextRole role = ui::TextRole::Label) {
    return std::make_unique<ui::Text>(
        _assets, ui::TextProps{.value = std::move(value),
                               .font = _font,
                               .wrap = ui::TextWrap::AvailableInlineSize,
                               .textRole = role});
  }

  void report(std::string value) {
    _status->setMessage(value);
    _statusText->applyPatch(
        {.value = Patch<std::string>::set(std::move(value))});
    _information->setOffset({});
  }

  auto button(std::string name) {
    auto value = std::make_unique<ui::Button>(label(name), ui::ButtonProps{});
    value->setControlLayout(ui::ControlLayout::Choice);
    value->setSemanticProps({.name = std::move(name)});
    return value;
  }

  auto icon(const SVGDocumentHandle &source) {
    auto node = std::make_unique<ui::Vector>(
        _assets,
        ui::VectorProps{.source = source,
                        .colorTreatment = ui::ColorTreatment::Adaptive},
        layout::BoxProps{.width = layout::SizeRule::fixed(24),
                         .height = layout::SizeRule::fixed(24)});
    node->setSemanticProps({.exposure = ui::SemanticExposure::HiddenSubtree});
    return node;
  }

  std::vector<ui::ChoiceItem> choices() {
    std::vector<ui::ChoiceItem> values;
    values.push_back({"first", "First", label("First", ui::TextRole::Value)});
    values.push_back(
        {"second", "Second", label("Second", ui::TextRole::Value)});
    values.push_back({"disabled", "Disabled",
                      label("Disabled option", ui::TextRole::Value), false});
    return values;
  }

protected:
  layout::MeasureResult
  measureContent(ui::MeasureContext &,
                 const layout::SizeConstraints &offered) override {
    // This gallery fills a viewport. Measure scroll children only against their
    // final panes, not a provisional full width that would clamp their offsets.
    const auto preferred = *config::viewPolicy.preferredWindowSize;
    return {offered.clamp(
        {static_cast<float>(preferred.x), static_cast<float>(preferred.y)})};
  }

  void arrangeChildren(ui::ArrangeContext &context,
                       math::Rect bounds) override {
    const auto area = math::inset(bounds, math::Insets::all(16));
    const bool side = _sidePanel.select(bounds.size);
    const float width = side ? std::min(300.f, area.w()) : area.w();
    const float limit = side ? area.h() : area.h() * 0.45f;
    const auto measured = _information->measure(
        context, {layout::AxisConstraints::tight(width), {0, limit}});
    const float height = std::min(limit, measured.size.height);
    const float gap = std::min(16.f, side ? std::max(0.f, area.w() - width)
                                          : std::max(0.f, area.h() - height));
    _information->arrange(context,
                          math::rect(side ? area.right() - width : area.x(),
                                     area.y(), width, height));
    const auto controls =
        side ? math::rect(area.x(), area.y(), area.w() - width - gap, area.h())
             : math::rect(area.x(), area.y() + height + gap, area.w(),
                          area.h() - height - gap);
    _controls->measure(context, layout::SizeConstraints::tight(controls.size));
    _controls->arrange(context, controls);
    _tooltip->setAnchor(_tooltipTrigger->id());
  }

public:
  ControlsShowcase(AssetRegistry &assets, const ViewResources &resources,
                   const ViewProps &props)
      : ZStack{{layout::Alignment::stretch()}}, _assets{assets},
        _font{std::make_shared<const Font>(
            resources.font->cloneWith({.size = 18.f}))} {
    setPaintStyle({.themeBackground = true});
    auto information = std::make_unique<ui::VStack>(
        layout::StackProps{
            .gap = 12, .childrenAlignment = layout::CrossAlignment::Stretch},
        layout::BoxProps{.padding = math::Insets::all(12)});
    information->setPaintStyle({.themeBackground = true});
    information->append(
        label("Demo 2D — controls and accessibility", ui::TextRole::Heading));
    auto statusText =
        label("Ready — try the controls below", ui::TextRole::Body);
    _statusText = statusText.get();
    auto status = std::make_unique<ui::Status>(
        std::move(statusText), "Ready — try the controls below");
    _status = status.get();
    information->append(std::move(status));
    information->append(
        label("Tab / Shift+Tab: focus • arrows: navigate or adjust • "
              "Enter/Space: activate • Esc: settings; Ctrl/Cmd+Shift+M: menu. "
              "Gamepad D-pad and south/east buttons work too.",
              ui::TextRole::Body));
    auto column = std::make_unique<ui::VStack>(
        layout::StackProps{
            .gap = 16, .childrenAlignment = layout::CrossAlignment::Stretch},
        layout::BoxProps{.padding = math::Insets::all(4)});
    auto preview = makeDemo2DPreview(assets, resources, props);
    auto box = preview->boxProps();
    box.height = layout::SizeRule::fixed(180);
    preview->setBoxProps(box);
    column->append(std::move(preview));

    column->append(label("Buttons and switches"));
    auto buttons = std::make_unique<ui::Flow>(
        layout::FlowProps{.itemGap = 12, .lineGap = 12});
    auto push = button("Push button");
    _connections.push_back(
        push->onActivate([this] { report("Button activated"); }));
    buttons->append(std::move(push));
    auto disabled = button("Disabled button");
    disabled->setEnabled(false);
    buttons->append(std::move(disabled));
    auto toggle = std::make_unique<ui::ToggleButton>(
        label("Toggle"), ui::ToggleProps{.name = "Toggle"});
    _connections.push_back(toggle->onValueChanged([this](auto v) {
      report(v == ui::CheckState::On ? "Toggle on" : "Toggle off");
    }));
    buttons->append(std::move(toggle));
    column->append(std::move(buttons));
    auto check = std::make_unique<ui::Checkbox>(
        label("Checkbox (starts mixed)"),
        ui::ToggleProps{.checked = ui::CheckState::Mixed,
                        .allowMixed = true,
                        .name = "Checkbox"});
    _connections.push_back(check->onValueChanged([this](auto v) {
      report(v == ui::CheckState::On ? "Checkbox checked"
                                     : "Checkbox unchecked");
    }));
    column->append(std::move(check));
    auto sw = std::make_unique<ui::Switch>(label("Switch"),
                                           ui::ToggleProps{.name = "Switch"});
    _connections.push_back(sw->onValueChanged([this](auto v) {
      report(v == ui::CheckState::On ? "Switch on" : "Switch off");
    }));
    column->append(std::move(sw));

    auto group = std::make_unique<ui::FieldGroup>(
        "Text and numeric fields",
        layout::StackProps{
            .gap = 12, .childrenAlignment = layout::CrossAlignment::Stretch});
    group->append(label("Text and numeric fields"));
    auto text = std::make_unique<ui::TextField>(
        ui::TextFieldProps{.font = _font,
                           .required = true,
                           .placeholder = "Type Unicode text",
                           .textRole = ui::TextRole::Value},
        "Editable text");
    _connections.push_back(text->onValueChanged([this](std::string v) {
      report("Text changed (" + std::to_string(v.size()) + " UTF-8 bytes)");
    }));
    _connections.push_back(
        text->onCommit([this](std::string) { report("Text committed"); }));
    group->append(std::make_unique<ui::Field>(
        std::move(text), label("Name (required)"),
        label("Try selection, clipboard, undo, emoji, combining accents, RTL "
              "and an IME.",
              ui::TextRole::Caption),
        ui::FieldProps{.label = "Name", .description = "Required plain text"}));
    group->append(std::make_unique<ui::TextField>(
        ui::TextFieldProps{.font = _font,
                           .editing = {.password = true},
                           .name = "Password",
                           .placeholder = "Password — not copied or announced",
                           .textRole = ui::TextRole::Value}));
    group->append(std::make_unique<ui::TextField>(
        ui::TextFieldProps{.font = _font,
                           .editing = {.readOnly = true},
                           .name = "Read-only text",
                           .textRole = ui::TextRole::Value},
        "Read-only text can still be selected and copied"));
    group->append(std::make_unique<ui::TextArea>(
        ui::TextFieldProps{.font = _font,
                           .name = "Multiline notes",
                           .textRole = ui::TextRole::Value},
        "Multiple lines, wrapping and selection.\nUnicode: café, á, שלום, "
        "مرحبا.\nEnter adds a line; Ctrl/Cmd+Enter commits.",
        layout::BoxProps{.height = layout::SizeRule::fixed(130)}));
    auto number = std::make_unique<ui::NumberField>(
        ui::TextFieldProps{.font = _font,
                           .name = "Number from zero to one hundred",
                           .textRole = ui::TextRole::Value},
        ui::NumberFieldProps{.range = {20, 0, 100, 1}, .integer = true});
    _connections.push_back(number->onNumberChanged(
        [this](double v) { report(std::format("Number: {}", v)); }));
    group->append(std::move(number));
    column->append(std::move(group));

    column->append(label("Ranges and progress"));
    auto progress = std::make_unique<ui::ProgressBar>(
        ui::ProgressProps{.range = {30, 0, 100, 1}, .name = "Slider progress"});
    auto *meter = progress.get();
    auto slider = std::make_unique<ui::Slider>(
        ui::SliderProps{.range = {30, 0, 100, 1}, .name = "Progress value"});
    _connections.push_back(slider->onValueChanged([this, meter](double v) {
      auto p = meter->props();
      p.range.value = v;
      meter->setProps(p);
      report(std::format("Slider: {}", v));
    }));
    column->append(std::move(slider));
    column->append(std::move(progress));
    auto readout = label("3", ui::TextRole::Value);
    auto *readoutPtr = readout.get();
    auto stepper = std::make_unique<ui::NumberStepper>(
        std::move(readout), icon(resources.removeIcon), icon(resources.addIcon),
        ui::NumberStepperProps{
            .value = 3, .maximum = 10, .name = "NumberStepper"});
    _connections.push_back(stepper->onValueChanged([this, readoutPtr](int v) {
      readoutPtr->applyPatch(
          {.value = Patch<std::string>::set(std::to_string(v))});
      report(std::format("NumberStepper: {}", v));
    }));
    column->append(std::move(stepper));

    auto choiceLabel = label("First", ui::TextRole::Value);
    auto *choiceText = choiceLabel.get();
    auto choiceStepper = std::make_unique<ui::ChoiceStepper>(
        std::move(choiceLabel), choices(),
        ui::SelectionProps{.selected = "first", .name = "Choice stepper"});
    _connections.push_back(
        choiceStepper->onSelectionChanged([this, choiceText](std::string key) {
          choiceText->applyPatch({.value = Patch<std::string>::set(key)});
          report("Choice: " + key);
        }));
    column->append(label("Choice stepper"));
    column->append(std::move(choiceStepper));
    column->append(label("Independent choices"));
    column->append(std::make_unique<ui::CheckboxGroup>(
        choices(), ui::ToggleGroupProps{.name = "Checkbox group"}));
    column->append(label("Managed utilization (example)"));
    column->append(std::make_unique<ui::Meter>(
        ui::MeterProps{.value = 75,
                       .maximum = 100,
                       .warning = 70,
                       .critical = 90,
                       .name = "Example utilization",
                       .unit = "MiB"}));
    std::vector<ui::AccordionItem> sections;
    sections.push_back({"first", label("Accordion: first"),
                        label("Single-expansion content")});
    sections.push_back({"second", label("Accordion: second"),
                        label("Opening this closes the first section")});
    column->append(std::make_unique<ui::Accordion>(std::move(sections)));
    column->append(std::make_unique<ui::Popover>(
        label("Open popover"),
        label("Interactive anchored content; Escape dismisses")));
    column->append(std::make_unique<ui::TooltipTrigger>(
        std::make_unique<ui::Button>(label("Focus or hover for help")),
        label("Shared tooltip timing", ui::TextRole::Caption),
        "Shared tooltip timing"));

    column->append(label("Radio group"));
    auto radio = std::make_unique<ui::RadioGroup>(
        choices(), ui::SelectionProps{.selected = "first",
                                      .required = true,
                                      .name = "Radio group"});
    _connections.push_back(radio->onSelectionChanged(
        [this](std::string v) { report("Radio: " + v); }));
    column->append(std::move(radio));
    column->append(label("List box"));
    auto list = std::make_unique<ui::ListBox>(
        choices(), ui::SelectionProps{.selected = "first", .name = "List box"});
    _connections.push_back(list->onSelectionChanged(
        [this](std::string v) { report("List: " + v); }));
    column->append(std::move(list));
    auto selectLabel = label("Select: First", ui::TextRole::Value);
    auto *selectedLabel = selectLabel.get();
    auto select = std::make_unique<ui::Select>(
        std::move(selectLabel), choices(),
        ui::SelectionProps{.selected = "first", .name = "Select an option"});
    _connections.push_back(
        select->onSelectionChanged([this, selectedLabel](std::string v) {
          selectedLabel->applyPatch(
              {.value = Patch<std::string>::set("Select: " + v)});
          report("Select: " + v);
        }));
    column->append(std::move(select));

    column->append(std::make_unique<ui::Disclosure>(
        label("Expand disclosure"),
        label("Retained disclosure content. Collapsing removes it from layout "
              "and accessibility, without destroying it."),
        ui::ExpansionProps{.name = "Disclosure"}));
    std::vector<ui::TabItem> tabs;
    tabs.push_back(
        {"overview", "Overview", label("Overview"),
         label("Overview panel — try Left/Right or Home/End on the tabs.")});
    tabs.push_back({"details", "Details", label("Details"),
                    std::make_unique<ui::TextField>(
                        ui::TextFieldProps{.font = _font,
                                           .name = "Details note",
                                           .textRole = ui::TextRole::Value},
                        "Retained when you switch tabs")});
    column->append(std::make_unique<ui::Tabs>(
        std::move(tabs), ui::SelectionProps{.name = "Showcase tabs"}));
    auto menu = std::make_unique<ui::MenuList>(
        choices(),
        ui::SelectionProps{.selected = "first", .name = "Command menu"});
    _connections.push_back(
        menu->onInvoked([this](std::string v, ui::ActionSource) {
          report("Menu command: " + v);
        }));
    column->append(label("Command menu — arrows move, Enter invokes"));
    column->append(std::move(menu));

    auto tooltip = std::make_unique<ui::Tooltip>(
        label("Tooltip: passive help, not another keyboard stop.",
              ui::TextRole::Caption),
        "Passive help text");
    auto *hint = tooltip.get();
    _tooltip = hint;
    auto tooltipToggle = button("Show/hide tooltip");
    _tooltipTrigger = tooltipToggle.get();
    _connections.push_back(
        tooltipToggle->onActivate([hint] { hint->setOpen(!hint->isOpen()); }));
    column->append(std::move(tooltipToggle));
    column->append(std::move(tooltip));
    auto open = button("Open modal dialog");
    _connections.push_back(open->onActivate(
        [this] { _dialog->applyPatch({.open = Patch<bool>::set(true)}); }));
    column->append(std::move(open));

    auto scroll = std::make_unique<ui::ScrollView>(
        std::move(column), ui::ScrollProps{.sizing = ui::ScrollSizing::Fill});
    _controls = scroll.get();
    append(std::move(scroll));
    auto info = std::make_unique<ui::ScrollView>(
        std::move(information),
        ui::ScrollProps{.sizing = ui::ScrollSizing::Content});
    _information = info.get();
    append(std::move(info));
    auto dialogContent = std::make_unique<ui::VStack>(layout::StackProps{
        .gap = 16, .childrenAlignment = layout::CrossAlignment::Stretch});
    dialogContent->append(
        label("Modal dialog — background controls cannot receive input. Close "
              "to restore the opener.",
              ui::TextRole::Body));
    dialogContent->append(std::make_unique<ui::TextField>(
        ui::TextFieldProps{.font = _font,
                           .name = "Dialog input",
                           .placeholder = "Try typing here",
                           .textRole = ui::TextRole::Value}));
    auto close = button("Close dialog");
    _connections.push_back(close->onActivate([this] {
      _dialog->performAction(ui::SetExpanded{false}, ui::ActionSource::Program);
    }));
    dialogContent->append(std::move(close));
    auto dialog = std::make_unique<ui::Dialog>(
        std::make_unique<ui::ScrollView>(
            std::move(dialogContent),
            ui::ScrollProps{.sizing = ui::ScrollSizing::Content}),
        ui::DialogProps{.name = "Controls test dialog",
                        .description = "A modal scope with focus restoration"},
        layout::BoxProps{.width = layout::SizeRule::fill(),
                         .maxWidth = 440.f,
                         .padding = math::Insets::all(24)});
    _dialog = dialog.get();
    _connections.push_back(dialog->onDismissed(
        [this] { report("Dialog dismissed; opener focus restored"); }));
    append(std::move(dialog),
           {.alignmentOverride = layout::Alignment::center()});
  }
};
} // namespace

std::unique_ptr<ui::Node> makeDemo2DUI(AssetRegistry &assets,
                                       const ViewResources &resources,
                                       const ViewProps &props) {
  return std::make_unique<ControlsShowcase>(assets, resources, props);
}
} // namespace playground::demo2d
