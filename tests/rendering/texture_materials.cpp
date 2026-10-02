#include <cmath>
#include <limits>

#include <rendering/Texture.hpp>
#include <scene/Environment.hpp>
#include <support/Test.hpp>

using namespace playground;

int main() {
  return test::run([] {
    rendering::RGBA8Image bytes{{2, 1},
                                rendering::AlphaMode::Straight,
                                rendering::ColorEncoding::SRGB,
                                {0, 0, 0, 255, 255, 255, 255, 255}};
    auto color = rendering::makeTexture(bytes, rendering::TextureRole::Color);
    test::require(color->levels().size() == 2 &&
                      std::abs(color->levels()[1].texels[0].x - .5f) < .005f,
                  "color mip averages linear light");
    bytes.pixels = {128, 128, 128, 255, 128, 128, 128, 255};
    auto data = rendering::makeTexture(bytes, rendering::TextureRole::Data);
    color = rendering::makeTexture(bytes, rendering::TextureRole::Color);
    test::require(data->levels()[0].texels[0].x > .5f &&
                      color->levels()[0].texels[0].x < .22f,
                  "data texture bypasses gamma");
    auto odd = rendering::makeTexture(
        {{3, 1}, {{0, 0, 0, 1}, {0, 0, 0, 1}, {3, 0, 0, 1}}},
        rendering::TextureRole::Data);
    test::require(std::abs(odd->levels()[1].texels[0].x - 1) < 1e-6f,
                  "odd dimensions preserve average");
    test::require(odd->bytes() == 4 * sizeof(math::Vec4f),
                  "all mip bytes accounted");
    auto alpha = rendering::makeTexture({{2, 1}, {{1, 0, 0, 0}, {0, 1, 0, 1}}},
                                        rendering::TextureRole::Color);
    test::require(alpha->levels()[1].texels[0] == math::Vec4f{0, 1, 0, .5f},
                  "transparent RGB does not bleed into color mips");
    test::require(&color->upload(false) == color.get() && color->opaque(),
                  "opaque texture upload aliases validated immutable source");
    const auto &associated = alpha->upload(false);
    const auto &preparedOpaque = alpha->upload(true);
    test::require(
        &associated == &alpha->upload(false) &&
            &preparedOpaque == &alpha->upload(true) &&
            &associated != &preparedOpaque &&
            associated.alphaMode() == rendering::AlphaMode::Premultiplied &&
            preparedOpaque.opaque() && !alpha->opaque(),
        "immutable upload variants are prepared once per alpha policy");
    const auto opaque = rendering::makeOpaqueTexture(*alpha);
    test::require(opaque->levels()[0].texels[0] == math::Vec4f{1, 0, 0, 1} &&
                      opaque->levels()[1].texels[0] ==
                          math::Vec4f{.5f, .5f, 0, 1},
                  "opaque preparation ignores coverage before mip filtering");
    test::require(alpha->levels()[0].texels[0].w == 0 &&
                      alpha->levels()[1].texels[0] ==
                          math::Vec4f{0, 1, 0, .5f} &&
                      opaque->bytes() == alpha->bytes(),
                  "opaque variant preserves source and mip byte accounting");
    const rendering::Texture partial{
        rendering::TextureRole::Color,
        {{{4, 4}, std::vector<math::Vec4f>(16, {1, 0, 0, 0})},
         {{2, 2}, std::vector<math::Vec4f>(4)}}};
    const auto partialOpaque = rendering::makeOpaqueTexture(partial);
    test::require(partialOpaque->levels().size() == 2 &&
                      partialOpaque->levels()[1].texels[0] ==
                          math::Vec4f{1, 0, 0, 1},
                  "partial mip chains retain their level count when rebuilt");
    const rendering::Texture authored{
        rendering::TextureRole::Color,
        {{{2, 2}, std::vector<math::Vec4f>(4, {1, 0, 0, 1})},
         {{1, 1}, {{0, 0, 1, 1}}}}};
    test::require(
        rendering::makeOpaqueTexture(authored)->levels()[1].texels[0] ==
            math::Vec4f{0, 0, 1, 1},
        "already-opaque authored mip chains remain intact");
    test::rejects([&] { rendering::makeOpaqueTexture(*data); },
                  "opaque conversion refuses numerical data");
    auto masked = rendering::preserveAlphaCoverage(alpha, .8f);
    test::require(masked->levels()[0].texels == alpha->levels()[0].texels,
                  "coverage preparation preserves authored base texels");
    test::rejects([&] { rendering::preserveAlphaCoverage(alpha, 2); },
                  "invalid coverage cutoff rejected");
    test::rejects<std::length_error>(
        [&] {
          rendering::makeTexture(bytes, rendering::TextureRole::Color,
                                 rendering::MipPolicy::Generate, 8);
        },
        "mip budget includes tail");
    test::rejects(
        [] {
          rendering::Texture(rendering::TextureRole::Data,
                             {{{2, 2}, std::vector<math::Vec4f>(4)},
                              {{2, 1}, std::vector<math::Vec4f>(2)}});
        },
        "invalid mip dimensions rejected");
    test::rejects(
        [] {
          rendering::SamplerProps p;
          p.anisotropy = 17;
          p.validate();
        },
        "anisotropy bounded");
    scene::MaterialProps material;
    material.pbr.emplace();
    material.pbr->normalTexture.texture = color;
    test::rejects([&] { scene::validate(material); },
                  "normal binding refuses color interpretation");
    rendering::UVTransform uv{{.25f, .5f}, {.5f, .25f}};
    test::require(uv.apply({1, 1}) == math::Vec2f{.75f, .75f},
                  "binding-local UV transform");
    auto constant = rendering::makeTexture(
        {{2, 1}, {{2, 1, .5f, 1}, {2, 1, .5f, 1}}},
        rendering::TextureRole::Environment, rendering::MipPolicy::None);
    auto env = scene::prepareEnvironment(
        *constant,
        {.diffuseWidth = 4, .specularWidth = 4, .brdfSize = 4, .samples = 32});
    for (const auto &level : env.specular->levels())
      for (auto p : level.texels)
        test::require(std::abs(p.x - 2) < 1e-4f && std::abs(p.y - 1) < 1e-4f,
                      "constant environment is invariant under convolution");
    test::require(std::abs(env.diffuse->levels()[0].texels[0].x - 2) < 1e-4f,
                  "diffuse stores irradiance divided by pi");
    std::stop_source stopped;
    stopped.request_stop();
    test::rejects<std::runtime_error>(
        [&] { scene::prepareEnvironment(*constant, {}, stopped.get_token()); },
        "environment cancellation");
    scene::MeshData mesh{{{{0, 0, 0}, {0, 0, 1}, {0, 0}},
                          {{1, 0, 0}, {0, 0, 1}, {1, 0}},
                          {{0, 1, 0}, {0, 0, 1}, {0, 1}}},
                         {0, 1, 2}};
    scene::generateTangents(mesh);
    for (auto v : mesh.vertices)
      test::require(std::abs(v.tangent.x - 1) < 1e-5f && v.tangent.w == 1,
                    "MikkTSpace basis and handedness");
  });
}
