# Materials, textures and playback

## Capability and composition

The renderer provides a bounded glTF metallic–roughness path alongside explicit
unlit rendering. Directional light and prepared environment illumination operate
in linear HDR; scene exposure/tone mapping precede UI composition. Software
rendering remains an explicitly selected unlit preview, never a silent PBR fallback.
Apps requiring this path set RendererRequirements::metallicRoughness. Demo 3D
also requests Vulkan without fallback. `scene::unlitPreview` is an explicit lossy
conversion: it preserves base color/texture and alpha/culling, discarding lighting,
normal, occlusion, roughness, metal and emissive behavior. Software texture sampling
uses level zero and the magnification filter; GPU implements mip/minification policy.
Exposure multiplies scene RGB on both backends without changing alpha, including
when tone mapping is disabled. Software output is SDR and clips on encoding;
software requests for tone mapping fail explicitly. GPU tone mapping is opt-in
and compresses the exposed scene before ordinary UI composition. Apps that require
it must select the GPU backend; it is not implied by the shared unlit capability.

## API map

### Preparation boundary

The preparation boundary separates immutable packed texture payloads from float
working values. PNG/JPEG artwork/data retain byte storage; Radiance HDR decoding
defaults to half-float with an explicit float32 option. Authored float input
retains float32 until explicitly packed.
Encoding, alpha association and texture
role remain explicit. Mips are filtered in linear/numerical space and packed as
they are completed. Source RGB remains available for independent opaque and
coverage-aware realizations. KTX2 loading is bounded and rejects unsupported
shapes/interpretations rather than guessing. Runtime compression is not implied.

Mesh preparation generates missing attributes, then exactly reindexes complete
vertex records using meshoptimizer. Triangle order and winding remain unchanged;
there is no tolerance welding, topology simplification or implicit triangle
reordering. Temporary corner expansion is budgeted separately from final output.
Preparation publishes only validated results; failures preserve caller data.

| Contract | Meaning |
|---|---|
| rendering::Texture / TextureHandle | Immutable packed texels, interpretation, alpha association and validated mip dimensions; shared const ownership |
| TextureLevel | Transient float32 authoring/working values, not retained texture storage |
| PackedTextureLevel / PackedTexels | Size plus byte payload; iteration/indexing decodes linear working values |
| TextureFormat | RGBA8, R8, RGBA16F, RGBA32F; sRGB encoding is supported on RGBA8 only |
| packTexture | Explicit precision/encoding conversion and optional linear-light association; rejects range overflow |
| makeOpaqueTexture | Independent opaque mip preparation from straight source RGB; rejects associated input |
| TextureRole | Color (linear RGB, straight coverage), Emission (linear RGB, ignore source alpha), Normal (XYZ encoded in [0,1]), Data (numerical channels), Environment (linear HDR) |
| makeTexture | Convert byte color/data and optionally build area-filtered mips; no implicit gamma correction of data |
| Texture(role, levels) | Explicit provided mip chain; makeTexture rejects MipPolicy::Provided because one level cannot describe it |
| SamplerProps | Independent minification/magnification, no/nearest/linear mip filtering, U/V addressing, anisotropy 1..16 |
| TextureBinding / UVTransform | Immutable resource plus per-binding sampler, UV set 0/1, offset/scale/rotation in radians |
| MetallicRoughnessProps | Floating-point factors and five independent texture bindings |
| MaterialProps::pbr | Present selects metallic–roughness; absent selects unlit baseColor plus either baseColorImage or colorTexture |
| SceneRenderProps::Lighting | Direction **toward** the directional light, RGB irradiance, prepared environment handles and intensity |
| SceneRenderProps::exposure/toneMap | Linear multiplier and opt-in Khronos PBR Neutral compression before composition; not an HDR display mode |
| SceneViewProps / SceneViewPatch | Expose lighting, exposure and toneMap alongside existing camera/viewport properties |
| EnvironmentProps / prepareEnvironment | Bounded deterministic CPU convolution and BRDF integration, with cooperative stop between rows |
| Playback | Instance-owned seconds/rate/pause; loop or clamp; explicit seek and advance |
| AnimationClip / TransformTrack | Immutable after model publication; model-local node index, TRS path, interpolation, seconds and key values |
| ModelAsset::clips/applyAnimation | Sample a clip into an instance; reject a different model identity or stale targets before writes |
| FlipbookProps / flipbookFrame | Frame grid/count/FPS and inset; optional pixel cells for nondivisible/cropped atlases; returns a UVTransform |
| billboard | Camera-facing transform for a local XY quad; no renderer-owned motion or simulation |
| MeshPrepareProps / prepareMesh | Attribute generation, scratch admission, exact reindexing and transactional replacement |
| MeshPreparationStats | Source/corner/final counts, payload bytes and conservative scratch estimate |
| PreparationBudget | Shared admission leases, current/peak estimated bytes; immediate refusal, no hidden waiting |

Authored unlit PaintImage support is retained for existing CPU/GPU artwork users;
numerical Texture is the import/material path. A material cannot supply conflicting
image/binding/PBR representations. PBR ignores the unlit baseColor field and uses
its own floating-point baseColor. Set the matching property group deliberately.

Texture interpretation belongs to resource preparation, not UI painting. Color
texels are decoded to linear light before mip filtering; normal and numerical
maps are not gamma corrected or alpha premultiplied. Samplers retain independent
minification, magnification, mip and address policies. Resource accounting includes
all levels. UI and glyph atlases retain their existing preparation policy.
Color mip generation weights RGB by coverage, then stores straight color. GPU
realization associates color RGB in linear light for hardware filtering; the shader
recovers straight RGB before material evaluation. Emission ignores image alpha.
Opaque base-color use selects a separate GPU realization: `makeOpaqueTexture`
sets base-level coverage to one before rebuilding coverage-weighted mip levels.
Source handles remain immutable and can also serve blend/mask materials; both
realizations are independently charged to residency. The original mip count is
retained, including partial chains; already-opaque provided chains are preserved.
Provided chains containing alpha are rebuilt from level zero because discarded
RGB cannot be recovered from their filtered levels. Software previews ignore
coverage before base-level bilinear filtering. Premultiplied PaintImage inputs
cannot recover RGB already lost at zero alpha; use straight source images or
numerical Texture assets when opaque rendering needs that color.
Normal mip levels renormalize XYZ; material/data channels are averaged numerically.
Masked base-color maps receive bounded best-effort alpha-coverage preservation at
the material cutoff, accounting for its constant alpha factor. Vertex-alpha variation
and exact coverage in tiny/discrete mip levels cannot be preserved by this rule.

PNG/JPEG mip storage is RGBA8, retaining sRGB for color/emission and linear bytes
for numerical maps. Radiance HDR defaults to RGBA16F; `decodeHDR` accepts an
explicit RGBA32F storage choice when half precision/range is insufficient.
Authored float levels remain
RGBA32F unless packing is explicitly requested. R8 is opt-in for scalar inputs.
GPU realization uses the matching SDL format and refuses unsupported formats;
it no longer universally narrows float32 to half-float. CPU filtering decodes
working values; GPU sRGB sampling decodes before interpolation. Associated RGB
is calculated in linear space before encoding. The shader ABI is unchanged.

GPU material texture residency has a separate byte-budgeted
LRU; bytes include all levels. `maxMaterialTextureBytes` bounds one realized chain;
`maxMaterialResidentBytes` bounds cached ownership, not all live caller/in-flight
allocations. Transfer-offset alignment is charged to staging, not resident payload.
A full 4K RGBA8 chain is about 85 MiB on both CPU and GPU, compared with 341 MiB
float32 or 171 MiB half-float. Opaque and blend representations still require
separate resident copies when both are requested. Eviction never invalidates
live handles or GPU submissions.

Mip construction keeps completed levels packed and uses one next-level float
buffer, not a retained float pyramid. Repacking/association uses 4096-texel blocks.
Decoding, mesh preparation and GPU staging use a shared 512 MiB estimated admission
budget (`resourcePreparationBudget`). Decoder overloads and MeshPrepareProps can
borrow an explicit alternative domain. Leases release on success, exceptions and
moves. Exhaustion throws: scheduling/retry is an explicit caller responsibility.
Decoder admission follows bounded header inspection, before full decoding.
These are conservative operation estimates, not allocator interception: dependency
allocations, retained handles and GPU completion memory are not measured process
RSS. ModelImportProps::maxPreparationBytes also bounds individual mesh work;
final geometry is charged to the import total. Limits are application safeguards,
not queried free RAM/VRAM.
The native scene adapter reports `meshResidentBytes()` and `textureResidentBytes()`
separately; neither is total device memory. Samplers are reused by exact properties
for the backend lifetime, outside those texture/mesh byte budgets. Avoid continuously
varying sampler descriptions; animate a binding's UV transform instead.

Mesh attributes include normals, tangent handedness, vertex color and two UV sets.
UV selection/transform belongs to each material texture binding. Missing normals
are generated per face; missing tangents use MikkTSpace with temporary corner
splitting. meshoptimizer then reindexes explicit attribute streams (no struct
padding comparison). Position, normal, both UVs, tangent/sign and color all
participate. Exact bit patterns are conservative: signed zero/drift may retain
extra vertices. Triangle sequence/winding remain unchanged, including blended
surfaces. Cache/overdraw reordering, quantization, simplification and 16-bit index
selection are deliberately separate future policies.
Unsupported material extensions, skinning and morph targets remain explicit errors
or documented optional-extension warnings rather than fabricated support.

Environment preparation produces diffuse irradiance, roughness-filtered specular
radiance and a BRDF integration table. These are immutable CPU resources, uploaded
on the renderer owner thread. No disk watcher or dynamic authoring format is added.
The source is latitude–longitude, +Y at the top, +Z at U=.5. Diffuse stores
irradiance divided by pi; specular levels correspond to increasing GGX roughness,
not ordinary image downsampling. A split-sum BRDF table accounts for view angle
and roughness. Defaults are 32-wide diffuse, 128-wide specular, 64-square BRDF,
128 deterministic samples. This is a bounded learning implementation, not a
production-quality offline light baker; narrow bright sources may need more samples.

stb provides HDR decoding and PNG/JPEG header inspection; SDL_image still performs
the PNG/JPEG pixel decode. MikkTSpace supplies the tangent basis, meshoptimizer
1.3 reindexes, and KTX-Software 4.4.2 reads/transcodes KTX2. Revisions are pinned
in CMake and their license notices are installed. Imported mesh colors and PBR
factors are linear; texture color/emission bytes are sRGB, other channels numerical.

### KTX2 input

`decodeKTX2` accepts bounded 2D, single-face, non-array textures: R8, RGBA8
linear/sRGB, RGBA16F, RGBA32F, and Basis ETC1S/UASTC transcoded to RGBA8. The glTF
importer routes `KHR_texture_basisu` sources through the same decoder. Direct BC/
ASTC GPU payload residency, encoding tools, cube/volume/array textures and disk
conversion pipelines are not implemented. SDL retains native upload ownership.

Dimensions, mip totals and encoded/inflated bytes are checked before image load.
Associated input, non-right/down orientation, nontrivial swizzles, unsupported
transfer functions/primaries and numerical sRGB content are rejected. No implicit
color management, flipping or channel remapping is promised. Provided mips are
retained; decodeTexture with None discards tails, and Generate builds a tail for
a single-level container. decodeKTX2 retains the authored level count. Failure
never publishes a partial texture. KTX's internal KTX1 reader is compiled to
satisfy static-link dependencies; the project decoder accepts KTX2 only.

Rigid animation uses immutable model-local TRS tracks and per-instance playback.
STEP, LINEAR (quaternion slerp) and cubic Hermite interpolation are specified by
glTF, not by the simulation pose-history interpolator. Playback changes authored
scene transforms. Skinning, VAT and morph animation are deferred. Flipbooks use
the same explicit time/loop convention, with atlas rectangles per instance; atlas
mip generation must not mix neighboring animation frames.
Playback time is supplied by the application: use fixed ticks for simulation-coupled
motion, presentation delta for decorative motion, or explicit seek for inspection.
The renderer never advances animation. Clip application begins from the model's
rest pose, then applies tracks; procedural edits should run **after** clip sampling.
No blending, events/root motion, skinning or VAT is implied. Cubic rotation results
are normalized after interpolation; unlike LINEAR slerp, cubic tangents must not be
independently sign-flipped. Direct mutable track users must validate after editing.

## Demo 3D

Install the project and launch it normally; choose **Demo 3D** or press **2**. CPU model,
HDR and smoke preparation run as one bounded worker job. It owns captured catalog
and result state, not AppContext or UI pointers. Exit cancels the job; completion
is polled only by the living app. GPU realization happens during owner-thread scene
preparation. Renderer recreation reuses CPU assets; no native resources are stored
in the app model.

Arrow keys orbit, W/S adjust distance, Q/E adjust exposure, L toggles direct light,
Space pauses the smoke, Escape opens shared settings; Ctrl/Cmd+Shift+M returns to the menu. Environment illumination remains
when direct light is disabled. Asset provenance and the cropped smoke-row detail
are in [the asset manifest](../../assets/demo3d/README.md).

## Verification

Focused constituent tests cover texture conversion/mips/budgets, tangent generation,
UV transforms, animation interpolation/seeking/invalid tracks/stale instances,
and capability refusal. Opt-in Vulkan tests exercise material output and scene
composition; build success alone is not visual verification. Demo 3D uses
a CC0 BoomBox prop, CC0 smoke atlas and CC0 studio environment, with provenance
stored beside the assets. Lighting/PBR is in scope; shadows, skinning and VAT are not.
`gpu_materials` checks directional response, normal perturbation, HDR emission,
exposure with/without tone mapping, environment-lit metal, mip-level selection, per-instance UVs and
alpha masks, then renders the real prop.
The alpha regressions cover zero-alpha opaque PBR, shared opaque/blend/mask
realizations, bilinear/mip filtering and straight PaintImage sampling on GPU
and software. `texture_materials` also checks partial and authored opaque chains.
The optional first executable argument to `gpu_materials`
is a BMP capture path. These checks are not a calibrated reference-viewer comparison;
broader driver testing, complex UV/tangent seam visual cases and appearance matching
remain verification work, not claims established by the unit suite.
`software_materials` checks exposure/alpha preservation, explicit preview conversion
and refusal of unsupported tone mapping/PBR.
`texture_storage` covers compact/half/full precision, admission and association;
`mesh_preparation` covers seams, order, deterministic remapping and atomic failure;
`ktx_texture` covers synthetic containers, malformed input, limits and ETC1S/UASTC
fixtures from the pinned dependency (no network during tests).
