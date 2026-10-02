# Control behavior and composition

[Theme contracts and typography](THEMING.md) define shared control presentation.

## State and ownership

Controls retain typed props and explicit signals. Setters and patches validate
before publication and do not emit user-action notifications. Interaction paths
share validated operations with assistive actions. Notifications run after state
publication; observer exceptions do not roll back committed state.
Text refreshes and numeric formatting validate capacity, Unicode and codec
round-trip behavior before replacing accepted state or the revert target.

| State | Authority |
|---|---|
| Requested/application value | Caller model |
| Text draft, highlighted item, drag preview | Owning editor or collection |
| Accepted control value | Validated control model |
| Applied runtime value | Application/host acknowledgement |
| Focus, capture, attached identity | UIRoot |

Value events distinguish draft changes, accepted changes, interaction completion
and cancellation. Change context records ActionSource and reason. Ordinary
refreshes preserve active drafts; explicit reset replaces them. An external value
change while editing is reported as a conflict. No implicit global state store,
property observer or application rollback is introduced.

## Number controls

NumberField edits a number. NumberStepper composes numeric adjustment buttons
with a NumberField or a caller-supplied readout. ChoiceStepper operates on keyed
choices; it does not expose numeric indices as values. The numeric stepper is named
NumberStepper throughout the public API, tests and documentation.

Numeric controls share range validation and adjustment. Typed input has Empty,
Incomplete, Invalid and Valid parse states. Finite values, inclusive limits,
integer mode and optional custom validation determine acceptance. Empty never
means zero. Formatting and parsing are paired policies; default formatting
preserves numeric precision. Units are separate presentation content.

Enter and focus loss attempt a numeric commit; explicit form submission also
validates the focused editor. Invalid text remains visible and retains the last
accepted value. Escape restores that value. IME preedit is never parsed as a
completed edit. Buttons first validate a pending draft, then step that value.
Typed out-of-range values are rejected; stepping saturates at bounds. Step is an
adjustment amount, not a required typed-value multiple. Optional snapping is a
separate constraint. Integer ranges require integral values and steps.

Readout-only permits adjustment without typing. Read-only permits inspection and
copying but no mutation. Disabled controls reject user actions. Up/Down adjusts
an editor, while Left/Right and platform text shortcuts retain editing semantics.
Page Up/Down uses a larger increment. One editable composite exposes one numeric
semantic value. Programmatic range changes supply a valid replacement value.

Slider shares finite inclusive RangeValue validation and owns spatial adjustment.
snapToStep controls pointer snapping independently of keyboard increments; integer
and custom validators belong to NumberField. Preview changes are distinct from
interaction completion. onInteractionFinished reports Drag/Step on completion or
Cancel on cancellation, including disable/read-only transitions. Detachment
releases capture without invoking observers. Applications decide whether a preview
is cheap enough to apply continuously.

## Choices, navigation and commands

Selection uses stable string keys, not positions or node addresses. Highlight,
selection and keyboard focus are separate. Collection navigation shares enabled
item traversal, orientation, optional wrapping, Home/End and typeahead.
Focused lists and open choosers request native text input for Unicode typeahead.
IME preedit never navigates or invokes an item; Escape first cancels composition.

Select shows a dropdown trigger; opening highlights the selection. Arrows preview,
Enter/click accepts, Escape cancels and Tab dismisses before normal navigation.
ChoiceStepper adds previous/next buttons and a dropdown or readout center. It
skips disabled choices, does not wrap by default, and has one selection authority.
Optional selection is an explicit empty state; Auto and None can be real choices.
When reconstructing item collections, the caller preserves valid selection keys
and explicitly resolves removed selections.

MenuList invokes commands with onInvoked(key, source); it does not store a selected
application value. DropdownMenu and ContextMenu compose MenuList and Popup.
ContextMenu supports pointer and keyboard invocation. All invocations carry their
actual source. Menus remain single-level; nested menus are deferred.

Toolbar provides one sequential navigation entry and internal arrow navigation.
It skips decoration and maps composite descendants to their logical entry without
removing pointer or assistive focusability. An embedded editor retains its editing
keys. ToggleGroup supports explicit single
or multiple choice; CheckboxGroup exposes independent selection and aggregate
mixed state. Accordion coordinates Disclosure expansion with single/multiple
policies. Tabs retain panels and their state while inactive.
Select, ChoiceStepper and Tabs expose onSelectionEdited(key, source);
Disclosure and Accordion expose onExpandedEdited(value, source). Legacy
onSelectionChanged/onExpandedChanged callbacks remain available without provenance.
CheckboxGroup remains multiple-selection through every property update.
ToggleGroup and CheckboxGroup use wrapping Flow layout with shared themed gaps;
arrow keys retain logical item order across rows.

## Fields and forms

Field associates label, help and errors with the actual semantic control and its
focus target. Validation issues have a stable code and a readable message.
Expected invalid input is a result, not an application exception. Validation
presentation can occur on edit, commit or explicit submission.

Form coordinates stable field keys and lifetime-checked editor handles. Submission
validates a snapshot, including inactive tabs, before application publication.
Cross-field checks run on the complete candidate. Failure reveals the containing
tab, scrolls and focuses the first invalid field. No partial application is
performed. Field edits may remain accepted locally after failed form validation.

Revert restores acknowledged applied values. Reset to defaults is separate. A
successful submission acknowledges the submitted snapshot, preserving newer edits.
Saving acknowledges persistence only after the host reports success. Runtime
rejections remain visible and can be associated with fields. Async application
results carry request identity; stale responses cannot overwrite newer drafts.

Settings is a Form consumer. Numeric options use NumberStepper; choices use
Select or ChoiceStepper; toggles use Checkbox. Apply changes this run; Save also
persists. Automatic quality never dirties preferences. Requested and effective
values are reported separately. Future renderer settings remain saved inactive
preferences.

## Routing, focus and transient surfaces

Keep capture/target/bubble/default routing and the distinct handled,
stopPropagation and preventDefault flags. UI receives local dismissal before host
fallback. FocusWithinGained/FocusWithinLost notify ancestors only when focus crosses
their subtree boundary; moving between sibling descendants does not leave/reenter
the common ancestor. Deferred focus
requests resolve after layout, so revealing a hidden tab precedes focusing its
invalid editor. Escape cancels one layer: IME composition, nested chooser, local draft,
enclosing surface, then host settings. Ctrl/Cmd+Shift+M remains the menu shortcut.

Composites publish a semantic focus target; labels focus editors/triggers rather
than wrappers. sequentialFocusTarget maps Tab traversal within composites while
ordinary focusability remains available to other input paths. Modal surfaces accept
initial and return focus targets. Closing
restores a valid opener or a deterministic eligible fallback. Removal, disable,
focus loss and input cancellation release held presses, timers and captures.
Outside dismissal consumes the whole dismissing pointer interaction.

Popover composes an anchored trigger and interactive content over Popup. Dialog
adds modality. AlertDialog uses caller-supplied decision actions and deliberate
initial focus; it remains modal through property updates and disables outside
pointer dismissal. Tooltip is passive and never takes focus. Its timing follows
hover/focus state and cancellation; essential instructions remain available
without hover.

## Measurements and notifications

Meter displays a current measurement relative to limits, not task completion.
It supports units, thresholds and unavailable measurements. Over-limit readings
retain the actual value in text/semantics while visual fill saturates.
Warning and critical states overlay distinct markers inside the track, with a
themed right inset; the track keeps its full width in every state. Optional
monochrome `TintableContent` icons (`Vector` for SVGs) replace the built-in
triangle/cross. The meter owns icon
visibility, placement and tint. Each marker is clipped at the fill boundary and
painted using `onWarning`/`onError` over the fill and `meterOnTrack` over the empty
`meterTrack`. Empty tracks use dark ink in light mode and light ink in dark
mode. No backing square or background sampling is required. High contrast
outlines the full range and keeps paired inks and distinct shapes. Demo 2D uses
filled Material warning/error SVGs and a visible value/status readout.
ProgressBar represents task completion; unknown progress is indeterminate, never
zero. Neither is adjustable or sequentially focusable.

ToastHost owns a bounded notification queue with stable IDs, replacement/
deduplication, dismiss actions and interaction-paused timeouts. It never steals
focus. Persistent validation/runtime errors remain inline; a toast is not their
only record. Timers use the existing Scheduler and end on detachment. Animated
indeterminate content honors reduced-motion policy; no animation is required to
communicate unknown progress.

## Presentation and accessibility

ThemeMetrics supplies live geometry and spacing for all stock controls;
ThemeTypography supplies text roles and appearance palettes supply semantic colors. Center slots
align vertically; labels and editors use consistent row alignment. Content,
fonts and units remain caller supplied. Vector icons avoid font coverage issues.
Narrow rows wrap/reflow or scroll without reducing readable font size.

Semantic error/warning/success colors accompany text and shapes. Focus, selection
and validation never rely on color alone. Meter, toolbar and alert-dialog roles
and related states are translated by the native adapter. A composite exposes one
logical value and its actual editor/trigger, not duplicate controls. Existing
high-contrast inheritance remains authoritative.

## Planned: Combobox and Autocomplete

These two controls are documented contracts only and are excluded from the current
implementation.

Combobox commits a keyed option; its search query, highlighted option and selected
key are separate. Typing filters without committing. Enter accepts an enabled
option, Escape cancels the chooser, and invalid/unmatched text does not invent a
key. Single selection is the baseline; multi-selection is a later extension.

Autocomplete accepts free text; suggestions optionally replace/complete that
text. It does not require an option identity. Both controls reuse TextEditModel,
collection navigation, Field and Popup. Filtering uses Unicode text rather than
keycodes. Composition is not a completed query. Empty, loading and failed search
states are distinct and accessible.

Async providers return through the existing bounded CompletionQueue. Each query
has a generation; stale responses are discarded. Virtualized results use existing
keyed collections, keep the active option realized, and expose valid semantic
identity. Neither control owns networking, worker scheduling or a second list
virtualizer.

## Deferred extensions

Nested menus, multi-thumb sliders, command palettes, docking and data grids need
specific consumers before implementation. Avatar, separator and similar visual
elements remain ordinary node compositions. Wheel adjustment, drag scrubbing and
asynchronous validators are not part of numeric editing's baseline.

## Verification

Cover numeric precision/bounds, incomplete drafts, validation/submit failure,
source-preserving semantic actions, IME, nested dismissal, changing/removed focus
targets, read-only/disabled behavior, and settings apply boundaries. Verify layout
at different text scales, RTL and high contrast. Native assistive checks supplement
semantic tests; semantic tests alone do not verify a screen reader.

## Motion feedback

Buttons interpolate hover/press background color using `ThemeMotion::feedback`;
focus indicators remain immediate and high-contrast colors snap directly.
App UI roots reveal with `ThemeMotion::reveal`. Demo 2D includes a reversible
Presence panel. Custom controls can own `MotionValue<T>` and playback handles;
[the motion contract](MOTION.md) defines lifetime and reduced-motion behavior.
