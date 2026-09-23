#include <app/AppContext.hpp>
#include <app/AppHost.hpp>

const WindowState &AppContext::windowState() const {
  return _host.windowState();
}

const AppWindowProps &AppContext::windowProps() const {
  return _host.windowProps();
}

std::string AppContext::assetPath(std::string_view relativePath) const {
  return _host.assetPath(relativePath);
}

AssetRegistry &AppContext::assets() { return _host.assets(); }

PerformanceMonitor &AppContext::performance() { return _host.performance(); }

void AppContext::requestSwitch(AppId appId) {
  _host.request(
      PendingAppCommand{.type = AppCommandType::SwitchTo, .target = appId});
}

void AppContext::requestMenu() {
  _host.request(PendingAppCommand{.type = AppCommandType::ReturnToMenu,
                                  .target = AppId::Menu});
}

void AppContext::requestQuit() {
  _host.request(PendingAppCommand{.type = AppCommandType::Quit});
}

void AppContext::requestWindowProps(AppWindowProps config) {
  _host.request(PendingAppCommand{.type = AppCommandType::SetWindowProps,
                                  .window = std::move(config)});
}
void AppContext::requestViewPolicy(playground::platform::AppViewPolicy policy) {
  policy.validate();
  _host.request(
      {.type = AppCommandType::SetViewPolicy, .view = std::move(policy)});
}

playground::platform::WindowMetrics AppContext::windowMetrics() const {
  return _host.windowMetrics();
}
const playground::platform::PresentationProps &
AppContext::presentation() const {
  return _host.presentation();
}
const playground::platform::AppViewPolicy &AppContext::viewPolicy() const {
  return _host.viewPolicy();
}
const playground::platform::SettingsDocument &AppContext::userSettings() const {
  return _host.userSettings();
}
void AppContext::requestFitContent() {
  _host.request({.type = AppCommandType::FitContent});
}
void AppContext::requestPresentation(
    playground::platform::PresentationProps props) {
  props.validate();
  _host.request({.type = AppCommandType::SetPresentation,
                 .presentation = std::move(props)});
}
void AppContext::requestReloadSettings() {
  _host.request({.type = AppCommandType::ReloadSettings});
}
void AppContext::requestUserSettings(
    playground::platform::SettingsDocument settings, bool persist) {
  _host.request({.type = AppCommandType::SetUserSettings,
                 .settings = std::move(settings),
                 .persist = persist});
}
