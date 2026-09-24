#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <layout/LayoutPrimitives.hpp>
#include <rendering/PaintImage.hpp>

namespace playground::ui {

enum class ContentFit { None, Stretch, Contain, Cover, Shrink };

struct ContentStyle {
  ContentFit fit{ContentFit::Contain};
  layout::Alignment alignment{layout::Alignment::center()};
  rendering::ImagePaint paint;

  bool operator==(const ContentStyle &) const = default;
};

struct ResolvedContent {
  math::Rect source;
  math::Rect destination;
};

namespace content_detail {

void validate(const ContentStyle &style);

float alignmentFactor(layout::Align alignment, bool reverse = false);

inline math::Rect contentBounds(const math::Rect &bounds, math::Insets insets) {
  return math::inset({{}, bounds.size}, insets);
}

ResolvedContent resolve(math::Rect source, math::Size2 natural,
                        math::Rect destination, const ContentStyle &style,
                        layout::LayoutDirection direction);

} // namespace content_detail
} // namespace playground::ui
