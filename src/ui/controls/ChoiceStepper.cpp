#include <ui/controls/ChoiceStepper.hpp>
#include <ui/controls/Meter.hpp>

namespace playground::ui {
ChoiceStepper::ChoiceStepper(std::unique_ptr<Node> display,
                             std::vector<ChoiceItem> items,
                             SelectionProps props, ChoiceCenter mode, bool wrap,
                             layout::BoxProps box,
                             std::optional<ControlMetrics> metrics)
    : HStack{{.childrenAlignment = layout::CrossAlignment::Center}, box},
      _wrap{wrap} {
  if (metrics)
    setThemeOverrides({.stepper = metrics});
  setControlLayout(ControlLayout::StepperRow);
  for (auto &item : items)
    _items.push_back({item.key, item.label, item.enabled});
  auto previous = std::make_unique<Button>(
      std::make_unique<ControlIcon>(ControlGlyph::Previous), ButtonProps{},
      layout::BoxProps{});
  previous->setControlLayout(ControlLayout::StepperButton);
  _previous = previous.get();
  _connections.push_back(
      previous->onInvoke([this](ActionSource source) { stepBy(-1, source); }));
  append(std::move(previous), {.shrink = 0});
  if (mode == ChoiceCenter::Dropdown) {
    auto select =
        std::make_unique<Select>(std::move(display), std::move(items), props);
    _select = select.get();
    _center = _select;
    _connections.push_back(
        select->onSelectionEdited([this](std::string key, ActionSource source) {
          refresh();
          _edited.emit(key, source);
          _changed.emit(key);
        }));
    append(std::move(select), {.grow = 1});
  } else {
    auto center = std::make_unique<Box>(
        layout::BoxProps{},
        BoxContentProps{{layout::Align::Stretch, layout::Align::Center}});
    center->setControlLayout(ControlLayout::StepperCenter);
    center->setChild(std::move(display));
    _center = center.get();
    _center->setInputProps({.focusable = true});
    append(std::move(center), {.grow = 1});
    auto list = std::make_unique<ListBox>(std::move(items), props);
    _list = list.get();
    list->setVisibility(Visibility::Collapsed);
    _connections.push_back(
        list->onSelectionEdited([this](std::string key, ActionSource source) {
          refresh();
          _edited.emit(key, source);
          _changed.emit(key);
        }));
    append(std::move(list));
  }
  auto next = std::make_unique<Button>(
      std::make_unique<ControlIcon>(ControlGlyph::Next), ButtonProps{},
      layout::BoxProps{});
  next->setControlLayout(ControlLayout::StepperButton);
  _next = next.get();
  _connections.push_back(
      next->onInvoke([this](ActionSource source) { stepBy(1, source); }));
  append(std::move(next), {.shrink = 0});
  refresh();
}

const SelectionProps &ChoiceStepper::selectionProps() const {
  return _select ? _select->selectionProps() : _list->selectionProps();
}

void ChoiceStepper::setSelectionProps(SelectionProps p) {
  if (_select)
    _select->setSelectionProps(std::move(p));
  else
    _list->setSelectionProps(std::move(p));
  refresh();
}

void ChoiceStepper::refresh() {
  auto &p = selectionProps();
  _previous->setEnabled(p.enabled &&
                        bool(_navigation.next(_items, p.selected, -1, _wrap)));
  _next->setEnabled(p.enabled &&
                    bool(_navigation.next(_items, p.selected, 1, _wrap)));
  _previous->setSemanticProps({.name = "Previous " + p.name});
  _next->setSemanticProps({.name = "Next " + p.name});
  if (_list) {
    auto s = _center->semanticProps();
    s.role = SemanticRole::Group;
    s.name = p.name;
    s.value = _list->semanticState().description.value;
    _center->setSemanticProps(std::move(s));
  }
}

void ChoiceStepper::stepBy(int direction, ActionSource source) {
  if (auto key =
          _navigation.next(_items, selectionProps().selected, direction, _wrap))
    performAction(SelectItem{*key}, source);
}

ActionResult ChoiceStepper::performAction(const UIAction &a,
                                          ActionSource source) {
  if (!selectionProps().enabled)
    return ActionResult::Unavailable;
  return _select ? _select->performAction(a, source)
                 : _list->performAction(a, source);
}

void ChoiceStepper::onDefaultEvent(UIEvent &e) {
  if (!e.handled && e.type == EventType::KeyDown &&
      (e.logicalKey == Key::Left || e.logicalKey == Key::Right)) {
    stepBy((e.logicalKey == Key::Right ? 1 : -1) *
               (layoutDirection() == layout::LayoutDirection::RightToLeft ? -1
                                                                          : 1),
           e.source);
    e.handled = true;
  }
}
} // namespace playground::ui
