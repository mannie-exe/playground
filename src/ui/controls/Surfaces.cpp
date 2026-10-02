#include <ui/controls/Surfaces.hpp>

namespace playground::ui {
Popover::Popover(std::unique_ptr<Node> trigger, std::unique_ptr<Node> content,
                 PopupProps props, layout::BoxProps box)
    : VStack{{.childrenAlignment = layout::CrossAlignment::Stretch}, box} {
  auto button = std::make_unique<Button>(std::move(trigger));
  _trigger = button.get();
  auto popup = std::make_unique<Popup>(std::move(content), props);
  _popup = popup.get();
  _connections.push_back(button->onActivate([this] { setOpen(!isOpen()); }));
  _connections.push_back(popup->onDismissed([this](DismissReason reason) {
    if (reason != DismissReason::OwnerUnavailable)
      _trigger->requestFocus();
  }));
  append(std::move(button));
  append(std::move(popup));
}

void Popover::arrangeChildren(ArrangeContext &c, math::Rect b) {
  VStack::arrangeChildren(c, b);
  _popup->setAnchor(_trigger->id());
}

void Popover::setOpen(bool open) {
  _popup->setOpen(open);
  _trigger->requestFocus();
}

DropdownMenu::DropdownMenu(std::unique_ptr<Node> trigger,
                           std::unique_ptr<MenuList> menu, layout::BoxProps box)
    : Popover{std::move(trigger), std::move(menu), {}, box} {
  auto *list = dynamic_cast<MenuList *>(popup().children().front().get());
  _invocation =
      list->onInvoked([this](std::string, ActionSource) { setOpen(false); });
}

ContextMenu::ContextMenu(std::unique_ptr<Node> owner,
                         std::unique_ptr<MenuList> menu, layout::BoxProps box)
    : VStack{{}, box} {
  if (!owner || !menu)
    throw std::invalid_argument("ContextMenu requires owner and menu");
  _owner = owner.get();
  _owner->setFocusable(true);
  _invocation = menu->onInvoked([this](std::string, ActionSource) {
    _popup->setOpen(false);
    _owner->focusTarget().requestFocus();
  });
  auto popup = std::make_unique<Popup>(
      std::move(menu), PopupProps{.width = PopupWidth::Content, .gap = 0});
  _popup = popup.get();
  _dismissed = popup->onDismissed([this](DismissReason reason) {
    if (reason != DismissReason::OwnerUnavailable)
      _owner->focusTarget().requestFocus();
  });
  append(std::move(owner));
  append(std::move(popup));
}

void ContextMenu::arrangeChildren(ArrangeContext &c, math::Rect b) {
  VStack::arrangeChildren(c, b);
  _popup->setAnchor(_owner->id());
}

void ContextMenu::openAt(std::optional<math::Point2> position) {
  auto p = _popup->popupProps();
  p.position = position;
  p.open = true;
  _popup->setPopupProps(p);
  _owner->requestFocus();
}

void ContextMenu::onDefaultEvent(UIEvent &e) {
  if (e.handled)
    return;
  if ((e.type == EventType::PointerDown && e.button == 3) ||
      (e.type == EventType::KeyDown && e.logicalKey == Key::ContextMenu)) {
    openAt(e.type == EventType::PointerDown ? std::optional{e.position}
                                            : std::nullopt);
    e.handled = true;
  }
}

TooltipTrigger::TooltipTrigger(std::unique_ptr<Node> owner,
                               std::unique_ptr<Node> help,
                               std::string description, TooltipTiming timing)
    : VStack{{}}, _timing{timing} {
  if (!owner ||
      (timing.showDelay &&
       (!std::isfinite(*timing.showDelay) || *timing.showDelay < 0)) ||
      (timing.hideDelay &&
       (!std::isfinite(*timing.hideDelay) || *timing.hideDelay < 0)))
    throw std::invalid_argument("Invalid tooltip owner/timing");
  _owner = owner.get();
  auto tooltip = std::make_unique<Tooltip>(std::move(help), description);
  _tooltip = tooltip.get();
  auto semantic = owner->focusTarget().semanticProps();
  semantic.description = std::move(description);
  owner->focusTarget().setSemanticProps(std::move(semantic));
  append(std::move(owner));
  append(std::move(tooltip));
}

void TooltipTrigger::arrangeChildren(ArrangeContext &c, math::Rect b) {
  VStack::arrangeChildren(c, b);
  _tooltip->setAnchor(_owner->id());
}

void TooltipTrigger::schedule() {
  _timer.disconnect();
  if (!services() || !services()->scheduler)
    return;
  const bool open = _hovered || _focused;
  auto self = handle<TooltipTrigger>();
  _timer = services()->scheduler->schedule(
      open ? _timing.showDelay.value_or(themeMetrics().tooltipShowDelay)
           : _timing.hideDelay.value_or(themeMetrics().tooltipHideDelay),
      [self, open] {
        if (auto *node = self.get())
          node->_tooltip->setOpen(open);
      });
}

void TooltipTrigger::onDefaultEvent(UIEvent &e) {
  if (e.type == EventType::PointerEnter)
    _hovered = true;
  else if (e.type == EventType::PointerLeave)
    _hovered = false;
  else if ((e.type == EventType::FocusGained ||
            e.type == EventType::FocusWithinGained))
    _focused = true;
  else if ((e.type == EventType::FocusLost ||
            e.type == EventType::FocusWithinLost))
    _focused = false;
  else if (e.type == EventType::InputCancel ||
           e.type == EventType::PointerDown ||
           (e.type == EventType::KeyDown && e.logicalKey == Key::Escape)) {
    _timer.disconnect();
    _tooltip->setOpen(false);
    return;
  } else
    return;
  schedule();
}
} // namespace playground::ui
