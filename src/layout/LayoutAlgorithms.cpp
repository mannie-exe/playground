#include <layout/LayoutAlgorithms.hpp>

namespace playground::layout {

float resolveSizeRule(SizeRule rule, float content,
                      const AxisConstraints &offered,
                      std::optional<float> percentageBasis) {
  offered.validate();
  detail::nonnegative(content, "Content extent must be finite and nonnegative");
  if (percentageBasis)
    detail::nonnegative(*percentageBasis,
                        "Percentage basis must be finite and nonnegative");
  float result = content;
  switch (rule.kind()) {
  case SizeKind::Content:
    break;
  case SizeKind::Fixed:
    result = rule.value();
    break;
  case SizeKind::Percent:
    if (percentageBasis)
      result =
          detail::checked(static_cast<double>(*percentageBasis) * rule.value());
    break;
  case SizeKind::Fill:
    if (offered.maximum)
      result = *offered.maximum;
    break;
  }
  return offered.clamp(result);
}

FlexAllocation allocateStack(std::span<const FlexItem> items, float available,
                             float gap) {
  detail::nonnegative(available,
                      "Available stack extent must be finite and nonnegative");
  detail::nonnegative(gap, "Stack gap must be finite and nonnegative");
  FlexAllocation result;
  result.sizes.reserve(items.size());
  double remaining =
      available -
      (items.empty() ? 0.0 : static_cast<double>(gap) * (items.size() - 1));
  for (const auto &item : items) {
    detail::nonnegative(item.basis,
                        "Flex basis must be finite and nonnegative");
    detail::nonnegative(item.grow,
                        "Grow weight must be finite and nonnegative");
    detail::nonnegative(item.shrink,
                        "Shrink weight must be finite and nonnegative");
    const float initial = item.limits.clamp(item.basis);
    result.sizes.push_back(initial);
    remaining -= initial;
  }
  const bool growing = remaining > 0;
  std::vector<double> weights(items.size());
  std::vector<double> sizes(result.sizes.begin(), result.sizes.end());
  for (std::size_t i = 0; i < items.size(); ++i) {
    weights[i] = items[i].fixed ? 0.0
                 : growing
                     ? items[i].grow
                     : static_cast<double>(items[i].shrink) * items[i].basis;
  }
  // Every clamped pass freezes at least one item. A nonclamped pass completes.
  for (std::size_t pass = 0; pass <= items.size() && remaining != 0; ++pass) {
    double total = 0;
    for (std::size_t i = 0; i < items.size(); ++i) {
      const bool atLimit = growing ? (items[i].limits.maximum &&
                                      sizes[i] >= *items[i].limits.maximum)
                                   : sizes[i] <= items[i].limits.minimum;
      if (atLimit)
        weights[i] = 0;
      total += weights[i];
    }
    if (total == 0)
      break;
    const double requested = remaining;
    bool froze = false;
    double applied = 0;
    for (std::size_t i = 0; i < items.size(); ++i) {
      if (weights[i] == 0)
        continue;
      const double old = sizes[i];
      const double proposal = old + requested * (weights[i] / total);
      sizes[i] =
          growing && items[i].limits.maximum
              ? std::min(proposal,
                         static_cast<double>(*items[i].limits.maximum))
          : !growing
              ? std::max(proposal, static_cast<double>(items[i].limits.minimum))
              : proposal;
      if (sizes[i] != proposal) {
        weights[i] = 0;
        froze = true;
      }
      applied += sizes[i] - old;
    }
    remaining -= applied;
    if (!froze || applied == 0)
      break;
  }
  double used =
      items.empty() ? 0.0 : static_cast<double>(gap) * (items.size() - 1);
  for (std::size_t i = 0; i < sizes.size(); ++i) {
    result.sizes[i] = detail::checked(sizes[i]);
    used += result.sizes[i];
  }
  const double residual = static_cast<double>(available) - used;
  if (std::abs(residual) > std::numeric_limits<float>::max())
    throw std::overflow_error("Stack overflow exceeds float range");
  result.remaining = static_cast<float>(residual);
  return result;
}

DistributionOffsets distributionOffsets(Distribution distribution,
                                        float remaining, std::size_t count,
                                        float gap) {
  detail::finite(remaining, "Remaining extent must be finite");
  detail::nonnegative(gap, "Gap must be finite and nonnegative");
  DistributionOffsets result{0, gap};
  if (count == 0)
    return result;
  const double free = std::max(0.0f, remaining);
  switch (distribution) {
  case Distribution::Start:
    break;
  case Distribution::Center:
    result.leading = static_cast<float>(free / 2);
    break;
  case Distribution::End:
    result.leading = static_cast<float>(free);
    break;
  case Distribution::SpaceBetween:
    if (count > 1)
      result.between = detail::checked(gap + free / (count - 1));
    break;
  case Distribution::SpaceAround:
    result.between = detail::checked(gap + free / count);
    result.leading = static_cast<float>(free / count / 2);
    break;
  case Distribution::SpaceEvenly:
    result.leading =
        static_cast<float>(free / (static_cast<double>(count) + 1));
    result.between = detail::checked(static_cast<double>(gap) + result.leading);
    break;
  }
  return result;
}

float alignmentOffset(Align alignment, float available, float extent,
                      bool reverse) {
  detail::nonnegative(available,
                      "Alignment extent must be finite and nonnegative");
  detail::nonnegative(extent, "Child extent must be finite and nonnegative");
  const float spare = std::max(0.0f, available - extent);
  float offset = alignment == Align::Center ? spare / 2
                 : alignment == Align::End  ? spare
                                            : 0;
  // Mirror from the physical right edge, including when the child overflows.
  // Clamping this difference to zero would move RTL Start to the left edge.
  return reverse ? available - extent - offset : offset;
}

Rect alignBounds(Rect available, Size2 child, Alignment alignment,
                 Insets margin, LayoutDirection direction) {
  detail::insets(margin);
  detail::finite(available.position.x, "Position must be finite");
  detail::finite(available.position.y, "Position must be finite");
  detail::nonnegative(available.size.width,
                      "Width must be finite and nonnegative");
  detail::nonnegative(available.size.height,
                      "Height must be finite and nonnegative");
  detail::nonnegative(child.width, "Width must be finite and nonnegative");
  detail::nonnegative(child.height, "Height must be finite and nonnegative");
  const float width =
      std::max(0.0f, available.size.width - margin.left - margin.right);
  const float height =
      std::max(0.0f, available.size.height - margin.top - margin.bottom);
  if (alignment.horizontal == Align::Stretch)
    child.width = width;
  if (alignment.vertical == Align::Stretch)
    child.height = height;
  const Point2 position{
      available.position.x + margin.left +
          alignmentOffset(alignment.horizontal, width, child.width,
                          direction == LayoutDirection::RightToLeft),
      available.position.y + margin.top +
          alignmentOffset(alignment.vertical, height, child.height)};
  detail::finite(position.x, "Aligned position exceeds float range");
  detail::finite(position.y, "Aligned position exceeds float range");
  return {position, child};
}

std::vector<FlowLine> breakFlowLines(std::span<const float> outerExtents,
                                     std::optional<float> capacity, float gap) {
  if (capacity)
    detail::nonnegative(*capacity,
                        "Line capacity must be finite and nonnegative");
  detail::nonnegative(gap, "Flow gap must be finite and nonnegative");
  std::vector<FlowLine> lines;
  FlowLine line;
  for (std::size_t i = 0; i < outerExtents.size(); ++i) {
    detail::nonnegative(outerExtents[i],
                        "Flow item extent must be finite and nonnegative");
    const double next = static_cast<double>(line.mainExtent) +
                        (line.end > line.begin ? gap : 0) + outerExtents[i];
    if (capacity && line.end > line.begin && next > *capacity) {
      lines.push_back(line);
      line = {i, i, 0};
    }
    line.mainExtent =
        detail::checked(static_cast<double>(line.mainExtent) +
                        (line.end > line.begin ? gap : 0) + outerExtents[i]);
    line.end = i + 1;
  }
  if (line.begin != line.end)
    lines.push_back(line);
  return lines;
}

AnchoredAxis resolveAnchor(const AnchorAxis &anchor, float available,
                           float outerExtent, bool fixed, bool reverse) {
  detail::nonnegative(available,
                      "Anchor available extent must be finite and nonnegative");
  detail::nonnegative(outerExtent,
                      "Anchor child extent must be finite and nonnegative");
  return std::visit(
      [&](const auto &value) -> AnchoredAxis {
        value.validate();
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, AnchorPosition>) {
          const double position =
              static_cast<double>(value.parentFraction) * available -
              static_cast<double>(value.selfFraction) * outerExtent +
              value.offset;
          const double resolved =
              reverse ? available - outerExtent - position : position;
          if (!std::isfinite(resolved) ||
              std::abs(resolved) > std::numeric_limits<float>::max())
            throw std::overflow_error("Anchor position exceeds float range");
          return {static_cast<float>(resolved), outerExtent};
        } else {
          if (fixed)
            throw std::invalid_argument(
                "StretchBetween conflicts with a fixed size");
          const float extent =
              std::max(0.0f, available - value.startInset - value.endInset);
          return {reverse ? available - value.startInset - extent
                          : value.startInset,
                  extent};
        }
      },
      anchor);
}

} // namespace playground::layout
