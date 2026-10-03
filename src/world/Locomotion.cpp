#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

#include <world/Locomotion.hpp>

namespace playground::world {
namespace {
void elapsed(double seconds) {
  if (!std::isfinite(seconds) || seconds < 0)
    throw std::invalid_argument("Invalid locomotion elapsed time");
}

double angle(double value) {
  auto result = std::remainder(value, 2 * std::numbers::pi);
  return result == -std::numbers::pi ? std::numbers::pi : result;
}

struct Basis {
  Vec3d right, up, forward;
};

Basis basis(Vec3d up) {
  up = normalized(up);
  auto forward = Vec3d{0, 0, 1} - up * up.z;
  if (length(forward) < 1e-6)
    forward = Vec3d{1, 0, 0} - up * up.x;
  forward = normalized(forward);
  return {normalized(cross(up, forward)), up, forward};
}

Vec3d direction(const Basis &axes, double heading) {
  return axes.right * std::sin(heading) + axes.forward * std::cos(heading);
}

double heading(const Basis &axes, Vec3d forward) {
  const auto x = dot(forward, axes.right), z = dot(forward, axes.forward);
  if (std::hypot(x, z) < 1e-9)
    throw std::invalid_argument("Subject heading is parallel to movement up");
  return std::atan2(x, z);
}

math::Vec3f narrow(Vec3d value) {
  return {float(value.x), float(value.y), float(value.z)};
}

void sample(const LocomotionState &state, const FrameSample *frame,
            const SpatialLimits &limits) {
  validate(state.entity.id);
  validate(state.pose, limits);
  validate(state.velocity);
  if (!state.entity.epoch || !state.discontinuity ||
      state.entity.id.world != state.pose.position.space.world ||
      state.velocity.space != state.pose.position.space)
    throw std::invalid_argument("Invalid locomotion subject sample");
  if (frame &&
      (frame->id.space != state.pose.position.space ||
       frame->id.epoch != state.entity.epoch || frame->tick != state.tick ||
       frame->worldVersion.epoch != state.entity.epoch))
    throw std::invalid_argument(
        "Movement frame does not match subject boundary");
  if (frame)
    validate(*frame, limits);
}

double respond(double body, double target, TurnResponse response,
               double seconds, const LocomotionProps &props) {
  const auto difference = angle(target - body);
  if (response == TurnResponse::Immediate)
    return angle(target);
  if (response == TurnResponse::RateLimited)
    return angle(body + std::clamp(difference, -props.turnRate * seconds,
                                   props.turnRate * seconds));
  return angle(body + difference * -std::expm1(-std::numbers::ln2 * seconds /
                                               props.turnHalfLife));
}
} // namespace

void LocomotionProps::validate() const {
  limits.validate();
  if (unsigned(profile) > unsigned(LocomotionProfile::Tank) ||
      unsigned(basis) > unsigned(MovementBasis::Body) ||
      unsigned(facing) > unsigned(FacingTarget::View) ||
      unsigned(response) > unsigned(TurnResponse::Damped) ||
      unsigned(movementDuringTurn) >
          unsigned(MovementDuringTurn::ScaleByHeading) ||
      unsigned(freeLookReturn) > unsigned(FreeLookReturn::EaseToView) ||
      !isFinite(up) || (!std::isfinite(length(up)) || length(up) == 0) ||
      !std::isfinite(speed) || speed < 0 || !std::isfinite(turnRate) ||
      turnRate <= 0 || !std::isfinite(turnHalfLife) || turnHalfLife <= 0 ||
      !std::isfinite(returnHalfLife) || returnHalfLife <= 0 ||
      (profile == LocomotionProfile::Steered && basis == MovementBasis::Body &&
       facing == FacingTarget::Travel))
    throw std::invalid_argument(
        "Invalid locomotion profile/response combination");
}

LocomotionProps LocomotionProps::responsive() {
  LocomotionProps props;
  props.basis = MovementBasis::View;
  props.facing = FacingTarget::Travel;
  props.response = TurnResponse::Immediate;
  return props;
}

LocomotionProps LocomotionProps::outOfNowhere() { return {}; }

CharacterFacingController::CharacterFacingController(LocomotionProps props) {
  setProps(props);
}

void CharacterFacingController::setProps(LocomotionProps props) {
  props.validate();
  if (props.up != _props.up)
    reset();
  _props = props;
}

void CharacterFacingController::reset() noexcept {
  _subject.reset();
  _freeLook = _returning = _heldReference = false;
}

LocomotionRequest
CharacterFacingController::advance(const LocomotionState &state,
                                   LocomotionIntent intent, double seconds,
                                   const FrameSample *frame) {
  auto staged = *this;
  auto result = staged.evaluate(state, intent, seconds, frame);
  *this = std::move(staged);
  return result;
}

LocomotionRequest
CharacterFacingController::evaluate(const LocomotionState &state,
                                    LocomotionIntent intent, double seconds,
                                    const FrameSample *frame) {
  sample(state, frame, _props.limits);
  elapsed(seconds);
  if (!std::isfinite(intent.right) || !std::isfinite(intent.forward) ||
      !std::isfinite(intent.turn) || !std::isfinite(intent.viewHeading) ||
      std::abs(intent.right) > 1 || std::abs(intent.forward) > 1 ||
      std::abs(intent.turn) > 1)
    throw std::invalid_argument("Nonfinite locomotion intent");
  if (_subject == state.entity && state.tick < _tick)
    throw std::invalid_argument("Locomotion tick moved backwards");
  intent.viewHeading = angle(intent.viewHeading);
  const auto axes = basis(_props.up);
  auto forward = rotate(state.pose.orientation, {0, 0, 1});
  if (frame)
    forward = inverseRotate(frame->pose.orientation, forward);
  const auto body = heading(axes, forward);
  const auto frameId = frame ? std::optional{frame->id} : std::nullopt;
  if (_subject != state.entity || _discontinuity != state.discontinuity ||
      _frame != frameId) {
    _reference = _previousView = intent.viewHeading;
    _freeLook = _returning = _heldReference = false;
  }
  if (intent.freeLook && !_freeLook) {
    _reference = _previousView;
    _returning = _heldReference = false;
  }
  if (!intent.freeLook && _freeLook) {
    _heldReference = _props.freeLookReturn == FreeLookReturn::KeepReference;
    _returning = !_heldReference;
  }
  if (intent.recenter) {
    _heldReference = false;
    _returning = true;
  }
  if (!intent.freeLook && !_heldReference) {
    if (_returning) {
      _reference =
          angle(_reference + angle(intent.viewHeading - _reference) *
                                 -std::expm1(-std::numbers::ln2 * seconds /
                                             _props.returnHalfLife));
      if (std::abs(angle(intent.viewHeading - _reference)) < 1e-6)
        _returning = false;
    } else
      _reference = intent.viewHeading;
  }
  const auto view = intent.aim ? intent.viewHeading : _reference;
  const auto profile = intent.aim ? LocomotionProfile::Strafe : _props.profile;
  const auto magnitude = std::hypot(intent.right, intent.forward);
  if (magnitude > 1) {
    intent.right /= magnitude;
    intent.forward /= magnitude;
  }
  const auto movementBasis = profile == LocomotionProfile::Strafe  ? view
                             : _props.basis == MovementBasis::View ? view
                                                                   : body;
  Vec3d travel;
  double target = body, resolved = body;
  if (profile == LocomotionProfile::Tank) {
    const auto turn = std::clamp(intent.right + intent.turn, -1.0, 1.0);
    target = angle(body + turn * _props.turnRate * seconds);
    resolved = target;
    travel = direction(axes, body) * intent.forward;
  } else {
    const auto front = direction(axes, movementBasis),
               right = cross(axes.up, front);
    travel = right * intent.right + front * intent.forward;
    if (profile == LocomotionProfile::Strafe || intent.aim || intent.recenter)
      target = view;
    else if (length(travel) > 1e-9)
      target =
          _props.facing == FacingTarget::View ? view : heading(axes, travel);
    else if (intent.turn != 0)
      target = angle(body + std::clamp(intent.turn, -1.0, 1.0) *
                                _props.turnRate * seconds);
    resolved = respond(body, target, _props.response, seconds, _props);
  }
  auto speed = _props.speed;
  if (_props.movementDuringTurn == MovementDuringTurn::ScaleByHeading &&
      profile == LocomotionProfile::Steered)
    speed *= std::max(0.0, std::cos(angle(target - body)));
  LocomotionRequest request;
  request.entity = state.entity;
  request.tick = state.tick;
  request.discontinuity = state.discontinuity;
  request.desiredHeading = target;
  request.resolvedHeading = resolved;
  request.localVelocity = travel * speed;
  request.localAngular =
      axes.up * (seconds > 0 ? angle(resolved - body) / seconds : 0);
  request.orientation =
      math::lookRotation(narrow(direction(axes, resolved)), narrow(axes.up));
  request.velocity = {state.pose.position.space, request.localVelocity,
                      request.localAngular};
  request.inheritFrameVelocity = _props.inheritFrameVelocity;
  if (frame) {
    request.frame = *frame;
    request.orientation =
        math::normalizedRotation(frame->pose.orientation * request.orientation);
    request.velocity.linear =
        rotate(frame->pose.orientation, request.localVelocity);
    request.velocity.angular =
        rotate(frame->pose.orientation, request.localAngular);
    if (_props.inheritFrameVelocity) {
      const auto offset = relativePose(state.pose, frame->pose).offset;
      request.velocity.linear =
          request.velocity.linear +
          attachedVelocity(frame->pose, frame->velocity, offset).linear;
      request.velocity.angular =
          request.velocity.angular + frame->velocity.angular;
    }
  }
  validate(request.velocity);
  _subject = state.entity;
  _frame = frameId;
  _discontinuity = state.discontinuity;
  _tick = state.tick;
  _freeLook = intent.freeLook;
  _previousView = intent.viewHeading;
  return request;
}

LocomotionIntent travelIntent(const LocomotionState &state, Vec3d desired,
                              std::optional<double> facing,
                              const LocomotionProps &props, double seconds,
                              const FrameSample *frame) {
  props.validate();
  sample(state, frame, props.limits);
  elapsed(seconds);
  if (!isFinite(desired) || (facing && !std::isfinite(*facing)))
    throw std::invalid_argument("Invalid autonomous travel intent");
  const auto axes = basis(props.up);
  desired = desired - axes.up * dot(desired, axes.up);
  const auto speed = length(desired);
  if (!std::isfinite(speed))
    throw std::invalid_argument("Autonomous travel magnitude exceeds range");
  auto bodyForward = rotate(state.pose.orientation, {0, 0, 1});
  if (frame)
    bodyForward = inverseRotate(frame->pose.orientation, bodyForward);
  const auto body = heading(axes, bodyForward);
  if (speed < 1e-9 || props.speed == 0)
    return {.viewHeading = facing.value_or(body)};
  const auto target = heading(axes, desired);
  const auto amount = std::min(1.0, speed / props.speed);
  if (props.profile == LocomotionProfile::Tank)
    return {.forward = amount * std::max(0.0, std::cos(angle(target - body))),
            .turn = seconds > 0 ? std::clamp(angle(target - body) /
                                                 (props.turnRate * seconds),
                                             -1.0, 1.0)
                                : 0,
            .viewHeading = facing.value_or(target)};
  if (props.profile == LocomotionProfile::Steered)
    return {.forward = amount, .viewHeading = target};
  const auto view = facing.value_or(target);
  const auto front = direction(axes, view), right = cross(axes.up, front);
  return {.right = dot(desired * (1 / speed), right) * amount,
          .forward = dot(desired * (1 / speed), front) * amount,
          .viewHeading = view};
}

void KinematicMotorProps::validate() const {
  if (!std::isfinite(maximumStepSeconds) || maximumStepSeconds <= 0 ||
      !std::isfinite(maximumSpeed) || maximumSpeed <= 0 ||
      !std::isfinite(maximumAngularSpeed) || maximumAngularSpeed <= 0)
    throw std::invalid_argument("Invalid kinematic motor limits");
}

MovementResult realizeMovement(const LocomotionState &state,
                               const LocomotionRequest &request, double seconds,
                               MovementRealization mode,
                               KinematicMotorProps props,
                               SpatialLimits limits) {
  props.validate();
  limits.validate();
  elapsed(seconds);
  sample(state, request.frame ? &*request.frame : nullptr, limits);
  if (unsigned(mode) > unsigned(MovementRealization::RootMotion) ||
      request.entity != state.entity || request.tick != state.tick ||
      request.discontinuity != state.discontinuity ||
      request.velocity.space != state.pose.position.space)
    throw std::invalid_argument("Stale/invalid movement realization");
  if (mode != MovementRealization::Kinematic)
    return {MovementStatus::Unsupported, state};
  validate(request.velocity);
  if (!request.frame && (request.velocity.linear != request.localVelocity ||
                         request.velocity.angular != request.localAngular))
    throw std::invalid_argument("Unframed movement rates must agree");
  const auto orientation = math::normalizedRotation(request.orientation);
  if (seconds > props.maximumStepSeconds || !isFinite(request.localVelocity) ||
      !isFinite(request.localAngular) ||
      length(request.localVelocity) > props.maximumSpeed ||
      length(request.localAngular) > props.maximumAngularSpeed)
    throw std::length_error("Kinematic request exceeds step/rate limits");
  MovementResult result{MovementStatus::Applied, state};
  if (seconds > 0) {
    const auto velocity =
        request.frame
            ? rotate(request.frame->pose.orientation, request.localVelocity)
            : request.velocity.linear;
    result.actual.pose.position =
        translated(state.pose.position, velocity * seconds, limits);
    result.actual.pose.orientation = orientation;
    result.actual.velocity = request.velocity;
  }
  if (request.frame) {
    const auto &frame = *request.frame;
    result.attachment =
        attach(frame, result.actual.pose, result.actual.velocity,
               FrameVelocityPolicy::PreserveWorld, limits);
    if (seconds > 0 && request.inheritFrameVelocity) {
      result.attachment->linear = request.localVelocity;
      result.attachment->angular = request.localAngular;
      result.actual.velocity =
          resolve(*result.attachment, frame, limits).velocity;
    }
  }
  return result;
}
} // namespace playground::world
