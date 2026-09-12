#pragma once

#include <string_view>

#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_rect.h>

namespace study_sdl3::demo::config {
inline constexpr std::string_view gameName{"Demo"};
inline constexpr std::string_view windowTitle{"Sup"};
inline constexpr SDL_Point windowSize{750, 930};
inline constexpr bool windowResizable{true};
inline constexpr bool windowFullscreen{false};
inline constexpr SDL_Color clearColor{50, 50, 50, 255};

inline constexpr SDL_Color textColor{255, 255, 0, 255};
inline constexpr float textSize{42.0f};
inline constexpr std::string_view textValue{"Wow!"};

inline constexpr std::string_view imagePath{"assets/demo/images/IMG_6239.PNG"};
inline constexpr std::string_view fontPath{"assets/fonts/LBRITE.TTF"};
} // namespace study_sdl3::demo::config
