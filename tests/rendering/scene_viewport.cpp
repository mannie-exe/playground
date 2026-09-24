#include <limits>
#include <type_traits>

#include <scene/SceneViewport.hpp>
#include <support/Test.hpp>

using namespace playground;

int main() {
  return test::run([] {
    const auto close = [](float a, float b) { return std::abs(a - b) < 1e-4f; };
    const scene::CameraProps camera;
    const scene::SceneViewportProps props{
        math::rect(10, 20, 200, 100), {2, 3}, .5f, 1.f};
    const auto viewport = scene::resolveViewport(camera, props);
    test::require(viewport &&
                      viewport->contentBounds == math::rect(60, 20, 100, 100),
                  "fixed aspect centers in logical content bounds");
    test::require(viewport->pixelSize == math::Vec2i{100, 150},
                  "density and render scale affect pixels, not camera aspect");
    test::require(!viewport->normalizedPosition({20, 50}) &&
                      !viewport->normalizedPosition({160, 50}) &&
                      !viewport->normalizedPosition({80, 120}),
                  "letterboxes and trailing edges reject pointer mapping");
    test::require(viewport->normalizedPosition({60, 20}) == math::Vec2f{0, 0},
                  "leading edge belongs to viewport");
    const auto center = viewport->project({0, 0, 0});
    test::require(center && close(center->x, 110) && close(center->y, 70),
                  "world center projects to viewport center");
    const auto ray = viewport->rayAt(*center);
    test::require(ray && close(ray->direction.x, 0) &&
                      close(ray->direction.y, 0) && close(ray->direction.z, 1),
                  "project and picking agree");
    test::require(!viewport->project({0, 0, -4}), "behind-eye point rejected");
    test::require(!scene::resolveViewport(camera, {math::rect(0, 0, 0, 10)}),
                  "empty content requires no allocation");
    auto invalid = props;
    invalid.pixelScale.x = 0;
    test::rejects([&] { scene::resolveViewport(camera, invalid); },
                  "zero density rejected");
    invalid = props;
    invalid.aspectRatio = std::numeric_limits<float>::quiet_NaN();
    test::rejects([&] { scene::resolveViewport(camera, invalid); },
                  "nonfinite aspect rejected");
    invalid = props;
    invalid.resolutionScale = std::numeric_limits<float>::max();
    test::rejects<std::length_error>(
        [&] { scene::resolveViewport(camera, invalid); },
        "overflowing target rejected");

    scene::MeshData data{{{{-1, -1, 0}}, {{1, -1, 0}}, {{0, 1, 0}}}, {0, 1, 2}};
    const auto mesh = scene::makeMesh(data);
    data.vertices.front().position.x = -900;
    test::require(mesh->bounds().minimum.x == -1 &&
                      mesh->data().vertices.front().position.x == -1,
                  "published mesh separates authoring storage");
    static_assert(!std::is_copy_assignable_v<scene::Mesh>);
    static_assert(!std::is_move_assignable_v<scene::Mesh>);
    const scene::MaterialProps blend{.alpha =
                                         scene::MaterialProps::Alpha::Blend};
    const std::vector<scene::MeshDraw> draws{
        {mesh, blend, math::translation({0, 0, 1})},
        {mesh, {}, math::translation({0, 0, 2})},
        {mesh, blend, math::translation({0, 0, 4})}};
    const auto ordered = scene::orderedDraws(camera.view(1), draws);
    test::require(
        ordered[0].material.alpha == scene::MaterialProps::Alpha::Opaque &&
            ordered[1].model.at(2, 3) == 4 && ordered[2].model.at(2, 3) == 1,
        "opaque precedes stable far-to-near translucent objects");
    const auto authored = scene::orderedDraws(
        camera.view(1), draws, scene::TransparentOrder::Submission);
    test::require(authored[1].model.at(2, 3) == 1,
                  "submission policy preserves authored transparent order");
    const scene::CameraProps reversed{.eye = {0, 0, 10}, .target = {0, 0, 0}};
    const auto reverseOrder = scene::orderedDraws(reversed.view(1), draws);
    test::require(reverseOrder[1].model.at(2, 3) == 1,
                  "transparent order follows observer, not world z");
    test::rejects(
        [&] { scene::orderedDraws({}, draws, scene::TransparentOrder(99)); },
        "unknown ordering rejected");

    scene::Scene3D world;
    const auto parent = world.create({.transform = {.position = {1, 0, 0}}});
    const auto child = world.create({.mesh = mesh}, parent);
    auto previous = world.snapshot();
    world.applyPatch(parent,
                     {.transform = math::Transform3D{.position = {3, 0, 0}}});
    test::require(
        world.worldTransform(child).at(0, 3) == 3 &&
            previous.front().model.at(0, 3) == 1,
        "parent revisions refresh world cache without mutating snapshots");
    world.setParent(child, {});
    world.remove(parent);
    test::require(world.contains(child) &&
                      world.worldTransform(child).at(0, 3) == 0,
                  "reparent updates adjacency and preserves local transform");
    test::rejects([&] { world.create({.material = {.alphaCutoff = -1}}); },
                  "material validated even without geometry");
    auto leaf = child;
    for (int i = 0; i < 1000; ++i)
      leaf = world.create({}, leaf);
    world.worldTransform(leaf);
    world.remove(child);
    test::require(!world.contains(leaf) && world.snapshot().empty(),
                  "deep trees cache and remove without recursive traversal");
  });
}
