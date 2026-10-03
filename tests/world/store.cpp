#include <array>
#include <atomic>
#include <chrono>
#include <fstream>
#include <thread>

#include <platform/CheckpointStore.hpp>
#include <support/Test.hpp>

using namespace playground;
using namespace playground::world;
using test::require;

namespace {
struct FaultStorage : CheckpointStorage {
  std::optional<StoredCheckpoint> saved;
  bool fail{};

  std::optional<StoredCheckpoint> read(WorldId, std::size_t) override {
    return saved;
  }

  std::uint64_t publish(WorldId, std::uint64_t expected,
                        std::span<const std::byte> bytes,
                        std::size_t) override {
    if (fail)
      throw std::runtime_error("Injected write failure");
    if ((saved ? saved->revision : 0) != expected)
      throw std::invalid_argument("Store conflict");
    saved = StoredCheckpoint{expected + 1, {bytes.begin(), bytes.end()}};
    return saved->revision;
  }
};
} // namespace

int main() {
  return test::run([] {
    const WorldId id{93};
    const SpaceId space{id, 1};
    const EntityId actor{id, 1}, destroyed{id, 2};
    auto ledger = std::make_shared<runtime::ResourceLedger>();
    World world{id, ledger};
    EntityProps props{{{space, {1e6 + .02, 0, 0}}, {}},
                      {space},
                      Activation::Full,
                      {std::byte{7}}};
    const std::array<WorldMutation, 4> seed{
        CreateSpace{{space, {}}}, SpawnEntity{actor, props},
        SpawnEntity{destroyed, props}, DestroyEntity{destroyed}};
    world.apply(seed, world.snapshot().version(), 8);
    ReferenceFrames frames{world.snapshot(), ledger};
    const FrameId frame{space, world.snapshot().epoch(), 7};
    const std::array<FrameMutation, 1> create{
        CreateFrame{frame, {{}, {{1e6, 0, 0}, {}}}}};
    frames.apply(world.snapshot(), create, frames.snapshot().version());
    props.attachment =
        attach(frames.snapshot().resolve(frame), props.pose, props.velocity,
               FrameVelocityPolicy::PreserveWorld);
    const std::array<WorldMutation, 1> attachEntity{SetEntity{actor, props}};
    world.apply(attachEntity, world.snapshot().version(), 8);
    frames.apply(world.snapshot(), {}, frames.snapshot().version());
    CheckpointMetadata metadata{
        "exact-content-lock",
        {{"unloaded/cell-8", 4, {std::byte{3}, std::byte{9}}}}};
    auto storage = std::make_shared<FaultStorage>();
    WorldStoreProps limits;
    limits.maxCheckpointBytes = 1024 * 1024;
    WorldStore store{storage, ledger, limits};
    require(!store.load(id), "missing world is distinct from corrupt save");
    const auto snapshot = world.snapshot();
    auto saved = store.save(snapshot, frames.snapshot(), metadata, 0);
    require(saved.storeRevision == 1 && saved.captured == snapshot.version(),
            "save reports captured world boundary");
    const auto oldHandle = world.handle(actor);
    auto restored = store.load(id);
    require(restored->world->snapshot().find(actor)->props.pose == props.pose &&
                restored->world->snapshot().find(destroyed)->destroyed &&
                restored->world->snapshot().find(destroyed)->props.data.empty(),
            "reopened candidate preserves precision and deletion tombstones");
    test::rejects([&] { restored->world->snapshot().resolve(oldHandle); },
                  "load creates a fresh incarnation");
    const auto attachment =
        *restored->world->snapshot().find(actor)->props.attachment;
    require(attachment.frame.epoch == restored->world->snapshot().epoch() &&
                restored->frames->snapshot()
                        .resolve(attachment.frame)
                        .pose.position.meters.x == 1e6,
            "saved frame identities rebind before candidate publication");
    require(restored->metadata.contentLock == metadata.contentLock &&
                restored->metadata.records[0].bytes ==
                    metadata.records[0].bytes,
            "unloaded and unavailable domain records survive unchanged");
    test::rejects([&] { store.save(snapshot, frames.snapshot(), metadata, 0); },
                  "old completion cannot overwrite a newer checkpoint");
    storage->fail = true;
    test::rejects<std::runtime_error>(
        [&] { store.save(snapshot, frames.snapshot(), metadata, 1); },
        "failed write propagates");
    require(storage->saved->revision == 1 && store.load(id)->storeRevision == 1,
            "failed write preserves last checkpoint");
    storage->fail = false;
    const auto encoded = storage->saved->bytes;
    storage->saved->bytes[8] = std::byte{0};
    test::rejects([&] { store.load(id); },
                  "unknown schema refused before active replacement");
    store.addMigration(0, [](auto bytes, std::size_t maximum) {
      require(bytes.size() <= maximum, "migration receives a bound");
      std::vector<std::byte> result(bytes.begin(), bytes.end());
      result[8] = std::byte{1};
      return result;
    });
    require(store.load(id)->world->snapshot().find(actor)->props.pose ==
                props.pose,
            "registered migration candidate is validated and restored");
    storage->saved->bytes = encoded;
    storage->saved->bytes.pop_back();
    test::rejects<std::length_error>([&] { store.load(id); },
                                     "truncated payload refused");
    require(world.snapshot().version() == snapshot.version(),
            "failed decode never changes active world");
    storage->saved->bytes = encoded;
    storage->saved->bytes.push_back(std::byte{});
    test::rejects([&] { store.load(id); }, "trailing ambiguous state refused");
    storage->saved->bytes = encoded;

    const auto path =
        std::filesystem::temp_directory_path() /
        ("playground-world-store-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(path);
    struct Cleanup {
      std::filesystem::path path;
      ~Cleanup() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
      }
    } cleanup{path};
    auto disk = std::make_shared<platform::DirectoryCheckpointStore>(
        path, limits.maxCheckpointBytes);
    WorldStore native{disk, ledger, limits};
    native.save(snapshot, frames.snapshot(), metadata, 0);
    { // Interrupted candidate is ignored by recovery.
      std::ofstream partial{path / "93.candidate", std::ios::binary};
      partial << "partial";
    }
    auto reopenedDisk = std::make_shared<platform::DirectoryCheckpointStore>(
        path, limits.maxCheckpointBytes);
    WorldStore reopened{reopenedDisk, ledger, limits};
    require(reopened.load(id)->world->snapshot().find(actor)->props.pose ==
                props.pose,
            "native checkpoint reopens after abandoned staging file");
    std::atomic<unsigned> wins{}, conflicts{};
    auto writer = [&](WorldStore &target) {
      try {
        target.save(snapshot, frames.snapshot(), metadata, 1);
        ++wins;
      } catch (const std::invalid_argument &) {
        ++conflicts;
      }
    };
    {
      std::jthread a{writer, std::ref(native)}, b{writer, std::ref(reopened)};
    }
    require(wins == 1 && conflicts == 1 &&
                reopened.load(id)->storeRevision == 2,
            "separate storage handles serialize compare-and-swap writers");
    require(!std::filesystem::exists(path / "93.candidate"),
            "published candidate leaves no temporary file");
    {
      std::fstream corrupt{path / "93.world",
                           std::ios::binary | std::ios::in | std::ios::out};
      corrupt.seekp(30);
      corrupt.put(char(99));
    }
    test::rejects<std::runtime_error>([&] { reopened.load(id); },
                                      "native envelope detects corrupt data");
  });
}
