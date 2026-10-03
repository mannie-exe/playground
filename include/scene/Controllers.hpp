#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <math/Geometry2D.hpp>
#include <scene/WorldScene.hpp>

namespace playground::scene {

struct MovementIntent {
  world::Vec3d direction{};
};

struct MovementProps {
  double unitsPerSecond{5};
};

class MovementController {
  MovementProps _props;

public:
  explicit MovementController(MovementProps = {});

  const MovementProps &props() const noexcept { return _props; }

  void setProps(MovementProps);
  world::WorldPosition advance(world::WorldPosition, MovementIntent,
                               double seconds,
                               const world::SpatialLimits & = {}) const;
};

struct LookState {
  double yaw{}, pitch{};
};

struct LookProps {
  double maximumPitch{1.5533430342749532};
};

struct LookIntent {
  math::Vec2f deltaRadians{}, radiansPerSecond{};
};

class LookController {
  LookState _state;
  LookProps _props;

public:
  explicit LookController(LookState = {}, LookProps = {});

  LookState state() const noexcept { return _state; }

  void setState(LookState);
  void advance(LookIntent, double seconds);
  math::Quaternion orientation() const;
};

struct ZoomIntent {
  double deltaMeters{}, metersPerSecond{};
};

struct OrbitProps {
  world::WorldPosition target;
  std::uint64_t epoch{};
  double yaw{}, pitch{}, distance{3};
  double minimumDistance{.1}, maximumDistance{1000},
      maximumPitch{1.5533430342749532};
  world::SpatialLimits limits;
  CameraProps lens;
};

struct OrbitIntent {
  math::Vec2f radians{};
  double zoom{};
  double zoomLog{};
};

class OrbitController {
  OrbitProps _props;

public:
  explicit OrbitController(OrbitProps);

  const OrbitProps &props() const noexcept { return _props; }

  void setProps(OrbitProps);
  void update(OrbitIntent);
  void pan(math::Vec2f logicalPixels, double viewportHeight);
  WorldCamera camera() const;
  WorldCamera camera(CameraProps lens) const;
};

struct FreeCameraProps {
  world::WorldPosition position;
  std::uint64_t epoch{};
  double yaw{}, pitch{}, unitsPerSecond{5}, maximumPitch{1.5533430342749532};
  world::SpatialLimits limits;
};

struct FreeCameraIntent {
  world::Vec3d movement{}; // Local right, world up, local forward.
  math::Vec2f radians{};   // Displacement; rates belong in LookIntent.
};

class FreeCameraController {
  FreeCameraProps _props;

public:
  explicit FreeCameraController(FreeCameraProps);

  const FreeCameraProps &props() const noexcept { return _props; }

  void setProps(FreeCameraProps);
  void advance(FreeCameraIntent, double seconds);
  WorldCamera camera(CameraProps lens = {}) const;
};

using CameraId = std::uint64_t;

struct CameraSource {
  WorldCamera camera;
  int priority{};
  bool enabled{true};
};

struct CameraTransitionProps {
  double seconds{.3};
  bool reducedMotion{};
};

class CameraDirector {
  struct Entry {
    CameraId id;
    CameraSource source;
  };

  std::vector<Entry> _sources;
  std::optional<CameraId> _active;
  std::optional<WorldCamera> _fallback;
  WorldCamera _camera, _from, _to;
  CameraTransitionProps _props;
  world::SpatialLimits _limits;
  double _duration{}, _elapsed{};
  bool _transitioning{};
  void resolve(bool refreshActive = false);
  void transition(WorldCamera);

public:
  explicit CameraDirector(WorldCamera initial, double transitionSeconds = .3,
                          world::SpatialLimits = {});
  CameraDirector(const CameraDirector &) = delete;
  CameraDirector &operator=(const CameraDirector &) = delete;
  CameraId add(CameraSource);
  void update(CameraId, CameraSource);
  void remove(CameraId);
  void setFallback(std::optional<WorldCamera>);
  void setTransitionProps(CameraTransitionProps);
  void restartTransition();
  void cut();
  void advance(double seconds);

  const WorldCamera &camera() const noexcept { return _camera; }

  std::optional<CameraId> active() const noexcept { return _active; }

  bool transitioning() const noexcept { return _transitioning; }
};

struct CameraPathKey {
  double seconds{};
  WorldCamera camera;
};

class CameraPath {
  double _duration;
  std::vector<CameraPathKey> _keys;
  world::SpatialLimits _limits;

public:
  CameraPath(double durationSeconds, std::vector<CameraPathKey>,
             world::SpatialLimits = {});

  double durationSeconds() const noexcept { return _duration; }

  WorldCamera sample(double elapsedSeconds) const;
};

// Presentation samples are deliberately distinct from authoritative
// EntitySample.
struct PoseSample {
  world::EntityHandle entity;
  world::WorldPose pose;
  std::uint64_t tick{}, discontinuity{};
};

class PoseHistory {
  PoseSample _previous, _current;
  world::SpatialLimits _limits;

public:
  explicit PoseHistory(world::EntitySample, world::SpatialLimits = {});

  const PoseSample &current() const noexcept { return _current; }

  void publish(world::EntitySample);
  void teleport(world::EntitySample);
  PoseSample sample(double alpha) const;
};

enum class PerspectivePolicy { Automatic, FirstPerson, ThirdPerson };
enum class CameraPerspective { FirstPerson, ThirdPerson };
enum class CameraReturnPolicy {
  PreserveDisplayedView,
  RestoreSavedView,
  CutReset
};

struct CameraTargetSample {
  PoseSample subject;
  world::WorldPosition followAnchor, eyeAnchor;
};

CameraTargetSample cameraTarget(const PoseSample &,
                                world::Vec3d followOffset = {0, 1, 0},
                                world::Vec3d eyeOffset = {0, 1.7, 0},
                                const world::SpatialLimits & = {});

struct FollowCameraProps {
  PerspectivePolicy perspective{PerspectivePolicy::Automatic};
  double distance{3}, minimumDistance{}, maximumDistance{20};
  double enterDistance{.15}, exitDistance{.35};
  double followHalfLife{.08}, perspectiveHalfLife{.08}, settleMeters{.0001};
  world::Vec3d shoulder{.35, 0, 0};
  CameraProps thirdPersonLens, firstPersonLens;
  LookProps look;
  world::SpatialLimits limits;
  bool reducedMotion{};
  void validate() const;
};
enum class CameraObstructionStatus { Unavailable, Clear, Blocked, Unresolved };

struct CameraObstructionRequest {
  WorldCamera candidate;
  world::WorldPosition anchor;
  std::optional<WorldCamera> previous;
  std::vector<world::EntityHandle> excluded;
};

struct CameraObstructionResult {
  CameraObstructionStatus status{CameraObstructionStatus::Unavailable};
  WorldCamera camera;
};

class CameraObstructionQuery {
public:
  virtual ~CameraObstructionQuery() = default;

  virtual CameraObstructionResult
  evaluate(const CameraObstructionRequest &request) const {
    return {CameraObstructionStatus::Unavailable, request.candidate};
  }
};

struct FollowCameraState {
  PerspectivePolicy requested{PerspectivePolicy::Automatic};
  CameraPerspective effective{CameraPerspective::ThirdPerson};
  double requestedDistance{3}, resolvedDistance{};
  bool hasTarget{}, transitioning{};
  CameraObstructionStatus obstruction{CameraObstructionStatus::Unavailable};
};

struct CameraAdoption {
  LookState look;
  bool exact{};
};

class FollowCameraRig {
  FollowCameraProps _props;
  FollowCameraState _state;
  std::optional<CameraTargetSample> _target;
  std::optional<WorldCamera> _camera;
  double _firstPersonBlend{};
  LookState _look;
  void evaluate(const CameraTargetSample &, LookState, ZoomIntent, double,
                bool reset);

public:
  explicit FollowCameraRig(FollowCameraProps = {});

  const FollowCameraProps &props() const noexcept { return _props; }

  void setProps(FollowCameraProps);
  void advance(const CameraTargetSample &, LookState, ZoomIntent,
               double seconds);
  void reset(const CameraTargetSample &, LookState);
  void clearTarget() noexcept;
  CameraAdoption adoptView(const WorldCamera &, const CameraTargetSample &);
  CameraAdoption returnView(CameraReturnPolicy, const WorldCamera &displayed,
                            const CameraTargetSample &, LookState saved);

  FollowCameraState state() const noexcept { return _state; }

  const WorldCamera &camera() const;
};
} // namespace playground::scene
