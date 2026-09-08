#pragma once

#include <optional>
#include <string>

#include <SDL3/SDL_keycode.h>

#include <study_sdl3/app/IApp.hpp>
#include <study_sdl3/demo/Config.hpp>
#include <study_sdl3/demo/DemoUI.hpp>
#include <study_sdl3/support/Font.hpp>
#include <study_sdl3/surface/ImageSurface.hpp>
#include <study_sdl3/surface/TextSurface.hpp>

class DemoApp : public IApp {
  std::optional<ImageSurface> _image;
  std::optional<TextSurface> _text;
  DemoUI _ui;

public:
  static AppInfo staticInfo() {
    return AppInfo{
        .id = AppId::Demo,
        .name = study_sdl3::demo::config::gameName,
        .window = WindowConfig{
            .title = std::string{study_sdl3::demo::config::windowTitle},
            .size = study_sdl3::demo::config::windowSize,
            .resizable = study_sdl3::demo::config::windowResizable,
            .fullscreen = study_sdl3::demo::config::windowFullscreen,
            .clearColor = SDL_Color{50, 50, 50, 255}}};
  }

  AppInfo info() const override { return staticInfo(); }

  void onEnter(AppContext &ctx) override {
    _image.emplace(ctx.assetPath(study_sdl3::demo::config::imagePath));
    _text.emplace(
        TextProps{.value = std::string{study_sdl3::demo::config::textValue},
                  .style = {.fgColor = study_sdl3::demo::config::textColor},
                  .layout = {.fontFitWidth = ctx.drawableSize().x}},
        Font(FontProps{.path =
                           ctx.assetPath(study_sdl3::demo::config::fontPath),
                       .style = {
                           .size = study_sdl3::demo::config::textSize}}));
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
