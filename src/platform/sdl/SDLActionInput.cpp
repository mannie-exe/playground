#include <platform/sdl/SDLActionInput.hpp>

namespace playground::sdl {
std::optional<input::InputEvent> toActionInput(const SDL_Event &e) {
  using namespace input;
  switch (e.type) {
  case SDL_EVENT_KEY_DOWN:
  case SDL_EVENT_KEY_UP:
    return InputEvent{{ControlKind::Key, e.key.scancode, e.key.which},
                      e.key.down ? 1.0f : 0.0f,
                      e.key.repeat};
  case SDL_EVENT_MOUSE_BUTTON_DOWN:
  case SDL_EVENT_MOUSE_BUTTON_UP:
    if (e.button.which == SDL_TOUCH_MOUSEID)
      return {};
    return InputEvent{
        {ControlKind::MouseButton, e.button.button, e.button.which},
        e.button.down ? 1.0f : 0.0f};
  case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
  case SDL_EVENT_GAMEPAD_BUTTON_UP:
    return InputEvent{
        {ControlKind::GamepadButton, e.gbutton.button, e.gbutton.which},
        e.gbutton.down ? 1.0f : 0.0f};
  case SDL_EVENT_GAMEPAD_AXIS_MOTION:
    return InputEvent{{ControlKind::GamepadAxis, e.gaxis.axis, e.gaxis.which},
                      e.gaxis.value /
                          (e.gaxis.value < 0 ? 32768.0f : 32767.0f)};
  default:
    return {};
  }
}

void cancelActionInput(input::InputMap &map, const SDL_Event &e) {
  using input::ControlKind;
  switch (e.type) {
  case SDL_EVENT_WINDOW_FOCUS_LOST:
  case SDL_EVENT_DID_ENTER_BACKGROUND:
    map.cancelAll();
    break;
  case SDL_EVENT_GAMEPAD_REMOVED:
    map.cancelDevice(ControlKind::GamepadButton, e.gdevice.which);
    map.cancelDevice(ControlKind::GamepadAxis, e.gdevice.which);
    break;
  case SDL_EVENT_KEYBOARD_REMOVED:
    map.cancelDevice(ControlKind::Key, e.kdevice.which);
    break;
  case SDL_EVENT_MOUSE_REMOVED:
    map.cancelDevice(ControlKind::MouseButton, e.mdevice.which);
    break;
  default:
    break;
  }
}
} // namespace playground::sdl
