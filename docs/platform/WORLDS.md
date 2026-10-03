# Worlds, spaces and spatial state

An application owns its world; an authority can own the same model without a
window or renderer. World identity, coordinate frames, residency, simulation and
presentation are separate contracts. A small app can use one space, an always
resident cell and kinematic movement without creating unused services.

These contracts include moving frames, transfers, persistence and spatial queries.
[Streaming](STREAMING.md), [procedural/voxel content](PROCEDURAL.md),
[navigation](NAVIGATION.md) and spatial [networking](NETWORKING.md#spatial-authority-and-interest)
are implementation requirements. [Physics integration](PHYSICS.md#future-implementation)
remains future; its adapter boundary is specified here and in PHYSICS.md.

## Identity and ownership

| Value | Contract |
|---|---|
| `WorldId` | Stable saved/session world identity, distinct from app and package identities |
| `WorldEpoch` | Runtime incarnation; changes on replacement/restore and rejects stale work |
| `EntityId` | Stable identity within a world; never reused for a different logical entity |
| `SpaceId` | World-qualified stable coordinate-space identity; foreign-world values reject |
| `EntityHandle` | Checked runtime identity/generation; not a save or wire identity |
| `ZoneId` | Semantic area with explicit membership/rules; not a residency owner |
| `CellId` | Stable residency partition identity; entities can move between cells |
| `FrameId` | Checked reference-frame identity within one space |
| `WorldSnapshot` | Immutable retained state at an epoch, revision and simulation tick |
| `WorldVersion` | Expected epoch/revision pair; restoration cannot accidentally accept stale commands |

Entities reference immutable definitions and own mutable domain state. Neither
Scene3D ObjectId nor UI NodeHandle substitutes for EntityId. A SceneProjection
maps entity representations to scene handles; one entity can have zero or many
render objects in several views. Removing a view does not destroy its entities.
Render parents do not establish entity ownership, frame attachment or authority.

World owns mutation and publishes snapshots at declared boundaries. Snapshots
retain immutable storage, not permission to access the live model from workers.
Retention has byte/count/age limits; copy-on-write or owned copies are internal
choices. Cross-entity references resolve by identity and can be unresolved while
content is absent; they do not silently pin every referenced cell.

| Operation | Boundary/result |
|---|---|
| `World::apply(batch, expectedVersion, tick)` | Validate bounded domain mutations and commit once; conflict/failure preserves prior state |
| `World::resolve(entity)` | Return current checked handle/status; distinguish absent residency from destroyed identity |
| `World::snapshot()` | Retain an admitted immutable generation; never borrow mutable storage |
| `World::setActivation(request)` | Validate domain policy and readiness before the next simulation boundary |

Command batches carry world epoch, authority and command identity; app schemas own
payload validation. Successful commits advance revision and emit copied domain
events. The caller cannot bypass session validation by choosing a streaming cell.
Read-only snapshots expose only their retained coverage, not the entire persistent
world by implication. Retention pressure refuses new work or ends a consumer's
contract explicitly; it never invalidates memory behind a live lease.

## Coordinates and numerical bounds

World coordinates use meters, seconds and radians, +Y up and camera forward +Z.
Physical adapters use kilograms. Asset import explicitly converts authored units
and axes. UI pixels and arbitrary display-list coordinates remain separate;
2D world apps use a declared plane in a space and an explicit view mapping.

| Value/API | Contract |
|---|---|
| `WorldPosition` | SpaceId plus three finite CPU doubles in meters |
| `WorldPose` | WorldPosition and normalized orientation; scale is separate presentation/content data |
| `WorldVelocity` | SpaceId, linear meters/second and angular radians/second |
| `relativeTo(position, origin)` | Checked same-space double subtraction before any float conversion |
| `SpatialLimits` | Finite bounds, maximum local extent and required positional tolerance |
| `RenderOrigin` | Per-view SpaceId, double position and origin revision |
| `SceneProjection::extract(snapshot, view, origin)` | Immutable local render submission plus source revisions |

Reject mismatched spaces, nonfinite values, arithmetic overflow and positions
outside declared bounds. Double coordinates are finite and have a precision
budget: validate bounds against required tolerance rather than promising infinite
space. Integer chunk addresses also have explicit limits. A space definition
cannot silently change units or coordinate interpretation after publication.

Geometry stays mesh-local in floats. Preserve precision through frame/hierarchy
composition, subtract RenderOrigin in double precision, then validate conversion
to local float transforms. Do not flatten large world translations into float
matrices first. Camera, lighting positions, picking, bounds and material inputs
use the same origin for a submission. Stable world-coordinate material effects
need explicit high/low or cell/local inputs; local render coordinates cannot be
treated as persistent world addresses.

View orientation is stored independently of position; do not derive it by adding
a tiny forward offset to a large float eye. Camera movement/origin changes update
view-relative values, not asset identity, mesh uploads or every authored transform.
Retained submissions carry their own origin and remain valid until retired.
Origin changes remap previous/current presentation samples consistently; they
are not teleports and do not reset world velocity or emit gameplay movement.

Each view can have a different origin. A physics working frame is independently
chosen by its adapter, never implicitly tied to the active camera. Depth precision,
near/far clipping and visibility distance remain separate from world bounds.

## Reference frames and transfers

`ReferenceFrame` is a rigid parent-relative translation/orientation within one
space. Roots are space-relative; parents are checked and cycles rejected. Scale
and shear are excluded. `FrameSample` carries frame identity, world pose, linear
and angular velocity, epoch/tick, revision and discontinuity sequence.
`FramePosition` pairs a frame with a local offset; conversions require a matching
sample. Static frame definitions can have stable saved names; runtime FrameIds
are rebound on restore and are not serialized as handles.

Attach/detach commands explicitly preserve world pose or adopt local pose and
declare velocity inheritance. Conversion includes frame translation velocity and
angular contribution at the attachment point. Sampling must use a consistent tick
for the frame and subject. Interpolation resolves both in a common space; parent
replacement, teleports and transfers never blend unrelated histories. Frame removal
requires an explicit detach, transfer or dependent-removal policy before commit.
No implied forces or contact constraints follow from frame attachment.

`WorldTransfer::prepare(request)` validates source identity/revision, destination
SpaceId/pose, declared dependent entities, admission and readiness requirements.
The request specifies velocity policy, authority and cancellation/deadline.
`commit(ticket)` revalidates at a world mutation boundary and atomically updates
membership, frame attachment and poses; failure preserves the source. Preparation
does not make destination entities authoritative or send duplicate spawn events.
Changed source state requires revalidation or a fresh ticket, not stale placement.

Destination readiness can require simulation/query/navigation data independently
of visuals. A physical-placement requirement fails as Unsupported without a
capable adapter; kinematic transfer is an explicit policy. Cross-space velocities
are reset or mapped through an explicit rigid mapping, never copied by coincidence.
Successful transfer increments discontinuity, resets interpolation/follow history
and publishes one domain result. Local transfers remain within one world/authority;
distributed authority migration and seamless portals are future work.

## Spatial queries

| API | Contract |
|---|---|
| `SpatialSnapshot` | World epoch/tick, index revisions, space and explicit coverage |
| `QueryFilter` | Authored category masks, exclusions and query purpose |
| `QueryBudget` | Work, candidate and output limits; no unbounded collect-all |
| `SpatialQueries::raycast` | Ordered hits with entity identity, distance, position and normal where defined |
| `SpatialQueries::overlap` | Bounds/shape overlaps with declared broad-phase or exact semantics |
| `SpatialQueries::sweep` | Shape motion query when supported; never a disguised raycast |
| `QueryResult` | Completion/coverage, precision mode, hits, revisions and limit/capability diagnostics |

Queries operate in one identified space and snapshot. Index structures are
internal: static geometry, dynamic entities, navigation and streaming need not use
one universal grid. Render visibility/material opacity does not determine query
membership. Baseline bounds and triangle queries do not imply physical collision.

Results distinguish Complete, Incomplete, Unavailable, Unsupported and Invalid.
Only Complete with no hits establishes no hit for the declared geometry and scope.
Truncation, exhausted work or missing cells yields Incomplete, even if some hits
are available. Ray ordering uses distance then stable identity/primitive order for
ties; nearest-hit claims require complete nearer coverage. Missing data can prompt
an explicit streaming request; synchronous queries do not load disk/network data.

Zone enter/leave events compare committed membership using declared overlap and
boundary rules. Incomplete coverage cannot synthesize a confirmed exit. Initial
membership, teleports and destruction have explicit reasons and stable ordering.
Queries and triggers do not apply forces or solve penetration.

`world/Queries.hpp` supplies immutable, ledger-admitted query snapshots from a
WorldSnapshot, explicit SpatialCoverage and authored QueryPrimitives. Primitive
identity is `(EntityId, primitive)`; category and single-purpose bits filter
independently of presentation. A snapshot binds geometry to live entities at its
captured revision and retains that revision after later mutation/destruction.
Raycasts use world-relative double arithmetic and normalize finite nonzero
directions; distance is a finite nonnegative meter limit. Bounds hits originating
inside a box have distance zero and no invented surface normal. Triangle hits are
two-sided and expose their authored winding normal. Overlap uses closed AABBs,
including conservative triangle bounds, and explicitly reports Bounds precision.
Unsupported shape sweeps return Unsupported; malformed inputs return Invalid.

QueryBudget counts primitive visits, broad-phase candidates and retained hits.
Hit-limited results retain the nearest available ray hits but remain Incomplete.
Exclusions and output capacity have snapshot-level bounds; output storage is
admitted through the shared ledger before traversal. Admission refusal raises
ResourcePressure without mutating the index. Each result includes source world
version, tick, index revision and consumed work. A coverage box asserts complete
geometry only inside its extent; providers with holes mark coverage Incomplete
or publish separately complete regions. Never infer full coverage from an empty
primitive list.

## State, activation and persistence

Logical existence, CPU residency, simulation activation, view visibility and GPU
residency are independent. `ActivationPolicy` selects dormant, coarse or full
domain updates per entity/group. Coarse simulation is app-defined behavior, not
automatically running physics at an unsafe larger timestep. A dormant entity has
explicit wake/deadline handling and does not accidentally consume paused time.
Authority requirements override local visibility-based deactivation.

`WorldStore::save(snapshot, expectedStoreRevision)` writes a versioned candidate and
atomically publishes a durable checkpoint; mutation can continue after capture.
The expected store revision is a compare-and-swap guard against another writer,
separate from the snapshot's captured world revision.
The result reports the captured revision, not the latest live state. Per-world
serialization prevents an older save completion replacing a newer checkpoint.
`load` validates/migrates a candidate before owner-thread replacement and advances
WorldEpoch. Failure preserves the active world and last valid checkpoint.

A save transaction retains the persistent checkpoint plus committed dirty state;
snapshot coverage of resident entities alone is not a complete save. Unchanged
unloaded cells carry forward by validated stored references. Domain migrations,
transfer/delete journals and checkpoint publication share an atomic manifest or
equivalent transactional store. Flush requirements define durable acknowledgement;
an in-memory queue acceptance is not successful persistence.

Saved state includes stable identities, space/frame definitions, exact content
locks, generator identity/configuration, entity/voxel changes, deletions and
required domain state. Cell unload cannot discard unsaved changes. Keep dirty
data retained until acknowledged storage, or refuse eviction under pressure.
Meshes, GPU handles, navigation caches and runtime pointers are not authoritative
save data. Multi-cell edits/transfers use one transaction/checkpoint revision;
recovery cannot expose half a move or resurrect a deleted entity from base content.
Save storage is separate from immutable packs and shared settings.

## Future capabilities

Planetary/astronomical coordinate hierarchies, non-Euclidean or seamlessly rendered
portals, distributed authority migration, and cross-world atomic transfers are
future. Ordinary separate spaces, rigid moving frames and prepared local transfers
do not require them. Physics integration, camera obstruction and root motion retain
their explicit future status. No universal ECS or serialized executable behavior
is required; app authoring/loading and UI editing remain future capabilities.

See [world verification](WORLD_TESTING.md). Design references:
[camera-relative rendering](https://dev.epicgames.com/documentation/en-us/unreal-engine/large-world-coordinates-rendering-in-unreal-engine-5)
and [floating-point limits](https://docs.godotengine.org/en/stable/tutorials/physics/large_world_coordinates.html).
