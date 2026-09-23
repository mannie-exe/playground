#pragma once

#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_rect.h>

namespace playground::sdl {

constexpr bool equal(SDL_Point a, SDL_Point b) {
  return a.x == b.x && a.y == b.y;
}
constexpr bool equal(SDL_FPoint a, SDL_FPoint b) {
  return a.x == b.x && a.y == b.y;
}
constexpr bool equal(SDL_Rect a, SDL_Rect b) {
  return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}
constexpr bool equal(SDL_FRect a, SDL_FRect b) {
  return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}
constexpr bool equal(SDL_Color a, SDL_Color b) {
  return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

} // namespace playground::sdl
