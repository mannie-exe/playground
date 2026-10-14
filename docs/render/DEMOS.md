# Demo applications and fixtures

`playground_demo` owns these entries, controls and study-game policies.
`playground_voxels` owns medical/scalar-volume inspection; `playground_minecraft`
owns a native spectator client. These [application workloads](../apps/WORKLOADS.md)
exercise optional modules without defining core host rules. Oasis remains a useful
authored-voxel fixture.

Physics integration does not prescribe a new user-facing demo or menu entry.
Verification may reuse these assets/scenes or add focused test scenes/executables
through the shared harness. Additional user-facing shared-session demonstrations
are **future** and require application/UX design. The separate Minecraft spectator target is required in its
[application contract](../apps/MINECRAFT.md). Native multi-client session tests
use the non-spatial reference domain in [networking verification](../platform/NETWORKING.md#verification).

## Launcher and ownership

The launcher groups applications under Demo 2D, Demo 3D, Benchmark and Play. Group titles
are noninteractive; buttons use ordinary focus, activation and scrolling.

| Group | Entry | Persisted app key |
|---|---|---|
| Demo 2D | UI Test | demo |
| Demo 3D | Material Test | material-lab |
| Demo 3D | Scene: Bistro | scene-bistro |
| Demo 3D | Scene: Chess | scene-chess |
| Benchmark | Bistro: 5s / 15s / Infinite | benchmark-bistro |
| Play | Minesweeper | minesweeper |
| Play | Rock Paper Scissors | rock-paper-scissors |
| Play | Snake | snake |

Existing app identities survive label changes. Each scene is a separate app
activation with its own camera and preparation lifetime. Shared loading and
presentation code receives a scene definition; it does not duplicate import,
environment preparation or failure handling for every scene.

Material Test remains an object-inspection scene without architectural scenery.
It presents the SciFi Helmet and Flight Helmet, retaining the BoomBox/smoke
fixtures where useful. Environment illumination remains active. A small unlit
sphere displaying the source HDR environment provides a visual reference; it is
ordinary scene geometry, not a skybox or a new lighting feature.

Scene: Bistro presents the Amazon Lumberyard exterior. Scene: Chess presents
A Beautiful Game, preserving the individual pieces as scene objects. Neither
entry defines gameplay. Scene apps provide a place to exercise rendering,
controllers and later scene services independently of the material fixture.

## Content preparation and distribution

Models and textures live under assets/ and are installed beside the executable;
they are not linked into the binary. Future pack storage preserves the same
logical asset identities. Large source downloads are authoring inputs, not
runtime dependencies. Conversion runs offline and is reproducible from pinned
sources, tool versions and recorded transformations. Normal launches and installs
never download content or invoke Blender.

Keep source geometry and authored texture resolution unless an explicit measured
limit requires a documented derivative. File size, decoded CPU storage, preparation
scratch and GPU residency are separate measurements. A larger asset directory does
not justify unlimited runtime allocation. Use checked scene-specific import limits
and existing runtime budgets; failures identify the scene/resource and cause.

Bistro FBX/DDS sources are converted to glTF. Preserve base-color/opacity,
roughness-metallic channel meanings and emissive maps. Bistro's unused, zero-filled
occlusion channel must not become glTF occlusion. Convert DirectX
normal-map orientation to glTF's convention. Do not bake unsupported shadows or
lighting into a purported material reference. Retain licenses, authors, source
URLs, checksums and conversion notes with each distributed model.

Prepare only the activated scene, using owned catalog handles and cooperative
cancellation. Leave CPU assets reusable across renderer replacement; native
resources retain renderer-domain ownership. Failed or canceled preparation never
publishes a partial scene, and switching away releases activation-owned results.

## Rendering compatibility

These demos require the existing GPU metallic-roughness pipeline. They introduce
no shadows, glass transmission/refraction, global illumination or new transparency
technique. Existing opaque, alpha-mask and alpha-blend behavior remains available.
Unsupported optional material extensions use the core glTF metallic-roughness
material and retain a diagnostic. Glass may therefore appear opaque.

An explicitly selected demo material-fallback policy may ignore recognized
unsupported material extensions even when declared required. It must identify
the affected extension and preserve a usable core material. Default imports still
reject unsupported required extensions. Geometry/compression extensions, malformed
data and missing resources never become silent material fallbacks.

## Camera controls

Controllers consume typed intent and produce world cameras independently of SDL.
Material Test and Chess use inspection navigation: MMB orbit, Shift+MMB pan,
Ctrl+MMB zoom, wheel zoom, and R to reset panning while the viewport is focused.
Alt+primary drag substitutes for MMB. The cursor remains visible and each drag
owns input only until release/cancellation. F does not change these scenes into
free/follow mode. P toggles Material Test smoke playback.

Bistro uses WASD movement, arrows to look, Space/Left Shift to rise/fall in free
mode, F to switch free/follow, and R to restore the free-camera framing.
Right-click inside the viewport engages relative mouse look; Escape first unlocks,
then opens Settings on a subsequent press. Q/E changes exposure and L toggles
light. Bistro starts at its authored street camera; Chess orbits the board center.
Camera selection is viewport-local, and input displacement is consumed once.

Phantom Camera informs the separation of camera behavior, selection and final
viewport output; Playground does not depend on Godot or the plugin. The broader
rig/director and pointer ownership contract is in [CAMERAS.md](CAMERAS.md).
Opening Settings, losing focus or leaving an app cancels held navigation intent.
Exposure and light controls remain separate from movement. Resetting inspection
panning or free-camera framing does not rebuild scene resources.

### Subject-following navigation

Bistro exposes free navigation and subject-following navigation as distinct
interaction choices. A following view uses an app-owned kinematic anchor and an
optional simple avatar; importing Bistro or Chess does not imply a character,
physics world or animation skeleton. Free navigation and authored reset framing
remain available in Bistro. Material Test and Chess remain inspection workloads.

Follow navigation exercises [Steered, Strafe and Tank](../platform/LOCOMOTION.md)
with shared [control preferences](../platform/SETTINGS.md#shared-control-preferences).
The default is gradual OutOfNowhere-style steering. Perspective is independent of
locomotion; wheel/assigned zoom actions move through first/third-person thresholds
in Automatic mode. The app supplies follow/eye anchors and per-view self-visibility.
Manual control has explicit engagement, UI precedence and neutral rearming.

No scene object blocks the camera or kinematic subject in this scope. Obstruction
capability reports Unavailable. Do not add collision meshes, infer blockers from
material opacity or silently describe free passage as grounded walking. Physics
and [camera obstruction](CAMERAS.md#future-camera-obstruction) remain future work.
The benchmark retains its authored camera path and disables manual navigation;
interactive profile/perspective changes do not alter a measured workload.

## Bistro benchmark

The Benchmark group follows Scene: Chess. Its Bistro row contains three equally
sized buttons: 5s, 15s and Infinite. Duration selects one shared workload; it does
not select separate paths or content. The path loops every 15 seconds and is
sampled from monotonic elapsed time rather than frame count.

Loading and initial resource preparation precede warm-up. Warm-up renders a full
path cycle; finite measurement begins afterward and ends at its duration. Reports
separate CPU recording, completed GPU samples, submitted frames and wall time.
GPU completion is collected before a final report; incomplete samples remain
explicit. Infinite runs repeat the path until stopped and publish periodic
summaries. Completion retains the scene and results; R restarts the run.

Settings and focus loss invalidate a run even during loading; scene readiness
preserves that outcome until explicit restart. Window resizing or graphics-policy
changes during traversal also invalidate the run rather than compare different
workloads silently. Returning to the menu cancels preparation and releases
activation-owned state. Interactive camera
input is disabled during automated traversal. Escape still opens settings; the
normal return-to-menu shortcut remains available.

Results identify scene/content version, path version, duration, renderer/domain,
actual pixel extent and applied settings. Loading, warm-up, measured traversal and
completion draining are distinct states. Captures verify geometry and camera
correctness; they are not performance evidence on their own. Developer workloads
exercise the same app/controllers and support repeatable automated launches.

See [verification](TESTING.md) for resource and responsiveness invariants.


## Minesweeper screen contract

Minesweeper owns its difficulty draft, active board, and pending screen intent.
The difficulty screen and game screen are alternative retained views inside one
IApp, not entries in AppRegistry. Launcher navigation still uses AppContext.
Callbacks record intent; the app applies it after routing/update, never while a
tree is being traversed. Candidate model/view construction precedes replacement;
the old model outlives destruction of every view that borrows it.

Difficulty uses exact bomb counts. Presets and bounded custom rows/columns/bombs
feed the same validated board props. New Game resets the selected board; changing
difficulty constructs a new model. Returning to difficulty preserves the draft.
Bomb placement does not yet guarantee a safe first click.

Easy is 9 x 9 with 10 bombs; Hard is 16 x 16 with 40. Custom dimensions are
2..30 per axis and bombs are 1..(cell count - 1); shrinking a draft clamps its
bomb count. New Game rerolls the selected count, and Difficulty discards the old
board only after its replacement menu is attached. Selection persists for the
lifetime of this app instance, not across launcher reactivation or process exit.

Tab/Shift+Tab navigate controls; Enter/Space activate ordinary buttons on release.
Steppers also accept arrows. The game grid has one tab stop, arrows select adjacent
cells, Enter/Space reveal and Shift+Enter/Space flag. Focus follows the selected
cell and keyboard navigation scrolls only enough to reveal it. The Difficulty
button returns from game to difficulty; the Launcher button leaves difficulty.
Unhandled Escape opens shared settings; Ctrl/Cmd+Shift+M returns to the launcher.
Mouse cells retain primary reveal/secondary flag on press. Focus is assigned after layout, not during
construction, because root focus eligibility requires arranged nodes.

The window fits natural content up to the display work area, including the host's
decoration and density conversion. Scrolling is a fallback for actual overflow,
not an arbitrary grid-size breakpoint. Footer width is independent of board width.


## Manual Minesweeper acceptance

Launch `playground_demo`, select
Minesweeper, and try both presets and custom 2 x 2 / 30 x 30 boards. Verify the exact flag allowance, New Game reset,
Difficulty return, Escape settings, and Ctrl/Cmd+Shift+M launcher navigation.
Exercise mouse reveal/flag, Tab/Shift+Tab, arrow navigation and Shift+Enter flagging. At small sizes the footer
may be wider than the board; at large sizes the window should fill only as much
of the usable display as needed, then permit overflow scrolling. Keyboard focus
must remain visible when traversing an overflowing board. Repeat across display
scale/monitor changes when those environments are available.

Focused module tests cover NumberStepper, Button focus painting and Content scroll
measurement; they do not substitute for this desktop interaction review.


## Benchmark activation

The Bistro benchmark is one app identity with a launch duration (5 seconds,
15 seconds or unbounded). Launcher buttons share its implementation and looping
camera path. Launch configuration is activation-local, not a persisted graphics
setting. Settings remain shared across apps; benchmark results record the actual
settings and invalidate on changes during measurement.

## Built-in application reconstruction

Demo owns ViewProps and handles and constructs a candidate via makeDemo2DUI.
MinesweeperApp owns MinesweeperModel; UI/Grid/Cell borrow it and display its
revision. Board state lives in the model, independently of nodes and SDL input events.
Rebuilding a view preserves the model; new game explicitly resets it. A board's
dimensions remain immutable; different settings require a new matching model/UI.
Minesweeper's app-local difficulty screen owns no board: it edits the app's draft,
and Start stages a matching model/view before replacing the menu. See
[Minesweeper screen contract](#minesweeper-screen-contract) for input and sizing contracts.

The new-game button retains its view-owned connection. Cells invoke typed model
actions; visual synchronization follows routing and runs during update. Clear
operations stage a board copy and iterative flood queue before committing. This
prioritizes consistent failure over optimizing a small study board. Visual sync
can fail independently and retry while revisions differ. Tree reconstruction
intentionally resets hover/capture/node IDs, not durable model state.
