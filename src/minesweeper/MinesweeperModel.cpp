#include <algorithm>
#include <limits>
#include <numeric>
#include <stdexcept>

#include <minesweeper/MinesweeperModel.hpp>
#include <support/Random.hpp>

void MinesweeperBoardProps::validate() const {
  const auto count = static_cast<std::int64_t>(size.x) * size.y;
  if (!hasArea(size) || count > std::numeric_limits<int>::max() || bombs < 0 ||
      bombs >= count)
    throw std::invalid_argument("Invalid Minesweeper board props");
}

MinesweeperModel::MinesweeperModel(MinesweeperBoardProps props)
    : _props{props} {
  props.validate();
  reset();
}

std::size_t MinesweeperModel::index(playground::math::Vec2i p) const {
  if (!inBoundsExclusive(p, _props.size))
    throw std::out_of_range("Cell outside board");
  return static_cast<std::size_t>(p.y) * _props.size.x + p.x;
}

const MinesweeperCellState &
MinesweeperModel::cellAt(playground::math::Vec2i p) const {
  return _cells.at(index(p));
}

void MinesweeperModel::reset() {
  std::vector<MinesweeperCellState> cells(
      static_cast<std::size_t>(_props.size.x) * _props.size.y);
  playground::Random random;
  std::vector<int> positions(cells.size());
  std::iota(positions.begin(), positions.end(), 0);
  for (int i = 0; i < _props.bombs; ++i) {
    const int selected = random.get(i, static_cast<int>(cells.size()) - 1);
    std::swap(positions[i], positions[selected]);
    cells[positions[i]].bomb = true;
  }
  for (int y = 0; y < _props.size.y; ++y)
    for (int x = 0; x < _props.size.x; ++x)
      if (cells[index({x, y})].bomb)
        for (int dy = -1; dy <= 1; ++dy)
          for (int dx = -1; dx <= 1; ++dx) {
            const playground::math::Vec2i p{x + dx, y + dy};
            if ((dx || dy) && inBoundsExclusive(p, _props.size) &&
                !cells[index(p)].bomb)
              ++cells[index(p)].adjacentBombs;
          }
  _cells.swap(cells);
  _bombs = _props.bombs;
  _flags = 0;
  _remaining = static_cast<int>(_cells.size()) - _bombs;
  _state = GameState::Playing;
  ++_revision;
}

void MinesweeperModel::toggleFlag(playground::math::Vec2i p) {
  auto &cell = _cells.at(index(p));
  if (_state != GameState::Playing || cell.cleared ||
      (!cell.flagged && availableFlags() == 0))
    return;
  cell.flagged = !cell.flagged;
  _flags += cell.flagged ? 1 : -1;
  ++_revision;
}

void MinesweeperModel::clear(playground::math::Vec2i p) {
  const auto &first = cellAt(p);
  if (_state != GameState::Playing || first.cleared || first.flagged)
    return;
  // Stage a complete operation; allocation failure cannot publish half a flood.
  auto cells = _cells;
  std::vector<playground::math::Vec2i> pending{p};
  auto remaining = _remaining;
  auto flags = _flags;
  auto state = _state;
  for (std::size_t next = 0; next < pending.size(); ++next) {
    const auto position = pending[next];
    auto &cell = cells[index(position)];
    if (cell.cleared)
      continue;
    if (cell.flagged) {
      cell.flagged = false;
      --flags;
    }
    cell.cleared = true;
    if (cell.bomb) {
      state = GameState::Lost;
      break;
    }
    --remaining;
    if (cell.adjacentBombs)
      continue;
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx) {
        playground::math::Vec2i adjacent{position.x + dx, position.y + dy};
        if ((dx || dy) && inBoundsExclusive(adjacent, _props.size)) {
          const auto &other = cells[index(adjacent)];
          if (!other.bomb && !other.cleared)
            pending.push_back(adjacent);
        }
      }
  }
  if (state != GameState::Lost && !remaining)
    state = GameState::Won;
  if (state != GameState::Playing)
    for (auto &cell : cells)
      if (cell.bomb) {
        if (cell.flagged) {
          cell.flagged = false;
          --flags;
        }
        cell.cleared = true;
        cell.revealed = state == GameState::Won;
      }
  _cells.swap(cells);
  _remaining = remaining;
  _flags = flags;
  _state = state;
  ++_revision;
}
