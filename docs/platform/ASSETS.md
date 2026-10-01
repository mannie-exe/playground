# C++ authoring and asset ownership

C++ remains the authoring language. App factories remain compiled; no document
app, UI serializer, filesystem watcher or automatic disk reload is implied.

## Definitions, instances and realizations

Definitions describe immutable content and preparation inputs. Instances own
application/model state. Realizations are decoded or device-specific resources.
Reconstructing a view or replacing a renderer must not reset its application model.
Resource handles preserve lifetime, not thread safety or GPU completion.

The asset catalog owns typed logical identities, source definitions and resource
dependencies. The existing AssetRegistry remains the SDL resource cache; it is not
the catalog. Native realization caches remain renderer-owned. Build dependencies
(HLSL includes), resource dependencies (model images), and execution dependencies
(producer passes before consumers) are separate relationships.

Catalog registration is C++-only and freezes before consumption. File locations
are relative to a supplied content root; owned byte sources and procedural meshes
do not require a filesystem path. Definition revisions are explicit, not automatic
file-change detection. Variant parameters remain part of resource cache keys.
Catalog identity/revision is distinct from renderer ResourceDomainId.

Dependencies are validated, cycle-checked and traversed once per identity when
collecting a closure. They describe preparation, not permanent residency. A failed
load publishes no resource. Existing handles survive cache eviction.

## Application reconstruction

App-owned models outlive their view trees. C++ view construction receives explicit
props, resource bundles and model access. Nodes own hover/capture/layout state;
models own durable game state. Rebuilding a view does not preserve transient input
state unless an application explicitly supplies a view-state policy. Runtime node
IDs are never persistent model identities. Subscriptions belong to view instances.

## Preparation and concurrency

Synchronous acquisition remains the default for small startup assets. CPU model
preparation also has an explicit bounded background path. Workers receive owned
inputs and publish immutable results; they do not mutate a catalog, live scene,
UI tree, SDL font or GPU device. Owner-thread polling accepts only the current
request generation. Cancellation suppresses publication and is cooperative, not
forced interruption of a parser. Failure is retained as an exception result.
Executor destruction requests stop and joins workers before dependent services die.

No parallel layout, general task graph, native plugin loading, package format,
automatic reload or arbitrary callback serialization is part of this boundary.

## Public API

| API | Contract |
|---|---|
| `assets::AssetId<T>` | Owning logical name typed by definition kind; not a path or native handle |
| `AssetCatalog(root)` | Register C++ definitions under one content root |
| `add(id, definition, dependencies, revision)` | Reject invalid/duplicate identities; revision defaults to 1 |
| `freeze()` | Validate references/cycles before concurrent reads; failure leaves registration open |
| `dependencies(roots)` | Iterative dependency-first traversal, each identity once, including roots |
| `definition(id)` | Borrow immutable definition; retain its catalog |
| `read(source, limit)` | Bounded relative file or owned bytes; no implicit network access |
| `cacheKey(key)` | Process-local catalog identity + kind/name/revision; not a persisted ID/hash |
| `sdl::AssetResources` | Owner-thread adapter with shared frozen catalog and borrowed SDL cache |
| `font/image/vector/mesh/model/shader` | Typed acquisition; failed construction publishes no cache entry |
| `publishModel(result)` | Accept a successful matching-catalog result; reuse already-cached content |
| `trimUnused()` | Release unused vector/model/shader cache entries without invalidating live handles |
| `assets::prepareShader` | Read SPIR-V, reflect and verify stage/layout; no GPU allocation |
| `sdl::prepareModel` | Synchronous CPU import with registered URI mappings; no scene mutation |
| `sdl::ModelPreparation` | One consumer's generation-checked asynchronous request/result slot |
| `runtime::Executor` | Bounded worker execution, not a UI dispatcher or dependency scheduler |

`playground_assets` owns neutral definitions and depends on scene/rendering
contracts, never SDL. `playground_sdl` owns decoding/cache adapters.
`playground_runtime` owns the executor and completion mailbox. The catalog's
closed variant covers image/font/vector/binary/mesh/shader/model definitions;
it is not a plugin/type-registration system.

File sources use lexical/canonical confinement checks, not a filesystem-race
sandbox. ByteSource owns bytes. Fonts currently use files; images, vectors,
models and shaders also accept encoded bytes. Mesh assets hold immutable
MeshHandles. Definitions are not file snapshots: acquisition reads then caches.
Changing disk contents does not invalidate handles; frozen catalogs cannot be
mutated. New catalogs can explicitly define new revisions without aliasing old
cache keys. The numeric kind and catalog identity are process-local details.

AssetResourceProps bounds encoded/decoded images and SVG inputs. Image decoded
bytes are checked after decoding and on cache hits, not through a capped allocator.
ModelAsset owns its ModelImportProps; import variants use distinct IDs. Published mesh and packed texture payloads carry shared CPU ledger charges.
Decoded registry surfaces are charged on adoption by pitch times height; decoder
private allocations are outside that coverage. General model graphs, shader
code, strings and font-engine internals are not complete process-heap accounting. Font
variant keys include catalog identity; clones preserve it unless changing path.

AssetRegistry retains its existing name and low-level Text/Vector cache APIs.
AppContext::resources is typed acquisition; assets is the low-level cache. App
Assets.hpp files register paths centrally. The obsolete AppContext::assetPath
shortcut and support/AssetPath.hpp were removed.

## Model and shader preparation

ModelAsset::resources maps exact external glTF URIs to BinaryAsset IDs, adding
dependency edges automatically. Embedded GLB/data resources need no catalog entry.
The separate sdl::loadGLTF(path) convenience API retains relative-file loading;
catalog imports never implicitly use it. Both share PNG/JPEG and KTX2 decoding with
private stream/surface/container ownership. ModelImportServices::decodeTexture receives the
requested color/emission/normal/data interpretation; cached resources include that
interpretation. Radiance environment bytes use a private stb decoder because
SDL_image's bundled stb path disables HDR; stb also inspects PNG/JPEG dimensions
before SDL_image decoding. KTX2/Basis uses pinned libktx, including glTF
KHR_texture_basisu sources. Packed format/encoding and mip bytes survive CPU
preparation; GPU upload remains a separate owner-thread phase. Decoders and mesh
preparation share the rendering ledger through `PreparationBudget`; retained payloads and temporary
estimates are different measurements. No font or GPU work runs in the importer.

assets::ModelAsset is a preparation definition; scene::ModelAsset is its immutable
prepared result. Calling instantiate explicitly mutates an owner-thread Scene3D.
Scene instances, snapshots and device realizations remain distinct.

ShaderAsset describes compiled SPIR-V, stage, entry point and binding layout.
HLSL compilation and explicit include dependencies remain in cmake/Shaders.cmake.
Built-in shader loading uses catalog preparation after selecting installed or
development output. GPUShaderPipeline accepts prepared vertex/fragment values
and native pipeline props; this constructor never polls files. The existing
path-based constructor retains explicit poll behavior. Custom pipelines are
native extensions, not interchangeable with built-in MaterialProps or a material graph.

## Asynchronous requests

AppContext::workers creates the executor lazily. Demo/Minesweeper remain synchronous;
no worker starts merely to display them. An app owns a ModelPreparation slot and
calls start with executor, frozen catalog handle and ModelAsset ID. False means
admission refused and the previous request is unchanged. Acceptance supersedes
the old generation without blocking. Superseded tasks still occupy capacity until
they finish or are dequeued.

Poll during app update. No result means not ready. A result owns generation,
asset ID, definition key and either model or exception_ptr; it is consumed once.
The app can publishModel into its resource cache and explicitly instantiate at a
safe scene boundary. This path uses a future slot, not captured app callbacks or
a second completion queue. Existing UI CompletionQueue still routes UI callbacks.

The future becoming ready does not itself wake AppHost. An on-demand app must
declare update demand or a polling deadline while a request is pending; otherwise
publication can wait until an unrelated event or the host's maintenance update.
Worker paths with a wake endpoint must publish the result before notifying it.
See [host activity](ACTIVITY.md) for demand and fallback behavior.

cancel discards the slot and requests cooperative stop. Checks surround reads,
decoding and parsing; a blocking parser/decoder/read is not forcibly interrupted.
Admission reserves configured document/parser/resource byte maxima, not all heap
usage: geometry, decoded peaks, retained results and catalog bytes are separate.
Jobs have a noexcept execution boundary and must transport their own errors;
ModelPreparation does so. close stops admission, cancels queued/running jobs and
wakes workers; destruction additionally joins. Never destroy the executor from
its own job. Owner slots must not be mutated concurrently. A queued job discarded
by shutdown can yield broken-promise error if its consumer has not canceled.

## Application reconstruction

Demo owns ViewProps and handles and constructs a candidate via makeDemo2DUI.
MinesweeperApp owns MinesweeperModel; UI/Grid/Cell borrow it and display its
revision. Board state no longer lives in nodes or pointer-bearing SDL user events.
Rebuilding a view preserves the model; new game explicitly resets it. A board's
dimensions remain immutable; different settings require a new matching model/UI.
Minesweeper's app-local difficulty screen owns no board: it edits the app's draft,
and Start stages a matching model/view before replacing the menu. See
[application composition](APPLICATIONS.md) for input and sizing contracts.

The new-game button retains its view-owned connection. Cells invoke typed model
actions; visual synchronization follows routing and runs during update. Clear
operations stage a board copy and iterative flood queue before committing. This
prioritizes consistent failure over optimizing a small study board. Visual sync
can fail independently and retry while revisions differ. Tree reconstruction
intentionally resets hover/capture/node IDs, not durable model state.

## Verification and extensions

### Content organization

Keep engine shaders under `shaders/`, shader-test fixtures under `tests/shaders/`,
and installed content under `assets/`. CMake owns shader compilation and packaging;
catalog IDs own application-facing identity. A file move should only require
updating its registration, not changing UI nodes or logical IDs. Demo 2D and
Minesweeper register their assets in their respective `Assets.hpp` files;
Demo 3D registers its scene assets alongside its app implementation.
`app/Assets.hpp` owns the font shared by the launcher and demos.

Do not move or remove unreferenced study assets just because current catalog
registrations do not load them. Before distributing content, record its source,
author, license and any modifications alongside the asset or in an asset manifest.
Existing image/font provenance is not fully documented; dependency license
installation does not establish permission to redistribute application content.
The [runtime content inventory](../../assets/README.md) lists current assets,
their catalog usage and unresolved provenance without inventing license grants.
New imported models should include their external buffers/textures and explicit
catalog dependencies. Editable source files and generated runtime outputs are
different artifacts; keep generated outputs in the build/install tree.

asset_catalog covers identity, dependency ordering/cycles, reads and freeze failure.
asset_resources covers resource variants and generic UI reconstruction.
runtime_executor/model_preparation cover admission, cancellation, supersession,
worker failure and owner publication. gpu_shaders executes catalog-prepared native
pipelines. These do not establish arbitrary codec security or global memory budgets.

Disk watching, dependency-triggered reload, UI serialization, plugins, packing and
general task graphs remain future choices. Build/source/runtime dependency graphs
must stay distinct if those are added.
Preparation reservations are conservative scratch/admission estimates and can
overlap the lifetime of newly published payload charges. They are not another
measurement of physical RAM. Packed texture copies acquire their own charge;
shared handles retain one charge. Externally supplied vectors/surfaces have
already allocated before adoption; ownership accounting does not retroactively
bound that caller's allocation. See [RESOURCES.md](../render/RESOURCES.md).
