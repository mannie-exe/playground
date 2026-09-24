# Rendering architecture and phase-one contracts

This document fixes the boundaries used by the current architecture migration.
It is a contract, not a claim that every item below has already been implemented.
GPU.md, 2D.md, 3D.md and the source describe the implemented subset. Deferred
features remain visible rather than being represented by successful no-op APIs.

The current scope includes model importing, reflected custom pipelines and
reload, bounded residency/streaming, conservative target reuse, batching and
direct vector paths. Lighting/PBR, shadows and animation remain excluded.
GPU timestamps use a pinned, reproducible Vulkan-only SDL extension, exposed
through a versioned device-property table. Bounded asynchronous queries share the
central submission completion tracker; see [PROFILING.md](PROFILING.md). CPU
submission duration must never be labelled GPU execution time.

Direct paths use authored move/line/quadratic/cubic/close commands, bounded
adaptive flattening, nonzero/even-odd fill, and round-cap/round-join strokes.
The GPU evaluates coverage from segments, without a CPU bitmap upload. Existing
SVG documents retain their full raster compatibility path; this API does not
silently reinterpret arbitrary SVG CSS, filters or gradients as solid paths.

## Backend policy

Software and SDL GPU/Vulkan are the supported implementations in this phase.
Auto means Vulkan when eligible, otherwise software if requirements permit it.
Direct3D12 and Metal are not selectable implementations. HLSL remains the authored
shader language; SPIR-V is the packaged hardware format. DXC is a compiler, not
a requirement to use Direct3D. macOS/iOS hardware support needs a separately
validated Vulkan portability path; it is not implied by this policy.

3D is compiled with the project but optional at runtime. Required capabilities
are prepared before activating an application; optional 3D pipelines remain lazy.
A candidate listing is not proof that device, shader or target creation succeeds.

## Ownership and dependency direction

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
AllocationLimits centralizes extent, pixel, transfer and residency safeguards.
Configured budgets are not measurements of free VRAM.

## Observer and viewport

CameraProps defines an observer (eye, target, up and perspective/orthographic
projection). A viewport maps a UI content rectangle—not its border/padding—to
normalized camera coordinates and a physical render extent. It supplies:

- camera view/projection at logical aspect;
- logical-to-normalized pointer mapping with outside/empty rejection;
- a picking ray in world space;
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
mailbox with a request generation; owner-thread acceptance rejects stale results.
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

Static scenes should reuse snapshots/output. Dynamic scenes should not validate
every immutable vertex or rebuild unrelated world transforms every frame. Scene
mutation advances revisions; backend-domain changes invalidate realizations, not
application state. A cached layer containing an unchanged Scene2DView must remain
reusable.

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

Device-loss handling needs typed failure classification, a bounded retry/rebuild
policy, new resource domains, and retained CPU/reconstruction inputs. Do not infer
device loss by matching arbitrary SDL error strings or silently retry every error.
If the native API cannot reliably classify a failure, expose an explicit recovery
request and an observable failure state rather than pretending recovery occurred.

## Feature gates and acceptance

Lighting/PBR, shadows and animation are deliberately outside this phase. The
following each need their own executable contract before being called complete:
shader reflection/custom pipeline ABI and atomic hot reload; model importing and
axis/unit conversion; GPU timing; batching/streaming/residency/target pooling;
direct GPU vector paths. These are implementation requirements, not merely seams;
no placeholder interface counts as an implementation. In-app GPU timestamps
require native capability; unsupported devices produce no fabricated samples.

Verification includes invalid props, failed preparation/activation, stale domains,
snapshot ownership, camera/input mapping, alpha ordering, cache reuse and bounded
allocation. Vulkan readback tests are opt-in. Colored/complex glyph comparisons
must use appropriate licensed test fonts and color-space-aware tolerances.
Cross-platform execution remains unverified until run on those platforms.
