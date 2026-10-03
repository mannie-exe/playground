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

## Future implementation

Solver selection/integration, collision cooking, rigid-body dynamics, character
motors, vehicle dynamics, cross-region physical interaction and root motion are
future. Camera obstruction remains separately [future](../render/CAMERAS.md#future-camera-obstruction).
Prediction/rollback requires additional state capture/replay contracts and is
also future; this interface makes no determinism or rewind guarantee.

Contract tests use a bounded fake adapter for command/result ownership, missing
capability, tick order, transfers and stale-region rejection. They establish the
integration boundary only, never physical correctness or collision safety. See
[world verification](WORLD_TESTING.md) and [locomotion](LOCOMOTION.md).
