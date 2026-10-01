#include <format>

#include <ui/collections/ScrollView.hpp>
#include <ui/containers/Stack.hpp>
#include <ui/controls/Choice.hpp>
#include <ui/controls/Composite.hpp>
#include <ui/controls/Stepper.hpp>
#include <ui/views/SettingsView.hpp>

namespace playground::ui {
using namespace rendering;

SettingsView::SettingsView(AssetRegistry &assets, FontHandle font,
                           GraphicsSettings settings,
                           SettingsViewActions actions)
    : SettingsPanel{
          layout::BoxProps{.maxWidth = 960, .padding = math::Insets::all(20)}},
      _assets{assets}, _font{std::move(font)}, _actions{std::move(actions)},
      _draft{settings}, _applied{settings} {
  setPaintStyle({.themeBackground = true});
  setSemanticProps(
      {.role = SemanticRole::Group, .name = "Playground settings"});
  build();
}

std::unique_ptr<Text> SettingsView::text(std::string value) {
  return std::make_unique<Text>(
      _assets, TextProps{.value = std::move(value),
                         .font = _font,
                         .wrap = TextWrap::AvailableInlineSize});
}

void SettingsView::edited() {
  if (_status)
    _status->applyPatch(
        {.value = Patch<std::string>::set(
             "Draft changed. Apply for this run or Save to persist.")});
}

void SettingsView::submit(bool persist) {
  try {
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
  auto root = std::make_unique<VStack>(layout::StackProps{
      .gap = 12, .childrenAlignment = layout::CrossAlignment::Stretch});
  root->append(text("Playground settings"));
  root->append(text("Shared by every app. Edit a draft, then Apply for this "
                    "run or Save for future launches."));
  std::vector<TabItem> tabs;
  const std::string names[]{"General", "2D", "3D", "Automatic", "Resources"};
  for (unsigned group = 0; group < 5; ++group) {
    auto column = std::make_unique<VStack>(layout::StackProps{
        .gap = 10, .childrenAlignment = layout::CrossAlignment::Stretch});
    if (group == 1)
      column->append(
          text("Raster scale applies to layers that opt in. Ordinary UI and "
               "text keep their authored resolution."));
    if (group == 2)
      column->append(text("Future options are stored preferences only. They "
                          "have no rendering effect until supported."));
    if (group == 3)
      column->append(text(
          "Automatic mode overrides only permitted scene resolution. It never "
          "changes saved preferences, UI scale or simulation speed."));
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
        auto row = std::make_unique<HStack>(layout::StackProps{
            .gap = 16, .childrenAlignment = layout::CrossAlignment::Center});
        row->append(text(label), {.grow = 1});
        auto readout = text("");
        auto *value = readout.get();
        const auto refresh = [this, value, &field] {
          const auto v = field.get(_draft);
          const auto display =
              field.choices.empty()
                  ? std::format("{:.0f} {}", v, field.unit)
                  : std::string{field.choices[static_cast<std::size_t>(v)]};
          value->applyPatch({.value = Patch<std::string>::set(display)});
        };
        auto stepper = std::make_unique<Stepper>(
            std::move(readout), text("-"), text("+"),
            StepperProps{.value =
                             static_cast<int>(std::lround(field.get(_draft))),
                         .minimum = static_cast<int>(field.minimum),
                         .maximum = static_cast<int>(field.maximum),
                         .step = static_cast<int>(field.step),
                         .name = label},
            ButtonProps{},
            layout::BoxProps{.width = layout::SizeRule::fixed(220),
                             .height = layout::SizeRule::fixed(40)});
        auto *control = stepper.get();
        _connections.push_back(
            stepper->onValueChanged([this, &field, refresh](int v) {
              field.set(_draft, v);
              refresh();
              edited();
            }));
        _refresh.push_back([this, control, &field, refresh] {
          control->applyPatch({.value = Patch<int>::set(static_cast<int>(
                                   std::lround(field.get(_draft))))});
          refresh();
        });
        refresh();
        row->append(std::move(stepper), {.shrink = 0});
        column->append(std::move(row));
      }
    }
    if (group == 4) {
      auto meters = text("");
      _meters = meters.get();
      column->append(std::move(meters));
    }
    auto scroll = std::make_unique<ScrollView>(
        std::move(column), ScrollProps{},
        layout::BoxProps{.height = layout::SizeRule::fixed(340)});
    tabs.push_back({std::to_string(group), names[group], text(names[group]),
                    std::move(scroll)});
  }
  root->append(std::make_unique<Tabs>(
      std::move(tabs), SelectionProps{.selected = "0",
                                      .required = true,
                                      .name = "Settings categories"}));
  auto status = text("Changes are not applied until Apply or Save.");
  _status = status.get();
  root->append(std::move(status));
  auto actions = std::make_unique<HStack>(layout::StackProps{.gap = 10});
  const auto button = [&](std::string label, std::function<void()> action) {
    auto node = std::make_unique<Button>(
        text(label), ButtonProps{},
        layout::BoxProps{.padding = math::Insets::all(8)});
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
                    "edits are discarded on close."));
  setChild(std::move(root));
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
