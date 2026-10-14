# Runtime architecture and scope

Playground hosts trusted applications through explicit lifetimes, scoped services
and publication boundaries. Applications select reusable modules and own their
behavior. An application can be a viewer, collaboration tool, game or launcher.

## Layers and dependency direction

| Layer | Responsibility | Excludes |
|---|---|---|
| Runtime core | IDs/epochs, scoped lifetime, work admission, completion, publication, accounting and diagnostics | Application rules, native solver/device objects, required scene graph |
| Optional modules | UI, rendering/scenes, assets/storage, world coordinates, spatial datasets, physics, audio, networking | Automatic application construction or mandatory use by every consumer |
| Native adapters | SDL presentation, Jolt, FMOD, Asio, libcurl and GameNetworkingSockets | Public native handles in portable module APIs |
| Applications | Models, composition, controls, source/recipe implementations and policy | Ownership of shared host devices through accidental globals |

Dependencies point from applications/adapters to reusable contracts. Dataset and
world tools run without SDL video, fonts, UI or a renderer. Basic math remains a
small value library; advanced spatial indexes are optional implementations. Neither
an ECS nor a universal behavior tree, material model or message bus is required.

```mermaid
flowchart TD
    Demo[playground_demo] --> Host[Native application host]
    Voxel[playground_voxels] --> Host
    Minecraft[playground_minecraft] --> Host
    Host --> Core[Runtime core and scoped services]
    Host --> Modules[Selected reusable modules]
    Modules --> Core
    Adapters[SDL / Jolt / FMOD / network providers] --> Modules
```

## Build and composition targets

| Target | Contents |
|---|---|
| `playground_lib` | Public CMake aggregate of selected reusable modules, preserving direct module linkage |
| `playground_demo` | Launcher, existing UI/scene fixtures and physical-object tests |
| `playground_voxels` | Medical/scalar-volume inspection and editing |
| `playground_minecraft` | Native Java Edition spectator client with application-owned protocol and terrain interpretation |

Retain internal targets such as playground_runtime, playground_world,
playground_assets, playground_scene and playground_sdl. Introduce focused module
and adapter targets for new services. The aggregate includes the configured module
selection, not every optional native adapter. A headless consumer links the neutral runtime and required modules directly
or configures a headless aggregate; enabling an unrelated executable must not change
those modules' dependency closure. The aggregate does not flatten ownership or
force headless consumers to link presentation. Native dependencies are PRIVATE
unless a public adapter header requires them; neutral APIs contain no native types.

Extract host/session code from concrete application registrations. Each executable
supplies its own catalog construction, factory registry, initial app and shell
policy. A mono-app distribution and a launcher distribution use the same launch
sequence. Neither the library nor host constructor names Bistro or a menu.

Optional dependencies have explicit build switches and runtime capability reports.
Disabled Jolt, FMOD or transport adapters must not be fetched, initialized or linked
incidentally. CMake pins native source dependencies; externally supplied SDKs record
and validate an exact supported version and target. FMOD packaging is conditional
on a supplied SDK and applicable redistribution terms. No SDK download credentials
or vendor binaries belong in the source tree.

### Configuration contract

The following cache inputs define the intended CP01 interface. Existing presets
select the desktop demo explicitly; a bare library configuration selects runtime
only. Selection is resolved before any dependency fetch or platform initialization.

| Input | Meaning |
|---|---|
| `PLAYGROUND_MODULES` | Semicolon-separated reusable module roots; default `runtime`; aggregate contains these roots and their neutral dependency closure |
| `PLAYGROUND_APPLICATIONS` | Semicolon-separated `demo`, `voxels`, `minecraft`; default empty; each adds its own required closure without changing the aggregate's selected roots |
| `PLAYGROUND_PROVIDERS` | Explicit native providers: `sdl`, `jolt`, `asio`, `curl`, `gns`, `fmod`; default empty |
| `PLAYGROUND_BUILD_TESTS` | Tests for selected capabilities only; tests cannot enable a disabled provider |
| `PLAYGROUND_GPU` | GPU implementation within selected rendering/presentation; OFF excludes shader compilation, GPU SDK fetches and hardware tests |
| `PLAYGROUND_FMOD_ROOT` | Supplied SDK location, inspected only when `fmod` is selected |

Neutral module roots include runtime/math, world, storage, assets, rendering, scene,
layout, text, UI, spatial data/products, simulation, physics, sockets, HTTP, message
transport, sessions and audio. CMake maps these names to focused targets; public
target names retain the `playground_` prefix. Module contracts may build with fake
providers. Executables requiring an absent provider fail configuration with the
missing requirement; selection never silently fetches a disabled adapter. Unknown
names and unavailable application targets fail rather than producing empty binaries.

Asset identity/catalog/storage must not require scene import or rendering. Put
model decoding and scene realization in their owning optional modules. Runtime,
world, storage and sampled-data consumers exclude SDL, ICU, fonts, shader tools and
GPU loaders. Text/UI explicitly select their text dependencies. Host policy and
registries remain reusable; SDL presentation is an adapter, not a runtime dependency.

Export/install only configured targets and their actual dependency closure. Generate
editor commands from selected translation units without requiring a demo entrypoint.
Verify runtime-only, world/storage, sampled-data, each enabled provider and each
application configuration independently. Compile and link an external installed
consumer as well as an in-tree test; absence from a link line alone does not prove
absence from configure-time fetching or installed runtime artifacts.

## Stable boundaries

Reuse ServicePump/ServiceScope, Executor, CompletionQueue, ActivationLifetime,
ResourceLedger, AssetCatalog, WorldSnapshot and existing render resource handles.
New services extend these contracts rather than creating parallel lifecycle or
accounting frameworks. Resource ownership is distinct from logical demand, thread
safety, GPU completion and permission. Native trusted extensions are not isolated.

Every cross-owner API specifies inputs, units, identity/epoch, calling thread,
admission outcome, publication boundary, retained result and retirement behavior.
Completion wakes are notifications; the service owns the result. Accepted work is
not completed work, and committed model state is not necessarily durable state.

Use typed requests/results and snapshots at concrete boundaries. A universal
serialized command envelope is not required for native calls. Network/guest codecs
translate validated values explicitly. Independent services need not share a clock,
worker pool implementation or atomic transaction.

## Required optional modules

Rendering retains meshes, materials, textures and explicit capability negotiation.
Content services retain typed identity, bounded acquisition, immutable definitions
and independent device realizations. Saved [composition](COMPOSITION.md) references
those definitions and registered behavior configuration without serializing code.

Spatial services support finite manifests and bounded paged discovery for mesh cells or datasets, precise
coordinates, independently ready products and durable state. "Infinite" sources
resolve previously unvisited addresses within declared finite numeric bounds;
resident data and work remain bounded. Chunk size, tree layout, generation rules
and region shape are not runtime-core contracts.

Bounded voxel data, edits and scalar/label presentation belong to optional sampled-
data modules. Baseline adapters cover `.vox` and a declared NRRD subset, exposed-face
meshes and scalar slices. Application-defined meaning does not require one entity
per sample. Small regional simulation provides ordering and activation; applications
supply kernels. It is not a prerequisite for loaded static data.

Jolt realizes physical bodies and queries. FMOD realizes audio playback and authored
Studio events. Asio realizes TCP/UDP sockets, libcurl realizes explicit HTTPS, and
GameNetworkingSockets realizes reliable/unreliable messages. These are independent
capabilities; raw TCP clients do not require the message/session module. An optional
session module provides ordered authority, filtered baselines and replicas for collaborative
and real-time domains. Enabling networking never forces world/voxel replication.

## Application and future scope

Demo views, controls and workflows remain application requirements. Locomotion,
camera rigs and navigation are optional utilities; their algorithms do not prescribe
the application model or block storage/physics acceptance. Terrain rules, general
fracture, water, inventories and other domain simulations belong to prototypes.

One resident native application with one foreground presentation is the initial
host policy. Instance lifetime, suspension and presentation attachment remain
separate so concurrent background apps and multiple windows can be added later.
A retained replacement candidate is not a second active application. Downloaded
logic, isolation, hot code replacement, storefronts and remote installation remain
[future](DISTRIBUTED_APPS.md). Built-in startup selection is required now.

Editor workspaces, transform tools, timelines, asset browsers and general viewport
composition are [future UI facilities](../apps/WORKLOADS.md#future-reusable-editor-components).
Minecraft spectator input is a required application controller, not a deferred
physical character motor. Its packet schemas and resource interpretation remain
app-owned even when they require new reusable rendering or I/O facilities.

## Completion evidence

[Host verification](HOST_TESTING.md) defines composition and service-ordering evidence.
[Spatial verification](SPATIAL_TESTING.md) uses both label volumes and scalar fields.
Native adapter tests prove actual solver/device/transport behavior separately from
fake-provider sequencing. Performance records identify build, hardware, input trace
and budgets. A capability is supported only after its relevant checks pass; a
skipped native test is untested coverage. Internal representations remain replaceable
without altering these guarantees. The [design-freeze boundary](DESIGN_FREEZE.md)
separates stable semantics from native implementation and application conformance.
