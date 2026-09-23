#pragma once

#include <memory>
#include <string>

#include <SDL3/SDL_keycode.h>

#include <app/IApp.hpp>
#include <minesweeper/Config.hpp>
#include <minesweeper/MinesweeperUI.hpp>
#include <platform/sdl/UISession.hpp>

namespace minesweeper_config = playground::minesweeper::config;

class MinesweeperApp : public IApp {
  playground::minesweeper::MinesweeperEvents _events{
      playground::minesweeper::events()};
  playground::sdl::UISession _session;
  playground::ui::NodeHandle<MinesweeperUI> _view;
  FontHandle _baseFont;

public:
  static AppInfo staticInfo() {
    return {
        .id = AppId::Minesweeper,
        .name = minesweeper_config::gameName,
        .window = {.title = std::string{minesweeper_config::windowTitle},
                   .clearColor = minesweeper_config::bgColor},
        .view = minesweeper_config::viewPolicy,
        .presentation = {.window = {.mode = minesweeper_config::windowMode}}};
  }

  AppInfo info() const override { return staticInfo(); }
  std::optional<playground::math::Size2>
  preferredContentSize(playground::math::Size2 maximum,
                       playground::math::Vec2f density) override {
    return _session.root().preferredSize(maximum, density);
  }

  void onEnter(AppContext &ctx) override {
    _baseFont = ctx.assets().getFont(
        FontProps{.path = ctx.assetPath(minesweeper_config::baseFontPath),
                  .style = {.size = 32.0f}});
    const auto grid = defaultGridProps();
    const auto layout = defaultLayoutProps();
    auto view = std::make_unique<MinesweeperUI>(
        makeUIProps(grid, layout), _events,
        playground::minesweeper::ViewResources{
            ctx.assets(), _baseFont,
            ctx.assetPath(minesweeper_config::bombImagePath),
            ctx.assetPath(minesweeper_config::flagImagePath)});
    auto *pointer = view.get();
    _session.root().setContent(std::move(view));
    _view = pointer->handle<MinesweeperUI>();
    _session.synchronize(ctx.windowMetrics(), ctx.presentation().viewport);
  }

  void onExit(AppContext &) override {
    _session.clear();
    _view = {};
    _baseFont.reset();
  }

  EventResult handleEvent(AppContext &ctx, const SDL_Event &event) override {
    if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
      ctx.requestMenu();
      return EventResult::Consumed;
    }
    _session.synchronize(ctx.windowMetrics(), ctx.presentation().viewport);
    auto *view = _view.get();
    const auto result =
        event.type >= SDL_EVENT_USER
            ? (view ? view->handleEvent(event) : EventResult::Ignored)
            : _session.handleEvent(event);
    if (view && view->processPendingActions())
      ctx.requestFitContent();
    return result;
  }

  void update(AppContext &, float dt) override { _session.update(dt); }

  void render(AppContext &ctx,
              playground::rendering::RenderFrame &frame) override {
    _session.synchronize(ctx.windowMetrics(), ctx.presentation().viewport);
    _session.render(frame.paint2D());
  }

private:
  static MinesweeperGridProps defaultGridProps() {
    return {.size = minesweeper_config::gridSize,
            .cellSize = minesweeper_config::cellSize,
            .gap = minesweeper_config::gridGap,
            .bombChance = minesweeper_config::bombChance};
  }

  static MinesweeperUILayoutProps defaultLayoutProps() {
    return {.padding = minesweeper_config::outerPadding,
            .footerHeight = minesweeper_config::footerHeight,
            .footerCounterWidth = minesweeper_config::footerCounterWidth,
            .footerGap = minesweeper_config::footerGap};
  }

  static MinesweeperUIProps
  makeUIProps(const MinesweeperGridProps &grid,
              const MinesweeperUILayoutProps &layout) {
    return MinesweeperUIProps{
        .grid = grid,
        .cell = {.bombColor = minesweeper_config::bombBgColor,
                 .revealedColor = minesweeper_config::revealedBgColor,
                 .clearedColor = minesweeper_config::buttonClearedColor,
                 .flagColor = minesweeper_config::flagCounterIconColor,
                 .iconPadding = minesweeper_config::iconPadding,
                 .labelColors = minesweeper_config::cellLabelColors,
                 .button = {.normal = minesweeper_config::buttonBaseColor,
                            .hover = minesweeper_config::buttonHoverColor,
                            .pressed = minesweeper_config::buttonActiveColor}},
        .newGameButton =
            {.button = {.normal = minesweeper_config::buttonBaseColor,
                        .hover = minesweeper_config::buttonHoverColor,
                        .pressed = minesweeper_config::buttonActiveColor},
             .labelColor = minesweeper_config::newGameLabelColor},
        .flagCounter = {.button = {.disabled =
                                       minesweeper_config::buttonBaseColor},
                        .iconColor = minesweeper_config::flagCounterIconColor,
                        .labelColor =
                            minesweeper_config::flagCounterLabelColor},
        .layout = layout};
  }
};
