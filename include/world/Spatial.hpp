#pragma once

#include <compare>
#include <cstdint>

#include <math/Geometry3D.hpp>

namespace playground::world {

struct WorldId {
  std::uint64_t value{};
  auto operator<=>(const WorldId &) const = default;
};

struct SpaceId {
  WorldId world;
  std::uint64_t value{};
  auto operator<=>(const SpaceId &) const = default;
};

struct EntityId {
  WorldId world;
  std::uint64_t value{};
  auto operator<=>(const EntityId &) const = default;
};

struct Vec3d {
  double x{}, y{}, z{};
  bool operator==(const Vec3d &) const = default;
};

bool isFinite(Vec3d) noexcept;
Vec3d operator+(Vec3d, Vec3d);
Vec3d operator-(Vec3d, Vec3d);
Vec3d operator*(Vec3d, double);
double dot(Vec3d, Vec3d);
Vec3d cross(Vec3d, Vec3d);
double length(Vec3d);
Vec3d normalized(Vec3d);
Vec3d rotate(math::Quaternion, Vec3d);
Vec3d inverseRotate(math::Quaternion, Vec3d);

struct WorldPosition {
  SpaceId space;
  Vec3d meters;
  bool operator==(const WorldPosition &) const = default;
};

struct WorldPose {
  WorldPosition position;
  math::Quaternion orientation{};
  bool operator==(const WorldPose &) const = default;
};

struct WorldVelocity {
  SpaceId space;
  Vec3d linear{}, angular{};
  bool operator==(const WorldVelocity &) const = default;
};

struct SpatialLimits {
  double maximumCoordinate{1e9};
  double positionTolerance{1e-5};
  double maximumLocalCoordinate{8192};
  double localTolerance{0.001};
  void validate() const;
  void validate(WorldPosition) const;
};

struct RenderOrigin {
  WorldPosition position;
  std::uint64_t revision{1};
  bool operator==(const RenderOrigin &) const = default;
};

void validate(SpaceId);
void validate(EntityId);
void validate(WorldPose, const SpatialLimits & = {});
void validate(WorldVelocity);
Vec3d relativeTo(WorldPosition, WorldPosition);
WorldPosition translated(WorldPosition, Vec3d, const SpatialLimits & = {});
math::Vec3f renderPosition(WorldPosition, RenderOrigin,
                           const SpatialLimits & = {});
WorldPosition worldPosition(math::Vec3f, RenderOrigin,
                            const SpatialLimits & = {});

// Rigid local offset, independent of render scale or numerical working origin.
struct LocalPose {
  Vec3d offset;
  math::Quaternion orientation{};
};

WorldPose compose(WorldPose parent, LocalPose, const SpatialLimits & = {});
LocalPose relativePose(WorldPose, WorldPose parent, const SpatialLimits & = {});
WorldVelocity attachedVelocity(WorldPose parent, WorldVelocity parentVelocity,
                               Vec3d localOffset, Vec3d localVelocity = {});

struct GridCoordinate {
  std::int64_t x{}, y{}, z{};
  auto operator<=>(const GridCoordinate &) const = default;
};

struct ChunkAxis {
  std::int64_t chunk{};
  std::uint32_t local{};
  bool operator==(const ChunkAxis &) const = default;
};

// Half-open cells; unlike truncation, negative positions belong to the lower
// cell.
GridCoordinate cellAt(WorldPosition, WorldPosition gridOrigin,
                      double cellMeters);
ChunkAxis splitBlock(std::int64_t block, std::uint32_t chunkExtent);
std::int64_t joinBlock(ChunkAxis, std::uint32_t chunkExtent);

} // namespace playground::world
