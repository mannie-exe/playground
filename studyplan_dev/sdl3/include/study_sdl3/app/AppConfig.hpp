#pragma once

#include <string_view>

#include <study_sdl3/support/SDLPrimitives.hpp>

namespace study_sdl3::config {
inline constexpr std::string_view defaultWindowTitle{"study_sdl3"};
inline constexpr Vec2i defaultWindowSize{800, 600};
inline constexpr bool defaultWindowResizable{true};
inline constexpr bool defaultWindowFullscreen{false};
} // namespace study_sdl3::config
