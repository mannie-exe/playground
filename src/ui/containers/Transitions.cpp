#include <ui/containers/Transitions.hpp>

namespace playground::ui {
Presence::Presence(std::unique_ptr<Node> child, bool shown,
                   layout::BoxProps box)
    : Box{box}, _shown{shown},
      _state{shown ? PresenceState::Present : PresenceState::Hidden} {
  setChild(std::move(child));
  if (!shown) {
    setVisibility(Visibility::Collapsed);
    setInert(true);
    setMotionValue(MotionProperty::Opacity, 0.f);
  }
}

void Presence::setShown(bool shown) {
  if (_shown == shown)
    return;
  _shown = shown;
  animate();
}

void Presence::animate() {
  const auto generation = ++_generation;
  auto *s = services();
  if (!s || !s->motion) {
    setVisibility(_shown ? Visibility::Visible : Visibility::Collapsed);
    setInert(!_shown);
    setMotionValue(MotionProperty::Opacity, _shown ? 1.f : 0.f);
    _state = _shown ? PresenceState::Present : PresenceState::Hidden;
    return;
  }
  setInert(!_shown);
  if (_shown)
    setVisibility(Visibility::Visible);
  _state = _shown ? PresenceState::Entering : PresenceState::Exiting;
  auto self = handle<Presence>();
  _animation = s->motion->transition(
      motion::opacity(handle()), _shown ? 1.f : 0.f,
      resolvedTheme().motion.get(_shown ? MotionRole::Reveal
                                        : MotionRole::Dismiss),
      [self, generation](AnimationStatus status) {
        if (status != AnimationStatus::Completed)
          return;
        if (auto *node = self.get(); node && node->_generation == generation) {
          node->_state =
              node->_shown ? PresenceState::Present : PresenceState::Hidden;
          if (!node->_shown)
            node->setVisibility(Visibility::Collapsed);
        }
      });
  if (_animation.status() == AnimationStatus::Completed) {
    _state = _shown ? PresenceState::Present : PresenceState::Hidden;
    if (!_shown)
      setVisibility(Visibility::Collapsed);
  }
}

TransitionHost::TransitionHost(layout::BoxProps box) : Node{box} {
  setClip(true);
}

layout::MeasureResult
TransitionHost::measureContent(MeasureContext &ctx,
                               const layout::SizeConstraints &constraints) {
  return current() ? current()->measure(ctx, constraints)
                   : layout::MeasureResult{};
}

void TransitionHost::arrangeChildren(ArrangeContext &ctx, math::Rect bounds) {
  for (auto &child : children()) {
    child->measure(ctx, layout::SizeConstraints::tight(bounds.size));
    child->arrange(ctx, bounds);
  }
}

void TransitionHost::replace(ItemKey key, std::unique_ptr<Node> child,
                             bool focusIncoming) {
  if (!child)
    throw std::invalid_argument("Transition content must be non-null");
  if (current() && key == _key)
    return;
  auto *outgoing = current();
  auto previousKey = _key;
  const bool previousInert = outgoing && outgoing->inert();
  const float authoredOpacity = outgoing ? outgoing->paintStyle().opacity : 1.f;
  const float outgoingOpacity =
      outgoing ? std::get<float>(outgoing->motionValue(MotionProperty::Opacity))
               : 1.f;
  NodeId previousFocus;
  if (outgoing) {
    const auto findFocus = [&](auto &&self, Node &node) -> void {
      if (node.hasFocus())
        previousFocus = node.id();
      for (auto &child : node.children())
        self(self, *child);
    };
    findFocus(findFocus, *outgoing);
  }
  // Attach first: a failing candidate leaves existing content intact.
  appendChild(std::move(child));
  auto *s = services();
  try {
    _key = std::move(key);
    ++_generation;
    _enter.cancel();
    _exit.cancel();
    if (outgoing)
      outgoing->setMotionValue(MotionProperty::Opacity, outgoingOpacity);
    while (children().size() > 2)
      takeChildAt(0);
    if (outgoing)
      outgoing->setInert(true);
    if (!s || !s->motion) {
      while (children().size() > 1)
        takeChildAt(0);
    } else {
      _enter = s->motion->play(
          motion::opacity(current()->handle()),
          Keyframes<float>{{{0, 0.f},
                            {1, std::get<float>(current()->motionValue(
                                    MotionProperty::Opacity))}}},
          resolvedTheme().motion.reveal);
      if (outgoing) {
        auto self = handle<TransitionHost>();
        const auto generation = _generation;
        _exit = s->motion->transition(
            motion::opacity(outgoing->handle()), 0.f,
            resolvedTheme().motion.dismiss,
            [self, generation](AnimationStatus status) {
              if (status != AnimationStatus::Completed)
                return;
              if (auto *node = self.get();
                  node && node->_generation == generation) {
                node->services()->defer([self, generation] {
                  if (auto *n = self.get(); n && n->_generation == generation &&
                                            n->children().size() > 1)
                    n->takeChildAt(0);
                });
              }
            });
      }
    }
  } catch (...) {
    ++_generation;
    _enter.cancel();
    _exit.cancel();
    takeChildAt(children().size() - 1);
    while (children().size() > 1)
      takeChildAt(0);
    _key.swap(previousKey);
    if (outgoing) {
      outgoing->setMotionValue(MotionProperty::Opacity, authoredOpacity);
      outgoing->setInert(previousInert);
    }
    if (previousFocus != NodeId{} && s && s->focusAfterLayout)
      s->focusAfterLayout(previousFocus);
    throw;
  }
  if (focusIncoming && s && s->focusAfterLayout)
    s->focusAfterLayout(current()->focusTarget().id());
}
} // namespace playground::ui
