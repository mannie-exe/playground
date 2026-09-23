#pragma once

#include <string>

#include <SDL3/SDL_keycode.h>

#include <app/IApp.hpp>
#include <demo/Config.hpp>
#include <demo/DemoUI.hpp>
#include <platform/sdl/UISession.hpp>
#include <support/Font.hpp>

class DemoApp : public IApp {
  playground::sdl::UISession _ui;

public:
  static AppInfo staticInfo() {
    return AppInfo{
        .id = AppId::Demo,
        .name = playground::demo::config::gameName,
        .window =
            AppWindowProps{
                .title = std::string{playground::demo::config::windowTitle},
                .clearColor = playground::demo::config::clearColor},
        .view = playground::demo::config::viewPolicy,
        .presentation = {
            .window = {.mode = playground::demo::config::windowMode}}};
  }

  AppInfo info() const override { return staticInfo(); }
  std::optional<playground::math::Size2>
  preferredContentSize(playground::math::Size2 maximum,
                       playground::math::Vec2f density) override {
    return _ui.root().preferredSize(maximum, density);
  }

  void onEnter(AppContext &ctx) override {
    const std::string imagePath{
        ctx.assetPath(playground::demo::config::imagePath)};
    const std::string fontPath{
        ctx.assetPath(playground::demo::config::fontPath)};
    _ui.root().setContent(playground::demo::makeDemoUI(
        ctx.assets(), ctx.assets().getImage(imagePath),
        ctx.assets().getFont(
            FontProps{.path = fontPath,
                      .style = {.size = playground::demo::config::textSize}})));
    _ui.synchronize(ctx.windowMetrics(), ctx.presentation().viewport);
  }

  void onExit(AppContext &) override { _ui.clear(); }

  EventResult handleEvent(AppContext &ctx, const SDL_Event &event) override {
    if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
      ctx.requestMenu();
      return EventResult::Consumed;
    }

    _ui.synchronize(ctx.windowMetrics(), ctx.presentation().viewport);

    return _ui.handleEvent(event);
  }

  void update(AppContext &, float dt) override { _ui.update(dt); }

  void render(AppContext &ctx,
              playground::rendering::RenderFrame &frame) override {
    _ui.synchronize(ctx.windowMetrics(), ctx.presentation().viewport);
    _ui.render(frame.paint2D());
  }
};
