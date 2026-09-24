#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>
#include <tuple>

#include <spirv_reflect.h>

#include <rendering/Shader.hpp>

namespace playground::rendering {
namespace {
void checked(SpvReflectResult result) {
  if (result != SPV_REFLECT_RESULT_SUCCESS)
    throw std::invalid_argument("SPIR-V reflection failed: " +
                                std::to_string(result));
}
ShaderValueType valueType(SpvReflectFormat format) {
  switch (format) {
  case SPV_REFLECT_FORMAT_R32_SFLOAT:
    return ShaderValueType::Float;
  case SPV_REFLECT_FORMAT_R32G32_SFLOAT:
    return ShaderValueType::Float2;
  case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT:
    return ShaderValueType::Float3;
  case SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT:
    return ShaderValueType::Float4;
  case SPV_REFLECT_FORMAT_R32_SINT:
    return ShaderValueType::Int;
  case SPV_REFLECT_FORMAT_R32G32_SINT:
    return ShaderValueType::Int2;
  case SPV_REFLECT_FORMAT_R32G32B32_SINT:
    return ShaderValueType::Int3;
  case SPV_REFLECT_FORMAT_R32G32B32A32_SINT:
    return ShaderValueType::Int4;
  case SPV_REFLECT_FORMAT_R32_UINT:
    return ShaderValueType::UInt;
  case SPV_REFLECT_FORMAT_R32G32_UINT:
    return ShaderValueType::UInt2;
  case SPV_REFLECT_FORMAT_R32G32B32_UINT:
    return ShaderValueType::UInt3;
  case SPV_REFLECT_FORMAT_R32G32B32A32_UINT:
    return ShaderValueType::UInt4;
  default:
    throw std::invalid_argument(
        "Shader interfaces currently require 32-bit scalar/vector values");
  }
}
ShaderBindingKind kind(SpvReflectDescriptorType type) {
  switch (type) {
  case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
    return ShaderBindingKind::Sampler;
  case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    return ShaderBindingKind::SampledImage;
  case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    return ShaderBindingKind::CombinedImageSampler;
  case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    return ShaderBindingKind::StorageImage;
  case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    return ShaderBindingKind::StorageBuffer;
  case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    return ShaderBindingKind::UniformBuffer;
  default:
    throw std::invalid_argument("Unsupported shader descriptor kind");
  }
}
void interfaceVariable(const SpvReflectInterfaceVariable &value,
                       std::vector<ShaderInterface> &out) {
  if (value.decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN)
    return;
  if (value.member_count) {
    for (std::uint32_t i = 0; i < value.member_count; ++i)
      interfaceVariable(value.members[i], out);
    return;
  }
  if (value.location == std::numeric_limits<std::uint32_t>::max() ||
      value.array.dims_count || value.numeric.matrix.column_count > 1 ||
      (value.component != 0 &&
       value.component != std::numeric_limits<std::uint32_t>::max()))
    throw std::invalid_argument("Shader interface requires scalar/vector "
                                "locations, not arrays or matrices");
  if (value.format == SPV_REFLECT_FORMAT_UNDEFINED)
    throw std::invalid_argument(
        "Shader interface has unsupported numeric format");
  out.push_back({value.location, valueType(value.format)});
}
void sortInterfaces(std::vector<ShaderInterface> &values) {
  std::ranges::sort(values, {}, &ShaderInterface::location);
  for (std::size_t i = 1; i < values.size(); ++i)
    if (values[i - 1].location == values[i].location)
      throw std::invalid_argument("Duplicate shader interface location");
}
void blockLayout(const SpvReflectBlockVariable &block,
                 std::vector<std::uint32_t> &out) {
  out.insert(
      out.end(),
      {block.offset, block.size, block.padded_size, block.decoration_flags,
       block.type_description ? block.type_description->type_flags : 0,
       block.numeric.scalar.width, block.numeric.scalar.signedness,
       block.numeric.vector.component_count, block.numeric.matrix.column_count,
       block.numeric.matrix.row_count, block.numeric.matrix.stride,
       block.array.dims_count, block.array.stride, block.member_count});
  for (std::uint32_t i = 0; i < block.array.dims_count; ++i)
    out.push_back(block.array.dims[i]);
  for (std::uint32_t i = 0; i < block.member_count; ++i)
    blockLayout(block.members[i], out);
}
} // namespace

ShaderReflection reflectSPIRV(std::span<const std::uint32_t> words,
                              std::string entryPoint) {
  if (words.size() < 5 || words[0] != SpvMagicNumber || words[4] != 0 ||
      entryPoint.empty() || entryPoint.find('\0') != std::string::npos)
    throw std::invalid_argument("Invalid SPIR-V header or entry point");
  // Reject truncated instructions before passing bytes to the reflection
  // parser.
  for (std::size_t at = 5; at < words.size();) {
    const auto count = words[at] >> 16;
    if (!count || count > words.size() - at)
      throw std::invalid_argument("Truncated SPIR-V instruction");
    at += count;
  }
  SpvReflectShaderModule module{};
  checked(
      spvReflectCreateShaderModule(words.size_bytes(), words.data(), &module));
  struct Guard {
    SpvReflectShaderModule *module;
    ~Guard() { spvReflectDestroyShaderModule(module); }
  } guard{&module};
  const auto *entry = spvReflectGetEntryPoint(&module, entryPoint.c_str());
  if (!entry)
    throw std::invalid_argument("Shader entry point not found");
  ShaderReflection result;
  if (entry->shader_stage == SPV_REFLECT_SHADER_STAGE_VERTEX_BIT)
    result.stage = ShaderStage::Vertex;
  else if (entry->shader_stage == SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT)
    result.stage = ShaderStage::Fragment;
  else
    throw std::invalid_argument(
        "Only vertex and fragment custom shaders are supported");
  if (entry->used_push_constant_count || module.spec_constant_count)
    throw std::invalid_argument("Push constants and specialization constants "
                                "are not part of the SDL shader ABI");
  result.entryPoint = std::move(entryPoint);
  std::uint32_t count{};
  checked(spvReflectEnumerateEntryPointDescriptorBindings(
      &module, result.entryPoint.c_str(), &count, nullptr));
  std::vector<SpvReflectDescriptorBinding *> bindings(count);
  checked(spvReflectEnumerateEntryPointDescriptorBindings(
      &module, result.entryPoint.c_str(), &count, bindings.data()));
  for (const auto *binding : bindings) {
    result.bindings.push_back({binding->set, binding->binding, binding->count,
                               binding->block.padded_size,
                               kind(binding->descriptor_type),
                               binding->name ? binding->name : ""});
    blockLayout(binding->block, result.bindings.back().blockLayout);
  }
  std::ranges::sort(result.bindings, [](const auto &a, const auto &b) {
    return std::tuple{a.set, a.binding, a.kind} <
           std::tuple{b.set, b.binding, b.kind};
  });
  for (std::uint32_t i = 0; i < entry->input_variable_count; ++i)
    interfaceVariable(*entry->input_variables[i], result.inputs);
  for (std::uint32_t i = 0; i < entry->output_variable_count; ++i)
    interfaceVariable(*entry->output_variables[i], result.outputs);
  sortInterfaces(result.inputs);
  sortInterfaces(result.outputs);
  return result;
}

std::vector<std::uint32_t> readSPIRV(const std::filesystem::path &path,
                                     std::size_t maximumBytes) {
  std::ifstream file{path, std::ios::binary | std::ios::ate};
  if (!file)
    throw std::runtime_error("Cannot open shader: " + path.string());
  const auto length = file.tellg();
  if (length < 20 || static_cast<std::uintmax_t>(length) > maximumBytes ||
      length % 4)
    throw std::invalid_argument(
        "Shader byte length is invalid or exceeds budget");
  std::vector<std::uint32_t> words(static_cast<std::size_t>(length) / 4);
  file.seekg(0);
  if (!file.read(reinterpret_cast<char *>(words.data()), length))
    throw std::runtime_error("Shader changed or could not be read completely");
  return words;
}

void ShaderReflection::validateLayout(ShaderLayout layout) const {
  if ((stage != ShaderStage::Vertex && stage != ShaderStage::Fragment) ||
      layout.samplers > 16 || layout.storageTextures > 8 ||
      layout.storageBuffers > 8 || layout.uniformBuffers > 4)
    throw std::invalid_argument(
        "Shader layout exceeds SDL stage resource limits");
  const std::uint32_t resources = stage == ShaderStage::Vertex ? 0 : 2;
  std::set<std::tuple<std::uint32_t, std::uint32_t, ShaderBindingKind>> seen;
  std::vector<bool> textures(layout.samplers + layout.storageTextures),
      buffers(layout.storageBuffers), uniforms(layout.uniformBuffers);
  for (const auto &binding : bindings) {
    if (binding.count != 1 ||
        !seen.emplace(binding.set, binding.binding, binding.kind).second)
      throw std::invalid_argument(
          "Duplicate/array descriptors are not supported");
    for (const auto &[set, slot, other] : seen) {
      if (set == binding.set && slot == binding.binding &&
          other != binding.kind &&
          !((other == ShaderBindingKind::Sampler &&
             binding.kind == ShaderBindingKind::SampledImage) ||
            (other == ShaderBindingKind::SampledImage &&
             binding.kind == ShaderBindingKind::Sampler)))
        throw std::invalid_argument(
            "Conflicting descriptor kinds at the same set/binding");
    }
    if (binding.kind == ShaderBindingKind::UniformBuffer) {
      if (binding.set != resources + 1 ||
          binding.binding >= layout.uniformBuffers || !binding.byteSize ||
          binding.byteSize > 4096)
        throw std::invalid_argument(
            "Uniform buffer descriptor is outside declared ABI");
      uniforms[binding.binding] = true;
      continue;
    }
    if (binding.set != resources)
      throw std::invalid_argument(
          "Shader resource uses wrong stage descriptor set");
    if (binding.kind == ShaderBindingKind::Sampler ||
        binding.kind == ShaderBindingKind::CombinedImageSampler) {
      if (binding.binding >= layout.samplers)
        throw std::invalid_argument(
            "Sampler descriptor is outside declared ABI");
      if (binding.kind == ShaderBindingKind::CombinedImageSampler)
        textures[binding.binding] = true;
    } else if (binding.kind == ShaderBindingKind::SampledImage) {
      if (binding.binding >= textures.size())
        throw std::invalid_argument("Image descriptor is outside declared ABI");
      textures[binding.binding] = true;
    } else if (binding.kind == ShaderBindingKind::StorageBuffer) {
      const auto first = layout.samplers + layout.storageTextures;
      if (binding.binding < first || binding.binding - first >= buffers.size())
        throw std::invalid_argument(
            "Storage buffer descriptor is outside declared ABI");
      buffers[binding.binding - first] = true;
    } else
      throw std::invalid_argument(
          "Writable storage images are not supported by graphics stage ABI");
  }
  const auto missing = [](const auto &slots) {
    return std::ranges::find(slots, false) != slots.end();
  };
  if (missing(textures) || missing(buffers) || missing(uniforms))
    throw std::invalid_argument(
        "Declared shader resource slot is absent from bytecode");
}

std::uint32_t ShaderReflection::uniformSize(std::uint32_t slot) const {
  for (const auto &binding : bindings)
    if (binding.kind == ShaderBindingKind::UniformBuffer &&
        binding.binding == slot)
      return binding.byteSize;
  throw std::out_of_range("Shader uniform slot does not exist");
}

void validateShaderLink(const ShaderReflection &vertex,
                        const ShaderReflection &fragment) {
  if (vertex.stage != ShaderStage::Vertex ||
      fragment.stage != ShaderStage::Fragment)
    throw std::invalid_argument(
        "Graphics pipeline requires vertex then fragment shaders");
  for (const auto input : fragment.inputs) {
    const auto found = std::ranges::find(vertex.outputs, input.location,
                                         &ShaderInterface::location);
    if (found == vertex.outputs.end() || found->type != input.type)
      throw std::invalid_argument(
          "Shader stage interface location/type mismatch");
  }
}
bool isShaderABICompatible(const ShaderReflection &previous,
                           const ShaderReflection &replacement) {
  return previous.stage == replacement.stage &&
         previous.entryPoint == replacement.entryPoint &&
         previous.bindings == replacement.bindings &&
         previous.inputs == replacement.inputs &&
         previous.outputs == replacement.outputs;
}
} // namespace playground::rendering
