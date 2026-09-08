#pragma once

#include <optional>
#include <string>

#include <SDL3/SDL_keycode.h>

#include <study_sdl3/app/IApp.hpp>
#include <study_sdl3/minesweeper/Config.hpp>
#include <study_sdl3/minesweeper/MinesweeperUI.hpp>

class MinesweeperApp : public IApp {
  std::optional<MinesweeperUI> _ui;

public:
  static AppInfo staticInfo() {
    return AppInfo{
        .id = AppId::Minesweeper,
        .name = study_sdl3::minesweeper::config::gameName,
        .window = WindowConfig{
            .title = std::string{study_sdl3::minesweeper::config::windowTitle},
            .size = study_sdl3::minesweeper::config::windowSize,
            .resizable = study_sdl3::minesweeper::config::windowResizable,
            .fullscreen = study_sdl3::minesweeper::config::windowFullscreen,
            .clearColor = study_sdl3::minesweeper::config::backgroundColor}};
  }

  AppInfo info() const override { return staticInfo(); }

  void onEnter(AppContext &ctx) override {
    _ui.emplace(DisplayState{.windowSize = ctx.windowSize(),
                             .drawableSize = ctx.drawableSize()});
  }

  void onExit(AppContext &) override { _ui.reset(); }

  EventResult handleEvent(AppContext &ctx, const SDL_Event &event) override {
    if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
      ctx.requestMenu();
      return EventResult::Consumed;
    }

    if ((event.type == SDL_EVENT_WINDOW_RESIZED ||
         event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) &&
        _ui) {
      _ui->setDisplayState(
          {.windowSize = ctx.windowSize(), .drawableSize = ctx.drawableSize()});
      return EventResult::Handled;
    }

    return _ui ? _ui->handleEvent(event) : EventResult::Ignored;
  }

  void render(AppContext &, SDL_Surface &targetSurface) override {
    if (_ui)
      _ui->render(targetSurface);
  }
};
