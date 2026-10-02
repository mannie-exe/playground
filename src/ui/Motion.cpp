#include <algorithm>
#include <exception>

#include <ui/Motion.hpp>
#include <ui/Node.hpp>

namespace playground::ui {
void Easing::validate() const {
  if (kind < Kind::Linear || kind > Kind::CubicBezier || !steps ||
      !std::isfinite(x1) || !std::isfinite(x2) || !std::isfinite(y1) ||
      !std::isfinite(y2) || x1 < 0 || x1 > 1 || x2 < 0 || x2 > 1 || y1 < 0 ||
      y1 > 1 || y2 < 0 || y2 > 1)
    throw std::invalid_argument("Invalid bounded easing");
}

double Easing::sample(double p) const noexcept {
  p = std::clamp(p, 0., 1.);
  if (kind == Kind::Linear)
    return p;
  if (kind == Kind::Steps)
    return std::min(1., (std::floor(p * steps) + (jumpStart ? 1 : 0)) / steps);
  const auto curve = [](double t, double a, double b) {
    const double u = 1 - t;
    return 3 * u * u * t * a + 3 * u * t * t * b + t * t * t;
  };
  double low = 0, high = 1;
  for (int i = 0; i < 32; ++i) {
    const auto mid = (low + high) * .5;
    if (curve(mid, x1, x2) < p)
      low = mid;
    else
      high = mid;
  }
  return p == 0 || p == 1 ? p : curve((low + high) * .5, y1, y2);
}

void MotionSpec::validate() const {
  easing.validate();
  if (direction < MotionDirection::Normal ||
      direction > MotionDirection::AlternateReverse ||
      !std::isfinite(duration) || !std::isfinite(delay) || duration < 0 ||
      delay < 0 || !std::isfinite(end()))
    throw std::invalid_argument("Invalid motion timing");
}

const MotionSpec &ThemeMotion::get(MotionRole role) const noexcept {
  return role == MotionRole::Reveal    ? reveal
         : role == MotionRole::Dismiss ? dismiss
                                       : feedback;
}

void ThemeMotion::validate() const {
  feedback.validate();
  reveal.validate();
  dismiss.validate();
}

double TimelineSpec::duration() const noexcept {
  double result = 0;
  for (auto &track : tracks)
    result = std::max(result, track.at + track.motion.end());
  return result;
}

namespace detail {
void validateMotionValue(float v) {
  if (!std::isfinite(v))
    throw std::invalid_argument("Motion value must be finite");
}

void validateMotionValue(math::Vec2f v) {
  validateMotionValue(v.x);
  validateMotionValue(v.y);
}

void validateMotionValue(math::ColorRGBA8) {}

struct Track {
  MotionTarget target;
  MotionTrackSpec spec;
  std::uint64_t revision{};
};

struct MotionPlayback {
  std::vector<Track> tracks;
  double elapsed{}, duration{};
  AnimationStatus status{AnimationStatus::Running};
  std::optional<AnimationStatus> interruption;
  std::function<void(AnimationStatus)> done;
  bool notified{};
  bool transition{};
  bool sampling{}, cancelRequested{};
};

struct MotionCore {
  MotionLimits limits;
  std::vector<std::shared_ptr<MotionPlayback>> playbacks;
  MotionPreference preference{MotionPreference::Full};
  bool traversing{};
};

class Traversal {
  bool &_active;

public:
  explicit Traversal(bool &active) : _active{active} {
    if (active)
      throw std::logic_error("Cannot mutate motion during sampling");
    active = true;
  }

  ~Traversal() { _active = false; }
};

static bool active(const MotionPlayback &p) {
  return p.status == AnimationStatus::Running ||
         p.status == AnimationStatus::Paused;
}

static void stop(MotionPlayback &p, AnimationStatus status) {
  if (!active(p))
    return;
  if (p.sampling) {
    if (status == AnimationStatus::Cancelled) {
      p.cancelRequested = true;
      return;
    }
    throw std::logic_error("Cannot stop motion during sampling");
  }
  p.status = status == AnimationStatus::Completed && p.interruption
                 ? *p.interruption
                 : status;
  auto tracks = std::move(p.tracks);
  std::exception_ptr error;
  for (auto &t : tracks)
    try {
      if (t.target.alive() && t.target.revision() == t.revision)
        t.target.clear();
    } catch (...) {
      if (!error)
        error = std::current_exception();
    }
  if (error)
    std::rethrow_exception(error);
}

static MotionDatum interpolate(const MotionDatum &a, const MotionDatum &b,
                               double p) {
  return std::visit(
      [&](auto first) -> MotionDatum {
        using T = decltype(first);
        const auto last = std::get<T>(b);
        if constexpr (std::is_same_v<T, math::ColorRGBA8>) {
          const auto channel = [p](auto x, auto y) {
            return static_cast<std::uint8_t>(std::clamp(
                std::lround(std::lerp(double(x), double(y), p)), 0L, 255L));
          };
          return T{channel(first.r, last.r), channel(first.g, last.g),
                   channel(first.b, last.b), channel(first.a, last.a)};
        } else if constexpr (std::is_same_v<T, float>)
          return static_cast<float>(std::lerp(double(first), double(last), p));
        else
          return T{
              static_cast<float>(std::lerp(double(first.x), double(last.x), p)),
              static_cast<float>(
                  std::lerp(double(first.y), double(last.y), p))};
      },
      a);
}

template <typename Predicate>
static void removeTracks(MotionPlayback &p, Predicate matches,
                         AnimationStatus reason) {
  bool removed = false;
  std::exception_ptr error;
  std::erase_if(p.tracks, [&](auto &track) {
    if (!matches(track))
      return false;
    removed = true;
    try {
      if (track.target.alive() && track.target.revision() == track.revision)
        track.target.clear();
    } catch (...) {
      if (!error)
        error = std::current_exception();
    }
    return true;
  });
  if (removed) {
    if (!p.interruption || reason == AnimationStatus::TargetGone)
      p.interruption = reason;
    p.duration = 0;
    for (const auto &track : p.tracks)
      p.duration =
          std::max(p.duration, track.spec.at + track.spec.motion.end());
    if (p.tracks.empty() || p.elapsed >= p.duration)
      stop(p, AnimationStatus::Completed);
  }
  if (error)
    std::rethrow_exception(error);
}

static bool valid(MotionPlayback &p) {
  removeTracks(
      p, [](const auto &track) { return !track.target.alive(); },
      AnimationStatus::TargetGone);
  removeTracks(
      p,
      [](const auto &track) {
        return track.target.revision() != track.revision;
      },
      AnimationStatus::Replaced);
  return active(p);
}

static void sample(MotionPlayback &p) {
  if (!active(p) || !valid(p))
    return;
  {
    Traversal guard{p.sampling};
    for (auto &t : p.tracks) {
      const auto &s = t.spec;
      const double local = p.elapsed - s.at - s.motion.delay;
      if (local < 0) {
        bool previous = false;
        for (const auto &earlier : p.tracks) {
          if (&earlier == &t)
            break;
          if (earlier.target.identity == t.target.identity &&
              earlier.target.property == t.target.property &&
              p.elapsed >= earlier.spec.at + earlier.spec.motion.delay) {
            previous = true;
            break;
          }
        }
        if (!previous) {
          if (p.transition)
            t.target.present(s.values.front());
          else
            t.target.clear();
        }
        continue;
      }
      const auto duration = s.motion.duration;
      const auto total = duration * (double(s.motion.repeats) + 1);
      const auto iteration =
          local >= total ? s.motion.repeats
          : duration > 0 ? static_cast<unsigned>(std::min(
                               double(s.motion.repeats), local / duration))
                         : 0;
      double fraction = local >= total || duration == 0
                            ? 1
                            : std::fmod(local, duration) / duration;
      const auto direction = s.motion.direction;
      if (direction == MotionDirection::Reverse ||
          (direction == MotionDirection::Alternate && iteration % 2) ||
          (direction == MotionDirection::AlternateReverse && !(iteration % 2)))
        fraction = 1 - fraction;
      fraction = s.motion.easing.sample(fraction);
      const auto upper =
          std::upper_bound(s.offsets.begin(), s.offsets.end(), fraction);
      const auto i = std::min<std::size_t>(
          std::max<std::size_t>(1, upper - s.offsets.begin()),
          s.offsets.size() - 1);
      t.target.present(interpolate(s.values[i - 1], s.values[i],
                                   (fraction - s.offsets[i - 1]) /
                                       (s.offsets[i] - s.offsets[i - 1])));
    }
  }
  if (p.cancelRequested)
    stop(p, AnimationStatus::Cancelled);
}
} // namespace detail

AnimationHandle::~AnimationHandle() { cancel(); }

AnimationHandle::AnimationHandle(AnimationHandle &&other) noexcept
    : _playback{std::move(other._playback)} {}

AnimationHandle &AnimationHandle::operator=(AnimationHandle &&other) noexcept {
  if (this != &other) {
    cancel();
    _playback = std::move(other._playback);
  }
  return *this;
}

AnimationStatus AnimationHandle::status() const noexcept {
  return _playback ? _playback->status : AnimationStatus::Cancelled;
}

void AnimationHandle::cancel() noexcept {
  if (_playback)
    try {
      detail::stop(*_playback, AnimationStatus::Cancelled);
    } catch (...) {
    }
}

void AnimationHandle::pause() {
  if (_playback && _playback->sampling)
    throw std::logic_error("Cannot pause during sampling");
  if (_playback && status() == AnimationStatus::Running)
    _playback->status = AnimationStatus::Paused;
}

void AnimationHandle::resume() {
  if (_playback && _playback->sampling)
    throw std::logic_error("Cannot resume during sampling");
  if (_playback && status() == AnimationStatus::Paused)
    _playback->status = AnimationStatus::Running;
}

void AnimationHandle::seek(double seconds) {
  if (_playback && _playback->sampling)
    throw std::logic_error("Cannot seek during sampling");
  if (!std::isfinite(seconds) || seconds < 0)
    throw std::invalid_argument("Invalid motion seek");
  if (_playback && detail::active(*_playback)) {
    _playback->elapsed = std::min(seconds, _playback->duration);
    detail::sample(*_playback);
  }
}

void AnimationHandle::finish() {
  if (_playback && _playback->sampling)
    throw std::logic_error("Cannot finish during sampling");
  if (_playback && detail::active(*_playback)) {
    _playback->elapsed = _playback->duration;
    detail::sample(*_playback);
    detail::stop(*_playback, AnimationStatus::Completed);
  }
}

MotionEngine::MotionEngine(MotionLimits limits)
    : _core{std::make_unique<detail::MotionCore>()} {
  if (!limits.tracks || !limits.keyframes)
    throw std::invalid_argument("Empty motion limits");
  _core->limits = limits;
}

MotionEngine::~MotionEngine() {
  for (auto &p : _core->playbacks)
    try {
      detail::stop(*p, AnimationStatus::Cancelled);
    } catch (...) {
    }
}

AnimationHandle MotionEngine::start(const TimelineSpec &spec,
                                    const MotionBindings &bindings,
                                    std::function<void(AnimationStatus)> done,
                                    bool transition) {
  detail::Traversal traversal{_core->traversing};
  if (spec.tracks.size() > _core->limits.tracks)
    throw std::length_error("Motion track limit exceeded");
  std::size_t requestedFrames = 0;
  for (const auto &track : spec.tracks) {
    if (track.values.size() > _core->limits.keyframes - requestedFrames)
      throw std::length_error("Motion keyframe limit exceeded");
    requestedFrames += track.values.size();
  }
  auto p = std::make_shared<detail::MotionPlayback>();
  p->done = std::move(done);
  p->transition = transition;
  std::size_t frames = 0;
  if (spec.tracks.empty())
    throw std::invalid_argument("Empty timeline");
  for (auto track : spec.tracks) {
    track.motion.validate();
    if (!std::isfinite(track.at) || track.at < 0 ||
        !std::isfinite(track.at + track.motion.end()) ||
        track.binding >= bindings._targets.size() || track.offsets.size() < 2 ||
        track.offsets.size() != track.values.size() ||
        track.offsets.front() != 0 || track.offsets.back() != 1)
      throw std::invalid_argument("Invalid timeline track");
    auto target = bindings._targets[track.binding];
    if (!target.alive())
      throw std::invalid_argument("Expired motion target");
    double previous = -1;
    for (std::size_t i = 0; i < track.values.size(); ++i) {
      if (!std::isfinite(track.offsets[i]) || track.offsets[i] <= previous ||
          track.offsets[i] > 1 ||
          track.values[i].index() != target.read().index())
        throw std::invalid_argument("Invalid typed keyframe");
      std::visit([](auto v) { detail::validateMotionValue(v); },
                 track.values[i]);
      if (target.property == MotionProperty::Opacity &&
          (std::get<float>(track.values[i]) < 0 ||
           std::get<float>(track.values[i]) > 1))
        throw std::invalid_argument("Opacity must be in [0,1]");
      previous = track.offsets[i];
    }
    for (auto &t : p->tracks)
      if (t.target.identity == target.identity &&
          t.target.property == target.property &&
          std::max(t.spec.at + t.spec.motion.delay,
                   track.at + track.motion.delay) <
              std::min(t.spec.at + t.spec.motion.end(),
                       track.at + track.motion.end()))
        throw std::invalid_argument("Timeline property conflict");
    if (transition && track.values.front() == track.values.back()) {
      track.motion.duration = track.motion.delay = track.at = 0;
      track.motion.repeats = 0;
    }
    frames += track.values.size();
    p->duration = std::max(p->duration, track.at + track.motion.end());
    p->tracks.push_back({std::move(target), std::move(track), 0});
  }
  std::stable_sort(p->tracks.begin(), p->tracks.end(),
                   [](const auto &a, const auto &b) {
                     return a.spec.at + a.spec.motion.delay <
                            b.spec.at + b.spec.motion.delay;
                   });
  p->duration = 0;
  for (auto &entry : p->tracks) {
    auto &track = entry.spec;
    const auto &target = entry.target;
    if (_core->preference == MotionPreference::None ||
        (_core->preference == MotionPreference::Reduced &&
         (target.spatial || track.motion.decorative))) {
      track.at = 0;
      track.motion.duration = track.motion.delay = 0;
      track.motion.repeats = 0;
    } else if (_core->preference == MotionPreference::Reduced) {
      track.motion.duration = std::min(.12, track.motion.duration);
      track.motion.repeats = 0;
    }
    p->duration = std::max(p->duration, track.at + track.motion.end());
  }
  std::erase_if(_core->playbacks, [](auto &old) {
    if (detail::active(*old) || old->done)
      return false;
    old->notified = true;
    std::vector<detail::Track>{}.swap(old->tracks);
    return true;
  });
  const auto conflicts = [&](const auto &track) {
    for (const auto &replacement : p->tracks)
      if (track.target.identity == replacement.target.identity &&
          track.target.property == replacement.target.property)
        return true;
    return false;
  };
  auto usage = stats();
  std::size_t pending = 0;
  for (const auto &old : _core->playbacks) {
    if (!detail::active(*old)) {
      ++pending;
      continue;
    }
    std::size_t replaced = 0;
    for (const auto &track : old->tracks)
      if (conflicts(track)) {
        ++replaced;
        --usage.tracks;
        usage.keyframes -= track.spec.values.size();
      }
    if (replaced == old->tracks.size() && old->done)
      ++pending;
  }
  if (pending > _core->limits.tracks)
    throw std::length_error("Pending motion limit exceeded");
  if (p->tracks.size() > _core->limits.tracks - usage.tracks ||
      frames > _core->limits.keyframes - usage.keyframes)
    throw std::length_error("Motion admission limit exceeded");
  _core->playbacks.reserve(_core->playbacks.size() + 1);
  for (auto &old : _core->playbacks)
    if (detail::active(*old))
      detail::removeTracks(*old, conflicts, AnimationStatus::Replaced);
  try {
    for (auto &t : p->tracks) {
      if (transition)
        t.target.author(t.spec.values.back());
      t.revision = t.target.revision();
      if (transition)
        t.target.present(t.spec.values.front());
    }
    _core->playbacks.push_back(p);
    detail::sample(*p);
    if (p->duration == 0)
      detail::stop(*p, AnimationStatus::Completed);
  } catch (...) {
    try {
      detail::stop(*p, AnimationStatus::Cancelled);
    } catch (...) {
    }
    throw;
  }
  return AnimationHandle{p};
}

void MotionEngine::advance(double seconds) {
  detail::Traversal traversal{_core->traversing};
  if (!std::isfinite(seconds) || seconds < 0)
    throw std::invalid_argument("Invalid motion elapsed time");
  for (auto &p : _core->playbacks)
    if (detail::active(*p) && detail::valid(*p) &&
        p->status == AnimationStatus::Running) {
      p->elapsed = std::min(p->duration, p->elapsed + seconds);
      if (p->elapsed >= p->duration) {
        detail::sample(*p);
        detail::stop(*p, AnimationStatus::Completed);
      }
    }
}

void MotionEngine::sample() {
  detail::Traversal traversal{_core->traversing};
  for (auto &p : _core->playbacks)
    detail::sample(*p);
}

void MotionEngine::dispatchCompletions() {
  if (_core->traversing)
    throw std::logic_error("Cannot dispatch completions during sampling");
  // Snapshot completion ownership before callbacks can append playback.
  std::vector<std::shared_ptr<detail::MotionPlayback>> completed;
  completed.reserve(_core->playbacks.size());
  for (auto &p : _core->playbacks)
    if (!detail::active(*p) && !p->notified) {
      p->notified = true;
      completed.push_back(p);
    }
  std::erase_if(_core->playbacks, [](auto &p) { return !detail::active(*p); });
  std::exception_ptr error;
  for (auto &p : completed) {
    std::vector<detail::Track>{}.swap(p->tracks);
    auto callback = std::move(p->done);
    if (callback)
      try {
        callback(p->status);
      } catch (...) {
        if (!error)
          error = std::current_exception();
      }
  }
  if (error)
    std::rethrow_exception(error);
}

bool MotionEngine::needsFrame() const noexcept {
  for (auto &p : _core->playbacks)
    if (p->status == AnimationStatus::Running)
      for (auto &t : p->tracks)
        if (p->elapsed >= t.spec.at + t.spec.motion.delay &&
            p->elapsed < t.spec.at + t.spec.motion.end())
          return true;
  return false;
}

std::optional<double> MotionEngine::nextDelay() const noexcept {
  std::optional<double> delay;
  auto consider = [&](double d) {
    if (!delay || d < *delay)
      delay = std::max(0., d);
  };
  for (auto &p : _core->playbacks) {
    if (!detail::active(*p) && !p->notified)
      consider(0);
    if (p->status != AnimationStatus::Running)
      continue;
    consider(p->duration - p->elapsed);
    for (auto &t : p->tracks) {
      const auto start = t.spec.at + t.spec.motion.delay;
      if (start > p->elapsed)
        consider(start - p->elapsed);
    }
  }
  return delay;
}

MotionStats MotionEngine::stats() const noexcept {
  MotionStats result;
  result.retainedBytes = sizeof(detail::MotionCore) +
                         _core->playbacks.capacity() *
                             sizeof(std::shared_ptr<detail::MotionPlayback>);
  for (auto &p : _core->playbacks) {
    result.retainedBytes += sizeof(detail::MotionPlayback) +
                            p->tracks.capacity() * sizeof(detail::Track);
    if (detail::active(*p)) {
      ++result.playbacks;
      result.tracks += p->tracks.size();
    }
    for (auto &t : p->tracks) {
      if (detail::active(*p))
        result.keyframes += t.spec.values.size();
      result.retainedBytes += t.spec.values.capacity() * sizeof(MotionDatum) +
                              t.spec.offsets.capacity() * sizeof(double);
    }
  }
  return result;
}

void MotionEngine::setPreference(MotionPreference preference,
                                 bool systemReduced) {
  detail::Traversal traversal{_core->traversing};
  if (preference < MotionPreference::System ||
      preference > MotionPreference::None)
    throw std::invalid_argument("Invalid motion preference");
  if (preference == MotionPreference::System)
    preference =
        systemReduced ? MotionPreference::Reduced : MotionPreference::Full;
  if (_core->preference == preference)
    return;
  _core->preference = preference;
  if (preference != MotionPreference::Full)
    for (auto &p : _core->playbacks)
      if (detail::active(*p)) {
        p->elapsed = p->duration;
        detail::sample(*p);
        detail::stop(*p, AnimationStatus::Completed);
      }
}

namespace motion {
template <typename T>
static MotionBinding<T> bind(NodeHandle<Node> handle, MotionProperty property,
                             bool spatial = false) {
  return MotionBinding<T>{{handle.get(), property, spatial,
                           [handle] { return handle.get() != nullptr; },
                           [handle, property] {
                             auto *n = handle.get();
                             return n ? n->motionRevision(property) : 0;
                           },
                           [handle, property]() -> MotionDatum {
                             return handle.get()->motionValue(property);
                           },
                           [handle, property](const MotionDatum &v) {
                             if (auto *n = handle.get())
                               n->setMotionValue(property, v);
                           },
                           [handle, property](const MotionDatum &v) {
                             if (auto *n = handle.get())
                               n->presentMotion(property, v);
                           },
                           [handle, property] {
                             if (auto *n = handle.get())
                               n->presentMotion(property, {});
                           }}};
}

MotionBinding<float> opacity(NodeHandle<Node> h) {
  return bind<float>(h, MotionProperty::Opacity);
}

MotionBinding<math::Vec2f> translation(NodeHandle<Node> h) {
  return bind<math::Vec2f>(h, MotionProperty::Translation, true);
}

MotionBinding<math::Vec2f> scale(NodeHandle<Node> h) {
  return bind<math::Vec2f>(h, MotionProperty::Scale, true);
}

MotionBinding<float> rotation(NodeHandle<Node> h) {
  return bind<float>(h, MotionProperty::Rotation, true);
}

MotionBinding<math::ColorRGBA8> background(NodeHandle<Node> h) {
  return bind<math::ColorRGBA8>(h, MotionProperty::Background);
}

MotionBinding<math::ColorRGBA8> borderColor(NodeHandle<Node> h) {
  return bind<math::ColorRGBA8>(h, MotionProperty::BorderColor);
}
} // namespace motion
} // namespace playground::ui
