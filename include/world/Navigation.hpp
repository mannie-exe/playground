#pragma once

#include <array>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <world/Frames.hpp>
#include <world/Streaming.hpp>

namespace playground::world {

struct NavigationNodeId {
  CellId tile;
  std::uint32_t value{};
  auto operator<=>(const NavigationNodeId &) const = default;
};

struct NavigationLinkId {
  CellId tile;
  std::uint64_t value{};
  auto operator<=>(const NavigationLinkId &) const = default;
};

enum class NavigationRepresentation { Graph, Grid, GroundMesh };
enum class TraversalKind { Walk, Jump, Board, Transfer };

constexpr std::uint64_t traversalMask(TraversalKind value) {
  return std::uint64_t{1} << unsigned(value);
}

struct NavigationProfile {
  double radius{.25}, height{1.8}, maximumSlope{.7853981633974483},
      maximumStep{.3};
  std::uint64_t areas{~std::uint64_t{0}}, traversals{1}, permissions{};
  void validate() const;
};

struct NavigationNode {
  NavigationNodeId id;
  WorldPosition position;
  std::optional<FramePosition> frame;
  std::uint64_t frameDiscontinuity{};
  double clearance{.5}, height{2}, slope{};
  std::uint64_t area{1};
  // Ground mesh projection uses the actual triangle, not a nearest centroid.
  std::optional<WorldTriangle> triangle;
};

struct NavigationLink {
  NavigationLinkId id;
  NavigationNodeId from, to;
  double cost{1}, clearance{.5}, height{2}, slope{}, step{};
  TraversalKind traversal{TraversalKind::Walk};
  std::uint64_t permissions{};
  bool available{true};
  std::optional<WorldPosition> portal;
};

struct NavigationTile {
  runtime::ResourceLedger::Token charge;
  CellId id;
  std::uint64_t revision{1};
  NavigationRepresentation representation{NavigationRepresentation::Graph};
  bool complete{true};
  std::vector<NavigationNode> nodes;
  std::vector<NavigationLink> links;
  // Pin an explicitly admitted streamed product, if this tile was streamed.
  CellLease lease;
  NavigationTile() = default;
  NavigationTile(NavigationTile &&) = default;
  NavigationTile &operator=(NavigationTile &&) = default;
};

struct NavigationGridCell {
  bool walkable{true};
  // Conservative default clearance is half a cell. Providers can publish
  // clearance derived from a larger occupancy neighborhood explicitly.
  std::optional<double> clearance;
  double height{2}, cost{1};
  std::uint64_t area{1};
};

NavigationTile navigationGrid(CellId, std::uint64_t revision,
                              WorldPosition origin, std::uint32_t width,
                              std::uint32_t depth, double cellMeters,
                              std::span<const NavigationGridCell>,
                              bool complete = true,
                              std::shared_ptr<runtime::ResourceLedger> =
                                  runtime::defaultResourceLedger());

struct NavigationTriangle {
  WorldTriangle geometry;
  double height{2}, cost{1};
  std::uint64_t area{1};
};

// Shared exact edges connect authored triangles. Narrow/acute passages use
// conservative center-to-portal clearance; no implicit geometry welding.
NavigationTile navigationMesh(CellId, std::uint64_t revision,
                              std::span<const NavigationTriangle>,
                              bool complete = true,
                              std::shared_ptr<runtime::ResourceLedger> =
                                  runtime::defaultResourceLedger());

struct NavigationSnapshotProps {
  std::size_t maxTiles{1024}, maxNodes{65536}, maxLinks{262144};
  void validate() const;
};

class NavigationSnapshot {
  struct Data;
  std::shared_ptr<const Data> _data;
  friend struct NavigationPlanner;

public:
  NavigationSnapshot(const WorldSnapshot &, std::span<const NavigationTile>,
                     std::shared_ptr<runtime::ResourceLedger>,
                     NavigationSnapshotProps = {},
                     const FrameSnapshot * = nullptr);
  WorldVersion worldVersion() const;
  WorldId world() const;
  std::uint64_t tick() const;
  bool contains(CellId, std::uint64_t revision) const;
  bool accepts(EntityHandle) const;
  bool sameTopology(const NavigationSnapshot &) const;
};

struct NavigationBudget {
  std::size_t expansions{4096}, frontier{8192}, points{256};
  std::size_t projectionVisits{65536};
  std::size_t edgeVisits{65536};
  void validate() const;
};

struct NavigationRequest {
  EntityHandle agent;
  std::uint64_t goalRevision{1};
  WorldPosition start, goal;
  NavigationProfile profile;
  NavigationBudget budget;
  double projectionTolerance{.5}, goalTolerance{.1};
  bool allowPartial{};
};

enum class NavigationStatus {
  Planning,
  Complete,
  Partial,
  NoPath,
  NeedsData,
  Unsupported,
  BudgetExceeded,
  Cancelled,
  Failed
};

struct NavigationWaypoint {
  WorldPosition position;
  std::optional<FramePosition> frame;
  NavigationLinkId link;
  TraversalKind traversal{TraversalKind::Walk};
  std::uint64_t frameDiscontinuity{};
};

struct NavigationDependency {
  CellId tile;
  std::uint64_t revision{};
  CellLease lease;
};

struct NavigationPath {
  runtime::ResourceLedger::Token charge;
  WorldVersion source;
  EntityHandle agent;
  std::uint64_t goalRevision{};
  WorldPosition projectedStart, projectedGoal;
  double goalTolerance{}, cost{};
  bool complete{};
  std::vector<NavigationWaypoint> points;
  std::vector<NavigationDependency> dependencies;
  NavigationPath() = default;
  NavigationPath(const NavigationPath &) = delete;
  bool valid(const NavigationSnapshot &) const;
};

struct NavigationResult {
  NavigationStatus status{NavigationStatus::Planning};
  std::shared_ptr<const NavigationPath> path;
  std::size_t expansions{}, peakFrontier{};
  std::size_t edgeVisits{};
  double planningSeconds{};
  std::optional<CellId> missing; // First unavailable dependency, stable order.
  std::string diagnostic;
};

// Bounded deterministic shortest-path planning. Zero heuristic is admissible
// for authored weights, frame links and cross-space traversals alike.
NavigationResult planNavigation(const NavigationSnapshot &,
                                const NavigationRequest &,
                                std::shared_ptr<runtime::ResourceLedger>,
                                std::stop_token = {});

class NavigationService {
  struct Impl;
  std::shared_ptr<Impl> _impl;
  static runtime::ServiceWork advanceImpl(Impl &,
                                          runtime::ActivityClock::time_point,
                                          runtime::ServiceWorkBudget);
  static runtime::ServiceDemand demandImpl(const Impl &);

public:
  NavigationService(NavigationSnapshot, runtime::Executor &,
                    std::shared_ptr<runtime::ResourceLedger>,
                    std::size_t maxRequests = 64);
  ~NavigationService();
  NavigationService(const NavigationService &) = delete;
  void setSnapshot(NavigationSnapshot);
  std::uint64_t request(NavigationRequest);
  void cancel(std::uint64_t);
  void forget(std::uint64_t);
  NavigationResult poll(std::uint64_t) const;
  runtime::ServiceWork advance(runtime::ActivityClock::time_point,
                               runtime::ServiceWorkBudget = {});
  runtime::ServiceDemand demand() const;
  runtime::ServiceHandle attach(runtime::ServiceScope &);
  void close() noexcept;
};

enum class NavigationState {
  Planning,
  Following,
  WaitingForData,
  Traversing,
  Arrived,
  Blocked,
  Failed,
  Cancelled
};

struct PathFollowerProps {
  double speed{3}, waypointTolerance{.1}, stallSeconds{2}, progressMeters{.01};
  void validate() const;
};

struct PathFollowResult {
  NavigationState state{NavigationState::Following};
  WorldVelocity desired;
  std::optional<double> facingRadians;
  std::optional<NavigationWaypoint> traversal;
};

class PathFollower {
  PathFollowerProps _props;
  std::shared_ptr<const NavigationPath> _path;
  std::size_t _next{};
  double _stalled{}, _bestDistance{};
  std::optional<NavigationLinkId> _traversing;
  bool _traversalCompleted{};
  std::optional<std::uint64_t> _discontinuity, _tick;
  NavigationState _state{NavigationState::Planning};

public:
  explicit PathFollower(PathFollowerProps = {});
  void follow(std::shared_ptr<const NavigationPath>);
  void cancel();
  // The app performs explicit traversals (including WorldTransfer), then
  // acknowledges success. Failed traversal stops without changing actual pose.
  void completeTraversal(NavigationLinkId link, bool success);
  PathFollowResult advance(const EntitySample &actual, double seconds,
                           const NavigationSnapshot &,
                           const FrameSnapshot *frames = nullptr);
};
} // namespace playground::world
