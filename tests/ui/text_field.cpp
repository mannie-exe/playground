#include <memory>
#include <string>

#include <app/TTFGuard.hpp>
#include <platform/sdl/SurfacePainter.hpp>
#include <support/AssetRegistry.hpp>
#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/controls/TextField.hpp>

using namespace playground;

int main() {
  return test::run([] {
    TTFGuard ttf;
    AssetRegistry assets;
    const auto font =
        assets.getFont({.path = std::string{PLAYGROUND_SOURCE_DIR} +
                                "/assets/fonts/LBRITE.TTF",
                        .style = {.size = 20}});
    ui::UIRoot root;
    auto field = std::make_unique<ui::TextField>(
        ui::TextFieldProps{.font = font, .name = "Name"}, "office a\xCC\x81");
    auto *raw = field.get();
    int notifications{};
    auto connection =
        raw->onValueChanged([&](std::string) { ++notifications; });
    root.setContent(std::move(field));
    root.flushLayout({280, 48});
    root.requestFocus(raw->id());
    test::require(root.inputClaims().keyboard,
                  "font-backed text field claims physical typing input");
    test::require(raw->textInputState().caret.h() > 0,
                  "font-backed caret geometry");
    root.performAction(raw->id(), ui::TextSelection{7, 10},
                       ui::ActionSource::Assistive);
    ui::UIEvent composition{.type = ui::EventType::TextEditing, .text = "x"};
    root.dispatch(composition);
    test::require(raw->model().value() == "office a\xCC\x81" &&
                      notifications == 0,
                  "composition isolated from committed value");
    ui::UIEvent cancel{.type = ui::EventType::InputCancel};
    root.dispatch(cancel);
    test::require(
        !raw->textInputState().composing,
        "input cancellation clears IME composition without committing");
    ui::UIEvent input{.type = ui::EventType::TextInput, .text = "x"};
    root.dispatch(input);
    test::require(raw->model().value() == "office x" && notifications == 1,
                  "committed text replaces selection once");
    root.flushLayout({280, 48});
    auto snapshot = root.semanticSnapshot();
    const auto &state = snapshot.nodes.front().state;
    test::require(state.description.value == "office x" &&
                      !state.textRuns.empty(),
                  "accessible layout publishes text runs");
    for (const auto &run : state.textRuns) {
      test::require(run.byteOffsets.size() == run.positions.size() + 1 &&
                        run.positions.size() == run.widths.size(),
                    "character arrays agree");
      test::require(math::isFinite(run.bounds), "finite run geometry");
    }
    SDLResource<SDL_Surface, SDL_DestroySurface> surface{
        SDL_CreateSurface(300, 140, SDL_PIXELFORMAT_RGBA32)};
    test::require(bool(surface), "offscreen surface");
    sdl::SurfacePainter painter{*surface};
    root.prepare({});
    root.render(painter);
    std::string clipboard;
    root.services().writeClipboard = [&](std::string_view text) {
      clipboard = text;
    };
    root.services().readClipboard = [&] { return clipboard; };
    root.performAction(raw->id(), ui::TextSelection{0, 6},
                       ui::ActionSource::Assistive);
    ui::UIEvent copy{.type = ui::EventType::KeyDown,
                     .logicalKey = ui::Key::C,
                     .control = true,
                     .command = true};
    root.dispatch(copy);
    test::require(clipboard == "office",
                  "copy uses injected clipboard, not global desktop");
    auto p = raw->props();
    p.editing.password = true;
    raw->setProps(p);
    root.flushLayout({280, 48});
    snapshot = root.semanticSnapshot();
    test::require(snapshot.nodes.front().state.description.value == "" &&
                      snapshot.nodes.front().state.textRuns.empty() &&
                      !snapshot.nodes.front().state.selection,
                  "password semantic snapshot has no text or offsets");
    clipboard = "unchanged";
    copy.handled = false;
    root.dispatch(copy);
    test::require(clipboard == "unchanged", "password copy blocked");
    p.editing.password = false;
    p.editing.readOnly = true;
    raw->setProps(p);
    input.handled = false;
    root.dispatch(input);
    test::require(raw->model().value() == "office x",
                  "read-only rejects committed text");
    ui::UIEvent blur{.type = ui::EventType::FocusLost};
    root.dispatch(blur);
    test::require(raw->model().composition().empty() && !raw->hasFocus(),
                  "blur cancels editing session");
    auto area = std::make_unique<ui::TextArea>(
        ui::TextFieldProps{.font = font},
        "abc \xD7\x90\xD7\x91\xD7\x92\nline two");
    auto *multiline = area.get();
    root.setContent(std::move(area));
    root.flushLayout({85, 120});
    root.prepare({});
    root.render(painter);
    snapshot = root.semanticSnapshot();
    test::require(snapshot.nodes.front().state.textRuns.size() > 1 &&
                      multiline->textInputState().multiline,
                  "wrapped mixed bidi multiline layout");
    auto number = std::make_unique<ui::NumberField>(
        ui::TextFieldProps{.font = font},
        ui::NumberFieldProps{.range = {2, 0, 10, 1}, .integer = true});
    auto *numeric = number.get();
    root.setContent(std::move(number));
    root.flushLayout({200, 50});
    root.requestFocus(numeric->id());
    numeric->setValue("-");
    ui::UIEvent commit{.type = ui::EventType::KeyDown,
                       .logicalKey = ui::Key::Enter};
    root.dispatch(commit);
    test::require(numeric->numberProps().range.value == 2 &&
                      numeric->semanticState().invalid,
                  "invalid numeric draft preserves committed range");
    numeric->setValue("7");
    commit.handled = false;
    root.dispatch(commit);
    test::require(numeric->numberProps().range.value == 7 &&
                      !numeric->semanticState().invalid,
                  "valid numeric commit");
  });
}
