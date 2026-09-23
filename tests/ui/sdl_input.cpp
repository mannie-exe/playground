#include <array>
#include <format>
#include <platform/sdl/EventResult.hpp>
#include <platform/sdl/SDLInput.hpp>
#include <support/Test.hpp>
#include <utility>

using namespace playground;

int main() {
  return test::run([] {
    constexpr std::array results{EventResult::Ignored, EventResult::Handled,
                                 EventResult::Consumed};
    constexpr EventResult combined[3][3]{
        {EventResult::Ignored, EventResult::Handled, EventResult::Consumed},
        {EventResult::Handled, EventResult::Handled, EventResult::Consumed},
        {EventResult::Consumed, EventResult::Consumed, EventResult::Consumed}};
    for (std::size_t i = 0; i < results.size(); ++i) {
      test::require(isTerminal(results[i]) == (i == 2),
                    "only consumed SDL events terminate dispatch");
      for (std::size_t j = 0; j < results.size(); ++j)
        test::require(combine(results[i], results[j]) == combined[i][j],
                      "SDL event results preserve the strongest outcome");
    }
    test::require(std::format("{} {} {}", results[0], results[1], results[2]) ==
                      "Ignored Handled Consumed",
                  "standalone SDL event-result formatting");

    for (auto [key, expected] : std::array<std::pair<SDL_Keycode, ui::Key>, 10>{
             {{SDLK_SPACE, ui::Key::Space},
              {SDLK_RETURN, ui::Key::Enter},
              {SDLK_KP_ENTER, ui::Key::Enter},
              {SDLK_TAB, ui::Key::Tab},
              {SDLK_ESCAPE, ui::Key::Escape},
              {SDLK_LEFT, ui::Key::Left},
              {SDLK_RIGHT, ui::Key::Right},
              {SDLK_UP, ui::Key::Up},
              {SDLK_DOWN, ui::Key::Down},
              {SDLK_A, ui::Key::Unknown}}})
      test::require(sdl::toUIKey(key) == expected, "key translation table");
    for (auto type : {SDL_EVENT_KEY_DOWN, SDL_EVENT_KEY_UP}) {
      SDL_Event event{};
      event.key.type = type;
      event.key.key = SDLK_SPACE;
      event.key.mod = SDL_KMOD_SHIFT;
      event.key.repeat = true;
      const auto result = sdl::toUIEvent(event, {200, 100});
      test::require(result && result->logicalKey == ui::Key::Space &&
                        result->shift && result->repeat,
                    "key metadata retained");
    }
    for (auto type : {SDL_EVENT_FINGER_DOWN, SDL_EVENT_FINGER_MOTION,
                      SDL_EVENT_FINGER_UP, SDL_EVENT_FINGER_CANCELED}) {
      SDL_Event event{};
      event.tfinger.type = type;
      event.tfinger.fingerID = 7;
      event.tfinger.x = 0.25f;
      event.tfinger.y = 0.5f;
      event.tfinger.dx = 0.1f;
      auto result = sdl::toUIEvent(event, {200, 100});
      test::require(result && result->position == math::Point2{50, 50} &&
                        result->delta.x == 20 && result->button == 1,
                    "normalized touch coordinates become window coordinates");
      test::require(result->pointer != 7,
                    "touch and ordinary mouse IDs occupy separate domains");
    }
    for (auto type : {SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_EVENT_MOUSE_BUTTON_UP}) {
      SDL_Event event{};
      event.button.type = type;
      event.button.which = 3;
      event.button.x = 25;
      event.button.y = 15;
      event.button.button = 2;
      auto result = sdl::toUIEvent(event, {200, 100});
      test::require(
          result && result->position == math::Point2{25, 15} &&
              result->button == 2,
          "mouse coordinates stay in window space until viewport mapping");
      event.button.which = SDL_TOUCH_MOUSEID;
      test::require(!sdl::toUIEvent(event, {200, 100}),
                    "synthetic touch mouse click suppressed");
    }
    SDL_Event wheel{};
    wheel.wheel.type = SDL_EVENT_MOUSE_WHEEL;
    wheel.wheel.x = 2;
    wheel.wheel.y = 3;
    test::require(sdl::toUIEvent(wheel, {})->delta == math::Vec2f{2, -3},
                  "wheel vertical direction mapped");
    wheel.wheel.direction = SDL_MOUSEWHEEL_FLIPPED;
    test::require(sdl::toUIEvent(wheel, {})->delta == math::Vec2f{-2, 3},
                  "flipped wheel mapped once");
    SDL_Event unknown{};
    unknown.type = SDL_EVENT_QUIT;
    test::require(!sdl::toUIEvent(unknown, {}),
                  "host event not reinterpreted as UI event");
  });
}
