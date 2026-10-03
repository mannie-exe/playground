#include <array>
#include <cmath>

#include <support/Test.hpp>
#include <world/Lifecycle.hpp>

using namespace playground;
using namespace playground::world;
using test::require;

int main() {
  return test::run([] {
    auto ledger = std::make_shared<runtime::ResourceLedger>();
    World world{{81}, ledger};
    const SpaceId a{{81}, 1}, b{{81}, 2};
    const EntityId actor{{81}, 1}, other{{81}, 2};
    auto props = [&](double x) {
      return EntityProps{
          {{a, {x, 0, 0}}, {}}, {a, {2, 0, 0}}, Activation::Full};
    };
    auto apply = [&](std::initializer_list<WorldMutation> changes,
                     std::uint64_t tick = 1) {
      world.apply({changes.begin(), changes.size()}, world.snapshot().version(),
                  tick);
    };
    apply({CreateSpace{{a, {}}}, CreateSpace{{b, {}}},
           SpawnEntity{actor, props(0)}, SpawnEntity{other, props(10)}});
    ActivationScheduler scheduler{
        world.snapshot(), ledger, {.coarseSeconds = 1}, 8};
    require(scheduler.advance(world.snapshot(), 0).steps.empty(),
            "first sample establishes cadence");
    require(scheduler.advance(world.snapshot(), .1).steps.size() == 2,
            "full activation receives elapsed simulation time");
    world.setActivation(
        {world.handle(actor), Activation::Dormant, Activation::Dormant, true},
        world.snapshot().version(), 1);
    scheduler.advance(world.snapshot(), .2);
    scheduler.wake(world.handle(actor), 2);
    auto wake = scheduler.advance(world.snapshot(), 2);
    require(wake.steps.size() == 2 && wake.steps[0].wake &&
                wake.steps[0].seconds == 0,
            "dormant wake is explicit and carries no accumulated motion");
    test::rejects(
        [&] {
          world.setActivation({world.handle(actor), Activation::Dormant,
                               Activation::Full, true},
                              world.snapshot().version(), 1);
        },
        "authority refuses visibility-driven deactivation");
    world.setActivation(
        {world.handle(actor), Activation::Coarse, Activation::Dormant, true},
        world.snapshot().version(), 1);
    scheduler.advance(world.snapshot(), 3);
    require(scheduler.advance(world.snapshot(), 3.5).steps.size() == 1,
            "coarse entities wait for their domain interval");
    auto coarse = scheduler.advance(world.snapshot(), 4);
    require(coarse.steps[0].entity.id == actor && coarse.steps[0].seconds == 1,
            "mode changes discard dormant elapsed time");
    test::rejects([&] { scheduler.advance(world.snapshot(), 3); },
                  "clock reversal rejected");

    const ZoneId zone{{81}, 1};
    ZoneTracker zones{
        world.snapshot(),
        std::array{ZoneDefinition{zone, {a, {-1, -1, -1}, {1, 1, 1}}}}, ledger,
        8};
    auto initial = zones.updateBounds(world.snapshot());
    require(initial.values.size() == 1 && initial.values[0].entered &&
                initial.values[0].reason == ZoneEventReason::Initial,
            "initial membership is explicit");
    auto outside = props(4);
    apply({SetEntity{actor, outside, true}});
    auto incomplete = zones.update(
        world.snapshot(), std::array{ZoneObservation{actor, {}, false}});
    require(incomplete.values.empty(),
            "missing coverage cannot synthesize exit");
    auto exit = zones.updateBounds(world.snapshot());
    require(exit.values.size() == 1 && !exit.values[0].entered &&
                exit.values[0].reason == ZoneEventReason::Teleport,
            "complete observation confirms exit");
    apply({SetEntity{actor, props(1), true}});
    auto teleport = zones.updateBounds(world.snapshot());
    require(teleport.values.size() == 1 &&
                teleport.values[0].reason == ZoneEventReason::Teleport,
            "closed zone boundary and teleport reason retained");

    WorldTransfer transfers{world, nullptr, ledger};
    auto request = [&](EntityId id, double x) {
      return TransferRequest{
          {TransferEntity{world.handle(id), {{b, {x, 0, 0}}, {}}}}, {}, true};
    };
    auto ticket = transfers.prepare(request(actor, 20));
    const auto before = world.snapshot();
    require(transfers.state(ticket).status == TransferStatus::Preparing &&
                before.find(actor)->props.pose.position.space == a,
            "preparation does not move an entity");
    transfers.commit(ticket, 2, {});
    require(
        transfers.state(ticket).status == TransferStatus::Committed &&
            world.snapshot().find(actor)->props.pose.position.space == b &&
            world.snapshot().find(actor)->props.velocity.linear == Vec3d{} &&
            world.snapshot().find(actor)->discontinuity ==
                before.find(actor)->discontinuity + 1,
        "commit changes membership, resets velocity and marks discontinuity");
    transfers.forget(ticket);
    auto stale = transfers.prepare(request(other, 30));
    apply({SetEntity{other, props(11)}}, 2);
    test::rejects<std::logic_error>([&] { transfers.commit(stale, 3, {}); },
                                    "changed source invalidates ticket");
    require(world.snapshot().find(other)->props.pose.position.space == a,
            "failed transfer leaves source intact");
    auto physicalRequest = request(other, 0);
    physicalRequest.requirePhysicalPlacement = true;
    auto physical = transfers.prepare(physicalRequest);
    require(transfers.state(physical).status == TransferStatus::Unsupported,
            "missing physics is observable");
    auto expiredRequest = request(other, 0);
    expiredRequest.deadline = runtime::ActivityClock::time_point{};
    auto expired = transfers.prepare(expiredRequest);
    transfers.advance({});
    require(transfers.state(expired).status == TransferStatus::Failed,
            "expired destination preparation fails");
    auto cancelled = transfers.prepare(request(other, 0));
    transfers.cancel(cancelled);
    require(transfers.state(cancelled).status == TransferStatus::Cancelled,
            "cancellation outcome retained");
    auto unauthorized = request(other, 0);
    unauthorized.authorized = false;
    test::rejects([&] { transfers.prepare(unauthorized); },
                  "unapproved domain transfer rejected");
    auto group = request(actor, 40);
    group.entities.push_back({world.handle(other), {{b, {50, 0, 0}}, {}}});
    auto grouped = transfers.prepare(group);
    const auto stable = world.snapshot();
    auto original = ledger->snapshot().budgets;
    auto tight = original;
    tight.cpuBytes = ledger->snapshot().memory[0].bytes;
    ledger->setBudgets(tight);
    test::rejects<runtime::ResourcePressure>(
        [&] { transfers.commit(grouped, 3, {}); },
        "admission rejects whole transfer group");
    require(world.snapshot().version() == stable.version() &&
                world.snapshot().find(other)->props.pose.position.space == a,
            "failed group cannot partially move any entity");
    ledger->setBudgets(original);

    ReferenceFrames frames{world.snapshot(), ledger};
    const FrameId frame{b, world.snapshot().epoch(), 1};
    const std::array<FrameMutation, 1> create{
        CreateFrame{frame, {{}, {{10, 0, 0}, {}}}}};
    frames.apply(world.snapshot(), create, frames.snapshot().version());
    WorldTransfer attached{world, nullptr, ledger, 64, 256, &frames};
    auto board = request(other, 12);
    board.entities[0].destinationFrame = frames.snapshot().resolve(frame);
    auto boarding = attached.prepare(board);
    attached.commit(boarding, 2, {});
    const auto &attachment = *world.snapshot().find(other)->props.attachment;
    require(attachment.local.offset.x == 2 && attachment.frame == frame,
            "attachment and pose publish together");
    const std::array<FrameMutation, 1> remove{RemoveFrame{frame}};
    test::rejects(
        [&] {
          frames.apply(world.snapshot(), remove, frames.snapshot().version());
        },
        "attached frame cannot disappear without detach");
    auto unboard = transfers.prepare(request(other, 14));
    transfers.commit(unboard, 2, {});
    require(!world.snapshot().find(other)->props.attachment,
            "unframed transfer explicitly detaches");
    frames.apply(world.snapshot(), remove, frames.snapshot().version());
    apply({DestroyEntity{actor}}, 2);
    auto destroyed = zones.update(world.snapshot(), {});
    require(destroyed.values.size() == 1 &&
                destroyed.values[0].reason == ZoneEventReason::Destroyed,
            "destroyed identities confirm exits without geometric coverage");
    const auto saved = world.snapshot();
    world.restore(saved, saved.version());
    test::rejects([&] { scheduler.advance(world.snapshot(), 5); },
                  "restored epoch rejects old scheduling state");
    attached.close();
    transfers.close();
  });
}
