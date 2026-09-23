#include "../detail/LayoutEngines.hpp"
#include <ui/collections/AdaptiveStack.hpp>

namespace playground::ui {

layout::Axis
AdaptiveStack::select(const layout::SizeConstraints &offered) const {
  return _breakpoints.select(offered);
}

void AdaptiveStack::validatePlacement(const layout::StackPlacement &p) const {
  p.validate();
  if (p.crossAlignmentOverride) {
    layout::StackProps{0, {}, *p.crossAlignmentOverride}.validate(
        _props.breakpoints.fallback);
    for (const auto &c : _props.breakpoints.rules)
      layout::StackProps{0, {}, *p.crossAlignmentOverride}.validate(c.mode);
  }
}

layout::MeasureResult
AdaptiveStack::measureContent(MeasureContext &context,
                              const layout::SizeConstraints &offered) {
  _selected = select(offered);
  const auto plan = container_detail::stackPlan(
      *this, container_detail::childIndices(*this),
      [&](auto i) -> const auto & { return placementInParent(i); }, context,
      offered, _selected, _props.stack);
  return {plan.size, plan.firstBaseline, plan.lastBaseline};
}

void AdaptiveStack::arrangeChildren(ArrangeContext &context,
                                    math::Rect bounds) {
  const auto offered = layout::SizeConstraints::tight(bounds.size);
  _selected = select(offered);
  const auto plan = container_detail::stackPlan(
      *this, container_detail::childIndices(*this),
      [&](auto i) -> const auto & { return placementInParent(i); }, context,
      offered, _selected, _props.stack, true);
  container_detail::arrangeStackPlan(*this, context, plan, bounds, _selected);
}

void AdaptiveStack::applyPlacementPatch(std::size_t index,
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

void AdaptiveStack::setProps(AdaptiveStackProps value) {
  value.validate();
  for (std::size_t i = 0; i < children().size(); ++i)
    if (auto cross = placementInParent(i).crossAlignmentOverride) {
      layout::StackProps{0, {}, *cross}.validate(value.breakpoints.fallback);
      for (const auto &c : value.breakpoints.rules)
        layout::StackProps{0, {}, *cross}.validate(c.mode);
    }
  if (value != _props) {
    layout::BreakpointSet<layout::Axis> selector{value.breakpoints};
    _props = std::move(value);
    _breakpoints = std::move(selector);
    invalidateLayout();
  }
}

void AdaptiveStack::applyPatch(const AdaptiveStackPatch &p) {
  const AdaptiveStackProps d;
  auto selector = _breakpoints;
  selector.applyPatch(p.breakpoints, d.breakpoints);
  setProps({selector.props(), p.stack.appliedTo(_props.stack, d.stack)});
}

} // namespace playground::ui
