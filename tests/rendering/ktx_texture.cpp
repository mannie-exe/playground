#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>

#include <ktx.h>

#include <platform/sdl/TextureDecode.hpp>
#include <support/Test.hpp>

using namespace playground;

namespace {
void word(std::vector<std::byte> &bytes, std::size_t offset,
          std::uint32_t value) {
  for (unsigned i = 0; i < 4; ++i)
    bytes[offset + i] = std::byte((value >> (8 * i)) & 255);
}

std::vector<std::byte> fixture() {
  ktxTextureCreateInfo info{};
  info.vkFormat = 37;
  info.baseWidth = 2;
  info.baseHeight = 1;
  info.baseDepth = 1;
  info.numDimensions = 2;
  info.numLevels = 1;
  info.numLayers = 1;
  info.numFaces = 1;
  ktxTexture2 *raw{};
  test::require(ktxTexture2_Create(&info, KTX_TEXTURE_CREATE_NO_STORAGE,
                                   &raw) == KTX_SUCCESS,
                "create synthetic DFD");
  std::unique_ptr<ktxTexture2, decltype(&ktxTexture2_Destroy)> owner{
      raw, ktxTexture2_Destroy};
  const auto dfdSize = raw->pDfd[0];
  const auto pixelOffset = (104 + dfdSize + 7) & ~7u;
  std::vector<std::byte> bytes(pixelOffset + 8);
  const std::array<unsigned char, 12> signature{
      0xab, 0x4b, 0x54, 0x58, 0x20, 0x32, 0x30, 0xbb, 0x0d, 0x0a, 0x1a, 0x0a};
  std::memcpy(bytes.data(), signature.data(), 12);
  word(bytes, 12, 37);
  word(bytes, 16, 1);
  word(bytes, 20, 2);
  word(bytes, 24, 1);
  word(bytes, 36, 1);
  word(bytes, 40, 1);
  word(bytes, 48, 104);
  word(bytes, 52, dfdSize);
  word(bytes, 80, pixelOffset);
  word(bytes, 88, 8);
  word(bytes, 96, 8);
  std::memcpy(bytes.data() + 104, raw->pDfd, dfdSize);
  const std::array<unsigned char, 8> pixels{255, 0, 0, 0, 0, 255, 0, 255};
  std::memcpy(bytes.data() + pixelOffset, pixels.data(), 8);
  return bytes;
}

std::vector<std::byte> read(const char *name) {
  std::ifstream file{std::filesystem::path{KTX_TEST_IMAGES} / name,
                     std::ios::binary | std::ios::ate};
  test::require(bool(file), "open pinned upstream KTX fixture");
  std::vector<std::byte> bytes(std::size_t(file.tellg()));
  file.seekg(0);
  file.read(reinterpret_cast<char *>(bytes.data()), bytes.size());
  return bytes;
}
} // namespace

int main() {
  return test::run([] {
    auto bytes = fixture();
    auto texture =
        sdl::decodeTexture(bytes, "image/ktx2", rendering::TextureRole::Color,
                           rendering::MipPolicy::Provided);
    test::require(
        texture->bytes() == 8 &&
            texture->levels()[0].texels[0] == math::Vec4f{1, 0, 0, 0},
        "KTX2 retains compact straight source including invisible RGB");
    test::require(
        sdl::decodeTexture(bytes, "image/ktx2", rendering::TextureRole::Color)
                ->levels()
                .size() == 2,
        "single-level containers can generate requested mips");
    auto mips = read("rgba-mipmap-reference-basis.ktx2");
    test::require(sdl::decodeTexture(mips, "image/ktx2",
                                     rendering::TextureRole::Color,
                                     rendering::MipPolicy::None)
                          ->levels()
                          .size() == 1,
                  "explicit no-mip requests discard provided tails");
    for (const char *name : {"cyan_rgb_reference_basis.ktx2",
                             "luminance_alpha_reference_uastc.ktx2",
                             "rgba-mipmap-reference-basis.ktx2"}) {
      auto basis = sdl::decodeKTX2(read(name), rendering::TextureRole::Color);
      test::require(basis->bytes() > 0 && basis->levels()[0].texels.format() ==
                                              rendering::TextureFormat::RGBA8,
                    "ETC1S/UASTC transcodes into portable compact pixels");
    }
    auto malformed = bytes;
    word(malformed, 20, 0xffffffff);
    test::rejects(
        [&] { sdl::decodeKTX2(malformed, rendering::TextureRole::Color); },
        "oversized header rejected before allocation");
    malformed = bytes;
    word(malformed, 36, 6);
    test::rejects(
        [&] { sdl::decodeKTX2(malformed, rendering::TextureRole::Color); },
        "cube maps require separate contract");
    malformed = bytes;
    malformed.pop_back();
    test::rejects(
        [&] { sdl::decodeKTX2(malformed, rendering::TextureRole::Color); },
        "truncated payload rejected");
    PreparationBudget tiny{1};
    test::rejects<std::length_error>(
        [&] {
          sdl::decodeKTX2(bytes, rendering::TextureRole::Color, 1024, tiny);
        },
        "admission rejects excessive preparation");
    test::require(tiny.snapshot().used == 0,
                  "failed admission has no leaked reservation");
  });
}
