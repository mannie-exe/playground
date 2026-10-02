#include <chrono>
#include <fstream>
#include <optional>
#include <string_view>
#include <thread>

#include <app/AppHost.hpp>
#include <app/SDLGuard.hpp>
#include <support/TemporaryDirectory.hpp>
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
    test::TemporaryDirectory user{"playground-host-test"};
    std::ofstream{user.path() / "settings.toml"}
        << "schema_version = 5\n[graphics]\nrenderer = 'software'\n";
    ui::SettingsView *settings{};
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
                host.requestGraphics(graphics, true);
              }
              ++stage;
              break;
            case 2:
              if (host.graphicsState().requested.threeD.shadows !=
                  rendering::QualityLevel::Ultra)
                return;
              test::require(host.userSettings().graphics.has_value(),
                            "settings persist to isolated user storage");
              host.requestSettings(false);
              ++stage;
              break;
            case 3:
              if (host.settingsVisible())
                return;
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
  });
}
