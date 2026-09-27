#pragma once

#include <string>

#include <SDL3/SDL_scancode.h>

#include <app/IApp.hpp>
#include <demo2d/Config.hpp>
#include <demo2d/Demo2DUI.hpp>
#include <platform/sdl/UISession.hpp>
#include <support/Font.hpp>

namespace playground::demo2d {
class Demo2DApp final : public IApp {
  playground::demo2d::ViewProps _props;
  playground::demo2d::ViewResources _resources;

  playground::sdl::UISession _ui{playground::sdl::UISessionTiming::Monotonic};

public:
  Demo2DApp() {
    input().addContext(
        {.name = "navigation", .stage = playground::input::InputStage::AfterUI},
        {{.action = "back", .code = SDL_SCANCODE_ESCAPE}});
  }

  void onActions(AppContext &ctx,
                 const playground::input::InputSnapshot &actions) override {
    if (actions["back"].pressed)
      ctx.requestMenu();
  }

  static AppInfo staticInfo() {
    return AppInfo{
        .id = AppId::Demo2D,
        .name = playground::demo2d::config::gameName,
        .window =
            AppWindowProps{
                .title = std::string{playground::demo2d::config::windowTitle},
                .clearColor = playground::demo2d::config::clearColor},
        .view = playground::demo2d::config::viewPolicy,
        .presentation = {
            .window = {.mode = playground::demo2d::config::windowMode}}};
  }

  AppInfo info() const override { return staticInfo(); }

  runtime::ActivityProps activityProps() const override {
    return {false, false};
  }

  runtime::ActivityDemand activityDemand() override {
    return _ui.activityDemand();
  }

  std::optional<playground::math::Size2>
  preferredContentSize(playground::math::Size2 maximum,
                       playground::math::Vec2f density) override {
    return _ui.root().preferredSize(maximum, density);
  }

  void onEnter(AppContext &ctx) override {
    _resources = playground::demo2d::acquireResources(
        ctx.resources(), playground::demo2d::config::textSize);
    rebuildView(ctx);
    _ui.synchronize(ctx);
  }

  void onExit(AppContext &) override {
    _ui.clear();
    _resources = {};
  }

  void rebuildView(AppContext &ctx) {
    auto candidate =
        playground::demo2d::makeDemo2DUI(ctx.assets(), _resources, _props);
    _ui.root().setContent(std::move(candidate));
  }

  EventResult handleEvent(AppContext &ctx, const SDL_Event &event) override {
    _ui.update(0);
    _ui.synchronize(ctx);

    return _ui.handleEvent(event);
  }

  void update(AppContext &ctx, float dt) override {
    _ui.update(dt);
    _ui.synchronize(ctx);
  }

  void render(AppContext &ctx,
              playground::rendering::RenderFrame &frame) override {
    _ui.synchronize(ctx);
    _ui.render(frame);
  }
};
} // namespace playground::demo2d
