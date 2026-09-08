#pragma once

#include <string>

#include <SDL3/SDL_keycode.h>

#include <study_sdl3/app/IApp.hpp>
#include <study_sdl3/snake/Config.hpp>

class SnakeApp : public IApp {
public:
  static AppInfo staticInfo() {
    return AppInfo{
        .id = AppId::Snake,
        .name = study_sdl3::snake::config::gameName,
        .window = WindowConfig{
            .title = std::string{study_sdl3::snake::config::windowTitle},
            .size = study_sdl3::snake::config::windowSize,
            .resizable = study_sdl3::snake::config::windowResizable,
            .fullscreen = study_sdl3::snake::config::windowFullscreen,
            .clearColor = SDL_Color{8, 16, 8, 255}}};
  }

  AppInfo info() const override { return staticInfo(); }

  EventResult handleEvent(AppContext &ctx, const SDL_Event &event) override {
    if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
      ctx.requestMenu();
      return EventResult::Consumed;
    }

    return EventResult::Ignored;
  }
};
