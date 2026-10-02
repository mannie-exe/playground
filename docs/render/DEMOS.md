# Rendering demos

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

Controllers consume typed intent and produce CameraProps independently of SDL,
UI nodes and rendering. Material inspection keeps keyboard orbit/zoom. Scene
navigation adds a free camera with explicit movement/angular rates and resettable
framing. WASD moves, arrows look, Page Up/Down changes elevation, R resets,
Right-click inside the viewport toggles relative mouse look; Escape first
unlocks the mouse, then opens Settings on a subsequent press. Relative motion
is angular displacement, not multiplied by frame time. Q/E changes exposure and
L toggles direct light. Bistro starts at its source
street camera; Chess starts framed around the board. Future camera selection and
bookmarks are viewport-local; named shots contain pose and lens values, never
projection matrices or borrowed scene pointers.

Phantom Camera informs the separation of camera behavior, selection and final
viewport output; Playground does not depend on Godot or the plugin. The broader
rig/director and pointer-orbit design is in [CAMERAS.md](CAMERAS.md).
Opening Settings, losing focus or leaving an app cancels held navigation intent.
Exposure and light controls remain separate from movement. Reset restores the
scene's authored inspection camera without rebuilding its resources.

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
