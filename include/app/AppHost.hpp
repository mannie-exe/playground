#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <SDL3/SDL_events.h>

#include <app/AppRegistry.hpp>
#include <app/AppTypes.hpp>
#include <app/PresentationSession.hpp>
#include <app/SDLGuard.hpp>
#include <app/TTFGuard.hpp>
#include <math/Color.hpp>
#include <math/Geometry2D.hpp>
#include <platform/Window.hpp>
#include <platform/sdl/AssetResources.hpp>
#include <platform/sdl/EventWake.hpp>
#include <platform/sdl/SDLGamepads.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderBackendProps.hpp>
#include <rendering/RenderFailure.hpp>
#include <runtime/Activity.hpp>
#include <runtime/Executor.hpp>
#include <runtime/UpdateClock.hpp>
#include <support/AssetRegistry.hpp>
#include <support/PerformanceMonitor.hpp>

class AppHost {
  SDLGuard _sdl;
  TTFGuard _ttf;
  playground::sdl::EventWake _wake;
  playground::sdl::SDLGamepads _gamepads;
  playground::platform::DirectoryStore _projectFiles;
  playground::platform::DirectoryStore _userFiles;
  playground::platform::SettingsStore _settings;

  AssetRegistry _assets;
  std::shared_ptr<const playground::assets::AssetCatalog> _catalog;
  std::unique_ptr<playground::sdl::AssetResources> _resources;
  std::unique_ptr<playground::runtime::Executor> _workers;
  PerformanceMonitor _performance;

  playground::app::PresentationSession _session;
  playground::rendering::ResourceDomainId _notifiedRendererDomain;
  playground::runtime::UpdateClock _updateClock;
  AppRegistry _registry;

  bool _running{true};
  bool _commandsSuppressed{};
  bool _inputFocused{true};
  bool _updateRequested{true};
  playground::runtime::PaintRequest _paintRequest;
  std::optional<PendingAppCommand> _pendingCommand;
  std::string _lastCommandError;
  std::unique_ptr<IApp> _activeApp;
  AppId _activeAppId{AppId::Menu};

public:
  std::function<void()> wakeCallback() const { return _wake.callback(); }

  void requestUpdate() { _updateRequested = true; }

  void requestRepaint() { _paintRequest.request(); }

  explicit AppHost(WindowConfig initialWindow = WindowConfig{},
                   playground::rendering::RenderBackendProps backendProps = {});

  const WindowState &windowState() const { return _session.windowState(); }

  playground::sdl::WindowServices &windowServices() {
    return _session.windowServices();
  }

  playground::platform::WindowMetrics windowMetrics() const {
    return _session.windowMetrics();
  }

  const playground::platform::PresentationProps &presentation() const {
    return _session.presentation();
  }

  const playground::rendering::RendererState &rendererState() const {
    return _session.rendererState();
  }

  const playground::rendering::RecoveryState &
  rendererRecovery() const noexcept {
    return _session.recovery();
  }

  const std::string &lastCommandError() const noexcept {
    return _lastCommandError;
  }

  const playground::platform::AppViewPolicy &viewPolicy() const {
    return _session.viewPolicy();
  }

  const playground::platform::SettingsDocument &userSettings() const {
    return _settings.user();
  }

  const AppWindowProps &windowProps() const { return _session.windowProps(); }

  AssetRegistry &assets() { return _assets; }

  playground::sdl::AssetResources &resources() { return *_resources; }

  playground::runtime::Executor &workers() {
    if (!_workers)
      _workers = std::make_unique<playground::runtime::Executor>();
    return *_workers;
  }

  PerformanceMonitor &performance() { return _performance; }

  playground::runtime::ActivationSink completions() {
    return {_activeApp->_completions.sink(), _activeApp->activationToken()};
  }

  playground::runtime::ActivationToken activationToken() const {
    return _activeApp->activationToken();
  }

  std::optional<playground::runtime::SimulationState> simulationState() const {
    if (_activeApp && _activeApp->_simulation)
      return _activeApp->_simulation->state();
    return {};
  }

  void setSimulationPaused(bool);

  void request(PendingAppCommand command);

  int run();

private:
  struct RuntimeCheckpoint {
    AppId appId;
    playground::app::PresentationSession::Checkpoint presentation;
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
  void synchronizeInputClaims(AppContext &);

  void processPendingCommand();
  void executeCommand(PendingAppCommand command);

  void applyViewSizing();
  void resolveSettings();
  void configureRenderer(playground::rendering::RendererPreferences,
                         playground::rendering::RendererRequirements);
  void saveWindowSession();
};
