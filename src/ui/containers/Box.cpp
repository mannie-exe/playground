#include <ui/containers/Box.hpp>

namespace playground::ui {

layout::MeasureResult
Box::measureContent(MeasureContext &context,
                    const layout::SizeConstraints &offered) {
  if (children().empty() || children()[0]->isPortal() ||
      children()[0]->visibility() == Visibility::Collapsed)
    return {};
  const auto margin = placementInParent(0).margin;
  auto measured = children()[0]->measure(
      context, container_detail::loose(offered.deflated(margin)));
  measured.size.width =
      container_detail::sum(measured.size.width, margin.horizontal());
  measured.size.height =
      container_detail::sum(measured.size.height, margin.vertical());
  if (measured.firstBaseline)
    *measured.firstBaseline += margin.top;
  if (measured.lastBaseline)
    *measured.lastBaseline += margin.top;
  return measured;
}

void Box::arrangeChildren(ArrangeContext &context, math::Rect bounds) {
  if (!children().empty() && !children()[0]->isPortal()) {
    const auto &placement = placementInParent(0);
    container_detail::placeAligned(
        *children()[0], context, bounds, placement.margin,
        placement.alignmentOverride.value_or(_props.contentAlignment));
  }
}

void Box::setContentProps(BoxContentProps props) {
  if (_props != props) {
    _props = props;
    invalidateLayout();
  }
}

void Box::applyPlacementPatch(const BoxPlacementPatch &patch) {
  const auto &current = placementInParent(0);
  setPlacementInParent(
      0, {patch.margin.appliedTo(current.margin, {}),
          patch.alignmentOverride.appliedTo(current.alignmentOverride, {})});
}

Node &Box::setChild(std::unique_ptr<Node> child,
                    layout::BoxPlacement placement) {
  // Attach the replacement first: allocation/attachment failures preserve the
  // old child.
  Node &result = PlacementContainer::append(std::move(child), placement);
  if (children().size() > 1)
    PlacementContainer::remove(0);
  return result;
}

} // namespace playground::ui
