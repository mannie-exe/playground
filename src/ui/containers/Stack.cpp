#include "../detail/LayoutEngines.hpp"
#include <ui/containers/Stack.hpp>

namespace playground::ui {

void Stack::validatePlacement(const layout::StackPlacement &placement) const {
  placement.validate();
  if (placement.crossAlignmentOverride)
    layout::StackProps{0, {}, *placement.crossAlignmentOverride}.validate(
        _axis);
}

layout::MeasureResult
Stack::measureContent(MeasureContext &context,
                      const layout::SizeConstraints &offered) {
  const auto plan = container_detail::stackPlan(
      *this, container_detail::childIndices(*this),
      [&](auto index) -> const auto & { return placementInParent(index); },
      context, offered, _axis, effectiveProps());
  return {plan.size, plan.firstBaseline, plan.lastBaseline};
}

void Stack::arrangeChildren(ArrangeContext &context, math::Rect bounds) {
  const auto plan = container_detail::stackPlan(
      *this, container_detail::childIndices(*this),
      [&](auto index) -> const auto & { return placementInParent(index); },
      context, layout::SizeConstraints::tight(bounds.size), _axis,
      effectiveProps(), true);
  container_detail::arrangeStackPlan(*this, context, plan, bounds, _axis);
}

void Stack::setAxis(layout::Axis axis) {
  _props.validate(axis);
  for (std::size_t i = 0; i < children().size(); ++i)
    if (placementInParent(i).crossAlignmentOverride)
      layout::StackProps{0, {}, *placementInParent(i).crossAlignmentOverride}
          .validate(axis);
  if (_axis != axis) {
    _axis = axis;
    invalidateLayout();
  }
}

void Stack::setProps(layout::StackProps props) {
  props.validate(_axis);
  if (_props != props) {
    _props = props;
    invalidateLayout();
  }
}

void Stack::applyPatch(const StackPatch &patch) {
  const layout::StackProps defaults;
  setProps(
      {patch.gap.appliedTo(_props.gap, defaults.gap),
       patch.distribution.appliedTo(_props.distribution, defaults.distribution),
       patch.childrenAlignment.appliedTo(_props.childrenAlignment,
                                         defaults.childrenAlignment)});
}

void Stack::applyPlacementPatch(std::size_t index,
                                const StackPlacementPatch &patch) {
  const auto &value = placementInParent(index);
  const layout::StackPlacement defaults;
  setPlacementInParent(
      index,
      {patch.margin.appliedTo(value.margin, defaults.margin),
       patch.grow.appliedTo(value.grow, defaults.grow),
       patch.shrink.appliedTo(value.shrink, defaults.shrink),
       patch.crossAlignmentOverride.appliedTo(
           value.crossAlignmentOverride, defaults.crossAlignmentOverride)});
}

Node &Stack::append(std::unique_ptr<Node> child) {
  layout::StackPlacement placement;
  if (dynamic_cast<Spacer *>(child.get()))
    placement.grow = 1;
  return PlacementContainer::append(std::move(child), placement);
}

} // namespace playground::ui
