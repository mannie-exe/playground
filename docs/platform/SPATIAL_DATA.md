# Spatial datasets and sources

Spatial datasets own revisioned sampled data independently of scenes and worlds.
The runtime supplies scheduling, accounting, publication and persistence; the
application supplies meaning. [Voxel storage and edits](VOXELS.md),
[derived products](SPATIAL_PRODUCTS.md) and [simulation](SIMULATION.md) use this
boundary. These are implementation contracts, not a status report.

## Ownership and identities

| Value | Shape and contract |
|---|---|
| `DatasetId` | Stable opaque identity within an explicit catalog/save namespace; instances do not alias solely because they share a definition |
| `DatasetEpoch` | Runtime incarnation; replacement/restoration changes it and rejects earlier work |
| `DatasetRevision` | Monotonic publication revision within an epoch; exhaustion fails rather than wrapping |
| `GridId / ChannelId` | Stable definition-local identifiers, qualified by dataset at runtime |
| `ChunkAddress` | GridId plus signed int64 x/y/z; validated against declared bounds before arithmetic |
| `ChunkVersion` | DatasetId, epoch, address and content revision; unchanged chunks retain their revision |
| `SchemaRef / SemanticRef` | Stable schema/interpretation ID, format version and exact definition digest |
| `DatasetDefinition` | Definition identity/version, grids, schemas, source bindings and explicit storage/work limits |
| `DatasetSnapshot` | Retained immutable root identifying one coherent dataset revision and its available chunk versions |
| `ChunkSnapshot` | ChunkVersion, schema, valid sample bounds, channel views and retained StorageLease |
| `StorageLease` | Shared ownership of immutable backing allocations and their ledger tokens, not an extra byte charge |

Definitions, dataset instances, world entities, package entries and GPU realizations
have distinct identities. A dataset can be inspected, edited, simulated or exported
without AppHost, SDL or a World. A world attachment adds placement and authority
policy, not a copy of the data. One voxel does not imply one entity.

One owner publishes each dataset. Readers retain snapshots across threads;
workers never borrow mutable dataset containers. Snapshot lookup identifies
Known, Missing or OutsideDomain. A known uniform default is data; a missing chunk
is not equivalent. Coverage and revision are part of query results.

## Grid and channel definitions

| Value | Fields and validation |
|---|---|
| `IndexBounds` | Checked half-open int64 sample/cell bounds; finite addressable domain, including sparse datasets |
| `Extent3` | Positive bounded integer extents; checked volume and byte multiplication |
| `GridDefinition` | GridId, cell bounds, chunk extent, GridMapping and SchemaRef |
| `GridMapping` | Local origin, orthonormal axis orientation, positive finite per-axis spacing and explicit units |
| `ChannelDefinition` | ChannelId, ScalarFormat, bounded component count, SampleLocation, typed default, value constraints and optional SemanticRef |
| `ChannelSchema` | Ordered unique channel definitions and canonical encoding rules |
| `SampleLocation` | CellCenter or Vertex; defines sample support and boundary ownership |
| `ScalarFormat` | Explicit-width signed/unsigned 8/16/32/64-bit integers and float32/float64 |
| `SamplingProfile` | Channel, nearest/linear filter, explicit missing/outside policy and output limits |
| `SampleResult` | Status, typed value when known, contributing versions and missing coverage |

Grids use floor division on negative coordinates. Integer keys preserve full
validated addresses; truncated coordinate packing cannot silently alias cells.
Samples at cell centers are offset by half a cell from integer corners. Vertex
samples at shared chunk boundaries have one canonical owner, the chunk on the
positive side of the boundary; at the domain's upper edge, the last valid chunk
owns the endpoint. Other chunks request those samples as dependencies.

GridMapping converts index coordinates to dataset-local coordinates with checked
precision. A world binding then converts to its SpaceId/frame and obeys
SpatialLimits. Units must be declared and convertible before world/physics use;
a dimensionless visualization may remain dataset-local. Unequal axis spacing is
valid. An adapter that only supports uniform spacing rejects incompatible data.
Nonuniform spacing is not implicit nonuniform physical body scaling.

Channel identity does not dictate meaning. Occupancy, labels, intensity, color,
velocity and temperature are schema/interpretation choices, not mandatory fields
in a universal Voxel struct. A label channel rejects linear interpolation;
continuous numeric channels opt in through their interpretation. Conversion and
rounding are explicit. NaN/infinity acceptance and missing-value encodings are
schema rules, never confused with unavailable storage. Baseline geometry adapters
reject nonfinite geometry-producing values. Numerical channels are not sRGB.

## Storage and views

Storage encoding is separate from schema and canonical content identity.
`ChunkStorage` supports Uniform and Dense encodings through validated channel
views. Uniform stores a typed constant per channel; Dense stores bounded samples.
Palette/compressed providers advertise their encodings and decode through the
same admitted boundary; consumers never infer layout from a codec name.

`ChannelView::sample` validates address/type. `denseSpan<T>` succeeds only for a
matching native contiguous representation whose lease is retained; other layouts
require explicit bounded decode. No unaligned casts of arbitrary disk bytes.
Channel groups declare which channels share allocations and decode units. Loading
one channel may retain its group, which is visible in demand/accounting; the API
neither requires one allocation per channel nor mandatory all-channel loading.

`DatasetStore::snapshot()` retains a root; `lookup(address, channels)` returns a
versioned view/coverage result. Mutation is only through [SpatialEdit](VOXELS.md#editing-api)
or a validated source/simulation candidate. No mutable spans escape publication.
Copy-on-write storage shares unchanged pages. Old/new overlap, root/index pages
and all retained readers remain charged. Triple buffering is a simulation choice,
not the storage cost of every dataset.

## Service ownership and limits

`DatasetServiceProps` bounds datasets, resident chunks, channel groups, directory
pages, requests, jobs, dependency depth and retained results. PreparationLimits
bounds per-job bytes/work; ProductServiceProps and EditServiceProps independently
bound product keys, consumers, change history, candidates and receipts. Validate
positive required capacities, finite timings and checked byte/count arithmetic at
construction. A zero optional retention target disables caching, not live leases.
Policy updates are complete validated owner-boundary replacements; lower limits
preserve existing retained allocations and refuse growth until usage permits it.

Construct stores/services with explicit source/recipe registries, shared Executor,
ResourceLedger and wake endpoint. Services can attach to ServiceScope or be pumped
manually. Request methods return a stable request ID and Accepted/Busy/TooLarge/Closed
admission; only Accepted owns a result slot. Service admission is separate from
executor admission: an accepted service request can remain queued while the executor
is Busy. `poll` observes a retained result until `forget`. Temporary pressure has bounded retries/deadlines; invalid schema/input
fails before provider invocation. Closing prevents admission/publication immediately
while captured inputs remain alive until workers retire. Destructors do not perform
hidden durable saves or wait inside the app's ordinary update/render callbacks.

`DatasetStore::publishSource` validates a source candidate's epoch, request/sample
identity, coverage and expected root inputs before exchanging its immutable root.
Edits and simulation use the same owner publication mechanism. Independent chunk
publications need not reject solely because an unrelated chunk changed; validate
the exact read set, schema and coverage instead. A coherent sample/transaction
publishes one prebuilt root. The owner retains failed terminal diagnostics separately
from whatever previous usable snapshot remains available.

## Sources and preparation

| API/value | Shape and contract |
|---|---|
| `SourceRef` | Registered provider ID/version, source mode, schema, exact configuration and declared content references |
| `SourceMode` | Immutable, Reproducible or Live |
| `SourceCapabilities` | Schemas/encodings, channels, region limits, reproduction compatibility and supported targets |
| `DataRequest` | Dataset/epoch, request generation, address/range, channels, expected source identity/sample, priority, deadline and limits |
| `InputSnapshots` | Bounded owned source/dependency views with exact versions and coverage |
| `PreparationLimits` | Maximum inputs, output bytes, scratch bytes and work units |
| `PreparationContext` | Admitted storage/scratch facilities, work counter and stop token; no implicit world, UI or network access |
| `PrepareResult` | Ready candidate, NeedsInput, Unsupported, BudgetExceeded, Cancelled or Failed; dependency stamps, measurements and diagnostic |
| `SpatialDataSource::capabilities` | Describe supported requests before admission |
| `SpatialDataSource::prepare` | Worker-only bounded operation over immutable inputs; returns an owned candidate, never publishes |
| `DatasetService::request / cancel / poll / forget` | Owner-thread demand with retained terminal outcomes and bounded request storage |
| `DatasetService::advance / attach / close` | Publication through ServiceScope or explicit pumping; close invalidates publication before retirement |

A Ready result includes complete validated requested coverage, schema, payload and
source identity. NeedsInput names bounded missing dependencies, not a partial
successful chunk. Failure publishes nothing. Providers declare concurrency safety;
non-concurrent providers are serialized through admission, not called concurrently
by accident. Cancellation checkpoints and work-unit definitions are provider
contracts. Native callbacks cannot be forcibly preempted or sandboxed by a budget.

Requests use installed content or an explicitly supplied acquisition adapter.
Package verification, decode, generation and publication are separate stages.
Neither sample queries nor a generic source lookup performs hidden network I/O.
Sources include pack entries, decoded volumes, application recipes and captured
live frames. Concrete generator algorithms belong to applications; see
[procedural providers](PROCEDURAL.md).

## Reproduction and live samples

Immutable sources pin exact content/schema digests. Reproducible sources pin
provider/build compatibility, parameters, dependencies, seed and numerical rules.
Live sources identify session/epoch, monotonically increasing sample sequence,
clock domain and optional timestamp; timestamps alone do not establish identity.
Capturing live data creates immutable payload identity. Reopening a capture does
not reconnect to its former producer.

Cross-platform determinism is an explicit capability verified by fixtures, not a
condition imposed on every visualization. An authoritative domain requires a
compatible provider or baked/recorded authority data. A seed alone is insufficient.
Cached data from incompatible providers cannot satisfy current requests.

A live sample declares coherent coverage. Chunks from different samples may mix
only under an explicit consumer policy carrying per-region sample identities;
a coherent capture pins a complete sample or fails with missing coverage. Producer
queues are bounded. Callbacks with real-time constraints post preallocated bounded
messages; they cannot allocate, decode or acquire the resource ledger there.

## Bounded directory and demand

`DatasetDirectory` describes regions without enumerating the entire domain.
`DirectoryQuery` carries dataset/epoch, expected directory revision, index bounds,
channels, maximum entries/bytes and an opaque continuation cursor. `DirectoryPage`
returns validated descriptors, coverage, revision and a next cursor or completion.
`DatasetDirectory::query` is admitted worker work; paged indexes can require I/O.
Cursors bind the query and revision; replacement requires restart, not a mixed page.
Absent descriptors distinguish known default regions from unavailable content.

Demand limits bound outstanding queries, descriptors, recursion/dependency depth
and retained pages. No provider can expand the address domain implicitly.
Grid-backed directories compute checked descriptors; indexed datasets use bounded
pages. WorldStreamer adapts spatial demand to these descriptors while finite
WorldManifest cells remain supported. Dataset chunk, world cell, physics region,
navigation tile and package boundaries need not coincide.

## Verification

[Spatial workloads](SPATIAL_TESTING.md) verify schema/layout separation, negative
addresses, shared vertex ownership, anisotropic placement, coherent snapshots,
live sample replacement, bounded directory traversal and source failure. Unknown
coverage, unsupported schema and invalid values are observable outcomes, not zeros.
