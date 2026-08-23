#pragma once

#include <IDrawable.hpp>
#include <IInteractable.hpp>

class DisplayObject : public IDrawable, public IInteractable {
public:
  DisplayObject() = default;
  virtual ~DisplayObject() = default;

  virtual bool isPointInObject(int x, int y) const = 0;

  DisplayObject(const DisplayObject &) = delete;
  DisplayObject &operator=(const DisplayObject &) = delete;

  bool hover{false};
};
