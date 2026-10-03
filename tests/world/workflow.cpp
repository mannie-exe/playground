#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

#include <platform/CheckpointStore.hpp>
#include <scene/Controllers.hpp>
#include <support/Test.hpp>
#include <world/Lifecycle.hpp>
#include <world/Locomotion.hpp>
#include <world/Navigation.hpp>

using namespace playground;
using namespace playground::world;
using test::require;

namespace {
// Compiled authored fixture: streamed query/navigation data is independent of
// renderer visibility.
struct Cell final : CellProduct {
  CellId id;
  std::uint64_t revision;
  WorldBounds bounds;
  std::array<NavigationNode, 2> nodes;
  std::array<NavigationLink, 2> links;

  explicit Cell(const CellDefinition &definition)
      : id{definition.id}, revision{definition.revision},
        bounds{definition.bounds} {
    nodes[0] = {{id, 1}, {id.space, {bounds.minimum.x, 0, 0}}};
    nodes[1] = {{id, 2}, {id.space, {bounds.maximum.x, 0, 0}}};
    const auto cost = bounds.maximum.x - bounds.minimum.x;
    links[0] = {{id, 1}, nodes[0].id, nodes[1].id, cost};
    links[1] = {{id, 2}, nodes[1].id, nodes[0].id, cost};
  }

  Readiness readiness() const noexcept override {
    return Readiness::Data | Readiness::Query | Readiness::Navigation |
           Readiness::Simulation;
  }

  std::size_t bytes() const noexcept override { return sizeof(*this); }
};

struct Provider final : CellProvider {
  Readiness capabilities() const noexcept override {
    return Readiness::Data | Readiness::Query | Readiness::Navigation |
           Readiness::Simulation;
  }

  std::shared_ptr<const CellProduct>
  prepare(const CellDefinition &definition, Readiness, std::size_t budget,
          std::size_t, std::span<const std::shared_ptr<const CellProduct>>,
          std::stop_token stop) override {
    if (stop.stop_requested())
      throw std::runtime_error("Preparation cancelled");
    if (budget < sizeof(Cell))
      throw std::length_error("Authored cell exceeds admission");
    return std::make_shared<Cell>(definition);
  }
};
} // namespace

int main() {
  return test::run([] {
    const auto started = std::chrono::steady_clock::now();
    auto ledger = std::make_shared<runtime::ResourceLedger>();
    const auto initial = ledger->snapshot().memory[0].bytes;
    std::size_t ticks{}, zones{};
    double travel{};
    std::uint64_t published{};
    {
      const WorldId id{401};
      const SpaceId outside{id, 1}, inside{id, 2};
      const EntityId actor{id, 1};
      World world{id, ledger};
      const std::array<WorldMutation, 3> seed{
          CreateSpace{{outside, {}}}, CreateSpace{{inside, {}}},
          SpawnEntity{actor, {{{outside, {1e6, 0, 0}}, {}}, {outside}}}};
      world.apply(seed, world.snapshot().version(), 0);
      const CellId street{outside, 1}, room{inside, 1};
      WorldManifest manifest{
          id,
          1,
          "compiled-workflow-v1",
          {{street, {outside, {1e6, -2, -2}, {1e6 + 4, 2, 2}}, 1, "street"},
           {room, {inside, {-2, -2, -2}, {2, 2, 2}}, 1, "room"}},
          {{outside, {}}, {inside, {}}}};
      runtime::Executor executor;
      runtime::ServicePump pump;
      auto scope = pump.scope();
      auto provider = std::make_shared<Provider>();
      WorldStreamer stream{manifest,
                           world.snapshot().epoch(),
                           provider,
                           executor,
                           ledger,
                           {.productBytes = 4096,
                            .scratchBytes = 4096,
                            .minimumRetentionSeconds = 0},
                           scope.wakeCallback()};
      const auto streamingService = stream.attach(scope);
      auto now = runtime::ActivityClock::now();
      const auto wait = [&](auto done) {
        const auto deadline =
            runtime::ActivityClock::now() + std::chrono::seconds{5};
        while (!done()) {
          now = runtime::ActivityClock::now();
          pump.advance(now);
          require(pump.takeFailures().empty(), "world services remain healthy");
          if (now > deadline)
            throw std::runtime_error("World service publication timed out");
          std::this_thread::yield();
        }
      };
      const std::array sources{
          StreamingSource{{outside, {1e6, 0, 0}},
                          {},
                          1,
                          3,
                          0,
                          0,
                          Readiness::Navigation | Readiness::Query}};
      stream.setSources(1, sources);
      wait([&] { return stream.state(street).status == CellStatus::Ready; });
      auto streetLease = stream.lease(street, Readiness::Navigation, 1);
      const auto &cell = dynamic_cast<const Cell &>(streetLease.product());
      NavigationTile tile;
      tile.id = cell.id;
      tile.revision = cell.revision;
      tile.lease = streetLease;
      tile.nodes.assign(cell.nodes.begin(), cell.nodes.end());
      tile.links.assign(cell.links.begin(), cell.links.end());
      NavigationSnapshot navigation{world.snapshot(), {&tile, 1}, ledger};
      NavigationService planner{navigation, executor, ledger};
      const auto navigationService = planner.attach(scope);
      const auto job = planner.request({world.handle(actor),
                                        1,
                                        {outside, {1e6, 0, 0}},
                                        {outside, {1e6 + 4, 0, 0}}});
      wait([&] {
        return planner.poll(job).status != NavigationStatus::Planning;
      });
      const auto planned = planner.poll(job);
      require(planner.stats().completed == 1 && planner.stats().expansions > 0,
              "navigation diagnostics expose completed planning work");
      require(planned.status == NavigationStatus::Complete,
              "streamed tile produces complete route");
      PathFollower follower;
      follower.follow(planned.path);
      CharacterFacingController facing{LocomotionProps::responsive()};
      ActivationScheduler activation{world.snapshot(), ledger};
      activation.advance(world.snapshot(), 0);
      const ZoneDefinition zone{{id, 1},
                                {outside, {1e6 + 2, -1, -1}, {1e6 + 5, 1, 1}}};
      ZoneTracker tracker{world.snapshot(), {&zone, 1}, ledger};
      scene::PoseHistory history{world.snapshot().sample(world.handle(actor))};
      scene::FollowCameraRig camera;
      camera.reset(scene::cameraTarget(history.current()), {});
      for (std::uint64_t tick = 1; tick <= 240; ++tick) {
        const auto actual = world.snapshot().sample(world.handle(actor));
        const auto intent = follower.advance(actual, 1. / 60, navigation);
        if (intent.state == NavigationState::Arrived)
          break;
        require(intent.state == NavigationState::Following,
                "navigation progresses without synthetic arrival");
        const auto locomotion =
            travelIntent(actual, intent.desired.linear, intent.facingRadians,
                         facing.props(), 1. / 60);
        const auto result = realizeMovement(
            actual, facing.advance(actual, locomotion, 1. / 60), 1. / 60);
        auto props = world.snapshot().resolve(actual.entity).props;
        props.pose = result.actual.pose;
        props.velocity = result.actual.velocity;
        const std::array<WorldMutation, 1> update{SetEntity{actor, props}};
        world.apply(update, world.snapshot().version(), tick);
        require(activation.advance(world.snapshot(), tick / 60.).steps.size() ==
                    1,
                "full activation admits each simulation step");
        history.publish(world.snapshot().sample(actual.entity));
        camera.advance(scene::cameraTarget(history.sample(.5)), {}, {},
                       1. / 60);
        require(camera.camera().epoch == actual.entity.epoch,
                "camera follows the published world incarnation");
        zones += tracker.updateBounds(world.snapshot()).values.size();
        ++ticks;
      }
      travel =
          world.snapshot().sample(world.handle(actor)).pose.position.meters.x -
          1e6;
      require(travel > 3.8 && zones == 1,
              "motor reaches destination and emits one zone entry");
      WorldTransfer transfer{world, &stream, ledger};
      TransferRequest request;
      request.authorized = true;
      request.entities = {{world.handle(actor), {{inside, {}}, {}}}};
      request.readiness = {{room, world.snapshot().epoch(), 1, 0,
                            Readiness::Simulation | Readiness::Query}};
      const auto ticket = transfer.prepare(request);
      wait([&] {
        transfer.advance(now);
        return transfer.state(ticket).status == TransferStatus::Ready;
      });
      const auto oldHandle = world.handle(actor);
      transfer.commit(ticket, world.snapshot().tick() + 1, now);
      auto actual = world.snapshot().sample(oldHandle);
      history.publish(actual);
      camera.advance(scene::cameraTarget(history.current()), {}, {}, 1. / 60);
      require(camera.camera().pose.position.space == inside &&
                  !camera.state().transitioning,
              "transfer cuts camera and interpolation history");
      require(tracker.updateBounds(world.snapshot()).values.size() == 1,
              "transfer emits zone exit");
      ReferenceFrames frames{world.snapshot(), ledger};
      const auto directory =
          std::filesystem::temp_directory_path() /
          ("playground-world-workflow-" +
           std::to_string(started.time_since_epoch().count()));
      std::filesystem::create_directory(directory);
      struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() {
          std::error_code error;
          std::filesystem::remove_all(path, error);
        }
      } cleanup{directory};
      auto disk = std::make_shared<platform::DirectoryCheckpointStore>(
          directory, 1024 * 1024);
      WorldStore store{disk, ledger, {.maxCheckpointBytes = 1024 * 1024}};
      store.save(
          world.snapshot(), frames.snapshot(),
          {manifest.contentLock, {{"unloaded/street", 1, {std::byte{1}}}}}, 0);
      auto restored = store.load(id);
      require(restored &&
                  restored->world->snapshot().epoch() != oldHandle.epoch &&
                  restored->metadata.contentLock == manifest.contentLock,
              "checkpoint restores fresh incarnation with exact content lock");
      test::rejects([&] { restored->world->snapshot().resolve(oldHandle); },
                    "old handle cannot cross restoration");
      stream.setSources(1, {});
      stream.advance(now + std::chrono::seconds{5});
      require(planned.path->valid(navigation),
              "retained navigation snapshot pins admitted topology across "
              "source removal");
      published = stream.stats().published;
      scope.close();
      executor.close();
      require(streamingService.status() == runtime::ServiceStatus::Closed &&
                  navigationService.status() == runtime::ServiceStatus::Closed,
              "world scope closes all workers and services");
    }
    require(ledger->snapshot().memory[0].bytes == initial &&
                ledger->snapshot().owners.empty(),
            "world teardown retires all shared accounting owners");
    std::cout << "WorldWorkflow ticks=" << ticks << " travel_m=" << travel
              << " zone_entries=" << zones << " cells=" << published
              << " cpu_peak_bytes=" << ledger->snapshot().memory[0].peak
              << " elapsed_ms="
              << std::chrono::duration<double, std::milli>(
                     std::chrono::steady_clock::now() - started)
                     .count()
              << '\n';
  });
}
