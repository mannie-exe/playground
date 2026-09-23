#pragma once

#include <SDL3/SDL_keycode.h>

#include <app/IApp.hpp>

class MenuApp : public IApp {
public:
  static AppInfo staticInfo() {
    return AppInfo{.id = AppId::Menu,
                   .name = "Menu",
                   .window = AppWindowProps{.title = "Me n' U",
                                            .clearColor = {24, 24, 24, 255}}};
  }

  AppInfo info() const override { return staticInfo(); }

  EventResult handleEvent(AppContext &ctx, const SDL_Event &event) override {
    if (event.type != SDL_EVENT_KEY_DOWN)
      return EventResult::Ignored;

    switch (event.key.key) {
    case SDLK_1:
      ctx.requestSwitch(AppId::Demo);
      return EventResult::Consumed;
    case SDLK_2:
      ctx.requestSwitch(AppId::Minesweeper);
      return EventResult::Consumed;
    case SDLK_3:
      ctx.requestSwitch(AppId::Snake);
      return EventResult::Consumed;
    case SDLK_ESCAPE:
    case SDLK_Q:
      ctx.requestQuit();
      return EventResult::Consumed;
    default:
      return EventResult::Ignored;
    }
  }
};
