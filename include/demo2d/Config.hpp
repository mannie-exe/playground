#pragma once

#include <string_view>

#include <math/Color.hpp>
#include <platform/Presentation.hpp>

namespace playground::demo2d::config {
inline constexpr std::string_view gameName{"Demo 2D"};
inline constexpr std::string_view windowTitle{"Demo 2D"};
inline constexpr playground::platform::AppViewPolicy viewPolicy{
    .initialSizing = playground::platform::InitialWindowSizing::Preferred,
    .preferredWindowSize = playground::math::Vec2i{960, 760}};
inline constexpr auto windowMode = playground::platform::WindowMode::Windowed;
inline constexpr playground::math::ColorRGBA8 clearColor{50, 50, 50, 255};

inline constexpr playground::math::ColorRGBA8 textColor{255, 255, 0, 255};
inline constexpr float textSize{42.0f};
inline constexpr std::string_view textValue{"Wow!"};

} // namespace playground::demo2d::config
