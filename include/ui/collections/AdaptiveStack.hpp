#pragma once

#include <layout/Breakpoints.hpp>
#include <ui/containers/Stack.hpp>

namespace playground::ui {

struct AdaptiveStackProps {
  layout::BreakpointProps<layout::Axis> breakpoints{.fallback =
                                                        layout::Axis::Vertical};
  layout::StackProps stack;
  bool operator==(const AdaptiveStackProps &) const = default;
  void validate() const {
    layout::BreakpointSet<layout::Axis>::validate(breakpoints);
    stack.validate(breakpoints.fallback);
    for (const auto &rule : breakpoints.rules)
      stack.validate(rule.mode);
  }
};
struct AdaptiveStackPatch {
  layout::BreakpointPatch<layout::Axis> breakpoints;
  Patch<layout::StackProps> stack;
};

class AdaptiveStack : public PlacementContainer<layout::StackPlacement> {
  AdaptiveStackProps _props;

  layout::BreakpointSet<layout::Axis> _breakpoints;
  layout::Axis _selected{layout::Axis::Vertical};

  layout::Axis select(const layout::SizeConstraints &offered) const;

protected:
  void validatePlacement(const layout::StackPlacement &p) const override;
  layout::MeasureResult
  measureContent(MeasureContext &context,
                 const layout::SizeConstraints &offered) override;
  void arrangeChildren(ArrangeContext &context, math::Rect bounds) override;

public:
  explicit AdaptiveStack(AdaptiveStackProps props = {},
                         layout::BoxProps box = {})
      : PlacementContainer{box}, _props{std::move(props)},
        _breakpoints{_props.breakpoints} {
    _props.validate();
  }
  const AdaptiveStackProps &props() const noexcept { return _props; }
  void applyPlacementPatch(NodeId child, const StackPlacementPatch &patch) {
    applyPlacementPatch(indexOf(child), patch);
  }
  void applyPlacementPatch(std::size_t index, const StackPlacementPatch &patch);
  layout::Axis selectedAxis() const noexcept { return _selected; }
  void setProps(AdaptiveStackProps value);
  void applyPatch(const AdaptiveStackPatch &p);
};

} // namespace playground::ui
