#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

#include <scene/Controllers.hpp>

namespace playground::scene {
MovementController::MovementController(MovementProps props) { setProps(props); }

void MovementController::setProps(MovementProps props) {
  if (!std::isfinite(props.unitsPerSecond) || props.unitsPerSecond < 0)
    throw std::invalid_argument(
        "Movement speed must be finite and nonnegative");
  _props = props;
}

math::Vec3f MovementController::advance(math::Vec3f position,
                                        MovementIntent intent,
                                        double seconds) const {
  if (!math::isFinite(position) || !math::isFinite(intent.direction) ||
      !std::isfinite(seconds) || seconds < 0)
    throw std::invalid_argument("Invalid movement input");
  auto direction = intent.direction;
  const double length = std::hypot(static_cast<double>(direction.x),
                                   static_cast<double>(direction.y),
                                   static_cast<double>(direction.z));
  if (length > 1)
    direction = direction * static_cast<float>(1 / length);
  auto result = position +
                direction * static_cast<float>(_props.unitsPerSecond * seconds);
  if (!math::isFinite(result))
    throw std::overflow_error("Movement exceeds coordinate range");
  return result;
}

OrbitController::OrbitController(OrbitProps props) { setProps(props); }

void OrbitController::setProps(OrbitProps props) {
  if (!math::isFinite(props.target) || !std::isfinite(props.yaw) ||
      !std::isfinite(props.pitch) || !std::isfinite(props.distance) ||
      !std::isfinite(props.minimumDistance) ||
      !std::isfinite(props.maximumDistance) ||
      !std::isfinite(props.maximumPitch) || props.minimumDistance <= 0 ||
      props.maximumDistance < props.minimumDistance ||
      props.distance < props.minimumDistance ||
      props.distance > props.maximumDistance || props.maximumPitch <= 0 ||
      props.maximumPitch >= std::numbers::pi_v<float> / 2 ||
      std::abs(props.pitch) > props.maximumPitch)
    throw std::invalid_argument("Invalid orbit camera props");
  props.yaw = std::remainder(props.yaw, 2 * std::numbers::pi_v<float>);
  _props = props;
}

void OrbitController::update(OrbitIntent intent) {
  if (!std::isfinite(intent.radians.x) || !std::isfinite(intent.radians.y) ||
      !std::isfinite(intent.zoom))
    throw std::invalid_argument("Invalid orbit intent");
  auto next = _props;
  next.yaw = static_cast<float>(std::remainder(
      static_cast<double>(next.yaw) + intent.radians.x, 2 * std::numbers::pi));
  next.pitch = static_cast<float>(
      std::clamp(static_cast<double>(next.pitch) + intent.radians.y,
                 -static_cast<double>(next.maximumPitch),
                 static_cast<double>(next.maximumPitch)));
  next.distance = static_cast<float>(
      std::clamp(static_cast<double>(next.distance) + intent.zoom,
                 static_cast<double>(next.minimumDistance),
                 static_cast<double>(next.maximumDistance)));
  setProps(next);
}

CameraProps OrbitController::camera(CameraProps lens) const {
  const float horizontal = std::cos(_props.pitch) * _props.distance;
  lens.target = _props.target;
  lens.eye = lens.target + math::Vec3f{std::sin(_props.yaw) * horizontal,
                                       std::sin(_props.pitch) * _props.distance,
                                       -std::cos(_props.yaw) * horizontal};
  lens.up = {0, 1, 0};
  lens.view(
      1); // Validate lens and numerical camera separation before publication.
  return lens;
}

namespace {
math::Transform3D checked(math::Transform3D pose) {
  pose.matrix();
  pose.orientation = math::normalizedRotation(pose.orientation);
  return pose;
}
} // namespace

PoseHistory::PoseHistory(math::Transform3D initial) { teleport(initial); }

void PoseHistory::publish(math::Transform3D pose) {
  pose = checked(pose);
  _previous = _current;
  _current = pose;
}

void PoseHistory::teleport(math::Transform3D pose) {
  pose = checked(pose);
  _previous = _current = pose;
}

math::Transform3D PoseHistory::sample(double alpha) const {
  if (!std::isfinite(alpha) || alpha < 0 || alpha > 1)
    throw std::invalid_argument("Interpolation alpha must be in [0,1]");
  const float t = static_cast<float>(alpha);
  auto mix = [t](math::Vec3f a, math::Vec3f b) {
    return math::Vec3f{std::lerp(a.x, b.x, t), std::lerp(a.y, b.y, t),
                       std::lerp(a.z, b.z, t)};
  };
  const auto a = _previous.orientation;
  auto b = _current.orientation;
  if (a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w < 0)
    b = {-b.x, -b.y, -b.z, -b.w};
  return {.position = mix(_previous.position, _current.position),
          .orientation = math::normalizedRotation(
              {std::lerp(a.x, b.x, t), std::lerp(a.y, b.y, t),
               std::lerp(a.z, b.z, t), std::lerp(a.w, b.w, t)}),
          .scale = mix(_previous.scale, _current.scale)};
}
} // namespace playground::scene
