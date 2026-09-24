#pragma once

#include <scene/Scene2D.hpp>
#include <ui/Node.hpp>
#include <ui/Patch.hpp>

namespace playground::ui {

struct Scene2DViewProps {
  std::shared_ptr<const scene::Scene2D> scene;
  // Maps scene coordinates into this node's logical content box.
  math::Transform2D camera;
  math::Size2 preferredSize{320, 240};
};
struct Scene2DViewPatch {
  Patch<std::shared_ptr<const scene::Scene2D>> scene;
  Patch<math::Transform2D> camera;
  Patch<math::Size2> preferredSize;
};
class Scene2DView final : public Node {
  Scene2DViewProps _props;
  std::vector<scene::Item2DProps> _snapshot;
  std::optional<std::uint64_t> _preparedRevision;
  rendering::ResourceDomainId _imageDomain;
  bool _prepared{};

protected:
  layout::MeasureResult
  measureContent(MeasureContext &, const layout::SizeConstraints &) override;
  void prepareContent(PrepareContext &) override;
  void paint(PaintContext &) const override;

public:
  explicit Scene2DView(Scene2DViewProps props, layout::BoxProps box = {});
  const Scene2DViewProps &props() const noexcept { return _props; }
  void setProps(Scene2DViewProps props);
  void applyPatch(const Scene2DViewPatch &patch);
};

} // namespace playground::ui
