# Host and module verification

Host acceptance proves ownership and ordering across independently optional modules.
Tests identify the contracts exercised; native behavior and fake-service sequencing
are separate evidence.

## Lifecycle workloads

| Workload | Required observations |
|---|---|
| Bootstrap | Mono-app and launcher distributions select injected entrypoints/catalogs without host source changes |
| Registry | Duplicate IDs, null factories, incompatible requirements and invalid arguments fail before activation |
| Replacement | Candidate preparation failure retains the old app; successful commit assigns input once and retires old scopes |
| Suspension | Input cancels, model identity persists, local time rebases, permitted services progress without painting |
| Presentation rebuild | Renderer/view replacement preserves model, content identity and instance-owned services |
| Disposal | Late completions are rejected; captured native work retires before resource or code release |
| Scope ownership | Host, instance and presentation operations have distinct lifetimes and no ambient current-app access |
| Saved composition | Resolve, stage, activate, save and restore through the same definitions and module adapters |
| Headless use | Runtime, storage, datasets, physics and sessions operate without video/UI initialization |
| Optional dependencies | Disabled adapters do not initialize or fetch native libraries; unsupported requests fail explicitly |

Construct the same definition in two sequential instances to detect retained global
mutable state. Simultaneously active/background instances remain future acceptance.
Tests may retain an old immutable snapshot or replacement candidate without making
it an active app. Use isolated injected settings/content/save roots.

## Ordering and fault injection

Record request, admission, native start/completion, model publication, presentation
submission and retirement separately. Saturate result and wake paths; notification
loss must not lose retained outcomes. Inject stale epochs, superseding requests,
cancellation, allocation failure and shutdown during preparation. Retain old/new
resources in measurements until actual retirement.

Physics tests delay a step while input, services and presentation remain responsive.
No later authoritative tick or collision activation overtakes it. Network tests
continue receipt during local pause and apply changes at the model owner boundary.
Audio tests continue control/update deadlines without a paint request. Contact and
transport callbacks stage owned records; they do not recursively mutate app/UI state.

A cross-module fixture loads a composition, advances physical objects, publishes
poses to rendering and emits an application-chosen audio cue/network state record.
Enabling one optional adapter must not require the others. Test each independently,
then combined contention with declared CPU/work budgets. No universal shared tick
or transaction is implied by this fixture.

## Native and performance checks

Use real Jolt contacts/queries, FMOD playback/bank lifecycle when its SDK is enabled,
and separate-process GameNetworkingSockets loopback. A silent audio adapter or an
in-memory transport verifies ordering only. Report missing SDK/device coverage as
skipped; it cannot satisfy native acceptance for that adapter.

Reuse activation, service, host transition, input and scene harnesses. Extend them
with injected registries/providers instead of duplicating a host loop in tests.
Source revision, enabled features, dependency versions, platform, data and budgets
accompany results. Measure service queue age, update/commit latency, native retirement,
accounted bytes and idle wake/paint counts. Timing limits require identified hardware;
semantic checks use revisions, outcome sequences and ownership assertions.
