#pragma once

#include <memory>

#include <math/Color.hpp>
#include <math/Geometry2D.hpp>
#include <rendering/ImageTypes.hpp>

namespace playground::rendering {

class PaintImage {
public:
  virtual ~PaintImage() = default;
  virtual math::Size2 pixelSize() const noexcept = 0;
  virtual std::size_t bytesPerPixel() const noexcept { return 4; }
  virtual rendering::AlphaMode alphaMode() const noexcept {
    return rendering::AlphaMode::Straight;
  }
  virtual rendering::ColorEncoding colorEncoding() const noexcept {
    return rendering::ColorEncoding::SRGB;
  }
};

using PaintImageHandle = std::shared_ptr<const PaintImage>;

enum class Sampling { Nearest, Linear };

struct ImagePaint {
  math::ColorRGBA8 tint{255, 255, 255, 255};
  Sampling sampling{Sampling::Linear};

  bool operator==(const ImagePaint &) const = default;
};

} // namespace playground::rendering
