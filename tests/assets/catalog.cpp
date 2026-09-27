#include <array>
#include <type_traits>

#include <assets/AssetCatalog.hpp>
#include <support/Test.hpp>

using namespace playground;
using namespace playground::assets;

int main() {
  return test::run([] {
    static_assert(
        !std::is_convertible_v<AssetId<FontAsset>, AssetId<ImageAsset>>);
    AssetCatalog catalog{"."};
    AssetId<BinaryAsset> leaf{"leaf"}, left{"left"}, right{"right"},
        root{"root"};
    const BinaryAsset bytes{ByteSource{{std::byte{42}}}};
    catalog.add(root, bytes, {key(left), key(right)});
    catalog.add(left, bytes, {key(leaf)});
    catalog.add(right, bytes, {key(leaf)});
    catalog.add(leaf, bytes);
    test::rejects([&] { catalog.add(leaf, bytes); },
                  "duplicate identity rejected");
    test::rejects([&] { catalog.add(AssetId<BinaryAsset>{"bad/name"}, bytes); },
                  "invalid logical ID");
    test::rejects(
        [&] { catalog.add(AssetId<BinaryAsset>{"zero"}, bytes, {}, 0); },
        "zero revision rejected");
    test::rejects(
        [&] {
          catalog.add(AssetId<BinaryAsset>{"escape"},
                      BinaryAsset{FileSource{"../outside"}});
        },
        "relative escape rejected");
    test::rejects<std::logic_error>([&] { (void)catalog.cacheKey(key(root)); },
                                    "unfrozen acquisition rejected");
    catalog.freeze();
    const auto closure = catalog.dependencies(std::array{key(root)});
    test::require(closure.size() == 4 && closure.front() == key(leaf) &&
                      closure.back() == key(root),
                  "diamond dependencies are unique and dependency-first");
    test::rejects<std::logic_error>(
        [&] { catalog.add(AssetId<BinaryAsset>{"late"}, bytes); },
        "frozen registration rejected");
    test::rejects<std::out_of_range>(
        [&] { (void)catalog.definition(AssetId<FontAsset>{"leaf"}); },
        "typed identity does not alias another kind");
    test::require(catalog.read(bytes.source, 1) == std::vector{std::byte{42}},
                  "owned bytes read");
    test::rejects<std::length_error>(
        [&] { (void)catalog.read(bytes.source, 0); },
        "owned source obeys budget");
    AssetCatalog another{"."};
    another.add(leaf, BinaryAsset{ByteSource{{std::byte{3}}}});
    another.freeze();
    test::require(catalog.cacheKey(key(leaf)) != another.cacheKey(key(leaf)),
                  "distinct catalogs cannot collide in shared caches");
    AssetCatalog missing{"."};
    missing.add(root, bytes, {key(leaf)});
    test::rejects([&] { missing.freeze(); }, "missing dependency rejected");
    test::require(!missing.isFrozen(),
                  "failed freeze preserves registration state");
    missing.add(leaf, bytes);
    missing.freeze();
    AssetCatalog cycle{"."};
    cycle.add(root, bytes, {key(leaf)});
    cycle.add(leaf, bytes, {key(root)});
    test::rejects([&] { cycle.freeze(); }, "cycle rejected");
    AssetCatalog self{"."};
    self.add(root, bytes, {key(root)});
    test::rejects([&] { self.freeze(); }, "self dependency rejected");
    AssetCatalog deep{"."};
    for (int i = 0; i < 3000; ++i) {
      std::vector<AssetKey> deps;
      if (i)
        deps.push_back(
            key(AssetId<BinaryAsset>{"node" + std::to_string(i - 1)}));
      deep.add(AssetId<BinaryAsset>{"node" + std::to_string(i)}, bytes,
               std::move(deps));
    }
    deep.freeze();
    test::require(
        deep.dependencies(std::array{key(AssetId<BinaryAsset>{"node2999"})})
                .size() == 3000,
        "deep graph uses iterative traversal");
    AssetCatalog model{"."};
    model.add(
        AssetId<ModelAsset>{"model"},
        ModelAsset{.source = ByteSource{}, .resources = {{"mesh.bin", leaf}}});
    test::rejects([&] { model.freeze(); },
                  "model URIs add resource dependency edges");
    model.add(leaf, bytes);
    model.freeze();
    test::require(
        model.dependencies(std::array{key(AssetId<ModelAsset>{"model"})})
                .size() == 2,
        "model dependency included");
    AssetCatalog shader{"."};
    AssetId<ShaderAsset> shaderId{"broken"};
    shader.add(shaderId, ShaderAsset{ByteSource{{std::byte{1}}},
                                     rendering::ShaderStage::Vertex});
    shader.freeze();
    test::rejects([&] { (void)prepareShader(shader, shaderId); },
                  "invalid shader bytes rejected before publication");
    test::rejects<std::runtime_error>(
        [&] {
          (void)catalog.read(FileSource{"nonexistent-playground-test-file"},
                             16);
        },
        "missing file is a load error");
  });
}
