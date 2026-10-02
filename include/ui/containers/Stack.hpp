#pragma once

#include <ui/containers/Container.hpp>

namespace playground::ui {

struct StackPatch {
  Patch<float> gap;
  Patch<layout::Distribution> distribution;
  Patch<layout::CrossAlignment> childrenAlignment;
};

struct StackPlacementPatch {
  Patch<math::Insets> margin;
  Patch<float> grow;
  Patch<float> shrink;
  Patch<std::optional<layout::CrossAlignment>> crossAlignmentOverride;
};

class Spacer : public Node {
protected:
  layout::MeasureResult
  measureContent(MeasureContext &, const layout::SizeConstraints &) override {
    return {};
  }

public:
  explicit Spacer(layout::BoxProps box = {}) : Node{box} {
    setHitTestPolicy(HitTestPolicy::None);
  }
};

class Stack : public PlacementContainer<layout::StackPlacement> {
  layout::Axis _axis;
  layout::StackProps _props;

protected:
  void
  validatePlacement(const layout::StackPlacement &placement) const override;

  layout::MeasureResult
  measureContent(MeasureContext &context,
                 const layout::SizeConstraints &offered) override;

  void arrangeChildren(ArrangeContext &context, math::Rect bounds) override;

public:
  explicit Stack(layout::Axis axis, layout::StackProps props = {},
                 layout::BoxProps box = {})
      : PlacementContainer{box}, _axis{axis}, _props{props} {
    _props.validate(_axis);
  }

  layout::StackProps effectiveProps() const noexcept {
    auto value = _props;
    if (auto gap = resolvedControlStyle().gap)
      value.gap = *gap;
    return value;
  }

  layout::Axis axis() const noexcept { return _axis; }

  const layout::StackProps &props() const noexcept { return _props; }

  void setAxis(layout::Axis axis);

  void setProps(layout::StackProps props);

  void applyPatch(const StackPatch &patch);

  void applyPlacementPatch(NodeId child, const StackPlacementPatch &patch) {
    applyPlacementPatch(indexOf(child), patch);
  }

  void applyPlacementPatch(std::size_t index, const StackPlacementPatch &patch);

  Node &append(std::unique_ptr<Node> child, layout::StackPlacement placement) {
    return PlacementContainer::append(std::move(child), placement);
  }

  Node &append(std::unique_ptr<Node> child);
};

class HStack : public Stack {
public:
  explicit HStack(layout::StackProps props = {}, layout::BoxProps box = {})
      : Stack{layout::Axis::Horizontal, props, box} {}
};

class VStack : public Stack {
public:
  explicit VStack(layout::StackProps props = {}, layout::BoxProps box = {})
      : Stack{layout::Axis::Vertical, props, box} {}
};

} // namespace playground::ui
