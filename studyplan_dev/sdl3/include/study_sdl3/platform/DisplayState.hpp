#pragma once

#include <SDL3/SDL_rect.h>

struct DisplayState {
  SDL_Point windowSize{};
  SDL_Point drawableSize{};
};
