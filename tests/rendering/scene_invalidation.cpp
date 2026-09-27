#include <limits>

#include <scene/Scene3D.hpp>
#include <support/Test.hpp>

using namespace playground;

int main() {
  return test::run([] {
    scene::Scene3D world;
    const auto mesh = scene::makeMesh(
        {{{{-1, -1, 0}}, {{1, -1, 0}}, {{0, 1, 0}}}, {0, 1, 2}});
    const auto a = world.create();
    const auto child = world.create({.mesh = mesh}, a);
    const auto b = world.create({.mesh = mesh});
    world.snapshot();
    auto before = world.cacheStats();
    world.applyPatch(a,
                     {.transform = math::Transform3D{.position = {3, 0, 0}}});
    test::require(world.worldTransform(child).at(0, 3) == 3 &&
                      world.worldTransform(b).at(0, 3) == 0,
                  "only changed branch inherits new transform");
    test::require(world.cacheStats().worldTransforms - before.worldTransforms ==
                      2,
                  "unrelated world matrices stay cached");
    before = world.cacheStats();
    world.applyPatch(child, {.material = scene::MaterialProps{
                                 .baseColor = {255, 0, 0, 255}}});
    world.snapshot();
    test::require(
        world.cacheStats().worldTransforms == before.worldTransforms &&
            world.cacheStats().visibilityUpdates == before.visibilityUpdates,
        "material change rebuilds draw list, not hierarchy values");
    before = world.cacheStats();
    world.applyPatch(a, {.visible = false});
    test::require(
        world.snapshot().size() == 1 &&
            world.cacheStats().worldTransforms == before.worldTransforms &&
            world.cacheStats().visibilityUpdates - before.visibilityUpdates ==
                2,
        "visibility invalidates descendants but not matrices");
    world.setParent(child, b);
    test::require(world.snapshot().size() == 2 &&
                      world.worldTransform(child).at(0, 3) == 0,
                  "reparent refreshes inherited visibility and pose");
    before = world.cacheStats();
    world.snapshot();
    test::require(world.cacheStats().snapshots == before.snapshots,
                  "unchanged snapshot reuses cache");
    world.remove(child);
    const auto recycled = world.create({.mesh = mesh}, a);
    test::require(!world.contains(child) &&
                      world.worldTransform(recycled).at(0, 3) == 3 &&
                      world.snapshot().size() == 1,
                  "recycled slot has fresh dirty state and identity");
    const auto overflowParent =
        world.create({.transform = {.scale = {1e30f, 1, 1}}});
    const auto overflowChild =
        world.create({.transform = {.scale = {1e30f, 1, 1}}}, overflowParent);
    test::rejects<std::overflow_error>([&] { world.snapshot(); },
                                       "world composition overflow rejected");
    world.applyPatch(overflowChild, {.transform = math::Transform3D{}});
    world.snapshot();
    test::require(math::isFinite(world.worldTransform(overflowChild)),
                  "failed cache rebuild can be repaired");
  });
}
