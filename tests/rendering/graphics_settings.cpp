#include <limits>
#include <set>

#include <rendering/GraphicsSettings.hpp>
#include <rendering/ResourceEstimate.hpp>
#include <support/Test.hpp>
using namespace playground;
using namespace playground::rendering;

int main() {
  return test::run([] {
    GraphicsSettings settings;
    settings.validate();
    std::set<std::string_view> keys;
    for (const auto &field : graphicsSettingsSchema()) {
      test::require(keys.insert(field.key).second, "schema identifiers unique");
      GraphicsSettings copy = settings;
      setGraphicsSetting(copy, field, field.get(settings));
      test::require(copy == settings, "schema getters/setters preserve values");
      test::rejects([&] { setGraphicsSetting(copy, field, field.maximum + 1); },
                    "schema bounds enforced");
    }
    test::require(estimateTextureStorage({.width = 5,
                                          .height = 7,
                                          .blockWidth = 4,
                                          .blockHeight = 4,
                                          .blockBytes = 16}) == 64,
                  "compressed block rounding");
    test::require(estimateTextureStorage({.width = 4,
                                          .height = 4,
                                          .depthOrLayers = 6,
                                          .levels = 3,
                                          .samples = 2}) ==
                      (16 + 4 + 1) * 6 * 2 * 4,
                  "mips layers samples charged once");
    test::require(estimateTextureStorage({.width = 4,
                                          .height = 4,
                                          .depthOrLayers = 4,
                                          .levels = 3,
                                          .volume = true}) == (64 + 8 + 1) * 4,
                  "volume mip depth shrinks");
    test::rejects<std::overflow_error>(
        [] {
          estimateTextureStorage(
              {.width = std::numeric_limits<std::size_t>::max(), .height = 2});
        },
        "estimate overflow rejected");
    auto ledger = std::make_shared<ResourceLedger>();
    auto allowance = ledger->reserve(MemoryClass::CPU,
                                     ResourceKind::Preparation, 1024, "plan");
    auto payload =
        ledger->splitReservation(allowance, 512, ResourceKind::Asset);
    payload->setState(AllocationState::Owned);
    test::require(ledger->snapshot().memory[0].bytes == 1024 &&
                      allowance->bytes() == 512 &&
                      ledger->snapshot()
                              .kinds[static_cast<unsigned>(ResourceKind::Asset)]
                              .bytes == 512,
                  "reservation transferred without duplicate commitment");
    test::rejects([&] { ledger->splitReservation(allowance, 513); },
                  "oversized partition rejected atomically");
    allowance.reset();
    test::require(ledger->snapshot().memory[0].bytes == 512,
                  "unused planning allowance released independently");
    payload.reset();
    test::require(ledger->snapshot().memory[0].bytes == 0,
                  "transferred ownership retires");
    QualityController controller;
    const auto domain = acquireResourceDomain();
    settings.automatic.enabled = true;
    settings.threeD.shadows = QualityLevel::Ultra;
    controller.configure(settings);
    controller.reset(domain, true);
    const auto sample = [&](std::uint64_t frame, double ms,
                            std::uint64_t revision) {
      return GPUTimingSample{.label = "scene3d",
                             .milliseconds = ms,
                             .domain = domain,
                             .context = {.frameId = frame,
                                         .workloadId = 1,
                                         .qualityRevision = revision}};
    };
    const auto revision = controller.state().revision;
    for (unsigned i = 1; i <= 8; ++i)
      controller.observe(sample(i, 25, revision));
    test::require(controller.state().sceneScale < 1 &&
                      controller.state().requested == settings,
                  "slow scene changes only effective resolution");
    const auto scale = controller.state().sceneScale;
    for (unsigned i = 9; i < 100; ++i)
      controller.observe(sample(i, 50, revision));
    test::require(controller.state().sceneScale == scale,
                  "delayed old-revision samples ignored");
    for (unsigned i = 100; i < 191; ++i)
      controller.observe(sample(i, 1, controller.state().revision));
    test::require(controller.state().sceneScale > scale,
                  "quality recovers slowly toward manual ceiling");
    for (unsigned i = 0; i < 20; ++i)
      controller.pressure();
    test::require(controller.state().sceneScale == .5f &&
                      controller.state().atMinimum,
                  "pressure cannot cross quality floor");
    controller.configure(settings);
    CPUSample cpu;
    cpu.measured[static_cast<unsigned>(CPUPhase::Update)] = true;
    cpu.milliseconds[static_cast<unsigned>(CPUPhase::Update)] = 100;
    for (int i = 0; i < 30; ++i)
      controller.observeCPU(cpu);
    for (unsigned i = 1; i < 30; ++i)
      controller.observe(sample(i, 30, controller.state().revision));
    test::require(controller.state().sceneScale == 1,
                  "CPU update bottleneck does not lower scene resolution");
    controller.configure(settings);
    FramePacingProps cap{.maximumFramesPerSecond = 20};
    for (unsigned i = 1; i < 30; ++i)
      controller.observe(sample(i, 30, controller.state().revision), &cap);
    test::require(controller.state().sceneScale == 1,
                  "effective runtime cap limits quality timing objective");
    settings.automatic.enabled = false;
    controller.configure(settings);
    test::require(controller.state().sceneScale == 1 && !controller.pressure(),
                  "manual mode restores requested settings");
    settings.automatic.enabled = true;
    controller.configure(settings);
    controller.reset(domain, false);
    for (unsigned i = 1; i < 100; ++i)
      controller.observe(sample(i, 40, controller.state().revision));
    test::require(controller.state().sceneScale == 1 && controller.pressure(),
                  "unsupported timings suspend time control but permit "
                  "pressure response");
  });
}
