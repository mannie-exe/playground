# Accessible interaction and plain text controls

Control behavior and composition: [CONTROLS.md](CONTROLS.md).

## Appearance and transient surfaces

Controls inherit a resolved semantic palette: surface, text, border, accent,
selection, focus and disabled text. Color scheme (system/light/dark) and contrast
(system/normal/high) are independent. User settings override application scheme;
an application scheme override never disables system high contrast. Only an
explicit user contrast preference does so. Unknown platform preferences use a
documented fallback rather than masquerading as detected normal contrast.

`runtime/AppearancePreference.hpp` defines the preferences, re-exported by
`ui/Theme.hpp`; `AppViewPolicy::colorScheme` is the app baseline.
Settings schema 4 adds `color_scheme` and `contrast`. `SettingsStore` honors
contrast overrides only from the user document. Shared General controls expose
these overrides, UI motion and text size; see
[SETTINGS.md](../platform/SETTINGS.md#shared-graphics-and-settings-view). UISession calls
`UIRoot::setAppearance` with policy and native observation; the installed
ThemeDefinition remains authoritative for application styling. Unknown scheme
falls back to light; unknown contrast to normal. Observation runs on the owner
thread, at most four polls/second (Linux portal requests at most once/second).
Windows reads high-contrast status and system colors; macOS reads Increase
Contrast; Linux reads the desktop appearance portal when available. A missing
portal is unknown, not proof that contrast is disabled.

`Node::setTheme(optional<ThemePalette>)` supplies an inherited local palette;
clearing it resumes inheritance. A root high-contrast palette takes precedence
over local palette overrides. Text, Button, TextField, Slider and ProgressBar use
the palette by default. Only content nodes expose
`ColorTreatment::PreserveArtwork`; it preserves their authored colors without
changing surrounding control chrome. Theme changes invalidate prepared paint;
metric/typography changes also reflow layout, preserving app and editor models. UI roots remain transparent unless
`PaintStyle::themeBackground` is enabled or a background is authored. Explicit
background and border colors adapt under high contrast. See [theming](THEMING.md).

Popup content remains owned by its originating component. Root overlay presentation
escapes ancestor scroll clips but stays within the UI viewport. It does not
contribute to preferred window size. Logical ancestry supplies theme, event routes
and modal membership; presentation order must not create duplicate semantic nodes.
Anchor movement/scroll/resize repositions open popups. Owner removal, hiding or
app replacement closes them. Outside dismissal consumes the entire dismissing
pointer interaction; it must not activate the control underneath.

Select separates committed selection from the active option. Arrows preview;
Enter/click commits; Escape cancels; Tab closes and continues navigation. A popup
inside a dialog belongs to that dialog's modal scope, not above every modal.
Dialog modality, popup anchoring, and overlay painting are separate contracts.

`containers/Popup.hpp` supplies Portal, PopupProps and PopupPatch. Popup props
specify a generational anchor ID, preferred/fallback placements, anchor-matched
or content width, gap, viewport inset, height cap, autofocus and dismissal policy.
Start/End placement follows layout direction. The root tries the preferred side,
then fitting fallback sides, then clamps to the usable viewport. Put long content
in ScrollView: clipping bounds an oversized custom child but is not scrolling.
Popups are excluded from ordinary stack/flow/grid/box placement. Keep solver
constraints on their logical owning component, not on the out-of-flow popup.
Root presentation bypasses ancestor clip, affine transform and opacity; anchor
bounds are mapped through those transforms. Logical visibility, enabled state,
theme and modal eligibility still apply. Nested popup order follows logical tree
order, and later root siblings paint above earlier siblings. No native child
windows are created.

Select owns a bounded ScrollView/ListBox popup. Its active descendant is exposed
to AccessKit independently of committed selection. Dialog centers in the usable
viewport, dims the background when modal, traps focus and restores its opener.
Outside presses do not dismiss a Dialog by default. Escape first cancels IME
composition, then dismisses the top transient surface. Tooltip uses an anchor
without autofocus or hit testing; the author still owns timing/open state.

Wrapping labels retain readable font size. Measured line breaks/height must remain
valid at arrangement width. Indicator and padding space is reserved exactly once;
clipping must not conceal a mismatch between measured and painted text.

Text measurement includes target density. At equal measurement/preparation density,
preparation reuses the arranged font/wrap result rather than selecting new line
breaks with differently hinted glyph advances. Wrap widths tolerate float
round-trip error at pixel boundaries. Authors must still provide enough height
or scrolling when choosing a fixed-height box. Pure visual scaling can use a
different raster density; it is not a substitute for normal layout reflow.
Text preparation compares line ranges if that density differs from measurement.
If hinting would change breaks or truncation, it retains the arranged-density
raster/atlas layout and scales that result. This favors stable geometry over
extra sharpness in that exceptional case; normal matching-density DPI layout
still uses the higher-resolution font. Prepared text always paints into its
arranged logical extent, not a newly inferred physical-pixel extent.

## Ownership and delivery

Directional navigation claims arrow keys only when UI has or obtains eligible
focus, or a modal owns the navigation scope. A focused control at the edge of
that scope still consumes navigation; a decorative tree with no focusable nodes
does not. `UIRoot::focusDirection` returns this ownership decision, not merely
whether focus moved. Unclaimed arrows reach `AfterUI` app actions (such as Demo
3D orbit), without promoting camera bindings ahead of editors or modal controls.

UI controls own behavior and derive semantic state. Pointer, keyboard, gamepad,
and native assistive actions use the same validated operations. Native actions
are not synthetic pointer or key events. UIRoot owns focus/navigation and creates
immutable semantic snapshots after layout. Native callbacks consume snapshots
and enqueue owned actions; they never traverse live nodes. Session identities
and generational node handles reject stale delivery after app replacement.

The window owns native accessibility/text-input services independently of its
renderer. UISession attaches one UI root, maps geometry and synchronizes text
input. Adapter teardown precedes SDL window destruction. Native callbacks must
not throw across the C ABI or hold a UI lock while invoking native APIs.

## Configuration

Native accessibility Auto/Enabled/Disabled is independent of sequential and
directional navigation. Auto permits demand activation; it does not poll for a
screen reader. Disabling the native adapter does not disable core semantics.
User overrides take precedence over application defaults. Gameplay contexts
may capture movement without disabling menu focus or assistive actions.

`AppViewPolicy::interaction` supplies InteractionProps. Settings schema 3 adds
`accessibility="auto"|"enabled"|"disabled"`, `sequential_navigation` and
`directional_navigation`. Both Auto and Enabled permit the platform's demand
activation; Enabled does not start a screen reader. Disabled publishes only the
native window, not the app tree. Navigation remains independently configurable.

## Semantic contracts

The semantic tree omits layout/decorative nodes, retains useful groups, and
exposes disabled controls without permitting their activation. Names, label
relationships and descriptions are authored; checked/selected/expanded/range,
read-only, validation and available actions are derived from actual state.
Stable identities are scoped to a root lifetime, not reusable pointers/indices.
Changed native nodes are complete descriptions, not partial property patches.
Geometry follows layout, scroll, transforms, viewport mapping and window scale.
Publication is independent of painting. Failed publication must remain retryable.

`Node::semanticState()` derives a complete value. Generic authored SemanticProps
provide roles, names/descriptions, exposure and label/help relationships. Auto
omits anonymous layout nodes; Self exposes one node and hides descendants;
ChildrenOnly omits the wrapper; HiddenSubtree omits the entire branch. Only the
active modal subtree is published. Disabled controls remain readable, without
actions. A CustomAction has a stable control-local integer ID and description;
Minesweeper uses this for place/remove flag, separately from ordinary reveal.

The adapter uses complete-tree updates when snapshots change. This favors a
small, auditable implementation over an incremental-diff engine. Native IDs are
monotonic and mapped from root session plus generational NodeId; retired mappings
are discarded without recycling IDs. The native action queue accepts at most 256
pending requests. Delivery revalidates root identity, node attachment, eligibility
and modal scope on the owner thread. It is not a worker API for mutating nodes.

## Focus and controls

Tab navigates controls/groups; arrows adjust a control before moving focus.
Modal scopes restrict input and restore a valid opener on dismissal. Directional
navigation uses explicit neighbors before deterministic geometry. Selection is
not focus. Cancellation never activates a held control. Controls share range,
selection and editing models, not duplicate accessibility state.

The control catalog includes buttons/toggles, checkbox/switch/radio groups, numeric
stepper/slider/field, plain text field/area, list/select, disclosure/accordion/tabs,
dialog/menu/popover/tooltip, meter/progress/status/toasts, toolbars and forms.
Combobox and Autocomplete are planned only; their contracts are in CONTROLS.md.
Rich text, spreadsheets, docking and custom file browsers remain deferred.

Disclosure expands in normal layout flow; Select opens a root-presented popup.
Their visible labels and
panels are supplied nodes, not hidden strings rendered by another framework.
Confirming an unchanged Select value still closes its list and restores trigger
focus, without emitting a value-change notification.
Tabs retain their panels. Dialog supplies root-level presentation/modality/focus
behavior; authors retain it alongside its logical owner. Tooltip supplies passive
help semantics and explicit visibility; the author supplies its anchor and
hover/focus timer; TooltipTrigger supplies these when composed around an owner.
MenuList invokes keyed commands independently of selection. DropdownMenu and
ContextMenu supply presentation; nested menus remain deferred.

## Editing

Text models store UTF-8 with byte-offset selections at grapheme boundaries,
bounded undo history and separate transient composition. SDL committed text
events insert text; keycodes perform editing/navigation only. ICU provides
Unicode boundaries and bidi ordering; SDL_ttf/HarfBuzz shape and measure runs.
Caret/selection geometry must match the exact displayed layout on both backends.
Editable spaces and tabs retain their advances at line ends, including blank
lines; caret stops, selection bounds and accessible text use that same geometry.
Password controls omit plaintext from semantic snapshots, logs and clipboard
copy, and disable undo retention. Ordinary changes, commits and programmatic
setters have distinct notification semantics. Numeric drafts are separate from
parsed/committed values. Native text input stops on blur, detach and app exit.

TextEditProps defaults to 64 KiB of UTF-8 and 256 KiB of undo history. Read-only
fields permit selection/copy, but not mutation. Password history and copy/cut are
disabled. TextField setters do not echo onValueChanged; onCommit is distinct.
NumberField retains invalid drafts and exposes validation state; its committed
RangeValue remains valid. SDL input supplies committed UTF-8 and transient preedit
selection; keycodes never stand in for typed characters. Clipboard operations are
injectable UIServices functions, backed by SDL only at the window boundary.

ICU computes grapheme/word/line boundaries and visual bidi runs. SDL_ttf/HarfBuzz
shapes each resolved directional run; caret/selection geometry uses its substring
metrics, including divided caret advances for ligatures. Left/right moves through
visual caret stops; word movement uses logical ICU boundaries. Wrapped lines use
bounded grapheme fitting and Unicode line-break opportunities. This is a plain
editor, not a rich-text paragraph/typesetting engine. AccessKit receives text runs,
byte lengths and selection translations; passwords provide no text-run children.

On Windows, WindowServices attaches to a hidden SDL window before it is shown.
PresentationSession owns it until before window destruction. macOS uses the
AccessKit view/window subclass adapter; Linux uses the Unix adapter. Only the
Windows configuration is verified in this worktree. Native IME and screen-reader
coordinate behavior on other platforms still needs interactive acceptance.

## Verification

Input ownership is separate from handled/default-prevented/propagation flags.
`UIRoot::inputClaims()` reserves keyboard input for an eligible focused
TextInputClient, all input domains for a modal, and captured pointer identities.
`UISession` exposes these claims; UI-backed IApps forward them to AppHost.
Claims suppress AfterUI actions, not UI dispatch or explicit BeforeUI shortcuts.
The host applies changes even without another physical input event. Neutral input
is required before a previously reserved control can activate gameplay again.
Readonly editors still own keyboard navigation and selection. Decorative trees
do not consume Tab when no focus target or modal exists.
Replacing a root discards the old tree's modal restoration history; it must not
prevent focus entering the replacement or restore detached opener identities.

Background/device removal cancels retained interaction; current removal behavior
conservatively cancels all UI captures rather than preserving other devices.
TextField cancellation discards IME composition, never commits it. Keyboard
per-device focus and gamepad stick-to-navigation repeat remain future policies.

Focused tests cover actions, snapshots, stale owners, modal navigation, Unicode
editing, range/selection validation, and native-adapter boundary behavior.
Actual Narrator/NVDA, VoiceOver and Orca checks are separate platform acceptance
work; mock/unit results never establish screen-reader interoperability.

## Module map and verification limits

| Module | Responsibility |
|---|---|
| `ui/Interaction.hpp`, `ui/Semantics.hpp` | Configuration, typed actions, derived states, immutable snapshots |
| `ui/UIRoot`, `ui/Semantics.cpp` | Routing, focus/modal scopes, explicit neighbors and geometric fallback |
| `ui/TextEdit`, `ui/controls/TextField` | ICU editing state, SDL_ttf layout, selection and text painting |
| `ui/controls/{Choice,Slider,Composite}` | State-owning controls and retained compositions |
| `ui/Theme`, `platform/sdl/SystemAppearance` | Backend-independent palettes and native appearance observation |
| `ui/containers/Popup`, `ui/Overlays.cpp` | Logical ownership with viewport-level placement, painting and dismissal |
| `platform/sdl/WindowServices` | AccessKit adapters, native request queue, SDL clipboard/IME |
| `platform/sdl/UISession`, `app/UISession.cpp` | Root/window attachment and app-specific convenience wiring |

The current semantic tree describes realized nodes. Virtualized offscreen item
realization for assistive queries, mobile adapters, rich editing and automatic
tooltip ownership/timing are not implemented. Native rotated/skewed text bounds
are approximate axis-aligned extents, not a rich text geometry provider.

Demo2D contains a scrollable manual controls gallery, including a modal dialog.
Use keyboard-only, gamepad, IME and a screen reader as separate acceptance passes.
Check field names, disabled state, value changes, label/help relationships,
password privacy, selection, scroll-to-focus, app switching and opener restoration.
Unicode editing and bidi support do not supply missing font glyphs. The demo's
font may show missing-glyph boxes for some scripts or emoji; font fallback is a
separate rendering concern.

The gallery uses a viewport-filling ScrollView, wrapping Flow rows and content-height
labels; its preview uses centered Contain fitting. Checkbox marks, radio dots,
switch thumbs and disclosure/select chevrons are renderer-native paths/rectangles,
not downloaded icons. Check at narrow/wide widths and 100/125/150/200% scaling,
in light/dark/high-contrast palettes. Native OS transitions and screen-reader
announcements remain separate manual checks from deterministic palette tests.

Dependencies: checksum-pinned [AccessKit C 0.23.1](https://github.com/AccessKit/accesskit-c/releases/tag/0.23.1)
and [ICU 78.3](https://github.com/unicode-org/icu/releases/tag/release-78.3).
See [SDL text input](https://wiki.libsdl.org/SDL3/SDL_StartTextInputWithProperties),
[SDL IME geometry](https://wiki.libsdl.org/SDL3/SDL_SetTextInputArea), and
[ICU boundary analysis](https://unicode-org.github.io/icu/userguide/boundaryanalysis/).
Appearance adapters follow [SDL system theme](https://wiki.libsdl.org/SDL3/SDL_GetSystemTheme),
[Apple Increase Contrast](https://developer.apple.com/documentation/appkit/nsworkspace/accessibilitydisplayshouldincreasecontrast),
and the [desktop Settings portal](https://github.com/flatpak/xdg-desktop-portal/blob/main/data/org.freedesktop.portal.Settings.xml).
