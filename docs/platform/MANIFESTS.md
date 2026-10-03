# Versions, manifests and generation

Manifests describe immutable content, compatibility and dependency resolution.
Host build-info.json, asset-pack manifests and executable-app manifests are
separate artifacts. Executable-app authoring/loading and its browser are
[future capabilities](DISTRIBUTED_APPS.md).

## Version domains

| Value | Meaning and compatibility rule |
|---|---|
| schema_version | Integer document-format version; explicit reader support/migration, never guessed from app version |
| package id + version | Stable identity and released content contract; immutable after publication |
| content digest | SHA-256 of exact generated runtime-manifest bytes, which enumerate payload hashes; exact identity for directories and archives |
| artifact digest | SHA-256 of the complete transport archive; compression/container changes can alter this without altering content identity |
| host API / logic ABI | Explicit supported interface range and required features; independent of host executable release |
| protocol version | Network message/session compatibility, negotiated separately from content and host |
| model/save version | Persistence schema and transactional migration policy |
| asset revision | Authored generation within catalogs; not a globally unique version or content hash |
| tool version | Manifest/cook implementation identity used for reproducibility and cache invalidation |
| world/cell schema | Space, identity, bounds, payload and save interpretation; explicit migrations |
| generator identity/version | Exact algorithm/build contract, seed encoding, configuration digest and output compatibility |
| derived product key | Source/neighbor revisions, profile/settings, cooker version and target encoding |

Use [SemVer](https://semver.org/spec/v2.0.0.html) for released app/pack
contracts: exported asset IDs, kinds and documented interpretation form the pack's
public interface. Removing/changing that interface is breaking; additions are
compatible only if existing consumers retain their behavior. A content correction
can be a patch, but changing gameplay-affecting data still requires multiplayer
content compatibility checks. Version labels alone never authorize mixing worlds.

Pre-release and major-zero versions are explicitly unstable. Build metadata does
not order releases and cannot select a newer artifact. Local unpublished bundles
may retain a development version while their digests change; a public registry
must reject replacing a published identity/version with different content.
No timestamp-based version bump or implicit `latest` dependency is required.
Dependencies use exact versions/digests. Range expressions and implicit latest
selection are not accepted by the resolver.

## Authored versus generated data

| Artifact | Owner and contents |
|---|---|
| Authoring definition | Human/editor/C++ registration input: identity/version, exports, preparation settings, dependency intent and provenance |
| Resolved lock | Explicit authoring resolution: exact package versions/content digests, source identities and selected artifact variants |
| Runtime manifest | Generated normalized definitions, resolved asset references, payload names/sizes/hashes, required features and producer/schema versions |
| Distribution index | Generated set of app/pack content and artifact digests plus target/compatibility metadata |
| Online listing (future) | Search/display metadata and references to verifiable immutable artifacts; never trusted executable instructions |

Do not maintain independent CMake, C++ and editor inventories of the same assets.
The writer consumes one validated definition model, then derives manifests,
payload lists and dependency output from it. Existing compiled registrations can
be an explicit exporter adapter; it must not scrape C++ headers or execute app
lifecycle callbacks. A declarative authoring manifest can populate the same model.
Unsupported procedural/native definitions fail export or require an explicit
cooker; never silently omit them. Catalog export enumerates immutable records
through an explicit definition adapter, independent of cache/native realizations.

Author the package release version once. Generate byte sizes/hashes from actual
cooked outputs rather than author-provided values. Keep source graph/lock and
runtime resource graph distinct: both are traceable, neither is guessed from a
filesystem scan at activation. Repeated generation with identical source, lock,
tool version and target must yield identical runtime manifests and pack bytes.
Do not embed absolute paths, wall-clock timestamps or machine secrets.

The manifest does not contain its own content digest. A lock/index outside the
package pins the expected digest; the manifest contains its members' hashes.
An archive digest is recorded outside the archive too. Verification checks the
external identity, then manifest and member bytes before publication. Checksums
alone do not establish publisher authenticity; authenticated indexes carry the
trust and update policy.

## World and generated content

World manifests declare stable world-definition/space/cell names, numerical bounds,
cell payload references, exact content locks and optional generator recipes. A
runtime WorldId identifies an instance/save rather than the reusable definition.
Definition-local space/entity names bind to world-qualified identities on creation;
loading the same definition into two worlds cannot alias their runtime state.
Registered definition schemas use inspectable JSON for world/cell records; large
products use explicit bounded binary encodings. Canonical field/record ordering,
finite doubles and lossless stable-ID/integer encoding are required. Large integer
identities/addresses use canonical decimal strings in JSON to avoid consumer
precision loss; wire/binary encodings use defined integer widths and byte order.

World/cell validation checks units, duplicate identities, frame/reference cycles,
checked address ranges, content closure and supported generator/profile schemas.
Cook outputs record dependencies on voxel neighbors, navigation profiles and tool
versions. Edit invalidation follows those dependencies rather than package names.
Generated temporary/runtime products need not become a new published pack.

Saves pin baseline content and generator identity separately from derived cache
versions. Content updates require explicit migration/compatibility before replacing
an active baseline; navigation/mesh caches can be regenerated without rewriting
authoritative edits. See [world persistence](WORLDS.md#state-activation-and-persistence)
and [generation contracts](PROCEDURAL.md).

## Local authoring, build, install and CPack

```text
C++ registrations or authored definitions + explicit lock
  -> validate and cook -> runtime manifest + payload inventory
  -> deterministic pack -> distribution index
  -> install staging -> CPack host release archive
```

Authoring can invoke the same pack writer directly, without a host application
build. CMake runs it as an explicit build dependency, after
shader/content outputs exist. Declare outputs/byproducts and input dependencies;
use generated dependency information for discovered model/font/media inputs.
Do not rewrite outputs when their contents are unchanged. Packaging commands are
idempotent and produce diagnostics identifying source asset, dependency and phase.
See [CMake custom commands](https://cmake.org/cmake/help/latest/command/add_custom_command.html).

Install and CPack consume already-generated packs and verify the expected index.
They fail clearly when outputs are absent or stale; they do not compile tools,
resolve new dependency versions or perform hidden network acquisition. This fits
[CPack's install-based packaging](https://cmake.org/cmake/help/latest/module/CPack.html)
and the build-before-package workflow. The installed host's
build-info.json remains separate from content/app manifests. A host release
archive can contain several asset packs; an asset pack does not contain another
host installer.

Install rules may invoke the built writer to verify/stage local
outputs, but generation semantics and inputs must match the authoring/build path.
Generation belongs to the build; install-time work only verifies/stages outputs.
Keep loose development content as an explicit mode; shipped pack failures must
not silently fall back to arbitrary developer paths. Native pack tools still obey
the repository's host OS/CPU build constraints; data portability does not enable
cross-compilation.

Tests cover incremental rebuilds, changed/removed transitive inputs, stable bytes,
missing/stale outputs, directory/archive identity parity, relocation and an offline
install/CPack run after dependencies and tools are built. Release metadata must
contain only the exact generated graph tested and staged.

## Content acquisition

Compiled apps declare required and optional pack dependencies through an exact
PackageLock. PackageAcquisition resolves missing content from explicit local or
remote sources into PackageStore, with progress, cancellation, quota checks and
verification. Required closure is ready before activation; an optional declared
pack is prepared and mounted transactionally at a safe owner-thread boundary.
AssetReader never fetches the network. Offline installed closures remain usable.

Package readiness means verified content is available locally, not that every
world cell is decoded, simulated or on the GPU. [World streaming](STREAMING.md)
uses indexed local reads and bounded preparation; optional remote dependencies
require explicit acquisition before they can satisfy cell readiness.

Acquisition has Queued, Fetching, Verifying, Ready, Failed and Cancelled outcomes.
Its durable result survives notification-queue pressure. A request generation
prevents superseded downloads from publishing a mount; cancellation cannot remove
content pinned by another consumer. Source configuration identifies trusted
metadata/keys, permitted endpoints, timeouts and maximum encoded/expanded bytes.
Redirects and resolved destinations obey the same network policy as other host
requests. Credentials remain scoped to their configured origin/provider.

Fetch into quarantine, verify the expected artifact/content graph, then atomically
publish. Interrupted or corrupt transfers never replace a working version. A
caller-supplied hash establishes expected identity, not publisher authenticity;
public sources require authenticated metadata, expiry, key rotation/revocation
and rollback protection. Local explicitly trusted content can be unsigned without
being misrepresented as publisher-authenticated. Content storage garbage collection
removes only unpinned/unreferenced versions under an explicit cache policy.

## App browsing and installation (future)

App bundles declare pack locks and use the same acquisition service. An in-program
browser treats listings as discovery data. Selection resolves a specific app
distribution, displays publisher/compatibility/permission information, verifies
its complete graph and installs it transactionally. Browsing/downloading never
runs app code. Untrusted listing media/markup cannot replace host permission UI.

In-app installation requests use the granted host surface. Apps cannot silently
install executable dependencies or enlarge their own permissions. Public app
registry identity and executable activation follow [distributed app contracts](DISTRIBUTED_APPS.md)
and the [security boundary](SECURITY.md). The content reader is independent of
an app marketplace or a particular managed catalog operator.
