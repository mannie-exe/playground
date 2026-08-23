#pragma once

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_video.h>

#include <string>

class Window {
  SDL_Window *_windowPrimary{nullptr};

public:
  Window(const char *title, int width = 800, int height = 600,
         SDL_WindowFlags windowFlags = 0)
      : _windowPrimary{SDL_CreateWindow(title, width, height, windowFlags)} {
    std::string err{SDL_GetError()};
    if (!err.empty()) {
      SDL_Log("Failed to construct Window@%p:\n  %s", this, err.c_str());
      SDL_Log("  SDL_Window@%p", _windowPrimary);
      SDL_ClearError();
      throw err;
    }

    getSurface();
    update();
  }

  SDL_Surface *getSurface() const {
    return SDL_GetWindowSurface(_windowPrimary);
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

  ~Window() {
    SDL_Log("~Window() fired");
    if (_windowPrimary && SDL_WasInit(SDL_INIT_VIDEO)) {
      SDL_DestroyWindow(_windowPrimary);
    }
  }

  Window(const Window &) = delete;
  Window &operator=(const Window &) = delete;
};
