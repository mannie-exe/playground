#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

#include "GPUSceneRenderer.hpp"
#include <app/SDLGuard.hpp>
#include <assets/AssetCatalog.hpp>
#include <platform/sdl/GPURenderBackend.hpp>
#include <platform/sdl/ModelImport.hpp>
#include <platform/sdl/TextureDecode.hpp>
#include <scene/Environment.hpp>
#include <scene/Scene3D.hpp>
#include <support/GPUReadback.hpp>
#include <support/SDLError.hpp>

using namespace playground;
using namespace playground::sdl;
using namespace playground::sdl::gpu_detail;

namespace {
constexpr std::string_view usage =
    "Usage: playground_model_capture MODEL.gltf|MODEL.glb OUTPUT.bmp\n"
    "       playground_model_capture --help\n"
    "Render one 256x256 Vulkan image with the bundled studio environment.\n"
    "Models use bounds framing; Bistro.glb retains the reference street view.\n"
    "This explicit capture tool does not run material regression tests.\n";

scene::CameraView cameraFor(std::span<const scene::MeshDraw> draws,
                            const std::filesystem::path &model) {
  const float inf = std::numeric_limits<float>::infinity();
  math::Vec3f low{inf, inf, inf}, high{-inf, -inf, -inf};
  for (const auto &item : draws) {
    const auto box = item.mesh->bounds();
    for (int corner = 0; corner < 8; ++corner) {
      const auto point = math::transformPoint(
          item.model, {corner & 1 ? box.maximum.x : box.minimum.x,
                       corner & 2 ? box.maximum.y : box.minimum.y,
                       corner & 4 ? box.maximum.z : box.minimum.z});
      if (!math::isFinite(point))
        throw std::invalid_argument("Model has nonfinite transformed bounds");
      low = {std::min(low.x, point.x), std::min(low.y, point.y),
             std::min(low.z, point.z)};
      high = {std::max(high.x, point.x), std::max(high.y, point.y),
              std::max(high.z, point.z)};
    }
  }
  if (!math::isFinite(low) || !math::isFinite(high))
    throw std::invalid_argument("Model contains no bounded geometry");
  const auto center = (low + high) * .5f, extent = high - low;
  const float size = std::max({extent.x, extent.y, extent.z});
  auto camera = scene::CameraProps{
      .eye = center + math::Vec3f{0, size * .35f, -size * 1.1f},
      .target = center,
      .nearPlane = .01f,
      .farPlane = 5000};
  if (model.filename() == "Bistro.glb")
    camera = {.eye = {24.82285f, 3.16055f, 61.64814f},
              .target = {24.50443f, 3.10232f, 60.70198f},
              .nearPlane = .02f,
              .farPlane = 5000};
  return camera.view(1);
}
} // namespace

int main(int argc, char **argv) {
  if (argc == 2 && std::string_view{argv[1]} == "--help") {
    std::cout << usage;
    return 0;
  }
  if (argc != 3 || std::string_view{argv[1]}.starts_with("--") ||
      std::string_view{argv[2]}.starts_with("--")) {
    std::cerr << usage;
    return 2;
  }
  try {
    const auto modelPath = std::filesystem::path{
        std::u8string{reinterpret_cast<const char8_t *>(argv[1])}};
    const auto outputPath = std::filesystem::path{
        std::u8string{reinterpret_cast<const char8_t *>(argv[2])}};
    if (std::filesystem::weakly_canonical(modelPath) ==
        std::filesystem::weakly_canonical(outputPath))
      throw std::invalid_argument(
          "Capture output cannot replace the input model");
#ifdef __APPLE__
    if (SDL_setenv_unsafe("MVK_CONFIG_LOG_LEVEL", "2", 0) != 0)
      throwSDLError("Cannot set default MoltenVK log level");
#endif
    SDLGuard sdl{SDL_INIT_VIDEO};
    if (!packagedShaderFormats() ||
        !SDL_GPUSupportsShaderFormats(packagedShaderFormats(), "vulkan"))
      throw std::runtime_error("Model capture requires the Vulkan GPU backend");
    scene::ModelImportProps limits;
    limits.allowMaterialFallback = true;
    limits.maxDocumentBytes = limits.maxResourceBytes = 320ULL * 1024 * 1024;
    limits.maxTotalResourceBytes = 1024ULL * 1024 * 1024;
    const auto model = loadGLTF(modelPath, limits);
    scene::Scene3D scene;
    model->instantiate(scene);
    const auto draws = scene.snapshot();
    scene::SceneRenderProps view{.camera = cameraFor(draws, modelPath),
                                 .pixelSize = {256, 256},
                                 .clearColor = {0, 0, 0, 255},
                                 .toneMap = true};
    const assets::AssetCatalog catalog{
        std::filesystem::path{PLAYGROUND_SOURCE_DIR} / "assets"};
    const auto data = catalog.read(
        assets::FileSource{"demo3d/studio_small_09_1k.hdr"}, 16 * 1024 * 1024);
    const auto environment = scene::prepareEnvironment(*decodeHDR(data));
    view.lighting.diffuseEnvironment = environment.diffuse;
    view.lighting.specularEnvironment = environment.specular;
    view.lighting.brdf = environment.brdf;
    view.lighting.directionToLight = {0, 0, -1};
    view.lighting.irradiance = {2, 2, 2};
    auto device = std::make_shared<GPUDevice>(
        GPUDeviceProps{packagedShaderFormats(), true, "vulkan"});
    PaintDevice paint{device};
    GPUSceneRenderer renderer{paint};
    const auto ordered = scene::orderedDraws(view.camera, draws);
    const auto image = std::dynamic_pointer_cast<const GPUImage>(
        renderer.render(view, ordered));
    const auto pixels = test::readPixels(device, *image);
    SDLResource<SDL_Surface, SDL_DestroySurface> capture{
        requireSDL(SDL_CreateSurface(256, 256, SDL_PIXELFORMAT_RGBA32),
                   "Cannot allocate capture surface")};
    for (int y = 0; y < 256; ++y)
      for (int x = 0; x < 256; ++x) {
        const auto &pixel = pixels[std::size_t(y) * 256 + x];
        for (const auto channel : pixel)
          if (!std::isfinite(channel))
            throw std::runtime_error("Model capture produced nonfinite pixels");
        const auto color =
            math::toSRGB({pixel[0], pixel[1], pixel[2], pixel[3]});
        if (!SDL_WriteSurfacePixel(capture.get(), x, y, color.r, color.g,
                                   color.b, color.a))
          throwSDLError("Cannot write capture pixel");
      }
    if (!SDL_SaveBMP(capture.get(), argv[2]))
      throwSDLError("Cannot save model capture");
    std::cout << argv[2] << '\n';
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Model capture: " << error.what() << '\n';
    return 1;
  }
}
