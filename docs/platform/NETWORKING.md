# Networking and shared sessions

Networking consists of optional sockets, HTTP, message transport and session
modules. The runtime core supplies scopes, scheduling, bounded handoff and publication;
it does not depend on
sockets, a game protocol or replicated worlds. Collaborative tools and real-time
applications use the same networking primitives.

## Module boundary

| Module | Required implementation when enabled | Does not require |
|---|---|---|
| Sockets | Standalone Asio adapter for TCP streams, UDP datagrams and bounded asynchronous address resolution | Message framing, reliability over UDP, session authority, World or UI |
| HTTP | libcurl multi adapter for bounded HTTPS requests and explicit content acquisition | Hidden AssetReader downloads or an application login policy |
| Message transport | GameNetworkingSockets adapter, bounded reliable/unreliable messages, capabilities and native loopback tests | Raw TCP compatibility, session authority, World or physics |
| Sessions | Version negotiation, authority/replica lifecycle, ordered commands/receipts, filtered baselines/deltas and a small reference domain | Game rules, spatial interest, production identity/hosting services |
| Domain adapters | App-defined codecs, validation, visibility and state transitions | A universal scene or ECS schema |

The session module is an optional higher-level library, not future work. Its
baseline reference domain is a revisioned shared document with transient presence.
A real-time value stream exercises sequencing and bounded interpolation separately.
Spatial world/voxel/body replication sections below are future adapter designs;
they do not gate baseline transport, sessions, storage or Jolt acceptance.

The APIs below retain existing request, revision, snapshot and service conventions.
Applications select modules without changing host, rendering or persistence
semantics. The [Minecraft client](../apps/MINECRAFT.md) uses sockets and HTTP
directly. Its protocol is not a generic authority-session adapter.

## Socket values and operations

These are intended public shapes, not claims of existing headers. Operations reuse
runtime request identity, scoped lifetime, retained results and accounting. Calls
enter through the owning service thread; other threads submit owned requests
through bounded handoff. Native callbacks never call application code.

| Value/API | Shape and contract |
|---|---|
| `NetworkScope` | Explicit ServiceScope (instance or headless owner) plus limits, capabilities and epoch; owns operations and handles |
| `Endpoint` | IPv4/IPv6 address, IPv6 scope when needed, uint16 port; host names remain separate resolution inputs |
| `ResolveRequest` | Host, port, permitted address families, deadline and maximum results; preserves original authority for identity checks |
| `StreamHandle / DatagramHandle / ListenerHandle` | Distinct opaque IDs with scope epoch and generation; never native descriptors |
| `NetworkCapabilities` | Independently available TCP, UDP, IPv4/IPv6, HTTPS and message-delivery modes; missing modes return Unsupported, never implicit substitution |
| `SocketLimits` | Connections, pending accepts/operations, resolve results, receive/send bytes per handle and scope, datagram size, deadlines and owner drain budget |
| `resolve(request)` | Bounded asynchronous name resolution; returns validated endpoints or terminal failure, without opening a connection |
| `listen / accept / connect` | Explicit endpoint and policy; retained result yields a typed handle or failure; connect never implicitly listens |
| `readSome(stream, maximumBytes, deadline)` | One outstanding read per stream; owned bytes plus terminal status; bytes precede EOF/error when both occur |
| `writeAll(stream, ownedBytes, deadline)` | Serialized per stream; retained result includes bytes transferred and terminal status; failure/cancel may follow partial progress |
| `shutdownWrite(stream, deadline) / close(handle, mode, deadline)` | Half-close inserts a write-queue barrier and rejects later writes; full close rejects all new operations, with Abort or DrainThenAbort policy |
| `openDatagram(bindEndpoint, optionalPeer)` | Creates a bounded DatagramHandle; peer filtering is explicit and does not create reliable delivery |
| `receiveDatagram(handle, maximumBytes, deadline)` | Owned payload, source endpoint and status; preserves empty datagrams and reports truncation as invalid data |
| `sendDatagram(handle, endpoint, ownedBytes, deadline)` | One datagram within the configured limit; no implicit splitting, retry or reliability |
| `poll(request) / forget(request) / cancel(request)` | Retained terminal result until forgotten or scope retirement; cancellation does not prove that no bytes were sent |

Admission returns Accepted(request), Backpressured, Closed, InvalidArgument,
TooLarge or Unsupported. Rejected operations take no work and retain no payload.
Owned payload leases remain valid for the caller; acceptance retains a service
lease through native completion.
An accepted operation terminates exactly once as Complete, EndOfStream, Cancelled,
TimedOut or Failed, with a typed diagnostic and actual progress. Poll observes
without consuming; forget releases a terminal result, while early forget cancels
observation and retains native
leases until retirement. EndOfStream applies only to stream reads. Admission reserves
result capacity; notification loss cannot lose the result. Closing the scope ends
consumer access but retains native operation memory until callbacks retire.

TCP is an ordered byte stream. Reads may split or combine application messages;
write completion means local transport progress, not peer receipt or model commit.
A failed partial write cannot be retried as a whole message on the same stream.
An incomplete writeAll after any transferred bytes closes that stream and fails
queued writes; no later frame may follow a truncated frame. Canceling a queued,
never-dispatched write removes only that request. Canceling an active native write
closes the stream conservatively even if no transferred bytes have yet been reported;
cancellation can race with progress. Higher-level protocols may still require
closing when omitting an unsent operation would violate their ordering.
The application frames and bounds messages. Graceful close has an explicit drain
deadline followed by abort; it cannot indefinitely delay app replacement.

UDP preserves datagram boundaries but permits loss, duplication and reordering.
Receive truncation discards the entire datagram before protocol decoding. Payload
limits account for the selected path/MTU policy; IP fragmentation is not a delivery
guarantee. A connected UDP socket filters peers but does not authenticate them.
TCP fallback, reliable UDP, discovery and NAT traversal are separate policies.

For a requested payload cap N, reserve a checked N+1 receive buffer and reject a
completion longer than N or a native truncation indication. This detects oversized
datagrams even on platforms that silently truncate to the supplied buffer length.
Validate N against the supported UDP size and allocation limits before adding one.
An empty datagram remains a successful zero-length message. Test N-1, N, N+1 and
much larger payloads on each supported OS; an Asio error code alone is insufficient
cross-platform truncation evidence.

## HTTP and credential boundary

`HttpService::request / poll / cancel / forget` uses the same admission, retained
result and retirement rules as socket operations. `HttpLimits` caps concurrent
requests, connections, header/body bytes, queued consumer bytes and redirect count.
`HttpRequest` carries method, HTTPS URL, bounded headers, owned body or bounded
producer, deadline, redirect policy and response byte limits. Its retained result
separates transport/TLS failure from HTTP status and application validation.
Responses use either a capped owned body or an explicit bounded consumer. A slow
consumer pauses acquisition or cancels it; buffering cannot grow without bound.
HTTP shares NetworkScope admission and retirement, not TCP framing APIs.

Peer certificate and hostname verification are mandatory for HTTPS. Redirects are
disabled unless explicitly allowed; every destination is revalidated and credentials
are never forwarded across origins. Requests with effects are not automatically
retried. Credentials are transient secret inputs, excluded from logs, manifests,
command-line launch properties and trace captures. HTTP success does not authenticate
a raw socket peer or authorize application commands.

Use [libcurl multi](https://curl.se/libcurl/c/libcurl-multi.html), its bounded
[response callback](https://curl.se/libcurl/c/CURLOPT_WRITEFUNCTION.html), and
[hostname verification](https://curl.se/libcurl/c/CURLOPT_SSL_VERIFYHOST.html).
The content-acquisition owner verifies digests before publishing immutable assets;
AssetReader remains local and side-effect free. Minecraft online login is an app
adapter over HTTPS plus its own versioned socket handshake, not a core account service.

The libcurl write callback either consumes the whole callback span or consumes
none and pauses/aborts. Paused data is delivered again: do not append a prefix and
return pause. Reserve a consumer window sufficient for the supported callback size;
unpause only on the I/O owner, with reentrant callback delivery accounted for.
Header and decoded-body limits are independent and checked with overflow-safe sums.
Automatic decompression must enforce the decoded limit. Native paused-transfer
buffers are separately estimated; application queue limits do not bound them.
The baseline bounded acquisition profile uses HTTP/1.1; enabling multiplexed HTTP
requires additional paused-stream memory evidence. Expired/blocked consumers cancel
instead of retaining a transfer indefinitely.
[Pause and replay semantics](https://curl.se/libcurl/c/curl_easy_pause.html).

## Native I/O execution

Standalone Asio owns an I/O execution domain shared by admitted sockets, with
serialized operations per handle. It does not occupy independent-job Executor
workers with blocking event loops. DNS, timers and completions run there; decoding
may use workers after the protocol owner validates and orders complete frames.
An OS resolver that cannot promptly cancel still owns memory until completion;
logical cancellation suppresses publication and bounded resolver admission prevents
replacement requests from accumulating unbounded native work.

Asio [asynchronous operations](https://think-async.com/Asio/asio-1.36.0/doc/asio/overview/core/async.html)
require explicit object/buffer lifetime. In particular,
[partial writes](https://think-async.com/Asio/asio-1.36.0/doc/asio/reference/basic_stream_socket/async_write_some.html)
require retaining the original buffer and tracking transferred prefixes.
Handlers stage owned results, then wake ServicePump. Owner delivery checks scope,
handle generation and request identity. ServiceWorkBudget bounds publication, not
arbitrary native callback duration. Native buffers and in-flight work also have
admission limits and ResourceLedger charges.

Shutdown stops admission, invalidates publication, requests cancellation/closes
handles, drains completions and releases their leases before destroying the I/O
provider. A shutdown deadline may report failure and retain a retiring provider;
it never frees memory still referenced by a native operation. The host owns shared
provider lifetime across instance replacement. No detached callback can access an
expired AppContext, model or UI tree.

## Meaning of multiplayer

| Workload | Shared state and policy |
|---|---|
| Real-time game | Authority ticks, player intents, snapshots, interpolation; prediction only where required |
| Turn/event-based game | Validated commands, phase/revision, private player views and reconnect |
| Live collaboration | Revisioned document/domain operations, permissions, presence and conflict responses |
| Hybrid experience | Reliable domain events plus transient presence or real-time state |

An authoritative session model serves all four. Revisioned operations
are not a CRDT. Offline concurrent edits, peer authority, deterministic lockstep,
rollback and host migration remain [future capabilities](#future-session-capabilities).
Camera, UI and local renderer state are presentation unless the app explicitly
models a shared camera or document. Do not replicate Node trees or pointers.

## Layers and API

| Layer | Values/operations | Ownership |
|---|---|---|
| Message transport | ConnectionHandle, TransportCapabilities, send, receive, close, stats | I/O service owns messages; separate from StreamHandle and DatagramHandle |
| Protocol | ProtocolVersion, MessageType, bounded encode/decode, channel policy | No UI or renderer dependency |
| Session | SessionId, ParticipantId, authenticate/join/leave/resume, SessionEpoch | Authority owns admission and participant visibility |
| Domain | CommandId, expected revision/tick, validate/apply, public/private views | App model owns rules and durable state |
| Client presentation | Snapshot buffer, interpolation, command results, presence | Owner thread projects accepted state into UI/scenes |
| Hosting | AuthorityRunner, embedded or dedicated; deployment configuration | Host owns service lifetime and server pacing |

Transport connection IDs, authenticated account/provider IDs, session participants
and domain entity IDs are separate. Connection generations reject stale I/O;
SessionEpoch rejects state from a replaced authority. Authentication establishes
identity, authorization checks each operation, encryption protects transport;
none substitutes for the others. A player host can inspect its authoritative
model; player-hosted private information cannot be protected from that host.

`MessageTransport::send` reports Accepted, Backpressured, Closed, TooLarge or UnsupportedDelivery.
Accepted means locally queued, not delivered or applied. Application command
results acknowledge validation/application separately. Message payload ownership
is explicit; callbacks cannot retain borrowed I/O memory past delivery.

## Authority and replica state

| Value/API | Shape and contract |
|---|---|
| `AuthoritySession` | Session/epoch, participants, permissions, ordered clock, domain owner and bounded command/replication queues |
| `AuthoritySession::enqueue / advance / close` | Validate queued commands at model boundaries; no direct privileged client mutation |
| `CommandEnvelope` | Protocol/session epoch, authenticated issuer, command sequence/ID, expected domain revisions/tick and typed bounded payload |
| `CommandReceipt` | Command identity, validation outcome, applied commit/tick, result payload and separate durability acknowledgement |
| `ReplicationBaseline` | Baseline/interest identity, captured domain revision, filtered records, coverage and exact content/schema compatibility |
| `ReplicationDelta` | Baseline/interest identity, sequence, source/target revisions, tick and bounded updates |
| `ClientReplica` | Session epoch, applied baseline/revisions, retained interpolation state, missing dependencies and resync status |
| `ClientReplica::accept / snapshot / reset` | Validate/decode/stage owned messages, publish coherent state and reject obsolete epochs |
| `ReplicaState` | Disconnected, Joining, ReceivingBaseline, Live, NeedsResync or Closed, with independent content/visual readiness |
| `AuthorityLimits` | Participants, commands/bytes per issuer and total, dedup history, baseline/delta storage, decode work and tick backlog |

The authority clock and mutation boundary are shared by local and remote commands.
An embedded client uses queued loopback messages and the same validation/receipts;
it cannot directly call a privileged model mutation method. Authority-side domain
code can emit internal commands with explicitly distinct authority provenance.
Session, connection, world, dataset and body generations remain separate guards.

Validation distinguishes invalid/denied/conflicting commands from transient queue
pressure. Transport acceptance is not authority admission, commit or durability.
Command sequence gaps/expired dedup history trigger explicit refusal/resync rather
than reapplying an old mutation. Domain receipts retain the exact terminal outcome.
Baseline capture and subsequent deltas share a revision barrier; bounded staging
publishes one replica root only when required parts/dependencies validate. Content
waiting is visible independently from connected session/heartbeat progress.

The initial replica protocol transfers `BaselineBegin(id, interest, revision,
partCount, totalBytes)`, numbered bounded parts, and `BaselineEnd(digest)`. Parts
are keyed by session epoch and baseline ID; duplicates must match exact bytes.
Stage deltas after the captured revision in a separately bounded ordered queue.
Publish the baseline only after all parts and its digest validate, then apply
contiguous source/target revisions. Interest changes allocate a new baseline ID;
late parts cannot populate its staging root. Missing parts, gaps, conflicting
duplicates or expired history enter NeedsResync and discard incomplete staging.
Admission/timeout bounds apply even while a reliable transport remains connected.

CP10 owns a version-1 envelope codec with explicit widths, endian, string/collection
limits and golden byte fixtures before native session acceptance. The domain codec
is registered separately. The reference document has namespaced record IDs,
expected revisions and authority-assigned commit sequences; presence expires and
never changes the document revision. Tests deliver baseline parts and deltas across
independent transport lanes to verify that wire delivery order is not the commit
barrier. This protocol is not used by the Minecraft target.

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
unrelated participants indefinitely. Wake after retaining an owned result;
owner-thread delivery checks both connection generation and instance lifetime.
CompletionQueue is a notification path, not the only copy of a received command.

AppHost pumps instance-scoped services independently of local app updates, fixed simulation
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
independent capabilities. LAN is not automatically trusted; session deployments
declare authenticated peer/server identity policy, not merely encrypted packets. External protocol clients declare their own server-trust model
and cannot claim identities their protocol does not establish.

## Spatial authority and interest

**Future adapter.** These rules apply when a domain exposes shared spatial state.

Spatial sessions use [WorldId, WorldEpoch, EntityId and SpaceId](WORLDS.md), not
scene handles, render origins or client cell residency. SessionEpoch and WorldEpoch
are independent. The authority validates tick/revision, controlled-entity identity,
movement/edit permissions, bounds and rate limits before accepting commands.
Clients send intent and explicit domain commands, not trusted poses or generated
geometry. Authority-side simulation/data readiness is independent of client views.

| API | Contract |
|---|---|
| `InterestSource` | Authorized observer/region and bounded requested extent; server owns effective scope |
| `ReplicationView` | Per-participant filtered entities/fields, baseline revision and interest generation |
| `SpatialBaseline` | Complete admitted interest state with content/generator compatibility and world epoch |
| `SpatialDelta` | Sequenced updates against an identified baseline and interest generation |
| `EntityPresence` | Enter, LeaveInterest or Destroyed; leaving interest is not domain deletion |

Interest combines authority policy, space, domain relevance and spatial proximity.
Permission filtering precedes encoding; proximity is not authorization. Reuse
spatial-query services without requiring identical physics, rendering and network
partitions. Bound observer counts, radii, entity counts, queued baselines and update
work. Apply hysteresis and explicit priority to avoid boundary churn. Reject or
negotiate impossible interest rather than silently omitting required baseline data.

Interest changes establish a new revision/generation barrier. Receivers reject
old-generation updates that would resurrect an entity after LeaveInterest/Destroyed.
Missing baselines request resynchronization; no dependence on cross-channel arrival
order. Reentry supplies sufficient current state, not stale client cache assumptions.
Readiness and acknowledgements distinguish received, applied and durable effects.

Wire poses identify their space and tick/discontinuity. Encode validated float64
world coordinates or a negotiated bounded cell/local quantization with explicit
error, range and overflow rules. Never serialize raw C++ layouts or assume peer
render origins agree. Moving-frame deltas declare parent identity/revision; missing
parents trigger resync or an explicit absolute-pose fallback. Transfers publish
one authoritative source-to-destination change and reset interpolation history.

The authority determines voxel edits and generated entity state. Peers negotiate
exact generator/configuration/content compatibility; procedural generation is not
permission to invent authoritative results. Compatible clients can regenerate base
data and receive edits; incompatible clients need supported authoritative products
or fail admission. Bounded content transfer remains separate from movement traffic.
Interpolation buffers are bounded; stale observations freeze/report age rather than
silently extrapolate without limit. A local client waiting for visual assets does
not stop authority ticks or make missing collision coverage safe.

## Dataset authority adapters

**Future adapter.** Local DatasetStore/EditService do not require a session.

`DatasetBaseline` carries dataset/schema/source identities, epoch, root revision,
admitted coverage and exact required profile/content references. `DatasetDelta`
identifies its baseline, command/commit sequence and bounded typed RegionPatches or
verified payload references. Encode validated values, never raw C++ storage.
Authority validates permissions, sizes, address bounds and expected revisions before
using the same EditService as local tools. Dedup watermarks/receipts survive retries;
received, committed and durable acknowledgements are distinct.

Missing baselines, sequence gaps or incompatible interpretation trigger explicit
resynchronization/refusal. Interest exit releases demand but does not delete saved
samples/overrides. Compatible reproducible sources can regenerate baselines;
otherwise transmit bounded authoritative data through authorized content acquisition.
Clients do not submit trusted collision meshes or navigation readiness claims.
Live visualization streams may negotiate Latest samples; authoritative edits and
simulation retain ordered semantics. Clock domains and sample age remain visible.

In-memory fault transport adapters verify these domain contracts before native
transport integration. They do not establish authenticated internet support.
See [spatial tests](SPATIAL_TESTING.md) and [simulation](SIMULATION.md).

## Physics authority and presentation

**Future adapter.** Baseline physics exposes results without imposing replication.

The authority runs [Jolt physics](PHYSICS.md) and publishes completed valid body
states with tick, entity identity, discontinuity and active collision/dataset
revisions. Clients send permitted intent/commands, not authoritative poses, contacts
or native shape objects. A Compromised physics region does not advance its published
authoritative tick; fault/recovery state is observable to replicas.

Client presentation interpolates bounded snapshots without requiring a client solver.
Do not extrapolate indefinitely when snapshots stop. Teleport/recovery clears the
relevant interpolation history. Missing local visual products can retain explicit
placeholders; they do not pause authority or turn incomplete collision coverage into
safe local movement. An edit commit and a physics snapshot identify the required
geometry revision even when separate delivery channels arrive out of order.
Clients retain/reject bounded pending dependencies or request a baseline; they do
not reinterpret a pose against whatever geometry happened to arrive first.

Replicate Playground identities, domain values and versions. Jolt BodyID, pointers,
internal contact caches and opaque SaveState bytes are not the network protocol or
portable save format. Solver save/restore does not restore application-side body
lifecycle and all domain changes automatically. Prediction, rewind and deterministic
lockstep require their own history/state contracts and remain outside baseline
server-authoritative interpolation.

## Future session capabilities

Spatial interest and voxel/body replication, client prediction/reconciliation,
rollback, deterministic lockstep, distributed/peer authority, host migration and
offline CRDT merging are future. General multiplayer frameworks, discovery,
matchmaking, relay provisioning and managed hosting are future applications/services.
The Minecraft spectator client is required application scope and uses its own TCP
protocol; this future label does not defer it. Baseline sessions require reconnect,
revision conflicts, receipt deduplication, filtered baselines and bounded replica publication for the reference domain.

A network connection does not define the application's collaboration model. The
reference authority serializes accepted changes; it does not claim offline merge
or exactly-once external side effects. Persistence of a committed command/result is
an explicit domain/storage integration, independent of transport reliability.

## Message transport choice

The transport interface has a deterministic in-memory fault adapter and a native
[GameNetworkingSockets](https://github.com/ValveSoftware/GameNetworkingSockets)
adapter for reliable/unreliable messages, encryption and statistics. Steam
identity, signaling and relay services are separate provider adapters, not implied
by the public transport library. Do not implement custom cryptography or reliable
UDP. Library-native handles and provider credentials remain behind the adapter.

Delivery capabilities are explicit. A future browser message adapter reports missing
modes instead of silently substituting reliable delivery for an unreliable
real-time channel. Protocol schemas own bounded codec validation; UI JSON is not
the network wire format. Codecs must support deterministic round-trip fixtures
and reject malformed or oversized input before domain publication.

## Bounded native integration

Pin Asio, libcurl and GameNetworkingSockets versions and build options independently
in the dependency module. Report enabled capabilities separately. Minecraft's
application adapter uses a pinned OpenSSL EVP provider for required protocol crypto,
including [AES modes](https://docs.openssl.org/3.5/man7/EVP_CIPHER-AES/), instead of
implementing cryptographic primitives. This is distinct from HTTPS TLS and does
not impose OpenSSL on consumers that omit the application and relevant adapters.

GameNetworkingSockets native handles remain private. The adapter maps connect/listen/accept/close and receive
poll groups to generation-checked Playground connections. RunCallbacks dispatches
connection events; receive messages have explicit Release ownership. A designated
I/O/control service performs native dispatch and stages owned records for the model
owner. Native callback batches are not assumed preemptible by ServiceWorkBudget.
Bound admitted connections/native buffering, keep callbacks short and measure their
cost. [Native interface](https://github.com/ValveSoftware/GameNetworkingSockets/blob/master/include/steam/isteamnetworkingsockets.h).

The initial message adapter targets the v1.6.1 API. One process coordinator owns
library initialization/shutdown and native callback dispatch across hosts. Each
received `SteamNetworkingMessage_t` is released exactly once after copying into
admitted owned storage or completing a retained native lease, including rejection
and scope-close paths. Set native send-buffer/connection limits before traffic;
local service bounds alone do not constrain library queues. CP10 records the exact
source commit, crypto backend, flags and target in its dependency lock and acceptance
receipt. [Message ownership](https://github.com/ValveSoftware/GameNetworkingSockets/blob/v1.6.1/include/steam/steamnetworkingtypes.h).

Transport send acceptance, remote receipt and model commit remain distinct. Configure
reliable/unreliable delivery and per-lane priorities explicitly. Separate channels
provide no global order. Do not implement custom reliable UDP or cryptography.
Connection encryption does not select an account identity or authorization provider.

Use a deterministic fault adapter and separate-process loopback tests for transport.
The optional session module reuses the same protocol/domain fixture with queued
embedded authority, fault transport and native loopback. No private pool per
connection or unbounded receive-drain work is allowed. Shutdown closes admission,
retires callbacks/messages and then releases native runtime ownership.

Inject ephemeral endpoints, timeouts and storage roots. Readiness handshakes replace
sleep-based synchronization; tests terminate/reap owned child processes. Loopback
identity is test-only. Public message-session endpoints require an explicitly
supplied authenticated identity policy; missing providers report unavailable. Native loopback establishes
neither internet identity nor signaling/relay/deployment coverage.

## Verification

Socket tests split and coalesce TCP frames, produce partial writes, EOF with final
bytes, half-close, timeout, cancellation after a prefix, reconnect and stale callbacks.
UDP tests cover empty datagrams, truncation, size limits, source addresses, loss,
duplication and reordering. Exercise IPv4 and IPv6 with explicit unavailable-family
results. Use deterministic fault adapters and separate-process native loopback;
never infer native coverage from mocks. HTTP fixtures cover TLS/hostname rejection,
redirect credential isolation, body limits, stalled consumers and cancellation.
Shutdown under full queues must retain terminal results and native buffer leases.

Transport tests cover reliable delivery, unreliable loss/reordering, queue pressure,
connection failure, native message release, callback retirement and late generations.
Network peers remain untrusted independently of executable-app isolation. Decode
limits and authentication/authorization policy apply before domain publication.

Session tests use at least two clients editing a small shared document, plus transient
presence and an independently sequenced value stream. Test duplicate commands,
revision conflicts, reconnect, baseline replacement, permission-filtered fields,
expired dedup history and slow consumers. A command sequence gap rejects/resynchronizes
rather than silently skipping required work. Retained receipts have explicit bounds;
retired sequences are never accidentally executed again.

A baseline and subsequent deltas share a captured revision boundary. Missing history
requests a new baseline. Presentation pause must not interrupt connection progress;
model publication follows the domain owner's declared boundary. Use fake time and
controlled completion order, then native separate-process transport. These workloads
do not prescribe a user-facing collaborative or game application.


Measure RTT/jitter, useful bytes by category, backlog age, snapshot age, decode
cost and authority tick tails. Test settings/focus/minimize without losing session
liveness. Prediction, CRDTs, voice chat, matchmaking, commerce and production
service operations are separate from the transport/session contract.
