# Physics and Jolt integration

Playground owns physics identity, commands, scheduling, accounting and world
publication. The Jolt adapter owns solver objects and native execution. Baseline
rigid-body physics, collision queries, voxel collision replacement and a bounded
character motor are implementation requirements. Applications can omit physics;
required missing capabilities return Unsupported, never successful empty results.

## Ownership and state

| Value/API | Shape and contract |
|---|---|
| `PhysicsWorldDefinition` | World/SpaceId binding, numerical origin, gravity, fixed interval, layers and PhysicsLimits |
| `PhysicsRegion` | Bounded origin/extent, generation, required collision coverage and committed tick |
| `PhysicsLimits` | Bodies, body pairs, contacts/constraints, commands/events, query work, jobs, scratch and candidate/native storage |
| `PhysicsCapabilities` | Supported shapes/motion types, queries, mappings, origin shifts and coherent replacement |
| `PhysicsBodyDefinition` | Entity binding, pose, Static/Kinematic/Dynamic mode, CollisionAsset, mass/inertia policy, physical material and collision filter |
| `PhysicsBodyHandle` | Adapter-world identity/epoch plus slot generation; native BodyID stays private |
| `PhysicalMaterial` | Stable ID/revision, validated friction/restitution and explicit combination policy |
| `CollisionAsset` | Immutable source geometry, dependency stamps, cooking profile, backend/build compatibility and retained storage |
| `PhysicsCommandBatch` | Target region generation/tick, ordered command IDs and bounded creation/removal, movement, force and replacement requests |
| `PhysicsStepRequest` | Expected committed tick, fixed delta, command batch, coverage stamps and execution admission |
| `PhysicsStepResult` | Region generation, completed tick, actual body states, contacts, active geometry versions, validity and diagnostics |
| `PhysicsStepValidity` | Complete, RefusedBeforeStep or Compromised; identifies whether native mutation may have occurred |
| `PhysicsRegionState` | Idle, WaitingForCoverage, Queued, Stepping, ReadyToPublish, Faulted or Closed |
| `PhysicsDiagnostics` | Counts/capacities, queue/step/publication duration, scratch peaks, capacity failures and pending replacements |

World authority owns entity identity. The adapter maps entity handles to private
bodies and retains that mapping through callback/result retirement. Body-slot
reuse cannot redirect an old contact or command to a different entity. A standalone
fixture may supply its own entity bindings without AppHost, SDL or a renderer.

Positions/velocities use declared SpaceId and meters/seconds; mass uses kilograms,
angles radians and +Y up. Conversion to the solver's bounded numerical frame is
explicit. Camera origins and rendering precision never dictate solver origin.
Render hierarchy/scale/opacity do not define mass, collision, joints or filtering.
Mass/inertia calculation versus explicit values is a validated body policy.

## Adapter and service API

| API | Contract |
|---|---|
| `PhysicsAdapter::capabilities` | Validate requested region/body/shape/query support before admission |
| `PhysicsAdapter::prepareShape` | Cook an admitted immutable CollisionAsset into a retained native shape candidate |
| `PhysicsAdapter::step` | Solver-domain operation; consume one admitted request and return one owned result |
| `PhysicsService::submit / cancel / poll / forget` | Owner-thread bounded requests and retained results; no borrowed native handles |
| `PhysicsService::advance / attach / close` | ServiceScope integration, completion wakes and publication; close rejects late results |
| `PhysicsAdapter::readScope` | Lexical access to one identified completed step with stepping/replacement excluded |
| `PhysicsService::query` | Queue a bounded ray/overlap/sweep against a required tick or explicit latest-completed policy |
| `PhysicsAdapter::shiftOrigin` | Change numerical representation at a safe boundary, preserving world state |

Baseline native shapes include spheres, boxes and capsules for dynamic/kinematic
bodies and bounded static voxel compounds. Shape cooking validates positive finite
dimensions, scale, convex radii and output size. Collision layers/filters and
friction/restitution are explicit. Known-empty collision is different from missing
coverage. Unsupported geometry/motion combinations fail before body activation.

Commands target a declared tick and handle generation. The owner validates entity
authority and immutable input; the solver domain validates native capacity/state
before stepping. Creation results return Playground handles, not Jolt IDs. Removal
excludes subsequent simulation before destruction and retains event identity until
consumers finish. Forces, impulses, kinematic targets and teleports have distinct
operations; teleport marks discontinuity rather than creating artificial velocity.

At most one step is outstanding per physics world. A queued request can be canceled
before execution. Once native stepping starts, cancellation suppresses obsolete
publication but cannot undo or forcibly interrupt the solver. Retain execution
resources through completion; never run the next step concurrently. Owner commands
and dataset activation cannot overtake an in-flight physics step. Local pause stops
new admission; an already admitted step reaches its normal commit boundary before
pause is acknowledged. Closing/replacing the world instead invalidates publication
and retires native work. An online authority does not pause on local focus/settings.

## Step validity and recovery

A physics update is not an atomic dataset transaction. Jolt's contact/pair capacity
errors can mean contacts were ignored after native state changed. Any such error,
incomplete authoritative event capture or ambiguous native failure produces
Compromised, not a complete tick or an automatically retryable request.

RefusedBeforeStep guarantees no step mutation and permits corrected resubmission.
Compromised faults the region, stops authoritative progression and preserves the
last published complete world state. Keep the native instance quarantined until
explicit recovery or teardown; no query may advertise that instance as the last
valid tick. Do not replay the same step against its already-mutated state.

`PhysicsRecoveryRequest` selects rebuild from an application-owned complete
body/geometry snapshot, restore from a supported validated checkpoint, or close.
Recovery increments region generation and marks pose discontinuity. Rebuild resets
solver contact/warm-start history and is not deterministic rollback. Record failure,
recovery policy and lost continuity. A failed recovery cannot silently resume.

Authoritative ticks are fixed and ordered with bounded backlog. Sustained overload
reports lag and follows explicit slow-time/admission/termination policy; never
inherit the desktop clock's silent dropped-step policy. Local interactive domains
may opt into a documented dropped-time clock independently.

## Queries, events and movement

`PhysicsReadScope` identifies region generation, completed tick, numerical origin
and active CollisionAsset revisions. It is solver-domain lexical access, not a
retained historical snapshot or a public body pointer. Bounded queued queries run
between steps/replacements; caller-held handles cannot stall stepping indefinitely.
Results are immutable copies with completeness, represented versions and hit IDs.
Retaining a WorldSnapshot does not retain a historical solver query state.

Raycasts, overlaps and shape sweeps bound visited/output work and filtering. Miss
requires complete relevant coverage; unsupported shape/query, initial overlap,
missing coverage, truncated output and exhausted work are explicit outcomes.
Immutable dataset selection/navigation products remain separate query providers.

Jolt callbacks may run on solver workers. Copy bounded contact/activation data into
preallocated buffers; do not invoke application code, acquire the shared ledger,
perform I/O or mutate live worlds/datasets there. `PhysicsContactEvent` carries tick,
entity/body generations, collision source/subshape identity, event kind and contact
geometry when available. Removed-contact mapping remains retained long enough to
resolve identity. Owner delivery happens after step validity is known. Buffer
exhaustion cannot silently discard authoritative events and call the step Complete.

A bounded character motor consumes LocomotionRequest, capsule/profile configuration
and current collision coverage; it returns achieved pose/velocity, grounding and
blocked status. Player and navigation intents share this path. Slope, step height,
support motion, overlap recovery and motion limits are explicit profile values.
No camera smoothing or desired velocity overwrites the achieved result. See
[locomotion](LOCOMOTION.md#physics-movement-realization).

## Collision preparation, editing and residency

`CollisionProfile` names occupancy/label interpretation, mapping, filters, physical
surface profiles, cooking recipe and limits. `CollisionRecipe` prepares immutable
local geometry through the shared product pipeline. Baseline occupied-cell boxes
may merge only when occupied volume and physical profile boundaries are preserved.
One occupied sample does not require one solver body. Chunk compounds remain bounded
and fragmented input can be refused or explicitly partitioned before activation.

`stageCollision(product, placement, regionGeneration)` creates an inactive candidate
and returns owned `CollisionActivation` with expected old binding, source versions,
body-intersection policy and intended activation tick. The final replacement occurs
between steps with external queries excluded. Stage native capacity first, validate
again, exchange bindings, then publish the matching dataset/world view. Individual
Jolt operations are not a cross-system database transaction.

The final exchange must be non-failing after validation or support a proven rollback
before claiming coherent replacement. Stage/precondition failure preserves old
bindings. Unexpected native failure during exchange faults the region and suppresses
mixed-state publication; it cannot claim a successful rollback it did not perform.
`retireCollision` releases old shapes/bodies only after outstanding native/query/event
uses finish. RequireProducts edits include this activation barrier.

`BodyIntersectionPolicy` is RejectOverlap, AllowSolverResolution or RelocateByApp.
Evaluate against staged geometry and current body poses at the activation boundary.
Removing support and adding solid volume through a body have distinct checks.
RelocateByApp requires validated explicit destination commands in the same activation;
never silently teleport or assume the solver can resolve arbitrary deep overlap.
AllowSolverResolution reports that penetration resolution is subject to solver
limits, not guaranteed immediate separation. Defaults are explicit in app profiles.

Physics demand pins collision independently of camera visibility, including bounded
motion prediction/sweep coverage. Missing mandatory terrain pauses/refuses that
region's step or follows an explicit coherent retained-generation policy. Streaming
cannot unload support under active bodies merely because it is offscreen. Pinned
working sets that exceed limits report failure instead of evicting live shapes.

Origin shifts run between steps with query/event mapping protected; preserve world
poses, velocities and contacts where supported. Multiple regions remain independent;
ordinary transfers require prepared destination coverage and a coordinated handoff.
Cross-region forces/joints are not implied by compatible coordinates.

## Execution, allocation and build boundary

The Jolt adapter is a dedicated module with no JPH types in public world/dataset,
wire or save values. Pin its source version and integration build options through
CMake's existing dependency conventions. Native cooking cache keys include Jolt
version/build profile, shape recipe, source digest, units/scale and physical profiles.
Changing solver compatibility recooks native data; it does not rewrite voxel edits.

Data loading, generic collision preparation and product work use the shared Executor.
Stepping uses a budgeted PhysicsExecutionDomain with Jolt's supported job/barrier
machinery. Solver steps run off the UI thread; the model owner consumes completed
results without waiting in update/render. Jolt jobs can create dependent work while
a barrier waits, so a naive adapter to a saturated independent-job pool is unsafe.
A private solver job domain is permitted with an explicit shared CPU-worker budget;
no second unconstrained hardware-sized pool per region. Tests cover multiple hosts,
world closure and pending workers. Runtime worker-budget changes wait for safe
quiescence, never destroy a scheduler beneath a running barrier.

Reserve fixed solver capacity, temporary scratch, cooked shapes, body metadata,
callback buffers and old/new activation overlap before work where controllable.
Separate tracked allocations, reservations and estimated dependency-private storage.
Process-wide Jolt initialization/allocator hooks have one coordinated lifetime;
per-world accounting cannot be implemented by replacing global hooks per host.
Allocation failure after admission remains possible. Track pending retirement and
keep native resources alive until execution-domain acknowledgement.

## Verification and scope

[Spatial verification](SPATIAL_TESTING.md) includes real Jolt body/contact/query,
collision-edit, character/navigation and pressure/recovery workloads. Fake adapters
remain useful for controlled failure injection but cannot establish physical behavior.
Reuse shared test executables/host infrastructure and existing licensed assets where
useful. No particular user-facing physics demo or menu scene is specified.

## Future advanced physics

Vehicle physics, elaborate joints/articulations, destruction/fracture, soft bodies,
cloth, fluid solvers, ragdoll systems, root-motion realization and cross-region
physical coupling are **future**. Prediction/rollback and deterministic lockstep
remain [future session work](NETWORKING.md#future-session-capabilities).
Camera obstruction remains separately [future](../render/CAMERAS.md#future-camera-obstruction).
Baseline rigid bodies, queries, collision preparation/activation and character
movement are not part of these deferrals.

Jolt references: [body lifecycle, callbacks and state restoration](https://github.com/jrouwe/JoltPhysics/blob/master/Docs/Architecture.md),
[job dependencies/barriers](https://github.com/jrouwe/JoltPhysics/blob/master/Jolt/Core/JobSystem.h),
[update capacity errors](https://github.com/jrouwe/JoltPhysics/blob/master/Jolt/Physics/EPhysicsUpdateError.h).
Adapter behavior must be verified against the pinned version; these references do
not establish that all Jolt features are exposed or tested by Playground.
