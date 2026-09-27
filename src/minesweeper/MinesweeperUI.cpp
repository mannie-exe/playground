#include <minesweeper/MinesweeperUI.hpp>

MinesweeperUI::MinesweeperUI(
    MinesweeperUIProps props, MinesweeperModel &model,
    const playground::minesweeper::ViewResources &resources)
    : Box{{}, {playground::layout::Alignment::center()}}, _model{model},
      _props{std::move(props)}, _resources{resources} {
  using namespace playground;
  _props.grid.validate();
  const auto &p = _props.layout;
  if (p.padding < 0 || p.footerHeight <= 0 || p.footerCounterWidth <= 0 ||
      p.footerGap < 0 || p.actionWidth <= 0)
    throw std::invalid_argument("Invalid Minesweeper footer props");

  setBoxProps({.padding = math::Insets::all(static_cast<float>(p.padding))});

  auto column = std::make_unique<ui::VStack>(
      layout::StackProps{.gap = static_cast<float>(p.padding),
                         .childrenAlignment = layout::CrossAlignment::Center});
  auto grid = makeGrid();
  _grid = grid.get();
  column->append(std::move(grid));

  auto footer = std::make_unique<ui::HStack>(
      layout::StackProps{.gap = static_cast<float>(p.footerGap),
                         .childrenAlignment = layout::CrossAlignment::Stretch},
      layout::BoxProps{.height = layout::SizeRule::fixed(
                           static_cast<float>(p.footerHeight))});
  const math::Size2 actionSize{static_cast<float>(p.actionWidth),
                               static_cast<float>(p.footerHeight)};
  auto button = std::make_unique<ActionButton>(
      resources, actionSize, _props.actionButton, "New Game",
      [this] { _resetRequested = true; });
  _newGameButton = button.get();
  footer->append(std::move(button));
  footer->append(std::make_unique<ActionButton>(
      resources, actionSize, _props.actionButton, "Difficulty",
      [this] { _menuRequested = true; }));

  auto counterProps = _props.flagCounter;
  counterProps.amount = _model.availableFlags();
  auto counter = std::make_unique<FlagCounter>(
      resources,
      math::Size2{static_cast<float>(p.footerCounterWidth),
                  static_cast<float>(p.footerHeight)},
      counterProps);
  _flagCounter = counter.get();
  counter->setBoxProps({.width = layout::SizeRule::fixed(
                            static_cast<float>(p.footerCounterWidth))});
  footer->append(std::move(counter));
  column->append(std::move(footer));
  const float helpWidth = static_cast<float>(
      2 * p.actionWidth + p.footerCounterWidth + 2 * p.footerGap);
  const float helpHeight = static_cast<float>(p.footerHeight) / 2;
  auto status = minesweeper::makeLabel(resources, "Playing",
                                       _props.actionButton.labelColor,
                                       {helpWidth, helpHeight});
  _status = status.get();
  status->setBoxProps({.width = layout::SizeRule::fixed(helpWidth),
                       .height = layout::SizeRule::fixed(helpHeight)});
  column->append(std::move(status));
  auto help = minesweeper::makeLabel(
      resources, "Arrows move | Enter reveals | Shift+Enter flags",
      _props.actionButton.labelColor, {helpWidth, helpHeight});
  help->setBoxProps({.width = layout::SizeRule::fixed(helpWidth),
                     .height = layout::SizeRule::fixed(helpHeight)});
  column->append(std::move(help));
  setChild(std::move(column));
  synchronize();
}

void MinesweeperUI::synchronize() {
  if (_revision == _model.revision())
    return;
  _grid->synchronize();
  _flagCounter->setAmount(_model.availableFlags());
  auto semantics = _grid->semanticProps();
  semantics.enabled = isPlaying();
  _grid->setSemanticProps(std::move(semantics));
  auto status = _status->props();
  status.value = gameState() == GameState::Won    ? "You won!"
                 : gameState() == GameState::Lost ? "Bomb hit - try again"
                                                  : "Playing";
  _status->setProps(std::move(status));
  _revision = _model.revision();
}

bool MinesweeperUI::processPendingActions() {
  if (!_resetRequested) {
    synchronize();
    return false;
  }
  reset();
  _resetRequested = false;
  return true;
}

void MinesweeperUI::reset() {
  _model.reset();
  synchronize();
}
