#pragma once

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_surface.h>

#include <interfaces/IDrawable.hpp>
#include <interfaces/IInteractable.hpp>
#include <support/SDLPrimitives.hpp>

class IDisplayObject : public IDrawable, public IInteractable {
protected:
  RectTransform _transform;
  bool _visible;

  explicit IDisplayObject(const RectTransform transform,
                          const bool visible = true)
      : _transform{transform}, _visible{visible} {}

public:
  virtual ~IDisplayObject() = default;

  RectTransform getTransform() const { return _transform; }
  bool isVisible() const { return _visible; }
  bool isShown() const { return isVisible(); }

  virtual void setTransform(const RectTransform &transform) {
    _transform = transform;
  }

  virtual void show(const bool show = true) { _visible = show; }
  virtual void hide(const bool hide = true) { show(!hide); }

  virtual bool isPointInObject(float x, float y) const {
    return _transform.contains(x, y);
  }

  virtual EventResult handleEvent(const SDL_Event &) override {
    return EventResult::Ignored;
  }

  virtual void render(SDL_Surface &) override = 0;

  IDisplayObject(IDisplayObject &&) noexcept = default;
  IDisplayObject &operator=(IDisplayObject &&) noexcept = default;

  IDisplayObject(const IDisplayObject &) = delete;
  IDisplayObject &operator=(const IDisplayObject &) = delete;
};
