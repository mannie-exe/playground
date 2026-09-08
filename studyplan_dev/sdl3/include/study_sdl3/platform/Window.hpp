#pragma once

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_video.h>

#include <format>
#include <string>

#include <study_sdl3/support/SDLError.hpp>
#include <study_sdl3/support/SDLResource.hpp>

using WindowResource = SDLResource<SDL_Window, SDL_DestroyWindow>;

class Window {
  WindowResource _window;
  SDL_Point size;

public:
  Window(std::string title, SDL_Point initializeSize = SDL_Point{800, 600},
         SDL_WindowFlags windowFlags = 0)
      : _window{createWindow(this, title, initializeSize, windowFlags)},
        size{initializeSize} {
    if (!_window) {
      throwSDLError(
          std::format("Window@{} failed to create SDL_Window", (void *)this));
    }
    getSurface();
    updateSurface();
  }

  SDL_Surface *getSurface() const {
    SDL_Surface *surface = SDL_GetWindowSurface(_window.get());
    if (!surface) {
      throwSDLError(std::format("Window@{} failed to get SDL_Window surface",
                                (void *)this));
    }
    return surface;
  }

  SDL_Point getSurfaceSize() const {
    SDL_Surface *surface = getSurface();
    return SDL_Point{surface->w, surface->h};
  }

  void setTitle(const std::string &title) {
    if (!SDL_SetWindowTitle(_window.get(), title.c_str()))
      throwSDLError(
          std::format("Window@{} failed to set window title", (void *)this));
  }

  void setSize(SDL_Point newSize) {
    if (newSize.x <= 0 || newSize.y <= 0)
      return;

    if (newSize.x == size.x && newSize.y == size.y)
      return;

    if (!SDL_SetWindowSize(_window.get(), newSize.x, newSize.y))
      throwSDLError(
          std::format("Window@{} failed to set window size", (void *)this));

    size = newSize;
  }

  void setResizable(bool resizable) {
    if (!SDL_SetWindowResizable(_window.get(), resizable))
      throwSDLError(std::format("Window@{} failed to set window resizable",
                                (void *)this));
  }

  void setFullscreen(bool fullscreen) {
    if (!SDL_SetWindowFullscreen(_window.get(), fullscreen))
      throwSDLError(std::format("Window@{} failed to set window fullscreen",
                                (void *)this));
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
  WindowResource createWindow(void *owner, const std::string &title,
                              SDL_Point size, SDL_WindowFlags flags = 0) {
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
};
