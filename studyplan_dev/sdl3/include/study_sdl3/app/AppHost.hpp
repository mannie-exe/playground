#pragma once

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_timer.h>

#include <study_sdl3/app/AppConfig.hpp>
#include <study_sdl3/app/AppContext.hpp>
#include <study_sdl3/app/AppRegistry.hpp>
#include <study_sdl3/app/AppTypes.hpp>
#include <study_sdl3/app/SDLGuard.hpp>
#include <study_sdl3/app/TTFGuard.hpp>
#include <study_sdl3/demo/DemoApp.hpp>
#include <study_sdl3/menu/MenuApp.hpp>
#include <study_sdl3/minesweeper/MinesweeperApp.hpp>
#include <study_sdl3/platform/Window.hpp>
#include <study_sdl3/snake/SnakeApp.hpp>
#include <study_sdl3/support/AssetPath.hpp>
#include <study_sdl3/support/SDLError.hpp>

class AppHost {
  SDLGuard _sdl;
  TTFGuard _ttf;
  Window _window;
  SDL_Color _clearColor;
  AppRegistry _registry;
  std::unique_ptr<IApp> _activeApp;
  AppId _activeAppId{AppId::Menu};
  std::optional<PendingAppCommand> _pendingCommand;
  bool _running{true};

public:
  explicit AppHost(WindowConfig initialWindow = WindowConfig{
                       .title =
                           std::string{study_sdl3::config::defaultWindowTitle},
                       .size = study_sdl3::config::defaultWindowSize,
                       .resizable = true})
      : _sdl{SDL_INIT_VIDEO}, _ttf{}, _window{initialWindow.title,
                                              initialWindow.size,
                                              windowFlags(initialWindow)},
        _clearColor{initialWindow.clearColor} {
    registerDefaultApps();
    switchTo(AppId::Menu);
  }

  Window &window() { return _window; }
  const Window &window() const { return _window; }

  SDL_Surface &surface() { return *_window.getSurface(); }

  SDL_Point windowSize() const { return _window.getSurfaceSize(); }
  SDL_Point drawableSize() const { return _window.getSurfaceSize(); }

  std::string assetPath(std::string_view relativePath) const {
    return study_sdl3::assets::path(relativePath);
  }

  void request(PendingAppCommand command) {
    if (command.type == AppCommandType::None)
      return;

    if (command.type == AppCommandType::Quit ||
        !_pendingCommand || _pendingCommand->type != AppCommandType::Quit) {
      _pendingCommand = std::move(command);
    }
  }

  void switchTo(AppId appId) {
    AppContext ctx{*this};

    if (_activeApp)
      _activeApp->onExit(ctx);

    _activeApp = _registry.create(appId);
    _activeAppId = appId;

    applyWindowConfig(_activeApp->info().window);
    _activeApp->onEnter(ctx);
  }

  int run() {
    SDL_Event event;
    uint64_t previousCounter = SDL_GetPerformanceCounter();
    const uint64_t counterFrequency = SDL_GetPerformanceFrequency();

    while (_running) {
      AppContext ctx{*this};

      while (SDL_PollEvent(&event)) {
        handleHostEvent(event);

        if (!_running)
          break;

        if (_activeApp)
          _activeApp->handleEvent(ctx, event);

        processPendingCommand();

        if (!_running)
          break;
      }

      if (!_running)
        break;

      const uint64_t currentCounter = SDL_GetPerformanceCounter();
      const float deltaSeconds =
          static_cast<float>(currentCounter - previousCounter) /
          static_cast<float>(counterFrequency);
      previousCounter = currentCounter;

      if (_activeApp)
        _activeApp->update(ctx, deltaSeconds);

      processPendingCommand();

      if (!_running)
        break;

      _window.clearSurface(true, _clearColor);

      if (_activeApp)
        _activeApp->render(ctx, surface());

      _window.updateSurface();
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

  void handleHostEvent(const SDL_Event &event) {
    if (event.type == SDL_EVENT_QUIT)
      request(PendingAppCommand{.type = AppCommandType::Quit});
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
    _window.setTitle(config.title);
    _window.setSize(config.size);
    _window.setResizable(config.resizable);
    _window.setFullscreen(config.fullscreen);
    _clearColor = config.clearColor;
  }

  static SDL_WindowFlags windowFlags(const WindowConfig &config) {
    SDL_WindowFlags flags{0};

    if (config.resizable)
      flags |= SDL_WINDOW_RESIZABLE;
    if (config.fullscreen)
      flags |= SDL_WINDOW_FULLSCREEN;

    return flags;
  }
};

inline Window &AppContext::window() { return _host.window(); }

inline SDL_Surface &AppContext::surface() { return _host.surface(); }

inline SDL_Point AppContext::windowSize() const { return _host.windowSize(); }

inline SDL_Point AppContext::drawableSize() const { return _host.drawableSize(); }

inline std::string AppContext::assetPath(std::string_view relativePath) const {
  return _host.assetPath(relativePath);
}

inline void AppContext::requestSwitch(AppId appId) {
  _host.request(PendingAppCommand{.type = AppCommandType::SwitchTo,
                                  .target = appId});
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
