#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>
#include <optional>

namespace playground::math {

// Component arithmetic has the same preconditions as its scalar type: integer
// results must be representable, and integer division requires a nonzero
// divisor.
template <typename T>
  requires(std::same_as<T, int> || std::same_as<T, float>)
struct Vec2 {
  T x{};
  T y{};
  constexpr bool operator==(const Vec2 &) const = default;

  constexpr Vec2 &operator+=(Vec2 rhs) {
    x += rhs.x;
    y += rhs.y;
    return *this;
  }

  constexpr Vec2 &operator-=(Vec2 rhs) {
    x -= rhs.x;
    y -= rhs.y;
    return *this;
  }

  constexpr Vec2 &operator*=(T rhs) {
    x *= rhs;
    y *= rhs;
    return *this;
  }

  constexpr Vec2 &operator/=(T rhs) {
    x /= rhs;
    y /= rhs;
    return *this;
  }

  constexpr Vec2 &operator*=(Vec2 rhs) {
    x *= rhs.x;
    y *= rhs.y;
    return *this;
  }

  constexpr Vec2 &operator/=(Vec2 rhs) {
    x /= rhs.x;
    y /= rhs.y;
    return *this;
  }

  friend constexpr Vec2 operator+(Vec2 a, Vec2 b) { return a += b; }

  friend constexpr Vec2 operator-(Vec2 a, Vec2 b) { return a -= b; }

  friend constexpr Vec2 operator-(Vec2 a) { return {-a.x, -a.y}; }

  friend constexpr Vec2 operator*(Vec2 a, T b) { return a *= b; }

  friend constexpr Vec2 operator*(T a, Vec2 b) { return b *= a; }

  friend constexpr Vec2 operator/(Vec2 a, T b) { return a /= b; }

  friend constexpr Vec2 operator*(Vec2 a, Vec2 b) { return a *= b; }

  friend constexpr Vec2 operator/(Vec2 a, Vec2 b) { return a /= b; }
};

using Vec2i = Vec2<int>;
using Vec2f = Vec2<float>;

template <typename T> constexpr bool isNonNegative(Vec2<T> value) {
  return value.x >= 0 && value.y >= 0;
}

template <typename T> constexpr bool isPositive(Vec2<T> value) {
  return value.x > 0 && value.y > 0;
}

template <typename T> constexpr bool hasArea(Vec2<T> value) {
  return isPositive(value);
}

template <typename T>
constexpr bool inBounds(Vec2<T> value, Vec2<T> minimum, Vec2<T> maximum) {
  return value.x >= minimum.x && value.y >= minimum.y && value.x <= maximum.x &&
         value.y <= maximum.y;
}

template <typename T>
constexpr bool inBoundsExclusive(Vec2<T> value, Vec2<T> minimum,
                                 Vec2<T> maximum) {
  return value.x >= minimum.x && value.y >= minimum.y && value.x < maximum.x &&
         value.y < maximum.y;
}

template <typename T>
constexpr bool inBoundsExclusive(Vec2<T> value, Vec2<T> extent) {
  return inBoundsExclusive(value, Vec2<T>{}, extent);
}

constexpr std::int64_t area(Vec2i value) {
  return static_cast<std::int64_t>(value.x) * value.y;
}

constexpr float area(Vec2f value) { return value.x * value.y; }

inline bool isFinite(Vec2f value) {
  return std::isfinite(value.x) && std::isfinite(value.y);
}

// A point is a location; subtracting two points produces a displacement.
struct Point2 {
  float x{};
  float y{};
  constexpr bool operator==(const Point2 &) const = default;

  constexpr Point2 &operator+=(Vec2f offset) {
    x += offset.x;
    y += offset.y;
    return *this;
  }

  constexpr Point2 &operator-=(Vec2f offset) {
    x -= offset.x;
    y -= offset.y;
    return *this;
  }
};

constexpr Point2 operator+(Point2 point, Vec2f offset) {
  return point += offset;
}

constexpr Point2 operator+(Vec2f offset, Point2 point) {
  return point += offset;
}

constexpr Point2 operator-(Point2 point, Vec2f offset) {
  return point -= offset;
}

constexpr Vec2f operator-(Point2 a, Point2 b) { return {a.x - b.x, a.y - b.y}; }

inline bool isFinite(Point2 point) {
  return std::isfinite(point.x) && std::isfinite(point.y);
}

struct Size2 {
  float width{};
  float height{};
  constexpr bool operator==(const Size2 &) const = default;
};

constexpr Size2 operator*(Size2 size, float factor) {
  return {size.width * factor, size.height * factor};
}

constexpr Size2 operator*(float factor, Size2 size) { return size * factor; }

constexpr Size2 operator/(Size2 size, float factor) {
  return {size.width / factor, size.height / factor};
}

constexpr Size2 operator+(Size2 a, Size2 b) {
  return {a.width + b.width, a.height + b.height};
}

constexpr Size2 operator-(Size2 a, Size2 b) {
  return {a.width - b.width, a.height - b.height};
}

constexpr bool isNonNegative(Size2 size) {
  return size.width >= 0 && size.height >= 0;
}

constexpr bool hasArea(Size2 size) { return size.width > 0 && size.height > 0; }

constexpr float area(Size2 size) { return size.width * size.height; }

inline bool isFinite(Size2 size) {
  return std::isfinite(size.width) && std::isfinite(size.height);
}

constexpr Vec2f toVector(Point2 point) { return {point.x, point.y}; }

constexpr Vec2f toVector(Size2 size) { return {size.width, size.height}; }

constexpr Point2 toPoint(Vec2f vector) { return {vector.x, vector.y}; }

constexpr Size2 toSize(Vec2f vector) { return {vector.x, vector.y}; }

struct Insets {
  float left{};
  float top{};
  float right{};
  float bottom{};
  constexpr bool operator==(const Insets &) const = default;

  static constexpr Insets all(float value) {
    return {value, value, value, value};
  }

  static constexpr Insets symmetric(float horizontal, float vertical) {
    return {horizontal, vertical, horizontal, vertical};
  }

  constexpr float horizontal() const { return left + right; }

  constexpr float vertical() const { return top + bottom; }
};

inline bool isFinite(Insets value) {
  return std::isfinite(value.left) && std::isfinite(value.top) &&
         std::isfinite(value.right) && std::isfinite(value.bottom);
}

constexpr bool isNonNegative(Insets value) {
  return value.left >= 0 && value.top >= 0 && value.right >= 0 &&
         value.bottom >= 0;
}

struct Gap2 {
  float horizontal{};
  float vertical{};
  constexpr bool operator==(const Gap2 &) const = default;
};

struct Rect {
  Point2 position{};
  Size2 size{};
  constexpr bool operator==(const Rect &) const = default;

  constexpr float x() const { return position.x; }

  constexpr float y() const { return position.y; }

  constexpr float w() const { return size.width; }

  constexpr float h() const { return size.height; }

  constexpr float left() const { return x(); }

  constexpr float top() const { return y(); }

  constexpr float right() const { return x() + w(); }

  constexpr float bottom() const { return y() + h(); }

  constexpr Point2 center() const { return {x() + w() / 2, y() + h() / 2}; }

  constexpr bool hasArea() const { return math::hasArea(size); }

  constexpr bool contains(float px, float py) const {
    return px >= left() && py >= top() && px < right() && py < bottom();
  }

  constexpr bool contains(Point2 point) const {
    return contains(point.x, point.y);
  }

  constexpr Rect &operator+=(Vec2f offset) {
    position += offset;
    return *this;
  }

  constexpr Rect &operator-=(Vec2f offset) {
    position -= offset;
    return *this;
  }
};

constexpr Rect rect(float x, float y, float width, float height) {
  return {{x, y}, {width, height}};
}

constexpr Rect operator+(Rect bounds, Vec2f offset) { return bounds += offset; }

constexpr Rect operator-(Rect bounds, Vec2f offset) { return bounds -= offset; }

inline bool isFinite(Rect bounds) {
  return isFinite(bounds.position) && isFinite(bounds.size) &&
         std::isfinite(bounds.right()) && std::isfinite(bounds.bottom());
}

constexpr Rect inset(Rect bounds, Insets insets) {
  return rect(bounds.x() + insets.left, bounds.y() + insets.top,
              std::max(0.0f, bounds.w() - insets.horizontal()),
              std::max(0.0f, bounds.h() - insets.vertical()));
}

constexpr Rect outset(Rect bounds, Insets insets) {
  return inset(bounds,
               {-insets.left, -insets.top, -insets.right, -insets.bottom});
}

constexpr Rect intersect(Rect a, Rect b) {
  const float left = std::max(a.left(), b.left());
  const float top = std::max(a.top(), b.top());
  return rect(left, top, std::max(0.0f, std::min(a.right(), b.right()) - left),
              std::max(0.0f, std::min(a.bottom(), b.bottom()) - top));
}

constexpr Rect unite(Rect a, Rect b) {
  if (!a.hasArea())
    return b;
  if (!b.hasArea())
    return a;
  const float left = std::min(a.left(), b.left());
  const float top = std::min(a.top(), b.top());
  return rect(left, top, std::max(a.right(), b.right()) - left,
              std::max(a.bottom(), b.bottom()) - top);
}

inline bool almostEqual(float a, float b, float epsilon = 0.001f) {
  return std::fabs(a - b) <= epsilon;
}

inline bool almostEqual(Vec2f a, Vec2f b, float epsilon = 0.001f) {
  return almostEqual(a.x, b.x, epsilon) && almostEqual(a.y, b.y, epsilon);
}

inline bool almostEqual(Point2 a, Point2 b, float epsilon = 0.001f) {
  return almostEqual(a.x, b.x, epsilon) && almostEqual(a.y, b.y, epsilon);
}

inline bool almostEqual(Size2 a, Size2 b, float epsilon = 0.001f) {
  return almostEqual(a.width, b.width, epsilon) &&
         almostEqual(a.height, b.height, epsilon);
}

inline bool almostEqual(Rect a, Rect b, float epsilon = 0.001f) {
  return almostEqual(a.position, b.position, epsilon) &&
         almostEqual(a.size, b.size, epsilon);
}

// Column-vector convention: (a * b) applies b first, then a.
struct Transform2D {
  float a{1}, b{}, c{}, d{1}, tx{}, ty{};
  constexpr bool operator==(const Transform2D &) const = default;

  static constexpr Transform2D translation(Vec2f offset) {
    return {1, 0, 0, 1, offset.x, offset.y};
  }

  static constexpr Transform2D scaling(Vec2f factor) {
    return {factor.x, 0, 0, factor.y, 0, 0};
  }

  static Transform2D rotation(float radians) {
    const float cosine = std::cos(radians), sine = std::sin(radians);
    return {cosine, sine, -sine, cosine, 0, 0};
  }

  constexpr Point2 mapPoint(Point2 point) const {
    return {a * point.x + c * point.y + tx, b * point.x + d * point.y + ty};
  }

  constexpr Vec2f mapVector(Vec2f vector) const {
    return {a * vector.x + c * vector.y, b * vector.x + d * vector.y};
  }

  constexpr Transform2D operator*(Transform2D rhs) const {
    return {a * rhs.a + c * rhs.b,        b * rhs.a + d * rhs.b,
            a * rhs.c + c * rhs.d,        b * rhs.c + d * rhs.d,
            a * rhs.tx + c * rhs.ty + tx, b * rhs.tx + d * rhs.ty + ty};
  }

  static constexpr Transform2D around(Point2 pivot, Transform2D transform) {
    return translation(toVector(pivot)) * transform *
           translation(-toVector(pivot));
  }

  std::optional<Transform2D> inverse() const {
    const double determinant =
        static_cast<double>(a) * d - static_cast<double>(b) * c;
    if (determinant == 0 || !std::isfinite(determinant))
      return std::nullopt;
    const double values[]{
        d / determinant,
        -b / determinant,
        -c / determinant,
        a / determinant,
        (static_cast<double>(c) * ty - static_cast<double>(d) * tx) /
            determinant,
        (static_cast<double>(b) * tx - static_cast<double>(a) * ty) /
            determinant};
    // Validate before narrowing: an inverse can exist mathematically but not be
    // representable by this float-based transform (including its translation).
    constexpr double maximum = std::numeric_limits<float>::max();
    for (const double value : values)
      if (!std::isfinite(value) || value < -maximum || value > maximum)
        return std::nullopt;
    return Transform2D{
        static_cast<float>(values[0]), static_cast<float>(values[1]),
        static_cast<float>(values[2]), static_cast<float>(values[3]),
        static_cast<float>(values[4]), static_cast<float>(values[5])};
  }

  Rect mapBounds(Rect bounds) const {
    const Point2 p0 = mapPoint(bounds.position),
                 p1 = mapPoint({bounds.right(), bounds.top()}),
                 p2 = mapPoint({bounds.left(), bounds.bottom()}),
                 p3 = mapPoint({bounds.right(), bounds.bottom()});
    const float left = std::min({p0.x, p1.x, p2.x, p3.x}),
                top = std::min({p0.y, p1.y, p2.y, p3.y});
    return rect(left, top, std::max({p0.x, p1.x, p2.x, p3.x}) - left,
                std::max({p0.y, p1.y, p2.y, p3.y}) - top);
  }
};

} // namespace playground::math
