# Window, viewport, and presentation contracts

The host owns the native window; applications describe intent; UI measures and
arranges content. These are separate authorities, not three copies of window size.
See [settings](SETTINGS.md), [UI contracts](../ui/CONTRACTS.md), and
[rendering](../render/2D.md) for their adjoining boundaries.

## Values and ownership

`app::PresentationSession` owns the Window and window-lifetime native
WindowServices (AccessKit, clipboard and IME),
backend, presentation/view/window values, observed state, checkpoint restoration
and renderer recovery. AppHost owns settings persistence, active-app lifecycle,
command processing, frame orchestration, clock rebasing and renderer-domain
notifications. UISession remains a separate UI mapping/input/root adapter.
Preferred content measurement is a synchronous borrowed callback. Restoration
is explicit and fallible, never a destructor operation; HostTransitions continues
to supply the shared activation/recovery sequencing tested with fake resources.

| Contract | Owner / meaning |
|---|---|
| `WindowConfig` | Low-level creation options for `Window`/the initial AppHost window, including high-pixel-density and transparency capabilities |
| `AppInfo::window` / `AppWindowProps` | App title, backend-neutral clear color, focusability, visibility, mouse grab, always-on-top; no sizing side effects |
| `AppInfo::view` / `AppViewPolicy` | Initial size policy, optional preferred window-coordinate size, minimum logical UI size, and app-controlled resizability |
| `AppInfo::presentation` / `PresentationProps` | App baseline for window mode/display, viewport mapping, and rendering resolution; settings can override it |
| `WindowState` | Queried native flags, actual size, optional desktop position, pixel dimensions, display scale and last normal-window bounds |
| `WindowMetrics` | Current window-coordinate size, physical drawable size, and OS display scale |
| `ViewportMapping` | Derived logical viewport, offsets, forward painting scale and inverse input mapping |

Public declarations live in `include/platform/{WindowTypes,Presentation}.hpp`
and `include/app/{AppTypes,IApp,AppContext}.hpp`. Pure presentation values have
no SDL types; native queries and creation remain in `Window`.

`AppWindowProps` deliberately has no fullscreen/resizable fields. Set these via
`presentation.window.mode` and `view.resizable`, respectively. Creation-only
features cannot change just because a new app enters the existing window.
Resizability is not a user-file override. A nonresizable app can still request a
new size through its host when content changes.
WindowConfig is native creation configuration, not a frame-clear contract; it
does not contain clearColor. App colors use math::ColorRGBA8, with conversion only
at native boundaries.

Titles are app properties, not flags. Requested modes are not observed state:
an OS can delay or deny native requests. Geometry/events are re-queried rather
than treating every requested rectangle as an accomplished result. Normal bounds
are kept separate from maximized, minimized, fullscreen and fake-fullscreen bounds.
`actualPosition` and `windowedPosition` are optional: an unavailable global
position is not the origin and must not be persisted as one. Session schema 2
stores normal size even when placement cannot be observed; schema 1 coordinates
remain readable. See [session compatibility](SETTINGS.md#files-and-precedence).

## Entry and sizing

```text
app baseline + project settings + user settings
    -> resolved policy/presentation
    -> stage preferred/bootstrap size; apply app window props
    -> onEnter: construct/attach UI and load required resources
    -> preferred size, content measurement, or saved normal bounds
    -> submit geometry, mode, and minimum-size/resizing policy
    -> event/update/render loop advances native transition from observations
```

| InitialWindowSizing | Behavior |
|---|---|
| `Preferred` | Use `AppViewPolicy::preferredWindowSize`, or the host's shared bootstrap size when absent |
| `FitContent` | Ask `IApp::preferredContentSize(maximum, density)` after onEnter; keep preferred size if it returns nullopt |
| `RestorePrevious` | Restore saved normal size, clamped onto an available work area; restore saved placement only when present and supported; use preferred size if no saved state exists |

The bootstrap comes from the host's initial WindowConfig, not from each app's
layout arithmetic. Demo and Minesweeper declare FitContent with no preferred
window size; their trees are the sizing authority. Before onEnter the host stages
the bootstrap/preferred size, then replaces it with measurement/restoration when
available. Measurement is synchronous; only owned numeric values enter the native
transition. onEnter does not imply that the previous native mode or size has
already changed. The initial native window has a positive bootstrap size.

An app with a retained UI forwards the query to `UIRoot::preferredSize`. This is
a bounded loose measurement including box padding/borders; it does not arrange
the tree, resize a window, or continually chase the current viewport. Measurements
may update normal measurement caches. Call outside UI traversal/lifecycle callbacks.
Flexible/percentage/wrapping content is constrained by the offered maximum, not an
unbounded promise of a unique natural size.

`FitContent` converts logical size to window units and caps it to the selected
display work area, subtracting native decorations where the platform reports them.
System limits win over content that is too large. Content still needs appropriate
scrolling, clipping or responsive layout; the host does not manufacture it.
Fixed-canvas fitting uses its declared canvas size instead of a tree measurement.

Fitting occurs on entry or `ctx.requestFitContent()`, **not every frame**. This
avoids a size/layout feedback loop and fighting a user's resize gesture. Explicit
fit requests operate in windowed mode. Demo and Minesweeper implement preferred
measurement; Minesweeper requests another fit when it rebuilds for a new grid.
Successful saved-position restoration takes precedence over initial centering.

## Presentation modes and displays

| WindowMode | Native meaning |
|---|---|
| `Windowed` | Normal client window, optional decorations |
| `Maximized` | OS-managed maximization |
| `DesktopFullscreen` | SDL fullscreen using the desktop mode |
| `ExclusiveFullscreen` | Closest available mode to `exclusiveSize` and `refreshRate`; zero refresh means no explicit preference |
| `BorderlessDisplay` | Ordinary borderless window sized/positioned to full display bounds |
| `BorderlessWorkArea` | Ordinary borderless window sized/positioned to usable bounds, excluding OS-reserved areas where reported |

Borderless modes are not SDL fullscreen. Work-area sizing does **not** force a
taskbar, Dock, Start menu, panel or desktop shell to remain visible: the OS owns
those policies. No portable interface can guarantee exact desktop coordinates
everywhere. The active SDL video driver determines placement capability; Wayland
does not advertise global-position queries or placement requests. This concerns
native window management, not the Vulkan rendering backend: Wayland sessions
remain usable without pretending a submitted request has moved the window.

`DisplayPreference` chooses `Primary`, `Current`, or `Named`. An unavailable
named/current display falls back to primary when preferences are applied. Names
are best-effort hints, not unique stable hardware identities; matching uses the
first display of that name. Never persist SDL_DisplayID: it is a runtime token.
On placement-capable backends, explicit display selection requests placement on
that display even when `center` is false; `Current` plus `center=false` preserves
placement. Changing
apps or explicitly reapplying presentation may center again. Ordinary rendering
and OS resize events do not issue centering requests.

Queries refresh on window/display events. Reapply presentation after a topology
change if the OS fallback placement is insufficient; automatic display-mode
negotiation across hotplug is not a guaranteed contract. Invalid preferences or
unavailable mandatory native modes can be rejected before submission. Unsupported
placement is distinct from a failed native call; the host does not silently
substitute a different mode.

## Layout scale, viewport fit, and raster scale

| Setting | What changes | What does not |
|---|---|---|
| `ViewportMode::Reflow` | Logical viewport follows available window space; tree remeasures/rearranges |
| `ViewportMode::FixedCanvas` | Logical viewport stays at `canvasSize`; mapping uses Contain, Cover, or Stretch |
| `ViewportProps::scale` | Reflow logical-unit size (user UI zoom), combined with system scale when enabled |
| `RenderSettings::resolutionScale` | Number of pixels used for whole-frame rendering | Layout, window size, hit geometry |

Contain letterboxes; Cover crops; Stretch can distort. `alignment` is a normalized
0..1 positioning factor for the resulting spare/cropped space. FixedCanvas derives
its mapping from canvas/window dimensions, not the reflow zoom/system-scale fields.
Those fields also supply the logical-to-window conversion for initial content fit.

For Reflow, `followSystemScale=true` combines display scale and physical/window
pixel density once. This accounts for platforms where window coordinates are
already density-independent and those where they are not. With it disabled, scale
is directly in window-coordinate units. A UI node's local transform is a separate
operation and is not a replacement for this viewport contract.

`UISession::synchronize(ctx.windowMetrics(), ctx.presentation().viewport)` resolves
layout space and physical density. The session clips/transforms its borrowed
PaintContext and inversely maps mouse/touch positions and motion deltas. Wheel
steps remain steps, not geometric distances. Letterbox areas cannot start hits;
captured releases still reach controls outside the canvas. Synchronize before
dispatch/render after dimensions or preferences change. Supply WindowMetrics and
ViewportProps explicitly; the old two-size compatibility overload is removed.
For one logical unit per window unit, use Reflow, scale=1, followSystemScale=false.

The software backend supports finite resolution scales greater than zero through
4. At 1 it draws directly to the current window surface; otherwise it reuses an
offscreen target and linearly scales it on presentation. Target extents round up.
The painter reports actual target density, including rounding; resource preparation
uses that density. Lower resolution can blur **all** content, including text. This
is not a scene-only resolution slider or a GPU implementation.

## Runtime requests and failure boundaries

`AppContext` exposes `windowProps()`, `windowState()`, `windowMetrics()`, `presentation()`,
`viewPolicy()`, `windowRequestStatus()`, `windowPlacementCapabilities()`, and:

- `requestWindowProps(AppWindowProps)` replaces current app window properties without resizing, recentering or changing presentation. Copy windowProps(), modify, then request the replacement.
- `requestViewPolicy(AppViewPolicy)` replaces the runtime sizing/resizability policy and reapplies its selected sizing behavior to the existing content. This is the explicit route for resizing; it does not reconstruct the app.
- `requestFitContent()` requests a one-shot fit without changing policy.
- `requestPresentation(PresentationProps)` replaces the current runtime presentation.
- `requestReloadSettings()` reloads files and resolves the active app's baseline.
- `requestUserSettings(SettingsDocument, bool persist=false)` replaces user overrides.

Requests are deferred until host-safe boundaries, never executed while a frame is
alive. The existing pending-command slot **coalesces**: the last non-quit request
wins; Quit cannot be displaced. It is not a FIFO. Do not issue several independent
requests expecting all of them to run. Prefer one complete presentation/settings
value. Runtime presentation/view-policy overrides are not automatically persisted and end at
the next app switch/settings resolution.

SDL window setters submit requests: leaving fullscreen, restoring, resizing,
and placement can complete later or be denied. Resizing while still fullscreen
or maximized has no effect. `WindowTransition` therefore waits for normal state
before geometry, then requests the final mode. Desktop-fullscreen display routing
runs only if the observed display differs from the target. See SDL's
[fullscreen](https://wiki.libsdl.org/SDL3/SDL_SetWindowFullscreen),
[resize](https://wiki.libsdl.org/SDL3/SDL_SetWindowSize), and
[restore](https://wiki.libsdl.org/SDL3/SDL_RestoreWindow) contracts.

`Window::requestPreferences(WindowRequest)` validates before replacing pending
work and returns a generation. Omitted normal geometry retains the pending
request's values, or the last observed normal bounds when no request is pending.
`requestStatus()` reports the generation, outcome, diagnostic, and optional
placement result. AppContext exposes the same status. The host logs terminal
diagnostics; asynchronous failure does not roll back accepted native changes.

| Outcome | Meaning |
|---|---|
| `Idle` / `Pending` | No request / transition still being advanced |
| `Observed` | SDL reported the required geometry, mode and fullscreen display; unsupported optional placement is excluded |
| `Unconfirmed` | Required observations did not arrive by the deadline, or optional placement failed; not proof of OS denial |
| `Failed` | A required native operation failed; remaining stages stop |
| `Cancelled` | Remaining stages were cancelled; already submitted native requests may still take effect |

`advanceTransition()` runs at host-safe boundaries; `transitionWakeAt()` keeps
pending work progressing during idle periods. Each stage submits at most once.
The geometry grace period is 250 ms, pending work schedules another poll after
16 ms, and the observation deadline is five seconds. These bound application
waiting, not native call duration. The adapter does not call
[SDL_SyncWindow](https://wiki.libsdl.org/SDL3/SDL_SyncWindow); SDL can still
synchronize internally during Cocoa Spaces changes. Cancellation and supersession
do not undo requests already submitted to SDL. Checkpoints preserve a pending
request; observed normal bounds remain valid even if a later mode request fails.

`placementCapabilities()` distinguishes global placement support from acceptance.
Wayland exposes no global placement for ordinary toplevel windows; size restoration
still works without saved coordinates. Borderless display/work-area modes require
placement and are rejected there before native changes. Desktop-fullscreen output
selection remains a separate operation. See SDL's
[Wayland limitations](https://wiki.libsdl.org/SDL3/README-wayland).
`requestWindowedPosition()` returns `Submitted`, `Unsupported`, or `Failed`.
Optional placement failure is nonfatal but cannot yield `Observed`.
`setWindowedPosition()` and `applyPreferences()` are convenience request APIs;
returning is not a completion guarantee. `applyPreferences()` requires subsequent
transition advancement, which AppHost supplies.

## Verification boundary

`window_transitions` covers placement capability, observation-gated ordering,
supersession/cancellation, bounded waiting and ignored geometry using a synthetic
clock, with no native window. `ui_viewport`, `ui_presentation_settings`, and
`ui_render_backend` cover math, measurement, input mapping, session migration,
and offscreen rendering. The latter also checks adapter supersession, preflight
failure, cancellation, and preservation of normal geometry after a mode failure.
They are UI constituent tests, not game/AppHost smoke tests.
Real multi-monitor, fullscreen, mixed-DPI, taskbar and non-Windows behavior requires
interactive platform verification; SDL's dummy driver cannot establish it.

Native references: [window sizing](https://wiki.libsdl.org/SDL3/SDL_SetWindowSize),
[placement](https://wiki.libsdl.org/SDL3/SDL_SetWindowPosition),
[fullscreen modes](https://wiki.libsdl.org/SDL3/SDL_SetWindowFullscreenMode),
[usable display bounds](https://wiki.libsdl.org/SDL3/SDL_GetDisplayUsableBounds).

## Removed contracts and replacements

Appearance is view policy, not an SDL window flag or rendering backend option.
`AppViewPolicy::colorScheme` chooses system/light/dark; its resolved userContrast
is supplied by settings. `UISession::synchronize(AppContext&)` observes desktop
appearance and updates the root palette. Page backgrounds are explicit
`PaintStyle::themeBackground` fills; transparent scene overlays stay transparent.
Native contrast observation never changes OS settings. See
[appearance contracts](../ui/ACCESSIBILITY.md#appearance-and-transient-surfaces)
and [settings precedence](SETTINGS.md).

| Previous API/assumption | Current authority |
|---|---|
| Per-app window-size arithmetic mirroring UI padding/footer | UIRoot preferred measurement and FitContent |
| AppWindowProps::preferredSize | Optional AppViewPolicy::preferredWindowSize; absent uses the host bootstrap |
| requestWindowConfig / ReconfigureWindow | requestWindowProps / SetWindowProps for metadata; requestViewPolicy or requestFitContent for sizing; requestPresentation for window mode/viewport/rendering |
| WindowConfig::clearColor and AppHost's separate color copy | AppWindowProps::clearColor, passed directly into each frame |
| DisplayState and AppContext's separate windowSize/drawableSize queries | WindowMetrics, including display scale |
| UISession two-size overload | Explicit WindowMetrics + ViewportProps |
| Window::setSize / setPosition aliases | setWindowedSize for size submission; requestWindowedPosition for explicit placement result |
| Synchronous Window::applyPreferences completion assumption | requestPreferences(WindowRequest) plus requestStatus/observed WindowState; applyPreferences remains a submission wrapper |

Changing metadata does not reapply presentation, and content sizing does not need
SDL surface dimensions or application-level color conversions. App configs use
AppViewPolicy and WindowMode rather than a separate fullscreen boolean.
