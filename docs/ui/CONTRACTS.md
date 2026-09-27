# UI interfaces and runtime contracts

C++ remains the UI authoring language. Reconstructible views receive app-owned
models, explicit props and resource bundles; runtime node IDs, callbacks and
layout caches are not serialized descriptions. See
[asset and reconstruction contracts](../platform/ASSETS.md). Rebuilding a view
does not imply resetting its model or restoring transient focus/capture state.

This is the author/host agreement for the retained UI, not an algorithm guide.
Use [REFERENCE.md](REFERENCE.md) for the node/property catalog and
[GUIDE.md](GUIDE.md) for additive usage. Source declarations remain authoritative
for signatures. Renderer contracts live in [2D](../render/2D.md),
[GPU](../render/GPU.md) and [3D](../render/3D.md).

## 1. Responsibility boundaries

Appearance and transient presentation are described in [ACCESSIBILITY.md](ACCESSIBILITY.md).
The root owns a resolved palette and portal presentation order; nodes retain logical
ownership, event ancestry and semantic identity. Portal anchors use validated NodeIds.
UI sessions remain transparent unless an authored node requests a themed background.
Palette notifications are nonthrowing cache invalidations, not resource acquisition
hooks; resource preparation happens in the normal prepare phase. Reparenting refreshes
inherited appearance. Modal scopes and popup placement are distinct responsibilities.

| Participant | Owns the decision | Must not take over |
|---|---|---|
| Application | Model, composition, selected layout mode, host-level intent | Window presentation from individual nodes |
| Host/adapter | Event translation, environment, timing, frame acquisition/presentation | A control's semantic action or a container's child placement |
| UIRoot | Attached identity, ownership root, routing, deferred work, UI services | Device selection, OS event polling or an application model |
| Container | Child ownership and placement-in-parent records | Child model state or duplicated child-side placement authority |
| Content/control | Intrinsic measurement, content preparation, local painting, defined input/default actions | Global scheduling, window changes or arbitrary sibling mutation during traversal |
| Resource provider | Equal-request sharing, resource lifetime and realization | Node bounds, focus or model updates |
| PaintContext | Compatible drawing operations and scoped drawing state | Layout policy or application actions |

Do not make IApp inherit a visual node just to share lifecycle hooks. An app is
an orchestration boundary; a node is a participant in a retained tree. Likewise,
UIServices is not an AppContext copied into every node.

App view policy, window presentation, viewport mapping and persistence are defined
in [platform/WINDOWING.md](../platform/WINDOWING.md) and
[platform/SETTINGS.md](../platform/SETTINGS.md). Content-to-window fitting is a
host request, not a side effect of Node::measure. Reflow, fixed-canvas mapping and
whole-frame raster resolution have distinct contracts.

## 2. Authoritative versus derived data

| Data | Authority | Mutation route |
|---|---|---|
| Model and interaction state | Application or owning control | Input/update/actions |
| Authored props | Node/container's typed Props | Validating setter or Patch |
| Placement in parent | Parent's typed placement record | Parent placement setter/patch |
| Bounds, baselines, fitted destinations | Layout result | Measure/arrange, not another user setter |
| Prepared image/raster/cache | Resource preparation/backend | Rebuild or invalidate from its dependencies |
| Focus/capture/attached identity | Root runtime | Root/node routing APIs |

Props describe values. Patch<T> describes an operation: Keep, Set(value), Reset.
Set(false/0/nullopt/empty) is not Keep. Reset uses a declared baseline, not the
original constructor argument or an implicit theme lookup. Required fields without
a baseline reject Reset. Callers can inspect props but cannot mutate them by reference.

A successful setter commits authored state; a later notification may still throw.
There is no universal transactional rollback covering model changes, resources,
callbacks and containers. Document stronger guarantees individually rather than
inferring them from the presence of a Patch type.

## 3. Lifetime and threading

| Object/reference | Ownership and validity |
|---|---|
| unique_ptr<Node> | One parent/root owns the instance; move the owner, not the Node |
| NodeId | Root-local slot/generation; not a global persistent/serialized identity |
| NodeHandle<T> | Nonowning checked lookup; invalid root/attachment/type resolves to null |
| Parent/services/provider references | Borrowed; their owner outlives use |
| Connection / TimerHandle | RAII subscription/cancellation; retain the token while interested |
| PaintImageHandle / font handle | Shared resource lifetime, not shared ownership of a node |
| Measure/Arrange/Prepare/Paint contexts | Borrowed for the call/pass; do not store or post to workers |
| CompletionSink | Worker-safe mailbox access, not permission to access the UI from a worker |

Tree, props, signals, timers, painting and resource adapters operate on the owning
UI/render thread. shared_ptr keeps an object alive; it does not make mutation safe.
NodeHandle is not a cross-thread locking or ownership primitive.

Same-root compatible reparenting preserves identity. Removal/detachment invalidates
handles and clears routing state; cross-root transfer creates a new attachment.
Release borrowed-state subscriptions/timers before destroying that state. onDetach
must not throw. Destroying/removing content does not automatically cancel every
timer/job owned by its surrounding application.

## 4. Host-facing sequence

Named input contexts bracket UI routing: BeforeUI can consume shortcuts; a UI
Handled/Consumed result blocks AfterUI gameplay actions. Focus loss still reaches
UI capture/focus cleanup. Text/IME and pointer hit testing remain UI responsibilities.
Optional fixed simulation runs separately; UI update/timers continue while simulation
is paused. See [runtime input and lifetime](../platform/RUNTIME.md).

The host supplies these existing APIs, directly or through a session adapter:

| Entry point | Preconditions and effect |
|---|---|
| UIRoot::setContent(unique_ptr<Node>) | Detached subtree; outside traversal/lifecycle callbacks; establishes identity |
| preferredSize(Size2 maximum, Vec2f pixelScale={1,1}) | Positive finite offer/density, outside traversal; loose bounded measurement including padding, without arrangement or native window mutation |
| flushLayout(LayoutEnvironment) | Finite nonnegative viewport, positive density, valid usable insets; synchronizes geometry |
| dispatch(UIEvent&) | Translated root-logical coordinates; routes through current layout; event is borrowed |
| update(double seconds) | Owning thread, nonrecursive; delivers eligible completions, advances timers and flushes work |
| prepare(PrepareContext) | Current environment/layout; compatible image service and density; resolves drawable resources |
| render(PaintContext&) const | Matching prepared resources; issues full visible paint; does not present |

Normal dependency order:

```text
environment + translated input
    → update / deferred mutations / committed-prop notifications
    → measure and arrange
    → prepare for target density and image service
    → paint into the borrowed target
    → host presentation
```

Some entry points synchronize layout internally; this does not allow drawing with
stale preparation. A host can update without drawing when the target is unavailable.
needsPaint is a signal, not a complete exposure/damage/idle scheduler. A cleared
target needs the whole visible composition drawn, even if all nodes are unchanged.
`IApp::activityProps/activityDemand` and `UISession::activityDemand` connect this
signal to [whole-frame host activity](../platform/ACTIVITY.md). Default apps remain
continuous; opt-in UI apps declare timers, completions and visual changes.

## 5. Node extension protocol

Customize protected hooks; invoke public measure/arrange/prepare/render entry
points so validation, revision tracking and traversal scopes are not bypassed.

| Hook | What belongs here | Important restriction |
|---|---|---|
| measureContent(MeasureContext&, const SizeConstraints&) | Finite desired size and optional baselines | No application actions; honor constraints and indefinite axes |
| arrangeChildren(ArrangeContext&, Rect) | Child border boxes in the correct parent-local space | Do not add padding/border offsets twice |
| prepareChildren(MeasureContext&, const SizeConstraints&) | Controlled collection realization | Not a general structural-mutation escape hatch |
| prepareContent(PrepareContext&) | Resolve/reuse resources for final geometry/density | Mark failed preparation unready; never expose stale work as successful |
| paint(PaintContext&) const | Local drawing | No model/ownership mutation; context does not escape |
| paintSubtree(PaintContext&) const | Deliberate subtree composition override | Preserve child ordering, scopes and opacity semantics |
| onEvent(UIEvent&) | Observe or handle a routed event | Propagation/default flags have distinct meanings |
| onDefaultEvent(UIEvent&) | Default control action after routing | Respect preventDefault |
| onAttach(UIServices&) | Acquire attachment-scoped subscriptions/services | Do not retain transient traversal references |
| onDetach() noexcept | Cancel/release attachment-scoped state | Must be safe during cleanup |
| onPropsChanged(const ChangeSet&) | Respond to coalesced committed changes | Not a pre-commit validation hook |
| validateBoxProps(const BoxProps&) const | Enforce a specialized node's box invariant before commit | No mutations; throw to reject, as LayoutBoundary does for nonfixed size rules |

Input geometry must agree with paint geometry: specialize containsLocal,
containsClip and applyContentClip consistently when changing shapes. A transform
affects local-to-parent painting and inverse-transform picking; it does not reflow
siblings. Singular transforms are not hittable and produce no painted subtree.

## 6. Placement and responsive composition

Placement describes the child *in its parent*. The parent owns margin, alignment
override, grow/shrink, grid cell/span or anchor records. The child's BoxProps own
its size request, padding and border widths. ContentFit/alignment then place the
image/text inside the assigned content box; they do not replace parent placement.

BreakpointSet<Mode> is a pure available-space selector. Ranges are finite,
nonnegative and half-open, overlaps are rejected, and unknown axes match only
unrestricted axes. AdaptiveStack uses it only to choose a stack axis. Custom
compositions choose their own mode vocabulary and patch/rebuild at safe boundaries.
Do not choose from your own measured result and feed that result back as an offer.
There is no responsive component base class or automatic subtree reconciliation.

## 7. Input, scheduling and mutation

Input routes capture → target → bubble → default action. handled records handling;
stopPropagation stops routing; preventDefault vetoes default behavior. Those flags
are not synonyms. Focus and pointer capture are distinct from hover. A control
must clean up capture/press state on cancellation or disablement, not only release.

Use root.defer for structural changes requested during input, layout, notifications
or lifecycle callbacks. A mutation flush processes only the work present at its
entry; newly enqueued work waits. Callbacks must not recursively flush/update.

Scheduler advances from supplied elapsed time. It is not a worker pool or a fixed
simulation-step scheduler. Repeating timers fire at most once per advance, rather
than replaying an unbounded backlog. Keep an explicit simulation accumulator if
the application needs fixed-step physics.

Disconnecting a Signal connection or cancelling a timer releases its callback
captures immediately, unless that callback is executing. Self-disconnection keeps
captures alive until all nested invocations return, including exception unwinding.
Cancelled timer heap records can remain until they reach the heap's top, but no
longer retain callback-owned resources. Signal disconnection similarly leaves an
inactive slot until subsequent connection cleanup or signal destruction. A throwing
signal callback stops that emission; a throwing repeating timer stays scheduled
unless cancelled. None of these operations roll back callback side effects.

Workers compute independent results. CompletionSink::post(handle, revision,
callback) enqueues delivery to the UI thread. Receipt checks attachment and source
revision; stale results are dropped. Add a request-specific generation when node
revision alone does not express the lifetime of your job. No automatic async asset
loader, cancellation of external computation, or worker executor is implied.
The mailbox is bounded: post returns false on a full or closed queue. Its producer
owns backpressure/drop policy. UIRoot accepts CompletionQueueProps; default limits
are 4096 pending callbacks and 256 attempts per update. Delivery never holds the
mailbox lock while invoking user code or destroying discarded callback captures.

## 8. Failure and recovery boundaries

| Failure | Contract / caller responsibility |
|---|---|
| Invalid props/ranges | Reject the candidate; do not assume a later callback cannot fail |
| Completion/deferred callback throws | Exception propagates; unattempted queue tail remains; failed callback is not automatically retried |
| Post-commit notification throws | Committed state remains; pending work can be resumed; already emitted callbacks are not rolled back |
| Preparation fails | Content stays unready; retry preparation before painting |
| Painting fails | State scopes unwind; already written pixels/issued work are not rolled back |
| Unsupported painter operation | Explicit error rather than silently approximating a missing feature |
| Expired handle/stale worker result | No node access; drop the result |

These guarantees allow a caller to recover deliberately; they do not install a
recovery UI. The existing host exits on an uncaught exception. Decide fail-fast
versus app/frame isolation at the host boundary, not inside random leaf catches.

## 9. What is not promised

No full-tree reconciliation, implicit property observation, rich text editor,
automatic backend switching, damage presentation, hard total-memory budget, or
thread-safe UI mutation is provided. Plain text editing and desktop accessibility
adapters are described in [ACCESSIBILITY.md](ACCESSIBILITY.md). Additional controls
and partial-damage work remain in [the scope record](REFERENCE.md#scope).
The [test inventory](TESTING.md) distinguishes tested contracts from exhaustive
coverage. A backend-neutral interface is a compatibility boundary, not a promise
that every backend already implements every operation.

## 10. Work reuse and diagnostics

Operation-only `Patch`, `Keep`, and `Reset` live in
`support/Patch.hpp` in `playground`. Reusable property groups have distinct names:
Node settings, Box content props, Button button props, and Grid grid props.
App-specific props retain the shorter `props`/`applyPatch` vocabulary.

Layout optimization must preserve mutation and exception semantics. Invalidation
visits ancestors once; successful arrangement may be reused only when bounds,
environment and relevant descendant revisions agree. Text layout caching is
separate from raster realization. Collection preparation remains conservative:
an A/B/A size-cache hit must not restore a size without the corresponding children.

Authored-change notifications and sourceRevision belong to the locally invalidated
node. Inherited layout invalidation updates ancestor layout dependencies and paint
revisions, not ancestor authored-property notifications. A worker result depending
on an aggregate subtree needs its own request generation/validation, not just the
parent's sourceRevision. Measurement cache reuse does not replay diagnostics or
application actions; measurement hooks must not depend on being called each pass.

A layout boundary has an explicit child-independent extent and exports no child
baseline. It isolates size dependencies, not paint, overflow, focus or resource
dependencies. Queued work uses attachment-validated identities and survives failed
passes. Fixed width alone and paint layers do not establish this contract.

UI work counters measure requests, executed work, reuse and ancestor visits;
instrumented buffer growth is not a count of every allocation. Optional root-phase
timings are overlapping breakdowns of host phases, never additional frame time.
Acceptance uses deterministic counter/behavior checks, not wall-clock thresholds.
The test inventory records the corresponding checks and coverage boundaries.
