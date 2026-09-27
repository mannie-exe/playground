#pragma once

#include <functional>
#include <optional>
#include <vector>

#include <minesweeper/MinesweeperModel.hpp>
#include <minesweeper/ViewResources.hpp>
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
  const MinesweeperModel &_model;
  playground::math::Vec2i _gridPos;
  MinesweeperCellStyle _style;
  playground::minesweeper::ViewResources _resources;
  playground::math::Size2 _labelSize;
  std::function<void(playground::math::Vec2i, int)> _activate;

  playground::ui::Text *_label{};
  playground::ui::Node *_bombIcon{};
  playground::ui::Node *_flagIcon{};
  std::optional<std::uint64_t> _revision;

protected:
  void onDefaultEvent(playground::ui::UIEvent &) override;

public:
  MinesweeperCell(float cellSize, const MinesweeperModel &,
                  playground::math::Vec2i, const MinesweeperCellStyle &,
                  const playground::minesweeper::ViewResources &,
                  std::function<void(playground::math::Vec2i, int)>);

  const MinesweeperCellState &state() const { return _model.cellAt(_gridPos); }

  int adjacentBombs() const { return state().adjacentBombs; }

  bool isBomb() const { return state().bomb; }

  bool isFlagged() const { return state().flagged; }

  bool isCleared() const { return state().cleared; }

  bool isRevealed() const { return state().revealed; }

  void synchronize();
  playground::ui::SemanticState semanticState() const override;
  playground::ui::ActionResult
  performAction(const playground::ui::UIAction &,
                playground::ui::ActionSource) override;
};
