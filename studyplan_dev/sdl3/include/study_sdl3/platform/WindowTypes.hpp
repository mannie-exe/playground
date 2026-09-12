#pragma once

#include <format>
#include <string>
#include <string_view>

#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_video.h>

#include <study_sdl3/app/AppConfig.hpp>
#include <study_sdl3/support/SDLPrimitives.hpp>

struct WindowConfig {
  std::string title{study_sdl3::config::defaultWindowTitle};
  Vec2i windowedSize{study_sdl3::config::defaultWindowSize};
  Vec2i windowedPosition{SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED};
  bool resizable{study_sdl3::config::defaultWindowResizable};
  bool fullscreen{study_sdl3::config::defaultWindowFullscreen};
  bool borderless{false};
  bool alwaysOnTop{false};
  bool focusable{true};
  bool highPixelDensity{false};
  bool hidden{false};
  bool maximized{false};
  bool minimized{false};
  bool transparent{false};
  bool mouseGrabbed{false};
  SDL_Color clearColor{study_sdl3::config::defaultClearColor};
};

struct WindowState {
  std::string title;
  Vec2i windowedSize;
  Vec2i windowedPosition;
  bool resizable;
  bool fullscreen;
  bool borderless;
  bool alwaysOnTop;
  bool focusable;
  bool highPixelDensity;
  bool hidden;
  bool maximized;
  bool minimized;
  bool transparent;
  bool mouseGrabbed;
  SDL_DisplayID display;
};

template <>
struct std::formatter<WindowConfig> : std::formatter<std::string_view> {
  auto format(const WindowConfig &config, format_context &ctx) const {
    return std::format_to(
        ctx.out(),
        "WindowConfig{{.title = \"{}\", .windowedSize = {}, "
        ".windowedPosition = {}, .resizable = {}, .fullscreen = {}, "
        ".borderless = {}, .alwaysOnTop = {}, .focusable = {}, "
        ".highPixelDensity = {}, .hidden = {}, .maximized = {}, "
        ".minimized = {}, .transparent = {}, .mouseGrabbed = {}, "
        ".clearColor = {}}}",
        config.title, config.windowedSize, config.windowedPosition,
        config.resizable, config.fullscreen, config.borderless,
        config.alwaysOnTop, config.focusable, config.highPixelDensity,
        config.hidden, config.maximized, config.minimized, config.transparent,
        config.mouseGrabbed, config.clearColor);
  }
};

template <>
struct std::formatter<WindowState> : std::formatter<std::string_view> {
  auto format(const WindowState &state, format_context &ctx) const {
    return std::format_to(
        ctx.out(),
        "WindowState{{.title = \"{}\", .windowedSize = {}, "
        ".windowedPosition = {}, .resizable = {}, .fullscreen = {}, "
        ".borderless = {}, .alwaysOnTop = {}, .focusable = {}, "
        ".highPixelDensity = {}, .hidden = {}, .maximized = {}, "
        ".minimized = {}, .transparent = {}, .mouseGrabbed = {}, "
        ".display = {}}}",
        state.title, state.windowedSize, state.windowedPosition, state.resizable,
        state.fullscreen, state.borderless, state.alwaysOnTop, state.focusable,
        state.highPixelDensity, state.hidden, state.maximized, state.minimized,
        state.transparent, state.mouseGrabbed, state.display);
  }
};
