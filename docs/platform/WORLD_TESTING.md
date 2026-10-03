# World verification

Tests separate world/model contracts, adapters and native presentation. A fake
physics or network provider establishes ownership/error behavior only. Baseline
world, streaming, procedural/voxel and navigation acceptance does not require a
physics backend, editor, executable app loader or new rendering features.

## Contract workloads

| Area | Required cases |
|---|---|
| Coordinates | Equivalent scenes near zero and at ±1,000,000 meters; conversion within declared tolerance, invalid ranges and mismatched spaces |
| Frames | Attach/detach policies, angular velocity contribution, cycles, deleted parents and consistent tick sampling |
| Views | Two distant simultaneous origins, origin changes during interpolation, stable picking and no repeated mesh uploads |
| Queries | Nearest-hit ordering, filters, overlapping bounds versus exact hits, missing coverage, truncation and unsupported sweeps |
| Identity | Unload/reload preserves entity identity; replacement epochs reject all stale results |
| Transfers | Failed preparation leaves source intact; cancellation, source revision changes, dependent entities and exactly one commit |
| Persistence | Old saves complete late, failed writes, schema migration, multi-cell edits, deletion records and crash recovery |
| Cells | Three adjacent cells, negative boundaries, shared assets, large objects and repeated crossings without respawn or churn |
| Pressure | Mandatory working set exceeds budget, canceled jobs still consume capacity, staged old/new overlap and delayed retirement |
| Generation | Golden hashes, different traversal/worker order, supported target parity, neighbor halos, bounded failures and version mismatch |
| Voxels | Empty overrides, boundary edits, missing neighbors, stale derived output, interrupted compaction and reload parity |
| Navigation | Graph/grid/mesh, clearance, disconnected layers, no path versus missing data, budget exhaustion, invalid corridors and stalls |
| Autonomous motion | Actual-pose progress, Tank/Steered/Strafe integration, moving-frame traversal and failed cross-space links |
| Replication | Two observers, filtered baselines, interest exit versus deletion, stale epochs/sequences, edits and incompatible generator identity |
| Physics boundary | Fake adapter command/result order, missing capability, stale region/tick and explicit kinematic policy |

Use injected clocks, deterministic executors and storage fault injection. Require
terminal results after cancellation/failure, bounded retry work and no late live
mutation. Recovery tests reopen persisted data, not merely inspect a successful
write callback. Coordinate fixtures compare world-space quantities before float
projection as well as local output; increasing tolerance to hide lost precision
is not an acceptable far-origin test.

## Dataset integration

[Spatial verification](SPATIAL_TESTING.md) defines dataset/source schemas,
transactional multi-chunk edits, product freshness, simulation ordering and bounded
live streams. World workflows bind exact dataset/frame snapshots, save checkpoint
references and restore fresh epochs. Verify that editing a standalone dataset does
not require a world, while world-bound edits cannot bypass its model coordinator.

RequireProducts failure preserves old bindings. Source generation and surface,
selection, collision and navigation preparation have independent capability and
revision assertions. Shared fixtures exercise unloaded boundaries, stale workers,
failed saves and accounting retirement instead of counting nominal API calls.

## Focused commands

```sh
cmake --build --preset debug --target playground_tests --parallel 4
ctest --preset debug -R '^(world_.*|runtime_services|host_smoke)$'
# Native GPU readback and immutable-upload reuse, when GPU support is built:
ctest --preset debug -R '^gpu_scene_residency$'
```

The world presentation fixture compares complete software images at zero and
±1,000,000 meters and verifies retained UI invalidation and picking. Native scene
residency checks equivalent GPU images, simultaneous distant origins and upload
reuse after rebasing. These checks establish only the exercised platform/backend;
unsupported native execution is reported as skipped, not passed coverage.

## Integrated workloads

`world_workflow` streams compiled query/navigation cells through scoped services,
plans a route, realizes autonomous locomotion at fixed ticks, publishes follow
camera samples, tracks zone crossings, prepares a cross-space transfer and restores
a durable checkpoint with a fresh epoch. It asserts zero live accounted storage
and owners after teardown. The fixture reports ticks, distance, cell publications,
CPU peak bytes and elapsed time. `world_streaming` separately stresses repeated
crossings, revisions, cancellation and retained leases; `world_store` injects
storage failures. Procedural/voxel and replication workloads remain separate
acceptance requirements for their respective domains. Exercise interactive camera
presentation through the scene host workload.
Settings/focus pause local input according to policy without stopping required
services or an online authority.

Use an owned automation path and reproducible configuration, not mouse timing as
the only oracle. Readiness, epoch/revision, entity state and resource counts are
assertions; screenshots supplement them. Reuse [scene workflows](../render/TESTING.md)
for camera and GPU observations. The existing Bistro benchmark remains a rendering
baseline; loading its whole model does not establish world-streaming coverage.

## Measurements and completion

Record source revision, build/platform, providers, content/generator identities,
seeds, bounds, budgets, traversal and cold/warm state. Report CPU preparation and
publication p50/p95/max, navigation latency/expansions, missed readiness deadlines,
bytes by owner/stage, resource uploads, pending retirement and frame timing.
Compare fixed workloads across strategies; do not combine idle time with active
cost or imply one platform's native results establish portability.

Acceptance requires bounded memory/work under pressure, preserved state on failure,
correct far-origin behavior and no unbounded boundary retry loop. Numerical and
latency thresholds belong to explicit fixture configuration. Deferred physics,
planetary frames, seamless portals, prediction/rollback and crowd avoidance have
no implied implementation coverage from these tests.
