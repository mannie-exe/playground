#pragma once

#include <ui/containers/Container.hpp>

namespace playground::ui {

struct AnchorPlacementPatch {
  Patch<layout::AnchorAxis> horizontal;
  Patch<layout::AnchorAxis> vertical;
  Patch<math::Insets> margin;
};

class AnchorLayout : public PlacementContainer<layout::AnchorPlacement> {
protected:
  layout::MeasureResult
  measureContent(MeasureContext &context,
                 const layout::SizeConstraints &offered) override;

  void arrangeChildren(ArrangeContext &context, math::Rect bounds) override;

public:
  explicit AnchorLayout(layout::BoxProps box = {}) : PlacementContainer{box} {}
  void applyPlacementPatch(NodeId child, const AnchorPlacementPatch &patch) {
    applyPlacementPatch(indexOf(child), patch);
  }
  void applyPlacementPatch(std::size_t index,
                           const AnchorPlacementPatch &patch);
};

} // namespace playground::ui
