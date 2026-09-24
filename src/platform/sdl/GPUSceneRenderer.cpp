#include <algorithm>
#include <cstring>
#include <limits>

#include "GPUSceneRenderer.hpp"

namespace playground::sdl::gpu_detail {

GPUSceneRenderer::GPUSceneRenderer(PaintDevice &device)
    : _device{device}, _vertex{loadShader(device.device, "scene_unlit_vertex",
                                          SDL_GPU_SHADERSTAGE_VERTEX, 0, 1)},
      _fragment{loadShader(device.device, "scene_unlit_fragment",
                           SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1)},
      _sampler{sampler(device.device)}, _uploads{device.device} {
  device.device->checkOwnerThread();
  if (!SDL_GPUTextureSupportsFormat(
          device.device->get(), SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
          SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET))
    throw std::runtime_error(
        "GPU does not support required scene depth attachments");
  const SDL_GPUVertexBufferDescription buffer{
      0, sizeof(scene::Vertex3D), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0};
  const std::array<SDL_GPUVertexAttribute, 3> attributes{
      {{0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
        offsetof(scene::Vertex3D, position)},
       {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
        offsetof(scene::Vertex3D, normal)},
       {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
        offsetof(scene::Vertex3D, uv)}}};
  SDL_GPUColorTargetDescription target{SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT,
                                       blend()};
  for (int variant = 0; variant < 9; ++variant) {
    const int alpha = variant % 3;
    const int sidedness = variant / 3;
    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = _vertex.get();
    info.fragment_shader = _fragment.get();
    info.vertex_input_state = {&buffer, 1, attributes.data(),
                               Uint32(attributes.size())};
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    info.rasterizer_state.cull_mode =
        sidedness == 0 ? SDL_GPU_CULLMODE_NONE : SDL_GPU_CULLMODE_BACK;
    // Scene fronts face -Z in our LH convention. SDL's flipped Vulkan viewport
    // makes this CLOCKWISE; a reflected model reverses the declared winding.
    info.rasterizer_state.front_face = sidedness == 2
                                           ? SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE
                                           : SDL_GPU_FRONTFACE_CLOCKWISE;
    info.rasterizer_state.enable_depth_clip = true;
    info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    info.depth_stencil_state.enable_depth_test = true;
    info.depth_stencil_state.enable_depth_write = alpha != 2;
    info.target_info = {&target, 1, SDL_GPU_TEXTUREFORMAT_D32_FLOAT, true};
    auto *nativePipeline =
        SDL_CreateGPUGraphicsPipeline(device.device->get(), &info);
    if (!nativePipeline)
      throwSDLError("Cannot create 3D GPU pipeline");
    _pipelines[variant] = {device.device, nativePipeline};
  }
}

void GPUSceneRenderer::pruneMeshes() {
  std::erase_if(_meshes, [&](const auto &entry) {
    if (!entry.first.expired())
      return false;
    _residentBytes -= entry.second.bytes;
    return true;
  });
  if (_clock == std::numeric_limits<std::uint64_t>::max()) {
    _meshes.clear();
    _residentBytes = 0;
    _clock = 0;
  }
}

std::shared_ptr<const GPUSceneRenderer::Mesh>
GPUSceneRenderer::mesh(scene::MeshHandle source) {
  if (auto it = _meshes.find(source); it != _meshes.end()) {
    it->second.lastUse = ++_clock;
    return it->second.mesh;
  }
  if (source->data().vertices.size() >
          std::numeric_limits<Uint32>::max() / sizeof(scene::Vertex3D) ||
      source->data().indices.size() >
          std::numeric_limits<Uint32>::max() / sizeof(std::uint32_t))
    throw std::length_error("GPU mesh exceeds transfer capacity");
  const auto vertices =
      source->data().vertices.size() * sizeof(scene::Vertex3D);
  const auto indices = source->data().indices.size() * sizeof(std::uint32_t);
  if (indices > std::numeric_limits<Uint32>::max() - vertices)
    throw std::length_error("GPU mesh exceeds combined transfer capacity");
  _device.device->limits().validateUpload(vertices + indices);
  SDL_GPUBufferCreateInfo vertexInfo{SDL_GPU_BUFFERUSAGE_VERTEX,
                                     Uint32(vertices)};
  SDL_GPUBufferCreateInfo indexInfo{SDL_GPU_BUFFERUSAGE_INDEX, Uint32(indices)};
  auto device = _device.device;
  Mesh result{{device, SDL_CreateGPUBuffer(device->get(), &vertexInfo)},
              {device, SDL_CreateGPUBuffer(device->get(), &indexInfo)},
              Uint32(source->data().indices.size())};
  auto *mapped = static_cast<std::byte *>(_uploads.map(vertices + indices));
  std::memcpy(mapped, source->data().vertices.data(), vertices);
  std::memcpy(mapped + vertices, source->data().indices.data(), indices);
  _uploads.unmap();
  Commands commands{device, "mesh upload"};
  auto *copy = SDL_BeginGPUCopyPass(commands.value);
  if (!copy)
    throwSDLError("Cannot begin mesh upload");
  const SDL_GPUTransferBufferLocation vertexSource{_uploads.get(), 0},
      indexSource{_uploads.get(), Uint32(vertices)};
  const SDL_GPUBufferRegion vertexTarget{result.vertices.get(), 0,
                                         Uint32(vertices)},
      indexTarget{result.indices.get(), 0, Uint32(indices)};
  SDL_UploadToGPUBuffer(copy, &vertexSource, &vertexTarget, false);
  SDL_UploadToGPUBuffer(copy, &indexSource, &indexTarget, false);
  SDL_EndGPUCopyPass(copy);
  commands.submit();
  auto published = std::make_shared<Mesh>(std::move(result));
  const auto bytes = vertices + indices;
  const auto budget = _device.device->limits().maxMeshResidentBytes;
  if (bytes <= budget) {
    while (_residentBytes > budget - bytes) {
      auto oldest = std::min_element(
          _meshes.begin(), _meshes.end(), [](const auto &a, const auto &b) {
            return a.second.lastUse < b.second.lastUse;
          });
      if (oldest == _meshes.end())
        break;
      _residentBytes -= oldest->second.bytes;
      _meshes.erase(oldest);
    }
    _meshes.emplace(source, MeshEntry{published, bytes, ++_clock});
    _residentBytes += bytes;
  }
  return published;
}

rendering::PaintImageHandle
GPUSceneRenderer::render(const scene::SceneRenderProps &view,
                         std::span<const scene::MeshDraw> draws) {
  _device.device->checkOwnerThread();
  scene::validate(view, draws);
  // Combined live color/depth allocation, not merely the color pixel count.
  _device.device->limits().validateTarget(view.pixelSize, 8 + 4);
  pruneMeshes();
  struct Prepared {
    std::shared_ptr<const Mesh> mesh;
    math::Matrix4 matrix;
    std::array<float, 12> material;
    rendering::PaintImageHandle image;
    int pipeline;
  };
  std::vector<Prepared> prepared;
  for (const auto &draw : draws) {
    auto image = draw.material.baseColorImage
                     ? _device.images.prepare(draw.material.baseColorImage)
                     : _device.white;
    const auto tint = math::toLinear(draw.material.baseColor);
    const auto &m = draw.model;
    const double determinant =
        double(m.at(0, 0)) * (double(m.at(1, 1)) * m.at(2, 2) -
                              double(m.at(1, 2)) * m.at(2, 1)) -
        double(m.at(0, 1)) * (double(m.at(1, 0)) * m.at(2, 2) -
                              double(m.at(1, 2)) * m.at(2, 0)) +
        double(m.at(0, 2)) *
            (double(m.at(1, 0)) * m.at(2, 1) - double(m.at(1, 1)) * m.at(2, 0));
    const int sidedness = draw.material.doubleSided ? 0
                          : determinant < 0         ? 2
                                                    : 1;
    prepared.push_back(
        {mesh(draw.mesh),
         view.camera.projection * view.camera.view * draw.model,
         {tint.r, tint.g, tint.b, tint.a,
          image->alphaMode() == rendering::AlphaMode::Premultiplied ? 1.f : 0.f,
          image->colorEncoding() == rendering::ColorEncoding::SRGB ? 1.f : 0.f,
          float(draw.material.alpha), draw.material.alphaCutoff,
          draw.material.sampling == rendering::Sampling::Linear ? 1.f : 0.f,
          float(draw.material.addressU), float(draw.material.addressV), 0},
         std::move(image),
         int(draw.material.alpha) + sidedness * 3});
  }
  auto target = _device.targets.color(view.pixelSize);
  auto depth = _device.targets.depth(view.pixelSize);
  Commands commands{_device.device, "scene3d"};
  _device.device->recordTexture(commands.value, target->get());
  _device.device->recordTexture(commands.value, depth->texture.get());
  for (const auto &draw : prepared)
    _device.device->recordTexture(
        commands.value, static_cast<const GPUImage &>(*draw.image).get());
  const auto clear = math::premultiply(math::toLinear(view.clearColor));
  SDL_GPUColorTargetInfo colorInfo{};
  colorInfo.texture = target->get();
  colorInfo.clear_color = {clear.r, clear.g, clear.b, clear.a};
  colorInfo.load_op = SDL_GPU_LOADOP_CLEAR;
  colorInfo.store_op = SDL_GPU_STOREOP_STORE;
  SDL_GPUDepthStencilTargetInfo depthInfo{};
  depthInfo.texture = depth->texture.get();
  depthInfo.clear_depth = 1;
  depthInfo.load_op = SDL_GPU_LOADOP_CLEAR;
  depthInfo.store_op = SDL_GPU_STOREOP_DONT_CARE;
  depthInfo.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
  depthInfo.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
  auto *pass =
      SDL_BeginGPURenderPass(commands.value, &colorInfo, 1, &depthInfo);
  if (!pass)
    throwSDLError("Cannot begin GPU scene pass");
  for (const auto &draw : prepared) {
    SDL_BindGPUGraphicsPipeline(pass, _pipelines[draw.pipeline].get());
    SDL_PushGPUVertexUniformData(commands.value, 0, draw.matrix.elements.data(),
                                 sizeof(math::Matrix4));
    SDL_PushGPUFragmentUniformData(commands.value, 0, draw.material.data(),
                                   sizeof(draw.material));
    const SDL_GPUTextureSamplerBinding image{
        static_cast<const GPUImage &>(*draw.image).get(), _sampler.get()};
    SDL_BindGPUFragmentSamplers(pass, 0, &image, 1);
    const SDL_GPUBufferBinding vertices{draw.mesh->vertices.get(), 0},
        indices{draw.mesh->indices.get(), 0};
    SDL_BindGPUVertexBuffers(pass, 0, &vertices, 1);
    SDL_BindGPUIndexBuffer(pass, &indices, SDL_GPU_INDEXELEMENTSIZE_32BIT);
    SDL_DrawGPUIndexedPrimitives(pass, draw.mesh->count, 1, 0, 0, 0);
  }
  SDL_EndGPURenderPass(pass);
  commands.submit();
  return target;
}

} // namespace playground::sdl::gpu_detail
