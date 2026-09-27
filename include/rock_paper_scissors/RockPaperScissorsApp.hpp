#pragma once

#include <string>

#include <SDL3/SDL_scancode.h>

#include <app/IApp.hpp>
#include <rock_paper_scissors/Config.hpp>

namespace rock_paper_scissors = playground::rock_paper_scissors::config;

class RockPaperScissorsApp : public IApp {
public:
  RockPaperScissorsApp() {
    input().addContext({.name = "navigation",
                        .stage = playground::input::InputStage::BeforeUI},
                       {{.action = "back", .code = SDL_SCANCODE_ESCAPE}});
  }

  static AppInfo staticInfo() {
    return AppInfo{
        .id = AppId::RockPaperScissors,
        .name = rock_paper_scissors::gameName,
        .window =
            AppWindowProps{.title =
                               std::string{rock_paper_scissors::windowTitle},
                           .clearColor = rock_paper_scissors::clearColor},
        .view = rock_paper_scissors::viewPolicy,
        .presentation = {.window = {.mode = rock_paper_scissors::windowMode}}};
  };

  AppInfo info() const override { return staticInfo(); }

  void onActions(AppContext &ctx,
                 const playground::input::InputSnapshot &actions) override {
    if (actions["back"].pressed)
      ctx.requestMenu();
  }
};
