#include <cmath>
#include <cstring>
#include <string>

#include <scene/ModelImport.hpp>
#include <support/Test.hpp>

using namespace playground;

namespace {
const std::string fixture = R"({
 "asset":{"version":"2.0"},
 "extensionsRequired":["KHR_materials_unlit","KHR_texture_transform"],
 "buffers":[{"byteLength":66,"uri":"data:application/octet-stream;base64,AACAvwAAgL8AAIA/AACAPwAAgL8AAIA/AAAAAAAAgD8AAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAABAAIA"}],
 "bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":24},{"buffer":0,"byteOffset":60,"byteLength":6}],
 "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5126,"count":3,"type":"VEC2"},{"bufferView":2,"componentType":5123,"count":3,"type":"SCALAR"}],
 "images":[{"uri":"data:image/png;base64,AA=="}],
 "samplers":[{"magFilter":9728,"minFilter":9728,"wrapS":33071,"wrapT":33648}],
 "textures":[{"source":0,"sampler":0}],
 "materials":[{"extensions":{"KHR_materials_unlit":{}},"alphaMode":"MASK","alphaCutoff":0.3,"pbrMetallicRoughness":{"baseColorFactor":[0.25,0.5,0.75,0.5],"baseColorTexture":{"index":0,"extensions":{"KHR_texture_transform":{"offset":[0.25,0.5],"scale":[0.5,0.25]}}}}}],
 "meshes":[{"primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":1},"indices":2,"material":0,"mode":4}]}],
 "nodes":[{"name":"root","translation":[1,2,3],"children":[1,2]},{"name":"trs","mesh":0,"translation":[0,0,1]},{"name":"matrix","mesh":0,"matrix":[1,0,0,0,0,1,0,0,0,0,1,0,2,0,0,1]}],
 "scenes":[{"nodes":[0]}],"scene":0
})";

std::span<const std::byte> bytes(std::string_view value) {
  return {reinterpret_cast<const std::byte *>(value.data()), value.size()};
}

std::string replaced(std::string value, std::string_view from,
                     std::string_view to) {
  const auto position = value.find(from);
  test::require(position != std::string::npos, "fixture replacement exists");
  value.replace(position, from.size(), to);
  return value;
}

class TestImage final : public rendering::PaintImage {
public:
  math::Size2 pixelSize() const noexcept override { return {1, 1}; }
};
} // namespace

int main() {
  return test::run([] {
    int decodes{};
    const scene::ModelImportServices services{
        .decodeTexture = [&](std::span<const std::byte> input, std::string_view,
                             rendering::TextureRole role) {
          test::require(input.size() == 1 && input[0] == std::byte{},
                        "data image bytes reach explicit decoder");
          ++decodes;
          return rendering::makeTexture({{1, 1}, {{1, 1, 1, 1}}}, role,
                                        rendering::MipPolicy::None);
        }};
    auto model =
        scene::importGLTF(bytes(fixture), services, {.unitsPerMeter = 2});
    test::require(model->nodes().size() == 3 && decodes == 1,
                  "selected hierarchy imported and texture decoded once");
    auto basisFixture =
        replaced(fixture, "\"KHR_texture_transform\"]",
                 "\"KHR_texture_transform\",\"KHR_texture_basisu\"]");
    basisFixture = replaced(
        basisFixture, "\"source\":0,\"sampler\":0",
        "\"extensions\":{\"KHR_texture_basisu\":{\"source\":0}},\"sampler\":0");
    auto basisModel = scene::importGLTF(bytes(basisFixture), services);
    test::require(!basisModel->nodes().empty() && decodes == 2,
                  "required Basis extension resolves through explicit decoder "
                  "once per model");
    test::require(model->warnings().empty(),
                  "supported unlit import is lossless within color precision");
    const auto &first = model->nodes()[1].primitives.front();
    const auto &second = model->nodes()[2].primitives.front();
    test::require(first.mesh == second.mesh,
                  "instanced source mesh shares immutable geometry");
    test::require(
        first.mesh->data().vertices[0].position == math::Vec3f{-2, -2, -2} &&
            first.mesh->data().vertices[1].position == math::Vec3f{0, 2, -2},
        "RH meters convert to LH units with reflected winding");
    test::require(
        first.mesh->data().vertices[0].uv == math::Vec2f{} &&
            first.material.colorTexture.transform.apply({}) ==
                math::Vec2f{.25f, .5f},
        "texture transform is binding-local, not baked into geometry");
    test::require(!first.material.doubleSided &&
                      first.material.alpha ==
                          scene::MaterialProps::Alpha::Mask &&
                      first.material.alphaCutoff == .3f &&
                      first.material.colorTexture.sampler.addressV ==
                          scene::TextureAddress::MirroredRepeat &&
                      first.material.colorTexture.sampler.magnification ==
                          rendering::Sampling::Nearest,
                  "alpha, culling and sampler semantics retained");
    scene::Scene3D world;
    const auto instance = model->instantiate(world);
    test::require(world.worldTransform(instance.nodes[1]).at(2, 3) == -8 &&
                      world.worldTransform(instance.nodes[2]).at(0, 3) == 6 &&
                      world.snapshot().size() == 2,
                  "TRS and matrix hierarchy instantiate correctly");
    const auto copy = model->instantiate(world, {.position = {10, 0, 0}});
    test::require(world.worldTransform(copy.nodes[1]).at(0, 3) == 12,
                  "instance root transform composes without changing asset");
    const auto snapshot = world.snapshot();
    world.remove(instance.root);
    test::require(!world.contains(instance.nodes[1]) && snapshot.size() == 4,
                  "instance removal does not invalidate published snapshots");
    const auto revision = world.revision();
    test::rejects(
        [&] {
          world.createBatch(std::array{scene::ObjectProps{}},
                            std::array{std::optional<std::size_t>{0}});
        },
        "invalid batch parenting rejected");
    test::require(world.revision() == revision,
                  "failed batch leaves scene unchanged");

    const auto reject = [&](const std::string &input, std::string_view why) {
      test::rejects([&] { scene::importGLTF(bytes(input), services); }, why);
    };
    reject(replaced(fixture, "KHR_materials_unlit\",\"KHR_texture_transform",
                    "KHR_unsupported\",\"KHR_texture_transform"),
           "required extension rejected");
    reject(replaced(fixture, "\"mode\":4", "\"mode\":1"),
           "line topology rejected");
    reject(replaced(fixture, "\"minFilter\":9728", "\"minFilter\":42"),
           "invalid sampler filter rejected");
    reject(replaced(fixture, "\"count\":3", "\"count\":4611686018427387904"),
           "overflowing accessor count rejected before native reads");
    reject(replaced(fixture, "\"byteOffset\":36",
                    "\"byteOffset\":18446744073709551615"),
           "overflowing buffer-view offset rejected");
    reject(replaced(fixture, "\"byteLength\":66", "\"byteLength\":65"),
           "buffer length mismatch rejected");
    reject(replaced(fixture, "\"children\":[1,2]", "\"children\":[0,2]"),
           "cyclic node hierarchy rejected");
    test::rejects([&] { scene::importGLTF(bytes(fixture)); },
                  "texture import cannot silently omit missing decoder");
    test::rejects(
        [&] { scene::importGLTF(bytes(fixture), services, {.sceneIndex = 3}); },
        "missing scene index rejected");
    test::rejects(
        [&] {
          scene::importGLTF(bytes(fixture), services, {.maxVertices = 2});
        },
        "geometry budget enforced");
    test::rejects(
        [&] {
          scene::importGLTF(bytes(fixture), services, {.unitsPerMeter = 0});
        },
        "invalid unit scale rejected");

    // GLB stores the same owned JSON with its buffer in an external-URI-free
    // BIN chunk.
    auto json =
        replaced(fixture,
                 ",\"uri\":\"data:application/"
                 "octet-stream;base64,AACAvwAAgL8AAIA/AACAPwAAgL8AAIA/"
                 "AAAAAAAAgD8AAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAABAAIA\"",
                 "");
    while (json.size() % 4)
      json += ' ';
    std::vector<std::byte> binary;
    const auto append32 = [&](std::uint32_t value) {
      for (int i = 0; i < 4; ++i)
        binary.push_back(std::byte((value >> (i * 8)) & 255));
    };
    append32(0x46546c67);
    append32(2);
    append32(std::uint32_t(12 + 8 + json.size() + 8 + 68));
    append32(std::uint32_t(json.size()));
    append32(0x4e4f534a);
    binary.insert(binary.end(), bytes(json).begin(), bytes(json).end());
    append32(68);
    append32(0x004e4942);
    const float values[]{-1, -1, 1, 1, -1, 1, 0, 1, 1, 0, 0, 1, 0, 0, 1};
    for (const float value : values) {
      std::uint32_t bits;
      std::memcpy(&bits, &value, sizeof(bits));
      append32(bits);
    }
    binary.insert(binary.end(),
                  {std::byte{0}, std::byte{0}, std::byte{1}, std::byte{0},
                   std::byte{2}, std::byte{0}, std::byte{0}, std::byte{0}});
    test::require(scene::importGLTF(binary, services)->nodes().size() == 3,
                  "GLB binary buffers are imported without filesystem access");
  });
}
