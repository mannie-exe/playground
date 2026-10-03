#include <cmath>

#include <SDL3/SDL.h>

#include <platform/sdl/SDLInput.hpp>
#include <platform/sdl/SoftwareSceneRenderer.hpp>
#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/content/InspectionView.hpp>

using namespace playground;
using test::require;

int main() {
  return test::run([] {
    const world::SpaceId space{{1}, 1};
    const scene::OrbitProps orbit{
        .target = {space, {}}, .epoch = 1, .distance = 10};
    const world::RenderOrigin origin{{space, {}}, 1};
    auto view = std::make_unique<ui::InspectionView>(
        ui::SceneViewProps{
            .scene = std::make_shared<scene::Scene3D>(),
            .camera =
                scene::OrbitController{orbit}.camera().localCamera(origin, {}),
            .preferredSize = {200, 100},
            .aspectRatio = 1},
        orbit);
    auto *inspection = view.get();
    unsigned changed{};
    auto connection =
        inspection->onCameraChanged([&](scene::WorldCamera camera) {
          ++changed;
          auto props = inspection->props();
          props.camera = camera.localCamera(origin, {});
          inspection->setProps(props);
        });
    ui::UIRoot root;
    root.setContent(std::move(view));
    root.flushLayout(math::Size2{200, 100});
    sdl::SoftwareSceneRenderer renderer;
    root.prepare({.scenes = &renderer});
    const auto send = [&](ui::UIEvent event) {
      root.dispatch(event);
      return event.handled;
    };
    require(!send({.type = ui::EventType::Wheel,
                   .position = {5, 50},
                   .delta = {0, -1}}) &&
                !changed,
            "letterbox wheel remains available to surrounding UI");
    require(send({.type = ui::EventType::Wheel,
                  .position = {100, 50},
                  .delta = {0, -1}}) &&
                inspection->orbitProps().distance < 10 &&
                !inspection->isNavigating(),
            "hover wheel zoom needs no persistent capture or engagement");
    const auto zoom = inspection->orbitProps().distance;
    require(send({.type = ui::EventType::PointerDown,
                  .position = {100, 50},
                  .pointer = 7,
                  .button = 2,
                  .shift = true}) &&
                inspection->isNavigating(),
            "shift middle press begins scoped pan");
    send({.type = ui::EventType::PointerMove,
          .position = {130, 60},
          .pointer = 8});
    require(inspection->orbitProps().target == orbit.target,
            "other pointers cannot drive a captured gesture");
    send({.type = ui::EventType::PointerMove,
          .position = {230, 60},
          .pointer = 7});
    require(inspection->orbitProps().target != orbit.target &&
                inspection->orbitProps().yaw == 0 &&
                inspection->orbitProps().distance == zoom,
            "captured pan continues outside content and keeps press-time "
            "modifiers");
    require(
        send({.type = ui::EventType::KeyDown, .logicalKey = ui::Key::Escape}) &&
            !inspection->isNavigating() &&
            root.inputClaims().capturedPointers.empty(),
        "Escape consumes and ends inspection before host Settings");
    require(send({.type = ui::EventType::KeyDown, .logicalKey = ui::Key::R}) &&
                inspection->orbitProps().target == orbit.target &&
                inspection->orbitProps().distance == zoom,
            "reset restores authored pivot while preserving zoom");
    send({.type = ui::EventType::PointerDown,
          .position = {100, 50},
          .pointer = 7,
          .button = 1,
          .alt = true});
    send({.type = ui::EventType::PointerMove,
          .position = {120, 60},
          .pointer = 7});
    require(inspection->orbitProps().yaw != 0 &&
                inspection->orbitProps().pitch != 0,
            "Alt-primary drag provides orbit without a middle button");
    send({.type = ui::EventType::FocusLost});
    require(!inspection->isNavigating() &&
                root.inputClaims().capturedPointers.empty(),
            "focus loss cancels pointer ownership");
    const auto before = inspection->camera();
    send({.type = ui::EventType::PointerMove,
          .position = {140, 60},
          .pointer = 7});
    require(inspection->camera() == before,
            "focus loss cannot resume a drag implicitly");
    send({.type = ui::EventType::PointerDown,
          .position = {100, 50},
          .pointer = 7,
          .button = 2,
          .control = true});
    send({.type = ui::EventType::PointerMove,
          .position = {100, 70},
          .pointer = 7});
    require(inspection->orbitProps().distance > zoom,
            "control middle drag zooms without panning");
    require(send({.type = ui::EventType::PointerUp,
                  .position = {250, 70},
                  .pointer = 7,
                  .button = 2}) &&
                !inspection->isNavigating() &&
                root.inputClaims().capturedPointers.empty(),
            "release outside the viewport ends pointer ownership");
    send({.type = ui::EventType::PointerDown,
          .position = {100, 50},
          .pointer = 7,
          .button = 2,
          .shift = true});
    send({.type = ui::EventType::PointerMove,
          .position = {110, 60},
          .pointer = 7});
    const auto rotated = inspection->orbitProps();
    send({.type = ui::EventType::KeyDown, .logicalKey = ui::Key::R});
    require(inspection->orbitProps().target == orbit.target &&
                inspection->orbitProps().yaw == rotated.yaw &&
                inspection->orbitProps().pitch == rotated.pitch &&
                inspection->orbitProps().distance == rotated.distance &&
                !inspection->isNavigating(),
            "reset cancels active pan and preserves orientation and zoom");
    send({.type = ui::EventType::PointerDown,
          .position = {100, 50},
          .pointer = 7,
          .button = 2});
    root.setContent(nullptr);
    require(root.inputClaims().capturedPointers.empty(),
            "detachment releases capture");

    require(SDL_Init(0), "SDL input adapter initialization");
    struct Quit {
      ~Quit() {
        SDL_SetModState(SDL_KMOD_NONE);
        SDL_Quit();
      }
    } quit;
    SDL_SetModState(SDL_KMOD_SHIFT | SDL_KMOD_ALT);
    SDL_Event mouse{};
    mouse.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    mouse.button.button = SDL_BUTTON_LEFT;
    const auto event = sdl::toUIEvent(mouse, {200, 100});
    require(event && event->shift && event->alt && !event->control,
            "SDL pointer adapter retains modifier state for UI gestures");
    require(sdl::toUIKey(SDLK_R) == ui::Key::R,
            "inspection reset uses a logical UI key");
  });
}
