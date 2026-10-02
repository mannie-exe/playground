#include <limits>

#include <runtime/Benchmark.hpp>
#include <support/Test.hpp>
using namespace playground;

int main() {
  return test::run([] {
    for (double duration : {5., 15., 0.}) {
      runtime::BenchmarkRun run{duration};
      rendering::CPUSample cpu;
      cpu.measured[2] = true;
      cpu.milliseconds[2] = 4;
      run.record(cpu);
      run.ready(10);
      run.advance(24.9, 20, false);
      test::require(run.phase() == runtime::BenchmarkPhase::Warmup &&
                        !run.cpu[2].count,
                    "loading and full loop warm-up excluded");
      run.advance(25, 21, false);
      run.record(cpu);
      rendering::GPUTimingSample gpu{.label = "scene3d", .milliseconds = 2};
      gpu.context.frameId = 21;
      run.record(gpu);
      gpu.context.frameId = 22;
      run.record(gpu);
      test::require(run.cpu[2].count == 1 && run.gpuScene.count == 1,
                    "sample admission excludes warm-up GPU completions");
      run.advance(25 + (duration ? duration : 1000), 30, true);
      if (!duration) {
        test::require(run.phase() == runtime::BenchmarkPhase::Measuring,
                      "infinite run remains measuring across loops");
        run.invalidate();
      } else {
        test::require(run.phase() == runtime::BenchmarkPhase::Draining,
                      "finite duration enters completion drain");
        gpu.context.frameId = 31;
        run.record(gpu);
        test::require(run.gpuScene.count == 1, "post-run frames excluded");
        gpu.context.frameId = 30;
        run.record(gpu);
        run.advance(26 + duration, 31, false);
        test::require(run.finished() && run.gpuScene.count == 2 &&
                          !run.drainTimedOut,
                      "late measured GPU samples drain before completion");
      }
      run.record(cpu);
      run.record(gpu);
      test::require(run.cpu[2].count == 1,
                    "finished runs stop collecting CPU samples");
    }
    runtime::BenchmarkRun timeout{5, 0};
    timeout.ready(1);
    timeout.advance(1, 0, false);
    timeout.advance(6, 2, true);
    timeout.advance(8, 3, true);
    test::require(timeout.drainTimedOut && timeout.finished(),
                  "drain is bounded and marks incomplete queries");
    test::rejects([&] { timeout.advance(7, 4, false); },
                  "backwards clock rejected");
    test::rejects([] { runtime::BenchmarkRun run{-1}; },
                  "negative duration rejected");
    runtime::BenchmarkDistribution stats;
    for (std::size_t i = 0; i < stats.capacity + 1; ++i)
      stats.add(double(i));
    test::require(stats.recent.size() == stats.capacity &&
                      stats.count == stats.capacity + 1 &&
                      stats.percentile(1) == stats.capacity,
                  "infinite run retains bounded percentile storage");
  });
}
