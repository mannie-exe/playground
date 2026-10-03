#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

#include <scene/Controllers.hpp>

namespace playground::scene {
namespace {
void elapsed(double seconds) {
  if (!std::isfinite(seconds) || seconds < 0)
    throw std::invalid_argument("Invalid camera elapsed time");
}

double angle(double value) {
  return std::remainder(value, 2 * std::numbers::pi);
}

CameraProps lensOnly(CameraProps camera) {
  camera.eye = {};
  camera.target = {0, 0, 1};
  camera.up = {0, 1, 0};
  camera.view(1);
  return camera;
}

world::WorldPose interpolate(world::WorldPose a, world::WorldPose b, double t) {
  if (a.position.space != b.position.space)
    throw std::invalid_argument("Cannot interpolate different spaces");
  const auto x = a.position.meters, y = b.position.meters;
  return {{a.position.space,
           {std::lerp(x.x, y.x, t), std::lerp(x.y, y.y, t),
            std::lerp(x.z, y.z, t)}},
          math::slerp(a.orientation, b.orientation, t)};
}

bool compatible(const WorldCamera &a, const WorldCamera &b) {
  return a.pose.position.space == b.pose.position.space && a.epoch == b.epoch &&
         a.lens.orthographicHeight.has_value() ==
             b.lens.orthographicHeight.has_value();
}

WorldCamera blend(const WorldCamera &a, const WorldCamera &b, double alpha,
                  const world::SpatialLimits &limits) {
  if (alpha >= 1)
    return b;
  if (alpha <= 0 || !compatible(a, b))
    return a;
  const auto t = alpha * alpha * (3 - 2 * alpha);
  WorldCamera result = b;
  result.pose = interpolate(a.pose, b.pose, t);
  result.focusDistance = std::lerp(a.focusDistance, b.focusDistance, t);
  const auto mix = [t](float x, float y) {
    return float(std::lerp(double(x), double(y), t));
  };
  result.lens.verticalFov = mix(a.lens.verticalFov, b.lens.verticalFov);
  result.lens.nearPlane = mix(a.lens.nearPlane, b.lens.nearPlane);
  result.lens.farPlane = mix(a.lens.farPlane, b.lens.farPlane);
  if (a.lens.orthographicHeight)
    result.lens.orthographicHeight =
        mix(*a.lens.orthographicHeight, *b.lens.orthographicHeight);
  result.validate(limits);
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

PoseSample checked(world::EntitySample value,
                   const world::SpatialLimits &limits) {
  world::validate(value.entity.id);
  world::validate(value.pose, limits);
  world::validate(value.velocity);
  if (!value.entity.epoch || !value.discontinuity ||
      value.entity.id.world != value.pose.position.space.world ||
      value.velocity.space != value.pose.position.space)
    throw std::invalid_argument("Invalid pose history sample");
  value.pose.orientation = math::normalizedRotation(value.pose.orientation);
  return {value.entity, value.pose, value.tick, value.discontinuity};
}
} // namespace

MovementController::MovementController(MovementProps props) { setProps(props); }

void MovementController::setProps(MovementProps props) {
  if (!std::isfinite(props.unitsPerSecond) || props.unitsPerSecond < 0)
    throw std::invalid_argument(
        "Movement speed must be finite and nonnegative");
  _props = props;
}

world::WorldPosition
MovementController::advance(world::WorldPosition position,
                            MovementIntent intent, double seconds,
                            const world::SpatialLimits &limits) const {
  elapsed(seconds);
  limits.validate(position);
  const auto length = world::length(intent.direction);
  if (!world::isFinite(intent.direction) || !std::isfinite(length))
    throw std::invalid_argument("Invalid movement direction");
  if (length > 1)
    intent.direction = intent.direction * (1 / length);
  return world::translated(
      position, intent.direction * (_props.unitsPerSecond * seconds), limits);
}

LookController::LookController(LookState state, LookProps props)
    : _props{props} {
  if (!std::isfinite(props.maximumPitch) || props.maximumPitch <= 0 ||
      props.maximumPitch >= std::numbers::pi / 2)
    throw std::invalid_argument("Invalid look pitch limit");
  setState(state);
}

void LookController::setState(LookState state) {
  if (!std::isfinite(state.yaw) || !std::isfinite(state.pitch) ||
      std::abs(state.pitch) > _props.maximumPitch)
    throw std::invalid_argument("Invalid look state");
  state.yaw = angle(state.yaw);
  _state = state;
}

void LookController::advance(LookIntent intent, double seconds) {
  elapsed(seconds);
  if (!math::isFinite(intent.deltaRadians) ||
      !math::isFinite(intent.radiansPerSecond))
    throw std::invalid_argument("Invalid look intent");
  const auto yaw = _state.yaw + intent.deltaRadians.x +
                   double(intent.radiansPerSecond.x) * seconds;
  const auto pitch = _state.pitch + intent.deltaRadians.y +
                     double(intent.radiansPerSecond.y) * seconds;
  if (!std::isfinite(yaw) || !std::isfinite(pitch))
    throw std::overflow_error("Look input exceeds range");
  setState({angle(yaw),
            std::clamp(pitch, -_props.maximumPitch, _props.maximumPitch)});
}

math::Quaternion LookController::orientation() const {
  return math::lookRotation(
      {float(std::sin(_state.yaw) * std::cos(_state.pitch)),
       float(std::sin(_state.pitch)),
       float(std::cos(_state.yaw) * std::cos(_state.pitch))});
}

OrbitController::OrbitController(OrbitProps props) { setProps(props); }

void OrbitController::setProps(OrbitProps props) {
  props.limits.validate(props.target);
  LookController{{props.yaw, props.pitch}, {props.maximumPitch}};
  if (!props.epoch || !std::isfinite(props.distance) ||
      !std::isfinite(props.minimumDistance) ||
      !std::isfinite(props.maximumDistance) || props.minimumDistance <= 0 ||
      props.maximumDistance < props.minimumDistance ||
      props.distance < props.minimumDistance ||
      props.distance > props.maximumDistance)
    throw std::invalid_argument("Invalid orbit camera distance/epoch");
  props.yaw = angle(props.yaw);
  props.lens = lensOnly(props.lens);
  _props = props;
}

void OrbitController::update(OrbitIntent intent) {
  if (!std::isfinite(intent.zoom) || !std::isfinite(intent.zoomLog))
    throw std::invalid_argument("Invalid orbit zoom");
  auto next = _props;
  LookController look{{next.yaw, next.pitch}, {next.maximumPitch}};
  look.advance({intent.radians, {}}, 0);
  next.yaw = look.state().yaw;
  next.pitch = look.state().pitch;
  next.distance = std::clamp(next.distance + intent.zoom, next.minimumDistance,
                             next.maximumDistance);
  const auto scale = [&](double value) {
    const auto logarithm = std::log(value) + intent.zoomLog;
    if (logarithm <= std::log(next.minimumDistance))
      return next.minimumDistance;
    if (logarithm >= std::log(next.maximumDistance))
      return next.maximumDistance;
    return std::clamp(std::exp(logarithm), next.minimumDistance,
                      next.maximumDistance);
  };
  if (intent.zoomLog != 0) {
    if (next.lens.orthographicHeight)
      next.lens.orthographicHeight =
          float(scale(*next.lens.orthographicHeight));
    else
      next.distance = scale(next.distance);
  }
  setProps(next);
}

void OrbitController::pan(math::Vec2f pixels, double height) {
  if (!math::isFinite(pixels) || !std::isfinite(height) || height <= 0)
    throw std::invalid_argument("Invalid inspection pan extent/displacement");
  const double visible =
      _props.lens.orthographicHeight
          ? double(*_props.lens.orthographicHeight)
          : 2 * _props.distance * std::tan(_props.lens.verticalFov / 2);
  auto next = _props;
  next.target =
      world::translated(next.target,
                        world::rotate(camera().pose.orientation,
                                      {-double(pixels.x) * visible / height,
                                       double(pixels.y) * visible / height, 0}),
                        next.limits);
  setProps(next);
}

WorldCamera OrbitController::camera() const { return camera(_props.lens); }

WorldCamera OrbitController::camera(CameraProps lens) const {
  const auto horizontal = std::cos(_props.pitch) * _props.distance;
  const world::Vec3d offset{std::sin(_props.yaw) * horizontal,
                            std::sin(_props.pitch) * _props.distance,
                            -std::cos(_props.yaw) * horizontal};
  const auto orientation = LookController{
      {-_props.yaw, -_props.pitch},
      {_props.maximumPitch}}.orientation();
  WorldCamera result{
      {world::translated(_props.target, offset, _props.limits), orientation},
      lensOnly(lens),
      _props.distance,
      _props.epoch};
  result.validate(_props.limits);
  return result;
}

FreeCameraController::FreeCameraController(FreeCameraProps props) {
  setProps(props);
}

void FreeCameraController::setProps(FreeCameraProps props) {
  props.limits.validate(props.position);
  LookController{{props.yaw, props.pitch}, {props.maximumPitch}};
  if (!props.epoch || !std::isfinite(props.unitsPerSecond) ||
      props.unitsPerSecond < 0)
    throw std::invalid_argument("Invalid free camera speed/epoch");
  props.yaw = angle(props.yaw);
  _props = props;
}

void FreeCameraController::advance(FreeCameraIntent intent, double seconds) {
  elapsed(seconds);
  if (!world::isFinite(intent.movement))
    throw std::invalid_argument("Invalid free camera movement");
  auto next = _props;
  LookController look{{next.yaw, next.pitch}, {next.maximumPitch}};
  look.advance({intent.radians, {}}, seconds);
  next.yaw = look.state().yaw;
  next.pitch = look.state().pitch;
  const auto forward = world::rotate(look.orientation(), {0, 0, 1});
  const world::Vec3d right{std::cos(next.yaw), 0, -std::sin(next.yaw)};
  next.position = MovementController{{next.unitsPerSecond}}.advance(
      next.position,
      {right * intent.movement.x + world::Vec3d{0, 1, 0} * intent.movement.y +
       forward * intent.movement.z},
      seconds, next.limits);
  setProps(next);
}

WorldCamera FreeCameraController::camera(CameraProps lens) const {
  WorldCamera result{
      {_props.position,
       LookController{{_props.yaw, _props.pitch}, {_props.maximumPitch}}
           .orientation()},
      lensOnly(lens),
      1,
      _props.epoch};
  result.validate(_props.limits);
  return result;
}

CameraDirector::CameraDirector(WorldCamera initial, double seconds,
                               world::SpatialLimits limits)
    : _camera{initial}, _from{initial}, _to{initial}, _limits{limits} {
  initial.validate(_limits);
  setTransitionProps({seconds});
}

void CameraDirector::setTransitionProps(CameraTransitionProps props) {
  if (!std::isfinite(props.seconds) || props.seconds < 0)
    throw std::invalid_argument("Invalid camera transition duration");
  _props = props;
}

void CameraDirector::transition(WorldCamera target) {
  _from = _camera;
  _to = target;
  _elapsed = 0;
  _duration = _props.reducedMotion ? 0 : _props.seconds;
  _transitioning = _duration > 0 && compatible(_from, _to);
  if (!_transitioning)
    _camera = target;
}

void CameraDirector::resolve(bool refresh) {
  const Entry *winner{};
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
  const auto target = winner ? std::optional{winner->source.camera} : _fallback;
  if (next != _active) {
    _active = next;
    if (target)
      transition(*target);
    else {
      _to = _camera;
      _transitioning = false;
    }
  } else if (refresh && target) {
    if (!compatible(_camera, *target)) {
      _camera = _to = *target;
      _transitioning = false;
    } else {
      _to = *target;
      _camera = _transitioning
                    ? blend(_from, _to, _elapsed / _duration, _limits)
                    : _to;
    }
  } else if (!target) {
    _to = _camera;
    _transitioning = false;
  }
}

CameraId CameraDirector::add(CameraSource source) {
  source.camera.validate(_limits);
  auto id = nextCameraId();
  _sources.push_back({id, source});
  resolve();
  return id;
}

void CameraDirector::update(CameraId id, CameraSource source) {
  const auto entry = std::find_if(_sources.begin(), _sources.end(),
                                  [&](const auto &e) { return e.id == id; });
  if (entry == _sources.end())
    throw std::invalid_argument("Stale or foreign camera ID");
  source.camera.validate(_limits);
  entry->source = source;
  resolve(_active == id);
}

void CameraDirector::remove(CameraId id) {
  const auto entry = std::find_if(_sources.begin(), _sources.end(),
                                  [&](const auto &e) { return e.id == id; });
  if (entry == _sources.end())
    throw std::invalid_argument("Stale or foreign camera ID");
  _sources.erase(entry);
  resolve();
}

void CameraDirector::setFallback(std::optional<WorldCamera> camera) {
  if (camera)
    camera->validate(_limits);
  if (_fallback == camera)
    return;
  _fallback = camera;
  if (!_active) {
    if (camera)
      transition(*camera);
    else {
      _to = _camera;
      _transitioning = false;
    }
  }
}

void CameraDirector::restartTransition() { transition(_to); }

void CameraDirector::cut() {
  _camera = _to;
  _transitioning = false;
  _elapsed = _duration;
}

void CameraDirector::advance(double seconds) {
  elapsed(seconds);
  if (!_transitioning)
    return;
  _elapsed += std::min(seconds, _duration - _elapsed);
  _camera = blend(_from, _to, _elapsed / _duration, _limits);
  _transitioning = _elapsed < _duration;
}

CameraPath::CameraPath(double duration, std::vector<CameraPathKey> keys,
                       world::SpatialLimits limits)
    : _duration{duration}, _keys{std::move(keys)}, _limits{limits} {
  if (!std::isfinite(duration) || duration <= 0 || _keys.size() < 2 ||
      _keys.size() > 65536 || _keys.front().seconds != 0)
    throw std::invalid_argument("Invalid camera path duration or keys");
  double previous = -1;
  const auto &first = _keys.front().camera;
  for (const auto &key : _keys) {
    if (!std::isfinite(key.seconds) || key.seconds <= previous ||
        key.seconds >= duration || key.camera.epoch != first.epoch ||
        key.camera.pose.position.space != first.pose.position.space)
      throw std::invalid_argument(
          "Camera path crosses time, epoch or space bounds");
    key.camera.validate(_limits);
    previous = key.seconds;
  }
}

WorldCamera CameraPath::sample(double seconds) const {
  elapsed(seconds);
  const auto time = std::fmod(seconds, _duration);
  const auto next = std::upper_bound(
      _keys.begin(), _keys.end(), time,
      [](double t, const auto &key) { return t < key.seconds; });
  const auto &from = *std::prev(next),
             &to = next == _keys.end() ? _keys.front() : *next;
  const auto end = next == _keys.end() ? _duration : to.seconds;
  return blend(from.camera, to.camera,
               (time - from.seconds) / (end - from.seconds), _limits);
}

PoseHistory::PoseHistory(world::EntitySample initial,
                         world::SpatialLimits limits)
    : _limits{limits} {
  teleport(initial);
}

void PoseHistory::publish(world::EntitySample value) {
  const auto pose = checked(value, _limits);
  if (pose.entity != _current.entity ||
      pose.pose.position.space != _current.pose.position.space ||
      pose.discontinuity != _current.discontinuity) {
    _previous = _current = pose;
    return;
  }
  if (pose.tick <= _current.tick)
    throw std::invalid_argument("Pose history requires a new simulation tick");
  _previous = _current;
  _current = pose;
}

void PoseHistory::teleport(world::EntitySample value) {
  _previous = _current = checked(value, _limits);
}

PoseSample PoseHistory::sample(double alpha) const {
  if (!std::isfinite(alpha) || alpha < 0 || alpha > 1)
    throw std::invalid_argument("Interpolation alpha must be in [0,1]");
  auto result = _current;
  result.pose = interpolate(_previous.pose, _current.pose, alpha);
  return result;
}
} // namespace playground::scene
