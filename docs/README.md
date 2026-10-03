# Playground contracts

These documents define the target architecture, behavior and APIs for
implementation. Sections marked **future** are excluded from the implementation
scope. Contract text is not a report of implementation or test status.

| Area | Contracts |
|---|---|
| Build, install and release | [Contributing](../CONTRIBUTING.md), [manifest generation](platform/MANIFESTS.md) |
| Applications and models | [Applications](platform/APPLICATIONS.md), [runtime](platform/RUNTIME.md), [activity](platform/ACTIVITY.md) |
| Worlds and space | [Identity, coordinates, frames and persistence](platform/WORLDS.md), [streaming](platform/STREAMING.md), [verification](platform/WORLD_TESTING.md) |
| Spatial data and voxels | [Datasets and sources](platform/SPATIAL_DATA.md), [storage/edits/checkpoints](platform/VOXELS.md), [products/scheduling](platform/SPATIAL_PRODUCTS.md), [verification](platform/SPATIAL_TESTING.md) |
| Generation and simulation | [Procedural providers](platform/PROCEDURAL.md), [simulation](platform/SIMULATION.md), [navigation](platform/NAVIGATION.md), [Jolt physics](platform/PHYSICS.md) |
| Locomotion and cameras | [Profiles and facing](platform/LOCOMOTION.md), [rigs and hand-off](render/CAMERAS.md), [verification](render/TESTING.md) |
| Content | [Asset ownership](platform/ASSETS.md), [packs](platform/PACKAGES.md), [versions/manifests](platform/MANIFESTS.md) |
| Audio | [Playback, synthesis, spatial state and accounting](audio/README.md) |
| Networking | [Transport, shared sessions and hosting](platform/NETWORKING.md) |
| Desktop policy | [Windows](platform/WINDOWING.md), [settings](platform/SETTINGS.md), [graphics](platform/GRAPHICS.md) |
| Rendering | [Demos](render/DEMOS.md), [Architecture](render/ARCHITECTURE.md), [coordinates and values](render/CONTRACTS.md), [resources](render/RESOURCES.md), [spatial presentation/selection](render/SPATIAL.md) |
| UI composition | [Guide](ui/GUIDE.md), [reference](ui/REFERENCE.md), [controls](ui/CONTROLS.md) |
| UI services | [Themes](ui/THEMING.md), [motion](ui/MOTION.md), [async](ui/ASYNC.md), [accessibility](ui/ACCESSIBILITY.md), [testing](ui/TESTING.md) |

## Future capabilities

- [App authoring, executable bundles and dynamic loading](platform/DISTRIBUTED_APPS.md)
- [Executable-app permissions and isolation](platform/SECURITY.md)
- [Runtime UI documents and editor](ui/DOCUMENTS.md)
- [Camera obstruction](render/CAMERAS.md#future-camera-obstruction)
- [Vehicle movement and root motion](platform/LOCOMOTION.md#future-advanced-movement)
- [Vehicles, elaborate joints, destruction and advanced physics](platform/PHYSICS.md#future-advanced-physics)
- [User-facing multiplayer demos](platform/NETWORKING.md#future-session-capabilities)
- [Planetary frames, seamless portals and distributed transfers](platform/WORLDS.md#future-capabilities)
- [Prediction/rollback and authority migration](platform/NETWORKING.md#future-session-capabilities)
- Partial texture-mip residency and automatic HLOD in [streaming](platform/STREAMING.md#content-and-missing-regions)
- Smooth isosurface terrain, fluid solvers and multiresolution terrain; the [spatial interfaces and baseline adapters](render/SPATIAL.md) are implementation requirements
- Volume planners and crowd avoidance in [navigation](platform/NAVIGATION.md)
- Combobox and Autocomplete in [control contracts](ui/CONTROLS.md)

[C++ notes](NOTES.md), the [historical migration guide](CATCH_UP.md), and
[original learning outline](TODO.md) are learning material, not architecture
requirements or implementation backlogs.

[Scene verification workloads](render/TESTING.md) cover camera, resource reuse and
native benchmark workflows.
