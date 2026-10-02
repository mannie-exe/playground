#pragma once

#include <ui/containers/Stack.hpp>

namespace playground::ui {

struct FlowPatch {
  Patch<layout::Axis> mainAxis;
  Patch<std::optional<float>> itemGap, lineGap;
  Patch<layout::Distribution> distribution;
  Patch<layout::CrossAlignment> childrenAlignment;
};

class Flow : public PlacementContainer<layout::StackPlacement> {
  layout::FlowProps _props;

protected:
  void validatePlacement(const layout::StackPlacement &p) const override;
  layout::MeasureResult
  measureContent(MeasureContext &context,
                 const layout::SizeConstraints &offered) override;
  void arrangeChildren(ArrangeContext &context, math::Rect bounds) override;

public:
  explicit Flow(layout::FlowProps props = {}, layout::BoxProps box = {})
      : PlacementContainer{box}, _props{props} {
    _props.validate();
  }

  const layout::FlowProps &props() const noexcept { return _props; }

  layout::FlowProps effectiveProps() const noexcept {
    auto value = _props;
    const auto gap = resolvedControlStyle().gap.value_or(0);
    value.itemGap = controlStyle().gap.value_or(_props.itemGap.value_or(gap));
    value.lineGap = controlStyle().gap.value_or(_props.lineGap.value_or(gap));
    return value;
  }

  void setProps(layout::FlowProps props);
  void applyPatch(const FlowPatch &patch);

  void applyPlacementPatch(NodeId child, const StackPlacementPatch &patch) {
    applyPlacementPatch(indexOf(child), patch);
  }

  void applyPlacementPatch(std::size_t index, const StackPlacementPatch &patch);
};

} // namespace playground::ui
