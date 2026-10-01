#include <algorithm>
#include <format>
#include <memory>
#include <utility>

#include <minesweeper/Config.hpp>
#include <minesweeper/DifficultyUI.hpp>
#include <ui/containers/Stack.hpp>

using namespace playground;
namespace mineConfig = playground::minesweeper::config;

DifficultyUI::DifficultyUI(MinesweeperBoardProps &draft,
                           const minesweeper::ViewResources &resources,
                           const ActionButtonProps &style,
                           std::function<void(MinesweeperBoardProps)> start,
                           std::function<void()> launcher)
    : Box{{.padding = math::Insets::all(mineConfig::outerPadding)},
          {layout::Alignment::stretch()}},
      _draft{draft}, _resources{resources} {
  auto column = std::make_unique<ui::VStack>(layout::StackProps{
      .gap = 12, .childrenAlignment = layout::CrossAlignment::Stretch});
  auto title = minesweeper::makeLabel(resources, "Minesweeper",
                                      style.labelColor, {408, 54});
  title->setBoxProps({.height = layout::SizeRule::fixed(54)});
  column->append(std::move(title));

  auto presets = std::make_unique<ui::HStack>(layout::StackProps{.gap = 12});
  presets->append(std::make_unique<ActionButton>(
      resources, math::Size2{198, 48}, style, "Easy: 9 x 9",
      [this] { select(mineConfig::easy); }));
  presets->append(std::make_unique<ActionButton>(
      resources, math::Size2{198, 48}, style, "Hard: 16 x 16",
      [this] { select(mineConfig::hard); }));
  column->append(std::move(presets));

  const auto addStepper = [&](const std::string &name, int value, int minimum,
                              int maximum, ui::NumberStepper *&stepper,
                              ui::Text *&label,
                              support::MoveOnlyFunction<void(int)> changed) {
    auto text =
        minesweeper::makeLabel(resources, name + ": " + std::to_string(value),
                               style.labelColor, {312, 44});
    label = text.get();
    auto node = std::make_unique<ui::NumberStepper>(
        std::move(text),
        minesweeper::makeLabel(resources, "-", style.labelColor, {40, 44}),
        minesweeper::makeLabel(resources, "+", style.labelColor, {40, 44}),
        ui::NumberStepperProps{double(value), double(minimum), double(maximum),
                               1, true, name},
        style.button,
        layout::BoxProps{.width = layout::SizeRule::fixed(408),
                         .height = layout::SizeRule::fixed(44)});
    stepper = node.get();
    _connections.push_back(node->onValueChanged(std::move(changed)));
    column->append(std::move(node));
  };
  addStepper("Columns", draft.size.x, mineConfig::minimumDimension,
             mineConfig::maximumDimension, _columns, _columnsLabel,
             [this](int value) {
               _draft.size.x = value;
               refresh();
             });
  addStepper("Rows", draft.size.y, mineConfig::minimumDimension,
             mineConfig::maximumDimension, _rows, _rowsLabel,
             [this](int value) {
               _draft.size.y = value;
               refresh();
             });
  addStepper("Bombs", draft.bombs, 1, draft.size.x * draft.size.y - 1, _bombs,
             _bombsLabel, [this](int value) {
               _draft.bombs = value;
               refresh();
             });

  auto status =
      minesweeper::makeLabel(resources, "Ready", style.labelColor, {408, 28});
  status->setBoxProps({.height = layout::SizeRule::fixed(28)});
  _status = status.get();
  column->append(std::move(status));
  auto actions = std::make_unique<ui::HStack>(layout::StackProps{.gap = 12});
  actions->append(
      std::make_unique<ActionButton>(resources, math::Size2{198, 48}, style,
                                     "Start", [this, start = std::move(start)] {
                                       _draft.validate();
                                       start(_draft);
                                     }));
  actions->append(std::make_unique<ActionButton>(
      resources, math::Size2{198, 48}, style, "Launcher", std::move(launcher)));
  column->append(std::move(actions));
  auto help = minesweeper::makeLabel(
      resources, "Tab: focus | arrows: adjust | Enter: activate",
      style.labelColor, {408, 24});
  help->setBoxProps({.height = layout::SizeRule::fixed(24)});
  column->append(std::move(help));
  setChild(std::move(column));
  setSemanticProps(
      {.role = ui::SemanticRole::Group, .name = "Minesweeper difficulty"});
  refresh();
}

void DifficultyUI::select(MinesweeperBoardProps value) {
  value.validate();
  _draft = value;
  refresh();
}

void DifficultyUI::setStatus(std::string value) {
  auto text = _status->props();
  text.value = std::move(value);
  text.font = minesweeper::fittedFont(_resources, text.value, {408, 28});
  _status->setProps(std::move(text));
}

void DifficultyUI::refresh() {
  _draft.bombs = std::clamp(_draft.bombs, 1, _draft.size.x * _draft.size.y - 1);
  const auto update = [&](ui::NumberStepper &stepper, ui::Text &label,
                          int value, int maximum) {
    auto props = stepper.props();
    props.value = value;
    props.maximum = maximum;
    stepper.setProps(props);
    auto text = label.props();
    text.value = props.name + ": " + std::to_string(value);
    text.font = minesweeper::fittedFont(_resources, text.value, {312, 44});
    label.setProps(std::move(text));
  };
  update(*_columns, *_columnsLabel, _draft.size.x,
         mineConfig::maximumDimension);
  update(*_rows, *_rowsLabel, _draft.size.y, mineConfig::maximumDimension);
  update(*_bombs, *_bombsLabel, _draft.bombs,
         _draft.size.x * _draft.size.y - 1);
  const auto preset = _draft == mineConfig::easy   ? "Easy"
                      : _draft == mineConfig::hard ? "Hard"
                                                   : "Custom";
  setStatus(std::format("{}: {} x {}, {} bombs", preset, _draft.size.x,
                        _draft.size.y, _draft.bombs));
}
