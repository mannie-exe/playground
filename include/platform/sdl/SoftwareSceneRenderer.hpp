#pragma once

#include <rendering/AllocationLimits.hpp>
#include <rendering/ResourceLedger.hpp>
#include <scene/SceneRenderer.hpp>

namespace playground::sdl {

// Reference unlit triangle rasterizer: homogeneous clipping, depth buffering,
// perspective-correct UVs and linear-light material multiplication/blending.
class SoftwareSceneRenderer final : public scene::SceneRenderer {
  rendering::AllocationLimits _limits;
  std::shared_ptr<rendering::ResourceLedger> _resources;

public:
  explicit SoftwareSceneRenderer(
      rendering::AllocationLimits limits = {},
      std::shared_ptr<rendering::ResourceLedger> resources =
          rendering::defaultResourceLedger())
      : _limits{limits}, _resources{std::move(resources)} {
    _limits.validate();
  }

  rendering::PaintImageHandle
  render(const scene::SceneRenderProps &view,
         std::span<const scene::MeshDraw> draws) override;
};

} // namespace playground::sdl
