#pragma once

#include <format>
#include <string>
#include <string_view>

#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_video.h>

#include <study_sdl3/support/SDLPrimitives.hpp>

struct WindowConfig {
  std::string title{"study_sdl3"};
  Vec2i windowedSize{800, 600};
  bool resizable{true};
  bool fullscreen{false};
  SDL_Color clearColor{50, 50, 50, 255};
};

struct WindowState {
  std::string title{"study_sdl3"};
  Vec2i windowedSize{800, 600};
  bool resizable{true};
  bool fullscreen{false};
  SDL_DisplayID display{0};
};

template <>
struct std::formatter<WindowConfig> : std::formatter<std::string_view> {
  auto format(const WindowConfig &config, format_context &ctx) const {
    return std::format_to(
        ctx.out(),
        "WindowConfig{{.title = \"{}\", .windowedSize = {}, .resizable = {}, "
        ".fullscreen = {}, .clearColor = {}}}",
        config.title, config.windowedSize, config.resizable, config.fullscreen,
        config.clearColor);
  }
};

template <>
struct std::formatter<WindowState> : std::formatter<std::string_view> {
  auto format(const WindowState &state, format_context &ctx) const {
    return std::format_to(
        ctx.out(),
        "WindowState{{.title = \"{}\", .windowedSize = {}, .resizable = {}, "
        ".fullscreen = {}, .display = {}}}",
        state.title, state.windowedSize, state.resizable, state.fullscreen,
        state.display);
  }
};
