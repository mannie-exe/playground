#pragma once

#include <format>
#include <string>
#include <string_view>

#include <SDL3/SDL_video.h>

#include <math/GeometryFormatters.hpp>
#include <platform/Presentation.hpp>

struct WindowConfig {
  std::string title{"Window"};
  playground::math::Vec2i windowedSize{800, 600};
  playground::math::Vec2i windowedPosition{SDL_WINDOWPOS_CENTERED,
                                           SDL_WINDOWPOS_CENTERED};
  bool resizable{true};
  bool fullscreen{false};
  bool borderless{false};
  bool alwaysOnTop{false};
  bool focusable{true};
  bool highPixelDensity{true};
  bool hidden{false};
  bool maximized{false};
  bool minimized{false};
  bool transparent{false};
  bool mouseGrabbed{false};
};

struct WindowState {
  std::string title;
  playground::math::Vec2i windowedSize;
  playground::math::Vec2i windowedPosition;
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
  playground::math::Vec2i actualSize;
  playground::math::Vec2i actualPosition;
  playground::math::Vec2i drawableSize;
  float displayScale;
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
        ".minimized = {}, .transparent = {}, .mouseGrabbed = {}}}",
        config.title, config.windowedSize, config.windowedPosition,
        config.resizable, config.fullscreen, config.borderless,
        config.alwaysOnTop, config.focusable, config.highPixelDensity,
        config.hidden, config.maximized, config.minimized, config.transparent,
        config.mouseGrabbed);
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
        ".display = {}, .actualSize = {}, .actualPosition = {}, "
        ".drawableSize = {}, .displayScale = {}}}",
        state.title, state.windowedSize, state.windowedPosition,
        state.resizable, state.fullscreen, state.borderless, state.alwaysOnTop,
        state.focusable, state.highPixelDensity, state.hidden, state.maximized,
        state.minimized, state.transparent, state.mouseGrabbed, state.display,
        state.actualSize, state.actualPosition, state.drawableSize,
        state.displayScale);
  }
};
