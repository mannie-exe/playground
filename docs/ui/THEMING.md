# Themes and control presentation

ThemeDefinition owns four palettes (light, dark, light high contrast, dark high
contrast), ThemeMetrics and ThemeTypography. The built-in definition supplies
complete defaults. ColorSchemePreference and ContrastPreference independently
select an appearance; System follows platform preferences. A native contrast
palette selects forced colors, distinct from increased contrast.

UIRoot::setThemeDefinition publishes a validated definition. setAppearance updates
preferences without replacing the definition. resolvedTheme() exposes the effective
read-only snapshot. Node::setThemeOverrides applies optional palette, metric and
typography groups to a subtree. Clearing an override resumes inheritance. Existing
setTheme(ThemePalette) remains a palette-only convenience.

Resolution order is definition, inherited subtree overrides, control overrides,
then accessibility policy. Authored props remain unchanged. Equal updates do not
invalidate. Palette changes refresh prepared paint; metric and typography changes
also invalidate measurement and arrangement. Updates preserve node identity,
focus, drafts, selections and capture. Reparenting refreshes inherited values.

## Colors and accessibility

ThemePalette names surface, elevated, text, mutedText, border, accent/onAccent,
hover, pressed, focus, selection, error, warning, success, scrollbar and backdrop.
Control color overrides are optional: absent inherits, present replaces. Controls
resolve them through Node::resolveColor; high contrast and forced colors retain
the effective accessibility palette. Native palette foreground/background pairs
are used together. Root accessibility colors override local palettes without
replacing local typography or spacing.

Interactive chrome, labels, selection, caret, validation and focus remain adaptive.
Selection/check/expanded/error states retain geometry or text when colors coincide.
Focus thickness cannot be disabled through a cosmetic override. Disabled content
uses semantic ink rather than multiplying the whole control's opacity. Disabled
controls add a dashed boundary in high contrast, including when disabled by an
ancestor. The pattern has bounded drawing work.

ColorTreatment::PreserveArtwork preserves only the owning content node's authored
colors. It never propagates to children or changes control chrome, text scaling,
input or semantics. Text used as artwork is explicit; text over imagery can request
an adaptive backing surface. Ordinary labels and icons use Adaptive. Images and
scene output remain authored content; surrounding controls remain adaptive.

## Metrics and composition

ThemeMetrics owns stock control geometry: padding, minimum height, spacing,
stepper buttons, indicators, glyphs, focus/borders, sliders, progress/meters,
text editors, scrollbars, popup limits, tooltip timing and toast spacing.
ControlLayout identifies shared layout recipes. ControlStyle provides optional
padding, minimum height, width and gap overrides. An explicit zero remains zero.
resolvedControlStyle() reports effective values. BoxProps remains caller-owned
layout; its padding is additional to control padding, and its explicit size constraints
remain authoritative. A Content width uses the recipe's intrinsic width. Generic layout containers have no implicit control styling.

Stock controls and composed controls use these same recipes. Future Combobox and
Autocomplete reuse the input, trigger, list-row, popup and supporting-text roles;
they remain planned controls, with no separate palette or sizing system. New core
controls must use semantic roles and shared metrics, adding tokens only for a
presentation decision that cannot be expressed by an existing role. Value ranges,
validation, navigation, queue capacity and domain behavior remain control props.

## Typography

TextRole identifies Display, Title, Heading, Body, Label, Caption and Code.
FontFamily identifies Interface, Display and Monospace resources. A TextStyle
specifies family, size, emphasis and line-height ratio. ThemeTypography stores the
style table, optional family handles and textScale. Existing font resources serve
as provisional fallbacks; family selection remains an explicit follow-up decision.
Visual text roles do not assign accessibility heading levels. TextInk selects
Primary, Secondary, Error, Warning, Success or OnAccent semantic ink independently
of typography.

Text and TextField accept an optional textRole. Without a role, an explicit font
retains its authored size; textScale still applies. With a role, the resolved style
selects its family or falls back to the supplied font. Font resources are resolved
through the existing font/asset infrastructure. Missing both family and fallback
is an error at measurement, not silent invisible text. Typography changes clear
text layout caches and reflow content without replacing edit models. DPI scaling
and user text scaling are separate and are each applied once. Tabs retain their
navigation height; Settings scrolls overflowing content instead of shrinking text
or making actions unreachable.

## Customization

```cpp
auto definition = ui::defaultThemeDefinition();
definition.metrics.padding = 12;
definition.typography.textScale = 1.25f;
definition.typography.families[static_cast<unsigned>(ui::FontFamily::Interface)] = font;
root.setThemeDefinition(std::move(definition));
root.setAppearance(ui::ColorSchemePreference::System,
                   ui::ContrastPreference::System, appearance);
button.setControlStyle({.padding = math::Insets::all(0)});
```

Set an override to an empty optional to resume inheritance. Theme groups replace
as groups; the optional stepper group changes only stepper metrics. Font families
are borrowed shared resource handles, with ownership retained by the snapshot.
Settings for validation, navigation and resource budgets are unaffected.

## Verification

Test four appearances, native forced colors, local overrides, explicit zero,
invalid metrics, reparenting and live changes while editing. Check paired text and
surface contrast for built-in palettes. Rendering tests cover state markers,
scrollbars and artwork exceptions. Test enlarged text for layout and clipping;
interactive assistive-technology checks supplement automated semantic tests.
