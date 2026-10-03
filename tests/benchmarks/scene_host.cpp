#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

#include <app/AppHost.hpp>
#include <platform/sdl/ProcessEnvironment.hpp>
#include <support/CommandLine.hpp>
#include <support/HostPreferences.hpp>
#include <support/Test.hpp>
using namespace playground;
using namespace std::chrono_literals;

namespace {
struct Log {
  SDL_LogOutputFunction previous{};
  void *userdata{};
  std::atomic<bool> finished{}, invalid{};
  std::atomic<unsigned> periodic{};
  std::string_view expectedInterruption;

  static void write(void *data, int category, SDL_LogPriority priority,
                    const char *message) {
    auto &log = *static_cast<Log *>(data);
    log.previous(log.userdata, category, priority, message);
    const std::string_view text{message};
    if (text.starts_with("BenchmarkJSON ") &&
        text.contains("\"final\":false")) {
      ++log.periodic;
      if (text.contains("\"width\":0,") || text.contains("\"height\":0,"))
        log.invalid = true;
    }
    if (text.starts_with("BenchmarkJSON ") && text.contains("\"final\":true")) {
      const bool complete = text.contains("\"phase\":\"complete\"");
      const bool expected =
          !log.expectedInterruption.empty() &&
          text.contains(std::string{"\"interruption\":\""} +
                        std::string{log.expectedInterruption} + "\"");
      log.invalid =
          log.invalid ||
          (log.expectedInterruption.empty() ? !complete : !expected) ||
          text.contains("\"drain_timed_out\":true") ||
          (complete && text.contains("\"cpu_render_samples\":0"));
      log.finished = true;
    }
  }

  explicit Log(std::string_view expected = {})
      : expectedInterruption{expected} {
    SDL_GetLogOutputFunction(&previous, &userdata);
    SDL_SetLogOutputFunction(write, this);
  }

  ~Log() { SDL_SetLogOutputFunction(previous, userdata); }
};
} // namespace

int main(int argc, char **argv) {
  constexpr std::string_view usage =
      "Usage: playground_scene_host_workload "
      "material|bistro|chess|benchmark|infinite|interrupted|camera [seconds]\n";
  if (test::cli::helpRequested(argc, argv))
    return test::cli::help(usage);
  if (argc < 2 || argc > 3)
    return test::cli::usageError(usage);
  const std::string_view scene{argv[1]};
  if (scene != "material" && scene != "bistro" && scene != "chess" &&
      scene != "benchmark" && scene != "infinite" && scene != "interrupted" &&
      scene != "camera")
    return test::cli::usageError(usage, "Unknown scene workload");
  const auto duration = argc == 3 ? test::cli::number(argv[2], 0, 3600)
                                  : std::optional<double>{5};
  if (!duration)
    return test::cli::usageError(usage, "Duration must be 0..3600 seconds");
  const double seconds = *duration;
  return test::run([&] {
    const bool infinite = scene == "infinite";
    const bool interrupted = scene == "interrupted";
    const bool benchmark = scene == "benchmark" || infinite || interrupted;
    const bool camera = scene == "camera";
    const bool inspection = scene == "material" || scene == "chess";
    const auto id = benchmark             ? AppId::BistroBenchmark
                    : camera              ? AppId::Bistro
                    : scene == "material" ? AppId::Demo3D
                    : scene == "bistro"   ? AppId::Bistro
                    : scene == "chess"    ? AppId::Chess
                                          : AppId::Menu;
    sdl::configureProcessEnvironment();
    test::HostPreferences user{
        "playground-scene-workload",
        {.presentation = {.vsync = false},
         .renderer = {rendering::RendererChoice::SDLGPU}}};
    Log log{infinite ? "cancelled" : interrupted ? "focus" : ""};
    AppHost host{{.resizable = false},
                 {},
                 ui::makeSettingsView,
                 {.project = PLAYGROUND_SOURCE_DIR, .user = user.path()}};
    host.request({.type = AppCommandType::SwitchTo,
                  .target = id,
                  .launch = benchmark ? AppLaunchProps{infinite ? 0 : seconds}
                                      : AppLaunchProps{}});
    const auto sink = host.hostCompletions();
    const auto started = std::chrono::steady_clock::now();
    bool timedOut{}, interruptionSent{};
    rendering::SceneWork baseline;
    std::uint64_t baselineSamples{};
    std::vector<double> renderTimes;
    bool warm{}, raised{}, sized{}, completed{};
    double measuredAt{};
    std::uint64_t resumedMeshHits{};
    unsigned cameraStage{};
    std::optional<unsigned> focusStage;
    auto focusAt = started;
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
          if (!sized && host.windowState().title == toString(id) &&
              host.windowRequestStatus().outcome !=
                  platform::WindowTransitionOutcome::Pending) {
            auto policy = host.viewPolicy();
            policy.resizable = false;
            policy.initialSizing = platform::InitialWindowSizing::Preferred;
            policy.preferredWindowSize = math::Vec2i{960, 720};
            host.request(
                {.type = AppCommandType::SetViewPolicy, .view = policy});
            sized = true;
          }
          if (!raised && sized && elapsed > 2) {
            int count{};
            auto windows = SDL_GetWindows(&count);
            for (int i = 0; i < count; ++i)
              SDL_RaiseWindow(windows[i]);
            SDL_free(windows);
            raised = true;
          }
          if (sized && elapsed > 10 &&
              host.windowState().title == toString(id) &&
              host.windowState().actualSize != math::Vec2i{960, 720}) {
            std::cerr << "Window manager changed the requested 960x720 "
                         "workload geometry\n";
            log.invalid = true;
            host.request({.type = AppCommandType::Quit});
          }
          if (benchmark) {
            if (interrupted && !interruptionSent &&
                host.windowState().title == toString(id)) {
              test::require(!state.sceneWork.uploads,
                            "interruption arrives before scene resources");
              SDL_Event event{};
              event.type = SDL_EVENT_WINDOW_FOCUS_LOST;
              SDL_PushEvent(&event);
              interruptionSent = true;
            }
            if (infinite && log.periodic >= 2)
              host.request({.type = AppCommandType::Quit});
            if (log.finished || (seconds && elapsed > seconds + 150)) {
              timedOut = !log.finished;
              host.request({.type = AppCommandType::Quit});
            }
          } else if (camera && elapsed > 30 && state.sceneWork.uploads &&
                     std::chrono::steady_clock::now() - stageAt > 250ms) {
            const bool captureStage = cameraStage == 0 || cameraStage == 3 ||
                                      cameraStage == 8 || cameraStage == 11 ||
                                      cameraStage == 13 || cameraStage == 16;
            if (captureStage && focusStage != cameraStage) {
              // Window-manager focus follows the real pointer on some desktops.
              // Establish native focus before injecting the engagement gesture.
              int count{};
              auto windows = SDL_GetWindows(&count);
              for (int i = 0; i < count; ++i) {
                SDL_WarpMouseInWindow(windows[i],
                                      host.windowState().actualSize.x / 2.f,
                                      host.windowState().actualSize.y / 2.f);
                SDL_RaiseWindow(windows[i]);
              }
              SDL_free(windows);
              focusStage = cameraStage;
              focusAt = std::chrono::steady_clock::now();
              return;
            }
            if (captureStage && !host.windowServices().focused()) {
              if (std::chrono::steady_clock::now() - focusAt > 5s) {
                std::cerr << "Native focus unavailable; desktop policy "
                             "prevents the capture workload\n";
                log.invalid = true;
                host.request({.type = AppCommandType::Quit});
              }
              return;
            }
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
              resumedMeshHits = state.sceneWork.meshHits;
              mouse();
              key(SDL_SCANCODE_W, true);
              break;
            case 4:
              require(state.sceneWork.meshHits > resumedMeshHits,
                      "held movement redraws the scene before Settings");
              host.requestSettings();
              break;
            case 5:
              require(host.settingsVisible() &&
                          !host.windowServices().relativeMouseActive(),
                      "settings releases mouse lock");
              key(SDL_SCANCODE_W, false);
              break;
            case 6:
              host.requestSettings(false);
              break;
            case 7:
              resumedMeshHits = state.sceneWork.meshHits;
              break;
            case 8:
              require(state.sceneWork.meshHits == resumedMeshHits,
                      "camera stays stationary after releasing movement in "
                      "Settings");
              mouse();
              break;
            case 9: {
              require(host.windowServices().relativeMouseActive(),
                      "mouse relocks after settings");
              SDL_Event e{};
              e.type = SDL_EVENT_WINDOW_FOCUS_LOST;
              SDL_PushEvent(&e);
              break;
            }
            case 10: {
              require(!host.windowServices().relativeMouseActive(),
                      "focus loss releases mouse lock");
              SDL_Event e{};
              e.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
              SDL_PushEvent(&e);
              break;
            }
            case 11:
              mouse();
              break;
            case 12:
              require(host.windowServices().relativeMouseActive(),
                      "mouse relocks after focus restoration");
              key(SDL_SCANCODE_F, true);
              key(SDL_SCANCODE_F, false);
              break;
            case 13:
              require(!host.windowServices().relativeMouseActive(),
                      "follow takeover revokes manual lease");
              mouse();
              key(SDL_SCANCODE_W, true);
              resumedMeshHits = state.sceneWork.meshHits;
              break;
            case 14: {
              require(host.windowServices().relativeMouseActive() &&
                          state.sceneWork.meshHits > resumedMeshHits,
                      "follow motor and camera redraw after engagement");
              key(SDL_SCANCODE_W, false);
              SDL_Event wheel{};
              wheel.type = SDL_EVENT_MOUSE_WHEEL;
              wheel.wheel.y = 20;
              SDL_PushEvent(&wheel);
              break;
            }
            case 15:
              key(SDL_SCANCODE_F, true);
              key(SDL_SCANCODE_F, false);
              break;
            case 16:
              require(!host.windowServices().relativeMouseActive(),
                      "return to free camera requires reengagement");
              mouse();
              break;
            case 17:
              host.request({.type = AppCommandType::ReturnToMenu});
              break;
            case 18:
              require(!host.windowServices().relativeMouseActive(),
                      "app exit releases mouse lock");
              completed = !log.invalid;
              std::cout << "CameraWorkflow checks=12 complete=" << completed
                        << '\n';
              host.request({.type = AppCommandType::Quit});
              break;
            }
            stageAt = std::chrono::steady_clock::now();
          } else if (!camera && elapsed > 30 && state.sceneWork.uploads) {
            if (!warm) {
              baseline = state.sceneWork;
              baselineSamples = state.cpuSamples;
              warm = true;
              measuredAt = elapsed;
              if (inspection) {
                SDL_Event event{};
                event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
                event.button.button = SDL_BUTTON_MIDDLE;
                event.button.down = true;
                event.button.x = host.windowState().actualSize.x / 2.f;
                event.button.y = host.windowState().actualSize.y / 2.f;
                SDL_PushEvent(&event);
              } else {
                if (!host.windowServices().focused()) {
                  std::cerr
                      << "Native focus unavailable for free-camera workload\n";
                  log.invalid = true;
                  host.request({.type = AppCommandType::Quit});
                  return;
                }
                SDL_Event event{};
                event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
                event.button.button = SDL_BUTTON_RIGHT;
                event.button.down = true;
                event.button.x = host.windowState().actualSize.x / 2.f;
                event.button.y = host.windowState().actualSize.y / 2.f;
                SDL_PushEvent(&event);
                event.type = SDL_EVENT_MOUSE_BUTTON_UP;
                event.button.down = false;
                SDL_PushEvent(&event);
                key(SDL_SCANCODE_RIGHT, true);
              }
            }
            if (inspection) {
              SDL_Event event{};
              event.type = SDL_EVENT_MOUSE_MOTION;
              event.motion.x = host.windowState().actualSize.x / 2.f +
                               100 * float(std::sin(elapsed - measuredAt));
              event.motion.y =
                  host.windowState().actualSize.y / 2.f +
                  40 * float(std::sin((elapsed - measuredAt) * .7));
              SDL_PushEvent(&event);
            }
            const auto telemetry = host.renderTelemetry();
            const auto available = std::min<std::uint64_t>(
                state.cpuSamples - baselineSamples, telemetry.cpu.size());
            for (std::size_t i = telemetry.cpu.size() - available;
                 i < telemetry.cpu.size(); ++i)
              if (telemetry.cpu[i].measured[2])
                renderTimes.push_back(telemetry.cpu[i].milliseconds[2]);
            baselineSamples = state.cpuSamples;
            if (seconds && elapsed - measuredAt >= seconds) {
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
              log.invalid = log.invalid ||
                            state.sceneWork.uploads != baseline.uploads ||
                            renderTimes.empty();
              completed = !log.invalid;
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
    if (!benchmark)
      test::require(completed,
                    "native workload completed its checks and measured frames");
    if (benchmark)
      test::require(log.finished, "benchmark emitted a final result");
    if (infinite)
      test::require(log.periodic >= 2 && log.finished,
                    "infinite benchmark loops, reports and cancels explicitly");
    test::require(!timedOut && !log.invalid,
                  "scene workload finishes without timeout or invalidation");
  });
}
