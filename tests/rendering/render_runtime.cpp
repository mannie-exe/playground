#include <atomic>
#include <limits>
#include <thread>

#include <rendering/RenderRuntime.hpp>
#include <support/Test.hpp>

using namespace playground;
using namespace playground::rendering;
using namespace std::chrono_literals;

int main() {
  return test::run([] {
    auto ledger = std::make_shared<ResourceLedger>(
        ResourceBudgetProps{.cpuBytes = 128,
                            .gpuBytes = 128,
                            .targetBytes = 96,
                            .preparationBytes = 64});
    auto target =
        ledger->reserve(MemoryClass::GPU, ResourceKind::Target, 80, "target");
    target->setState(AllocationState::Owned);
    auto copy = target;
    test::require(ledger->snapshot().memory[1].bytes == 80,
                  "sharing resource ownership charges only once");
    test::rejects<ResourcePressure>(
        [&] {
          ledger->reserve(MemoryClass::GPU, ResourceKind::Target, 17,
                          "replacement");
        },
        "category ceiling rejects replacement overlap atomically");
    auto budgets = ledger->snapshot().budgets;
    budgets.gpuBytes = 32;
    ledger->setBudgets(budgets);
    test::rejects<ResourcePressure>(
        [&] {
          ledger->reserve(MemoryClass::GPU, ResourceKind::Texture, 1, "growth");
        },
        "lowered ceiling preserves ownership but blocks growth without "
        "underflow");
    target->setState(AllocationState::Retiring);
    target.reset();
    test::require(ledger->snapshot().states[2] == 80,
                  "retiring shared resource remains committed");
    copy.reset();
    test::require(ledger->snapshot().memory[1].bytes == 0,
                  "last lifetime releases charge after lowering budget");
    test::rejects<ResourcePressure>(
        [&] {
          ledger->reserve(MemoryClass::CPU, ResourceKind::Asset,
                          std::numeric_limits<std::size_t>::max(), "overflow");
        },
        "overflow request cannot corrupt the ledger");

    std::atomic<bool> release{}, ready{};
    std::thread worker{[&] {
      auto token = ledger->reserve(MemoryClass::CPU, ResourceKind::Preparation,
                                   64, "worker");
      ready = true;
      while (!release)
        std::this_thread::yield();
    }};
    while (!ready)
      std::this_thread::yield();
    const auto held = ledger->snapshot();
    bool refused{};
    try {
      ledger->reserve(MemoryClass::CPU, ResourceKind::Preparation, 1,
                      "other worker");
    } catch (const ResourcePressure &) {
      refused = true;
    }
    release = true;
    worker.join();
    test::require(held.memory[0].bytes == 64 && refused &&
                      ledger->snapshot().memory[0].bytes == 0,
                  "worker reservations share admission and retire safely");

    RenderRuntime runtime{ledger};
    const auto domain = acquireResourceDomain();
    runtime.attachDomain(domain);
    const RenderClock::time_point now{};
    auto first = runtime.beginFrame(now, domain, 1);
    auto submittedWork =
        first; // native recording/submission retains the credit
    first.reset();
    auto second = runtime.beginFrame(now, domain, 1);
    test::require(runtime.admission(now, 1).status ==
                      FrameAdmissionStatus::Busy,
                  "two frames bound work before any additional preparation");
    second.reset();
    runtime.abandoned();
    test::require(runtime.snapshot().outstandingFrames == 1,
                  "abandonment does not retire already submitted work");
    submittedWork.reset();
    runtime.applyPatch(
        {.pacing = FramePacingProps{.maximumFramesPerSecond = 60}});
    test::require(runtime.admission(now, 1).status ==
                      FrameAdmissionStatus::Paced,
                  "live cap schedules from previous admission");
    auto paced = runtime.beginFrame(now + 17ms, domain, 1);
    test::require(bool(paced), "frame becomes eligible at cap deadline");
    paced.reset();
    auto late = runtime.beginFrame(now + 10s, domain, 1);
    late.reset();
    test::require(!runtime.beginFrame(now + 10s, domain, 1),
                  "missed deadlines do not generate catch-up rendering");
    runtime.applyPatch({.pacing = FramePacingProps{}});
    runtime.block("working set cannot fit", 1);
    test::require(runtime.admission(now + 11s, 1).status ==
                      FrameAdmissionStatus::Blocked,
                  "persistent pressure has no timed busy retry");
    test::require(runtime.admission(now + 11s, 2).status ==
                      FrameAdmissionStatus::Ready,
                  "changed application demand permits another attempt");
    test::require(
        !runtime.snapshot().pressure.empty(),
        "allowing a retry preserves the refusal diagnostic until success");
    runtime.submitted();
    test::require(runtime.snapshot().pressure.empty(),
                  "successful presentation clears the pressure diagnostic");
    runtime.block("working set cannot fit", 2);
    auto token = ledger->reserve(MemoryClass::CPU, ResourceKind::Surface, 8,
                                 "released resource");
    token.reset();
    test::require(runtime.admission(now + 11s, 2).status ==
                      FrameAdmissionStatus::Ready,
                  "accounting changes can unblock pressure");
    const auto before = runtime.snapshot().resources.policyRevision;
    test::rejects<std::invalid_argument>(
        [&] {
          runtime.applyPatch(
              {.budgets = ResourceBudgetProps{},
               .pacing = FramePacingProps{.maxOutstandingFrames = 0}});
        },
        "invalid live patch fails transactionally");
    test::require(runtime.snapshot().resources.policyRevision == before,
                  "failed pacing validation cannot publish budget changes");
    for (int i = 0; i < 300; ++i) {
      runtime.beginIteration();
      runtime.begin(CPUPhase::Update);
      runtime.end(CPUPhase::Update);
      runtime.endIteration();
    }
    test::require(
        runtime.cpuHistory().size() == 240 &&
            runtime.snapshot().cpuSamples == 300,
        "baseline CPU measurement has bounded retention without reporting");
  });
}
