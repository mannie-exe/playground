# Application activity and full-frame presentation

Applications declare update and paint policy separately. Continuous behavior is
the compatibility default; an on-demand app must declare scheduled work, visual
changes and queued completions. An unchanged UI does not imply a paused game.

The host processes events, ready services and completions, runs requested/due updates,
settles UI work, and renders a complete frame only when demanded. It waits for an
SDL event or the earliest deadline when no immediate work remains. Timer deadlines
use monotonic time; UI scheduler delays are translated at the session boundary.
Simulation keeps its independent bounded catch-up/pause contract.

## Public seams

| Contract | Responsibility |
|---|---|
| `runtime::ActivityProps` | Independent `continuousUpdate` and `continuousPaint`, both true by default |
| `IApp::activityProps()` | Opt-in cadence; Menu, Demo 2D and Minesweeper use on-demand UI cadence |
| `runtime::ActivityDemand` / `IApp::activityDemand()` | Current update/paint demand and optional absolute steady-clock `wakeAt` |
| `AppContext::requestUpdate/requestRepaint` | Owner-thread model work or window presentation invalidation |
| `runtime::ServicePump` / `ServiceDemand` | Bounded service processing and deadlines independent of local simulation or painting |
| `UISession::activityDemand()` | Pending root work, paint invalidation and the next UI scheduler deadline |
| `CompletionQueue::setWakeCallback` | Notify after publishing work; failure does not retract an accepted completion |
| `sdl::EventWake` | Lifetime-safe weak posting endpoint, coalesced private SDL event, no raw host pointer |
| `runtime::PaintRequest` | Requested/submitted presentation revisions, independent of root dirty flags |

Demand queries must be inexpensive and must not create a new `now + interval`
deadline on every query. A deadline represents already scheduled work. Custom
continuous visualizations must opt into continuous painting or explicitly request
their next update/repaint; neither a stable layout nor a clean UI pauses simulation.
Live fixed-step simulations still request continuous host updates in this version;
simulation deadline-based sleeping is not inferred from the step accumulator.

Demand-driven apps construct UISession with `UISessionTiming::Monotonic`;
the default `CallerDelta` retains deterministic/caller-controlled stepping and
does not provide wall-clock activity deadlines. Monotonic updates advance its
scheduler with elapsed time.
UISession advances it before dispatching input, so newly scheduled tooltip,
caret and repeat timers do not inherit time spent idle before the input. The host
does not drop timer time, while repeating UI timers deliberately fire at most once
per advance. Continuous sessions continue accepting the caller-supplied delta.

Wake notifications are coalesced. Producers publish work before signaling;
the owner clears/arms notification state before rechecking demand. A bounded
fallback wait covers failed/filtered event delivery and native platform services.
Returned wait events are dispatched, never discarded. Native accessibility
actions and application/UI completions are wake sources. Waiting is measured
separately from active CPU frame work and does not impose an FPS cap.

The host rechecks at most every 100 ms when no event/deadline wakes it earlier and
performs a one-second maintenance update for native appearance/services. This is
a failure/platform fallback, not normal input latency. SDL waits retain events in
the queue for ordinary polling/dispatch. Pending GPU queries are polled without
painting; observed completion latency can therefore increase while idle without
GPU execution becoming slower. Outstanding GPU work adds a 2 ms completion
poll deadline; an unavailable target is retried after 16 ms. Pending
paint is retained. Native exposure/resize events also request a full redraw.
Pending [window transitions](WINDOWING.md#runtime-requests-and-failure-boundaries)
contribute their own polling deadline, so presentation requests progress even
when the application has no update or paint demand.

Worker posting endpoints may outlive the host, but cannot use SDL after the wake
owner is closed. Queue callbacks execute outside queue locks. Native accessibility
actions notify the same host wake endpoint after entering their bounded queue.
UI properties remain owner-thread-only; asynchronous producers post completions.

Presentation requests carry a revision. A successful full-frame presentation
acknowledges only the revision captured for that frame; new invalidation during
recording stays pending. Failure, minimization, unavailable swapchain or recovery
must not consume the request. Exposure and target/domain/scale changes force a
full redraw. No swapchain-content preservation is assumed.

Partial damage rendering, retained-target regional updates and shared clip-record
storage are deferred. Idle decisions use revision/queue/deadline summaries, not
whole-tree comparisons or screen-pixel comparisons.

This does not yet suppress arbitrary custom node preparation independently of
rendering, infer deadlines from arbitrary application code, pause continuous apps
when minimized, or implement regional repaint. Those need their own contracts.

## Runtime frame admission

`RenderRuntime` checks the optional frame-rate deadline and outstanding-frame
credits before expensive painting. Its wake deadline joins input, application,
window-transition, service and GPU-completion deadlines; it does not sleep inside the
render callback. A cap limits demanded frames but does not create demand.
Missed presentation deadlines do not accumulate catch-up frames.

Continuous variable updates run with eligible paints and retain an independent
update deadline while painting is paced or blocked. That deadline uses the fixed
simulation interval when present, otherwise the default simulation interval.
Events and explicit update requests still run immediately. Fixed simulation's
step duration, catch-up bounds and pause rules remain independent of frame caps.

Resource pressure preserves paint demand, reclaims backend caches and reports a
blocked state until policy, ownership or demand changes. One immediate
reclamation retry is permitted for the same demand. GPU work submitted by an
abandoned frame remains accounted and retains credits until retirement. Renderer
replacement starts credits for its new queue while old memory charges survive.
See [runtime resources](../render/RESOURCES.md).

## Host settings presentation

The shared settings view replaces application painting and input while open.
Local application simulation and variable updates pause; scoped service work,
session authority, worker completions and native window transitions continue. Held actions, pointer gestures and composition are canceled at the
boundary. Settings UI demand, 500 ms meter refreshes and service deadlines drive
its activity; closing rebases app elapsed time and restores its window policy
without replacing the renderer. See [GRAPHICS.md](GRAPHICS.md).

## Event dispatch

AppHost drains each SDL poll cycle before updating and presenting. SDL's
default-enabled poll sentinel ends the cycle even when new events keep arriving;
the host adds no event-count or elapsed-time cap. This preserves FIFO ordering
and avoids rendering intermediate states within a queued input batch. A large
batch or expensive handler can still delay updates. Pending frame revisions
remain pending until submission; no redraw cooldown keeps a settled UI awake.

Native OS resize/move can block SDL_PollEvent or SDL_WaitEventTimeout.
Continuous rendering within that modal loop requires SDL main callbacks or an
owner-thread, reentrancy-safe exposed-event callback; the current host does not
provide that path. See [SDL's resize guidance](https://wiki.libsdl.org/SDL3/AppFreezeDuringDrag).
Programmatic window transitions and ordinary queued resize events use the normal
host loop.

UI motion separates clock advancement from presentation sampling. Active tracks
request admitted frames; input dispatch does not sample the entire motion engine.
See [MOTION.md](../ui/MOTION.md) for pause, deadlines and final-frame ownership.
