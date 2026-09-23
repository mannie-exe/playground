#pragma once

#include <minesweeper/FlagCounter.hpp>
#include <minesweeper/MinesweeperGrid.hpp>
#include <minesweeper/NewGameButton.hpp>
#include <optional>

enum class GameState { Playing, Won, Lost };

struct MinesweeperUILayoutProps {
  int padding;
  int footerHeight;
  int footerCounterWidth;
  int footerGap;
};

struct MinesweeperUIProps {
  MinesweeperGridProps grid;
  MinesweeperCellStyle cell;
  NewGameButtonProps newGameButton;
  FlagCounterProps flagCounter;
  MinesweeperUILayoutProps layout;
};

struct MinesweeperUIPatch {
  std::optional<GameState> state;
};

class MinesweeperUI : public playground::ui::Box {
  playground::minesweeper::MinesweeperEvents _events;
  MinesweeperUIProps _props;
  playground::minesweeper::ViewResources _resources;
  MinesweeperGrid::BombSelector _selectBomb;

  playground::ui::VStack *_column{};
  MinesweeperGrid *_grid{};
  NewGameButton *_newGameButton{};
  FlagCounter *_flagCounter{};

  GameState _state{GameState::Playing};
  bool _resetRequested{};

public:
  MinesweeperUI(MinesweeperUIProps props,
                const playground::minesweeper::MinesweeperEvents &events,
                const playground::minesweeper::ViewResources &resources,
                MinesweeperGrid::BombSelector selectBomb = {});

  const MinesweeperUIProps &getProps() const { return _props; }
  const MinesweeperGrid &getGrid() const { return *_grid; }
  MinesweeperGrid &getGrid() { return *_grid; }
  const FlagCounter &getFlagCounter() const { return *_flagCounter; }
  const NewGameButton &getNewGameButton() const { return *_newGameButton; }
  GameState getGameState() const { return _state; }
  bool isPlaying() const { return _state == GameState::Playing; }

  void setGameState(GameState state);

  // Called by the app after dispatch, never while walking/routing this tree.
  bool processPendingActions();

  void reset();

  void applyPropsPatch(const MinesweeperUIPatch &patch) {
    if (patch.state)
      setGameState(*patch.state);
  }

  EventResult handleEvent(const SDL_Event &event);

private:
  std::unique_ptr<MinesweeperGrid> makeGrid() {
    return std::make_unique<MinesweeperGrid>(_props.grid, _props.cell, _events,
                                             _resources, _selectBomb);
  }
};
