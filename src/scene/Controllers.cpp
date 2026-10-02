#include <algorithm>
#include <atomic>
#include <cmath>
#include <iterator>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <utility>

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

FreeCameraController::FreeCameraController(FreeCameraProps props) {
  setProps(props);
}

void FreeCameraController::setProps(FreeCameraProps props) {
  if (!math::isFinite(props.position) || !std::isfinite(props.yaw) ||
      !std::isfinite(props.pitch) || !std::isfinite(props.unitsPerSecond) ||
      props.unitsPerSecond < 0 || !std::isfinite(props.maximumPitch) ||
      props.maximumPitch <= 0 ||
      props.maximumPitch >= std::numbers::pi_v<float> / 2 ||
      std::abs(props.pitch) > props.maximumPitch)
    throw std::invalid_argument("Invalid free camera props");
  props.yaw = std::remainder(props.yaw, 2 * std::numbers::pi_v<float>);
  _props = props;
}

void FreeCameraController::advance(FreeCameraIntent intent, double seconds) {
  if (!math::isFinite(intent.movement) || !std::isfinite(intent.radians.x) ||
      !std::isfinite(intent.radians.y) || !std::isfinite(seconds) ||
      seconds < 0)
    throw std::invalid_argument("Invalid free camera intent");
  auto next = _props;
  next.yaw = static_cast<float>(std::remainder(
      static_cast<double>(next.yaw) + intent.radians.x, 2 * std::numbers::pi));
  next.pitch = static_cast<float>(
      std::clamp(static_cast<double>(next.pitch) + intent.radians.y,
                 -static_cast<double>(next.maximumPitch),
                 static_cast<double>(next.maximumPitch)));
  const math::Vec3f forward{std::sin(next.yaw) * std::cos(next.pitch),
                            std::sin(next.pitch),
                            std::cos(next.yaw) * std::cos(next.pitch)};
  const math::Vec3f right{std::cos(next.yaw), 0, -std::sin(next.yaw)};
  next.position = MovementController{{next.unitsPerSecond}}.advance(
      next.position,
      {right * intent.movement.x + math::Vec3f{0, 1, 0} * intent.movement.y +
       forward * intent.movement.z},
      seconds);
  setProps(next);
}

CameraProps FreeCameraController::camera(CameraProps lens) const {
  lens.eye = _props.position;
  lens.target =
      lens.eye + math::Vec3f{std::sin(_props.yaw) * std::cos(_props.pitch),
                             std::sin(_props.pitch),
                             std::cos(_props.yaw) * std::cos(_props.pitch)};
  lens.up = {0, 1, 0};
  lens.view(1);
  return lens;
}

namespace {
void validateCamera(const CameraProps &camera) {
  if (!std::isfinite(camera.verticalFov) || camera.verticalFov <= 0 ||
      camera.verticalFov >= std::numbers::pi_v<float>)
    throw std::invalid_argument("Invalid camera FOV");
  const auto view = camera.view(1);
  if (!math::isFinite(view.view) || !math::isFinite(view.projection))
    throw std::invalid_argument("Nonfinite camera matrices");
}

math::Quaternion orientation(const CameraProps &camera) {
  const auto z = math::normalized(camera.target - camera.eye);
  const auto x = math::normalized(math::cross(math::normalized(camera.up), z));
  const auto y = math::cross(z, x);
  // Basis columns form the camera's world rotation, inverse of its view basis.
  const float m[3][3]{{x.x, y.x, z.x}, {x.y, y.y, z.y}, {x.z, y.z, z.z}};
  const float trace = m[0][0] + m[1][1] + m[2][2];
  math::Quaternion q;
  if (trace > 0) {
    const float scale = 2 * std::sqrt(trace + 1);
    q = {(m[2][1] - m[1][2]) / scale, (m[0][2] - m[2][0]) / scale,
         (m[1][0] - m[0][1]) / scale, scale / 4};
  } else {
    int i = m[1][1] > m[0][0] ? 1 : 0;
    if (m[2][2] > m[i][i])
      i = 2;
    const int j = (i + 1) % 3, k = (j + 1) % 3;
    const float scale = 2 * std::sqrt(1 + m[i][i] - m[j][j] - m[k][k]);
    float xyz[3]{};
    xyz[i] = scale / 4;
    xyz[j] = (m[j][i] + m[i][j]) / scale;
    xyz[k] = (m[k][i] + m[i][k]) / scale;
    q = {xyz[0], xyz[1], xyz[2], (m[k][j] - m[j][k]) / scale};
  }
  return math::normalizedRotation(q);
}

math::Quaternion slerp(math::Quaternion a, math::Quaternion b, double t) {
  double cosine = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
  if (cosine < 0) {
    b = {-b.x, -b.y, -b.z, -b.w};
    cosine = -cosine;
  }
  double left = 1 - t, right = t;
  if (cosine < 0.9995) {
    const double angle = std::acos(std::clamp(cosine, 0.0, 1.0));
    left = std::sin((1 - t) * angle) / std::sin(angle);
    right = std::sin(t * angle) / std::sin(angle);
  }
  return math::normalizedRotation(
      {static_cast<float>(a.x * left + b.x * right),
       static_cast<float>(a.y * left + b.y * right),
       static_cast<float>(a.z * left + b.z * right),
       static_cast<float>(a.w * left + b.w * right)});
}

bool sameProjection(const CameraProps &a, const CameraProps &b) {
  return a.orthographicHeight.has_value() == b.orthographicHeight.has_value();
}

CameraProps blend(const CameraProps &a, const CameraProps &b, double alpha) {
  if (alpha >= 1)
    return b;
  if (alpha <= 0 || !sameProjection(a, b))
    return a;
  const double t = alpha * alpha * (3 - 2 * alpha);
  auto mix = [t](float x, float y) {
    return static_cast<float>(
        std::lerp(static_cast<double>(x), static_cast<double>(y), t));
  };
  CameraProps result = a;
  result.eye = {mix(a.eye.x, b.eye.x), mix(a.eye.y, b.eye.y),
                mix(a.eye.z, b.eye.z)};
  const auto rotation =
      math::rotation(slerp(orientation(a), orientation(b), t));
  const auto forward = math::transformDirection(rotation, {0, 0, 1});
  auto distance = [](const CameraProps &c) {
    return std::hypot(static_cast<double>(c.target.x) - c.eye.x,
                      static_cast<double>(c.target.y) - c.eye.y,
                      static_cast<double>(c.target.z) - c.eye.z);
  };
  const double focus = std::lerp(distance(a), distance(b), t);
  result.target = {static_cast<float>(result.eye.x + forward.x * focus),
                   static_cast<float>(result.eye.y + forward.y * focus),
                   static_cast<float>(result.eye.z + forward.z * focus)};
  result.up = math::transformDirection(rotation, {0, 1, 0});
  result.verticalFov = mix(a.verticalFov, b.verticalFov);
  result.nearPlane = mix(a.nearPlane, b.nearPlane);
  result.farPlane = mix(a.farPlane, b.farPlane);
  if (a.orthographicHeight)
    result.orthographicHeight =
        mix(*a.orthographicHeight, *b.orthographicHeight);
  validateCamera(result);
  return result;
}

CameraId nextCameraId() {
  static std::atomic<CameraId> next{1};
  auto id = next.load(std::memory_order_relaxed);
  do {
    if (id == std::numeric_limits<CameraId>::max())
      throw std::overflow_error("Camera identity exhausted");
  } while (!next.compare_exchange_weak(id, id + 1, std::memory_order_relaxed));
  return id;
}
} // namespace

CameraDirector::CameraDirector(CameraProps initial, double transitionSeconds)
    : _camera(initial), _from(initial), _to(initial),
      _duration(transitionSeconds) {
  validateCamera(initial);
  if (!std::isfinite(_duration) || _duration < 0)
    throw std::invalid_argument("Invalid camera transition duration");
}

void CameraDirector::transition(CameraProps target) {
  _from = _camera;
  _to = target;
  _elapsed = 0;
  _transitioning = _duration > 0 && sameProjection(_from, _to);
  if (!_transitioning)
    _camera = target;
}

void CameraDirector::resolve(bool refreshActive) {
  const Entry *winner = nullptr;
  for (const auto &entry : _sources) {
    if (!entry.source.enabled)
      continue;
    if (!winner || entry.source.priority > winner->source.priority ||
        (entry.source.priority == winner->source.priority &&
         (entry.id == _active ||
          (winner->id != _active && entry.id < winner->id))))
      winner = &entry;
  }
  const auto next = winner ? std::optional{winner->id} : std::nullopt;
  if (next == _active && !refreshActive)
    return;
  _active = next;
  if (winner)
    transition(winner->source.camera);
  else if (_fallback)
    transition(*_fallback);
  else
    _transitioning = false;
}

CameraId CameraDirector::add(CameraSource source) {
  validateCamera(source.camera);
  const auto id = nextCameraId();
  _sources.push_back({id, source});
  resolve();
  return id;
}

void CameraDirector::update(CameraId id, CameraSource source) {
  const auto entry = std::find_if(_sources.begin(), _sources.end(),
                                  [id](const Entry &e) { return e.id == id; });
  if (entry == _sources.end())
    throw std::invalid_argument("Stale or foreign camera ID");
  validateCamera(source.camera);
  entry->source = source;
  resolve(_active == id);
}

void CameraDirector::remove(CameraId id) {
  const auto entry = std::find_if(_sources.begin(), _sources.end(),
                                  [id](const Entry &e) { return e.id == id; });
  if (entry == _sources.end())
    throw std::invalid_argument("Stale or foreign camera ID");
  _sources.erase(entry);
  resolve();
}

void CameraDirector::setFallback(std::optional<CameraProps> camera) {
  if (camera)
    validateCamera(*camera);
  _fallback = camera;
  if (!_active)
    resolve(true);
}

void CameraDirector::advance(double seconds) {
  if (!std::isfinite(seconds) || seconds < 0)
    throw std::invalid_argument("Invalid camera elapsed time");
  if (!_transitioning)
    return;
  const double elapsed = _elapsed + std::min(seconds, _duration - _elapsed);
  const auto camera = blend(_from, _to, elapsed / _duration);
  _camera = camera;
  _elapsed = elapsed;
  _transitioning = elapsed < _duration;
}

CameraPath::CameraPath(double durationSeconds, std::vector<CameraPathKey> keys)
    : _duration(durationSeconds), _keys(std::move(keys)) {
  if (!std::isfinite(_duration) || _duration <= 0 || _keys.size() < 2 ||
      _keys.front().seconds != 0)
    throw std::invalid_argument("Invalid camera path duration or keys");
  double previous = -1;
  for (const auto &key : _keys) {
    if (!std::isfinite(key.seconds) || key.seconds <= previous ||
        key.seconds >= _duration)
      throw std::invalid_argument("Camera path keys must increase within loop");
    validateCamera(key.camera);
    previous = key.seconds;
  }
}

CameraProps CameraPath::sample(double elapsedSeconds) const {
  if (!std::isfinite(elapsedSeconds) || elapsedSeconds < 0)
    throw std::invalid_argument("Invalid camera path elapsed time");
  const double time = std::fmod(elapsedSeconds, _duration);
  const auto next =
      std::upper_bound(_keys.begin(), _keys.end(), time,
                       [](double seconds, const CameraPathKey &key) {
                         return seconds < key.seconds;
                       });
  const auto &from = *std::prev(next);
  const auto &to = next == _keys.end() ? _keys.front() : *next;
  const double end = next == _keys.end() ? _duration : to.seconds;
  return blend(from.camera, to.camera,
               (time - from.seconds) / (end - from.seconds));
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
