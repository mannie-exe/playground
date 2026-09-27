#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

#include <math/Geometry2D.hpp>
#include <minesweeper/MinesweeperCell.hpp>
#include <ui/containers/Grid.hpp>

struct MinesweeperGridProps {
  playground::math::Vec2i size;
  int cellSize;
  int gap;

  int width() const { return extent(size.x); }

  int height() const { return extent(size.y); }

  void validate() const {
    if (!hasArea(size) || cellSize <= 0 || gap < 0)
      throw std::invalid_argument("Invalid Minesweeper grid dimensions");
    (void)width();
    (void)height();
    if (static_cast<std::int64_t>(size.x) * size.y >
        std::numeric_limits<int>::max())
      throw std::overflow_error("Minesweeper cell count exceeds int range");
  }

private:
  int extent(int count) const {
    if (count <= 0 || cellSize <= 0 || gap < 0)
      throw std::invalid_argument("Invalid Minesweeper grid dimensions");
    const auto value = static_cast<std::int64_t>(count) * cellSize +
                       static_cast<std::int64_t>(count - 1) * gap;
    if (value > std::numeric_limits<int>::max())
      throw std::overflow_error("Minesweeper extent exceeds int range");
    return static_cast<int>(value);
  }
};

class MinesweeperGrid : public playground::ui::Grid {
  MinesweeperModel &_model;

  MinesweeperGridProps _props;
  MinesweeperCellStyle _cellStyle;

  std::vector<MinesweeperCell *> _cells;
  playground::math::Vec2i _focused{};

protected:
  void onDefaultEvent(playground::ui::UIEvent &) override;

public:
  explicit MinesweeperGrid(
      MinesweeperGridProps props, const MinesweeperCellStyle &cellStyle,
      MinesweeperModel &model,
      const playground::minesweeper::ViewResources &resources);

  MinesweeperCell &cellAt(const playground::math::Vec2i &gridPos);

  MinesweeperCell &cellAt(const int x, const int y) {
    return cellAt(playground::math::Vec2i{x, y});
  }

  const MinesweeperCell &cellAt(const playground::math::Vec2i &gridPos) const;

  const MinesweeperCell &cellAt(const int x, const int y) const {
    return cellAt(playground::math::Vec2i{x, y});
  }

  int bombCount() const { return _model.bombCount(); }

  const MinesweeperGridProps &props() const { return _props; }

  const MinesweeperCellStyle &cellStyle() const { return _cellStyle; }

  void synchronize();
  void focusCell(playground::math::Vec2i);

private:
  static playground::layout::GridProps layoutFor(const MinesweeperGridProps &p);
};
