#pragma once

#include <ui/containers/Box.hpp>

namespace playground::ui {

struct ZStackProps {
  layout::Alignment childrenAlignment;
  bool operator==(const ZStackProps &) const = default;
};

struct ZStackPatch {
  Patch<layout::Alignment> childrenAlignment;
};

class ZStack : public PlacementContainer<layout::LayerPlacement> {
  ZStackProps _props;

protected:
  layout::MeasureResult
  measureContent(MeasureContext &context,
                 const layout::SizeConstraints &offered) override;

  void arrangeChildren(ArrangeContext &context, math::Rect bounds) override;

public:
  explicit ZStack(ZStackProps props = {}, layout::BoxProps box = {})
      : PlacementContainer{box}, _props{props} {}
  const ZStackProps &props() const noexcept { return _props; }
  void setProps(ZStackProps value);
  void applyPatch(const ZStackPatch &patch) {
    setProps({patch.childrenAlignment.appliedTo(_props.childrenAlignment, {})});
  }
  void applyPlacementPatch(NodeId child, const BoxPlacementPatch &patch) {
    applyPlacementPatch(indexOf(child), patch);
  }
  void applyPlacementPatch(std::size_t index, const BoxPlacementPatch &patch);
};

} // namespace playground::ui
