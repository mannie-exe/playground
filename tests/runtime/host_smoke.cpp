#include <chrono>
#include <optional>
#include <string_view>
#include <thread>

#include <app/AppHost.hpp>
#include <app/SDLGuard.hpp>
#include <demo3d/Demo3DApp.hpp>
#include <support/HostPreferences.hpp>
#include <support/Test.hpp>

using namespace playground;
using namespace std::chrono_literals;

int main() {
  std::optional<SDLGuard> video;
  try {
    video.emplace(SDL_INIT_VIDEO);
  } catch (const std::runtime_error &error) {
    std::cerr << "Skipping native host test: " << error.what() << '\n';
    return 77;
  }
  const std::string_view driverName = SDL_GetCurrentVideoDriver();
  if (driverName == "dummy" || driverName == "offscreen")
    return 77;
  return test::run([] {
    for (const auto kind : {demo3d::DemoKind::Material, demo3d::DemoKind::Chess,
                            demo3d::DemoKind::Bistro}) {
      demo3d::Demo3DApp app{kind};
      auto &map = app.input();
      const auto press = [&](int key) {
        input::routeInputEvent(map, {{input::ControlKind::Key, key}, 1}, false,
                               [] { return false; });
        const auto snapshot = map.takeFrameSnapshot();
        input::routeInputEvent(map, {{input::ControlKind::Key, key}, 0}, false,
                               [] { return false; });
        map.takeFrameSnapshot();
        return snapshot;
      };
      const auto space = press(SDL_SCANCODE_SPACE);
      test::require(space["rise"].pressed && !space["pause"].pressed &&
                        press(SDL_SCANCODE_LSHIFT)["fall"].pressed &&
                        !press(SDL_SCANCODE_PAGEUP)["rise"].pressed &&
                        !press(SDL_SCANCODE_PAGEDOWN)["fall"].pressed &&
                        press(SDL_SCANCODE_P)["pause"].pressed,
                    "demo elevation and playback bindings do not conflict");
    }
    test::HostPreferences user{
        "playground-host-test",
        {.renderer = {rendering::RendererChoice::Software}}};
    ui::SettingsView *settings{};
    unsigned serviceCalls{}, settingsServiceCalls{};
    bool serviceCancelled{};
    auto serviceWakeAt = runtime::ActivityClock::now();
    AppHost host{{},
                 {},
                 [&](auto &assets, auto font, auto graphics, auto actions) {
                   auto view =
                       ui::makeSettingsView(assets, font, graphics, actions);
                   settings = view.get();
                   return view;
                 },
                 {.project = PLAYGROUND_SOURCE_DIR, .user = user.path()}};
    auto sink = host.completions();
    const auto service = host.services().add(
        {.name = "host service smoke",
         .demand =
             [&] { return runtime::ServiceDemand{.wakeAt = serviceWakeAt}; },
         .advance =
             [&](auto now, auto) {
               ++serviceCalls;
               if (host.settingsVisible())
                 ++settingsServiceCalls;
               serviceWakeAt = now + 5ms;
               return runtime::ServiceWork{1};
             },
         .cancel = [&] { serviceCancelled = true; }});
    unsigned stage{};
    math::Vec2i original;
    std::exception_ptr failure;
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    std::jthread driver{[&](std::stop_token stop) {
      while (!stop.stop_requested()) {
        sink.post([&] {
          try {
            test::require(std::chrono::steady_clock::now() < deadline,
                          "host settings workflow completes before deadline");
            if (host.windowRequestStatus().outcome ==
                platform::WindowTransitionOutcome::Pending)
              return;
            switch (stage) {
            case 0:
              original = host.windowState().actualSize;
              host.requestSettings();
              ++stage;
              break;
            case 1:
              if (!host.settingsVisible())
                return;
              test::require(host.windowState().actualSize == original &&
                                settings->bounds().w() > 0,
                            "settings preserve current native window geometry");
              {
                auto graphics = host.graphicsState().requested;
                graphics.threeD.shadows = rendering::QualityLevel::Ultra;
                graphics.colorScheme = ui::ColorSchemePreference::Dark;
                graphics.contrast = ui::ContrastPreference::Normal;
                graphics.motion = runtime::MotionPreference::None;
                graphics.textScale = 1.5f;
                host.requestGraphics(graphics, true);
              }
              ++stage;
              break;
            case 2: {
              if (!settingsServiceCalls)
                return;
              if (host.graphicsState().requested.threeD.shadows !=
                  rendering::QualityLevel::Ultra)
                return;
              if (settings->resolvedTheme().typography.textScale != 1.5f)
                return;
              test::require(
                  host.viewPolicy().colorScheme ==
                          ui::ColorSchemePreference::Dark &&
                      host.viewPolicy().userContrast ==
                          ui::ContrastPreference::Normal &&
                      !settings->theme().highContrast &&
                      settings->theme().surface ==
                          ui::resolveTheme(ui::ColorSchemePreference::Dark,
                                           ui::ContrastPreference::Normal, {})
                              .surface,
                  "live settings publish scheme, contrast and text size");
              platform::DirectoryStore disk{user.path(), false};
              const auto persisted =
                  platform::parseSettings(*disk.read("settings.toml"));
              test::require(
                  persisted.graphics == host.userSettings().graphics,
                  "saved appearance matches applied settings on disk");
              test::require(host.userSettings().graphics.has_value(),
                            "settings persist to isolated user storage");
              host.requestSettings(false);
              ++stage;
              break;
            }
            case 3:
              if (host.settingsVisible())
                return;
              test::require(host.viewPolicy().colorScheme ==
                                    ui::ColorSchemePreference::Dark &&
                                host.viewPolicy().userContrast ==
                                    ui::ContrastPreference::Normal,
                            "closing settings retains applied appearance");
              test::require(host.windowState().actualSize == original,
                            "closing settings restores app geometry");
              test::require(host.renderRuntimeState().submitted > 0,
                            "host presents demanded UI frames");
              ++stage;
              host.request({.type = AppCommandType::Quit});
              break;
            }
          } catch (...) {
            failure = std::current_exception();
            host.request({.type = AppCommandType::Quit});
          }
        });
        std::this_thread::sleep_for(20ms);
      }
    }};
    const auto result = host.run();
    driver.request_stop();
    driver.join();
    if (failure)
      std::rethrow_exception(failure);
    test::require(result == 0 && stage == 4 && host.lastCommandError().empty(),
                  "isolated host workflow succeeds");
    test::require(
        serviceCalls && settingsServiceCalls && serviceCancelled &&
            service.status() == runtime::ServiceStatus::Closed,
        "host services continue during Settings and close on app exit");
  });
}
