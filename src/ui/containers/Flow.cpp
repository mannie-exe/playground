#include "../detail/LayoutEngines.hpp"
#include <ui/containers/Flow.hpp>

namespace playground::ui {

void Flow::validatePlacement(const layout::StackPlacement &p) const {
  p.validate();
  if (p.crossAlignmentOverride)
    layout::StackProps{0, {}, *p.crossAlignmentOverride}.validate(
        _props.mainAxis);
}

void Flow::setProps(layout::FlowProps props) {
  props.validate();
  for (std::size_t i = 0; i < children().size(); ++i)
    if (placementInParent(i).crossAlignmentOverride)
      layout::StackProps{0, {}, *placementInParent(i).crossAlignmentOverride}
          .validate(props.mainAxis);
  if (_props != props) {
    _props = props;
    invalidateLayout();
  }
}

void Flow::applyPatch(const FlowPatch &patch) {
  const layout::FlowProps defaults;
  setProps(
      {patch.mainAxis.appliedTo(_props.mainAxis, defaults.mainAxis),
       patch.itemGap.appliedTo(_props.itemGap, defaults.itemGap),
       patch.lineGap.appliedTo(_props.lineGap, defaults.lineGap),
       patch.distribution.appliedTo(_props.distribution, defaults.distribution),
       patch.childrenAlignment.appliedTo(_props.childrenAlignment,
                                         defaults.childrenAlignment)});
}

void Flow::applyPlacementPatch(std::size_t index,
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

layout::MeasureResult
Flow::measureContent(MeasureContext &context,
                     const layout::SizeConstraints &offered) {
  auto engine = container_detail::FlowEngine{
      *this, _props, [this](std::size_t i) -> const layout::StackPlacement & {
        return placementInParent(i);
      }};
  return engine.measure(context, offered);
}

void Flow::arrangeChildren(ArrangeContext &context, math::Rect bounds) {
  auto engine = container_detail::FlowEngine{
      *this, _props, [this](std::size_t i) -> const layout::StackPlacement & {
        return placementInParent(i);
      }};
  engine.arrange(context, bounds);
}

} // namespace playground::ui
