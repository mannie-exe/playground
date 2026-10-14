# Native Minecraft spectator application

`playground_minecraft` is a Playground-native Java Edition client. It connects to
one configured server, renders the received world and lets the player look and
move in server-granted spectator mode. It does not embed a JVM client. Inventory,
crafting, chat UI, block interaction and world editing are outside this application.
Launch, credential entry, connection progress and recoverable errors need minimal
presentation; they do not require a general launcher or editor framework.

## Compatibility profile

The initial target is Java Edition **26.3, protocol 777, data version 5023**.
The app owns `MinecraftCompatibilityProfile`: game version, protocol number,
packet/registry schema digest, resource definition digest and supported features.
A fixture additionally records dimension registries, configuration and trace schema.
Never infer packet compatibility from a release name alone or silently use an older
codec. Unknown required configuration, registries or packets fail compatibility
with a diagnostic; explicitly ignorable optional packets have a declared policy.

The local Moxy 26.3 input's `version.json` identifies resource pack version 97.1
and data pack version 121.0. The inspected input is the merged artifact
`net.minecraft:minecraft-merged-123345746e:26.3`; Moxy's input lock records content
SHA-256 `584d0b94ff10c54744d41fbb5ef2637937b387224c6c7bae2cbbec3891489032`
using `zip-sorted-name-and-entry-sha256-v1`. Read-only inspection of its classes confirms
movement and correction payloads below. This is shape evidence, not a completed
codec or numeric packet-ID table. At minecraft-data revision
`201ca802a1b2c968b02e7fa5b31dd74e8bff3d8b`, the
[protocol version registry](https://github.com/PrismarineJS/minecraft-data/blob/201ca802a1b2c968b02e7fa5b31dd74e8bff3d8b/data/pc/common/protocolVersions.json)
lists 26.3 but the
[data-path registry](https://github.com/PrismarineJS/minecraft-data/blob/201ca802a1b2c968b02e7fa5b31dd74e8bff3d8b/data/dataPaths.json)
has no 26.3 packet-schema entry. Implementation acceptance requires a complete
pinned codec and controlled-server traces for this exact version. No packet IDs
or binary field encodings are guessed here.

## App state and service wiring

| Value | Shape and owner |
|---|---|
| `MinecraftLaunch` | Server authority/port, compatibility profile, local resource root and viewer budgets; no credential |
| `MinecraftCredentials` | In-memory Minecraft Services access token and validated profile identity; instance-scoped, redacted and never persisted |
| `MinecraftConnectionState` | Disconnected, Authenticating, Resolving, Connecting, Login, Configuring, Playing, Closing or Failed; transition reason and connection epoch |
| `MinecraftPlayerState` | Connection/dimension epoch, server entity identity, mode/abilities, world position, velocity, yaw/pitch, correction sequence and last acknowledged teleport |
| `MinecraftChunkKey` | Connection epoch, dimension generation, signed chunk X/Z and signed section Y where applicable |
| `MinecraftChunkStamp` | Chunk key, monotonically advancing chunk lifetime generation and data revision; unload/reload cannot reuse old publication identity |
| `MinecraftViewState` | Eye transform, render origin, input session, visible readiness and app-owned diagnostic/launch presentation |

Construct scopes and validate resources/capabilities before connection admission.
Use the [socket and HTTPS services](../platform/NETWORKING.md) with instance lifetime,
not presentation lifetime. TCP login/configuration and stream encryption are
application protocol state. HTTPS profile/session requests implement the version's
online authentication procedure. A pasted token is a Minecraft Services token,
not an arbitrary Microsoft OAuth token. The reference client's
[authentication API](https://github.com/PrismarineJS/node-minecraft-protocol/blob/master/docs/API.md)
and [session-join integration](https://github.com/PrismarineJS/node-minecraft-protocol/blob/master/src/client/encrypt.js)
illustrate the separate HTTP and socket responsibilities; they are not the 26.3
codec specification. Online session join authenticates the player according to that
protocol; it is not a general certificate-based identity for the configured TCP
server. The token is sent only to the declared verified HTTPS service, never the
game server or a server-supplied URL. Expiry or rejection returns to credential
entry. Offline test servers require explicit test configuration, never automatic
authentication fallback.

Start Disconnected. Credential/profile validation enters Authenticating, followed
by Resolving and Connecting; a connected stream enters Login. The session-join HTTP
request during the login exchange is a retained Login suboperation. Successful
configuration enters Playing; a server reconfiguration returns to Configuring with
publication suspended. Graceful closure retires through Closing to Disconnected.
Failures retain their reason until explicit retry or dismissal; retries use fresh
request/connection generations.

The protocol owner serializes framing, encryption and compression transitions at
exact packet boundaries. Bound packet lengths, decompressed bytes/ratios, NBT depth,
array counts, palette indexes, registries and cumulative staged bytes. Workers may
prepare validated immutable chunk data, but cannot reorder protocol state changes.
TCP fragments and coalesced packets follow the same parser path as recorded input.

Service pumping keeps keepalives, configuration replies, teleport acknowledgements
and chunk-batch feedback live while local presentation is paused. Reserve bounded
outgoing control capacity; coalesce movement only before serialization. There is
no out-of-order control lane inside the server's TCP stream. Receive backpressure
can delay control packets behind bulk data; sustained overload has a disconnect
policy, and feedback reduces admitted chunk load where the protocol allows it.

Disconnect closes admission, invalidates connection/dimension epochs, cancels
preparation and retires socket, product and frame leases. Reconfiguration temporarily
stops incompatible world publication. Respawn/dimension changes replace the relevant
registry/world generation and reset pose history. Reconnect creates a new connection
epoch, even for the same server address and account.

## Mouse look and player movement

Spectator traversal is required application behavior, independent of future Jolt
character motors. The server must grant spectator mode and appropriate abilities;
otherwise report an unsupported mode and do not invent flight or noclip permissions.
Reuse `ViewControlSession`, `LookController`, `MovementController` and the existing
free-camera intent conventions. The app's player pose remains separate from the
camera eye pose; camera movement alone does not update a network player.

| Input or value | Contract |
|---|---|
| Mouse look | Relative pointer displacement converted once to radians using sensitivity; yaw around world up and pitch clamped short of the poles |
| Movement | WASD in the horizontal heading plane, Space ascend, Shift descend; combine and normalize intent before applying app speed |
| Speed | Configurable multiplier constrained by server abilities and the compatibility profile; never claim vanilla speed parity without traces |
| Input ownership | Engage only on the active focused viewport; UI/IME gets first claim; release on Escape, detach or interruption |
| Local advancement | Held intent integrates on the app's fixed movement clock; mouse displacement is consumed once, not once per catch-up tick |
| Network cadence | Version-profile movement schedule, nominal 20 Hz; position/rotation/status selection and unchanged-state refresh verified against the server |
| World/camera mapping | Player feet/world coordinates use double precision; camera applies the profile's eye offset before render-origin conversion |

Core look values use radians. The app codec converts to wire yaw/pitch in degrees,
including Minecraft's axis/sign convention; golden tests map all cardinal headings,
vertical look and inverse conversion. Map one block unit to one metre in the viewer
world and keep the conversion explicit. Render-origin rebasing must never alter
wire positions. Profile conversion is explicit rather than reusing a local
CameraProps position as a network pose.

Focus loss, Settings, UI capture or presentation suspension cancels relative deltas
and held intent, releases pointer capture and requires neutral input before rearming.
The last valid player pose and required protocol replies continue independently of
presentation. No catch-up movement is applied when the view returns. Server
ability/mode changes immediately revise movement admission; loss of spectator permission stops local
traversal and reports the unsupported mode. Movement flags reflect actual app state,
not invented grounded contact from an absent character solver.

The inspected 26.3 classes expose these payload shapes. They belong to this app,
not the generic networking or camera API:

| Packet shape | Fields and handling |
|---|---|
| `ServerboundMovePlayerPacket` variants | Optional float64 X/Y/Z and float32 yaw/pitch, onGround and horizontalCollision; variant determines presence |
| `ClientboundPlayerPositionPacket` | Teleport ID, PositionMoveRotation and a set of relative flags |
| `PositionMoveRotation` | Position vec3, deltaMovement vec3, float32 yaw/pitch; relative flags determine how each component resolves |
| `ServerboundAcceptTeleportationPacket` | Teleport ID **plus float64 X/Y/Z and float32 yaw/pitch** in the inspected 26.3 class |
| `ServerboundChunkBatchReceivedPacket` | float32 desiredChunksPerTick, derived from bounded decode/publication capacity |

Apply server corrections on the player owner in protocol order, resolving the
version's relative position, velocity and rotation flags against the required
prior state. Publish a discontinuity, discard superseded unsent movement and reset
camera interpolation. Acknowledge the resolved correction with the exact versioned
payload before subsequent dependent movement. Already transmitted poses cannot be
retracted. Teleport/correction logic is app-level reconciliation, not a promise of
engine-wide rollback or predicted physical characters. Emit no interaction packets.

## Chunk and resource pipeline

Decoded section palettes, biomes, lighting, height maps and block entities are
app-owned values. Minecraft block IDs are not generic scalar intensity or material
IDs. Map protocol coordinates into WorldPosition and independently resident products;
there is no entity per voxel and no requirement to change the server's chunk layout.
Server arrivals/updates/unloads feed an app-owned live directory with revision-bound
queries and explicit coverage. They do not rebuild a finite WorldManifest on every
packet or invoke a local terrain generator for unavailable server data.

`frame -> validated chunk revision -> immutable section data -> mesh/material jobs
-> owner publication -> device realization` is the preparation path. Product keys
include resource definitions, source revision and required neighbor/light revisions.
Block updates invalidate affected products even when the section remains resident.
Unloads invalidate pending publication; renderer leases retire after frame completion.
Missing neighbors, unavailable chunks and known air remain distinguishable. Bound
wire buffers, decoded source, products, textures, upload work and device residency
independently. Fast traversal prioritizes visible work without losing protocol order.

Derived products may be evicted and rebuilt. A received chunk base must remain
recoverable while the server can send deltas against it: retain decoded state or a
revisioned connection-local backing store that preserves ordered updates. Eviction
alone is not a server unload. Negotiate bounded view distance and apply chunk-batch
feedback; if the admitted source working set cannot fit, fail/reduce the connection
workload explicitly. Never drop a base, interpret the next delta against air, or
assume the server will resend it on demand. A local cache is not a Minecraft save
format and does not authorize offline edits or transmission back to the server.

The server sends state, not finished renderable meshes. Load compatible local
blockstate/model/texture definitions from a user-supplied installation or resource
pack. Remote pack requests require an explicit acquisition policy, digest verification,
limits and supported format, or fail the required-pack handshake. They never execute
code or silently fetch during AssetReader access. Upstream game art/code is not
vendored here; [distribution terms](https://www.minecraft.net/en-us/usage-guidelines)
are separate from wire compatibility.

| Visible coverage | Application responsibility |
|---|---|
| Terrain | Cubic and non-cubic models, blockstate variants/multipart composition, face culling and updates |
| Materials | Texture selection/animation, tint, lighting, cutout/translucency and visible fluids without fluid simulation |
| Other world content | Entity/block-entity presentation, sky, weather and supported effects with explicit capability/coverage records |
| Comparison | Fixed camera/time/weather/resource profile with missing visual classes listed, rather than claiming full parity from terrain alone |

Complete rendered-world coverage is the application goal. Staged terrain acceptance
is useful but is not full Minecraft visual parity. Missing generic renderer features
are evaluated at their rendering contract; they do not become core requirements just
because vanilla uses them. Audio, world interaction and general game UI are excluded.
Moxy runs inside the JVM client; compare it on the same server and camera trace,
without attempting to load its mod into this native executable.

## Acceptance stages

1. Replay versioned synthetic/captured fixtures through bounded decode, chunk updates,
   meshing and rendering. Record no tokens, authentication packets or private account data.
2. Complete real online login/configuration on a controlled 26.3 server and render
   the first admitted chunks from matching resource definitions.
3. Exercise mouse look and all movement axes, server corrections, keepalives, batch
   feedback, unload/reload, dimension changes and disconnect during preparation.
4. Traverse faster than preparation, suspend presentation and exhaust each budget.
   Verify bounded residency, visible missing coverage and continued control service.
5. Compare visual coverage captures and report supported classes and gaps explicitly.

Tests include split/coalesced frames, malformed compressed/NBT data, late generations,
relative correction flags, non-spectator refusal, focus loss while moving and input
at varied frame rates. Native server evidence is separate from decoder fixtures.
