#include <filesystem>
#include <fstream>

#include <platform/sdl/ModelImport.hpp>
#include <platform/sdl/TextureDecode.hpp>
#include <support/Test.hpp>

using namespace playground;

std::vector<std::byte> read(const char *name) {
  std::ifstream file{std::filesystem::path{PLAYGROUND_SOURCE_DIR} /
                         "assets/demo3d" / name,
                     std::ios::binary | std::ios::ate};
  test::require(bool(file), "sample file exists");
  std::vector<std::byte> bytes(static_cast<std::size_t>(file.tellg()));
  file.seekg(0);
  file.read(reinterpret_cast<char *>(bytes.data()), bytes.size());
  return bytes;
}

int main() {
  return test::run([] {
    scene::ModelImportProps props;
    props.maxTotalResourceBytes = 512 * 1024 * 1024;
    auto model = sdl::loadGLTF(std::filesystem::path{PLAYGROUND_SOURCE_DIR} /
                                   "assets/demo3d/BoomBox.glb",
                               props);
    test::require(!model->nodes().empty(), "licensed prop imports");
    bool pbr{};
    for (const auto &node : model->nodes())
      for (const auto &p : node.primitives)
        if (p.material.pbr) {
          pbr = true;
          test::require(bool(p.material.pbr->normalTexture.texture),
                        "prop retains normal texture");
        }
    test::require(pbr, "prop is metallic-roughness, not silently unlit");
    auto hdr = sdl::decodeHDR(read("studio_small_09_1k.hdr"));
    bool bright{};
    for (auto p : hdr->levels()[0].texels)
      bright |= p.x > 1 || p.y > 1 || p.z > 1;
    test::require(bright, "HDR values survive above SDR white");
    const auto hdrBytes = read("studio_small_09_1k.hdr");
    auto fullPrecision = sdl::decodeHDR(hdrBytes, 256 * 1024 * 1024,
                                        sdl::texturePreparationBudget(),
                                        rendering::TextureFormat::RGBA32F);
    test::require(fullPrecision->bytes() == hdr->bytes() * 2,
                  "explicit full-precision HDR storage is retained");
    test::rejects(
        [&] {
          sdl::decodeHDR(hdrBytes, 256 * 1024 * 1024,
                         sdl::texturePreparationBudget(),
                         rendering::TextureFormat::RGBA8);
        },
        "HDR decoding does not silently narrow to normalized bytes");
    auto smoke = sdl::decodeTexture(read("Smoke30Frames.png"), "image/png",
                                    rendering::TextureRole::Color,
                                    rendering::MipPolicy::None);
    const auto size = smoke->levels()[0].size;
    test::require(
        size == math::Vec2i{1536, 1279},
        "downloaded smoke is six by five cells with one cropped trailing row");
    test::rejects([&] { sdl::decodeHDR(read("Smoke30Frames.png")); },
                  "HDR decoder rejects LDR");
  });
}
