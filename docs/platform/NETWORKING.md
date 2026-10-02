# Networking and shared sessions

Games and collaborative applications are equal consumers of the transport,
session and authority interfaces. Dedicated/self-hosted, player-hosted/LAN and managed hosting share the
same authority protocol; deployment changes endpoints and service provision.

## Meaning of multiplayer

| Workload | Shared state and policy |
|---|---|
| Real-time game | Authority ticks, player intents, snapshots, interpolation; prediction only where required |
| Turn/event-based game | Validated commands, phase/revision, private player views and reconnect |
| Live collaboration | Revisioned document/domain operations, permissions, presence and conflict responses |
| Hybrid experience | Reliable domain events plus transient presence or real-time state |

An authoritative session model serves all four. Revisioned operations
are not a CRDT. Offline concurrent edits, peer authority, deterministic lockstep,
rollback and host migration are separate features with their own semantics.
Camera, UI and local renderer state are presentation unless the app explicitly
models a shared camera or document. Do not replicate Node trees or pointers.

## Layers and API

| Layer | Values/operations | Ownership |
|---|---|---|
| Transport | ConnectionHandle, TransportCapabilities, send, receive, close, stats | I/O service owns sockets and bounded buffers |
| Protocol | ProtocolVersion, MessageType, bounded encode/decode, channel policy | No UI or renderer dependency |
| Session | SessionId, ParticipantId, authenticate/join/leave/resume, SessionEpoch | Authority owns admission and player visibility |
| Domain | CommandId, expected revision/tick, validate/apply, public/private views | App model owns rules and durable state |
| Client presentation | Snapshot buffer, interpolation, command results, presence | Owner thread projects accepted state into UI/scenes |
| Hosting | AuthorityRunner, embedded or dedicated; deployment configuration | Host owns service lifetime and server pacing |

Transport connection IDs, authenticated account/provider IDs, session participants
and domain entity IDs are separate. Connection generations reject stale I/O;
SessionEpoch rejects state from a replaced authority. Authentication establishes
identity, authorization checks each operation, encryption protects transport;
none substitutes for the others. A player host can inspect its authoritative
model; player-hosted private information cannot be protected from that host.

`send` reports Accepted, Backpressured, Closed, TooLarge or UnsupportedDelivery.
Accepted means locally queued, not delivered or applied. Application command
results acknowledge validation/application separately. Message payload ownership
is explicit; callbacks cannot retain borrowed I/O memory past delivery.

## Delivery and protocol contracts

| Category | Ordering/loss | Overflow |
|---|---|---|
| Commands/results | Reliable, ordered within their declared channel | Backpressure/refuse; never silently discard accepted domain work |
| State snapshots | Sequenced, newer supersedes older when self-contained | Replace stale queued state; missing delta base requests resync |
| Presence/transient effects | Best effort, expiry and sequence where useful | Drop/coalesce with counters |
| Bulk content | Separate throttled transfer policy | Cannot starve commands; not arbitrary pack execution |

Do not assume total ordering between channels. An envelope includes protocol
version, session epoch, type, bounded length and category-specific sequence,
revision/tick and command ID. Codecs define byte order, integer widths, UTF-8,
finite numeric ranges, field counts and unsupported-field behavior. Transport
reliability is not exactly-once model mutation: deduplicate command IDs within a
specified retention window, return the prior result, and reject expired retries.
Durable effects require atomic state/result persistence before durable success.

Join/reconnect negotiates protocol/features and authorized content versions,
then receives a filtered baseline followed by changes after that baseline's
revision. Bound snapshot assembly and retained change history. If the history or
delta base is unavailable, restart from a new baseline. Never send the complete
server model and rely on client UI to conceal private fields. Resume tokens are
short-lived scoped credentials, not connection handles or persisted plaintext logs.

## Scheduling and host services

I/O waits occur off the UI/model thread. Bound queued bytes/messages, connections,
per-peer/category rates, decode work and per-service drain work. A reliable queue
that cannot progress has an explicit timeout/disconnect policy; it cannot block
unrelated participants indefinitely. Wake after publishing a durable result;
owner-thread delivery checks both connection generation and activation lifetime.
CompletionQueue is a notification path, not the only copy of a received command.

AppHost pumps scoped services independently of local app updates, fixed simulation
and presentation. Network receipt, heartbeats, authority processing and
resynchronization continue during Settings, focus loss or local presentation
pause. Cancel held local input and send neutral intent as needed; opening Settings
cannot pause another participant's world. Offline app simulation can pause while
its service scopes remain live. See [runtime service contracts](RUNTIME.md#service-processing).
Bounded network draining is independent of SDL event polling; it does not impose
an event-count cap on the SDL loop.

A headless AuthorityRunner owns the same domain model and protocol without
IApp/AppContext, SDL video, UI, font or GPU initialization. IApp is a desktop
adapter, not the authority interface. Its server clock never pauses on
window focus or silently applies the interactive clock's dropped-time policy.
Bound catch-up; on sustained overload expose degraded service/admission failure
or terminate the session according to policy. Tests inject clocks and record
late ticks; a fixed step alone is not a determinism guarantee.

An embedded player host uses the same runner and queued loopback messages, not
synchronous privileged client calls. A managed deployment runs the dedicated
artifact with external configuration, identity, monitoring and lifecycle services.
LAN/direct addressing, discovery, internet signaling, NAT traversal and relays are
independent capabilities. LAN is not automatically trusted; public internet use
requires authenticated peer/server identity, not merely encrypted packets.

## Transport choice

The transport interface has a deterministic in-memory fault adapter and a native
[GameNetworkingSockets](https://github.com/ValveSoftware/GameNetworkingSockets)
adapter for reliable/unreliable messages, encryption and statistics. Steam
identity, signaling and relay services are separate provider adapters, not implied
by the public transport library. Do not implement custom cryptography or reliable
UDP. Library-native handles and provider credentials remain behind the adapter.

Delivery capabilities are explicit. An HTTP/browser adapter must report missing
modes instead of silently substituting reliable delivery for an unreliable
real-time channel. Protocol schemas own bounded codec validation; UI JSON is not
the network wire format. Codecs must support deterministic round-trip fixtures
and reject malformed or oversized input before domain publication.

## Verification

Network peers are untrusted even before creator-app isolation exists. Protocol
validation, authentication, rate limits and bounded native decode work belong to
every internet-capable session, independent of the future executable-app sandbox.

Use fake time and an in-memory fault transport for delay, loss, duplication,
reordering, disconnect, queue pressure and reconnect tests. Verify command
idempotency, revision conflicts, stale-session rejection and per-player filtering.
Run two clients against both a shared-board model and a moving-space model.
Exercise embedded, separate local server and deployable server configurations;
managed identity/lobby/relay integration has separate service tests.

Measure RTT/jitter, useful bytes by category, backlog age, snapshot age, decode
cost and authority tick tails. Test settings/focus/minimize without losing session
liveness. Prediction, CRDTs, voice chat, matchmaking, commerce and production
service operations are separate from the transport/session contract.
