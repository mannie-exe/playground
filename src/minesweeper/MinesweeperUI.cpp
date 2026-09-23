#include <minesweeper/MinesweeperUI.hpp>

MinesweeperUI::MinesweeperUI(
    MinesweeperUIProps props,
    const playground::minesweeper::MinesweeperEvents &events,
    const playground::minesweeper::ViewResources &resources,
    MinesweeperGrid::BombSelector selectBomb)
    : Box{{}, {playground::layout::Alignment::center()}}, _events{events},
      _props{std::move(props)}, _resources{resources},
      _selectBomb{std::move(selectBomb)} {
  using namespace playground;
  _props.grid.validate();
  const auto &p = _props.layout;
  const auto gridWidth = _props.grid.width();
  if (p.padding < 0 || p.footerHeight <= 0 || p.footerCounterWidth <= 0 ||
      p.footerGap < 0 || p.footerCounterWidth >= gridWidth ||
      p.footerGap >= gridWidth - p.footerCounterWidth)
    throw std::invalid_argument("Minesweeper footer does not fit the grid");

  auto column = std::make_unique<ui::VStack>(
      layout::StackProps{.gap = static_cast<float>(p.padding),
                         .childrenAlignment = layout::CrossAlignment::Stretch});
  _column = column.get();
  auto grid = makeGrid();
  _grid = grid.get();
  column->append(std::move(grid));

  const float newGameWidth =
      static_cast<float>(gridWidth - p.footerCounterWidth - p.footerGap);
  auto footer = std::make_unique<ui::HStack>(
      layout::StackProps{.gap = static_cast<float>(p.footerGap),
                         .childrenAlignment = layout::CrossAlignment::Stretch},
      layout::BoxProps{.height = layout::SizeRule::fixed(
                           static_cast<float>(p.footerHeight))});
  auto button = std::make_unique<NewGameButton>(
      resources, math::Size2{newGameWidth, static_cast<float>(p.footerHeight)},
      _props.newGameButton, [this] { _resetRequested = true; });
  _newGameButton = button.get();
  button->setBoxProps({.width = layout::SizeRule::fixed(newGameWidth)});
  footer->append(std::move(button));

  auto counterProps = _props.flagCounter;
  counterProps.amount = _grid->getBombCount();
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
  setChild(std::move(column));
}

void MinesweeperUI::setGameState(GameState state) {
  auto semantics = _grid->semanticProps();
  semantics.enabled = state == GameState::Playing;
  _grid->setSemanticProps(std::move(semantics));
  _state = state;
}

bool MinesweeperUI::processPendingActions() {
  if (!_resetRequested)
    return false;
  reset();
  _resetRequested = false;
  return true;
}

void MinesweeperUI::reset() {
  auto grid = makeGrid();
  auto *next = grid.get();
  _column->insert(0, std::move(grid));
  _column->remove(std::size_t{1});
  _grid = next;
  setGameState(GameState::Playing);
  _flagCounter->setAmount(_grid->getBombCount());
}

EventResult MinesweeperUI::handleEvent(const SDL_Event &event) {
  if (event.type == _events.gameWon || event.type == _events.gameLost) {
    if (event.user.code == _grid->generation())
      setGameState(event.type == _events.gameWon ? GameState::Won
                                                 : GameState::Lost);
    return EventResult::Consumed;
  }
  if (event.type == _events.newGameRequested) {
    _resetRequested = true;
    return EventResult::Consumed;
  }
  if (event.type == _events.flagToggled) {
    const auto change =
        playground::minesweeper::takeFlagChange(event.user.code);
    if (!change || change->grid != _grid->generation())
      return EventResult::Consumed;
    if (_flagCounter->getAmount() == 0 && change->flagged) {
      _grid->getCellAt(change->position).setFlagged(false);
      return EventResult::Consumed;
    }
    _flagCounter->setAmount(_flagCounter->getAmount() +
                            (change->flagged ? -1 : 1));
    return EventResult::Consumed;
  }
  return EventResult::Ignored;
}
