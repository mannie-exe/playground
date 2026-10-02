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
StackProps.gap is optional: absent inherits the control recipe or zero; explicit
values, including zero, override the recipe. ControlStyle.gap takes precedence.
Resetting StackPatch.gap restores inheritance.

Stock controls and composed controls use these same recipes. Future Combobox and
Autocomplete reuse the input, trigger, list-row, popup and supporting-text roles;
they remain planned controls, with no separate palette or sizing system. New core
controls must use semantic roles and shared metrics, adding tokens only for a
presentation decision that cannot be expressed by an existing role. Value ranges,
validation, navigation, queue capacity and domain behavior remain control props.

## Typography

TextRole identifies Display, Title, Heading, Body, Label, Caption, Code, Value
and Prose. FontFamily identifies Interface, Display, Monospace and Serif resources.
A TextStyle specifies family, size, line-height ratio and FontSelection. Selection
contains weight (1–1000), slant (Upright/Italic) and optical design
(Text/Caption/SmallText/Subhead/Display). ThemeTypography stores the style table,
optional immutable FontFamilyDefinition handles and textScale. Definitions enumerate
stable face IDs, resolved file sources, cache identities and selection metadata;
they contain no open font objects. Titles/headings default to weight 700; other
roles use 400.

Matching prefers the requested slant, then optical design (Text fallback), then
nearest weight with lighter ties. Other optical ties use enum order. Selection is
deterministic and does not synthesize styles for registered families. Optical
design is explicit, independent of DPI and textScale. Explicit legacy fallback
fonts may synthesize bold/italic when a role or instance requests it.

All static weight/italic variants are bundled, including Source Serif's five
optical designs. Faces open on demand. OpenType shaping uses renderer defaults;
arbitrary feature switches and variable axes are not exposed by this contract.
No scene serialization or editor is required to enumerate or select these faces.

ThemeTypography.fallbacks is an ordered list of immutable font families for
missing glyphs. Absence lets the host supply defaults; an empty list disables
fallback. A subtree typography override can replace the list. Selection uses
the same weight/slant/optical request as the primary face. FontProps.fallbacks
stores the resolved paths and cache identities; font variants preserve the list
and open matching-size faces owned by the primary font. Order and source identity
participate in font caching. SDL_ttf shapes missing-glyph spans with the chain on
both rendering backends; fallback does not replace glyphs already in the primary
font or promise complete language/emoji-presentation coverage.

On macOS the host first uses the installed Apple Color Emoji font, then bundled
Noto Color Emoji. Other platforms use Noto. Apple fonts are neither copied into
assets nor distributed. The bundled font and its OFL license follow the ordinary
CMake asset installation. FreeType shares SDL_image's managed PNG/zlib libraries
for bitmap color glyphs. Missing or invalid explicitly configured font files are
reported as font-loading errors rather than silently ignored.

| Roles | Bundled family | Use |
| --- | --- | --- |
| Display, Title, Heading, Label | Inter Display | Titles, buttons, tabs and control labels |
| Body, Caption | Inter | Descriptions, help, errors and small captions |
| Value | Inter | Editable or read-only values and selection choices |
| Code | JetBrains Mono | Code, logs and technical measurements |
| Prose | Source Serif 4 | Optional serif prose |

Roles follow content purpose and readability, not focus or editability. A value
keeps its role when made read-only. Small labels may use Caption. Prose is opt-in;
ordinary interface text remains sans serif. All four appearances share typography.
The host UISession supplies missing bundled families on attachment, preserving
explicit family overrides. Standalone UIRoot users supply their own family handles
or explicit fallback fonts. Bundled versions and licenses live in assets/fonts/README.md.
Visual text roles do not assign accessibility heading levels. TextInk selects
Primary, Secondary, Error, Warning, Success or OnAccent semantic ink independently
of typography.

Text and TextField accept optional textRole, fontFamily and fontSelection props.
An instance family/selection overrides the role's family/selection without changing
its size or line height. Reset restores the role. Without a role, family-only text
uses the explicit fallback size or 18 units. A control's text child can override
its family independently; a subtree may override the typography table. Without a role, an explicit font
retains its authored size; textScale still applies. With a role, the resolved style
selects its family or falls back to the supplied font. Font resources are resolved
through the existing font/asset infrastructure. Missing both resolved family and fallback
is an error at measurement, not silent invisible text. Typography changes clear
text layout caches and reflow content without replacing edit models. Editors retain
directional faces across width changes and replace them when typography changes. DPI scaling
and user text scaling are separate and are each applied once. Tabs retain their
navigation height; Settings scrolls overflowing content instead of shrinking text
or making actions unreachable.

## Customization

```cpp
auto definition = ui::defaultThemeDefinition();
definition.metrics.padding = 12;
definition.typography.textScale = 1.25f;
definition.typography.families[static_cast<unsigned>(ui::FontFamily::Interface)] = customFamily;
root.setThemeDefinition(std::move(definition));
root.setAppearance(ui::ColorSchemePreference::System,
                   ui::ContrastPreference::System, appearance);
button.setControlStyle({.padding = math::Insets::all(0)});
text.applyPatch({
    .fontFamily = Patch<std::optional<ui::FontFamily>>::set(ui::FontFamily::Serif),
    .fontSelection = Patch<std::optional<ui::FontSelection>>::set(
        {.weight = 600, .slant = ui::FontSlant::Italic})});
```

Set an override to an empty optional to resume inheritance. Theme groups replace
as groups; the optional stepper group changes only stepper metrics. Font family
definitions are shared immutable resources, with ownership retained by the snapshot.
Settings for validation, navigation and resource budgets are unaffected.

## Verification

Test four appearances, native forced colors, local overrides, explicit zero,
invalid metrics, reparenting and live changes while editing. Check paired text and
surface contrast for built-in palettes. Rendering tests cover state markers,
scrollbars and artwork exceptions. Test enlarged text for layout and clipping;
interactive assistive-technology checks supplement automated semantic tests.
