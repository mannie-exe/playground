#include <algorithm>
#include <cstring>
#include <filesystem>

#include "GPUInternal.hpp"
#include <assets/AssetCatalog.hpp>
#include <platform/FileStore.hpp>
#include <rendering/Shader.hpp>

namespace playground::sdl::gpu_detail {

void UploadStream::trim() {
  std::erase_if(
      _slots, [](const auto &s) { return s.transfer.use().use_count() == 1; });
}

void *UploadStream::map(std::size_t bytes) {
  _device->checkOwnerThread();
  _device->limits().validateUpload(bytes, "GPU stream upload");
  _device->pollCompletions();
  auto it = std::find_if(_slots.begin(), _slots.end(), [&](const auto &s) {
    return s.capacity >= bytes && s.transfer.use().use_count() == 1;
  });
  if (it == _slots.end()) {
    trim();
    if (_slots.size() >= _device->limits().maxInFlightSubmissions)
      throw rendering::ResourcePressure(
          "Upload backing slots", 1, _slots.size(),
          _device->limits().maxInFlightSubmissions,
          rendering::ResourcePressure::Unit::Slots);
    const SDL_GPUTransferBufferCreateInfo info{
        SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, static_cast<Uint32>(bytes)};
    _slots.push_back({createTransfer(_device, info), info.size});
    _current = _slots.size() - 1;
  } else {
    _current = static_cast<std::size_t>(it - _slots.begin());
  }
  auto *mapped = SDL_MapGPUTransferBuffer(_device->get(), get(), false);
  if (!mapped)
    throwSDLError("Cannot map streaming transfer buffer");
  return mapped;
}

void StreamBuffer::trim() {
  _upload.trim();
  std::erase_if(_slots,
                [](const auto &s) { return s.buffer.use().use_count() == 1; });
}

SDL_GPUBuffer *StreamBuffer::write(SDL_GPUCommandBuffer *commands,
                                   std::span<const std::byte> bytes) {
  _device->checkOwnerThread();
  _device->limits().validateUpload(bytes.size(), "GPU stream upload");
  if (bytes.size() > _device->limits().maxStreamBytes)
    throw std::length_error("Draw stream chunk exceeds streaming budget");
  _device->pollCompletions();
  auto it = std::find_if(_slots.begin(), _slots.end(), [&](const auto &s) {
    return s.capacity >= bytes.size() && s.buffer.use().use_count() == 1;
  });
  if (it == _slots.end()) {
    trim();
    if (_slots.size() >= _device->limits().maxInFlightSubmissions)
      throw rendering::ResourcePressure(
          "Draw backing slots", 1, _slots.size(),
          _device->limits().maxInFlightSubmissions,
          rendering::ResourcePressure::Unit::Slots);
    const SDL_GPUBufferCreateInfo info{_usage,
                                       static_cast<Uint32>(bytes.size())};
    _slots.push_back(
        {createBuffer(_device, info, rendering::ResourceKind::Stream),
         info.size});
    it = std::prev(_slots.end());
  }
  auto &buffer = it->buffer;
  _capacity = it->capacity;
  std::memcpy(_upload.map(bytes.size()), bytes.data(), bytes.size());
  _upload.unmap();
  _upload.record(commands);
  _device->recordUse(commands, buffer.use());
  auto *copy = SDL_BeginGPUCopyPass(commands);
  if (!copy)
    throwRenderError("Cannot begin streaming copy pass",
                     rendering::RenderOperation::Record);
  const SDL_GPUTransferBufferLocation source{_upload.get(), 0};
  const SDL_GPUBufferRegion destination{buffer.get(), 0,
                                        static_cast<Uint32>(bytes.size())};
  SDL_UploadToGPUBuffer(copy, &source, &destination, false);
  SDL_EndGPUCopyPass(copy);
  return buffer.get();
}

bool TargetPool::makeRoom(std::size_t bytes) {
  const auto budget = _device->limits().maxTargetPoolBytes;
  if (bytes > budget)
    return false;
  auto removeUnused = [&](auto &entries) {
    std::ranges::stable_sort(entries, [](const auto &a, const auto &b) {
      return a.lastUse < b.lastUse;
    });
    for (auto it = entries.begin();
         it != entries.end() && _stats.retainedBytes > budget - bytes;) {
      if (it->target.use_count() == 1 && !it->target->isLeased()) {
        _stats.retainedBytes -= it->bytes;
        it = entries.erase(it);
        ++_stats.evictions;
      } else
        ++it;
    }
  };
  removeUnused(_colors);
  removeUnused(_depths);
  return _stats.retainedBytes <= budget - bytes;
}

void TargetPool::makeAllocationRoom(std::size_t bytes) {
  const auto cap =
      std::min(_device->limits().maxLivePoolBytes,
               _device->resources()->snapshot().budgets.targetBytes);
  const auto current = liveBytes();
  if (current > cap || bytes > cap - current)
    trim();
  const auto used = liveBytes();
  if (used > cap || bytes > cap - used) {
    ++_stats.pressureFailures;
    throw rendering::ResourcePressure("GPU targets", bytes, used, cap);
  }
}

std::shared_ptr<ColorTarget>
TargetPool::color(math::Vec2i size, std::string_view context,
                  rendering::ResourceLedger::Token reservation) {
  _device->checkOwnerThread();
  _device->pollCompletions();
  trimAged();
  const auto bytes = _device->limits().validateTarget(size, 8, context);
  for (auto &entry : _colors)
    if (entry.target.use_count() == 1 &&
        entry.target->pixelSize() == math::Size2{static_cast<float>(size.x),
                                                 static_cast<float>(size.y)}) {
      if (entry.target->isLeased()) {
        ++_stats.busyMisses;
        continue;
      }
      ++_stats.reuses;
      entry.lastUse = _device->completedSubmission();
      entry.target->beginWrite();
      return entry.target;
    }
  const bool retain = makeRoom(bytes);
  if (!reservation)
    makeAllocationRoom(bytes);
  auto result =
      std::make_shared<ColorTarget>(_device, size, std::move(reservation));
  ++_stats.allocations;
  if (retain) {
    _colors.push_back({result, bytes, _device->completedSubmission()});
    _stats.retainedBytes += bytes;
  }
  return result;
}

std::shared_ptr<DepthTarget>
TargetPool::depth(math::Vec2i size,
                  rendering::ResourceLedger::Token reservation) {
  _device->checkOwnerThread();
  _device->pollCompletions();
  trimAged();
  const auto bytes =
      _device->limits().validateTarget(size, 4, "GPU pooled depth target");
  for (auto &entry : _depths)
    if (entry.target.use_count() == 1 && entry.target->size == size) {
      if (entry.target->isLeased()) {
        ++_stats.busyMisses;
        continue;
      }
      ++_stats.reuses;
      entry.lastUse = _device->completedSubmission();
      return entry.target;
    }
  const bool retain = makeRoom(bytes);
  if (!reservation)
    makeAllocationRoom(bytes);
  auto result = std::make_shared<DepthTarget>(DepthTarget{
      size, createDepthTarget(_device, size, std::move(reservation))});
  ++_stats.allocations;
  if (retain) {
    _depths.push_back({result, bytes, _device->completedSubmission()});
    _stats.retainedBytes += bytes;
  }
  return result;
}

TargetPool::SceneTargets TargetPool::sceneTargets(math::Vec2i size,
                                                  bool postProcess) {
  _device->checkOwnerThread();
  _device->pollCompletions();
  trimAged();
  const auto colorBytes =
      _device->limits().validateTarget(size, 8, "Scene color attachment");
  const auto depthBytes =
      _device->limits().validateTarget(size, 4, "Scene depth attachment");
  // Hold reusable attachments before eviction/planning so none can be stolen
  // by another attachment in this same operation.
  SceneTargets result;
  const auto reuseColor = [&]() -> std::shared_ptr<ColorTarget> {
    for (auto &entry : _colors)
      if (entry.target.use_count() == 1 && !entry.target->isLeased() &&
          entry.target->pixelSize() ==
              math::Size2{float(size.x), float(size.y)}) {
        ++_stats.reuses;
        entry.lastUse = _device->completedSubmission();
        entry.target->beginWrite();
        return entry.target;
      }
    return {};
  };
  result.color = reuseColor();
  if (postProcess)
    result.output = reuseColor();
  for (auto &entry : _depths)
    if (entry.target.use_count() == 1 && !entry.target->isLeased() &&
        entry.target->size == size) {
      ++_stats.reuses;
      entry.lastUse = _device->completedSubmission();
      result.depth = entry.target;
      break;
    }
  auto bytes = rendering::checkedSum(result.color ? 0 : colorBytes,
                                     result.depth ? 0 : depthBytes);
  if (postProcess && !result.output)
    bytes = rendering::checkedSum(bytes, colorBytes);
  rendering::ResourceLedger::Token plan;
  if (bytes) {
    makeAllocationRoom(bytes);
    plan = _device->resources()->reserve(rendering::MemoryClass::GPU,
                                         rendering::ResourceKind::Target, bytes,
                                         "Scene attachment plan");
  }
  if (!result.color)
    result.color =
        color(size, "Scene color",
              _device->resources()->splitReservation(plan, colorBytes));
  if (!result.depth)
    result.depth =
        depth(size, _device->resources()->splitReservation(plan, depthBytes));
  if (postProcess && !result.output)
    result.output =
        color(size, "Scene output",
              _device->resources()->splitReservation(plan, colorBytes));
  return result;
}

void TargetPool::trimAged() {
  _device->checkOwnerThread();
  const auto completed = _device->completedSubmission();
  const auto age = _device->limits().targetRetentionSubmissions;
  const auto trim = [&](auto &entries) {
    std::erase_if(entries, [&](const auto &entry) {
      if (entry.target.use_count() != 1 || entry.target->isLeased() ||
          completed - std::max(entry.lastUse, entry.target->lastSubmission()) <
              age)
        return false;
      _stats.retainedBytes -= entry.bytes;
      ++_stats.evictions;
      return true;
    });
  };
  trim(_colors);
  trim(_depths);
}

void TargetPool::trim() {
  _device->checkOwnerThread();
  _device->pollCompletions();
  auto removeUnused = [&](auto &entries) {
    std::erase_if(entries, [&](const auto &entry) {
      if (entry.target.use_count() != 1 || entry.target->isLeased())
        return false;
      _stats.retainedBytes -= entry.bytes;
      ++_stats.evictions;
      return true;
    });
  };
  removeUnused(_colors);
  removeUnused(_depths);
}

Texture createDepthTarget(GPUDeviceHandle device, math::Vec2i size,
                          rendering::ResourceLedger::Token reservation) {
  if (!device)
    throw std::invalid_argument("Depth target requires a device");
  device->checkOwnerThread();
  device->limits().validateTarget(size, 4, "GPU depth target");
  SDL_GPUTextureCreateInfo info{};
  info.type = SDL_GPU_TEXTURETYPE_2D;
  info.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
  info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
  info.width = static_cast<Uint32>(size.x);
  info.height = static_cast<Uint32>(size.y);
  info.layer_count_or_depth = 1;
  info.num_levels = 1;
  if (!SDL_GPUTextureSupportsFormat(device->get(), info.format, info.type,
                                    info.usage))
    throw std::runtime_error("GPU does not support D32 depth attachments");
  return createTexture(device, info, rendering::ResourceKind::Target,
                       std::move(reservation));
}

Shader loadShader(GPUDeviceHandle device, const char *name,
                  SDL_GPUShaderStage stage, Uint32 samplers, Uint32 uniforms,
                  Uint32 storageBuffers) {
  device->checkOwnerThread();
  const auto formats = SDL_GetGPUShaderFormats(device->get());
  SDL_GPUShaderFormat format{};
  const char *extension{};
  const char *entry = "main";
#ifdef PLAYGROUND_SHADER_SPIRV
  if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
    format = SDL_GPU_SHADERFORMAT_SPIRV;
    extension = ".spirv";
  }
#endif
  if (!format)
    throw std::runtime_error("No packaged shader format for this GPU");
  auto path = platform::executableDirectory() / "assets" / "shaders" /
              (std::string{name} + extension);
#ifdef PLAYGROUND_SHADER_DIRECTORY
  if (!std::filesystem::exists(path))
    path = std::filesystem::path(u8"" PLAYGROUND_SHADER_DIRECTORY) /
           (std::string{name} + extension);
#endif
  const auto expectedStage = stage == SDL_GPU_SHADERSTAGE_VERTEX
                                 ? rendering::ShaderStage::Vertex
                                 : rendering::ShaderStage::Fragment;
  assets::AssetCatalog catalog{path.parent_path()};
  const assets::AssetId<assets::ShaderAsset> id{name};
  catalog.add(id, assets::ShaderAsset{assets::FileSource{path.filename()},
                                      expectedStage,
                                      entry,
                                      {samplers, 0, storageBuffers, uniforms}});
  catalog.freeze();
  auto compiled = assets::prepareShader(catalog, id);
  const auto &words = compiled.words;
  SDL_GPUShaderCreateInfo info{};
  info.code_size = words.size() * sizeof(std::uint32_t);
  info.code = reinterpret_cast<const Uint8 *>(words.data());
  info.entrypoint = entry;
  info.format = format;
  info.stage = stage;
  info.num_samplers = samplers;
  info.num_uniform_buffers = uniforms;
  info.num_storage_buffers = storageBuffers;
  return {device, SDL_CreateGPUShader(device->get(), &info)};
}
} // namespace playground::sdl::gpu_detail
