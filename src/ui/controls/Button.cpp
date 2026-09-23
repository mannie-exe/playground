#include <ui/controls/Button.hpp>

namespace playground::ui {

void Button::cancel() noexcept {
  _pointer.reset();
  _key.reset();
  _button = 0;
  releaseAllPointers();
  invalidatePaint();
}

void Button::paint(PaintContext &context) const {
  const auto color = !_props.enabled      ? _props.disabled
                     : (_pointer || _key) ? _props.pressed
                     : _hovered           ? _props.hover
                                          : _props.normal;
  context.fill({{}, bounds().size}, color);
}

void Button::onDefaultEvent(UIEvent &event) {
  if (event.type == EventType::PointerCancel ||
      event.type == EventType::FocusLost) {
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
        _activated.emit();
    }
    break;
  case EventType::KeyDown:
    if (hasFocus() && !event.repeat && !_pointer &&
        (event.logicalKey == Key::Space || event.logicalKey == Key::Enter)) {
      _key = event.logicalKey;
      invalidatePaint();
      event.handled = true;
    }
    break;
  case EventType::KeyUp:
    if (hasFocus() && _key == event.logicalKey) {
      cancel();
      event.handled = true;
      _activated.emit();
    }
    break;
  default:
    break;
  }
}

Button::Button(std::unique_ptr<Node> content, ButtonProps props,
               layout::BoxProps box)
    : Box{box, {layout::Alignment::center()}}, _props{props} {
  setHitTestPolicy(HitTestPolicy::SelfAndChildren);
  setFocusable(true);
  setSemanticProps({.role = SemanticRole::Button, .enabled = props.enabled});
  if (content)
    setChild(std::move(content));
}

void Button::setProps(ButtonProps value) {
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
  setProps(p);
}

void Button::applyPatch(const ButtonPatch &p) {
  const ButtonProps d;
  setProps({p.enabled.appliedTo(_props.enabled, d.enabled),
            p.normal.appliedTo(_props.normal, d.normal),
            p.hover.appliedTo(_props.hover, d.hover),
            p.pressed.appliedTo(_props.pressed, d.pressed),
            p.disabled.appliedTo(_props.disabled, d.disabled)});
}

} // namespace playground::ui
