#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <ui/controls/ControlPaint.hpp>
#include <ui/controls/Slider.hpp>

namespace playground::ui {
Slider::Slider(SliderProps props, layout::BoxProps box) : Node{box} {
  setInputProps({HitTestPolicy::Self, true});
  setProps(std::move(props));
}

void Slider::setProps(SliderProps props) {
  props.range.validate();
  if (props.axis != layout::Axis::Horizontal &&
      props.axis != layout::Axis::Vertical)
    throw std::invalid_argument("Invalid slider axis");
  const bool canceled = _pointer && (!props.enabled || props.readOnly);
  _props = std::move(props);
  if (!_props.enabled || _props.readOnly) {
    _hovered = false;
    _pointer.reset();
    releaseAllPointers();
  }
  invalidate(DirtyFlags::Measure | DirtyFlags::Paint | DirtyFlags::Semantics);
  if (canceled)
    _finished.emit({ActionSource::Program, ChangeReason::Cancel});
}

void Slider::applyPatch(const SliderPatch &p) {
  const SliderProps d;
  setProps({p.range.appliedTo(_props.range, d.range),
            p.enabled.appliedTo(_props.enabled, d.enabled),
            p.readOnly.appliedTo(_props.readOnly, d.readOnly),
            p.axis.appliedTo(_props.axis, d.axis),
            p.name.appliedTo(_props.name, d.name),
            p.track.appliedTo(_props.track, d.track),
            p.thumb.appliedTo(_props.thumb, d.thumb),
            p.useTheme.appliedTo(_props.useTheme, d.useTheme),
            p.snapToStep.appliedTo(_props.snapToStep, d.snapToStep)});
}

layout::MeasureResult Slider::measureContent(MeasureContext &,
                                             const layout::SizeConstraints &) {
  return {_props.axis == layout::Axis::Horizontal ? math::Size2{160, 24}
                                                  : math::Size2{24, 160}};
}

void Slider::paint(PaintContext &p) const {
  const auto &r = _props.range;
  const float t = r.maximum == r.minimum
                      ? 0
                      : float((r.value - r.minimum) / (r.maximum - r.minimum));
  const bool horizontal = _props.axis == layout::Axis::Horizontal;
  const float length = horizontal ? bounds().w() : bounds().h();
  const float breadth = horizontal ? bounds().h() : bounds().w();
  const float thumbLength = std::min(16.f, length);
  const float thumbBreadth = std::min(20.f, breadth);
  const float trackBreadth = std::min(4.f, breadth);
  const auto rect = [&](float x, float y, float w, float h) {
    return horizontal ? math::rect(x, y, w, h) : math::rect(y, x, h, w);
  };
  const auto thumb =
      rect((horizontal ? t : 1 - t) * (length - thumbLength),
           (breadth - thumbBreadth) / 2, thumbLength, thumbBreadth);
  p.fill(rect(thumbLength / 2, (breadth - trackBreadth) / 2,
              length - thumbLength, trackBreadth),
         _props.useTheme ? theme().border : _props.track);
  const bool interactive = _props.enabled && !_props.readOnly;
  p.fill(thumb, _props.useTheme
                    ? (!interactive ? theme().mutedText : theme().accent)
                    : _props.thumb);
  if (interactive && (_hovered || _pointer))
    control_paint::outline(p, thumb,
                           _props.useTheme ? theme().text : _props.track,
                           _pointer ? 3.f : 2.f);
  if (hasFocus() && _props.enabled)
    control_paint::outline(p, {{}, bounds().size},
                           _props.useTheme ? theme().focus : _props.thumb, 2);
}

SemanticState Slider::semanticState() const {
  auto s = Node::semanticState();
  s.description.role = SemanticRole::Slider;
  s.description.name = _props.name;
  s.description.enabled = _props.enabled;
  s.range = _props.range;
  s.readOnly = _props.readOnly;
  if (_props.enabled && !_props.readOnly)
    s.actions.insert(s.actions.end(),
                     {SemanticAction::SetValue, SemanticAction::Increment,
                      SemanticAction::Decrement});
  return s;
}

ActionResult Slider::performAction(const UIAction &action,
                                   ActionSource source) {
  if (!_props.enabled || _props.readOnly)
    return ActionResult::Unavailable;
  double value;
  if (const auto *set = std::get_if<SetValue>(&action))
    value = set->value;
  else if (const auto *step = std::get_if<Increment>(&action))
    value = _props.range.adjusted(step->direction);
  else
    return ActionResult::Unsupported;
  if (!std::isfinite(value) || value < _props.range.minimum ||
      value > _props.range.maximum)
    return ActionResult::Unavailable;
  if (value == _props.range.value)
    return ActionResult::Unchanged;
  auto props = _props;
  props.range.value = value;
  setProps(std::move(props));
  _edited.emit(value, {source, _pointer || source == ActionSource::Pointer
                                   ? ChangeReason::Drag
                                   : ChangeReason::Step});
  _changed.emit(value);
  if (!_pointer && source != ActionSource::Pointer)
    _finished.emit({source, ChangeReason::Step});
  return ActionResult::Applied;
}

void Slider::onDefaultEvent(UIEvent &e) {
  if (e.type == EventType::FocusGained) {
    invalidatePaint();
    return;
  }
  if (e.type == EventType::PointerEnter || e.type == EventType::PointerLeave) {
    _hovered =
        e.type == EventType::PointerEnter && _props.enabled && !_props.readOnly;
    invalidatePaint();
    return;
  }
  if (e.type == EventType::FocusLost || e.type == EventType::InputCancel ||
      (e.type == EventType::PointerCancel && _pointer == e.pointer)) {
    const bool canceled = _pointer.has_value();
    _pointer.reset();
    _hovered = false;
    releaseAllPointers();
    invalidatePaint();
    if (canceled)
      _finished.emit({e.source, ChangeReason::Cancel});
    return;
  }
  if (e.handled || !_props.enabled || _props.readOnly)
    return;
  if ((e.type == EventType::PointerDown || e.type == EventType::PointerUp) &&
      e.button != 1)
    return;
  if (e.type == EventType::PointerDown && e.button == 1 && !_pointer) {
    _pointer = e.pointer;
    capturePointer(e.pointer);
    requestFocus();
    invalidatePaint();
  }
  if (_pointer == e.pointer &&
      (e.type == EventType::PointerDown || e.type == EventType::PointerMove ||
       e.type == EventType::PointerUp)) {
    const bool horizontal = _props.axis == layout::Axis::Horizontal;
    const float extent = horizontal ? bounds().w() : bounds().h();
    const float halfThumb = std::min(16.f, extent) / 2;
    double t =
        extent > 2 * halfThumb
            ? ((horizontal ? e.localPosition.x : extent - e.localPosition.y) -
               halfThumb) /
                  (extent - 2 * halfThumb)
            : 0;
    t = std::clamp(t, 0., 1.);
    double value = _props.range.minimum +
                   t * (_props.range.maximum - _props.range.minimum);
    const double steps = (value - _props.range.minimum) / _props.range.step;
    // An overflowing quotient means the step is below representable resolution
    // at this position. Keep the interpolated value instead of snapping to inf.
    if (_props.snapToStep && std::isfinite(steps))
      value = std::clamp(_props.range.minimum +
                             std::round(steps) * _props.range.step,
                         _props.range.minimum, _props.range.maximum);
    e.handled = true;
    _hovered = math::Rect{{}, bounds().size}.contains(e.localPosition);
    if (e.type == EventType::PointerUp) {
      _pointer.reset();
      releaseAllPointers();
    }
    invalidatePaint();
    performAction(SetValue{value}, ActionSource::Pointer);
    if (e.type == EventType::PointerUp)
      _finished.emit({ActionSource::Pointer, ChangeReason::Drag});
  }
  if (e.type == EventType::KeyDown) {
    if (e.logicalKey == Key::Left || e.logicalKey == Key::Down) {
      performAction(Increment{-1}, e.source);
      e.handled = true;
    }
    if (e.logicalKey == Key::Right || e.logicalKey == Key::Up) {
      performAction(Increment{1}, e.source);
      e.handled = true;
    }
    if (e.logicalKey == Key::Home || e.logicalKey == Key::End) {
      performAction(SetValue{e.logicalKey == Key::Home ? _props.range.minimum
                                                       : _props.range.maximum},
                    e.source);
      e.handled = true;
    }
  }
}

ProgressBar::ProgressBar(ProgressProps p, layout::BoxProps box) : Node{box} {
  setProps(std::move(p));
}

void ProgressBar::setProps(ProgressProps p) {
  p.range.validate();
  _props = std::move(p);
  invalidate(DirtyFlags::Paint | DirtyFlags::Semantics);
}

layout::MeasureResult
ProgressBar::measureContent(MeasureContext &, const layout::SizeConstraints &) {
  return {{160, 16}};
}

void ProgressBar::paint(PaintContext &p) const {
  p.fill({{}, bounds().size}, _props.useTheme ? theme().border : _props.track);
  const auto &r = _props.range;
  const float t = _props.indeterminate ? .35f
                  : r.minimum == r.maximum
                      ? 0
                      : float((r.value - r.minimum) / (r.maximum - r.minimum));
  p.fill(math::rect(0, 0, bounds().w() * t, bounds().h()),
         _props.useTheme ? theme().accent : _props.fill);
}

SemanticState ProgressBar::semanticState() const {
  auto s = Node::semanticState();
  s.description.role = SemanticRole::Progress;
  s.description.name = _props.name;
  if (!_props.indeterminate)
    s.range = _props.range;
  else
    s.description.value = "In progress; completion unknown";
  s.readOnly = true;
  return s;
}
} // namespace playground::ui
