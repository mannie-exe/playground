# Shared graphics settings

## Ownership and resolution

`GraphicsSettings` describes playground-wide UI appearance, presentation, 2D, 3D,
automatic quality and managed-resource preferences. Applications share one host-owned value;
changing apps does not replace it. App-authored window sizing, layout, camera,
lighting, exposure and tone mapping remain application/content responsibilities.
Quality never changes simulation speed or accessibility/layout scale. Renderer
and driver preferences are shared manual settings; applying them uses normal
capability negotiation and rollback. An app requiring unsupported features still
rejects entry. Automatic quality never changes renderer preference.

The settings schema in `graphicsSettingsSchema()` supplies stable field names,
labels, units, ranges, categories and inactive-feature markers to persistence and
the standard view. New fields require validation, round-trip tests and a stated
capability contract. A stored value is not evidence that a renderer implements it.

Settings writers emit schema version 7 with an optional `[graphics]` section;
version 7 adds shared `[controls]` without changing graphics field semantics.
Enumeration values use stable string choices (for example `linear`, `high`,
`msaa-4x`); scales are percentages and memory ceilings are MiB.
One user graphics value replaces the project graphics value; omitted fields in a
present graphics section use compiled defaults, except legacy appearance defaults
are retained when the new appearance keys are absent; see
[SETTINGS.md](SETTINGS.md#shared-graphics-and-settings-view). This is a complete
preference object, not a patch over the project object. Without a project graphics
section, legacy project `[defaults]` render fields
provide its presentation baseline. When the user graphics section is absent,
legacy user-wide render preferences still override the project baseline. Legacy
per-app render overrides do not override shared graphics. Versions 1–6 remain
readable; window/session persistence retains its separate schema and ownership.

`SettingsStore::graphics()` returns the resolved shared preferences.
`AppContext::requestGraphics(value, persist)` validates and queues a replacement
at a host boundary. `persist=false` applies to this run; `persist=true` uses the
existing atomic settings-file replacement. Validation/native presentation/write
failure preserves the previously published preferences. Effective automatic
adjustments never write the settings file.

## Requested and effective values

`AppContext::graphicsState()` returns requested settings, the current scene-scale
decision, revision, reason, timing availability and minimum-quality state.
`appliedSceneScale` is the last observed matching scene GPU execution;
`pending` distinguishes a new decision awaiting that observation. On a backend
without scene GPU timing, this observation remains unavailable. It is not inferred
from a successful UI-only presentation.

Whole-frame resolution changes the final composition target, including text.
The 3D scale multiplies each `SceneView`'s authored resolution scale. Automatic
scene scaling therefore leaves ordinary UI/text at their requested composition
resolution. `SceneViewProps::adaptiveResolution=false` opts a view out of
automatic scaling while retaining the shared manual 3D scale. Reconstruction
filtering applies when the scene image is composed.

The 2D reconstruction setting applies to `Scene2DView` images;
`useGraphicsSampling=false` preserves authored per-item sampling. Cached layers
opt into the shared raster scale with `LayerProps::useGraphicsScale`; ordinary
layers and text retain their authored scale. This avoids silently reducing the
quality of controls or pixel-critical content. The global raster setting does
not change layout or hit testing. Per-content paint APIs remain available for
applications with authored sampling requirements.

Texture quality, mesh detail, shadows, effects, antialiasing, anisotropy, ambient
occlusion and reflections are stored future preferences. The standard view labels
each as **future; inactive**. Both GPU and software renderers currently leave
these preferences inactive. No shader feature, mip streaming, LOD selection,
shadow pass or temporal history is implied by selecting them. Exposure, lighting
and tone mapping are not automatic quality switches.

## Automatic scene resolution

`QualityController` runs on the owner thread and proposes one shared scale for
participating scene views. Its upper bound is the manual scene scale; its lower
bound is `minimumSceneScale`. The master switch and `sceneResolution` permission
must both be enabled. Manual mode restores the requested scale through normal
resource admission. Quality cannot bypass ceilings or revoke existing resources.

The target frame rate is distinct from the presentation cap. The heuristic uses
the lesser target/effective runtime cap, reserves 15% timing headroom, and divides the remaining
scene allowance among recently observed participating views. Per-view GPU scopes
are compared to that allowance; they are not summed into a claimed GPU-frame
critical path. This conservative equal-share policy does not promise an FPS or
prioritize one viewport over another.

Samples carry frame identity, stable workload identity and quality revision.
Only finite scene timings from the active domain and current revision can drive
adjustment. Backend/app changes reset the observation history. Missing timings,
UI-only work and late results from older revisions do not count as headroom.
An exponentially smoothed CPU update cost above the frame target suspends
time-based reductions; lowering resolution cannot fix that update bottleneck.
With GPU timing unavailable, time-based adaptation is suspended; managed-memory
pressure can still lower permitted scene resolution in a scene-capable app.

The controller waits for at least eight distinct observed frames before a
sustained-overload reduction. It reduces scale by five percentage points after
three over-budget observations. Recovery needs at least 90 observed frames and
60 inexpensive observations, increasing by 2.5 points. Allocation pressure uses
10-point reductions. These are bounded heuristics, not performance guarantees.
At the configured floor, remaining pressure is reported and rendering follows
the normal blocked/retry contract. Input and settings remain routable. No
automatic frame-rate reduction, renderer switching or quality-floor violation is
permitted.

Changing resolution may create new backing storage. The target pool first holds
reusable attachments, then reserves the combined missing color/depth/output
commitment before creating any new scene attachment. Reservations partition into
individual resource tokens without a release/reacquire gap. Old in-use attachments
remain charged. Smaller extents do not claim reclaimed memory while larger
allocations remain cached or retained by consumers.

## Settings view and host integration

`ui::SettingsView` is a retained, theme-aware component accepting assets, a font,
initial preferences and callbacks. It owns only the draft and controls: it does
not access files, mutate the host or start GPU work. General, 2D, 3D, Automatic
and Resources tabs use the graphics schema; Controls uses the shared controls
schema. Resource readouts show managed usage, ceilings, peaks, retirement,
outstanding frames and quality/pressure state; these
are not physical RAM/VRAM measurements.

NumberStepper supplies direct numeric editing; keyed Select controls present
enumerations. Form submission validates all numeric drafts, including inactive
tabs, before cross-field validation and host publication. Invalid fields show
inline errors and receive focus after their tab is revealed. Resource meters
represent managed utilization, not task completion.

Apply publishes the draft for this run. Save applies and persists it. Revert
draft restores the last acknowledged applied value. Close discards unapplied
edits; it does not undo applied settings. Status text distinguishes applied,
saved and rejected requests. Meter refreshes do not rebuild controls or overwrite
a draft. Settings are not autosaved while stepping through expensive options. The standard
view refuses a reduced ceiling below the latest observed live commitment; return
to the menu to release scene resources first. Low-level runtime policy retains
its ability to lower budgets below existing commitments for controlled pressure
handling. This UI check is not a native-allocation guarantee.

The host accepts a `SettingsViewFactory`; callers can supply any
`SettingsPanel` implementation or reuse/theme `SettingsView`. Custom panels do
not construct the standard controls. The host requires only settings-result and
runtime-state publication methods.
An empty factory disables the host settings view. Embedded sub-apps can reuse the
component or call `AppContext::requestSettings()` to open the host view.

Escape opens/closes the shared view. Ctrl/Cmd+Shift+M returns to the launcher;
the view also has a Return to menu button. Local composition, popup and editor
cancellation precedes the playground host fallback. The reusable UI library
supports local Escape dismissal for other hosts.

While settings is visible, local app input and simulation are paused; services,
session authority and worker completions continue. Only the settings UI renders. The host cancels
held actions and rebases elapsed time on entry/exit. The view uses reflow layout,
full composition resolution and the existing window size and resize policy.
Content reflows within the available viewport; tab bodies and the view scroll
when necessary. Closing restores the app's previous window/view policy while
preserving new shared graphics settings. Fullscreen presentation follows native window
capabilities rather than forcing a new display mode for settings.

## Verification

Contract tests cover schema uniqueness/ranges, future preference round trips,
manual/automatic isolation, stale sample rejection, quality bounds and recovery.
Settings-view tests drive semantic actions through draft editing, Apply, Save,
future controls and navigation. Native host checks exercise opening/closing,
window restoration, shared settings across app transitions and GPU scene scaling.
Resource-estimate tests cover block rounding, mip/layer/sample counts, overflow,
reservation transfer and rejection without partial accounting.
