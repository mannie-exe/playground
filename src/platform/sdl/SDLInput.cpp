#include <platform/sdl/SDLInput.hpp>

namespace playground::sdl {

ui::Key toUIKey(SDL_Keycode value) {
  switch (value) {
  case SDLK_PAGEUP:
    return ui::Key::PageUp;
  case SDLK_PAGEDOWN:
    return ui::Key::PageDown;
  case SDLK_APPLICATION:
    return ui::Key::ContextMenu;
  case SDLK_HOME:
    return ui::Key::Home;
  case SDLK_END:
    return ui::Key::End;
  case SDLK_BACKSPACE:
    return ui::Key::Backspace;
  case SDLK_DELETE:
    return ui::Key::Delete;
  case SDLK_A:
    return ui::Key::A;
  case SDLK_C:
    return ui::Key::C;
  case SDLK_V:
    return ui::Key::V;
  case SDLK_X:
    return ui::Key::X;
  case SDLK_Y:
    return ui::Key::Y;
  case SDLK_Z:
    return ui::Key::Z;
  case SDLK_SPACE:
    return ui::Key::Space;
  case SDLK_RETURN:
  case SDLK_KP_ENTER:
    return ui::Key::Enter;
  case SDLK_TAB:
    return ui::Key::Tab;
  case SDLK_ESCAPE:
    return ui::Key::Escape;
  case SDLK_LEFT:
    return ui::Key::Left;
  case SDLK_RIGHT:
    return ui::Key::Right;
  case SDLK_UP:
    return ui::Key::Up;
  case SDLK_DOWN:
    return ui::Key::Down;
  default:
    return ui::Key::Unknown;
  }
}

std::optional<ui::UIEvent> toUIEvent(const SDL_Event &event,
                                     math::Size2 windowSize) {
  ui::UIEvent result;
  switch (event.type) {
  case SDL_EVENT_GAMEPAD_REMOVED:
  case SDL_EVENT_KEYBOARD_REMOVED:
  case SDL_EVENT_MOUSE_REMOVED:
  case SDL_EVENT_DID_ENTER_BACKGROUND:
    result.type = ui::EventType::InputCancel;
    break;
  case SDL_EVENT_TEXT_INPUT:
    result.type = ui::EventType::TextInput;
    if (event.text.text)
      result.text = event.text.text;
    break;
  case SDL_EVENT_TEXT_EDITING:
    result.type = ui::EventType::TextEditing;
    if (event.edit.text)
      result.text = event.edit.text;
    result.compositionStart = event.edit.start;
    result.compositionLength = event.edit.length;
    break;
  case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
  case SDL_EVENT_GAMEPAD_BUTTON_UP:
    result.source = ui::ActionSource::Gamepad;
    result.type = event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN
                      ? ui::EventType::KeyDown
                      : ui::EventType::KeyUp;
    switch (event.gbutton.button) {
    case SDL_GAMEPAD_BUTTON_SOUTH:
      result.logicalKey = ui::Key::Enter;
      break;
    case SDL_GAMEPAD_BUTTON_EAST:
      result.logicalKey = ui::Key::Escape;
      break;
    case SDL_GAMEPAD_BUTTON_DPAD_UP:
      result.logicalKey = ui::Key::Up;
      break;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
      result.logicalKey = ui::Key::Down;
      break;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
      result.logicalKey = ui::Key::Left;
      break;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
      result.logicalKey = ui::Key::Right;
      break;
    default:
      return {};
    }
    break;
  case SDL_EVENT_MOUSE_MOTION:
    if (event.motion.which == SDL_TOUCH_MOUSEID)
      return {};
    result.type = ui::EventType::PointerMove;
    result.position = {event.motion.x, event.motion.y};
    result.delta = {event.motion.xrel, event.motion.yrel};
    result.pointer = event.motion.which;
    break;
  case SDL_EVENT_MOUSE_BUTTON_DOWN:
  case SDL_EVENT_MOUSE_BUTTON_UP:
    if (event.button.which == SDL_TOUCH_MOUSEID)
      return {};
    result.type = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN
                      ? ui::EventType::PointerDown
                      : ui::EventType::PointerUp;
    result.position = {event.button.x, event.button.y};
    result.pointer = event.button.which;
    result.button = event.button.button;
    break;
  case SDL_EVENT_MOUSE_WHEEL: {
    result.type = ui::EventType::Wheel;
    result.position = {event.wheel.mouse_x, event.wheel.mouse_y};
    result.pointer = event.wheel.which;
    const float sign =
        event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.0f : 1.0f;
    result.delta = {event.wheel.x * sign, -event.wheel.y * sign};
    break;
  }
  case SDL_EVENT_KEY_DOWN:
  case SDL_EVENT_KEY_UP:
    result.type = event.type == SDL_EVENT_KEY_DOWN ? ui::EventType::KeyDown
                                                   : ui::EventType::KeyUp;
    result.key = static_cast<int>(event.key.key);
    result.logicalKey = toUIKey(event.key.key);
    if (event.key.key == SDLK_F10 && (event.key.mod & SDL_KMOD_SHIFT))
      result.logicalKey = ui::Key::ContextMenu;
    result.repeat = event.key.repeat;
    result.shift = (event.key.mod & SDL_KMOD_SHIFT) != 0;
    result.control = (event.key.mod & SDL_KMOD_CTRL) != 0;
    result.alt = (event.key.mod & SDL_KMOD_ALT) != 0;
    result.command = (event.key.mod & SDL_KMOD_GUI) != 0;
    break;
  case SDL_EVENT_WINDOW_FOCUS_LOST:
    result.type = ui::EventType::FocusLost;
    break;
  case SDL_EVENT_WINDOW_FOCUS_GAINED:
    result.type = ui::EventType::FocusGained;
    break;
  case SDL_EVENT_WINDOW_MOUSE_LEAVE:
    result.type = ui::EventType::PointerLeave;
    break;
  case SDL_EVENT_FINGER_DOWN:
  case SDL_EVENT_FINGER_MOTION:
  case SDL_EVENT_FINGER_UP:
  case SDL_EVENT_FINGER_CANCELED:
    result.type =
        event.type == SDL_EVENT_FINGER_DOWN       ? ui::EventType::PointerDown
        : event.type == SDL_EVENT_FINGER_UP       ? ui::EventType::PointerUp
        : event.type == SDL_EVENT_FINGER_CANCELED ? ui::EventType::PointerCancel
                                                  : ui::EventType::PointerMove;
    result.pointer = static_cast<std::uint64_t>(event.tfinger.fingerID) |
                     (std::uint64_t{1} << 63);
    result.button = 1;
    result.position = {event.tfinger.x * windowSize.width,
                       event.tfinger.y * windowSize.height};
    result.delta = {event.tfinger.dx * windowSize.width,
                    event.tfinger.dy * windowSize.height};
    break;
  default:
    return {};
  }
  return result;
}

} // namespace playground::sdl
