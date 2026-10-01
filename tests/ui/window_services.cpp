#include <memory>
#include <string>
#include <string_view>

#include <SDL3/SDL.h>

#include <app/SDLGuard.hpp>
#include <app/TTFGuard.hpp>
#include <platform/sdl/WindowServices.hpp>
#include <support/AssetRegistry.hpp>
#include <support/SDLResource.hpp>
#include <support/Test.hpp>
#include <ui/controls/Button.hpp>
#include <ui/controls/TextField.hpp>

using namespace playground;

int main() {
  bool unsupported{};
  const auto result = test::run([&] {
    SDLGuard sdl{SDL_INIT_VIDEO};
    TTFGuard ttf;
    AssetRegistry assets;
    SDLResource<SDL_Window, SDL_DestroyWindow> window{SDL_CreateWindow(
        "accessibility adapter constituent", 200, 100, SDL_WINDOW_HIDDEN)};
    test::require(bool(window), "hidden native window");
    const std::string_view driver = SDL_GetCurrentVideoDriver();
    // Offscreen drivers have no native accessibility host. Other tests cover
    // core semantics without a desktop; this check exercises native ownership
    // only.
    if (driver == "dummy" || driver == "offscreen") {
      unsupported = true;
      return;
    }
    sdl::WindowServices services{window.get()};
    {
      SDLResource<SDL_Window, SDL_DestroyWindow> second{SDL_CreateWindow(
          "second accessibility host", 100, 100, SDL_WINDOW_HIDDEN)};
      test::require(bool(second), "second hidden native window");
      sdl::WindowServices repeated{second.get()};
    }
    ui::UIRoot root;
    root.setContent(std::make_unique<ui::Button>());
    root.flushLayout({200, 100});
    auto attachment = services.attach(root);
    const platform::ViewportMapping mapping{.logicalSize = {200, 100}};
    services.publish(root, mapping);
    const auto initialPublications = root.stats().publications;
    services.publish(root, mapping);
    test::require(root.stats().publications == initialPublications &&
                      root.stats().publicationHits > 0,
                  "unchanged native publication reuses semantic snapshot");
    auto changedMapping = mapping;
    changedMapping.offset = {5, 7};
    services.publish(root, changedMapping);
    test::require(root.stats().publications == initialPublications + 1,
                  "viewport mapping invalidates native semantic geometry");
    services.publish(root, mapping);
    root.flushLayout({180, 90});
    services.publish(root, mapping);
    test::require(root.stats().publications == initialPublications + 3,
                  "arranged geometry invalidates publication without authored "
                  "props changes");
    services.setMode(ui::AccessibilityMode::Disabled);
    services.publish(root, mapping);
    services.setMode(ui::AccessibilityMode::Enabled);
    services.publish(root, mapping);
    services.pump();
    const auto font =
        assets.getFont({.path = std::string{PLAYGROUND_SOURCE_DIR} +
                                "/assets/fonts/LBRITE.TTF",
                        .style = {.size = 18}});
    auto field = std::make_unique<ui::TextArea>(
        ui::TextFieldProps{.font = font, .name = "Notes"}, "First\nSecond");
    auto *editor = field.get();
    root.setContent(std::move(field));
    root.flushLayout({200, 100});
    services.publish(root, mapping);
    root.requestFocus(editor->id());
    ui::UIEvent composing{.type = ui::EventType::TextEditing,
                          .text = "pending"};
    root.dispatch(composing);
    test::require(editor->textInputState().composing,
                  "composition starts before modal suspension");
    services.cancelInput();
    test::require(
        !editor->textInputState().composing &&
            root.focusedNode() == editor->id(),
        "modal cancellation clears composition but preserves logical focus");
    auto props = editor->props();
    props.enabled = false;
    editor->setProps(props);
    // Losing focus while publishing can discard layout. Do not publish stale
    // selection offsets against missing native text runs.
    services.publish(root, mapping);
    test::require(root.focusedNode() == ui::NodeId{},
                  "disabled editor releases focus");
    root.flushLayout({200, 100});
    services.publish(root, mapping);
    attachment.disconnect();
    root.setContent({});
    ui::UIRoot replacement;
    replacement.setContent(std::make_unique<ui::Button>());
    replacement.flushLayout({200, 100});
    auto next = services.attach(replacement);
    services.publish(replacement, mapping);
    services.pump();
  });
  return result ? result : unsupported ? 77 : 0;
}
