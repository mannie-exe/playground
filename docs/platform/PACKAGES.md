# Asset packs

Asset packs extend the typed [catalog and source contracts](ASSETS.md).
[Manifest generation and versioning](MANIFESTS.md) define authoring, build,
install and acquisition. Executable [app bundles](DISTRIBUTED_APPS.md) are future
work and remain distinct from asset packs.

## Definitions and identity

An asset pack contains immutable data, typed definitions, dependencies and
provenance. It has no executable entry point, install hook or permission grant.
UI/theme documents are data members; their [loader/editor](../ui/DOCUMENTS.md)
is future work. Mounting a document does not execute logic or instantiate a UI.

A package has a stable namespaced ID, release version and exact content digest.
Schema version, package version, host API compatibility and asset revision are
separate fields. Persist names and kind strings, never AssetDefinition's variant
index or AssetCatalog's process-local cache key. A resolved asset reference is
package identity + local typed asset ID; an app's lock selects the exact version
and digest. Existing compiled IDs remain valid through an explicit adapter.

Authoring manifests use TOML, matching current settings and the existing
parser. UI trees use [JSON documents](../ui/DOCUMENTS.md). Both become validated
value definitions; neither format is parsed every frame. A manifest contains:

| Field | Contract |
|---|---|
| schema_version, kind | Supported schema and `assets`; reject executable kinds in the pack reader |
| id, version | Stable package ID and release version, independent of file name |
| requires | Host/content feature requirements; no silent substitute for missing required features |
| dependencies | Package ID, exact version/digest and declared source identity |
| assets | Local ID, stable kind, source entry, preparation props and typed dependency references |
| files | Normalized entry name, exact decoded byte length and SHA-256 digest |
| provenance | Source, author, license/notices and modifications for redistributable content |

The lock records exact versions, dependency closure and digests. Runtime mounting
uses only that lock and local content. No implicit download or filesystem search
changes the graph. There is one version per package ID within an app's mount set, rejecting conflicts/cycles; distinct app instances may mount different
versions. Resource preparation dependencies, package dependencies, build inputs
and execution ordering remain different graphs.

## Container and access

Use a directory form for development and a restricted ZIP container for portable
packs. Use stored entries for already compressed media and seekable audio, and
bounded deflate for suitable small entries.
The package API must support indexed entry access without unpacking everything.
ZIP64 is an explicit container capability with the same application limits, not
permission for unbounded files. Reject encryption, multipart/self-extracting
archives, links, devices, unsupported compression and implicit nested mounts.
The archive adapter uses a maintained implementation with indexed entry access
and tested decode limits; archive-library types do not cross the package API.
[ZIP specification](https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT);
[libarchive capabilities](https://libarchive.org/).

Entry paths use a portable, bounded relative-name profile. Reject absolute paths,
`..`, backslashes, NUL, drive/UNC forms, duplicate or case-colliding names, reserved
platform names and ambiguous normalization. Prefer lowercase ASCII storage names;
localized labels belong in metadata. Validate header/index agreement and checked
range arithmetic before reading payloads. Do not restore executable permissions
or extract entries into arbitrary host paths.

Hash the exact manifest bytes and declared payload bytes; do not sign a parser's
reserialized map. The external lock/distribution index carries the manifest or
archive digest so there is no self-hash cycle. Reproducible writers fix ordering,
metadata and compression settings. Hashes establish content identity/integrity,
not publisher trust. Validate every source, including local bundles. Data validation
and decoder limits do not establish isolated execution of hostile code or media;
that guarantee belongs to the future [security boundary](SECURITY.md).

## API and lifetime

| Primitive | Responsibility |
|---|---|
| PackageManifest / PackageLock | Validated definitions and exact resolution; no live handles |
| PackageStore | Immutable digest-addressed content, staged import, verify then atomic publication |
| PackageMount | Read-only indexed entry source; owns archive/file/byte lifetimes |
| AssetMountSet | Explicit package namespace and dependency closure for one app |
| AssetReader | Bounded whole-asset reads and seekable streams; no network side effects |
| CatalogBuilder | Convert validated definitions into a frozen typed catalog snapshot |
| PackageDiagnostics | Package/entry/asset identity, phase, limit and causal error |

Unmount removes discovery, not live data. Existing streams/handles pin their
mount until readers and realizations retire. Updating content constructs a new
mount/catalog generation and swaps it at an owner-thread boundary; failed
validation/preparation leaves the old generation intact. Automatic hot reload is
not implicit in mounting. Cache keys include resolved package content and
preparation variants while native caches still include ResourceDomainId.

AssetSource acquisition uses AssetReader; AssetRegistry and renderer caches
retain their own responsibilities. FileSource and ByteSource are adapters;
MountSource references a retained mount/entry. FontAsset uses the same sources
and Font cache keys include source identity, so fonts need no temporary extraction. SDL's [TTF_OpenFontIO](https://wiki.libsdl.org/SDL3_ttf/TTF_OpenFontIO)
requires the stream to remain valid until the font closes; clones must retain
that same source ownership. Use existing owner-thread font access rules.
Procedural mesh definitions remain compiled code unless a bounded data recipe is
explicitly supported; packs cannot name arbitrary C++ constructors.

## World payloads

[World/cell records](STREAMING.md) and [generator recipes](PROCEDURAL.md) are typed
data entries. Registered compiled providers validate recipe schemas and limits;
packs grant no code execution. World indices reference independently readable cell
payloads and shared asset dependencies. A package, scene, zone and cell are different
units; mounting a pack does not instantiate its world or make every cell resident.
Navigation tiles and generated meshes are versioned derived products, not saves.
Mutable entities, voxel edits and deletion records live in separate app storage.

## Bounds and preparation

Bound manifest bytes, files, path lengths, dependency depth/edges, encoded bytes,
expanded bytes per entry and aggregate, simultaneous streams/jobs and decoded
resource extents. Count actual decompression output against limits; do not trust
archive sizes or compression ratios alone. Reserve staging/output overlap and
keep source buffers charged while consumers retain them. Reads and decode jobs
return explicit refusal, cancellation, corruption or incompatibility outcomes.

Reuse frozen catalog validation and bounded worker preparation. Publication uses
request generation plus activation lifetime, and wakes an idle consumer only
after storing the result. Last-reference destruction of native resources stays
on its required owner thread. Dependency preparation does not require a general
task graph.

## Acceptance

Directory/pack parity covers fonts and variants, SVGs, image/model dependencies,
shaders for trusted built-in apps and missing-asset diagnostics. Test lazy reads,
seek behavior, dependency conflicts, duplicate IDs, truncation, path escapes,
symlinks, integer overflow, oversized expansion, cancellation and failed updates.
Keep old handles usable after unmount and replace a renderer without changing
content identity. Measure cold startup, peak preparation bytes and repeated reads;
packing is not assumed faster than loose files. Verify generated packs through
the build/install contracts in [MANIFESTS.md](MANIFESTS.md).

## Spatial payloads

[Dataset manifests](MANIFESTS.md#spatial-dataset-manifests) reference bounded
independently decodable channel groups/index pages with encoded and decoded limits.
Pack boundaries do not define world cells or voxel chunks. The writer preserves
exact schemas/source identities and can include derived products keyed by their
recipes/targets. Mounted packages remain immutable; [dataset checkpoints](VOXELS.md)
store runtime edits separately. Source adapters consume verified AssetReader data
without hidden extraction or network acquisition. Missing providers preserve
manifest/payload identity while refusing unsupported preparation.
