#pragma once

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>

#include <study_sdl3/app/AppConfig.hpp>
#include <study_sdl3/app/AppContext.hpp>
#include <study_sdl3/app/AppRegistry.hpp>
#include <study_sdl3/app/AppTypes.hpp>

#include <study_sdl3/app/SDLGuard.hpp>
#include <study_sdl3/app/TTFGuard.hpp>

#include <study_sdl3/support/AssetPath.hpp>
#include <study_sdl3/support/PerformanceMonitor.hpp>
#include <study_sdl3/support/SDLError.hpp>

#include <study_sdl3/platform/Window.hpp>

#include <study_sdl3/demo/DemoApp.hpp>
#include <study_sdl3/menu/MenuApp.hpp>
#include <study_sdl3/minesweeper/MinesweeperApp.hpp>
#include <study_sdl3/snake/SnakeApp.hpp>

class AppHost {
  SDLGuard _sdl;
  TTFGuard _ttf;

  AssetRegistry _assets;
  PerformanceMonitor _performance;

  Window _window;
  SDL_Color _clearColor;
  AppRegistry _registry;

  bool _running{true};
  std::optional<PendingAppCommand> _pendingCommand;
  std::unique_ptr<IApp> _activeApp;
  AppId _activeAppId{AppId::Menu};

public:
  explicit AppHost(WindowConfig initialWindow = WindowConfig{})
      : _sdl{SDL_INIT_VIDEO}, _ttf{}, _window{initialWindow},
        _clearColor{initialWindow.clearColor} {
    registerDefaultApps();
    switchTo(AppId::Menu);
  }

  Window &window() { return _window; }
  const Window &window() const { return _window; }

  SDL_Surface &surface() { return *_window.getSurface(); }

  const WindowState &windowState() const { return _window.state(); }

  Vec2i windowSize() const { return _window.getSurfaceSize(); }
  Vec2i drawableSize() const { return _window.getSurfaceSize(); }

  std::string assetPath(std::string_view relativePath) const {
    return study_sdl3::assets::path(relativePath);
  }

  AssetRegistry &assets() { return _assets; }
  PerformanceMonitor &performance() { return _performance; }

  void request(PendingAppCommand command) {
    if (command.type == AppCommandType::None)
      return;

    if (command.type == AppCommandType::Quit || !_pendingCommand ||
        _pendingCommand->type != AppCommandType::Quit) {
      _pendingCommand = std::move(command);
    }
  }

  void switchTo(AppId appId) {
    AppContext ctx{*this};

    if (_activeApp)
      _activeApp->onExit(ctx);

    std::unique_ptr<IApp> nextApp{_registry.create(appId)};
    const AppInfo nextInfo{nextApp->info()};

    _activeApp = std::move(nextApp);
    _activeAppId = appId;

    _assets.trim(study_sdl3::config::maxCachedFonts,
                 study_sdl3::config::maxCachedImages,
                 study_sdl3::config::maxCachedVectors);

    applyWindowConfig(nextInfo.window);
    _activeApp->onEnter(ctx);
  }

  int run() {
    SDL_Event event;
    uint64_t previousCounter = SDL_GetPerformanceCounter();
    const uint64_t counterFrequency = SDL_GetPerformanceFrequency();

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
      const uint64_t currentCounter = SDL_GetPerformanceCounter();
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
      _window.clearSurface(true, _clearColor);
      if (_activeApp)
        _activeApp->render(ctx, surface());

      _performance.end(FramePhase::Render);

      _performance.begin(FramePhase::Present);
      _window.updateSurface();
      _performance.end(FramePhase::Present);
      _performance.endFrame();
      processPendingCommand();
    }

    AppContext ctx{*this};
    if (_activeApp)
      _activeApp->onExit(ctx);

    return 0;
  }

private:
  void registerDefaultApps() {
    _registry.add(MenuApp::staticInfo(),
                  [] { return std::make_unique<MenuApp>(); });
    _registry.add(DemoApp::staticInfo(),
                  [] { return std::make_unique<DemoApp>(); });
    _registry.add(MinesweeperApp::staticInfo(),
                  [] { return std::make_unique<MinesweeperApp>(); });
    _registry.add(SnakeApp::staticInfo(),
                  [] { return std::make_unique<SnakeApp>(); });
  }

  bool handleHostEvent(const SDL_Event &event) {
    if (event.type == SDL_EVENT_KEY_DOWN &&
        _performance.handleHotkey(event.key))
      return true;

    _window.handleEvent(event);

    if (event.type == SDL_EVENT_QUIT)
      request(PendingAppCommand{.type = AppCommandType::Quit});

    return event.type == SDL_EVENT_QUIT;
  }

  void processPendingCommand() {
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
    case AppCommandType::ReconfigureWindow:
      if (command.window)
        applyWindowConfig(*command.window);
      return;
    }
  }

  void applyWindowConfig(const WindowConfig &config) {
    if (!config.fullscreen)
      _window.setFullscreen(false);

    _window.setTitle(config.title);
    _window.setResizable(config.resizable);
    _window.setBorderless(config.borderless);
    _window.setAlwaysOnTop(config.alwaysOnTop);
    _window.setFocusable(config.focusable);
    _window.setMouseGrabbed(config.mouseGrabbed);
    _window.setWindowedSize(config.windowedSize);
    _window.setMinimized(config.minimized);
    _window.setMaximized(config.maximized);

    if (config.fullscreen)
      _window.setFullscreen(true);
    else
      _window.setPosition(config.windowedPosition);

    _window.setHidden(config.hidden);

    _clearColor = config.clearColor;
  }
};

inline Window &AppContext::window() { return _host.window(); }

inline SDL_Surface &AppContext::surface() { return _host.surface(); }

inline const WindowState &AppContext::windowState() const {
  return _host.windowState();
}

inline Vec2i AppContext::windowSize() const { return _host.windowSize(); }

inline Vec2i AppContext::drawableSize() const { return _host.drawableSize(); }

inline std::string AppContext::assetPath(std::string_view relativePath) const {
  return _host.assetPath(relativePath);
}

inline AssetRegistry &AppContext::assets() { return _host.assets(); }

inline PerformanceMonitor &AppContext::performance() {
  return _host.performance();
}

inline void AppContext::requestSwitch(AppId appId) {
  _host.request(
      PendingAppCommand{.type = AppCommandType::SwitchTo, .target = appId});
}

inline void AppContext::requestMenu() {
  _host.request(PendingAppCommand{.type = AppCommandType::ReturnToMenu,
                                  .target = AppId::Menu});
}

inline void AppContext::requestQuit() {
  _host.request(PendingAppCommand{.type = AppCommandType::Quit});
}

inline void AppContext::requestWindowConfig(WindowConfig config) {
  _host.request(PendingAppCommand{.type = AppCommandType::ReconfigureWindow,
                                  .window = std::move(config)});
}
