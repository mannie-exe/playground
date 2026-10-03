#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include <app/AppTypes.hpp>
#include <math/Geometry2D.hpp>
#include <platform/WindowTypes.hpp>
#include <platform/sdl/AssetResources.hpp>
#include <rendering/RenderFailure.hpp>
#include <runtime/CompletionQueue.hpp>
#include <runtime/Executor.hpp>
#include <runtime/Services.hpp>
#include <runtime/SimulationClock.hpp>
#include <support/AssetRegistry.hpp>
#include <support/PerformanceMonitor.hpp>

class AppHost;
class IApp;

namespace playground::sdl {
class WindowServices;
}

class AppContext {
  AppHost &_host;
  IApp *_owner{};

public:
  // Owner-thread requests. Workers post completions; they do not mutate app
  // state.
  void requestUpdate();
  void requestRepaint();
  std::function<void()> wakeCallback() const;

  explicit AppContext(AppHost &host) : _host{host} {}

  AppContext(AppHost &host, IApp &owner) : _host{host}, _owner{&owner} {}

  const WindowState &windowState() const;
  const WindowRequestStatus &windowRequestStatus() const;
  playground::platform::WindowPlacementCapabilities
  windowPlacementCapabilities() const;
  playground::sdl::WindowServices &windowServices();
  const AppWindowProps &windowProps() const;
  playground::platform::WindowMetrics windowMetrics() const;
  const playground::platform::PresentationProps &presentation() const;
  const playground::rendering::RendererState &rendererState() const;
  const playground::rendering::RecoveryState &rendererRecovery() const;
  const std::string &lastCommandError() const;
  const playground::platform::AppViewPolicy &viewPolicy() const;
  const playground::platform::SettingsDocument &userSettings() const;

  AssetRegistry &assets();
  playground::sdl::AssetResources &resources();
  playground::runtime::Executor &workers();
  playground::runtime::ServiceScope &services();
  PerformanceMonitor &performance();
  const playground::rendering::ResolvedGraphicsState &graphicsState() const;
  void requestGraphics(playground::rendering::GraphicsSettings,
                       bool persist = false);
  playground::input::ControlsState controlsState() const;
  void requestControls(playground::input::ControlsSettings,
                       bool persist = false);
  void requestSettings(bool visible = true);
  playground::rendering::RenderRuntimeSnapshot renderRuntimeState() const;
  playground::rendering::RenderTelemetrySnapshot renderTelemetry() const;
  void requestRenderRuntime(playground::rendering::RenderRuntimePatch patch);
  playground::runtime::ActivationSink completions();
  playground::runtime::ActivationToken activationToken() const;
  std::optional<playground::runtime::SimulationState> simulationState() const;
  void setSimulationPaused(bool);

  void requestSwitch(AppId appId, AppLaunchProps launch = {});
  void requestMenu();
  void requestQuit();
  void requestWindowProps(AppWindowProps config);
  void requestViewPolicy(playground::platform::AppViewPolicy policy);
  void requestFitContent();
  void requestPresentation(playground::platform::PresentationProps props);
  void requestReloadSettings();
  void requestRendererRecovery();
  void requestUserSettings(playground::platform::SettingsDocument settings,
                           bool persist = false);
};
