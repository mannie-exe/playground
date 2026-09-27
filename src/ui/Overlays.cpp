#include <algorithm>
#include <vector>

#include <ui/TextEdit.hpp>
#include <ui/UIRoot.hpp>
#include <ui/containers/Popup.hpp>

namespace playground::ui {
void UIRoot::layoutOverlays() {
  _overlays.clear();
  if (!_content)
    return;
  std::vector<Node *> nodes;
  collect(*_content, nodes);
  // Logical preorder places nested popups after their owning popup.
  Node *modal{};
  for (auto *n : nodes) {
    bool visible = true;
    for (auto *p = n; p; p = p->parent())
      visible &=
          p->visibility() == Visibility::Visible && p->isInteractionEnabled();
    if (n->inputProps().modal && visible)
      modal = n;
  }
  const auto viewport =
      _environment ? math::inset({{}, _viewport}, _environment->usableInsets)
                   : math::Rect{{}, _viewport};
  for (auto *n : nodes) {
    auto *portal = dynamic_cast<Portal *>(n);
    if (!portal || !portal->popupProps().open)
      continue;
    bool available = true;
    for (auto *p = portal->parent(); p; p = p->parent())
      available &=
          p->visibility() == Visibility::Visible && p->isInteractionEnabled();
    auto *anchor = resolve(portal->popupProps().anchor);
    if (portal->popupProps().placement != PopupPlacement::Center)
      available &= anchor && acceptsAction(*anchor);
    if (available && anchor) {
      auto visible = math::intersect(
          viewport,
          anchor->worldTransform().mapBounds({{}, anchor->bounds().size}));
      for (auto *p = anchor->parent(); p && !p->isPortal(); p = p->parent())
        if (p->clipsContent())
          visible = math::intersect(
              visible,
              p->worldTransform().mapBounds(p->visualProps().clipRect.value_or(
                  math::Rect{{}, p->bounds().size})));
      available = visible.hasArea();
    }
    if (!available || (modal && !portal->inputProps().modal &&
                       !withinScope(*portal, modal))) {
      portal->dismiss(DismissReason::OwnerUnavailable);
      continue;
    }
    portal->present(_context, viewport, anchor);
    _overlays.push_back(portal->id());
    if (portal->popupProps().autoFocus && anchor && anchor->hasFocus()) {
      const auto captures = _table->captures;
      for (const auto &capture : captures)
        if (auto *node = resolve(capture.node);
            node && node != anchor && !withinScope(*node, portal)) {
          node->releasePointer(capture.pointer);
          UIEvent cancel{.type = EventType::PointerCancel,
                         .pointer = capture.pointer};
          direct(*node, cancel);
        }
      std::vector<Node *> candidates;
      collect(*portal, candidates);
      for (auto *child : candidates)
        if (child->isFocusable() && acceptsAction(*child)) {
          requestFocus(child->id());
          break;
        }
    }
  }
}

Node *UIRoot::presentationHit(math::Point2 point) {
  if (math::Rect{{}, _viewport}.contains(point))
    for (auto it = _overlays.rbegin(); it != _overlays.rend(); ++it)
      if (auto *node = resolve(*it))
        if (auto inverse = node->worldTransform().inverse())
          if (auto *found = hit(*node, inverse->mapPoint(point)))
            return found;
  if (_content)
    if (auto inverse = inputInverse(*_content))
      return hit(*_content, inverse->mapPoint(point));
  return nullptr;
}

bool UIRoot::routeOverlayDismissal(UIEvent &event) {
  const auto suppressed = std::find(_dismissedPointers.begin(),
                                    _dismissedPointers.end(), event.pointer);
  if (suppressed != _dismissedPointers.end() &&
      (event.type == EventType::PointerDown ||
       event.type == EventType::PointerUp ||
       event.type == EventType::PointerMove ||
       event.type == EventType::PointerCancel)) {
    if (event.type == EventType::PointerUp ||
        event.type == EventType::PointerCancel)
      _dismissedPointers.erase(suppressed);
    event.handled = event.propagationStopped = true;
    return true;
  }
  if (event.type == EventType::FocusLost ||
      event.type == EventType::InputCancel) {
    _dismissedPointers.clear();
    for (auto id : _overlays)
      if (auto *p = dynamic_cast<Portal *>(resolve(id));
          p && !p->inputProps().modal)
        p->dismiss(DismissReason::OwnerUnavailable);
    return false;
  }
  Portal *popup{};
  for (auto it = _overlays.rbegin(); it != _overlays.rend(); ++it) {
    auto *candidate = dynamic_cast<Portal *>(resolve(*it));
    if (!candidate || !candidate->popupProps().open)
      continue;
    const auto &p = candidate->popupProps();
    if ((event.type == EventType::PointerDown && p.dismissOutside) ||
        (event.type == EventType::KeyDown &&
         ((event.logicalKey == Key::Escape && p.dismissOnEscape) ||
          (event.logicalKey == Key::Tab && p.closeOnTab)))) {
      popup = candidate;
      break;
    }
    // A modal owns dismissal/navigation; never close an older surface behind
    // it.
    if (candidate->inputProps().modal)
      break;
  }
  if (!popup)
    return false;
  const auto props = popup->popupProps();
  if (!props.open)
    return false;
  if (event.type == EventType::KeyDown && event.logicalKey == Key::Escape &&
      props.dismissOnEscape) {
    if (auto *client =
            dynamic_cast<TextInputClient *>(resolve(_table->focused));
        client && client->textInputState().composing)
      return false;
    event.handled = event.propagationStopped = true;
    popup->dismiss(DismissReason::Escape);
  } else if (event.type == EventType::KeyDown && event.logicalKey == Key::Tab &&
             props.closeOnTab) {
    popup->dismiss(DismissReason::Tab);
    return false;
  } else if (event.type == EventType::PointerDown && props.dismissOutside) {
    auto *target = presentationHit(event.position);
    auto *anchor = resolve(props.anchor);
    if ((target && withinScope(*target, popup)) ||
        (anchor && anchor->worldTransform()
                       .mapBounds({{}, anchor->bounds().size})
                       .contains(event.position)))
      return false;
    _dismissedPointers.push_back(event.pointer);
    event.handled = event.propagationStopped = true;
    const auto captures = _table->captures;
    for (const auto &capture : captures)
      if (capture.pointer == event.pointer)
        if (auto *node = resolve(capture.node)) {
          node->releasePointer(event.pointer);
          UIEvent cancel{.type = EventType::PointerCancel,
                         .pointer = event.pointer};
          direct(*node, cancel);
        }
    popup->dismiss(DismissReason::OutsidePointer);
  } else
    return false;
  event.handled = event.propagationStopped = true;
  return true;
}
} // namespace playground::ui
