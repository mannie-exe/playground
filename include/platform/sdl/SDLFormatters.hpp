#pragma once

#include <format>
#include <string_view>

#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_rect.h>

template <>
struct std::formatter<SDL_Point> : std::formatter<std::string_view> {
  auto format(SDL_Point value, std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        std::format("SDL_Point{{{}, {}}}", value.x, value.y), context);
  }
};
template <>
struct std::formatter<SDL_FPoint> : std::formatter<std::string_view> {
  auto format(SDL_FPoint value, std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        std::format("SDL_FPoint{{{}, {}}}", value.x, value.y), context);
  }
};
template <> struct std::formatter<SDL_Rect> : std::formatter<std::string_view> {
  auto format(SDL_Rect value, std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        std::format("SDL_Rect{{{}, {}, {}, {}}}", value.x, value.y, value.w,
                    value.h),
        context);
  }
};
template <>
struct std::formatter<SDL_FRect> : std::formatter<std::string_view> {
  auto format(SDL_FRect value, std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        std::format("SDL_FRect{{{}, {}, {}, {}}}", value.x, value.y, value.w,
                    value.h),
        context);
  }
};
template <>
struct std::formatter<SDL_Color> : std::formatter<std::string_view> {
  auto format(SDL_Color value, std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        std::format("SDL_Color{{{}, {}, {}, {}}}", value.r, value.g, value.b,
                    value.a),
        context);
  }
};
