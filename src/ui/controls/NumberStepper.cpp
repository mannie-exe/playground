#include <ui/containers/Stack.hpp>
#include <ui/controls/NumberStepper.hpp>

namespace playground::ui {
void NumberStepperProps::validate() const {
  RangeValue{value, minimum, maximum, step}.validate();
}

NumberStepper::NumberStepper(std::unique_ptr<Node> center,
                             std::unique_ptr<Node> decrease,
                             std::unique_ptr<Node> increase,
                             NumberStepperProps props, ButtonProps buttons,
                             layout::BoxProps box,
                             std::optional<ControlMetrics> metrics)
    : Box{box, {layout::Alignment::stretch()}} {
  props.validate();
  if (metrics)
    setThemeOverrides({.stepper = metrics});
  if (!center || !decrease || !increase)
    throw std::invalid_argument(
        "NumberStepper requires center and button content");
  _center = center.get();
  _editor = dynamic_cast<NumericEditor *>(_center);
  auto row = std::make_unique<HStack>(
      layout::StackProps{.childrenAlignment = layout::CrossAlignment::Stretch});
  row->setControlLayout(ControlLayout::StepperRow);
  auto left = std::make_unique<Button>(std::move(decrease), buttons,
                                       layout::BoxProps{});
  auto right = std::make_unique<Button>(std::move(increase), buttons,
                                        layout::BoxProps{});
  left->setControlLayout(ControlLayout::StepperButton);
  right->setControlLayout(ControlLayout::StepperButton);
  _decrease = left.get();
  _increase = right.get();
  _decreaseConnection =
      left->onInvoke([this](ActionSource source) { stepBy(-1, source); });
  _increaseConnection =
      right->onInvoke([this](ActionSource source) { stepBy(1, source); });
  row->append(std::move(left), {.shrink = 0});
  auto slot = std::make_unique<Box>(
      layout::BoxProps{},
      BoxContentProps{{layout::Align::Stretch, layout::Align::Center}});
  slot->setChild(std::move(center));
  row->append(std::move(slot), {.grow = 1});
  row->append(std::move(right), {.shrink = 0});
  setChild(std::move(row));
  if (_editor) {
    _editorConnection = _editor->onNumberEdited(
        [this](double v, ChangeContext c) { publish(v, c); });
    _draftConnection = _editor->onDraftChanged([this] { refresh(); });
  } else
    setInputProps({.focusable = true});
  setProps(std::move(props));
}

void NumberStepper::refresh() {
  _props.value = value();
  const bool editable = _props.enabled && !_props.readOnly;
  const bool pending = _editor && _editor->draftDirty();
  _decrease->setEnabled(editable && (pending || value() > _props.minimum));
  _increase->setEnabled(editable && (pending || value() < _props.maximum));
  _decrease->setSemanticProps(
      {.role = SemanticRole::Button, .name = "Decrease " + _props.name});
  _increase->setSemanticProps(
      {.role = SemanticRole::Button, .name = "Increase " + _props.name});
  auto p = focusTarget().semanticProps();
  p.name = _props.name;
  p.value = formatNumber(value());
  focusTarget().setSemanticProps(std::move(p));
  invalidate(DirtyFlags::Semantics);
}

void NumberStepper::setProps(NumberStepperProps props) {
  props.validate();
  if (_editor)
    _editor->setNumericRange(
        {props.value, props.minimum, props.maximum, props.step});
  _props = std::move(props);
  if (_editor)
    _editor->setNumericInteraction(_props.enabled, _props.readOnly);
  refresh();
}

void NumberStepper::publish(double v, ChangeContext c) {
  _props.value = v;
  refresh();
  _edited.emit(v, c);
  _changed.emit(v);
}

void NumberStepper::stepBy(int direction, ActionSource source) {
  performAction(Increment{direction}, source);
}

ActionResult NumberStepper::performAction(const UIAction &a,
                                          ActionSource source) {
  if (!_props.enabled || _props.readOnly)
    return ActionResult::Unavailable;
  if (_editor) {
    if (auto *step = std::get_if<Increment>(&a))
      return _editor->adjustNumber(step->direction,
                                   {source, ChangeReason::Step});
    return _center->performAction(a, source);
  }
  double next = value();
  if (auto *step = std::get_if<Increment>(&a))
    next = RangeValue{value(), _props.minimum, _props.maximum, _props.step}
               .adjusted(step->direction);
  else if (auto *set = std::get_if<SetValue>(&a))
    next = set->value;
  else
    return ActionResult::Unsupported;
  if (validateNumber(
          next, {value(), _props.minimum, _props.maximum, _props.step}, false))
    return ActionResult::Unavailable;
  if (next == value())
    return ActionResult::Unchanged;
  publish(next, {source, ChangeReason::Step});
  return ActionResult::Applied;
}

SemanticState NumberStepper::semanticState() const {
  auto s = Node::semanticState();
  if (_editor) {
    s.description.exposure = SemanticExposure::ChildrenOnly;
    return s;
  }
  s.description.role = SemanticRole::SpinButton;
  s.description.enabled = _props.enabled;
  s.readOnly = _props.readOnly;
  s.range = RangeValue{value(), _props.minimum, _props.maximum, _props.step};
  if (_props.enabled && !_props.readOnly)
    s.actions = {SemanticAction::Increment, SemanticAction::Decrement,
                 SemanticAction::SetValue, SemanticAction::Focus};
  return s;
}

void NumberStepper::onDefaultEvent(UIEvent &e) {
  if (e.handled || e.type != EventType::KeyDown)
    return;
  if (e.logicalKey == Key::Up || e.logicalKey == Key::Down ||
      (!_editor && (e.logicalKey == Key::Left || e.logicalKey == Key::Right))) {
    stepBy(e.logicalKey == Key::Up || e.logicalKey == Key::Right ? 1 : -1,
           e.source);
    e.handled = true;
  }
}

void NumberStepper::applyPatch(const NumberStepperPatch &p) {
  const NumberStepperProps d;
  setProps({p.value.appliedTo(value(), d.value),
            p.minimum.appliedTo(_props.minimum, d.minimum),
            p.maximum.appliedTo(_props.maximum, d.maximum),
            p.step.appliedTo(_props.step, d.step),
            p.enabled.appliedTo(_props.enabled, d.enabled),
            p.name.appliedTo(_props.name, d.name),
            p.readOnly.appliedTo(_props.readOnly, d.readOnly)});
}
} // namespace playground::ui
