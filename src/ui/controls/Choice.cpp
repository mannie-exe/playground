#include <algorithm>
#include <set>
#include <stdexcept>

#include <ui/collections/ScrollView.hpp>
#include <ui/controls/Choice.hpp>
#include <ui/controls/ControlPaint.hpp>

namespace playground::ui {
ToggleButton::ToggleButton(std::unique_ptr<Node> content, ToggleProps props,
                           ButtonProps button, layout::BoxProps box,
                           SemanticRole role)
    : Button{std::move(content), button, box}, _role{role} {
  auto insets = boxProps();
  if (insets.padding == math::Insets{})
    insets.padding = math::Insets::all(10);
  if (role == SemanticRole::Checkbox || role == SemanticRole::Switch)
    insets.padding.left += role == SemanticRole::Switch ? 48.f : 30.f;
  setBoxProps(insets);
  setContentAlignment({layout::Align::Start, layout::Align::Center});
  setProps(std::move(props));
}

void ToggleButton::setProps(ToggleProps props) {
  if (props.checked != CheckState::Off && props.checked != CheckState::On &&
      props.checked != CheckState::Mixed)
    throw std::invalid_argument("Invalid toggle state");
  if (props.checked == CheckState::Mixed &&
      (!props.allowMixed || _role == SemanticRole::Switch))
    throw std::invalid_argument("This toggle does not allow mixed state");
  _props = std::move(props);
  invalidate(DirtyFlags::Paint | DirtyFlags::Semantics);
}

void ToggleButton::applyPatch(const TogglePatch &p) {
  setProps({p.checked.appliedTo(_props.checked, CheckState::Off),
            p.allowMixed.appliedTo(_props.allowMixed, false),
            p.name.appliedTo(_props.name, {})});
}

SemanticState ToggleButton::semanticState() const {
  auto state = Button::semanticState();
  state.description.role = _role;
  if (!_props.name.empty())
    state.description.name = _props.name;
  state.checked = _props.checked;
  if (isEnabled())
    state.actions.push_back(SemanticAction::SetChecked);
  return state;
}

ActionResult ToggleButton::performAction(const UIAction &action,
                                         ActionSource source) {
  if (!isEnabled())
    return ActionResult::Unavailable;
  auto next = _props;
  if (std::holds_alternative<Activate>(action))
    next.checked =
        _props.checked == CheckState::On ? CheckState::Off : CheckState::On;
  else if (const auto *value = std::get_if<SetChecked>(&action))
    next.checked = value->value;
  else
    return Button::performAction(action, source);
  if (next.checked == CheckState::Mixed &&
      (!next.allowMixed || _role == SemanticRole::Switch))
    return ActionResult::Unavailable;
  if (next == _props)
    return ActionResult::Unchanged;
  setProps(std::move(next));
  _edited.emit(_props.checked, source);
  _changed.emit(_props.checked);
  return ActionResult::Applied;
}

void ToggleButton::paint(PaintContext &context) const {
  const auto &t = theme();
  const auto ink = isEnabled() ? t.accent : t.mutedText;
  if (_role == SemanticRole::Button) {
    Button::paint(context);
    if (_props.checked != CheckState::Off)
      control_paint::outline(
          context, math::inset({{}, bounds().size}, math::Insets::all(3)), ink,
          3);
    return;
  }
  if (isEnabled() && (isHovered() || isPressed()))
    context.fill({{}, bounds().size}, isPressed() ? t.pressed : t.hover);
  const float indicatorY = std::max(0.f, (bounds().h() - 22) / 2);
  if (_role == SemanticRole::Switch) {
    const bool on = _props.checked == CheckState::On;
    control_paint::circle(context, math::rect(10, indicatorY, 22, 22),
                          on ? ink : t.border);
    context.fill(math::rect(21, indicatorY, 16, 22), on ? ink : t.border);
    control_paint::circle(context, math::rect(26, indicatorY, 22, 22),
                          on ? ink : t.border);
    control_paint::circle(context,
                          math::rect(on ? 29.f : 13.f, indicatorY + 3, 16, 16),
                          t.elevated);
  } else {
    const float y = indicatorY + 1;
    control_paint::outline(context, math::rect(10, y, 20, 20), ink, 2);
    if (_props.checked == CheckState::Mixed)
      context.fill(math::rect(14, y + 8, 12, 4), ink);
    if (_props.checked == CheckState::On) {
      math::Path2D path;
      path.moveTo({14, y + 10}).lineTo({18, y + 14}).lineTo({27, y + 5});
      context.drawPath(path, {.fill = {}, .stroke = ink, .strokeWidth = 3});
    }
  }
}

class ListBox::Option final : public Button {
public:
  bool selected{};
  bool active{};
  SemanticRole role{SemanticRole::Option};
  using Button::Button;

  SemanticState semanticState() const override {
    auto state = Button::semanticState();
    state.description.role = role;
    state.selected = selected;
    if (role == SemanticRole::Radio)
      state.checked = selected ? CheckState::On : CheckState::Off;
    return state;
  }

protected:
  void paintSubtree(PaintContext &context) const override {
    // The owning list manages focus; options paint their own state marker,
    // not an additional Button focus border around the same content.
    Box::paintSubtree(context);
  }

  void paint(PaintContext &context) const override {
    const auto &p = buttonProps();
    const auto &t = theme();
    // Options share their parent's surface, not each button's resting chrome.
    if (isEnabled() && (isPressed() || isHovered() || active || selected))
      context.fill({{}, bounds().size},
                   p.useTheme ? (isPressed()             ? t.pressed
                                 : isHovered() || active ? t.hover
                                                         : t.selection)
                              : (isPressed() ? p.pressed : p.hover));
    const auto ink =
        p.useTheme ? (isEnabled() ? t.accent : t.mutedText) : p.focus;
    const auto marker =
        active && isEnabled() ? (p.useTheme ? t.focus : p.focus) : ink;
    if (role == SemanticRole::Radio) {
      const float y = std::max(0.f, (bounds().h() - 20) / 2);
      control_paint::circle(context, math::rect(10, y, 20, 20), marker);
      control_paint::circle(context, math::rect(12, y + 2, 16, 16), t.elevated);
      if (selected)
        control_paint::circle(context, math::rect(15, y + 5, 10, 10), ink);
    } else if (selected || active)
      context.fill(math::rect(2, 2,
                              std::min(3.f, std::max(0.f, bounds().w() - 2)),
                              std::max(0.f, bounds().h() - 4)),
                   marker);
  }
};

ListBox::ListBox(std::vector<ChoiceItem> items, SelectionProps props,
                 ButtonProps button, layout::BoxProps box, SemanticRole role)
    : VStack{{.childrenAlignment = layout::CrossAlignment::Stretch}, box},
      _role{role} {
  std::set<std::string> keys;
  for (const auto &item : items)
    if (item.key.empty() || !keys.insert(item.key).second || !item.content)
      throw std::invalid_argument(
          "Choice items require unique keys and content");
  for (auto &item : items) {
    auto node = std::make_unique<Option>(std::move(item.content), button);
    node->role = role == SemanticRole::RadioGroup ? SemanticRole::Radio
                 : role == SemanticRole::Menu     ? SemanticRole::MenuItem
                                                  : SemanticRole::Option;
    node->setSemanticProps({.name = item.label});
    auto box = node->boxProps();
    box.padding = math::Insets::all(10);
    if (role == SemanticRole::RadioGroup)
      box.padding.left = 40;
    node->setBoxProps(box);
    node->setContentAlignment({layout::Align::Start, layout::Align::Center});
    _connections.push_back(
        node->onInvoke([this, key = item.key](ActionSource source) {
          performAction(SelectItem{key}, source);
        }));
    _items.push_back({item.key, item.label, node.get(), item.enabled});
    append(std::move(node));
  }
  setInputProps({.hitTest = HitTestPolicy::SelfAndChildren, .focusable = true});
  setSelectionProps(std::move(props));
}

void ListBox::setSelectionProps(SelectionProps props) {
  if (props.required && !props.selected && !_items.empty())
    throw std::invalid_argument("Selection is required");
  if (props.selected &&
      std::none_of(_items.begin(), _items.end(),
                   [&](auto &item) { return item.key == *props.selected; }))
    throw std::invalid_argument("Unknown choice key");
  if (_role == SemanticRole::Menu) {
    if (props.selected)
      _active = props.selected;
    if (std::none_of(_items.begin(), _items.end(), [&](const auto &item) {
          return item.enabled && _active == item.key;
        })) {
      _active.reset();
      for (const auto &item : _items)
        if (item.enabled) {
          _active = item.key;
          break;
        }
    }
    props.selected.reset();
    props.required = false;
  }
  _props = std::move(props);
  for (auto &item : _items) {
    item.button->selected = _props.selected == item.key;
    item.button->setEnabled(_props.enabled && item.enabled);
    item.button->setFocusable(false);
    item.button->invalidate(DirtyFlags::Paint | DirtyFlags::Semantics);
  }
  if (_role == SemanticRole::Menu)
    highlight(_active);
  invalidate(DirtyFlags::Paint | DirtyFlags::Semantics);
}

void ListBox::applySelectionPatch(const SelectionPatch &p) {
  setSelectionProps({p.selected.appliedTo(_props.selected, {}),
                     p.enabled.appliedTo(_props.enabled, true),
                     p.required.appliedTo(_props.required, false),
                     p.name.appliedTo(_props.name, {})});
}

SemanticState ListBox::semanticState() const {
  auto state = Node::semanticState();
  state.description.role = _role;
  state.description.name = _props.name;
  state.description.enabled = _props.enabled;
  state.required = _props.required;
  if (_props.selected) {
    const auto item =
        std::find_if(_items.begin(), _items.end(),
                     [&](const auto &i) { return i.key == *_props.selected; });
    if (item != _items.end())
      state.description.value = item->label;
  } else
    state.description.value.reset();
  for (const auto &item : _items)
    if (item.button->active && item.button->id() != NodeId{})
      state.activeDescendant = item.button->id();
  return state;
}

ActionResult ListBox::performAction(const UIAction &action,
                                    ActionSource source) {
  if (!_props.enabled)
    return ActionResult::Unavailable;
  const auto *selection = std::get_if<SelectItem>(&action);
  if (!selection)
    return ActionResult::Unsupported;
  const auto item = std::find_if(_items.begin(), _items.end(), [&](auto &i) {
    return i.key == selection->key;
  });
  if (item == _items.end() || !item->enabled)
    return ActionResult::Unavailable;
  if (_role == SemanticRole::Menu) {
    _invoked.emit(selection->key, source);
    return ActionResult::Applied;
  }
  if (_props.selected == selection->key)
    return ActionResult::Unchanged;
  auto props = _props;
  props.selected = selection->key;
  setSelectionProps(std::move(props));
  requestFocus();
  _selected.emit(selection->key, source);
  _changed.emit(selection->key);
  return ActionResult::Applied;
}

std::vector<NavigationItem> ListBox::items() const {
  std::vector<NavigationItem> result;
  result.reserve(_items.size());
  for (auto &item : _items)
    result.push_back({item.key, item.label, item.enabled});
  return result;
}

bool ListBox::handleComposition(UIEvent &event) {
  if (event.type == EventType::TextEditing) {
    _composing = !event.text.empty();
    event.handled = true;
    return true;
  }
  if (event.type == EventType::FocusLost ||
      event.type == EventType::InputCancel) {
    _composing = false;
    _navigation.reset();
  }
  if (event.type == EventType::TextInput)
    _composing = false;
  if (_composing && event.type == EventType::KeyDown) {
    if (event.logicalKey == Key::Escape)
      _composing = false;
    event.handled = true;
    return true;
  }
  return false;
}

void ListBox::onDefaultEvent(UIEvent &event) {
  if (handleComposition(event))
    return;
  if (event.handled || !_props.enabled)
    return;
  auto current = _role == SemanticRole::Menu ? _active : _props.selected;
  if (event.type == EventType::KeyDown &&
      (event.logicalKey == Key::Enter || event.logicalKey == Key::Space) &&
      current) {
    performAction(SelectItem{*current}, event.source);
    event.handled = true;
    return;
  }
  auto e = event;
  if (e.logicalKey == Key::Right)
    e.logicalKey = Key::Down;
  if (e.logicalKey == Key::Left)
    e.logicalKey = Key::Up;
  auto next = _navigation.navigate(
      items(), current, e,
      services() && services()->scheduler ? services()->scheduler->now() : 0);
  if (next) {
    if (_role == SemanticRole::Menu) {
      _active = next;
      highlight(next);
    } else
      performAction(SelectItem{*next}, event.source);
    event.handled = true;
  }
}

std::vector<std::string> ListBox::enabledKeys() const {
  std::vector<std::string> keys;
  for (const auto &item : _items)
    if (item.enabled)
      keys.push_back(item.key);
  return keys;
}

void ListBox::highlight(const std::optional<std::string> &key) {
  for (auto &item : _items) {
    item.button->active = key == item.key;
    item.button->invalidate(DirtyFlags::Paint | DirtyFlags::Semantics);
    if (item.button->active)
      for (auto *p = parent(); p && !p->isPortal(); p = p->parent())
        if (auto *scroll = dynamic_cast<ScrollView *>(p))
          scroll->scrollIntoView(*item.button);
  }
  invalidate(DirtyFlags::Semantics);
}
} // namespace playground::ui
