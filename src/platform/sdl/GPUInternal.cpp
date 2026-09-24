#include <algorithm>
#include <cstring>
#include <filesystem>

#include <SDL3/SDL_filesystem.h>

#include "GPUInternal.hpp"
#include <rendering/Shader.hpp>

namespace playground::sdl::gpu_detail {

void *UploadStream::map(std::size_t bytes) {
  _device->checkOwnerThread();
  _device->limits().validateUpload(bytes);
  if (bytes > _capacity) {
    const SDL_GPUTransferBufferCreateInfo info{
        SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, static_cast<Uint32>(bytes)};
    Transfer candidate{_device,
                       SDL_CreateGPUTransferBuffer(_device->get(), &info)};
    _transfer = std::move(candidate);
    _capacity = static_cast<Uint32>(bytes);
  }
  auto *mapped =
      SDL_MapGPUTransferBuffer(_device->get(), _transfer.get(), true);
  if (!mapped)
    throwSDLError("Cannot map streaming transfer buffer");
  return mapped;
}

SDL_GPUBuffer *StreamBuffer::write(SDL_GPUCommandBuffer *commands,
                                   std::span<const std::byte> bytes) {
  _device->checkOwnerThread();
  _device->limits().validateUpload(bytes.size());
  if (bytes.size() > _device->limits().maxStreamBytes)
    throw std::length_error("Draw stream chunk exceeds streaming budget");
  if (bytes.size() > _capacity) {
    const SDL_GPUBufferCreateInfo info{_usage,
                                       static_cast<Uint32>(bytes.size())};
    Buffer candidate{_device, SDL_CreateGPUBuffer(_device->get(), &info)};
    _buffer = std::move(candidate);
    _capacity = static_cast<Uint32>(bytes.size());
  }
  std::memcpy(_upload.map(bytes.size()), bytes.data(), bytes.size());
  _upload.unmap();
  auto *copy = SDL_BeginGPUCopyPass(commands);
  if (!copy)
    throwSDLError("Cannot begin streaming copy pass");
  const SDL_GPUTransferBufferLocation source{_upload.get(), 0};
  const SDL_GPUBufferRegion destination{_buffer.get(), 0,
                                        static_cast<Uint32>(bytes.size())};
  SDL_UploadToGPUBuffer(copy, &source, &destination, true);
  SDL_EndGPUCopyPass(copy);
  return _buffer.get();
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

std::shared_ptr<void> TargetPool::reserve(std::size_t bytes) {
  const auto cap = _device->limits().maxLiveTargetBytes;
  if (bytes <= cap && _allocations.bytes() > cap - bytes)
    trim();
  try {
    return _allocations.reserve(bytes);
  } catch (const std::length_error &) {
    ++_stats.pressureFailures;
    throw;
  }
}

std::shared_ptr<GPUImage> TargetPool::color(math::Vec2i size) {
  _device->checkOwnerThread();
  _device->pollCompletions();
  trimAged();
  const auto bytes = _device->limits().validateTarget(size, 8);
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
      return entry.target;
    }
  const bool retain = makeRoom(bytes);
  auto reservation = reserve(bytes);
  auto result = std::make_shared<GPUImage>(_device, size);
  result->use()->allocation = std::move(reservation);
  ++_stats.allocations;
  if (retain) {
    _colors.push_back({result, bytes, _device->completedSubmission()});
    _stats.retainedBytes += bytes;
  }
  return result;
}

std::shared_ptr<DepthTarget> TargetPool::depth(math::Vec2i size) {
  _device->checkOwnerThread();
  _device->pollCompletions();
  trimAged();
  const auto bytes = _device->limits().validateTarget(size, 4);
  for (auto &entry : _depths)
    if (entry.target.use_count() == 1 && entry.target->size == size) {
      if (entry.target->use.use_count() > 1) {
        ++_stats.busyMisses;
        continue;
      }
      ++_stats.reuses;
      entry.lastUse = _device->completedSubmission();
      return entry.target;
    }
  const bool retain = makeRoom(bytes);
  auto reservation = reserve(bytes);
  auto result = std::make_shared<DepthTarget>(DepthTarget{
      size, createDepthTarget(_device, size),
      std::make_shared<rendering::ResourceUse>(_device->resourceDomain())});
  _device->registerTexture(result->texture.get(), result->use);
  result->use->allocation = std::move(reservation);
  ++_stats.allocations;
  if (retain) {
    _depths.push_back({result, bytes, _device->completedSubmission()});
    _stats.retainedBytes += bytes;
  }
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

Texture createDepthTarget(GPUDeviceHandle device, math::Vec2i size) {
  if (!device)
    throw std::invalid_argument("Depth target requires a device");
  device->checkOwnerThread();
  device->limits().validateTarget(size, 4);
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
  return {device, SDL_CreateGPUTexture(device->get(), &info)};
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
  const auto *base = SDL_GetBasePath();
  auto path = std::filesystem::path(base ? base : "") / "assets" / "shaders" /
              (std::string{name} + extension);
#ifdef PLAYGROUND_SHADER_DIRECTORY
  if (!std::filesystem::exists(path))
    path = std::filesystem::path(PLAYGROUND_SHADER_DIRECTORY) /
           (std::string{name} + extension);
#endif
  auto words = rendering::readSPIRV(path);
  const auto reflection = rendering::reflectSPIRV(words, entry);
  const auto expectedStage = stage == SDL_GPU_SHADERSTAGE_VERTEX
                                 ? rendering::ShaderStage::Vertex
                                 : rendering::ShaderStage::Fragment;
  if (reflection.stage != expectedStage)
    throw std::invalid_argument(
        "Shader bytecode stage does not match requested stage");
  reflection.validateLayout({samplers, 0, storageBuffers, uniforms});
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
