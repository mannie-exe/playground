# Spatial data verification

Optional spatial modules use shared fixtures for storage, edits, products, simulation
and presentation. Native physics, session adapters and rendering each add their own
checks; enabling one does not force every other module. Workload names describe
acceptance responsibilities, not implementation status.

## Contract fixtures

| Area | Required observations |
|---|---|
| Schemas | Invalid types/defaults, duplicate IDs, overflow, byte order and nonfinite-value rules |
| Addressing | Negative boundaries, int64 limits, half-open extents, canonical shared vertices and no packed-key aliases |
| Mapping | Anisotropic spacing, units/orientation, unsupported shear, inverse conversion and far-origin precision |
| Storage | Uniform/dense equivalence, immutable readers, channel-group retention and charged copy-on-write overlap |
| Coverage | Known default, missing and outside-domain remain distinct |
| Sources/directories | Loaded/generated/captured equivalence, bounded pages, stale cursors, dependency cycles and cancellation |
| Edits | Atomic multi-chunk/channel publication, stale expectations, no-op edits and cancellation before/after commit |
| Overrides | Set-to-empty differs from RevertToBaseline; unavailable source identity fails explicitly |
| Undo | Bounded preimages/history, conflict, refusal and no implicit reversal of external effects |
| Products | Typed leases, exact dependency stamps, stale/current distinction, failed replacement and shared demand |
| Scheduling | Latest progress/coalescing, Ordered overload, Deadline expiration, Background fairness and Durable retention |
| Persistence | Truncated data, unknown codec preservation, incompatible baseline, root CAS, uncertain acknowledgement and failed compaction |
| Accounting | Source/scratch/candidate/index bytes, canceled work until retirement and clean teardown |
| Selection | Exact sample/version, ties, start-inside, missing coverage and stale placement |

Use injected clocks, controlled completion order and fault-injecting storage.
Concurrency tests also run real workers. Small reference implementations establish
expected results; record property-test seeds. Test relevant sanitizer configurations.
A local edit fixture does not need a session identity or deduplication protocol.

## Editable volume

Load or generate at least three adjacent label chunks. Select a boundary sample,
apply a cross-chunk edit and publish surface/query products. Unload/reload and restore
an exact checkpoint, including negative coordinates and an empty override. Test
stale selection, delayed preparation, cancel, save failure and epoch replacement.

Assert coherent revisions, selective invalidation, preserved state on failure and
retirement of all owned storage. Mesh assertions include exposed-face coverage,
winding, valid indices, merge boundaries and deterministic ordering. Known closed
fixtures check edge incidence; temporary missing-neighbor boundaries are explicitly
open. An unchanged view produces no repeated uploads.

A world-bound variant uses DatasetBinding and exact checkpoint references. A
standalone variant performs the same edits without World, SDL or a renderer.
Navigation and character traversal are not prerequisites.

## Scalar-volume viewer

Import a bounded NRRD scalar fixture with non-unit spacing and a label overlay.
Verify raw values, orientation, units, slice sampling, window/level changes and
label editing. Transfer-function changes invalidate presentation without mutating
source intensities. Save/restore the overlay. Unsupported transforms/codecs fail
without a partially published volume. Use synthetic known-value fixtures for exact
assertions; MRI/CT samples supply realistic workloads, not the numerical oracle.

Drive explicit-time scalar updates faster than preparation can finish. Require
bounded pending work, useful intermediate output, monotonic represented samples
and eventual convergence when input stops. Epoch replacement rejects every old
result. Headless preparation and native slice presentation are separate checks.

## Streaming and generation

Compare a finite index with an application-supplied procedural directory under the
same demand and budgets. The procedural fixture resolves newly visited addresses
without enumerating its entire numeric domain. Generator identity, configuration
and traversal trace reproduce the workload. No terrain aesthetic or biome rule is
part of acceptance.

Exercise cold storage reads, warm reuse, generation, repeated crossings, missing
halos, page churn and pinned working sets that exceed budgets. Observe data, mesh,
texture and collision residency separately. Many chunks using the same texture do
not establish texture pressure; a dedicated fixture exceeds the distinct-texture
budget. Whole-texture residency is baseline; partial mip residency is future.

## Physics integration

When Jolt is enabled, use primitives/convex objects over static triangle scenery,
a kinematic platform and editable-volume collision. Verify resting/sliding bodies,
material/filter behavior, ray/overlap/sweep results and actual completed poses.
Pre-separated pieces exercise body activation/removal without requiring fracture.

Test support removal, insertion through bodies under each intersection policy,
replacement between steps, delayed cooking, offscreen support retention, missing
sweep coverage and near/far-origin equivalence. Render, source and collision versions
must agree at required barriers. Queries never see half-exchanged native geometry.

Inject command/event capacity failure, admission refusal and stale region generations.
RefusedBeforeStep preserves native state; Compromised suppresses complete-tick
publication and requires recovery. Test body-slot reuse, callback identity, close
while stepping and solver/content worker contention. Fake adapters establish failure
sequencing; real Jolt establishes exercised contacts and queries.

## Regional simulation

A deterministic propagation/diffusion fixture crosses chunk boundaries, requests
coverage, sleeps when settled and wakes on edits/timers. Compare declared reproducible
results across worker orders. Conflicting boundary writes follow domain arbitration;
missing neighbors are not air. Overload reports lag/refusal rather than dropping
required ticks. A final settled revision still invalidates its render product.
Checkpoint the domain state needed to resume, including active work/timers where
required; restoring values alone is not a simulation continuation test.

## Optional authority adapters

[Session verification](NETWORKING.md#verification) uses a shared-document domain and
sequenced value stream. It does not depend on voxel/physics fixtures. Spatial
replication, multi-client terrain edits and physical prediction remain future
adapter workloads. Local storage and collision contracts must remain testable without
a connection, account, authority protocol or remote codec.

## Measurements and completion

Record source/build revision, enabled modules, dependency versions, OS/CPU/GPU,
provider/schema/profile identities, sample digests, bounds, budgets and traversal/edit
trace. Compare solid/fragmented volumes, interior/boundary edits and cold/warm storage.
Measure preparation/publication p50/p95/max, result age, missed deadlines, output counts,
cache reuse, upload bytes, native retirement and input responsiveness.

Timing thresholds belong to identified hardware/configuration. Correctness uses
coverage, identities, revisions and outcomes. Skipped native tests establish no
native coverage. Each enabled module passes its contract and integration workload;
a fake transport/solver or a screenshot alone cannot satisfy all layers.
