#pragma once

#include <cmath>
#include <format>
#include <string_view>

#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_rect.h>

using Vec2f = SDL_FPoint;

constexpr bool operator==(const SDL_Point &a, const SDL_Point &b) {
  return a.x == b.x && a.y == b.y;
}

constexpr bool operator!=(const SDL_Point &a, const SDL_Point &b) {
  return !(a == b);
}

constexpr bool operator==(const SDL_FPoint &a, const SDL_FPoint &b) {
  return a.x == b.x && a.y == b.y;
}

constexpr bool operator!=(const SDL_FPoint &a, const SDL_FPoint &b) {
  return !(a == b);
}

constexpr SDL_FPoint operator+(const SDL_FPoint &a, const SDL_FPoint &b) {
  return SDL_FPoint{a.x + b.x, a.y + b.y};
}

constexpr SDL_FPoint operator-(const SDL_FPoint &a, const SDL_FPoint &b) {
  return SDL_FPoint{a.x - b.x, a.y - b.y};
}

constexpr SDL_FPoint operator*(const SDL_FPoint &point, float scalar) {
  return SDL_FPoint{point.x * scalar, point.y * scalar};
}

constexpr SDL_FPoint operator*(float scalar, const SDL_FPoint &point) {
  return point * scalar;
}

constexpr SDL_FPoint operator/(const SDL_FPoint &point, float scalar) {
  return SDL_FPoint{point.x / scalar, point.y / scalar};
}

constexpr bool operator==(const SDL_Rect &a, const SDL_Rect &b) {
  return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

constexpr bool operator!=(const SDL_Rect &a, const SDL_Rect &b) {
  return !(a == b);
}

constexpr bool operator==(const SDL_FRect &a, const SDL_FRect &b) {
  return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

constexpr bool operator!=(const SDL_FRect &a, const SDL_FRect &b) {
  return !(a == b);
}

constexpr bool operator==(const SDL_Color &a, const SDL_Color &b) {
  return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

constexpr bool operator!=(const SDL_Color &a, const SDL_Color &b) {
  return !(a == b);
}

struct RectTransform {
  Vec2f position{};
  Vec2f size{};

  float x() const { return position.x; }
  float y() const { return position.y; }
  float w() const { return size.x; }
  float h() const { return size.y; }

  float left() const { return position.x; }
  float right() const { return position.x + size.x; }
  float top() const { return position.y; }
  float bottom() const { return position.y + size.y; }

  bool contains(float px, float py) const {
    return px >= left() && px < right() && py >= top() && py < bottom();
  }

  bool contains(Vec2f point) const {
    return contains(point.x, point.y);
  }

  SDL_FRect toFRect() const {
    return SDL_FRect{position.x, position.y, size.x, size.y};
  }

  SDL_Rect toRect() const {
    return SDL_Rect{static_cast<int>(position.x), static_cast<int>(position.y),
                    static_cast<int>(size.x), static_cast<int>(size.y)};
  }

  SDL_Rect toSDL() const { return toRect(); }
};

constexpr RectTransform rect(float x, float y, float w, float h) {
  return RectTransform{.position = Vec2f{x, y}, .size = Vec2f{w, h}};
}

constexpr RectTransform rect(SDL_Rect value) {
  return rect(static_cast<float>(value.x), static_cast<float>(value.y),
              static_cast<float>(value.w), static_cast<float>(value.h));
}

constexpr RectTransform rect(SDL_FRect value) {
  return rect(value.x, value.y, value.w, value.h);
}

constexpr bool operator==(const RectTransform &a, const RectTransform &b) {
  return a.position == b.position && a.size == b.size;
}

constexpr bool operator!=(const RectTransform &a, const RectTransform &b) {
  return !(a == b);
}

inline bool almostEqual(float a, float b, float epsilon = 0.001f) {
  return std::fabs(a - b) <= epsilon;
}

inline bool almostEqual(SDL_FPoint a, SDL_FPoint b,
                        float epsilon = 0.001f) {
  return almostEqual(a.x, b.x, epsilon) && almostEqual(a.y, b.y, epsilon);
}

inline bool almostEqual(SDL_FRect a, SDL_FRect b, float epsilon = 0.001f) {
  return almostEqual(a.x, b.x, epsilon) && almostEqual(a.y, b.y, epsilon) &&
         almostEqual(a.w, b.w, epsilon) && almostEqual(a.h, b.h, epsilon);
}

inline bool almostEqual(RectTransform a, RectTransform b,
                        float epsilon = 0.001f) {
  return almostEqual(a.position, b.position, epsilon) &&
         almostEqual(a.size, b.size, epsilon);
}

struct Padding {
  float left{};
  float top{};
  float right{};
  float bottom{};
};

struct Spacing {
  float x{};
  float y{};
};

struct DisplayBox {
  RectTransform rect;
  Padding padding;
};

enum class HorizontalAlign {
  Left,
  Center,
  Right,
};

enum class VerticalAlign {
  Top,
  Middle,
  Bottom,
};

struct DisplayAlignment {
  HorizontalAlign horizontal{HorizontalAlign::Left};
  VerticalAlign vertical{VerticalAlign::Top};
};

enum class Overflow {
  Visible,
  Hidden,
  Clip,
};

enum class Orientation {
  Horizontal,
  Vertical,
};

enum class SizePolicy {
  Fixed,
  Content,
  Fill,
};

constexpr std::string_view toString(HorizontalAlign align) {
  switch (align) {
  case HorizontalAlign::Left:
    return "Left";
  case HorizontalAlign::Center:
    return "Center";
  case HorizontalAlign::Right:
    return "Right";
  default:
    return "Unknown";
  }
}

constexpr std::string_view toString(VerticalAlign align) {
  switch (align) {
  case VerticalAlign::Top:
    return "Top";
  case VerticalAlign::Middle:
    return "Middle";
  case VerticalAlign::Bottom:
    return "Bottom";
  default:
    return "Unknown";
  }
}

constexpr std::string_view toString(Overflow overflow) {
  switch (overflow) {
  case Overflow::Visible:
    return "Visible";
  case Overflow::Hidden:
    return "Hidden";
  case Overflow::Clip:
    return "Clip";
  default:
    return "Unknown";
  }
}

constexpr std::string_view toString(Orientation orientation) {
  switch (orientation) {
  case Orientation::Horizontal:
    return "Horizontal";
  case Orientation::Vertical:
    return "Vertical";
  default:
    return "Unknown";
  }
}

constexpr std::string_view toString(SizePolicy policy) {
  switch (policy) {
  case SizePolicy::Fixed:
    return "Fixed";
  case SizePolicy::Content:
    return "Content";
  case SizePolicy::Fill:
    return "Fill";
  default:
    return "Unknown";
  }
}

constexpr bool sameRect(const SDL_Rect &a, const SDL_Rect &b) {
  return a == b;
}

constexpr bool sameColor(const SDL_Color &a, const SDL_Color &b) {
  return a == b;
}

template <>
struct std::formatter<SDL_Point> : std::formatter<std::string_view> {
  auto format(SDL_Point point, format_context &ctx) const {
    return std::format_to(ctx.out(), "SDL_Point{{{}, {}}}", point.x, point.y);
  }
};

template <>
struct std::formatter<SDL_FPoint> : std::formatter<std::string_view> {
  auto format(SDL_FPoint point, format_context &ctx) const {
    return std::format_to(ctx.out(), "SDL_FPoint{{{}, {}}}", point.x, point.y);
  }
};

template <>
struct std::formatter<SDL_Rect> : std::formatter<std::string_view> {
  auto format(SDL_Rect rect, format_context &ctx) const {
    return std::format_to(ctx.out(), "SDL_Rect{{{}, {}, {}, {}}}", rect.x,
                          rect.y, rect.w, rect.h);
  }
};

template <>
struct std::formatter<SDL_FRect> : std::formatter<std::string_view> {
  auto format(SDL_FRect rect, format_context &ctx) const {
    return std::format_to(ctx.out(), "SDL_FRect{{{}, {}, {}, {}}}", rect.x,
                          rect.y, rect.w, rect.h);
  }
};

template <>
struct std::formatter<SDL_Color> : std::formatter<std::string_view> {
  auto format(SDL_Color color, format_context &ctx) const {
    return std::format_to(ctx.out(), "SDL_Color{{{}, {}, {}, {}}}", color.r,
                          color.g, color.b, color.a);
  }
};

template <>
struct std::formatter<RectTransform> : std::formatter<std::string_view> {
  auto format(RectTransform transform, format_context &ctx) const {
    return std::format_to(ctx.out(),
                          "RectTransform{{.position = {}, .size = {}}}",
                          transform.position, transform.size);
  }
};

template <>
struct std::formatter<HorizontalAlign> : std::formatter<std::string_view> {
  auto format(HorizontalAlign align, format_context &ctx) const {
    return std::formatter<std::string_view>::format(toString(align), ctx);
  }
};

template <>
struct std::formatter<VerticalAlign> : std::formatter<std::string_view> {
  auto format(VerticalAlign align, format_context &ctx) const {
    return std::formatter<std::string_view>::format(toString(align), ctx);
  }
};

template <>
struct std::formatter<Overflow> : std::formatter<std::string_view> {
  auto format(Overflow overflow, format_context &ctx) const {
    return std::formatter<std::string_view>::format(toString(overflow), ctx);
  }
};

template <>
struct std::formatter<Orientation> : std::formatter<std::string_view> {
  auto format(Orientation orientation, format_context &ctx) const {
    return std::formatter<std::string_view>::format(toString(orientation), ctx);
  }
};

template <>
struct std::formatter<SizePolicy> : std::formatter<std::string_view> {
  auto format(SizePolicy policy, format_context &ctx) const {
    return std::formatter<std::string_view>::format(toString(policy), ctx);
  }
};
