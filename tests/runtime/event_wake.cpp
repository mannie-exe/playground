#include <functional>
#include <thread>

#include <SDL3/SDL_events.h>

#include <app/SDLGuard.hpp>
#include <platform/sdl/EventWake.hpp>
#include <support/Test.hpp>

using namespace playground;

int main() {
  return test::run([] {
    SDLGuard sdl{SDL_INIT_EVENTS};
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
    std::function<void()> late;
    {
      sdl::EventWake wake;
      late = wake.callback();
      std::jthread worker{[&] {
        for (int i = 0; i < 20; ++i)
          late();
      }};
      worker.join();
      test::require(SDL_WaitEventTimeout(nullptr, 10),
                    "wake remains queued when waiting without consuming");
      SDL_Event event{};
      test::require(SDL_PollEvent(&event) && wake.consume(event),
                    "owner recognizes queued wake");
      test::require(!SDL_PollEvent(nullptr),
                    "repeated notifications are coalesced");
      late();
      test::require(SDL_PollEvent(&event) && wake.consume(event),
                    "drained wake rearms future notifications");
      SDL_SetEventFilter(
          [](void *data, SDL_Event *) -> bool {
            (*static_cast<std::function<void()> *>(data))();
            return false;
          },
          &late);
      late();
      SDL_SetEventFilter(nullptr, nullptr);
      test::require(!SDL_PollEvent(nullptr),
                    "filtered wake is not spuriously considered delivered");
      late();
      test::require(SDL_PollEvent(&event) && wake.consume(event),
                    "failed delivery can be retried without deadlock");
    }
    late();
    test::require(!SDL_PollEvent(nullptr),
                  "late endpoint cannot access a destroyed owner");
  });
}
