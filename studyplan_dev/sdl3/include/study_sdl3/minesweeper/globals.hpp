#pragma once

#include <string>

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_pixels.h>

namespace Minesweeper {
inline const std::string GAME_NAME{"Minesweeper"};

inline constexpr int WINDOW_WIDTH{400};
inline constexpr int WINDOW_HEIGHT{200};

inline constexpr SDL_Color BG_COLOR{170, 170, 170, 255};
inline constexpr SDL_Color BUTTON_COLOR{200, 200, 200, 255};
inline constexpr SDL_Color BUTTON_HOVER_COLOR{220, 220, 220, 255};
inline constexpr SDL_Color BUTTON_ACTIVE_COLOR{232, 232, 232, 255};

inline const std::string BASE_PATH{SDL_GetBasePath()};

inline const std::string BOMB_IMAGE{BASE_PATH + "assets/images/bomb.png"};
inline const std::string FLAG_IMAGE{BASE_PATH + "assets/images/flag.png"};

inline const std::string FONT_BASE{BASE_PATH +
                                   "assets/fonts/jurriaan_3d-fill.ttf"};
inline const std::string FONT_TITLE{BASE_PATH +
                                    "assets/fonts/jurriaan_3d-shaded.ttf"};
} // namespace Minesweeper
