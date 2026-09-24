#pragma once

#include <string>
#include <string_view>

#include <app/AppTypes.hpp>
#include <math/Geometry2D.hpp>
#include <platform/WindowTypes.hpp>
#include <rendering/RenderFailure.hpp>
#include <support/AssetRegistry.hpp>
#include <support/PerformanceMonitor.hpp>

class AppHost;

class AppContext {
  AppHost &_host;

public:
  explicit AppContext(AppHost &host) : _host{host} {}

  const WindowState &windowState() const;
  const AppWindowProps &windowProps() const;
  playground::platform::WindowMetrics windowMetrics() const;
  const playground::platform::PresentationProps &presentation() const;
  const playground::rendering::RendererState &rendererState() const;
  const playground::rendering::RecoveryState &rendererRecovery() const;
  const std::string &lastCommandError() const;
  const playground::platform::AppViewPolicy &viewPolicy() const;
  const playground::platform::SettingsDocument &userSettings() const;

  std::string assetPath(std::string_view relativePath) const;
  AssetRegistry &assets();
  PerformanceMonitor &performance();

  void requestSwitch(AppId appId);
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
