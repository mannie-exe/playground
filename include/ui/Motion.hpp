#pragma once

#include <cmath>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <variant>
#include <vector>

#include <math/Color.hpp>
#include <math/Geometry2D.hpp>
#include <runtime/MotionPreference.hpp>

namespace playground::ui {
class Node;
template <typename T> class NodeHandle;
using runtime::MotionPreference;
enum class MotionRole { Feedback, Reveal, Dismiss };
enum class MotionDirection { Normal, Reverse, Alternate, AlternateReverse };
enum class AnimationStatus {
  Running,
  Paused,
  Completed,
  Cancelled,
  Replaced,
  TargetGone
};

struct Easing {
  enum class Kind { Linear, Steps, CubicBezier };
  Kind kind{Kind::Linear};
  double x1{}, y1{}, x2{1}, y2{1};
  unsigned steps{1};
  bool jumpStart{};
  void validate() const;
  double sample(double progress) const noexcept;
  bool operator==(const Easing &) const = default;
};

struct MotionSpec {
  double duration{.12}, delay{};
  Easing easing;
  unsigned repeats{}; // Additional iterations; finite by design.
  MotionDirection direction{MotionDirection::Normal};
  bool decorative{};
  void validate() const;

  double end() const noexcept {
    return delay + duration * (double(repeats) + 1);
  }

  bool operator==(const MotionSpec &) const = default;
};

struct ThemeMotion {
  MotionSpec feedback{.12}, reveal{.18}, dismiss{.12};
  const MotionSpec &get(MotionRole role) const noexcept;
  void validate() const;
  bool operator==(const ThemeMotion &) const = default;
};

using MotionDatum = std::variant<float, math::Vec2f, math::ColorRGBA8>;
enum class MotionProperty {
  Opacity,
  Translation,
  Scale,
  Rotation,
  Background,
  BorderColor,
  Custom
};

namespace detail {
void validateMotionValue(float);
void validateMotionValue(math::Vec2f);
void validateMotionValue(math::ColorRGBA8);

struct MotionTarget {
  const void *identity{};
  MotionProperty property{MotionProperty::Custom};
  bool spatial{};
  std::function<bool()> alive;
  std::function<std::uint64_t()> revision;
  std::function<MotionDatum()> read;
  std::function<void(const MotionDatum &)> author, present;
  std::function<void()> clear;
};
struct MotionPlayback;
struct MotionCore;
} // namespace detail

template <typename T> struct Keyframe {
  double offset{};
  T value{};
};

template <typename T> struct Keyframes {
  std::vector<Keyframe<T>> values;

  void validate() const {
    if (values.size() < 2 || values.front().offset != 0 ||
        values.back().offset != 1)
      throw std::invalid_argument("Keyframes require endpoints zero and one");
    double previous = -1;
    for (const auto &frame : values) {
      if (!std::isfinite(frame.offset) || frame.offset <= previous ||
          frame.offset > 1)
        throw std::invalid_argument("Keyframe offsets must strictly increase");
      detail::validateMotionValue(frame.value);
      previous = frame.offset;
    }
  }
};

template <typename T> class MotionBinding {
  detail::MotionTarget _target;

public:
  explicit MotionBinding(detail::MotionTarget target)
      : _target{std::move(target)} {}

  const detail::MotionTarget &target() const noexcept { return _target; }
};

// Optional custom control state; binding lifetime never extends its owner.
template <typename T> class MotionValue {
  struct State {
    T authored;
    std::optional<T> displayed;
    std::uint64_t revision{};
    std::function<void()> invalidate;
  };

  std::shared_ptr<State> _state;

public:
  explicit MotionValue(T value = {}, std::function<void()> invalidate = {})
      : _state{std::make_shared<State>(
            State{value, {}, 0, std::move(invalidate)})} {
    detail::validateMotionValue(value);
  }

  MotionValue(const MotionValue &) = delete;
  MotionValue &operator=(const MotionValue &) = delete;

  T value() const { return _state->displayed.value_or(_state->authored); }

  T authored() const { return _state->authored; }

  void set(T value) {
    detail::validateMotionValue(value);
    _state->authored = value;
    _state->displayed.reset();
    ++_state->revision;
    if (_state->invalidate)
      _state->invalidate();
  }

  MotionBinding<T> binding(bool spatial = false) {
    std::weak_ptr<State> weak = _state;
    return MotionBinding<T>{
        {_state.get(), MotionProperty::Custom, spatial,
         [weak] { return !weak.expired(); },
         [weak] {
           auto s = weak.lock();
           return s ? s->revision : 0;
         },
         [weak]() -> MotionDatum {
           auto s = weak.lock();
           return s ? s->displayed.value_or(s->authored) : T{};
         },
         [weak](const MotionDatum &v) {
           if (auto s = weak.lock()) {
             s->authored = std::get<T>(v);
             ++s->revision;
           }
         },
         [weak](const MotionDatum &v) {
           if (auto s = weak.lock()) {
             const auto next = std::get<T>(v);
             if (s->displayed.value_or(s->authored) != next) {
               s->displayed = next;
               if (s->invalidate)
                 s->invalidate();
             }
           }
         },
         [weak] {
           if (auto s = weak.lock(); s && s->displayed) {
             s->displayed.reset();
             if (s->invalidate)
               s->invalidate();
           }
         }}};
  }
};

struct MotionTrackSpec {
  std::size_t binding{};
  MotionSpec motion;
  std::vector<double> offsets;
  std::vector<MotionDatum> values;
  double at{};
};

struct TimelineSpec {
  std::vector<MotionTrackSpec> tracks;
  double duration() const noexcept;

  template <typename T>
  void at(double offset, std::size_t binding, const Keyframes<T> &frames,
          MotionSpec spec = {}) {
    frames.validate();
    spec.validate();
    if (!std::isfinite(offset) || offset < 0)
      throw std::invalid_argument("Invalid timeline offset");
    MotionTrackSpec track{binding, spec, {}, {}, offset};
    for (const auto &frame : frames.values) {
      track.offsets.push_back(frame.offset);
      track.values.emplace_back(frame.value);
    }
    tracks.push_back(std::move(track));
  }

  template <typename T>
  void append(std::size_t binding, const Keyframes<T> &frames,
              MotionSpec spec = {}) {
    at(duration(), binding, frames, spec);
  }
};

class MotionBindings {
  std::vector<detail::MotionTarget> _targets;
  friend class MotionEngine;

public:
  template <typename T> std::size_t add(const MotionBinding<T> &binding) {
    _targets.push_back(binding.target());
    return _targets.size() - 1;
  }
};

struct MotionLimits {
  std::size_t tracks{1024}, keyframes{16384};
};

struct MotionStats {
  std::size_t playbacks{}, tracks{}, keyframes{}, retainedBytes{};
};

class AnimationHandle {
  std::shared_ptr<detail::MotionPlayback> _playback;
  friend class MotionEngine;

  explicit AnimationHandle(const std::shared_ptr<detail::MotionPlayback> &p)
      : _playback{p} {}

public:
  AnimationHandle() = default;
  ~AnimationHandle();
  AnimationHandle(AnimationHandle &&) noexcept;
  AnimationHandle &operator=(AnimationHandle &&) noexcept;
  AnimationHandle(const AnimationHandle &) = delete;
  AnimationHandle &operator=(const AnimationHandle &) = delete;
  AnimationStatus status() const noexcept;
  void pause();
  void resume();
  void seek(double seconds);
  void finish();
  void cancel() noexcept;
};

class MotionEngine {
  std::unique_ptr<detail::MotionCore> _core;
  ThemeMotion _theme;
  AnimationHandle start(const TimelineSpec &, const MotionBindings &,
                        std::function<void(AnimationStatus)>, bool transition);

public:
  explicit MotionEngine(MotionLimits = {});
  ~MotionEngine();
  MotionEngine(const MotionEngine &) = delete;
  MotionEngine &operator=(const MotionEngine &) = delete;

  AnimationHandle play(const TimelineSpec &spec, const MotionBindings &bindings,
                       std::function<void(AnimationStatus)> done = {}) {
    return start(spec, bindings, std::move(done), false);
  }

  template <typename T>
  AnimationHandle play(const MotionBinding<T> &binding,
                       const Keyframes<T> &frames, MotionSpec spec = {},
                       std::function<void(AnimationStatus)> done = {}) {
    TimelineSpec timeline;
    timeline.at(0, 0, frames, spec);
    MotionBindings bindings;
    bindings.add(binding);
    return play(timeline, bindings, std::move(done));
  }

  template <typename T>
  AnimationHandle transition(const MotionBinding<T> &binding, T target,
                             MotionSpec spec = {},
                             std::function<void(AnimationStatus)> done = {}) {
    detail::validateMotionValue(target);
    if (!binding.target().alive())
      return {};
    const auto from = std::get<T>(binding.target().read());
    TimelineSpec timeline;
    timeline.at(0, 0, Keyframes<T>{{{0, from}, {1, target}}}, spec);
    MotionBindings bindings;
    bindings.add(binding);
    return start(timeline, bindings, std::move(done), true);
  }

  void setThemeMotion(ThemeMotion theme) {
    theme.validate();
    _theme = std::move(theme);
  }

  template <typename T>
  AnimationHandle transition(const MotionBinding<T> &binding, T target,
                             MotionRole role,
                             std::function<void(AnimationStatus)> done = {}) {
    return transition(binding, target, _theme.get(role), std::move(done));
  }

  void advance(double seconds);
  void sample();
  void dispatchCompletions();
  bool needsFrame() const noexcept;
  std::optional<double> nextDelay() const noexcept;
  MotionStats stats() const noexcept;
  void setPreference(MotionPreference, bool systemReduced = false);
};

namespace motion {
MotionBinding<float> opacity(NodeHandle<Node>);
MotionBinding<math::Vec2f> translation(NodeHandle<Node>);
MotionBinding<math::Vec2f> scale(NodeHandle<Node>);
MotionBinding<float> rotation(NodeHandle<Node>);
MotionBinding<math::ColorRGBA8> background(NodeHandle<Node>);
MotionBinding<math::ColorRGBA8> borderColor(NodeHandle<Node>);
} // namespace motion
} // namespace playground::ui
