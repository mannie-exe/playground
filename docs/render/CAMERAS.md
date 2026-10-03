# Camera ownership and evaluation

Camera behavior is application/runtime state, not renderer behavior. These
contracts cover games, editing tools, presentations and 2D pan/zoom views.
[Locomotion](../platform/LOCOMOTION.md) controls movement and facing independently
of camera placement. Neither following a subject nor changing perspective implies
physics or collision support.

## Controller boundaries

| API | Contract |
|---|---|
| `Scene2DViewProps::camera` | Affine scene-to-content mapping |
| `CameraProps` | Resolved 3D eye/target/up and explicit lens values |
| `SceneViewport` | Content bounds, aspect, pixel extent and projection/picking mapping |
| `OrbitController` | Angular/distance intent around an inspection target |
| `FreeCameraController` | Unconstrained local movement/look intent |
| `LookController` | Shared continuous yaw/pitch; separate angular displacement and rate input |
| `FollowCameraRig` | Subject anchors, first/third-person placement, zoom and follow damping |
| `CameraDirector` | Viewport-local source selection and explicit pose/lens hand-off |
| `CameraPath` | Immutable timed looping camera samples |
| `PoseHistory` | Previous/current model poses and presentation interpolation |
| `ViewControlSession` | Viewport input eligibility and scoped pointer ownership; see [runtime](../platform/RUNTIME.md#viewport-control-sessions) |

Controllers consume intent and copied state, never SDL events, GPU handles or UI
nodes. An app owns controllers and their evaluation order. Input can originate
from a user, scripted behavior or replay. Rendering consumes resolved values;
changing one viewport must not mutate a global active camera.

The composition is input ownership -> look/locomotion -> subject presentation
sample -> camera rig -> director -> CameraProps -> viewport. A rig evaluates an
anchor even without an avatar mesh. No dummy scene objects or general rig class
hierarchy are required to keep body heading, orbit and eye placement independent.

## Coordinates and clocks

Pointer coordinates pass through window/UI mapping into viewport content,
including letterboxing. Reject gestures started outside content; an accepted
captured gesture can continue outside until release. Use SceneViewport mapping
for 3D interaction and inverse camera mapping for 2D picking/panning.

`LookIntent` separates `deltaRadians` from `radiansPerSecond`. Accumulate pointer
displacement once; never multiply it by elapsed time. Stick rates integrate once
over their declared interval. `ZoomIntent` similarly separates distance impulses
from held zoom rates. Normalize fractional wheel direction in the input adapter,
not again in the rig. Units, sensitivity and inversion are explicit preferences.

Look state is app-owned control intent. Local viewing applies it without waiting
for a fixed tick; simulation samples the intended heading at its input boundary.
Do not replay the same delta through both frame and tick controllers. Publish
actual subject poses after fixed ticks, then evaluate following cameras against
interpolated presentation poses. Never use smoothed camera output as simulation
input. Replay/authority input timing is declared by the app, not inferred from a
renderer frame number.

Follow position, body presentation and camera orientation have independent
responses. Mouse look is direct by default; follow damping uses elapsed-time
half-lives. Continuous updates must not restart a fixed-duration transition each
frame. Active damping/transitions retain update and paint demand until settled;
idle policy cannot infer motion from a clean layout. Teleports reset history and
relevant damping. Pause/resume rebases time without integrating the suspended gap.

## Follow targets and rig state

`CameraTargetSample` contains a validated subject identity/generation, world pose,
world-space follow and eye anchors, and an explicit discontinuity marker. Resolve
identities in the app/model and pass copied samples. Removed/replaced targets
cancel follow ownership and select an app-declared fallback; retained pointers
cannot extend the target's lifetime. Eye/follow anchors already include model
offsets. The rig applies its shoulder offset once in the look-yaw/up frame, without
inheriting the body's turning a second time.

| API | Contract |
|---|---|
| `LookController::advance(intent, seconds)` | Validate and apply displacement/rates; retain yaw and bounded pitch |
| `FollowCameraRig::setProps(props)` | Validate and replace placement, lens, zoom limits and response settings |
| `FollowCameraRig::advance(target, look, zoom, seconds)` | Evaluate copied target/look samples and publish placement state |
| `FollowCameraRig::adoptView(camera, target)` | Seed reachable look/placement from an outgoing view; expose constrained results |
| `FollowCameraRig::reset(target, look)` | Reset placement/history on teleport or explicit reset |
| `FollowCameraRig::state()` | Requested/effective perspective, requested/resolved distance, transition and target status |
| `FollowCameraRig::camera()` | Resolved CameraProps, independent of renderer allocation |

`FollowCameraProps` contains anchor blending and shoulder placement, lens values,
zoom range, perspective policy, entry/exit thresholds and response half-lives.
Reject nonfinite values, inverted ranges, invalid lenses and unordered thresholds
before mutation. Body rotation must not implicitly rotate the orbit twice through
parent inheritance. Follow anchors describe placement; look yaw/pitch describes
view orientation. A rig can follow translation without following body rotation.

## Perspective and zoom

`PerspectivePolicy` is `Automatic`, `FirstPerson`, or `ThirdPerson`. Effective
perspective and transition progress are separate from that requested policy.
Both perspectives share look yaw/pitch; switching does not restore an unrelated
camera orientation. Pitch constraints apply consistently through transitions.

Requested distance records user zoom intent; resolved distance records displayed
placement. In Automatic mode, enter first person at or below `enterDistance` and
leave it at or above `exitDistance`, with `enterDistance < exitDistance`. Requested
distance remains adjustable in first person even though its placement uses the eye
anchor. The hysteresis gap prevents mode oscillation and does not trap zoom at zero.
A future obstruction correction changes resolved distance only, never perspective
policy or requested distance.

Forced policies do not switch on wheel thresholds. First person retains the
third-person distance preference; zoom is not implicitly FOV magnification.
Aiming/magnification is a separate app action. Perspective transitions blend eye,
follow/shoulder offsets and explicitly configured lens values while preserving
look direction. Reverse an interrupted transition from the displayed state.
Reduced motion removes optional transition/damping effects without suppressing
direct look or changing locomotion semantics.

Self-visibility is viewport-local. `SceneViewProps::visibility` supplies explicit
object exclusions for the observing subject, such as its head or whole body;
exclusions use validated identities and participate in view invalidation. Never
hide the shared Scene3D object globally when entering first person. Visibility
changes affect draw selection, not asset identity or resource recreation. Body
visibility and optional first-person meshes remain app-authored policies.

## Camera selection and hand-off

`CameraDirector` owns resolved source values. Register with `add`, publish with
`update`, and retire IDs with `remove`. IDs are unique across directors and never
reused. Highest enabled priority wins; ties retain the active source, then use the
lowest registration ID. Stale/foreign IDs and invalid values reject before mutation.
Sources own no input, SDL, target lifetime or rendering resources.

Updating the selected source's pose does not initiate or restart a selection
transition. Outside a transition it follows the published pose directly. During a
transition, destination updates preserve elapsed time and its fixed starting pose.
Rig damping handles continuous following. A selection change starts a new finite
blend from the currently displayed pose; interrupting it uses that displayed pose.
Explicit `restartTransition()` requests a new blend to the selected source;
`cut()` snaps to it. `setTransitionProps` configures subsequent selections rather
than silently restarting the active blend.

Call `advance(seconds)` on the declared presentation clock and publish `camera()`.
A zero duration cuts. Perspective/orthographic projection changes cut. Same-mode
blends interpolate position, shortest-arc quaternion orientation, focus distance,
FOV, near/far planes and orthographic height; never projection matrices. Smoothstep
weighting applies to the blend parameter; moving endpoints need not have zero
world velocity. `transitioning()` exposes activity demand.

Removing/disabling the winner chooses the next eligible source. With no source,
select the configured fallback or hold the last view and stop transitioning.
Target removal may require immediate fallback and control revocation rather than
holding a view of an invalid subject.

`CameraReturnPolicy` is `PreserveDisplayedView`, `RestoreSavedView`, or `CutReset`.
The receiving rig adopts the outgoing view where reachable, blends back to saved
state, or resets explicitly. Constraints such as pitch/zoom limits can prevent
exact adoption; report that result and use a declared blend/cut. Suspend obsolete
input accumulation during takeover. Control transfer occurs at an app-declared
boundary independently of blend completion. Camera priority never grants input
ownership; cinematic/benchmark playback explicitly suspends manual channels.

## Native pointer ownership

`WindowServices::lockRelativeMouse(ActivationToken)` requires a focused active app
and returns a `Connection`. Disconnecting releases SDL relative mode; a stale
connection cannot release a newer lock. App/window loss, Settings and teardown
release it and clear pending look/navigation. Acquisition failure is observable;
never pretend capture succeeded. Relative mode is window-local, not
`SDL_CaptureMouse`, and changes on the owner thread.

Mouse eligibility, UI focus, viewport selection and camera selection are distinct.
A deliberate engagement action acquires the viewport and consumes that action;
hover alone does not grant keyboard/gamepad control. Right-click toggles relative
look in the demos. Escape first unlocks, then opens Settings on a later press;
UI/IME cancellation retains its routing priority. Focus return does not recapture.
Gamepad viewing needs eligible viewport ownership but no relative mouse lease.
See [control sessions](../platform/RUNTIME.md#viewport-control-sessions).

## Timed looping paths

`CameraPath(durationSeconds, keys)` owns validated copied `CameraPathKey` values.
Duration is finite and positive; two or more strictly increasing key times begin
at zero and stay below duration. Pose/lens values are finite and valid. The last
segment returns to the first key at loop duration.

`sample(elapsedSeconds)` accepts finite nonnegative time and wraps by duration.
Same-mode segments use smoothstep pose/lens interpolation with zero endpoint
velocity at each fixed key and the seam. This is an authored tour, not a
constant-speed spline. Across projection modes, hold the previous camera until
the next key cuts. Authored paths remain responsible for avoiding geometry.
Named shots store pose/lens values, not matrices or borrowed scene pointers.

## Future camera obstruction

Camera obstruction, static collision baking and physics integration are future
work. No collision backend, spatial index or automatic blocker classification is
required by the follow-rig implementation. Without one, placement is unconstrained
and reports obstruction capability as Unavailable rather than a tested clear path.

The extension boundary is a `CameraObstructionQuery`: copied candidate placement,
anchor, lens footprint, previous valid position, blocker filters and excluded
subject identities enter; supported/clear/blocked/unresolved status and a safe
placement result return. It runs outside rendering and does not grant input or
alter requested zoom. Final director output must also be constrained so blends
cannot cross geometry even when their endpoints are valid.

A later backend may sweep a volume accounting for near-plane extent, retract
immediately and damp outward recovery. Initial overlap/invalid anchors require
explicit failure/fallback behavior. Camera blockers are authored separately from
material opacity and render visibility: walls or invisible proxies may block,
foliage may not, and the observing subject is excluded. Query data is revisioned,
accounted and prepared once, not rebuilt with camera motion. A missing backend
must not be substituted with bounds picking and described as collision safety.

Character contacts, grounding, vehicle dynamics and root motion are separate
future [movement realizations](../platform/LOCOMOTION.md#future-physics-and-root-motion).
Camera obstruction alone cannot prevent a subject moving through a wall.

## References

[PhantomCameraHost](https://phantom-camera.dev/core-nodes/phantom-camera-host)
and [follow modes](https://phantom-camera.dev/follow-modes/overview) inform source
selection and placement boundaries. [Godot's spring-arm guidance](https://docs.godotengine.org/en/stable/tutorials/3d/spring_arm.html)
informs the deferred query boundary. These are design references, not dependencies.
