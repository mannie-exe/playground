#include <filesystem>

#include <platform/sdl/ProcessEnvironment.hpp>
#include <support/CommandLine.hpp>
#include <support/HostPreferences.hpp>
#include <support/Test.hpp>

using namespace playground;

int main() {
  return test::run([] {
#ifdef __APPLE__
    test::require(SDL_setenv_unsafe("MVK_CONFIG_LOG_LEVEL", "0", 1) == 0,
                  "set explicit logging override");
    sdl::configureProcessEnvironment();
    test::require(std::string_view{SDL_getenv_unsafe("MVK_CONFIG_LOG_LEVEL")} ==
                      "0",
                  "startup preserves explicit logging override");
    test::require(SDL_unsetenv_unsafe("MVK_CONFIG_LOG_LEVEL") == 0,
                  "clear process-local logging override");
    sdl::configureProcessEnvironment();
    test::require(std::string_view{SDL_getenv_unsafe("MVK_CONFIG_LOG_LEVEL")} ==
                      "2",
                  "startup provides default logging level");
#endif
    for (const auto token :
         {"", "nan", "inf", "-1", "3601", "1s", " 1", "1 ", "1e999", "--help"})
      test::require(!test::cli::number(token, 0, 3600),
                    "duration rejects invalid or partial tokens");
    test::require(test::cli::number("0", 0, 3600) == 0 &&
                      test::cli::number("3600", 0, 3600) == 3600 &&
                      test::cli::number("1.5e1", 0, 3600) == 15,
                  "duration accepts boundaries and fractional exponents");
    rendering::GraphicsSettings graphics;
    graphics.renderer.backend = rendering::RendererChoice::Software;
    graphics.presentation.vsync = false;
    graphics.pacing.maximumFramesPerSecond = 30;
    graphics.motion = runtime::MotionPreference::Full;
    std::filesystem::path root;
    {
      test::HostPreferences user{"playground-preferences-test", graphics};
      root = user.path();
      platform::DirectoryStore files{root, false};
      const auto text = files.read("settings.toml");
      test::require(text && platform::parseSettings(*text).graphics == graphics,
                    "native preferences roundtrip production settings");
      test::HostPreferences second{"playground-preferences-test", {}};
      test::require(second.path() != root,
                    "simultaneous hosts have isolated preferences");
    }
    test::require(!std::filesystem::exists(root),
                  "preferences are removed after host lifetime");
    graphics.pacing.maximumFramesPerSecond = 0;
    test::rejects(
        [&] {
          test::HostPreferences invalid{"playground-preferences-test",
                                        graphics};
        },
        "invalid workload settings fail before host launch");
  });
}
