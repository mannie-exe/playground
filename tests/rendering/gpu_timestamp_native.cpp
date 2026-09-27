#include <cmath>
#include <exception>
#include <iostream>
#include <memory>

#include <SDL3/SDL_log.h>

#include "GPUPainter.hpp"
#include <app/SDLGuard.hpp>
#include <platform/sdl/GPURenderBackend.hpp>
#include <support/GPUReadback.hpp>
#include <support/Test.hpp>

using namespace playground;
using namespace playground::sdl;
using namespace playground::sdl::gpu_detail;

int main() {
  try {
    SDLGuard sdl{SDL_INIT_VIDEO};
    if (!packagedShaderFormats() ||
        !SDL_GPUSupportsShaderFormats(packagedShaderFormats(), "vulkan")) {
      SDL_Log("Vulkan timestamp regression requires a supported device");
      return 77;
    }

    return test::run([] {
      for (const bool debug : {false, true}) {
        auto props = GPUDeviceProps{packagedShaderFormats(), debug, "vulkan"};
        props.limits.maxTimestampScopes = 2;
        auto device = std::make_shared<GPUDevice>(props);
        PaintDevice paint{device};
        auto target = paint.targets.color({4, 4});

        // Warm up without profiling, then toggle it on/off/on. Readback
        // completes each submission so SDL can recycle command buffers between
        // frames.
        for (const bool enabled : {false, true, false, true}) {
          device->setProfilingEnabled(enabled);
          SDL_Log(
              "Timestamp regression: GPU debug=%d, profiling=%d, supported=%d",
              debug, enabled, device->supportsTimestamps());
          for (int frame = 0; frame < 4; ++frame) {
            GPUPainter painter{paint, {1, 1}};
            painter.fill(math::rect(0, 0, 4, 4), {255, 0, 0, 255});
            painter.finish(target->get(), {4, 4}, {});
            test::require(test::readPixel(device, *target, 1, 1)[0] > .99f,
                          "profiling toggles preserve rendered output");

            const auto samples = device->takeGPUTimings();
            const auto collection = device->gpuTimingCollection();
            if (enabled && device->supportsTimestamps()) {
              test::require(!samples.empty(),
                            "recycled buffers produce completed GPU timings");
              for (const auto &sample : samples)
                test::require(
                    sample.sequence > 0 &&
                        sample.domain == device->resourceDomain() &&
                        sample.collectionGeneration == collection.generation &&
                        std::isfinite(sample.milliseconds) &&
                        sample.milliseconds >= 0,
                    "timings retain valid duration and device identity");
              for (const auto &sample : samples)
                if (sample.label == "paint2d")
                  test::require(sample.context.targetPixels ==
                                    math::Vec2i{4, 4},
                                "native paint samples retain target size");
            } else {
              test::require(
                  samples.empty(),
                  "disabled or unsupported timing produces no samples");
            }
          }
        }
        if (device->supportsTimestamps()) {
          Commands old{device, "old collection"};
          const auto generation = device->gpuTimingCollection().generation;
          device->setProfilingEnabled(false);
          device->setProfilingEnabled(true);
          test::require(
              device->gpuTimingCollection().generation == generation + 1 &&
                  device->gpuTimingCollection().pending == 0,
              "new collection excludes old live queries from its gauge");
          old.submit();
          test::require(SDL_WaitForGPUIdle(device->get()),
                        "old collection completion");
          test::require(
              device->takeGPUTimings().empty(),
              "late old-generation results are drained, not published");
          {
            Commands first{device, "held first"};
            Commands second{device, "held second"};
            Commands overflow{device, "no query slot"};
            test::require(
                device->gpuTimingCollection().queryDrops == 1 &&
                    device->gpuTimingCollection().pending == 2,
                "query exhaustion is observable without blocking commands");
          }
          test::require(device->gpuTimingCollection().pending == 0,
                        "cancellation removes pending queries");
          for (int i = 0; i < 3; ++i) {
            Commands completed{device, "buffered result"};
            completed.submit();
            test::require(SDL_WaitForGPUIdle(device->get()),
                          "buffered completion");
            device->pollCompletions();
          }
          const auto collection = device->gpuTimingCollection();
          test::require(
              collection.bufferDiscards == 1 && collection.queryDrops == 1 &&
                  collection.pending == 0 &&
                  device->takeGPUTimings().size() == 2,
              "completed buffer eviction is distinct from query-slot loss");
          device->setProfilingEnabled(false);
          device->setProfilingEnabled(true);
          test::require(device->gpuTimingCollection().queryDrops == 0 &&
                            device->gpuTimingCollection().bufferDiscards == 0,
                        "loss counters restart with the collection generation");
        }
      }
    });
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
