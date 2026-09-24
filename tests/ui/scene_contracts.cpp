#include <limits>

#include <scene/SceneRenderer.hpp>
#include <support/Test.hpp>

using namespace playground;

int main() {
  return test::run([] {
    scene::MeshData data{.vertices = {{{0, 0, 0}}, {{1, 0, 0}}, {{0, 1, 0}}},
                         .indices = {0, 1, 2}};
    auto mesh = scene::makeMesh(data);
    scene::SceneRenderProps view{.pixelSize = {64, 32}};
    std::vector<scene::MeshDraw> draws{{.mesh = mesh}};
    scene::validate(view, draws);
    scene::validate(view, {});
    test::rejects([] { scene::MeshData{}.validate(); }, "empty mesh rejected");
    data.indices = {0, 1};
    test::rejects([&] { scene::makeMesh(data); },
                  "incomplete triangle rejected");
    data.indices = {0, 1, 3};
    test::rejects([&] { scene::makeMesh(data); },
                  "out-of-range index rejected");
    data.indices = {0, 1, 2};
    data.vertices[0].uv.x = std::numeric_limits<float>::quiet_NaN();
    test::rejects([&] { scene::makeMesh(data); }, "nonfinite vertex rejected");
    test::require(mesh->data().vertices[0].uv.x == 0,
                  "editing authoring data cannot mutate published mesh");
    view.pixelSize.x = 0;
    test::rejects([&] { scene::validate(view, draws); },
                  "empty output rejected");
    view.pixelSize.x = 64;
    draws[0].model.at(0, 0) = std::numeric_limits<float>::infinity();
    test::rejects([&] { scene::validate(view, draws); },
                  "nonfinite model rejected");
    draws[0].model = {};
    draws[0].mesh.reset();
    test::rejects([&] { scene::validate(view, draws); }, "null mesh rejected");
  });
}
