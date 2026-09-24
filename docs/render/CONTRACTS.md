# Rendering contracts and implementation sequence

Rendering is a service, not an application or UI inheritance hierarchy. A backend
may supply 2D composition and 3D scene rendering independently. Both software and
SDL GPU 3D are required project targets; neither requires a 2D-only app to create
a scene, camera, depth buffer, or glyph engine. Allocate these resources on use.

## Vocabulary and ownership

| Value/service | Responsibility |
|---|---|
| RendererRequirements | Non-negotiable app needs; settings cannot weaken them |
| RendererPreferences | Requested backend/driver and permission to fall back |
| RendererCapabilities | Implemented services and composition color behavior |
| RendererState | Requested preferences, selected implementation, fallback reason |
| RenderBackend | Own renderer resources; lend one frame at a time |
| RenderFrame | Borrow backend; record work, then explicitly present with Submitted/Skipped outcome |
| SubmissionId / ResourceUse | Track ordered native completion and resource leases within one domain |
| AllocationBudget | Retain estimated allocation reservations independently of cache ownership |
| SceneRenderer | Render a view from immutable submissions, not own the world |
| PaintContext | Compose 2D content; never measure layout or own a camera |

Shared PaintImage, PaintImageHandle, ImagePaint and Sampling live in
`rendering/PaintImage.hpp`; UI consumes them. `rendering/RenderFormatters.hpp`
formats renderer choices/drivers and Sampling without requiring UI headers.

`Auto` is a selection policy, never an actual backend. Selection checks app
requirements before preferences. A permitted fallback is reported, not silently
treated as the requested backend. Explicit GPU driver selection does not mean a
GPU driver is available. Unknown enum values are errors. Prefer GPU then software
for Auto when compiled shaders and a compatible device are available. Both
software and GPU backends implement 2D and unlit 3D; only GPU composition is linear.

AppInfo owns requirements. PresentationProps owns preferences, merged using the
existing project/user settings precedence. AppContext exposes resolved state.
Settings changes are validated at host safe boundaries, outside a frame. Backend
replacement releases the previous window presentation owner, constructs the
replacement, and attempts to restore the previous backend on failure. CPU sources
survive; images, layer caches and atlas targets re-realize for the new device.
Never weaken requirements to make a fallback appear successful. Typed native
frame failures can trigger bounded renderer recreation after the frame is gone;
validation and arbitrary app exceptions still propagate. Candidate activation
and settings publication restore captured host state on failure. Arbitrary app
side effects are not reversible; failed restoration is terminal. See
[architecture](ARCHITECTURE.md) and [runtime behavior](GPU.md#runtime-contract).

## Coordinates and constraints

Keep the existing left-handed camera convention: world +Y up, camera forward +Z,
column vectors, column-major matrix storage, radians, and clip = P * V * M * p.
Normalized depth is [0,1]; UI and target pixels have +Y down. Texture source
coordinates, UI logical coordinates and target pixels remain distinct. SDL GPU
normalizes native backend differences; do not add Vulkan-specific shader flips.
Importers convert source units, axes, winding and UV conventions at their boundary.

Rotation is represented by a normalized quaternion; scaling remains separate.
Negative scales reverse orientation. Singular transforms cannot be inverted for
picking or normal transformation. Public CPU math has no SIMD alignment promise:
optimized kernels may use packed/internal SIMD representations without making
storage layout, shader buffer layout and public values accidentally identical.

| Constraint family | Authority / communication |
|---|---|
| UI sizing/placement | Parent offers SizeConstraints; child measures; parent arranges |
| Kiwi layout relations | Linear relations inside ConstraintLayout only |
| Scene hierarchy | Parent/local transforms derive world transforms; reject cycles |
| Renderer limits | Validate target extent, formats and allocation policy before work |
| Future physics/navigation | Update model poses/results; do not mutate UI or GPU resources from workers |

These systems exchange values and handles, not a universal constraint solver.

## Color contract

Authored ColorRGBA8 values and ordinary image RGB bytes represent sRGB; alpha is
linear coverage. LinearRGBA represents straight, linear-light floating-point RGBA.
Premultiplication happens **after** sRGB decoding. Composite premultiplied values
using source-over; unpremultiply and encode only when the output representation
requires it. Alpha-zero conversion produces zero RGB, avoiding hidden-color
division. HDR, ICC profiles and wide-gamut working spaces are separate extensions.

New renderers should compose in linear light, with explicit source encoding,
alpha representation and target format. The current SurfacePainter blends encoded
byte values. Its capability reports EncodedSRGB, not Linear. Do not relabel existing
premultiplied surface bytes as linear, and do not claim pixel parity between these
paths. Linear conversion helpers do not themselves migrate the existing painter.

## Frame and scheduling contract

Owner thread: input/update -> layout -> content preparation/uploads -> scene views
-> UI/layers -> final composition -> presentation. SceneView integration needs a
post-layout/pre-paint preparation phase; no UI mutation from paint callbacks.
Nested layers require ordered producer/consumer passes. Do not sample an active
attachment. Preserve translucent draw order when batching.

CPU source ownership, device-local realization, frame recording and in-flight GPU
usage have different lifetimes. Immutable CPU handles survive backend replacement;
device realizations do not. Shared ownership is not permission to overwrite an
in-flight buffer. Use SDL cycling for transient reuse and fences where completion
must be observed. Keep decode jobs CPU-only and publish through the completion
mailbox; no worker mutation of nodes or registries.

Frame destruction abandons application recording, not arbitrary GPU work. SDL
forbids cancelling a command buffer after swapchain acquisition. The GPU backend
must acquire late in present(), define its post-acquisition failure cleanup, and
never throw from destructors. Already submitted uploads are not rolled back.

## Implementation sequence and acceptance

| Stage | Deliverable / acceptance |
|---|---|
| Foundations | Requirements/preferences/capabilities, settings, color/math contracts, shader tooling |
| GPU frame | Device/window ownership, target allocation, upload scheduling, late presentation |
| GPU 2D | Existing shapes, images, affine/rounded clips, layers and captures; Demo/Minesweeper compatibility |
| GPU text | SDL_ttf atlas batches preserving current wrapping, fitting, direction and style semantics |
| Scene model | Checked handles, parent transforms, bounds, immutable meshes/materials, camera |
| GPU 3D | Indexed unlit textured geometry, clipping, depth, explicit culling/alpha policy |
| Scene integration | UI viewport sizing, picking, target reuse and scene-only resolution scale |
| Software 3D | Same submission contract; homogeneous clipping, coverage, depth and perspective-correct interpolation |
| Replacement/recovery | Safe cache invalidation, requested/resolved state, explicit recovery/failure policy |

GPUTextEngine owns SDL_ttf's atlas; do not duplicate that allocator. Prepared text
must share layout semantics with CPU text. Full-text raster uploads remain the
compatibility path until glyph drawing supports those semantics. SVG remains
rasterize-at-density then upload; direct vector-path rendering is separate.

Software/GPU 3D are implemented unlit capabilities. SceneView prepares after layout,
caches by scene revision, pixel extent and both scene/image resource domains, and supports independent
resolutionScale. Scene3D provides immutable draw snapshots, cycle-safe parenting,
checked identities, mesh bounds and CPU triangle picking. Scene2D is a separate
ordered affine display list; Scene2DView uses either 2D painter.
PBR, shadows, animation/skinning and advanced transparency follow the unlit path.
Reconciliation is not required. Idle/damage updates, additional controls and native
accessibility remain in the UI plan, independently of renderer work.

Tests cover pure contracts and failure paths without hardware. Device tests must
be opt-in and report backend/platform; CPU/offscreen success does not establish
GPU or cross-platform correctness. Compare color in the declared working space
with tolerances, not indiscriminate byte-identical screenshots.

## Sources

- [SDL GPU coordinates, cycling and shader formats](https://wiki.libsdl.org/SDL3/CategoryGPU)
- [Command-buffer cancellation](https://wiki.libsdl.org/SDL3/SDL_CancelGPUCommandBuffer)
- [SDL_ttf atlas drawing](https://wiki.libsdl.org/SDL3_ttf/TTF_GetGPUTextDrawData)
- [SDL_shadercross](https://github.com/libsdl-org/SDL_shadercross)

SDL_shadercross has no published tags/releases at the dependency review. The
optional source build is pinned to 1ff05bec573988a98ef9e0260b4da44f512b8367;
the project's 3.0.0 CMake version is not a release tag. HLSL is shader source,
not a library dependency. Software builds do not require shader tools.
