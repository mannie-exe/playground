#include <fstream>

#include <SDL3_image/SDL_image.h>

#include <platform/sdl/ModelImport.hpp>
#include <platform/sdl/SurfacePaintImage.hpp>
#include <platform/sdl/TextureDecode.hpp>
#include <support/SDLError.hpp>

namespace playground::sdl {
namespace {
std::vector<std::byte> readBytes(const std::filesystem::path &path,
                                 std::size_t limit) {
  std::ifstream file{path, std::ios::binary | std::ios::ate};
  if (!file)
    throw std::runtime_error("Cannot open model resource: " + path.string());
  const auto length = file.tellg();
  if (length < 0 || static_cast<std::uintmax_t>(length) > limit)
    throw std::length_error("Model resource exceeds read budget");
  std::vector<std::byte> bytes(static_cast<std::size_t>(length));
  file.seekg(0);
  if (!file.read(reinterpret_cast<char *>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size())))
    throw std::runtime_error("Cannot read model resource: " + path.string());
  return bytes;
}

std::filesystem::path relativeURI(std::string_view uri) {
  std::string decoded;
  const auto hex = [](char c) -> int {
    if (c >= '0' && c <= '9')
      return c - '0';
    if (c >= 'a' && c <= 'f')
      return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
      return c - 'A' + 10;
    throw std::invalid_argument("Invalid model URI encoding");
  };
  for (std::size_t i = 0; i < uri.size(); ++i) {
    if (uri[i] != '%')
      decoded += uri[i];
    else {
      if (i + 2 >= uri.size())
        throw std::invalid_argument("Incomplete model URI encoding");
      decoded += char(hex(uri[i + 1]) * 16 + hex(uri[i + 2]));
      i += 2;
    }
  }
  if (decoded.empty() || decoded.find('\0') != std::string::npos ||
      decoded.find_first_of(":?#\\") != std::string::npos)
    throw std::invalid_argument("Model URI must be a relative file path");
  const auto path = std::filesystem::u8path(decoded).lexically_normal();
  if (path.is_absolute() || path.has_root_name() || path.has_root_directory())
    throw std::invalid_argument("Absolute model URI is not allowed");
  for (const auto &part : path)
    if (part == "..")
      throw std::invalid_argument("Model URI escapes its asset directory");
  return path;
}
} // namespace

rendering::PaintImageHandle decodeModelImage(std::span<const std::byte> encoded,
                                             std::string_view mime,
                                             std::size_t maximumBytes) {
  if (!mime.empty() && mime != "image/png" && mime != "image/jpeg")
    throw std::invalid_argument("glTF images must be PNG or JPEG");
  const auto byte = [&](std::size_t i) {
    return std::to_integer<unsigned>(encoded[i]);
  };
  const bool png = encoded.size() >= 8 && byte(0) == 137 && byte(1) == 80 &&
                   byte(2) == 78 && byte(3) == 71 && byte(4) == 13 &&
                   byte(5) == 10 && byte(6) == 26 && byte(7) == 10;
  const bool jpeg =
      encoded.size() >= 3 && byte(0) == 255 && byte(1) == 216 && byte(2) == 255;
  if (!png && !jpeg)
    throw std::invalid_argument("Model texture is not a PNG/JPEG image");
  auto *stream = SDL_IOFromConstMem(encoded.data(), encoded.size());
  if (!stream)
    throwSDLError("Cannot open model image bytes");
  SurfaceHandle surface{IMG_Load_IO(stream, true), SurfaceHandleDeleter{}};
  if (!surface)
    throwSDLError("Cannot decode model image");
  if (surface->pitch < 0 || surface->h < 0 ||
      static_cast<std::uint64_t>(surface->pitch) * surface->h > maximumBytes)
    throw std::length_error("Decoded model image exceeds resident budget");
  return makeSurfaceImage(std::move(surface));
}

scene::ModelHandle loadGLTF(const std::filesystem::path &path,
                            const scene::ModelImportProps &props) {
  const auto documentPath = std::filesystem::absolute(path);
  const auto root =
      std::filesystem::weakly_canonical(documentPath.parent_path());
  const auto document = readBytes(documentPath, props.maxDocumentBytes);
  const scene::ModelImportServices services{
      .readResource =
          [&](std::string_view uri) {
            const auto target =
                std::filesystem::weakly_canonical(root / relativeURI(uri));
            const auto relative = target.lexically_relative(root);
            if (relative.empty() || relative.is_absolute())
              throw std::invalid_argument(
                  "Model resource is outside its directory");
            for (const auto &part : relative)
              if (part == "..")
                throw std::invalid_argument(
                    "Model resource symlink escapes its directory");
            return readBytes(target, props.maxResourceBytes);
          },
      .decodeTexture =
          [&](std::span<const std::byte> encoded, std::string_view mime,
              rendering::TextureRole role) {
            return decodeTexture(encoded, mime, role,
                                 rendering::MipPolicy::Generate,
                                 props.maxResourceBytes);
          }};
  return scene::importGLTF(document, services, props);
}

} // namespace playground::sdl
