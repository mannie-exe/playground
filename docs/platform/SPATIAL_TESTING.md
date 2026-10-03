# Spatial data verification

Acceptance covers [datasets](SPATIAL_DATA.md), [voxel edits](VOXELS.md),
[products](SPATIAL_PRODUCTS.md), [procedural sources](PROCEDURAL.md),
[simulation](SIMULATION.md) and [presentation](../render/SPATIAL.md).
Contract, integration, native and performance results identify their actual scope.
Test names below identify workload responsibilities, not already-available targets.

## Contract fixtures

| Area | Required observations |
|---|---|
| Schema | Invalid types/defaults/components, duplicate IDs, overflow, byte order, alignment and declared nonfinite-value rules |
| Addressing | Negative boundaries, int64 limits, no packed-key aliases, half-open extents and canonical shared vertex ownership |
| Mapping | Unequal axis spacing, units/orientation, inverse mapping tolerance, singular/invalid inputs and ±1,000,000 meter placement |
| Storage | Uniform/dense equivalence, grouped-channel retention, typed-span refusal, immutable readers and charged copy-on-write overlap |
| Coverage | Known default versus missing/outside, partial interpolation support and no false empty-space results |
| Sources | Loaded/generated/captured equivalence, concurrency capabilities, bounded output/scratch/work, failure and cooperative cancellation |
| Directory | Paged traversal, cursor revision mismatch, missing/default descriptors, query bounds, dependency cycles and bounded retention |
| Live data | Coherent sample capture, explicit mixed-sample policy, epoch changes and no unbounded producer queue |
| Edits | Atomic multi-chunk/channel publication, overlapping patches, stale expectations, no-op edits and post-commit cancellation |
| Authority | Denied commands, duplicate receipts, retired sequence watermark, gaps, restart epoch and bounded dedup history |
| Overrides | Set-to-empty versus RevertToBaseline, unavailable baseline and failed overlay rebase |
| Undo | Retained preimages, memory refusal, conflict, history eviction and no implicit reversal of external effects |
| Dependencies | Channel/range/halo edits, absent neighbor becoming available, profile changes and conservative invalidation after history expiry |
| Products | Typed lease compatibility, stale/current separation, failure retaining old output, equivalent-request sharing and consumer cancellation |
| Scheduling | Latest coalescing without output starvation, Ordered overload, Deadline expiration, Background fairness and Durable retention |
| Accounting | Source/candidate/scratch/metadata/old-new overlap, canceled work until retirement, lower live budgets and teardown baseline |
| Persistence | Truncated/corrupt data, incompatible generator/schema, unknown codec preservation, durable root CAS and uncertain acknowledgement |
| Recovery | Interrupted compaction, orphan cleanup with retained roots, failed save with dirty state and exact world/dataset checkpoint restore |
| Simulation | Ordered steps, changed worker order, boundary write conflicts, missing halos, overload and final settled mesh invalidation |
| Selection | Exact sample/channel/version, tied boundaries, start-inside, missing coverage, finite traversal and stale placement rejection |
| Adapters | Ground navigation clearance/seams, collision capability refusal, profile invalidation and coherent activation barriers |

Use injected clocks, bounded deterministic executors, controllable completion order
and fault-injecting storage. Concurrency tests also run real workers; an inline
executor alone cannot establish race safety. Use compiler sanitizers where the
platform/dependencies support them. Property tests compare small generated cases
to simple reference implementations and record seeds for failures.

Mesh tests assert occupied/exposed face coverage, winding, valid indices and merge
boundaries. For fixtures promising closed surfaces, test edge incidence/orientation
and expected boundaries, not merely nonempty output or a triangle-count multiple.
Unknown-boundary fixtures have deliberately open topology and different assertions.

## Integrated workloads

### Editable volume

The `spatial_edit_workflow` fixture loads or generates at least three adjacent
label/occupancy chunks, selects a boundary sample, applies a cross-chunk edit and
publishes surface, query and ground-navigation products. It traverses the admitted
route using actual-pose locomotion, then unloads/reloads and restores a saved world
with exact dataset checkpoints. Include negative coordinates and an empty override.

Inject stale selection, late mesh completion, missing collision capability, failed
RequireProducts activation, cancellation and storage failure. Assert preserved old
state on failure, new revision agreement where required, selective invalidation,
no navigation through unknown space and no retained owned bytes after teardown.
A fake physics consumer proves readiness/activation sequencing, not physical contact
correctness. A supplied real solver needs its own physical validation suite.

### Live scalar visualization

The `spatial_live_workflow` fixture supplies explicit-time scalar samples faster
than preparation can consume them. It renders bounded slices and exercises source
pause/resume, live sample replacement, retained previous output, transfer-profile
changes and immutable capture/edit/export. Run without an audio device using
recorded control samples; test a live multimedia bridge separately.

Assert bounded pending work/storage, monotonic represented sample identity,
measurable useful output during overload and eventual latest-sample convergence
when input stops. Epoch changes reject all old results. An unchanged sample/view
settles to no repeated mesh/image uploads or idle paint requests. UI interaction,
settings and camera input remain serviced while preparation is busy.

### Headless and authority adapters

Run source/edit/product/persistence workloads without SDL/video or a renderer.
Use an in-memory fault transport for duplicate/reordered commands, incompatible
baseline/provider identities, missing deltas, interest exit versus deletion and
resynchronization. Local and remote adapters use the same authority validation and
receipts. Transport-free execution establishes domain contracts; it is not native
network/authentication coverage. Similarly, fake collision cooking is not solver
support. Retain those distinctions in every report.

## Performance comparisons

Record commit/build, OS/CPU/GPU/backend, dataset/schema/provider/profile digests,
seed or captured sample stream, extents/channel layout, budgets, worker count,
traversal/edit trace and cold/warm state. Timing thresholds are fixture parameters
for identified hardware, not unexplained universal pass criteria.

| Workload | Compare and measure |
|---|---|
| Uniform/static | Uniform versus dense memory, decode cost, warm product reuse and zero unchanged uploads |
| Fragmented | Solid versus checkerboard/sparse surfaces; visited cells, emitted geometry, output-limit refusal and peak storage |
| Local edits | Interior versus boundary edits; exact rebuilt products, stale-display duration and edit-to-current-query latency |
| Streaming | Repeated crossings, directory page churn, missing halos, queue age and dependency/pinned working set |
| Live input | FIFO reference versus Latest policy; bounded backlog, result age, useful publication cadence and coalesced work |
| Simulation | One versus multiple workers for reproducible kernels; step cost, ordering, boundary arbitration and settled invalidation |
| Pressure/recovery | Old/new overlap, slow consumers, canceled jobs, native upload pressure, save failure and retirement time |

Report preparation/publication p50/p95/max, input-to-commit and sample-to-present
age separately, deadline misses, meaningful progress, bytes by owner/stage, cache
hits, redundant work, upload counts/bytes and frame/input responsiveness. Exclude
idle waits from active CPU cost; do not sum overlapping GPU intervals into a frame.
Accounted storage is not measured RAM/VRAM residency. Temporary captures and raw
measurements remain local artifacts unless a small reproducible fixture requires them.

## Workflow and completion

Extend the existing CMake test/harness organization. Pure contracts belong to CPU
module tests, connected workflows to integration tests, and native presentation to
explicit smoke/readback workloads. Reuse shared CLI/host harness facilities rather
than adding per-experiment shell scripts. Add runnable commands to
[Contributing](../../CONTRIBUTING.md) when their targets exist; do not document a
nonexistent command as a working workflow.

Completion requires both integrated workloads, failure/pressure coverage, selective
invalidation evidence and settled teardown accounting. CPU success does not imply
native presentation, network transport, physics or cross-platform coverage. Record
unsupported/skipped checks and the actual tested snapshot. A fake adapter's green
result cannot establish that an unimplemented capability is finished.
