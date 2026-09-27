#pragma once

#include <array>
#include <string_view>

#include <SDL3/SDL_scancode.h>

#include <app/AppTypes.hpp>

namespace playground::menu {
struct Entry {
  AppId app;
  SDL_Scancode key;
  std::string_view shortcut;
};

inline constexpr std::array entries{
    Entry{AppId::Demo2D, SDL_SCANCODE_1, "1"},
    Entry{AppId::Demo3D, SDL_SCANCODE_2, "2"},
    Entry{AppId::Minesweeper, SDL_SCANCODE_3, "3"},
    Entry{AppId::RockPaperScissors, SDL_SCANCODE_4, "4"},
    Entry{AppId::Snake, SDL_SCANCODE_5, "5"}};

inline constexpr float contentWidth{360};
inline constexpr float padding{24};
inline constexpr float gap{12};
inline constexpr float buttonHeight{48};
} // namespace playground::menu
