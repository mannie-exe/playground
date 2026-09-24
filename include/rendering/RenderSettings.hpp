#pragma once

#include <math/Geometry2D.hpp>

namespace playground::rendering {

struct RenderSettings {
  // Whole-frame resolution; this never changes layout or input coordinates.
  float resolutionScale{1};
  bool glyphAtlases{true};
  bool vsync{true};

  void validate() const;
  math::Vec2i targetSize(math::Vec2i drawable) const;
  bool operator==(const RenderSettings &) const = default;
};

} // namespace playground::rendering
