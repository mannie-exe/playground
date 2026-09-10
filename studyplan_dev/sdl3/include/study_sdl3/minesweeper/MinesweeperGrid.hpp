#pragma once

#include <format>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <SDL3/SDL_log.h>
#include <SDL3/SDL_rect.h>

#include <study_sdl3/interfaces/IDisplayObject.hpp>
#include <study_sdl3/interfaces/IInteractable.hpp>
#include <study_sdl3/minesweeper/Config.hpp>
#include <study_sdl3/minesweeper/MinesweeperCell.hpp>
#include <study_sdl3/minesweeper/MinesweeperEvents.hpp>
#include <study_sdl3/support/Random.hpp>
#include <study_sdl3/support/SDLPrimitives.hpp>

class MinesweeperGrid : public IDisplayObject {
  study_sdl3::minesweeper::MinesweeperEvents _events;

  std::vector<std::unique_ptr<MinesweeperCell>> _cells;
  std::vector<Vec2i> _bombs;
  study_sdl3::Random _random;
  int _rows{};
  int _cols{};

public:
  explicit MinesweeperGrid(
      const RectTransform transform,
      const study_sdl3::minesweeper::MinesweeperEvents &events,
      const std::string &bombImagePath, Font &font,
      int rows = study_sdl3::minesweeper::config::gridRows,
      int cols = study_sdl3::minesweeper::config::gridColumns)
      : IDisplayObject{transform}, _events{events}, _random{}, _rows{rows},
        _cols{cols} {
    _cells.reserve(rows * cols);
    for (int row{0}; row < rows; ++row) {
      for (int col{0}; col < cols; ++col) {
        constexpr int spacing{
            static_cast<int>(study_sdl3::minesweeper::config::cellSize) +
            study_sdl3::minesweeper::config::padding};

        const bool bomb = _random.get(0.0f, 1.0f) <=
                                  study_sdl3::minesweeper::config::bombChance
                              ? true
                              : false;

        _cells.emplace_back(std::make_unique<MinesweeperCell>(
            rect(transform.position.x + spacing * col,
                 transform.position.y + spacing * row,
                 study_sdl3::minesweeper::config::cellSize,
                 study_sdl3::minesweeper::config::cellSize),
            bomb, Vec2i{col, row}, *this, _events, bombImagePath, font));

        if (bomb)
          _bombs.emplace_back(Vec2i{col, row});
      }
    }

    for (const auto gridPos : _bombs) {
      for (int i = -1; i <= 1; ++i) {
        for (int j = -1; j <= 1; ++j) {
          if (i == 0 && j == 0)
            continue;
          Vec2i target{gridPos.x + i, gridPos.y + j};
          if (target.x >= 0 && target.x < _cols && target.y >= 0 &&
              target.y < _rows)
            getCellAt(target).incrementAdjacentBombs();
        }
      }
    }
  }
  ~MinesweeperGrid() = default;

  MinesweeperCell &getCellAt(const Vec2i &gridPos) {
    if (gridPos.x < 0 || gridPos.x >= _cols || gridPos.y < 0 ||
        gridPos.y >= _rows)
      throw std::out_of_range("MinesweeperGrid coordinate out of bounds");
    return *_cells.at(static_cast<std::size_t>(gridPos.y * _cols + gridPos.x));
  }

  const MinesweeperCell &getCellAt(const Vec2i &gridPos) const {
    if (gridPos.x < 0 || gridPos.x >= _cols || gridPos.y < 0 ||
        gridPos.y >= _rows)
      throw std::out_of_range("MinesweeperGrid coordinate out of bounds");
    return *_cells.at(static_cast<std::size_t>(gridPos.y * _cols + gridPos.x));
  }

  MinesweeperCell &getCellAt(const int x, const int y) {
    return getCellAt(Vec2i{x, y});
  }

  void render(SDL_Surface &targetSurface) override {
    for (const auto &cell : _cells) {
      cell->render(targetSurface);
    }
  }

  EventResult handleEvent(const SDL_Event &event) override {
    EventResult result{EventResult::Ignored};

    if (event.type == _events.cellHit) {
      SDL_Log("%s", std::format("Cell@{} hit: {}", event.user.data2,
                                *static_cast<SDL_Point *>(event.user.data1))
                        .c_str());
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
