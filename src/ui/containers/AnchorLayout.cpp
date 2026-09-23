#include <ui/containers/AnchorLayout.hpp>

namespace playground::ui {

layout::MeasureResult
AnchorLayout::measureContent(MeasureContext &context,
                             const layout::SizeConstraints &offered) {
  if ((!offered.width.maximum || !offered.height.maximum) &&
      context.diagnostics)
    context.diagnostics->report(
        id(), LayoutPhase::Measure, LayoutIssue::IndefiniteAnchor,
        "AnchorLayout needs definite parent dimensions; using minima on "
        "indefinite axes");
  // Anchored descendants do not drive intrinsic size or trigger size cycles.
  return {{offered.width.maximum.value_or(offered.width.minimum),
           offered.height.maximum.value_or(offered.height.minimum)},
          {},
          {}};
}

void AnchorLayout::arrangeChildren(ArrangeContext &context, math::Rect bounds) {
  for (std::size_t i = 0; i < children().size(); ++i) {
    auto &child = *children()[i];
    if (child.visibility() == Visibility::Collapsed)
      continue;
    const auto &placement = placementInParent(i);
    const bool fixedWidth =
        child.boxProps().width.kind() == layout::SizeKind::Fixed;
    const bool fixedHeight =
        child.boxProps().height.kind() == layout::SizeKind::Fixed;
    if ((fixedWidth && std::holds_alternative<layout::StretchBetween>(
                           placement.horizontal)) ||
        (fixedHeight &&
         std::holds_alternative<layout::StretchBetween>(placement.vertical)))
      throw std::invalid_argument("Fixed size conflicts with stretch anchors");
    auto offered = container_detail::loose(
        layout::SizeConstraints::tight(bounds.size).deflated(placement.margin));
    if (std::holds_alternative<layout::StretchBetween>(placement.horizontal)) {
      const auto axis =
          layout::resolveAnchor(placement.horizontal, bounds.size.width, 0);
      offered.width = layout::AxisConstraints::tight(
          container_detail::limits(child, layout::Axis::Horizontal)
              .clamp(
                  std::max(0.0f, axis.extent - placement.margin.horizontal())));
    }
    if (std::holds_alternative<layout::StretchBetween>(placement.vertical)) {
      const auto axis =
          layout::resolveAnchor(placement.vertical, bounds.size.height, 0);
      offered.height = layout::AxisConstraints::tight(
          container_detail::limits(child, layout::Axis::Vertical)
              .clamp(
                  std::max(0.0f, axis.extent - placement.margin.vertical())));
    }
    const auto measured = child.measure(context, offered);
    auto x = layout::resolveAnchor(
        placement.horizontal, bounds.size.width,
        container_detail::sum(measured.size.width,
                              placement.margin.horizontal()),
        fixedWidth, context.direction == layout::LayoutDirection::RightToLeft);
    const auto y = layout::resolveAnchor(
        placement.vertical, bounds.size.height,
        container_detail::sum(measured.size.height,
                              placement.margin.vertical()),
        fixedHeight);
    // A capped stretch dimension remains at logical Start, including RTL.
    if (context.direction == layout::LayoutDirection::RightToLeft &&
        std::holds_alternative<layout::StretchBetween>(placement.horizontal))
      x.position +=
          x.extent - measured.size.width - placement.margin.horizontal();
    child.arrange(context,
                  {{bounds.position.x + x.position + placement.margin.left,
                    bounds.position.y + y.position + placement.margin.top},
                   measured.size});
  }
}

void AnchorLayout::applyPlacementPatch(std::size_t index,
                                       const AnchorPlacementPatch &patch) {
  const auto &value = placementInParent(index);
  const layout::AnchorPlacement defaults;
  setPlacementInParent(
      index, {patch.horizontal.appliedTo(value.horizontal, defaults.horizontal),
              patch.vertical.appliedTo(value.vertical, defaults.vertical),
              patch.margin.appliedTo(value.margin, defaults.margin)});
}

} // namespace playground::ui
