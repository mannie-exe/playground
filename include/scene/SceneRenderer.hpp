#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <vector>

#include <math/Geometry3D.hpp>
#include <ui/PaintImage.hpp>

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
using MeshHandle = std::shared_ptr<const MeshData>;

// Unlit material description, not a PBR model. Textures share the image
// resource boundary with UI; scene submission must prepare them for its device.
struct MaterialProps {
  math::ColorRGBA8 baseColor{255, 255, 255, 255};
  ui::PaintImageHandle baseColorImage;
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
struct SceneViewProps {
  CameraView camera;
  math::Vec2i pixelSize;
  math::ColorRGBA8 clearColor;
};

void validate(const SceneViewProps &view, std::span<const MeshDraw> draws);

// Frame-scoped, renderer-thread-only. The renderer retains/copies all data
// required by submitted work. Output is a sampleable image on the same backend;
// render scenes before opening/recording their consuming 2D composition.
class SceneRenderer {
public:
  virtual ~SceneRenderer() = default;
  virtual ui::PaintImageHandle render(const SceneViewProps &view,
                                      std::span<const MeshDraw> draws) = 0;
};

} // namespace playground::scene
