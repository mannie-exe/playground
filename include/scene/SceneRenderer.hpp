#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

#include <math/Geometry3D.hpp>
#include <rendering/PaintImage.hpp>
#include <rendering/ResourceDomain.hpp>
#include <rendering/Texture.hpp>
#include <support/PreparationBudget.hpp>

namespace playground::scene {

struct Vertex3D {
  math::Vec3f position;
  math::Vec3f normal{0, 0, 1};
  math::Vec2f uv{};
  math::Vec4f tangent{1, 0, 0, 1};
  math::Vec4f color{1, 1, 1, 1};
  math::Vec2f uv1{};
};

struct MeshData {
  std::vector<Vertex3D> vertices;
  std::vector<std::uint32_t> indices;
  void validate() const;
};

struct Bounds3 {
  math::Vec3f minimum, maximum;
};

// Mutable authoring data becomes a validated, immutable published resource.
class Mesh final {
  rendering::ResourceLedger::Token _allocation;
  MeshData _data;
  Bounds3 _bounds;

public:
  explicit Mesh(MeshData data);

  Mesh(const Mesh &other) : Mesh{other._data} {}

  Mesh &operator=(const Mesh &) = delete;
  Mesh &operator=(Mesh &&) = delete;

  const MeshData &data() const noexcept { return _data; }

  const Bounds3 &bounds() const noexcept { return _bounds; }
};

using MeshHandle = std::shared_ptr<const Mesh>;
MeshHandle makeMesh(MeshData data);

// Unlit artwork can share UI images. PBR uses interpretation-aware texture
// data.
using TextureAddress = rendering::TextureAddress;

struct MetallicRoughnessProps {
  math::Vec4f baseColor{1, 1, 1, 1};
  float metallic{1}, roughness{1};
  math::Vec3f emissive{};
  float normalScale{1}, occlusionStrength{1};
  rendering::TextureBinding baseColorTexture, metallicRoughnessTexture,
      normalTexture, occlusionTexture, emissiveTexture;
};

struct MaterialProps {
  math::ColorRGBA8 baseColor{255, 255, 255, 255};
  rendering::PaintImageHandle baseColorImage;
  // Opaque replaces destination alpha; Blend is submission-ordered and does
  // not write depth. Submit transparent objects back-to-front explicitly.
  enum class Alpha { Opaque, Mask, Blend } alpha{Alpha::Opaque};
  float alphaCutoff{0.5f};
  bool doubleSided{true};
  rendering::Sampling sampling{rendering::Sampling::Nearest};
  TextureAddress addressU{TextureAddress::Clamp};
  TextureAddress addressV{TextureAddress::Clamp};
  // Present selects PBR; absent keeps the existing explicit unlit path.
  std::optional<MetallicRoughnessProps> pbr;
  rendering::TextureBinding colorTexture;
};

struct MeshDraw {
  MeshHandle mesh;
  MaterialProps material;
  math::Matrix4 model;
};

struct CameraView {
  math::Matrix4 view;
  math::Matrix4 projection;
};

struct SceneRenderProps {
  CameraView camera;
  math::Vec2i pixelSize;
  math::ColorRGBA8 clearColor;

  struct Lighting {
    math::Vec3f directionToLight{0, 1, -1};
    math::Vec3f irradiance{3, 3, 3};
    rendering::TextureHandle diffuseEnvironment, specularEnvironment, brdf;
    float environmentIntensity{1};
    bool operator==(const Lighting &) const = default;
  } lighting;

  // Apply exposure to scene RGB on both backends; leave alpha unchanged.
  float exposure{1};
  // Optional GPU-only compression, after exposure and before UI composition.
  bool toneMap{};
  std::uint64_t workloadId{}, qualityRevision{};
  std::shared_ptr<const void> resourceOwner;
};

void generateNormals(MeshData &mesh);
void generateTangents(MeshData &mesh, unsigned uvSet = 0);

struct MeshPrepareProps {
  bool generateNormals{}, generateTangents{};
  unsigned tangentUVSet{};
  std::size_t maxScratchBytes{256 * 1024 * 1024};
  PreparationBudget *admission{}; // null selects the shared process budget
};

struct MeshPreparationStats {
  std::size_t sourceVertices{}, cornerRecords{}, finalVertices{};
  std::size_t sourceBytes{}, finalBytes{}, scratchEstimateBytes{};
};

// Transactional attribute generation and exact reindexing. Preserves triangle
// order/winding; scratch accounting is a conservative admission estimate.
MeshPreparationStats prepareMesh(MeshData &mesh, MeshPrepareProps props = {});

void validate(const SceneRenderProps &view, std::span<const MeshDraw> draws);
void validate(const MaterialProps &material);
MaterialProps unlitPreview(MaterialProps material);

enum class TransparentOrder { BackToFront, Submission };
std::vector<MeshDraw>
orderedDraws(const CameraView &camera, std::span<const MeshDraw> draws,
             TransparentOrder order = TransparentOrder::BackToFront);

// Frame-scoped, renderer-thread-only. The renderer retains/copies all data
// required by submitted work. Output is a sampleable image on the same backend;
// render scenes before opening/recording their consuming 2D composition.
class SceneRenderer {
public:
  virtual ~SceneRenderer() = default;

  virtual rendering::ResourceDomainId resourceDomain() const noexcept {
    return rendering::ResourceDomainId::cpu();
  }

  virtual rendering::PaintImageHandle
  render(const SceneRenderProps &view, std::span<const MeshDraw> draws) = 0;
};

} // namespace playground::scene
