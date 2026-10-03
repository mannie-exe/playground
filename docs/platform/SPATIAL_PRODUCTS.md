# Spatial products and scheduling

Derived products consume immutable [dataset](SPATIAL_DATA.md) snapshots. Meshes,
selection indexes, collision data and navigation tiles have independent readiness
and lifetime. Product preparation uses the shared Executor, ServiceScope and
ResourceLedger; it does not install a second worker pool or GPU scheduler.

## Product state and API

| Value/API | Shape and contract |
|---|---|
| `ProductKind` | Registered typed output identity; baseline kinds are SurfaceMesh, VolumePresentation, Selection, Collision and Navigation |
| `RecipeRef` | Registered recipe ID, schema/build compatibility and exact configuration digest |
| `DependencyStamp` | Dataset/epoch, grid/address, channels, region, source/content revisions and coverage state; also exact external profile/content identities |
| `DependencySet` | Bounded canonical ordered stamps, including absent-neighbor availability |
| `ProductKey` | Kind, recipe, dependencies, profile digest and target encoding; placement only when the product bakes it |
| `ProductVersion` | Complete ProductKey plus publication generation; no pointer identity in persistent keys |
| `ProductCapabilities` | Accepted schemas/interpretations, transforms, sampling, output kinds and supported targets |
| `ProductRequest` | Consumer/scope, inputs, recipe/profile, priority/deadline, limits, work policy and FreshnessPolicy |
| `ProductCandidate` | Typed immutable payload, validated dependency set, reserved storage and preparation measurements |
| `ProductState` | Requested key, WorkStatus, optional available version, Freshness, queue age, retained bytes and diagnostic |
| `ProductLease<T>` | Typed retained generation; wrong output type fails before exposure |
| `SpatialRecipe::prepare` | Worker-only operation over declared immutable inputs and PreparationContext |
| `ProductService::request / cancel / poll / forget` | Bounded per-consumer demand and retained outcomes; equivalent requests share admitted work where compatible |
| `ProductService::state / lease` | Inspect status or retain an explicitly matching generation/freshness |
| `ProductService::advance / attach / close` | Owner publication, scoped wake demand and cancellation; no waits inside rendering |

WorkStatus is Absent, Queued, Preparing, WaitingForInput, Ready, Failed or Cancelled.
Freshness is Current, Stale or Unavailable and is independent of work status. A
replacement can fail while an older usable product remains retained. Outcome
codes distinguish Unsupported, BudgetExceeded, Conflict, DeadlineExceeded and
provider failure; admission uses existing Accepted/Busy/TooLarge/Closed semantics.
A request moves Queued → Preparing → Ready or a terminal failure/cancellation;
WaitingForInput returns to Queued only when admitted dependencies become available.
New work uses a new generation; terminal outcomes remain until forgotten rather
than silently restarting. Ready establishes the request's declared outcome, not
every consumer's readiness.

`FreshnessPolicy` is Exact or AllowPrevious with bounded age/sample lag and a
missing-data policy. State exposes the represented source revisions even when old
presentation is permitted. A stale visual lease cannot satisfy an Exact query,
physics or navigation request. Diagnostic snapshots never convert absence to zero.

Readiness flags remain aggregate queries over exact product requirements. A
Presentation bit does not imply a mesh, volume renderer and every device variant
all exist. Consumers request a particular recipe/type/profile. Headless consumers
never acquire GPU resources merely to make a cell generally Ready.

## Preparation and publication

Recipes declare inputs, channels, neighbor halo, dependency depth, output type,
maximum output/scratch/work and target capabilities. Dependencies form an acyclic
bounded graph; cycles fail validation. Simulation feedback uses consecutive ticks,
not recursive product dependencies. Dynamic input discovery returns a bounded
NeedsInput description to the owner; workers cannot recursively spawn unlimited
jobs or perform implicit data acquisition.

The owner admits work, pins inputs and reserves storage before dispatch. Workers
return candidates without live scene, world, device or dataset mutation. Completion
is retained before wake notification. Publication validates scope/epoch, schema,
recipe, expected dependency stamps and consumer demand. Replaced authority inputs
reject publication as current. A captured previous live sample may publish only
as explicitly stale presentation within AllowPrevious limits; it cannot replace a
newer sample or survive an epoch change. Close invalidates publication before
releasing owners. Canceled jobs retain admission and storage until retirement.

Typed recipes retain their own output validation. Mesh recipes check topology,
counts and indices; navigation recipes check profile/coverage; collision recipes
check geometry and solver encoding capabilities. Unsupported providers are explicit
hooks with diagnostics, not successful empty products. A dependency becoming empty
can legitimately produce a complete empty result if coverage is known.

## Precise invalidation

An edit invalidates only products whose declared channels and input regions
intersect its ChangeSet. Halo widths include sampling, filters and conservative
geometry/profile needs. Changing neighbor availability invalidates boundary
products even when sample values in the original chunk did not change.

Dependency tracking uses chunk/channel revisions and bounded region-change history.
When stamps are too coarse or history has expired, conservatively rebuild; never
claim precise reuse without evidence. Interpretation/profile versions are inputs.
Render color changes need not invalidate collision/navigation, while occupancy or
traversability changes can. Camera movement, presentation exposure, world-cell
membership and rigid placement do not rewrite source data or local geometry.

Cache keys include source/sample identity, recipe/cooker version, semantic profiles
and target encoding. A persistent cache uses exact content identities rather than
runtime epochs alone. Cache reuse validates schemas, bytes and dependencies.
Device-domain realizations are separate from CPU product identity. Product eviction
cannot evict retained inputs still required by an in-flight consumer.

## Work policies and admission

| `WorkPolicy` | Contract |
|---|---|
| `Latest` | Coalesce queued replacements within a declared consumer stream; bounded stale presentation permitted only by FreshnessPolicy |
| `Ordered` | Preserve required source/tick/command order; overload is observable and never silently drops authoritative steps |
| `Deadline` | Prioritize required readiness with finite deadlines; expiration has a terminal result |
| `Background` | Fair lower-priority cooking/export/cache work with bounded retention |
| `Durable` | Retain committed dirty state until persistence acknowledges it; retry is bounded and cannot discard unsaved edits |

ProductService maintains bounded demand and policy queues before submitting to the
shared Executor. Physics stepping is a separate bounded native execution domain
specified in [PHYSICS.md](PHYSICS.md#execution-allocation-and-build-boundary), not a
SpatialRecipe job graph. Existing executor capacity and rejection rules remain authoritative;
priority is not a promise of OS thread preemption. Aging/fair shares prevent
background starvation within admissible workloads. No scheduling policy can promise
progress for an impossible pinned working set or a nonterminating native provider.

Latest uses a bounded pending slot per stream, not one queued job per sample. A
policy can let one admitted job finish and publish its eligible captured sample
while retaining the newest pending request. Repeated cancellation must not starve
all output. Bound consumers, keys, pending replacements, jobs and retained terminal
results. Consumer cancellation releases only its demand, not another consumer's
shared work or lease. Epoch changes reject every prior publication regardless of
stale-display policy.

Job-count caps are not byte/time caps. Requests separately reserve input leases,
output, scratch, temporary decode, old/new overlap, metadata and publication work.
Owner-side staging is incremental under ServiceWorkBudget; final publication is a
prebuilt root/handle exchange, not an unbounded instantiate/upload callback.
Admission policy can reclaim unused caches or wait for retirement. TooLarge and
Closed fail without indefinite retry; transient Busy has a bounded retry deadline.

Work units are explicit provider operations such as visited samples or emitted
vertices. Cooperative elapsed-time checkpoints support responsiveness, not hard
preemption or cross-machine determinism. Framework allocation helpers reserve and
transfer ledger charges; uncontrolled native/library allocations remain identified
estimates outside allocator enforcement. No accounting token can guarantee that
the OS/driver allocation will succeed.

## GPU and host integration

CPU products publish through services; native realization stays in the renderer's
resource/preparation boundary. PreRender never waits for mesh generation. Rendering
chooses an admitted available product or an explicit placeholder/wait state.
Native upload capacity, bytes and frame credits bound realization separately from
CPU preparation. A product is native-ready only after the backend establishes the
required ordered use/completion guarantees. Failure preserves the previous usable
binding and diagnostic; it does not acknowledge the new source revision.

Product invalidation requests paint and scoped service work. Idle waiting uses
completion wakes/deadlines, not continuous polling or forced rendering. Services
can prepare during a paused app; authoritative activation remains at the app's
model boundary. Dataset-only tools pump the same services without a window.

## Measurements

Record source/recipe/profile identity, request/source/publication times, queue age,
preparation and publication p50/p95/max, result age, coalesced/canceled/stale work,
cache reuse, output counts/bytes, missed deadlines and blocked reasons. Attribute
allocation ownership separately from logical demand and estimates. Track GPU
uploads/bytes/retirement separately; source-to-present timing cannot be inferred
from CPU preparation duration. Histories and labels are bounded, available without
F10, and detailed logs are explicitly enabled. See [verification](SPATIAL_TESTING.md).
