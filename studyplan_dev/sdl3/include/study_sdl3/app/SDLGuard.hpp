#pragma once

#include <SDL3/SDL_init.h>

#include <study_sdl3/support/SDLError.hpp>

class SDLGuard {
  bool _initialized{false};

public:
  SDLGuard(SDL_InitFlags flags) {
    if (!SDL_Init(flags)) {
      throwSDLError("Failed to initialize SDL");
    }

    _initialized = true;
  }

  ~SDLGuard() {
    if (_initialized) {
      SDL_Quit();
    }
  }

  bool isInitialized() const { return _initialized; }

  SDLGuard(SDLGuard &&) = delete;
  SDLGuard &operator=(SDLGuard &&) = delete;

  SDLGuard(const SDLGuard &) = delete;
  SDLGuard &operator=(const SDLGuard &) = delete;
};
