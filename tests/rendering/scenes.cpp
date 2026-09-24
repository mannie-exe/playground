#include <limits>

#include <platform/sdl/SoftwareSceneRenderer.hpp>
#include <platform/sdl/SurfacePaintImage.hpp>
#include <platform/sdl/SurfacePainter.hpp>
#include <scene/Scene2D.hpp>
#include <scene/Scene3D.hpp>
#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/content/Scene2DView.hpp>
#include <ui/content/SceneView.hpp>

using namespace playground;

static scene::MeshHandle triangle(float depth) {
  return scene::makeMesh(scene::MeshData{
      {{{-.8f, -.8f, depth}}, {{.8f, -.8f, depth}}, {{0, .8f, depth}}},
      {0, 1, 2}});
}
static math::ColorRGBA8 pixel(const rendering::PaintImageHandle &image, int x,
                              int y) {
  const auto &surface = dynamic_cast<const sdl::SurfacePaintImage &>(*image);
  math::ColorRGBA8 c;
  test::require(SDL_ReadSurfacePixel(surface.surface().get(), x, y, &c.r, &c.g,
                                     &c.b, &c.a),
                "read scene pixel");
  return c;
}

class CountingScene final : public scene::SceneRenderer {
  sdl::SoftwareSceneRenderer _renderer;

public:
  int calls{};
  rendering::PaintImageHandle
  render(const scene::SceneRenderProps &view,
         std::span<const scene::MeshDraw> draws) override {
    ++calls;
    return _renderer.render(view, draws);
  }
};

class CountingImages final : public rendering::ImagePreparer {
public:
  int calls{};
  rendering::ResourceDomainId domain{rendering::acquireResourceDomain()};
  rendering::ResourceDomainId resourceDomain() const noexcept override {
    return domain;
  }
  rendering::PaintImageHandle
  prepare(rendering::PaintImageHandle image) override {
    ++calls;
    return image;
  }
};

int main() {
  return test::run([] {
    auto scene = std::make_shared<scene::Scene3D>();
    const auto parent = scene->create({.transform = {.position = {1, 0, 0}}});
    const auto child = scene->create(
        {.transform = {.position = {0, 2, 0}}, .mesh = triangle(.5f)}, parent);
    test::require(math::transformPoint(scene->worldTransform(child), {}) ==
                      math::Vec3f{1, 2, 0},
                  "parent transform composes");
    test::rejects([&] { scene->setParent(parent, child); }, "cycle rejected");
    scene::Scene3D foreign;
    test::rejects([&] { foreign.props(child); }, "foreign handle rejected");
    auto snapshot = scene->snapshot();
    scene->remove(parent);
    test::require(!scene->contains(child) && snapshot.size() == 1,
                  "subtree deletion preserves snapshots");
    const auto replacement = scene->create();
    test::require(replacement != parent && !scene->contains(parent),
                  "slot reuse changes generation");
    test::rejects([&] { scene->setProps(child, {}); }, "stale update rejected");
    const auto hitObject = scene->create({.mesh = triangle(.5f)});
    const auto ray = scene::pickingRay({}, {.5f, .5f});
    const auto hit = scene->pick(ray);
    test::require(hit && hit->object == hitObject &&
                      std::abs(hit->distance - .5f) < 1e-5f,
                  "camera unprojection and geometric picking");
    test::require(
        scene::intersect(ray, scene::meshBounds(*triangle(.5f))).has_value(),
        "parallel slab ray reaches mesh bounds");
    scene->applyPatch(hitObject, {.visible = false});
    test::require(!scene->pick(ray), "hidden object not pickable");
    scene::Scene2D flat;
    const auto first =
        flat.create({.bounds = math::rect(0, 0, 2, 2), .zOrder = 10});
    flat.create({.bounds = math::rect(1, 0, 2, 2), .zOrder = 0});
    test::require(flat.snapshot().front().bounds.x() == 1,
                  "2D snapshot sorts z order");
    flat.remove(first);
    test::require(!flat.contains(first), "2D deletion invalidates identity");
    test::rejects([&] { flat.props(first); }, "stale 2D access rejected");
    const auto remaining = flat.create({.bounds = math::rect(0, 0, 2, 2)});
    flat.applyPatch(remaining, {.visible = false});
    test::require(!flat.props(remaining).visible, "false patch is not Keep");
    test::rejects(
        [&] {
          flat.applyPatch(remaining,
                          {.paint = rendering::ImagePaint{
                               .sampling = rendering::Sampling(99)}});
        },
        "invalid sampling rejected");
    test::require(!flat.props(remaining).visible,
                  "failed patch preserves previous props");

    sdl::SoftwareSceneRenderer renderer;
    const scene::SceneRenderProps view{{}, {32, 32}, {0, 0, 0, 255}};
    std::vector<scene::MeshDraw> draws{{triangle(.2f), {{255, 0, 0, 255}}, {}},
                                       {triangle(.8f), {{0, 0, 255, 255}}, {}}};
    auto output = renderer.render(view, draws);
    test::require(pixel(output, 16, 16).r == 255 && pixel(output, 0, 0).r == 0,
                  "depth wins independently of order");
    std::reverse(draws.begin(), draws.end());
    test::require(pixel(renderer.render(view, draws), 16, 16).r == 255,
                  "reversed depth ordering");
    draws = {{triangle(-.1f), {{255, 0, 0, 255}}, {}}};
    test::require(pixel(renderer.render(view, draws), 16, 16).r == 0,
                  "near clipping");
    draws = {{triangle(1.1f), {{255, 0, 0, 255}}, {}}};
    test::require(pixel(renderer.render(view, draws), 16, 16).r == 0,
                  "far clipping");
    auto clipped = triangle(.5f)->data();
    clipped.vertices[0].position.z = -.5f;
    draws = {{scene::makeMesh(std::move(clipped)), {{0, 255, 0, 255}}, {}}};
    test::require(pixel(renderer.render(view, draws), 16, 12).g > 0,
                  "crossing near plane retained");
    draws = {{triangle(.5f),
              {{255, 0, 0, 128}, {}, scene::MaterialProps::Alpha::Blend},
              {}}};
    const auto blended = pixel(renderer.render(view, draws), 16, 16);
    test::require(blended.r > 180 && blended.r < 190,
                  "linear light blend encoded for output");
    draws[0].material.alpha = scene::MaterialProps::Alpha::Mask;
    draws[0].material.alphaCutoff = .75f;
    test::require(pixel(renderer.render(view, draws), 16, 16).r == 0,
                  "alpha mask discards");
    test::rejects([&] { renderer.render({{}, {0, 1}, {}}, {}); },
                  "empty target rejected");
    auto overflowing = triangle(.5f)->data();
    overflowing.vertices[0].position.x = 2;
    const scene::MeshDraw invalidDraw{
        scene::makeMesh(std::move(overflowing)),
        {},
        math::scaling({std::numeric_limits<float>::max(), 1, 1})};
    test::rejects<std::overflow_error>(
        [&] { renderer.render(view, std::span{&invalidDraw, 1}); },
        "finite inputs cannot silently overflow transformed coordinates");

    SurfaceHandle texture{SDL_CreateSurface(4, 1, SDL_PIXELFORMAT_RGBA32),
                          SurfaceHandleDeleter{}};
    test::require(bool(texture), "scene texture allocation");
    for (int x = 0; x < 4; ++x)
      test::require(SDL_WriteSurfacePixel(texture.get(), x, 0, x == 0 ? 255 : 0,
                                          x == 1 ? 255 : 0, x == 2 ? 255 : 0,
                                          255),
                    "scene texture pixel");
    auto sampledMesh = triangle(.5f)->data();
    for (auto &vertex : sampledMesh.vertices)
      vertex.uv = {.2f, .5f};
    draws = {{scene::makeMesh(std::move(sampledMesh)),
              {{255, 255, 255, 255}, sdl::makeSurfaceImage(texture)},
              {}}};
    test::require(pixel(renderer.render(view, draws), 16, 16).r == 255,
                  "nearest uses floor(u * extent), like GPU sampling");
    const auto sampledTriangle = [&](float u) {
      auto data = triangle(.5f)->data();
      for (auto &vertex : data.vertices)
        vertex.uv = {u, .5f};
      return scene::makeMesh(std::move(data));
    };
    draws[0].mesh = sampledTriangle(.25f);
    draws[0].material.sampling = rendering::Sampling::Linear;
    auto filtered = pixel(renderer.render(view, draws), 16, 16);
    test::require(filtered.r > 180 && filtered.r < 190 && filtered.g > 180 &&
                      filtered.g < 190,
                  "material filtering interpolates decoded linear colors");
    draws[0].mesh = sampledTriangle(1.4f);
    draws[0].material.sampling = rendering::Sampling::Nearest;
    draws[0].material.addressU = scene::TextureAddress::Repeat;
    test::require(pixel(renderer.render(view, draws), 16, 16).g == 255,
                  "repeat addressing wraps UVs");
    draws[0].material.addressU = scene::TextureAddress::MirroredRepeat;
    test::require(pixel(renderer.render(view, draws), 16, 16).b == 255,
                  "mirrored repeat reflects alternating intervals");
    auto front = triangle(.5f)->data();
    std::swap(front.indices[1], front.indices[2]);
    draws = {{scene::makeMesh(std::move(front)),
              {.baseColor = {255, 0, 0, 255}, .doubleSided = false},
              {}}};
    test::require(pixel(renderer.render(view, draws), 16, 16).r == 255,
                  "LH front face survives backface culling");
    draws[0].model = math::scaling({-1, 1, 1});
    test::require(pixel(renderer.render(view, draws), 16, 16).r == 255,
                  "mirrored model compensates front-face winding");
    draws[0].model = {};
    draws[0].mesh = triangle(.5f);
    test::require(pixel(renderer.render(view, draws), 16, 16).r == 0,
                  "single-sided back face is rejected");

    auto flatWorld = std::make_shared<scene::Scene2D>();
    flatWorld->create({.bounds = math::rect(0, 0, 8, 8),
                       .paint = {.tint = {255, 0, 0, 255}}});
    SurfaceHandle flatTarget{SDL_CreateSurface(16, 16, SDL_PIXELFORMAT_RGBA32),
                             SurfaceHandleDeleter{}};
    test::require(bool(flatTarget), "2D target allocation");
    test::require(SDL_ClearSurface(flatTarget.get(), 0, 0, 0, 1),
                  "clear 2D target");
    sdl::SurfacePainter flatPainter{*flatTarget};
    ui::UIRoot flatRoot;
    flatRoot.setContent(std::make_unique<ui::Scene2DView>(
        ui::Scene2DViewProps{.scene = flatWorld,
                             .camera = math::Transform2D::translation({4, 4}),
                             .preferredSize = {16, 16}}));
    flatRoot.flushLayout(math::Size2{16, 16});
    flatRoot.prepare({});
    flatRoot.render(flatPainter);
    auto flatImage = sdl::makeSurfaceImage(flatTarget);
    test::require(pixel(flatImage, 5, 5).r == 255 &&
                      pixel(flatImage, 1, 1).r == 0,
                  "2D scene camera maps world into UI content");
    const auto imageItem =
        flatWorld->create({.bounds = math::rect(0, 0, 1, 1),
                           .image = sdl::makeSurfaceImage(texture)});
    CountingImages images;
    flatRoot.prepare({.images = &images});
    flatRoot.prepare({.images = &images});
    test::require(images.calls == 1,
                  "unchanged 2D scene preserves prepared snapshot");
    flatWorld->applyPatch(imageItem, {.zOrder = 3});
    flatRoot.prepare({.images = &images});
    test::require(images.calls == 2,
                  "2D scene revision refreshes prepared images");
    images.domain = rendering::acquireResourceDomain();
    flatRoot.prepare({.images = &images});
    test::require(images.calls == 3,
                  "resource domain change invalidates 2D cache");

    ui::UIRoot root;
    root.setContent(
        std::make_unique<ui::SceneView>(ui::SceneViewProps{.scene = scene}));
    root.flushLayout(math::Size2{32, 32});
    test::rejects<std::logic_error>([&] { root.prepare({}); },
                                    "missing scene capability rejected");
    CountingScene counted;
    root.prepare({.scenes = &counted});
    root.prepare({.scenes = &counted});
    test::require(counted.calls == 1, "unchanged scene output is reused");
    scene->applyPatch(hitObject, {.visible = true});
    root.prepare({.scenes = &counted});
    test::require(counted.calls == 2,
                  "model revision invalidates scene output");
  });
}
