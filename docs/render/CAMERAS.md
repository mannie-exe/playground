# Camera ownership and evaluation

Camera behavior is application/runtime state, not renderer behavior. These
contracts cover games, editing tools, multimedia presentations and ordinary 2D
pan/zoom views. They do not require a gameplay scene tree or a physics engine.

## Controller boundaries

| Contract | Meaning |
|---|---|
| `Scene2DViewProps::camera` | Affine mapping from scene coordinates to local logical content coordinates |
| `CameraProps` | 3D eye/target/up and perspective or orthographic lens settings |
| `SceneViewport` | Resolved content bounds, aspect, pixel size and projection/picking mapping |
| `OrbitController` | Explicit angular/distance deltas, pitch/distance constraints, resolved CameraProps |
| `FreeCameraController` | Explicit local movement/look intent, world-up constraints and resolved CameraProps |
| `MovementController` | Kinematic direction and units-per-second advancement, no collision solver |
| `PoseHistory` | Fixed-step previous/current poses and interpolated presentation sampling |
| `InputClaims` | UI ownership checked before application/controller input |

Controllers consume intent rather than SDL events, network packets, GPU handles
or UI nodes. An app owns each controller and determines its target and call order.
The same intent may originate in a user, a script, recorded playback or a remote
model. Rendering only consumes the resulting camera values. Each viewport has
its own camera; changing one must not mutate a global active camera.

## Coordinates and clocks

Pointer coordinates pass through window/UI mapping into the viewport's local
content coordinates, including letterboxing. Reject gestures started outside the
content area; an accepted captured gesture may continue outside until release.
2D inverse camera mapping handles scene-space picking/panning. 3D uses the resolved
viewport's normalizedPosition/rayAt helpers rather than window dimensions.

Pointer motion is displacement; accumulate it once and do not multiply it by dt.
Held keyboard/stick intent is a rate and must be integrated over the chosen clock.
Wheel steps are impulses. Sensitivity, inversion and rate units belong to the
input adapter/controller configuration, not the shader or projection matrix.

Publish model poses after fixed ticks, then sample presentation poses and evaluate
following cameras in a declared order. Do not feed an interpolated camera/target
back into authoritative simulation. UI and presentation animations can use a
monotonic presentation clock independent of paused simulation. Document which
clock a camera follows; an art presentation need not have fixed-step simulation.

Active transitions/damping require update and paint demand until settled. Idle
policy cannot infer animation from a clean layout. Focus/device/app loss cancels
input gestures, not necessarily scripted camera playback. Teleports reset relevant
history/damping instead of interpolating across discontinuities.

## Extension design, not yet implemented

The intended composition is intent -> controller/rig -> viewport-local director
-> resolved camera -> renderer. Orbit and free-camera controllers establish
independent consumers before a generic rig hierarchy is introduced. A rig owns
target identity, behavior, lens configuration and smoothing state. A director owns selection/transition state, with explicit
priority ties, interruption behavior and a fallback if a target or rig disappears.
Use validated object identities; do not retain borrowed pointers across removal.

Blend positions, quaternion orientations and declared lens fields, not projection
matrices. Perspective/orthographic switches default to an explicit cut until a
deliberate transition is designed. Damping should specify a time-based response
(such as half-life), not a frame-dependent lerp fraction. Camera collision arms
query a separate spatial/collision service. They are not mesh rendering features.

A future pointer controller needs a scoped drag/relative-input lease. Record its
pointer, viewport and activation owner; release capture/relative mode on gesture
end, cancellation, teardown or failure. UI pointer ownership is not OS relative
mouse mode. Do not enable relative mode globally simply because a 3D view exists.

Material Test uses keyboard-step orbit. Bistro and Chess use free-camera intent
with rate-based movement/look and scene reset. Pointer orbit, follow, camera
blending, collision arms and a director remain future work. Controller tests cover
finite inputs, pole limits, diagonal speed and time subdivision; pointer lifecycle
and transition tests belong with their eventual implementations.

## References

[PhantomCameraHost](https://phantom-camera.dev/core-nodes/phantom-camera-host)
separates behavior selection from output cameras;
[follow modes](https://phantom-camera.dev/follow-modes/overview) distinguish
following/framing policies. These inform the boundary, not a Godot dependency.

## Named shots and workloads

A CameraShot contains a stable authored name, pose and lens. A viewport-local
CameraDirector selects eligible rigs by explicit priority; ties retain the active
rig, otherwise use stable registration order. Removing the selected rig resolves
a declared fallback or retains the last valid camera. Retargeting starts from the
currently displayed pose. Follow and look-at targets resolve independently and
invalid targets never leave dangling pointers. These director/transition features
remain future work alongside pointer orbit and follow behavior.

The [repeatable scene workload](DEMOS.md#repeatable-camera-workloads-future) is
future work. Its camera samples must depend on explicit elapsed time and recorded
inputs, not frame count. Interactive and recorded intent share controller math;
benchmarks do not require a global active camera or a second playback renderer.
