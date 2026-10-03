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

## Integrated workloads

The small streaming fixture combines authored cells, a generated voxel boundary,
an autonomous agent and persistent edits. A scripted traversal crosses boundaries,
revisits evicted cells, prepares a transfer and replaces the world while work is
pending. Exercise both a renderer-free authority and interactive presentation.
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
