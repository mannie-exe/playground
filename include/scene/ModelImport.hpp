#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

#include <scene/Scene3D.hpp>

namespace playground::scene {

struct ModelPrimitive {
  MeshHandle mesh;
  MaterialProps material;
};
struct ModelNode {
  std::string name;
  math::Transform3D transform;
  std::optional<std::size_t> parent;
  std::vector<ModelPrimitive> primitives;
};
struct ModelInstance {
  ObjectId root;
  std::vector<ObjectId> nodes;
};

class ModelAsset final {
  std::vector<ModelNode> _nodes;
  std::vector<std::string> _warnings;

public:
  explicit ModelAsset(std::vector<ModelNode> nodes,
                      std::vector<std::string> warnings = {});
  ModelAsset &operator=(const ModelAsset &) = delete;
  ModelAsset &operator=(ModelAsset &&) = delete;
  const std::vector<ModelNode> &nodes() const noexcept { return _nodes; }
  const std::vector<std::string> &warnings() const noexcept {
    return _warnings;
  }
  ModelInstance instantiate(Scene3D &scene, math::Transform3D transform = {},
                            std::optional<ObjectId> parent = {}) const;
};
using ModelHandle = std::shared_ptr<const ModelAsset>;

struct ModelImportProps {
  float unitsPerMeter{1};
  std::optional<std::size_t> sceneIndex;
  std::size_t maxDocumentBytes{16 * 1024 * 1024};
  std::size_t maxResourceBytes{128 * 1024 * 1024};
  std::size_t maxTotalResourceBytes{256 * 1024 * 1024};
  std::size_t maxVertices{4 * 1024 * 1024};
  std::size_t maxIndices{12 * 1024 * 1024};
  std::size_t maxNodes{65536};
  std::size_t maxParserBytes{64 * 1024 * 1024};
};
struct ModelImportServices {
  // The host owns URI resolution/access policy. No network or file reads are
  // implicit.
  std::function<std::vector<std::byte>(std::string_view)> readResource;
  std::function<rendering::PaintImageHandle(std::span<const std::byte>,
                                            std::string_view mimeType)>
      decodeImage;
};

// glTF 2.0 / GLB, static triangle scenes. Converts RH +Y-up meters to LH +Y-up.
// Worker-safe when the supplied services are worker-safe; no GPU calls.
ModelHandle importGLTF(std::span<const std::byte> document,
                       const ModelImportServices &services = {},
                       const ModelImportProps &props = {});

} // namespace playground::scene
