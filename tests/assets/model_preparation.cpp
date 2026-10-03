#include <array>
#include <cstring>
#include <future>

#include <platform/sdl/AssetResources.hpp>
#include <support/ModelFixture.hpp>
#include <support/TaskGate.hpp>
#include <support/Test.hpp>

using namespace playground;

int main() {
  return test::run([] {
    const assets::AssetId<assets::ModelAsset> id{"triangle"}, bad{"bad"};
    auto catalog = std::make_shared<assets::AssetCatalog>(".");
    catalog->add(id, assets::ModelAsset{.source = test::modelFixture()});
    catalog->add(bad, assets::ModelAsset{.source = assets::ByteSource{}});
    catalog->freeze();
    auto ledger = std::make_shared<runtime::ResourceLedger>();
    const auto globalBefore =
        runtime::defaultResourceLedger()->snapshot().memory[0].bytes;
    AssetRegistry cache{ledger};
    sdl::AssetResources resources{catalog, cache};
    auto original = resources.model(id);
    test::require(original == resources.model(id),
                  "synchronous model requests share immutable result");
    test::require(
        ledger->snapshot().memory[0].bytes > 0 &&
            runtime::defaultResourceLedger()->snapshot().memory[0].bytes ==
                globalBefore,
        "model storage is charged to resource provider ledger");
    scene::Scene3D scene;
    original->instantiate(scene);
    const auto before = scene.snapshot();
    test::rejects([&] { resources.model(bad); },
                  "failed model preparation throws");
    test::require(scene.snapshot().size() == before.size(),
                  "preparation cannot mutate live scene");

    runtime::Executor executor{{.workers = 1,
                                .maxOutstanding = 8,
                                .maxReservedBytes = 1024ull * 1024 * 1024}};
    auto gate = std::make_shared<test::TaskGate>();
    std::promise<void> entered;
    auto enteredFuture = entered.get_future();
    executor.submit(
        [&entered, gate](std::stop_token stop) noexcept {
          entered.set_value();
          gate->wait(stop);
        },
        0);
    enteredFuture.get();
    sdl::ModelPreparation request;
    test::require(request.start(executor, catalog, bad),
                  "initial request queued");
    test::require(request.start(executor, catalog, id),
                  "new request supersedes old");
    runtime::Executor full{{.maxReservedBytes = 1}};
    test::require(!request.start(full, catalog, bad),
                  "rejected admission preserves current request");
    test::require(request.isPending() && !request.poll(),
                  "poll is nonblocking while worker occupied");
    std::promise<void> barrier;
    auto done = barrier.get_future();
    executor.submit(
        [&barrier](std::stop_token) noexcept { barrier.set_value(); }, 0);
    gate->open();
    done.get();
    auto result = request.poll();
    test::require(result && result->model && !result->error &&
                      result->generation == 2,
                  "only latest successful generation published");
    test::require(!request.poll() && !request.isPending(),
                  "completion consumed once");
    test::require(resources.publishModel(*result) == original,
                  "owner publication reuses cached identity");
    auto other = std::make_shared<assets::AssetCatalog>(".");
    other->add(id, assets::ModelAsset{.source = test::modelFixture()});
    other->freeze();
    sdl::AssetResources otherResources{other, cache};
    test::rejects([&] { otherResources.publishModel(*result); },
                  "foreign catalog result rejected");

    test::require(request.start(executor, catalog, bad),
                  "failure request admitted");
    std::promise<void> barrier2;
    auto done2 = barrier2.get_future();
    executor.submit(
        [&barrier2](std::stop_token) noexcept { barrier2.set_value(); }, 0);
    done2.get();
    auto failed = request.poll();
    test::require(failed && failed->error && !failed->model,
                  "worker exception transported to owner");
    test::require(resources.model(id) == original,
                  "failure retains working resource");
    request.start(executor, catalog, id);
    request.cancel();
    test::require(!request.isPending() && !request.poll(),
                  "cancellation suppresses even ready results");
    std::stop_source stop;
    stop.request_stop();
    test::rejects<std::runtime_error>(
        [&] { sdl::prepareModel(*catalog, id, stop.get_token()); },
        "canceled sync preparation rejects");
    test::require(scene.snapshot().size() == before.size(),
                  "async completion does not implicitly instantiate");

    // Resolve external geometry only through declared catalog dependencies.
    auto fixture = test::modelFixture();
    std::string json{reinterpret_cast<const char *>(fixture.bytes.data()),
                     fixture.bytes.size()};
    const auto uriStart = json.find("data:application");
    const auto uriEnd = json.find('"', uriStart);
    json.replace(uriStart, uriEnd - uriStart, "mesh.bin");
    const auto jsonBytes = std::as_bytes(std::span{json.data(), json.size()});
    assets::ByteSource geometry{std::vector<std::byte>(66)};
    const std::array<float, 9> positions{-1, -1, 1, 1, -1, 1, 0, 1, 1};
    const std::array<std::uint16_t, 3> indices{0, 1, 2};
    std::memcpy(geometry.bytes.data(), positions.data(), sizeof(positions));
    std::memcpy(geometry.bytes.data() + 60, indices.data(), sizeof(indices));
    const assets::AssetId<assets::BinaryAsset> geometryId{"geometry"};
    assets::AssetCatalog external{"."};
    external.add(geometryId, assets::BinaryAsset{geometry});
    external.add(
        id, assets::ModelAsset{.source = assets::ByteSource{{jsonBytes.begin(),
                                                             jsonBytes.end()}},
                               .resources = {{"mesh.bin", geometryId}}});
    external.freeze();
    test::require(
        sdl::prepareModel(external, id)->nodes()[0].primitives.size() == 1,
        "registered URI dependency supplies geometry");
    assets::AssetCatalog undeclared{"."};
    undeclared.add(
        id, assets::ModelAsset{.source = assets::ByteSource{
                                   {jsonBytes.begin(), jsonBytes.end()}}});
    undeclared.freeze();
    test::rejects([&] { sdl::prepareModel(undeclared, id); },
                  "external URI cannot fall back to implicit disk IO");
  });
}
