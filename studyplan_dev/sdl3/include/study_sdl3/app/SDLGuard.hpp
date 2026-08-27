#pragma once

#include <string>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_init.h>

class SDLGuard {
  bool _initialized{false};

public:
  SDLGuard(SDL_InitFlags flags) {
    if (!SDL_Init(flags)) {
      throw std::string{SDL_GetError()};
    }

    _initialized = true;
  }

  ~SDLGuard() {
    if (_initialized) {
      SDL_Quit();
    }
  }

  bool initialized() const { return _initialized; }

  SDLGuard(const SDLGuard &) = delete;
  SDLGuard &operator=(const SDLGuard &) = delete;
};
