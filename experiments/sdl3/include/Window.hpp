#pragma once

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_video.h>

class Window {
  SDL_Window *_windowPrimary{nullptr};

public:
  Window(const char *title, int width = 800, int height = 600,
         SDL_WindowFlags windowFlags = 0)
      : _windowPrimary{SDL_CreateWindow(title, width, height, windowFlags)} {
    getSurface();
    SDL_UpdateWindowSurface(_windowPrimary);
  }

  SDL_Surface *getSurface() const {
    return SDL_GetWindowSurface(_windowPrimary);
  }

  bool update() { return SDL_UpdateWindowSurface(_windowPrimary); }

  ~Window() {
    if (_windowPrimary && SDL_WasInit(SDL_INIT_VIDEO)) {
      SDL_DestroyWindow(_windowPrimary);
    }
  }

  Window(const Window &) = delete;
  Window &operator=(const Window &) = delete;
};
