# Application composition

Games and collaborative applications share the same host/service boundaries.
[Distributed app authoring/loading](DISTRIBUTED_APPS.md) and executable-app
[permissions/isolation](SECURITY.md) remain explicitly future capabilities.

## Ownership and composition

| Owner | Responsibility |
|---|---|
| AppHost | Platform services, one foreground compiled IApp, settings and presentation |
| IApp | App model, action map, activation lifetime and local simulation policy |
| Domain model | Stable entities, validated commands, rules, revisions and durable state |
| World | Spaces, entity identity, frame relationships, snapshots and durable changes; see [world contracts](WORLDS.md) |
| Session authority | Participants, command validation, shared state and private views |
| UIRoot / scene | UI behavior or render descriptions; neither owns gameplay authority |
| Asset catalog/mount | Immutable definitions, dependency closure and retained source lifetimes |
| Service scope | App-owned audio, network and asynchronous work independent of local presentation pause |

Models accept domain commands and publish state/events without owning SDL events,
windows, UI nodes or GPU resources. Local input and network adapters translate to
the same domain operations; presentation projects accepted state into UI, scenes
and sound cues. Rebuilding a view or replacing a renderer preserves the model.
Persistent model IDs, session IDs, asset IDs and transient NodeHandles remain
separate. Never serialize pointers, callbacks, variant indexes or cache keys.

One app can mount several packs; many apps can share immutable pack storage.
Mounting data does not create an app instance or grant code execution. Each
service defines admission, cancellation, deadlines and terminal outcomes; the
completion mailbox does not replace its byte/voice/connection limits.

Durable model snapshots carry an app identity and schema version. Validate and
migrate a candidate before atomic replacement; failure preserves the old state.
Save data belongs to app storage, separate from shared settings and immutable
packs. Domain commands with durable external effects record their result with
the state transition before acknowledging success. UI state is persisted only
through an explicit app view-state policy.

Gameplay worlds use owned model collections and explicit controller/system order.
Render parenting does not imply physics ownership. Kinematic controllers,
collision/physics, navigation and character pose production remain separate
interfaces; a universal ECS or behavior tree is not required by this boundary.

## Spatial-data applications

Apps may own DatasetStore, source/product/edit services and optional simulation
without creating a World. A world app attaches datasets through explicit bindings;
neither storage chunks nor samples automatically become entities. Generators,
material interpretations, update rules and multimedia meaning belong to app/domain
providers. Core services own bounds, lifetime, admission and publication.

UI input, automation and network adapters submit the same validated SpatialEdit
commands. Views select current versus displayed-generation queries explicitly.
Apps choose live ReadOnly/Captured/Overlay editing, undo retention, durability and
required-product barriers. There is no implicit universal game material or ECS.
See [datasets](SPATIAL_DATA.md), [edits](VOXELS.md) and [simulation](SIMULATION.md).

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
lifetime of this IApp instance, not across launcher reactivation or process exit.

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

## Scene/game runtime boundaries

Scene3D owns render-object hierarchy, transforms, visibility and immutable
resource references. It is not a general gameplay SceneTree. The retained UI tree
has different layout, event-routing and lifecycle responsibilities. Do not merge
them merely because both have parents and children.

AppHost owns activation, presentation and frame boundaries. An application should
own its gameplay world and choose its simulation policy. The host routes input,
accepts guarded CPU completions, runs optional bounded fixed steps, then performs
frame update and presentation. See [runtime contracts](RUNTIME.md) for APIs,
input snapshots, pause policy and controller usage. Hierarchy changes and destruction occur at safe
boundaries; render parents need not imply ownership of physics or gameplay actors.
Add controllers/components when an actual app exercises that contract, rather
than giving every render object update hooks and all possible services.

Worlds use [space-aware positions and stable identities](WORLDS.md); SceneProjection
creates local render representations without transferring authority to Scene3D.
[Streaming](STREAMING.md) and activation preserve durable entities when views/cells
retire. [Procedural data](PROCEDURAL.md) shares that lifecycle. [Navigation](NAVIGATION.md)
supplies autonomous movement requests; the app arbitrates player, path-following
and scripted control explicitly. The [Jolt adapter](PHYSICS.md) realizes physical
movement; an owned world or explicitly kinematic navigation can still omit it. Headless apps can use all
model/service boundaries without constructing rendering or UI services.

Procedural character work should separate desired motion, motor/controller output,
physical constraints/contact resolution, and rendered pose. Muscle/neuron models
are optional simulation models, not prerequisites for a camera or scene graph.
Define units, timestep, authority and interpolation before introducing parallel
simulation. Workers produce owned results; they do not mutate the live scene.

## Input integration

Launcher numeric selection and Q-to-quit use named BeforeUI actions.
Ctrl/Cmd+Shift+M is a host shortcut; unhandled Escape opens/closes shared settings
after app/UI routing, preserving local popup/edit cancellation. Minesweeper's
focused cell navigation, reveal/flag operations and button activation remain
routed UI behavior: these actions depend on focus/hit testing,
not an independent second gameplay dispatch. UI text/IME remains separate.
Apps can add AfterUI gameplay contexts and rebind them through `IApp::input()`.
Binding persistence, capture UI, gestures and response curves remain future work.

## Launcher and demo organization

The launcher groups UI Test, Material Test, Bistro/Chess scenes and study games.
[Rendering demos](../render/DEMOS.md) define entries, asset ownership, compatibility
fallbacks and controls. Menu configuration is the single ordered list for groups,
labels and numeric bindings. Group labels are not focus stops; the content scrolls
when the available window cannot contain all entries.

Q quits from the launcher. Unhandled Escape opens settings; Ctrl/Cmd+Shift+M
returns from an app. Button callbacks queue an app identity, and AppHost switches
at a safe boundary. Failures remain visible in the launcher or scene loading view.
Persisted keys demo and material-lab retain existing UI/material preferences.
Rock Paper Scissors and Snake remain study stubs.

## Accessibility boundary

Controls expose names, values, enabled state, keyboard operations and visible
focus through the implemented AccessKit bridge. WindowServices publishes validated
semantic identities and routes native actions on the owner thread. Native reader,
IME and cross-platform acceptance remain distinct from unit/build coverage; see
[accessibility contracts](../ui/ACCESSIBILITY.md). Do not expose hidden Minesweeper
bomb state in semantics. UI-backed apps expose session input claims through IApp;
modal/editor ownership blocks AfterUI actions even for otherwise unhandled keys.

## Manual Minesweeper acceptance

Launch `dist/debug/bin/playground` (`playground.exe` on Windows), select
Minesweeper, and try both presets and custom 2 x 2 / 30 x 30 boards. Verify the exact flag allowance, New Game reset,
Difficulty return, Escape settings, and Ctrl/Cmd+Shift+M launcher navigation.
Exercise mouse reveal/flag, Tab/Shift+Tab, arrow navigation and Shift+Enter flagging. At small sizes the footer
may be wider than the board; at large sizes the window should fill only as much
of the usable display as needed, then permit overflow scrolling. Keyboard focus
must remain visible when traversing an overflowing board. Repeat across display
scale/monitor changes when those environments are available.

Focused module tests cover NumberStepper, Button focus painting and Content scroll
measurement; they do not substitute for this desktop interaction review.

## Future browser target: platform port, not just a renderer

Web/WASM is an exploratory target, not a supported build. SDL itself supports
Emscripten, but [SDL_GPU currently excludes the web](https://wiki.libsdl.org/SDL3/FAQDevelopment).
A GPU browser renderer would therefore implement the existing rendering contracts
through a separate API, potentially [Emscripten's WebGPU port](https://emscripten.org/docs/porting/multimedia_and_graphics/WebGPU-support.html).
The native SDL command-buffer/custom-pipeline and timestamp extensions are not
portable browser interfaces. Shader production, resource bindings, limits,
compression support and glyph realization need backend-specific validation.

The current host's blocking desktop loop needs a nonblocking frame-pump adapter
that returns control to the browser. Asset acquisition and persistent settings
need browser storage/fetch implementations rather than native paths; the native
AccessKit window bridge needs a browser accessibility/input adapter, potentially
projecting the same semantics into DOM elements. Fullscreen, pointer capture,
clipboard and IME must follow browser policy rather than desktop assumptions.

Dependency builds (including Unicode/font/asset libraries) require a WASM
toolchain audit. [Emscripten pthreads](https://emscripten.org/docs/porting/pthreads.html)
require cross-origin isolation for shared memory; a threadless target would need
an explicitly cooperative execution policy, not blocking waits on the UI thread.
[Browser networking](https://emscripten.org/docs/porting/networking.html) also needs
transport adapters instead of assuming ordinary native sockets. Keep shared
application models, UI semantics and renderer contracts independent of these
adapters; no browser implementation or new platform dependency is introduced now.

## Benchmark activation

The Bistro benchmark is one app identity with a launch duration (5 seconds,
15 seconds or unbounded). Launcher buttons share its implementation and looping
camera path. Launch configuration is activation-local, not a persisted graphics
setting. Settings remain shared across apps; benchmark results record the actual
settings and invalidate on changes during measurement.

## Host integrations and interruption

`AppHost::hostCompletions()` is a weak, bounded owner-thread delivery endpoint for
host integrations and automation that must survive app switches. It is not an
app permission surface. App work uses activation-scoped `completions()` instead.
Both queues wake the normal scheduler and close with their owner; callbacks run
at the update boundary, never on producer threads.

`IApp::onActivityInterrupted` carries an `AppInterruption` reason and is a
nonthrowing notification for Settings and focus
loss. Exclusive interaction and uninterrupted measurements release/invalidate
there. Scene benchmarks use activation-local `AppLaunchProps::benchmarkSeconds`:
zero is infinite, absence selects the app default. Ordinary apps reject it.
