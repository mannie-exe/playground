#include <algorithm>

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

Mesh::Mesh(MeshData data) : _data{std::move(data)} {
  _data.validate();
  _bounds = {_data.vertices.front().position, _data.vertices.front().position};
  for (const auto &vertex : _data.vertices) {
    _bounds.minimum = {std::min(_bounds.minimum.x, vertex.position.x),
                       std::min(_bounds.minimum.y, vertex.position.y),
                       std::min(_bounds.minimum.z, vertex.position.z)};
    _bounds.maximum = {std::max(_bounds.maximum.x, vertex.position.x),
                       std::max(_bounds.maximum.y, vertex.position.y),
                       std::max(_bounds.maximum.z, vertex.position.z)};
  }
}

MeshHandle makeMesh(MeshData data) {
  return std::make_shared<const Mesh>(std::move(data));
}

void validate(const MaterialProps &material) {
  const auto validAddress = [](TextureAddress address) {
    return address == TextureAddress::Clamp ||
           address == TextureAddress::Repeat ||
           address == TextureAddress::MirroredRepeat;
  };
  if (!validAddress(material.addressU) || !validAddress(material.addressV) ||
      (material.sampling != rendering::Sampling::Nearest &&
       material.sampling != rendering::Sampling::Linear))
    throw std::invalid_argument("Invalid material sampler");
  if (!std::isfinite(material.alphaCutoff) || material.alphaCutoff < 0 ||
      material.alphaCutoff > 1 ||
      (material.alpha != MaterialProps::Alpha::Opaque &&
       material.alpha != MaterialProps::Alpha::Mask &&
       material.alpha != MaterialProps::Alpha::Blend))
    throw std::invalid_argument("Invalid material alpha settings");
  if (material.baseColorImage) {
    const auto size = material.baseColorImage->pixelSize();
    if (!math::isFinite(size) || !math::hasArea(size))
      throw std::invalid_argument("Invalid scene material image");
  }
}

void validate(const SceneRenderProps &view, std::span<const MeshDraw> draws) {
  if (!math::hasArea(view.pixelSize) || !math::isFinite(view.camera.view) ||
      !math::isFinite(view.camera.projection))
    throw std::invalid_argument("Invalid scene view");
  for (const auto &draw : draws) {
    validate(draw.material);
    if (!draw.mesh || !math::isFinite(draw.model))
      throw std::invalid_argument("Invalid scene draw");
    const auto matrix = view.camera.projection * view.camera.view * draw.model;
    if (!math::isFinite(matrix))
      throw std::overflow_error("Scene transform composition overflow");
    const auto bounds = draw.mesh->bounds();
    for (int corner = 0; corner < 8; ++corner) {
      const auto position =
          matrix * math::Vec4f{corner & 1 ? bounds.maximum.x : bounds.minimum.x,
                               corner & 2 ? bounds.maximum.y : bounds.minimum.y,
                               corner & 4 ? bounds.maximum.z : bounds.minimum.z,
                               1};
      if (!math::isFinite(position))
        throw std::overflow_error("Scene vertex transformation overflow");
    }
  }
}

std::vector<MeshDraw> orderedDraws(const CameraView &camera,
                                   std::span<const MeshDraw> draws,
                                   TransparentOrder order) {
  if (order != TransparentOrder::BackToFront &&
      order != TransparentOrder::Submission)
    throw std::invalid_argument("Invalid transparency ordering policy");
  validate({camera, {1, 1}, {}}, draws);
  struct Submission {
    MeshDraw draw;
    float depth;
  };
  std::vector<Submission> sorted;
  sorted.reserve(draws.size());
  for (const auto &draw : draws) {
    const auto bounds = draw.mesh->bounds();
    const math::Vec3f center{
        float((double(bounds.minimum.x) + bounds.maximum.x) * .5),
        float((double(bounds.minimum.y) + bounds.maximum.y) * .5),
        float((double(bounds.minimum.z) + bounds.maximum.z) * .5)};
    const auto depth = math::transformPoint(camera.view * draw.model, center).z;
    sorted.push_back({draw, depth});
  }
  const auto blended = std::stable_partition(
      sorted.begin(), sorted.end(), [](const Submission &item) {
        return item.draw.material.alpha != MaterialProps::Alpha::Blend;
      });
  if (order == TransparentOrder::BackToFront)
    std::stable_sort(blended, sorted.end(), [](const auto &a, const auto &b) {
      return a.depth > b.depth;
    });
  std::vector<MeshDraw> result;
  result.reserve(sorted.size());
  for (auto &item : sorted)
    result.push_back(std::move(item.draw));
  return result;
}

} // namespace playground::scene
