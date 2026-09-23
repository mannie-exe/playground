#include <app/AppHost.hpp>

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_timer.h>
#include <algorithm>
#include <app/AppConfig.hpp>
#include <app/AppContext.hpp>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

#include <demo/DemoApp.hpp>
#include <menu/MenuApp.hpp>
#include <minesweeper/MinesweeperApp.hpp>
#include <platform/sdl/SurfaceRenderBackend.hpp>
#include <snake/SnakeApp.hpp>

AppHost::AppHost(WindowConfig initialWindow)
    : _sdl{SDL_INIT_VIDEO}, _ttf{},
      _projectFiles{playground::platform::executableDirectory(), false},
      _userFiles{
          playground::platform::preferenceDirectory("Playground", "Playground"),
          true},
      _settings{_projectFiles, _userFiles}, _window{initialWindow},
      _renderer{std::make_unique<playground::sdl::SurfaceRenderBackend>(
          *_window.get())},
      _bootstrapSize{initialWindow.windowedSize} {
  registerDefaultApps();
  _settings.reload();
  switchTo(AppId::Menu);
}

void AppHost::request(PendingAppCommand command) {
  if (command.type == AppCommandType::None)
    return;

  if (command.type == AppCommandType::Quit || !_pendingCommand ||
      _pendingCommand->type != AppCommandType::Quit) {
    _pendingCommand = std::move(command);
  }
}

void AppHost::switchTo(AppId appId) {
  AppContext ctx{*this};

  if (_activeApp) {
    saveWindowSession();
    _activeApp->onExit(ctx);
  }

  std::unique_ptr<IApp> nextApp{_registry.create(appId)};
  const AppInfo nextInfo{nextApp->info()};

  _activeApp = std::move(nextApp);
  _activeAppId = appId;

  _assets.trim(playground::config::maxCachedFonts,
               playground::config::maxCachedImages,
               playground::config::maxCachedVectors);
  _assets.trimSurfaceBytes(playground::config::maxCachedSurfaceBytes);

  _windowProps = nextInfo.window;
  resolveSettings();
  prepareWindowForSizing();
  applyWindowProps(_windowProps);
  _activeApp->onEnter(ctx);
  applyViewSizing();
}

void AppHost::prepareWindowForSizing() {
  auto normal = _presentation.window;
  normal.mode = playground::platform::WindowMode::Windowed;
  _window.setMinimumSize({1, 1});
  _window.applyPreferences(normal);
  _window.setWindowedSize(_viewPolicy.initialWindowSize(_bootstrapSize));
}

void AppHost::applyViewSizing() {
  using playground::platform::InitialWindowSizing;
  bool restored = false;
  if (_viewPolicy.initialSizing == InitialWindowSizing::FitContent)
    fitContent();
  else if (_viewPolicy.initialSizing == InitialWindowSizing::RestorePrevious) {
    if (const auto found = _settings.session().find(appKey(_activeAppId));
        found != _settings.session().end()) {
      auto preference = _presentation.window.display;
      if (preference.selection ==
              playground::platform::DisplaySelection::Current &&
          !found->second.displayName.empty())
        preference = {playground::platform::DisplaySelection::Named,
                      found->second.displayName};
      const auto area = _window.usableBounds(preference);
      const playground::math::Vec2i size{
          std::min(found->second.size.x, static_cast<int>(area.w())),
          std::min(found->second.size.y, static_cast<int>(area.h()))};
      _window.setWindowedSize(size);
      _window.setWindowedPosition(
          {std::clamp(found->second.position.x, static_cast<int>(area.x()),
                      static_cast<int>(area.right()) - size.x),
           std::clamp(found->second.position.y, static_cast<int>(area.y()),
                      static_cast<int>(area.bottom()) - size.y)});
      restored = true;
    }
  }
  applyPresentation(restored);
}

int AppHost::run() {
  SDL_Event event;
  std::uint64_t previousCounter = SDL_GetPerformanceCounter();
  const std::uint64_t counterFrequency = SDL_GetPerformanceFrequency();

  while (_running) {
    AppContext ctx{*this};

    _performance.beginFrame();
    _performance.begin(FramePhase::Poll);

    while (SDL_PollEvent(&event)) {
      const bool hostHandled{handleHostEvent(event)};

      if (!_running)
        break;

      if (!hostHandled && _activeApp)
        _activeApp->handleEvent(ctx, event);

      processPendingCommand();

      if (!_running)
        break;
    }

    _performance.end(FramePhase::Poll);

    if (!_running)
      break;

    _performance.begin(FramePhase::Update);
    const std::uint64_t currentCounter = SDL_GetPerformanceCounter();
    const float deltaSeconds =
        static_cast<float>(currentCounter - previousCounter) /
        static_cast<float>(counterFrequency);
    previousCounter = currentCounter;

    if (_activeApp)
      _activeApp->update(ctx, deltaSeconds);

    processPendingCommand();

    _performance.end(FramePhase::Update);

    if (!_running)
      break;

    _performance.begin(FramePhase::Render);
    auto frame = _renderer->beginFrame({.clearColor = _windowProps.clearColor,
                                        .settings = _presentation.render});
    if (frame && _activeApp)
      _activeApp->render(ctx, *frame);

    _performance.end(FramePhase::Render);

    _performance.begin(FramePhase::Present);
    if (frame)
      frame->present();
    frame.reset();
    _performance.end(FramePhase::Present);
    _performance.endFrame();
    processPendingCommand();
  }

  AppContext ctx{*this};
  saveWindowSession();
  if (_activeApp)
    _activeApp->onExit(ctx);

  return 0;
}

void AppHost::registerDefaultApps() {
  _registry.add(MenuApp::staticInfo(),
                [] { return std::make_unique<MenuApp>(); });
  _registry.add(DemoApp::staticInfo(),
                [] { return std::make_unique<DemoApp>(); });
  _registry.add(MinesweeperApp::staticInfo(),
                [] { return std::make_unique<MinesweeperApp>(); });
  _registry.add(SnakeApp::staticInfo(),
                [] { return std::make_unique<SnakeApp>(); });
}

bool AppHost::handleHostEvent(const SDL_Event &event) {
  if (event.type == SDL_EVENT_KEY_DOWN && _performance.handleHotkey(event.key))
    return true;

  _window.handleEvent(event);

  if (event.type == SDL_EVENT_QUIT)
    request(PendingAppCommand{.type = AppCommandType::Quit});

  return event.type == SDL_EVENT_QUIT;
}

void AppHost::processPendingCommand() {
  if (!_pendingCommand)
    return;

  PendingAppCommand command = std::move(*_pendingCommand);
  _pendingCommand.reset();

  switch (command.type) {
  case AppCommandType::None:
    return;
  case AppCommandType::Quit:
    _running = false;
    return;
  case AppCommandType::SwitchTo:
    switchTo(command.target);
    return;
  case AppCommandType::ReturnToMenu:
    switchTo(AppId::Menu);
    return;
  case AppCommandType::SetWindowProps:
    if (command.window) {
      applyWindowProps(*command.window);
      _windowProps = std::move(*command.window);
    }
    return;
  case AppCommandType::SetViewPolicy:
    if (command.view) {
      command.view->validate();
      _viewPolicy = *command.view;
      prepareWindowForSizing();
      applyViewSizing();
    }
    return;
  case AppCommandType::FitContent:
    if (_presentation.window.mode == playground::platform::WindowMode::Windowed)
      fitContent();
    return;
  case AppCommandType::SetPresentation:
    if (command.presentation) {
      _presentation = *command.presentation;
      applyPresentation();
    }
    return;
  case AppCommandType::ReloadSettings:
    _settings.reload();
    resolveSettings();
    applyPresentation();
    return;
  case AppCommandType::SetUserSettings:
    if (command.settings) {
      const auto info = _activeApp->info();
      auto candidate = info.presentation;
      auto policy = info.view;
      _settings.resolveWithUser(appKey(_activeAppId), *command.settings,
                                candidate, policy);
      _settings.setUser(std::move(*command.settings), command.persist);
      resolveSettings();
      applyPresentation();
    }
    return;
  }
}

void AppHost::applyWindowProps(const AppWindowProps &config) {
  _window.setTitle(config.title);
  _window.setAlwaysOnTop(config.alwaysOnTop);
  _window.setFocusable(config.focusable);
  _window.setMouseGrabbed(config.mouseGrabbed);
  _window.setHidden(config.hidden);
}

void AppHost::resolveSettings() {
  const auto info = _activeApp->info();
  auto presentation = info.presentation;
  auto policy = info.view;
  _settings.resolve(appKey(_activeAppId), presentation, policy);
  _presentation = std::move(presentation);
  _viewPolicy = policy;
}

void AppHost::applyPresentation(bool preservePosition) {
  _presentation.validate();
  _viewPolicy.validate();
  auto preferences = _presentation.window;
  if (preservePosition) {
    preferences.center = false;
    preferences.display.selection =
        playground::platform::DisplaySelection::Current;
  }
  _window.applyPreferences(preferences);
  const auto factor = playground::platform::uiWindowScale(
      _presentation.viewport, _window.metrics());
  const auto minimum = _viewPolicy.minimumSize;
  const auto coordinate = [](double value) {
    if (!std::isfinite(value) || value > std::numeric_limits<int>::max())
      throw std::overflow_error("Window minimum size overflow");
    return std::max(1, static_cast<int>(std::ceil(value)));
  };
  const auto area = _window.usableBounds(preferences.display);
  _window.setMinimumSize(
      {coordinate(std::min(double(minimum.width) * factor.x, double(area.w()))),
       coordinate(
           std::min(double(minimum.height) * factor.y, double(area.h())))});
  _window.setResizable(_viewPolicy.resizable);
}

void AppHost::fitContent() {
  auto normal = _presentation.window;
  normal.mode = playground::platform::WindowMode::Windowed;
  _window.applyPreferences(normal);
  const auto area = _window.usableBounds(_presentation.window.display);
  const auto metrics = _window.metrics();
  const auto factor =
      playground::platform::uiWindowScale(_presentation.viewport, metrics);
  int top{}, left{}, bottom{}, right{};
  SDL_GetWindowBordersSize(_window.get(), &top, &left, &bottom, &right);
  const playground::math::Size2 available{
      std::max(1.0f, area.w() - left - right),
      std::max(1.0f, area.h() - top - bottom)};
  const playground::math::Size2 maximum{available.width / factor.x,
                                        available.height / factor.y};
  const auto pixelScale =
      playground::platform::resolveViewport(_presentation.viewport, metrics)
          .pixelsPerLogical;
  const auto preferred =
      _presentation.viewport.mode ==
              playground::platform::ViewportMode::FixedCanvas
          ? std::optional{_presentation.viewport.canvasSize}
          : _activeApp->preferredContentSize(maximum, pixelScale);
  if (!preferred)
    return;
  if (!playground::math::isFinite(*preferred) ||
      !playground::math::isNonNegative(*preferred))
    throw std::invalid_argument(
        "App returned an invalid preferred content size");
  _window.setWindowedSize(
      {static_cast<int>(std::clamp(
           std::ceil(double(std::max(preferred->width,
                                     _viewPolicy.minimumSize.width)) *
                     factor.x),
           1.0, double(available.width))),
       static_cast<int>(std::clamp(
           std::ceil(double(std::max(preferred->height,
                                     _viewPolicy.minimumSize.height)) *
                     factor.y),
           1.0, double(available.height)))});
  if (_presentation.window.center) {
    _window.applyPreferences(normal);
  }
}

void AppHost::saveWindowSession() {
  if (!_activeApp)
    return;
  _window.refreshState();
  const auto &state = _window.state();
  auto session = _settings.session();
  const char *name = SDL_GetDisplayName(state.display);
  session.insert_or_assign(
      std::string{appKey(_activeAppId)},
      playground::platform::SavedWindow{
          state.windowedSize, state.windowedPosition, name ? name : ""});
  try {
    _settings.saveSession(std::move(session));
  } catch (const std::exception &error) {
    SDL_Log("Cannot save window session: %s", error.what());
  }
}
