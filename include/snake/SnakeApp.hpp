#pragma once

#include <string>

#include <SDL3/SDL_scancode.h>

#include <app/IApp.hpp>
#include <snake/Config.hpp>

namespace snake = playground::snake::config;

class SnakeApp : public IApp {
public:
  SnakeApp() = default;

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

};
