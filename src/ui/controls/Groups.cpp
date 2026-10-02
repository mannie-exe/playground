#include <algorithm>

#include <ui/controls/Groups.hpp>

namespace playground::ui {
ToggleGroup::ToggleGroup(std::vector<ChoiceItem> items, ToggleGroupProps props,
                         bool checkboxes, layout::BoxProps box)
    : HStack{{}, box}, _checkboxes{checkboxes} {
  setControlLayout(ControlLayout::Group);
  std::set<std::string> keys;
  for (auto &item : items) {
    if (item.key.empty() || !keys.insert(item.key).second || !item.content)
      throw std::invalid_argument("Invalid toggle group item");
    auto button = std::make_unique<ToggleButton>(
        std::move(item.content), ToggleProps{.name = item.label}, ButtonProps{},
        layout::BoxProps{},
        checkboxes ? SemanticRole::Checkbox : SemanticRole::Button);
    _items.push_back({item.key, item.label, item.enabled});
    _buttons.push_back(button.get());
    _connections.push_back(button->onValueEdited(
        [this, key = item.key](CheckState, ActionSource source) {
          performAction(SelectItem{key}, source);
          setProps(_props);
        }));
    append(std::move(button));
  }
  setProps(std::move(props));
}

void ToggleGroup::setProps(ToggleGroupProps p) {
  p.multiple |= _checkboxes;
  if ((!p.multiple && p.selected.size() > 1) ||
      (p.required && p.selected.empty()))
    throw std::invalid_argument("Invalid group selection count");
  for (auto &key : p.selected)
    if (std::none_of(_items.begin(), _items.end(),
                     [&](auto &i) { return i.key == key; }))
      throw std::invalid_argument("Unknown group key");
  _props = std::move(p);
  for (std::size_t i = 0; i < _items.size(); ++i) {
    _buttons[i]->applyPatch(
        {.checked = Patch<CheckState>::set(
             _props.selected.contains(_items[i].key) ? CheckState::On
                                                     : CheckState::Off)});
    _buttons[i]->setEnabled(_props.enabled && _items[i].enabled);
  }
  setSemanticProps({.role = SemanticRole::Group,
                    .name = _props.name,
                    .enabled = _props.enabled});
}

CheckState ToggleGroup::aggregate() const {
  std::size_t enabled{}, selected{};
  for (auto &i : _items)
    if (i.enabled) {
      ++enabled;
      selected += _props.selected.contains(i.key);
    }
  return selected == 0         ? CheckState::Off
         : selected == enabled ? CheckState::On
                               : CheckState::Mixed;
}

ActionResult ToggleGroup::performAction(const UIAction &a,
                                        ActionSource source) {
  if (!_props.enabled)
    return ActionResult::Unavailable;
  auto next = _props;
  if (auto *selected = std::get_if<SelectItem>(&a)) {
    auto item = std::find_if(_items.begin(), _items.end(), [&](auto &i) {
      return i.key == selected->key && i.enabled;
    });
    if (item == _items.end())
      return ActionResult::Unavailable;
    if (next.selected.contains(item->key)) {
      if (next.required && next.selected.size() == 1)
        return ActionResult::Unchanged;
      next.selected.erase(item->key);
    } else {
      if (!next.multiple)
        next.selected.clear();
      next.selected.insert(item->key);
    }
  } else if (auto *checked = std::get_if<SetChecked>(&a);
             checked && next.multiple) {
    if (checked->value == CheckState::Mixed)
      return ActionResult::Unavailable;
    for (auto &i : _items)
      if (i.enabled) {
        if (checked->value == CheckState::On)
          next.selected.insert(i.key);
        else
          next.selected.erase(i.key);
      }
    if (next.required && next.selected.empty())
      return ActionResult::Unavailable;
  } else
    return ActionResult::Unsupported;
  if (next.selected == _props.selected)
    return ActionResult::Unchanged;
  setProps(std::move(next));
  _changed.emit(_props.selected, source);
  return ActionResult::Applied;
}

void ToggleGroup::onDefaultEvent(UIEvent &e) {
  if (e.handled)
    return;
  std::optional<std::string> key;
  for (std::size_t i = 0; i < _items.size(); ++i)
    if (_buttons[i]->hasFocus())
      key = _items[i].key;
  if (auto next = _navigation.navigate(
          _items, key, e, services() ? services()->scheduler->now() : 0,
          {layout::Axis::Horizontal, true, layoutDirection()})) {
    for (std::size_t i = 0; i < _items.size(); ++i)
      if (_items[i].key == next)
        _buttons[i]->requestFocus();
    e.handled = true;
  }
}

Toolbar::Toolbar(std::string name, layout::BoxProps box)
    : HStack{{.childrenAlignment = layout::CrossAlignment::Center}, box} {
  setControlLayout(ControlLayout::Group);
  setSemanticProps({.role = SemanticRole::Toolbar, .name = std::move(name)});
}

namespace {
Node *toolbarTarget(Node &node) {
  if (!node.isInteractionEnabled() ||
      node.visibility() != Visibility::Visible || node.isPortal())
    return nullptr;
  auto &target = node.focusTarget();
  if (target.isFocusable())
    for (auto *p = &target; p; p = p->parent()) {
      if (!p->isInteractionEnabled() || p->visibility() != Visibility::Visible)
        break;
      if (p == &node)
        return &target;
    }
  for (auto &child : node.children())
    if (auto *candidate = toolbarTarget(*child))
      return candidate;
  return nullptr;
}

bool containsFocus(const Node &node) {
  if (node.hasFocus())
    return true;
  for (auto &child : node.children())
    if (containsFocus(*child))
      return true;
  return false;
}
} // namespace

Node *Toolbar::activeEntry() {
  Node *first{}, *retained{};
  for (auto &child : children())
    if (auto *target = toolbarTarget(*child)) {
      if (!first)
        first = target;
      if (containsFocus(*child)) {
        _active = target->handle();
        return target;
      }
      if (_active.get() == target)
        retained = target;
    }
  auto *target = retained ? retained : first;
  _active = target ? target->handle() : NodeHandle<Node>{};
  return target;
}

void Toolbar::onDefaultEvent(UIEvent &e) {
  if (e.handled)
    return;
  std::vector<NavigationItem> items;
  items.reserve(children().size());
  std::optional<std::string> key;
  auto *active = activeEntry();
  for (std::size_t i = 0; i < children().size(); ++i) {
    auto *target = toolbarTarget(*children()[i]);
    auto id = std::to_string(i);
    items.push_back({id, target ? target->semanticState().description.name : "",
                     target != nullptr});
    if (target && target == active)
      key = id;
  }
  if (auto next = _navigation.navigate(
          items, key, e, services() ? services()->scheduler->now() : 0,
          {layout::Axis::Horizontal, true, layoutDirection()})) {
    auto *node = toolbarTarget(*children()[std::stoul(*next)]);
    if (node) {
      _active = node->handle();
      node->requestFocus();
    }
    e.handled = true;
  }
}

Accordion::Accordion(std::vector<AccordionItem> items, bool multiple,
                     layout::BoxProps box)
    : VStack{{.childrenAlignment = layout::CrossAlignment::Stretch}, box},
      _multiple{multiple} {
  setControlLayout(ControlLayout::Group);
  std::set<std::string> keys;
  for (auto &item : items) {
    if (item.key.empty() || !keys.insert(item.key).second)
      throw std::invalid_argument("Duplicate accordion key");
    auto disclosure = std::make_unique<Disclosure>(std::move(item.label),
                                                   std::move(item.content));
    _items.push_back({item.key, disclosure.get()});
    _connections.push_back(disclosure->onExpandedEdited(
        [this, key = item.key](bool open, ActionSource source) {
          auto state = expanded();
          if (open && !_multiple)
            state = {key};
          setExpanded(state);
          _edited.emit(state, source);
          _changed.emit(state);
        }));
    append(std::move(disclosure));
  }
}

std::set<std::string> Accordion::expanded() const {
  std::set<std::string> result;
  for (auto &[key, node] : _items)
    if (node->expansionProps().expanded)
      result.insert(key);
  return result;
}

void Accordion::setExpanded(std::set<std::string> keys) {
  if (!_multiple && keys.size() > 1)
    throw std::invalid_argument("Accordion allows one expanded item");
  for (auto &key : keys)
    if (std::none_of(_items.begin(), _items.end(),
                     [&](auto &i) { return i.first == key; }))
      throw std::invalid_argument("Unknown accordion key");
  for (auto &[key, node] : _items)
    node->applyExpansionPatch(
        {.expanded = Patch<bool>::set(keys.contains(key))});
}
} // namespace playground::ui
