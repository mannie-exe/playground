#pragma once

#include <string_view>

#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_video.h>

namespace study_sdl3::config {
inline constexpr std::string_view defaultWindowTitle{"study_sdl3"};
inline constexpr SDL_Point defaultWindowSize{800, 600};
inline constexpr bool defaultWindowResizable{true};
inline constexpr bool defaultWindowFullscreen{false};
} // namespace study_sdl3::config
