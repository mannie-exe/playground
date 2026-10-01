#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <ui/controls/Button.hpp>
#include <ui/controls/ControlPaint.hpp>

namespace playground::ui {
SemanticState Button::semanticState() const {
  auto state = Node::semanticState();
  state.description.enabled = _props.enabled;
  if (_props.enabled)
    state.actions.push_back(SemanticAction::Activate);
  return state;
}

ActionResult Button::performAction(const UIAction &action,
                                   ActionSource source) {
  if (!_props.enabled)
    return ActionResult::Unavailable;
  if (!std::holds_alternative<Activate>(action))
    return ActionResult::Unsupported;
  _invoked.emit(source);
  _activated.emit();
  return ActionResult::Applied;
}

void ButtonProps::validate() const {
  if (!std::isfinite(focusWidth) || focusWidth < 0)
    throw std::invalid_argument("Invalid button focus width");
}

void Button::paintSubtree(PaintContext &context) const {
  Box::paintSubtree(context);
  if (!hasFocus() || !_props.enabled)
    return;
  const auto size = bounds().size;
  const float t =
      std::min({_props.focusWidth, size.width / 2, size.height / 2});
  if (t <= 0)
    return;
  control_paint::outline(context, {{}, size},
                         _props.useTheme ? theme().focus : _props.focus, t);
}

void Button::cancel() noexcept {
  _pointer.reset();
  _key.reset();
  _button = 0;
  releaseAllPointers();
  invalidatePaint();
}

void Button::paint(PaintContext &context) const {
  if (_props.useTheme) {
    const auto &t = theme();
    context.fill({{}, bounds().size}, !_props.enabled ? t.surface
                                      : isPressed()   ? t.pressed
                                      : isHovered()   ? t.hover
                                                      : t.elevated);
    control_paint::outline(context, {{}, bounds().size},
                           !_props.enabled ? t.mutedText : t.border,
                           t.highContrast ? 2.f : 1.f);
    return;
  }
  const auto color = !_props.enabled      ? _props.disabled
                     : (_pointer || _key) ? _props.pressed
                     : _hovered           ? _props.hover
                                          : _props.normal;
  context.fill({{}, bounds().size}, color);
}

void Button::onDefaultEvent(UIEvent &event) {
  if (event.type == EventType::FocusGained) {
    invalidatePaint();
    return;
  }
  if (event.type == EventType::PointerCancel ||
      event.type == EventType::FocusLost ||
      event.type == EventType::InputCancel) {
    cancel();
    return;
  }
  if (event.type == EventType::PointerEnter) {
    _hovered = true;
    invalidatePaint();
    return;
  }
  if (event.type == EventType::PointerLeave) {
    _hovered = false;
    invalidatePaint();
    return;
  }
  if (!_props.enabled || event.handled)
    return;
  const bool inside =
      math::Rect{{}, bounds().size}.contains(event.localPosition);
  switch (event.type) {
  case EventType::PointerDown:
    if (event.button == 1 && inside && !_pointer && !_key) {
      _pointer = event.pointer;
      _button = event.button;
      _hovered = true;
      capturePointer(event.pointer);
      requestFocus();
      invalidatePaint();
      event.handled = true;
    }
    break;
  case EventType::PointerMove:
    if (_pointer && *_pointer == event.pointer) {
      _hovered = inside;
      invalidatePaint();
    }
    break;
  case EventType::PointerUp:
    if (_pointer && *_pointer == event.pointer && event.button == _button) {
      cancel();
      _hovered = inside;
      event.handled = true;
      if (inside)
        performAction(Activate{}, ActionSource::Pointer);
    }
    break;
  case EventType::KeyDown:
    if (hasFocus() && !event.repeat && !_pointer && !_key &&
        (event.logicalKey == Key::Space || event.logicalKey == Key::Enter)) {
      _key = event.logicalKey;
      _keySource = event.source;
      invalidatePaint();
      event.handled = true;
    }
    break;
  case EventType::KeyUp:
    if (hasFocus() && _key == event.logicalKey && _keySource == event.source) {
      cancel();
      event.handled = true;
      performAction(Activate{}, event.source);
    }
    break;
  default:
    break;
  }
}

Button::Button(std::unique_ptr<Node> content, ButtonProps props,
               layout::BoxProps box)
    : Box{box, {layout::Alignment::center()}}, _props{props} {
  _props.validate();
  setHitTestPolicy(HitTestPolicy::SelfAndChildren);
  setFocusable(true);
  setSemanticProps({.role = SemanticRole::Button, .enabled = props.enabled});
  if (content)
    setChild(std::move(content));
}

void Button::setButtonProps(ButtonProps value) {
  value.validate();
  if (value == _props)
    return;
  auto semantics = semanticProps();
  semantics.enabled = value.enabled;
  setSemanticProps(std::move(semantics));
  _props = value;
  if (!value.enabled) {
    cancel();
    clearFocus();
  }
  invalidatePaint();
}

void Button::setEnabled(bool value) {
  auto p = _props;
  p.enabled = value;
  setButtonProps(p);
}

void Button::applyButtonPatch(const ButtonPatch &p) {
  const ButtonProps d;
  setButtonProps({p.enabled.appliedTo(_props.enabled, d.enabled),
                  p.normal.appliedTo(_props.normal, d.normal),
                  p.hover.appliedTo(_props.hover, d.hover),
                  p.pressed.appliedTo(_props.pressed, d.pressed),
                  p.disabled.appliedTo(_props.disabled, d.disabled),
                  p.focus.appliedTo(_props.focus, d.focus),
                  p.focusWidth.appliedTo(_props.focusWidth, d.focusWidth),
                  p.useTheme.appliedTo(_props.useTheme, d.useTheme)});
}

} // namespace playground::ui
