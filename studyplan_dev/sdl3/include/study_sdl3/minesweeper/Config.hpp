#pragma once

#include <string_view>
#include <vector>

#include <SDL3/SDL_pixels.h>
#include <study_sdl3/support/SDLPrimitives.hpp>

namespace study_sdl3::minesweeper::config {
inline constexpr std::string_view gameName{"Minesweeper"};

inline constexpr int gridColumns{8};
inline constexpr int gridRows{8};

inline constexpr float bombChance{0.12f};

inline constexpr int cellSize{128};
inline constexpr int padding{16};
inline constexpr int gridHeight{cellSize * gridRows + padding * (gridRows - 1)};
inline constexpr int gridWidth{cellSize * gridColumns +
                               padding * (gridColumns - 1)};

inline constexpr int bombImagePadding{padding * 2};

inline constexpr std::string_view windowTitle{"Minesweeper"};
inline constexpr Vec2i windowSize{gridWidth + padding * 2,
                                  gridHeight + padding * 2};
inline constexpr bool windowResizable{false};
inline constexpr bool windowFullscreen{false};

inline constexpr SDL_Color backgroundColor{170, 170, 170, 255};
inline constexpr SDL_Color bombBackgroundColor{202, 130, 140, 255};
inline constexpr SDL_Color buttonBaseColor{200, 200, 200, 255};
inline constexpr SDL_Color buttonHoverColor{220, 220, 220, 255};
inline constexpr SDL_Color buttonActiveColor{232, 232, 232, 255};
inline constexpr SDL_Color buttonClearedColor{240, 240, 240, 255};
inline const std::vector<SDL_Color> labelColors{
    /* 0 */ {0, 0, 0, 255}, // Unused
    /* 1 */ {0, 1, 249, 255},
    /* 2 */ {1, 126, 1, 255},
    /* 3 */ {250, 1, 2, 255},
    /* 4 */ {1, 0, 128, 255},
    /* 5 */ {129, 1, 0, 255},
    /* 6 */ {0, 128, 128, 255},
    /* 7 */ {0, 0, 0, 255},
    /* 8 */ {128, 128, 128, 255}};

inline constexpr std::string_view bombImagePath{
    "assets/minesweeper/images/bomb.png"};
inline constexpr std::string_view flagImagePath{
    "assets/minesweeper/images/flag.png"};
inline constexpr std::string_view baseFontPath{
    "assets/fonts/jurriaan_3d-fill.ttf"};
inline constexpr std::string_view titleFontPath{
    "assets/fonts/jurriaan_3d-shaded.ttf"};

} // namespace study_sdl3::minesweeper::config
