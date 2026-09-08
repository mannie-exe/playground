#pragma once

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_mouse.h>

#include <study_sdl3/interfaces/IInteractable.hpp>
#include <study_sdl3/ui/IDisplayObject.hpp>
#include <study_sdl3/ui/InteractionState.hpp>
#include <study_sdl3/ui/Rectangle.hpp>

struct ButtonStyle {
  SDL_Color baseColor{55, 82, 61, 255};
  SDL_Color hoverColor{131, 183, 141, 255};
  SDL_Color activeColor{117, 133, 120, 255};
};

class Button : public IDisplayObject {
  Rectangle _background;
  IInteractable &_parent;
  ButtonStyle _style;
  InteractionState _state{InteractionState::Normal};

public:
  Button(const RectTransform transform, IInteractable &parent,
         const ButtonStyle style = {})
      : IDisplayObject{transform}, _background{transform}, _parent{parent},
        _style{style} {}

  ButtonStyle getStyle() const { return _style; }
  InteractionState getState() const { return _state; }

  void setStyle(const ButtonStyle &style) { _style = style; }

  virtual EventResult handleEvent(const SDL_Event &event) override {
    if (_state == InteractionState::Disabled)
      return EventResult::Ignored;

    if (event.type == SDL_EVENT_KEY_UP || event.type == SDL_EVENT_KEY_DOWN) {
      return onKey(event.key);
    }

    if (event.type == SDL_EVENT_MOUSE_MOTION) {
      bool inObject = isPointInObject(event.motion.x, event.motion.y);
      if ((_state == InteractionState::Hovered ||
           _state == InteractionState::Pressed) &&
          !inObject)
        return onMouseExit(event.motion.x, event.motion.y);
      if ((_state == InteractionState::Normal ||
           _state == InteractionState::PressedOutside) &&
          inObject)
        return onMouseEnter(event.motion.x, event.motion.y);
      return onMouseMove(event.motion);
    }

    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
      bool inObject = isPointInObject(event.button.x, event.button.y);
      if (event.button.button == SDL_BUTTON_LEFT && inObject)
        return onMouseClick(event.button);
      return EventResult::Ignored;
    }

    if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
      bool inObject = isPointInObject(event.button.x, event.button.y);
      if ((_state == InteractionState::Pressed ||
           _state == InteractionState::PressedOutside) &&
          event.button.button == SDL_BUTTON_LEFT)
        return onMouseRelease(event.button, inObject);
      return EventResult::Ignored;
    }

    if (event.type == SDL_EVENT_MOUSE_WHEEL) {
      if (isPointInObject(event.wheel.mouse_x, event.wheel.mouse_y))
        return onMouseWheel(event.wheel);
      return EventResult::Ignored;
    }

    if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST ||
        event.type == SDL_EVENT_WINDOW_MOUSE_LEAVE ||
        event.type == SDL_EVENT_WINDOW_FOCUS_GAINED ||
        event.type == SDL_EVENT_WINDOW_MOUSE_ENTER) {
      float mX, mY;
      SDL_GetMouseState(&mX, &mY);
      bool inObject = isPointInObject(mX, mY);

      if ((event.type == SDL_EVENT_WINDOW_FOCUS_LOST ||
           event.type == SDL_EVENT_WINDOW_MOUSE_LEAVE) &&
          (_state == InteractionState::Hovered ||
           _state == InteractionState::Pressed ||
           _state == InteractionState::PressedOutside)) {
        return resetInteraction();
      }

      if ((event.type == SDL_EVENT_WINDOW_FOCUS_GAINED ||
           event.type == SDL_EVENT_WINDOW_MOUSE_ENTER) &&
          inObject) {
        return onMouseEnter(mX, mY);
      }
    }

    return EventResult::Ignored;
  }

  virtual void render(SDL_Surface &targetSurface) override {
    const SDL_Color &color = (_state == InteractionState::Pressed ||
                              _state == InteractionState::PressedOutside)
                                 ? _style.activeColor
                             : _state == InteractionState::Hovered
                                 ? _style.hoverColor
                                 : _style.baseColor;
    _background.render(targetSurface, color);
  }

  EventResult onKey(const SDL_KeyboardEvent &) { return EventResult::Ignored; }

  EventResult onMouseMove(const SDL_MouseMotionEvent &) {
    return EventResult::Ignored;
  }

  EventResult onMouseEnter(const float, const float) {
    if (_state == InteractionState::PressedOutside)
      _state = InteractionState::Pressed;
    else
      _state = InteractionState::Hovered;
    return EventResult::Handled;
  }

  EventResult onMouseExit(const float, const float) {
    if (_state == InteractionState::Pressed)
      _state = InteractionState::PressedOutside;
    else
      _state = InteractionState::Normal;
    return EventResult::Handled;
  }

  EventResult onMouseClick(const SDL_MouseButtonEvent &mButtonEvent) {
    if (mButtonEvent.button != SDL_BUTTON_LEFT)
      return EventResult::Ignored;

    _state = InteractionState::Pressed;
    return EventResult::Consumed;
  }

  EventResult onMouseRelease(const SDL_MouseButtonEvent &,
                             const bool inObject) {
    _state = inObject ? InteractionState::Hovered : InteractionState::Normal;
    return inObject ? EventResult::Consumed : EventResult::Handled;
  }

  EventResult onMouseWheel(const SDL_MouseWheelEvent &) {
    return EventResult::Ignored;
  }

  EventResult resetInteraction() {
    _state = InteractionState::Normal;
    return EventResult::Handled;
  }
};
