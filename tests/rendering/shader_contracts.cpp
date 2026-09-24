#include <array>

#include <rendering/Shader.hpp>
#include <support/Test.hpp>

using namespace playground;
using namespace playground::rendering;

int main() {
  return test::run([] {
    test::rejects([] { reflectSPIRV({}); }, "empty bytecode rejected");
    const std::array<std::uint32_t, 6> truncated{0x07230203, 0x00010000, 0, 1,
                                                 0,          0x00020000};
    test::rejects([&] { reflectSPIRV(truncated); },
                  "truncated instruction rejected before reflection");
    // A minimal fragment module with Location 0 but no Component decoration.
    // SPIRV-Reflect represents that absent optional decoration as UINT32_MAX.
    const std::array<std::uint32_t, 48> fragmentModule{
        0x07230203, 0x00010000, 0,          8,          0,          0x00020011,
        1,          0x0003000e, 0,          1,          0x0006000f, 4,
        4,          0x6e69616d, 0,          6,          0x00030010, 4,
        7,          0x00040047, 6,          30,         0,          0x00020013,
        1,          0x00030021, 2,          1,          0x00030016, 3,
        32,         0x00040020, 5,          3,          3,          0x0004003b,
        5,          6,          3,          0x00050036, 1,          4,
        0,          2,          0x000200f8, 7,          0x000100fd, 0x00010038};
    const auto reflected = reflectSPIRV(fragmentModule);
    test::require(reflected.outputs ==
                      std::vector<ShaderInterface>{{0, ShaderValueType::Float}},
                  "absent Component decoration means normal location, not "
                  "unsupported packing");
    ShaderReflection vertex{
        .stage = ShaderStage::Vertex,
        .entryPoint = "main",
        .bindings = {{1, 0, 1, 64, ShaderBindingKind::UniformBuffer, "Camera"}},
        .outputs = {{0, ShaderValueType::Float2}}};
    vertex.validateLayout({.uniformBuffers = 1});
    auto uniformBoundary = vertex;
    uniformBoundary.bindings[0].byteSize = 4096;
    uniformBoundary.validateLayout({.uniformBuffers = 1});
    uniformBoundary.bindings[0].byteSize = 4097;
    test::rejects(
        [&] { uniformBoundary.validateLayout({.uniformBuffers = 1}); },
        "uniform blocks cannot exceed SDL Vulkan's 4096-byte slot range");
    uniformBoundary.bindings[0].byteSize = 0;
    test::rejects(
        [&] { uniformBoundary.validateLayout({.uniformBuffers = 1}); },
        "empty uniform blocks rejected");
    auto replacement = vertex;
    test::require(isShaderABICompatible(vertex, replacement),
                  "identical ABI accepted");
    replacement.bindings[0].blockLayout.push_back(1);
    test::require(!isShaderABICompatible(vertex, replacement),
                  "block layout change rejects compatible reload");
    test::require(vertex.uniformSize(0) == 64,
                  "uniform padded byte layout available");
    test::rejects<std::out_of_range>([&] { vertex.uniformSize(1); },
                                     "unknown uniform slot rejected");
    test::rejects([&] { vertex.validateLayout({}); },
                  "unadvertised resource rejected");
    test::rejects([&] { vertex.validateLayout({.uniformBuffers = 2}); },
                  "missing advertised slot rejected");
    ShaderReflection fragment{
        .stage = ShaderStage::Fragment,
        .entryPoint = "main",
        .bindings = {{2, 0, 1, 0, ShaderBindingKind::SampledImage, "Image"},
                     {2, 0, 1, 0, ShaderBindingKind::Sampler, "Sampler"}},
        .inputs = {{0, ShaderValueType::Float2}}};
    fragment.validateLayout({.samplers = 1});
    auto conflicting = fragment;
    conflicting.bindings.push_back(
        {2, 0, 1, 0, ShaderBindingKind::CombinedImageSampler, "Combined"});
    test::rejects([&] { conflicting.validateLayout({.samplers = 1}); },
                  "different resource kinds cannot alias a slot except HLSL "
                  "image/sampler pairs");
    validateShaderLink(vertex, fragment);
    fragment.inputs[0].type = ShaderValueType::Float3;
    test::rejects([&] { validateShaderLink(vertex, fragment); },
                  "cross-stage type mismatch rejected");
    fragment.bindings[0].set = 0;
    test::rejects([&] { fragment.validateLayout({.samplers = 1}); },
                  "stage descriptor set checked");
    fragment.bindings[0].set = 2;
    fragment.bindings[0].count = 2;
    test::rejects([&] { fragment.validateLayout({.samplers = 1}); },
                  "descriptor arrays rejected");
    test::rejects([&] { vertex.validateLayout({.uniformBuffers = 5}); },
                  "SDL resource count limits checked");
  });
}
