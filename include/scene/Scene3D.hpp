#pragma once

#include <optional>

#include <scene/SceneRenderer.hpp>

namespace playground::scene {

struct ObjectId {
  std::uint64_t owner{};
  std::uint32_t index{}, generation{};
  bool operator==(const ObjectId &) const = default;
};

struct ObjectProps {
  math::Transform3D transform;
  MeshHandle mesh;
  MaterialProps material;
  bool visible{true};
};

struct ObjectPatch {
  std::optional<math::Transform3D> transform;
  std::optional<MeshHandle> mesh;
  std::optional<MaterialProps> material;
  std::optional<bool> visible;
};

struct Ray3 {
  math::Vec3f origin, direction;
};

struct PickResult {
  ObjectId object;
  float distance;
  math::Vec3f position;
};

Ray3 pickingRay(const CameraView &camera,
                math::Vec2f normalizedViewportPosition);
Bounds3 meshBounds(const Mesh &mesh);
std::optional<float> intersect(Ray3 ray, Bounds3 bounds);

struct CameraProps {
  math::Vec3f eye{0, 0, -3}, target{}, up{0, 1, 0};
  float verticalFov{1.04719755f};
  float nearPlane{0.1f}, farPlane{1000};
  std::optional<float> orthographicHeight;

  CameraView view(float aspect) const;
};

struct SceneCacheStats {
  std::uint64_t worldTransforms{}, visibilityUpdates{}, snapshots{};
};

// Owns object records, not GPU realizations. Handles belong to exactly one
// scene. Snapshotting retains immutable assets; mutation and rendering are
// single-threaded.
class Scene3D {
  struct Entry {
    std::optional<ObjectProps> props;
    std::optional<ObjectId> parent;
    std::uint32_t generation{1};
    std::vector<ObjectId> children;
    math::Matrix4 world;
    bool worldVisible{};
    bool worldDirty{true};
    bool visibilityDirty{true};
  };

  std::uint64_t _owner;
  mutable std::vector<Entry> _entries;
  std::vector<std::uint32_t> _free;
  std::uint64_t _revision{};
  mutable std::optional<std::uint64_t> _cachedRevision;
  mutable std::vector<MeshDraw> _cachedDraws;
  mutable SceneCacheStats _cacheStats;

  Entry &entry(ObjectId id);
  const Entry &entry(ObjectId id) const;
  void refreshWorldCache() const;
  std::vector<std::uint32_t> descendants(ObjectId id) const;

public:
  Scene3D();
  Scene3D(const Scene3D &) = delete;
  Scene3D &operator=(const Scene3D &) = delete;
  Scene3D(Scene3D &&) = delete;
  Scene3D &operator=(Scene3D &&) = delete;

  ObjectId create(ObjectProps props = {}, std::optional<ObjectId> parent = {});
  // Parent indices must refer to an earlier entry; absent attaches to parent.
  // Stages record storage before committing any authored scene mutation.
  std::vector<ObjectId>
  createBatch(std::span<const ObjectProps> objects,
              std::span<const std::optional<std::size_t>> parents,
              std::optional<ObjectId> parent = {});
  const ObjectProps &props(ObjectId id) const;
  void setProps(ObjectId id, ObjectProps props);
  void applyPatch(ObjectId id, const ObjectPatch &patch);
  void setParent(ObjectId id, std::optional<ObjectId> parent);
  void remove(ObjectId id);
  bool contains(ObjectId id) const noexcept;
  math::Matrix4 worldTransform(ObjectId id) const;
  std::vector<MeshDraw> snapshot() const;
  std::optional<PickResult> pick(Ray3 ray) const;

  std::uint64_t revision() const noexcept { return _revision; }

  const SceneCacheStats &cacheStats() const noexcept { return _cacheStats; }
};

} // namespace playground::scene
