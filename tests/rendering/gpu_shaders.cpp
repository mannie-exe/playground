#include <array>
#include <cmath>
#include <fstream>

#include <SDL3/SDL_log.h>
#include <SDL3/SDL_timer.h>

#include <app/SDLGuard.hpp>
#include <platform/Window.hpp>
#include <platform/sdl/GPUFrameAccess.hpp>
#include <platform/sdl/GPURenderBackend.hpp>
#include <platform/sdl/GPUResources.hpp>
#include <platform/sdl/GPUShaderPipeline.hpp>
#include <rendering/RenderFailure.hpp>
#include <support/Test.hpp>

using namespace playground;

static std::array<float, 4> readPixel(sdl::GPUDeviceHandle device,
                                      const sdl::GPUImage &image) {
  SDL_GPUTransferBufferCreateInfo transferInfo{
      SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD, 8};
  sdl::GPUResource<SDL_GPUTransferBuffer, SDL_ReleaseGPUTransferBuffer>
      transfer{device,
               SDL_CreateGPUTransferBuffer(device->get(), &transferInfo)};
  auto *commands = SDL_AcquireGPUCommandBuffer(device->get());
  test::require(commands != nullptr, "readback commands");
  auto *pass = SDL_BeginGPUCopyPass(commands);
  const SDL_GPUTextureRegion region{
      .texture = image.get(), .x = 4, .y = 4, .w = 1, .h = 1, .d = 1};
  const SDL_GPUTextureTransferInfo destination{.transfer_buffer =
                                                   transfer.get(),
                                               .pixels_per_row = 1,
                                               .rows_per_layer = 1};
  SDL_DownloadFromGPUTexture(pass, &region, &destination);
  SDL_EndGPUCopyPass(pass);
  sdl::GPUResource<SDL_GPUFence, SDL_ReleaseGPUFence> fence{
      device, SDL_SubmitGPUCommandBufferAndAcquireFence(commands)};
  auto *rawFence = fence.get();
  test::require(SDL_WaitForGPUFences(device->get(), true, &rawFence, 1),
                "readback completion");
  const auto *bytes = static_cast<const Uint16 *>(
      SDL_MapGPUTransferBuffer(device->get(), transfer.get(), false));
  test::require(bytes != nullptr, "readback mapping");
  std::array<float, 4> pixel;
  for (int i = 0; i < 4; ++i) {
    const unsigned exponent = (bytes[i] >> 10) & 31;
    const float fraction = (bytes[i] & 1023) / 1024.0f;
    pixel[i] = (bytes[i] & 32768 ? -1.f : 1.f) *
               (exponent ? std::ldexp(1 + fraction, int(exponent) - 15)
                         : std::ldexp(fraction, -14));
  }
  SDL_UnmapGPUTransferBuffer(device->get(), transfer.get());
  return pixel;
}

int main() {
  try {
    SDLGuard sdlGuard{SDL_INIT_VIDEO};
    if (!(sdl::packagedShaderFormats() & SDL_GPU_SHADERFORMAT_SPIRV) ||
        !SDL_GPUSupportsShaderFormats(SDL_GPU_SHADERFORMAT_SPIRV, "vulkan"))
      return 77;
    return test::run([] {
      const auto directory =
          std::filesystem::temp_directory_path() /
          ("playground-shaders-" + std::to_string(SDL_GetTicksNS()));
      test::require(std::filesystem::create_directory(directory),
                    "fresh test shader directory");
      struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() {
          std::error_code ignored;
          std::filesystem::remove_all(path, ignored);
        }
      } cleanup{directory};
      const std::filesystem::path source{PLAYGROUND_SHADER_TEST_DIR};
      const auto vs = directory / "vertex.spirv",
                 fs = directory / "fragment.spirv";
      std::filesystem::copy_file(source / "custom_pipeline_vertex.spirv", vs);
      std::filesystem::copy_file(source / "custom_pipeline_fragment.spirv", fs);
      Window window{WindowConfig{.windowedSize = {16, 16}, .hidden = true}};
      sdl::GPURenderBackend backend{*window.get(),
                                    rendering::GPUDriver::Vulkan};
      auto frame = backend.beginFrame({});
      auto *native = sdl::gpuAccess(*frame);
      test::require(native != nullptr,
                    "active GPU frame exposes explicit native extension");
      auto device = native->gpuDevice();
      assets::AssetCatalog catalog{directory};
      const assets::AssetId<assets::ShaderAsset> vertexId{"vertex"},
          fragmentId{"fragment"};
      catalog.add(vertexId,
                  assets::ShaderAsset{assets::FileSource{"vertex.spirv"},
                                      rendering::ShaderStage::Vertex});
      catalog.add(fragmentId,
                  assets::ShaderAsset{assets::FileSource{"fragment.spirv"},
                                      rendering::ShaderStage::Fragment,
                                      "main",
                                      {.uniformBuffers = 1}});
      catalog.freeze();
      sdl::GPUPipelineProps props{
          .vertex = {.path = vs},
          .fragment = {.path = fs, .layout = {.uniformBuffers = 1}},
          .colorTargets = {
              {.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT}}};
      sdl::GPUShaderPipeline pipeline{device, props};
      auto original = pipeline.snapshot();
      sdl::GPUShaderPipeline assetPipeline{
          device, props, assets::prepareShader(catalog, vertexId),
          assets::prepareShader(catalog, fragmentId)};
      test::require(assetPipeline.poll() == sdl::PipelineReload::Unchanged,
                    "immutable asset pipeline does not poll disk");
      const auto draw = [&](const auto &snapshot) {
        return native->renderOffscreen(
            {.size = {8, 8}}, [&](sdl::GPURecordingContext context) {
              snapshot->bind(context.pass);
              const std::array<float, 4> red{1, 0, 0, 1};
              snapshot->pushUniform(context.commands,
                                    rendering::ShaderStage::Fragment, 0,
                                    std::as_bytes(std::span{red}));
              SDL_DrawGPUPrimitives(context.pass, 3, 1, 0, 0);
            });
      };
      auto red = std::dynamic_pointer_cast<const sdl::GPUImage>(draw(original));
      auto assetRed = std::dynamic_pointer_cast<const sdl::GPUImage>(
          draw(assetPipeline.snapshot()));
      test::require(readPixel(device, *assetRed)[0] > .99f,
                    "catalog-prepared pipeline renders equivalently");
      test::require(readPixel(device, *red)[0] > .99f,
                    "custom pipeline executes and renders red");
      frame->paint2D().drawImage(red, {{}, red->pixelSize()},
                                 math::rect(0, 0, 16, 16), {});
      test::require(pipeline.poll() == sdl::PipelineReload::Unchanged,
                    "unchanged bytes avoid rebuild");
      {
        std::ofstream invalid{fs, std::ios::binary | std::ios::trunc};
        invalid << "broken";
      }
      test::require(pipeline.poll() == sdl::PipelineReload::Rejected &&
                        pipeline.snapshot() == original &&
                        !pipeline.lastError().empty(),
                    "failed reload preserves live pipeline");
      std::filesystem::copy_file(
          source / "custom_pipeline_fragment_alt.spirv", fs,
          std::filesystem::copy_options::overwrite_existing);
      test::require(pipeline.poll() == sdl::PipelineReload::Published &&
                        pipeline.snapshot()->generation() == 2,
                    "changed compatible shader publishes a new generation");
      auto blue = std::dynamic_pointer_cast<const sdl::GPUImage>(
          draw(pipeline.snapshot()));
      test::require(readPixel(device, *blue)[2] > .99f,
                    "reloaded shader actually changes output");
      test::require(
          readPixel(device, *std::dynamic_pointer_cast<const sdl::GPUImage>(
                                draw(original)))[0] > .99f,
          "retained old generation still draws correctly");
      test::rejects<std::runtime_error>(
          [&] {
            native->renderOffscreen({.size = {8, 8}}, [](auto) {
              throw std::runtime_error{"callback failure"};
            });
          },
          "throwing custom callback cancels producer without corrupting frame");
      frame->present();
      test::rejects<std::logic_error>(
          [&] { native->gpuDevice(); },
          "native extension rejects completed frame");
    });
  } catch (const std::exception &error) {
    SDL_Log("GPU shader test failed: %s", error.what());
    return 1;
  }
}
