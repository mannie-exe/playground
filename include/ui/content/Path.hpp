#pragma once

#include <rendering/PathPaint.hpp>
#include <ui/Node.hpp>
#include <ui/Patch.hpp>
#include <ui/content/ContentTypes.hpp>

namespace playground::ui {

struct PathProps {
  math::Path2D path;
  math::Rect viewBox;
  rendering::PathPaint paint;
  ContentFit fit{ContentFit::Contain};
  layout::Alignment alignment{layout::Alignment::center()};
  bool operator==(const PathProps &) const = default;
};
struct PathPatch {
  Patch<math::Path2D> path;
  Patch<math::Rect> viewBox;
  Patch<rendering::PathPaint> paint;
  Patch<ContentFit> fit;
  Patch<layout::Alignment> alignment;
};

class Path final : public Node {
  PathProps _props;

  layout::LayoutDirection _direction{layout::LayoutDirection::LeftToRight};

  static void validate(const PathProps &);

protected:
  layout::MeasureResult
  measureContent(MeasureContext &, const layout::SizeConstraints &) override {
    return {.size = _props.viewBox.size};
  }
  void arrangeChildren(ArrangeContext &context, math::Rect) override {
    _direction = context.direction;
  }
  void paint(PaintContext &) const override;

public:
  explicit Path(PathProps props, layout::BoxProps box = {});
  const PathProps &props() const noexcept { return _props; }
  void setProps(PathProps);
  void applyPatch(const PathPatch &);
};
} // namespace playground::ui
