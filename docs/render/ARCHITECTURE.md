# Rendering architecture

This document explains dependency direction, ownership and runtime sequencing.
[GPU.md](GPU.md), [2D.md](2D.md) and [3D.md](3D.md) define the detailed APIs and
limitations; [CONTRACTS.md](CONTRACTS.md) fixes coordinate and color conventions.

The current scope includes model importing, reflected custom pipelines and
reload, bounded residency/streaming, conservative target reuse, batching and
direct vector paths, metallic–roughness PBR, directional/environment lighting and
rigid transform animation. Shadows, skinning and VAT remain separate extensions.
GPU timestamps use a pinned, reproducible Vulkan-only SDL extension, exposed
through a versioned device-property table. Bounded asynchronous queries share the
central submission completion tracker; see [PROFILING.md](PROFILING.md). CPU
submission duration must never be labelled GPU execution time.

Direct paths use authored move/line/quadratic/cubic/close commands, bounded
adaptive flattening, nonzero/even-odd fill, and round-cap/round-join strokes.
The GPU evaluates coverage from segments, without a CPU bitmap upload. Existing
SVG documents retain their full raster compatibility path; this API does not
silently reinterpret arbitrary SVG CSS, filters or gradients as solid paths.

## Spatial-data products

[Dataset sources](../platform/SPATIAL_DATA.md) and
[product recipes](../platform/SPATIAL_PRODUCTS.md) prepare versioned immutable
outputs without owning scenes/devices. [Spatial presentation](SPATIAL.md) supplies
baseline block surfaces, scalar/label slices and selection. Scene adapters bind
those outputs to world/local placements; renderers retain ordinary resource
identity and native accounting. No frame callback waits for a worker mesh batch.
Source edits, product invalidation and view paint demand are distinct revisions.

## Backend policy

Software and SDL GPU/Vulkan are the supported implementations.
Auto means Vulkan when eligible, otherwise software if requirements permit it.
Direct3D12 and Metal are not selectable implementations. HLSL remains the authored
shader language; SPIR-V is the packaged hardware format. DXC is a compiler, not
a requirement to use Direct3D. GPU-enabled macOS builds bundle MoltenVK for the
Vulkan portability path; see [build/install requirements](../../CONTRIBUTING.md#macos).
iOS is not a supported target. Hardware availability still requires a compatible
device and successful runtime creation.

3D is compiled with the project but optional at runtime. Required capabilities
are prepared before activating an application; optional 3D pipelines remain lazy.
A candidate listing is not proof that device, shader or target creation succeeds.

## Ownership and dependency direction

[C++ asset definitions](../platform/ASSETS.md) distinguish source definitions,
model/scene instances and native realizations. Typed catalog registration and
explicit asynchronous CPU model preparation do not introduce document apps,
automatic reload or parallel scene mutation. The host executor is lazy; apps
explicitly request work and accept completed models during owner-thread update.

| Boundary | Owns / lends |
|---|---|
| Application | Mutable world/model and retained UI composition |
| Scene3D / Scene2D | Object identities, authored props and revisions |
| Immutable mesh | Validated vertices/indices and local bounds; no mutable published alias |
| Scene snapshot | Value draw descriptions and shared immutable resource handles |
| Scene viewport | Camera/observer, logical content bounds, pixel density and ordering policy |
| Rendering | Paint/frame/settings/resource-domain contracts; does not depend on UI layout |
| Native backend | Device, pipelines, targets and queued native work |
| UI | Consumes rendering services; owns layout, input routing and retained nodes |
| Platform/AppHost | Window, settings precedence, activation and safe-boundary orchestration |

The submission type is SceneRenderProps; the UI node retains SceneViewProps.
Generic PaintContext and RenderSettings belong to rendering. Text preparation is
an explicit optional borrowed service, separate from image preparation. The
current font provider remains SDL_ttf-backed; moving an interface does not make
its font resources platform-independent.

ResourceDomainId is a typed compatibility identity, not a pointer cast or a GPU
completion fence. CPU-compatible results use a documented shared domain; each
new GPU device gets a distinct non-reused domain. Preparation/capture caches
must include domain, content revision and density/extent as appropriate.

GPU native texture ownership is distinct from PaintImage. Only initialized,
sampleable color images cross the PaintImage boundary. Depth attachments are not
paint images. Format, usage, alpha association, encoding and byte accounting
must agree; arbitrary native flags cannot silently redefine those semantics.
RenderBackendProps carries immutable AllocationLimits and debugging policy from
AppHost through factory selection, rollback and recovery. AllocationLimits
centralizes extent, pixel, transfer and residency safeguards. Configured budgets
are not measurements of free VRAM, and the live-pool cap is not a whole-device cap.

## Observer and viewport

WorldCamera defines a precise space-identified observer; the view's RenderOrigin
produces local CameraProps (eye, target, up and perspective/orthographic projection).
SceneProjection derives local draws from the same world snapshot and origin without
mutating shared assets. A viewport maps a UI content rectangle—not its border/padding—to
normalized camera coordinates and a physical render extent. It supplies:

- camera view/projection at logical aspect;
- logical-to-normalized pointer mapping with outside/empty rejection;
- a local picking ray with its captured RenderOrigin for world conversion;
- pixel extent derived from density and an independent render scale;
- ordered submissions, without changing the scene model's storage order.

Opaque and masked geometry precede blended geometry. Blended draws use stable
back-to-front camera-space bounds-center order by default; authored/submission
order is an explicit alternative. This is object sorting, not order-independent
transparency, and cannot perfectly resolve intersecting translucent geometry.
Camera controllers and gestures remain application/input concerns.

Scene patches use optional overrides (disengaged = Keep; engaged null handle =
clear). UI patches additionally offer Reset to a documented default. They need
not share one template. A material's validity does not depend on whether its
object currently has a mesh.

## Scheduling, concurrency and cache lifecycle

Owner-thread sequence:

```text
input -> publish completed CPU work -> model update -> layout
      -> prepare resources and ordered scene views -> record UI
      -> submit producers before consumers -> acquire/present -> safe-boundary commands
```

Workers may decode/compute immutable CPU results. They never mutate a live Node,
Scene, registry, window or device. Publish through a lifetime-scoped completion
mailbox or owner-polled result slot with a request generation; owner-thread
acceptance rejects stale results. AssetPreparation uses a bounded executor and
latest-request future slot; it does not enqueue callbacks into UIRoot. See
[asset preparation](../platform/ASSETS.md#asynchronous-requests).
Cancellation and stale-result rejection are separate concerns. Existing UI
CompletionSink is the UI delivery boundary, not a general GPU queue or job graph.

`runtime::CompletionQueue` is the bounded many-producer/one-owner mailbox under
UIRoot's CompletionSink. Its immutable props set maximum pending callbacks and
maximum work per drain. Posting returns false when closed, expired or full; it
does not execute work. Draining runs only on its creating thread, rejects recursive
drains, and attempts a throwing callback once while preserving the untouched tail.
Posts made during a drain wait for a later drain. Closing rejects publication and
discards pending work outside the lock. Node handle/revision acceptance stays in
the UI wrapper; other consumers provide their own request-generation checks.

Caches distinguish source lifetime, backend realization, unused residency and
in-flight native use. Shared ownership is not completion. Reuse targets only when
no published image owns them and queued usage is ordered/protected by the backend;
use fences or SDL cycling where ordering alone is insufficient. Cache eviction
never invalidates a live handle. Shutdown stops publication before services die.

Scene3D caches world/visibility values by dirty branch and draw snapshots by revision.
Snapshot results are owned copies; unchanged SceneView output can be reused without
a snapshot rebuild. Backend-domain changes invalidate realizations, not application
state. See 3D.md for the remaining linear scans and diagnostic counters.

## Activation and failure boundaries

Stage settings, validate values, realize required capabilities, apply window
policy, then commit/persist. A rejected activation must not leave unusable user
settings on disk. Backend replacement releases the exclusive window owner before
claiming another and restores the old selection if construction fails.

App switching preserves the old application until the new application's preparation
succeeds. Arbitrary callback side effects cannot be automatically undone: lifecycle
hooks must have explicit staging/activation/cleanup obligations before claiming a
transactional guarantee. Cleanup must not throw; candidate commands must not leak
into the previous application's command slot on failed activation.

RenderFailure classifies native operation failures for bounded recovery; it does
not prove device loss. Recovery publishes a fresh domain, rebases the update clock
and notifies the app. Explicit recovery remains available. Invalid values,
allocation-policy refusal and arbitrary callback exceptions are not automatically
retried. HostTransitions contains the host's tested sequencing operations; GPU.md
defines the failure categories and terminal cases.

## Scope and verification

Shadows, skeletal deformation and VAT remain outside the current scope.
Reflection/hot reload, bounded model/rigid-clip import, material/texture preparation,
PBR/IBL, GPU timing, streaming/batching, residency/pooling and direct vector paths
have focused tests under tests/rendering.
Those tests verify their bounded contracts, not arbitrary formats, shaders or
driver behavior. In-app GPU timestamps require native capability; unsupported
devices produce no fabricated samples.

Verification includes invalid props, failed preparation/activation, stale domains,
snapshot ownership, camera/input mapping, alpha ordering, cache reuse and bounded
allocation. Vulkan readback tests explicitly skip when no device is available. Colored/complex glyph comparisons
must use appropriate licensed test fonts and color-space-aware tolerances.
Cross-platform execution remains unverified until run on those platforms.
