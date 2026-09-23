#pragma once

#include <math/Color.hpp>
#include <platform/Presentation.hpp>
#include <string_view>

#include <math/Geometry2D.hpp>

namespace playground::rock_paper_scissors::config {
inline constexpr std::string_view gameName{"Rock Paper Scissors"};

inline constexpr std::string_view windowTitle{"Rock Paper Scissors"};
inline constexpr playground::platform::AppViewPolicy viewPolicy{.resizable =
                                                                    false};
inline constexpr auto windowMode = playground::platform::WindowMode::Windowed;
inline constexpr playground::math::ColorRGBA8 clearColor{0, 200, 200, 255};

inline constexpr int contentWidth{500};
inline constexpr int contentPadding{10};
inline constexpr int playerDisplayHeight{220};
inline constexpr int opponentDisplayHeight{140};
inline constexpr int contentHeight{playerDisplayHeight + opponentDisplayHeight};
inline constexpr playground::math::Vec2i playerDisplaySize{contentWidth,
                                                           playerDisplayHeight};
inline constexpr playground::math::Vec2i opponentDisplaySize{
    contentWidth, opponentDisplayHeight};
inline constexpr playground::math::Vec2i contentSize{contentWidth,
                                                     contentHeight};
inline constexpr playground::math::Vec2i windowSize{
    contentSize.x + contentPadding, contentSize.y + contentPadding};

inline constexpr playground::math::Vec2i menuButtonSize{200, 130};
inline constexpr int menuBackgroundHeight{
    static_cast<int>(menuButtonSize.y * 1.2)};

inline constexpr playground::math::Vec2i moveCardSize{180, 240};
inline constexpr int moveIconSize{128};
inline constexpr int turnIconSize{64};

inline constexpr float baseFontSize{32.0f};
inline constexpr float titleFontSize{baseFontSize * 2.5f};

inline constexpr std::string_view baseFontPath{
    "assets/fonts/jurriaan_3d-fill.ttf"};
inline constexpr std::string_view titleFontPath{
    "assets/fonts/jurriaan_3d-shaded.ttf"};

inline constexpr std::string_view rockImagePath{
    "assets/rock_paper_scissors/rock.svg"};
inline constexpr std::string_view paperImagePath{
    "assets/rock_paper_scissors/paper.svg"};
inline constexpr std::string_view scissorImagePath{
    "assets/rock_paper_scissors/scissor.svg"};
} // namespace playground::rock_paper_scissors::config
