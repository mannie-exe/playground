#include <algorithm>
#include <chrono>
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
  const std::array<SDL_GPUVertexAttribute, 4> attributes{
      {{0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
        offsetof(scene::Vertex3D, position)},
       {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
        offsetof(scene::Vertex3D, normal)},
       {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
        offsetof(scene::Vertex3D, uv)},
       {3, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
        offsetof(scene::Vertex3D, color)}}};
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
  _pbrVertex = loadShader(device.device, "scene_pbr_vertex",
                          SDL_GPU_SHADERSTAGE_VERTEX, 0, 1);
  _pbrFragment = loadShader(device.device, "scene_pbr_fragment",
                            SDL_GPU_SHADERSTAGE_FRAGMENT, 8, 1);
  const std::array<SDL_GPUVertexAttribute, 6> pbrAttributes{
      {{0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
        offsetof(scene::Vertex3D, position)},
       {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
        offsetof(scene::Vertex3D, normal)},
       {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
        offsetof(scene::Vertex3D, uv)},
       {3, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
        offsetof(scene::Vertex3D, tangent)},
       {4, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
        offsetof(scene::Vertex3D, color)},
       {5, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
        offsetof(scene::Vertex3D, uv1)}}};
  for (int variant = 0; variant < 9; ++variant) {
    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = _pbrVertex.get();
    info.fragment_shader = _pbrFragment.get();
    info.vertex_input_state = {&buffer, 1, pbrAttributes.data(),
                               Uint32(pbrAttributes.size())};
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    info.rasterizer_state.cull_mode =
        variant / 3 == 0 ? SDL_GPU_CULLMODE_NONE : SDL_GPU_CULLMODE_BACK;
    info.rasterizer_state.front_face = variant / 3 == 2
                                           ? SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE
                                           : SDL_GPU_FRONTFACE_CLOCKWISE;
    info.rasterizer_state.enable_depth_clip = true;
    info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    info.depth_stencil_state.enable_depth_test = true;
    info.depth_stencil_state.enable_depth_write = variant % 3 != 2;
    info.target_info = {&target, 1, SDL_GPU_TEXTUREFORMAT_D32_FLOAT, true};
    _pbrPipelines[variant] = {device.device, SDL_CreateGPUGraphicsPipeline(
                                                 device.device->get(), &info)};
  }
  _toneVertex = loadShader(device.device, "scene_tone_vertex",
                           SDL_GPU_SHADERSTAGE_VERTEX, 0, 0);
  _toneFragment = loadShader(device.device, "scene_tone_fragment",
                             SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1);
  SDL_GPUGraphicsPipelineCreateInfo tone{};
  tone.vertex_shader = _toneVertex.get();
  tone.fragment_shader = _toneFragment.get();
  tone.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  tone.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
  SDL_GPUColorTargetDescription toneTarget{};
  toneTarget.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
  tone.target_info.color_target_descriptions = &toneTarget;
  tone.target_info.num_color_targets = 1;
  _tonePipeline = {device.device,
                   SDL_CreateGPUGraphicsPipeline(device.device->get(), &tone)};
  const auto solid = [&device](math::Vec4f p) {
    return rendering::makeTexture({{1, 1}, {p}}, rendering::TextureRole::Data,
                                  rendering::MipPolicy::None, 16,
                                  device.device->resources());
  };
  _whiteTexture = solid({1, 1, 1, 1});
  _normalTexture = solid({.5f, .5f, 1, 1});
  _blackTexture = solid({0, 0, 0, 1});
}

std::shared_ptr<GPUTextureData>
GPUSceneRenderer::texture(const rendering::TextureHandle &source,
                          bool ignoreAlpha) {
  const TextureKey key{
      source, ignoreAlpha && source->role() == rendering::TextureRole::Color &&
                  !source->opaque()};
  if (auto it = _textures.find(key); it != _textures.end()) {
    ++_work.textureHits;
    it->second.lastUse = ++_clock;
    return it->second.texture;
  }
  ++_work.textureMisses;
  auto result = std::make_shared<GPUTextureData>(_device.device, *source,
                                                 key.ignoreAlpha);
  const auto bytes = result->bytes();
  _textures.emplace(key, TextureEntry{result, bytes, ++_clock});
  _textureBytes += bytes;
  ++_work.uploads;
  _work.uploadBytes += bytes;
  return result;
}

SDL_GPUSampler *
GPUSceneRenderer::textureSampler(const rendering::SamplerProps &props) {
  for (auto &entry : _samplers)
    if (entry.first == props)
      return entry.second.get();
  const auto filter = [](rendering::Sampling f) {
    return f == rendering::Sampling::Nearest ? SDL_GPU_FILTER_NEAREST
                                             : SDL_GPU_FILTER_LINEAR;
  };
  const auto address = [](rendering::TextureAddress a) {
    switch (a) {
    case rendering::TextureAddress::Repeat:
      return SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    case rendering::TextureAddress::MirroredRepeat:
      return SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT;
    default:
      return SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    }
  };
  SDL_GPUSamplerCreateInfo info{};
  info.min_filter = filter(props.minification);
  info.mag_filter = filter(props.magnification);
  info.mipmap_mode = props.mip == rendering::MipFilter::Linear
                         ? SDL_GPU_SAMPLERMIPMAPMODE_LINEAR
                         : SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
  info.address_mode_u = address(props.addressU);
  info.address_mode_v = address(props.addressV);
  info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  info.max_lod = props.mip == rendering::MipFilter::None ? 0 : 32;
  info.max_anisotropy = props.anisotropy;
  info.enable_anisotropy = props.anisotropy > 1;
  _samplers.emplace_back(
      props, Sampler{_device.device,
                     SDL_CreateGPUSampler(_device.device->get(), &info)});
  return _samplers.back().second.get();
}

void GPUSceneRenderer::pruneResources() {
  std::erase_if(_plans, [](const auto &p) { return p.first.expired(); });
  _currentPlan = {};
  std::erase_if(_textures, [&](const auto &e) {
    if (!e.first.source.expired())
      return false;
    _textureBytes -= e.second.bytes;
    return true;
  });
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
    _textures.clear();
    _textureBytes = 0;
  }
}

std::shared_ptr<const GPUSceneRenderer::Mesh>
GPUSceneRenderer::mesh(scene::MeshHandle source) {
  if (auto it = _meshes.find(source); it != _meshes.end()) {
    ++_work.meshHits;
    it->second.lastUse = ++_clock;
    return it->second.mesh;
  }
  ++_work.meshMisses;
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
  _device.device->limits().validateUpload(vertices + indices,
                                          "GPU mesh upload");
  SDL_GPUBufferCreateInfo vertexInfo{SDL_GPU_BUFFERUSAGE_VERTEX,
                                     Uint32(vertices)};
  SDL_GPUBufferCreateInfo indexInfo{SDL_GPU_BUFFERUSAGE_INDEX, Uint32(indices)};
  auto device = _device.device;
  Mesh result{createBuffer(device, vertexInfo, rendering::ResourceKind::Mesh),
              createBuffer(device, indexInfo, rendering::ResourceKind::Mesh),
              Uint32(source->data().indices.size())};
  auto *mapped = static_cast<std::byte *>(_uploads.map(vertices + indices));
  std::memcpy(mapped, source->data().vertices.data(), vertices);
  std::memcpy(mapped + vertices, source->data().indices.data(), indices);
  _uploads.unmap();
  Commands commands{device, "mesh upload"};
  _uploads.record(commands.value);
  device->recordUse(commands.value, result.vertices.use());
  device->recordUse(commands.value, result.indices.use());
  auto *copy = SDL_BeginGPUCopyPass(commands.value);
  if (!copy)
    throwRenderError("Cannot begin mesh upload",
                     rendering::RenderOperation::Record);
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
  _meshes.emplace(source, MeshEntry{published, bytes, ++_clock});
  _residentBytes += bytes;
  ++_work.uploads;
  _work.uploadBytes += bytes;
  return published;
}

bool GPUSceneRenderer::pinned(const TextureKey &key) const {
  if (_currentPlan.textures.contains(key))
    return true;
  for (const auto &[owner, plan] : _plans)
    if (!owner.expired() && plan.textures.contains(key))
      return true;
  return false;
}

bool GPUSceneRenderer::pinned(const MeshKey &key) const {
  if (_currentPlan.meshes.contains(key))
    return true;
  for (const auto &[owner, plan] : _plans)
    if (!owner.expired() && plan.meshes.contains(key))
      return true;
  return false;
}

void GPUSceneRenderer::trim(bool reclaim) {
  const auto evict = [&](auto &entries, std::size_t &bytes,
                         std::size_t target) {
    std::size_t idle{};
    for (const auto &[key, entry] : entries)
      if (!pinned(key))
        idle += entry.bytes;
    while (idle > target) {
      auto oldest = entries.end();
      for (auto it = entries.begin(); it != entries.end(); ++it)
        if (!pinned(it->first) && (oldest == entries.end() ||
                                   it->second.lastUse < oldest->second.lastUse))
          oldest = it;
      if (oldest == entries.end())
        break;
      idle -= oldest->second.bytes;
      bytes -= oldest->second.bytes;
      entries.erase(oldest);
      ++_work.evictions;
    }
  };
  evict(_textures, _textureBytes,
        reclaim ? 0 : _device.device->limits().maxMaterialResidentBytes);
  evict(_meshes, _residentBytes,
        reclaim ? 0 : _device.device->limits().maxMeshResidentBytes);
}

void GPUSceneRenderer::plan(const scene::SceneRenderProps &view,
                            std::span<const scene::MeshDraw> draws) {
  ResourcePlan next;
  const auto texture = [&](const rendering::TextureHandle &t,
                           bool opaque = false) {
    if (t)
      next.textures.insert({t, opaque &&
                                   t->role() == rendering::TextureRole::Color &&
                                   !t->opaque()});
  };
  for (const auto &d : draws) {
    next.meshes.insert(d.mesh);
    const auto &m = d.material;
    const bool opaque = m.alpha == scene::MaterialProps::Alpha::Opaque;
    texture(m.colorTexture.texture, opaque);
    if (m.pbr) {
      texture(m.pbr->baseColorTexture.texture, opaque);
      texture(m.pbr->normalTexture.texture);
      texture(m.pbr->metallicRoughnessTexture.texture);
      texture(m.pbr->occlusionTexture.texture);
      texture(m.pbr->emissiveTexture.texture);
    }
  }
  texture(view.lighting.diffuseEnvironment);
  texture(view.lighting.specularEnvironment);
  texture(view.lighting.brdf);
  texture(_whiteTexture);
  texture(_normalTexture);
  texture(_blackTexture);
  _currentPlan = std::move(next);
  if (view.resourceOwner)
    _plans[view.resourceOwner] = _currentPlan;
  trim();
  std::size_t missing{}, required{};
  const auto add = [&](std::size_t bytes, bool absent) {
    if (bytes > std::numeric_limits<std::size_t>::max() - required)
      throw std::overflow_error("Scene resource plan overflow");
    required += bytes;
    if (absent)
      missing += bytes;
  };
  for (const auto &key : _currentPlan.textures) {
    const auto source = key.source.lock();
    const auto size = source->levels().front().size;
    const auto &limits = _device.device->limits();
    if (unsigned(size.x) > limits.maxTextureDimension ||
        unsigned(size.y) > limits.maxTextureDimension ||
        source->bytes() > limits.maxMaterialTextureBytes)
      throw std::length_error("Scene material exceeds allocation policy");
    std::size_t transfer{};
    for (const auto &level : source->levels()) {
      if (transfer > std::numeric_limits<Uint32>::max() - 15)
        throw std::length_error(
            "Scene texture transfer exceeds native capacity");
      transfer = (transfer + 15) & ~std::size_t{15};
      if (level.texels.data().size() >
          std::numeric_limits<Uint32>::max() - transfer)
        throw std::length_error(
            "Scene texture transfer exceeds native capacity");
      transfer += level.texels.data().size();
    }
    limits.validateUpload(transfer, "Scene texture upload plan");
    add(source->bytes(), !_textures.contains(key));
  }
  for (const auto &key : _currentPlan.meshes) {
    const auto source = key.lock();
    const auto bytes =
        source->data().vertices.size() * sizeof(scene::Vertex3D) +
        source->data().indices.size() * sizeof(std::uint32_t);
    _device.device->limits().validateUpload(bytes, "Scene mesh upload plan");
    add(bytes, !_meshes.contains(key));
  }
  _work.workingSetBytes = required;
  try {
    // GPU allocation is owner-thread-only. Probe the whole missing set before
    // uploads; individual creation still reserves its own charge below.
    if (missing)
      (void)_device.device->resources()->reserve(
          rendering::MemoryClass::GPU, rendering::ResourceKind::Texture,
          missing, "Scene working set");
  } catch (...) {
    ++_work.refusedPlans;
    throw;
  }
}

rendering::PaintImageHandle
GPUSceneRenderer::render(const scene::SceneRenderProps &view,
                         std::span<const scene::MeshDraw> draws) {
  _device.device->checkOwnerThread();
  scene::validate(view, draws);
  // Combined live color/depth allocation, not merely the color pixel count.
  _device.device->limits().validateTarget(view.pixelSize, 8 + 4,
                                          "GPU scene color and depth");
  pruneResources();
  const auto preparationStarted = std::chrono::steady_clock::now();
  // Admit required attachments before mesh/material preparation can submit.
  auto attachments = _device.targets.sceneTargets(
      view.pixelSize, view.toneMap || view.exposure != 1);
  auto target = std::move(attachments.color);
  auto depth = std::move(attachments.depth);
  auto output = std::move(attachments.output);
  plan(view, draws);

  struct Prepared {
    std::shared_ptr<const Mesh> mesh;
    math::Matrix4 matrix;
    std::array<float, 12> material;
    rendering::PaintImageHandle image;
    int pipeline;
    bool modern{};
    std::array<math::Matrix4, 3> matrices;
    std::array<math::Vec4f, 17> uniforms{};
    std::array<std::shared_ptr<GPUTextureData>, 8> textures;
    std::array<SDL_GPUTextureSamplerBinding, 8> bindings{};
  };

  if (draws.size() > std::numeric_limits<std::size_t>::max() / sizeof(Prepared))
    throw std::overflow_error("Scene preparation size overflow");
  auto preparation = draws.empty() ? rendering::ResourceLedger::Token{}
                                   : _device.device->resources()->reserve(
                                         rendering::MemoryClass::CPU,
                                         rendering::ResourceKind::Preparation,
                                         sizeof(Prepared) * draws.size(),
                                         "Scene draw preparation");
  std::vector<Prepared> prepared;
  prepared.reserve(draws.size());
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
    // Zero-scale authored nodes are valid; collapsed geometry has no normal
    // matrix and contributes no surface to this triangle renderer.
    if (determinant == 0)
      continue;
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
    auto &p = prepared.back();
    p.modern =
        bool(draw.material.pbr) || bool(draw.material.colorTexture.texture);
    if (p.modern) {
      auto normal = math::inverse(draw.model);
      for (int row = 0; row < 4; ++row)
        for (int col = row + 1; col < 4; ++col)
          std::swap(normal.at(row, col), normal.at(col, row));
      p.matrices = {p.matrix, draw.model, normal};
      const auto &m = draw.material;
      const auto camera =
          math::transformPoint(math::inverse(view.camera.view), {});
      const auto &light = view.lighting;
      p.uniforms[0] = {tint.r, tint.g, tint.b, tint.a};
      p.uniforms[3] = {
          float(m.alpha), m.alphaCutoff, 0,
          light.specularEnvironment
              ? float(light.specularEnvironment->levels().size() - 1)
              : 0};
      p.uniforms[4] = {camera.x, camera.y, camera.z,
                       light.environmentIntensity};
      p.uniforms[5] = {light.directionToLight.x, light.directionToLight.y,
                       light.directionToLight.z, determinant < 0 ? -1.f : 1.f};
      p.uniforms[6] = {light.irradiance.x, light.irradiance.y,
                       light.irradiance.z, 0};
      std::array<rendering::TextureBinding, 8> bindings{};
      bindings[0] = m.colorTexture;
      if (m.pbr) {
        const auto &a = *m.pbr;
        p.uniforms[0] = a.baseColor;
        p.uniforms[1] = {a.metallic, a.roughness, a.normalScale,
                         a.occlusionStrength};
        p.uniforms[2] = {a.emissive.x, a.emissive.y, a.emissive.z, 1};
        p.uniforms[3].z = a.normalTexture.texture ? 1.f : 0;
        bindings[0] = a.baseColorTexture;
        bindings[1] = a.metallicRoughnessTexture;
        bindings[2] = a.normalTexture;
        bindings[3] = a.occlusionTexture;
        bindings[4] = a.emissiveTexture;
      }
      bindings[5].texture = light.diffuseEnvironment;
      bindings[6].texture = light.specularEnvironment;
      bindings[7].texture = light.brdf;
      for (int i = 5; i < 8; ++i) {
        bindings[i].sampler.addressU = i == 7
                                           ? rendering::TextureAddress::Clamp
                                           : rendering::TextureAddress::Repeat;
        bindings[i].sampler.addressV = rendering::TextureAddress::Clamp;
      }
      for (int i = 0; i < 8; ++i) {
        auto &b = bindings[i];
        p.textures[i] =
            texture(b.texture ? b.texture
                              : (i == 2   ? _normalTexture
                                 : i >= 5 ? _blackTexture
                                          : _whiteTexture),
                    i == 0 && m.alpha == scene::MaterialProps::Alpha::Opaque);
        p.bindings[i] = {p.textures[i]->get(), textureSampler(b.sampler)};
        if (i < 5) {
          const auto &t = b.transform;
          const float c = std::cos(t.rotation), s = std::sin(t.rotation);
          p.uniforms[7 + i * 2] = {c * t.scale.x, -s * t.scale.y, t.offset.x,
                                   float(b.uvSet)};
          p.uniforms[8 + i * 2] = {s * t.scale.x, c * t.scale.y, t.offset.y, 0};
        }
      }
    }
  }
  _work.preparationMilliseconds +=
      std::chrono::duration<double, std::milli>(
          std::chrono::steady_clock::now() - preparationStarted)
          .count();
  Commands commands{_device.device,
                    "scene3d",
                    {.targetPixels = view.pixelSize,
                     .workloadId = view.workloadId,
                     .qualityRevision = view.qualityRevision}};
  _device.device->recordTexture(commands.value, target->get());
  _device.device->recordTexture(commands.value, depth->texture.get());
  for (const auto &draw : prepared) {
    _device.device->recordUse(commands.value, draw.mesh->vertices.use());
    _device.device->recordUse(commands.value, draw.mesh->indices.use());
    _device.device->recordTexture(
        commands.value, static_cast<const GPUImage &>(*draw.image).get());
    if (draw.modern)
      for (const auto &t : draw.textures)
        _device.device->recordTexture(commands.value, t->get());
  }
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
    throwRenderError("Cannot begin GPU scene pass",
                     rendering::RenderOperation::Record);
  for (const auto &draw : prepared) {
    SDL_BindGPUGraphicsPipeline(pass, draw.modern
                                          ? _pbrPipelines[draw.pipeline].get()
                                          : _pipelines[draw.pipeline].get());
    if (draw.modern) {
      SDL_PushGPUVertexUniformData(commands.value, 0, draw.matrices.data(),
                                   sizeof(draw.matrices));
      SDL_PushGPUFragmentUniformData(commands.value, 0, draw.uniforms.data(),
                                     sizeof(draw.uniforms));
      SDL_BindGPUFragmentSamplers(pass, 0, draw.bindings.data(),
                                  Uint32(draw.bindings.size()));
    } else {
      SDL_PushGPUVertexUniformData(commands.value, 0,
                                   draw.matrix.elements.data(),
                                   sizeof(math::Matrix4));
      SDL_PushGPUFragmentUniformData(commands.value, 0, draw.material.data(),
                                     sizeof(draw.material));
      const SDL_GPUTextureSamplerBinding image{
          static_cast<const GPUImage &>(*draw.image).get(), _sampler.get()};
      SDL_BindGPUFragmentSamplers(pass, 0, &image, 1);
    }
    const SDL_GPUBufferBinding vertices{draw.mesh->vertices.get(), 0},
        indices{draw.mesh->indices.get(), 0};
    SDL_BindGPUVertexBuffers(pass, 0, &vertices, 1);
    SDL_BindGPUIndexBuffer(pass, &indices, SDL_GPU_INDEXELEMENTSIZE_32BIT);
    SDL_DrawGPUIndexedPrimitives(pass, draw.mesh->count, 1, 0, 0, 0);
  }
  SDL_EndGPURenderPass(pass);
  commands.submit();
  if (view.toneMap || view.exposure != 1) {
    Commands toneCommands{_device.device,
                          "scene post-processing",
                          {.targetPixels = view.pixelSize,
                           .sourcePixels = view.pixelSize,
                           .workloadId = view.workloadId,
                           .qualityRevision = view.qualityRevision}};
    _device.device->recordTexture(toneCommands.value, target->get());
    _device.device->recordTexture(toneCommands.value, output->get());
    SDL_GPUColorTargetInfo out{};
    out.texture = output->get();
    out.load_op = SDL_GPU_LOADOP_DONT_CARE;
    out.store_op = SDL_GPU_STOREOP_STORE;
    auto *tonePass =
        SDL_BeginGPURenderPass(toneCommands.value, &out, 1, nullptr);
    if (!tonePass)
      throwRenderError("Cannot begin scene tone mapping",
                       rendering::RenderOperation::Record);
    SDL_BindGPUGraphicsPipeline(tonePass, _tonePipeline.get());
    const std::array<float, 4> settings{view.exposure, float(view.toneMap), 0,
                                        0};
    SDL_PushGPUFragmentUniformData(toneCommands.value, 0, settings.data(),
                                   sizeof(settings));
    const SDL_GPUTextureSamplerBinding input{target->get(), _sampler.get()};
    SDL_BindGPUFragmentSamplers(tonePass, 0, &input, 1);
    SDL_DrawGPUPrimitives(tonePass, 3, 1, 0, 0);
    SDL_EndGPURenderPass(tonePass);
    toneCommands.submit();
    return output->publish();
  }
  return target->publish();
}

} // namespace playground::sdl::gpu_detail
