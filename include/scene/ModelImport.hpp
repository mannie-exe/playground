#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <scene/Animation.hpp>
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
  std::shared_ptr<const void> modelIdentity;
};

class ModelAsset final {
  std::vector<ModelNode> _nodes;
  std::vector<std::string> _warnings;
  std::vector<AnimationClip> _clips;
  const std::shared_ptr<const void> _identity{std::make_shared<const int>(0)};

public:
  explicit ModelAsset(std::vector<ModelNode> nodes,
                      std::vector<std::string> warnings = {},
                      std::vector<AnimationClip> clips = {});
  ModelAsset &operator=(const ModelAsset &) = delete;
  ModelAsset &operator=(ModelAsset &&) = delete;

  const std::vector<ModelNode> &nodes() const noexcept { return _nodes; }

  const std::vector<AnimationClip> &clips() const noexcept { return _clips; }

  void applyAnimation(Scene3D &scene, const ModelInstance &instance,
                      std::size_t clip, const Playback &playback) const;

  const std::vector<std::string> &warnings() const noexcept {
    return _warnings;
  }

  ModelInstance instantiate(Scene3D &scene, math::Transform3D transform = {},
                            std::optional<ObjectId> parent = {}) const;
};

using ModelHandle = std::shared_ptr<const ModelAsset>;

struct ModelImportProps {
  float unitsPerMeter{1};
  // Explicit preview policy; geometry and unknown required extensions still
  // fail.
  bool allowMaterialFallback{};
  std::optional<std::size_t> sceneIndex;
  std::size_t maxDocumentBytes{16 * 1024 * 1024};
  std::size_t maxResourceBytes{128 * 1024 * 1024};
  std::size_t maxTotalResourceBytes{256 * 1024 * 1024};
  std::size_t maxVertices{4 * 1024 * 1024};
  std::size_t maxIndices{12 * 1024 * 1024};
  std::size_t maxNodes{65536};
  std::size_t maxParserBytes{64 * 1024 * 1024};
  std::size_t maxPreparationBytes{256 * 1024 * 1024};
  std::size_t maxAnimationClips{1024};
  std::size_t maxAnimationKeys{1024 * 1024};
  void validate() const;
};

struct ModelImportServices {
  // The host owns URI resolution/access policy. No network or file reads are
  // implicit.
  std::function<std::vector<std::byte>(std::string_view)> readResource;
  std::function<rendering::TextureHandle(
      std::span<const std::byte>, std::string_view, rendering::TextureRole)>
      decodeTexture;
};

// glTF 2.0 / GLB triangles and rigid TRS clips. RH +Y-up meters to LH +Y-up.
// Worker-safe when the supplied services are worker-safe; no GPU calls.
ModelHandle importGLTF(std::span<const std::byte> document,
                       const ModelImportServices &services = {},
                       const ModelImportProps &props = {});

} // namespace playground::scene
