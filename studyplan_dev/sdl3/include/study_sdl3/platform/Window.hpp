#pragma once

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_video.h>

#include <format>

#include <study_sdl3/support/SDLError.hpp>
#include <study_sdl3/support/SDLResource.hpp>

class Window {
  SDLResource<SDL_Window, SDL_DestroyWindow> _window;

public:
  Window(const char *title, int width = 800, int height = 600,
         SDL_WindowFlags windowFlags = 0)
      : _window{SDL_CreateWindow(title, width, height, windowFlags)} {
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
};
