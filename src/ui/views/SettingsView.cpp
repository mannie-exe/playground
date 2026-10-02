#include <format>

#include <ui/collections/ScrollView.hpp>
#include <ui/containers/Stack.hpp>
#include <ui/controls/Choice.hpp>
#include <ui/controls/Composite.hpp>
#include <ui/controls/NumberStepper.hpp>
#include <ui/views/SettingsView.hpp>

namespace playground::ui {
using namespace rendering;

SettingsView::SettingsView(AssetRegistry &assets, FontHandle font,
                           GraphicsSettings settings,
                           SettingsViewActions actions)
    : SettingsPanel{layout::BoxProps{.maxWidth = 960}}, _assets{assets},
      _font{std::move(font)}, _actions{std::move(actions)}, _draft{settings},
      _applied{settings} {
  setControlLayout(ControlLayout::Settings);
  setPaintStyle({.themeBackground = true});
  setSemanticProps(
      {.role = SemanticRole::Group, .name = "Playground settings"});
  build();
}

std::unique_ptr<Text> SettingsView::text(std::string value, TextRole role) {
  return std::make_unique<Text>(_assets,
                                TextProps{.value = std::move(value),
                                          .font = _font,
                                          .wrap = TextWrap::AvailableInlineSize,
                                          .textRole = role});
}

void SettingsView::arrangeChildren(ArrangeContext &ctx, math::Rect bounds) {
  SettingsPanel::arrangeChildren(ctx, bounds);
  if (_registrations.empty())
    for (auto &entry : _editors) {
      auto *error = entry.error;
      _registrations.push_back(_form.registerField(
          entry.key, entry.editor->handle(),
          [this, group = entry.group] {
            _tabs->applySelectionPatch(
                {.selected = Patch<std::optional<std::string>>::set(
                     std::to_string(group))});
          },
          [error](ValidationResult issue) {
            error->applyPatch({.value = Patch<std::string>::set(
                                   issue ? issue->message : "")});
          }));
    }
}

void SettingsView::edited() {
  if (_status)
    _status->applyPatch(
        {.value = Patch<std::string>::set(
             "Draft changed. Apply for this run or Save to persist.")});
}

void SettingsView::submit(bool persist) {
  try {
    if (!_form.commit()) {
      edited();
      return;
    }
    if (_draft.automatic.enabled &&
        _draft.automatic.minimumSceneScale > _draft.threeD.resolutionScale) {
      _form.reject("minimum_scene_scale",
                   {"bounds", "Minimum scene resolution must not exceed "
                              "requested scene resolution"});
      return;
    }
    _draft.validate();
    // Avoid saving a lower ceiling that already cannot cover this live UI.
    // Low-level runtime policy still permits intentional over-budget operation.
    if (_usage) {
      const auto check = [](std::size_t next, std::size_t previous,
                            std::size_t live) {
        if (next < previous && next < live)
          throw std::invalid_argument(
              "A lowered limit must cover current managed usage. Return to the "
              "menu to release scene resources, then lower the limit.");
      };
      check(_draft.budgets.cpuBytes, _applied.budgets.cpuBytes,
            _usage->memory[0].bytes);
      check(_draft.budgets.gpuBytes, _applied.budgets.gpuBytes,
            _usage->memory[1].bytes);
      check(_draft.budgets.targetBytes, _applied.budgets.targetBytes,
            _usage->kinds[static_cast<unsigned>(ResourceKind::Target)].bytes);
      check(_draft.budgets.preparationBytes, _applied.budgets.preparationBytes,
            _usage->kinds[static_cast<unsigned>(ResourceKind::Preparation)]
                .bytes);
    }
    _actions.apply(_draft, persist);
  } catch (const std::exception &error) {
    _status->applyPatch({.value = Patch<std::string>::set(error.what())});
  }
}

void SettingsView::build() {
  auto root = std::make_unique<VStack>(
      layout::StackProps{.childrenAlignment = layout::CrossAlignment::Stretch});
  root->setControlLayout(ControlLayout::Section);
  root->append(text("Playground settings", TextRole::Title));
  root->append(text("Shared by every app. Edit a draft, then Apply for this "
                    "run or Save for future launches.",
                    TextRole::Body));
  std::vector<TabItem> tabs;
  const std::string names[]{"General", "2D", "3D", "Automatic", "Resources"};
  for (unsigned group = 0; group < 5; ++group) {
    auto column = std::make_unique<VStack>(layout::StackProps{
        .childrenAlignment = layout::CrossAlignment::Stretch});
    column->setControlLayout(ControlLayout::Group);
    if (group == 1)
      column->append(
          text("Raster scale applies to layers that opt in. Ordinary UI and "
               "text keep their authored resolution.",
               TextRole::Body));
    if (group == 2)
      column->append(text("Future options are stored preferences only. They "
                          "have no rendering effect until supported.",
                          TextRole::Body));
    if (group == 3)
      column->append(text(
          "Automatic mode overrides only permitted scene resolution. It never "
          "changes saved preferences, UI scale or simulation speed.",
          TextRole::Body));
    for (const auto &field : graphicsSettingsSchema()) {
      if (static_cast<unsigned>(field.group) != group)
        continue;
      const auto label = std::string{field.label} +
                         (field.inactive ? " (future; inactive)" : "");
      if (field.kind == SettingKind::Boolean) {
        auto toggle = std::make_unique<Checkbox>(
            text(label),
            ToggleProps{.checked = field.get(_draft) ? CheckState::On
                                                     : CheckState::Off,
                        .name = label});
        auto *control = toggle.get();
        _connections.push_back(
            toggle->onValueChanged([this, &field](CheckState v) {
              field.set(_draft, v == CheckState::On);
              edited();
            }));
        _refresh.push_back([this, control, &field] {
          control->applyPatch(
              {.checked = Patch<CheckState>::set(
                   field.get(_draft) ? CheckState::On : CheckState::Off)});
        });
        column->append(std::move(toggle));
      } else {
        std::unique_ptr<Node> control;
        auto error = text("", TextRole::Caption);
        error->applyPatch({.ink = Patch<TextInk>::set(TextInk::Error)});
        auto *errorText = error.get();
        if (!field.choices.empty()) {
          std::vector<ChoiceItem> choices;
          for (auto key : field.choices)
            choices.push_back({std::string{key}, std::string{key},
                               text(std::string{key}, TextRole::Value)});
          auto display =
              text(std::string{field.choices[std::size_t(field.get(_draft))]},
                   TextRole::Value);
          auto *readout = display.get();
          auto select = std::make_unique<Select>(
              std::move(display), std::move(choices),
              SelectionProps{
                  .selected =
                      std::string{
                          field.choices[std::size_t(field.get(_draft))]},
                  .required = true,
                  .name = label},
              ButtonProps{}, layout::BoxProps{});
          auto *raw = select.get();
          _connections.push_back(select->onSelectionChanged(
              [this, &field, readout](std::string key) {
                const auto index =
                    std::find(field.choices.begin(), field.choices.end(), key) -
                    field.choices.begin();
                field.set(_draft, double(index));
                readout->applyPatch({.value = Patch<std::string>::set(key)});
                edited();
              }));
          _refresh.push_back([this, raw, readout, &field] {
            const auto key =
                std::string{field.choices[std::size_t(field.get(_draft))]};
            raw->applySelectionPatch(
                {.selected = Patch<std::optional<std::string>>::set(key)});
            readout->applyPatch({.value = Patch<std::string>::set(key)});
          });
          control = std::move(select);
        } else {
          auto editor = std::make_unique<NumberField>(
              TextFieldProps{.font = _font,
                             .required = true,
                             .name = label,
                             .textRole = TextRole::Value},
              NumberFieldProps{.range = {field.get(_draft), field.minimum,
                                         field.maximum, field.step},
                               .integer = field.kind == SettingKind::Integer},
              layout::BoxProps{});
          auto *input = editor.get();
          _editors.push_back({std::string{field.key}, group, input, errorText});
          auto stepper = std::make_unique<NumberStepper>(
              std::move(editor),
              std::make_unique<ControlIcon>(ControlGlyph::Minus),
              std::make_unique<ControlIcon>(ControlGlyph::Plus),
              NumberStepperProps{.value = field.get(_draft),
                                 .minimum = field.minimum,
                                 .maximum = field.maximum,
                                 .step = field.step,
                                 .name = label},
              ButtonProps{}, layout::BoxProps{});
          auto *raw = stepper.get();
          _connections.push_back(
              stepper->onValueChanged([this, &field](double value) {
                field.set(_draft, value);
                edited();
              }));
          _connections.push_back(
              input->onValueChanged([this](std::string) { edited(); }));
          _connections.push_back(
              input->onValidationChanged([errorText](ValidationResult issue) {
                errorText->applyPatch({.value = Patch<std::string>::set(
                                           issue ? issue->message : "")});
              }));
          _refresh.push_back([this, raw, input, &field] {
            raw->applyPatch({.value = Patch<double>::set(field.get(_draft))});
            input->revertDraft();
          });
          control = std::move(stepper);
        }
        control->setControlLayout(ControlLayout::InputGroup);
        column->append(std::make_unique<Field>(
            std::move(control),
            text(label + (field.unit.empty()
                              ? ""
                              : " (" + std::string{field.unit} + ")")),
            std::move(error), FieldProps{.label = label}, layout::BoxProps{},
            true));
      }
    }
    if (group == 4) {
      auto meters = text("", TextRole::Code);
      _meters = meters.get();
      column->append(std::move(meters));
      auto cpu = std::make_unique<Meter>(
          MeterProps{.name = "Managed CPU usage", .unit = "MiB"});
      _cpuMeter = cpu.get();
      column->append(std::move(cpu));
      auto gpu = std::make_unique<Meter>(
          MeterProps{.name = "Managed GPU usage", .unit = "MiB"});
      _gpuMeter = gpu.get();
      column->append(std::move(gpu));
    }
    auto scroll = std::make_unique<ScrollView>(
        std::move(column), ScrollProps{}, layout::BoxProps{.maxHeight = 340});
    tabs.push_back({std::to_string(group), names[group], text(names[group]),
                    std::move(scroll)});
  }
  auto categories = std::make_unique<Tabs>(
      std::move(tabs), SelectionProps{.selected = "0",
                                      .required = true,
                                      .name = "Settings categories"});
  _tabs = categories.get();
  root->append(std::move(categories));
  auto status =
      text("Changes are not applied until Apply or Save.", TextRole::Body);
  _status = status.get();
  root->append(std::move(status));
  auto actions = std::make_unique<HStack>(layout::StackProps{});
  actions->setControlLayout(ControlLayout::Group);
  const auto button = [&](std::string label, std::function<void()> action) {
    auto node = std::make_unique<Button>(text(label), ButtonProps{});
    node->setControlLayout(ControlLayout::Choice);
    node->setSemanticProps({.role = SemanticRole::Button, .name = label});
    _connections.push_back(node->onActivate(std::move(action)));
    actions->append(std::move(node));
  };
  button("Apply", [this] { submit(false); });
  button("Save", [this] { submit(true); });
  button("Revert draft", [this] {
    _draft = _applied;
    for (auto &refresh : _refresh)
      refresh();
    _status->applyPatch({.value = Patch<std::string>::set(
                             "Draft restored to applied settings.")});
  });
  button("Close", [this] { _actions.close(); });
  button("Return to menu", [this] { _actions.returnToMenu(); });
  root->append(std::move(actions));
  root->append(text("Esc: close settings | Ctrl/Cmd+Shift+M: menu | Unapplied "
                    "edits are discarded on close.",
                    TextRole::Caption));
  setChild(std::make_unique<ScrollView>(
      std::move(root), ScrollProps{.sizing = ScrollSizing::Content}));
}

void SettingsView::setResult(GraphicsSettings applied, std::string message) {
  const bool preserveDraft = dirty();
  _applied = std::move(applied);
  if (!preserveDraft) {
    _draft = _applied;
    for (auto &refresh : _refresh)
      refresh();
  }
  if (dirty())
    message += " New draft edits are not applied.";
  _status->applyPatch({.value = Patch<std::string>::set(std::move(message))});
}

void SettingsView::setRuntime(const ResolvedGraphicsState &graphics,
                              const RenderRuntimeSnapshot &runtime) {
  _usage = runtime.resources;
  _cpuMeter->setProps(
      {.value = runtime.resources.memory[0].bytes / 1048576.,
       .maximum = std::max(1., runtime.resources.budgets.cpuBytes / 1048576.),
       .name = "Managed CPU usage",
       .unit = "MiB"});
  _gpuMeter->setProps(
      {.value = runtime.resources.memory[1].bytes / 1048576.,
       .maximum = std::max(1., runtime.resources.budgets.gpuBytes / 1048576.),
       .name = "Managed GPU usage",
       .unit = "MiB"});
  const auto mib = [](std::size_t bytes) { return bytes / 1048576.0; };
  auto value = std::format(
      "Managed CPU: {:.1f} / {:.0f} MiB (peak {:.1f})\nManaged GPU: {:.1f} / "
      "{:.0f} MiB (peak {:.1f})\nRetiring: {:.1f} MiB | outstanding frames: "
      "{}\nScene resolution: {:.0f}% requested / {:.0f}% effective\n{}\n{}",
      mib(runtime.resources.memory[0].bytes),
      mib(runtime.resources.budgets.cpuBytes),
      mib(runtime.resources.memory[0].peak),
      mib(runtime.resources.memory[1].bytes),
      mib(runtime.resources.budgets.gpuBytes),
      mib(runtime.resources.memory[1].peak), mib(runtime.resources.states[2]),
      runtime.outstandingFrames,
      graphics.requested.threeD.resolutionScale * 100,
      graphics.sceneScale * 100,
      graphics.reason +
          (graphics.pending ? " (awaiting scene observation)" : ""),
      runtime.pressure);
  if (runtime.cpu)
    value += std::format(
        "\nLatest CPU iteration: {:.2f} ms",
        runtime.cpu->milliseconds[static_cast<std::size_t>(CPUPhase::Total)]);
  if (runtime.latestGPU)
    value +=
        std::format("\nLatest GPU scope ({}): {:.2f} ms",
                    runtime.latestGPU->label, runtime.latestGPU->milliseconds);
  if (_meters->props().value != value)
    _meters->applyPatch({.value = Patch<std::string>::set(value)});
}
} // namespace playground::ui

namespace playground::ui {
std::unique_ptr<SettingsView>
makeSettingsView(AssetRegistry &assets, FontHandle font,
                 rendering::GraphicsSettings settings,
                 SettingsViewActions actions) {
  return std::make_unique<SettingsView>(
      assets, std::move(font), std::move(settings), std::move(actions));
}
} // namespace playground::ui
