#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include <scene/SceneViewport.hpp>

namespace playground::scene {

std::optional<SceneViewport> resolveViewport(const CameraProps &camera,
                                             const SceneViewportProps &props) {
  if (!math::isFinite(props.logicalBounds) ||
      !math::isNonNegative(props.logicalBounds.size) ||
      !math::isFinite(props.pixelScale) ||
      !math::isPositive(props.pixelScale) ||
      !std::isfinite(props.resolutionScale) || props.resolutionScale <= 0 ||
      (props.aspectRatio &&
       (!std::isfinite(*props.aspectRatio) || *props.aspectRatio <= 0)))
    throw std::invalid_argument("Invalid scene viewport properties");
  if (!math::hasArea(props.logicalBounds.size))
    return {};
  auto area = props.logicalBounds;
  if (props.aspectRatio) {
    const double ratio = *props.aspectRatio;
    const float width =
        float(std::min(double(area.size.width), area.size.height * ratio));
    const float height = float(width / ratio);
    area.position.x += (area.size.width - width) * .5f;
    area.position.y += (area.size.height - height) * .5f;
    area.size = {width, height};
  }
  const double width = std::ceil(double(area.size.width) * props.pixelScale.x *
                                 props.resolutionScale);
  const double height = std::ceil(double(area.size.height) *
                                  props.pixelScale.y * props.resolutionScale);
  if (!std::isfinite(width) || !std::isfinite(height) || width < 1 ||
      height < 1 || width > std::numeric_limits<int>::max() ||
      height > std::numeric_limits<int>::max())
    throw std::length_error("Scene viewport extent exceeds integer range");
  return SceneViewport{area,
                       {int(width), int(height)},
                       camera.view(area.size.width / area.size.height)};
}

std::optional<math::Vec2f>
SceneViewport::normalizedPosition(math::Point2 local) const {
  if (!math::isFinite(local) || !contentBounds.contains(local) ||
      !math::hasArea(contentBounds.size))
    return {};
  return math::Vec2f{(local.x - contentBounds.x()) / contentBounds.size.width,
                     (local.y - contentBounds.y()) / contentBounds.size.height};
}

std::optional<Ray3> SceneViewport::rayAt(math::Point2 local) const {
  if (const auto normalized = normalizedPosition(local))
    return pickingRay(camera, *normalized);
  return {};
}

std::optional<math::Point2> SceneViewport::project(math::Vec3f world) const {
  if (!math::isFinite(world))
    throw std::invalid_argument("Nonfinite projected position");
  const auto clip = camera.projection * camera.view *
                    math::Vec4f{world.x, world.y, world.z, 1};
  if (!math::isFinite(clip) || clip.w <= 0 || clip.z < 0 || clip.z > clip.w ||
      clip.x < -clip.w || clip.x > clip.w || clip.y < -clip.w ||
      clip.y > clip.w)
    return {};
  return math::Point2{contentBounds.x() + (clip.x / clip.w + 1) * .5f *
                                              contentBounds.size.width,
                      contentBounds.y() + (1 - clip.y / clip.w) * .5f *
                                              contentBounds.size.height};
}

} // namespace playground::scene
