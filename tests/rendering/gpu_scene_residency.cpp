#include "GPUSceneRenderer.hpp"
#include <app/SDLGuard.hpp>
#include <platform/sdl/GPURenderBackend.hpp>
#include <support/GPUReadback.hpp>
using namespace playground;
using namespace playground::sdl;
using namespace playground::sdl::gpu_detail;

int main() {
  SDLGuard sdl{SDL_INIT_VIDEO};
  if (!packagedShaderFormats() ||
      !SDL_GPUSupportsShaderFormats(packagedShaderFormats(), "vulkan"))
    return 77;
  return test::run([] {
    auto ledger = std::make_shared<rendering::ResourceLedger>();
    auto device = std::make_shared<GPUDevice>(GPUDeviceProps{
        packagedShaderFormats(),
        true,
        "vulkan",
        {.maxMeshResidentBytes = 0, .maxMaterialResidentBytes = 0},
        ledger});
    PaintDevice paint{device};
    GPUSceneRenderer renderer{paint};
    auto mesh = scene::makeMesh(
        {{{{-.9f, -.9f, .5f}}, {{.9f, -.9f, .5f}}, {{0, .9f, .5f}}},
         {0, 2, 1}});
    auto red = rendering::makeTexture({{1, 1}, {{1, 0, 0, 1}}},
                                      rendering::TextureRole::Color);
    auto green = rendering::makeTexture({{1, 1}, {{0, 1, 0, 1}}},
                                        rendering::TextureRole::Color);
    scene::MeshDraw draw{mesh, {}, {}};
    scene::SceneRenderProps view{
        .pixelSize = {16, 16}, .resourceOwner = std::make_shared<const int>(1)};
    auto firstOwner = view.resourceOwner;
    draw.material.colorTexture.texture = red;
    auto render = [&] {
      return std::dynamic_pointer_cast<const GPUImage>(
          renderer.render(view, std::span{&draw, 1}));
    };
    test::require(test::readPixel(device, *render(), 8, 8)[0] > .9f,
                  "first scene shades red");
    view.resourceOwner = std::make_shared<const int>(2);
    draw.material.colorTexture.texture = green;
    test::require(test::readPixel(device, *render(), 8, 8)[1] > .9f,
                  "second scene shades green");
    renderer.takeWork();
    renderer.trimUnused();
    for (int i = 0; i < 8; ++i) {
      auto secondOwner = view.resourceOwner;
      view.resourceOwner = firstOwner;
      draw.material.colorTexture.texture = red;
      test::readPixel(device, *render(), 8, 8);
      view.resourceOwner = secondOwner;
      draw.material.colorTexture.texture = green;
      test::readPixel(device, *render(), 8, 8);
    }
    const auto warm = renderer.takeWork();
    test::require(!warm.uploads && !warm.evictions && warm.textureHits &&
                      warm.meshHits,
                  "two live owners retain overlapping working sets above zero "
                  "idle budget");
    firstOwner.reset();
    view.resourceOwner.reset();
    renderer.trimUnused();
    test::require(!renderer.textureResidentBytes() &&
                      !renderer.meshResidentBytes(),
                  "released owners make resources eligible for idle eviction");
    const auto snapshot = ledger->snapshot();
    auto budgets = snapshot.budgets;
    budgets.gpuBytes = snapshot.memory[1].bytes + 1;
    ledger->setBudgets(budgets);
    renderer.takeWork();
    test::rejects<rendering::ResourcePressure>(
        [&] { render(); }, "whole working set refused before material uploads");
    test::require(!renderer.takeWork().uploads,
                  "refused plan performs no partial scene uploads");
  });
}
