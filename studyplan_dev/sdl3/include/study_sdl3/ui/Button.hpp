#pragma once

#include <memory>
#include <utility>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_mouse.h>

#include <study_sdl3/interfaces/IDisplayObject.hpp>
#include <study_sdl3/interfaces/IInteractable.hpp>
#include <study_sdl3/ui/DisplayImage.hpp>
#include <study_sdl3/ui/DisplayText.hpp>
#include <study_sdl3/ui/EventTypes.hpp>
#include <study_sdl3/ui/InteractionState.hpp>
#include <study_sdl3/ui/Rectangle.hpp>

struct ButtonStyle {
  SDL_Color baseColor{55, 82, 61, 255};
  SDL_Color hoverColor{131, 183, 141, 255};
  SDL_Color activeColor{117, 133, 120, 255};
  SDL_Color disabledColor{96, 96, 96, 255};
};

struct ButtonContent {
  std::unique_ptr<DisplayText> label;
  std::unique_ptr<DisplayImage> icon;
  // TODO: positioning/alignment, sizing, and rendering options
};

class Button : public IDisplayObject {
protected:
  Rectangle _background;
  IInteractable &_parent;
  ButtonStyle _style;
  InteractionState _state{InteractionState::Normal};
  ButtonContent _content;

public:
  Button(const RectTransform transform, IInteractable &parent,
         const ButtonStyle style = {}, ButtonContent content = {})
      : IDisplayObject{transform}, _background{transform}, _parent{parent},
        _style{style}, _content{std::move(content)} {}

  ButtonStyle getStyle() const { return _style; }
  InteractionState getState() const { return _state; }
  bool isDisabled() const { return _state == InteractionState::Disabled; }
  DisplayImage *getIcon() { return _content.icon.get(); }
  const DisplayImage *getIcon() const { return _content.icon.get(); }
  DisplayText *getLabel() { return _content.label.get(); }
  const DisplayText *getLabel() const { return _content.label.get(); }

  void setStyle(const ButtonStyle &style) { _style = style; }
  void setDisabled(bool disabled = true) {
    _state = disabled ? InteractionState::Disabled : InteractionState::Normal;
  }
  void setEnabled(bool enabled = true) { setDisabled(!enabled); }
  void setIcon(DisplayImage icon) {
    _content.icon = std::make_unique<DisplayImage>(std::move(icon));
  }
  void setLabel(DisplayText label) {
    _content.label = std::make_unique<DisplayText>(std::move(label));
  }

  virtual EventResult handleEvent(const SDL_Event &event) override {
    if (!isVisible() || _state == InteractionState::Disabled)
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

  void showIcon(const bool show = true) {
    if (!_content.icon)
      return;
    _content.icon->show(show);
  }

  void showLabel(const bool show = true) {
    if (!_content.label)
      return;
    _content.label->show(show);
  }

  void hideIcon(const bool hide = true) { showIcon(!hide); }
  void hideLabel(const bool hide = true) { showLabel(!hide); }

  virtual void publishEventParent(const SDL_Event &event) {
    _parent.handleEvent(event);
  }

  virtual void publishEventGlobal(const SDL_Event &event) {
    SDL_Event eventCopy{event};
    SDL_PushEvent(&eventCopy);
  }

  void renderWithStyle(SDL_Surface &targetSurface, const ButtonStyle &style) {
    if (!isVisible())
      return;
    const SDL_Color &color =
        _state == InteractionState::Disabled ? style.disabledColor
        : (_state == InteractionState::Pressed ||
           _state == InteractionState::PressedOutside)
            ? style.activeColor
        : _state == InteractionState::Hovered ? style.hoverColor
                                              : style.baseColor;
    _background.render(targetSurface, color);
    if (_content.icon)
      _content.icon->render(targetSurface);
    if (_content.label)
      _content.label->render(targetSurface);
  }

  virtual void render(SDL_Surface &targetSurface) override {
    if (!isVisible())
      return;
    renderWithStyle(targetSurface, _style);
  }

  virtual EventResult onKey(const SDL_KeyboardEvent &) {
    return EventResult::Ignored;
  }

  virtual EventResult onMouseMove(const SDL_MouseMotionEvent &) {
    return EventResult::Ignored;
  }

  virtual EventResult onMouseEnter(const float, const float) {
    if (_state == InteractionState::PressedOutside)
      _state = InteractionState::Pressed;
    else
      _state = InteractionState::Hovered;
    return EventResult::Handled;
  }

  virtual EventResult onMouseExit(const float, const float) {
    if (_state == InteractionState::Pressed)
      _state = InteractionState::PressedOutside;
    else
      _state = InteractionState::Normal;
    return EventResult::Handled;
  }

  virtual EventResult onMouseClick(const SDL_MouseButtonEvent &mButtonEvent) {
    if (mButtonEvent.button != SDL_BUTTON_LEFT)
      return EventResult::Ignored;

    _state = InteractionState::Pressed;
    publishEventGlobal(
        {.user = {.type = study_sdl3::ui::events::buttonPressed(),
                  .data1 = this}});
    return EventResult::Consumed;
  }

  virtual EventResult onMouseRelease(const SDL_MouseButtonEvent &,
                                     const bool inObject) {
    _state = inObject ? InteractionState::Hovered : InteractionState::Normal;
    return inObject ? EventResult::Consumed : EventResult::Handled;
  }

  virtual EventResult onMouseWheel(const SDL_MouseWheelEvent &) {
    return EventResult::Ignored;
  }

  EventResult resetInteraction() {
    _state = InteractionState::Normal;
    return EventResult::Handled;
  }
};
