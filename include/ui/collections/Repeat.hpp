#pragma once

#include <ui/collections/Collection.hpp>
#include <ui/containers/Flow.hpp>
#include <ui/containers/Grid.hpp>
#include <ui/containers/Stack.hpp>

namespace playground::ui {
enum class RepeatLayout { Stack, Grid, Flow };

struct RepeatProps {
  layout::Axis axis{layout::Axis::Vertical};
  layout::StackProps stack;
  RepeatLayout layout{RepeatLayout::Stack};
  playground::layout::GridProps grid;
  playground::layout::FlowProps flow;

  void validate() const {
    stack.validate(axis);
    grid.validate();
    flow.validate();
  }

  bool operator==(const RepeatProps &) const = default;
};

struct RepeatPatch {
  Patch<layout::Axis> axis;
  Patch<layout::StackProps> stack;
  Patch<RepeatLayout> layout;
  Patch<playground::layout::GridProps> grid;
  Patch<playground::layout::FlowProps> flow;
};

class Repeat : public detail::KeyedChildren {
  RepeatProps _props;
  layout::StackPlacement _placement;
  layout::GridPlacement _gridPlacement;

protected:
  void prepareChildren(MeasureContext &,
                       const layout::SizeConstraints &) override {
    reconcile(_keys);
  }

  layout::MeasureResult
  measureContent(MeasureContext &context,
                 const layout::SizeConstraints &offered) override;
  void arrangeChildren(ArrangeContext &context, math::Rect bounds) override;

public:
  Repeat(std::shared_ptr<const CollectionSource> source, ItemFactory factory,
         RepeatProps props = {}, layout::BoxProps box = {})
      : KeyedChildren{std::move(source), std::move(factory), box},
        _props{props} {
    _props.validate();
  }

  const RepeatProps &props() const noexcept { return _props; }

  void setProps(RepeatProps value);

  void setArrangement(layout::Axis axis, layout::StackProps stack = {}) {
    setProps({axis, stack});
  }

  void setArrangement(layout::GridProps props);
  void setArrangement(layout::FlowProps props);
  void applyPatch(const RepeatPatch &p);
  void applyChanges(const CollectionChangeSet &batch);
  void applyCollectionChanges();
};

} // namespace playground::ui
