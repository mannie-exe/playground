#pragma once

#include <optional>

#include <app/IApp.hpp>
#include <menu/MenuUI.hpp>
#include <platform/sdl/UISession.hpp>

class MenuApp final : public IApp {
  playground::sdl::UISession _ui{playground::sdl::UISessionTiming::Monotonic};
  playground::menu::MenuUI *_view{};
  std::optional<AppId> _pending;

  void synchronize(AppContext &);
  void launchPending(AppContext &);

public:
  MenuApp();
  static AppInfo staticInfo();

  AppInfo info() const override { return staticInfo(); }

  playground::input::InputClaims inputClaims() override {
    return _ui.inputClaims();
  }

  playground::runtime::ActivityProps activityProps() const override {
    return {false, false};
  }

  playground::runtime::ActivityDemand activityDemand() override {
    return _ui.activityDemand();
  }

  std::optional<playground::math::Size2>
  preferredContentSize(playground::math::Size2 maximum,
                       playground::math::Vec2f density) override;
  void onEnter(AppContext &) override;
  void onExit(AppContext &) override;
  void onActions(AppContext &,
                 const playground::input::InputSnapshot &) override;
  EventResult handleEvent(AppContext &, const SDL_Event &) override;
  void update(AppContext &, float) override;
  void render(AppContext &, playground::rendering::RenderFrame &) override;
};
