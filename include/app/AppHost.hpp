#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>

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
#include <platform/sdl/UISession.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderBackendProps.hpp>
#include <rendering/RenderFailure.hpp>
#include <runtime/Activity.hpp>
#include <runtime/Executor.hpp>
#include <runtime/UpdateClock.hpp>
#include <support/AssetRegistry.hpp>
#include <support/PerformanceMonitor.hpp>
#include <ui/views/SettingsView.hpp>

struct AppHostDirectories {
  std::optional<std::filesystem::path> project, user;
};

class AppHost {
  SDLGuard _sdl;
  TTFGuard _ttf;
  playground::sdl::EventWake _wake;
  playground::sdl::EventWake _serviceWake;
  playground::runtime::CompletionQueue _hostCompletions;
  playground::runtime::ServicePump _servicePump;
  playground::sdl::SDLGamepads _gamepads;
  playground::platform::DirectoryStore _projectFiles;
  playground::platform::DirectoryStore _userFiles;
  playground::platform::SettingsStore _settings;

  AssetRegistry _assets;
  std::shared_ptr<const playground::assets::AssetCatalog> _catalog;
  std::unique_ptr<playground::sdl::AssetResources> _resources;
  std::unique_ptr<playground::runtime::Executor> _workers;
  PerformanceMonitor _performance;
  playground::rendering::RenderRuntime _renderRuntime;
  std::optional<playground::rendering::RenderRuntimePatch> _pendingRuntimePatch;
  playground::rendering::QualityController _quality;
  void processSettings();
  void showSettings(bool visible);
  void applyGraphics(playground::rendering::GraphicsSettings, bool persist);
  std::string _reportedResourcePressure;
  std::uint64_t _lastCompletedWork{};
  std::optional<std::uint64_t> _pressureRetriedRevision;

  playground::app::PresentationSession _session;
  playground::sdl::UISession _settingsUI{
      playground::sdl::UISessionTiming::Monotonic};
  playground::ui::SettingsViewFactory _settingsViewFactory;
  playground::ui::SettingsPanel *_settingsView{};
  std::optional<playground::app::PresentationSession::Checkpoint>
      _settingsWindow;
  std::optional<bool> _pendingSettingsVisible;
  std::optional<std::pair<playground::rendering::GraphicsSettings, bool>>
      _pendingGraphics;
  playground::runtime::ActivityClock::time_point _settingsMetersAt{};
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
  std::uint64_t _reportedWindowRequest{};
  std::unique_ptr<IApp> _activeApp;
  AppId _activeAppId{AppId::Menu};

public:
  std::function<void()> wakeCallback() const { return _wake.callback(); }

  void requestUpdate() { _updateRequested = true; }

  void requestRepaint() { _paintRequest.request(); }

  explicit AppHost(WindowConfig initialWindow = WindowConfig{},
                   playground::rendering::RenderBackendProps backendProps = {},
                   playground::ui::SettingsViewFactory settingsView =
                       playground::ui::makeSettingsView,
                   AppHostDirectories directories = {});
  ~AppHost();

  const WindowState &windowState() const { return _session.windowState(); }

  const WindowRequestStatus &windowRequestStatus() const {
    return _session.windowRequestStatus();
  }

  auto windowPlacementCapabilities() const {
    return _session.windowPlacementCapabilities();
  }

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

  const auto &graphicsState() const noexcept { return _quality.state(); }

  playground::input::ControlsState controlsState() const {
    return {_settings.controls(), _activeApp->controlCapabilities(),
            _activeApp->controlRestriction()};
  }

  bool settingsVisible() const noexcept { return _settingsView != nullptr; }

  void requestSettings(bool visible = true) {
    _pendingSettingsVisible = visible;
    requestUpdate();
  }

  void requestGraphics(playground::rendering::GraphicsSettings settings,
                       bool persist = false) {
    settings.validate();
    _pendingGraphics = std::pair{std::move(settings), persist};
    requestUpdate();
  }

  auto renderRuntimeState() const { return _renderRuntime.snapshot(); }

  auto renderTelemetry() const { return _renderRuntime.telemetrySnapshot(); }

  void requestRenderRuntime(playground::rendering::RenderRuntimePatch patch) {
    if (patch.budgets)
      patch.budgets->validate();
    if (patch.pacing)
      patch.pacing->validate();
    if (!_pendingRuntimePatch)
      _pendingRuntimePatch.emplace();
    if (patch.budgets)
      _pendingRuntimePatch->budgets = patch.budgets;
    if (patch.pacing)
      _pendingRuntimePatch->pacing = patch.pacing;
    requestRepaint();
  }

  // Host integrations survive app switches; app work uses completions().
  playground::runtime::CompletionSink hostCompletions() const {
    return _hostCompletions.sink();
  }

  playground::runtime::ActivationSink completions() {
    return {_activeApp->_completions.sink(), _activeApp->activationToken()};
  }

  playground::runtime::ServiceScope &services() {
    if (!_activeApp)
      throw std::logic_error("No active application service scope");
    return _activeApp->services();
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
  void collectRendererTelemetry();
  void synchronizeRendererDomain();
  void cleanupApp(IApp &) noexcept;

  void switchTo(AppId appId, AppLaunchProps launch = {});

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
  void advanceWindowTransition(
      playground::platform::WindowTransition::TimePoint now);
};
