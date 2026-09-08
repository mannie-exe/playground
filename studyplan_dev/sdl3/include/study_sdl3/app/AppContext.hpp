#pragma once

#include <string>
#include <string_view>

#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_surface.h>

#include <study_sdl3/app/AppTypes.hpp>

class AppHost;
class Window;

class AppContext {
  AppHost &_host;

public:
  explicit AppContext(AppHost &host) : _host{host} {}

  Window &window();
  SDL_Surface &surface();

  SDL_Point windowSize() const;
  SDL_Point drawableSize() const;

  std::string assetPath(std::string_view relativePath) const;

  void requestSwitch(AppId appId);
  void requestMenu();
  void requestQuit();
  void requestWindowConfig(WindowConfig config);
};
