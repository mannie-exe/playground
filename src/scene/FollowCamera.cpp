#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

#include <scene/Controllers.hpp>

namespace playground::scene {
namespace {
void checkTarget(const CameraTargetSample &target,
                 const world::SpatialLimits &limits) {
  const auto &subject = target.subject;
  world::validate(subject.entity.id);
  world::validate(subject.pose, limits);
  limits.validate(target.followAnchor);
  limits.validate(target.eyeAnchor);
  if (!subject.entity.epoch || !subject.discontinuity ||
      subject.entity.id.world != subject.pose.position.space.world ||
      target.followAnchor.space != subject.pose.position.space ||
      target.eyeAnchor.space != subject.pose.position.space)
    throw std::invalid_argument("Invalid camera target identity or anchors");
}

double response(double seconds, double halfLife, bool immediate) {
  return immediate || halfLife == 0
             ? 1
             : -std::expm1(-std::numbers::ln2 * seconds / halfLife);
}

CameraProps lens(CameraProps a, CameraProps b, double t) {
  if (a.orthographicHeight.has_value() != b.orthographicHeight.has_value())
    return t < 1 ? a : b;
  const auto mix = [t](float x, float y) {
    return float(std::lerp(double(x), double(y), t));
  };
  a.verticalFov = mix(a.verticalFov, b.verticalFov);
  a.nearPlane = mix(a.nearPlane, b.nearPlane);
  a.farPlane = mix(a.farPlane, b.farPlane);
  if (a.orthographicHeight)
    a.orthographicHeight = mix(*a.orthographicHeight, *b.orthographicHeight);
  a.eye = {};
  a.target = {0, 0, 1};
  a.up = {0, 1, 0};
  return a;
}
} // namespace

CameraTargetSample cameraTarget(const PoseSample &sample, world::Vec3d follow,
                                world::Vec3d eye,
                                const world::SpatialLimits &limits) {
  CameraTargetSample result{
      sample,
      world::translated(sample.pose.position,
                        world::rotate(sample.pose.orientation, follow), limits),
      world::translated(sample.pose.position,
                        world::rotate(sample.pose.orientation, eye), limits)};
  checkTarget(result, limits);
  return result;
}

void FollowCameraProps::validate() const {
  limits.validate();
  LookController{{}, look};
  for (double value :
       {distance, minimumDistance, maximumDistance, enterDistance, exitDistance,
        followHalfLife, perspectiveHalfLife, settleMeters})
    if (!std::isfinite(value) || value < 0)
      throw std::invalid_argument("Invalid follow camera range/response");
  if (minimumDistance > distance || distance > maximumDistance ||
      enterDistance >= exitDistance || enterDistance < minimumDistance ||
      exitDistance > maximumDistance || settleMeters <= 0 ||
      !world::isFinite(shoulder) ||
      (perspective != PerspectivePolicy::Automatic &&
       perspective != PerspectivePolicy::FirstPerson &&
       perspective != PerspectivePolicy::ThirdPerson))
    throw std::invalid_argument("Invalid follow camera props");
  for (auto value : {thirdPersonLens, firstPersonLens}) {
    value.eye = {};
    value.target = {0, 0, 1};
    value.up = {0, 1, 0};
    value.view(1);
    value.orthographicHeight.reset();
    value.view(1);
  }
}

FollowCameraRig::FollowCameraRig(FollowCameraProps props) { setProps(props); }

void FollowCameraRig::setProps(FollowCameraProps props) {
  props.validate();
  _props = props;
  _state.requested = props.perspective;
  _state.requestedDistance = props.distance;
  if (_target)
    _state.transitioning = true;
}

void FollowCameraRig::evaluate(const CameraTargetSample &target, LookState look,
                               ZoomIntent zoom, double seconds,
                               bool resetState) {
  checkTarget(target, _props.limits);
  if (!std::isfinite(seconds) || seconds < 0 ||
      !std::isfinite(zoom.deltaMeters) || !std::isfinite(zoom.metersPerSecond))
    throw std::invalid_argument("Invalid follow camera elapsed time/zoom");
  LookController orientation{look, _props.look};
  if (_target && !resetState &&
      target.subject.entity == _target->subject.entity &&
      target.subject.tick < _target->subject.tick)
    throw std::invalid_argument("Camera target time moved backward");
  const bool reset =
      resetState || !_target || !_camera ||
      target.subject.entity != _target->subject.entity ||
      target.subject.pose.position.space !=
          _target->subject.pose.position.space ||
      target.subject.discontinuity != _target->subject.discontinuity;
  const double requested = _state.requestedDistance + zoom.deltaMeters +
                           zoom.metersPerSecond * seconds;
  if (!std::isfinite(requested))
    throw std::overflow_error("Camera zoom exceeds range");
  _state.requestedDistance =
      std::clamp(requested, _props.minimumDistance, _props.maximumDistance);
  _state.requested = _props.perspective;
  if (_props.perspective == PerspectivePolicy::FirstPerson)
    _state.effective = CameraPerspective::FirstPerson;
  else if (_props.perspective == PerspectivePolicy::ThirdPerson)
    _state.effective = CameraPerspective::ThirdPerson;
  else if (_state.requestedDistance <= _props.enterDistance)
    _state.effective = CameraPerspective::FirstPerson;
  else if (_state.requestedDistance >= _props.exitDistance)
    _state.effective = CameraPerspective::ThirdPerson;
  const double wantedBlend =
      _state.effective == CameraPerspective::FirstPerson ? 1 : 0;
  _firstPersonBlend = std::lerp(_firstPersonBlend, wantedBlend,
                                response(seconds, _props.perspectiveHalfLife,
                                         reset || _props.reducedMotion));
  if (std::abs(_firstPersonBlend - wantedBlend) < 1e-5)
    _firstPersonBlend = wantedBlend;
  const auto rotation = orientation.orientation();
  const auto yawRotation =
      LookController{{look.yaw, 0}, _props.look}.orientation();
  const auto third = world::translated(
      target.followAnchor,
      world::rotate(yawRotation, _props.shoulder) -
          world::rotate(rotation, {0, 0, _state.requestedDistance}),
      _props.limits);
  const auto wanted =
      _firstPersonBlend == 1
          ? target.eyeAnchor
          : world::translated(third,
                              world::relativeTo(target.eyeAnchor, third) *
                                  _firstPersonBlend,
                              _props.limits);
  auto position = wanted;
  if (!reset && !_props.reducedMotion) {
    position =
        world::translated(_camera->pose.position,
                          world::relativeTo(wanted, _camera->pose.position) *
                              response(seconds, _props.followHalfLife, false),
                          _props.limits);
    if (world::length(world::relativeTo(wanted, position)) <=
        _props.settleMeters)
      position = wanted;
  }
  WorldCamera camera{
      {position, rotation},
      lens(_props.thirdPersonLens, _props.firstPersonLens, _firstPersonBlend),
      std::max(.001, _state.requestedDistance),
      target.subject.entity.epoch};
  camera.validate(_props.limits);
  _state.resolvedDistance =
      world::length(world::relativeTo(position, target.followAnchor));
  _state.hasTarget = true;
  _state.transitioning = _firstPersonBlend != wantedBlend || position != wanted;
  _state.obstruction = CameraObstructionStatus::Unavailable;
  _target = target;
  _camera = camera;
  _look = orientation.state();
}

void FollowCameraRig::advance(const CameraTargetSample &target, LookState look,
                              ZoomIntent zoom, double seconds) {
  auto candidate = *this;
  candidate.evaluate(target, look, zoom, seconds, false);
  *this = std::move(candidate);
}

void FollowCameraRig::reset(const CameraTargetSample &target, LookState look) {
  auto candidate = *this;
  candidate.evaluate(target, look, {}, 0, true);
  *this = std::move(candidate);
}

void FollowCameraRig::clearTarget() noexcept {
  _target.reset();
  _camera.reset();
  _state.hasTarget = _state.transitioning = false;
}

const WorldCamera &FollowCameraRig::camera() const {
  if (!_camera)
    throw std::logic_error("Follow camera has no target");
  return *_camera;
}

CameraAdoption FollowCameraRig::adoptView(const WorldCamera &view,
                                          const CameraTargetSample &target) {
  view.validate(_props.limits);
  checkTarget(target, _props.limits);
  if (view.epoch != target.subject.entity.epoch ||
      view.pose.position.space != target.followAnchor.space)
    throw std::invalid_argument(
        "Cannot adopt a view across world epochs or spaces");
  auto candidate = *this;
  const auto forward = world::rotate(view.pose.orientation, {0, 0, 1});
  LookState look{std::atan2(forward.x, forward.z),
                 std::clamp(std::asin(std::clamp(forward.y, -1., 1.)),
                            -_props.look.maximumPitch,
                            _props.look.maximumPitch)};
  const auto shoulder =
      world::rotate(LookController{{look.yaw, 0}, _props.look}.orientation(),
                    _props.shoulder);
  candidate._state.requestedDistance = std::clamp(
      world::dot(world::relativeTo(target.followAnchor, view.pose.position) +
                     shoulder,
                 forward),
      _props.minimumDistance, _props.maximumDistance);
  candidate.reset(target, look);
  const auto &resolved = candidate.camera();
  const bool exact =
      world::length(world::relativeTo(
          resolved.pose.position, view.pose.position)) <= _props.settleMeters &&
      world::length(world::rotate(resolved.pose.orientation, {0, 0, 1}) -
                    forward) < 1e-5 &&
      world::length(world::rotate(resolved.pose.orientation, {0, 1, 0}) -
                    world::rotate(view.pose.orientation, {0, 1, 0})) < 1e-5 &&
      lens(view.lens, view.lens, 0) == lens(resolved.lens, resolved.lens, 0);
  *this = std::move(candidate);
  return {look, exact};
}

CameraAdoption FollowCameraRig::returnView(CameraReturnPolicy policy,
                                           const WorldCamera &displayed,
                                           const CameraTargetSample &target,
                                           LookState saved) {
  if (policy == CameraReturnPolicy::PreserveDisplayedView)
    return adoptView(displayed, target);
  if (policy != CameraReturnPolicy::RestoreSavedView &&
      policy != CameraReturnPolicy::CutReset)
    throw std::invalid_argument("Unknown camera return policy");
  auto candidate = *this;
  candidate.reset(target, saved);
  // The director owns selection transitions. Publish the receiving rig's saved
  // target; the caller blends from its displayed view or explicitly cuts.
  *this = std::move(candidate);
  return {saved, false};
}
} // namespace playground::scene
