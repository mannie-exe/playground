#include <array>
#include <memory>
#include <stdexcept>
#include <vector>

#include <support/Test.hpp>
#include <world/World.hpp>

using namespace playground;
using namespace playground::world;
using test::require;

int main() {
  return test::run([] {
    const WorldId id{17};
    const SpaceId a{id, 1}, b{id, 2};
    const EntityId actor{id, 1}, other{id, 2};
    auto ledger = std::make_shared<rendering::ResourceLedger>();
    World world(id, ledger);
    auto props = [&](SpaceId space, double x) {
      return EntityProps{
          {{space, {x, 0, 0}}, {}}, {space}, Activation::Full, {std::byte{42}}};
    };
    auto apply = [&](std::initializer_list<WorldMutation> mutations,
                     std::uint64_t tick = 1) {
      return world.apply({mutations.begin(), mutations.size()},
                         world.snapshot().version(), tick);
    };
    require(world.snapshot().revision() == 0 &&
                world.snapshot().entities().empty(),
            "new world has a valid empty immutable snapshot");
    const auto empty = world.snapshot();
    const std::array<WorldMutation, 3> create{CreateSpace{{a, {}}},
                                              SpawnEntity{actor, props(a, 1e6)},
                                              CreateSpace{{b, {}}}};
    require(world.apply(create, empty.version(), 1) == 1,
            "batch publishes once after space and entity validation");
    const auto old = world.snapshot();
    const auto handle = world.handle(actor);
    require(old.resolve(handle).props.pose.position.meters.x == 1e6 &&
                old.tick() == 1 && empty.entities().empty(),
            "retained snapshot remains unchanged after publication");
    require(ledger->snapshot().memory[0].bytes >=
                empty.reservedBytes() + old.reservedBytes(),
            "old and current states remain charged while retained");
    apply({SetEntity{actor, props(a, 1e6 + .02)}});
    require(old.resolve(handle).props.pose.position.meters.x == 1e6 &&
                world.snapshot().resolve(handle).props.pose.position.meters.x ==
                    1e6 + .02,
            "live mutations do not alter immutable readers");
    const auto stable = world.snapshot();
    test::rejects([&] { world.apply(create, empty.version(), 2); },
                  "stale revision refused");
    test::rejects([&] { apply({SetEntity{actor, props(b, 3)}}); },
                  "cross-space change requires discontinuity");
    require(world.snapshot().version() == stable.version(),
            "failed batch preserves revision");
    test::rejects(
        [&] {
          apply({SpawnEntity{other, props(a, 1)},
                 SpawnEntity{actor, props(a, 2)}});
        },
        "later error rolls back earlier commands");
    require(!world.snapshot().find(other),
            "failed candidate never leaks partial spawn");
    test::rejects([&] { apply({SpawnEntity{{{88}, 1}, props(a, 2)}}); },
                  "foreign entity rejected");
    test::rejects([&] { apply({CreateSpace{{a, {}}}}); },
                  "duplicate space rejected");
    test::rejects([&] { apply({SpawnEntity{other, props({id, 99}, 2)}}); },
                  "undefined space rejected");
    auto mismatch = props(a, 1);
    mismatch.velocity.space = b;
    test::rejects([&] { apply({SetEntity{actor, mismatch}}); },
                  "mismatched velocity space rejected");
    mismatch = props(a, 1);
    mismatch.activation = static_cast<Activation>(99);
    test::rejects([&] { apply({SetEntity{actor, mismatch}}); },
                  "invalid activation rejected");
    test::rejects([&] { apply({}, 0); }, "backward tick rejected");
    apply({SetEntity{actor, props(b, 3), true}}, 2);
    require(world.snapshot().resolve(handle).discontinuity == 2,
            "explicit transfer preserves identity and advances discontinuity");
    const auto beforeDestroy = world.snapshot();
    apply({DestroyEntity{actor}}, 2);
    require(world.snapshot().find(actor)->destroyed &&
                world.snapshot().find(actor)->props.data.empty(),
            "destruction retains identity tombstone without domain payload");
    test::rejects([&] { world.snapshot().resolve(handle); },
                  "destroyed runtime handle rejected");
    test::rejects([&] { apply({SpawnEntity{actor, props(a, 0)}}, 2); },
                  "destroyed identity cannot be reused");
    require(!beforeDestroy.resolve(handle).destroyed,
            "old snapshot still describes its tick");
    world.restore(beforeDestroy, world.snapshot().version());
    require(world.snapshot().epoch() != beforeDestroy.epoch() &&
                world.snapshot().find(actor)->props.pose.position.space == b,
            "restoration preserves model identity with a fresh runtime epoch");
    test::rejects([&] { world.snapshot().resolve(handle); },
                  "pre-restore handle rejected");
    test::rejects([&] { world.apply({}, beforeDestroy.version(), 3); },
                  "old epoch rejected even with matching revision");
    require(world.snapshot().resolve(world.handle(actor)).id == actor,
            "fresh restored handle resolves");

    World small(id, ledger,
                {.maxEntities = 1, .maxCommands = 2, .maxEntityBytes = 1});
    const std::array<WorldMutation, 2> seed{CreateSpace{{a, {}}},
                                            SpawnEntity{actor, props(a, 0)}};
    small.apply(seed, small.snapshot().version(), 0);
    const auto smallBefore = small.snapshot();
    test::rejects<std::length_error>(
        [&] { small.apply(create, smallBefore.version(), 0); },
        "command bound enforced before work");
    const std::array<WorldMutation, 1> extra{SpawnEntity{other, props(a, 0)}};
    test::rejects<std::length_error>(
        [&] { small.apply(extra, smallBefore.version(), 0); },
        "entity bound enforced");
    auto large = props(a, 0);
    large.data.resize(2);
    const std::array<WorldMutation, 1> bytes{SetEntity{actor, large}};
    test::rejects<std::length_error>(
        [&] { small.apply(bytes, smallBefore.version(), 0); },
        "domain data bound enforced");
    require(small.snapshot().version() == smallBefore.version(),
            "admission failures leave state unchanged");
    auto budgets = ledger->snapshot().budgets;
    budgets.cpuBytes = ledger->snapshot().memory[0].bytes;
    ledger->setBudgets(budgets);
    test::rejects<rendering::ResourcePressure>(
        [&] { world.apply({}, world.snapshot().version(), 3); },
        "pinned snapshot pressure refuses publication");
    require(world.snapshot().tick() == 2,
            "failed admission does not advance simulation");
    test::rejects([&] { World invalid({}, ledger); },
                  "world identity required");
    test::rejects([&] { World invalid(id, {}); }, "world accounting required");
  });
}
