#pragma once

#include <SDL3/SDL_log.h>
#include <SDL3/SDL_video.h>

class AppState {
  SDL_Window *_windowPrimary{nullptr};

public:
  AppState(const char *title, int width = 800, int height = 600,
           SDL_WindowFlags windowFlags = 0)
      : _windowPrimary{SDL_CreateWindow(title, width, height, windowFlags)} {

    SDL_GetWindowSurface(_windowPrimary);
    SDL_UpdateWindowSurface(_windowPrimary);
  }

  ~AppState() {
    SDL_Log("~AppState() fired");
    SDL_DestroyWindow(_windowPrimary);
  }

  AppState(const AppState &) = delete;
  AppState &operator=(const AppState &) = delete;
};
