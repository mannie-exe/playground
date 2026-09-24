#include <bit>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include <platform/sdl/ModelImport.hpp>
#include <support/Test.hpp>

using namespace playground;

namespace {
class TemporaryDirectory {
  std::filesystem::path _path;

public:
  TemporaryDirectory() {
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    for (int attempt = 0; attempt < 100; ++attempt) {
      auto candidate = std::filesystem::temp_directory_path() /
                       ("playground-model-import-" + std::to_string(stamp) +
                        "-" + std::to_string(attempt));
      if (std::filesystem::create_directory(candidate)) {
        _path = std::move(candidate);
        return;
      }
    }
    throw std::runtime_error("Cannot create isolated model-import directory");
  }
  ~TemporaryDirectory() {
    std::error_code error;
    std::filesystem::remove_all(_path, error);
  }
  const std::filesystem::path &path() const noexcept { return _path; }
};

class WorkingDirectory {
  std::filesystem::path _previous{std::filesystem::current_path()};

public:
  explicit WorkingDirectory(const std::filesystem::path &path) {
    std::filesystem::current_path(path);
  }
  ~WorkingDirectory() {
    std::error_code error;
    std::filesystem::current_path(_previous, error);
  }
};

void writeModel(std::string_view uri) {
  std::ofstream model{"model.gltf", std::ios::binary};
  model << R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":36,"uri":")"
        << uri << R"("}],"bufferViews":[{"buffer":0,"byteLength":36}],
          "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"}],
          "meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}],
          "nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0})";
  model.close();
  test::require(bool(model), "write isolated glTF fixture");
}
} // namespace

int main() {
  return test::run([] {
    TemporaryDirectory directory;
    WorkingDirectory working{directory.path()};
    std::ofstream buffer{"mesh data.bin", std::ios::binary};
    for (const float value : {-1.f, -1.f, 0.f, 1.f, -1.f, 0.f, 0.f, 1.f, 0.f}) {
      const auto bits = std::bit_cast<std::uint32_t>(value);
      for (int shift = 0; shift < 32; shift += 8)
        buffer.put(char((bits >> shift) & 255));
    }
    buffer.close();
    test::require(bool(buffer), "write isolated binary fixture");

    writeModel("mesh%20data.bin");
    const auto relative = sdl::loadGLTF("model.gltf");
    test::require(
        relative->nodes().size() == 1 && relative->nodes()
                                                 .front()
                                                 .primitives.front()
                                                 .mesh->data()
                                                 .vertices.size() == 3,
        "bare relative document resolves percent-encoded sibling resource");
    const auto absolute = sdl::loadGLTF(directory.path() / "model.gltf");
    test::require(absolute->nodes().size() == relative->nodes().size(),
                  "relative and absolute document paths import equivalently");

    writeModel("../outside.bin");
    test::rejects([&] { sdl::loadGLTF("model.gltf"); },
                  "relative resource cannot escape asset directory");
    writeModel("https://example.invalid/model.bin");
    test::rejects([&] { sdl::loadGLTF("model.gltf"); },
                  "file convenience adapter rejects network URIs");
  });
}
