# Application workloads

Applications exercise reusable Playground modules while owning formats, rules and
workflow. These are intended target responsibilities, not implementation status.
The [architecture](../platform/ARCHITECTURE.md) defines linkage and host composition;
the [freeze boundary](../platform/DESIGN_FREEZE.md) defines what is being stabilized.

| Target | Responsibility | Primary pressure |
|---|---|---|
| `playground_lib` | Reusable modules with selective linkage and optional native adapters | Lifetime, scheduling, publication and bounded ownership |
| `playground_demo` | Existing UI, scenes, materials and physical-object fixtures | 2D/3D rendering, input, assets and Jolt integration |
| `playground_voxels` | [Scalar-volume inspection and label editing](VOLUMES.md) | Bricked I/O, physical coordinates, linked slices and durable changes |
| `playground_minecraft` | [Java 26.3 spectator client](MINECRAFT.md) | TCP/HTTPS, mouse look/player movement, chunk streaming and native rendering |

The sampled-data consumers need not share a Voxel struct. Medical intensities keep
signed ranges, units and sampling semantics. Minecraft palettes, lighting, dimensions
and registries belong to its app adapter. Both reuse admission, revisions, coordinate
mapping, residency and rendering. Neither introduces an ECS or game protocol into
playground_lib. Generic authority-session acceptance uses its own small nonspatial
reference domain; it does not wait for the Minecraft application.

## Future reusable editor components

Editor components remain optional UI work. They do not gate the four target split,
initial volume viewer or required Minecraft controls. Their contracts live with
the owning systems rather than in an application-specific feature catalog.

| Component | Owning contract |
|---|---|
| Edit sessions, preview/commit/cancel and undo history | [UI contracts](../ui/CONTRACTS.md#future-edit-sessions-and-history) |
| Docked workspace splits/tabs and transform tools | [Controls](../ui/CONTROLS.md#future-workspace-panels) |
| Exact-time ruler, timeline, playhead and seek intent | [Controls](../ui/CONTROLS.md#future-timeline-and-ruler) |
| File/asset browser and asynchronous thumbnails | [Async UI](../ui/ASYNC.md#future-file-and-asset-browser) |
| Cursor-anchored pan/zoom and coordinate mapping | [2D rendering](../render/2D.md#future-canvas-navigation) |
| Viewport composition and picture-in-picture | [3D rendering](../render/3D.md#future-composed-viewports) |
| Cross-component acceptance | [UI verification](../ui/TESTING.md#future-editor-component-verification) |

The [Artcraft catalog](https://getartcraft.com/apps), PhotoCraft, FilmCraft, Qt and
Godot supply interaction/API references. They are not new runtime dependencies or
claims of interchangeable implementations. Domain-specific brushes, clip editing,
medical tools and Minecraft content interpretation stay in applications. General
UI document authoring, native floating windows and multi-app hosting remain future.
