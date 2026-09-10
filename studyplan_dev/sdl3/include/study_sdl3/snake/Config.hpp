#pragma once

#include <string_view>

#include <study_sdl3/support/SDLPrimitives.hpp>

namespace study_sdl3::snake::config {

inline constexpr std::string_view gameName{"Snake"};
inline constexpr std::string_view windowTitle{"Snake"};
inline constexpr Vec2i windowSize{800, 600};
inline constexpr bool windowResizable{true};
inline constexpr bool windowFullscreen{false};

}
