#pragma once

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_surface.h>

#include <study_sdl3/interfaces/IDrawable.hpp>
#include <study_sdl3/interfaces/IInteractable.hpp>
#include <study_sdl3/support/SDLPrimitives.hpp>

class IDisplayObject : public IDrawable, public IInteractable {
protected:
  RectTransform _transform;

  explicit IDisplayObject(const RectTransform transform)
      : _transform{transform} {}

public:
  virtual ~IDisplayObject() = default;

  RectTransform getTransform() const { return _transform; }

  virtual void setTransform(const RectTransform &transform) {
    _transform = transform;
  }

  virtual bool isPointInObject(float x, float y) const {
    return _transform.contains(x, y);
  }

  virtual EventResult handleEvent(const SDL_Event &) override {
    return EventResult::Ignored;
  }

  virtual void render(SDL_Surface &targetSurface) override = 0;

  IDisplayObject(IDisplayObject &&) noexcept = default;
  IDisplayObject &operator=(IDisplayObject &&) noexcept = default;

  IDisplayObject(const IDisplayObject &) = delete;
  IDisplayObject &operator=(const IDisplayObject &) = delete;
};
