#include "../detail/LayoutEngines.hpp"
#include <ui/collections/Repeat.hpp>

namespace playground::ui {

layout::MeasureResult
Repeat::measureContent(MeasureContext &context,
                       const layout::SizeConstraints &offered) {
  if (_props.layout == RepeatLayout::Grid)
    return container_detail::GridEngine{
        *this, _props.grid,
        [this](auto) -> const auto & { return _gridPlacement; }}
        .measure(context, offered);
  if (_props.layout == RepeatLayout::Flow)
    return container_detail::FlowEngine{
        *this, _props.flow, [this](auto) -> const auto & { return _placement; }}
        .measure(context, offered);
  const auto plan = container_detail::stackPlan(
      *this, container_detail::childIndices(*this),
      [&](auto) -> const auto & { return _placement; }, context, offered,
      _props.axis, _props.stack);
  return {plan.size, plan.firstBaseline, plan.lastBaseline};
}

void Repeat::arrangeChildren(ArrangeContext &context, math::Rect bounds) {
  if (_props.layout == RepeatLayout::Grid) {
    container_detail::GridEngine{
        *this, _props.grid,
        [this](auto) -> const auto & { return _gridPlacement; }}
        .arrange(context, bounds);
    return;
  }
  if (_props.layout == RepeatLayout::Flow) {
    container_detail::FlowEngine{
        *this, _props.flow, [this](auto) -> const auto & { return _placement; }}
        .arrange(context, bounds);
    return;
  }
  const auto plan = container_detail::stackPlan(
      *this, container_detail::childIndices(*this),
      [&](auto) -> const auto & { return _placement; }, context,
      layout::SizeConstraints::tight(bounds.size), _props.axis, _props.stack,
      true);
  container_detail::arrangeStackPlan(*this, context, plan, bounds, _props.axis);
}

void Repeat::setProps(RepeatProps value) {
  value.validate();
  if (value != _props) {
    _props = std::move(value);
    invalidateLayout();
  }
}

void Repeat::setArrangement(layout::GridProps props) {
  auto p = _props;
  p.layout = RepeatLayout::Grid;
  p.grid = std::move(props);
  setProps(std::move(p));
}

void Repeat::setArrangement(layout::FlowProps props) {
  auto p = _props;
  p.layout = RepeatLayout::Flow;
  p.flow = props;
  setProps(std::move(p));
}

void Repeat::applyPatch(const RepeatPatch &p) {
  const RepeatProps d;
  setProps({p.axis.appliedTo(_props.axis, d.axis),
            p.stack.appliedTo(_props.stack, d.stack),
            p.layout.appliedTo(_props.layout, d.layout),
            p.grid.appliedTo(_props.grid, d.grid),
            p.flow.appliedTo(_props.flow, d.flow)});
}

void Repeat::applyChanges(const CollectionChangeSet &batch) {
  validateChanges(batch);
  checkStructuralMutation();
  auto keys = detail::collectionKeys(*_source);
  reconcile(keys);
  replaceKeys(std::move(keys));
  invalidateLayout();
  finishChanges(batch);
}

void Repeat::applyCollectionChanges() {
  checkStructuralMutation();
  auto keys = detail::collectionKeys(*_source);
  reconcile(keys);
  replaceKeys(std::move(keys));
  _sourceRevision = _source->revision();
  invalidateLayout();
  for (const auto &key : _keys)
    refreshItem(key);
}

} // namespace playground::ui
