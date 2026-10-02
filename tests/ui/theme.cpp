#include <limits>
#include <memory>

#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/collections/ScrollView.hpp>
#include <ui/containers/Box.hpp>
#include <ui/containers/Popup.hpp>
#include <ui/containers/Stack.hpp>
#include <ui/controls/Button.hpp>
#include <ui/controls/NumberStepper.hpp>

using namespace playground;

int main() {
  return test::run([] {
    const auto light = ui::resolveTheme(ui::ColorSchemePreference::System,
                                        ui::ContrastPreference::System, {});
    const auto dark =
        ui::resolveTheme(ui::ColorSchemePreference::System,
                         ui::ContrastPreference::System, {.dark = true});
    test::require(light != dark && light == ui::defaultTheme(),
                  "unknown appearance falls back to light normal");
    const auto high = ui::resolveTheme(ui::ColorSchemePreference::Dark,
                                       ui::ContrastPreference::System,
                                       {.dark = false, .highContrast = true});
    test::require(high.highContrast,
                  "app color override preserves system contrast");
    test::require(!ui::resolveTheme(ui::ColorSchemePreference::Dark,
                                    ui::ContrastPreference::Normal,
                                    {.highContrast = true})
                       .highContrast,
                  "explicit user contrast override supported");
    test::rejects(
        [] {
          ui::resolveTheme(static_cast<ui::ColorSchemePreference>(99),
                           ui::ContrastPreference::System, {});
        },
        "invalid theme preference rejected");
    const auto luminance = [](math::ColorRGBA8 c) {
      const auto channel = [](unsigned char v) {
        const double n = v / 255.;
        return n <= .04045 ? n / 12.92 : std::pow((n + .055) / 1.055, 2.4);
      };
      return .2126 * channel(c.r) + .7152 * channel(c.g) + .0722 * channel(c.b);
    };
    const auto contrastRatio = [&](math::ColorRGBA8 a, math::ColorRGBA8 b) {
      const auto x = luminance(a), y = luminance(b);
      return (std::max(x, y) + .05) / (std::min(x, y) + .05);
    };
    for (auto scheme :
         {ui::ColorSchemePreference::Light, ui::ColorSchemePreference::Dark})
      for (auto contrast :
           {ui::ContrastPreference::Normal, ui::ContrastPreference::High}) {
        const auto t = ui::resolveTheme(ui::defaultThemeDefinition(), scheme,
                                        contrast, {});
        for (auto ink : {t.colors.text, t.colors.mutedText, t.colors.error,
                         t.colors.warning, t.colors.success})
          for (auto surface : {t.colors.surface, t.colors.elevated})
            test::require(
                contrastRatio(ink, surface) >= 4.5,
                "built-in semantic text has readable paired surfaces");
        test::require(contrastRatio(t.colors.onAccent, t.colors.accent) >= 4.5,
                      "accent foreground is paired with accent background");
      }
    ui::UIRoot root;
    auto box = std::make_unique<ui::Box>();
    auto *parent = box.get();
    auto child = std::make_unique<ui::Button>();
    auto *button = child.get();
    box->setChild(std::move(child));
    root.setContent(std::move(box));
    root.setTheme(dark);
    test::require(button->theme() == dark, "root theme inherited");
    parent->setTheme(light);
    test::require(button->theme() == light, "local palette override inherited");
    root.setTheme(high);
    test::require(button->theme() == high,
                  "system contrast takes priority over local palette");
    parent->setTheme(std::nullopt);
    test::require(button->theme() == high,
                  "removing override restores current inheritance");
    parent->applyPaintPatch({.themeBackground = Patch<bool>::set(true)});
    test::require(parent->paintStyle().themeBackground,
                  "theme background opt-in");
    parent->applyPaintPatch({.themeBackground = Patch<bool>::reset()});
    test::require(!parent->paintStyle().themeBackground,
                  "default UI remains transparent");

    auto definition = ui::defaultThemeDefinition();
    definition.metrics.padding = 17;
    definition.metrics.stepper = {52, 60, 13};
    root.setThemeDefinition(definition);
    root.setAppearance(ui::ColorSchemePreference::Dark,
                       ui::ContrastPreference::Normal, {});
    button->setControlLayout(ui::ControlLayout::Choice);
    root.flushLayout({300, 200});
    test::require(button->contentInsets().left == 17 &&
                      button->boxProps().padding.left == 0,
                  "theme padding does not overwrite authored layout");
    button->setControlStyle({.padding = math::Insets::all(0)});
    root.flushLayout({300, 200});
    test::require(button->contentInsets().left == 0,
                  "explicit zero overrides inherited control padding");
    button->setControlStyle({});
    const auto invalid = definition;
    definition.metrics.sliderLength = std::numeric_limits<float>::quiet_NaN();
    test::rejects([&] { root.setThemeDefinition(definition); },
                  "invalid metrics rejected");
    test::require(root.themeDefinition() == invalid,
                  "invalid definition preserves published state");
    definition = invalid;
    const math::ColorRGBA8 custom{1, 2, 3, 255};
    test::require(button->resolveColor(&ui::ThemePalette::text, custom) ==
                      custom,
                  "normal appearance accepts explicit color override");
    auto local = definition.metrics;
    local.padding = 23;
    parent->setThemeOverrides({.colors = light, .metrics = local});
    root.setAppearance(ui::ColorSchemePreference::Dark,
                       ui::ContrastPreference::High, {});
    test::require(
        button->resolveColor(&ui::ThemePalette::text, custom) ==
                button->theme().text &&
            button->themeMetrics().padding == 23 &&
            !root.resolvedTheme().forcedColors,
        "increased contrast protects colors while preserving local geometry");
    auto native = high;
    native.text = {240, 255, 200, 255};
    root.setAppearance(ui::ColorSchemePreference::Dark,
                       ui::ContrastPreference::System,
                       {.highContrast = true, .contrastPalette = native});
    test::require(root.resolvedTheme().forcedColors &&
                      button->theme() == native,
                  "native forced colors retain their paired palette");
    test::require(button->resolvedFocusWidth(0) >= 2,
                  "local cosmetic overrides cannot remove focus");
    root.setAppearance(ui::ColorSchemePreference::Light,
                       ui::ContrastPreference::Normal, {});
    test::require(button->theme() == light &&
                      parent->themeOverrides().colors == light,
                  "leaving contrast restores authored subtree palette");
    auto detached = parent->takeChild();
    ui::Box other;
    auto otherMetrics = definition.metrics;
    otherMetrics.padding = 31;
    other.setThemeOverrides({.metrics = otherMetrics});
    other.setChild(std::move(detached));
    test::require(button->themeMetrics().padding == 31,
                  "detached reparenting refreshes inherited theme");

    ui::UIRoot steppers;
    auto stepper = std::make_unique<ui::NumberStepper>(
        std::make_unique<ui::Box>(), std::make_unique<ui::Box>(),
        std::make_unique<ui::Box>());
    auto *number = stepper.get();
    steppers.setContent(std::move(stepper));
    steppers.setThemeDefinition(definition);
    steppers.flushLayout({400, 100});
    auto *row = number->children().front().get();
    test::require(row->children().front()->bounds().w() == 60 &&
                      row->resolvedControlStyle().gap == 13,
                  "stepper uses live inherited metrics");
    definition.metrics.stepper.buttonWidth = 75;
    steppers.setThemeDefinition(definition);
    steppers.flushLayout({400, 100});
    test::require(row->children().front()->bounds().w() == 75,
                  "theme change remeasures existing composite children");
    ui::VStack authored{layout::StackProps{.gap = 23}};
    authored.setControlLayout(ui::ControlLayout::Group);
    test::require(authored.effectiveProps().gap == 23,
                  "authored stack gap overrides theme recipe");
    authored.setProps({.gap = 0});
    test::require(authored.effectiveProps().gap == 0,
                  "explicit zero stack gap overrides theme recipe");
    authored.applyPatch({.gap = Patch<std::optional<float>>::reset()});
    test::require(authored.effectiveProps().gap == authored.themeMetrics().gap,
                  "reset stack gap resumes theme recipe");
    authored.applyPatch({.gap = Patch<std::optional<float>>::set(19)});
    test::require(authored.effectiveProps().gap == 19,
                  "stack gap patch overrides theme recipe");
    authored.setControlStyle({.gap = 7});
    test::require(authored.effectiveProps().gap == 7,
                  "explicit control gap takes priority over stack gap");
    ui::Popup popup{std::make_unique<ui::Box>()};
    popup.setThemeOverrides({.metrics = definition.metrics});
    test::require(!popup.popupProps().gap &&
                      popup.effectivePopupProps().gap == 4,
                  "popup exposes authored and effective geometry separately");
    popup.applyPopupPatch({.gap = Patch<std::optional<float>>::set(0)});
    test::require(popup.effectivePopupProps().gap == 0,
                  "popup zero gap is explicit");
  });
}
