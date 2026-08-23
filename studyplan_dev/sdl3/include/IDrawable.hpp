#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>

class IDrawable {
public:
  IDrawable() = default;
  virtual ~IDrawable() = default;

  virtual void render(SDL_Surface &surface) const = 0;

  IDrawable(const IDrawable &) = delete;
  IDrawable &operator=(const IDrawable &) = delete;
};
