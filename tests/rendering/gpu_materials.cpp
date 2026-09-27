#include <filesystem>
#include <fstream>

#include "GPUSceneRenderer.hpp"
#include <app/SDLGuard.hpp>
#include <platform/sdl/GPURenderBackend.hpp>
#include <platform/sdl/ModelImport.hpp>
#include <platform/sdl/TextureDecode.hpp>
#include <scene/Environment.hpp>
#include <support/GPUReadback.hpp>

using namespace playground;
using namespace playground::sdl;
using namespace playground::sdl::gpu_detail;

int main(int argc, char **argv) {
  try {
    SDLGuard sdl{SDL_INIT_VIDEO};
    if (!packagedShaderFormats() ||
        !SDL_GPUSupportsShaderFormats(packagedShaderFormats(), "vulkan"))
      return 77;
    return test::run([&] {
      auto device = std::make_shared<GPUDevice>(
          GPUDeviceProps{packagedShaderFormats(), true, "vulkan"});
      PaintDevice paint{device};
      const auto excessive = rendering::makeTexture(
          {{1, 1}, {{70000, 0, 0, 1}}}, rendering::TextureRole::Data,
          rendering::MipPolicy::None);
      test::rejects([&] { GPUTextureData missing{nullptr, *excessive}; },
                    "material upload rejects a missing device");
      GPUTextureData fullPrecision{device, *excessive};
      test::require(
          fullPrecision.bytes() == 16,
          "numerical float32 textures are not silently narrowed on upload");
      GPUSceneRenderer renderer{paint};
      auto mesh = scene::makeMesh({{{{-.9f, -.9f, .5f}, {0, 0, -1}, {0, 1}},
                                    {{.9f, -.9f, .5f}, {0, 0, -1}, {1, 1}},
                                    {{0, .9f, .5f}, {0, 0, -1}, {.5f, 0}}},
                                   {0, 2, 1}});
      scene::MaterialProps material;
      material.pbr = scene::MetallicRoughnessProps{};
      material.pbr->baseColor = {.8f, .1f, .05f, 1};
      material.pbr->metallic = 0;
      scene::MeshDraw draw{mesh, material, {}};
      scene::SceneRenderProps view{{}, {32, 32}, {0, 0, 0, 255}};
      view.lighting.directionToLight = {0, 0, -1};
      auto render = [&] {
        return std::dynamic_pointer_cast<const GPUImage>(
            renderer.render(view, std::span{&draw, 1}));
      };
      auto lit = test::readPixel(device, *render(), 16, 16);
      test::require(lit[0] > .15f && lit[0] > lit[1] * 2,
                    "directional dielectric material shades red");
      draw.material.pbr->baseColorTexture.texture = rendering::makeTexture(
          {{1, 1}, {{1, 1, 1, 0}}}, rendering::TextureRole::Color);
      draw.material.pbr->baseColor.w = 0;
      const auto opaqueZeroAlpha = test::readPixel(device, *render(), 16, 16);
      test::require(
          std::abs(opaqueZeroAlpha[0] - lit[0]) < .002f,
          "opaque PBR ignores texture and factor alpha, including zero");
      draw.material.alpha = scene::MaterialProps::Alpha::Blend;
      test::require(
          test::readPixel(device, *render(), 16, 16)[0] < .001f,
          "shared zero-alpha texture remains transparent for blending");
      draw.material.alpha = scene::MaterialProps::Alpha::Opaque;
      test::require(
          std::abs(test::readPixel(device, *render(), 16, 16)[0] - lit[0]) <
              .002f,
          "cached opaque variant survives a blended use of the source");
      draw.material.pbr->baseColorTexture.texture.reset();
      draw.material.pbr->baseColor.w = 1;
      draw.material.pbr->normalTexture.texture = rendering::makeTexture(
          {{1, 1}, {{1, .5f, .5f, 1}}}, rendering::TextureRole::Normal,
          rendering::MipPolicy::None);
      auto sideways = test::readPixel(device, *render(), 16, 16);
      test::require(sideways[0] < lit[0] * .2f,
                    "tangent-space normal changes directional response");
      draw.material.pbr->normalTexture.texture.reset();
      view.lighting.irradiance = {};
      auto dark = test::readPixel(device, *render(), 16, 16);
      test::require(dark[0] < .001f, "no implicit ambient term");
      draw.material.pbr->emissive = {4, 0, 0};
      auto hdr = test::readPixel(device, *render(), 16, 16);
      test::require(hdr[0] > 3.9f,
                    "emissive radiance remains HDR before tone mapping");
      view.exposure = 2;
      auto exposed = test::readPixel(device, *render(), 16, 16);
      test::require(exposed[0] > 7.9f && exposed[0] < 8.1f,
                    "exposure applies even with tone mapping disabled");
      view.exposure = 1;
      view.toneMap = true;
      auto mapped = test::readPixel(device, *render(), 16, 16);
      test::require(mapped[0] > .5f && mapped[0] < 1,
                    "tone mapping compresses HDR scene");
      draw.material.pbr->emissive = {};
      draw.material.pbr->metallic = 1;
      auto constant = rendering::makeTexture(
          {{2, 1}, {{1, 1, 1, 1}, {1, 1, 1, 1}}},
          rendering::TextureRole::Environment, rendering::MipPolicy::None);
      auto environment = scene::prepareEnvironment(*constant, {4, 8, 8, 64});
      view.lighting.diffuseEnvironment = environment.diffuse;
      view.lighting.specularEnvironment = environment.specular;
      view.lighting.brdf = environment.brdf;
      auto metal = test::readPixel(device, *render(), 16, 16);
      test::require(
          metal[0] > .05f,
          "metal receives environment reflection without direct light");
      draw.material.pbr.reset();
      const auto sharedCoverage =
          rendering::makeTexture({{2, 1}, {{1, 0, 0, 0}, {0, 1, 0, 1}}},
                                 rendering::TextureRole::Color);
      draw.material.colorTexture.texture = sharedCoverage;
      view.toneMap = false;
      for (bool minify : {false, true}) {
        draw.material.colorTexture.transform =
            minify ? rendering::UVTransform{{}, {128, 128}}
                   : rendering::UVTransform{{.5f, .5f}, {0, 0}};
        draw.material.alpha = scene::MaterialProps::Alpha::Opaque;
        auto opaqueSample = test::readPixel(device, *render(), 16, 16);
        test::require(
            std::abs(opaqueSample[0] - .5f) < .002f &&
                std::abs(opaqueSample[1] - .5f) < .002f,
            "opaque linear/minified filtering includes RGB beneath zero alpha");
        const auto opaqueBytes = renderer.textureResidentBytes();
        draw.material.alpha = scene::MaterialProps::Alpha::Blend;
        auto blendedSample = test::readPixel(device, *render(), 16, 16);
        test::require(
            blendedSample[0] < .002f &&
                std::abs(blendedSample[1] - .5f) < .002f,
            "shared blend variant retains coverage-weighted filtering");
        if (!minify)
          test::require(renderer.textureResidentBytes() ==
                            opaqueBytes + sharedCoverage->bytes(),
                        "both alpha realizations count toward GPU residency");
        draw.material.alpha = scene::MaterialProps::Alpha::Mask;
        draw.material.alphaCutoff = .75f;
        test::require(test::readPixel(device, *render(), 16, 16)[1] < .002f,
                      "mask retains filtered alpha for cutoff");
        draw.material.alphaCutoff = .25f;
        test::require(test::readPixel(device, *render(), 16, 16)[1] > .99f,
                      "accepted mask retains straight visible color");
        draw.material.alpha = scene::MaterialProps::Alpha::Opaque;
        test::require(
            std::abs(test::readPixel(device, *render(), 16, 16)[0] - .5f) <
                .002f,
            "opaque variant is independent of shared blend/mask uses");
      }
      test::require(sharedCoverage->levels()[0].texels[0].w == 0 &&
                        sharedCoverage->levels()[1].texels[0].x == 0,
                    "GPU preparation does not modify CPU source textures");
      draw.material.colorTexture = {};
      rendering::RGBA8Image compact{{2, 1},
                                    rendering::AlphaMode::Straight,
                                    rendering::ColorEncoding::SRGB,
                                    {255, 0, 0, 0, 0, 255, 0, 255}};
      draw.material.colorTexture.texture =
          rendering::makeTexture(compact, rendering::TextureRole::Color);
      draw.material.colorTexture.transform = {{.5f, .5f}, {0, 0}};
      for (bool opaque : {true, false}) {
        draw.material.alpha = opaque ? scene::MaterialProps::Alpha::Opaque
                                     : scene::MaterialProps::Alpha::Blend;
        const auto sample = test::readPixel(device, *render(), 16, 16);
        test::require(
            std::abs(sample[1] - .5f) < .01f &&
                std::abs(sample[0] - (opaque ? .5f : 0.f)) < .01f,
            "compact sRGB upload preserves linear alpha-aware filtering");
      }
      draw.material.alpha = scene::MaterialProps::Alpha::Opaque;
      draw.material.colorTexture = {};
      draw.material.colorTexture.texture =
          rendering::makeTexture({{2, 1}, {{1, 0, 0, 1}, {0, 1, 0, 1}}},
                                 rendering::TextureRole::Color);
      draw.material.colorTexture.sampler.minification =
          draw.material.colorTexture.sampler.magnification =
              rendering::Sampling::Nearest;
      draw.material.colorTexture.sampler.mip = rendering::MipFilter::None;
      draw.material.colorTexture.transform.scale = {0, 0};
      draw.material.colorTexture.transform.offset = {.75f, .5f};
      view.toneMap = false;
      auto green = test::readPixel(device, *render(), 16, 16);
      test::require(green[1] > .99f && green[0] < .01f,
                    "per-instance texture transform selects atlas region");
      draw.material.colorTexture.texture =
          std::make_shared<const rendering::Texture>(
              rendering::TextureRole::Color,
              std::vector<rendering::TextureLevel>{
                  {{4, 4}, std::vector<math::Vec4f>(16, {1, 0, 0, 1})},
                  {{2, 2}, std::vector<math::Vec4f>(4, {0, 1, 0, 1})},
                  {{1, 1}, {{0, 0, 1, 1}}}});
      draw.material.colorTexture.transform = {{}, {128, 128}};
      draw.material.colorTexture.sampler.mip = rendering::MipFilter::Linear;
      auto minified = test::readPixel(device, *render(), 16, 16);
      test::require(minified[2] > .99f && minified[0] < .01f,
                    "minification actually samples uploaded lower mip levels");
      draw.material.alpha = scene::MaterialProps::Alpha::Mask;
      draw.material.alphaCutoff = .5f;
      draw.material.colorTexture.texture = rendering::makeTexture(
          {{1, 1}, {{1, 0, 0, .2f}}}, rendering::TextureRole::Color,
          rendering::MipPolicy::None);
      test::require(test::readPixel(device, *render(), 16, 16)[0] < .001f,
                    "masked material discards low coverage");
      draw.material = {};
      SurfaceHandle transparentRed{
          SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_RGBA32),
          SurfaceHandleDeleter{}};
      test::require(
          bool(transparentRed) &&
              SDL_WriteSurfacePixel(transparentRed.get(), 0, 0, 255, 0, 0, 0),
          "create straight image with RGB beneath zero alpha");
      draw.material.baseColorImage = makeSurfaceImage(transparentRed);
      for (const auto filter :
           {rendering::Sampling::Nearest, rendering::Sampling::Linear}) {
        draw.material.sampling = filter;
        draw.material.alpha = scene::MaterialProps::Alpha::Opaque;
        test::require(test::readPixel(device, *render(), 16, 16)[0] > .99f,
                      "opaque PaintImage path ignores alpha before filtering");
        draw.material.alpha = scene::MaterialProps::Alpha::Blend;
        test::require(test::readPixel(device, *render(), 16, 16)[0] < .001f,
                      "same PaintImage retains alpha when blended");
      }
      draw.material = {};
      auto tinted = mesh->data();
      for (auto &vertex : tinted.vertices)
        vertex.color = {0, 1, 0, 1};
      draw.mesh = scene::makeMesh(std::move(tinted));
      auto vertexColor = test::readPixel(device, *render(), 16, 16);
      test::require(vertexColor[1] > .99f && vertexColor[0] < .01f,
                    "legacy unlit path also applies vertex colors");
      // Real sample exercises importer->texture->PBR->tone-map together.
      scene::ModelImportProps limits;
      limits.maxTotalResourceBytes = 512 * 1024 * 1024;
      auto model = loadGLTF(std::filesystem::path{PLAYGROUND_SOURCE_DIR} /
                                "assets/demo3d/BoomBox.glb",
                            limits);
      scene::Scene3D world;
      model->instantiate(world, {.scale = {100, 100, 100}});
      auto draws = world.snapshot();
      view.camera = scene::CameraProps{.eye = {2, 1, -4}}.view(1);
      view.pixelSize = {256, 256};
      view.toneMap = true;
      view.lighting.irradiance = {2, 2, 2};
      auto prop = std::dynamic_pointer_cast<const GPUImage>(
          renderer.render(view, draws));
      auto pixels = test::readPixels(device, *prop);
      std::size_t visible{};
      for (auto pixel : pixels) {
        for (float c : pixel)
          test::require(std::isfinite(c), "finite sample rendering");
        visible += pixel[0] + pixel[1] + pixel[2] > .1f;
      }
      test::require(visible > 100,
                    "downloaded prop produces visible shaded pixels");
      if (argc > 1) {
        SDLResource<SDL_Surface, SDL_DestroySurface> capture{
            SDL_CreateSurface(256, 256, SDL_PIXELFORMAT_RGBA32)};
        for (int y = 0; y < 256; ++y)
          for (int x = 0; x < 256; ++x) {
            auto p = pixels[std::size_t(y) * 256 + x];
            const auto c = math::toSRGB({p[0], p[1], p[2], p[3]});
            SDL_WriteSurfacePixel(capture.get(), x, y, c.r, c.g, c.b, c.a);
          }
        test::require(SDL_SaveBMP(capture.get(), argv[1]),
                      "optional capture saved");
      }
    });
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
