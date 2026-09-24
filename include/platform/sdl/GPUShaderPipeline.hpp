#pragma once

#include <memory>

#include <platform/sdl/GPUResource.hpp>
#include <rendering/Shader.hpp>

namespace playground::sdl {

using GPUPipelineResource =
    GPUResource<SDL_GPUGraphicsPipeline, SDL_ReleaseGPUGraphicsPipeline>;

struct GPUShaderProps {
  std::filesystem::path path;
  std::string entryPoint{"main"};
  rendering::ShaderLayout layout;
};

struct GPUPipelineProps {
  GPUShaderProps vertex, fragment;
  std::vector<SDL_GPUVertexBufferDescription> vertexBuffers;
  std::vector<SDL_GPUVertexAttribute> vertexAttributes;
  std::vector<SDL_GPUColorTargetDescription> colorTargets;
  SDL_GPUPrimitiveType primitive{SDL_GPU_PRIMITIVETYPE_TRIANGLELIST};
  SDL_GPURasterizerState rasterizer{};
  SDL_GPUMultisampleState multisample{};
  SDL_GPUDepthStencilState depthStencil{};
  SDL_GPUTextureFormat depthFormat{SDL_GPU_TEXTUREFORMAT_INVALID};
};

class GPUPipelineGeneration {
  GPUDeviceHandle _device;
  GPUPipelineResource _pipeline;
  rendering::ShaderReflection _vertex, _fragment;
  std::uint64_t _generation;

public:
  GPUPipelineGeneration(GPUDeviceHandle device, GPUPipelineResource pipeline,
                        rendering::ShaderReflection vertex,
                        rendering::ShaderReflection fragment,
                        std::uint64_t generation);
  SDL_GPUGraphicsPipeline *get() const noexcept { return _pipeline.get(); }
  rendering::ResourceDomainId resourceDomain() const noexcept {
    return _device->resourceDomain();
  }
  std::uint64_t generation() const noexcept { return _generation; }
  const rendering::ShaderReflection &vertex() const noexcept { return _vertex; }
  const rendering::ShaderReflection &fragment() const noexcept {
    return _fragment;
  }
  // Native pass/commands must belong to this generation's device and thread.
  void bind(SDL_GPURenderPass *pass) const;
  void pushUniform(SDL_GPUCommandBuffer *commands, rendering::ShaderStage stage,
                   std::uint32_t slot, std::span<const std::byte> bytes) const;
};
using GPUPipelineHandle = std::shared_ptr<const GPUPipelineGeneration>;
enum class PipelineReload { Unchanged, Published, Rejected };

// Owner-thread manager. Copy a snapshot before recording and retain it through
// submission. Poll only at safe boundaries; old snapshots survive successful
// reload.
class GPUShaderPipeline {
  GPUDeviceHandle _device;
  GPUPipelineProps _props;
  GPUPipelineHandle _current;
  std::vector<std::uint32_t> _vertexCode, _fragmentCode;
  std::string _lastError;

  void publish(std::vector<std::uint32_t> vertex,
               std::vector<std::uint32_t> fragment);

public:
  GPUShaderPipeline(GPUDeviceHandle device, GPUPipelineProps props);
  GPUPipelineHandle snapshot() const;
  const GPUPipelineProps &props() const noexcept { return _props; }
  const std::string &lastError() const noexcept { return _lastError; }
  PipelineReload poll();
};

} // namespace playground::sdl
