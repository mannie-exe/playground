#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <SDL3/SDL_events.h>

#include <app/AppRegistry.hpp>
#include <app/AppTypes.hpp>
#include <app/SDLGuard.hpp>
#include <app/TTFGuard.hpp>
#include <math/Color.hpp>
#include <math/Geometry2D.hpp>
#include <platform/Window.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderFailure.hpp>
#include <runtime/UpdateClock.hpp>
#include <support/AssetPath.hpp>
#include <support/AssetRegistry.hpp>
#include <support/PerformanceMonitor.hpp>

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
  playground::rendering::RendererState _rendererState;
  playground::rendering::RecoveryState _rendererRecovery;
  playground::rendering::ResourceDomainId _notifiedRendererDomain;
  playground::runtime::UpdateClock _updateClock;
  const playground::math::Vec2i _bootstrapSize;
  AppRegistry _registry;

  bool _running{true};
  std::optional<PendingAppCommand> _pendingCommand;
  std::string _lastCommandError;
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
  const playground::rendering::RendererState &rendererState() const {
    return _rendererState;
  }
  const playground::rendering::RecoveryState &
  rendererRecovery() const noexcept {
    return _rendererRecovery;
  }
  const std::string &lastCommandError() const noexcept {
    return _lastCommandError;
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
  struct RuntimeCheckpoint {
    AppId appId;
    AppWindowProps windowProps;
    playground::platform::AppViewPolicy view;
    playground::platform::PresentationProps presentation;
    playground::rendering::RendererState renderer;
    playground::rendering::RendererRequirements requirements;
    bool hasRenderer;
    WindowState window;
    std::optional<PendingAppCommand> pending;
  };
  RuntimeCheckpoint checkpoint();
  void restore(const RuntimeCheckpoint &);
  void recoverRenderer(std::string reason);
  void synchronizeRendererDomain();
  void cleanupApp(IApp &) noexcept;

  void switchTo(AppId appId);

  void registerDefaultApps();

  bool handleHostEvent(const SDL_Event &event);

  void processPendingCommand();
  void executeCommand(PendingAppCommand command);

  void applyWindowProps(const AppWindowProps &config);
  void prepareWindowForSizing();
  void applyViewSizing();
  void fitContent();
  void applyPresentation(bool preservePosition = false);
  void resolveSettings();
  playground::rendering::RendererState
      resolveRenderer(playground::rendering::RendererPreferences,
                      playground::rendering::RendererRequirements) const;
  playground::rendering::RendererState
      configureRenderer(playground::rendering::RendererPreferences,
                        playground::rendering::RendererRequirements);
  void saveWindowSession();
};
