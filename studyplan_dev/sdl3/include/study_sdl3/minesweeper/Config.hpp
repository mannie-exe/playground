#pragma once

#include <string_view>

#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_video.h>

namespace study_sdl3::minesweeper::config {
inline constexpr std::string_view gameName{"Minesweeper"};

inline constexpr int gridColumns{8};
inline constexpr int gridRows{4};
inline constexpr int cellSize{100};
inline constexpr int padding{5};

inline constexpr int gridHeight{cellSize * gridRows + padding * (gridRows - 1)};
inline constexpr int gridWidth{cellSize * gridColumns +
                               padding * (gridColumns - 1)};

inline constexpr std::string_view windowTitle{"Minesweeper"};
inline constexpr SDL_Point windowSize{gridWidth + padding * 2,
                                      gridHeight + padding * 2};
inline constexpr bool windowResizable{true};
inline constexpr bool windowFullscreen{false};

inline constexpr SDL_Color backgroundColor{170, 170, 170, 255};
inline constexpr SDL_Color buttonBaseColor{200, 200, 200, 255};
inline constexpr SDL_Color buttonHoverColor{220, 220, 220, 255};
inline constexpr SDL_Color buttonActiveColor{232, 232, 232, 255};

inline constexpr std::string_view bombImagePath{
    "assets/minesweeper/images/bomb.png"};
inline constexpr std::string_view flagImagePath{
    "assets/minesweeper/images/flag.png"};
inline constexpr std::string_view baseFontPath{
    "assets/fonts/jurriaan_3d-fill.ttf"};
inline constexpr std::string_view titleFontPath{
    "assets/fonts/jurriaan_3d-shaded.ttf"};
} // namespace study_sdl3::minesweeper::config
