# Saved composition

Composition documents describe reusable 2D/3D objects and registered attachments
without serializing executable code or choosing an application state container.

## Definitions and runtime state

| Value/API | Contract |
|---|---|
| `CompositionDefinition` | Schema version, definition ID, units/spaces, content lock, ordered object records and requirements |
| `CompositionObject` | Stable local ID, optional same-domain parent, transform and bounded typed attachments |
| `AttachmentRecord` | Registered type ID/version, required flag and validated data; no callbacks or native layout |
| `CompositionRegistry` | Allowlisted validators and preparation/realization adapters, supplied by the application |
| `CompositionCandidate` | Validated definition, resolved asset references and admitted prepared resources |
| `CompositionInstance` | Fresh instance identity, local-ID-to-runtime-handle mappings and owned service/resource bindings |
| `CompositionDiagnostics` | Document/object/attachment path, phase, expected type/limit and cause |

AssetCatalog remains the authority for immutable asset definitions. Do not add a
catalog variant for every application record. A document may be a BinaryAsset whose
schema is interpreted by its registered module. Reuse ModelAsset, Scene2D/Scene3D,
World/ReferenceFrames, DatasetBinding and physics body definitions rather than
creating parallel mesh, transform or world implementations.

Source definitions and instance checkpoints have different identities. Instantiating
the same definition twice does not alias mutable objects. Shared immutable meshes,
textures and source volumes keep shared ownership. Definition-local IDs resolve
only within the selected instance. Cross-document references name the target
instance explicitly; unavailable references are diagnostic failures.

## Baseline attachments

| Attachment | Data and interpretation |
|---|---|
| 2D renderable | Asset reference, transform and authored material values |
| 3D renderable | Model/mesh reference, selected subobjects and material overrides |
| Camera | Projection values and placement; controller behavior is supplied separately |
| Physical body | Shape recipe/reference, motion type, material, mass policy and filter |
| Dataset | Dataset definition/checkpoint reference and validated spatial placement |
| Behavior | Registered compiled provider ID/version, parameter schema and explicit object/resource references |
| Application record | Namespaced type/version and bounded opaque payload preserved by the document reader |

Parent transforms have explicit 2D or 3D semantics. Reject cycles and unsupported
cross-domain parenting; UI layout is not another transform hierarchy. World-bound
placements use declared spaces/units and precise positions. Physics dimensions
and presentation scaling remain separate. A body attachment does not infer
collision or mass from an object's visible mesh/material.

Behavior records configure known compiled implementations. Loading a document
cannot name arbitrary C++ symbols, run scripts or grant filesystem/network access.
Required unknown attachments reject activation. Optional unknown records survive
round-trip without executing or claiming their capability. Application save logic
must decide whether an uninterpreted record permits continued editing.

## Encoding and validation

Use versioned UTF-8 JSON for definitions, following [manifest conventions](MANIFESTS.md).
Use canonical decimal strings for identities/int64 addresses that would lose
precision in generic JSON readers. Numeric geometry values must be finite; unit
conversion and supported transforms are validated. Bound bytes, objects, depth,
references, strings and attachment payloads before instantiation. Parse once.

A minimal 3D definition has the following shape. The content lock is a logical
reference resolved by the supplied content environment, not a host filesystem path.

```json
{
  "schema_version": 1,
  "id": "example.inspection",
  "content_lock": "example.assets.v1",
  "spaces": [{ "id": "room", "units": "m", "up": "+Y" }],
  "objects": [{
    "id": "scenery",
    "space": "room",
    "position": [0, 0, 0],
    "orientation": [0, 0, 0, 1],
    "attachments": [{
      "type": "scene.model", "version": 1, "required": true,
      "data": { "asset": "example.assets:room" }
    }]
  }]
}
```

An absent parent denotes a space root; an absent scale is unit presentation scale.
Attachment schemas define defaults and unknown-field handling explicitly. Validate
asset kinds, references, dependency cycles and required module capabilities before
realizing native objects. No file-extension heuristic grants a behavior loader.

## Loading and publication

1. Read and validate a bounded definition and its exact content closure.
2. Prepare immutable assets and attachment candidates off the live scene.
3. Stage owner-thread scene, dataset and native bindings within a publication budget.
4. Revalidate expected instance/world/data versions and required readiness.
5. Activate at the model/adapter boundary and publish the complete instance mapping.

Adapters declare dependencies, thread affinity, failure and retirement behavior.
Preparation forms a bounded acyclic graph, not arbitrary code scheduling. Mutable
feedback happens in subsequent ticks. Native collision exchanges follow the physics
activation contract; they are not claimed as database transactions. Preparation
failure preserves the old instance. A compromised native activation faults the
candidate/region and suppresses mixed-state publication.

Replace whole instances initially. Automatic file watching, per-property live
reconciliation and a general editor are future work. An app may implement a bounded
explicit update using module edit APIs without changing document loading semantics.

## Checkpoints and exports

Export authored definitions without GPU/Jolt handles, runtime epochs, subscriptions,
input state or cached products. A checkpoint additionally captures app-owned mutable
records and exact dataset roots at one declared boundary. Reuse WorldStore where
world state is present; standalone apps use the same versioned storage principles
without manufacturing a World. Simulation continuation requires domain state and
pending work specified by that domain, not merely transforms.

Persist behavior provider identity and configuration separately from its state.
Migration validates a candidate before replacing the active instance. Unknown
provider identity preserves data and reports incompatibility. A prepared scene is
not proof that all required behavior or physics can resume.

## Verification

Round-trip a scene with shared assets, cameras, bodies and an optional dataset.
Verify stable local IDs, new runtime identities, duplicate/cyclic references, missing
required adapters, unknown optional records, unit/transform errors and migration
failure. Restore after renderer replacement and failed native preparation. Compare
semantic records and rendered/physical observations; byte equality is required only
for canonical encodings that promise it. UI documents, ECS storage and executable
loading are not prerequisites.
