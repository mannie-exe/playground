#pragma once

#include <format>
#include <string_view>

#include "math/Color.hpp"
#include "math/Geometry2D.hpp"
#include <math/Shapes2D.hpp>

template <typename T>
struct std::formatter<playground::math::Vec2<T>>
    : std::formatter<std::string_view> {
  auto format(playground::math::Vec2<T> value,
              std::format_context &context) const {
    constexpr auto name = std::same_as<T, float> ? "Vec2f" : "Vec2i";
    return std::formatter<std::string_view>::format(
        std::format("{}{{{}, {}}}", name, value.x, value.y), context);
  }
};

template <>
struct std::formatter<playground::math::Point2>
    : std::formatter<std::string_view> {
  auto format(playground::math::Point2 value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        std::format("Point2{{{}, {}}}", value.x, value.y), context);
  }
};

template <>
struct std::formatter<playground::math::Size2>
    : std::formatter<std::string_view> {
  auto format(playground::math::Size2 value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        std::format("Size2{{{}, {}}}", value.width, value.height), context);
  }
};

template <>
struct std::formatter<playground::math::Rect>
    : std::formatter<std::string_view> {
  auto format(playground::math::Rect value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        std::format("Rect{{.position = {}, .size = {}}}", value.position,
                    value.size),
        context);
  }
};

template <>
struct std::formatter<playground::math::Insets>
    : std::formatter<std::string_view> {
  auto format(playground::math::Insets value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        std::format("Insets{{{}, {}, {}, {}}}", value.left, value.top,
                    value.right, value.bottom),
        context);
  }
};

template <>
struct std::formatter<playground::math::Gap2>
    : std::formatter<std::string_view> {
  auto format(playground::math::Gap2 value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        std::format("Gap2{{{}, {}}}", value.horizontal, value.vertical),
        context);
  }
};

template <>
struct std::formatter<playground::math::Transform2D>
    : std::formatter<std::string_view> {
  auto format(playground::math::Transform2D value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        std::format("Transform2D{{{}, {}, {}, {}, {}, {}}}", value.a, value.b,
                    value.c, value.d, value.tx, value.ty),
        context);
  }
};

template <>
struct std::formatter<playground::math::ColorRGBA8>
    : std::formatter<std::string_view> {
  auto format(playground::math::ColorRGBA8 value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        std::format("ColorRGBA8{{{}, {}, {}, {}}}", value.r, value.g, value.b,
                    value.a),
        context);
  }
};

template <>
struct std::formatter<playground::math::CornerRadii>
    : std::formatter<std::string_view> {
  auto format(const playground::math::CornerRadii &value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        std::format("CornerRadii{{{}, {}, {}, {}}}", value.topLeft,
                    value.topRight, value.bottomRight, value.bottomLeft),
        context);
  }
};

template <>
struct std::formatter<playground::math::RoundedRect>
    : std::formatter<std::string_view> {
  auto format(const playground::math::RoundedRect &value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        std::format("RoundedRect{{{}, {}}}", value.bounds, value.radii),
        context);
  }
};
