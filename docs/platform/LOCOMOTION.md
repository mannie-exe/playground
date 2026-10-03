# Locomotion profiles and facing

Locomotion consumes intent and produces requested movement and facing. Camera
placement, device routing and movement realization are separate responsibilities.
Apps own the controlled subject; profiles require neither a character mesh nor a
physics engine. See [input ownership](RUNTIME.md#viewport-control-sessions) and
[camera evaluation](../render/CAMERAS.md).

## Values and ownership

| API | Contract |
|---|---|
| `LocomotionProfile` | `Steered`, `Strafe`, or `Tank`; independent of camera perspective |
| `LocomotionIntent` | Normalized movement vector, turn input and contextual aim/free-look actions; no physical key codes |
| `LocomotionProps` | Profile, movement basis, facing target, speed limits, turn response and movement-during-turn policy |
| `LocomotionState` | Actual subject pose, body heading and velocity supplied by the app's movement realization |
| `LocomotionRequest` | Space/frame-identified requested velocity and facing target; does not assert that either was achieved |
| `CharacterFacingController` | Evaluates a heading target and response from explicit state, intent and elapsed seconds |

Desired view heading, desired travel direction, actual body heading and actual
velocity are distinct values. Visual mesh orientation may follow independently;
it is not the authority for movement. Controllers hold copied values and validated
identities, not borrowed mesh/body pointers. The app publishes actual results
after applying requests and feeds those results into the next evaluation.

`world/Locomotion.hpp` uses `LocomotionState = EntitySample` so player control,
navigation and camera targets share checked identity, actual pose/velocity, tick
and discontinuity values. `CharacterFacingController::advance` retains only
steering/free-look history and returns an owned request. Failed validation leaves
that history unchanged. `LocomotionProps::responsive()` and `outOfNowhere()` provide
the Steered recipes; Strafe and Tank use the same controller. Input axes are finite
values in [-1,1]; combined movement is length-limited without losing analog scale.
`travelIntent` converts desired autonomous/scripted travel into the selected profile
instead of bypassing it with a direct pose write.

Positions and rates follow [world coordinates and frames](WORLDS.md): meters,
seconds, radians and +Y up. Ground movement projects onto the
configured up plane and uses look yaw, not pitched camera forward; looking near
vertical must not collapse the movement basis. Preserve analog magnitude and
limit combined movement to length one. Rates use meters/second or radians/second;
no device sensitivity or frame-count-based increment belongs in locomotion.

Requests identify their simulation tick and movement reference frame. Conversion
to space-relative motion uses the matching FrameSample, including declared frame
velocity inheritance. The motor publishes actual space-relative pose/velocity;
camera smoothing never supplies either. [Navigation](NAVIGATION.md) can supply
desired travel and facing through the same profile evaluation. The app explicitly
arbitrates autonomous, scripted and player control and clears obsolete requests
on takeover, target removal or transfer.

## Profiles

| Profile | Movement | Facing |
|---|---|---|
| Steered | View-relative or body-relative, selected by the recipe | Configured travel heading or view heading while moving |
| Strafe | View/aim-relative, including lateral and backward movement | View/aim heading independent of movement |
| Tank | Forward/backward along body heading; lateral input requests turning | Explicit turn rate, independent of camera orbit |

Tank preserves reverse movement and allows turning while stationary in the
kinematic realization. A physical vehicle may impose different constraints.
Left/right input in Tank is turning, not an additional strafe axis.

Steered has two built-in recipes:

| Recipe | Movement basis | Facing target | Characteristic |
|---|---|---|---|
| Responsive exploration | View heading | Desired world travel direction | Travel changes immediately; visible facing can still turn gradually |
| OutOfNowhere-style | Actual body heading | View heading while moving | Travel curves as the body catches up; lateral/backward inputs remain body-relative |

The default for a new subject-following demo is OutOfNowhere-style with gradual
turning. This is a control preference, not a physical-accuracy guarantee. Material
inspection and free flight retain their own controllers.

Movement-basis and facing choices are validated together. A travel-facing target
must come from a stable desired world direction, not repeatedly from the body's
own newly rotated basis; the latter creates self-chasing turns. Built-in recipes
avoid that feedback. Zero movement preserves heading unless an explicit aim,
turn or recenter policy supplies a target.

There is no separate forward-only-turn hybrid preset. Diagonal and analog input
follow the selected recipe uniformly. Apps can supply different intent or facing
targets without adding another controller implementation.

## Turning and contextual overrides

Turn response is `Immediate`, `RateLimited`, or `Damped`. Rate limits are finite
positive radians/second; damping uses a finite positive half-life. Damped response
uses elapsed-time decay rather than an unclamped per-frame lerp multiplier.
Turn along the shortest angular arc with a stable convention for opposite headings.
Reject invalid configuration before replacing active state; zero elapsed time
does not advance a rate or damping response.

Movement during a turn can remain unrestricted or scale by heading mismatch.
For travel-facing movement, compare body forward with desired travel heading;
for the OutOfNowhere recipe, compare body forward with desired view heading.
Do not penalize intentional backward/sideways movement merely for differing from
body forward. Apps may supply their own speed response within declared limits.

Aiming may temporarily select Strafe; release restores the base profile. Free
look can retain the prior steering/aim reference while changing camera look.
Releasing free look explicitly chooses to keep that reference or ease it toward
the new view heading. It must not rotate the subject by an accidental camera copy.
The app declares override precedence; built-in aim takes precedence over free look.
Contextual overrides reuse eligible movement input; aiming while moving does not
require releasing and pressing the movement control again.

Changing profile preserves position, velocity and view orientation. New requests
take effect at the next movement boundary; a kinematic app may realize a changed
velocity immediately, while another motor may accelerate toward it. The profile
change itself does not teleport, rotate instantly or clear momentum. First/third
person changes do not implicitly change locomotion unless the app pairs them.

## Movement realization and animation

The baseline realization is kinematic: it applies bounded movement and facing
without gravity, contact, slopes or penetration recovery. Following a subject
does not imply that the subject cannot pass through scene geometry. Movement
realization belongs to the app/model, never the renderer or camera director.

`realizeMovement` validates subject/tick/discontinuity, step size and rate limits
before returning actual state and an optional updated frame attachment. It does
not publish World mutations. `MovementRealization::Physics` delegates to the
configured CharacterMotor; absent capabilities return Unsupported with unchanged
published state. `RootMotion` remains Unsupported until its adapter exists. A
zero-duration kinematic step preserves actual pose and velocity.

Movement axes and up direction use the explicitly sampled frame, or space axes
without a frame. The default frame policy includes sampled point/angular velocity
in the requested and reported world velocity. The domain resolves inherited frame
placement before evaluating locomotion; the motor integrates relative travel once
and updates the attachment, avoiding duplicate parent motion. A domain choosing
`inheritFrameVelocity = false` retains world placement instead of first applying
parent motion, and realizes only requested world velocity. Both policies require
a frame sample matching the subject's epoch, space and tick. Space-specific
`SpatialLimits` validate movement and placement independently of render origins.

Simulation advances actual subject state on its declared clock. Presentation
samples `PoseHistory`; desired heading, actual heading and actual velocity are
available to visual turning and animation selection. Teleports reset history and
turn/follow damping. Smoothed visual transforms never feed back into movement.

## Physics movement realization

`CharacterMotorProfile` defines capsule dimensions, up direction, speed/acceleration,
gravity, maximum slope/step, support-motion inheritance, overlap recovery limits and
collision filters. `CharacterMotorRequest` carries subject/region generation, tick,
LocomotionRequest, declared jump intent and required collision coverage. The Jolt
adapter realizes bounded movement against its completed/current step boundary and
returns `CharacterMotorResult`: achieved pose/velocity, grounding/support identity,
blocked/coverage status and tick validity. Actual state feeds the next controller,
navigation follower and camera sample; desired movement never replaces it.

Player, script and PathFollower requests share this interface. The owner defines
motor-versus-rigid-body step order and consumes each request once. Moving support
velocity is inherited once, not also re-applied by frame attachment code. Missing
coverage stops/waits explicitly; failure cannot silently select collision-free
kinematic movement. Teleports/recovery reset motor and interpolation history.
Headless fixtures cover walls, slopes/steps, support removal, frame motion and
Tank/Steered/Strafe requests through the same [physics boundary](PHYSICS.md).

## Future advanced movement

Vehicle dynamics and root-motion realization are **future**. They consume
locomotion requests and publish actual results; they may reject or constrain
requested motion. A vehicle can translate steering
targets into steering angle/curvature rather than rotate its chassis in place.
Root motion may supply displacement, but camera evaluation does not become its
authority. Network authority/replay chooses its own simulation input boundary.

Camera obstruction is separately deferred in
[CAMERAS.md](../render/CAMERAS.md#future-camera-obstruction). A camera query does
not implement character collision or establish that requested movement is safe.
