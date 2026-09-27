# GPU resources and rendering runtime

Read [2D.md](2D.md) for painting, [3D.md](3D.md) for scenes, and
[CONTRACTS.md](CONTRACTS.md) for coordinates, color and constraint boundaries.

## Available backends

| Backend | 2D | 3D | Composition |
|---|---|---|---|
| SurfaceRenderBackend | Shapes, affine/rounded clips, images, layers/capture | Reference unlit rasterizer | Encoded sRGB UI; scene internals linear |
| GPURenderBackend | Same operations through SDL GPU | Indexed unlit/PBR geometry, directional/environment lighting and depth | Linear RGBA16_FLOAT, scene tone mapping, final SDR sRGB encoding |

Hardware rendering selects Vulkan only; Direct3D and Metal are not selectable.
GPU eligibility requires packaged SPIR-V shaders and a supported device/driver. Auto
prefers GPU; explicit software remains available without shader tools. Strict
preferences and application requirements are never weakened. Creation failures
may try another compatible candidate under the selected fallback policy.

SDL GPU is part of SDL3, not an additional Vulkan wrapper. The pinned SDL_ttf
build enables HarfBuzz and PlutoSVG; FreeType is mandatory upstream. HarfBuzz
shapes glyphs, FreeType rasterizes them, and SDL_ttf owns GPU atlas allocation.
PlutoSVG supports font SVG glyphs, not general UI SVG image rendering.
[SHADERS.md](SHADERS.md) describes offline HLSL compilation and packaging.

## Vocabulary

| Term | Meaning |
|---|---|
| Source | Immutable authored/decoded resource, independent of its displayed box |
| Realization | Backend/device-compatible representation of a source |
| Preparation | Obtain a realization; may submit uploads |
| Recording | Collect CPU draw commands and retain their inputs |
| Submission | Send commands to the device queue; not completion |
| Completion | Device has finished work; fences establish this when needed |
| Presentation | Make output available to the window/display |
| Residency | Retaining unused resources, separate from live ownership |

Input/update -> layout -> prepare uploads/text/scenes -> record UI -> submit
offscreen producers -> UI composition -> acquire swapchain -> encode/present.

<a id="runtime-contract"></a>
## Host/frame ownership

AppHost owns PresentationSession, which owns Window and RenderBackend.
IApp borrows RenderFrame; the host presents.
Destroy the frame before replacing its backend or reconfiguring the window.
SDL/TTF guards outlive all dependent handles.

| API | Contract |
|---|---|
| RenderBackend::description() | Actual backend, driver and capabilities |
| drawableSize() | Physical pixels, not logical layout units |
| beginFrame(RenderFrameProps) | One live frame; null for minimized/empty targets |
| frame.paint2D() | Borrowed painter; never store beyond the frame |
| frame.scene3D() | Borrowed optional scene service, not an application world |
| frame.present() | Explicit once-only submission/presentation |
| Frame destruction | Discard unfinished recording, not previously submitted work |
| UISession::render(frame) | Supplies painter and scene services during preparation |

The host processes pending intent after event callbacks, update and frame
destruction. AppContext is temporary; persistent callbacks must not retain it.
PendingAppCommand is one coalescing optional slot, not a FIFO; Quit has priority.
UI completion/deferred queues and GPU work have independent lifetimes.

App requirements are prepared before activation. The previous app stays alive
while the candidate enters and sizes its window. Failure restores captured host,
window, command and renderer state; failed restoration is a distinct terminal
RestorationFailure carrying both exceptions. This does not undo arbitrary external
effects in application callbacks. Exit callbacks are isolated and cannot issue
commands into the next app. AppContext::lastCommandError reports rejected intent.

Typed native acquire/submit/present failures trigger bounded renderer recreation
after frame destruction. CPU sources survive; fresh ResourceDomainId values
invalidate realizations. Update is not replayed. SDL does not expose reliable
device-loss classification, so RenderFailure means a recovery candidate rather
than proof of device loss. Validation and arbitrary application exceptions still
propagate; no recovery screen is installed.

`RenderFailure::operation()` identifies Acquire, Record, Submit, Present or Query;
Unknown covers an already invalid domain or exhausted recovery. This is an
operation label, not a portable device-loss diagnosis. `HostTransitions.hpp`
contains the sequencing used by AppHost itself: frame scope ends before recovery,
old backend is invalidated/destroyed before releasing window presentation, and a
new domain is published before notification. Exit hooks suppress new commands
without moving or copying the pending command payload.

| Failure | Host treatment |
|---|---|
| Invalid arguments, state misuse, arithmetic overflow | Reject/propagate; do not recreate the renderer |
| AllocationLimits or submission-capacity refusal (`length_error`) | Policy refusal; no automatic resolution reduction or recovery |
| Native frame acquire/record/submit/present/query (`RenderFailure`) | Bounded recreation, after frame destruction; no update replay |
| Resource construction, font/image decoding or native allocation errors | Ordinary exceptions; candidate creation may follow configured fallback |
| Failed rollback or recovery factory/notification | Terminal; do not keep using partially restored state |

`RenderBackendProps` supplies immutable creation policy to AppHost and the backend
factory: `allocations` and `gpuDebug`. It is preserved across switching, restoration
and recovery, not stored as a per-frame option or user-settings field. Both software
frame/scene allocation and GPU device allocation consume this policy. UI layer and
software composition cache budgets remain separate and explicitly scoped.

RenderSettings controls whole-frame resolutionScale, glyphAtlases and vsync,
without changing input/layout coordinates. SceneView has a separate resolution
multiplier. GPU immediate presentation falls back to VSYNC when unsupported;
software pacing remains platform-controlled. See [settings](../platform/SETTINGS.md).

<a id="resource-contracts"></a>
## Image sources and ownership

PaintImageHandle is shared immutable identity. pixelSize, alphaMode and
colorEncoding describe actual pixels: Straight/Premultiplied and SRGB/Linear.
Premultiplication is in the declared encoding. Encoded associated sources must
be unassociated before decoding. Metadata does not convert bytes or prove labels.

| API | Behavior |
|---|---|
| ImagePreparer::prepare | Compatible realization preserving dimensions/metadata |
| prepareImage | Validates source/result; null preparer means identity |
| RGBA8Image | Tight top-down RGBA bytes, dimensions and encoding/association |
| byteSize/validate | Reject invalid dimensions, overflow, byte counts and enums |
| SurfacePaintImage | Shared CPU source; do not mutate published pixels |
| GPUDevice/GPUDeviceHandle | Native ownership/shared lifetime, not SDL initialization ownership |
| GPUImage(device, pixels) | Raw RGBA8_UNORM single-level upload retaining device |
| Internal ColorTarget | Writable linear-associated RGBA16F attachment, not a public PaintImage |
| GPUTextureResource / GPUResource | Device-retaining native RAII; depth is not a sampleable PaintImage |
| get/device accessors | Native borrows/device inspection, not ownership transfer |
| packSurfaceRGBA8 | Preserve pitch/metadata; no gamma conversion |
| GPUImagePreparer | CPU surface -> cached upload; same-device image -> identity; foreign device -> error |

Uninitialized size-only GPUImage construction and mutable image lease access are
private. Internal ColorTarget publishes a const image only after its initializing
submission succeeds. Publication means queued in producer-before-consumer order,
not GPU completion. Published image owners and recording/submission leases prevent
pool reuse. Reacquisition requires a fresh submission before another publication;
internal native writers must still initialize the entire attachment. Raw native
texture access is an escape hatch, not permission to mutate published images.

The realization cache keys shared surface ownership plus alpha/encoding and holds
strong, byte-budgeted LRU results. Wrappers around one source can share uploads;
distinct equal allocations need not. AssetRegistry handles authored-value sharing
upstream. Eviction releases cache ownership, not live caller handles. Limits are
policy estimates, not a query of free VRAM. Live images retain their device.

UI PaintImages have no mipmap generation, compressed formats, mutable streaming
updates or video planes. Scene Texture resources have explicit mip chains and
independent sampler policies; see [MATERIALS.md](MATERIALS.md). Transient draw records and mesh transfers do reuse cycling
upload/storage buffers. Transfers reject sizes exceeding SDL's 32-bit capacity. No automatic
GPU-to-CPU or cross-device copy exists. GPU-only authored sources require retained
CPU sources or regeneration information for backend switching. Use these APIs on
the renderer thread and release resources before SDL/TTF shutdown.

## Drawing and synchronization

GPUPainter records values and retained image handles. Consecutive compatible
same-texture quads become instanced batches, preserving painter order; paths
remain isolated because they have distinct segment uniforms. Draw records stream
in bounded chunks through reusable SDL-cycled transfer/storage buffers. HLSL implements inverse
affine rounded shapes, joined fill/border coverage, nearest/linear filtering,
32 nested clip masks and 4x4 coverage. Exceeding clip capacity throws. 2D filtering
decodes source texels before interpolation. Composition uses premultiplied
ONE / ONE_MINUS_SRC_ALPHA into linear RGBA16_FLOAT targets.

Offscreen groups/captures are submitted before consumers. Group opacity applies
once. The main target is acquired from the same completion-aware pool as offscreen
targets, rather than overwritten while a previous submission might use it. No pass samples its own
attachment. SDL's GPU release APIs defer native retirement; shared_ptr is not a
fence. present() submits the UI target, acquires the swapchain late, then encodes
sRGB for SDR output. After acquisition an error guard submits rather than cancels:
SDL forbids cancellation then. Earlier uploads/captures cannot be rolled back.
CPU Render/Present profiling does not measure GPU execution time.

AllocationLimits centralizes maximum dimensions, target bytes, transfer bytes and
retained-cache bytes. Default target cap is 64 MiB, dimensions 16384 per axis;
scene color-plus-depth accounting uses 12 bytes/pixel. UI Layer also enforces its
root/individual cache policies. Layer and text compare ResourceDomainId;
SceneView includes scene-renderer and image-preparer domains. Changed domains
rebuild output, not the application model. No ID is recycled during the process.

Mesh residency and target pooling have their own byte budgets. Target reuse
requires exclusive pool ownership **and** no recording or pending-submission
leases. `GPUDevice` acquires commands, retains resource-use leases, submits with
an SDL fence and polls completion on its owner thread. Submission IDs increase
within one resource domain; neither an ID nor a shared pointer proves completion.
Leases contain bookkeeping, not device-owning handles, avoiding ownership cycles.
Recorded painter draws retain images before encoding; encoded commands lease
every project texture they read or write. Native custom callbacks must declare
sampled project images with `GPURecordingContext::use(image)` before drawing.
Do not submit/cancel the borrowed commands yourself.

The pool uses exact-size matching, grows when compatible targets are busy, and
evicts unused completed targets under retention pressure. Ordinary reuse never
waits for device idle. Recent targets remain cached for 240 completed submissions
by default; aging is evaluated on later acquisitions. This is demand-driven
hysteresis, not a GPU-time-based adaptive controller or a free-VRAM query.

`maxTargetPoolBytes` (64 MiB) limits retained cache entries; `maxLivePoolBytes`
(256 MiB) limits estimated pool allocations, including published targets and
reservations held by recorded/in-flight uses. Allocation pressure first trims
unused completed targets, then throws `length_error` if capacity is still absent.
It does not block or silently reduce resolution. Native retirement and driver
overhead are not exact VRAM accounting; independent uploads, mesh residency and
TTF atlas pages have separate ownership and are not charged to this pool.
`maxInFlightSubmissions` bounds tracked recordings plus pending batches (256);
capacity exhaustion is a policy error, not proof of device loss. SDL fence polls
do not distinguish not-ready from every native failure, so recovery may first be
triggered by a later acquisition/submission/query failure or an explicit request.

PaintDevice exposes quad/draw-call/streamed-byte counters; TargetPool exposes
retained/live estimated bytes, allocations, reuses, busy misses, evictions and
pressure failures. These are diagnostic work counters, not GPU duration. Software
scene targets use the same allocation-policy type, including the explicit
`maxSoftwareTargetPixels` limit. Public native custom passes use GPUFrameAccess;
see [custom pipelines and reload](SHADERS.md) and [profiling](PROFILING.md).

## Text and SVG

GPUTextEngine owns TTF's engine. GPUText retains engine/font/TTF_Text and exposes
setValue, setWrapWidth, size and borrowed drawData. Consume sequences before text
or engine changes. Atlas textures belong to TTF; never release them yourself.
The adapter converts +Y-up positions to UI +Y-down while preserving the pinned
TTF implementation's supplied UV orientation.

Text keeps existing measurement, fitting, wrapping, direction, alignment and
truncation logic, then passes the resolved font/string/wrap to TextImagePreparer.
The neutral service takes a borrowed TextSource; SDL's FontTextSource carries
the actual immutable FontHandle. Text does not discover this by cross-casting
ImagePreparer, and either service can be supplied independently.
Blended horizontal text uses atlas quads to produce a cached linear image.
Font layout's `lineSpace` is an optional baseline advance in **pixels**, not a
multiplier: absent means the font's natural line skip. `getLineSpace()` returns
the authored override; `getLineSkip()` returns the resolved native pixel advance.
A `FontPatch` omits `lineSpace` to keep it, supplies an engaged integer to set it,
or uses `patch.lineSpace.emplace(std::nullopt)` to reset it to natural metrics.
Text density scaling multiplies only explicit overrides; automatic metrics follow
the newly sized font. Size/style/outline changes rebuild mutable Font resources,
so native pointers borrowed through `get()` must not survive those mutations.
Native custom fallback registrations are outside FontProps and must be registered
again after a rebuild; registry-provided FontHandles expose immutable fonts.

Unchanged text reuses it: this avoids CPU full-string raster work but is not
direct glyph drawing into the window every frame.

Solid, Shaded, LCD, vertical columns and SDF retain CPU raster/upload compatibility,
not silently reduced features. glyph_atlases=false requests this path explicitly.
The pinned SDL_ttf is locally patched by
[`SDLTTFText.cmake`](../../cmake/patches/SDLTTFText.cmake). Text operations use
actual raster glyph bearings/dimensions, including italic/outline expansion and
COLR glyphs without base outlines. Atlas UVs preserve source cropping instead of
stretching an entire glyph into the cropped destination. These styles and color
fonts therefore stay on the atlas path, not a full-string fallback.
Color atlas glyphs unassociate encoded-premultiplied bytes before linear decoding;
their foreground RGB is ignored, but foreground opacity still applies.
The patch also converts color glyph RGB to straight alpha when SDL_ttf writes a
CPU blended-text surface. Monochrome and color runs can then share that surface
without mixed alpha associations; `TTF_GetGlyphImage` and native atlas pixels
remain encoded-premultiplied for color glyphs. Foreground RGB affects monochrome
runs only, while foreground opacity affects both.

Tests compare complete CPU/GPU text output for Twemoji, Bungee and a mixed
monochrome/color fallback-font string, including wrapping, partial alpha and a
nonwhite translucent foreground. This is still not exhaustive script/font
conformance: arbitrary overlapping runs can differ because the legacy CPU text
rasterizer's overlap arithmetic is not linear-light source-over. SDF, LCD and
vertical-text algorithms remain the explicitly selected compatibility paths.
SVG documents remain rasterize-at-density then upload. Authored Path2D geometry
has a separate direct GPU coverage path, exposed by PaintContext::drawPath and
the retained UI Path node; see [vector paths](2D.md#vector-paths).
Preparation failures remain retryable.

## Extensions and verification

Reflected custom pipelines/reload and glTF/GLB importing are implemented separately
from UI; see [SHADERS.md](SHADERS.md) and [3D.md](3D.md). CPU background work can
publish through the bounded CompletionQueue, without worker mutation of the scene
or UI. Fixed-step simulation is an explicit runtime service; PBR, environment
lighting, HDR scene tone mapping and rigid clips are explicit scene services.
HDR display output/ICC/wide-gamut color, shadows, skinning and advanced transparency
are not implicit in a GPU device. A general ECS/job
graph is not required to use the current path.

The project applies a pinned Vulkan-only SDL timestamp extension. Its versioned
device-property interface feeds a bounded asynchronous query ring, sharing
GPUDevice's submission completion tracker. Measurements cover whole command-
buffer intervals, not individual shader stages; absent capability and unavailable
results produce no samples. [PROFILING.md](PROFILING.md) defines timing identity,
counter arithmetic, cancellation and native pool lifetime.

External tools such as [NVIDIA Nsight Graphics](https://docs.nvidia.com/nsight-graphics/UserGuide/gpu-trace-overview.html)
and [RenderDoc](https://docs.vulkan.org/tutorial/latest/Advanced_glTF/Debugging_Visual_Auditing/04_renderdoc_analysis.html)
remain useful for detailed pipeline/pass analysis. CPU phase times include
submission/wait costs but do not establish GPU execution duration. Replacing or
extending the native adapter does not change PaintContext, Scene3D or UI layout
contracts.

### Replacing SDL_GPU

SDL_GPU is the current Vulkan adapter, not the engine's required public graphics
API. A native Vulkan implementation would implement RenderBackend/RenderFrame,
PaintContext, ImagePreparer, TextImagePreparer and SceneRenderer, with fresh resource
domains and its own synchronization, memory and swapchain ownership. Immutable CPU
image/mesh/model inputs remain reconstruction sources; native handles do not migrate.

The optional GPUFrameAccess/custom-pipeline extension is deliberately SDL-specific.
Applications using it would need a corresponding native extension or a later neutral
pipeline API; those calls are not promised a transparent backend swap. SDL_ttf's GPU
text engine also takes an SDL_GPUDevice. Replacing SDL_GPU requires a different text
engine/atlas adapter (or CPU raster/upload compatibility), not merely replacing
texture allocation underneath the existing engine.

Prefer an SDL extension for a narrowly missing capability that fits its ownership
model. Prefer a separate backend when required command scheduling, resource models
or Vulkan extensions cannot fit that model cleanly. Keep any such fork isolated,
version-pinned and tested at the adapter boundary. The current implementation
applies the narrow timestamp extension described in [PROFILING.md](PROFILING.md),
plus SDL_ttf raster/atlas corrections; it does not replace SDL's backend.

Ordinary tests cover packing, ownership, preparation, software pixels and failures
without hardware. PLAYGROUND_GPU_TESTS adds gpu_device and gpu_shaders (Vulkan),
skip code 77 for unsupported drivers. These read back selected pixels
and exercise layers, paths, atlases, custom-pipeline reload and frame lifecycle. Windows success is not Linux,
mobile or exhaustive visual verification. See [CONTRIBUTING](../../CONTRIBUTING.md#testing).
