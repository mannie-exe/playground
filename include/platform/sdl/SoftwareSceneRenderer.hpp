#pragma once

#include <rendering/AllocationLimits.hpp>
#include <scene/SceneRenderer.hpp>

namespace playground::sdl {

// Reference unlit triangle rasterizer: homogeneous clipping, depth buffering,
// perspective-correct UVs and linear-light material multiplication/blending.
class SoftwareSceneRenderer final : public scene::SceneRenderer {
  rendering::AllocationLimits _limits;

public:
  explicit SoftwareSceneRenderer(rendering::AllocationLimits limits = {})
      : _limits{limits} {
    _limits.validate();
  }

  rendering::PaintImageHandle
  render(const scene::SceneRenderProps &view,
         std::span<const scene::MeshDraw> draws) override;
};

} // namespace playground::sdl
