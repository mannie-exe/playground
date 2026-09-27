#pragma once

#include <scene/SceneViewport.hpp>
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
  Patch<bool> toneMap;
};

class SceneView final : public Node {
  SceneViewProps _props;

  rendering::PaintImageHandle _image;
  std::uint64_t _renderedRevision{};
  rendering::ResourceDomainId _rendererDomain, _imageDomain;
  std::optional<scene::SceneViewport> _viewport;
  float _renderedAspect{};
  bool _prepared{};

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
