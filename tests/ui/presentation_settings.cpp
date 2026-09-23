#include <chrono>
#include <platform/Settings.hpp>
#include <support/Test.hpp>

using namespace playground;
class MemoryStore final : public platform::FileStore {
public:
  std::map<std::string, std::string, std::less<>> files;
  bool failWrite{};
  std::optional<std::string> read(std::string_view name) const override {
    const auto it = files.find(name);
    return it == files.end() ? std::nullopt : std::optional{it->second};
  }
  void replace(std::string_view name, std::string_view content) override {
    if (failWrite)
      throw std::runtime_error("simulated write failure");
    files.insert_or_assign(std::string{name}, std::string{content});
  }
};
int main() {
  return test::run([] {
    MemoryStore project, user;
    project.files["project.toml"] =
        "schema_version=1\n[defaults]\nui_scale=2.0\n[apps.demo]\ninitial_"
        "sizing='fit-content'\n";
    user.files["settings.toml"] =
        "schema_version=1\n[apps.demo]\nfollow_system_scale=false\nrefresh_"
        "rate=0\nui_scale=1.25\n";
    platform::SettingsStore store{project, user};
    store.reload();
    platform::PresentationProps props;
    platform::AppViewPolicy policy{.preferredWindowSize = math::Vec2i{900, 700},
                                   .resizable = false};
    store.resolve("demo", props, policy);
    test::require(
        props.viewport.scale == 1.25f && !props.viewport.followSystemScale &&
            !policy.resizable &&
            policy.preferredWindowSize == math::Vec2i{900, 700} &&
            policy.initialSizing == platform::InitialWindowSizing::FitContent,
        "per-app overrides preserve false/zero and cannot change resizability");
    const auto saved = platform::serializeSettings(store.user());
    platform::parseSettings(saved);
    platform::PresentationProps replacement;
    platform::AppViewPolicy replacementPolicy;
    store.resolveWithUser("demo", {}, replacement, replacementPolicy);
    test::require(replacement.viewport.scale == 2.0f &&
                      replacement.viewport.followSystemScale,
                  "removing user overrides resolves from project/app values, "
                  "not previous effective settings");
    user.files["settings.toml"] = "schema_version=2";
    test::rejects([&] { store.reload(); }, "unsupported schema rejected");
    test::require(platform::serializeSettings(store.user()) == saved,
                  "failed reload preserves published settings");
    for (const char *invalid : {"schema_version=1\n[defaults]\nui_scale=nan",
                                "schema_version=1\n[defaults]\nui_scale=0",
                                "schema_version=1\n[defaults]\nresizable=true",
                                "schema_version=1\n[defaults]\nmode='oops'",
                                "schema_version=1\n[defaults]\ncenter='false'"})
      test::rejects([&] { platform::parseSettings(invalid); },
                    "invalid settings rejected");
    user.failWrite = true;
    test::rejects<std::runtime_error>([&] { store.setUser({}, true); },
                                      "write failure propagates");
    test::require(platform::serializeSettings(store.user()) == saved,
                  "failed write does not publish candidate");
    user.failWrite = false;
    for (auto mode :
         {platform::WindowMode::Windowed, platform::WindowMode::Maximized,
          platform::WindowMode::DesktopFullscreen,
          platform::WindowMode::ExclusiveFullscreen,
          platform::WindowMode::BorderlessDisplay,
          platform::WindowMode::BorderlessWorkArea}) {
      platform::SettingsDocument modes;
      modes.defaults.mode = mode;
      test::require(platform::parseSettings(platform::serializeSettings(modes))
                            .defaults.mode == mode,
                    "every window mode survives serialization");
    }
    test::rejects<std::exception>(
        [] { platform::parseSettings("not toml ["); },
        "syntax errors do not masquerade as defaults");
    platform::PresentationProps unchanged;
    platform::AppViewPolicy unchangedPolicy;
    platform::SettingsDocument invalidName;
    invalidName.defaults.display = platform::DisplaySelection::Named;
    test::rejects(
        [&] {
          store.resolveWithUser("demo", invalidName, unchanged,
                                unchangedPolicy);
        },
        "named display requires a name after merge");
    test::require(unchanged == platform::PresentationProps{},
                  "failed resolution does not partially mutate output");
    platform::SessionState session{
        {"demo", {{640, 480}, {-800, 20}, "Secondary"}}};
    store.saveSession(session);
    test::require(platform::parseSession(*user.read("session.toml"))
                          .at("demo")
                          .position == math::Vec2i{-800, 20},
                  "negative desktop coordinates survive session roundtrip");

    // A unique test-owned directory, never the user's actual preference path.
    const auto directory =
        std::filesystem::current_path() /
        ("settings-test-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    test::require(std::filesystem::create_directory(directory),
                  "create isolated test directory");
    struct Cleanup {
      std::filesystem::path path;
      ~Cleanup() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
      }
    } cleanup{directory};
    platform::DirectoryStore disk{directory, true};
    test::require(!disk.read("settings.toml"),
                  "missing document is not an IO failure");
    disk.replace("settings.toml", saved);
    disk.replace("settings.toml", "schema_version=1\n");
    test::require(*disk.read("settings.toml") == "schema_version=1\n",
                  "replacement overwrites existing document");
    test::rejects([&] { disk.read("../settings.toml"); },
                  "path traversal rejected");
    test::rejects<std::length_error>(
        [&] {
          disk.replace("settings.toml", std::string(1024 * 1024 + 1, 'x'));
        },
        "oversized replacement rejected without changing destination");
    test::require(*disk.read("settings.toml") == "schema_version=1\n",
                  "failed replacement preserves previous file");
    platform::DirectoryStore readOnly{directory, false};
    test::rejects<std::logic_error>(
        [&] { readOnly.replace("settings.toml", ""); },
        "project store remains read-only");
  });
}
