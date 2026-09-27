#pragma once

#include <algorithm>

#include <math/Path2D.hpp>
#include <rendering/PaintContext.hpp>

namespace playground::ui::control_paint {
inline void outline(rendering::PaintContext &p, math::Rect r,
                    math::ColorRGBA8 color, float width = 1) {
  width = std::min(width, std::min(r.w(), r.h()) / 2);
  p.fill(math::rect(r.x(), r.y(), r.w(), width), color);
  p.fill(math::rect(r.x(), r.bottom() - width, r.w(), width), color);
  p.fill(math::rect(r.x(), r.y(), width, r.h()), color);
  p.fill(math::rect(r.right() - width, r.y(), width, r.h()), color);
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
                    math::ColorRGBA8 color) {
  math::Path2D path;
  if (open)
    path.moveTo({at.x, at.y + 7})
        .lineTo({at.x + 6, at.y + 1})
        .lineTo({at.x + 12, at.y + 7});
  else
    path.moveTo({at.x, at.y + 1})
        .lineTo({at.x + 6, at.y + 7})
        .lineTo({at.x + 12, at.y + 1});
  p.drawPath(path, {.fill = {}, .stroke = color, .strokeWidth = 2});
}
} // namespace playground::ui::control_paint
