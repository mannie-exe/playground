#pragma once

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_video.h>

#include <format>
#include <string>

class Window {
  SDL_Window *_windowPrimary;

public:
  Window(const char *title, int width = 800, int height = 600,
         SDL_WindowFlags windowFlags = 0)
      : _windowPrimary{SDL_CreateWindow(title, width, height, windowFlags)} {
    if (!_windowPrimary) {
      std::string err{SDL_GetError()};

      if (!err.empty()) {
        err = std::format("Window@{} Failed to construct SDL_Window@{}\n  {}",
                          (void *)this, (void *)_windowPrimary, err);
        SDL_ClearError();
        throw err;
      }

      throw std::format("Window@{} Failed to construct", (void *)this);
    }

    getSurface();
    update();
  }

  ~Window() {
    SDL_Log("~Window() fired");
    if (_windowPrimary && SDL_WasInit(SDL_INIT_VIDEO)) {
      SDL_DestroyWindow(_windowPrimary);
    }
  }

  SDL_Surface *getSurface() const {
    SDL_Surface *surface = SDL_GetWindowSurface(_windowPrimary);

    if (!surface) {
      std::string err{SDL_GetError()};
      if (!err.empty()) {
        err = std::format("Window@{} Failed to get SDL_Window surface:\n  {}",
                          (void *)this, err);
        SDL_ClearError();
        throw err;
      }

      throw std::format("Window@{} Failed to get SDL_Window surface",
                        (void *)this);
    }

    return surface;
  }

  bool update() { return SDL_UpdateWindowSurface(_windowPrimary); }

  void clear(bool skipUpdate = false) {
    SDL_Surface *surface = getSurface();

    const SDL_PixelFormatDetails *pixelFormat{
        SDL_GetPixelFormatDetails(surface->format)};
    SDL_FillSurfaceRect(surface, nullptr,
                        SDL_MapRGB(pixelFormat, nullptr, 50, 50, 50));
    if (skipUpdate)
      return;
    update();
  }

  Window(const Window &) = delete;
  Window &operator=(const Window &) = delete;
};
