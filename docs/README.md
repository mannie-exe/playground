# Playground contracts

Playground is an application runtime with optional presentation, content, spatial,
physics, audio and networking modules. These documents specify the intended
architecture, public behavior and implementation shape. They are not an inventory
of implemented features. Acceptance requires the verification for each module.

Start with [architecture and scope](platform/ARCHITECTURE.md), then
[application lifetime](platform/APPLICATIONS.md) and [runtime ordering](platform/RUNTIME.md).
The [build guide](../CONTRIBUTING.md) describes development and validation.

## Contract map

| Area | Contracts |
|---|---|
| Runtime and applications | [Architecture](platform/ARCHITECTURE.md), [applications](platform/APPLICATIONS.md), [runtime](platform/RUNTIME.md), [activity](platform/ACTIVITY.md), [host verification](platform/HOST_TESTING.md) |
| Saved content and state | [Composition documents](platform/COMPOSITION.md), [assets](platform/ASSETS.md), [packs](platform/PACKAGES.md), [versions/manifests](platform/MANIFESTS.md) |
| Spatial state | [Worlds](platform/WORLDS.md), [streaming](platform/STREAMING.md), [world verification](platform/WORLD_TESTING.md) |
| Optional sampled data | [Datasets](platform/SPATIAL_DATA.md), [voxel storage/edits](platform/VOXELS.md), [products](platform/SPATIAL_PRODUCTS.md), [presentation](render/SPATIAL.md), [verification](platform/SPATIAL_TESTING.md) |
| Optional computation | [Source providers](platform/PROCEDURAL.md), [regional simulation](platform/SIMULATION.md), [Jolt physics](platform/PHYSICS.md) |
| Optional communication and sound | [Transport and sessions](platform/NETWORKING.md), [audio and FMOD](audio/README.md) |
| Desktop policy | [Windows](platform/WINDOWING.md), [settings](platform/SETTINGS.md), [graphics](platform/GRAPHICS.md) |
| Rendering | [Architecture](render/ARCHITECTURE.md), [coordinates/values](render/CONTRACTS.md), [resources](render/RESOURCES.md), [materials](render/MATERIALS.md), [verification](render/TESTING.md) |
| UI | [Guide](ui/GUIDE.md), [reference](ui/REFERENCE.md), [controls](ui/CONTROLS.md), [themes](ui/THEMING.md), [motion](ui/MOTION.md), [async](ui/ASYNC.md), [accessibility](ui/ACCESSIBILITY.md), [verification](ui/TESTING.md) |
| Optional application utilities | [Cameras](render/CAMERAS.md), [locomotion](platform/LOCOMOTION.md), [navigation](platform/NAVIGATION.md) |
| Applications and experiments | [Demo and voxel targets](render/DEMOS.md) |

## Scope labels

**Core** contracts govern lifetime, identity, execution and communication between
owners. **Optional module** contracts are implementation requirements for the
module when enabled; they are not dependencies of every application. **Application
or prototype** contracts describe consumers and examples without adding engine
requirements. **Future** sections are excluded from baseline acceptance.

APIs name reusable boundaries and values. Reuse existing types where their
semantics match; private containers, algorithms and tuning remain implementation
choices. A documented workload name denotes a verification responsibility, not
proof that an executable or passing result exists.

## Future capabilities

- [Downloaded executable apps, background instances and dynamic loading](platform/DISTRIBUTED_APPS.md).
- [Untrusted app permissions and isolation](platform/SECURITY.md).
- [General UI documents and editor](ui/DOCUMENTS.md).
- [Character motors, vehicles and advanced movement](platform/LOCOMOTION.md#physics-movement-realization).
- [General fracture, advanced physics and cross-region coupling](platform/PHYSICS.md#future-advanced-physics).
- [Prediction, rollback, peer authority and offline merging](platform/NETWORKING.md#future-session-capabilities).
- [Planetary frames and seamless portals](platform/WORLDS.md#future-capabilities).
- Partial texture-mip residency and automatic HLOD in [streaming](platform/STREAMING.md#content-and-missing-regions).
- Smooth isosurfaces, volume ray marching, fluid solvers and multiresolution terrain.
- Terrain generation rules, infinite-world game design and material simulations in `playground_voxel`.
- [Camera obstruction](render/CAMERAS.md#future-camera-obstruction), volume planners and crowd avoidance.
- Combobox and Autocomplete in [controls](ui/CONTROLS.md).

[C++ notes](NOTES.md), [historical migration notes](CATCH_UP.md), and the
[learning outline](TODO.md) are reference material, not normative backlogs.
