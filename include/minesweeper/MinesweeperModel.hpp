#pragma once

#include <cstdint>
#include <vector>

#include <math/Geometry2D.hpp>

enum class GameState { Playing, Won, Lost };

struct MinesweeperBoardProps {
  playground::math::Vec2i size;
  int bombs;
  void validate() const;
  bool operator==(const MinesweeperBoardProps &) const = default;
};

struct MinesweeperCellState {
  int adjacentBombs{};
  bool bomb{}, flagged{}, cleared{}, revealed{};
};

// App-owned game state; no node, SDL event or rendering dependencies.
class MinesweeperModel {
  MinesweeperBoardProps _props;

  std::vector<MinesweeperCellState> _cells;
  GameState _state{GameState::Playing};
  int _bombs{}, _flags{}, _remaining{};
  std::uint64_t _revision{};

  std::size_t index(playground::math::Vec2i) const;

public:
  explicit MinesweeperModel(MinesweeperBoardProps);

  const MinesweeperBoardProps &props() const noexcept { return _props; }

  const MinesweeperCellState &cellAt(playground::math::Vec2i) const;

  GameState state() const noexcept { return _state; }

  int bombCount() const noexcept { return _bombs; }

  int availableFlags() const noexcept { return _bombs - _flags; }

  std::uint64_t revision() const noexcept { return _revision; }

  void reset();
  void clear(playground::math::Vec2i);
  void toggleFlag(playground::math::Vec2i);
};
