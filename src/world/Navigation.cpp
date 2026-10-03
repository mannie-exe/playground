#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>

#include <world/Navigation.hpp>

namespace playground::world {
namespace {
void nonnegative(double value) {
  if (!std::isfinite(value) || value < 0)
    throw std::invalid_argument("Invalid navigation metric");
}

void metric(double clearance, double height, double slope, double step = 0) {
  nonnegative(clearance);
  nonnegative(height);
  nonnegative(slope);
  nonnegative(step);
  if (height == 0 || slope > 1.5707963267948966)
    throw std::invalid_argument("Invalid navigation clearance/slope");
}

bool allows(const NavigationProfile &profile, const NavigationNode &node) {
  return node.clearance >= profile.radius && node.height >= profile.height &&
         node.slope <= profile.maximumSlope && (node.area & profile.areas);
}

bool allows(const NavigationProfile &profile, const NavigationLink &link) {
  return link.clearance >= profile.radius && link.height >= profile.height &&
         link.slope <= profile.maximumSlope &&
         link.step <= profile.maximumStep &&
         (profile.traversals & traversalMask(link.traversal)) &&
         (profile.permissions & link.permissions) == link.permissions;
}

// Closest point on a triangle, evaluated relative to its first vertex in
// double.
Vec3d closest(Vec3d p, Vec3d b, Vec3d c) {
  const auto d1 = dot(b, p), d2 = dot(c, p);
  if (d1 <= 0 && d2 <= 0)
    return {};
  const auto bp = p - b;
  const auto d3 = dot(b, bp), d4 = dot(c, bp);
  if (d3 >= 0 && d4 <= d3)
    return b;
  const auto vc = d1 * d4 - d3 * d2;
  if (vc <= 0 && d1 >= 0 && d3 <= 0)
    return b * (d1 / (d1 - d3));
  const auto cp = p - c;
  const auto d5 = dot(b, cp), d6 = dot(c, cp);
  if (d6 >= 0 && d5 <= d6)
    return c;
  const auto vb = d5 * d2 - d1 * d6;
  if (vb <= 0 && d2 >= 0 && d6 <= 0)
    return c * (d2 / (d2 - d6));
  const auto va = d3 * d6 - d5 * d4;
  if (va <= 0 && d4 - d3 >= 0 && d5 - d6 >= 0)
    return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
  const auto scale = 1 / (va + vb + vc);
  return b * (vb * scale) + c * (vc * scale);
}

WorldPosition project(const NavigationNode &node, WorldPosition point,
                      double radius) {
  if (!node.triangle)
    return node.position;
  const auto &v = node.triangle->vertices;
  auto position =
      translated(v[0], closest(relativeTo(point, v[0]), relativeTo(v[1], v[0]),
                               relativeTo(v[2], v[0])));
  double inward{};
  for (unsigned side = 0; side < 3; ++side) {
    const auto line = relativeTo(v[(side + 1) % 3], v[side]);
    const auto at =
        length(cross(line, relativeTo(position, v[side]))) / length(line);
    const auto center =
        length(cross(line, relativeTo(node.position, v[side]))) / length(line);
    if (at < radius)
      inward = std::max(inward, (radius - at) / (center - at));
  }
  return translated(position, relativeTo(node.position, position) *
                                  std::clamp(inward, 0.0, 1.0));
}
} // namespace

void NavigationProfile::validate() const {
  metric(radius, height, maximumSlope, maximumStep);
  if (!areas || !traversals || (traversals & ~15ULL))
    throw std::invalid_argument("Invalid navigation area/traversal mask");
}

void NavigationBudget::validate() const {
  if (!expansions || expansions > 1048576 || !frontier || frontier > 1048576 ||
      points < 2 || points > 65536 || !projectionVisits ||
      projectionVisits > 1048576 || !edgeVisits || edgeVisits > 4194304)
    throw std::invalid_argument("Invalid navigation work/output budget");
}

void NavigationSnapshotProps::validate() const {
  if (!maxTiles || maxTiles > 65536 || !maxNodes || maxNodes > 1048576 ||
      !maxLinks || maxLinks > 4194304)
    throw std::invalid_argument("Invalid navigation snapshot capacity");
}

struct NavigationSnapshot::Data {
  struct Tile {
    std::uint64_t revision;
    bool complete;
    CellLease lease;
  };

  runtime::ResourceLedger::Token charge;
  WorldSnapshot source;
  WorldId world;
  WorldVersion version;
  std::uint64_t tick;
  std::map<CellId, Tile> tiles;
  std::vector<NavigationNode> nodes;
  std::vector<NavigationLink> links;
  std::map<NavigationNodeId, std::size_t> index;
  std::vector<std::vector<std::size_t>> outgoing;
  bool complete{true};
  std::optional<CellId> missing;

  explicit Data(WorldSnapshot value) : source{std::move(value)} {}
};

NavigationSnapshot::NavigationSnapshot(
    const WorldSnapshot &world, std::span<const NavigationTile> tiles,
    std::shared_ptr<runtime::ResourceLedger> ledger,
    NavigationSnapshotProps props, const FrameSnapshot *frames) {
  props.validate();
  if (!ledger || tiles.size() > props.maxTiles)
    throw std::invalid_argument("Invalid navigation snapshot input");
  std::size_t nodes{}, links{};
  for (const auto &tile : tiles) {
    if (tile.nodes.size() > props.maxNodes - nodes ||
        tile.links.size() > props.maxLinks - links)
      throw std::length_error("Navigation snapshot exceeds data bounds");
    nodes += tile.nodes.size();
    links += tile.links.size();
  }
  auto charge = ledger->reserve(
      runtime::MemoryClass::CPU, runtime::ResourceKind::Navigation,
      sizeof(Data) + nodes * (sizeof(NavigationNode) + 128) +
          links * (sizeof(NavigationLink) + 96) + tiles.size() * 128,
      "Navigation snapshot", {world.id().value, world.epoch(), 3});
  auto data = std::make_shared<Data>(world);
  data->charge = std::move(charge);
  data->world = world.id();
  data->version = world.version();
  data->tick = world.tick();
  data->complete = !tiles.empty();
  data->nodes.reserve(nodes);
  data->links.reserve(links);
  for (const auto &tile : tiles) {
    if (!tile.id.value || !tile.revision || !world.space(tile.id.space) ||
        unsigned(tile.representation) >
            unsigned(NavigationRepresentation::GroundMesh) ||
        !data->tiles
             .emplace(tile.id,
                      Data::Tile{tile.revision, tile.complete, tile.lease})
             .second)
      throw std::invalid_argument("Invalid navigation tile");
    if (tile.lease &&
        (tile.lease.cell() != tile.id ||
         tile.lease.revision() != tile.revision ||
         !satisfies(tile.lease.product().readiness(), Readiness::Navigation)))
      throw std::invalid_argument(
          "Navigation lease does not match tile product");
    if (!tile.complete) {
      data->complete = false;
      if (!data->missing || tile.id < *data->missing)
        data->missing = tile.id;
    }
    for (auto node : tile.nodes) {
      if (node.id.tile != tile.id || !node.id.value ||
          node.position.space != tile.id.space || !node.area)
        throw std::invalid_argument("Invalid navigation node identity");
      world.space(tile.id.space)->limits.validate(node.position);
      metric(node.clearance, node.height, node.slope);
      if (node.frame && (node.frame->frame.space != node.position.space ||
                         node.frame->frame.epoch != world.epoch() ||
                         !node.frame->frame.value ||
                         !isFinite(node.frame->offset) || node.triangle))
        throw std::invalid_argument(
            "Invalid/unsupported framed navigation node");
      if (node.frame) {
        if (!frames || frames->world().version() != world.version() ||
            frames->world().id() != world.id())
          throw std::invalid_argument(
              "Navigation frame sampling requires the same world boundary");
        const auto &sample = frames->resolve(node.frame->frame);
        node.position = worldPosition(*node.frame, sample,
                                      world.space(node.position.space)->limits);
        node.frameDiscontinuity = sample.discontinuity;
      }
      if (node.triangle) {
        const auto &v = node.triangle->vertices;
        for (auto vertex : v) {
          if (vertex.space != node.position.space)
            throw std::invalid_argument("Navigation triangle crosses spaces");
          world.space(tile.id.space)->limits.validate(vertex);
        }
        if (length(cross(relativeTo(v[1], v[0]), relativeTo(v[2], v[0]))) <=
            1e-12)
          throw std::invalid_argument(
              "Degenerate navigation projection triangle");
      }
      data->nodes.push_back(node);
    }
    data->links.insert(data->links.end(), tile.links.begin(), tile.links.end());
  }
  std::sort(data->nodes.begin(), data->nodes.end(),
            [](const auto &a, const auto &b) { return a.id < b.id; });
  for (std::size_t i = 0; i < data->nodes.size(); ++i)
    if (!data->index.emplace(data->nodes[i].id, i).second)
      throw std::invalid_argument("Repeated navigation node");
  std::sort(data->links.begin(), data->links.end(),
            [](const auto &a, const auto &b) { return a.id < b.id; });
  data->outgoing.resize(nodes);
  std::optional<NavigationLinkId> previous;
  for (std::size_t i = 0; i < data->links.size(); ++i) {
    const auto &link = data->links[i];
    metric(link.clearance, link.height, link.slope, link.step);
    if (!link.id.value || !data->tiles.contains(link.id.tile) ||
        link.id.tile != link.from.tile || link.id == previous ||
        !std::isfinite(link.cost) || link.cost <= 0 ||
        unsigned(link.traversal) > unsigned(TraversalKind::Transfer) ||
        !link.to.value || !link.to.tile.value ||
        link.to.tile.space.world != world.id() ||
        (link.from.tile.space != link.to.tile.space &&
         link.traversal != TraversalKind::Transfer))
      throw std::invalid_argument("Invalid navigation link");
    const auto from = data->index.find(link.from);
    if (from == data->index.end() ||
        (data->tiles.contains(link.to.tile) && !data->index.contains(link.to)))
      throw std::invalid_argument(
          "Navigation link references missing authored node");
    const auto target = data->index.find(link.to);
    if (!link.available || target == data->index.end()) {
      data->complete = false;
      if (!data->missing || link.to.tile < *data->missing)
        data->missing = link.to.tile;
    }
    const auto fromFrame = data->nodes[from->second].frame;
    const auto toFrame = target == data->index.end()
                             ? std::optional<FramePosition>{}
                             : data->nodes[target->second].frame;
    if (target != data->index.end() && link.traversal == TraversalKind::Walk &&
        (bool(fromFrame) != bool(toFrame) ||
         (fromFrame && fromFrame->frame != toFrame->frame)))
      throw std::invalid_argument(
          "Frame boundary requires an explicit traversal");
    if (link.portal) {
      if (fromFrame || toFrame || link.traversal != TraversalKind::Walk)
        throw std::invalid_argument("Portals require unframed walking edges");
      if (link.portal->space != link.from.tile.space ||
          link.to.tile.space != link.portal->space)
        throw std::invalid_argument("Navigation portal crosses spaces");
      world.space(link.portal->space)->limits.validate(*link.portal);
    }
    data->outgoing[from->second].push_back(i);
    previous = link.id;
  }
  data->charge->setState(runtime::AllocationState::Owned);
  _data = std::move(data);
}

WorldVersion NavigationSnapshot::worldVersion() const { return _data->version; }

WorldId NavigationSnapshot::world() const { return _data->world; }

std::uint64_t NavigationSnapshot::tick() const { return _data->tick; }

bool NavigationSnapshot::contains(CellId id, std::uint64_t revision) const {
  const auto found = _data->tiles.find(id);
  return found != _data->tiles.end() && found->second.revision == revision;
}

bool NavigationSnapshot::accepts(EntityHandle handle) const {
  try {
    _data->source.resolve(handle);
    return true;
  } catch (const std::invalid_argument &) {
    return false;
  }
}

bool NavigationSnapshot::sameTopology(const NavigationSnapshot &other) const {
  if (world() != other.world() ||
      worldVersion().epoch != other.worldVersion().epoch ||
      _data->tiles.size() != other._data->tiles.size())
    return false;
  for (const auto &[id, tile] : _data->tiles)
    if (!other.contains(id, tile.revision))
      return false;
  return true;
}

bool NavigationPath::valid(const NavigationSnapshot &snapshot) const {
  return snapshot.accepts(agent) &&
         snapshot.worldVersion().epoch == source.epoch &&
         std::all_of(dependencies.begin(), dependencies.end(),
                     [&](const auto &dependency) {
                       return snapshot.contains(dependency.tile,
                                                dependency.revision);
                     });
}

struct NavigationPlanner {
  static NavigationResult plan(const NavigationSnapshot &snapshot,
                               const NavigationRequest &request,
                               std::shared_ptr<runtime::ResourceLedger> ledger,
                               std::stop_token stop) {
    request.profile.validate();
    request.budget.validate();
    nonnegative(request.projectionTolerance);
    nonnegative(request.goalTolerance);
    const auto &data = *snapshot._data;
    if (!ledger || !request.agent.id.value ||
        request.agent.id.world != data.world ||
        request.agent.epoch != data.version.epoch || !request.goalRevision ||
        request.start.space.world != data.world ||
        request.goal.space.world != data.world ||
        !isFinite(request.start.meters) || !isFinite(request.goal.meters))
      throw std::invalid_argument("Invalid navigation request identity/space");
    NavigationResult result;
    if (stop.stop_requested()) {
      result.status = NavigationStatus::Cancelled;
      return result;
    }
    data.source.resolve(request.agent);
    const auto *startSpace = data.source.space(request.start.space);
    const auto *goalSpace = data.source.space(request.goal.space);
    if (!startSpace || !goalSpace)
      throw std::invalid_argument("Navigation endpoint has no declared space");
    startSpace->limits.validate(request.start);
    goalSpace->limits.validate(request.goal);
    bool complete = data.complete;
    result.missing = data.missing;
    const auto count = data.nodes.size();
    if (count > request.budget.projectionVisits) {
      result.status = NavigationStatus::BudgetExceeded;
      result.diagnostic = "Endpoint projection exceeds visit budget";
      return result;
    }
    const auto owner =
        runtime::ResourceOwner{data.world.value, data.version.epoch, 3};
    auto scratch = ledger->reserve(
        runtime::MemoryClass::CPU, runtime::ResourceKind::Preparation,
        count * (sizeof(double) + 3 * sizeof(std::size_t) + 1) +
            request.budget.frontier * 64 +
            request.budget.points * sizeof(std::size_t),
        "Navigation search scratch", owner);
    const auto projection = [&](WorldPosition position) {
      std::optional<std::pair<std::size_t, WorldPosition>> best;
      double distance = request.projectionTolerance;
      for (std::size_t i = 0; i < count; ++i) {
        if (stop.stop_requested())
          break;
        const auto &node = data.nodes[i];
        if (node.position.space != position.space ||
            !allows(request.profile, node))
          continue;
        const auto candidate = project(node, position, request.profile.radius);
        const auto separation = length(relativeTo(candidate, position));
        if (separation <= distance && (!best || separation < distance)) {
          best = {i, candidate};
          distance = separation;
        }
      }
      return best;
    };
    const auto start = projection(request.start),
               goal = projection(request.goal);
    if (stop.stop_requested()) {
      result.status = NavigationStatus::Cancelled;
      return result;
    }
    if (!start || !goal) {
      result.status =
          complete ? NavigationStatus::NoPath : NavigationStatus::NeedsData;
      result.diagnostic = "No eligible endpoint within projection tolerance";
      return result;
    }
    constexpr auto absent = std::numeric_limits<std::size_t>::max();
    std::vector<double> distance(count,
                                 std::numeric_limits<double>::infinity());
    std::vector<std::size_t> parent(count, absent);
    std::vector<bool> closed(count);
    std::set<std::pair<double, std::size_t>> frontier;
    distance[start->first] = 0;
    frontier.emplace(0, start->first);
    std::size_t best = start->first;
    double closestGoal =
        data.nodes[best].position.space == goal->second.space
            ? length(relativeTo(data.nodes[best].position, goal->second))
            : std::numeric_limits<double>::infinity();
    bool reached{}, exhausted{};
    while (!frontier.empty()) {
      if (stop.stop_requested()) {
        result.status = NavigationStatus::Cancelled;
        return result;
      }
      if (result.expansions >= request.budget.expansions) {
        exhausted = true;
        break;
      }
      result.peakFrontier = std::max(result.peakFrontier, frontier.size());
      const auto [cost, current] = *frontier.begin();
      frontier.erase(frontier.begin());
      closed[current] = true;
      ++result.expansions;
      if (current == goal->first) {
        best = current;
        reached = true;
        break;
      }
      const auto &node = data.nodes[current];
      if (node.position.space == goal->second.space) {
        const auto separation = length(relativeTo(node.position, goal->second));
        if (separation < closestGoal) {
          best = current;
          closestGoal = separation;
        }
      }
      for (auto edge : data.outgoing[current]) {
        if (result.edgeVisits >= request.budget.edgeVisits) {
          exhausted = true;
          break;
        }
        ++result.edgeVisits;
        const auto &link = data.links[edge];
        if (!allows(request.profile, link))
          continue;
        const auto target = data.index.find(link.to);
        if (!link.available || target == data.index.end()) {
          complete = false;
          if (!result.missing)
            result.missing = link.to.tile;
          continue;
        }
        const auto next = target->second;
        if (closed[next] || !allows(request.profile, data.nodes[next]))
          continue;
        const auto candidate = cost + link.cost;
        if (!std::isfinite(candidate))
          throw std::overflow_error("Navigation route cost overflow");
        if (candidate >= distance[next])
          continue;
        if (std::isfinite(distance[next]))
          frontier.erase({distance[next], next});
        if (frontier.size() >= request.budget.frontier) {
          exhausted = true;
          break;
        }
        distance[next] = candidate;
        parent[next] = edge;
        frontier.emplace(candidate, next);
      }
      if (exhausted)
        break;
    }
    if (exhausted) {
      result.status = NavigationStatus::BudgetExceeded;
      return result;
    }
    if (!reached && (complete || !request.allowPartial)) {
      result.status =
          complete ? NavigationStatus::NoPath : NavigationStatus::NeedsData;
      return result;
    }
    std::vector<std::size_t> chain;
    for (auto node = best;;) {
      if (chain.size() >= request.budget.points) {
        result.status = NavigationStatus::BudgetExceeded;
        return result;
      }
      chain.push_back(node);
      if (node == start->first)
        break;
      if (parent[node] == absent)
        throw std::logic_error("Broken navigation predecessor chain");
      node = data.index.at(data.links[parent[node]].from);
    }
    std::reverse(chain.begin(), chain.end());
    auto path = std::make_shared<NavigationPath>();
    path->charge = ledger->reserve(
        runtime::MemoryClass::CPU, runtime::ResourceKind::Navigation,
        sizeof(NavigationPath) +
            request.budget.points *
                (sizeof(NavigationWaypoint) + sizeof(NavigationDependency)),
        "Navigation corridor", owner);
    path->points.reserve(request.budget.points);
    path->dependencies.reserve(request.budget.points);
    path->source = data.version;
    path->agent = request.agent;
    path->goalRevision = request.goalRevision;
    path->projectedStart = start->second;
    path->projectedGoal = goal->second;
    path->goalTolerance = request.goalTolerance;
    path->complete = reached;
    path->cost = distance[best];
    const auto append = [&](NavigationWaypoint point) {
      if (!path->points.empty()) {
        const auto &previous = path->points.back();
        if (point.traversal == TraversalKind::Walk &&
            previous.position == point.position &&
            previous.frame == point.frame &&
            previous.frameDiscontinuity == point.frameDiscontinuity)
          return;
      }
      if (path->points.size() >= request.budget.points)
        throw std::length_error("Navigation waypoint output limit");
      path->points.push_back(point);
    };
    try {
      append({start->second,
              data.nodes[start->first].frame,
              {},
              TraversalKind::Walk,
              data.nodes[start->first].frameDiscontinuity});
      for (std::size_t i = 0; i < chain.size(); ++i) {
        const auto &node = data.nodes[chain[i]];
        if (std::none_of(
                path->dependencies.begin(), path->dependencies.end(),
                [&](const auto &d) { return d.tile == node.id.tile; })) {
          const auto &tile = data.tiles.at(node.id.tile);
          path->dependencies.push_back(
              {node.id.tile, tile.revision, tile.lease});
        }
        NavigationWaypoint point{node.position,
                                 node.frame,
                                 {},
                                 TraversalKind::Walk,
                                 node.frameDiscontinuity};
        if (i) {
          const auto &link = data.links[parent[chain[i]]];
          point.link = link.id;
          point.traversal = link.traversal;
          if (link.portal)
            append({*link.portal});
        }
        append(point);
      }
      if (reached)
        append({goal->second,
                data.nodes[goal->first].frame,
                {},
                TraversalKind::Walk,
                data.nodes[goal->first].frameDiscontinuity});
    } catch (const std::length_error &) {
      result.status = NavigationStatus::BudgetExceeded;
      return result;
    }
    path->charge->setState(runtime::AllocationState::Owned);
    result.path = std::move(path);
    result.status =
        reached ? NavigationStatus::Complete : NavigationStatus::Partial;
    return result;
  }
};

NavigationResult planNavigation(const NavigationSnapshot &snapshot,
                                const NavigationRequest &request,
                                std::shared_ptr<runtime::ResourceLedger> ledger,
                                std::stop_token stop) {
  const auto began = runtime::ActivityClock::now();
  NavigationResult result;
  try {
    result =
        NavigationPlanner::plan(snapshot, request, std::move(ledger), stop);
  } catch (const runtime::ResourcePressure &error) {
    result.status = NavigationStatus::BudgetExceeded;
    result.diagnostic = error.what();
  } catch (const std::exception &error) {
    result.status = NavigationStatus::Failed;
    result.diagnostic = error.what();
  }
  result.planningSeconds =
      std::chrono::duration<double>(runtime::ActivityClock::now() - began)
          .count();
  return result;
}
} // namespace playground::world
