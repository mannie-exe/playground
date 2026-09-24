# Rendering telemetry

## Measurements and identity

CPU frame phases and GPU intervals are separate measurements. CPU submission
time includes host-side preparation and driver calls; it is not GPU execution
time. `GPUTimingSample` records a completed command-buffer interval in
milliseconds, its command label, central submission sequence and resource domain.
The sequence belongs to that domain: device recovery creates a new domain and
may restart its submission counter. Optional completion latency measures
host-observed submission-to-completion delay, not shader execution duration.

`PerformanceMonitor` keeps bounded CPU and GPU histories. Missing GPU samples
remain missing: unsupported timing, an exhausted query ring and a result that is
not ready do not produce zero-duration samples. Intervals can overlap; summing
independent submissions is not necessarily a meaningful frame duration.
Unmeasured CPU phases are reported as unmeasured, not as zero-duration work.
Labels contain 1–128 bytes and are validated before command acquisition even when
profiling is disabled. Ready samples within one poll are returned in submission
order, independent of recycled query-slot indices; a delayed result can still
arrive in a later poll, so consumers retain domain/sequence identity.

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
opt-in GPU test suite; capability absence must be distinguished from an invalid
result on a capable device. Tests assert finite nonnegative durations and lifecycle
behavior, not a wall-clock performance threshold. RenderDoc or vendor tools
remain useful for detailed pipeline statistics beyond command-buffer intervals.
