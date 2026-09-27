# Networking direction

This is a design boundary, not an implemented transport or multiplayer service.
C++ remains the application authoring language. No remote UI tree, executable
discovery, arbitrary remote property setter, or application deserialization is
implied by networking support.

## Layers

Shared application commands and models must be usable without AppHost, windows,
UI nodes or GPU objects. A client adapter projects permitted model state into UI
and scenes. Authority validates commands; only client-visible state is replicated.
Local scene/node identities and shared pointers are not wire identities.

A thin transport should expose connections, bounded messages, delivery options,
disconnects and statistics. Protocol codecs, session/authentication, replication,
prediction and service integration sit above it. Richer implementations can be
explicit opt-ins with capability negotiation rather than silently changing the
meaning of the thin API. Steam is a future integration: keep transport connection
IDs, application player IDs and provider identities separate. Do not require a
Steam runtime for ordinary local/client/server execution.

## Runtime obligations

I/O waits are asynchronous, not permission to mutate UI/model state concurrently.
Bound receive bytes, message sizes, pending work and per-update processing. Publish
decoded data at owner-thread boundaries using session generations and activation
lifetimes. Replace obsolete snapshots only when the protocol permits it; reliable
commands need backpressure or explicit failure, never silent dropping.

An authoritative server needs a headless runner and its own timing policy. The
interactive clock's focus pause and discarded catch-up time are not server or
lockstep guarantees. Protocol versions, time units, sequence numbers, wire IDs,
decode limits and reconnection/full-resynchronization behavior must be specified
before introducing a remote implementation. Local implementations should retain
the same queued completion ordering instead of hiding asynchronous behavior.

## Known workloads and shared foundations

| Workload | Application policy to decide |
|---|---|
| Real-time replicated space/RTS simulation | Tick authority, snapshot/delta cadence, relevance filtering, interpolation and whether prediction or deterministic lockstep is actually needed |
| Command-oriented card sessions | Turn/phase validation, idempotent command IDs, revisioned public state, player-private views and reconnect/resume |

Both need versioned bounded codecs, independent player/session/connection
identities, authenticated command validation, and an explicit authority. A
connection generation rejects late work after replacement. Reconnection starts
from an authoritative snapshot before applying later changes. Do not send a
complete server model and rely on client UI to hide private information.

Delivery policy belongs to message categories: reliable commands, replaceable
snapshots, and optional transient effects have different overflow and ordering
requirements. Expose queue sizes, round-trip estimates, loss/retry information
where available, and per-category byte/work budgets without leaking transport
implementation types. Platform/provider capabilities must be explicit; browser
transports and a future Steam adapter need not offer identical delivery modes.

Useful future constituent tests exercise a deterministic in-memory transport
with delay, loss, duplication, reordering, disconnects and bounded queue overflow.
Test resynchronization, stale-owner rejection, command deduplication and private
view filtering independently of graphics and live service credentials. Async I/O
publishes messages; concurrent simulation work is a separate scheduling decision.
None of these transport/protocol facilities is implemented yet.

## Decisions still required

- Which of the two workloads will first exercise a concrete protocol
- Authority and hosting: dedicated server, player host, or another explicit model
- Delivery/order requirements and overflow policy for each message category
- Identity/authentication, discovery, lobby and relay responsibilities
- Transport/library choice; none is added by this design
- Provider-specific Steam capabilities and fallback behavior

Camera, UI and renderer state are normally local presentation. Shared gameplay
state may drive them, but networking does not own their implementations.
