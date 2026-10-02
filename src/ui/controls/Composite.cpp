#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>

#include <ui/collections/ScrollView.hpp>
#include <ui/controls/Composite.hpp>
#include <ui/controls/ControlPaint.hpp>

namespace playground::ui {
class Disclosure::Header final : public Button {
  Disclosure &_owner;

public:
  Header(Disclosure &owner, std::unique_ptr<Node> label, ButtonProps props)
      : Button{std::move(label), props}, _owner{owner} {
    setControlLayout(ControlLayout::Trigger);
  }

protected:
  void paint(PaintContext &p) const override {
    Button::paint(p);
    control_paint::chevron(
        p,
        {std::max(0.f, bounds().w() - themeMetrics().padding -
                           themeMetrics().chevronWidth),
         std::max(0.f, (bounds().h() - themeMetrics().chevronHeight) / 2)},
        _owner.expansionProps().expanded,
        isEnabled() ? theme().text : theme().mutedText, themeMetrics());
  }

public:
  SemanticState semanticState() const override {
    auto s = _owner.semanticState();
    s.description.exposure = SemanticExposure::Auto;
    s.actions.push_back(SemanticAction::Focus);
    return s;
  }

  ActionResult performAction(const UIAction &a, ActionSource source) override {
    return _owner.performAction(a, source);
  }
};

Disclosure::Disclosure(std::unique_ptr<Node> label, std::unique_ptr<Node> body,
                       ExpansionProps props, ButtonProps button,
                       layout::BoxProps box)
    : VStack{{.childrenAlignment = layout::CrossAlignment::Stretch}, box} {
  setControlLayout(ControlLayout::Group);
  if (!label || !body)
    throw std::invalid_argument("Disclosure requires label and body");
  auto header = std::make_unique<Header>(*this, std::move(label), button);
  _header = header.get();
  _body = body.get();
  append(std::move(header));
  append(std::move(body));
  setExpansionProps(std::move(props));
}

void Disclosure::setExpansionProps(ExpansionProps props) {
  _props = std::move(props);
  _header->setEnabled(_props.enabled);
  _body->setVisibility(_props.expanded ? Visibility::Visible
                                       : Visibility::Collapsed);
  invalidate(DirtyFlags::Semantics);
}

void Disclosure::applyExpansionPatch(const ExpansionPatch &p) {
  setExpansionProps({p.expanded.appliedTo(_props.expanded, false),
                     p.enabled.appliedTo(_props.enabled, true),
                     p.name.appliedTo(_props.name, {})});
}

SemanticState Disclosure::semanticState() const {
  auto s = Node::semanticState();
  s.description.role = SemanticRole::Disclosure;
  s.description.exposure = SemanticExposure::ChildrenOnly;
  s.description.name = _props.name;
  s.description.enabled = _props.enabled;
  s.expanded = _props.expanded;
  s.actions = {SemanticAction::Activate, _props.expanded
                                             ? SemanticAction::Collapse
                                             : SemanticAction::Expand};
  return s;
}

ActionResult Disclosure::performAction(const UIAction &a, ActionSource source) {
  if (!_props.enabled)
    return ActionResult::Unavailable;
  auto next = _props;
  if (std::holds_alternative<Activate>(a))
    next.expanded = !next.expanded;
  else if (auto *p = std::get_if<SetExpanded>(&a))
    next.expanded = p->value;
  else
    return ActionResult::Unsupported;
  if (next == _props)
    return ActionResult::Unchanged;
  setExpansionProps(std::move(next));
  _edited.emit(_props.expanded, source);
  _changed.emit(_props.expanded);
  return ActionResult::Applied;
}

class Select::Trigger final : public Button {
  Select &_owner;

public:
  Trigger(Select &owner, std::unique_ptr<Node> label, ButtonProps p)
      : Button{std::move(label), p}, _owner{owner} {
    setControlLayout(ControlLayout::Trigger);
  }

protected:
  void paint(PaintContext &p) const override {
    Button::paint(p);
    control_paint::chevron(
        p,
        {std::max(0.f, bounds().w() - themeMetrics().padding -
                           themeMetrics().chevronWidth),
         std::max(0.f, (bounds().h() - themeMetrics().chevronHeight) / 2)},
        _owner.isExpanded(), isEnabled() ? theme().text : theme().mutedText,
        themeMetrics());
  }

public:
  SemanticState semanticState() const override {
    auto s = _owner.semanticState();
    const auto &authored = semanticProps();
    s.description.labelledBy = authored.labelledBy;
    s.description.describedBy = authored.describedBy;
    s.description.description = authored.description;
    s.description.exposure = SemanticExposure::Auto;
    s.actions.push_back(SemanticAction::Focus);
    return s;
  }

  ActionResult performAction(const UIAction &a, ActionSource source) override {
    return _owner.performAction(a, source);
  }
};

class Select::ChoiceList final : public ListBox {
  Select &_owner;
  std::optional<std::string> _active;

public:
  ChoiceList(Select &owner, std::vector<ChoiceItem> items, SelectionProps props,
             ButtonProps button)
      : ListBox{std::move(items), std::move(props), button}, _owner{owner} {}

  void resetActive() {
    _active = selectionProps().selected;
    const auto keys = enabledKeys();
    if (!_active || std::find(keys.begin(), keys.end(), *_active) == keys.end())
      _active = keys.empty() ? std::nullopt : std::optional{keys.front()};
    highlight(_active);
  }

protected:
  void onDefaultEvent(UIEvent &e) override {
    if (handleComposition(e))
      return;
    if (e.type == EventType::FocusGained)
      highlight(_active);
    if (e.handled)
      return;
    if (e.type == EventType::KeyDown &&
        (e.logicalKey == Key::Enter || e.logicalKey == Key::Space)) {
      if (_active)
        performAction(SelectItem{*_active}, e.source);
      e.handled = true;
      return;
    }
    auto next = _navigation.navigate(
        items(), _active, e, services() ? services()->scheduler->now() : 0);
    if (next) {
      _active = next;
      highlight(_active);
      e.handled = true;
    }
  }

public:
  ActionResult performAction(const UIAction &action,
                             ActionSource source) override {
    const auto result = ListBox::performAction(action, source);
    // Confirmation can close the picker even when its selected value is
    // unchanged.
    if (result == ActionResult::Unchanged &&
        std::holds_alternative<SelectItem>(action) && _owner.isExpanded()) {
      _owner.setExpanded(false);
      _owner._trigger->requestFocus();
      return ActionResult::Applied;
    }
    return result;
  }
};

Select::Select(std::unique_ptr<Node> label, std::vector<ChoiceItem> items,
               SelectionProps props, ButtonProps button, layout::BoxProps box)
    : VStack{{.childrenAlignment = layout::CrossAlignment::Stretch}, box} {
  if (!label)
    throw std::invalid_argument("Select requires a label");
  auto trigger = std::make_unique<Trigger>(*this, std::move(label), button);
  _trigger = trigger.get();
  append(std::move(trigger));
  auto list =
      std::make_unique<ChoiceList>(*this, std::move(items), props, button);
  _list = list.get();
  auto popup = std::make_unique<Popup>(std::make_unique<ScrollView>(
      std::move(list), ScrollProps{.sizing = ScrollSizing::Content}));
  _popup = popup.get();
  _connections.push_back(popup->onDismissed([this](DismissReason reason) {
    _expanded = false;
    if (reason != DismissReason::OwnerUnavailable)
      _trigger->requestFocus();
    invalidate(DirtyFlags::Semantics);
  }));
  append(std::move(popup));
  _connections.push_back(
      _list->onSelectionEdited([this](std::string key, ActionSource source) {
        setExpanded(false);
        _trigger->requestFocus();
        _edited.emit(key, source);
        _changed.emit(std::move(key));
      }));
  setSelectionProps(std::move(props));
  setExpanded(false);
}

void Select::setSelectionProps(SelectionProps p) {
  _list->setSelectionProps(std::move(p));
  _trigger->setEnabled(selectionProps().enabled);
  auto semantics = _trigger->semanticProps();
  semantics.role = SemanticRole::Select;
  semantics.name = selectionProps().name;
  _trigger->setSemanticProps(std::move(semantics));
  if (!selectionProps().enabled)
    setExpanded(false);
  invalidate(DirtyFlags::Semantics);
}

void Select::applySelectionPatch(const SelectionPatch &p) {
  _list->applySelectionPatch(p);
  _trigger->setEnabled(selectionProps().enabled);
  auto semantics = _trigger->semanticProps();
  semantics.role = SemanticRole::Select;
  semantics.name = selectionProps().name;
  _trigger->setSemanticProps(std::move(semantics));
  if (!selectionProps().enabled)
    setExpanded(false);
  invalidate(DirtyFlags::Semantics);
}

void Select::setExpanded(bool value) {
  _expanded = value && selectionProps().enabled;
  _popup->setOpen(_expanded);
  if (_expanded) {
    static_cast<ChoiceList *>(_list)->resetActive();
    _trigger->requestFocus();
  }
  invalidate(DirtyFlags::Semantics);
}

SemanticState Select::semanticState() const {
  auto s = _list->semanticState();
  s.description.role = SemanticRole::Select;
  s.activeDescendant.reset();
  s.description.exposure = SemanticExposure::ChildrenOnly;
  s.expanded = _expanded;
  s.actions = {SemanticAction::Activate,
               _expanded ? SemanticAction::Collapse : SemanticAction::Expand};
  return s;
}

ActionResult Select::performAction(const UIAction &a, ActionSource source) {
  if (!selectionProps().enabled)
    return ActionResult::Unavailable;
  if (std::holds_alternative<Activate>(a)) {
    setExpanded(!_expanded);
    return ActionResult::Applied;
  }
  if (auto *p = std::get_if<SetExpanded>(&a)) {
    if (p->value == _expanded)
      return ActionResult::Unchanged;
    setExpanded(p->value);
    return ActionResult::Applied;
  }
  return _list->performAction(a, source);
}

void Select::onDefaultEvent(UIEvent &e) {
  if (!e.handled && e.type == EventType::KeyDown &&
      e.logicalKey == Key::Escape && _expanded) {
    setExpanded(false);
    _trigger->requestFocus();
    e.handled = true;
  } else if (!e.handled && e.type == EventType::KeyDown &&
             e.logicalKey == Key::Down) {
    setExpanded(true);
    e.handled = true;
  }
}

void Select::arrangeChildren(ArrangeContext &c, math::Rect b) {
  VStack::arrangeChildren(c, b);
  _popup->setAnchor(_trigger->id());
}

class Tabs::Tab final : public Button {
public:
  bool selected{};
  using Button::Button;

  SemanticState semanticState() const override {
    auto s = Button::semanticState();
    s.description.role = SemanticRole::Tab;
    s.selected = selected;
    return s;
  }

protected:
  void paint(PaintContext &c) const override {
    Button::paint(c);
    if (selected)
      c.fill(math::rect(
                 0, std::max(0.f, bounds().h() - themeMetrics().emphasisWidth),
                 bounds().w(), themeMetrics().emphasisWidth),
             theme().accent);
  }
};

Tabs::Tabs(std::vector<TabItem> items, SelectionProps props, ButtonProps button,
           layout::BoxProps box)
    : VStack{{}, box} {
  setControlLayout(ControlLayout::Group);
  std::set<std::string> keys;
  for (const auto &i : items)
    if (i.key.empty() || !keys.insert(i.key).second || !i.label || !i.panel)
      throw std::invalid_argument(
          "Tabs require unique keys, labels and panels");
  auto row = std::make_unique<HStack>();
  row->setSemanticProps({.role = SemanticRole::Tabs, .name = props.name});
  auto *bar = row.get();
  append(std::move(row), {.shrink = 0});
  for (auto &item : items) {
    auto tab = std::make_unique<Tab>(std::move(item.label), button);
    tab->setControlLayout(ControlLayout::Choice);
    tab->setSemanticProps({.name = item.name});
    auto *raw = tab.get();
    bar->append(std::move(tab));
    auto *panel = item.panel.get();
    append(std::move(item.panel));
    _items.push_back({item.key, raw, panel, item.enabled});
    _connections.push_back(
        raw->onInvoke([this, key = item.key](ActionSource source) {
          performAction(SelectItem{key}, source);
        }));
  }
  if (!props.selected)
    for (auto &i : _items)
      if (i.enabled) {
        props.selected = i.key;
        break;
      }
  setSelectionProps(std::move(props));
}

void Tabs::setSelectionProps(SelectionProps p) {
  if (!_items.empty() &&
      (!p.selected ||
       std::none_of(_items.begin(), _items.end(), [&](const auto &i) {
         return i.enabled && i.key == *p.selected;
       })))
    throw std::invalid_argument("Tabs require an enabled selected key");
  _props = std::move(p);
  for (auto &i : _items) {
    const bool selected = _props.selected == i.key;
    static_cast<Tab *>(i.tab)->selected = selected;
    i.tab->setEnabled(_props.enabled && i.enabled);
    i.tab->setFocusable(selected);
    i.panel->setVisibility(selected ? Visibility::Visible
                                    : Visibility::Collapsed);
    i.tab->invalidate(DirtyFlags::Paint | DirtyFlags::Semantics);
  }
  auto semantics = children()[0]->semanticProps();
  semantics.name = _props.name;
  children()[0]->setSemanticProps(std::move(semantics));
}

void Tabs::applySelectionPatch(const SelectionPatch &p) {
  setSelectionProps({p.selected.appliedTo(_props.selected, {}),
                     p.enabled.appliedTo(_props.enabled, true),
                     p.required.appliedTo(_props.required, false),
                     p.name.appliedTo(_props.name, {})});
}

ActionResult Tabs::performAction(const UIAction &a, ActionSource source) {
  if (!_props.enabled)
    return ActionResult::Unavailable;
  const auto *p = std::get_if<SelectItem>(&a);
  if (!p)
    return ActionResult::Unsupported;
  auto found = std::find_if(_items.begin(), _items.end(), [&](const auto &i) {
    return i.enabled && i.key == p->key;
  });
  if (found == _items.end())
    return ActionResult::Unavailable;
  if (_props.selected == p->key)
    return ActionResult::Unchanged;
  auto next = _props;
  next.selected = p->key;
  setSelectionProps(std::move(next));
  found->tab->requestFocus();
  _edited.emit(p->key, source);
  _changed.emit(p->key);
  return ActionResult::Applied;
}

void Tabs::onDefaultEvent(UIEvent &e) {
  if (e.handled)
    return;
  std::vector<NavigationItem> items;
  items.reserve(_items.size());
  std::optional<std::string> current;
  for (auto &i : _items) {
    items.push_back({i.key, i.tab->semanticProps().name, i.enabled});
    if (i.tab->hasFocus())
      current = i.key;
  }
  if (!current)
    return;
  if (auto next = _navigation.navigate(
          items, current, e, services() ? services()->scheduler->now() : 0,
          {layout::Axis::Horizontal, true, layoutDirection()})) {
    performAction(SelectItem{*next}, e.source);
    e.handled = true;
  }
}

Dialog::Dialog(std::unique_ptr<Node> content, DialogProps props,
               layout::BoxProps box)
    : Popup{std::move(content)} {
  setBoxProps(box);
  setProps(std::move(props));
}

void Dialog::setProps(DialogProps p) {
  p.modal |= requiresModal();
  _props = std::move(p);
  setInputProps({.hitTest = HitTestPolicy::SelfAndChildren,
                 .focusable = true,
                 .focusScope = true,
                 .modal = _props.modal,
                 .initialFocus = _props.initialFocus,
                 .returnFocus = _props.returnFocus});
  setSemanticProps({.role = SemanticRole::Dialog,
                    .name = _props.name,
                    .description = _props.description});
  setPopupProps({.open = _props.open,
                 .placement = PopupPlacement::Center,
                 .width = PopupWidth::Content,
                 .maximumHeight = std::numeric_limits<float>::max(),
                 .dismissOutside = false,
                 .dismissOnEscape = _props.dismissOnEscape,
                 .closeOnTab = false,
                 .themeBackdrop = _props.modal});
}

void Dialog::applyPatch(const DialogPatch &p) {
  setProps({p.open.appliedTo(_props.open, false),
            p.modal.appliedTo(_props.modal, true),
            p.dismissOnEscape.appliedTo(_props.dismissOnEscape, true),
            p.name.appliedTo(_props.name, {}),
            p.description.appliedTo(_props.description, {}),
            p.initialFocus.appliedTo(_props.initialFocus, {}),
            p.returnFocus.appliedTo(_props.returnFocus, {})});
}

ActionResult Dialog::performAction(const UIAction &a, ActionSource) {
  const auto *p = std::get_if<SetExpanded>(&a);
  if (!p)
    return ActionResult::Unsupported;
  if (p->value == _props.open)
    return ActionResult::Unchanged;
  auto next = _props;
  next.open = p->value;
  setProps(std::move(next));
  if (!p->value)
    _dismissed.emit();
  return ActionResult::Applied;
}

void Dialog::onDefaultEvent(UIEvent &e) {
  if (!e.handled && e.type == EventType::KeyDown &&
      e.logicalKey == Key::Escape && _props.dismissOnEscape) {
    performAction(SetExpanded{false}, e.source);
    e.handled = true;
  }
}

void Dialog::dismiss(DismissReason) {
  performAction(SetExpanded{false}, ActionSource::Program);
}

void Dialog::paint(PaintContext &p) const {
  p.fill({{}, bounds().size}, theme().elevated);
  control_paint::outline(p, {{}, bounds().size}, theme().border,
                         themeMetrics().indicatorStroke);
}

Field::Field(std::unique_ptr<Node> control, std::unique_ptr<Node> label,
             std::unique_ptr<Node> description, FieldProps props,
             layout::BoxProps box, bool inlineControl)
    : VStack{{}, box}, _props{std::move(props)} {
  setControlLayout(ControlLayout::FieldStack);
  if (!control || !label)
    throw std::invalid_argument("Field requires control and label");
  _control = control.get();
  _label = label.get();
  _description = description.get();
  _labelBaseline = _label->semanticProps();
  if (_description)
    _descriptionBaseline = _description->semanticProps();
  if (inlineControl) {
    auto row = std::make_unique<HStack>(layout::StackProps{
        .childrenAlignment = layout::CrossAlignment::Center});
    row->setControlLayout(ControlLayout::FieldRow);
    row->append(std::move(label), {.grow = 1});
    row->append(std::move(control), {.shrink = 0});
    append(std::move(row));
  } else {
    append(std::move(label));
    append(std::move(control));
  }
  if (description)
    append(std::move(description));
  link();
}

void Field::link() {
  auto &target = _control->focusTarget();
  auto p = target.semanticProps();
  p.labelledBy = _label->id();
  if (_description)
    p.describedBy = _description->id();
  target.setSemanticProps(std::move(p));
  {
    auto label = _label->semanticProps();
    label.name = _props.label.empty() ? _labelBaseline.name : _props.label;
    _label->setSemanticProps(std::move(label));
  }
  if (_description) {
    auto description = _description->semanticProps();
    description.name = _props.description.empty() ? _descriptionBaseline.name
                                                  : _props.description;
    _description->setSemanticProps(std::move(description));
  }
}

void Field::onAttach(UIServices &) { link(); }

void Field::arrangeChildren(ArrangeContext &context, math::Rect bounds) {
  VStack::arrangeChildren(context, bounds);
  link();
}

void Field::setFieldProps(FieldProps p) {
  _props = std::move(p);
  link();
}

void Field::applyFieldPatch(const FieldPatch &p) {
  setFieldProps({p.label.appliedTo(_props.label, {}),
                 p.description.appliedTo(_props.description, {})});
}

void Field::onDefaultEvent(UIEvent &e) {
  if (!e.handled && e.type == EventType::PointerDown && e.button == 1) {
    _control->focusTarget().requestFocus();
    e.handled = true;
  }
}

FieldGroup::FieldGroup(std::string name, layout::StackProps props,
                       layout::BoxProps box)
    : VStack{props, box} {
  setSemanticProps({.role = SemanticRole::Group, .name = std::move(name)});
}

Status::Status(std::unique_ptr<Node> content, std::string message,
               layout::BoxProps box)
    : Box{box} {
  if (content)
    setChild(std::move(content));
  setSemanticProps({.role = SemanticRole::Status,
                    .name = std::move(message),
                    .exposure = SemanticExposure::Self});
  setHitTestPolicy(HitTestPolicy::None);
}

void Status::setMessage(std::string value) {
  auto p = semanticProps();
  p.name = std::move(value);
  setSemanticProps(std::move(p));
}

Tooltip::Tooltip(std::unique_ptr<Node> content, std::string description,
                 layout::BoxProps box)
    : Popup{std::move(content),
            {.placement = PopupPlacement::AboveStart,
             .fallbacks = {PopupPlacement::BelowStart},
             .width = PopupWidth::Content,
             .dismissOutside = false,
             .closeOnTab = false,
             .autoFocus = false}} {
  setBoxProps(box);
  setSemanticProps(
      {.role = SemanticRole::Tooltip, .name = std::move(description)});
  setHitTestPolicy(HitTestPolicy::None);
  setOpen(false);
}
} // namespace playground::ui
