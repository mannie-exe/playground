#include <array>
#include <cmath>
#include <limits>
#include <memory>

#include <support/Test.hpp>
#include <world/Queries.hpp>

using namespace playground;
using namespace playground::world;
using test::require;

int main() {
  return test::run([] {
    const WorldId id{1};
    const SpaceId space{id, 1}, foreign{id, 2};
    const EntityId a{id, 1}, b{id, 2};
    auto ledger = std::make_shared<runtime::ResourceLedger>();
    World world{id, ledger};
    const std::array<WorldMutation, 3> seed{
        CreateSpace{{space, {}}},
        SpawnEntity{a, {{{space, {1e6, 0, 0}}, {}}, {space}}},
        SpawnEntity{b, {{{space, {1e6, 0, 0}}, {}}, {space}}}};
    world.apply(seed, world.snapshot().version(), 8);
    const auto p = [&](double x, double y, double z) {
      return WorldPosition{space, {1e6 + x, y, z}};
    };
    const auto box = [&](double lo, double hi) {
      return WorldBounds{space, {1e6 + lo, -1, -1}, {1e6 + hi, 1, 1}};
    };
    const SpatialCoverage coverage{box(-10, 20)};
    const std::array<QueryPrimitive, 3> primitives{
        {{b, 0, 1, 1, box(5, 6)},
         {a, 1, 1, 1, WorldTriangle{{p(2, -1, -1), p(2, 1, -1), p(2, 0, 1)}}},
         {a, 0, 2, 2, box(2, 3)}}};
    SpatialSnapshot index{world.snapshot(), coverage, 9, primitives, ledger};
    const WorldRay ray{p(0, 0, 0), {2, 0, 0}};
    auto hits = SpatialQueries::raycast(index, ray, 10);
    require(hits.status == QueryStatus::Complete && hits.hits.size() == 2 &&
                hits.hits[0].entity == a && hits.hits[0].distance == 2 &&
                hits.hits[1].entity == b && hits.hits[1].distance == 5,
            "ray direction normalizes and hits sort by distance");
    require(hits.worldVersion == world.snapshot().version() && hits.tick == 8 &&
                hits.indexRevision == 9 && hits.hits[0].normal &&
                hits.hits[0].position == p(2, 0, 0),
            "hit retains source revisions and precise position/normal");
    auto bounds = SpatialQueries::raycast(index, ray, 10,
                                          {.categories = 2, .purpose = 2});
    require(bounds.hits.size() == 1 && bounds.hits[0].primitive == 0 &&
                bounds.hits[0].normal == Vec3d{-1, 0, 0},
            "membership is authored by purpose and category");
    const std::array<EntityId, 1> exclude{a};
    auto filtered =
        SpatialQueries::raycast(index, ray, 10, {.exclude = exclude});
    require(filtered.hits.size() == 1 && filtered.hits[0].entity == b,
            "entity exclusions apply to all its primitives");
    auto miss = SpatialQueries::raycast(index, {p(0, .9, .9), {1, 0, 0}}, 3);
    require(miss.status == QueryStatus::Complete && miss.hits.empty(),
            "triangle broad hit is not an exact hit");
    auto far = SpatialQueries::raycast(index, ray, 21);
    require(far.status == QueryStatus::Incomplete &&
                far.diagnostic == QueryDiagnostic::MissingCoverage,
            "out-of-coverage ray cannot assert complete nearest hits");
    auto limited = SpatialQueries::raycast(index, ray, 10, {}, {.hits = 1});
    require(limited.status == QueryStatus::Incomplete &&
                limited.diagnostic == QueryDiagnostic::HitLimit &&
                limited.hits.size() == 1 && limited.hits[0].distance == 2,
            "hit limit retains nearest available hit without claiming "
            "completeness");
    auto work = SpatialQueries::raycast(index, ray, 10, {}, {.work = 1});
    require(work.status == QueryStatus::Incomplete && work.work == 1 &&
                work.diagnostic == QueryDiagnostic::WorkLimit,
            "work includes filtered primitive visits");
    auto candidates =
        SpatialQueries::raycast(index, ray, 10, {}, {.candidates = 1});
    require(candidates.status == QueryStatus::Incomplete &&
                candidates.candidates == 1 &&
                candidates.diagnostic == QueryDiagnostic::CandidateLimit,
            "candidate exhaustion remains explicit");
    auto overlaps = SpatialQueries::overlap(index, box(2, 2));
    require(
        overlaps.status == QueryStatus::Complete &&
            overlaps.precision == QueryPrecision::Bounds &&
            overlaps.hits.size() == 1,
        "closed overlap includes boundary and declares conservative precision");
    auto inside = SpatialQueries::raycast(index, {p(5.5, 0, 0), {1, 0, 0}}, .1);
    require(inside.hits.size() == 1 && inside.hits[0].distance == 0 &&
                !inside.hits[0].normal,
            "starting inside bounds gives zero distance without invented "
            "surface normal");
    auto unsupported = SpatialQueries::sweep(index, box(0, 1), {2, 0, 0});
    require(unsupported.status == QueryStatus::Unsupported,
            "sweep never disguises a raycast as shape collision");
    auto bad = SpatialQueries::raycast(index, {{foreign, {}}, {1, 0, 0}}, 1);
    require(bad.status == QueryStatus::Invalid, "foreign-space ray invalid");
    require(SpatialQueries::raycast(index, {p(0, 0, 0), {}}, 1).status ==
                QueryStatus::Invalid,
            "zero direction invalid");
    require(SpatialQueries::raycast(index, ray,
                                    std::numeric_limits<double>::infinity())
                    .status == QueryStatus::Invalid,
            "infinite query invalid");
    require(SpatialQueries::raycast(index, ray, 1e10).status ==
                QueryStatus::Invalid,
            "out-of-world query invalid");
    require(SpatialQueries::raycast(index, ray, 1, {.purpose = 3}).status ==
                QueryStatus::Invalid,
            "purpose must select one declared bit");
    require(SpatialQueries::overlap(index, box(2, 1)).status ==
                QueryStatus::Invalid,
            "inverted bounds invalid");
    SpatialSnapshot absent{world.snapshot(),
                           {coverage.bounds, QueryStatus::Unavailable},
                           1,
                           {},
                           ledger};
    require(SpatialQueries::raycast(absent, ray, 1).status ==
                QueryStatus::Unavailable,
            "unavailable index is not empty geometry");
    SpatialSnapshot partial{world.snapshot(),
                            {coverage.bounds, QueryStatus::Incomplete},
                            2,
                            {},
                            ledger};
    require(SpatialQueries::overlap(partial, box(0, 1)).status ==
                QueryStatus::Incomplete,
            "partial empty coverage does not prove no overlap");
    const std::array<QueryPrimitive, 2> ties{
        {{b, 2, 1, 1, box(2, 3)}, {a, 3, 1, 1, box(2, 3)}}};
    SpatialSnapshot tie{world.snapshot(), coverage, 3, ties, ledger};
    const auto equal = SpatialQueries::raycast(tie, ray, 3);
    require(equal.hits[0].entity == a && equal.hits[1].entity == b,
            "equal hits use stable identity order");
    auto duplicate = primitives;
    duplicate[1] = duplicate[0];
    test::rejects(
        [&] {
          SpatialSnapshot badIndex{world.snapshot(), coverage, 1, duplicate,
                                   ledger};
        },
        "duplicate primitive identity rejected");
    duplicate = primitives;
    duplicate[1].geometry = WorldTriangle{{p(0, 0, 0), p(0, 0, 0), p(0, 0, 0)}};
    test::rejects(
        [&] {
          SpatialSnapshot badIndex{world.snapshot(), coverage, 1, duplicate,
                                   ledger};
        },
        "degenerate triangles rejected at publication");
    test::rejects<std::length_error>(
        [&] {
          SpatialSnapshot badIndex{
              world.snapshot(), coverage, 1,
              primitives,       ledger,   {.maxPrimitives = 2}};
        },
        "index admission bounded");
    const std::array<WorldMutation, 1> remove{DestroyEntity{a}};
    world.apply(remove, world.snapshot().version(), 9);
    require(SpatialQueries::raycast(index, ray, 3).hits.size() == 1,
            "retained query snapshot survives live entity destruction");
    test::rejects(
        [&] {
          SpatialSnapshot badIndex{world.snapshot(), coverage, 10, primitives,
                                   ledger};
        },
        "new snapshot cannot bind destroyed entity");
    auto budgets = ledger->snapshot().budgets;
    budgets.cpuBytes = ledger->snapshot().memory[0].bytes;
    ledger->setBudgets(budgets);
    test::rejects<runtime::ResourcePressure>(
        [&] { SpatialQueries::raycast(index, ray, 3); },
        "query output admitted through shared ledger");
  });
}
