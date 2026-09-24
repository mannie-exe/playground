# Shaders, reflection, and custom pipelines

HLSL is the authored language; SPIR-V/Vulkan is the only packaged hardware ABI.
DXC is a compiler, not a Direct3D rendering dependency. Software rendering does not
require a shader compiler. Runtime reflection is linked independently of tools.

## Offline compilation

Choose one host tool, or the optional vendored shadercross build:

```powershell
cmake --preset debug -DPLAYGROUND_DXC_EXECUTABLE=C:/tools/dxc.exe -DPLAYGROUND_SHADER_FORMATS=SPIRV
cmake --build --preset debug --target playground_shaders
```

Alternatively set `PLAYGROUND_SHADERCROSS_EXECUTABLE` to a host shadercross CLI, or
`PLAYGROUND_BUILD_SHADERCROSS=ON` to compile its vendored dependencies. Do not choose
multiple tools. Cross builds require a host executable. Source shadercross includes
DXC/LLVM and has upstream build prerequisites; it is not fetched by default.

`playground_add_shader(name source stage [dependencies...] [NO_INSTALL])` declares
vertex, fragment, or compute output. Include files must be named dependencies.
Outputs go to `build/<preset>/shaders`; install copies production shaders into
`bin/assets/shaders`. Test fixtures use NO_INSTALL. JSON from shadercross is an
inspection artifact; runtime validation reflects the actual SPIR-V bytes, not JSON.
The existing build-tree fallback is for development, not another installed source.

The upstream shadercross repository has no published tags/releases; its reviewed
source pin is `1ff05bec573988a98ef9e0260b4da44f512b8367`. DXC's reviewed release is
[v1.9.2607](https://github.com/microsoft/DirectXShaderCompiler/releases/tag/v1.9.2607).
SPIRV-Reflect is pinned to SDK tag
[vulkan-sdk-1.4.357.0](https://github.com/KhronosGroup/SPIRV-Reflect/tree/vulkan-sdk-1.4.357.0),
commit `9137a7e529e4e911a4dbd63f1ce71e4b3e8f4cfc`; it publishes SDK tags rather than
formal GitHub releases. Its static library is a private reflection implementation;
public headers contain no SPIRV-Reflect types. Dependency licenses are installed.

## Reflected ABI

`rendering/Shader.hpp` supplies `readSPIRV`, `reflectSPIRV`, `ShaderReflection`,
`ShaderLayout`, `validateShaderLink`, and `isShaderABICompatible`.

| Value | Meaning |
|---|---|
| ShaderStage | Vertex or Fragment for current graphics pipelines |
| ShaderLayout | Sampler, storage-texture, storage-buffer and uniform-buffer slot counts |
| ShaderBinding | Descriptor set/slot/kind/count, uniform byte size and recursively captured block layout |
| ShaderInterface | Explicit location and 32-bit scalar/vector type |
| ShaderReflection | Entry point, stage, bindings and input/output interfaces |

The adapter validates SDL's consecutive stage binding groups: vertex resources in
set0 and uniforms in set1; fragment resources in set2 and uniforms in set3. Images
accessed only through Load cannot be distinguished from samplerless storage-texture
slots solely from their SPIR-V descriptor type, so the authored ShaderLayout remains
explicit and is checked against reflection. Separate HLSL texture/sampler pairs may
share one binding; conflicting unrelated descriptor kinds are rejected.

Each push-uniform block is limited to 4096 bytes, matching the pinned SDL Vulkan
backend's per-slot descriptor range. Reflection rejects larger or empty blocks
before pipeline creation; larger arrays belong in storage buffers, not uniforms.

Graphics ABI currently rejects descriptor arrays, writable storage images, push
constants, specialization constants and matrix/array stage interfaces. Uniform
blocks may contain arrays/matrices: offsets, strides, numeric shapes and padded
sizes participate in compatibility checks. Vertex input descriptions must match
reflected locations/types and fit their buffer stride. Fragment inputs must match
vertex outputs. Reflected shader modules are developer-produced trusted assets;
reflection is not a security sandbox or full SPIR-V validation tool.

Built-in shaders also pass this reflection/layout validation when loaded. Their
resource-count declarations no longer silently accept incompatible bytecode.

## Custom pipeline ownership and reload

`sdl::GPUShaderPipeline` owns props and the current immutable
`GPUPipelineGeneration`. `GPUPipelineProps` owns paths, entry points, declared layouts,
vertex descriptions, color targets, rasterization, multisampling and depth state.
There are no retained pointers into caller-owned creation arrays.

`snapshot()` returns a shared `GPUPipelineHandle` with its device domain and generation
number. Retain that snapshot while recording/submitting work. `bind()` binds it to
a native render pass; `pushUniform()` verifies the supplied byte count against the
reflected padded block size before pushing it to the chosen stage/slot. Native passes
and command buffers must belong to the same device and owner thread.

`poll()` reads both compiled files and compares their bytes with the published pair.
Unchanged files return Unchanged. Changed files are read, reflected, ABI-checked,
linked and used to create a complete replacement before publication. A successful
reload returns Published and advances generation. A failure returns Rejected,
exposes `lastError()`, and preserves the previous handle and bytecode. Old retained
generations remain usable. A shader ABI change requires constructing a new pipeline;
it is not silently applied to existing binding code.

Polling is synchronous and caller-scheduled at a safe boundary, not an OS watcher
or per-draw implicit file read. HLSL is still compiled by the build tool. No runtime
shell command or untrusted compiler invocation is performed. Publish shader pairs
together when changing their interface; failed intermediate builds do not replace
the old pipeline. Native realization/publication stays on the device owner thread.

## Application/frame integration

The neutral RenderFrame API contains no SDL types. `sdl::gpuAccess(frame)` exposes an
optional `GPUFrameAccess` adapter; software returns null. A live GPU frame supplies
its `gpuDevice()` and a managed `renderOffscreen()` callback. It rejects access after
presentation. This lets an IApp create custom native pipelines compatible with the
actual frame instead of accidentally making a separate incompatible device.

```cpp
auto* gpu = playground::sdl::gpuAccess(frame);
if (!gpu)
  throw std::runtime_error("this application requires GPU rendering");
auto device = gpu->gpuDevice();
// Build/cache GPUShaderPipeline on this device. Recreate after domain changes.
auto generation = pipeline.snapshot();
if (generation->resourceDomain() != device->resourceDomain())
  throw std::runtime_error("rebuild custom pipeline for the current device");
auto image = gpu->renderOffscreen({.size = {640, 480}}, [&](auto native) {
  generation->bind(native.pass);
  generation->pushUniform(native.commands,
      playground::rendering::ShaderStage::Fragment, 0,
      std::as_bytes(std::span{uniforms}));
  SDL_DrawGPUPrimitives(native.pass, 3, 1, 0, 0);
});
frame.paint2D().drawImage(image, {{}, image->pixelSize()}, destination, {});
```

The callback borrows an already-open pass and commands. Do not store, end, cancel
or submit them yourself. The adapter closes/submits the producer before returning
its immutable image, and cleans up if the callback throws. Output uses RGBA16_FLOAT,
linear-premultiplied color; optional depth uses D32_FLOAT cleared to1. Pipeline target
formats must match. Caller-owned textures/buffers/pipeline snapshots must survive
through callback submission. Shared handles are lifetime ownership, not fences.
For each sampled project GPUImage, call `native.use(image)` before issuing native
draws; this records its resource-domain-checked lease through completion and keeps
pooled images from being overwritten. The adapter tracks its output/depth targets
automatically. Direct native submission bypasses this contract and is unsupported.

This is an explicit native extension for custom effects and scene passes, not a
generic material graph or permission to mutate a published PaintImage. Software
implementations for custom shaders are application choices. Native work must be
recorded/submitted on the acquiring thread; callbacks cannot move it to workers.

## Verification

`shader_contracts` checks malformed input, descriptor sets/counts, stage linking and
uniform contract failures without a device. Opt-in `gpu_shaders` creates a custom
pipeline, renders/readbacks pixels, verifies unchanged/rejected/successful reload,
draws through retained old generations, composes the result into a real frame and
checks callback/frame-lifetime failures. Missing Vulkan support is a skip, not a
successful hardware verification. Source compilation is a separate build result.

References: [SDL shader ABI](https://wiki.libsdl.org/SDL3/SDL_CreateGPUShader),
[SPIRV-Reflect](https://github.com/KhronosGroup/SPIRV-Reflect),
[SDL_shadercross](https://github.com/libsdl-org/SDL_shadercross),
[DXC SPIR-V sampler conventions](https://github.com/microsoft/DirectXShaderCompiler/wiki/Vulkan-combined-image-sampler-type).
