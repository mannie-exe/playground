#pragma once

#include <cstddef>
#include <string_view>

#include <SDL3/SDL_pixels.h>

#include <study_sdl3/support/SDLPrimitives.hpp>

namespace study_sdl3::config {
inline constexpr std::string_view defaultWindowTitle{"study_sdl3"};
inline constexpr Vec2i defaultWindowSize{800, 600};
inline constexpr bool defaultWindowResizable{true};
inline constexpr bool defaultWindowFullscreen{false};
inline constexpr SDL_Color defaultClearColor{50, 50, 50, 255};

inline constexpr std::size_t maxCachedFonts{8};
inline constexpr std::size_t maxCachedImages{64};
inline constexpr std::size_t maxCachedVectors{32};
} // namespace study_sdl3::config
