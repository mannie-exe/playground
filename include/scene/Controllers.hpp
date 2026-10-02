#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <math/Geometry2D.hpp>
#include <scene/Scene3D.hpp>

namespace playground::scene {

struct MovementIntent {
  math::Vec3f direction{};
};

struct MovementProps {
  float unitsPerSecond{5};
};

class MovementController {
  MovementProps _props;

public:
  explicit MovementController(MovementProps props = {});

  const MovementProps &props() const noexcept { return _props; }

  void setProps(MovementProps);
  math::Vec3f advance(math::Vec3f position, MovementIntent,
                      double seconds) const;
};

struct OrbitProps {
  math::Vec3f target{};
  float yaw{}, pitch{}, distance{3};
  float minimumDistance{0.1f}, maximumDistance{1000};
  float maximumPitch{1.553343f};
};

struct OrbitIntent {
  math::Vec2f radians{};
  float zoom{}; // Additive world-distance change; negative zooms in.
};

class OrbitController {
  OrbitProps _props;

public:
  explicit OrbitController(OrbitProps props = {});

  const OrbitProps &props() const noexcept { return _props; }

  void setProps(OrbitProps);
  void update(OrbitIntent);
  CameraProps camera(CameraProps lens = {}) const;
};

struct FreeCameraProps {
  math::Vec3f position{0, 0, -3};
  float yaw{}, pitch{}, unitsPerSecond{5};
  float maximumPitch{1.553343f};
};

struct FreeCameraIntent {
  math::Vec3f movement{}; // Local right, world up, local forward.
  math::Vec2f radians{};  // Displacement, never multiplied by time here.
};

class FreeCameraController {
  FreeCameraProps _props;

public:
  explicit FreeCameraController(FreeCameraProps props = {});

  const FreeCameraProps &props() const noexcept { return _props; }

  void setProps(FreeCameraProps);
  void advance(FreeCameraIntent, double seconds);
  CameraProps camera(CameraProps lens = {}) const;
};

using CameraId = std::uint64_t;

struct CameraSource {
  CameraProps camera;
  int priority{};
  bool enabled{true};
};

// Values are owned; IDs are unique across directors and are never reused.
class CameraDirector {
  struct Entry {
    CameraId id;
    CameraSource source;
  };

  std::vector<Entry> _sources;
  std::optional<CameraId> _active;
  std::optional<CameraProps> _fallback;
  CameraProps _camera, _from, _to;
  double _duration, _elapsed{};
  bool _transitioning{};

  void resolve(bool refreshActive = false);
  void transition(CameraProps);

public:
  explicit CameraDirector(CameraProps initial = {},
                          double transitionSeconds = 0.3);
  CameraDirector(const CameraDirector &) = delete;
  CameraDirector &operator=(const CameraDirector &) = delete;
  CameraDirector(CameraDirector &&) = delete;
  CameraDirector &operator=(CameraDirector &&) = delete;

  CameraId add(CameraSource);
  void update(CameraId, CameraSource);
  void remove(CameraId);
  void setFallback(std::optional<CameraProps>);
  void advance(double seconds);

  const CameraProps &camera() const noexcept { return _camera; }

  std::optional<CameraId> active() const noexcept { return _active; }

  bool transitioning() const noexcept { return _transitioning; }
};

struct CameraPathKey {
  double seconds{};
  CameraProps camera;
};

// Immutable loop, with zero velocity at every authored key and the loop seam.
class CameraPath {
  double _duration;
  std::vector<CameraPathKey> _keys;

public:
  CameraPath(double durationSeconds, std::vector<CameraPathKey> keys);

  double durationSeconds() const noexcept { return _duration; }

  CameraProps sample(double elapsedSeconds) const;
};

// Model poses, not renderer state. Publish once per completed simulation tick.
class PoseHistory {
  math::Transform3D _previous, _current;

public:
  explicit PoseHistory(math::Transform3D initial = {});

  const math::Transform3D &current() const noexcept { return _current; }

  void publish(math::Transform3D);
  void teleport(math::Transform3D);
  math::Transform3D sample(double alpha) const;
};
} // namespace playground::scene
