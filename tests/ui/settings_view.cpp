#include <app/Assets.hpp>
#include <app/TTFGuard.hpp>
#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/views/SettingsView.hpp>
using namespace playground;

int main() {
  return test::run([] {
    TTFGuard ttf;
    AssetRegistry assets;
    auto font = assets.getFont({.path = std::string{PLAYGROUND_SOURCE_DIR} +
                                        "/assets/fonts/LBRITE.TTF",
                                .style = {.size = 18}});
    auto catalog = std::make_shared<assets::AssetCatalog>(
        std::filesystem::path{PLAYGROUND_SOURCE_DIR} / "assets");
    app::registerAssets(*catalog);
    catalog->freeze();
    sdl::AssetResources resources{catalog, assets};
    ui::UIRoot root;
    auto definition = ui::defaultThemeDefinition();
    app::configureThemeFonts(definition.typography, resources);
    root.setThemeDefinition(definition);
    unsigned applied{}, saved{}, closed{}, menu{};
    rendering::GraphicsSettings result;
    auto view = ui::makeSettingsView(assets, font, {},
                                     {.apply =
                                          [&](auto value, bool persist) {
                                            result = value;
                                            ++applied;
                                            saved += persist;
                                          },
                                      .close = [&] { ++closed; },
                                      .returnToMenu = [&] { ++menu; }});
    auto *settings = view.get();
    root.setContent(std::move(view));
    root.flushLayout({900, 720});
    const auto find = [&](std::string_view name) {
      for (const auto &node : root.inspectTree())
        if (node.semantics.name == name &&
            node.semantics.role != ui::SemanticRole::Text)
          return node.id;
      throw std::runtime_error("Missing settings control: " +
                               std::string{name});
    };
    const auto activate = [&](std::string_view name) {
      test::require(root.performAction(find(name), ui::Activate{},
                                       ui::ActionSource::Assistive) ==
                        ui::ActionResult::Applied,
                    "settings action applied");
      root.flushLayout({900, 720});
    };
    root.performAction(find("Whole-frame resolution"), ui::SetValue{75},
                       ui::ActionSource::Assistive);
    test::require(settings->dirty() && applied == 0 &&
                      settings->draft().presentation.resolutionScale == .75f,
                  "draft edit has no runtime side effects");
    activate("Apply");
    test::require(applied == 1 && saved == 0 &&
                      result.presentation.resolutionScale == .75f,
                  "apply stays in memory");
    settings->setResult(result, "Applied");
    test::require(!settings->dirty(), "applied draft acknowledged");
    activate("Save");
    test::require(saved == 1, "save explicit");
    activate("3D");
    root.flushLayout({900, 720});
    test::require(root.performAction(find("Shadow quality (future; inactive)"),
                                     ui::SelectItem{"ultra"},
                                     ui::ActionSource::Assistive) ==
                      ui::ActionResult::Applied,
                  "future control accepts edit");
    activate("Save");
    test::require(result.threeD.shadows == rendering::QualityLevel::Ultra,
                  "future preferences editable and saved");
    rendering::RenderRuntimeSnapshot runtime;
    runtime.resources.memory[1].bytes = 512 * 1024 * 1024;
    settings->setRuntime({}, runtime);
    activate("Resources");
    root.performAction(find("Managed GPU ceiling"), ui::SetValue{128},
                       ui::ActionSource::Assistive);
    const auto beforeRefusal = applied;
    activate("Apply");
    test::require(
        applied == beforeRefusal,
        "settings UI refuses a lower ceiling below current live commitment");
    auto *invalid = dynamic_cast<ui::NumberField *>(
        root.resolve(find("3D scene resolution")));
    test::require(invalid != nullptr, "numeric settings use editable fields");
    invalid->setValue("-");
    const auto beforeInvalid = applied;
    activate("Apply");
    test::require(
        applied == beforeInvalid && invalid->semanticState().invalid &&
            invalid->hasFocus(),
        "submission reveals and focuses invalid field on another tab");
    invalid->revertDraft();
    activate("Close");
    activate("Return to menu");
    test::require(closed == 1 && menu == 1,
                  "navigation provided by host callbacks");
    definition.typography.textScale = 1.5f;
    root.setThemeDefinition(definition);
    root.flushLayout({900, 720});
    const auto tabs = root.resolve(find("Settings categories"));
    test::require(tabs && !tabs->children().empty(),
                  "settings retain tab navigation");
    ui::MeasureContext context;
    for (const auto &tab : tabs->children()) {
      const auto required = tab->measure(context, {{}, {}}).size.height;
      test::require(tab->bounds().h() >= required,
                    "enlarged settings text cannot collapse tab labels");
    }
  });
}
