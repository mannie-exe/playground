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
#include <minesweeper/MinesweeperEvents.hpp>
#include <platform/sdl/EventResult.hpp>
#include <support/Random.hpp>
#include <ui/containers/Grid.hpp>

struct MinesweeperGridProps {
  playground::math::Vec2i size;
  int cellSize;
  int gap;
  float bombChance;

  int width() const { return extent(size.x); }
  int height() const { return extent(size.y); }

  void validate() const {
    if (!hasArea(size) || cellSize <= 0 || gap < 0 ||
        !std::isfinite(bombChance) || bombChance < 0 || bombChance > 1)
      throw std::invalid_argument("Invalid Minesweeper grid dimensions/chance");
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
public:
  using BombSelector = std::function<bool(playground::math::Vec2i)>;

private:
  playground::minesweeper::MinesweeperEvents _events;
  Sint32 _generation{playground::minesweeper::nextGridGeneration()};
  playground::Random _random{};

  MinesweeperGridProps _props;
  MinesweeperCellStyle _cellStyle;

  std::vector<MinesweeperCell *> _cells;
  std::vector<playground::math::Vec2i> _bombs;

  int _cellsToClear{};

public:
  explicit MinesweeperGrid(
      MinesweeperGridProps props, const MinesweeperCellStyle &cellStyle,
      const playground::minesweeper::MinesweeperEvents &events,
      const playground::minesweeper::ViewResources &resources,
      const BombSelector &selectBomb = {});
  ~MinesweeperGrid() {
    playground::minesweeper::discardGridNotifications(_generation);
  }
  Sint32 generation() const noexcept { return _generation; }

  MinesweeperCell &getCellAt(const playground::math::Vec2i &gridPos);

  MinesweeperCell &getCellAt(const int x, const int y) {
    return getCellAt(playground::math::Vec2i{x, y});
  }

  const MinesweeperCell &
  getCellAt(const playground::math::Vec2i &gridPos) const;

  const MinesweeperCell &getCellAt(const int x, const int y) const {
    return getCellAt(playground::math::Vec2i{x, y});
  }

  int getBombCount() const { return static_cast<int>(_bombs.size()); }
  const MinesweeperGridProps &getProps() const { return _props; }
  const MinesweeperCellStyle &getCellStyle() const { return _cellStyle; }

  EventResult handleEvent(const SDL_Event &event);

private:
  static playground::layout::GridProps layoutFor(const MinesweeperGridProps &p);
};
