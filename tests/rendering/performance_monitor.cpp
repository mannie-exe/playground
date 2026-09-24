#include <limits>
#include <string>

#include <SDL3/SDL_log.h>

#include <support/PerformanceMonitor.hpp>
#include <support/Test.hpp>

int main() {
  return playground::test::run([] {
    PerformanceMonitor monitor{{.enabled = true,
                                .sampleEveryFrames = 60,
                                .historySize = 2,
                                .logSummary = false}};
    monitor.end(FramePhase::Render);
    monitor.endFrame();
    playground::test::require(
        monitor.history().empty(),
        "unstarted phases and frames do not create samples");
    playground::test::require(monitor.gpuTimingStatus() ==
                                  GPUTimingStatus::Unsupported,
                              "GPU timing starts explicitly unsupported");
    PerformanceSample sample;
    sample.measured[static_cast<std::size_t>(FramePhase::Total)] = true;
    sample.milliseconds[static_cast<std::size_t>(FramePhase::Total)] = 2;
    monitor.recordFrame(sample);
    monitor.recordFrame(sample);
    monitor.recordFrame(sample);
    playground::test::require(monitor.history().size() == 2 &&
                                  monitor.history().front().frame == 2 &&
                                  monitor.history().back().frame == 3,
                              "history evicts oldest complete sample");
    playground::test::require(
        !monitor.history()
             .back()
             .cpu.measured[static_cast<std::size_t>(FramePhase::Present)],
        "unmeasured phases remain distinct from zero-duration phases");

    SDL_KeyboardEvent key{};
    key.type = SDL_EVENT_KEY_DOWN;
    key.key = SDLK_F11;
    playground::test::require(monitor.handleHotkey(key), "F11 handled");
    playground::test::require(
        monitor.getConfig().enabled && !monitor.getConfig().logSummary &&
            monitor.getConfig().historySize == 2 &&
            monitor.getConfig().sampleEveryFrames == 300 &&
            monitor.history().size() == 2,
        "F11 changes only reporting interval");
    {
      struct LogCapture {
        SDL_LogOutputFunction previous{};
        void *userdata{};
        std::string message;
        LogCapture() {
          SDL_GetLogOutputFunction(&previous, &userdata);
          SDL_SetLogOutputFunction(
              [](void *self, int, SDL_LogPriority, const char *message) {
                static_cast<LogCapture *>(self)->message = message;
              },
              this);
        }
        ~LogCapture() { SDL_SetLogOutputFunction(previous, userdata); }
      } captured;
      monitor.report();
      playground::test::require(
          captured.message.contains("present unmeasured") &&
              captured.message.contains("total avg=2.000ms"),
          "summary distinguishes missing phases from measured durations");
    }
    playground::test::require(monitor.history().size() == 2 &&
                                  monitor.history().back().frame == 3,
                              "summary report preserves retained history");
    monitor.setGPUTimingAvailable(true);
    playground::test::require(monitor.gpuTimingStatus() ==
                                  GPUTimingStatus::Pending,
                              "enabled GPU timing waits for a completed query");
    monitor.recordGPU({1, "frame", 0.5, {2}});
    monitor.recordGPU({2, "frame", 0.6, {2}});
    monitor.recordGPU({3, "frame", 0.7, {2}});
    playground::test::rejects([&] { monitor.recordGPU({4, "frame", 0.5}); },
                              "GPU sample requires a resource domain");
    playground::test::rejects(
        [&] { monitor.begin(static_cast<FramePhase>(255)); },
        "invalid phase cannot index outside monitor state");
    playground::test::require(
        monitor.gpuHistory().size() == 2 &&
            monitor.gpuHistory().front().sequence == 2 &&
            monitor.history().back().frame == 3,
        "completed GPU samples have a separate bounded history");
    playground::test::rejects([&] { monitor.recordGPU({4, "frame", -1, {2}}); },
                              "invalid GPU sample rejected");
    {
      PerformanceMonitor signals{{.enabled = true, .logSummary = false}};
      signals.setGPUTimingAvailable(true);
      signals.recordFrame(sample);
      playground::test::require(
          signals.history().back().gpuTiming == GPUTimingStatus::Pending &&
              !signals.history().back().gpuSampleReceived,
          "pending GPU history does not invent a zero measurement");
      signals.recordGPU({10, "frame", 0.25, {2}, 1.25});
      signals.recordFrame(sample);
      playground::test::require(
          signals.history().back().gpuTiming == GPUTimingStatus::Measured &&
              signals.history().back().gpuSampleReceived &&
              signals.gpuHistory().back().completionLatencyMilliseconds == 1.25,
          "completed GPU duration and CPU completion latency remain separate");
      signals.recordFrame(sample);
      playground::test::require(
          !signals.history().back().gpuSampleReceived,
          "a retained GPU result is not a fresh result next frame");
      playground::test::rejects(
          [&] { signals.recordGPU({11, "frame", 0.25, {2}, -1}); },
          "negative completion latency rejected");
      playground::test::rejects(
          [&] {
            signals.recordGPU({11,
                               "frame",
                               0.25,
                               {2},
                               std::numeric_limits<double>::infinity()});
          },
          "nonfinite completion latency rejected");
      signals.report();
      playground::test::require(
          signals.gpuTimingStatus() == GPUTimingStatus::Pending,
          "report consumes latest-result status, not history");
      signals.setEnabled(false);
      playground::test::require(signals.gpuTimingStatus() ==
                                    GPUTimingStatus::Disabled,
                                "supported but disabled profiling is explicit");
      signals.setGPUTimingAvailable(false);
      signals.setEnabled(true);
      signals.recordGPU({12, "frame", 0.25});
      playground::test::require(
          signals.gpuTimingStatus() == GPUTimingStatus::Unsupported &&
              signals.gpuHistory().empty(),
          "unsupported timing cannot accidentally publish samples");
    }
    auto config = monitor.getConfig();
    config.historySize = 1;
    monitor.setConfig(config);
    playground::test::require(monitor.history().size() == 1 &&
                                  monitor.history().front().frame == 3 &&
                                  monitor.gpuHistory().size() == 1 &&
                                  monitor.gpuHistory().front().sequence == 3,
                              "shrinking history preserves newest entries");
    sample.milliseconds[static_cast<std::size_t>(FramePhase::Total)] =
        std::numeric_limits<double>::quiet_NaN();
    playground::test::rejects([&] { monitor.recordFrame(sample); },
                              "nonfinite measured duration rejected");
    playground::test::require(monitor.history().back().frame == 3,
                              "rejected sample does not partially publish");
    config.historySize = 0;
    monitor.setConfig(config);
    sample.milliseconds[static_cast<std::size_t>(FramePhase::Total)] = 0;
    monitor.recordFrame(sample);
    playground::test::require(monitor.history().empty(),
                              "zero history budget disables retention");
    playground::test::require(
        monitor.gpuHistory().empty(),
        "zero history budget also disables GPU retention");
    config.historySize = PerformanceMonitor::maximumHistorySize + 1;
    playground::test::rejects([&] { monitor.setConfig(config); },
                              "excessive history budget rejected");
    monitor.setEnabled(false);
    monitor.recordFrame(sample);
    playground::test::require(monitor.history().empty(),
                              "disabled monitor ignores samples");
  });
}
