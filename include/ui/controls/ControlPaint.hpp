#pragma once

#include <algorithm>
#include <cmath>

#include <math/Path2D.hpp>
#include <rendering/PaintContext.hpp>
#include <ui/Theme.hpp>

namespace playground::ui::control_paint {
inline void outline(rendering::PaintContext &p, math::Rect r,
                    math::ColorRGBA8 color, float width = 1) {
  width = std::min(width, std::min(r.w(), r.h()) / 2);
  p.fill(math::rect(r.x(), r.y(), r.w(), width), color);
  p.fill(math::rect(r.x(), r.bottom() - width, r.w(), width), color);
  p.fill(math::rect(r.x(), r.y(), width, r.h()), color);
  p.fill(math::rect(r.right() - width, r.y(), width, r.h()), color);
}

inline void disabledOutline(rendering::PaintContext &p, math::Rect r,
                            const ThemePalette &colors, const ThemeMetrics &m) {
  if (!colors.highContrast || !r.hasArea())
    return;
  const float width = std::min(m.borderWidth, std::min(r.w(), r.h()) / 2);
  const auto edge = [&](float length, bool horizontal) {
    // Bound work even for unusually large authored bounds.
    const int count = static_cast<int>(
        std::clamp(std::ceil(length / m.disabledDash), 1.f, 128.f));
    const float segment = length / count;
    for (int i = 0; i < count; i += 2) {
      if (horizontal) {
        p.fill(math::rect(r.x() + i * segment, r.y(), segment, width),
               colors.mutedText);
        p.fill(
            math::rect(r.x() + i * segment, r.bottom() - width, segment, width),
            colors.mutedText);
      } else {
        p.fill(math::rect(r.x(), r.y() + i * segment, width, segment),
               colors.mutedText);
        p.fill(
            math::rect(r.right() - width, r.y() + i * segment, width, segment),
            colors.mutedText);
      }
    }
  };
  edge(r.w(), true);
  edge(r.h(), false);
}

inline void circle(rendering::PaintContext &p, math::Rect r,
                   math::ColorRGBA8 color) {
  const float x = r.x(), y = r.y(), w = r.w(), h = r.h(), k = 0.276142375f;
  math::Path2D path;
  path.moveTo({x + w / 2, y})
      .cubicTo({x + w / 2 + k * w, y}, {x + w, y + h / 2 - k * h},
               {x + w, y + h / 2})
      .cubicTo({x + w, y + h / 2 + k * h}, {x + w / 2 + k * w, y + h},
               {x + w / 2, y + h})
      .cubicTo({x + w / 2 - k * w, y + h}, {x, y + h / 2 + k * h},
               {x, y + h / 2})
      .cubicTo({x, y + h / 2 - k * h}, {x + w / 2 - k * w, y}, {x + w / 2, y})
      .close();
  p.drawPath(path, {.fill = color});
}

inline void chevron(rendering::PaintContext &p, math::Point2 at, bool open,
                    math::ColorRGBA8 color, const ThemeMetrics &m) {
  math::Path2D path;
  const float top = m.indicatorStroke / 2, bottom = m.chevronHeight - top;
  path.moveTo({at.x, at.y + (open ? bottom : top)})
      .lineTo({at.x + m.chevronWidth / 2, at.y + (open ? top : bottom)})
      .lineTo({at.x + m.chevronWidth, at.y + (open ? bottom : top)});
  p.drawPath(path,
             {.fill = {}, .stroke = color, .strokeWidth = m.indicatorStroke});
}
} // namespace playground::ui::control_paint
