#pragma once

#include <memory>
#include <span>
#include <vector>

#include <scene/Scene3D.hpp>
#include <world/World.hpp>

namespace playground::scene {

struct WorldCamera {
  world::WorldPose pose;
  CameraProps lens;
  double focusDistance{1};
  std::uint64_t epoch{};
  bool operator==(const WorldCamera &) const = default;
  void validate(const world::SpatialLimits & = {}) const;
  static WorldCamera fromLocal(CameraProps, world::RenderOrigin,
                               std::uint64_t epoch,
                               const world::SpatialLimits & = {});
  CameraProps localCamera(world::RenderOrigin,
                          const world::SpatialLimits &) const;
  CameraView view(world::RenderOrigin, const world::SpatialLimits &,
                  float aspect) const;
};

struct EntityVisual {
  world::EntityId entity;
  MeshDraw draw; // Model is local to the entity; assets remain immutable.
  bool operator==(const EntityVisual &) const = default;
};

class WorldSceneSnapshot {
  // Release accounting after the retained arrays.
  rendering::ResourceLedger::Token _charge;
  world::WorldSnapshot _world;
  world::RenderOrigin _origin;
  world::SpatialLimits _limits;
  WorldCamera _camera;
  std::vector<MeshDraw> _draws;
  std::vector<world::EntityId> _entities;
  std::shared_ptr<const void> _resourceOwner{std::make_shared<const int>(0)};
  std::size_t _omitted{};

  explicit WorldSceneSnapshot(world::WorldSnapshot);
  friend class SceneProjection;

public:
  const world::WorldSnapshot &world() const noexcept { return _world; }

  world::RenderOrigin origin() const noexcept { return _origin; }

  const world::SpatialLimits &limits() const noexcept { return _limits; }

  const WorldCamera &camera() const noexcept { return _camera; }

  std::span<const MeshDraw> draws() const noexcept { return _draws; }

  std::span<const world::EntityId> entities() const noexcept {
    return _entities;
  }

  std::shared_ptr<const void> resourceOwner() const noexcept {
    return _resourceOwner;
  }

  std::size_t omittedForExtent() const noexcept { return _omitted; }
};

// Owner-thread bindings; published snapshots retain immutable draw values.
class SceneProjection {
  rendering::ResourceLedger::Token _charge;
  std::shared_ptr<rendering::ResourceLedger> _ledger;
  std::vector<EntityVisual> _visuals;
  mutable std::weak_ptr<const WorldSceneSnapshot> _last;

public:
  explicit SceneProjection(std::vector<EntityVisual>,
                           std::shared_ptr<rendering::ResourceLedger>,
                           std::size_t maxVisuals = 65536);
  // Returns false for an unchanged draw; existing snapshots remain valid.
  bool setVisual(std::size_t index, EntityVisual);
  std::shared_ptr<const WorldSceneSnapshot>
  extract(world::WorldSnapshot, const WorldCamera &, world::RenderOrigin) const;
};

// A scene supplies entity-local model instances. Use separate bindings for
// independently moving world entities; static scenery can remain one batch.
struct SceneInstanceBinding {
  world::EntityId entity;
  std::shared_ptr<const Scene3D> scene;
};

struct SceneBindingStats {
  std::uint64_t rebuilds{}, updatedVisuals{};
};

class SceneInstanceProjection {
  std::shared_ptr<rendering::ResourceLedger> _ledger;
  rendering::ResourceLedger::Token _charge;
  std::vector<SceneInstanceBinding> _bindings;
  std::vector<std::uint64_t> _revisions;
  std::vector<std::size_t> _counts;
  std::unique_ptr<SceneProjection> _projection;
  SceneBindingStats _stats;
  std::size_t _maxVisuals;

public:
  SceneInstanceProjection(std::vector<SceneInstanceBinding>,
                          std::shared_ptr<rendering::ResourceLedger>,
                          std::size_t maxVisuals = 65536);
  std::shared_ptr<const WorldSceneSnapshot>
  extract(world::WorldSnapshot, const WorldCamera &, world::RenderOrigin);

  const SceneBindingStats &stats() const noexcept { return _stats; }
};

} // namespace playground::scene
