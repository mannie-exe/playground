#pragma once

#include <optional>
#include <stdexcept>

#include <math/Color.hpp>
#include <math/Path2D.hpp>

namespace playground::rendering {
struct PathPaint {
  std::optional<math::ColorRGBA8> fill{math::ColorRGBA8{0, 0, 0, 255}};
  std::optional<math::ColorRGBA8> stroke;
  float strokeWidth{1};
  math::FillRule fillRule{math::FillRule::NonZero};

  void validate() const {
    if (!std::isfinite(strokeWidth) || strokeWidth < 0 ||
        (fillRule != math::FillRule::NonZero &&
         fillRule != math::FillRule::EvenOdd))
      throw std::invalid_argument("Invalid path paint");
  }

  bool operator==(const PathPaint &) const = default;
};
} // namespace playground::rendering
