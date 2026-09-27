#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <utility>

#include <ui/containers/Stack.hpp>
#include <ui/controls/Stepper.hpp>

namespace playground::ui {
SemanticState Stepper::semanticState() const {
  auto state = Node::semanticState();
  state.description.role = SemanticRole::SpinButton;
  state.description.enabled = _props.enabled;
  state.range = RangeValue{double(_props.value), double(_props.minimum),
                           double(_props.maximum), double(_props.step)};
  if (_props.enabled)
    state.actions = {SemanticAction::Increment, SemanticAction::Decrement,
                     SemanticAction::SetValue};
  return state;
}

ActionResult Stepper::performAction(const UIAction &action, ActionSource) {
  if (!_props.enabled)
    return ActionResult::Unavailable;
  const auto old = _props.value;
  if (auto *step = std::get_if<Increment>(&action))
    stepBy(step->direction);
  else if (auto *value = std::get_if<SetValue>(&action)) {
    if (!std::isfinite(value->value) || value->value < _props.minimum ||
        value->value > _props.maximum ||
        std::trunc(value->value) != value->value)
      return ActionResult::Unavailable;
    auto props = _props;
    props.value = int(value->value);
    setProps(std::move(props));
    if (old != _props.value)
      _changed.emit(_props.value);
  } else
    return ActionResult::Unsupported;
  return old == _props.value ? ActionResult::Unchanged : ActionResult::Applied;
}

void StepperProps::validate() const {
  if (minimum > maximum || value < minimum || value > maximum || step <= 0)
    throw std::invalid_argument("Invalid stepper range/value/step");
}

Stepper::Stepper(std::unique_ptr<Node> readout, std::unique_ptr<Node> decrease,
                 std::unique_ptr<Node> increase, StepperProps props,
                 ButtonProps buttons, layout::BoxProps box)
    : Box{box, {layout::Alignment::stretch()}} {
  props.validate();
  if (!readout || !decrease || !increase)
    throw std::invalid_argument("Stepper requires readout and button content");
  auto row = std::make_unique<HStack>(layout::StackProps{
      .gap = 8, .childrenAlignment = layout::CrossAlignment::Stretch});
  auto left = std::make_unique<Button>(
      std::move(decrease), buttons,
      layout::BoxProps{.width = layout::SizeRule::fixed(40), .minHeight = 40});
  auto right = std::make_unique<Button>(
      std::move(increase), buttons,
      layout::BoxProps{.width = layout::SizeRule::fixed(40), .minHeight = 40});
  _decrease = left.get();
  _increase = right.get();
  _decreaseConnection = left->onActivate([this] { stepBy(-1); });
  _increaseConnection = right->onActivate([this] { stepBy(1); });
  row->append(std::move(left), {.shrink = 0});
  row->append(std::move(readout), {.grow = 1});
  row->append(std::move(right), {.shrink = 0});
  setChild(std::move(row));
  setProps(std::move(props));
}

void Stepper::setProps(StepperProps value) {
  value.validate();
  auto decrease = _decrease->semanticProps();
  auto increase = _increase->semanticProps();
  decrease.name = "Decrease " + value.name;
  increase.name = "Increase " + value.name;
  decrease.value = increase.value = std::to_string(value.value);
  setSemanticProps({.role = SemanticRole::Group,
                    .name = value.name,
                    .value = std::to_string(value.value),
                    .enabled = value.enabled});
  _decrease->setSemanticProps(std::move(decrease));
  _increase->setSemanticProps(std::move(increase));
  _decrease->setEnabled(value.enabled && value.value > value.minimum);
  _increase->setEnabled(value.enabled && value.value < value.maximum);
  _props = std::move(value);
}

void Stepper::stepBy(int direction) {
  if (!_props.enabled || direction == 0)
    return;
  const auto next = std::clamp<std::int64_t>(
      std::int64_t{_props.value} + (direction > 0 ? std::int64_t{_props.step}
                                                  : -std::int64_t{_props.step}),
      _props.minimum, _props.maximum);
  if (next == _props.value)
    return;
  const bool focused = _decrease->hasFocus() || _increase->hasFocus();
  auto props = _props;
  props.value = static_cast<int>(next);
  setProps(std::move(props));
  // Reaching a bound disables the triggering button; keep keyboard focus here.
  if (focused && !_decrease->hasFocus() && !_increase->hasFocus())
    (_decrease->isEnabled() ? _decrease : _increase)->requestFocus();
  _changed.emit(_props.value);
}

void Stepper::onDefaultEvent(UIEvent &event) {
  if (event.handled || event.type != EventType::KeyDown || !_props.enabled)
    return;
  switch (event.logicalKey) {
  case Key::Left:
  case Key::Down:
    event.handled = true;
    stepBy(-1);
    break;
  case Key::Right:
  case Key::Up:
    event.handled = true;
    stepBy(1);
    break;
  default:
    break;
  }
}

void Stepper::applyPatch(const StepperPatch &patch) {
  const StepperProps defaults;
  setProps({patch.value.appliedTo(_props.value, defaults.value),
            patch.minimum.appliedTo(_props.minimum, defaults.minimum),
            patch.maximum.appliedTo(_props.maximum, defaults.maximum),
            patch.step.appliedTo(_props.step, defaults.step),
            patch.enabled.appliedTo(_props.enabled, defaults.enabled),
            patch.name.appliedTo(_props.name, defaults.name)});
}

} // namespace playground::ui
