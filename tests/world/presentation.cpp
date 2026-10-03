#include <array>
#include <cmath>
#include <memory>
#include <vector>

#include <platform/sdl/SoftwareSceneRenderer.hpp>
#include <platform/sdl/SurfacePaintImage.hpp>
#include <scene/WorldScene.hpp>
#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/content/SceneView.hpp>

using namespace playground;
using namespace playground::world;
using test::require;

namespace {
class CountingScene final : public scene::SceneRenderer {
  sdl::SoftwareSceneRenderer _renderer;

public:
  int calls{};
  rendering::PaintImageHandle image;
  std::shared_ptr<const void> owner;

  rendering::PaintImageHandle
  render(const scene::SceneRenderProps &props,
         std::span<const scene::MeshDraw> draws) override {
    ++calls;
    owner = props.resourceOwner;
    image = _renderer.render(props, draws);
    return image;
  }
};

math::ColorRGBA8 pixel(const rendering::PaintImageHandle &image) {
  const auto &surface = dynamic_cast<const sdl::SurfacePaintImage &>(*image);
  math::ColorRGBA8 result;
  require(SDL_ReadSurfacePixel(surface.surface().get(), 32, 32, &result.r,
                               &result.g, &result.b, &result.a),
          "world presentation produces readable pixels");
  return result;
}

std::vector<math::ColorRGBA8> pixels(const rendering::PaintImageHandle &image) {
  const auto &surface = dynamic_cast<const sdl::SurfacePaintImage &>(*image);
  std::vector<math::ColorRGBA8> result(64 * 64);
  for (int y = 0; y < 64; ++y)
    for (int x = 0; x < 64; ++x) {
      auto &c = result[y * 64 + x];
      require(SDL_ReadSurfacePixel(surface.surface().get(), x, y, &c.r, &c.g,
                                   &c.b, &c.a),
              "read full rendered world image");
    }
  return result;
}

} // namespace

int main() {
  return test::run([] {
    auto ledger = std::make_shared<rendering::ResourceLedger>();
    const auto mesh = scene::makeMesh(
        {{{{-.5f, -.5f, 0}}, {{.5f, -.5f, 0}}, {{0, .5f, 0}}}, {0, 1, 2}});
    const SpaceId space{{1}, 1}, other{{1}, 2};
    const EntityId id{{1}, 1};
    scene::SceneProjection projection({{id,
                                        {mesh,
                                         {.baseColor = {230, 20, 50, 255}},
                                         math::translation({.02f, 0, 0})}}},
                                      ledger);
    std::vector<math::ColorRGBA8> reference;
    for (double offset : {0., 1e6, -1e6}) {
      World world({1}, ledger);
      const EntityProps props{{{space, {offset, 0, 2}}, {}}, {space}};
      const std::array<WorldMutation, 3> create{CreateSpace{{space, {}}},
                                                CreateSpace{{other, {}}},
                                                SpawnEntity{id, props}};
      world.apply(create, world.snapshot().version(), 0);
      scene::WorldCamera camera{.pose = {{space, {offset, 0, 0}}, {}}};
      const RenderOrigin origin{camera.pose.position, 1};
      const auto extracted =
          projection.extract(world.snapshot(), camera, origin);
      require(extracted->draws().size() == 1 &&
                  extracted->draws()[0].mesh == mesh &&
                  std::abs(extracted->draws()[0].model.at(0, 3) - .02f) < 1e-8,
              "world extraction preserves small authored offsets without "
              "copying mesh");
      require(projection.extract(world.snapshot(), camera, origin) == extracted,
              "unchanged world view reuses retained immutable extraction");
      auto view = std::make_unique<ui::SceneView>(ui::SceneViewProps{
          .preferredSize = {64, 64}, .worldScene = extracted});
      auto *node = view.get();
      ui::UIRoot root;
      root.setContent(std::move(view));
      root.flushLayout(math::Size2{64, 64});
      CountingScene renderer;
      root.prepare({.scenes = &renderer});
      root.prepare({.scenes = &renderer});
      require(renderer.calls == 1 && pixel(renderer.image).r == 230,
              "near and distant worlds render through retained UI without idle "
              "redraw");
      const auto rendered = pixels(renderer.image);
      if (reference.empty())
        reference = rendered;
      require(rendered == reference,
              "full rendered image matches near-origin reference");
      const auto beforeOwner = renderer.owner;
      const auto viewport = *node->viewport();
      const auto ray = viewport.worldRayAt({32, 32});
      require(ray && ray->origin.space == space &&
                  std::abs(ray->origin.meters.x - offset) < 1e-7 &&
                  ray->direction.z > .999,
              "picking returns a world-qualified ray using captured origin");
      const auto point = viewport.projectWorld({space, {offset, 0, 2}});
      require(point && std::abs(point->x - 32) < 1e-4 &&
                  std::abs(point->y - 32) < 1e-4,
              "precise world point projects into viewport content");
      test::rejects([&] { viewport.projectWorld({other, {offset, 0, 2}}); },
                    "foreign-space projection rejected");
      const RenderOrigin shifted{{space, {offset + 1, 0, 0}}, 2};
      const auto rebased =
          projection.extract(world.snapshot(), camera, shifted);
      node->applyPatch(
          {.worldScene =
               Patch<std::shared_ptr<const scene::WorldSceneSnapshot>>::set(
                   rebased)});
      root.prepare({.scenes = &renderer});
      require(
          renderer.calls == 2 && renderer.owner == beforeOwner &&
              pixel(renderer.image).r == 230,
          "rebasing refreshes pixels while preserving native resource owner");
      require(viewport.worldRayAt({32, 32})->origin.meters.x ==
                  ray->origin.meters.x,
              "old viewport retains its own origin after rebase");
      auto moved = props;
      moved.pose.position.meters.x += .1;
      const std::array<WorldMutation, 1> edit{SetEntity{id, moved}};
      world.apply(edit, world.snapshot().version(), 1);
      const auto changed = projection.extract(world.snapshot(), camera, origin);
      require(changed != extracted && changed->draws()[0].mesh == mesh &&
                  extracted->draws()[0].model.at(0, 3) < .03,
              "model edits invalidate extracted values without changing assets "
              "or old snapshots");
      camera.pose.position.meters.x += 20000;
      const auto distant = projection.extract(world.snapshot(), camera,
                                              {camera.pose.position, 3});
      require(
          distant->draws().empty() && distant->omittedForExtent() == 1,
          "objects outside representable view extent are explicitly counted");
    }
    scene::WorldCamera smallTurn{
        .pose = {{space, {5000, 0, 0}}, math::axisAngle({0, 1, 0}, .00001f)}};
    const auto direct = smallTurn.view({{space, {}}, 1}, {}, 1);
    require(
        std::abs(direct.view.at(0, 2)) > .000009,
        "camera orientation is not lost through float eye-target subtraction");
    test::rejects([&] { scene::SceneProjection invalid({{id, {}}}, ledger); },
                  "missing visual mesh rejected");
    test::rejects([&] { ui::SceneView invalid({}); },
                  "missing scene source rejected");
    scene::SceneViewport legacy{};
    require(!legacy.worldRayAt({0, 0}) && !legacy.projectWorld({space, {}}),
            "legacy local viewport does not invent a world origin");
  });
}
