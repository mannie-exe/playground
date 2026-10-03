# World streaming and activation

Streaming maintains bounded working sets for a [world](WORLDS.md), independent of
whether content is authored or [generated](PROCEDURAL.md). Package acquisition,
CPU preparation, simulation activation and GPU realization have separate owners.
Cell membership is not entity identity, visibility or authority.

## Definitions and API

| API | Contract |
|---|---|
| `CellDefinition` | Stable ID, space/bounds, content references, revisions and declared dependencies |
| `WorldManifest` | Versioned space/cell index, exact content lock and optional generator recipes |
| `StreamingSource` | Space/position, priority, purpose, radii, prediction bounds and required readiness |
| `CellRequest` | World epoch, cell/revision, generation, readiness, priority and deadline |
| `CellLease` | Retains admitted data for a declared consumer/purpose until released |
| `CellState` | Requested versus available readiness, work status, leases and diagnostic reason |
| `WorldStreamer::setSources` | Validate and replace one owner's demand without changing others |
| `request / cancel / requestState / forget` | Bounded explicit demand; retain cancellation/failure until the owner forgets its request |
| `state / lease` | Inspect cell readiness or retain a matching current generation |
| `advance(budget)` | Admit and publish bounded work at the owner's service boundary |

Sources can represent players, views, authority regions, destination preparation
or explicit domain needs. They are not necessarily cameras. Readiness dimensions
include definition/data, query, navigation, simulation and presentation; no single
Loaded flag stands for all of them. A headless authority requires no GPU resources.

`world/Streaming.hpp` supplies an owner-thread `WorldStreamer` attached to a
`ServiceScope` or advanced explicitly. `WorldManifest::spaces` declares coordinate
limits; every cell and source must name a declared space. `CellProvider::prepare`
runs on the shared executor with immutable dependency products, admitted output
and scratch bounds, and cooperative cancellation. A provider advertises supported
readiness; unsupported dimensions fail explicitly. Package and generator domains
supply their providers rather than triggering implicit acquisition.

Control calls validate bounded source/request/manifest inputs synchronously.
`advance` grants count cell operations, copied metadata bytes and published worker
messages; provider work has separate scratch/output bounds. These are cooperative
work limits, not thread preemption or allocator-enforced resident-memory limits.
`replace` advances a cell's content revision and invalidates its dependency closure;
changing dependency topology requires a new manifest/scope. Leases retain the prior
immutable generation across replacement, eviction and scope closure.

Cells use explicit authored bounds or a declared grid. Grids use floor division
for negative coordinates and half-open intervals to assign boundaries once.
Zones may cross cells. Membership of large objects is indexed in all necessary
query regions while a declared owner retains the object only once. Moving an
entity across a boundary changes residency bookkeeping without respawning it.

## Admission, publication and retirement

Work status is Absent, Queued, Preparing, Ready, Failed or Cancelled; retirement
is a separate consumer-lifetime operation. Readiness is revisioned per product.
New preparation stages beside the current usable generation; failed replacement
does not remove that generation. Stale readiness remains explicitly stale and
cannot satisfy a request requiring current geometry or simulation data.

Each result carries world epoch, cell/content revision and request generation.
Workers own immutable inputs and return owned products. Publication rejects stale
work, retains terminal outcomes before wake notification and changes live state
only at owner boundaries. Closing a world cancels demand and invalidates publication
before releasing storage. Accepted jobs retain capacity until they actually retire.

Service work reserves encoded/decode/generation scratch, candidate output and old/new
overlap through the shared ledger. Budgets cover jobs, bytes, cell counts and
owner-thread publication work. Enormous cells must be cooked into bounded products
or refused; moving an unbounded instantiate operation onto the owner thread is not
streaming. Hidden staging can be incremental; activation publishes a coherent
generation only after all required pieces are ready.

Use load/unload hysteresis, minimum retention and bounded directional prediction.
Priority aging prevents indefinite starvation without violating pinned requirements.
Demand merges across owners; canceling one source cannot invalidate another's lease.
An explicitly pinned transfer, simulation dependency or unsaved edit outranks
speculative preparation. If mandatory working sets exceed budget, report admission
failure/degraded policy; never evict live resources or claim readiness anyway.

Logical deactivation, CPU eviction and GPU retirement are separate. Frames, queries,
audio voices, navigation corridors and saved-state writes can retain generations.
Release discovery first; retire backing storage only after consumers and native
submissions finish. Shared assets carry one charge per owned allocation, not one
per cell reference. Track pending retirement so it cannot hide budget pressure.

## Content and missing regions

Cell payloads are independently indexed and bounded [pack entries](PACKAGES.md).
Shared meshes/materials/textures use catalog identity and explicit dependency
closure. A whole-model GLB or one compressed world entry is not automatically
incrementally streamable. Cook cell records and appropriately sized asset payloads;
keep authoring groups distinct from runtime cells and pack boundaries.

Baseline streaming loads bounded asset products. Visibility culling and LOD are
independent policies. Partial texture-mip residency and automatic HLOD generation
are future capabilities; cell streaming does not imply either is implemented.

Required installed content uses the exact lock. Missing optional remote content
requires explicit PackageAcquisition; AssetReader and spatial queries never fetch
implicitly. Progress/failure identify cell, asset, phase and required readiness.

Apps declare unavailable-region behavior: hold at a known boundary, wait for a
transfer, keep an older safe generation, or use an explicitly permitted placeholder.
A visual placeholder cannot count as query, navigation or physics readiness.
Authoritative movement cannot enter an unavailable required simulation region by
mistaking absence for empty space. Kinematic demos can allow unconstrained travel
only through an explicit policy that does not claim collision safety.

## Measurement and verification

Report source demand, ready/stale cells by purpose, queue age, preparation and
publication duration, admitted/retired bytes, pinned reasons, cancellations and
failed deadlines. Separate cold load, warm reuse and steady traversal. Rendering
FPS alone cannot establish streaming correctness or responsiveness.

[World verification](WORLD_TESTING.md) defines boundary, pressure and recovery
workloads. [World Partition](https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition-in-unreal-engine)
is a reference for source-driven cell residency, not a dependency or required grid.
