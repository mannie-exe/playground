#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

#include "GPUSceneRenderer.hpp"
#include <app/SDLGuard.hpp>
#include <assets/AssetCatalog.hpp>
#include <demo3d/Views.hpp>
#include <platform/sdl/GPURenderBackend.hpp>
#include <platform/sdl/ModelImport.hpp>
#include <platform/sdl/ProcessEnvironment.hpp>
#include <platform/sdl/TextureDecode.hpp>
#include <scene/Environment.hpp>
#include <scene/Scene3D.hpp>
#include <support/CommandLine.hpp>
#include <support/GPUReadback.hpp>
#include <support/SDLError.hpp>

using namespace playground;
using namespace playground::sdl;
using namespace playground::sdl::gpu_detail;

namespace {
constexpr std::string_view usage =
    "Usage: playground_model_capture MODEL.gltf|MODEL.glb OUTPUT.bmp [--view "
    "bounds|bistro]\n"
    "       playground_model_capture --help\n"
    "Render one 256x256 Vulkan image with the bundled studio environment.\n"
    "Default: bounds framing. --view bistro selects the reference street "
    "view.\n"
    "This explicit capture tool does not run material regression tests.\n";

} // namespace

int main(int argc, char **argv) {
  if (test::cli::helpRequested(argc, argv))
    return test::cli::help(usage);
  if ((argc != 3 && argc != 5) || std::string_view{argv[1]}.starts_with("--") ||
      std::string_view{argv[2]}.starts_with("--"))
    return test::cli::usageError(usage);
  std::string_view viewName = "bounds";
  if (argc == 5) {
    viewName = argv[4];
    if (std::string_view{argv[3]} != "--view" ||
        (viewName != "bounds" && viewName != "bistro"))
      return test::cli::usageError(usage, "Unknown capture view");
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
    sdl::configureProcessEnvironment();
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
    const auto camera =
        viewName == "bistro"
            ? scene::FreeCameraController{demo3d::bistroView}.camera(
                  {.nearPlane = .02f, .farPlane = 5000})
            : scene::boundsCamera(scene::drawBounds(draws),
                                  {.nearPlane = .01f, .farPlane = 5000});
    scene::SceneRenderProps view{.camera = camera.view(1),
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
