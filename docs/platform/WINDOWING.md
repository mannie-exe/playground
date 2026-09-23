# Window, viewport, and presentation contracts

The host owns the native window; applications describe intent; UI measures and
arranges content. These are separate authorities, not three copies of window size.
See [settings](SETTINGS.md), [UI contracts](../ui/CONTRACTS.md), and
[rendering](../render/2D.md) for their adjoining boundaries.

## Values and ownership

| Contract | Owner / meaning |
|---|---|
| `WindowConfig` | Low-level creation options for `Window`/the initial AppHost window, including high-pixel-density and transparency capabilities |
| `AppInfo::window` / `AppWindowProps` | App title, backend-neutral clear color, focusability, visibility, mouse grab, always-on-top; no sizing side effects |
| `AppInfo::view` / `AppViewPolicy` | Initial size policy, optional preferred window-coordinate size, minimum logical UI size, and app-controlled resizability |
| `AppInfo::presentation` / `PresentationProps` | App baseline for window mode/display, viewport mapping, and rendering resolution; settings can override it |
| `WindowState` | Queried native flags, actual position/size, pixel dimensions, display scale and last normal-window bounds |
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

## Entry and sizing

```text
app baseline + project settings + user settings
    -> resolved policy/presentation
    -> return window to normal mode; apply app window props
    -> onEnter: construct/attach UI and load required resources
    -> preferred size, content measurement, or saved normal bounds
    -> apply requested presentation mode and minimum-size/resizing policy
    -> normal event/update/render loop
```

| InitialWindowSizing | Behavior |
|---|---|
| `Preferred` | Use `AppViewPolicy::preferredWindowSize`, or the host's shared bootstrap size when absent |
| `FitContent` | Ask `IApp::preferredContentSize(maximum, density)` after onEnter; keep preferred size if it returns nullopt |
| `RestorePrevious` | Restore that app's saved normal bounds, clamped onto an available work area; use preferred size if none exist |

The bootstrap comes from the host's initial WindowConfig, not from each app's
layout arithmetic. Demo and Minesweeper declare FitContent with no preferred
window size; their trees are the sizing authority. Before onEnter the host applies
the shared bootstrap (or an explicit preferred override), then replaces it with
measurement/restoration. Native windows must have a positive size before UI exists;
this preliminary size is not a second content-size specification.

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
those policies. Wayland and mobile platforms may restrict placement or modes.
No portable interface can guarantee exact desktop coordinates everywhere.

`DisplayPreference` chooses `Primary`, `Current`, or `Named`. An unavailable
named/current display falls back to primary when preferences are applied. Names
are best-effort hints, not unique stable hardware identities; matching uses the
first display of that name. Never persist SDL_DisplayID: it is a runtime token.
Explicit display selection positions the window on that display even when
`center` is false; `Current` plus `center=false` preserves placement. Changing
apps or explicitly reapplying presentation may center again. Ordinary rendering
and OS resize events do not issue centering requests.

Queries refresh on window/display events. Reapply presentation after a topology
change if the OS fallback placement is insufficient; automatic display-mode
negotiation across hotplug is not a guaranteed contract. Native requests can throw
on unsupported operations; the host does not silently substitute a different mode.

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
`viewPolicy()`, and:

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

Native transitions are not transactions: a late SDL error may follow an earlier
successful native change. Such errors currently propagate to the application's
top-level exception handler. RenderFrame/PaintContext lifetimes and resource RAII
still apply; a settings screen with recoverable error presentation is not provided.

## Verification boundary

`ui_viewport`, `ui_presentation_settings`, and `ui_render_backend` cover math,
measurement, input mapping, parsing/persistence failures, and offscreen software
resolution scaling. They are UI constituent tests, not game/AppHost smoke tests.
Real multi-monitor, fullscreen, mixed-DPI, taskbar and non-Windows behavior requires
interactive platform verification; SDL's dummy driver cannot establish it.

Native references: [window sizing](https://wiki.libsdl.org/SDL3/SDL_SetWindowSize),
[placement](https://wiki.libsdl.org/SDL3/SDL_SetWindowPosition),
[fullscreen modes](https://wiki.libsdl.org/SDL3/SDL_SetWindowFullscreenMode),
[usable display bounds](https://wiki.libsdl.org/SDL3/SDL_GetDisplayUsableBounds).

## Removed contracts and replacements

| Previous API/assumption | Current authority |
|---|---|
| Per-app window-size arithmetic mirroring UI padding/footer | UIRoot preferred measurement and FitContent |
| AppWindowProps::preferredSize | Optional AppViewPolicy::preferredWindowSize; absent uses the host bootstrap |
| requestWindowConfig / ReconfigureWindow | requestWindowProps / SetWindowProps for metadata; requestViewPolicy or requestFitContent for sizing; requestPresentation for window mode/viewport/rendering |
| WindowConfig::clearColor and AppHost's separate color copy | AppWindowProps::clearColor, passed directly into each frame |
| DisplayState and AppContext's separate windowSize/drawableSize queries | WindowMetrics, including display scale |
| UISession two-size overload | Explicit WindowMetrics + ViewportProps |
| Window::setSize / setPosition aliases | setWindowedSize / setWindowedPosition |

Changing metadata does not reapply presentation, and content sizing does not need
SDL surface dimensions or application-level color conversions. App configs use
AppViewPolicy and WindowMode rather than a separate fullscreen boolean.
