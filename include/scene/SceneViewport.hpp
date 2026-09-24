#pragma once

#include <scene/Scene3D.hpp>

namespace playground::scene {

// Bounds are local logical content coordinates, not window pixels or UI
// padding. A fixed aspect ratio letterboxes centrally; absent means fill the
// content box.
struct SceneViewportProps {
  math::Rect logicalBounds;
  math::Vec2f pixelScale{1, 1};
  float resolutionScale{1};
  std::optional<float> aspectRatio;
};

struct SceneViewport {
  math::Rect contentBounds;
  math::Vec2i pixelSize;
  CameraView camera;

  std::optional<math::Vec2f> normalizedPosition(math::Point2 local) const;
  std::optional<Ray3> rayAt(math::Point2 local) const;
  std::optional<math::Point2> project(math::Vec3f world) const;
};

// Empty content resolves to no viewport; malformed values throw before
// mutation.
std::optional<SceneViewport> resolveViewport(const CameraProps &camera,
                                             const SceneViewportProps &props);

} // namespace playground::scene
