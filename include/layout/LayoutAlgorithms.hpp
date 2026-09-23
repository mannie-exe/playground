#pragma once

#include "layout/LayoutPrimitives.hpp"

#include <span>
#include <type_traits>

namespace playground::layout {

float resolveSizeRule(SizeRule rule, float content,
                      const AxisConstraints &offered,
                      std::optional<float> percentageBasis = std::nullopt);

struct FlexItem {
  float basis{};
  AxisConstraints limits;
  float grow{};
  float shrink{1};
  bool fixed{};
};

struct FlexAllocation {
  std::vector<float> sizes;
  float remaining{};
};

// Margins are excluded: callers subtract them from available before allocation.
// Shrink weights retain the initial basis, including after other items freeze.
FlexAllocation allocateStack(std::span<const FlexItem> items, float available,
                             float gap = 0);

struct DistributionOffsets {
  float leading{};
  float between{};
};

DistributionOffsets distributionOffsets(Distribution distribution,
                                        float remaining, std::size_t count,
                                        float gap = 0);

float alignmentOffset(Align alignment, float available, float extent,
                      bool reverse = false);

// The result is a child's border box. Insets describe its outer margin box.
Rect alignBounds(Rect available, Size2 child, Alignment alignment,
                 Insets margin = {},
                 LayoutDirection direction = LayoutDirection::LeftToRight);

struct FlowLine {
  std::size_t begin{};
  std::size_t end{};
  float mainExtent{};
};

std::vector<FlowLine> breakFlowLines(std::span<const float> outerExtents,
                                     std::optional<float> capacity,
                                     float gap = 0);

struct AnchoredAxis {
  float position{};
  float extent{};
};

AnchoredAxis resolveAnchor(const AnchorAxis &anchor, float available,
                           float outerExtent, bool fixed = false,
                           bool reverse = false);

} // namespace playground::layout
