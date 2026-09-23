#pragma once

#include <SDL3/SDL_events.h>
#include <rendering/RenderBackend.hpp>

#include <interfaces/IActivatable.hpp>
#include <platform/sdl/EventResult.hpp>

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

  virtual void render(AppContext &, playground::rendering::RenderFrame &) {}

  IRuntimeObject(IRuntimeObject &&) noexcept = default;
  IRuntimeObject &operator=(IRuntimeObject &&) noexcept = default;

  IRuntimeObject(const IRuntimeObject &) = delete;
  IRuntimeObject &operator=(const IRuntimeObject &) = delete;
};
