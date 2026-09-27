#pragma once

#include <string>

#include <SDL3/SDL_scancode.h>

#include <app/IApp.hpp>
#include <snake/Config.hpp>

namespace snake = playground::snake::config;

class SnakeApp : public IApp {
public:
  SnakeApp() {
    input().addContext({.name = "navigation",
                        .stage = playground::input::InputStage::BeforeUI},
                       {{.action = "back", .code = SDL_SCANCODE_ESCAPE}});
  }

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

  void onActions(AppContext &ctx,
                 const playground::input::InputSnapshot &actions) override {
    if (actions["back"].pressed)
      ctx.requestMenu();
  }
};
