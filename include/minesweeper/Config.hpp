#pragma once

#include <string_view>
#include <vector>

#include <math/Color.hpp>
#include <math/Geometry2D.hpp>
#include <platform/Presentation.hpp>

namespace playground::minesweeper::config {
inline constexpr std::string_view gameName{"Minesweeper"};

inline constexpr std::string_view windowTitle{"Minesweeper"};
inline constexpr playground::platform::AppViewPolicy viewPolicy{
    .initialSizing = playground::platform::InitialWindowSizing::FitContent,
    .resizable = false};
inline constexpr auto windowMode = playground::platform::WindowMode::Windowed;

inline constexpr playground::math::Vec2i gridSize{7, 7};

inline constexpr float bombChance{0.12f};

inline constexpr int cellSize{96};
inline constexpr int gridGap{16};
inline constexpr int outerPadding{24};

inline constexpr int footerHeight{cellSize};
inline constexpr int footerCounterWidth{cellSize * 2};
inline constexpr int footerGap{gridGap * 2};
inline constexpr int iconPadding{gridGap / 2};

inline constexpr playground::math::ColorRGBA8 bgColor{170, 170, 170, 255};
inline constexpr playground::math::ColorRGBA8 bombBgColor{210, 80, 115, 255};
inline constexpr playground::math::ColorRGBA8 flagCounterIconColor{bombBgColor};
inline constexpr playground::math::ColorRGBA8 flagCounterLabelColor{255, 255,
                                                                    255, 255};
inline constexpr playground::math::ColorRGBA8 revealedBgColor{80, 210, 120,
                                                              255};
inline constexpr playground::math::ColorRGBA8 newGameLabelColor{
    revealedBgColor};
inline constexpr playground::math::ColorRGBA8 buttonBaseColor{200, 200, 200,
                                                              255};
inline constexpr playground::math::ColorRGBA8 buttonHoverColor{220, 220, 220,
                                                               255};
inline constexpr playground::math::ColorRGBA8 buttonActiveColor{232, 232, 232,
                                                                255};
inline constexpr playground::math::ColorRGBA8 buttonClearedColor{240, 240, 240,
                                                                 255};
inline const std::vector<playground::math::ColorRGBA8> cellLabelColors{
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
} // namespace playground::minesweeper::config
