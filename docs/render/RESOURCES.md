# Runtime resources and frame admission

## Ownership and scope

The rendering runtime connects managed resource accounting, frame admission,
pacing and baseline telemetry. Backends own native operations and completion;
applications supply demand and presentation preferences. CPU simulation remains
independent of presentation cadence. Runtime policy changes do not rewrite user
settings or silently select a different renderer or image quality.

`runtime::ResourceLedger` is the shared managed-storage account consumed by
rendering, content, audio and networking. Native resources, CPU preparation
workers and service buffers retain accounting tokens, never the host. Separate
ledgers support isolated tests and embedded backends; asset adapters use the
default process ledger. Compatibility domains are not accounting identities.
Service snapshots expose queue/voice/connection counts separately from byte
charges. Shared storage is charged once globally even when several apps or mounts
retain it; per-consumer logical demand is a separate view. Audio render callbacks
do not call the ledger: control-side admission reserves buffers before playback,
and acknowledged retirement releases them outside the callback.
Renderer replacement does not clear outstanding
charges from the previous domain.

Managed commitment is an estimate of project-controlled storage, not physical
RAM, free VRAM or an allocation guarantee. CPU and GPU accounts remain separate,
including on unified-memory systems. Driver observations, if available, carry a
source, observation time and scope; unknown is not zero. The SDL adapter supplies
no native budget observations. SDL swapchain/window-surface storage, SDL_ttf
internal atlases, driver overhead and dependency-private allocations are outside
managed coverage. Native allocation can fail after successful admission.

## Budgets and lifetime

Per-resource constraints validate dimensions and transfer sizes. Aggregate
budgets bound outstanding commitments. Cache targets govern retention, not the
right to allocate. Work limits bound frames, submissions and preparation.

Managed native allocations reserve before creation and commit on success.
External CPU payloads and decoder results are charged on adoption. Each charge
survives owners and recorded/submitted uses. Failed creation
releases its reservation. Cache eviction cannot release charges still retained
by consumers or GPU work. Reallocation includes old/new storage overlap.
Reservation, owned and retiring bytes are disjoint states; category totals are
alternative views of the same bytes, not additional usage.

The default aggregate ceilings are 1 GiB CPU, 2 GiB GPU, 1 GiB GPU targets and
512 MiB preparation estimates. They are application safeguards, not hardware
claims. Existing per-resource and cache ceilings apply independently. Runtime
budget replacement validates the complete value before publication. Lowering a
ceiling preserves existing reservations and prevents growth until usage permits
it. Snapshots include policy/usage revisions, peaks and refusal counts.

Resource charges do not retain devices, preventing device/submission ownership
cycles. Native release requests are not proof of completion. Ambiguous command
failure quarantines retained uses; invalidation alone must not publish their
completion. Backend replacement may explicitly wait for native idle to retire
submitted uses. A failed wait leaves them charged until device teardown; uncertain
unsubmitted recordings remain quarantined. Ordinary frame admission never takes
this blocking teardown path.

## Admission and pacing

Activity determines whether rendering is needed. Scheduling determines when a
frame may start. Resource admission determines whether its working set fits.
Known expensive resources are checked before their preparation; incremental
custom work still reserves before each managed allocation. Planning reuses
existing extent/cache information instead of repeating scene rendering.

A frame identity groups all its command submissions, including uploads and
offscreen work. A submission identity still belongs to one device domain.
Abandoning a frame does not roll back earlier submissions. Outstanding frame
credits retire only after recording closes and all associated work completes.
The default is two outstanding frames. Command submission capacity is a separate
bound, not a substitute for frame admission.

Pacing supports an optional runtime frame-rate cap and preserves on-demand
painting and VSync. Missed deadlines do not accumulate a rendering backlog.
Input, simulation, window transitions and completion polling continue while
painting is deferred. Native presentation may still block under VSync; measure
that time separately from GPU execution. Swapchain acquisition stays late, with
bounded preparation and cancellation-safe failure handling.

Pressure first reclaims unused cache storage, then defers work that can make
progress after completion. A request that cannot fit remains blocked until
relevant policy, usage or demand changes. Policy refusal (`ResourcePressure`) and
managed native allocation failure (`ResourceAllocationFailure`) are distinct
from device loss. Both retain
diagnostics and permit bounded reclamation without replaying the callback.
Never replay a rendering callback automatically after partial submission.
Repeated pressure updates state/counters rather than producing an unbounded log.

`AppContext::renderRuntimeState()` exposes the current accounting, admission and
measurement state. `requestRenderRuntime` publishes budget/pacing patches at host
boundaries; see [SETTINGS.md](../platform/SETTINGS.md). The last pressure diagnostic
survives retry wakes and clears on successful presentation, independently of
rate-limited logging.

## Measurements and extension

Baseline resource counters, admission state and bounded CPU/GPU timing history
exist independently of F10 reporting. Missing or dropped timings remain explicit.
Report resets do not reset allocation ownership, admission or backend collection.
GPU intervals can overlap and must not be summed into a claimed frame duration.
Completion observation is not display latency.

Rendering extent and allocation capacity are independent concepts. Smaller
rendering extents can reduce work without releasing backing storage. Allocation
changes require their own admission, account for overlap and avoid churn through
reuse. The scene-resolution controller consumes snapshots and requests live changes;
see [GRAPHICS.md](../platform/GRAPHICS.md). General heap tracking and native
residency control are outside managed coverage.

## Verification

Deterministic tests cover shared charges, rollback, lowered budgets, concurrent
CPU reservations, outstanding frame retirement and fake-clock pacing. Backend
tests cover real resource lifetimes, canceled/abandoned work, resize overlap and
software output. Telemetry tests separate reporting switches from collection.
Measure steady-state overhead, allocation churn and tail frame times against the
same workloads/build settings; estimated byte accounting is not a native memory
profiler. Sustained resize, scene-switch and upload workloads must settle to
bounded commitments once live owners and GPU work retire.

## Storage estimates and planning

`TextureStorageDesc` describes allocation extent, texel blocks, layers/depth,
mip levels and sample count. Checked arithmetic computes nominal backing bytes;
it does not substitute the current rendering viewport for allocation capacity.
The SDL factory derives this descriptor from the actual texture create info,
removing separately supplied byte estimates. Native padding, metadata and
allocator block overhead remain outside this nominal estimate.

`ResourceLedger::splitReservation` partitions an exclusively owned reservation,
optionally transferring its category, without releasing aggregate capacity.
Unused allowance and committed output then retire independently. Scene attachment
planning uses this to reserve all missing attachments together. Surface-to-GPU
preparation transfers completed conversion scratch allowance into upload staging,
so the same allowance is not charged twice. Other preparation estimates remain
conservative where dependency-private scratch cannot be attributed precisely.

Automatic adjustments consume this ledger and obey its ceilings; see
[GRAPHICS.md](../platform/GRAPHICS.md). Managed storage and physical residency
remain distinct even on unified-memory hardware.

## Active scene resources

A scene view supplies a lifetime identity for its active resource working set.
The renderer deduplicates mesh and texture realizations, protects active entries
from retention eviction, and preflights missing native bytes against the shared
ledger. Per-resource limits still apply. Retention targets cannot cause recurring
reuploads of an otherwise admissible working set. Replacing or destroying a view
releases its pins; submitted uses retire through existing completion tracking.

Resource counters distinguish cache hits/misses, native uploads and bytes,
evictions, required active bytes and refused plans. They do not claim physical
VRAM residency. Camera motion, exposure and smoke animation change rendered
output without changing immutable mesh/texture identity.
