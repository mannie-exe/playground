#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include <rendering/RenderSettings.hpp>

namespace playground::rendering {
void RenderSettings::validate() const {
  if (!std::isfinite(resolutionScale) || resolutionScale <= 0 ||
      resolutionScale > 4)
    throw std::invalid_argument("Render scale must be finite and in (0, 4]");
}

math::Vec2i RenderSettings::targetSize(math::Vec2i drawable) const {
  validate();
  if (!math::hasArea(drawable))
    throw std::invalid_argument("Render target must have positive dimensions");
  const auto axis = [&](int extent) {
    const double result =
        std::ceil(static_cast<double>(extent) * resolutionScale);
    if (result > std::numeric_limits<int>::max())
      throw std::overflow_error("Render target extent overflow");
    return std::max(1, static_cast<int>(result));
  };
  return {axis(drawable.x), axis(drawable.y)};
}
} // namespace playground::rendering
