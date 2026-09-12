#pragma once

#include <optional>
#include <string>

#include <SDL3/SDL_keycode.h>

#include <app/IApp.hpp>
#include <demo/Config.hpp>
#include <demo/DemoUI.hpp>
#include <support/Font.hpp>
#include <surface/ImageSurface.hpp>
#include <surface/TextSurface.hpp>

class DemoApp : public IApp {
  std::optional<ImageSurface> _image;
  std::optional<TextSurface> _text;
  DemoUI _ui;

public:
  static AppInfo staticInfo() {
    return AppInfo{
        .id = AppId::Demo,
        .name = playground::demo::config::gameName,
        .window = WindowConfig{
            .title = std::string{playground::demo::config::windowTitle},
            .windowedSize = playground::demo::config::windowSize,
            .resizable = playground::demo::config::windowResizable,
            .fullscreen = playground::demo::config::windowFullscreen,
            .clearColor = playground::demo::config::clearColor}};
  }

  AppInfo info() const override { return staticInfo(); }

  void onEnter(AppContext &ctx) override {
    const std::string imagePath{
        ctx.assetPath(playground::demo::config::imagePath)};
    const std::string fontPath{
        ctx.assetPath(playground::demo::config::fontPath)};
    _image.emplace(ctx.assets().getImage(imagePath), imagePath);
    _text.emplace(
        TextProps{.value = std::string{playground::demo::config::textValue},
                  .style = {.fgColor = playground::demo::config::textColor},
                  .layout = {.fontFitWidth = ctx.drawableSize().x}},
        ctx.assets().getFont(FontProps{
            .path = fontPath,
            .style = {.size = playground::demo::config::textSize}}));
  }

  void onExit(AppContext &) override {
    _text.reset();
    _image.reset();
  }

  EventResult handleEvent(AppContext &ctx, const SDL_Event &event) override {
    if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
      ctx.requestMenu();
      return EventResult::Consumed;
    }

    if ((event.type == SDL_EVENT_WINDOW_RESIZED ||
         event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) &&
        _text) {
      _text->setFontFitWidth(ctx.drawableSize().x);
    }

    return _ui.handleEvent(event);
  }

  void render(AppContext &, SDL_Surface &targetSurface) override {
    if (_image)
      _image->render(targetSurface);
    if (_text)
      _text->render(targetSurface);

    _ui.render(targetSurface);
  }
};
