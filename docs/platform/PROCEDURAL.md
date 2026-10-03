# Procedural spatial providers

Procedural generation is one [spatial data source](SPATIAL_DATA.md), alongside
loaded and live data. The framework defines requests, identity, bounds and
publication. Applications implement terrain, scalar fields, visual effects or other
generation algorithms. [Voxel storage/editing](VOXELS.md) and
[derived products](SPATIAL_PRODUCTS.md) do not require a procedural baseline.

## Definitions and API

| Value/API | Contract |
|---|---|
| `GeneratorDefinition` | Registered provider ID/version, schema, parameter schema, exact dependencies, supported targets and reproduction capability |
| `GenerationKey` | Provider/build compatibility, seed encoding where applicable, configuration/content digests, address/channels and output kind |
| `GenerationRequest` | DataRequest plus GenerationKey and bounded immutable neighbor inputs |
| `GenerationResult` | PrepareResult with generated source identity, dependency stamps, measurements and terminal outcome |
| `GeneratedEntityKey` | Stable domain feature identity mapped explicitly to persistent EntityId without scheduling-order dependence |
| `CellGenerator` | Application adapter implementing SpatialDataSource for generated regions; no second scheduler or mutable-world API |

Providers are trusted compiled implementations selected by an allowlisted ID.
Packs supply bounded validated parameters for registered recipes, not C++ names,
scripts, filesystem/network privilege or executable plugins. Unsupported recipes
fail before execution. Authoring/cooking and runtime invoke the same provider
contract; baked immutable data can replace generation without changing consumers.

## Reproduction and compatibility

A provider advertising reproducibility produces identical authoritative samples
for the same key on declared compatible targets regardless of scheduling/traversal.
Specify PRNG algorithm, seed encoding, coordinate hashing, integer overflow rules,
numeric operations and canonical serialization. Standard-library distributions or
unspecified floating-point behavior do not establish cross-platform determinism.
Golden fixtures establish supported compatibility; incompatible peers consume
baked/recorded authoritative samples or fail admission.

Live/nonreproducible providers use explicit sample identity and cannot claim that
a seed reconstructs a save. Derived visual products may declare separate numeric
tolerances without relaxing authoritative-data equality. Generator version,
interpretation version and mesh cooker version are independent identities.

Cross-region features have a canonical owner and bounded influence region.
Requests declare halos derived from immutable base inputs, not whichever neighbor
finished first. Dependency cycles and recursive unbounded generation are refused.
Missing neighbors produce bounded NeedsInput demand rather than assumed empty data.

## Work and persistence

Providers reserve output and scratch and cooperate with work/cancellation checks.
Oversized regions are explicitly tiled or refused. Cancellation, work exhaustion
or failure publishes no partial authoritative region. Native timeouts cannot safely
interrupt arbitrary provider execution. Framework budgets do not sandbox callbacks.

Baseline identity includes exact provider/configuration/content dependencies, not
just a seed. Edits retain overrides including set-to-empty values. Generator updates
require explicit migration, a retained compatible provider or baked baseline;
regenerating incompatible saved data without its edits is forbidden. Unknown
providers preserve stored references while reporting unavailable interpretation.
See [checkpoints](VOXELS.md#persistence-api-and-encoding).

## Verification

Reference providers cover a uniform/label grid and an explicitly timed scalar
field. They establish loading/generation interchangeability without mandating a
particular terrain algorithm. [Spatial workloads](SPATIAL_TESTING.md) verify
negative addresses, neighbor ownership, changed traversal/worker order, exact source
keys, captured live samples, bounded failure and incompatible saved baselines.
