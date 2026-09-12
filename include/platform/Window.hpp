#pragma once

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_video.h>

#include <format>
#include <string>

#include <platform/WindowTypes.hpp>
#include <support/SDLError.hpp>
#include <support/SDLResource.hpp>

using WindowResource = SDLResource<SDL_Window, SDL_DestroyWindow>;

class Window {
  WindowResource _window;
  WindowState _state;

public:
  explicit Window(const WindowConfig &config = WindowConfig{})
      : _window{createWindow(this, config)},
        _state{.title = config.title,
               .windowedSize = config.windowedSize,
               .windowedPosition = config.windowedPosition,
               .resizable = config.resizable,
               .fullscreen = config.fullscreen,
               .borderless = config.borderless,
               .alwaysOnTop = config.alwaysOnTop,
               .focusable = config.focusable,
               .highPixelDensity = config.highPixelDensity,
               .hidden = config.hidden,
               .maximized = config.maximized,
               .minimized = config.minimized,
               .transparent = config.transparent,
               .mouseGrabbed = config.mouseGrabbed,
               .display = SDL_GetDisplayForWindow(_window.get())} {
    if (!_window) {
      throwSDLError(
          std::format("Window@{} Failed to create SDL_Window", (void *)this));
    }
    // A maximized or minimized window reports its current display state here,
    // not necessarily the size and position it will restore to later.
    if (!config.fullscreen && !config.maximized && !config.minimized) {
      int x{};
      int y{};
      if (!SDL_GetWindowPosition(_window.get(), &x, &y))
        throwSDLError(std::format("Window@{} Failed to query window position",
                                  (void *)this));
      _state.windowedPosition = Vec2i{x, y};

      int width{};
      int height{};
      if (!SDL_GetWindowSize(_window.get(), &width, &height))
        throwSDLError(
            std::format("Window@{} Failed to query window size", (void *)this));
      _state.windowedSize = Vec2i{width, height};
    }
    getSurface();
    updateSurface();
  }

  const WindowState &state() const { return _state; }

  SDL_DisplayID display() const { return _state.display; }

  Vec2i windowedSize() const { return _state.windowedSize; }

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

  SDL_Surface *getSurface() const {
    SDL_Surface *surface = SDL_GetWindowSurface(_window.get());
    if (!surface) {
      throwSDLError(std::format("Window@{} Failed to get SDL_Window surface",
                                (void *)this));
    }
    return surface;
  }

  Vec2i getSurfaceSize() const {
    SDL_Surface *surface = getSurface();
    return Vec2i{surface->w, surface->h};
  }

  void setTitle(const std::string &title) {
    if (title == _state.title)
      return;

    if (!SDL_SetWindowTitle(_window.get(), title.c_str()))
      throwSDLError(
          std::format("Window@{} Failed to set window title", (void *)this));

    _state.title = title;
  }

  void setWindowedPosition(Vec2i position) {
    if (!SDL_SetWindowPosition(_window.get(), position.x, position.y))
      throwSDLError(
          std::format("Window@{} Failed to set window position", (void *)this));

    int x{};
    int y{};
    if (!SDL_GetWindowPosition(_window.get(), &x, &y))
      throwSDLError(std::format("Window@{} Failed to query window position",
                                (void *)this));

    _state.windowedPosition = Vec2i{x, y};
  }

  void setPosition(Vec2i position) { setWindowedPosition(position); }

  void setWindowedSize(Vec2i size) {
    if (!hasArea(size))
      return;

    if (size == _state.windowedSize)
      return;

    if (!SDL_SetWindowSize(_window.get(), size.x, size.y))
      throwSDLError(
          std::format("Window@{} Failed to set window size", (void *)this));

    _state.windowedSize = size;
  }

  void setSize(Vec2i size) { setWindowedSize(size); }

  void setResizable(bool resizable) {
    if (resizable == _state.resizable)
      return;

    if (!SDL_SetWindowResizable(_window.get(), resizable))
      throwSDLError(std::format("Window@{} Failed to set window resizable",
                                (void *)this));

    _state.resizable = resizable;
  }

  void setBorderless(bool borderless) {
    if (borderless == _state.borderless)
      return;
    if (!SDL_SetWindowBordered(_window.get(), !borderless))
      throwSDLError(std::format("Window@{} Failed to set window borderless",
                                (void *)this));
    _state.borderless = borderless;
  }

  void setAlwaysOnTop(bool alwaysOnTop) {
    if (alwaysOnTop == _state.alwaysOnTop)
      return;
    if (!SDL_SetWindowAlwaysOnTop(_window.get(), alwaysOnTop))
      throwSDLError(
          std::format("Window@{} Failed to set always-on-top", (void *)this));
    _state.alwaysOnTop = alwaysOnTop;
  }

  void setFocusable(bool focusable) {
    if (focusable == _state.focusable)
      return;
    if (!SDL_SetWindowFocusable(_window.get(), focusable))
      throwSDLError(
          std::format("Window@{} Failed to set focusable", (void *)this));
    _state.focusable = focusable;
  }

  void setMouseGrabbed(bool grabbed) {
    if (grabbed == _state.mouseGrabbed)
      return;
    if (!SDL_SetWindowMouseGrab(_window.get(), grabbed))
      throwSDLError(
          std::format("Window@{} Failed to set mouse grab", (void *)this));
    _state.mouseGrabbed = grabbed;
  }

  void setHidden(bool hidden) {
    if (hidden == _state.hidden)
      return;
    const bool success =
        hidden ? SDL_HideWindow(_window.get()) : SDL_ShowWindow(_window.get());
    if (!success)
      throwSDLError(
          std::format("Window@{} Failed to set hidden state", (void *)this));
    _state.hidden = hidden;
  }

  void setMaximized(bool maximized) {
    if (maximized == _state.maximized)
      return;

    const bool success = maximized ? SDL_MaximizeWindow(_window.get())
                                   : SDL_RestoreWindow(_window.get());
    if (!success)
      throwSDLError(
          std::format("Window@{} Failed to set maximized state", (void *)this));

    _state.maximized = maximized;
    if (maximized)
      _state.minimized = false;
  }

  void setMinimized(bool minimized) {
    if (minimized == _state.minimized)
      return;

    const bool success = minimized ? SDL_MinimizeWindow(_window.get())
                                   : SDL_RestoreWindow(_window.get());
    if (!success)
      throwSDLError(
          std::format("Window@{} Failed to set minimized state", (void *)this));

    _state.minimized = minimized;
    if (minimized)
      _state.maximized = false;
  }

  void setFullscreen(bool fullscreen) {
    if (fullscreen == _state.fullscreen)
      return;

    if (!SDL_SetWindowFullscreen(_window.get(), fullscreen))
      throwSDLError(std::format("Window@{} Failed to set window fullscreen",
                                (void *)this));

    _state.fullscreen = fullscreen;
    _state.display = SDL_GetDisplayForWindow(_window.get());
  }

  void handleEvent(const SDL_Event &event) {
    if (event.type != SDL_EVENT_WINDOW_RESIZED &&
        event.type != SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED &&
        event.type != SDL_EVENT_WINDOW_MOVED &&
        event.type != SDL_EVENT_WINDOW_DISPLAY_CHANGED &&
        event.type != SDL_EVENT_WINDOW_MAXIMIZED &&
        event.type != SDL_EVENT_WINDOW_MINIMIZED &&
        event.type != SDL_EVENT_WINDOW_RESTORED &&
        event.type != SDL_EVENT_WINDOW_ENTER_FULLSCREEN &&
        event.type != SDL_EVENT_WINDOW_LEAVE_FULLSCREEN)
      return;

    if (event.window.windowID != SDL_GetWindowID(_window.get()))
      return;

    if (event.type == SDL_EVENT_WINDOW_RESIZED && !_state.fullscreen)
      _state.windowedSize = Vec2i{event.window.data1, event.window.data2};

    if (event.type == SDL_EVENT_WINDOW_MOVED && !_state.fullscreen)
      _state.windowedPosition = Vec2i{event.window.data1, event.window.data2};

    if (event.type == SDL_EVENT_WINDOW_MAXIMIZED) {
      _state.maximized = true;
      _state.minimized = false;
    }
    if (event.type == SDL_EVENT_WINDOW_MINIMIZED) {
      _state.minimized = true;
      _state.maximized = false;
    }
    if (event.type == SDL_EVENT_WINDOW_RESTORED) {
      _state.maximized = false;
      _state.minimized = false;
    }

    if (event.type == SDL_EVENT_WINDOW_ENTER_FULLSCREEN)
      _state.fullscreen = true;

    if (event.type == SDL_EVENT_WINDOW_LEAVE_FULLSCREEN)
      _state.fullscreen = false;

    _state.display = SDL_GetDisplayForWindow(_window.get());
  }

  bool updateSurface() { return SDL_UpdateWindowSurface(_window.get()); }

  void
  clearSurface(bool skipUpdate = false,
               SDL_Color clearColor = playground::config::defaultClearColor) {
    SDL_Surface *surface = getSurface();
    const SDL_PixelFormatDetails *pixelFormat{
        SDL_GetPixelFormatDetails(surface->format)};
    SDL_FillSurfaceRect(surface, nullptr,
                        SDL_MapRGBA(pixelFormat, nullptr, clearColor.r,
                                    clearColor.g, clearColor.b, clearColor.a));
    if (skipUpdate)
      return;
    updateSurface();
  }

  Window(Window &&) noexcept = default;
  Window &operator=(Window &&) noexcept = default;

  Window(const Window &) = delete;
  Window &operator=(const Window &) = delete;

private:
  static WindowResource createWindow(void *owner, const WindowConfig &config) {
    if (!SDL_WasInit(SDL_INIT_VIDEO))
      throwSDLError(std::format(
          "Window@{} Failed to create window: SDL video not initialized",
          owner));

    SDL_PropertiesID properties{SDL_CreateProperties()};
    if (!properties)
      throwSDLError(
          std::format("Window@{} Failed to create SDL properties", owner));

    struct PropertiesGuard {
      SDL_PropertiesID value;
      ~PropertiesGuard() { SDL_DestroyProperties(value); }
    } propertiesGuard{properties};

    auto setBoolean = [&](const char *name, bool value) {
      if (!SDL_SetBooleanProperty(properties, name, value))
        throwSDLError(std::format("Window@{} Failed to set window property {}",
                                  owner, name));
    };
    auto setNumber = [&](const char *name, Sint64 value) {
      if (!SDL_SetNumberProperty(properties, name, value))
        throwSDLError(std::format("Window@{} Failed to set window property {}",
                                  owner, name));
    };

    if (!SDL_SetStringProperty(properties, SDL_PROP_WINDOW_CREATE_TITLE_STRING,
                               config.title.c_str()))
      throwSDLError(
          std::format("Window@{} Failed to set window title property", owner));

    setNumber(SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, config.windowedSize.x);
    setNumber(SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, config.windowedSize.y);
    setNumber(SDL_PROP_WINDOW_CREATE_X_NUMBER, config.windowedPosition.x);
    setNumber(SDL_PROP_WINDOW_CREATE_Y_NUMBER, config.windowedPosition.y);
    setBoolean(SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, config.resizable);
    setBoolean(SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, config.fullscreen);
    setBoolean(SDL_PROP_WINDOW_CREATE_BORDERLESS_BOOLEAN, config.borderless);
    setBoolean(SDL_PROP_WINDOW_CREATE_ALWAYS_ON_TOP_BOOLEAN,
               config.alwaysOnTop);
    setBoolean(SDL_PROP_WINDOW_CREATE_FOCUSABLE_BOOLEAN, config.focusable);
    setBoolean(SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN,
               config.highPixelDensity);
    setBoolean(SDL_PROP_WINDOW_CREATE_HIDDEN_BOOLEAN, config.hidden);
    setBoolean(SDL_PROP_WINDOW_CREATE_MAXIMIZED_BOOLEAN, config.maximized);
    setBoolean(SDL_PROP_WINDOW_CREATE_MINIMIZED_BOOLEAN, config.minimized);
    setBoolean(SDL_PROP_WINDOW_CREATE_TRANSPARENT_BOOLEAN, config.transparent);
    setBoolean(SDL_PROP_WINDOW_CREATE_MOUSE_GRABBED_BOOLEAN,
               config.mouseGrabbed);

    WindowResource window{SDL_CreateWindowWithProperties(properties)};
    if (!window)
      throwSDLError(std::format("Window@{} Failed to create window", owner));
    return window;
  }
};
