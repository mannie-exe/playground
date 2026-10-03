#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <variant>
#include <vector>

#include <world/World.hpp>

namespace playground::world {

struct WorldBounds {
  SpaceId space;
  Vec3d minimum, maximum;
  void validate(const SpatialLimits & = {}) const;
  bool contains(WorldPosition) const;
  bool contains(const WorldBounds &) const;
  bool intersects(const WorldBounds &) const;
};

struct WorldTriangle {
  std::array<WorldPosition, 3> vertices;
};

// Authored query membership is independent of render visibility or opacity.
struct QueryPrimitive {
  EntityId entity;
  std::uint64_t primitive{}, categories{1}, purposes{~std::uint64_t{0}};
  std::variant<WorldBounds, WorldTriangle> geometry;
};

struct QueryFilter {
  std::uint64_t categories{~std::uint64_t{0}}, purpose{1};
  std::span<const EntityId> exclude;
};

struct QueryBudget {
  std::size_t work{65536}, candidates{4096}, hits{256};
};

enum class QueryStatus {
  Complete,
  Incomplete,
  Unavailable,
  Unsupported,
  Invalid
};
enum class QueryPrecision { Bounds, Triangles, Mixed };
enum class QueryDiagnostic {
  None,
  MissingCoverage,
  WorkLimit,
  CandidateLimit,
  HitLimit,
  UnsupportedShape,
  InvalidInput
};

struct QueryHit {
  EntityId entity;
  std::uint64_t primitive{};
  double distance{};
  WorldPosition position;
  std::optional<Vec3d> normal;
};

struct QueryResult {
  // Charge outlives the output storage. Copies are forbidden to avoid uncharged
  // vector copies; move the result or retain its owner.
  rendering::ResourceLedger::Token charge;
  WorldVersion worldVersion;
  std::uint64_t tick{}, indexRevision{};
  QueryStatus status{QueryStatus::Invalid};
  QueryPrecision precision{QueryPrecision::Bounds};
  QueryDiagnostic diagnostic{QueryDiagnostic::InvalidInput};
  std::size_t work{}, candidates{};
  std::vector<QueryHit> hits;

  QueryResult() = default;
  QueryResult(QueryResult &&) noexcept = default;
  QueryResult &operator=(QueryResult &&) noexcept;
  QueryResult(const QueryResult &) = delete;
  QueryResult &operator=(const QueryResult &) = delete;
};

struct SpatialCoverage {
  WorldBounds bounds;
  // Complete means all declared query geometry intersecting bounds is present.
  // This assertion belongs to the provider, not to renderer visibility.
  QueryStatus status{QueryStatus::Complete};
};

struct SpatialSnapshotProps {
  std::size_t maxPrimitives{65536}, maxOutputHits{4096}, maxExclusions{4096};
};

class SpatialSnapshot {
  struct State;
  std::shared_ptr<const State> _state;
  friend class SpatialQueries;

public:
  SpatialSnapshot(WorldSnapshot, SpatialCoverage, std::uint64_t indexRevision,
                  std::span<const QueryPrimitive>,
                  std::shared_ptr<rendering::ResourceLedger>,
                  SpatialSnapshotProps = {});
  const WorldSnapshot &world() const;
  SpatialCoverage coverage() const;
  std::uint64_t revision() const;
};

class SpatialQueries {
public:
  static QueryResult raycast(const SpatialSnapshot &, WorldRay, double distance,
                             QueryFilter = {}, QueryBudget = {});
  // Closed AABB intersection; triangles use their conservative bounds. Results
  // explicitly report Bounds precision, not exact triangle/shape overlap.
  static QueryResult overlap(const SpatialSnapshot &, WorldBounds,
                             QueryFilter = {}, QueryBudget = {});
  static QueryResult sweep(const SpatialSnapshot &, WorldBounds, Vec3d,
                           QueryFilter = {}, QueryBudget = {});
};

} // namespace playground::world
