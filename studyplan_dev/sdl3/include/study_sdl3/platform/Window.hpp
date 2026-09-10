#pragma once

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_video.h>

#include <format>
#include <string>

#include <study_sdl3/platform/WindowTypes.hpp>
#include <study_sdl3/support/SDLError.hpp>
#include <study_sdl3/support/SDLResource.hpp>

using WindowResource = SDLResource<SDL_Window, SDL_DestroyWindow>;

class Window {
  WindowResource _window;
  WindowState _state;

public:
  explicit Window(const WindowConfig &config = WindowConfig{})
      : _window{createWindow(this, config.title, config.windowedSize,
                             toSDLWindowFlags(config))},
        _state{.title = config.title,
               .windowedSize = config.windowedSize,
               .resizable = config.resizable,
               .fullscreen = config.fullscreen,
               .display = SDL_GetDisplayForWindow(_window.get())} {
    if (!_window) {
      throwSDLError(
          std::format("Window@{} failed to create SDL_Window", (void *)this));
    }
    getSurface();
    updateSurface();
  }

  const WindowState &state() const { return _state; }

  SDL_DisplayID display() const { return _state.display; }

  Vec2i windowedSize() const { return _state.windowedSize; }

  bool isResizable() const { return _state.resizable; }

  bool isFullscreen() const { return _state.fullscreen; }

  SDL_Surface *getSurface() const {
    SDL_Surface *surface = SDL_GetWindowSurface(_window.get());
    if (!surface) {
      throwSDLError(std::format("Window@{} failed to get SDL_Window surface",
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
          std::format("Window@{} failed to set window title", (void *)this));

    _state.title = title;
  }

  void setWindowedSize(Vec2i newSize) {
    if (newSize.x <= 0 || newSize.y <= 0)
      return;

    if (newSize == _state.windowedSize)
      return;

    if (!SDL_SetWindowSize(_window.get(), newSize.x, newSize.y))
      throwSDLError(
          std::format("Window@{} failed to set window size", (void *)this));

    _state.windowedSize = newSize;
  }

  void setSize(Vec2i newSize) {
    setWindowedSize(newSize);
  }

  void setResizable(bool resizable) {
    if (resizable == _state.resizable)
      return;

    if (!SDL_SetWindowResizable(_window.get(), resizable))
      throwSDLError(std::format("Window@{} failed to set window resizable",
                                (void *)this));

    _state.resizable = resizable;
  }

  void setFullscreen(bool fullscreen) {
    if (fullscreen == _state.fullscreen)
      return;

    if (!SDL_SetWindowFullscreen(_window.get(), fullscreen))
      throwSDLError(std::format("Window@{} failed to set window fullscreen",
                                (void *)this));

    _state.fullscreen = fullscreen;
    _state.display = SDL_GetDisplayForWindow(_window.get());
  }

  void handleEvent(const SDL_Event &event) {
    if (event.type != SDL_EVENT_WINDOW_RESIZED &&
        event.type != SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED &&
        event.type != SDL_EVENT_WINDOW_DISPLAY_CHANGED &&
        event.type != SDL_EVENT_WINDOW_ENTER_FULLSCREEN &&
        event.type != SDL_EVENT_WINDOW_LEAVE_FULLSCREEN)
      return;

    if (event.window.windowID != SDL_GetWindowID(_window.get()))
      return;

    if (event.type == SDL_EVENT_WINDOW_RESIZED && !_state.fullscreen)
      _state.windowedSize = Vec2i{event.window.data1, event.window.data2};

    if (event.type == SDL_EVENT_WINDOW_ENTER_FULLSCREEN)
      _state.fullscreen = true;

    if (event.type == SDL_EVENT_WINDOW_LEAVE_FULLSCREEN)
      _state.fullscreen = false;

    _state.display = SDL_GetDisplayForWindow(_window.get());
  }

  bool updateSurface() { return SDL_UpdateWindowSurface(_window.get()); }

  void clearSurface(bool skipUpdate = false,
                    SDL_Color clearColor = SDL_Color{50, 50, 50, 255}) {
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
  static WindowResource createWindow(void *owner, const std::string &title,
                                     Vec2i size,
                                     SDL_WindowFlags flags = 0) {
    if (!SDL_WasInit(SDL_INIT_VIDEO))
      throwSDLError(std::format(
          "Window@{} Failed to create window: SDL video not initialized",
          owner));

    WindowResource window{
        SDL_CreateWindow(title.c_str(), size.x, size.y, flags)};
    if (!window)
      throwSDLError(std::format("Window@{} Failed to create window", owner));
    return window;
  }

  static SDL_WindowFlags toSDLWindowFlags(const WindowConfig &config) {
    SDL_WindowFlags flags{0};

    if (config.resizable)
      flags |= SDL_WINDOW_RESIZABLE;
    if (config.fullscreen)
      flags |= SDL_WINDOW_FULLSCREEN;

    return flags;
  }
};
