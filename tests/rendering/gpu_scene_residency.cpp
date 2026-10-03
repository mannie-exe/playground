#include <array>
#include <memory>
#include <span>
#include <vector>

#include "GPUSceneRenderer.hpp"
#include <app/SDLGuard.hpp>
#include <platform/sdl/GPURenderBackend.hpp>
#include <scene/WorldScene.hpp>
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
    {
      const world::WorldId worldId{31};
      const world::SpaceId space{worldId, 1};
      const world::EntityId entity{worldId, 1};
      scene::SceneProjection projection(
          {{entity, {mesh, {.baseColor = {255, 0, 0, 255}}, {}}}}, ledger);
      std::vector<std::array<float, 4>> reference;
      renderer.takeWork();
      bool warmed{};
      for (const auto offset : {0., 1e6, -1e6}) {
        world::World state{worldId, ledger};
        const std::array<world::WorldMutation, 2> seed{
            world::CreateSpace{{space, {}}},
            world::SpawnEntity{entity,
                               {{{space, {offset, 0, 2}}, {}}, {space}}}};
        state.apply(seed, state.snapshot().version(), 0);
        scene::WorldCamera camera{.pose = {{space, {offset, 0, 0}}, {}},
                                  .epoch = state.snapshot().epoch()};
        for (const double rebase : {0., 1.}) {
          const auto extracted = projection.extract(
              state.snapshot(), camera, {{space, {offset + rebase, 0, 0}}, 1});
          scene::SceneRenderProps projected{
              .camera =
                  camera.view(extracted->origin(), extracted->limits(), 1),
              .pixelSize = {32, 32},
              .resourceOwner = extracted->resourceOwner()};
          const auto image = std::dynamic_pointer_cast<const GPUImage>(
              renderer.render(projected, extracted->draws()));
          const auto pixels = test::readPixels(device, *image);
          test::require(pixels[16 * 32 + 16][0] > .9f,
                        "world-relative native GPU view shades geometry");
          if (reference.empty())
            reference = pixels;
          test::require(pixels == reference,
                        "native world images match across large translations "
                        "and origin rebasing");
          const auto work = renderer.takeWork();
          if (warmed)
            test::require(
                !work.uploads && !work.evictions && work.meshHits,
                "world and origin changes reuse native immutable mesh uploads");
          warmed = true;
        }
      }
    }
    {
      const world::WorldId worldId{32};
      const world::SpaceId space{worldId, 1};
      const world::EntityId near{worldId, 1}, far{worldId, 2};
      world::World state{worldId, ledger};
      const std::array<world::WorldMutation, 3> seed{
          world::CreateSpace{{space, {}}},
          world::SpawnEntity{near, {{{space, {0, 0, 2}}, {}}, {space}}},
          world::SpawnEntity{far, {{{space, {1e6, 0, 2}}, {}}, {space}}}};
      state.apply(seed, state.snapshot().version(), 0);
      const auto farMesh = scene::makeMesh(mesh->data());
      scene::SceneProjection projection(
          {{near, {mesh, {}, {}}}, {far, {farMesh, {}, {}}}}, ledger);
      const scene::WorldCamera nearCamera{.pose = {{space, {}}, {}},
                                          .epoch = state.snapshot().epoch()},
          farCamera{.pose = {{space, {1e6, 0, 0}}, {}},
                    .epoch = state.snapshot().epoch()};
      const std::array views{projection.extract(state.snapshot(), nearCamera,
                                                {nearCamera.pose.position, 1}),
                             projection.extract(state.snapshot(), farCamera,
                                                {farCamera.pose.position, 1})};
      std::vector<std::array<float, 4>> reference;
      for (const auto &extracted : views) {
        test::require(
            extracted->draws().size() == 1 &&
                extracted->omittedForExtent() == 1,
            "simultaneous distant views each admit their local object");
        scene::SceneRenderProps projected{
            .camera = extracted->camera().view(extracted->origin(),
                                               extracted->limits(), 1),
            .pixelSize = {32, 32},
            .resourceOwner = extracted->resourceOwner()};
        const auto image = std::dynamic_pointer_cast<const GPUImage>(
            renderer.render(projected, extracted->draws()));
        const auto pixels = test::readPixels(device, *image);
        if (reference.empty())
          reference = pixels;
        test::require(
            pixels == reference && pixels[16 * 32 + 16][0] > .9f,
            "simultaneously retained GPU views do not share a mutable origin");
      }
      renderer.takeWork();
      renderer.trimUnused();
      for (const auto &extracted : views) {
        scene::SceneRenderProps projected{
            .camera = extracted->camera().view(extracted->origin(),
                                               extracted->limits(), 1),
            .pixelSize = {32, 32},
            .resourceOwner = extracted->resourceOwner()};
        const auto image = std::dynamic_pointer_cast<const GPUImage>(
            renderer.render(projected, extracted->draws()));
        test::readPixels(device, *image);
      }
      const auto stable = renderer.takeWork();
      test::require(
          !stable.uploads && !stable.evictions && stable.meshHits,
          "distinct extracted views preserve disjoint resident meshes");
    }
    renderer.trimUnused();
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
