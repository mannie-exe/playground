# Procedural worlds and voxel data

Authored and generated cells share [streaming](STREAMING.md), identity, accounting
and persistence contracts. Generation produces data, not executable app instances.
Procedural/voxel generation, bounded block edits and derived mesh/navigation work
are implementation requirements; an editor or general scripting runtime is not.

## Definitions and generation

| API | Contract |
|---|---|
| `GeneratorDefinition` | Stable generator ID/version, supported schema, parameters and exact content dependencies |
| `GenerationKey` | Generator/build identity, seed, configuration digest, cell address and output kind |
| `GenerationRequest` | Key, bounded immutable neighbor inputs, output/work limits and cancellation |
| `CellGenerator::generate` | Produce owned immutable cell data under the request's limits |
| `GenerationResult` | Products, dependency revisions, measurements and terminal outcome |
| `GeneratedEntityKey` | Stable cell/feature identity mapped to persistent EntityId without runtime-order dependence |

Generators are trusted compiled providers selected by an allowlisted ID. Packs
can supply bounded validated parameters for registered recipes, not C++ names,
scripts, filesystem/network access or arbitrary executable plugins. Unsupported
recipes fail before work. Authoring/build and runtime invoke the same provider
contract; offline baking can replace generation with immutable equivalent outputs.

Same key and declared compatible generator build produce identical authoritative
data independent of scheduling and traversal order. Specify PRNG algorithm, seed
encoding, coordinate hashing, overflow behavior, numeric operations and canonical
serialization; standard-library distributions or unspecified floating-point math
cannot establish cross-platform determinism. Providers declare supported targets
and pass golden fixtures; incompatible targets consume authoritative baked data.
Derived visual products may have a separately declared tolerance/compatibility key.

Cross-cell features have a canonical owner and bounded influence region. Requests
declare neighbor halos and derive them from immutable base inputs, not whichever
neighbor finished first. No recursive unbounded generation or load-order-dependent
random stream. Cancellation, work exhaustion and output-limit failure publish no
partial authoritative cell. Long providers expose cooperative checkpoints; a job
timeout cannot forcibly interrupt unsafe native execution.

## Voxel coordinates and edits

`VoxelGridDefinition` specifies SpaceId, origin, positive voxel size in meters,
bounded integer chunk extent and stable block/material definitions. Signed 64-bit
chunk addresses and bounded local integer coordinates identify blocks; checked
floor division and half-open bounds apply on negative axes. Conversion to double
world positions must satisfy SpatialLimits rather than accepting every int64 value.
Rigid placement follows world frame contracts; nonuniform physical voxel scale is
not implicit. 2D tile fields use the same address rules on a declared plane.

`VoxelChunk` owns immutable revisioned block/palette data with explicit byte and
dimension bounds. Baseline geometry is a block grid; smooth density-field terrain,
fluid simulation and multiresolution voxel terrain are future capabilities.

`VoxelEdit` carries command identity, expected chunk revisions and bounded block
changes. The authority validates permissions, values and affected cells, stages the
transaction, then publishes all affected chunks together. Duplicated commands do
not reapply edits. Unloaded chunks are prepared explicitly before mutation or the
request fails; missing data is not assumed to be air. Emptying a generated block
is a persistent override, not deletion of the evidence that it was changed.

Edits invalidate only products depending on changed blocks and their declared
neighbor halo. Boundary edits invalidate the neighbor's affected product even if
that neighbor is absent; its next preparation observes the new revision. Queries
and navigation cannot advertise old data as matching new authoritative geometry.
Frame-local mesh presentation may lag under an explicit stale-visual policy.

## Derived products and persistence

Meshing, spatial indexes and navigation preparation consume immutable source
revisions and publish separately. Cache keys include generator/source revisions,
neighbor revisions, product settings and cooker version. Replace complete mesh
handles; never mutate geometry retained by a render submission. Baseline meshing
removes internal faces and preserves material boundaries; further mesh optimization
must preserve the same occupancy/material semantics.

Missing neighbor data has an explicit visual policy (temporary boundary faces or
deferred mesh). It never establishes empty query/navigation space. Changing that
availability invalidates affected products. Neighbor stitching and tile publication
must not expose navigable seams before their dependencies agree.

Saves pin generator identity/version/configuration and content digests, plus voxel
overrides, entity changes and deletion records. A seed alone is not a save. Cache
eviction can discard regenerable products but not uncommitted edits. Generator
upgrades require explicit migration or a retained old provider/baked baseline;
silently regenerating old cells with a new algorithm is forbidden.

Sparse edits can compact into bounded chunk snapshots under the same save revision.
Compaction preserves negative overrides and exact baseline identity. Storage quota,
failed writes and interrupted compaction preserve the previous checkpoint and dirty
state. See [world persistence](WORLDS.md#state-activation-and-persistence) and
[verification](WORLD_TESTING.md).
