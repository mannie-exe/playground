#include <memory>

#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/containers/Box.hpp>
#include <ui/controls/Button.hpp>

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
  });
}
