# World verification

World tests establish identity, precise spatial state, streaming and persistence
independently of rendering, physics, networking and application controls.

## Contract workloads

| Area | Required cases |
|---|---|
| Coordinates | Equivalent near-origin and ±1,000,000 meter scenes within declared tolerance; invalid ranges/spaces reject |
| Frames | Attach/detach, angular contribution, cycles, deleted parents and consistent tick sampling |
| Views | Independent origins, stable picking and no geometry rebuild after origin changes |
| Queries | Hit order, filters, broad-phase versus exact semantics, missing coverage and explicit unsupported sweeps |
| Identity | Residency changes preserve IDs; restored/replaced epochs reject old work |
| Transfers | Preparation failure preserves source; cancellation, changed revisions and exactly one commit |
| Persistence | Late old saves, failure, migration, deletions, unloaded references and crash recovery |
| Streaming | Adjacent cells, negative boundaries, shared assets, large objects and bounded directory traversal |
| Pressure | Impossible pinned sets, canceled jobs retaining capacity and delayed retirement |

Use controlled clocks/executors and storage faults, then real workers. Reopen saves
for recovery assertions. Terminal outcomes persist until their declared release;
late completion never mutates a replaced world. Compare world quantities before
float extraction, not merely visually similar images.

## Module integration

Dataset-enabled workflows bind captured roots and frame snapshots, perform edits,
unload/reload and restore exact checkpoint references. Standalone datasets need no
World; bound edits publish through their selected coordinator. Source, presentation,
query and optional collision readiness remain independent.

A physics-enabled workflow streams collision for moving bodies, pins offscreen
support and waits/refuses incomplete mandatory coverage. A rendering workflow
extracts the same world near and far from its origin and verifies resource reuse.
[Spatial workloads](SPATIAL_TESTING.md) specify the combined acceptance. Navigation,
character controls and session replication are separate optional/future workloads.

## Focused commands

Use the repository test presets and module labels. Representative constituent checks:

```sh
cmake --build --preset debug --target playground_tests --parallel 4
ctest --preset debug -R '^(world_.*|runtime_services|host_smoke)$'
ctest --preset debug -R '^gpu_scene_residency$'
```

A test name is not a coverage claim. Record the actual built snapshot and results;
unsupported native execution is skipped. Existing route/controller fixtures remain
useful optional utility tests without defining the core world acceptance boundary.

## Integrated workloads

A world fixture discovers bounded content, retains query products, changes object
placement, prepares a transfer and restores a durable checkpoint with a fresh epoch.
Include failed preparation and retirement while readers retain old snapshots.
Use explicit automation and readiness messages rather than mouse timing. Reuse
[host tests](HOST_TESTING.md) for app switching and service progress during pause.

Finite scenery supplies useful pressure tests when its working set exceeds a
configured budget. Whole-model loading is not incremental world streaming. A
procedural source extends address discovery with the same interface; its particular
terrain rules remain an application fixture. Include a non-voxel cell directory,
stale continuation, missing versus known-empty coverage and numeric address limits.
World discovery cannot require allocating a dataset solely to list mesh cells. Persistent edits survive eviction and
compatible source regeneration.

## Measurements and completion

Record build/platform, provider/content identity, seed where applicable, bounds,
budgets and traversal. Report queue age, preparation/publication cost, missed
readiness deadlines, retained bytes, uploads and pending retirement. Separate cold
I/O, warm reuse and procedural generation. Latency thresholds require identified
hardware; bounded state, correct revisions and failure preservation are semantic
assertions. Advanced frames, portals and cross-region physical coupling remain
future regardless of baseline test results.
