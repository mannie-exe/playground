#pragma once

#include <memory>

#include <SDL3/SDL_surface.h>

struct SurfaceHandleDeleter {
  void operator()(SDL_Surface *surface) const {
    if (surface)
      SDL_DestroySurface(surface);
  }
};

using SurfaceHandle = std::shared_ptr<SDL_Surface>;
