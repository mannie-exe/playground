#include <cmath>
#include <stdexcept>

#include <ui/content/ContentTypes.hpp>
#include <ui/content/InspectionView.hpp>

namespace playground::ui {
void InspectionProps::validate() const {
  if (!std::isfinite(orbitSensitivity) || orbitSensitivity <= 0 ||
      !std::isfinite(zoomSensitivity) || zoomSensitivity <= 0)
    throw std::invalid_argument("Invalid inspection sensitivity");
}

InspectionView::InspectionView(SceneViewProps view, scene::OrbitProps orbit,
                               InspectionProps navigation, layout::BoxProps box)
    : SceneView{std::move(view), box}, _orbit{orbit},
      _initialPivot{orbit.target}, _navigation{navigation} {
  navigation.validate();
  setHitTestPolicy(HitTestPolicy::Self);
  setFocusable(true);
}

void InspectionView::setNavigationProps(InspectionProps props) {
  props.validate();
  if (props == _navigation)
    return;
  cancelNavigation();
  _navigation = props;
}

void InspectionView::cancelNavigation() noexcept {
  _pointer.reset();
  releaseAllPointers();
}

void InspectionView::publish() { _changed.emit(_orbit.camera()); }

void InspectionView::resetPan() {
  cancelNavigation();
  if (_orbit.props().target == _initialPivot)
    return;
  auto props = _orbit.props();
  props.target = _initialPivot;
  _orbit.setProps(props);
  publish();
}

void InspectionView::onDefaultEvent(UIEvent &event) {
  if (event.type == EventType::InputCancel ||
      event.type == EventType::FocusLost ||
      (event.type == EventType::PointerCancel && _pointer == event.pointer)) {
    cancelNavigation();
    return;
  }
  if (event.type == EventType::KeyDown && !event.repeat) {
    if (event.logicalKey == Key::Escape && isNavigating()) {
      cancelNavigation();
      event.handled = true;
      event.preventDefault();
    } else if (event.logicalKey == Key::R && !event.control && !event.alt &&
               !event.command) {
      resetPan();
      event.handled = true;
      event.preventDefault();
    }
    return;
  }
  // Camera publication invalidates the rendered viewport. Input can contain
  // several events before the next paint, so navigation uses current layout.
  const auto view =
      isArranged()
          ? scene::resolveViewport(
                _orbit.props().lens,
                {content_detail::contentBounds(bounds(), contentInsets()),
                 {1, 1},
                 1,
                 props().aspectRatio})
          : std::nullopt;
  if (!view) {
    cancelNavigation();
    return;
  }
  const bool inside = bool(view->normalizedPosition(event.localPosition));
  if (event.type == EventType::PointerDown && !_pointer && inside &&
      (event.button == 2 ||
       (event.button == 1 && event.alt && _navigation.altPrimary))) {
    _gesture = event.shift     ? Gesture::Pan
               : event.control ? Gesture::Zoom
                               : Gesture::Orbit;
    _pointer = event.pointer;
    _button = event.button;
    _previous = event.localPosition;
    capturePointer(event.pointer);
    requestFocus();
    event.handled = true;
    event.preventDefault();
  } else if (event.type == EventType::PointerMove &&
             _pointer == event.pointer) {
    const auto delta = event.localPosition - _previous;
    _previous = event.localPosition;
    const auto before = _orbit.camera();
    if (_gesture == Gesture::Pan)
      _orbit.pan(delta, view->contentBounds.h());
    else if (_gesture == Gesture::Zoom)
      _orbit.update(
          {.zoomLog = double(delta.y) * .01 * _navigation.zoomSensitivity});
    else
      _orbit.update({.radians = {float(-delta.x * _navigation.orbitSensitivity),
                                 float(delta.y * _navigation.orbitSensitivity *
                                       (_navigation.invertY ? -1 : 1))}});
    if (_orbit.camera() != before)
      publish();
    event.handled = true;
    event.preventDefault();
  } else if (event.type == EventType::PointerUp && _pointer == event.pointer &&
             event.button == _button) {
    cancelNavigation();
    event.handled = true;
    event.preventDefault();
  } else if (event.type == EventType::Wheel && inside && !_pointer) {
    const auto before = _orbit.camera();
    _orbit.update(
        {.zoomLog = double(event.delta.y) * .1 * _navigation.zoomSensitivity});
    if (_orbit.camera() != before)
      publish();
    event.handled = true;
    event.preventDefault();
  }
}
} // namespace playground::ui
