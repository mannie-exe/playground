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
| `CameraDirector` | Viewport-local priority selection, stable IDs, cuts and timed pose/lens transitions |
| `CameraPath` | Validated immutable looping keys sampled at explicit elapsed seconds |
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

## Camera selection and transitions

`CameraDirector` owns resolved camera values rather than borrowed rig pointers.
Register `CameraSource` values with `add`, change them with `update`, and retire
IDs with `remove`. IDs are unique across directors and never reused; stale and
foreign IDs reject without mutation. Sources have integer priority and an enabled
flag. The highest enabled priority wins. Ties retain the active source, then use
the lowest registration ID. Changes resolve immediately; call `advance(seconds)`
on the chosen clock and publish `camera()` to the viewport.

A selection change starts from the currently displayed camera, including when a
transition is interrupted. Active source updates also restart from that camera.
A zero transition duration cuts. Changing perspective/orthographic mode cuts
immediately. Same-mode transitions interpolate eye position, shortest-arc
quaternion orientation, focus distance, vertical FOV, near/far planes and
orthographic height. They never interpolate projection matrices. Smoothstep time
weighting gives zero endpoint velocity. `transitioning()` exposes paint demand.

Removing or disabling the winner selects the next eligible source. If none
remain, an optional camera configured with `setFallback` is selected; otherwise
the last displayed camera is held and any transition stops. Invalid source,
fallback and time input is rejected before changing state. Sources own no SDL,
rendering, input or target lifecycle resources.

## Timed looping paths

`CameraPath(durationSeconds, keys)` validates and owns copied `CameraPathKey`
values. Duration must be finite and positive. At least two keys are required;
key times start at zero, increase strictly and stay below duration. Cameras must
have finite valid pose and lens fields, including FOV in orthographic mode.
The last segment returns to the first key at the loop duration.

`sample(elapsedSeconds)` accepts finite nonnegative elapsed seconds and wraps by
duration. It is independent of frame count, invocation order and time subdivision.
Same-mode segments use the director's pose/lens interpolation with smoothstep
weighting. Each key is a deliberate brief stop: position and orientation have
zero endpoint velocity, including at the loop seam. This is a smooth authored
tour, not a constant-speed spline. Across projection-mode changes, the previous
camera is held until a cut at the next key. Application-authored paths remain
responsible for scene framing and avoiding walls; no collision solver is implied.

## Future rig extensions

The composition is intent -> controller/rig -> viewport-local director ->
resolved camera -> renderer. Orbit and free-camera controllers establish
independent consumers without requiring a generic rig hierarchy. Follow targets,
look-at policies, damping and collision arms remain future work. Resolve target
identities in an application or spatial service and never retain borrowed pointers
across removal. Damping should specify a time-based response such as half-life.

Pointer adapters own scoped drag/relative-input lifetimes separately from camera
math. Release input capture on gesture end, cancellation, teardown or failure.
UI pointer ownership is not OS relative mouse mode. Each application chooses
its input adapter and presentation clock.

## References

[PhantomCameraHost](https://phantom-camera.dev/core-nodes/phantom-camera-host)
separates behavior selection from output cameras;
[follow modes](https://phantom-camera.dev/follow-modes/overview) distinguish
following/framing policies. These inform the boundary, not a Godot dependency.

## Named shots and workloads

Applications may associate stable authored shot names with camera source IDs.
Interactive and recorded intent share controller math; repeatable workloads sample
`CameraPath` at explicit elapsed time and use the ordinary camera/renderer path.
They do not require a global active camera or a second playback renderer.
