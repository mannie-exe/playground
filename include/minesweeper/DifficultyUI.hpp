#pragma once

#include <functional>
#include <string>
#include <vector>

#include <minesweeper/ActionButton.hpp>
#include <minesweeper/MinesweeperModel.hpp>
#include <ui/controls/NumberStepper.hpp>

class DifficultyUI : public playground::ui::Box {
  MinesweeperBoardProps &_draft;
  playground::minesweeper::ViewResources _resources;

  playground::ui::NumberStepper *_columns{}, *_rows{}, *_bombs{};
  playground::ui::Text *_columnsLabel{}, *_rowsLabel{}, *_bombsLabel{},
      *_status{};
  std::vector<playground::ui::Connection> _connections;

  void refresh();
  void select(MinesweeperBoardProps);

public:
  DifficultyUI(MinesweeperBoardProps &draft,
               const playground::minesweeper::ViewResources &,
               const ActionButtonProps &,
               std::function<void(MinesweeperBoardProps)> start,
               std::function<void()> launcher);
  void setStatus(std::string);
};
