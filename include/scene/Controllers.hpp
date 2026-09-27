#pragma once

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
