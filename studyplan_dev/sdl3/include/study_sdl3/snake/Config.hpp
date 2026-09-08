#pragma once

#include <string_view>

#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_video.h>

namespace study_sdl3::snake::config {

inline constexpr std::string_view gameName{"Snake"};
inline constexpr std::string_view windowTitle{"Snake"};
inline constexpr SDL_Point windowSize{800, 600};
inline constexpr bool windowResizable{true};
inline constexpr bool windowFullscreen{false};

}
