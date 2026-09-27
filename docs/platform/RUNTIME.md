# Runtime input, simulation and lifetime contracts

Applications remain C++ authored. These facilities do not introduce a gameplay
scene tree, renderer-owned behavior, automatic parallel simulation or serialized
applications. AppHost supplies boundaries; each app owns its model and controllers.

Update and presentation cadence are independent of simulation timing. See
[ACTIVITY.md](ACTIVITY.md) for explicit app demand, UI deadlines, completion wakes,
full-frame idle skipping and pending presentation acknowledgement.

## Input

`input::InputMap` owns named button/scalar/vector actions and ordered contexts.
Contexts choose BeforeUI (shortcuts/modal overrides) or AfterUI (gameplay). Higher
priority runs first; equal priority follows insertion order. Consumption blocks
lower contexts and, for BeforeUI actions, UI routing. UI-consumed events cannot
activate AfterUI bindings. Physical releases still clear earlier holds.

The SDL adapter translates physical scancodes, mouse buttons, gamepad buttons and
normalized gamepad axes. Text/IME and pointer hit testing stay with UI. Relative
pointer motion remains a UI/controller input, not a persistent action axis.
Bindings specify signed contribution/scale and dead zone. Context addition,
removal, property changes and rebinding conservatively cancel all current holds
in that map, suppressing already-held inputs until neutral. Repeat is not a new press.
Multiple bindings aggregate, with scalar clamp and vector length limiting.

Frame/event and fixed-tick snapshots latch edges independently. Taking a snapshot
clears its edges, not held values. Thus zero-tick frames retain presses and catch-up
ticks consume a press once. Cancellation is distinct from release, including
focus loss, device removal, disabled contexts and app exit. Bindings are runtime
values; persistence, binding-capture UI, gestures and response curves are deferred.
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

## Timing and ordering

Apps opt into `IApp::simulationTiming()` returning `SimulationTimingProps`; the
default is frame updates only. The policy is captured during activation.
The host routes events, drains owner-thread completions, runs bounded fixed ticks,
updates frame-based UI, then renders. Commands and app replacement run only after
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
the untouched tail remains queued. Workers capture owned inputs/results and weak
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
values or CameraProps, without SDL, GPU objects or renderer submission. Movement
is kinematic, not collision/physics. Orbit angles are radians; world units and +Y
up match scene math. A pose history supports interpolation and teleport reset.
Player input, AI and replay may all supply the same intent types.
`MovementController` limits direction magnitude to one, preserving analog input,
and advances at units/second. It provides no gravity/contact solver.
`OrbitController` clamps pitch away from the poles and distance to positive
bounds. Intent contains angular deltas in radians and additive zoom distance;
callers convert device values or rates to deltas. `camera(lens)` preserves lens
settings while replacing eye/target/up. `PoseHistory` linearly interpolates
position/scale and shortest-sign normalized quaternion orientation (nlerp, not
constant-angular-speed slerp). Published and teleported poses are validated first.

## API and composition

| Header | Owner / entry points |
|---|---|
| `input/InputMap.hpp` | IApp owns a map; `addContext`, `contextProps/setContextProps`, `bindings/rebind`, `setEnabled/removeContext` |
| `platform/sdl/SDLActionInput.hpp` | SDL physical event translation and cancellation; host calls `routeInputEvent` around UI |
| `runtime/SimulationClock.hpp` | Optional per-app clock; `beginFrame/nextStep`, `setPaused`, `state/reset` |
| `runtime/ActivationLifetime.hpp` | Owner activation; tokens are weak, generations are separate control-block identities |
| `runtime/CompletionQueue.hpp` | Thread-safe posting, owner-thread draining; guarded `ActivationSink` |
| `runtime/DeferredMutations.hpp` | Owner-thread `defer/remove/flush` at application-chosen safe boundaries |
| `scene/Controllers.hpp` | Kinematic movement, orbit camera and previous/current pose history |

For a simulated app, register digital/analog bindings in its constructor, return
timing props, and advance an owned pose in `fixedUpdate`. For example, inside that
hook (with owned `movement` and `poses` members):

```cpp
auto pose = poses.current();
const auto axis = actions["move"].value;
pose.position = movement.advance(pose.position, {{axis.x, 0, axis.y}}, step.seconds);
poses.publish(pose);
```

Before rendering, obtain `ctx.simulationState()->alpha`, sample the history and
apply its result to a Scene3D object's transform. Never feed that interpolated
render pose back into simulation. The focused controller test exercises precisely
this path with one moving object, without opening a window or adding a demo game.

For asynchronous work, capture `auto delivery = ctx.completions()` before starting
the worker. Post a closure owning its prepared result; it may borrow app state only
inside that owner-thread closure, guarded by the captured activation endpoint.
Handle `post` returning false: it means bounded-queue refusal or a dead activation,
not successful publication. Per-request supersession (for example ModelPreparation)
is still separate from the lifetime of the whole app activation.
Renderer resource-domain compatibility is another independent check: activation
tokens do not make native resources survive backend replacement.

## Verification

Focused constituent tests cover context consumption, rebinding, cancellation,
multiple bindings, edge latching, SDL translation, timing limits/pause, stale
delivery, deferred removal and controller arithmetic. App behavior is additionally
build-checked; ordinary tests do not open windows or modify desktop settings.
The focused cases are `input_actions`, `sdl_actions`, `simulation_clock`,
`activation_lifetime`, `controllers`, and the rollback cases in `host_transitions`.
