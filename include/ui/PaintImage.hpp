#pragma once

#include <memory>

#include <math/Color.hpp>
#include <math/Geometry2D.hpp>

namespace playground::ui {

class PaintImage {
public:
  virtual ~PaintImage() = default;
  virtual math::Size2 pixelSize() const noexcept = 0;
};

using PaintImageHandle = std::shared_ptr<const PaintImage>;

enum class Sampling { Nearest, Linear };

struct ImagePaint {
  math::ColorRGBA8 tint{255, 255, 255, 255};
  Sampling sampling{Sampling::Linear};

  bool operator==(const ImagePaint &) const = default;
};

} // namespace playground::ui
