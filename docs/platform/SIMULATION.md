# Spatial simulation

Simulation is an optional consumer and producer of [spatial data](SPATIAL_DATA.md).
The framework owns tick ordering, admitted working storage and publication;
applications own rules and physical meaning. A voxel field need not simulate.
Spatial kernels do not define a universal cellular automaton. Rigid-body execution
uses the separate [Jolt integration](PHYSICS.md), not a mutable voxel kernel.

## API and state

| Value/API | Shape and contract |
|---|---|
| `SimulationDefinition` | Kernel ID/version, input/output schemas, interpretations, fixed-step policy and reproduction capability |
| `SimulationRegion` | Dataset/epoch, bounded index region, active channels, required halo and authority owner |
| `SimulationInputs` | Coherent source snapshot, dependency/coverage stamps, ordered commands, tick and explicit delta |
| `SimulationLimits` | Active regions, jobs, visited samples, events, scratch, candidate bytes and publication limits |
| `SimulationKernel::step` | Produce an owned SimulationCandidate from immutable inputs and PreparationContext |
| `SimulationCandidate` | Expected input revisions/tick, bounded region patches/events, next activation state and measurements |
| `SimulationService::advance / attach / close` | Ordered preparation and model-boundary publication through shared runtime services |
| `SimulationState` | Stopped, WaitingForData, Admitted, Preparing, ReadyToCommit, Running, Overloaded or Failed, plus committed/target ticks |
| `RegionActivity` | Sleeping, Active or WaitingForCoverage with explicit wake reasons and last evaluated tick |

Kernel registration validates schemas, access sets, spacing and units before work.
Inputs include all required neighbor coverage. Unknown is never air or a no-contact
result. Each kernel declares authoritative versus presentation-only output and its
cross-target determinism/recording requirements. Fixed ticks alone imply neither.

## Ordering and activation

A step reads revision N and produces one candidate for N+1; it cannot write shared
neighbor buffers during worker traversal. Cross-region writes are bounded intents
with deterministic domain-defined arbitration before one coherent publication.
Conflicting edits/kernel outputs fail expected-revision validation and use an
explicit retry/rebase policy. No partial authoritative step or hidden recursive tick.
Private mutable buffers are admitted scratch; published snapshots remain immutable.

Sleeping regions wake on declared input/channel changes, commands, boundary changes
or timers. Simulation activity and render-product dirtiness are different state.
A settled simulation can still have a final unpresented geometry revision. Stopping
simulation cannot clear outstanding product invalidation. Scheduling coarse regions
is an explicit kernel policy; elapsed ticks cannot be skipped for rules requiring
exact ordered stepping.

Local interactive apps may use the existing bounded/dropped-time clock only when
their domain permits it. Ordered authoritative simulation uses an independent
clock/backlog policy: bound admitted work, report lag, slow domain time or reject
new work/terminate according to session policy. Never silently apply the interactive
clock's dropped-step behavior. Services and shutdown remain responsive under load.

## World, physics and multimedia

A world owner stages accepted dataset changes, movement/physics results and emitted
domain events at a declared model boundary. Events enter the next bounded command
batch; callbacks cannot recursively mutate solver bodies, datasets or scene nodes.
Required collision/navigation products participate in the edit/readiness barrier.
Actual movement results remain authority; a prepared navigation route is not a
collision-safe motion result. See [physics](PHYSICS.md) and [world bindings](WORLDS.md#dataset-bindings).

Presentation-only kernels may use Latest work policy over explicit time samples.
They do not accumulate mandatory fixed steps merely to animate a field. Music/audio
analysis arrives through bounded control-side snapshots with clock identity; audio
callbacks never generate meshes, acquire ledger tokens or await simulation work.
Captured samples allow reproducible playback/testing without requiring a live input.

## Native solver boundary

Immutable spatial kernels stage candidate patches; native physics stepping mutates
solver state and cannot claim the same rollback semantics. The model owner admits
one physics step, awaits its owned result without blocking UI/service processing,
and publishes actual poses only for a Complete tick. Compromised faults require
explicit region recovery. Dataset edits and collision activation cannot interleave
with a running native step. Region generation, active geometry versions and world
publication bind the two systems without merging their storage or schedulers.

## Verification

Supply deterministic reference kernels in tests/workloads, not a terrain game rule
engine in core. Verify ordered ticks, concurrent boundary intents, conflicts,
missing halos, final settled invalidation, bounded overload, cancellation and
teardown. Compare single-worker and varied scheduling for kernels advertising
reproducibility. A fake physics adapter establishes ownership/order only, not
physical accuracy. See [spatial workloads](SPATIAL_TESTING.md).
