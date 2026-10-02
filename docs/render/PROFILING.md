# Rendering telemetry

## Measurements and identity

CPU frame phases and GPU intervals are separate measurements. CPU submission
time includes host-side preparation and driver calls; it is not GPU execution
time. `GPUTimingSample` records a completed command-buffer interval in
milliseconds, its command label, central submission sequence and resource domain.
Context also carries the originating `frameId` (zero for preparation outside
an admitted frame). Participating scene views supply a stable `workloadId` and
the `qualityRevision` used to prepare that work. Automatic quality ignores stale
revisions; late completion cannot acknowledge a newer resolution decision.
The sequence belongs to that domain: device recovery creates a new domain and
may restart its submission counter. Optional completion latency measures
host-observed submission-to-completion delay, not shader execution duration.

`RenderRuntime` keeps baseline CPU history (240 iterations) and completed GPU
intervals (128 samples) independently of reporting. `PerformanceMonitor` consumes
the same collected values when detailed reporting is enabled. Missing GPU samples
remain missing: unsupported timing, an exhausted query ring and a result that is
not ready do not produce zero-duration samples. Intervals can overlap; summing
independent submissions is not necessarily a meaningful frame duration.
Unmeasured CPU phases are reported as unmeasured, not as zero-duration work.
Labels contain 1–128 bytes and are validated before command acquisition even when
profiling is disabled. Ready samples within one poll are returned in submission
order, independent of recycled query-slot indices; a delayed result can still
arrive in a later poll, so consumers retain domain/sequence identity.

## Workload reports

The reporting contract groups completed samples by resource domain, collection
generation, stable workload label and optional source/target pixel extents.
Extents describe the primary resource of that submission, not logical UI bounds;
omit them for uploads or heterogeneous work without one meaningful extent.
Presentation records the actual acquired swapchain extent, which may differ
from the size observed at frame start. Metadata is retained with the timestamp
ticket, never reconstructed from the current window when completion arrives.

`PerformanceMonitor::snapshotReport()` returns CPU phase statistics, UI work,
GPU groups and collection health as values. Duration and completion latency have
independent counts, totals, minima, maxima and optional averages. No samples is
not a zero-duration measurement. `report()` formats that snapshot and resets only
the reporting interval; raw bounded history and timing availability survive.

GPU counts cover completed samples **received during** the reporting interval,
not necessarily submissions from its CPU frames. A report is not a GPU frame
total or a utilization percentage; command intervals may overlap or contain
dependencies. Raw GPU context identifies the originating admitted frame, view and
quality revision; workload groups omit these identities so reporting does not
create a group per frame or adjustment. CPU iteration
measurements can include updates without an admitted frame. No display timestamp
is inferred.

At most 64 groups are retained per interval. Additional groups increment an
omitted-sample counter without blocking rendering or merging unrelated work.
Collection health distinguishes pending queries, query-slot exhaustion and
completed-sample buffer eviction. Pending is a current-generation gauge; loss
counts are interval deltas of cumulative counters. Raw history eviction is an
intentional retention policy, not a lost report sample. `historySize` bounds CPU
frame entries and GPU sample entries separately; their counts need not match.

Enabling native profiling starts a fresh collection generation. Old queries
remain alive until safely completed/canceled, but their results cannot enter the
new collection. Disabling profiling clears buffered results. Recovery creates a
new resource domain. Collection reads completed queries without waiting for GPU completion. Baseline
GPU scopes cover paint, scene rendering/post-processing, custom offscreen passes
and presentation composition. Detailed device collection can additionally time
upload and other command scopes. Query exhaustion drops measurements, never
blocks rendering. Reporting does not change pacing or VSync.

The host does not restart native collection when reporting is toggled or reset.
`statisticsRevision()` belongs only to the optional reporting session. The current
collection status remains measured after reporting; a new interval with no
completed results explicitly reports zero received samples instead of replaying
the previous interval's measurements.

Workload names remain stable (`paint2d`, `presentation composition`, `scene3d`,
`scene post-processing`, `text atlas`, `layer composition`, `custom offscreen`,
`mesh upload`, and other explicitly named command scopes). A group represents a
command-buffer interval, not necessarily one frame or one draw call. Captures
can produce additional `paint2d` submissions. Do not put object IDs or sizes in
labels; use the context fields. For composition, source is the internal render
target and target is the acquired presentation image.

### Reading a report

F10 toggles optional reporting and detailed UI measurement. While reporting is
enabled, Shift+F10 reports immediately; F11 switches the automatic report interval
between one and five seconds of monotonic wall time. Each report starts with CPU phases,
then collection health and one row per GPU group, then UI work counters. GPU rows
include independent execution-duration and observed-completion-latency statistics.
`unspecified` extents and `unmeasured` latency mean absent metadata, not zero.
The raw histories remain available for consumers independently of the report.

CPU render/present phases include preparation, submission and possible waiting;
they are not substitutes for GPU execution durations. Compare workload rows at
the same source/target size and device domain. Reporting introduces no summed GPU-frame duration or utilization estimate.
Frame caps and admission belong independently to [RESOURCES.md](RESOURCES.md).

Activity policy now independently permits whole-frame idle skipping; see
[ACTIVITY.md](../platform/ACTIVITY.md). `Idle` reports wait count and accumulated
milliseconds outside active CPU-frame totals. Reports use elapsed-time deadlines serviced at host boundaries, including idle
maintenance. A blocked owner thread can delay a deadline; missed intervals never
produce a catch-up burst.

These aggregates do not measure input-to-display latency. Diagnose perceived wake
delay by separating event arrival/polling, dispatch/update, first-frame preparation,
GPU execution and presentation. Compare the first frame after idle with subsequent
active frames; GPU clock changes and presentation waits are possible contributors,
not conclusions from utilization alone. The host's 100 ms fallback timeout is not
an input debounce: queued SDL input wakes the wait. GPU completion observation is
also not a measurement of when pixels became visible on the display.

`Paint` counts considered/rejected draw records, recorded quads/batches, streamed
record bytes and rectangular/general/presentation path selection. These include
offscreen text/layer work and composition; they are not fragment counts or GPU
utilization or successful queue submission. The UI line includes native-publication builds/cache hits and phase
time totals. Phase times overlap CPU work; do not add them to frame time. Semantic
snapshots reuse root identity/revisions, geometry, focus, viewport mapping, native
pixel scale, title and window-focus state. IME lifecycle/caret synchronization
still runs independently of that cache. Custom semantic changes must invalidate
their node rather than silently mutate data behind the retained tree.

## SDL Vulkan extension

Upstream SDL GPU does not expose timestamp queries. The project applies the
checked-in `cmake/SDLTimestamps.cmake` extension to its pinned SDL source during
configuration. Original-file hashes prevent application to an unknown revision,
and repeated configuration is idempotent. Unknown edits in the patched files are
rejected rather than overwritten. When upgrading SDL, review the integration
points and update the guarded originals deliberately.

The public extension header is `SDL_gpu_timestamps_playground.h`, copied from
`cmake/patches/sdl`. This is a project API, not an upstream SDL promise. A versioned
`SDL_GPUTimestampInterface` table is published through the existing device
properties API. No SDL exported function, dynapi entry or driver-vtable layout is
added. Vulkan implements it; absent properties mean unsupported, including
unmodified SDL and other graphics drivers.

The native table owns one query pool per device with 1–4096 timestamp pairs. Its
metadata comes from the selected Vulkan queue's `timestampValidBits` and the
physical device's `timestampPeriod`. Queries bracket a command buffer outside
render, compute and copy passes: begin resets its pair and records at
`TOP_OF_PIPE`; end records at `BOTTOM_OF_PIPE`. These are elapsed GPU timeline
intervals, potentially including dependencies and stalls, not per-shader
statistics. The duration masks unsigned subtraction to the counter's valid bits
and converts ticks using the native period. One interval must be shorter than a
complete hardware-counter wrap; no finite-width counter can infer multiple
wraps from its two endpoints. These rules follow Vulkan's
[timestamp command](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdWriteTimestamp.html)
and [query result contracts](https://docs.vulkan.org/spec/latest/chapters/queries.html).

Timing works with GPU debug mode both enabled and disabled. The extension always
checks pool, slot and device compatibility; pass/submission validation uses SDL's
debug-only command state only when GPU debugging is enabled. Callers must record
on live command buffers outside passes in either mode. SDL does not reset its
debug validation fields when recycling non-debug command buffers.

## Submission, cancellation and lifetime

`GPUTimestampRing` borrows the native SDL device and belongs to its owner thread.
Its capacity is fixed at construction. `begin` returns an optional generation-
checked ticket; a full ring drops telemetry without blocking rendering. `end`
must use the same command buffer. Successful submission associates the ticket
with `GPUDevice`'s central submission ID. The ring does not own a second fence or
wait for the device.

`poll(completedSubmission)` reads only submissions already known complete by the
central tracker. Native result reads are nonblocking and additionally check query
availability. Not-ready pairs stay occupied. A successful read makes its pair
reusable; failures preserve other ready samples and quarantine the failed pair.
Actual driver failures are `RenderFailure` recovery candidates, not fabricated
measurements or proof that every failure means device loss.

After native command cancellation, `cancel` frees an unsubmitted pair. An
ambiguous submission failure uses `abandon`, which permanently quarantines that
pair for the remaining device lifetime: the host cannot assume unknown GPU work
has stopped. Merely dropping a ticket does not cancel command-buffer work.

Destroy the ring before its borrowed device. Closing a ring only closes access to
the native pool; it does not free possibly-live query storage or wait for idle.
The patched SDL Vulkan renderer retains that one bounded pool until its ordinary
device teardown, after its existing completion/idle handling. A second ring
cannot replace the pool on the same native device; disabling telemetry should
retain the ring for subsequent re-enabling. Normal rendering never waits merely
to recycle timing slots.

Allocation of optional timing storage can fail without disabling rendering. The
device reports timing unavailable and logs the failure once; it does not retry
allocation each frame. Query execution failures remain typed rendering failure
candidates. `maxTimestampScopes` in AllocationLimits bounds the ring and pending
sample history (default128); performance history has its separate runtime cap.
Completion latency is measured from submission to the owner's observation of the
signaled fence, so infrequent polling contributes to that latency.

## Verification

The ordinary `gpu_timing` test injects a versioned native function table to test
capacity, completion ordering, unavailable results, cancellation, stale tickets,
counter wrap, failure quarantine, owner-thread checks and invalid metadata.
It does not require a Vulkan device. Native timestamp execution belongs to the
GPU test suite; capability absence must be distinguished from an invalid
result on a capable device. Tests assert finite nonnegative durations and lifecycle
behavior, not a wall-clock performance threshold. RenderDoc or vendor tools
remain useful for detailed pipeline statistics beyond command-buffer intervals.
`gpu_timestamp_native` warms up and recycles real Vulkan command buffers before
toggling profiling on/off/on with both debug and non-debug devices. It checks
rendered output and completed samples, covering runtime profiling activation
without depending on an application's window or settings.
It also checks captured target extents, old-generation rejection, pending-query
counts, query exhaustion and completed-buffer eviction. `gpu_timing` covers
delayed context publication and invalid context updates with an injected driver.
`performance_reports` covers grouping, latency denominators, resize/domain/session
separation, health deltas, group limits, structured snapshots and console output.

## Scene diagnostics

Each CPU phase reports its own sample count. Iterations, rendered frames and
presentations are distinct; averages with different denominators are not added.
Reports identify the app interval and scene resource work: cache hits/misses,
upload bytes/counts, evictions and CPU preparation. App transitions close the old
reporting interval. Delayed GPU results retain submission identity; they cannot
be attributed to the current app merely because collection occurs there.

Benchmark reports include load/warm-up duration, measured wall interval, CPU
percentiles and GPU sample counts/durations. They record pixels, settings and
content/path identity. Neither completion latency nor CPU recording time is GPU
execution time. Tests assert reuse/ownership/invalidation invariants; hardware
measurements are reported with environment metadata, not universal FPS promises.
