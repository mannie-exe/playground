# Application composition

AppHost supplies platform lifetime and execution boundaries. Each application owns
its model, behavior and presentation composition through explicit scoped services.

## Ownership and composition

| Owner | Responsibility |
|---|---|
| Distribution/bootstrap | Factories, content resolution, initial entrypoint and shell policy |
| AppHost | Native platform/session, service pump, shared resource policy and transitions |
| App instance | Durable model, command validation, selected services and instance lifetime |
| Presentation attachment | UI/scene views, input assignment and view-local transient state |
| Optional modules | Typed content, worlds, datasets, physics, audio, sockets/HTTP or sessions requested by the app |
| Native adapters | Solver/device/transport state and completion/retirement guarantees |

Models accept domain commands without depending on SDL events or UI nodes. Views
project model state and invoke permitted actions. Rebuilding a view or replacing
a renderer preserves the model. Render parenting does not imply physics ownership.
Applications choose collections and update order; an ECS is not required.

## Definitions, instances and entrypoints

| Value/API | Contract |
|---|---|
| `AppDefinition` | Owned namespaced AppId, owned display metadata, supported entrypoints, requirements and content references |
| `AppInstanceId` | Fresh identity for a model lifetime; distinct from definition, content, window and activation generations |
| `AppLaunchProps` | Selected entrypoint, bounded versioned arguments, resolved content lock and optional restore reference |
| `AppRegistry::add / create` | Validate unique IDs, non-null compiled factories and requirements; construct a candidate without activating it |
| `AppHostProps` | Registry/content providers, initial launch, directories, optional services and shell/exit policy |
| `InstanceServices` | Explicit scoped access to selected facilities; no ambient application lookup |
| `IApp / AppContext` | Trusted native desktop adapter and owner-thread requests; not a serialized or binary guest ABI |

Reuse AppInfo and AppRegistry with owned stable metadata. Preserve prior persisted
app keys through a mapping at the settings boundary. App-specific launch settings,
including benchmarks, belong to the entrypoint's validated arguments. Registry
construction and content registration happen outside AppHost.

App IDs, entrypoint names, asset IDs and storage paths occupy different namespaces.
Resolve package-relative content against its retained mount/root, never the process
working directory. Private saves use an explicit application namespace and save ID;
multiple instances cannot silently become concurrent writers to the same save.
Session credentials and device handles are never launch-document content.

A single app can own multiple screens, worlds or workflows. These are not separate
host instances unless the author requests independent lifetime. The host does not
infer app boundaries from directories, views or scene hierarchy. A launcher is an
ordinary supplied application; mono-app startup selects its entrypoint directly.
Host return/exit behavior is distribution policy, not a hardcoded Menu identity.

## Instance and presentation lifetime

The baseline admits one resident native instance and one foreground presentation.
A staged replacement can coexist during preparation without running its behavior.
Concurrent background instances and multiple windows are future capabilities.

| Transition | Required behavior |
|---|---|
| Prepare | Resolve definitions/content, validate requirements and construct hidden candidate state within budget |
| Activate | Revalidate candidate; attach services/input/presentation at the owner boundary |
| Suspend | Cancel assigned input and pause declared model work; retain model and instance identity |
| Resume | Rebase local elapsed time, rearm input and resume declared work without replaying paused wall time |
| Detach presentation | Retire view subscriptions/resources without destroying the model |
| Dispose | Close admission/publication, stop scoped work and retire resources before releasing owners |

Instance scope survives presentation suspension. Presentation scope owns gestures,
view subscriptions and view-specific jobs. Separate ActivationLifetime tokens express
these lifetimes; an activation token is not a permission grant. Service policies
declare whether work continues while suspended. Networking receipt and audio control
can continue even when local simulation and rendering stop.

Replacement prepares all fallible app/content/presentation state possible before
revoking the old instance. Preparation failure leaves it active. Commit exchanges
validated bindings; reversible failures restore prior state. A failure after an
irreversible native action reports a fault/recovery state rather than claiming a
rollback. External effects during preparation require an explicit policy and cannot
be disguised as reversible construction. Only the selected instance receives input.

Disposal suppresses new/late model publication immediately. Native workers retain
captured inputs until retirement; closing a scope does not unload executing code or
undo completed external effects. Host teardown joins remaining native work outside
ordinary update/render callbacks. Actual dynamic code unload remains future.

## Transition handoff

Reuse the existing registry, IApp and ActivationLifetime machinery with these
explicit owner-thread phases; these names describe API operations, not a guest ABI.

| Operation/value | Required behavior |
|---|---|
| `prepareLaunch(props)` | Returns a retained request; validates registry/content/capabilities and prepares a dormant candidate under a bounded preparation scope |
| `PreparedLaunch` | Single-use candidate with registry/content revisions, expected resident instance and prepared model/presentation resources |
| `commitLaunch(candidate)` | Revalidates expectations at a host boundary; rejects stale candidates before revoking old input; returns Activated, Conflict, FailedPreservingResident or Faulted with a retained diagnostic |
| `cancelLaunch(request)` | Invalidates candidate publication; retires captured resources without activating behavior |
| `attachPresentation / detachPresentation` | Create/revoke a separate presentation lifetime and input assignment; preserve instance model and service scope |

Admit at most one replacement candidate. Further requests return Busy or explicitly
cancel the earlier request before taking its slot; retiring native resources remain
charged. Registry factories construct inert instances. No fixed update, connection,
audio playback or registered behavior starts during ordinary preparation.

Before commit, build all fallible reversible state. On commit revoke old input,
detach its presentation and exchange the resident mapping, then activate the new
instance's behavior. Activation that cannot be made reversible has a declared
fault path; do not resume an old instance after its irreversible disposal. Dispose
the old instance only after a successful reversible exchange, or under that fault
path. Completion delivery validates instance lifetime independently of presentation
lifetime. Renderer replacement revokes only the latter and reconstructs views from
the same model; instance network requests continue.

CP02 preserves existing catalog/resources, directories/settings, worker admission,
service pumping, gamepad/input routing and renderer notifications through injected
facilities. Splitting ownership is not permission to remove their behavior. A test
registry with two inert factories must exercise every transition without naming a
demo; repeat renderer replacement with an outstanding instance completion and a
stale view completion to prove the two lifetimes differ.

## Service access and state

AppContext delegates to explicit instance services and the presentation attachment.
Retain typed assets/resources, workers, services, completions, activity and window
requests where applicable. Headless models consume module APIs directly without
AppContext or SDL. Shared device settings remain host policy; app-local controls
cannot mutate another instance's resources or implicitly select global defaults.
Native code remains trusted despite these ownership conventions.

Immutable catalog content, saved composition, mutable checkpoints and executable
behavior are different artifacts. [Composition documents](COMPOSITION.md) wrap
asset references and selected module definitions. App state is versioned and migrated
before replacement. A scene save is not automatically a complete app checkpoint.
UI focus/capture and other transient state persist only through an explicit policy.

## Spatial-data applications

DatasetStore and its source/product/edit services work without a World. World apps
attach snapshots through DatasetBinding and coordinate authoritative publication.
Generators, interpretation and simulation rules remain registered application
providers. An optional network adapter validates commands before using the same
local mutation API; local storage does not require a session protocol.

[Physics](PHYSICS.md), [audio](../audio/README.md) and [sessions](NETWORKING.md) consume
owned values at declared boundaries. Contact callbacks do not directly create scene
nodes, play sound or transmit application messages. The app converts accepted
results into later commands. Missing capabilities fail explicitly.

## Input integration

AppHost routes host policy, application shortcuts, UI and after-UI actions through
InputMap. Each presentation declares claims and device assignments. ViewControlSession
owns relative-pointer engagement without owning a camera or physical subject.
Loss of focus/suspension cancels held actions; resume does not synthesize presses.
UI text/IME remains separate from action routing. See [runtime input](RUNTIME.md#input).

## Scene/game runtime boundaries

Scene2D/Scene3D contain presentation objects and resource references. World owns
spatial identity/state where selected; the app owns domain behavior. UI layout,
render hierarchy and physical attachment remain distinct. Local fixedUpdate is
optional. Ordered simulation may instead use a module runner serviced independently
of presentation. See [ordering](RUNTIME.md#timing-and-ordering).

## Launcher and demo organization

`playground_demo` registers existing demonstrations and focused verification views.
`playground_voxels` owns medical/scalar inspection. `playground_minecraft` owns the
Java Edition spectator protocol, mouse look/player movement and world interpretation. [Application workloads](../apps/WORKLOADS.md)
define their scope. Shared viewers may consume
reusable volume/scene APIs but their controls and workflows are application code.
See [demo scope](../render/DEMOS.md); named game screen contracts are not host rules.

## Accessibility boundary

The native presentation supplies UI semantics, focus and platform accessibility
through the UI adapter. App models expose meaningful values/actions; native scene
objects do not automatically become accessible controls. Headless tools do not
initialize accessibility or video services.

## Host integrations and interruption

Host-owned integrations have explicit host scopes. Instance operations use instance
scopes; view operations use presentation scopes. No operation silently outlives its
owner. Interruptions report reason and affected channels so apps can invalidate
measurements or gestures without resetting durable state.

## Future browser target: platform port, not just a renderer

Browser deployment, isolated executable apps, concurrent resident apps and multiple
windows require separate platform/service implementations and acceptance. Native
SDL/Jolt/FMOD/transport availability does not establish browser support. The future
[loader](DISTRIBUTED_APPS.md) projects its instances through a host-owned adapter;
it does not export AppContext, C++ ABI or native pointers.

## Verification

[Host workloads](HOST_TESTING.md) cover launch, failure, suspension, ownership,
ordering and teardown with fake services and concrete native adapters.
