#pragma once

#include <string_view>

#include <math/Color.hpp>
#include <platform/Presentation.hpp>

namespace playground::snake::config {

inline constexpr std::string_view gameName{"Snake"};
inline constexpr std::string_view windowTitle{"Snake"};
inline constexpr playground::platform::AppViewPolicy viewPolicy{.resizable =
                                                                    true};
inline constexpr auto windowMode = playground::platform::WindowMode::Windowed;
inline constexpr playground::math::ColorRGBA8 clearColor{8, 16, 8, 255};

} // namespace playground::snake::config
