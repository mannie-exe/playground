#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_rect.h>

#include "math/Color.hpp"
#include "math/Geometry2D.hpp"

namespace playground::sdl {

constexpr SDL_Point toSDL(math::Vec2i value) { return {value.x, value.y}; }

constexpr SDL_FPoint toSDL(math::Vec2f value) { return {value.x, value.y}; }

constexpr SDL_FPoint toSDL(math::Point2 value) { return {value.x, value.y}; }

constexpr SDL_FPoint toSDL(math::Size2 value) {
  return {value.width, value.height};
}

constexpr SDL_FRect toSDL(math::Rect value) {
  return {value.x(), value.y(), value.w(), value.h()};
}

constexpr SDL_Color toSDL(math::ColorRGBA8 value) {
  return {value.r, value.g, value.b, value.a};
}

constexpr math::Vec2i fromSDL(SDL_Point value) { return {value.x, value.y}; }

constexpr math::Vec2f fromSDL(SDL_FPoint value) { return {value.x, value.y}; }

constexpr math::Rect fromSDL(SDL_Rect value) {
  return math::rect(static_cast<float>(value.x), static_cast<float>(value.y),
                    static_cast<float>(value.w), static_cast<float>(value.h));
}

constexpr math::Rect fromSDL(SDL_FRect value) {
  return math::rect(value.x, value.y, value.w, value.h);
}

constexpr math::ColorRGBA8 fromSDL(SDL_Color value) {
  return {value.r, value.g, value.b, value.a};
}

enum class PixelRounding { Nearest, Floor, Ceil, Truncate, Outward };

// Outward requires an interval; it is not meaningful for an isolated
// coordinate.
inline int checkedPixel(double value,
                        PixelRounding rounding = PixelRounding::Nearest) {
  if (!std::isfinite(value))
    throw std::invalid_argument("Pixel coordinate must be finite");
  switch (rounding) {
  case PixelRounding::Nearest:
    value = std::round(value);
    break;
  case PixelRounding::Floor:
    value = std::floor(value);
    break;
  case PixelRounding::Ceil:
    value = std::ceil(value);
    break;
  case PixelRounding::Truncate:
    value = std::trunc(value);
    break;
  case PixelRounding::Outward:
    throw std::invalid_argument("Outward rounding requires rectangle edges");
  default:
    throw std::invalid_argument("Unknown pixel rounding policy");
  }
  if (value < std::numeric_limits<int>::min() ||
      value > std::numeric_limits<int>::max())
    throw std::out_of_range("Pixel coordinate exceeds SDL integer range");
  return static_cast<int>(value);
}

inline SDL_Point toPixelPoint(math::Vec2f value,
                              PixelRounding rounding = PixelRounding::Nearest) {
  return {checkedPixel(value.x, rounding), checkedPixel(value.y, rounding)};
}

inline SDL_Point toPixelPoint(math::Point2 value,
                              PixelRounding rounding = PixelRounding::Nearest) {
  return {checkedPixel(value.x, rounding), checkedPixel(value.y, rounding)};
}

inline SDL_Point toPixelSize(math::Size2 value,
                             PixelRounding rounding = PixelRounding::Nearest) {
  if (!math::isNonNegative(value))
    throw std::invalid_argument("Pixel size cannot be negative");
  return {checkedPixel(value.width, rounding),
          checkedPixel(value.height, rounding)};
}

// Round shared edges rather than origin and extent separately. Outward rounding
// is useful for damage bounds; Nearest keeps adjacent layout partitions
// aligned.
inline SDL_Rect toPixelRect(math::Rect value,
                            PixelRounding rounding = PixelRounding::Outward) {
  if (!math::isFinite(value) || !math::isNonNegative(value.size))
    throw std::invalid_argument(
        "Pixel rectangle must be finite with nonnegative size");
  const auto startRounding =
      rounding == PixelRounding::Outward ? PixelRounding::Floor : rounding;
  const auto endRounding =
      rounding == PixelRounding::Outward ? PixelRounding::Ceil : rounding;
  const int left = checkedPixel(value.x(), startRounding),
            top = checkedPixel(value.y(), startRounding);
  // Match the float edge used when positioning the next layout partition.
  const int right =
      value.w() == 0 ? left : checkedPixel(value.right(), endRounding);
  const int bottom =
      value.h() == 0 ? top : checkedPixel(value.bottom(), endRounding);
  const std::int64_t width = static_cast<std::int64_t>(right) - left;
  const std::int64_t height = static_cast<std::int64_t>(bottom) - top;
  if (width > std::numeric_limits<int>::max() ||
      height > std::numeric_limits<int>::max())
    throw std::out_of_range("Pixel rectangle extent exceeds SDL integer range");
  return {left, top, static_cast<int>(width), static_cast<int>(height)};
}

} // namespace playground::sdl
