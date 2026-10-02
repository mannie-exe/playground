#include <filesystem>
#include <set>

#include <app/Assets.hpp>
#include <app/TTFGuard.hpp>
#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/content/Text.hpp>
#include <ui/controls/TextField.hpp>

using namespace playground;

int main() {
  return test::run([] {
    TTFGuard ttf;
    AssetRegistry cache;
    auto catalog = std::make_shared<assets::AssetCatalog>(
        std::filesystem::path{PLAYGROUND_SOURCE_DIR} / "assets");
    app::registerAssets(*catalog);
    catalog->freeze();
    sdl::AssetResources resources{catalog, cache};
    auto definition = ui::defaultThemeDefinition();
    app::configureThemeFonts(definition.typography, resources);
    definition.validate();
    test::require(app::themeFontAssets().size() == 112,
                  "all static face variants registered");
    std::set<std::string> paths;
    for (const auto &asset : app::themeFontAssets()) {
      const auto &family =
          *definition.typography.families[static_cast<unsigned>(asset.family)];
      const auto &face = ui::selectFontFace(family, asset.selection);
      test::require(
          face.id == asset.id.value && paths.insert(face.path).second,
          "every registered face has a unique source and exact selection");
      auto font =
          ui::resolveThemeFont(definition.typography, ui::TextRole::Body, {},
                               &cache, asset.family, asset.selection);
      test::require(font->getInfo().weight == asset.selection.weight,
                    "registered weight agrees with font metadata: " + face.id);
      test::require(font->getPath() == face.path &&
                        font->props().style.flags == TTF_STYLE_NORMAL,
                    "selected native face is not synthetically styled");
      int w{}, h{};
      test::require(TTF_GetStringSize(font->get(), "Aa 0123", 0, &w, &h) &&
                        w > 0 && h > 0,
                    "every bundled face loads and measures");
    }
    const auto &serif =
        *definition.typography
             .families[static_cast<unsigned>(ui::FontFamily::Serif)];
    test::require(ui::selectFontFace(serif, {.weight = 500}).selection.weight ==
                      400,
                  "missing intermediate weight uses lighter tie");
    const auto &mono =
        *definition.typography
             .families[static_cast<unsigned>(ui::FontFamily::Monospace)];
    test::require(
        ui::selectFontFace(
            mono, {.weight = 900, .opticalSize = ui::FontOpticalSize::Display})
                .selection == ui::FontSelection{800},
        "missing optical design falls back to text and nearest weight");
    auto invalid = serif.faces;
    invalid.push_back(invalid.front());
    test::rejects(
        [&] { ui::FontFamilyDefinition duplicate{serif.name, invalid}; },
        "duplicate face metadata rejected");
    test::rejects([&] { ui::selectFontFace(serif, {.weight = 0}); },
                  "invalid weight rejected");
    const auto previous = definition.typography.families;
    app::configureThemeFonts(definition.typography, resources);
    test::require(previous == definition.typography.families,
                  "host preserves configured families");
    auto label = ui::resolveThemeFont(definition.typography,
                                      ui::TextRole::Label, {}, &cache);
    auto value = ui::resolveThemeFont(definition.typography,
                                      ui::TextRole::Value, {}, &cache);
    auto code = ui::resolveThemeFont(definition.typography, ui::TextRole::Code,
                                     {}, &cache);
    test::require(
        label->getPath().ends_with("InterDisplay-Regular.ttf") &&
            value->getPath().ends_with("Inter-Regular.ttf") &&
            code->getInfo().monospaced,
        "default labels, values and diagnostics select agreed families");
    test::require(label == ui::resolveThemeFont(definition.typography,
                                                ui::TextRole::Label, {},
                                                &cache),
                  "theme face resolution shares cached variants");
    ui::UIRoot root;
    root.setThemeDefinition(definition);
    auto text = std::make_unique<ui::Text>(
        cache, ui::TextProps{.value = "Serif override",
                             .textRole = ui::TextRole::Title,
                             .fontFamily = ui::FontFamily::Serif,
                             .fontSelection = ui::FontSelection{
                                 .weight = 600,
                                 .slant = ui::FontSlant::Italic,
                                 .opticalSize = ui::FontOpticalSize::Subhead}});
    auto *content = text.get();
    root.setContent(std::move(text));
    root.flushLayout({400, 80});
    test::require(content->effectiveFont()->getPath().ends_with(
                      "SourceSerif4Subhead-SemiboldIt.ttf") &&
                      content->effectiveFont()->getSize() == 28,
                  "instance family and face override retains semantic size");
    content->applyPatch(
        {.fontFamily = Patch<std::optional<ui::FontFamily>>::reset(),
         .fontSelection = Patch<std::optional<ui::FontSelection>>::reset()});
    root.flushLayout({400, 80});
    test::require(
        content->effectiveFont()->getPath().ends_with("InterDisplay-Bold.ttf"),
        "reset resumes theme family and native heading weight");
    auto field = std::make_unique<ui::TextField>(
        ui::TextFieldProps{
            .textRole = ui::TextRole::Value,
            .fontFamily = ui::FontFamily::Serif,
            .fontSelection = ui::FontSelection{.slant = ui::FontSlant::Italic}},
        "draft");
    auto *editor = field.get();
    root.setContent(std::move(field));
    root.flushLayout({400, 80});
    editor->requestFocus();
    const auto caret = editor->textInputState().caret;
    auto props = editor->props();
    props.editing.readOnly = true;
    editor->setProps(props);
    root.flushLayout({400, 80});
    test::require(editor->textInputState().caret == caret &&
                      editor->model().value() == "draft",
                  "read-only transition retains instance typography and draft");
    definition.typography.textScale = 1.5f;
    root.setThemeDefinition(definition);
    root.flushLayout({400, 100});
    test::require(editor->hasFocus() && editor->model().value() == "draft" &&
                      editor->textInputState().caret.h() > caret.h(),
                  "font reflow preserves editor state");
  });
}
