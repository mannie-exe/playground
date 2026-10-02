# UI documents and editor (future)

This future contract defines runtime UI documents and editor authoring. Asset
packs carry documents as data without becoming executable apps. Combobox and
Autocomplete remain future controls and cannot appear as supported loader types
until their control contracts are implemented.

## Definitions, instances and realized state

A UIDocument describes authored node types, stable document IDs, props, layout
placement, theme references, bindings and motion specifications. A UIDocumentInstance
owns a realized Node tree, connections and document-ID-to-NodeHandle mapping.
Preparation caches, native assets and layout results remain runtime-owned.
An editor selects by document ID, never by a persisted NodeHandle or pointer.

Serialize authored values, including optional inheritance and explicit overrides.
Do not write measured rectangles, resolved colors, derived scrollbar gutters,
hover/focus/capture state, active callbacks, GPU resources or playback handles.
Theme definitions use existing semantic metrics, palettes and typography roles;
font references use package/family/face identities and style/weight selectors.
A derived value stays derived after loading, theme replacement or density change.

## Text format and validation

Use strict UTF-8 JSON for UI/theme documents. Its nested objects and arrays
fit composition and common editor/schema tooling; this is not a claim that JSON
is intrinsically faster than TOML or a binary format. Keep TOML for small project,
package and settings manifests to reuse the existing convention. The loader uses an explicitly selected, maintained parser dependency.
[JSON specification](https://www.rfc-editor.org/rfc/rfc8259).

Parse once into immutable validated definitions, not per frame. Require schema
version and supported type versions; reject duplicate keys, malformed UTF-8,
unknown required properties/types, duplicate IDs and invalid numeric ranges.
Bound total bytes, strings, nodes, depth, references, bindings and keyframes before
constructing a live tree. IDs are strings; do not rely on arbitrary 64-bit integers
round-tripping through floating-point JSON consumers.

Versioned migrations operate on definitions and report source/property paths.
A stable writer makes diffs predictable. If measured startup cost later warrants
a binary cache, key it by document digest, schema/compiler version and relevant
feature set; retain the text source as authority and validate the cache too.
[CBOR](https://www.rfc-editor.org/rfc/rfc8949) is one candidate, not a required
second format or an interchangeable C++ memory dump.

Document shape:

```json
{
  "schema_version": 1,
  "root": {
    "id": "join-panel",
    "type": "ui.vstack",
    "children": [
      { "id": "title", "type": "ui.text", "props": { "text": "Join session", "role": "heading" } },
      { "id": "join", "type": "ui.button", "action": "session.join", "children": [
        { "id": "join-label", "type": "ui.text", "props": { "text": "Join" } }
      ] }
    ]
  }
}
```

## Construction and behavior contracts

| Primitive | Responsibility |
|---|---|
| UITypeRegistry | Explicit allowed stable type IDs, schema/version, prop validation and trusted factory |
| UIDocument | Immutable authored tree and references; no live node pointers |
| UIBindingSchema | Typed readable values and permitted actions supplied by the app |
| UIDocumentInstance | Instantiate, own connections, map document identities and release resources |
| DocumentDiagnostics | Source document, node ID, property path, phase and cause |
| DocumentEdit | Validated reversible edit transaction for editor undo/redo |

Initially register supported built-in controls and explicit trusted extensions;
there is no automatic C++ reflection or loading of arbitrary type constructors.
Bindings read typed app values and invoke named app commands, not unrestricted
property setters or strings evaluated as code. App logic supplies behavior through
the binding schema. Future isolated apps use the brokered command surface.
Dynamic collection items have stable domain keys distinct from template node IDs.

Load/validate and resolve assets off the live tree; instantiate and attach on the
owner thread. Failure preserves the previous instance. First support atomic
replacement with a documented transient-state reset, keeping the app model intact.
Later keyed reconciliation may preserve compatible focus/edit state explicitly;
never infer that two changed controls share identity from array position. Edits
to active IME/capture state need the same cancellation contract as C++ mutations.

Motion documents contain timing, easing and property paths, not MotionBindings
or callbacks. Instance construction binds only allowlisted properties to live
handles. Async content uses existing request generations and AsyncResource/AsyncView;
loading a document cannot change completion, lifetime or accessibility semantics.

## Editor scope and acceptance

A future editor uses the same type registry, validation, layout, themes, motion
and renderer as runtime preview. Schema metadata supplies property panels and
permitted child/placement relations. Undo records definition edits; save is an
atomic source update. Preview failure does not overwrite the last valid document.
Untrusted project preview uses the same isolation boundary as playback.

Start with a representative Settings-like form built both in C++ and from a
document. Compare layout, screenshots, keyboard/focus behavior, accessibility,
font variants, inherited/explicit-zero theme values and narrow/high-contrast
layouts. Test invalid references, limits, migration, failed replacement, stale
async results and undo/redo. Measure parse, validation, realization and warm
reconstruction separately. A visual editor follows a proven reader/writer; it
is not necessary for asset packing, audio or networking.
