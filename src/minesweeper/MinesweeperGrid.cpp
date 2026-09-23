#include <minesweeper/MinesweeperGrid.hpp>

MinesweeperGrid::MinesweeperGrid(
    MinesweeperGridProps props, const MinesweeperCellStyle &cellStyle,
    const playground::minesweeper::MinesweeperEvents &events,
    const playground::minesweeper::ViewResources &resources,
    const BombSelector &selectBomb)
    : Grid{layoutFor(props)}, _events{events}, _props{props},
      _cellStyle{cellStyle} {
  _cells.reserve(static_cast<std::size_t>(_props.size.y) * _props.size.x);
  for (int row = 0; row < _props.size.y; ++row) {
    for (int col = 0; col < _props.size.x; ++col) {
      const playground::math::Vec2i position{col, row};
      const bool bomb = selectBomb
                            ? selectBomb(position)
                            : _random.get(0.0f, 1.0f) < _props.bombChance;
      auto cell = std::make_unique<MinesweeperCell>(
          static_cast<float>(_props.cellSize), bomb, position, _cellStyle,
          _events, resources, _generation,
          [this](const SDL_Event &event) { return handleEvent(event); });
      _cells.push_back(cell.get());
      append(std::move(cell), {.row = static_cast<std::size_t>(row),
                               .column = static_cast<std::size_t>(col)});
      if (bomb)
        _bombs.push_back(position);
    }
  }

  _cellsToClear =
      _props.size.y * _props.size.x - static_cast<int>(_bombs.size());

  for (const auto gridPos : _bombs) {
    for (int i = -1; i <= 1; ++i) {
      for (int j = -1; j <= 1; ++j) {
        if (i == 0 && j == 0)
          continue;
        playground::math::Vec2i target{gridPos.x + i, gridPos.y + j};
        if (inBoundsExclusive(target, _props.size))
          getCellAt(target).incrementAdjacentBombs();
      }
    }
  }
}

MinesweeperCell &
MinesweeperGrid::getCellAt(const playground::math::Vec2i &gridPos) {
  if (!inBoundsExclusive(gridPos, _props.size))
    throw std::out_of_range("MinesweeperGrid coordinate out of bounds");
  return *_cells.at(
      static_cast<std::size_t>(gridPos.y * _props.size.x + gridPos.x));
}

const MinesweeperCell &
MinesweeperGrid::getCellAt(const playground::math::Vec2i &gridPos) const {
  if (!inBoundsExclusive(gridPos, _props.size))
    throw std::out_of_range("MinesweeperGrid coordinate out of bounds");
  return *_cells.at(
      static_cast<std::size_t>(gridPos.y * _props.size.x + gridPos.x));
}

EventResult MinesweeperGrid::handleEvent(const SDL_Event &event) {
  EventResult result{EventResult::Ignored};

  if (event.type == _events.cellHit) {
    return EventResult::Consumed;
  }

  if (event.type == _events.bombDetonated) {
    for (const auto &gridPos : _bombs) {
      MinesweeperCell &cell = getCellAt(gridPos);
      if (cell.isCleared())
        continue;
      cell.clearCell(false);
    }
    SDL_Event gameLost{.user = {.type = _events.gameLost, .code = _generation}};
    SDL_PushEvent(&gameLost);
    return EventResult::Consumed;
  }

  if (event.type == _events.cellCleared) {
    --_cellsToClear;

    if (!_cellsToClear) {
      for (const auto &gridPos : _bombs) {
        MinesweeperCell &cell = getCellAt(gridPos);
        cell.setRevealed();
        cell.clearCell(false);
      }
      SDL_Event gameWon{.user = {.type = _events.gameWon, .code = _generation}};
      SDL_PushEvent(&gameWon);
      return EventResult::Consumed;
    }

    if ((*static_cast<const MinesweeperCell *>(event.user.data2))
            .getAdjacentBombs())
      return EventResult::Consumed;
  }

  for (const auto &cell : _cells) {
    result = combine(result, cell->handleEvent(event));
    if (isTerminal(result))
      break;
  }

  return result;
}

playground::layout::GridProps
MinesweeperGrid::layoutFor(const MinesweeperGridProps &p) {
  p.validate();
  using namespace playground;
  return {
      .columns = std::vector<layout::TrackSize>(
          p.size.x, layout::TrackSize::fixed(static_cast<float>(p.cellSize))),
      .rows = std::vector<layout::TrackSize>(
          p.size.y, layout::TrackSize::fixed(static_cast<float>(p.cellSize))),
      .gap = {static_cast<float>(p.gap), static_cast<float>(p.gap)}};
}
