#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

#include <world/Queries.hpp>

namespace playground::world {
namespace {
Vec3d minimum(Vec3d a, Vec3d b) {
  return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)};
}

Vec3d maximum(Vec3d a, Vec3d b) {
  return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)};
}

WorldBounds bounds(const QueryPrimitive &primitive) {
  if (const auto *box = std::get_if<WorldBounds>(&primitive.geometry))
    return *box;
  const auto &v = std::get<WorldTriangle>(primitive.geometry).vertices;
  return {v[0].space, minimum(minimum(v[0].meters, v[1].meters), v[2].meters),
          maximum(maximum(v[0].meters, v[1].meters), v[2].meters)};
}

bool ordered(const QueryHit &a, const QueryHit &b) {
  if (a.distance != b.distance)
    return a.distance < b.distance;
  if (a.entity != b.entity)
    return a.entity < b.entity;
  return a.primitive < b.primitive;
}

std::optional<QueryHit> rayBounds(WorldRay ray, double limit, WorldBounds box) {
  const auto lo = relativeTo({box.space, box.minimum}, ray.origin);
  const auto hi = relativeTo({box.space, box.maximum}, ray.origin);
  const std::array<double, 3> lows{lo.x, lo.y, lo.z}, highs{hi.x, hi.y, hi.z},
      dirs{ray.direction.x, ray.direction.y, ray.direction.z};
  double first{}, last{limit};
  std::optional<Vec3d> normal;
  for (std::size_t i = 0; i < 3; ++i) {
    if (dirs[i] == 0) {
      if (lows[i] > 0 || highs[i] < 0)
        return {};
      continue;
    }
    auto a = lows[i] / dirs[i], b = highs[i] / dirs[i];
    if (a > b)
      std::swap(a, b);
    if (a > first) {
      first = a;
      Vec3d n;
      const auto sign = dirs[i] > 0 ? -1.0 : 1.0;
      if (i == 0)
        n.x = sign;
      else if (i == 1)
        n.y = sign;
      else
        n.z = sign;
      normal = n;
    }
    last = std::min(last, b);
    if (first > last)
      return {};
  }
  return QueryHit{{},
                  0,
                  first,
                  {ray.origin.space, ray.origin.meters + ray.direction * first},
                  normal};
}

std::optional<QueryHit> rayTriangle(WorldRay ray, double limit,
                                    const WorldTriangle &triangle) {
  const auto a = relativeTo(triangle.vertices[0], ray.origin);
  const auto b = relativeTo(triangle.vertices[1], ray.origin);
  const auto c = relativeTo(triangle.vertices[2], ray.origin);
  const auto ab = b - a, ac = c - a;
  const auto n = normalized(cross(ab, ac));
  const auto denominator = dot(n, ray.direction);
  if (std::abs(denominator) <= 1e-12)
    return {};
  const auto distance = dot(n, a) / denominator;
  if (distance < 0 || distance > limit)
    return {};
  const auto p = ray.direction * distance;
  // Consistent winding test, in ray-relative double coordinates.
  if (dot(cross(ab, p - a), n) < 0 || dot(cross(c - b, p - b), n) < 0 ||
      dot(cross(a - c, p - c), n) < 0)
    return {};
  return QueryHit{
      {}, 0, distance, {ray.origin.space, ray.origin.meters + p}, n};
}

void incomplete(QueryResult &result, QueryDiagnostic reason) {
  result.status = QueryStatus::Incomplete;
  if (result.diagnostic == QueryDiagnostic::None ||
      result.diagnostic == QueryDiagnostic::MissingCoverage)
    result.diagnostic = reason;
}
} // namespace

void WorldBounds::validate(const SpatialLimits &limits) const {
  limits.validate({space, minimum});
  limits.validate({space, maximum});
  if (minimum.x > maximum.x || minimum.y > maximum.y || minimum.z > maximum.z)
    throw std::invalid_argument("Inverted world bounds");
}

bool WorldBounds::contains(WorldPosition p) const {
  return space == p.space && p.meters.x >= minimum.x &&
         p.meters.x <= maximum.x && p.meters.y >= minimum.y &&
         p.meters.y <= maximum.y && p.meters.z >= minimum.z &&
         p.meters.z <= maximum.z;
}

bool WorldBounds::contains(const WorldBounds &other) const {
  return contains(WorldPosition{other.space, other.minimum}) &&
         contains(WorldPosition{other.space, other.maximum});
}

bool WorldBounds::intersects(const WorldBounds &other) const {
  return space == other.space && minimum.x <= other.maximum.x &&
         maximum.x >= other.minimum.x && minimum.y <= other.maximum.y &&
         maximum.y >= other.minimum.y && minimum.z <= other.maximum.z &&
         maximum.z >= other.minimum.z;
}

QueryResult &QueryResult::operator=(QueryResult &&other) noexcept {
  if (this != &other) {
    QueryResult retired{std::move(*this)};
    charge = std::move(other.charge);
    worldVersion = other.worldVersion;
    tick = other.tick;
    indexRevision = other.indexRevision;
    status = other.status;
    precision = other.precision;
    diagnostic = other.diagnostic;
    work = other.work;
    candidates = other.candidates;
    hits = std::move(other.hits);
  }
  return *this;
}

struct SpatialSnapshot::State {
  rendering::ResourceLedger::Token charge;
  WorldSnapshot world;
  SpatialCoverage coverage;
  SpatialSnapshotProps props;
  std::shared_ptr<rendering::ResourceLedger> ledger;
  std::uint64_t revision{};
  std::vector<QueryPrimitive> primitives;
  std::vector<WorldBounds> boxes;

  State(WorldSnapshot source, SpatialCoverage area, SpatialSnapshotProps limits,
        std::shared_ptr<rendering::ResourceLedger> accounting,
        std::uint64_t rev)
      : world{std::move(source)}, coverage{area}, props{limits},
        ledger{std::move(accounting)}, revision{rev} {}

  QueryResult result() const {
    QueryResult result;
    result.worldVersion = world.version();
    result.tick = world.tick();
    result.indexRevision = revision;
    return result;
  }

  bool valid(QueryFilter filter, QueryBudget budget) const {
    if (!budget.work || !budget.candidates || !budget.hits ||
        budget.hits > props.maxOutputHits ||
        filter.exclude.size() > props.maxExclusions || !filter.purpose ||
        (filter.purpose & (filter.purpose - 1)))
      return false;
    return std::all_of(
        filter.exclude.begin(), filter.exclude.end(),
        [&](auto id) { return id.world == world.id() && id.value; });
  }

  template <class Candidate, class Hit>
  QueryResult query(QueryFilter filter, QueryBudget budget, bool covered,
                    QueryPrecision precision, Candidate candidate,
                    Hit hit) const {
    auto output = result();
    output.precision = precision;
    if (!valid(filter, budget))
      return output;
    output.status = coverage.status == QueryStatus::Unavailable
                        ? QueryStatus::Unavailable
                    : covered && coverage.status == QueryStatus::Complete
                        ? QueryStatus::Complete
                        : QueryStatus::Incomplete;
    output.diagnostic = output.status == QueryStatus::Complete
                            ? QueryDiagnostic::None
                            : QueryDiagnostic::MissingCoverage;
    if (output.status == QueryStatus::Unavailable)
      return output;
    const auto capacity = std::min(budget.hits, primitives.size());
    output.charge = ledger->reserve(
        rendering::MemoryClass::CPU, rendering::ResourceKind::Preparation,
        sizeof(QueryResult) + capacity * sizeof(QueryHit),
        "Spatial query output");
    output.hits.reserve(capacity);
    for (std::size_t i = 0; i < primitives.size(); ++i) {
      if (output.work == budget.work) {
        incomplete(output, QueryDiagnostic::WorkLimit);
        break;
      }
      ++output.work;
      const auto &p = primitives[i];
      if (!(p.categories & filter.categories) ||
          !(p.purposes & filter.purpose) ||
          std::find(filter.exclude.begin(), filter.exclude.end(), p.entity) !=
              filter.exclude.end() ||
          !candidate(boxes[i]))
        continue;
      if (output.candidates == budget.candidates) {
        incomplete(output, QueryDiagnostic::CandidateLimit);
        break;
      }
      ++output.candidates;
      auto value = hit(p);
      if (!value)
        continue;
      value->entity = p.entity;
      value->primitive = p.primitive;
      if (output.hits.size() < capacity)
        output.hits.push_back(*value);
      else {
        incomplete(output, QueryDiagnostic::HitLimit);
        const auto farthest =
            std::max_element(output.hits.begin(), output.hits.end(), ordered);
        if (farthest != output.hits.end() && ordered(*value, *farthest))
          *farthest = *value;
      }
    }
    std::sort(output.hits.begin(), output.hits.end(), ordered);
    output.charge->setState(rendering::AllocationState::Owned);
    return output;
  }
};

SpatialSnapshot::SpatialSnapshot(
    WorldSnapshot world, SpatialCoverage coverage, std::uint64_t revision,
    std::span<const QueryPrimitive> primitives,
    std::shared_ptr<rendering::ResourceLedger> ledger,
    SpatialSnapshotProps props) {
  if (!ledger || !revision || !props.maxPrimitives || !props.maxOutputHits ||
      !props.maxExclusions)
    throw std::invalid_argument(
        "Spatial snapshot requires accounting, revision and limits");
  if (props.maxOutputHits >
          (std::numeric_limits<std::size_t>::max() - sizeof(QueryResult)) /
              sizeof(QueryHit) ||
      primitives.size() > props.maxPrimitives ||
      primitives.size() >
          (std::numeric_limits<std::size_t>::max() - sizeof(State)) /
              (sizeof(QueryPrimitive) + sizeof(WorldBounds)))
    throw std::length_error("Spatial snapshot exceeds capacity");
  const auto *space = world.space(coverage.bounds.space);
  if (!space)
    throw std::invalid_argument("Spatial coverage requires a defined space");
  coverage.bounds.validate(space->limits);
  if (coverage.status != QueryStatus::Complete &&
      coverage.status != QueryStatus::Incomplete &&
      coverage.status != QueryStatus::Unavailable)
    throw std::invalid_argument("Invalid spatial coverage status");
  auto charge = ledger->reserve(
      rendering::MemoryClass::CPU, rendering::ResourceKind::Asset,
      sizeof(State) +
          primitives.size() * (sizeof(QueryPrimitive) + sizeof(WorldBounds)),
      "Spatial snapshot");
  auto state = std::make_shared<State>(std::move(world), coverage, props,
                                       std::move(ledger), revision);
  state->charge = std::move(charge);
  state->primitives.assign(primitives.begin(), primitives.end());
  std::sort(state->primitives.begin(), state->primitives.end(),
            [](const auto &a, const auto &b) {
              return a.entity != b.entity ? a.entity < b.entity
                                          : a.primitive < b.primitive;
            });
  state->boxes.reserve(primitives.size());
  for (std::size_t i = 0; i < state->primitives.size(); ++i) {
    const auto &p = state->primitives[i];
    const auto *entity = state->world.find(p.entity);
    if (!entity || entity->destroyed ||
        entity->props.pose.position.space != coverage.bounds.space ||
        !p.categories || !p.purposes)
      throw std::invalid_argument(
          "Query primitive requires a live same-space entity and membership");
    if (i && state->primitives[i - 1].entity == p.entity &&
        state->primitives[i - 1].primitive == p.primitive)
      throw std::invalid_argument("Duplicate query primitive identity");
    if (const auto *triangle = std::get_if<WorldTriangle>(&p.geometry)) {
      for (const auto &vertex : triangle->vertices) {
        space->limits.validate(vertex);
        if (vertex.space != coverage.bounds.space)
          throw std::invalid_argument("Triangle spans different spaces");
      }
      normalized(
          cross(relativeTo(triangle->vertices[1], triangle->vertices[0]),
                relativeTo(triangle->vertices[2], triangle->vertices[0])));
    }
    auto box = bounds(p);
    box.validate(space->limits);
    if (box.space != coverage.bounds.space)
      throw std::invalid_argument("Query bounds in a different space");
    state->boxes.push_back(box);
  }
  state->charge->setState(rendering::AllocationState::Owned);
  _state = std::move(state);
}

const WorldSnapshot &SpatialSnapshot::world() const { return _state->world; }

SpatialCoverage SpatialSnapshot::coverage() const { return _state->coverage; }

std::uint64_t SpatialSnapshot::revision() const { return _state->revision; }

QueryResult SpatialQueries::raycast(const SpatialSnapshot &snapshot,
                                    WorldRay ray, double distance,
                                    QueryFilter filter, QueryBudget budget) {
  const auto &state = *snapshot._state;
  WorldPosition end;
  try {
    if (ray.origin.space != state.coverage.bounds.space ||
        !std::isfinite(distance) || distance < 0)
      return state.result();
    const auto &limits = state.world.space(ray.origin.space)->limits;
    limits.validate(ray.origin);
    ray.direction = normalized(ray.direction);
    end = translated(ray.origin, ray.direction * distance, limits);
  } catch (const std::invalid_argument &) {
    return state.result();
  } catch (const std::out_of_range &) {
    return state.result();
  } catch (const std::overflow_error &) {
    return state.result();
  }
  return state.query(
      filter, budget,
      state.coverage.bounds.contains(ray.origin) &&
          state.coverage.bounds.contains(end),
      QueryPrecision::Mixed,
      [&](WorldBounds box) {
        return rayBounds(ray, distance, box).has_value();
      },
      [&](const QueryPrimitive &p) {
        if (const auto *box = std::get_if<WorldBounds>(&p.geometry))
          return rayBounds(ray, distance, *box);
        return rayTriangle(ray, distance, std::get<WorldTriangle>(p.geometry));
      });
}

QueryResult SpatialQueries::overlap(const SpatialSnapshot &snapshot,
                                    WorldBounds box, QueryFilter filter,
                                    QueryBudget budget) {
  const auto &state = *snapshot._state;
  try {
    if (box.space != state.coverage.bounds.space)
      return state.result();
    box.validate(state.world.space(box.space)->limits);
  } catch (const std::invalid_argument &) {
    return state.result();
  } catch (const std::out_of_range &) {
    return state.result();
  }
  return state.query(
      filter, budget, state.coverage.bounds.contains(box),
      QueryPrecision::Bounds,
      [&](WorldBounds candidate) { return candidate.intersects(box); },
      [&](const QueryPrimitive &p) -> std::optional<QueryHit> {
        const auto candidate = bounds(p);
        return QueryHit{
            {}, 0, 0, {box.space, maximum(candidate.minimum, box.minimum)}, {}};
      });
}

QueryResult SpatialQueries::sweep(const SpatialSnapshot &snapshot,
                                  WorldBounds box, Vec3d displacement,
                                  QueryFilter filter, QueryBudget budget) {
  const auto &state = *snapshot._state;
  auto result = state.result();
  try {
    if (box.space != state.coverage.bounds.space ||
        !state.valid(filter, budget) || !isFinite(displacement))
      return result;
    const auto &limits = state.world.space(box.space)->limits;
    box.validate(limits);
    translated({box.space, box.minimum}, displacement, limits);
    translated({box.space, box.maximum}, displacement, limits);
  } catch (const std::invalid_argument &) {
    return result;
  } catch (const std::out_of_range &) {
    return result;
  } catch (const std::overflow_error &) {
    return result;
  }
  result.status = QueryStatus::Unsupported;
  result.diagnostic = QueryDiagnostic::UnsupportedShape;
  return result;
}

} // namespace playground::world
