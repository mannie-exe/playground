#pragma once

#include <format>

#include <SDL3/SDL_events.h>

#include <study_sdl3/support/SDLError.hpp>

namespace study_sdl3::minesweeper {

struct MinesweeperEvents {
  Uint32 cellHit;
  Uint32 cellCleared;
  Uint32 bombPlaced;
  Uint32 bombDetonated;
  Uint32 flagToggled;
  Uint32 gameWon;
  Uint32 gameLost;
  Uint32 newGameRequested;
};

inline MinesweeperEvents registerMinesweeperEvents() {
  constexpr int eventCount{8};
  const Uint32 firstEvent{SDL_RegisterEvents(eventCount)};

  if (firstEvent == static_cast<Uint32>(-1))
    throwSDLError(
        std::format("Failed to register {} Minesweeper events", eventCount));

  return MinesweeperEvents{.cellHit = firstEvent,
                           .cellCleared = firstEvent + 1,
                           .bombPlaced = firstEvent + 2,
                           .bombDetonated = firstEvent + 3,
                           .flagToggled = firstEvent + 4,
                           .gameWon = firstEvent + 5,
                           .gameLost = firstEvent + 6,
                           .newGameRequested = firstEvent + 7};
}

inline const MinesweeperEvents &events() {
  static const MinesweeperEvents registeredEvents{registerMinesweeperEvents()};
  return registeredEvents;
}

} // namespace study_sdl3::minesweeper
