#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>

#include <world/Navigation.hpp>

namespace playground::world {
namespace {
void positive(double value) {
  if (!std::isfinite(value) || value <= 0)
    throw std::invalid_argument("Navigation dimensions/costs must be positive");
}

void tileId(CellId tile, std::uint64_t revision) {
  validate(tile.space);
  if (!tile.value || !revision)
    throw std::invalid_argument("Navigation tile identity/revision required");
}

NavigationTile admitted(CellId tile, std::uint64_t revision, std::size_t nodes,
                        std::size_t links, bool complete,
                        std::shared_ptr<runtime::ResourceLedger> ledger) {
  tileId(tile, revision);
  if (!ledger)
    throw std::invalid_argument("Navigation preparation accounting required");
  NavigationTile result;
  result.charge = ledger->reserve(
      runtime::MemoryClass::CPU, runtime::ResourceKind::Navigation,
      nodes * sizeof(NavigationNode) + links * sizeof(NavigationLink) +
          sizeof(NavigationTile),
      "Navigation tile preparation", {tile.space.world.value, 0, 3});
  result.id = tile;
  result.revision = revision;
  result.complete = complete;
  result.nodes.reserve(nodes);
  result.links.reserve(links);
  return result;
}
} // namespace

NavigationTile navigationGrid(CellId tile, std::uint64_t revision,
                              WorldPosition origin, std::uint32_t width,
                              std::uint32_t depth, double meters,
                              std::span<const NavigationGridCell> cells,
                              bool complete,
                              std::shared_ptr<runtime::ResourceLedger> ledger) {
  if (!width || !depth || std::uint64_t(width) * depth > 65536 ||
      std::uint64_t(width) * depth != cells.size() ||
      origin.space != tile.space)
    throw std::invalid_argument("Invalid navigation grid dimensions");
  positive(meters);
  auto result = admitted(tile, revision, cells.size(), cells.size() * 4,
                         complete, ledger);
  result.representation = NavigationRepresentation::Grid;
  for (std::size_t i = 0; i < cells.size(); ++i) {
    const auto &cell = cells[i];
    positive(cell.cost);
    positive(cell.height);
    const auto clearance = cell.clearance.value_or(meters * .5);
    if (!std::isfinite(clearance) || clearance < 0)
      throw std::invalid_argument("Invalid grid clearance");
    if (!cell.walkable)
      continue;
    NavigationNode node;
    node.id = {tile, std::uint32_t(i + 1)};
    node.position = translated(origin, {(double(i % width) + .5) * meters, 0,
                                        (double(i / width) + .5) * meters});
    node.clearance = clearance;
    node.height = cell.height;
    node.area = cell.area;
    result.nodes.push_back(node);
    const auto x = std::int64_t(i % width), z = std::int64_t(i / width);
    for (const auto [dx, dz] : std::array<std::pair<int, int>, 4>{
             {{-1, 0}, {0, -1}, {0, 1}, {1, 0}}}) {
      const auto nx = x + dx, nz = z + dz;
      if (nx < 0 || nz < 0 || nx >= width || nz >= depth)
        continue;
      const auto other = std::size_t(nz) * width + std::size_t(nx);
      const auto &neighbor = cells[other];
      if (!neighbor.walkable)
        continue;
      NavigationLink link;
      link.id = {tile, result.links.size() + 1};
      link.from = node.id;
      link.to = {tile, std::uint32_t(other + 1)};
      link.cost = meters * neighbor.cost;
      link.clearance =
          std::min(clearance, neighbor.clearance.value_or(meters * .5));
      link.height = std::min(cell.height, neighbor.height);
      result.links.push_back(link);
    }
  }
  result.charge->setState(runtime::AllocationState::Owned);
  return result;
}

NavigationTile navigationMesh(CellId tile, std::uint64_t revision,
                              std::span<const NavigationTriangle> triangles,
                              bool complete,
                              std::shared_ptr<runtime::ResourceLedger> ledger) {
  if (triangles.size() > 65536)
    throw std::length_error("Navigation mesh exceeds triangle bound");
  auto result = admitted(tile, revision, triangles.size(), triangles.size() * 3,
                         complete, ledger);
  auto scratch = ledger->reserve(
      runtime::MemoryClass::CPU, runtime::ResourceKind::Preparation,
      sizeof(NavigationTile) + triangles.size() * 512,
      "Navigation mesh edge matching", {tile.space.world.value, 0, 3});
  result.representation = NavigationRepresentation::GroundMesh;

  struct Edge {
    std::size_t triangle, side;
  };

  std::map<std::array<double, 6>, std::vector<Edge>> edges;
  for (std::size_t i = 0; i < triangles.size(); ++i) {
    const auto &triangle = triangles[i];
    positive(triangle.cost);
    positive(triangle.height);
    const auto &v = triangle.geometry.vertices;
    for (auto vertex : v)
      if (vertex.space != tile.space || !isFinite(vertex.meters))
        throw std::invalid_argument(
            "Invalid navigation triangle space/position");
    const auto ab = relativeTo(v[1], v[0]), ac = relativeTo(v[2], v[0]);
    const auto normal = cross(ab, ac);
    if (!isFinite(normal) || length(normal) <= 1e-12)
      throw std::invalid_argument("Degenerate navigation triangle");
    NavigationNode node;
    node.id = {tile, std::uint32_t(i + 1)};
    node.position = translated(v[0], (ab + ac) * (1.0 / 3.0));
    node.triangle = triangle.geometry;
    node.height = triangle.height;
    node.area = triangle.area;
    node.slope =
        std::acos(std::clamp(std::abs(normalized(normal).y), 0.0, 1.0));
    node.clearance = std::numeric_limits<double>::infinity();
    for (std::size_t side = 0; side < 3; ++side) {
      const auto a = v[side], b = v[(side + 1) % 3];
      const auto edge = relativeTo(b, a);
      node.clearance = std::min(
          node.clearance,
          length(cross(edge, relativeTo(node.position, a))) / length(edge));
      std::array<double, 3> first{a.meters.x, a.meters.y, a.meters.z},
          second{b.meters.x, b.meters.y, b.meters.z};
      if (second < first)
        std::swap(first, second);
      std::array<double, 6> key{first[0],  first[1],  first[2],
                                second[0], second[1], second[2]};
      auto &shared = edges[key];
      if (shared.size() >= 2)
        throw std::invalid_argument("Non-manifold navigation mesh edge");
      shared.push_back({i, side});
    }
    result.nodes.push_back(node);
  }
  for (const auto &[key, shared] : edges) {
    if (shared.size() != 2)
      continue;
    for (unsigned direction = 0; direction < 2; ++direction) {
      const auto a = shared[direction], b = shared[1 - direction];
      const auto &from = result.nodes[a.triangle],
                 &to = result.nodes[b.triangle];
      const auto &v = triangles[a.triangle].geometry.vertices;
      const auto portal = translated(
          v[a.side], relativeTo(v[(a.side + 1) % 3], v[a.side]) * .5);
      double clearance = std::min(from.clearance, to.clearance);
      for (const auto edge : shared) {
        const auto &vertices = triangles[edge.triangle].geometry.vertices;
        for (std::size_t side = 0; side < 3; ++side) {
          if (side == edge.side)
            continue;
          const auto line =
              relativeTo(vertices[(side + 1) % 3], vertices[side]);
          clearance =
              std::min(clearance,
                       length(cross(line, relativeTo(portal, vertices[side]))) /
                           length(line));
        }
      }
      NavigationLink link;
      link.id = {tile, result.links.size() + 1};
      link.from = from.id;
      link.to = to.id;
      link.cost = (length(relativeTo(portal, from.position)) +
                   length(relativeTo(to.position, portal))) *
                  triangles[b.triangle].cost;
      link.clearance = clearance;
      link.height = std::min(from.height, to.height);
      link.slope = std::max(from.slope, to.slope);
      link.portal = portal;
      result.links.push_back(link);
    }
  }
  result.charge->setState(runtime::AllocationState::Owned);
  return result;
}
} // namespace playground::world
