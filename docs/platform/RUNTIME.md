# Runtime input, simulation and lifetime contracts

Applications remain C++ authored. These facilities do not introduce a gameplay
scene tree, renderer-owned behavior, automatic parallel simulation or serialized
applications. AppHost supplies boundaries; each app owns its model and controllers.

Update and presentation cadence are independent of simulation timing. See
[ACTIVITY.md](ACTIVITY.md) for explicit app demand, UI deadlines, completion wakes,
full-frame idle skipping and pending presentation acknowledgement.
[Session services](NETWORKING.md#scheduling-and-host-services) process work
independently of local presentation pause.

## Service processing

AppHost owns a runtime ServicePump. App activation creates a ServiceScope for
content acquisition, audio control and network/session work. Services publish
immutable results from workers and mutate owned state only at their declared
owner boundary. Device audio mixing and socket I/O do not execute in UI update.

| API | Contract |
|---|---|
| ServiceScope | Activation-owned registrations and cancellation; destruction removes demand and rejects late publication |
| ServiceHandle | Retained terminal status/error and individual cancellation; does not retain callbacks after closure |
| ServiceDemand | Pending work and absolute monotonic wake deadline; no deadline invented by a query |
| ServiceWorkBudget | Per-pump work/byte/message limits; exhaustion preserves pending demand |
| ServicePump::demand | Combine service deadlines with application, window and rendering demand |
| ServicePumpProps | Bounded registrations, visits and shared work allowance; each registration can lower its own allowance |
| ServicePump::advance | Process ready bounded work before simulation/presentation, including while Settings is open |
| AppContext::services | Borrow active scope for owner-thread registration; workers retain only safe posting endpoints |

Registration specifies a handler, demand source and work limits; a service cannot
recursively pump or mutate registration during traversal. Changes publish at a
safe boundary. Fair rotation prevents one busy service from starving another.
Completion notifications do not own the only result copy; saturation preserves
durable outcomes. Terminal failures are delivered once with service/instance
identity. Closing a scope cancels pending work and invalidates publication before
resource destruction; it does not forcibly interrupt a native decoder.
`ServiceScope::wakeCallback` is safe to copy to workers and becomes inert when
the scope or pump closes. Publish into synchronized service-owned result storage
before waking; the wake itself transfers no result. Service wakes/deadlines do
not imply local app updates or paints. A service requests those explicitly when
it publishes visible changes. Cancellation may close another scope; cleanup
finishes before the outer closure returns. Closing from within a handler defers
cleanup until traversal returns, so callbacks cannot destroy their own owner.
`takeFailures` reports each terminal exception once; `stats` retains work and
failure counts. Services must honor every granted limit, including zero remaining
bytes/messages; an overreported allowance fails that registration.

Keep local simulation, authority/session ticks, UI monotonic time, audio sample
time and presentation time distinct. Settings/focus pause local app input and
simulation according to local policy; network authority and service processing
continue. An online model advances through its AuthorityRunner, not the paused
local SimulationClock. Headless hosting uses the same service/domain contracts
without the desktop IApp/SDL window adapter.

## Spatial work and clocks

[DatasetService](SPATIAL_DATA.md#sources-and-preparation),
[ProductService](SPATIAL_PRODUCTS.md) and edit/save services share Executor,
ServiceScope, wake endpoints and ResourceLedger. Policy queues bound and coalesce
work before executor admission; they do not create private competing pools.
Latest, Ordered, Deadline, Background and Durable work have explicit overload and
retention behavior. Completion precedes its wake, and publication validates epochs,
input stamps and request policy at the owner boundary.

ServiceWorkBudget bounds owner-side staging/publication. Prepare large metadata or
scene candidates incrementally, then exchange validated roots/handles; do not hide
unbounded work behind one counted operation. PreRender never waits for preparation.
Input, settings and window transitions remain serviced while work is queued.

[Simulation kernels](SIMULATION.md) choose local interactive time or an independent
ordered authority clock. Only domains explicitly permitting dropped steps use the
interactive catch-up policy. Services can prepare while paused; authoritative edits
and physics activation await their declared model boundary. Standalone/headless
dataset tools pump these services without AppHost or SDL. Real-time audio producers
use bounded preallocated handoff, not these allocation/publication calls directly.

## Physics execution domain

PhysicsService participates in ServiceScope admission, wakes and publication but
owns a budgeted solver execution domain. Generic data/collision cooking uses the
shared Executor; Jolt stepping uses supported job dependencies/barriers off the UI
thread. CPU policy accounts for content workers, solver concurrency and native I/O
together. Do not spawn a hardware-sized pool per region or recursively wait on
jobs queued behind a saturated independent-job executor.

One admitted native step owns its command batch, coverage/shape leases, scratch and
result/event buffers until retirement. While it runs, input, windows, settings,
network receipt and background preparation continue. The world owner does not run
a later authoritative tick, mutate active collision or publish predicted poses as
actual results. Complete results advance the committed tick; RefusedBeforeStep
permits corrected admission, while Compromised faults require explicit recovery.

Read scopes and collision activation execute between native steps. Closing an app
invalidates publication first; shared execution ownership retains native state
until completion. Process shutdown joins solver work before destroying Jolt's
coordinated runtime. The normal host render/update path never blocks on that join.
See [physics state and recovery](PHYSICS.md#step-validity-and-recovery).

## Input

### Ownership policy

Event handling and persistent input ownership are distinct. `InputClaims` reserves
keyboard, gamepad, all pointers, or individual captured pointers for UI. Editors
reserve the keyboard; modal scopes reserve all three domains. Decorative UI
reserves none. `IApp::inputClaims()` exposes the current UI/session claims to the
host; apps without UI return empty claims. BeforeUI contexts remain explicit
global overrides, not ordinary gameplay shortcuts.
Keyboard reservation includes unhandled Escape while editing: use UI navigation
to leave the field, or explicitly design a local escape/focus policy. Do not move
application Escape bindings to BeforeUI merely to bypass IME/modal cancellation.

The host refreshes claims around input dispatch and before simulation/after model
updates, including completion-driven changes. Newly claimed AfterUI controls are
canceled immediately, with pending edges cleared for affected actions. Unrelated
actions remain active. Releasing a claim never synthesizes a press: an already-held
control must return to neutral first. Physical release bookkeeping continues even
when input is reserved. UI still receives the event; ownership is not a substitute
for capture/target/bubble handling or text/IME delivery.
`onActions` receives an additional snapshot on ownership transitions outside event
dispatch, so cached controller intent can observe cancellation without waiting for
another event. Tick snapshots retain their independent edge latch.

Stable root/geometry/focus revisions reuse the ownership summary. Pointer captures
are read independently on each query; newly captured/released pointers cannot be
hidden by the cache. Custom eligibility changes must invalidate the owning node.
Actions aggregating several devices have aggregate edges; cancellation of a claimed
binding may clear pending edges of that same action, but not unrelated actions.

Sequential navigation reserves Tab only when focus exists/can be obtained or a
modal owns navigation. Removal/background cancellation must reach both InputMap
and retained UI. Device removal currently cancels UI interactions conservatively;
per-device UI keyboard ownership is not yet represented.

`input::InputMap` owns named button/scalar/vector and transient delta actions
with ordered contexts.
Contexts choose BeforeUI (shortcuts/modal overrides) or AfterUI (gameplay). Higher
priority runs first; equal priority follows insertion order. Consumption blocks
lower contexts and, for BeforeUI actions, UI routing. UI-consumed events cannot
activate AfterUI bindings. Physical releases still clear earlier holds.

The SDL adapter translates physical scancodes, mouse buttons, gamepad buttons and
normalized gamepad axes, relative pointer motion and fractional wheel deltas.
Text/IME and pointer hit testing stay with UI. Motion/wheel values are transient
displacement, never persistent held axes. Bindings declare signed scale and the
appropriate button, rate or delta semantics. Paired sticks use radial dead zones
and response curves; do not apply an additional per-axis dead zone first.
Context addition, removal, property changes and rebinding cancel current holds
in that map, suppressing already-held inputs until neutral. Repeat is not a new press.
Held bindings aggregate with scalar clamp and vector length limiting. Delta
actions accumulate without unit-vector clamping and clear after consumption or
cancellation. Normalize wheel direction once; one event cannot both scroll UI and
zoom a viewport.

Frame/event and fixed-tick snapshots latch edges independently. Taking a snapshot
clears its edges, not held values. Thus zero-tick frames retain presses and catch-up
ticks consume a press once. Cancellation is distinct from release, including
focus loss, device removal, disabled contexts and app exit. Bindings are runtime
values; arbitrary binding persistence, binding-capture UI and general gestures
remain deferred. Shared control preferences configure stick response and sensitivity.
Snapshots coalesce edges into booleans: multiple complete taps between ticks mean
"at least one press/release", not an ordered event log. Event callbacks receive
one frame snapshot after each SDL event; ticks have a separate latch. Removing a
device clears its held controls and pending edges for its bound actions. There are
no arbitrary modifier/chord expressions yet; UI modifiers retain existing behavior.

AppHost initializes and owns SDL gamepads through removal. No per-device selection
UI is imposed; optional binding device IDs select an instance for the current run.
Device IDs are not persistent hardware identities. Already-held controls are
transferred as suppressed physical state when switching apps. Gamepad disconnect
and keyboard/mouse removal cancel their matching actions.

### Viewport control sessions

`ViewControlSession` composes the existing InputMap, UI claims, activation token
and WindowServices lease. It does not introduce a parallel event router. Bind it
to one viewport, logical player and explicit runtime device assignment. Multiple
viewports do not share a global active camera/player. SDL device instance IDs are
runtime handles, not persistent player identities. `InputDevice` ID zero groups
local keyboards/mice for a desktop viewport; nonzero IDs select a specific device.
Gamepads always use explicit instance IDs. `physicalValue(kind, code)` inspects
the strongest physical value across a grouped keyboard/pointer channel.

| API / state | Contract |
|---|---|
| `engage` | Validate activation, window focus, viewport eligibility and device assignment; consume the initiating gesture |
| `suspend(reason, channels)` | Cancel selected channels and transient input; release the mouse lease when mouse look is affected |
| `release` | Revoke session ownership; old tokens cannot cancel a later engagement |
| `state` | Inactive, Active or Suspended, with focus/UI/device/app/target/explicit reason |
| `channels` | Movement, Look, Zoom and action ownership; per-channel device source and neutral/rearm state |

Window focus, UI focus, viewport selection, device/channel ownership and camera
presentation priority are separate. Hover alone grants no gameplay ownership.
A gamepad can engage an explicitly selected viewport without mouse capture.
A failed mouse acquisition leaves mouse look inactive with a reported reason;
other already-authorized channels need not fail with it.

Modal Settings and window/background loss suspend local control. Partial UI
claims cancel only affected channels. Returning focus or closing Settings never
synthesizes engagement, a press or a held stick. Buttons/sticks must return to
neutral before rearming; a held capture button must be released before engaging
again. Physical release bookkeeping continues while blocked. Clear app-cached
intent and accumulated motion as well as InputMap holds. Device removal cancels
that device's channels; another controller is not silently reassigned. Teardown
and subject removal revoke the session.

Assigned devices can cooperate, such as keyboard movement with gamepad look.
Within a channel, meaningful intentional input may take over from another device;
noise, repeats and synthetic cursor warps cannot. Radial activation/release
thresholds provide hysteresis. A displaced stick that loses ownership must return
to neutral before retaking it. Clear the departing source's unconsumed deltas;
apply the winning source's new input once, preserving the current view pose.
Last-used-device prompts do not change assignments, capture or channel ownership.
Background controller input remains disabled for local gameplay.

`Binding::cadence` selects a delta stream’s one frame or fixed-tick consumer.
`physicalValue(Control)` inspects normalized raw state without consuming a
snapshot; use it for neutral rearming even while a binding is suppressed. Untaken deltas
survive zero-tick frames; consumption happens once, not once per catch-up tick.
Inspection cannot consume input, and frame/event snapshots must not accidentally
clear a tick-owned delta. Local look can update at presentation cadence and expose
its intended heading for simulation to sample without integrating its deltas again.
The delta contract is distinct from the independent button-edge latches.

Cinematic control takeover cancels manual channels at an explicit boundary.
Returning control follows the receiving rig's [hand-off policy](../render/CAMERAS.md#camera-selection-and-hand-off),
with neutral rearming and explicit mouse re-engagement. Director priority alone
neither captures input nor decides whether an app pauses simulation.

## Timing and ordering

Apps opt into `IApp::simulationTiming()` returning `SimulationTimingProps`; the
default is frame updates only. The policy is captured during activation.
The host routes events, pumps ready services and owner-thread completions, runs
bounded local fixed ticks, updates frame-based UI, then renders. Commands and app replacement run only after
callbacks return. A fixed tick receives tick number, fixed delta and input snapshot.
UIRoot retains its own completion/timer delivery during UI update. Model work that
must publish before simulation should use the app activation sink, not a UI queue.
Use `IApp::fixedUpdate` for simulation and `IRuntimeObject::update` for frame work.
`AppContext::simulationState()` exposes tick, interpolation alpha, cumulative
dropped seconds and effective pause; it is empty for frame-only apps.
`setSimulationPaused` is app pause combined with focus pause, not renderer pause.
Fixed callbacks and frame update run as a bounded batch before pending host
commands execute; request a pause if later ticks in that batch must be skipped.

The clock validates finite nonnegative elapsed time, caps accepted frame duration,
and limits ticks per frame. Excess whole steps are discarded and counted as dropped
time; the fractional remainder supplies interpolation alpha. This local-interactive
policy is not a network lockstep guarantee. Pause discards pending whole ticks but
preserves the interpolation fraction so the displayed pose does not snap backward;
UI, events and completions continue. Resume rebases wall time and retains only that
fraction, never paused wall time. Focus loss pauses ticks.

Apps retain previous/current model poses and publish an interpolated presentation
pose; renderer state is never the simulation authority. Spawn/teleport sets both
poses to the same value. Controllers have explicit call order, not an implicit
registration-order scheduler. Fixed steps alone do not guarantee determinism.

## World simulation boundary

One world owner applies model changes. On each simulation tick: accept validated
domain commands and prepared generations, resolve frame samples, evaluate autonomous
or player movement intent, realize motion through the selected kinematic/physics
adapter, evaluate post-movement queries/zone events, then publish WorldSnapshot.
Navigation plans against an identified snapshot; late plans cannot mutate the tick
that requested them. Physics-generated events become bounded domain work at the
next declared mutation boundary, not recursive scene edits from solver callbacks.
An app's explicit dependency order can refine these stages without making service
registration order authoritative. See [worlds](WORLDS.md), [navigation](NAVIGATION.md)
and the [physics boundary](PHYSICS.md). Native realization can span multiple service
pumps: retain the staged model inputs, launch the admitted step, then finish the
tick only when its valid result arrives. This ordering does not require a blocking
solver call inside IApp::fixedUpdate. Authorities use their ordered runner rather
than letting the desktop catch-up loop dispatch overlapping physics ticks.

Streaming/generation and save I/O progress through scoped services while local
simulation is paused. Prepared data can stage during pause; simulation activation,
transfers and authoritative edits commit only at an explicit model boundary.
Render-only readiness can publish without advancing simulation. Headless/online
authority uses its own clock and coverage requirements. Missing mandatory world
data produces explicit waiting/admission failure, never simulation through a gap.

After simulation publication, presentation samples a consistent epoch/space and
discontinuity sequence, then evaluates cameras, render extraction and audio control
snapshots. Origin rebasing changes representation, not world time or pose history.
Save/load and world replacement invalidate stale epoch-scoped work before releasing
the old owner. Transfer commits and restoration cannot interleave half-published
entity sets with queries or replication.

## Lifetime and background publication

`ActivationLifetime` owns an activation; weak `ActivationToken`s do not extend it.
Deactivate invalidates tokens immediately; reactivation produces a new identity.
AppHost activates before onEnter and invalidates before onExit. A failed candidate
is invalidated; a previous app retained by rollback keeps its original activation
and queued work because it never exited. Guarded completion posts check validity again at delivery.
Cancellation saves work; token validation prevents stale publication.
Invalidating an activation does not forcibly stop workers. Retain TaskTickets and
cancel them during onExit when stopping the underlying CPU work is desirable.

Completion callbacks execute only at owner-thread boundaries. They may enqueue
more work but cannot recursively drain. A failed callback is attempted once and
the untouched tail remains queued. Replaced wake callbacks and discarded work
release their captures outside the queue mutex, so capture destructors may safely
access the queue. Workers capture owned inputs/results and weak
tokens, never borrowed mutable app/UI pointers to access from worker threads.
AppHost logs completion failures and retries the untouched tail on a later frame;
it cannot undo partial changes inside the failed callback. Simulation/update
exceptions remain application errors and are not silently replayed.
Token checks are not locks: activation/destruction and callback execution must
share the owner thread. Existing node/object generational handles remain the
mechanism for identifying individual UI/scene objects.
AppContext's `completions()` returns an `ActivationSink` safe to copy to a worker;
`activationToken()` can also guard other owner-thread publication paths. Do not
capture AppContext itself: it is a transient borrowed facade. App exit receives
the exiting app's token/sink, not those of the newly activated replacement.

`DeferredMutations` marks removal by invalidating a supplied lifetime immediately
and queues destruction for the next explicit flush. Additions become active only
when their queued operation runs. No universal entity base is required.
Queue admission failure leaves the owner active. Once admitted, removal marks it
inactive even if the eventual destruction callback throws; failed callbacks are
not automatically replayed. Existing UI deferred mutations are retained rather
than replaced by a second world-wide queue.

## Controllers

Concrete movement and orbit-camera controllers consume intent and write model
values or WorldCamera, without SDL, GPU objects or renderer submission. World
placement preserves double precision; local CameraProps is a render conversion.
Movement is kinematic, not collision/physics. Orbit angles are radians; meters and
+Y up follow the world contract. A pose history supports interpolation and
teleport reset.
Player input, AI and replay may all supply the same intent types.
[Locomotion profiles](LOCOMOTION.md) define Steered, Strafe and Tank requests,
turn responses and movement realization. [Camera rigs](../render/CAMERAS.md)
consume subject samples independently of those profiles.
`MovementController` limits direction magnitude to one, preserving analog input,
and advances at meters/second. It provides no gravity/contact solver.
`OrbitController` clamps pitch away from the poles and distance to positive
bounds. Intent contains angular deltas in radians and additive zoom distance;
callers convert device values or rates to deltas. `camera(lens)` preserves lens
settings while replacing world placement/orientation. `PoseHistory` linearly
interpolates same-space double position and shortest-arc quaternion orientation
(slerp). Its presentation-only `PoseSample` excludes authoritative velocity and visual scale.
Published and teleported poses are validated first. Epoch, space or discontinuity
changes reset history; an origin-only change does not.

## API and composition

| Header | Owner / entry points |
|---|---|
| `input/InputMap.hpp` | IApp owns a map; `addContext`, `contextProps/setContextProps`, `bindings/rebind`, `setEnabled/removeContext` |
| `platform/sdl/SDLActionInput.hpp` | SDL physical event translation and cancellation; host calls `routeInputEvent` around UI |
| `runtime/Services.hpp` | Host-owned pump; activation-owned scopes, demand, bounded advancement and cancellation |
| `runtime/SimulationClock.hpp` | Optional per-app clock; `beginFrame/nextStep`, `setPaused`, `state/reset` |
| `runtime/ActivationLifetime.hpp` | Owner activation; tokens are weak, generations are separate control-block identities |
| `runtime/CompletionQueue.hpp` | Thread-safe posting, owner-thread draining; guarded `ActivationSink` |
| `runtime/DeferredMutations.hpp` | Owner-thread `defer/remove/flush` at application-chosen safe boundaries |
| `scene/Controllers.hpp` | Kinematic movement, orbit/free cameras, camera director/path and pose history |

For a simulated app, register digital/analog bindings, return timing props and
publish actual world state in `fixedUpdate`. `LocomotionRequest` expresses desired
movement; the selected motor returns the realized `EntitySample`. Publish that
result through `World::apply`, then pass the snapshot sample to `PoseHistory`.
Before rendering, sample history with `ctx.simulationState()->alpha` and pass the
presentation sample to `cameraTarget`/`FollowCameraRig`. The renderer receives a
world extraction with its captured origin. Never feed the interpolated camera or
presentation pose back into authoritative simulation. `world_workflow` exercises
this sequence without a window; the scene camera workload exercises native input
handoff and rendering.

For asynchronous work, capture `auto delivery = ctx.completions()` before starting
the worker. Post a closure owning its prepared result; it may borrow app state only
inside that owner-thread closure, guarded by the captured activation endpoint.
Handle `post` returning false: it means bounded-queue refusal or a dead activation,
not successful publication. Per-request supersession (for example AssetPreparation)
is still separate from the lifetime of the whole app activation.
Renderer resource-domain compatibility is another independent check: activation
tokens do not make native resources survive backend replacement.

## Verification

Focused constituent tests cover context consumption, rebinding, cancellation,
multiple bindings, edge latching, SDL translation, timing limits/pause, stale
delivery, deferred removal and controller arithmetic. App behavior is additionally
build-checked; ordinary tests do not open windows or modify desktop settings.
The focused cases are `input_actions`, `sdl_actions`, `view_controls`, `simulation_clock`,
`activation_lifetime`, `controllers`, and the rollback cases in `host_transitions`.

Service tests use fake time to verify deadline aggregation, fairness under queue
pressure, wake-after-publication and one terminal result per request. Exercise
Settings, focus loss, app replacement and shutdown with active services; local
simulation may pause without stopping authority ticks or losing retained results.
