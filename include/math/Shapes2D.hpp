#pragma once

#include <algorithm>
#include <stdexcept>
#include <type_traits>
#include <variant>

#include <math/Geometry2D.hpp>

namespace playground::math {

struct CornerRadii {
  Size2 topLeft, topRight, bottomRight, bottomLeft;

  static constexpr CornerRadii all(float radius) {
    return {
        {radius, radius}, {radius, radius}, {radius, radius}, {radius, radius}};
  }

  bool operator==(const CornerRadii &) const = default;

  void validate() const {
    for (auto radius : {topLeft, topRight, bottomRight, bottomLeft})
      if (!isFinite(radius) || !isNonNegative(radius))
        throw std::invalid_argument(
            "Corner radii must be finite and nonnegative");
  }

  CornerRadii resolved(Size2 size) const {
    validate();
    if (!isFinite(size) || !isNonNegative(size))
      throw std::invalid_argument("Rounded rectangle size is invalid");
    double factor = 1;
    auto limit = [&](float extent, double sum) {
      if (sum > 0)
        factor = std::min(factor, extent / sum);
    };
    limit(size.width, double(topLeft.width) + topRight.width);
    limit(size.width, double(bottomLeft.width) + bottomRight.width);
    limit(size.height, double(topLeft.height) + bottomLeft.height);
    limit(size.height, double(topRight.height) + bottomRight.height);
    auto scale = [&](Size2 r) {
      return Size2{float(r.width * factor), float(r.height * factor)};
    };
    return {scale(topLeft), scale(topRight), scale(bottomRight),
            scale(bottomLeft)};
  }
};

struct RoundedRect {
  Rect bounds;
  CornerRadii radii;
  bool operator==(const RoundedRect &) const = default;

  void validate() const {
    if (!isFinite(bounds) || !isNonNegative(bounds.size))
      throw std::invalid_argument("Rounded rectangle bounds are invalid");
    radii.validate();
  }

  bool contains(Point2 point) const {
    if (!bounds.contains(point))
      return false;
    const auto r = radii.resolved(bounds.size);
    const float x = point.x - bounds.x(), y = point.y - bounds.y();
    auto ellipse = [](float dx, float dy, Size2 radius) {
      if (!hasArea(radius))
        return true;
      const double a = double(dx) / radius.width;
      const double b = double(dy) / radius.height;
      return a * a + b * b <= 1;
    };
    if (x < r.topLeft.width && y < r.topLeft.height)
      return ellipse(x - r.topLeft.width, y - r.topLeft.height, r.topLeft);
    if (x > bounds.w() - r.topRight.width && y < r.topRight.height)
      return ellipse(x - bounds.w() + r.topRight.width, y - r.topRight.height,
                     r.topRight);
    if (x > bounds.w() - r.bottomRight.width &&
        y > bounds.h() - r.bottomRight.height)
      return ellipse(x - bounds.w() + r.bottomRight.width,
                     y - bounds.h() + r.bottomRight.height, r.bottomRight);
    if (x < r.bottomLeft.width && y > bounds.h() - r.bottomLeft.height)
      return ellipse(x - r.bottomLeft.width,
                     y - bounds.h() + r.bottomLeft.height, r.bottomLeft);
    return true;
  }

  RoundedRect inset(Insets insets) const {
    validate();
    if (!isFinite(insets) || !isNonNegative(insets))
      throw std::invalid_argument(
          "Rounded rectangle insets must be finite and nonnegative");
    const auto r = radii.resolved(bounds.size);
    auto shrink = [](Size2 radius, float x, float y) {
      return Size2{std::max(0.0f, radius.width - x),
                   std::max(0.0f, radius.height - y)};
    };
    return {math::inset(bounds, insets),
            {shrink(r.topLeft, insets.left, insets.top),
             shrink(r.topRight, insets.right, insets.top),
             shrink(r.bottomRight, insets.right, insets.bottom),
             shrink(r.bottomLeft, insets.left, insets.bottom)}};
  }
};

using ClipShape = std::variant<Rect, RoundedRect>;

inline Rect shapeBounds(const ClipShape &shape) {
  return std::visit(
      [](const auto &s) -> Rect {
        if constexpr (std::is_same_v<std::decay_t<decltype(s)>, Rect>)
          return s;
        else
          return s.bounds;
      },
      shape);
}

inline bool contains(const ClipShape &shape, Point2 point) {
  return std::visit([&](const auto &s) { return s.contains(point); }, shape);
}

} // namespace playground::math
