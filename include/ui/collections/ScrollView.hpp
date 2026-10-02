#pragma once

#include <ui/Node.hpp>

namespace playground::ui {

enum class ScrollAxes { Horizontal, Vertical, Both };
enum class ScrollbarPolicy { Never, Auto, Always };
enum class ScrollSizing { Fill, Content };

struct ScrollProps {
  ScrollAxes axes{ScrollAxes::Vertical};
  float wheelStep{32};
  ScrollbarPolicy scrollbar{ScrollbarPolicy::Auto};
  std::optional<float> scrollbarThickness, minimumThumb;
  std::optional<math::ColorRGBA8> scrollbarColor;
  ScrollSizing sizing{ScrollSizing::Fill};
  bool operator==(const ScrollProps &) const = default;

  void validate() const {
    if (sizing != ScrollSizing::Fill && sizing != ScrollSizing::Content)
      throw std::invalid_argument("Invalid scroll sizing");
    layout::detail::nonnegative(wheelStep,
                                "Wheel step must be finite and nonnegative");
    if (scrollbarThickness)
      layout::detail::nonnegative(*scrollbarThickness,
                                  "Invalid scrollbar thickness");
    if (minimumThumb)
      layout::detail::nonnegative(*minimumThumb,
                                  "Invalid scrollbar thumb extent");
  }
};

struct ScrollPatch {
  Patch<ScrollAxes> axes;
  Patch<float> wheelStep;
  Patch<ScrollbarPolicy> scrollbar;
  Patch<std::optional<float>> scrollbarThickness, minimumThumb;
  Patch<std::optional<math::ColorRGBA8>> scrollbarColor;
  Patch<ScrollSizing> sizing;
};

class ScrollView : public Node {
  ScrollProps _props;

  math::Vec2f _offset;
  std::optional<std::uint64_t> _dragPointer;
  layout::Axis _dragAxis{layout::Axis::Vertical};
  float _dragStart{}, _dragOffset{};

  math::Size2 _viewport;
  math::Size2 _extent;
  bool _horizontalBar{}, _verticalBar{};

  bool horizontal() const { return _props.axes != ScrollAxes::Vertical; }

  bool vertical() const { return _props.axes != ScrollAxes::Horizontal; }

  void clampOffset() noexcept;
  void resolveViewport(MeasureContext &, math::Size2 available);
  bool showBar(layout::Axis axis) const;
  math::Rect track(layout::Axis axis) const;
  math::Rect thumb(layout::Axis axis) const;

protected:
  bool hitTestOverlay(math::Point2 point) const override;
  layout::MeasureResult
  measureContent(MeasureContext &context,
                 const layout::SizeConstraints &offered) override;
  void arrangeChildren(ArrangeContext &context, math::Rect content) override;
  void paintSubtree(PaintContext &context) const override;

  void onDetach() noexcept override {
    _dragPointer.reset();
    releaseAllPointers();
  }

  void onDefaultEvent(UIEvent &event) override;

public:
  math::Rect clipBounds() const noexcept override {
    return visualProps().clipRect.value_or(
        math::inset(math::Rect{{}, bounds().size}, contentInsets()));
  }

  explicit ScrollView(std::unique_ptr<Node> content, ScrollProps props = {},
                      layout::BoxProps box = {});

  const ScrollProps &props() const noexcept { return _props; }

  ScrollProps effectiveProps() const {
    auto value = _props;
    value.scrollbarThickness =
        value.scrollbarThickness.value_or(themeMetrics().scrollbarThickness);
    value.minimumThumb =
        value.minimumThumb.value_or(themeMetrics().scrollbarMinimumThumb);
    value.scrollbarColor =
        resolveColor(&ThemePalette::scrollbar, value.scrollbarColor);
    return value;
  }

  Node *child() const noexcept {
    return children().empty() ? nullptr : children()[0].get();
  }

  void setChild(std::unique_ptr<Node> value);

  std::unique_ptr<Node> takeChild() {
    return children().empty() ? nullptr : takeChildAt(0);
  }

  void applyPatch(const ScrollPatch &p);
  void setProps(ScrollProps props);

  math::Vec2f offset() const noexcept { return _offset; }

  math::Size2 viewportExtent() const noexcept { return _viewport; }

  math::Size2 contentExtent() const noexcept { return _extent; }

  void setOffset(math::Vec2f value);

  void scrollBy(math::Vec2f delta) { setOffset(_offset + delta); }

  void scrollIntoView(math::Rect target,
                      std::optional<layout::Alignment> alignment = {});
  void scrollIntoView(const Node &target,
                      std::optional<layout::Alignment> alignment = {});
};
} // namespace playground::ui
