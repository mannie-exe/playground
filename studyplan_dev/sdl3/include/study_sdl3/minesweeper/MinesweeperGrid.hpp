#pragma once

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <vector>

#include <SDL3/SDL_rect.h>

#include <study_sdl3/interfaces/IDisplayObject.hpp>
#include <study_sdl3/interfaces/IInteractable.hpp>
#include <study_sdl3/minesweeper/MinesweeperCell.hpp>
#include <study_sdl3/minesweeper/MinesweeperEvents.hpp>
#include <study_sdl3/support/Random.hpp>
#include <study_sdl3/support/SDLPrimitives.hpp>

struct MinesweeperGridProps {
  Vec2i size;
  int cellSize;
  int gap;
  float bombChance;

  int width() const { return size.x * cellSize + (size.x - 1) * gap; }
  int height() const { return size.y * cellSize + (size.y - 1) * gap; }
};

class MinesweeperGrid : public IDisplayObject {
  study_sdl3::minesweeper::MinesweeperEvents _events;
  study_sdl3::Random _random{};

  MinesweeperGridProps _props;
  MinesweeperCellStyle _cellStyle;

  std::vector<std::unique_ptr<MinesweeperCell>> _cells;
  std::vector<Vec2i> _bombs;

  int _cellsToClear{};

public:
  explicit MinesweeperGrid(
      const RectTransform transform, const MinesweeperGridProps props,
      const MinesweeperCellStyle cellStyle,
      const study_sdl3::minesweeper::MinesweeperEvents &events,
      SurfaceHandle bombImage, SurfaceHandle flagImage, FontHandle font)
      : IDisplayObject{transform}, _events{events}, _props{props},
        _cellStyle{cellStyle} {
    if (!hasArea(_props.size) || _props.cellSize <= 0 ||
        _props.gap < 0 || _props.bombChance < 0.0f || _props.bombChance > 1.0f)
      throw std::invalid_argument(
          "MinesweeperGridProps contains invalid dimensions or bomb chance");

    _cells.reserve(_props.size.y * _props.size.x);
    for (int row{0}; row < _props.size.y; ++row) {
      for (int col{0}; col < _props.size.x; ++col) {
        const int spacing{static_cast<int>(_props.cellSize) + _props.gap};

        const bool bomb = _random.get(0.0f, 1.0f) < _props.bombChance;

        _cells.emplace_back(std::make_unique<MinesweeperCell>(
            rect(transform.position.x + spacing * col,
                 transform.position.y + spacing * row, _props.cellSize,
                 _props.cellSize),
            bomb, false, Vec2i{col, row}, *this, _cellStyle, _events, bombImage,
            flagImage, font));

        if (bomb)
          _bombs.emplace_back(Vec2i{col, row});
      }
    }

    _cellsToClear =
        _props.size.y * _props.size.x - static_cast<int>(_bombs.size());

    for (const auto gridPos : _bombs) {
      for (int i = -1; i <= 1; ++i) {
        for (int j = -1; j <= 1; ++j) {
          if (i == 0 && j == 0)
            continue;
          Vec2i target{gridPos.x + i, gridPos.y + j};
          if (inBoundsExclusive(target, _props.size))
            getCellAt(target).incrementAdjacentBombs();
        }
      }
    }
  }
  ~MinesweeperGrid() = default;

  MinesweeperCell &getCellAt(const Vec2i &gridPos) {
    if (!inBoundsExclusive(gridPos, _props.size))
      throw std::out_of_range("MinesweeperGrid coordinate out of bounds");
    return *_cells.at(
        static_cast<std::size_t>(gridPos.y * _props.size.x + gridPos.x));
  }

  MinesweeperCell &getCellAt(const int x, const int y) {
    return getCellAt(Vec2i{x, y});
  }

  const MinesweeperCell &getCellAt(const Vec2i &gridPos) const {
    if (!inBoundsExclusive(gridPos, _props.size))
      throw std::out_of_range("MinesweeperGrid coordinate out of bounds");
    return *_cells.at(
        static_cast<std::size_t>(gridPos.y * _props.size.x + gridPos.x));
  }

  const MinesweeperCell &getCellAt(const int x, const int y) const {
    return getCellAt(Vec2i{x, y});
  }

  int getBombCount() const { return static_cast<int>(_bombs.size()); }
  const MinesweeperGridProps &getProps() const { return _props; }
  const MinesweeperCellStyle &getCellStyle() const { return _cellStyle; }

  void render(SDL_Surface &targetSurface) override {
    for (const auto &cell : _cells) {
      cell->render(targetSurface);
    }
  }

  EventResult handleEvent(const SDL_Event &event) override {
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
      SDL_Event gameLost{.user = {.type = _events.gameLost}};
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
        SDL_Event gameWon{.user = {.type = _events.gameWon}};
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
};
