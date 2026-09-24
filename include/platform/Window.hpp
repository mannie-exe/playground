#pragma once

#include <string>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>

#include <math/Geometry2D.hpp>
#include <platform/WindowTypes.hpp>
#include <support/SDLResource.hpp>

using WindowResource = SDLResource<SDL_Window, SDL_DestroyWindow>;

class Window {
  WindowResource _window;
  WindowState _state;
  playground::platform::WindowMode _requestedMode{
      playground::platform::WindowMode::Windowed};
  bool _applyingPreferences{};

public:
  explicit Window(const WindowConfig &config = WindowConfig{});

  const WindowState &state() const { return _state; }

  SDL_DisplayID display() const { return _state.display; }

  playground::math::Vec2i windowedSize() const { return _state.windowedSize; }

  bool isResizable() const { return _state.resizable; }

  bool isFullscreen() const { return _state.fullscreen; }
  bool isBorderless() const { return _state.borderless; }
  bool isAlwaysOnTop() const { return _state.alwaysOnTop; }
  bool isFocusable() const { return _state.focusable; }
  bool isHighPixelDensity() const { return _state.highPixelDensity; }
  bool isHidden() const { return _state.hidden; }
  bool isMaximized() const { return _state.maximized; }
  bool isMinimized() const { return _state.minimized; }
  bool isTransparent() const { return _state.transparent; }
  bool isMouseGrabbed() const { return _state.mouseGrabbed; }

  SDL_Window *get() const noexcept { return _window.get(); }
  playground::math::Vec2i getWindowSize() const;

  void setTitle(const std::string &title);

  void setWindowedPosition(playground::math::Vec2i position);

  void setWindowedSize(playground::math::Vec2i size);

  void setResizable(bool resizable);

  void setBorderless(bool borderless);

  void setAlwaysOnTop(bool alwaysOnTop);

  void setFocusable(bool focusable);

  void setMouseGrabbed(bool grabbed);

  void setHidden(bool hidden);

  void setMaximized(bool maximized);

  void setMinimized(bool minimized);

  void setFullscreen(bool fullscreen);

  void handleEvent(const SDL_Event &event);
  void refreshState();
  SDL_DisplayID
  resolveDisplay(const playground::platform::DisplayPreference &) const;
  playground::math::Rect
  usableBounds(const playground::platform::DisplayPreference &) const;
  void applyPreferences(const playground::platform::WindowPreferences &);
  void setMinimumSize(playground::math::Vec2i size);
  playground::platform::WindowMetrics metrics() const;

  Window(Window &&) noexcept = default;
  Window &operator=(Window &&) noexcept = default;

  Window(const Window &) = delete;
  Window &operator=(const Window &) = delete;

private:
  static WindowResource createWindow(void *owner, const WindowConfig &config);
};
