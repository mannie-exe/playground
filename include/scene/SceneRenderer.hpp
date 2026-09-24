#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <vector>

#include <math/Geometry3D.hpp>
#include <rendering/PaintImage.hpp>
#include <rendering/ResourceDomain.hpp>

namespace playground::scene {

struct Vertex3D {
  math::Vec3f position;
  math::Vec3f normal{0, 0, 1};
  math::Vec2f uv{};
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
  MeshData _data;
  Bounds3 _bounds;

public:
  explicit Mesh(MeshData data);
  Mesh(const Mesh &) = default;
  Mesh &operator=(const Mesh &) = delete;
  Mesh &operator=(Mesh &&) = delete;
  const MeshData &data() const noexcept { return _data; }
  const Bounds3 &bounds() const noexcept { return _bounds; }
};
using MeshHandle = std::shared_ptr<const Mesh>;
MeshHandle makeMesh(MeshData data);

// Unlit material description, not a PBR model. Textures share the image
// resource boundary with UI; scene submission must prepare them for its device.
enum class TextureAddress { Clamp, Repeat, MirroredRepeat };
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
};

void validate(const SceneRenderProps &view, std::span<const MeshDraw> draws);
void validate(const MaterialProps &material);

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
