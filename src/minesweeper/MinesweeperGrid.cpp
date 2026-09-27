#include <minesweeper/MinesweeperGrid.hpp>

MinesweeperGrid::MinesweeperGrid(
    MinesweeperGridProps props, const MinesweeperCellStyle &style,
    MinesweeperModel &model,
    const playground::minesweeper::ViewResources &resources)
    : Grid{layoutFor(props)}, _model{model}, _props{props}, _cellStyle{style} {
  if (model.props().size != props.size)
    throw std::invalid_argument("Grid props must describe their model");
  _cells.reserve(static_cast<std::size_t>(props.size.x) * props.size.y);
  for (int y = 0; y < props.size.y; ++y)
    for (int x = 0; x < props.size.x; ++x) {
      auto cell = std::make_unique<MinesweeperCell>(
          static_cast<float>(props.cellSize), model,
          playground::math::Vec2i{x, y}, style, resources,
          [this](playground::math::Vec2i p, int button) {
            focusCell(p);
            if (button == 1)
              _model.clear(p);
            else if (button == 3)
              _model.toggleFlag(p);
          });
      _cells.push_back(cell.get());
      cell->setFocusable(x == 0 && y == 0);
      append(std::move(cell), {.row = static_cast<std::size_t>(y),
                               .column = static_cast<std::size_t>(x)});
    }
}

void MinesweeperGrid::focusCell(playground::math::Vec2i p) {
  if (!inBoundsExclusive(p, _props.size))
    return;
  if (p != _focused)
    cellAt(_focused).setFocusable(false);
  _focused = p;
  cellAt(p).setFocusable(true);
  cellAt(p).requestFocus();
}

void MinesweeperGrid::onDefaultEvent(playground::ui::UIEvent &event) {
  using namespace playground::ui;
  if (event.handled || event.type != EventType::KeyDown)
    return;
  auto next = _focused;
  switch (event.logicalKey) {
  case Key::Left:
    --next.x;
    break;
  case Key::Right:
    ++next.x;
    break;
  case Key::Up:
    --next.y;
    break;
  case Key::Down:
    ++next.y;
    break;
  default:
    return;
  }
  event.handled = true;
  focusCell(next);
}

MinesweeperCell &MinesweeperGrid::cellAt(const playground::math::Vec2i &p) {
  if (!inBoundsExclusive(p, _props.size))
    throw std::out_of_range("Cell outside grid");
  return *_cells.at(static_cast<std::size_t>(p.y) * _props.size.x + p.x);
}

const MinesweeperCell &
MinesweeperGrid::cellAt(const playground::math::Vec2i &p) const {
  if (!inBoundsExclusive(p, _props.size))
    throw std::out_of_range("Cell outside grid");
  return *_cells.at(static_cast<std::size_t>(p.y) * _props.size.x + p.x);
}

void MinesweeperGrid::synchronize() {
  for (auto *cell : _cells)
    cell->synchronize();
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
