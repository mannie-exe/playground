#pragma once

#include <world/Frames.hpp>

namespace playground::world {

enum class LocomotionProfile { Steered, Strafe, Tank };
enum class MovementBasis { View, Body };
enum class FacingTarget { Travel, View };
enum class TurnResponse { Immediate, RateLimited, Damped };
enum class MovementDuringTurn { Unrestricted, ScaleByHeading };
enum class FreeLookReturn { KeepReference, EaseToView };

struct LocomotionProps {
  LocomotionProfile profile{LocomotionProfile::Steered};
  MovementBasis basis{MovementBasis::Body};
  FacingTarget facing{FacingTarget::View};
  TurnResponse response{TurnResponse::Damped};
  MovementDuringTurn movementDuringTurn{MovementDuringTurn::Unrestricted};
  FreeLookReturn freeLookReturn{FreeLookReturn::EaseToView};
  Vec3d up{0, 1, 0}; // In the declared movement frame, or space axes.
  SpatialLimits limits;
  double speed{3}, turnRate{3.141592653589793}, turnHalfLife{.12},
      returnHalfLife{.15};
  bool inheritFrameVelocity{true};
  void validate() const;
  static LocomotionProps responsive();
  static LocomotionProps outOfNowhere();
};

struct LocomotionIntent {
  double right{}, forward{}, turn{}, viewHeading{};
  bool aim{}, freeLook{}, recenter{};
};

using LocomotionState = EntitySample;

struct LocomotionRequest {
  EntityHandle entity;
  std::uint64_t tick{}, discontinuity{};
  WorldVelocity velocity;
  math::Quaternion orientation;
  double desiredHeading{}, resolvedHeading{};
  std::optional<FrameSample> frame;
  Vec3d localVelocity, localAngular;
  bool inheritFrameVelocity{};
};

class CharacterFacingController {
  LocomotionProps _props;
  std::optional<EntityHandle> _subject;
  std::optional<FrameId> _frame;
  std::uint64_t _discontinuity{}, _tick{};
  double _reference{}, _previousView{};
  bool _freeLook{}, _returning{}, _heldReference{};
  LocomotionRequest evaluate(const LocomotionState &, LocomotionIntent,
                             double seconds, const FrameSample *);

public:
  explicit CharacterFacingController(LocomotionProps = {});

  const LocomotionProps &props() const noexcept { return _props; }

  void setProps(LocomotionProps); // Never changes a subject's actual state.
  void reset() noexcept;
  LocomotionRequest advance(const LocomotionState &, LocomotionIntent,
                            double seconds, const FrameSample * = nullptr);
};

// Shared conversion for navigation/scripted travel. Tank turns toward the
// direction instead of acquiring a strafe velocity. Frame-relative requests
// require desired velocity already expressed in that frame's axes.
LocomotionIntent travelIntent(const LocomotionState &, Vec3d desiredVelocity,
                              std::optional<double> facing,
                              const LocomotionProps &, double seconds,
                              const FrameSample * = nullptr);

enum class MovementRealization { Kinematic, Physics, RootMotion };
enum class MovementStatus { Applied, Unsupported };

struct MovementResult {
  MovementStatus status{MovementStatus::Unsupported};
  EntitySample actual;
  std::optional<FrameAttachment> attachment;
};

struct KinematicMotorProps {
  double maximumStepSeconds{.25}, maximumSpeed{10000},
      maximumAngularSpeed{1000};
  void validate() const;
};

// Domain resolves attachment motion to the current frame before evaluation.
// This motor integrates relative travel once; inherited frame velocity remains
// part of reported world velocity. No contacts, gravity or collision claim.
MovementResult
realizeMovement(const LocomotionState &, const LocomotionRequest &,
                double seconds,
                MovementRealization = MovementRealization::Kinematic,
                KinematicMotorProps = {}, SpatialLimits = {});
} // namespace playground::world
