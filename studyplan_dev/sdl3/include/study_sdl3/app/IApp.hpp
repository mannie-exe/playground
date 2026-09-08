#pragma once

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_surface.h>

#include <study_sdl3/app/AppContext.hpp>
#include <study_sdl3/app/AppTypes.hpp>
#include <study_sdl3/interfaces/IInteractable.hpp>

class IApp {
public:
  virtual ~IApp() = default;

  virtual AppInfo info() const = 0;

  virtual void onEnter(AppContext &) {}
  virtual void onExit(AppContext &) {}

  virtual EventResult handleEvent(AppContext &, const SDL_Event &) {
    return EventResult::Ignored;
  }

  virtual void update(AppContext &, float) {}
  virtual void render(AppContext &, SDL_Surface &) {}

  IApp(IApp &&) noexcept = default;
  IApp &operator=(IApp &&) noexcept = default;

  IApp(const IApp &) = delete;
  IApp &operator=(const IApp &) = delete;

protected:
  IApp() = default;
};
