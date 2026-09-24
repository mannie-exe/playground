#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace playground::rendering {

enum class ShaderStage { Vertex, Fragment };
enum class ShaderBindingKind {
  Sampler,
  SampledImage,
  CombinedImageSampler,
  StorageImage,
  StorageBuffer,
  UniformBuffer
};
enum class ShaderValueType {
  Float,
  Float2,
  Float3,
  Float4,
  Int,
  Int2,
  Int3,
  Int4,
  UInt,
  UInt2,
  UInt3,
  UInt4
};

struct ShaderLayout {
  std::uint32_t samplers{}, storageTextures{}, storageBuffers{},
      uniformBuffers{};
  bool operator==(const ShaderLayout &) const = default;
};

struct ShaderBinding {
  std::uint32_t set{}, binding{}, count{1}, byteSize{};
  ShaderBindingKind kind{};
  std::string name;
  std::vector<std::uint32_t> blockLayout;
  bool operator==(const ShaderBinding &) const = default;
};

struct ShaderInterface {
  std::uint32_t location{};
  ShaderValueType type{};
  bool operator==(const ShaderInterface &) const = default;
};

struct ShaderReflection {
  ShaderStage stage;
  std::string entryPoint;
  std::vector<ShaderBinding> bindings;
  std::vector<ShaderInterface> inputs, outputs;

  // Validate SDL's stage-specific descriptor sets, consecutive slots and kinds.
  // Explicit counts disambiguate sampled images accessed without a sampler.
  void validateLayout(ShaderLayout layout) const;
  std::uint32_t uniformSize(std::uint32_t slot) const;
};

// Developer-produced SPIR-V only; reflection is not a hostile-bytecode sandbox.
ShaderReflection reflectSPIRV(std::span<const std::uint32_t> words,
                              std::string entryPoint = "main");
std::vector<std::uint32_t> readSPIRV(const std::filesystem::path &path,
                                     std::size_t maximumBytes = 16 * 1024 *
                                                                1024);
void validateShaderLink(const ShaderReflection &vertex,
                        const ShaderReflection &fragment);
bool isShaderABICompatible(const ShaderReflection &previous,
                           const ShaderReflection &replacement);

} // namespace playground::rendering
