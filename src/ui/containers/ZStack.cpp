#include <ui/containers/ZStack.hpp>

namespace playground::ui {

layout::MeasureResult
ZStack::measureContent(MeasureContext &context,
                       const layout::SizeConstraints &offered) {
  math::Size2 result;
  for (std::size_t i = 0; i < children().size(); ++i) {
    if (children()[i]->visibility() == Visibility::Collapsed)
      continue;
    const auto margin = placementInParent(i).margin;
    const auto measured = children()[i]->measure(
        context, container_detail::loose(offered.deflated(margin)));
    result.width =
        std::max(result.width, container_detail::sum(measured.size.width,
                                                     margin.horizontal()));
    result.height =
        std::max(result.height, container_detail::sum(measured.size.height,
                                                      margin.vertical()));
  }
  return {result, {}, {}};
}

void ZStack::arrangeChildren(ArrangeContext &context, math::Rect bounds) {
  for (std::size_t i = 0; i < children().size(); ++i) {
    if (children()[i]->visibility() == Visibility::Collapsed)
      continue;
    const auto &placement = placementInParent(i);
    container_detail::placeAligned(
        *children()[i], context, bounds, placement.margin,
        placement.alignmentOverride.value_or(_props.childrenAlignment));
  }
}

void ZStack::setProps(ZStackProps value) {
  if (_props != value) {
    _props = value;
    invalidateLayout();
  }
}

void ZStack::applyPlacementPatch(std::size_t index,
                                 const BoxPlacementPatch &patch) {
  const auto &value = placementInParent(index);
  setPlacementInParent(
      index, {patch.margin.appliedTo(value.margin, {}),
              patch.alignmentOverride.appliedTo(value.alignmentOverride, {})});
}

} // namespace playground::ui
