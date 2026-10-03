#include <array>
#include <cmath>
#include <memory>
#include <numbers>

#include <support/Test.hpp>
#include <world/Frames.hpp>

using namespace playground;
using namespace playground::world;
using test::require;

int main() {
  return test::run([] {
    auto ledger = std::make_shared<runtime::ResourceLedger>();
    World world{{7}, ledger};
    const SpaceId space{{7}, 1}, other{{7}, 2};
    const std::array<WorldMutation, 2> spaces{CreateSpace{{space, {}}},
                                              CreateSpace{{other, {}}}};
    world.apply(spaces, world.snapshot().version(), 3);
    const FrameId root{space, world.snapshot().epoch(), 1},
        child{space, root.epoch, 2}, grandchild{space, root.epoch, 3};
    ReferenceFrames frames{world.snapshot(), ledger};
    const ReferenceFrame rootProps{{}, {{1e6, 0, 0}, {}}, {2, 0, 0}, {0, 0, 1}};
    const ReferenceFrame childProps{
        root, {{2, 0, 0}, {}}, {1, 0, 0}, {0, 1, 0}};
    auto apply = [&](std::initializer_list<FrameMutation> changes) {
      return frames.apply(world.snapshot(), {changes.begin(), changes.size()},
                          frames.snapshot().version());
    };
    apply({CreateFrame{child, childProps}, CreateFrame{root, rootProps}});
    const auto first = frames.snapshot();
    const auto &sample = first.resolve(child);
    require(sample.pose.position.meters == Vec3d{1e6 + 2, 0, 0} &&
                sample.velocity.linear == Vec3d{3, 2, 0} &&
                sample.velocity.angular == Vec3d{0, 1, 1},
            "hierarchy resolves same-tick position and angular-point velocity");
    require(sample.tick == 3 &&
                sample.worldVersion == world.snapshot().version() &&
                sample.discontinuity == 1,
            "sample identifies source epoch tick and initial discontinuity");
    const WorldPose subject{{space, {1e6 + 2, 1, 0}}, {}};
    const WorldVelocity motion{space, {6, 7, 8}, {.1, .2, .3}};
    const auto attached =
        attach(sample, subject, motion, FrameVelocityPolicy::PreserveWorld);
    const auto detached = resolve(attached, sample);
    require(length(relativeTo(detached.pose.position, subject.position)) <
                    1e-8 &&
                length(detached.velocity.linear - motion.linear) < 1e-8 &&
                length(detached.velocity.angular - motion.angular) < 1e-8,
            "preserve-world attachment roundtrips velocity including angular "
            "offset");
    const auto follows = resolve(
        attach(sample, subject, motion, FrameVelocityPolicy::FollowFrame),
        sample);
    require(follows.velocity.linear == Vec3d{2, 2, 0} &&
                follows.velocity.angular == Vec3d{0, 1, 1},
            "follow-frame policy inherits frame-point motion");
    require(worldPosition(FramePosition{child, {0, 1, 0}}, sample) ==
                subject.position,
            "frame position conversion uses matching sample");
    test::rejects([&] { worldPosition(FramePosition{root, {}}, sample); },
                  "frame mismatch rejected");
    apply({CreateFrame{grandchild, {child, {{0, 0, 3}, {}}}}});
    auto moved = rootProps;
    moved.local.offset.x += .02;
    world.apply({}, world.snapshot().version(), 4);
    apply({SetFrame{root, moved}});
    require(
        frames.snapshot().resolve(child).discontinuity == 1 &&
            frames.snapshot().resolve(child).tick == 4 &&
            frames.snapshot().resolve(child).pose.position.meters.x ==
                1e6 + 2.02,
        "ordinary movement preserves history and resolves precise new tick");
    require(first.resolve(child).pose.position.meters.x == 1e6 + 2,
            "retained frame snapshots remain immutable");
    moved.local.offset.x = -1e6;
    apply({SetFrame{root, moved, true}});
    require(frames.snapshot().resolve(grandchild).discontinuity == 2,
            "teleport propagates discontinuity through descendants");
    const auto before = frames.snapshot();
    auto cycle = moved;
    cycle.parent = grandchild;
    test::rejects([&] { apply({SetFrame{root, cycle, true}}); },
                  "cycles rejected atomically");
    test::rejects([&] { apply({RemoveFrame{root}}); },
                  "live dependents prevent parent removal");
    require(frames.snapshot().version() == before.version(),
            "failed hierarchy leaves publication unchanged");
    auto reparented = childProps;
    reparented.parent.reset();
    reparented.local = {before.resolve(child).pose.position.meters,
                        before.resolve(child).pose.orientation};
    reparented.linear = before.resolve(child).velocity.linear;
    reparented.angular = before.resolve(child).velocity.angular;
    test::rejects([&] { apply({SetFrame{child, reparented}}); },
                  "reparent requires explicit history reset");
    apply({RemoveFrame{root}, SetFrame{child, reparented, true}});
    require(frames.snapshot().resolve(child).pose == before.resolve(child).pose,
            "explicit detach and parent removal can commit together");
    test::rejects([&] { frames.snapshot().resolve(root); },
                  "removed frame handle rejected");
    test::rejects([&] { apply({CreateFrame{root, rootProps}}); },
                  "frame identity tombstone prevents reuse");
    test::rejects([&] { frames.apply(world.snapshot(), {}, before.version()); },
                  "stale frame revision rejected");
    const FrameId alien{other, root.epoch, 99};
    test::rejects([&] { apply({CreateFrame{alien, {child, {}}}}); },
                  "cross-space parent rejected");
    ReferenceFrames shallow{world.snapshot(), ledger, {.maxDepth = 1}};
    const std::array<FrameMutation, 2> deep{CreateFrame{root, rootProps},
                                            CreateFrame{child, childProps}};
    test::rejects<std::length_error>(
        [&] {
          shallow.apply(world.snapshot(), deep, shallow.snapshot().version());
        },
        "depth bound enforced");
    require(shallow.snapshot().records().empty(),
            "failed initial frame batch remains empty");
    auto rotated = sample;
    rotated.pose.orientation =
        math::axisAngle({0, 1, 0}, std::numbers::pi_v<float> / 2);
    const auto rotatedAttach =
        attach(rotated, subject, motion, FrameVelocityPolicy::PreserveWorld);
    const auto rotatedResult = resolve(rotatedAttach, rotated);
    require(length(relativeTo(rotatedResult.pose.position, subject.position)) <
                    1e-5 &&
                length(rotatedResult.velocity.linear - motion.linear) < 1e-5,
            "attachment conversion rotates parent-local velocity and position");
    // The adapter publishes realized motion through the existing world
    // boundary.
    const EntityId rider{world.snapshot().id(), 1};
    const auto realized = resolve(FrameAttachment{child, {{0, 1, 0}, {}}},
                                  frames.snapshot().resolve(child));
    const std::array<WorldMutation, 1> spawn{
        SpawnEntity{rider, {realized.pose, realized.velocity}}};
    world.apply(spawn, world.snapshot().version(), world.snapshot().tick());
    require(
        world.snapshot().resolve(world.handle(rider)).props.pose ==
            realized.pose,
        "resolved attachment integrates through transactional entity state");
    const auto oldWorld = world.snapshot();
    world.restore(oldWorld, oldWorld.version());
    test::rejects(
        [&] {
          frames.apply(world.snapshot(), {}, frames.snapshot().version());
        },
        "world restore rejects old frame registry");
    ReferenceFrames restored{world.snapshot(), ledger};
    test::rejects(
        [&] {
          restored.apply(world.snapshot(), deep, restored.snapshot().version());
        },
        "restoration requires rebound frame identities");
    auto budgets = ledger->snapshot().budgets;
    budgets.cpuBytes = ledger->snapshot().memory[0].bytes;
    ledger->setBudgets(budgets);
    test::rejects<runtime::ResourcePressure>(
        [&] {
          restored.apply(world.snapshot(), {}, restored.snapshot().version());
        },
        "frame generations admitted through ledger");
  });
}
