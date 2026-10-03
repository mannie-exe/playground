#include <SDL3/SDL_scancode.h>

#include <platform/sdl/SDLActionInput.hpp>
#include <support/Test.hpp>

using playground::test::require;

int main() {
  return playground::test::run([] {
    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.scancode = SDL_SCANCODE_A;
    event.key.down = true;
    auto action = playground::sdl::toActionInput(event);
    require(action && action->control.code == SDL_SCANCODE_A &&
                action->value == 1,
            "physical scancode translation");
    event = {};
    event.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
    event.gaxis.value = -32768;
    require(playground::sdl::toActionInput(event)->value == -1,
            "negative axis normalized");
    event.gaxis.value = 32767;
    require(playground::sdl::toActionInput(event)->value == 1,
            "positive axis normalized");
    event.type = SDL_EVENT_TEXT_INPUT;
    require(!playground::sdl::toActionInput(event),
            "IME separate from actions");
    playground::input::InputMap map;
    map.addContext({.name = "test"}, {{.action = "a", .code = SDL_SCANCODE_A}});
    map.route(*action, playground::input::InputStage::BeforeUI);
    map.route(*action, playground::input::InputStage::AfterUI);
    event.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    playground::sdl::cancelActionInput(map, event);
    const auto canceled = map.takeTickSnapshot()["a"];
    require(canceled.canceled && !canceled.pressed && !canceled.held,
            "focus loss cancels latched input");
    event = {};
    event.type = SDL_EVENT_MOUSE_WHEEL;
    event.wheel.y = .25f;
    event.wheel.direction = SDL_MOUSEWHEEL_FLIPPED;
    require(playground::sdl::toActionInput(event)->displacement.y == -.25f,
            "fractional flipped wheel normalized once");
    event = {};
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.xrel = 17;
    event.motion.yrel = -2;
    require(playground::sdl::toActionInput(event)->displacement.x == 17,
            "relative pointer is unbounded displacement");
  });
}
