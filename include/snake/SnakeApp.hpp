#pragma once

#include <string>

#include <SDL3/SDL_keycode.h>

#include <app/IApp.hpp>
#include <snake/Config.hpp>

namespace snake = playground::snake::config;

class SnakeApp : public IApp {
public:
  static AppInfo staticInfo() {
    return AppInfo{.id = AppId::Snake,
                   .name = snake::gameName,
                   .window =
                       AppWindowProps{.title = std::string{snake::windowTitle},
                                      .clearColor = snake::clearColor},
                   .view = snake::viewPolicy,
                   .presentation = {.window = {.mode = snake::windowMode}}};
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
