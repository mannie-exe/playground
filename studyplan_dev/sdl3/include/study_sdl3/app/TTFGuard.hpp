#pragma once

#include <string>

#include <SDL3/SDL_error.h>
#include <SDL3_ttf/SDL_ttf.h>

class TTFGuard {
  bool _initialized{false};

public:
  TTFGuard() {
    if (!TTF_Init()) {
      throw std::string{SDL_GetError()};
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
