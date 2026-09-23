#pragma once

#include <app/IApp.hpp>
#include <rock_paper_scissors/Config.hpp>

namespace rock_paper_scissors = playground::rock_paper_scissors::config;

class RockPaperScissors : public IApp {
public:
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

  void render(AppContext &ctx,
              playground::rendering::RenderFrame &frame) override {}
};
