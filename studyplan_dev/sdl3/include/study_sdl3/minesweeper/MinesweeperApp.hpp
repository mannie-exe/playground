#pragma once

#include <optional>
#include <string>

#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_video.h>

#include <study_sdl3/app/IApp.hpp>
#include <study_sdl3/minesweeper/Config.hpp>
#include <study_sdl3/minesweeper/MinesweeperEvents.hpp>
#include <study_sdl3/minesweeper/MinesweeperGrid.hpp>
#include <study_sdl3/minesweeper/MinesweeperUI.hpp>
#include <study_sdl3/support/SDLPrimitives.hpp>

namespace minesweeper = study_sdl3::minesweeper::config;

class MinesweeperApp : public IApp {
  std::optional<MinesweeperUI> _ui;
  study_sdl3::minesweeper::MinesweeperEvents _events;
  FontHandle _font;

public:
  static AppInfo staticInfo() {
    return AppInfo{
        .id = AppId::Minesweeper,
        .name = minesweeper::gameName,
        .window = WindowConfig{.title = std::string{minesweeper::windowTitle},
                               .windowedSize = windowSizeFor(
                                   defaultGridProps(), defaultLayoutProps()),
                               .resizable = minesweeper::windowResizable,
                               .fullscreen = minesweeper::windowFullscreen,
                               .clearColor = minesweeper::bgColor}};
  }

  MinesweeperApp() : _events{study_sdl3::minesweeper::events()} {}

  AppInfo info() const override { return staticInfo(); }

  void onEnter(AppContext &ctx) override {
    _font = ctx.assets().getFont(
        FontProps{.path = ctx.assetPath(minesweeper::baseFontPath),
                  .style = {.size = 32.0f}});

    const MinesweeperGridProps grid{defaultGridProps()};
    const MinesweeperUILayoutProps layout{defaultLayoutProps()};
    const MinesweeperUIProps props{makeUIProps(ctx, grid, layout)};
    const Vec2i iconSize{minesweeper::cellSize, minesweeper::cellSize};
    _ui.emplace(props, _events,
                ctx.assets().getVector(
                    ctx.assetPath(minesweeper::bombImagePath), iconSize),
                ctx.assets().getVector(
                    ctx.assetPath(minesweeper::flagImagePath), iconSize),
                _font);
    requestWindowLayout(ctx, grid, layout);
  }

  void onExit(AppContext &) override { _ui.reset(); }

  EventResult handleEvent(AppContext &ctx, const SDL_Event &event) override {
    if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
      ctx.requestMenu();
      return EventResult::Consumed;
    }

    if ((event.type == SDL_EVENT_WINDOW_RESIZED ||
         event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) &&
        _ui) {
      _ui->setDisplayState(
          {.windowSize = ctx.windowSize(), .drawableSize = ctx.drawableSize()});
      return EventResult::Handled;
    }

    const EventResult result =
        _ui ? _ui->handleEvent(event) : EventResult::Ignored;
    if (_ui && event.type == _events.newGameRequested)
      requestWindowLayout(ctx, _ui->getProps().grid, _ui->getProps().layout);
    return result;
  }

  void render(AppContext &, SDL_Surface &targetSurface) override {
    if (_ui)
      _ui->render(targetSurface);
  }

private:
  static MinesweeperGridProps defaultGridProps() {
    return MinesweeperGridProps{.size = minesweeper::gridSize,
                                .cellSize = minesweeper::cellSize,
                                .gap = minesweeper::gridGap,
                                .bombChance = minesweeper::bombChance};
  }

  static MinesweeperUILayoutProps defaultLayoutProps() {
    return MinesweeperUILayoutProps{.padding = minesweeper::outerPadding,
                                    .footerHeight = minesweeper::footerHeight,
                                    .footerCounterWidth =
                                        minesweeper::footerCounterWidth,
                                    .footerGap = minesweeper::footerGap};
  }

  static Vec2i contentSize(const MinesweeperGridProps &grid,
                           const MinesweeperUILayoutProps &layout) {
    return Vec2i{grid.width(),
                 grid.height() + layout.padding + layout.footerHeight};
  }

  static Vec2i windowSizeFor(const MinesweeperGridProps &grid,
                             const MinesweeperUILayoutProps &layout) {
    const Vec2i gridSize{grid.width(), grid.height()};
    return minesweeper::calculateWindowSize(
        gridSize, layout.padding, layout.footerHeight, layout.padding);
  }

  static WindowConfig windowConfigFor(const MinesweeperGridProps &grid,
                                      const MinesweeperUILayoutProps &layout,
                                      const WindowState &current) {
    WindowConfig result{staticInfo().window};
    result.windowedSize = windowSizeFor(grid, layout);
    result.windowedPosition =
        Vec2i{SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED};
    result.fullscreen = current.fullscreen;
    return result;
  }

  static MinesweeperUIProps
  makeUIProps(const AppContext &ctx, const MinesweeperGridProps &grid,
              const MinesweeperUILayoutProps &layout) {
    const Vec2i content{contentSize(grid, layout)};
    return MinesweeperUIProps{
        .transform =
            rect(static_cast<float>(layout.padding),
                 static_cast<float>(layout.padding),
                 static_cast<float>(content.x), static_cast<float>(content.y)),
        .displayState = {.windowSize = ctx.windowSize(),
                         .drawableSize = ctx.drawableSize()},
        .grid = grid,
        .cell = {.bombColor = minesweeper::bombBgColor,
                 .revealedColor = minesweeper::revealedBgColor,
                 .clearedColor = minesweeper::buttonClearedColor,
                 .flagColor = minesweeper::flagCounterIconColor,
                 .iconPadding = minesweeper::iconPadding,
                 .labelColors = minesweeper::cellLabelColors,
                 .button = {.baseColor = minesweeper::buttonBaseColor,
                            .hoverColor = minesweeper::buttonHoverColor,
                            .activeColor = minesweeper::buttonActiveColor}},
        .newGameButton = {.button = {.baseColor = minesweeper::buttonBaseColor,
                                     .hoverColor =
                                         minesweeper::buttonHoverColor,
                                     .activeColor =
                                         minesweeper::buttonActiveColor},
                          .labelColor = minesweeper::newGameLabelColor},
        .flagCounter = {.button = {.disabledColor =
                                       minesweeper::buttonBaseColor},
                        .iconColor = minesweeper::flagCounterIconColor,
                        .labelColor = minesweeper::flagCounterLabelColor},
        .layout = layout};
  }

  static void requestWindowLayout(AppContext &ctx,
                                  const MinesweeperGridProps &grid,
                                  const MinesweeperUILayoutProps &layout) {
    ctx.requestWindowConfig(windowConfigFor(grid, layout, ctx.windowState()));
  }
};
