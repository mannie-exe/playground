#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>

#include <platform/FileStore.hpp>
#include <platform/Presentation.hpp>

namespace playground::platform {

// Missing means inherit. Reset a persisted override by removing the field;
// false/zero are values, never absence. These patches cannot override
// resizability.
struct SettingsPatch {
  std::optional<WindowMode> mode;
  std::optional<DisplaySelection> display;
  std::optional<std::string> displayName;
  std::optional<bool> decorated, center;
  std::optional<math::Vec2i> exclusiveSize;
  std::optional<float> refreshRate;
  std::optional<InitialWindowSizing> initialSizing;
  std::optional<ViewportMode> viewportMode;
  std::optional<ViewportFit> viewportFit;
  std::optional<math::Size2> canvasSize;
  std::optional<math::Vec2f> alignment;
  std::optional<float> uiScale;
  std::optional<bool> followSystemScale;
  std::optional<float> resolutionScale;
  std::optional<bool> glyphAtlases, vsync;
  std::optional<rendering::RendererChoice> renderer;
  std::optional<rendering::GPUDriver> gpuDriver;
  std::optional<bool> rendererFallback;
  void apply(PresentationProps &, AppViewPolicy &) const;
};
struct SettingsDocument {
  SettingsPatch defaults;
  std::map<std::string, SettingsPatch, std::less<>> apps;
  void apply(std::string_view app, PresentationProps &, AppViewPolicy &) const;
};
struct SavedWindow {
  math::Vec2i size;
  math::Vec2i position;
  std::string displayName;
};
using SessionState = std::map<std::string, SavedWindow, std::less<>>;

struct SettingsSnapshot {
  SettingsDocument project, user;
  SessionState session;
};

SettingsDocument parseSettings(std::string_view utf8);
std::string serializeSettings(const SettingsDocument &);
SessionState parseSession(std::string_view utf8);
std::string serializeSession(const SessionState &);

// Stores are borrowed; caller owns their lifetime. Synchronous owner-thread
// API. Parse/write failures preserve the published in-memory document.
class SettingsStore {
  FileStore &_projectFiles;
  FileStore &_userFiles;
  SettingsDocument _project, _user;
  SessionState _session;

public:
  SettingsStore(FileStore &project, FileStore &user)
      : _projectFiles{project}, _userFiles{user} {}
  void reload();
  SettingsSnapshot readSnapshot() const;
  SettingsSnapshot snapshot() const { return {_project, _user, _session}; }
  void publish(SettingsSnapshot snapshot) noexcept;
  void setUser(SettingsDocument document, bool persist);
  void saveSession(SessionState state);
  const SettingsDocument &user() const noexcept { return _user; }
  const SessionState &session() const noexcept { return _session; }
  void resolve(std::string_view app, PresentationProps &,
               AppViewPolicy &) const;
  void resolveWithUser(std::string_view app, const SettingsDocument &user,
                       PresentationProps &, AppViewPolicy &) const;
};
} // namespace playground::platform
