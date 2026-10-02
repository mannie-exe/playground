# Distributed applications and projects (future)

This future contract covers creator projects, executable bundles and isolated
instances. Trusted compiled C++ apps remain the native application boundary;
asset packs, audio and networking do not depend on dynamic app loading.

## Authoring and artifacts

Code and visual tools are provisionally equal authoring entry points; the final
creator language and editor scope remain undecided. Both must produce the same
validated contracts. An editor should not embed a second runtime or encode
private callbacks that a code author cannot express.

| Artifact | Contains | Does not contain |
|---|---|---|
| Project | Editable code/content, app definition, asset sources, lock and tool settings | Machine-specific handles or build cache |
| Asset pack | Immutable typed content and dependency metadata | Executable entry point or permission grant |
| App bundle | App manifest, logic artifact(s), entry points and exact pack requirements | Arbitrary installer or post-install script |
| Distribution | App/pack digests, compatibility, provenance and optional verified publisher metadata | Automatically granted capabilities |
| Instance | Model, mounted content, granted service scope and session connections | Authority over other apps or global host settings |

Project/app manifests use TOML for identity, version, host API range,
logic ABI, client/server entry points, content lock, declared capabilities and
resource requests. The schema is versioned independently from the package release
and game protocol. App requested limits are requests; host policy determines
actual grants. Editor metadata is separate from runtime definitions and secrets.

Authoring resolves dependencies and compiles/cooks sources. Publishing emits
immutable artifacts and notices; installation verifies/stages them. Activation
mounts a pinned closure and creates a fresh instance. No phase runs arbitrary
package hooks. [Manifest generation/versioning](MANIFESTS.md) separates authored
intent, exact locks, runtime manifests and online listings. Download/update services remain separate from asset reads.
A creator's build toolchain runs with that creator's trust, not in the end user's
app loader; a hosted build farm needs its own isolation contract.

## Logic boundary

Recommend evaluating WebAssembly with a narrow host API for untrusted logic.
C++ could remain one source language compiled for this restricted target, without
exporting the current native IApp ABI. A VM runtime and SDK/toolchain choice need
a measured prototype on supported CPUs. A scripting language can target or sit
inside a suitable sandbox later; it is not selected by this proposal.

Use versioned value records, bounded buffers, opaque generational resources and
explicit asynchronous results at the boundary. No STL ABI, raw C++ pointers,
SDL/GPU objects, host closures or filesystem paths cross it. Module compilation
and serialized compiled-code caches are trusted runtime operations; never accept
an app-supplied native code cache as a validated Wasm module.
[WIT](https://component-model.bytecodealliance.org/design/wit.html) is a candidate
interface-description/generation mechanism, not the app authoring language.

A host-owned IApp adapter can project an isolated instance into the current
desktop lifecycle; it must not pass AppContext through to the guest. Headless
server instances use the same domain contract without desktop presentation.
Native plugins remain trusted developer extensions and cannot claim the
permission guarantees of isolated creator apps.

## Lifecycle and service surface

| Primitive | Responsibility |
|---|---|
| AppDefinition | Stable string identity, display metadata, requirements, logic and pack references |
| AppFactory/loader adapter | Construct a native trusted app or an isolated instance explicitly |
| AppInstanceId | Fresh runtime identity distinct from package/version/account |
| InstanceServices | Scoped content, storage, input, presentation, audio, jobs and sessions |
| InstanceLifecycle | Prepare, activate, suspend/resume, deactivate, dispose; failures have one terminal result |
| CapabilityGrantSet | Host-owned opaque authority, checked on every service use |

Prepare candidate content/runtime before changing the foreground app. Activation
commits at an owner-thread boundary. Failure disposes the candidate and retains
the previous app when rollback is possible. Deactivation revokes grants, prevents
late publication, stops scoped voices/jobs and closes or explicitly transfers
sessions before retirement. Suspension does not implicitly erase persistent
state or revoke session authority; each service has an explicit background policy.

AppRegistry's current enum IDs and AppInfo's borrowed name strings are unsuitable
for arbitrary installed definitions. Introduce owned stable namespaced IDs and
owned metadata, preserving existing persisted app keys through an adapter.
Validate duplicate identities, null factories and incompatible requirements.
Avoid a broad registry rewrite until a definition loader exercises this path.
One foreground app remains the initial host policy; multiple windows/apps or
background instances require explicit quotas and input/audio ownership.

## Compatibility and updates

Reject unsupported required schema/API/capabilities before execution. Optional
features must be queried; a missing feature cannot silently change protocol or
security behavior. Keep client and server logic/artifact compatibility explicit.
Do not ship server secrets or hidden authoritative state in public client packs.

Install into a digest-addressed store and atomically select a complete validated
version. Running instances pin their existing version. Apply updates between
activations initially; save migrations run transactionally with backup/rollback.
Do not enable arbitrary live code replacement or infer persistence from a UI tree.
Signatures identify an authorized publisher; they do not make code safe. Public
updates additionally need expiry, key rotation, revocation and rollback/freeze
protection; evaluate [TUF](https://theupdateframework.github.io/specification/latest/)
rather than designing a signing/update protocol from scratch.

Implementation cannot begin accepting untrusted executable bundles until the
[security release gates](SECURITY.md#release-gates) pass. Asset packs, audio and
networking do not depend on completing this future platform.
