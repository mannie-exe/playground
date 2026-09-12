#pragma once

#include <string>

#include <SDL3/SDL_keycode.h>

#include <study_sdl3/app/IApp.hpp>
#include <study_sdl3/snake/Config.hpp>

namespace snake = study_sdl3::snake::config;

class SnakeApp : public IApp {
public:
  static AppInfo staticInfo() {
    return AppInfo{.id = AppId::Snake,
                   .name = snake::gameName,
                   .window =
                       WindowConfig{.title = std::string{snake::windowTitle},
                                    .windowedSize = snake::windowSize,
                                    .resizable = snake::windowResizable,
                                    .fullscreen = snake::windowFullscreen,
                                    .clearColor = snake::clearColor}};
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
