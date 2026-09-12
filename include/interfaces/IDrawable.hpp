#pragma once

#include <SDL3/SDL_surface.h>

class IDrawable {
protected:
  IDrawable() = default;

public:
  virtual ~IDrawable() = default;
  virtual void render(SDL_Surface &targetSurface) = 0;
};
