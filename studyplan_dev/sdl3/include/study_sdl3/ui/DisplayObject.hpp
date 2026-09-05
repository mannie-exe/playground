#pragma once

#include <SDL3/SDL_events.h>
#include <study_sdl3/interfaces/IDrawable.hpp>

struct RectTransform {
  float x{};
  float y{};
  float w{};
  float h{};

  float left() const { return x; }
  float right() const { return x + w; }
  float top() const { return y; }
  float bottom() const { return y + h; }

  bool contains(float px, float py) const {
    return px >= left() && px < right() && py >= top() && py < bottom();
  }

  SDL_Rect toSDL() const {
    return SDL_Rect{static_cast<int>(x), static_cast<int>(y),
                    static_cast<int>(w), static_cast<int>(h)};
  }
};

class DisplayObject : public IDrawable {
protected:
  RectTransform _transform;

public:
  DisplayObject(const RectTransform rect) : _transform{rect} {}
  virtual ~DisplayObject() = default;

  virtual bool isPointInObject(float x, float y) const {
    return _transform.contains(x, y);
  }

  DisplayObject(DisplayObject &&) noexcept = default;
  DisplayObject &operator=(DisplayObject &&) noexcept = default;

  DisplayObject(const DisplayObject &) = delete;
  DisplayObject &operator=(const DisplayObject &) = delete;
};
