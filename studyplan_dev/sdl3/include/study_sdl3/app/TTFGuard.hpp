#pragma once

#include <SDL3_ttf/SDL_ttf.h>

#include <study_sdl3/support/SDLError.hpp>

class TTFGuard {
  bool _initialized{false};

public:
  TTFGuard() {
    if (!TTF_Init()) {
      throwSDLError("Failed to initialize SDL_ttf");
    }

    _initialized = true;
  }

  ~TTFGuard() {
    if (_initialized) {
      TTF_Quit();
    }
  }

  bool initialized() const { return _initialized; }

  TTFGuard(const TTFGuard &) = delete;
  TTFGuard &operator=(const TTFGuard &) = delete;
};
