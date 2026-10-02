#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

#include <app/AppHost.hpp>
#include <support/TemporaryDirectory.hpp>
#include <support/Test.hpp>
using namespace playground;
using namespace std::chrono_literals;

namespace {
struct Log {
  SDL_LogOutputFunction previous{};
  void *userdata{};
  std::atomic<bool> finished{}, invalid{};

  static void write(void *data, int category, SDL_LogPriority priority,
                    const char *message) {
    auto &log = *static_cast<Log *>(data);
    log.previous(log.userdata, category, priority, message);
    const std::string_view text{message};
    if (text.starts_with("BenchmarkJSON ") && text.contains("\"final\":true")) {
      log.invalid = !text.contains("\"phase\":\"complete\"") ||
                    text.contains("\"drain_timed_out\":true");
      log.finished = true;
    }
  }

  Log() {
    SDL_GetLogOutputFunction(&previous, &userdata);
    SDL_SetLogOutputFunction(write, this);
  }

  ~Log() { SDL_SetLogOutputFunction(previous, userdata); }
};
} // namespace

int main(int argc, char **argv) {
  if (argc < 2 || argc > 3) {
    std::cerr << "Usage: playground_scene_host_workload "
                 "material|bistro|chess|benchmark|camera [seconds]\n";
    return 2;
  }
  SDL_setenv_unsafe("MVK_CONFIG_LOG_LEVEL", "2", 0);
  return test::run([&] {
    const std::string_view scene{argv[1]};
    const bool benchmark = scene == "benchmark";
    const bool camera = scene == "camera";
    const auto id = benchmark                         ? AppId::BistroBenchmark
                    : (scene == "material" || camera) ? AppId::Demo3D
                    : scene == "bistro"               ? AppId::Bistro
                    : scene == "chess"                ? AppId::Chess
                                                      : AppId::Menu;
    test::require(id != AppId::Menu, "known scene workload");
    const double seconds = argc == 3 ? std::stod(argv[2]) : 5.;
    test::require(std::isfinite(seconds) && seconds >= 0 && seconds <= 3600,
                  "duration 0..3600 seconds");
    test::TemporaryDirectory user{"playground-scene-workload"};
    std::ofstream{user.path() / "settings.toml"}
        << "schema_version = 5\n[graphics]\nrenderer = 'sdl-gpu'\nvsync = "
           "false\n";
    Log log;
    AppHost host{{.resizable = false},
                 {},
                 ui::makeSettingsView,
                 {.project = PLAYGROUND_SOURCE_DIR, .user = user.path()}};
    host.request(
        {.type = AppCommandType::SwitchTo,
         .target = id,
         .launch = benchmark ? AppLaunchProps{seconds} : AppLaunchProps{}});
    const auto sink = host.hostCompletions();
    const auto started = std::chrono::steady_clock::now();
    bool timedOut{};
    rendering::SceneWork baseline;
    std::uint64_t baselineSamples{};
    std::vector<double> renderTimes;
    bool warm{}, raised{};
    unsigned cameraStage{};
    auto stageAt = started;
    const auto key = [](SDL_Scancode code, bool down) {
      SDL_Event e{};
      e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
      e.key.scancode = code;
      e.key.key = SDL_GetKeyFromScancode(code, SDL_KMOD_NONE, false);
      e.key.down = down;
      SDL_PushEvent(&e);
    };
    std::jthread clock{[&](std::stop_token stop) {
      while (!stop.stop_requested()) {
        std::this_thread::sleep_for(50ms);
        sink.post([&] {
          const auto elapsed = std::chrono::duration<double>(
                                   std::chrono::steady_clock::now() - started)
                                   .count();
          const auto state = host.renderRuntimeState();
          if (!raised && elapsed > 2) {
            int count{};
            auto windows = SDL_GetWindows(&count);
            for (int i = 0; i < count; ++i)
              SDL_RaiseWindow(windows[i]);
            SDL_free(windows);
            raised = true;
          }
          if (benchmark) {
            if (log.finished || (seconds && elapsed > seconds + 150)) {
              timedOut = !log.finished;
              host.request({.type = AppCommandType::Quit});
            }
          } else if (camera && elapsed > 30 && state.sceneWork.uploads &&
                     std::chrono::steady_clock::now() - stageAt > 250ms) {
            const auto mouse = [&] {
              SDL_Event e{};
              e.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
              e.button.button = SDL_BUTTON_RIGHT;
              e.button.down = true;
              e.button.x = host.windowState().actualSize.x / 2.f;
              e.button.y = host.windowState().actualSize.y / 2.f;
              SDL_PushEvent(&e);
              e.type = SDL_EVENT_MOUSE_BUTTON_UP;
              e.button.down = false;
              SDL_PushEvent(&e);
            };
            const auto require = [&](bool value, const char *message) {
              if (!value) {
                std::cerr << message << '\n';
                log.invalid = true;
                host.request({.type = AppCommandType::Quit});
              }
            };
            switch (cameraStage++) {
            case 0:
              mouse();
              break;
            case 1: {
              require(host.windowServices().relativeMouseActive(),
                      "RMB locks viewport mouse");
              SDL_Event e{};
              e.type = SDL_EVENT_MOUSE_MOTION;
              e.motion.xrel = 80;
              e.motion.yrel = 20;
              SDL_PushEvent(&e);
              break;
            }
            case 2:
              key(SDL_SCANCODE_ESCAPE, true);
              key(SDL_SCANCODE_ESCAPE, false);
              break;
            case 3:
              require(!host.windowServices().relativeMouseActive() &&
                          !host.settingsVisible(),
                      "first Escape unlocks without settings");
              mouse();
              break;
            case 4:
              host.requestSettings();
              break;
            case 5:
              require(host.settingsVisible() &&
                          !host.windowServices().relativeMouseActive(),
                      "settings releases mouse lock");
              host.requestSettings(false);
              break;
            case 6:
              mouse();
              break;
            case 7: {
              require(host.windowServices().relativeMouseActive(),
                      "mouse relocks after settings");
              SDL_Event e{};
              e.type = SDL_EVENT_WINDOW_FOCUS_LOST;
              SDL_PushEvent(&e);
              break;
            }
            case 8: {
              require(!host.windowServices().relativeMouseActive(),
                      "focus loss releases mouse lock");
              SDL_Event e{};
              e.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
              SDL_PushEvent(&e);
              break;
            }
            case 9:
              mouse();
              break;
            case 10:
              require(host.windowServices().relativeMouseActive(),
                      "mouse relocks after focus restoration");
              host.request({.type = AppCommandType::ReturnToMenu});
              break;
            case 11:
              require(!host.windowServices().relativeMouseActive(),
                      "app exit releases mouse lock");
              std::cout << "CameraWorkflow checks=7 complete=true\n";
              host.request({.type = AppCommandType::Quit});
              break;
            }
            stageAt = std::chrono::steady_clock::now();
          } else if (!camera && elapsed > 30 && state.sceneWork.uploads) {
            if (!warm) {
              baseline = state.sceneWork;
              baselineSamples = state.cpuSamples;
              warm = true;
              if (scene != "material")
                key(SDL_SCANCODE_RIGHT, true);
            }
            const auto telemetry = host.renderTelemetry();
            const auto available = std::min<std::uint64_t>(
                state.cpuSamples - baselineSamples, telemetry.cpu.size());
            for (std::size_t i = telemetry.cpu.size() - available;
                 i < telemetry.cpu.size(); ++i)
              if (telemetry.cpu[i].measured[2])
                renderTimes.push_back(telemetry.cpu[i].milliseconds[2]);
            baselineSamples = state.cpuSamples;
            if (seconds && elapsed > 30 + seconds) {
              std::sort(renderTimes.begin(), renderTimes.end());
              std::cout << "SceneWorkload scene=" << scene
                        << " render_samples=" << renderTimes.size()
                        << " p50_ms="
                        << (renderTimes.empty()
                                ? 0
                                : renderTimes[renderTimes.size() / 2])
                        << " uploads="
                        << state.sceneWork.uploads - baseline.uploads
                        << " upload_bytes="
                        << state.sceneWork.uploadBytes - baseline.uploadBytes
                        << '\n';
              log.invalid = state.sceneWork.uploads != baseline.uploads;
              host.request({.type = AppCommandType::Quit});
            }
          } else if (elapsed > 150) {
            timedOut = true;
            host.request({.type = AppCommandType::Quit});
          }
        });
      }
    }};
    host.run();
    clock.request_stop();
    clock.join();
    test::require(!timedOut && !log.invalid,
                  "scene workload finishes without timeout or invalidation");
  });
}
