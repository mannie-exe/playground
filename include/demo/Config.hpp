#pragma once

#include <string_view>

#include <math/Color.hpp>
#include <platform/Presentation.hpp>

namespace playground::demo::config {
inline constexpr std::string_view gameName{"Demo"};
inline constexpr std::string_view windowTitle{"Sup"};
inline constexpr playground::platform::AppViewPolicy viewPolicy{
    .initialSizing = playground::platform::InitialWindowSizing::FitContent};
inline constexpr auto windowMode = playground::platform::WindowMode::Windowed;
inline constexpr playground::math::ColorRGBA8 clearColor{50, 50, 50, 255};

inline constexpr playground::math::ColorRGBA8 textColor{255, 255, 0, 255};
inline constexpr float textSize{42.0f};
inline constexpr std::string_view textValue{"Wow!"};

inline constexpr std::string_view imagePath{"assets/images/IMG_6239.PNG"};
inline constexpr std::string_view fontPath{"assets/fonts/LBRITE.TTF"};
} // namespace playground::demo::config
