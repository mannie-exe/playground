#pragma once

#include <string_view>
#include <vector>

#include <math/Color.hpp>
#include <math/Geometry2D.hpp>
#include <minesweeper/MinesweeperModel.hpp>
#include <platform/Presentation.hpp>

namespace playground::minesweeper::config {
inline constexpr std::string_view gameName{"Minesweeper"};

inline constexpr std::string_view windowTitle{"Minesweeper"};
inline constexpr playground::platform::AppViewPolicy viewPolicy{
    .initialSizing = playground::platform::InitialWindowSizing::FitContent,
    .resizable = false};
inline constexpr auto windowMode = playground::platform::WindowMode::Windowed;

inline constexpr MinesweeperBoardProps easy{{9, 9}, 10};
inline constexpr MinesweeperBoardProps hard{{16, 16}, 40};
inline constexpr int minimumDimension{2};
inline constexpr int maximumDimension{30};

inline constexpr int cellSize{48};
inline constexpr int gridGap{4};
inline constexpr int outerPadding{24};

inline constexpr int footerHeight{cellSize};
inline constexpr int footerCounterWidth{cellSize * 2};
inline constexpr int footerGap{12};
inline constexpr int iconPadding{4};
inline constexpr int actionWidth{144};

inline constexpr playground::math::ColorRGBA8 bgColor{170, 170, 170, 255};
inline constexpr playground::math::ColorRGBA8 bombBgColor{210, 80, 115, 255};
inline constexpr playground::math::ColorRGBA8 flagCounterIconColor{bombBgColor};
inline constexpr playground::math::ColorRGBA8 flagCounterLabelColor{255, 255,
                                                                    255, 255};
inline constexpr playground::math::ColorRGBA8 revealedBgColor{80, 210, 120,
                                                              255};
inline constexpr playground::math::ColorRGBA8 actionLabelColor{24, 24, 24, 255};
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

} // namespace playground::minesweeper::config
