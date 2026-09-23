#include <scene/SceneRenderer.hpp>

namespace playground::scene {

void MeshData::validate() const {
  if (vertices.empty() || indices.empty() || indices.size() % 3 != 0)
    throw std::invalid_argument("Scene mesh requires indexed triangles");
  for (const auto &v : vertices)
    if (!math::isFinite(v.position) || !math::isFinite(v.normal) ||
        !math::isFinite(v.uv))
      throw std::invalid_argument("Nonfinite mesh vertex");
  for (auto index : indices)
    if (index >= vertices.size())
      throw std::invalid_argument("Mesh index exceeds vertex count");
}

void validate(const SceneViewProps &view, std::span<const MeshDraw> draws) {
  if (!math::hasArea(view.pixelSize) || !math::isFinite(view.camera.view) ||
      !math::isFinite(view.camera.projection))
    throw std::invalid_argument("Invalid scene view");
  for (const auto &draw : draws) {
    if (!draw.mesh || !math::isFinite(draw.model))
      throw std::invalid_argument("Invalid scene draw");
    draw.mesh->validate();
    if (draw.material.baseColorImage) {
      const auto size = draw.material.baseColorImage->pixelSize();
      if (!math::isFinite(size) || !math::hasArea(size))
        throw std::invalid_argument("Invalid scene material image");
    }
  }
}

} // namespace playground::scene
