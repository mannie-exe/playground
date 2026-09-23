#pragma once

#include <math/Color.hpp>
#include <math/Geometry2D.hpp>

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <SDL3/SDL_events.h>
#include <app/AppRegistry.hpp>
#include <app/AppTypes.hpp>

#include <app/SDLGuard.hpp>
#include <app/TTFGuard.hpp>

#include <support/AssetPath.hpp>
#include <support/AssetRegistry.hpp>
#include <support/PerformanceMonitor.hpp>

#include <platform/Window.hpp>
#include <rendering/RenderBackend.hpp>

class AppHost {
  SDLGuard _sdl;
  TTFGuard _ttf;
  playground::platform::DirectoryStore _projectFiles;
  playground::platform::DirectoryStore _userFiles;
  playground::platform::SettingsStore _settings;

  AssetRegistry _assets;
  PerformanceMonitor _performance;

  Window _window;
  std::unique_ptr<playground::rendering::RenderBackend> _renderer;
  const playground::math::Vec2i _bootstrapSize;
  AppRegistry _registry;

  bool _running{true};
  std::optional<PendingAppCommand> _pendingCommand;
  std::unique_ptr<IApp> _activeApp;
  AppId _activeAppId{AppId::Menu};
  AppWindowProps _windowProps;
  playground::platform::AppViewPolicy _viewPolicy;
  playground::platform::PresentationProps _presentation;

public:
  explicit AppHost(WindowConfig initialWindow = WindowConfig{});

  const WindowState &windowState() const { return _window.state(); }
  playground::platform::WindowMetrics windowMetrics() const {
    return _window.metrics();
  }
  const playground::platform::PresentationProps &presentation() const {
    return _presentation;
  }
  const playground::platform::AppViewPolicy &viewPolicy() const {
    return _viewPolicy;
  }
  const playground::platform::SettingsDocument &userSettings() const {
    return _settings.user();
  }

  const AppWindowProps &windowProps() const { return _windowProps; }

  std::string assetPath(std::string_view relativePath) const {
    return playground::assets::path(relativePath);
  }

  AssetRegistry &assets() { return _assets; }
  PerformanceMonitor &performance() { return _performance; }

  void request(PendingAppCommand command);

  int run();

private:
  void switchTo(AppId appId);

  void registerDefaultApps();

  bool handleHostEvent(const SDL_Event &event);

  void processPendingCommand();

  void applyWindowProps(const AppWindowProps &config);
  void prepareWindowForSizing();
  void applyViewSizing();
  void fitContent();
  void applyPresentation(bool preservePosition = false);
  void resolveSettings();
  void saveWindowSession();
};
