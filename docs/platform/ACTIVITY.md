# Application activity and full-frame presentation

Applications declare update and paint policy separately. Continuous behavior is
the compatibility default; an on-demand app must declare scheduled work, visual
changes and queued completions. An unchanged UI does not imply a paused game.

The host processes events and ready completions, runs requested/due updates,
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
The built-in apps advance it before dispatching input, so newly scheduled tooltip,
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
GPU execution becoming slower. A skipped target is retried after 100 ms; pending
paint is retained. Native exposure/resize events also request a full redraw.

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
rendering, infer animation deadlines from application code, pause continuous apps
when minimized, or implement regional repaint. Those need their own contracts.
