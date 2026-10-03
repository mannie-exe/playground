#pragma once

#include <scene/SceneViewport.hpp>
#include <scene/WorldScene.hpp>
#include <support/Patch.hpp>
#include <ui/Node.hpp>

namespace playground::ui {

struct SceneViewProps {
  std::shared_ptr<const scene::Scene3D> scene;
  scene::CameraProps camera;
  math::Size2 preferredSize{320, 240};
  math::ColorRGBA8 clearColor{0, 0, 0, 255};
  float resolutionScale{1};
  std::optional<float> aspectRatio;
  scene::TransparentOrder transparentOrder{
      scene::TransparentOrder::BackToFront};
  scene::SceneRenderProps::Lighting lighting;
  float exposure{1};
  bool toneMap{};
  bool adaptiveResolution{true};
  // Exactly one source: scene or an immutable world extraction. World camera is
  // captured with its origin; camera remains the direct Scene3D source's
  // camera.
  std::shared_ptr<const scene::WorldSceneSnapshot> worldScene;
  bool operator==(const SceneViewProps &) const = default;
};

struct SceneViewPatch {
  Patch<std::shared_ptr<const scene::Scene3D>> scene;
  Patch<scene::CameraProps> camera;
  Patch<math::Size2> preferredSize;
  Patch<math::ColorRGBA8> clearColor;
  Patch<float> resolutionScale;
  Patch<std::optional<float>> aspectRatio;
  Patch<scene::TransparentOrder> transparentOrder;
  Patch<scene::SceneRenderProps::Lighting> lighting;
  Patch<float> exposure;
  Patch<bool> toneMap, adaptiveResolution;
  Patch<std::shared_ptr<const scene::WorldSceneSnapshot>> worldScene;
};

class SceneView final : public Node {
  SceneViewProps _props;
  std::shared_ptr<const void> _resourceOwner{std::make_shared<const int>(0)};

  rendering::PaintImageHandle _image;
  std::uint64_t _renderedRevision{};
  rendering::ResourceDomainId _rendererDomain, _imageDomain;
  std::optional<scene::SceneViewport> _viewport;
  float _renderedAspect{};
  bool _prepared{};
  rendering::Sampling _sampling{rendering::Sampling::Linear};
  const std::uint64_t _workload{nextUIWorkId()};

protected:
  layout::MeasureResult
  measureContent(MeasureContext &, const layout::SizeConstraints &) override;
  void prepareContent(PrepareContext &) override;
  void paint(PaintContext &) const override;

public:
  explicit SceneView(SceneViewProps props, layout::BoxProps box = {});

  const SceneViewProps &props() const noexcept { return _props; }

  void setProps(SceneViewProps props);
  void applyPatch(const SceneViewPatch &patch);

  const std::optional<scene::SceneViewport> &viewport() const noexcept {
    return _viewport;
  }
};

} // namespace playground::ui
