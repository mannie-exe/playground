#pragma once

#include <cmath>
#include <functional>
#include <vector>

#include <minesweeper/MinesweeperEvents.hpp>
#include <minesweeper/ViewResources.hpp>
#include <platform/sdl/EventResult.hpp>
#include <ui/containers/ZStack.hpp>
#include <ui/controls/Button.hpp>

struct MinesweeperCellStyle {
  playground::math::ColorRGBA8 bombColor;
  playground::math::ColorRGBA8 revealedColor;
  playground::math::ColorRGBA8 clearedColor;

  playground::math::ColorRGBA8 flagColor{255, 255, 255, 255};
  int iconPadding{16};
  std::vector<playground::math::ColorRGBA8> labelColors;
  playground::ui::ButtonProps button;
};

class MinesweeperCell : public playground::ui::Button {
  playground::minesweeper::MinesweeperEvents _events;
  Sint32 _gridGeneration;
  std::function<EventResult(const SDL_Event &)> _publish;

  playground::math::Vec2i _gridPos;
  MinesweeperCellStyle _style;
  playground::minesweeper::ViewResources _resources;
  playground::math::Size2 _labelSize;

  int _adjacentBombs{};
  bool _bomb{};
  bool _flagged{};
  bool _cleared{};
  bool _revealed{};
  bool _dirty{};

  playground::ui::Text *_label{};
  playground::ui::Node *_bombIcon{};
  playground::ui::Node *_flagIcon{};

protected:
  void onDefaultEvent(playground::ui::UIEvent &event) override;

public:
  MinesweeperCell(float cellSize, bool bomb, playground::math::Vec2i position,
                  const MinesweeperCellStyle &style,
                  const playground::minesweeper::MinesweeperEvents &events,
                  const playground::minesweeper::ViewResources &resources,
                  Sint32 generation,
                  std::function<EventResult(const SDL_Event &)> publish);

  int getAdjacentBombs() const { return _adjacentBombs; }
  bool isBomb() const { return _bomb; }
  bool isFlagged() const { return _flagged; }
  bool isCleared() const { return _cleared; }
  bool isRevealed() const { return _revealed; }

  void incrementAdjacentBombs();

  void setFlagged(bool flagged = true);

  void setRevealed(bool revealed = true) { _revealed = revealed; }

  bool isAdjacent(playground::math::Vec2i position) const {
    return std::abs(position.x - _gridPos.x) <= 1 &&
           std::abs(position.y - _gridPos.y) <= 1 && position != _gridPos;
  }

  void clearCell(bool propagate = true);

  EventResult handleEvent(const SDL_Event &event);

private:
  void synchronizeIcons();
};
