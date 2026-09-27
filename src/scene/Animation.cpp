#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

#include <scene/Animation.hpp>

namespace playground::scene {
Playback::Playback(PlaybackProps props) { setProps(props); }

void Playback::setProps(PlaybackProps props) {
  if (!std::isfinite(props.rate) ||
      (props.mode != PlaybackMode::Loop && props.mode != PlaybackMode::Clamp))
    throw std::invalid_argument("Invalid playback properties");
  _props = props;
}

void Playback::seek(double seconds) {
  if (!std::isfinite(seconds))
    throw std::invalid_argument("Nonfinite playback time");
  _time = seconds;
}

void Playback::advance(double seconds) {
  if (!std::isfinite(seconds) || seconds < 0)
    throw std::invalid_argument("Invalid playback elapsed time");
  if (!_paused)
    seek(_time + seconds * _props.rate);
}

double Playback::sampleTime(double duration) const {
  if (!std::isfinite(duration) || duration < 0)
    throw std::invalid_argument("Invalid clip duration");
  if (duration == 0)
    return 0;
  if (_props.mode == PlaybackMode::Clamp)
    return std::clamp(_time, 0., duration);
  auto t = std::fmod(_time, duration);
  return t < 0 ? t + duration : t;
}

void TransformTrack::validate(std::size_t nodeCount) const {
  if (node >= nodeCount || times.empty() ||
      (path != TrackPath::Translation && path != TrackPath::Rotation &&
       path != TrackPath::Scale) ||
      (interpolation != TrackInterpolation::Step &&
       interpolation != TrackInterpolation::Linear &&
       interpolation != TrackInterpolation::CubicSpline) ||
      values.size() /
              (interpolation == TrackInterpolation::CubicSpline ? 3 : 1) !=
          times.size() ||
      values.size() %
          (interpolation == TrackInterpolation::CubicSpline ? 3 : 1))
    throw std::invalid_argument("Invalid animation track");
  for (std::size_t i = 0; i < times.size(); ++i) {
    if (!std::isfinite(times[i]) || times[i] < 0 ||
        (i && times[i] <= times[i - 1]))
      throw std::invalid_argument("Animation keys must increase strictly");
    const auto v =
        values[interpolation == TrackInterpolation::CubicSpline ? i * 3 + 1
                                                                : i];
    if (path == TrackPath::Rotation)
      math::normalizedRotation({v.x, v.y, v.z, v.w});
  }
  for (auto v : values)
    if (!math::isFinite(v))
      throw std::invalid_argument("Nonfinite animation key");
}

math::Vec4f TransformTrack::sample(double seconds) const {
  if (!std::isfinite(seconds) || times.empty())
    throw std::invalid_argument("Invalid animation sampling");
  const auto value = [&](std::size_t i) {
    auto v = values.at(
        interpolation == TrackInterpolation::CubicSpline ? i * 3 + 1 : i);
    if (path == TrackPath::Rotation) {
      const auto q = math::normalizedRotation({v.x, v.y, v.z, v.w});
      v = {q.x, q.y, q.z, q.w};
    }
    return v;
  };
  auto hi = std::upper_bound(times.begin(), times.end(), seconds);
  if (hi == times.begin())
    return value(0);
  if (hi == times.end())
    return value(times.size() - 1);
  const auto b = std::size_t(hi - times.begin()), a = b - 1;
  auto x = value(a), y = value(b);
  if (interpolation == TrackInterpolation::Step)
    return x;
  const float dt = times[b] - times[a], t = float((seconds - times[a]) / dt);
  math::Vec4f result;
  if (interpolation == TrackInterpolation::CubicSpline) {
    const auto out = values.at(a * 3 + 2), in = values.at(b * 3);
    const float t2 = t * t, t3 = t2 * t, h0 = 2 * t3 - 3 * t2 + 1,
                h1 = t3 - 2 * t2 + t, h2 = -2 * t3 + 3 * t2, h3 = t3 - t2;
    result = {h0 * x.x + h1 * dt * out.x + h2 * y.x + h3 * dt * in.x,
              h0 * x.y + h1 * dt * out.y + h2 * y.y + h3 * dt * in.y,
              h0 * x.z + h1 * dt * out.z + h2 * y.z + h3 * dt * in.z,
              h0 * x.w + h1 * dt * out.w + h2 * y.w + h3 * dt * in.w};
  } else {
    float aWeight = 1 - t, bWeight = t;
    if (path == TrackPath::Rotation) {
      const auto q0 = math::normalizedRotation({x.x, x.y, x.z, x.w}),
                 q1 = math::normalizedRotation({y.x, y.y, y.z, y.w});
      x = {q0.x, q0.y, q0.z, q0.w};
      y = {q1.x, q1.y, q1.z, q1.w};
      float dot = x.x * y.x + x.y * y.y + x.z * y.z + x.w * y.w;
      if (dot < 0) {
        y = {-y.x, -y.y, -y.z, -y.w};
        dot = -dot;
      }
      if (dot < .9995f) {
        const float angle = std::acos(std::clamp(dot, 0.f, 1.f));
        aWeight = std::sin((1 - t) * angle) / std::sin(angle);
        bWeight = std::sin(t * angle) / std::sin(angle);
      }
    }
    result = {x.x * aWeight + y.x * bWeight, x.y * aWeight + y.y * bWeight,
              x.z * aWeight + y.z * bWeight, x.w * aWeight + y.w * bWeight};
  }
  if (path == TrackPath::Rotation) {
    const auto q =
        math::normalizedRotation({result.x, result.y, result.z, result.w});
    result = {q.x, q.y, q.z, q.w};
  }
  return result;
}

void AnimationClip::validate(std::size_t nodeCount) const {
  if (!std::isfinite(duration) || duration < 0)
    throw std::invalid_argument("Invalid animation duration");
  std::set<std::pair<std::size_t, TrackPath>> targets;
  for (const auto &track : tracks) {
    track.validate(nodeCount);
    if (track.times.back() > duration ||
        !targets.emplace(track.node, track.path).second)
      throw std::invalid_argument(
          "Duplicate track or keys outside clip duration");
  }
}

void FlipbookProps::validate() const {
  if (!math::hasArea(grid) || !frames ||
      frames > std::uint64_t(grid.x) * grid.y ||
      !std::isfinite(framesPerSecond) || framesPerSecond <= 0 ||
      !math::isFinite(inset) || inset.x < 0 || inset.y < 0 ||
      inset.x * 2 >= 1.f / grid.x || inset.y * 2 >= 1.f / grid.y)
    throw std::invalid_argument("Invalid flipbook description");
  if (pixels &&
      (!math::hasArea(pixels->atlas) || !math::hasArea(pixels->frame) ||
       std::int64_t(grid.x - 1) * pixels->frame.x >= pixels->atlas.x ||
       std::int64_t(grid.y - 1) * pixels->frame.y >= pixels->atlas.y))
    throw std::invalid_argument("Pixel atlas grid exceeds source");
}

rendering::UVTransform flipbookFrame(const FlipbookProps &props,
                                     const Playback &playback) {
  props.validate();
  const auto frame = std::min(
      props.frames - 1,
      unsigned(playback.sampleTime(props.frames / props.framesPerSecond) *
               props.framesPerSecond));
  if (props.pixels) {
    const auto &p = *props.pixels;
    const int x = int(frame % props.grid.x) * p.frame.x,
              y = int(frame / props.grid.x) * p.frame.y;
    const float w = float(std::min(p.frame.x, p.atlas.x - x)) / p.atlas.x,
                h = float(std::min(p.frame.y, p.atlas.y - y)) / p.atlas.y;
    if (w <= 2 * props.inset.x || h <= 2 * props.inset.y)
      throw std::invalid_argument("Atlas inset consumes frame");
    return {{float(x) / p.atlas.x + props.inset.x,
             float(y) / p.atlas.y + props.inset.y},
            {w - 2 * props.inset.x, h - 2 * props.inset.y}};
  }
  return {{float(frame % props.grid.x) / props.grid.x + props.inset.x,
           float(frame / props.grid.x) / props.grid.y + props.inset.y},
          {1.f / props.grid.x - 2 * props.inset.x,
           1.f / props.grid.y - 2 * props.inset.y}};
}

math::Transform3D billboard(math::Vec3f position, const CameraProps &camera,
                            math::Vec3f scale) {
  // Derive orientation from the camera basis without coupling to GPU state.
  const auto view = camera.view(1).view;
  const auto inverse = math::inverse(view);
  const auto forward = math::transformDirection(inverse, {0, 0, 1});
  const float yaw = std::atan2(forward.x, forward.z),
              pitch = -std::asin(std::clamp(forward.y, -1.f, 1.f));
  const auto facing =
      math::axisAngle({0, 1, 0}, yaw) * math::axisAngle({1, 0, 0}, pitch);
  const auto basis = math::rotation(facing);
  const auto up = math::transformDirection(inverse, {0, 1, 0});
  const float roll =
      std::atan2(-math::dot(up, math::transformDirection(basis, {1, 0, 0})),
                 math::dot(up, math::transformDirection(basis, {0, 1, 0})));
  return {position, facing * math::axisAngle({0, 0, 1}, roll), scale};
}
} // namespace playground::scene
