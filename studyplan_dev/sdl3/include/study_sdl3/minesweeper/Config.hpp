#pragma once

#include <string_view>
#include <vector>

#include <SDL3/SDL_pixels.h>
#include <study_sdl3/support/SDLPrimitives.hpp>

namespace study_sdl3::minesweeper::config {
inline constexpr std::string_view gameName{"Minesweeper"};

inline constexpr Vec2i gridSize{7, 7};

inline constexpr float bombChance{0.12f};

inline constexpr int cellSize{96};
inline constexpr int gridGap{16};
inline constexpr int outerPadding{24};

inline constexpr int footerHeight{cellSize};
inline constexpr int footerCounterWidth{cellSize * 2};
inline constexpr int footerGap{gridGap * 2};
inline constexpr int iconPadding{gridGap / 2};

constexpr Vec2i calculateWindowSize(Vec2i gridSize, int outerPadding,
                                    int footerHeight, int footerVerticalGap) {
  return Vec2i{gridSize.x + outerPadding * 2, gridSize.y + outerPadding +
                                                  footerVerticalGap +
                                                  footerHeight + outerPadding};
}

inline constexpr std::string_view windowTitle{"Minesweeper"};
inline constexpr bool windowResizable{false};
inline constexpr bool windowFullscreen{false};

inline constexpr SDL_Color bgColor{170, 170, 170, 255};
inline constexpr SDL_Color bombBgColor{210, 80, 115, 255};
inline constexpr SDL_Color flagCounterIconColor{bombBgColor};
inline constexpr SDL_Color flagCounterLabelColor{255, 255, 255, 255};
inline constexpr SDL_Color revealedBgColor{80, 210, 120, 255};
inline constexpr SDL_Color newGameLabelColor{revealedBgColor};
inline constexpr SDL_Color buttonBaseColor{200, 200, 200, 255};
inline constexpr SDL_Color buttonHoverColor{220, 220, 220, 255};
inline constexpr SDL_Color buttonActiveColor{232, 232, 232, 255};
inline constexpr SDL_Color buttonClearedColor{240, 240, 240, 255};
inline const std::vector<SDL_Color> cellLabelColors{
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
    "assets/minesweeper/images/bomb.svg"};
inline constexpr std::string_view flagImagePath{
    "assets/minesweeper/images/flag.svg"};
inline constexpr std::string_view baseFontPath{
    "assets/fonts/jurriaan_3d-fill.ttf"};
inline constexpr std::string_view titleFontPath{
    "assets/fonts/jurriaan_3d-shaded.ttf"};

} // namespace study_sdl3::minesweeper::config
