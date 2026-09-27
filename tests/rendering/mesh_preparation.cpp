#include <array>
#include <limits>

#include <scene/SceneRenderer.hpp>
#include <support/Test.hpp>

using namespace playground;

int main() {
  return test::run([] {
    scene::MeshData quad{{{{0, 0, 0}, {0, 0, 1}, {0, 0}},
                          {{1, 0, 0}, {0, 0, 1}, {1, 0}},
                          {{1, 1, 0}, {0, 0, 1}, {1, 1}},
                          {{0, 1, 0}, {0, 0, 1}, {0, 1}}},
                         {0, 1, 2, 0, 2, 3}};
    const auto original = quad;
    auto stats = scene::prepareMesh(quad, {.generateTangents = true});
    test::require(
        stats.sourceVertices == 4 && stats.cornerRecords == 6 &&
            stats.finalVertices == 4 && quad.indices == original.indices,
        "tangent generation recovers quad sharing without triangle reorder");
    for (auto v : quad.vertices)
      test::require(v.tangent == math::Vec4f{1, 0, 0, 1},
                    "tangent survives reindexing");
    const auto stable = quad;
    scene::prepareMesh(quad);
    test::require(quad.indices == stable.indices && quad.vertices.size() == 4,
                  "reindexing is deterministic/idempotent");
    for (int attribute = 0; attribute < 6; ++attribute) {
      scene::MeshData split;
      for (auto i : original.indices)
        split.vertices.push_back(original.vertices[i]);
      split.indices = {0, 1, 2, 3, 4, 5};
      auto &v = split.vertices[3];
      switch (attribute) {
      case 0:
        v.position.z = 1;
        break;
      case 1:
        v.normal = {0, 1, 0};
        break;
      case 2:
        v.uv.x = .5f;
        break;
      case 3:
        v.tangent.w = -1;
        break;
      case 4:
        v.color.x = 0;
        break;
      case 5:
        v.uv1.y = .5f;
        break;
      }
      auto prepared = scene::prepareMesh(split);
      test::require(
          prepared.finalVertices == 5 && split.indices[0] != split.indices[3],
          "every attribute, including mirrored tangent sign, prevents welding");
    }
    test::rejects<std::length_error>(
        [&] { scene::prepareMesh(quad, {.maxScratchBytes = 1}); },
        "scratch refuses before expansion");
    test::require(quad.indices == stable.indices &&
                      quad.vertices.size() == stable.vertices.size(),
                  "budget failure preserves caller mesh");
    auto invalid = quad;
    invalid.indices[0] = 999;
    test::rejects([&] { scene::prepareMesh(invalid); },
                  "invalid index rejected");
    test::require(invalid.indices[0] == 999,
                  "failed validation does not mutate input");
  });
}
