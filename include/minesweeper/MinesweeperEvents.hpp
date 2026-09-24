#pragma once

#include <format>
#include <limits>
#include <optional>
#include <unordered_map>

#include <SDL3/SDL_events.h>

#include <math/Geometry2D.hpp>
#include <support/SDLError.hpp>

namespace playground::minesweeper {

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

struct FlagChange {
  playground::math::Vec2i position;
  bool flagged;
  Sint32 grid;
};

inline auto &pendingFlags() {
  static std::unordered_map<Sint32, FlagChange> changes;
  return changes;
}
inline Sint32 nextGridGeneration() {
  static Sint32 generation{};
  if (generation == std::numeric_limits<Sint32>::max())
    throw std::overflow_error("Grid generation exhausted");
  return ++generation;
}
inline void publishFlagChange(const MinesweeperEvents &events,
                              FlagChange change) {
  static Sint32 token{};
  if (token == std::numeric_limits<Sint32>::max()) {
    if (!pendingFlags().empty())
      throw std::overflow_error("Flag event tokens exhausted");
    token = 0;
  }
  const Sint32 id = ++token;
  pendingFlags().emplace(id, change);
  SDL_Event event{.user = {.type = events.flagToggled, .code = id}};
  if (!SDL_PushEvent(&event))
    pendingFlags().erase(id);
}
inline std::optional<FlagChange> takeFlagChange(Sint32 id) {
  const auto it = pendingFlags().find(id);
  if (it == pendingFlags().end())
    return {};
  const auto result = it->second;
  pendingFlags().erase(it);
  return result;
}
inline void discardGridNotifications(Sint32 grid) {
  std::erase_if(pendingFlags(),
                [&](const auto &entry) { return entry.second.grid == grid; });
}

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

} // namespace playground::minesweeper
