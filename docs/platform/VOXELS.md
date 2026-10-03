# Voxel storage, edits and checkpoints

The voxel module specializes [spatial datasets](SPATIAL_DATA.md) for bounded
regular grids. It supplies addressing, chunk storage, editing and adapters without
requiring terrain generation, game materials, cellular physics or one entity per
sample. [Products](SPATIAL_PRODUCTS.md) consume declared interpretations.

## Values and interpretation

| Value | Shape and contract |
|---|---|
| `VoxelGridDefinition` | GridDefinition plus supported channel groups and voxel query/edit limits |
| `VoxelChunk` | Immutable ChunkSnapshot; no renderer, solver handle or simulation flags required |
| `VoxelInterpretation` | Versioned channel-to-domain rules, declared inputs and supported sampling/spacing |
| `SurfaceDefinition` | Stable domain surface ID and separate render, collision, navigation and sound profile references |
| `InterpretationRef` | Definition identity/version/digest included in dependent product keys |
| `VoxelSelection` | Dataset/epoch, chunk and product revisions, sample address, channel, placement revision and hit position/normal |

Applications define whether a sample represents a block ID, intensity, label,
occupancy or another quantity. A framework surface definition connects optional
profiles; it does not derive collision from opacity or sound from RGB. Changing a
render profile does not change the authoritative sample or physical profile.
Missing required mappings report Unsupported; optional profiles remain absent.
Domain labels and palette indices are different types/meanings. A chunk-local
palette index is never a stable material identity on disk or across chunks.

Baseline storage supports uniform and dense typed channels. An edit to a uniform
chunk materializes only the affected bounded storage group. Unchanged pages remain
shared. Palette compression is an interchangeable encoding with explicit codec
capability, not a requirement to quantize arbitrary scalar data.

## Editing API

| API/value | Fields and contract |
|---|---|
| `CommandId` | Issuer/session epoch plus monotonic sequence, scoped to an explicit authority |
| `ExpectedRevisions` | Dataset epoch, schema/source identities and bounded address/revision expectations; directory revision for absent/default regions |
| `RegionPatch` | Address/channel, checked local bounds, operation and typed bounded payload or mask |
| `PatchOperation` | SetValues, Fill or RevertToBaseline; no arbitrary executable callback in a serialized edit |
| `SpatialEdit` | CommandId, DatasetId, ExpectedRevisions, patches, authority context, limits and CommitPolicy |
| `EditLimits` | Chunks, samples, payload bytes, scratch, candidate bytes and preparation work |
| `CommitPolicy` | ModelBoundary or RequireProducts with an exact product/readiness barrier |
| `EditCandidate` | Owned replacements and change summary tied to captured inputs; unobservable until publication |
| `ChangeSet` | Commit identity, old/new root revisions, changed chunk/channel/index ranges and coverage changes |
| `EditReceipt` | CommandId, CommitId, dataset revision, ChangeSet and DurabilityState |
| `EditOutcome` | Applied, NoChange, Conflict, NeedsData, Denied, Unsupported, BudgetExceeded, Cancelled or Failed |
| `EditService::submit / cancel / poll / forget` | Bounded asynchronous preparation with retained outcomes |
| `DatasetStore::commit` | Owner-boundary expected-revision validation and atomic root replacement |

Trusted application code supplies the authority validator. Permission, schema,
values, ranges and bounds are checked before work and relevant authority/revision
conditions again before commit. Offline tools can use an explicit local authority;
remote commands never inherit its privilege. No creator-code isolation is implied.

Patch overlap order is the submitted order within one transaction. Payload sizes
must exactly match encoding and checked bounds. Commands requiring unloaded data
return NeedsData or explicitly request it through their service policy. No implicit
allocation of an unbounded empty world. Set-to-default/empty creates a persistent
override; only RevertToBaseline removes that override. Reversion requires the exact
baseline or returns NeedsData/Incompatible through a structured diagnostic.

Preparation reserves candidates, scratch, index updates and retained preimages.
It stages all chunks before one root publication. Conflict, cancellation before
commit or failure changes none. Once publication succeeds, cancellation cannot
undo it. NoChange returns a receipt without needless content revision increments.
Large brushes are bounded jobs; applications partition oversized operations into
explicit transactions rather than receiving hidden partial commits.

`EditState` is Queued, WaitingForData, Preparing, WaitingForProducts, ReadyToCommit
or Terminal. Applied/NoChange are terminal receipts; Conflict/Denied/Unsupported
and other failures are terminal outcomes. Retries after a terminal conflict are
new commands with explicit expectations, not automatic replay of arbitrary edits.
Durability can advance after Applied through Pending to Durable/Failed/Uncertain;
that update does not apply the edit again. EditService retains command outcomes
while DatasetPersistence owns bounded save demand and storage acknowledgements.

A bounded issuer receipt table provides deduplication. Retained command IDs return
the original outcome; compacted IDs at/below the accepted sequence watermark are
rejected as already retired, never executed again. Issuers send ordered commands;
a gap requires retry/resynchronization rather than skipping unseen authority work.
Persist watermarks with durable commands; scope replacement issues a new epoch.
`forget` releases caller result retention, not authority deduplication guarantees.

## Publication and derived readiness

Dataset publication precedes downstream invalidation at the same owner boundary.
Consumers acquire the new root plus its ChangeSet, never half of a multi-chunk
edit. Bounded change journals retain stamps for active jobs; if precise history
is unavailable, conservatively invalidate/rebuild rather than assume unchanged.
An empty region change can invalidate a product just as occupied samples do.

Boundary edits invalidate consumers whose declared input halos intersect changed
channels/ranges. Absent neighbors retain dependency/version evidence in the
index; a later load cannot reuse an old boundary product. Selection commands
revalidate VoxelSelection's epoch, address and revision; stale geometry is not
permission to edit whichever sample is now under the cursor.

ModelBoundary commits authority and exposes stale/pending products explicitly.
RequireProducts stages matching products against the candidate root, then validates
and publishes the root plus a coherent product set at the designated model/adapter
boundary. An unavailable required adapter fails or waits within deadline; no hidden
fallback. Activation of solver resources uses its explicit safe-boundary protocol;
failed activation preserves the prior dataset and active bindings.

Render-only consumers may retain older geometry under a declared age policy.
Collision/navigation consumers may stop, wait or pin an explicitly coherent older
simulation generation. They cannot label it current after geometry changes.
World-bound publication uses the [world dataset binding](WORLDS.md#dataset-bindings)
contract. One transaction covers one dataset; coordinated world checkpoints name
exact dataset roots rather than claiming a distributed transaction.

## Live data and undo

`LiveEditPolicy` is ReadOnly, Captured or Overlay. ReadOnly rejects mutations;
Captured edits a pinned immutable sample. Overlay identifies its baseline sample
and a domain-supplied bounded rebase/conflict policy. Source replacement cannot
silently reinterpret an overlay's addresses, labels or meaning.

`UndoRecord` retains original versions, changed values/override presence and the
forward command identity under explicit byte/history limits. Undo submits a new
validated edit with expected revisions; it can conflict. Simulation side effects,
remote actions and external I/O are not reversed automatically. History eviction
is observable. An operation that cannot retain its required preimage is refused
before publication or explicitly declared non-undoable by the caller.

## Persistence API and encoding

| API/value | Shape and contract |
|---|---|
| `DatasetCheckpoint` | Schema/version, dataset/grid definitions, exact baseline/source locks, root index, overrides, dedup watermarks and domain metadata |
| `ChunkBlobRef` | Exact digest, codec/schema, bounded encoded/decoded sizes and payload location |
| `CheckpointRoot` | Dataset revision, immutable index/blob references and parent store revision |
| `DatasetStorage::putBlob / readBlob` | Bounded immutable payload storage; verify digest/schema before adoption |
| `DatasetStorage::publishRoot` | Compare-and-swap root publication with documented durable acknowledgement |
| `DatasetPersistence::save / restore` | Retain a coherent snapshot; stage/validate all metadata before owner replacement |
| `DurabilityState` | Volatile, Pending, Durable, Failed or Uncertain, independent of edit publication |
| `SaveReceipt` | Commit/dataset revision, store revision, durability and diagnostic |

Large datasets use immutable bounded payload/index pages. WorldStore metadata
holds exact dataset checkpoint references instead of embedding the full resident
volume. New blobs become reachable only through the published root. Crash recovery
selects the last valid committed root; orphan reclamation respects retained roots,
readers and in-progress saves. Unknown codecs/providers preserve saved references
and report unavailable interpretation without destroying evidence.

Sparse overrides can compact into snapshots without losing set-to-empty edits or
baseline identity. Failed writes/compaction preserve the previous checkpoint and
committed dirty state. In-memory edit success is not durable saving. Uncertain
publication is reconciled by reading the root, not blindly retrying the previous
expected revision. Concurrent saves cannot let an older snapshot overwrite newer
state. Cross-dataset/world saves flush referenced blobs/roots before publishing one
world checkpoint that selects a coherent set; unselected newer roots do not alter it.

A changed generator/schema/domain interpretation requires explicit migration or a
retained compatible provider/baked baseline. A derived mesh cooker update only
invalidates its cache. Restore creates a new runtime epoch and rejects old jobs.
See [versions](MANIFESTS.md) and [verification](SPATIAL_TESTING.md).
