#pragma once

#include <optional>
#include <utility>

#include <ui/Node.hpp>
#include <ui/Patch.hpp>

namespace playground::ui {

struct RectangleProps {
  math::ColorRGBA8 fill;
  std::optional<math::ColorRGBA8> border;
  math::CornerRadii cornerRadii;

  bool operator==(const RectangleProps &) const = default;
};

struct RectanglePatch {
  Patch<math::ColorRGBA8> fill;
  Patch<std::optional<math::ColorRGBA8>> border;
  Patch<math::CornerRadii> cornerRadii;
};

class Rectangle final : public Node {
  RectangleProps _props;

protected:
  layout::MeasureResult
  measureContent(MeasureContext &, const layout::SizeConstraints &) override {
    return {};
  }

  void paint(PaintContext &context) const override {
    const math::Rect outer{{}, bounds().size};
    const auto borders = boxProps().borderWidths;
    if (_props.cornerRadii != math::CornerRadii{}) {
      const math::RoundedRect shape{outer, _props.cornerRadii};
      context.paintRoundedBox(shape, borders, _props.fill, _props.border);
      return;
    }
    if (_props.border) {
      const float top = std::min(borders.top, outer.h());
      const float bottom = std::min(borders.bottom, outer.h() - top);
      const float left = std::min(borders.left, outer.w());
      const float right = std::min(borders.right, outer.w() - left);
      context.fill(math::rect(0, 0, outer.w(), top), *_props.border);
      context.fill(math::rect(0, outer.h() - bottom, outer.w(), bottom),
                   *_props.border);
      context.fill(math::rect(0, top, left, outer.h() - top - bottom),
                   *_props.border);
      context.fill(
          math::rect(outer.w() - right, top, right, outer.h() - top - bottom),
          *_props.border);
    }
    context.fill(math::inset(outer, borders), _props.fill);
  }

public:
  explicit Rectangle(RectangleProps props, layout::BoxProps box = {})
      : Node{box}, _props{props} {
    props.cornerRadii.validate();
    setHitTestPolicy(HitTestPolicy::None);
  }

  const RectangleProps &props() const noexcept { return _props; }
  void setProps(RectangleProps props) {
    props.cornerRadii.validate();
    if (_props == props)
      return;
    _props = props;
    invalidate(DirtyFlags::Paint | DirtyFlags::HitTest);
  }
  void applyPatch(const RectanglePatch &patch) {
    const RectangleProps defaults{};
    setProps({patch.fill.appliedTo(_props.fill),
              patch.border.appliedTo(_props.border, defaults.border),
              patch.cornerRadii.appliedTo(_props.cornerRadii, {})});
  }
  bool containsLocal(math::Point2 point) const override {
    return math::RoundedRect{{{}, bounds().size}, _props.cornerRadii}.contains(
        point);
  }
};

} // namespace playground::ui
