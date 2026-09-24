#include <algorithm>
#include <limits>
#include <set>

#include <platform/sdl/GPUShaderPipeline.hpp>

namespace playground::sdl {
namespace {
using ShaderResource = GPUResource<SDL_GPUShader, SDL_ReleaseGPUShader>;
ShaderResource shader(GPUDeviceHandle device,
                      std::span<const std::uint32_t> code,
                      const rendering::ShaderReflection &reflected,
                      rendering::ShaderLayout layout) {
  reflected.validateLayout(layout);
  SDL_GPUShaderCreateInfo info{};
  info.code = reinterpret_cast<const Uint8 *>(code.data());
  info.code_size = code.size_bytes();
  info.entrypoint = reflected.entryPoint.c_str();
  info.format = SDL_GPU_SHADERFORMAT_SPIRV;
  info.stage = reflected.stage == rendering::ShaderStage::Vertex
                   ? SDL_GPU_SHADERSTAGE_VERTEX
                   : SDL_GPU_SHADERSTAGE_FRAGMENT;
  info.num_samplers = layout.samplers;
  info.num_storage_textures = layout.storageTextures;
  info.num_storage_buffers = layout.storageBuffers;
  info.num_uniform_buffers = layout.uniformBuffers;
  return {device, SDL_CreateGPUShader(device->get(), &info)};
}
SDL_GPUVertexElementFormat element(rendering::ShaderValueType type) {
  using enum rendering::ShaderValueType;
  switch (type) {
  case Float:
    return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT;
  case Float2:
    return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
  case Float3:
    return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
  case Float4:
    return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
  case Int:
    return SDL_GPU_VERTEXELEMENTFORMAT_INT;
  case Int2:
    return SDL_GPU_VERTEXELEMENTFORMAT_INT2;
  case Int3:
    return SDL_GPU_VERTEXELEMENTFORMAT_INT3;
  case Int4:
    return SDL_GPU_VERTEXELEMENTFORMAT_INT4;
  case UInt:
    return SDL_GPU_VERTEXELEMENTFORMAT_UINT;
  case UInt2:
    return SDL_GPU_VERTEXELEMENTFORMAT_UINT2;
  case UInt3:
    return SDL_GPU_VERTEXELEMENTFORMAT_UINT3;
  case UInt4:
    return SDL_GPU_VERTEXELEMENTFORMAT_UINT4;
  }
  throw std::invalid_argument("Unsupported vertex format");
}
void validateInputs(const GPUPipelineProps &props,
                    const rendering::ShaderReflection &vertex,
                    const rendering::ShaderReflection &fragment) {
  if (props.vertexAttributes.size() != vertex.inputs.size() ||
      props.vertexBuffers.size() > 16 || props.vertexAttributes.size() > 32 ||
      props.colorTargets.size() > 4)
    throw std::invalid_argument(
        "Pipeline vertex/target counts do not match shader or SDL limits");
  std::set<Uint32> slots, locations;
  for (const auto &buffer : props.vertexBuffers)
    if (buffer.slot >= 16 || !buffer.pitch || !slots.insert(buffer.slot).second)
      throw std::invalid_argument("Invalid/duplicate vertex buffer slot");
  for (const auto &attribute : props.vertexAttributes) {
    const auto input = std::ranges::find(vertex.inputs, attribute.location,
                                         &rendering::ShaderInterface::location);
    const auto buffer =
        std::ranges::find(props.vertexBuffers, attribute.buffer_slot,
                          &SDL_GPUVertexBufferDescription::slot);
    if (input == vertex.inputs.end() || buffer == props.vertexBuffers.end() ||
        attribute.format != element(input->type) ||
        attribute.offset >= buffer->pitch ||
        4 * (static_cast<unsigned>(input->type) % 4 + 1) >
            buffer->pitch - attribute.offset ||
        !locations.insert(attribute.location).second)
      throw std::invalid_argument(
          "Pipeline vertex attribute does not match reflected input");
  }
  for (const auto output : fragment.outputs)
    if (output.location >= props.colorTargets.size())
      throw std::invalid_argument(
          "Fragment output has no matching color target");
}
} // namespace

GPUPipelineGeneration::GPUPipelineGeneration(
    GPUDeviceHandle device, GPUPipelineResource pipeline,
    rendering::ShaderReflection vertex, rendering::ShaderReflection fragment,
    std::uint64_t generation)
    : _device{std::move(device)}, _pipeline{std::move(pipeline)},
      _vertex{std::move(vertex)}, _fragment{std::move(fragment)},
      _generation{generation} {}

void GPUPipelineGeneration::bind(SDL_GPURenderPass *pass) const {
  _device->checkOwnerThread();
  if (!pass)
    throw std::invalid_argument("Pipeline binding requires a live render pass");
  SDL_BindGPUGraphicsPipeline(pass, _pipeline.get());
}

void GPUPipelineGeneration::pushUniform(
    SDL_GPUCommandBuffer *commands, rendering::ShaderStage stage,
    std::uint32_t slot, std::span<const std::byte> bytes) const {
  _device->checkOwnerThread();
  if (!commands || (stage != rendering::ShaderStage::Vertex &&
                    stage != rendering::ShaderStage::Fragment))
    throw std::invalid_argument(
        "Uniform push requires live commands and a graphics stage");
  const auto &reflection =
      stage == rendering::ShaderStage::Vertex ? _vertex : _fragment;
  if (bytes.size() != reflection.uniformSize(slot))
    throw std::invalid_argument(
        "Uniform bytes must match reflected padded block size");
  if (stage == rendering::ShaderStage::Vertex)
    SDL_PushGPUVertexUniformData(commands, slot, bytes.data(),
                                 static_cast<Uint32>(bytes.size()));
  else
    SDL_PushGPUFragmentUniformData(commands, slot, bytes.data(),
                                   static_cast<Uint32>(bytes.size()));
}

GPUShaderPipeline::GPUShaderPipeline(GPUDeviceHandle device,
                                     GPUPipelineProps props)
    : _device{std::move(device)}, _props{std::move(props)} {
  if (!_device)
    throw std::invalid_argument("Custom pipeline requires a GPU device");
  _device->checkOwnerThread();
  publish(rendering::readSPIRV(_props.vertex.path),
          rendering::readSPIRV(_props.fragment.path));
}

GPUPipelineHandle GPUShaderPipeline::snapshot() const {
  _device->checkOwnerThread();
  return _current;
}

void GPUShaderPipeline::publish(std::vector<std::uint32_t> vertexCode,
                                std::vector<std::uint32_t> fragmentCode) {
  _device->checkOwnerThread();
  auto vertex = rendering::reflectSPIRV(vertexCode, _props.vertex.entryPoint);
  auto fragment =
      rendering::reflectSPIRV(fragmentCode, _props.fragment.entryPoint);
  if (_current &&
      (!rendering::isShaderABICompatible(_current->vertex(), vertex) ||
       !rendering::isShaderABICompatible(_current->fragment(), fragment)))
    throw std::invalid_argument("Hot reload cannot change the published shader "
                                "ABI; construct a new pipeline");
  rendering::validateShaderLink(vertex, fragment);
  validateInputs(_props, vertex, fragment);
  auto vs = shader(_device, vertexCode, vertex, _props.vertex.layout);
  auto fs = shader(_device, fragmentCode, fragment, _props.fragment.layout);
  SDL_GPUGraphicsPipelineCreateInfo info{};
  info.vertex_shader = vs.get();
  info.fragment_shader = fs.get();
  info.vertex_input_state = {
      _props.vertexBuffers.data(),
      static_cast<Uint32>(_props.vertexBuffers.size()),
      _props.vertexAttributes.data(),
      static_cast<Uint32>(_props.vertexAttributes.size())};
  info.primitive_type = _props.primitive;
  info.rasterizer_state = _props.rasterizer;
  info.multisample_state = _props.multisample;
  info.depth_stencil_state = _props.depthStencil;
  info.target_info.color_target_descriptions = _props.colorTargets.data();
  info.target_info.num_color_targets =
      static_cast<Uint32>(_props.colorTargets.size());
  info.target_info.depth_stencil_format = _props.depthFormat;
  info.target_info.has_depth_stencil_target =
      _props.depthFormat != SDL_GPU_TEXTUREFORMAT_INVALID;
  const auto generation = _current ? _current->generation() + 1 : 1;
  if (!generation)
    throw std::overflow_error("Pipeline generation exhausted");
  // Own native allocation before make_shared so allocation failure cannot leak
  // it.
  GPUResource<SDL_GPUGraphicsPipeline, SDL_ReleaseGPUGraphicsPipeline>
      candidate{_device, SDL_CreateGPUGraphicsPipeline(_device->get(), &info)};
  // Generation's constructor accepts ownership via a move, avoiding a
  // raw-release gap.
  auto published = std::make_shared<GPUPipelineGeneration>(
      _device, std::move(candidate), std::move(vertex), std::move(fragment),
      generation);
  _vertexCode = std::move(vertexCode);
  _fragmentCode = std::move(fragmentCode);
  _current = std::move(published);
  _lastError.clear();
}

PipelineReload GPUShaderPipeline::poll() {
  _device->checkOwnerThread();
  try {
    auto vertex = rendering::readSPIRV(_props.vertex.path);
    auto fragment = rendering::readSPIRV(_props.fragment.path);
    if (vertex == _vertexCode && fragment == _fragmentCode) {
      _lastError.clear();
      return PipelineReload::Unchanged;
    }
    publish(std::move(vertex), std::move(fragment));
    return PipelineReload::Published;
  } catch (const std::exception &error) {
    _lastError = error.what();
    return PipelineReload::Rejected;
  }
}

} // namespace playground::sdl
