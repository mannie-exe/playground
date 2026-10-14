# Design-freeze boundary

A documentation freeze stabilizes module responsibilities and observable contracts
so implementation can proceed without inventing cross-system ownership. It does not
claim that documented APIs exist, establish binary compatibility or certify passing
native integration. Future contracts remain proposals, not frozen requirements.

The four-target design is a freeze candidate at the engine boundary. Minecraft's
exact 26.3 wire conformance and rendered-content coverage still require application
validation. Those are explicit downstream obligations, not reasons to introduce
Minecraft state or an editor framework into the runtime core.

## Stable scope

| Boundary | Baseline commitment | Outside this freeze |
|---|---|---|
| Build | `playground_lib`, `playground_demo`, `playground_voxels`, `playground_minecraft`; neutral direct-module linkage and explicitly selected adapters | One mandatory all-feature engine binary |
| Host | Injected registry/bootstrap, one resident app, one foreground presentation, staged replacement, independent instance/presentation scopes | Downloaded executable apps, concurrent background apps, hot code loading and multiwindow hosting |
| Runtime | Bounded admission, retained outcomes, owner publication, identity/generation checks, shared accounting and native retirement | Universal ECS, event bus, global transaction or mandatory shared clock |
| Assets and saves | Typed immutable assets, explicit acquisition, registered composition, checkpoints and versioned source/product identity | Universal app serialization, arbitrary behavior execution from files |
| Spatial/streaming | Precise coordinates, finite manifests and bounded open-ended discovery, independent product readiness and leases | Infinite numeric precision, unlimited working sets or prescribed chunk/octree layout |
| Sampled data | Bounded storage/edits, `.vox` and declared NRRD import, exposed-face products and scalar slices | Minecraft palettes in core, terrain rules, DICOM/NIfTI, smooth extraction and volume ray marching |
| Simulation/physics | Bounded regional kernel scheduling; optional Jolt bodies, queries and safe collision replacement | Character motors, general fracture, fluids, rollback and cross-region physical coupling |
| Networking | Optional TCP/UDP, HTTPS, reliable/unreliable message transport and independent authority-session library | Mandatory spatial replication, discovery/hosting services, generic game prediction |
| Audio | Optional FMOD Core/Studio lifecycle, clip/bank/event control and spatial mapping | A universal media-editing clock or mandatory audio initialization |
| UI/rendering | Existing retained UI and selected rendering contracts, application-owned controllers and scene composition | General workspace/editor helpers, timeline/browser/transform tools and general nested viewport composition |

The core supplies lifecycle and communication primitives. Enabled modules have
required semantics; application targets select capabilities and supply their own
models, policies and codecs. A `.vox` importer or scene renderer need not become a
core dependency merely because two apps use it. Sharing a domain adapter later is
an optional library extraction, not a reason to freeze its rules into AppHost now.

## Construction and ordering

Distribution code selects module providers, registry, content roots and initial
entrypoint. The host constructs shared accounting/execution/service facilities,
then selected native providers, then an instance candidate with explicit scoped
access. Validate required capabilities/content before activation. Disabled optional
features remain unavailable; a required feature fails activation rather than
silently selecting a different transport, renderer or physics behavior.

A candidate can validate and prepare local resources without starting app behavior.
Connection/authentication and other external effects start after activation unless
the entrypoint explicitly declares an effectful preparation policy. Input is assigned
once when presentation attaches. App instance services outlive presentation rebuilds;
view-only jobs and gesture state do not. Startup failures retire partial construction
through the same ownership rules as normal closure.

The host drains bounded service publications before local movement/simulation and
presentation. Each domain defines its actual mutation boundary: ordered session
commands, Minecraft protocol transitions, dataset edit commits or completed physics
ticks. There is no implied atomic commit across them. Applications translate accepted
events into later typed commands. Physics and native I/O have accounted execution
domains; neither blocks the UI loop or waits recursively on the shared content pool.

Dispose in dependency order: revoke input and close admission/publication, cancel
scoped demand, retire view/model subscriptions, keep native operation and frame
leases until actual completion, then release providers and shared execution. Service
closure finishes logical cleanup; native retirement may continue under host-owned
state. A timeout reports incomplete retirement and preserves memory safety. It never
turns an in-flight native operation into a completed one. Process teardown may join
outside ordinary update/render; app replacement remains responsive and bounded.

## Contract review criteria

Every new cross-owner request must answer the following questions in its owning
contract, rather than rely on application convention:

1. Who owns the state, and which thread may call or mutate it?
2. Which IDs, epochs, revisions, units and numeric limits validate the request?
3. What is admitted, what is retained, and what happens under pressure or failure?
4. At which boundary does the result become observable, and can it be superseded?
5. What does cancellation actually prevent, and what memory/native work must retire?
6. Which independently enabled capability supplies it, and what test establishes it?

Reuse existing ServiceScope, ActivationLifetime, Executor, CompletionQueue,
ResourceLedger, typed assets, snapshots and resource handles. Proposed API tables
specify values and behavior; exact namespaces, overloads and private containers can
be reconciled with source without changing those guarantees. Wire/file encodings
require separate explicit versions and fixtures; raw C++ layouts are never formats.

## Acceptance and remaining evidence

| Evidence | Required completion |
|---|---|
| Documentation consistency | All targets, optional/future labels, links, ordering and failure semantics agree; examples validate |
| Dependency closure | Neutral/headless linkage plus each target separately; disabled adapters absent from fetch/link/startup/package |
| Host wiring | Registry injection, replacement failure, presentation rebuild, pause, full queues and shutdown with native work outstanding |
| Native providers | Jolt contacts/queries, FMOD lifecycle with supplied SDK, TCP/UDP and message loopback, HTTPS failure and cancellation fixtures |
| Spatial workload | Bounded source/product/device residency, stale generation rejection, negative coordinates, edits/save/reopen and expanding demand |
| Application compatibility | Exact Minecraft 26.3 codecs/registries and server traces; matching resources and explicit visual coverage; native volume sampling compared to source |

The dependency integration must pin source/SDK versions and build options before
native acceptance. Referenced vendor documentation selects API families; it is not
a lockfile. Unsupported target/SDK combinations fail explicitly. The existing volume
packaging/reference reader establishes dataset provenance and reconstruction only;
it cannot establish native viewer or GPU streaming behavior.

### Checkpoint gates

The [implementation ledger](../../IMPLEMENTATION.md#implementation-checkpoints)
owns execution order and review receipts; specs own intended behavior. Before a
checkpoint begins, reconcile its planning PR with the current owning specs. Its
acceptance includes successful paths, refusal/cancellation, stale publication and
retirement, plus actual native evidence for enabled providers. A fake provider is
not a substitute for that checkpoint's native acceptance.

CP01 freezes selection and installed dependency closure. CP02 freezes injected
host transitions. CP03 freezes composition activation and durable root behavior.
CP06 must validate the declared Jolt allocation/query limits against its pinned
build. CP04 and CP10 lock their file/wire codecs with golden fixtures before
persisting saves or claiming session compatibility. CP05a covers mesh/texture
streaming as well as sampled products. CP12 first resolves exact protocol evidence;
CP11 requires a supported FMOD SDK. These are implementation gates with specified
outcomes, not unspecified runtime ownership. Native allocation, OS scheduling and
third-party callback execution are not hard-preemptible merely because admission
and result retention are bounded.

The [Minecraft compatibility profile](../apps/MINECRAFT.md#compatibility-profile)
records the concrete remaining protocol issue: the inspected reference dataset has
a release number but no 26.3 packet schema. Local class signatures establish several
payload shapes, not complete encoding or handshake behavior. Building and validating
that app codec is required before claiming compatibility. Rendering acceptance also
names supported visible classes; a terrain-only result cannot be called full parity.
These gaps do not change the engine's stream, product, input or publication boundaries.

Once consistency review passes, this is sufficient to freeze the **engine design
scope** and move to implementation. It is insufficient to freeze an unvalidated
binary API or declare all application behavior complete. Reopen the design only for
a demonstrated contradiction, missing ownership/failure rule, or deliberately approved
scope change. Additive application experiments and explicitly future UI work proceed
without expanding baseline engine acceptance.
