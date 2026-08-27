#pragma once

#include <study_sdl3/interfaces/IDrawable.hpp>
#include <study_sdl3/interfaces/IInteractable.hpp>

class DisplayObject : public IDrawable, public IInteractable {
protected:
  bool _hover{false};

public:
  DisplayObject() = default;
  virtual ~DisplayObject() = default;

  virtual bool isPointInObject(float x, float y) const = 0;

  virtual void onMouseEnter() {};
  virtual void onMouseExit() {};

  DisplayObject(const DisplayObject &) = delete;
  DisplayObject &operator=(const DisplayObject &) = delete;
};
