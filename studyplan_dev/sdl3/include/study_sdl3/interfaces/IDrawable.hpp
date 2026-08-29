#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>

class IDrawable {
protected:
  IDrawable() = default;

public:
  virtual ~IDrawable() = default;
  virtual void render(SDL_Surface &targetSurface) = 0;
};
