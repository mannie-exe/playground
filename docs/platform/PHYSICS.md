# Physics adapter boundary

This document defines the integration contract for a working physics black box.
The concrete solver, body implementation and physics-backed movement are **future**.
World coordinates, scheduling, queries and movement remain usable without one;
an absent provider reports Unavailable, not an empty successful simulation.

## Ownership and API

| API | Contract |
|---|---|
| `PhysicsCapabilities` | Supported shapes, queries, bodies, origin shifts and local numerical limits |
| `PhysicsRegion` | SpaceId, bounded working origin/extent, region generation and fixed-step policy |
| `PhysicsCommands` | Bounded tick-targeted creation/removal, forces and movement requests with entity mappings |
| `PhysicsAdapter::step` | Consume validated commands and a declared fixed interval; publish one owned result |
| `PhysicsStepResult` | Tick/region generation, actual poses/velocities, contacts and terminal diagnostics |
| `PhysicsQueries` | Snapshot-scoped overlap/raycast/sweep with declared supported geometry and completeness |

World authority owns entities; the adapter owns solver bodies and maps private
handles to checked entity identities. Inputs and outputs use meters, seconds,
kilograms and radians, with explicit axis/rotation conversion at the adapter.
Render scale, material opacity and scene parenting do not define mass, colliders,
collision filters or joints. Collision shapes and layers are authored separately.

Movement requests describe intent, not achieved displacement. Consume actual
results once on the simulation owner before publishing WorldSnapshot. Kinematic
fallback is an explicit app policy without gravity, contacts or collision safety;
it must not silently replace an app's required physics capability.

## Frames, clocks and readiness

Each region chooses a bounded numerical frame independent of view origins.
Shift origins only at an adapter-safe step boundary, preserving world pose,
velocity, constraints and contact identity. Coordinate rebasing is not motion.
A provider unable to shift reports that capability before accepting the operation.
Translating a numerical origin is distinct from simulating a rotating frame;
moving platforms use explicit body/frame motion, not hidden fictitious forces.

Required collision data and neighboring coverage must be ready before stepping
into a region. Missing data never means free space. Multiple regions need an
explicit body/constraint transfer policy; unconstrained cross-region joints are
not implied. Teleports clear/rebuild history and contacts under a declared policy.

The runtime supplies fixed ticks; the adapter neither sleeps nor pumps services.
Substeps and internal worker execution have bounded policies. Fixed intervals do
not promise deterministic cross-platform simulation. Results identify dropped or
failed work; overflow must not silently omit authoritative contacts. Contact
callbacks cannot mutate world/scene ownership while the solver traverses bodies.

## Spatial collision products

Collision preparation and revision/readiness integration are implementation
requirements independent of solver selection. `CollisionProfile` declares occupancy
channels, interpretation, shape recipe, spacing/units, filters and output limits.
`CollisionRecipe` produces a typed immutable `CollisionProduct` with dependency
stamps, bounds, geometry encoding and required adapter capabilities. The baseline
recipe supplies bounded occupied-cell boxes in local coordinates; optional merging
must preserve occupied volume and profile boundaries. This is geometry preparation,
not a claim that rigid-body dynamics exists.

`PhysicsAdapter::stageCollision(product, placement, regionGeneration)` returns an
owned inactive candidate or Unsupported/Failed. `CollisionActivation` carries the
candidate, expected old binding and region/tick. `activateCollision` validates and
exchanges at a solver-safe model boundary; `retireCollision` retains native uses
until the adapter acknowledges release. An adapter advertises whether atomic
replacement is supported. RequireProducts edits need that capability and completed
staging; absent capability cannot become a successful no-op activation.

A failed stage/validation preserves the previous active binding. The final exchange
must be non-failing after validation, or the adapter must provide a proven rollback
protocol before claiming atomic replacement. Queries/results identify the active
source revision. After an authoritative geometry edit, stale collision does not
count as current; the app stops/waits or explicitly keeps a coherent older
simulation generation. Unknown coverage is not free space. Unequal grid spacing
is validated independently of renderer support.

Fake activation adapters test ownership, staging failure, safe-boundary exchange
and delayed retirement. Real solver contact/sweep behavior requires separate tests.
See [spatial products](SPATIAL_PRODUCTS.md) and [voxel commits](VOXELS.md).

## Future implementation

Solver selection/integration, solver-specific native cooking, rigid-body dynamics, character
motors, vehicle dynamics, cross-region physical interaction and root motion are
future. Camera obstruction remains separately [future](../render/CAMERAS.md#future-camera-obstruction).
Prediction/rollback requires additional state capture/replay contracts and is
also future; this interface makes no determinism or rewind guarantee.

Contract tests use a bounded fake adapter for command/result ownership, missing
capability, tick order, transfers and stale-region rejection. They establish the
integration boundary only, never physical correctness or collision safety. See
[world verification](WORLD_TESTING.md) and [locomotion](LOCOMOTION.md).
