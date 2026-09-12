#pragma once

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_surface.h>

#include <interfaces/IActivatable.hpp>
#include <interfaces/IInteractable.hpp>

class AppContext;

class IRuntimeObject : public IActivatable {
protected:
  IRuntimeObject() = default;

public:
  virtual ~IRuntimeObject() = default;

  virtual EventResult handleEvent(AppContext &, const SDL_Event &) {
    return EventResult::Ignored;
  }

  virtual void update(AppContext &, float) {}

  virtual void render(AppContext &, SDL_Surface &) {}

  IRuntimeObject(IRuntimeObject &&) noexcept = default;
  IRuntimeObject &operator=(IRuntimeObject &&) noexcept = default;

  IRuntimeObject(const IRuntimeObject &) = delete;
  IRuntimeObject &operator=(const IRuntimeObject &) = delete;
};
