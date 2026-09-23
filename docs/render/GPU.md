# GPU resources and rendering runtime

This document records APIs, ownership and runtime obligations, not a GPU renderer
implementation tutorial. Read [2D.md](2D.md) for PaintContext and [3D.md](3D.md)
for the separate scene service. UI layout and node contracts live in
[../ui/CONTRACTS.md](../ui/CONTRACTS.md).

## Availability

| Boundary | Exists now | Not implied |
|---|---|---|
| Application rendering | RenderBackend/RenderFrame, AppHost integration, software backend | GPU backend selection or live switching |
| Image realization | Generic handles/preparer, validated RGBA data, SDL GPU upload/ownership/cache helpers | GPU draw pipelines, samplers or presentation |
| Text | CPU Text path; GPUTextEngine/GPUText ownership and atlas draw-data access | GPU Text node implementation with equivalent wrapping/fitting/vertical behavior |
| Scene | Camera math, mesh/material values, abstract SceneRenderer | Scene renderer, depth pass, PBR or SceneView node |

AppHost constructs SurfaceRenderBackend. Merely enabling SDL GPU or constructing a
GPUImage does not cause an app to render on the GPU. Resource helpers are compiled
and available but are not wired into an active GPU painter.

## System vocabulary

| Term | Meaning here |
|---|---|
| Source | Immutable decoded/rasterized content; independent of where it is drawn |
| Realization | A representation usable by one backend/device, such as a GPU texture |
| Preparation | Obtain a realization; may submit upload work |
| Recording | Describe drawing commands on the CPU |
| Submission | Hand recorded work to the device queue |
| Completion | The GPU has finished the relevant work; returning from a draw/upload call is not this |
| Presentation | Make a frame available to the window/display system |
| Residency | Keep unused resources available; different from shared ownership of live resources |

The resource path and frame path meet, but are not the same lifecycle:

```text
asset request → source image → ImagePreparer → backend image ──┐
                                                            ↓
AppHost → RenderBackend → RenderFrame → paint2D / scene3D → present
```

<a id="runtime-contract"></a>
## Host and frame contract

Declarations: [RenderBackend.hpp](../../include/rendering/RenderBackend.hpp),
[IRuntimeObject.hpp](../../include/interfaces/IRuntimeObject.hpp),
[AppHost.hpp](../../include/app/AppHost.hpp),
[AppContext.hpp](../../include/app/AppContext.hpp).

| API | Contract |
|---|---|
| RenderBackend::drawableSize() const → Vec2i | Physical output extent, not logical UI/window size |
| beginFrame(RenderFrameProps) → unique_ptr<RenderFrame> | At most one live frame; null means temporarily unavailable, not a failed resource |
| RenderFrameProps::clearColor | Authored frame clear value; backend owns clearing |
| RenderFrameProps::settings | Whole-frame rendering resolution, independent of window/UI geometry; software backend supports a reusable scaled target |
| RenderFrame::paint2D() → PaintContext& | Borrowed 2D painter for this active frame |
| RenderFrame::scene3D() → SceneRenderer* | Borrowed optional capability; null explicitly means unsupported |
| RenderFrame::present() | Explicit end/presentation operation; do not call twice |
| RenderFrame destruction | Abandon unfinished frame without presenting; not rollback of pixels or previously submitted uploads |
| IApp/IRuntimeObject::render(AppContext&, RenderFrame&) | App chooses which frame services to use; host still presents |

Destroy a frame before its backend/window and before window reconfiguration.
Do not keep its painter or scene service in a node, job, or later frame. Helpers
retain device resources where necessary; a borrowed RenderFrame does not keep its
backend alive. Raw get() accessors are borrows, not ownership transfer.

The current host loop has these boundaries:

1. Poll platform events, handle host events, dispatch remaining input to the app;
   process pending app/window intent between event callbacks.
2. Call app.update(ctx, deltaSeconds), then process pending intent.
3. Acquire a frame; if available, call app.render(ctx, frame).
4. Present, destroy the frame, then process pending intent again.

Updates/input continue when no frame is available. They are not driven by GPU
completion. PerformanceMonitor's Render/Present phases measure CPU-side time;
future GPU timing needs explicit timestamps/fence-aware measurement.

AppContext is a temporary host facade: assets, asset paths, window facts, metrics
and requestSwitch/requestMenu/requestQuit/requestWindowProps, plus presentation,
view-policy, content-fit and settings requests. It no longer exposes
a target surface. Do not capture it in persistent callbacks. PendingAppCommand is
currently **one coalescing optional slot**, not a FIFO: later non-Quit requests can
replace earlier ones; Quit takes precedence. This is independent of the root's
deferred/completion queues and any future GPU submission queue.

Use requestWindowProps/requestPresentation/requestSwitch from callbacks. AppContext exposes window
facts, not a mutable Window. Immediate AppHost::switchTo is private; the host
applies app requests at the boundaries above, after callback/frame borrows end.
This facade does not make calls from worker threads safe; post results to the UI
thread before requesting host changes.

App switching applies the new app's window configuration and trims CPU asset
caches. It does not select a new renderer or manage device-local budgets. Switching
apps and rendering frames are not transactional. The top-level main catches
exceptions and exits; RAII unwinds resources, but no recovery screen is installed.
SDL failures are translated to std::runtime_error with operation/error context.

AppInfo separates AppWindowProps, AppViewPolicy and PresentationProps. The host
resolves project/user settings, constructs content before preferred measurement,
and applies window mode/display choices outside frame lifetime. See
[windowing](../platform/WINDOWING.md) and [settings/storage](../platform/SETTINGS.md).
A future GPU backend must preserve viewport/input coordinates while honoring target
resolution; separate scene-only resolution and swapchain policy remain its own design.

<a id="resource-contracts"></a>
## Generic image contracts

Sources: [PaintImage.hpp](../../include/ui/PaintImage.hpp),
[ImagePreparer.hpp](../../include/rendering/ImagePreparer.hpp),
[ImageData.hpp](../../include/rendering/ImageData.hpp).

| Type/API | Meaning and obligation |
|---|---|
| PaintImage::pixelSize() const noexcept | Pixel dimensions used for source geometry; not the assigned UI box |
| PaintImageHandle | shared_ptr<const PaintImage>; immutable image identity, retainable by nodes/recorded work |
| ImagePreparer::prepare(PaintImageHandle) | Return a compatible realization with the same dimensions and alpha interpretation |
| prepareImage(source, preparer) | Reject null source; null preparer means identity; reject null/differently sized result |
| PaintContext::imagePreparer() | Optional borrowed service; defaults to null |
| PrepareContext::images | Pass-local service supplied by the UI host/session |
| RGBA8Image | Positive Vec2i dimensions, AlphaMode, exactly width × height × 4 top-down RGBA bytes |
| RGBA8Image::byteSize/validate | Reject invalid dimensions, address-size overflow, byte-count mismatch or unknown alpha mode |

AlphaMode is Straight or Premultiplied. Keeping the same alpha interpretation is
an implementation obligation; the generic PaintImage interface only exposes size,
so prepareImage cannot independently validate it. No color-space/profile metadata
exists on generic images yet. Do not treat this as a finished color-management API.

Image is renderer-neutral. Text and Vector still rasterize through current CPU
providers; all three route their image output through preparation. They retain
sources separately from prepared results where needed. A failed preparation leaves
the node unready and retryable, not silently painting a stale realization.

## SDL GPU ownership APIs

Declarations: [GPUResources.hpp](../../include/platform/sdl/GPUResources.hpp).
These APIs are in playground::sdl, not the generic UI namespace.

| Type/API | Inputs, ownership and limitations |
|---|---|
| GPUDeviceProps | Required nonzero shaderFormats; debug=true; optional driver=nullptr. Formats are capabilities offered to SDL, not shader source or compiled shaders. |
| GPUDevice(props) | Creates/owns SDL_GPUDevice; creation can fail. No window claim or swapchain is created by this wrapper. |
| GPUDeviceHandle | Shared device lifetime for dependent resources |
| GPUImage(device, RGBA8Image) | Validates data, creates a sampled 2D texture and submits its upload; retains the device |
| GPUImage::get/device/alphaMode/pixelSize | Borrowed native texture, device owner, alpha interpretation and size; no mutation/readback API |
| packSurfaceRGBA8(SurfacePaintImage) | Converts source format to packed RGBA and respects source pitch/alpha convention; does not transfer ownership of the source |
| GPUImagePreparer(device) | Non-null device required; device-local realization service |
| GPUImagePreparer::prepare | SurfacePaintImage → cached/uploaded GPUImage; same-device GPUImage → identity; other sources/devices rejected |
| GPUImagePreparer::prune | Remove expired cache bookkeeping; also run during surface preparation |

Uploads currently produce R8G8B8A8_UNORM sampled textures with one mip level.
No gamma conversion, mip generation, render-target usage, compressed format,
streaming update or video-plane API is supplied. The packed staging format is
portable; SDL may perform an extra alignment copy on some GPU backends/hardware.
The wrapper also rejects uploads larger than SDL's 32-bit transfer-buffer size.

On success, the upload is submitted, not necessarily completed. The upload path
uses scoped texture/transfer ownership and abandons unsubmitted command buffers
on failure. Release requests use SDL's GPU resource-release APIs. A future renderer
must preserve resources through recording/submission and obey device timeline
rules; it must not use a stale raw texture because a shared handle was dropped.

These wrappers are noncopyable; share their designated handles instead. Resource
objects retain the device so native release occurs before device destruction.
That does **not** retain SDL initialization or make the objects thread-safe. Use
them on the owning renderer thread, and release all GPU/TTF objects before platform
shutdown. Device loss/recreation is not handled automatically.

## Upload cache semantics

GPUImagePreparer keys by shared ownership of the underlying source surface plus
alpha convention, not by filename or hashing pixel bytes. Different wrappers around
the same surface can share the realization. Distinct equal-looking allocations
need not share it; CPU AssetRegistry handles authored-value sharing upstream.

Both source keys and cached GPU outputs are weak. A live node or recorded draw
keeps its GPU image alive; the upload cache alone does not retain unused textures.
It is not the CPU registry's LRU, and it has no byte budget or memory-pressure
query. Pruning scans bookkeeping; it is not a constant-time residency manager.

Treat a published source as immutable. Editing shared pixels without replacing
identity can leave a cached upload stale. Replace the underlying surface owner,
not merely its adapter wrapper. Alpha labels also must match the actual pixels.
Backend/device replacement must invalidate incompatible layer/prepared caches;
there is no automatic cross-device copy or GPU-to-CPU readback fallback.

## Text and glyph atlases

Declarations: [GPUText.hpp](../../include/platform/sdl/GPUText.hpp).

| API | Contract |
|---|---|
| GPUTextEngine(GPUDeviceHandle) | Own TTF's GPU atlas engine and retain its device |
| GPUText(shared engine, FontHandle, string_view utf8) | Retain engine/font and own TTF_Text; validate UTF-8; reject missing owners |
| GPUText::setValue(string_view) | Validate/update the text; draw data must be reacquired |
| GPUText::drawData() | Borrowed linked TTF_GPUAtlasDrawSequence list; null can mean empty text; errors throw |
| GPUTextEngine::get/device | Borrow native engine or inspect retained device; not ownership release |

HarfBuzz shapes characters into glyph choices/positions. FreeType rasterizes glyphs.
SDL_ttf's GPU text engine manages atlas textures and supplies geometry: positions,
texture coordinates, indices, image type and a link to the next batch. Your renderer
still needs shaders, samplers, vertex/index uploads and draw calls for those batches.

The returned arrays are not persistent snapshots. Consume/copy them before text,
font or engine changes; retain the text/engine while recording/submitting dependent
work. Atlas textures belong to TTF—never destroy them yourself. Geometry in these
sequences uses +Y up, unlike UI layout's +Y down; conversion belongs in the adapter.
Glyph image types can require different shading/blending paths, not one assumed
monochrome mask. Platform/font lifetime and renderer-thread requirements still apply.

GPUText is currently a low-level helper, not the implementation of ui::Text. It
does not expose all wrapping, alignment, fitting, truncation, orientation or style
controls. Preserve existing Text semantics before substituting atlas drawing.
Until then, CPU-rasterized text uploaded as an ordinary image remains the available
preparation path. SVG likewise remains rasterize-at-target-size then upload, not
GPU vector-path rendering.

## Build and runtime prerequisites

[CMakeLists.txt](../../CMakeLists.txt) enables SDL_GPU. SDL GPU is part of SDL3,
not a separately added graphics library. SDL_ttf explicitly enables HarfBuzz and
PlutoSVG; FreeType is mandatory in the pinned SDL_ttf source, not an optional flag.
PlutoSVG supports SVG glyph content; it is not the UI's general SVG image renderer.

The active app requires no GPU device yet. To exercise GPU helpers, the caller
needs initialized SDL video, a supported driver/device and nonzero supported shader
format flags. Text additionally needs initialized TTF and valid fonts. Shader
compilation/reflection, packaging and backend-selection policy are not configured
by GPUDeviceProps and have not been installed as a new toolchain.

## Obligations of a future GPU backend

These are requirements to implement, not currently available APIs:

1. Choose/claim the window/device, acquire drawable/swapchain targets, handle null
   acquisition/minimization, resize and presentation without overlapping frames.
2. Define shader formats, binding layouts, color/alpha conventions and sampling.
   Keep node order, nested clips and group-opacity behavior equivalent to the 2D contract.
3. Supply a compatible ImagePreparer and retain resources required by recorded work.
   Integrate atlas text without losing Text's layout semantics.
4. Define command/pass ordering, upload batching, buffers, resource retirement and
   CPU/GPU timing separately. Shared ownership is not a fence or scheduling policy.
5. Define backend capability checks/fallback policy, device-loss handling and cache
   invalidation. Do not silently accept unsupported operations.
6. Add opt-in real-device tests, then a scene renderer with depth/target ownership.
   Keep ordinary UI tests independent of graphics hardware.

## Incremental work before a complete renderer

These are independent next steps, not prerequisites for committing the software
backend or promises of existing services:

| Work | Small, verifiable boundary |
|---|---|
| Shader assets | Choose the initial shader language and offline compilation path; define vertex layouts, bindings, target formats and packaged metadata together. Reject incompatible interfaces before drawing. |
| Procedural/imported geometry | Produce validated immutable MeshData first. An importer converts units, handedness, winding, UV orientation and indices at the boundary rather than leaking file-format conventions into scene math. |
| Background preparation | Start with one decode/parse job producing CPU-owned data. Deliver through CompletionSink with node/request generation checks; do not move UI mutation or GPU uploads onto that worker. |
| Resource versions | Keep CPU source identity and device-local realization separate. Replace immutable handles on edits; add explicit request generations for reloads before introducing automatic hot reload. |
| Frame resources | Specify upload-buffer reuse and submission retirement together. Record which frame owns each transient allocation and what completion permits reuse; shared_ptr alone is not GPU synchronization. |
| Simulation time | Add a bounded fixed-step accumulator only when a simulation needs it. Keep UI timers, elapsed wall time, simulation time and GPU timestamps distinct. |
| Profiling/budgets | Measure raster/upload counts and bytes before adding eviction or worker scheduling policy. CPU cache estimates, staging memory and GPU allocations are different budgets. |

The existing completion mailbox is thread-safe for posting, not a worker pool.
Signals, timers, registries and node mutations remain owner-thread operations.
Keep those boundaries while introducing concurrency; a general job graph or ECS
is not needed merely to load the first model or render the first triangle.

## Verification boundaries

[ui_image_preparation](../../tests/ui/image_preparation.cpp) tests CPU packing,
validation and fake realization/failure/retry across Image/Text/Vector.
[ui_render_backend](../../tests/ui/render_backend.cpp) tests software frame/session
contracts. GPU resource/atlas sources are compiled, but these tests create no GPU
device and establish no hardware behavior. Real upload/readback, device lifecycle,
atlas lifetime and draw correctness need explicit device-backed verification.

See [the testing inventory](../ui/TESTING.md) and
[CONTRIBUTING.md](../../CONTRIBUTING.md#testing) for scope and commands.
