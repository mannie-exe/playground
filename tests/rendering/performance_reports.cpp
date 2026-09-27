#include <cmath>
#include <limits>
#include <string>
#include <utility>

#include <SDL3/SDL_log.h>

#include <support/PerformanceMonitor.hpp>
#include <support/Test.hpp>

using namespace playground;

namespace {
rendering::GPUTimingSample sample(std::uint64_t sequence, std::string label,
                                  double ms, math::Vec2i size = {800, 600}) {
  return {sequence, std::move(label), ms, {2}, {}, {.targetPixels = size}, 1};
}

void groupsAndReports() {
  PerformanceMonitor monitor{
      {.enabled = true, .historySize = 2, .logSummary = false}};
  monitor.recordGPUCollection({{2}, 1, true, true, 2, 3, 4});
  monitor.recordGPU(sample(1, "paint2d", 1));
  auto composition = sample(2, "presentation composition", .25);
  composition.context.sourcePixels = math::Vec2i{400, 300};
  composition.completionLatencyMilliseconds = 12;
  monitor.recordGPU(composition);
  auto paint = sample(3, "paint2d", 3);
  paint.completionLatencyMilliseconds = 10;
  monitor.recordGPU(paint);
  monitor.recordFrame({});
  auto report = monitor.snapshotReport();
  test::require(
      report.cpuFrames == 1 && report.gpuSamplesReceived == 3 &&
          report.gpu.size() == 2 && report.queryDrops == 3 &&
          report.bufferDiscards == 4 && report.collection->pending == 2,
      "report separates CPU frames, completed queries and collection health");
  const auto &stats = report.gpu[0];
  test::require(
      stats.duration.count == 2 && stats.duration.totalMilliseconds == 4 &&
          stats.duration.average() == 2 &&
          stats.duration.minimumMilliseconds == 1 &&
          stats.duration.maximumMilliseconds == 3 &&
          stats.completionLatency.count == 1 &&
          stats.completionLatency.average() == 10,
      "interleaved groups and optional latency have independent denominators");
  test::require(report.gpu[1].key.context.sourcePixels ==
                        math::Vec2i{400, 300} &&
                    !report.cpu[0].average(),
                "absent CPU timing is not zero");
  monitor.recordGPU(sample(4, "paint2d", 5, {1600, 1200}));
  monitor.recordGPUCollection({{3}, 1, true, true});
  auto recovered = sample(1, "paint2d", 2);
  recovered.domain = {3};
  monitor.recordGPU(recovered);
  monitor.recordGPU(sample(5, "retired device", 99));
  test::require(
      monitor.snapshotReport().gpu.size() == 4 &&
          monitor.snapshotReport().gpuSamplesReceived == 5,
      "resize and recovery split groups; retired domains cannot publish");

  struct LogCapture {
    SDL_LogOutputFunction previous{};
    void *userdata{};
    std::string message;

    LogCapture() {
      SDL_GetLogOutputFunction(&previous, &userdata);
      SDL_SetLogOutputFunction(
          [](void *self, int, SDL_LogPriority, const char *line) {
            static_cast<LogCapture *>(self)->message = line;
          },
          this);
    }

    ~LogCapture() { SDL_SetLogOutputFunction(previous, userdata); }
  } captured;

  monitor.report();
  test::require(
      captured.message.contains("paint2d") &&
          captured.message.contains("presentation composition") &&
          captured.message.contains("source=400x300 target=800x600") &&
          captured.message.contains(
              "duration avg=2.000ms min=1.000ms max=3.000ms"),
      "console formats independent structured groups");
  report = monitor.snapshotReport();
  test::require(
      report.gpu.empty() && report.cpuFrames == 0 && report.queryDrops == 0 &&
          report.bufferDiscards == 0 && report.gpuSamplesReceived == 0 &&
          monitor.gpuHistory().size() == 2 && monitor.history().size() == 1 &&
          monitor.gpuTimingStatus() == GPUTimingStatus::Measured,
      "report reset preserves history and collection status");
}

void healthAndSessions() {
  PerformanceMonitor monitor{{.enabled = true, .logSummary = false}};
  monitor.recordGPUCollection({{2}, 1, true, true, 2, 3, 4});
  monitor.recordGPUCollection({{2}, 1, true, true, 1, 3, 4});
  test::require(monitor.snapshotReport().queryDrops == 3,
                "counter snapshots are not double-counted");
  monitor.resetReportInterval();
  monitor.recordGPUCollection({{2}, 1, true, true, 0, 5, 7});
  test::require(monitor.snapshotReport().queryDrops == 2 &&
                    monitor.snapshotReport().bufferDiscards == 3,
                "interval counters are deltas, pending is a gauge");
  test::rejects(
      [&] { monitor.recordGPUCollection({{2}, 1, true, true, 0, 4, 7}); },
      "backwards health counters are rejected");
  monitor.setEnabled(false);
  monitor.setEnabled(true);
  const auto revision = monitor.statisticsRevision();
  monitor.setEnabled(false);
  monitor.setEnabled(true);
  test::require(monitor.statisticsRevision() > revision,
                "off/on between frames remains observable to the host");
  monitor.recordGPUCollection({{2}, 2, true, true});
  monitor.recordGPU(sample(1, "old generation", 1));
  auto next = sample(2, "new generation", 2);
  next.collectionGeneration = 2;
  monitor.recordGPU(next);
  test::require(
      monitor.snapshotReport().gpu.size() == 1 &&
          monitor.gpuHistory().size() == 1 &&
          monitor.snapshotReport().gpu.front().key.collectionGeneration == 2,
      "late results cannot contaminate a new profiling generation");
  const auto sameRevision = monitor.statisticsRevision();
  monitor.resetReportInterval();
  test::require(monitor.statisticsRevision() == sameRevision,
                "report-only reset does not restart native collection");
  monitor.recordGPUCollection({});
  monitor.recordGPU(next);
  test::require(monitor.gpuTimingStatus() == GPUTimingStatus::Unsupported &&
                    monitor.gpuHistory().size() == 1,
                "software or unsupported backends publish no GPU timing");
}

void limitsAndValidation() {
  PerformanceMonitor monitor{
      {.enabled = true, .historySize = 0, .logSummary = false}};
  monitor.recordGPUCollection({{2}, 1, true, true});
  for (std::size_t i = 0; i < PerformanceMonitor::maximumGPUGroups + 1; ++i)
    monitor.recordGPU(sample(i + 1, "group" + std::to_string(i), 1));
  monitor.recordGPU(sample(100, "group0", 3));
  auto report = monitor.snapshotReport();
  test::require(
      report.gpu.size() == PerformanceMonitor::maximumGPUGroups &&
          report.omittedGPUSamples == 1 && report.gpuSamplesReceived == 66 &&
          report.gpu[0].duration.average() == 2 && monitor.gpuHistory().empty(),
      "group budget omits new groups but retains known groups without raw "
      "history");
  auto invalid = sample(101, "group0", 1, {-1, 2});
  test::rejects([&] { monitor.recordGPU(invalid); }, "invalid extent rejected");
  invalid = sample(102, "group0", std::numeric_limits<double>::infinity());
  test::rejects([&] { monitor.recordGPU(invalid); },
                "nonfinite duration rejected");
  test::require(monitor.snapshotReport().gpuSamplesReceived == 66,
                "invalid input cannot partially publish");
  DurationStats stats;
  test::require(!stats.average(), "empty average is absent");
  stats.add(0);
  stats.add(4);
  test::require(stats.average() == 2 && stats.minimumMilliseconds == 0 &&
                    stats.maximumMilliseconds == 4,
                "measured zero is included in statistics");
  test::rejects([&] { stats.add(-1); }, "negative duration rejected");
  test::require(stats.count == 2,
                "failed stats update leaves prior results intact");
}
} // namespace

int main() {
  return test::run([] {
    PerformanceMonitor monitor{{.enabled = true, .logSummary = false}};
    monitor.recordIdleWait(100);
    monitor.recordPaintWork({.considered = 3, .rejected = 1, .quads = 2});
    auto idle = monitor.snapshotReport();
    test::require(
        idle.cpuFrames == 0 && idle.idleWait.count == 1 &&
            !idle.cpu[4].average() && idle.paint.rejected == 1,
        "idle waiting and paint counters are not CPU duration samples");
    monitor.resetReportInterval();
    test::require(monitor.snapshotReport().idleWait.count == 0 &&
                      monitor.snapshotReport().paint.considered == 0,
                  "report reset clears idle and paint counters");
    groupsAndReports();
    healthAndSessions();
    limitsAndValidation();
  });
}
