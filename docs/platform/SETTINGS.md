# Project settings, user preferences, and files

The public API is typed C++; TOML is a storage format, not a dependency exposed
to apps/UI nodes. Declarations are in `include/platform/{Settings,FileStore}.hpp`.
See [WINDOWING.md](WINDOWING.md) for the presentation values they configure.

## Files and precedence

| Source | Location / role |
|---|---|
| AppInfo | Compiled app baseline and resize policy |
| `project.toml` | Read-only defaults beside the executable; installed from `config/project.toml` |
| `settings.toml` | User-authored overrides in SDL_GetPrefPath("Playground", "Playground") |
| `session.toml` | Machine-written normal bounds/display-name hint per app in that same preference directory |

Resolve app baseline -> project defaults -> project app overrides -> user defaults
-> user app overrides for window, viewport and interaction. Shared graphics
resolve separately and override legacy per-app render fields. Session state does not override settings; it is consulted
only for RestorePrevious sizing. Keys are stable strings: `menu`, `demo`,
`material-lab`, `minesweeper`, `rock-paper-scissors`, `snake`. Demo 2D retains
`demo` and Demo 3D retains `material-lab` so existing preferences survive renames.
Unknown app keys can be retained for
future apps; this does not register/implement those apps.

No current-working-directory dependence, hard-coded home directory, registry key,
or PATH change is required. Install prepares runtime files. Running directly from
the build tree need not find `project.toml`; absent files retain compiled defaults.
The project root is not treated as a writable preferences directory.

Settings readers accept integer schema versions 1 through 5; writers emit version 5.
Version 1 settings retain their defaults and are upgraded on the next explicit
save. Renderer and interaction preference fields are additive and accepted by this
reader in older documents; older readers reject unknown fields. Version 3 adds
accessibility/navigation preferences; version 4 adds appearance; version 5 adds
shared graphics and runtime policy. See [GRAPHICS.md](GRAPHICS.md).
Session readers accept versions 1 and 2; writers emit version 2. Version 1 requires
normal-window size, position and display name. Version 2 keeps size and display
name required but makes position optional: compositor-managed placement may not
expose desktop coordinates. Existing coordinates, including negative values and
the origin, survive reading and saving; unavailable positions are omitted, not
replaced with `[0, 0]`. A version 1 record missing position remains invalid, and
older readers reject version 2 instead of inventing placement. The next session
save performs this migration; reading alone does not rewrite a file.
Missing files mean no overrides;
malformed/unreadable files are errors, not silently replaced defaults. Unknown
sections, fields and enum strings are rejected to expose spelling/schema errors.
Unknown versions still fail rather than being silently repaired.

For example, a compositor-managed session can retain size without placement:

```toml
schema_version = 2

[apps.demo]
size = [960, 720]
display_name = "Built-in Display"
# position = [-800, 20] # optional, only when observed
```

`SavedWindow::position` is `std::optional<math::Vec2i>`. `RestorePrevious`
can restore size even when position is absent or the active windowing backend
cannot place windows. Display names remain best-effort hints, never persisted
native display IDs.

## Settings schema

Interaction keys configure AppViewPolicy::interaction, not the renderer.
`accessibility` accepts auto/enabled/disabled; `sequential_navigation` and
`directional_navigation` are independent booleans, both true by default. Normal
project/user/app precedence applies. Disabling a navigation fallback does not
disable a control's editing keys or a native assistive action.

`color_scheme` accepts system/light/dark (default system); normal app/project/user
precedence applies. `contrast` accepts system/normal/high (default system), but
only user-file overrides participate in effective contrast resolution. Project
or app appearance cannot suppress an OS high-contrast request. `userContrast` on
the resolved AppViewPolicy is a settings result, not an app preference. Native
appearance is observed by UISession, separate from persisted intent; unknown
scheme/contrast use light/normal fallbacks. No OS setting is modified.

Both project and user files use this shape; every field inside the tables is optional:

```toml
schema_version = 5

[defaults]
accessibility = "auto"
color_scheme = "system"
contrast = "system" # effective override only in the user settings file
sequential_navigation = true
directional_navigation = true
mode = "windowed"
display = "current"
decorated = true
center = true
initial_sizing = "preferred"
viewport_mode = "reflow"
ui_scale = 1.0
follow_system_scale = true
renderer = "auto"
gpu_driver = "auto"
renderer_fallback = true

[graphics]
frame_scale = 100
scene_scale = 100
automatic = false
scene_filter = "linear"

[apps.demo]
initial_sizing = "fit-content"

[apps.minesweeper]
display = "primary"
```

| Field | Accepted values / units |
|---|---|
| `mode` | windowed, maximized, desktop-fullscreen, exclusive-fullscreen, borderless-display, borderless-work-area |
| `display` | primary, current, named |
| `display_name` | String hint; required after merging when selection is named |
| `decorated`, `center` | Booleans; full/borderless modes may override decorations |
| `exclusive_size` | Two positive integers: requested display-mode pixel dimensions |
| `refresh_rate` | Finite nonnegative number; zero chooses no explicit Hz preference |
| `initial_sizing` | preferred, fit-content, restore-previous |
| `viewport_mode` | reflow, fixed-canvas |
| `viewport_fit` | contain, cover, stretch |
| `canvas_size` | Two finite positive logical extents, width then height |
| `alignment` | Two finite factors in [0,1], horizontal then vertical |
| `ui_scale` | Finite positive Reflow zoom |
| `follow_system_scale` | Boolean |
| `resolution_scale` | Finite value in (0,4], whole-frame raster multiplier |
| `glyph_atlases` | Boolean, default true; GPU blended horizontal text uses SDL_ttf atlases; false requests raster/upload compatibility |
| `vsync` | Boolean, default true; GPU requests VSYNC or immediate (falls back to VSYNC if unsupported); software pacing remains platform-controlled |
| `renderer` | auto, software, sdl-gpu; a preference, not an app requirement |
| `gpu_driver` | auto or vulkan; Auto selects Vulkan for hardware; dormant when explicitly selecting software |
| `renderer_fallback` | Allow another compatible backend/driver; never weakens app requirements |

AppInfo::rendererRequirements owns RendererRequirements. AppContext::rendererState reports
requested and actual selection, capabilities and a nonempty fallbackReason when
an explicit preference could not be met. Auto selects a compatible available
implementation without claiming fallback merely because it chooses software.
Software supports 2D and 3D. GPU supports both plus linear composition when compiled
shaders and a device are available. Backend changes are applied outside live frames;
failure attempts to restore the old backend. CPU sources are retained, while
device-specific layers, images and atlas output are rebuilt. The host
realizes required capabilities before activation, including scene shader/pipeline
preparation when 3D is required. It applies settings to the active runtime before
persisting them. Old `metal` and `direct3d12` preferences are rejected explicitly.
These are owner-thread safe-boundary operations, not atomic OS/GPU transactions.

The shipped project file specifies shared presentation defaults, not a duplicate
Demo sizing policy. Demo2D starts with a preferred resizable gallery viewport;
content-sized apps such as Minesweeper declare FitContent. An explicit user/project
override can still change it.

Unused mode-specific fields remain valid for a later mode change. Numeric values
are range-validated even when currently unused. Resizability, preferred window size, minimum size, app
title, asset paths and game rules are deliberately not part of this schema.
This is extensible presentation infrastructure, not an untyped global configuration bag.

`SettingsPatch` uses optional fields: absent means inherit, false/zero remain values.
Remove a persisted override to reset it to the next layer; this is not UI's
Keep/Set/Reset patch protocol. `SettingsDocument::apply` merges values only;
`SettingsStore::resolve` validates the completed result before publishing outputs.
`resolveWithUser` previews replacement user settings against the original baseline
and project values, not against yesterday's already-merged state.

## Publication and persistence

SettingsStore borrows two FileStores; both must outlive it. Its API is synchronous,
owning-thread only. `readSnapshot()` reads/parses all three documents without
publication; `snapshot()` captures published documents and `publish()` replaces
them. `reload()` combines reading and publication. AppHost uses snapshots to
restore documents if runtime activation fails. Cross-layer conditions, such as a named
display needing a name, are checked during resolution. A successfully parsed
document is not proof that every possible app-baseline combination is valid.

`setUser(document, persist)` validates serialized values and, if requested, writes
before publishing the replacement user document. AppHost validates, realizes the
required renderer services and submits the merged window preferences first.
Disk failures preserve the previous published document and trigger restoration of
the previous backend/window policy. A synchronous restoration failure is terminal
and retains both exception causes.

Window completion is asynchronous: saving preferences records requested intent,
not proof that the OS applied it. A later native failure or unconfirmed transition
is reported through `windowRequestStatus()` and logged; it does not undo a settings
save or app activation. Restoring a checkpoint likewise submits a window request.
See [window outcomes](WINDOWING.md#runtime-requests-and-failure-boundaries).
An explicit save rewrites TOML canonically: comments/formatting are not retained.
Runtime-only changes do not touch `settings.toml`. App switches/settings reloads
resolve values again; explicit runtime presentation requests do not edit preferences.

App switching keeps the outgoing app alive while constructing and entering the
candidate. Candidate commands are deferred and discarded on failure; on success,
the old app's cleanup runs before it is destroyed. `onEnter` must build owned state,
not perform irreversible external effects. Shared service mutations and external
IO cannot be rolled back automatically. `onExit` is cleanup-only; its exceptions
are logged and isolated, and its host commands are discarded. It must not rely on
the outgoing window configuration still being active. Synchronously rejected commands preserve
the prior app and are reported through `AppContext::lastCommandError()`; startup,
restoration and unrecoverable renderer failures terminate normally through the
entry-point exception handler. There is no interactive error dialog.

## Rendering recovery

GPU command acquisition/submission/presentation failures use `RenderFailure`,
distinct from invalid props, resource allocation and application exceptions. A
failed frame is abandoned before backend recreation; model updates and callbacks
are not replayed. CPU sources remain owned, and a new resource domain invalidates
native realizations. The default `RecoveryPolicy` permits one automatic attempt.
Its budget resets only after five accumulated seconds of observed, advancing
completed work on submitted frames; queue acceptance alone does not count. Each
observation contributes at most 0.25 seconds, so a single stalled frame cannot
clear probation. Skipped/no-target presentations restart probation, and a failed
recreation or second failure before probation completes is terminal.
`rendererRecovery()` exposes status, attempt count, healthy duration and reason. An explicit
`requestRendererRecovery()` starts a new attempt budget at the next safe boundary.

Recovery is synchronous at a safe frame boundary: simulation is paused during
recreation, and the next update receives zero delta rather than the time spent
recovering. Updates/callbacks already performed are never replayed. After a
published resource-domain change, `IApp::onRendererChanged(ctx, previous, current)`
can discard native caches without rebuilding model state or replaying `onEnter`.
This nonthrowing invalidation hook must not request host commands. Initial entry
uses an unspecified previous domain; software realizations share the CPU domain.
Settings-driven replacement and restoration similarly rebase the update clock.

`RenderFrame::present()` returns `PresentationOutcome::Submitted` or `Skipped`;
neither reports GPU completion. `RenderBackend::completedWork()` nonblockingly
polls a backend-local completion sequence. Software advances after a successful
surface update; GPU advancement requires completed submission fences.

SDL GPU does not expose a typed device-loss notification in the pinned version.
This policy is bounded recovery from native submission failures, not guaranteed
recovery from every device-loss or driver-hang scenario. It does not match SDL
error strings or catch arbitrary application exceptions and call them device loss.

## Performance sampling

`PerformanceMonitor` records CPU Poll, Update, Render, Present and Total phases;
these are host durations, not GPU execution times. `history()` retains at most
`PerformanceConfig::historySize` complete samples (default 240, maximum 65536).
Zero disables history retention without disabling summaries. Missing phases have
an explicit measured flag and are not fabricated zero-duration observations.
Resizing the history keeps newest samples; rejected props/samples do not publish
partial data. Reports clear interval aggregates but preserve history.

`recordGPU(GPUTimingSample)` accepts only completed native timestamp observations;
`gpuHistory()` retains them separately with the same entry budget. Samples carry
their resource domain, native sequence and label, rather than pretending to line up with the CPU
frame that happened to receive them. No supported query means no sample, not zero
GPU time. Reports show the latest newly collected GPU scope, without averaging
unrelated scopes or mixing GPU duration into CPU Total. Labels are limited to 128
bytes so retained telemetry is bounded beyond just its entry count.

`RenderBackend::supportsGPUTiming()` reports native support, separately from
whether monitoring is enabled. AppHost updates `setGPUTimingAvailable()` when
domains change and before recording. `gpuTimingStatus()` distinguishes
Unsupported, Disabled, Pending and Measured. Each CPU history entry snapshots the
status and whether a fresh completed GPU sample arrived during that frame; it
does not attribute the GPU work itself to that CPU frame. A report without a new
sample explicitly says unsupported or pending, never `0ms`.
Optional completion latency is separately validated and labeled: it measures
host submit-to-observed-completion delay, including queueing and polling delay,
and must not be mistaken for the native GPU scope's execution duration.

F10 toggles optional reporting and detailed UI measurement. While reporting is
enabled, Shift+F10 reports the current interval. F11 switches the reporting
interval between 60 and 300 frames while preserving enabled state, logging policy
and history budget. Toggling reporting resets only its reporting sample state;
baseline runtime counters and timing collection continue independently.

## Window session persistence

Normal window geometry is saved per app on app switch and orderly run-loop exit.
It uses signed desktop coordinates (negative positions are legitimate) and a
display-name hint, never native display IDs. Session write failure is logged and
does not block switching; parse/configuration failures propagate. Abrupt process
termination can lose recent unsaved geometry. OS maximize/fullscreen gestures do
not automatically rewrite the authored mode preference.

```toml
schema_version = 1
[apps.demo]
size = [798, 978]
position = [-1200, 40]
display_name = "External display"
```

## Filesystem boundary

`FileStore::read(name)` returns optional UTF-8 document text: nullopt means absent;
exceptions mean access/read/validation failure. `replace(name, text)` replaces one
complete document. Test implementations can provide an in-memory store.

`DirectoryStore(root, writable)` resolves only leaf names under one absolute root;
it rejects separators, parent traversal, Windows alternate-stream syntax and NUL.
It is not a security sandbox against symlinks, hostile filesystem races, or an
untrusted process. It limits documents to 1 MiB and distinguishes read-only project
stores from writable user stores.

Writes create a uniquely named, exclusively opened temporary file in the same
directory, check writing/closing, then use SDL_RenamePath to replace the destination.
Temporary files are cleaned on ordinary exception paths. This avoids exposing a
partially written final document, but promises neither fsync/power-loss durability
nor interprocess locking/conflict resolution. Concurrent writers are last-writer-wins.
No directory enumeration, arbitrary deletion, filesystem watching, asynchronous IO,
or general asset-management API is implied by this small document store.

`preferenceDirectory` and `executableDirectory` centralize native path discovery.
Public paths use std::filesystem::path; conversion at SDL boundaries is UTF-8.
AppHost constructs these after SDL initialization and owns stores longer than the
settings service. UI callbacks request changes through AppContext; they do not write
files during paint/layout or retain a temporary AppContext.

## Dependencies and extension boundaries

toml++ 3.4.0 parses/formats TOML privately in Settings.cpp; CPM builds it without
examples/tests and install includes its license. The public headers expose no TOML
nodes. JSONC support would be a different serializer, not a second runtime authority.
Build-time CMake options remain separate from these runtime project settings.

Before adding new persisted settings, specify units, layer precedence, validation,
whether they apply immediately/on entry/only after window recreation, and recovery
behavior. Keep transient observed state out of user preferences. Disk/parse errors
are std::exception-compatible and the current app entry point reports them; no
interactive preferences/error UI or background file watcher is implemented.

References: [SDL preference paths](https://wiki.libsdl.org/SDL3/SDL_GetPrefPath),
[SDL executable base path](https://wiki.libsdl.org/SDL3/SDL_GetBasePath),
[replacement rename](https://wiki.libsdl.org/SDL3/SDL_RenamePath),
[toml++ release](https://github.com/marzer/tomlplusplus/releases/tag/v3.4.0).

## Live resource and pacing policy

`AppContext::renderRuntimeState()` exposes managed commitments, policy revisions,
pressure, frame admission and the latest baseline CPU/GPU telemetry.
`renderTelemetry()` copies bounded CPU/GPU histories only when requested; raw
GPU samples retain originating frame IDs. Owner-thread
`requestRenderRuntime(RenderRuntimePatch)` validates a patch and applies it at the
next host boundary outside a frame. Repeated pending patches merge their budget
and pacing fields. These low-level runtime patches are transient and never
write user files. Applying shared `GraphicsSettings` replaces their budget/pacing
values; its explicit Save path persists those preferences in `[graphics]`.

```cpp
auto state = ctx.renderRuntimeState();
auto pacing = state.pacing;
pacing.maximumFramesPerSecond = 60;
ctx.requestRenderRuntime({.pacing = pacing});
// Remove the cap while retaining the outstanding-frame policy.
pacing.maximumFramesPerSecond.reset();
ctx.requestRenderRuntime({.pacing = pacing});
```

A pacing value validates one to three outstanding application frames and an
optional 1–1000 Hz cap. SDL's native presentation allowance remains independent
(default two); changing host admission does not flush the device queue. Budgets
can be lowered below existing commitments without revoking resources; new growth
is refused until usage permits it. See [RESOURCES.md](../render/RESOURCES.md).

## Shared graphics and settings view

[GRAPHICS.md](GRAPHICS.md) defines schema version 5 shared graphics, draft
Apply/Save behavior, inactive future features and automatic scene resolution.
These preferences apply across sub-apps. Escape opens the host settings view;
Ctrl/Cmd+Shift+M returns to the launcher.
