#pragma once

#include <optional>

#include <minesweeper/ActionButton.hpp>
#include <minesweeper/FlagCounter.hpp>
#include <minesweeper/MinesweeperGrid.hpp>

struct MinesweeperUILayoutProps {
  int padding;
  int footerHeight;
  int footerCounterWidth;
  int footerGap;
  int actionWidth;
};

struct MinesweeperUIProps {
  MinesweeperGridProps grid;
  MinesweeperCellStyle cell;
  ActionButtonProps actionButton;
  FlagCounterProps flagCounter;
  MinesweeperUILayoutProps layout;
};

class MinesweeperUI : public playground::ui::Box {
  MinesweeperModel &_model;
  MinesweeperUIProps _props;
  playground::minesweeper::ViewResources _resources;

  MinesweeperGrid *_grid{};
  ActionButton *_newGameButton{};
  FlagCounter *_flagCounter{};
  playground::ui::Text *_status{};

  std::optional<std::uint64_t> _revision;
  bool _resetRequested{};
  bool _menuRequested{};

public:
  MinesweeperUI(MinesweeperUIProps props, MinesweeperModel &model,
                const playground::minesweeper::ViewResources &resources);

  const MinesweeperUIProps &props() const { return _props; }

  const MinesweeperGrid &grid() const { return *_grid; }

  MinesweeperGrid &grid() { return *_grid; }

  const FlagCounter &flagCounter() const { return *_flagCounter; }

  const ActionButton &newGameButton() const { return *_newGameButton; }

  GameState gameState() const { return _model.state(); }

  bool isPlaying() const { return gameState() == GameState::Playing; }

  void synchronize();

  // Called by the app after dispatch, never while walking/routing this tree.
  bool processPendingActions();

  bool isMenuRequested() const noexcept { return _menuRequested; }

  void reset();

private:
  std::unique_ptr<MinesweeperGrid> makeGrid() {
    return std::make_unique<MinesweeperGrid>(_props.grid, _props.cell, _model,
                                             _resources);
  }
};
