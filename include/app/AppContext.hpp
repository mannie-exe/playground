#pragma once

#include <string>
#include <string_view>

#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_surface.h>

#include <app/AppTypes.hpp>
#include <support/AssetRegistry.hpp>
#include <support/PerformanceMonitor.hpp>

class AppHost;
class Window;

class AppContext {
  AppHost &_host;

public:
  explicit AppContext(AppHost &host) : _host{host} {}

  Window &window();
  SDL_Surface &surface();

  const WindowState &windowState() const;
  Vec2i windowSize() const;
  Vec2i drawableSize() const;

  std::string assetPath(std::string_view relativePath) const;
  AssetRegistry &assets();
  PerformanceMonitor &performance();

  void requestSwitch(AppId appId);
  void requestMenu();
  void requestQuit();
  void requestWindowConfig(WindowConfig config);
};
