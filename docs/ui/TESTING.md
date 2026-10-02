# Testing retained UI and its constituents

This document maps UI contracts to focused module tests and states the limits
of that coverage. [REFERENCE.md](REFERENCE.md) defines behavior; [GUIDE.md](GUIDE.md) teaches
usage. [CONTRIBUTING.md](../../CONTRIBUTING.md#testing) defines commands and test-authoring
rules. [tests/CMakeLists.txt](../../tests/CMakeLists.txt) owns test registration.
Author obligations are summarized in [CONTRACTS.md](CONTRACTS.md); painter,
device and scene boundaries are documented in [2D](../render/2D.md),
[GPU](../render/GPU.md) and [3D](../render/3D.md).

## Scope

Asset/runtime constituent coverage additionally includes asset_catalog,
asset_resources, model_preparation and runtime_executor. These verify logical
identity, dependency closure, shared resources, generic tree reconstruction,
bounded worker admission and explicit owner publication. They do not test
Minesweeper game rules or declare asynchronous layout safe.

Tests cover the UI module and its constituents: geometry, layout, retained
ownership, events, scheduling, SDL adapters, painting and resource support.
The host_smoke integration test opens/applies/closes Settings against the real
AppHost using a native window and an isolated temporary user directory. Headless
dummy/offscreen drivers or unavailable SDL video skip this test.
AppHostDirectories can override project/assets and user roots; omitted roots
retain executable-relative assets and the platform preference directory.
Native rendering/interactive platform behavior and game rules remain separate.
Resource tests may read checked-in assets without treating their owning game
as a fixture.

CTest runs independent executables; assertions use runtime checks that survive
NDEBUG or compile-time static_assert. Build the selected targets before running
CTest. A successful application build verifies compilation of consumers, not
their runtime behavior.

ui_text_field checks trailing-space/tab and bidirectional caret geometry.
ui_font_family checks ordered emoji fallback, cache identity, variants and
colored raster output, including the portable Noto chain. These do not establish
complete Unicode shaping or emoji-sequence coverage.

## Contract inventory

Idle integration adds `runtime_activity` (independent cadence, revision
acknowledgement, timer deadlines, post-before-wake and reentrant destruction of
replaced wake captures), `event_wake` (SDL event
coalescing, filtered/reentrant delivery, stale endpoints), and `ui_runtime`
paint-time invalidation checks. `ui_window_services` checks native publication
reuse and geometry/mapping invalidation. `performance_reports` separates idle
waits and paint counters from measured CPU frames. Opt-in `gpu_device` compares
fast/general pixels for fractional and mirrored rectangles, nested opacity,
rounded borders, and rejects clipped paths without corrupting preceding draws.
These checks do not replace interactive exposure/resize, IME, screen-reader and
power-utilization checks on each operating system.

`ui_control_paint` also checks slider snapping with a subnormal positive step:
an unrepresentably large step quotient must retain the interpolated position,
not overflow into an endpoint jump.

| Boundary | Tests and exercised behavior | Coverage limits |
|---|---|---|
| Math and SDL conversion | `geometry`, `geometry_properties`: arithmetic, bounds, empty rectangles, conversions/rounding, finite/overflow failures, 250 affine/rectangle sweeps | Not exhaustive IEEE-754 values or checked integer arithmetic; callers must avoid ordinary integer overflow/division by zero |
| Layout values and algorithms | `layout_values`, `layout_arithmetic`: Keep/Set/Reset/nullopt, size rules, tracks/anchors, 1,000 flex allocations, all six distributions and four alignment values, 100 flow sequences | Parameter sweeps are not formal proofs or every extreme float interaction |
| Common props and alignment | `ui_layout_variants`: all six cross alignments, lengths, bad opacity/insets/clips, patch preservation, bad display density, overlapping AdaptiveStack conditions | Not every field's setter/patch interaction or arbitrary invalid enum casts |
| Ownership/runtime | `ui_runtime`, `ui_runtime_failures`: attach rejection, handles, reparent, deferred removal, dirty caches, notifications, signals, timer exceptions/reentrancy/cancellation | No global allocation-failure injector; not every lifecycle callback can be interrupted at every instruction |
| Async delivery | `ui_completions` plus `ui_runtime`: real worker posting, owner-thread delivery, source revision mismatch, removed nodes, destroyed root | Throw/order/reentrancy/retry-after-failure cases; no thread sanitizer/high-contention stress |
| Box/Stack/Grid/Flow/Anchor/ZStack | `ui_containers`, `ui_layout_variants`, `ui_placement_patch`: bounds, gaps, fractions/spans, overlap rejection, alignment, RTL, placement-by-ID, Reset | Not full CSS conformance; large sparse-grid pathological complexity not benchmarked |
| Component/CustomView/Spacer/Transform | `ui_component_variants`: replacement success/failure, implicit spacer growth, source-order paint/reverse-order hits, translated picking | Not a GPU/rotated-render test |
| Visibility/input policy | `ui_component_variants`: all 3 visibility × 4 hit-test-policy combinations | Combination matrix uses a small overlay tree, not all ancestor/descendant structures |
| Button | `ui_button_states`, `ui_containers`: primary/secondary click, wrong release, drag out/back, cancel, disable mid-press, Space/Enter/repeat | Not every multitouch interleaving |
| NumberStepper | `ui_number_stepper`: keyboard/button activation, limit focus transfer, disabled/single-value range, saturating adjustment, invalid patches and throwing notifications | Native reader announcement still requires interactive verification |
| Unicode model | `ui_text_edit`, `ui_text_boundaries`: shared label/editor boundaries, combining/ZWJ/Indic clusters, bidi UTF-8 offsets, composition isolation, targeted deletion/undo, invalid UTF-8/NUL, capacity, readonly/password | Not every Unicode conformance corpus; segmentation follows the linked ICU version |
| Plain editor | `ui_text_field`: macOS Option/Command navigation and deletion, real font shaping, wrapped bidi runs, offscreen painting, clipboard injection, password snapshots, invalid/valid numeric drafts | Native IME candidate windows and reader text-range behavior are manual checks |
| Actions/semantics | `ui_accessibility`: derived disabled actions, modal restriction/restoration, stale identities, explicit neighbors and numeric rejection | Native OS callback concurrency is not simulated by these owner-thread tests |
| UI/application navigation ownership | `ui_navigation_routing`: decorative trees pass arrows/Tab; editor/modal claims block gameplay keys; focus, disable/hide/replacement and pointer capture invalidate ownership; `input_actions`: selective held-action cancellation, neutral reacquisition, global overrides and modal dismissal | Synthetic input routing, not an interactive camera test |
| Native adapter lifetime | `ui_window_services`: hidden native window attachment, mode changes, detach/replacement and destruction | No screen reader is driven; dummy/offscreen SDL drivers bypass the native adapter portion |
| Composite controls | `ui_control_variants`: toggles, radio/list selection, Select, Disclosure, Tabs, label/help relationships, Status, Tooltip, ProgressBar | Demo2D supplies a manual gallery; it is not an automated app test |
| Content-sized scrolling | `ui_content_scroll`: natural sizing, work-area clamps, overflow extent, resizing, offset reset and legacy Fill constraints | Actual OS work-area/window decoration behavior still needs desktop verification |
| ScrollView | `ui_scroll_variants`, `ui_containers`: all 3 axes × 3 scrollbar policies, offset clamping/invalid input, removal, overlay drag, nested local placement/hits, provisional measurement and residual wheel propagation | Not every nested scrollIntoView alignment or platform gesture |
| AdaptiveStack/Repeat | `ui_collections`, `ui_collection_failures`: mode switches, all three Repeat layouts, stable keyed reorder, duplicate keys, null factory, retry, empty model, refreshed model-backed child measurements | Not all partially throwing update/onAttach permutations |
| VirtualList/VirtualGrid | `ui_collections`, `ui_collection_failures`, `ui_virtual_list_scale`: fixed/estimated, horizontal/vertical, RTL, indexed scroll, focus pinning/removal, million-item bounded realization; update-then-erase | Not all variable-height anchor changes under arbitrary model edits or captured-pointer pinning combinations |
| Extent index | `ui_extent_index`: 100 updates, every prefix versus linear sum, boundary/search sweep versus linear lookup, zeros, invalid reset/update preservation | Extreme double-sum overflow/cancellation not exhaustively exercised; production virtual-list boundary also validates representable totals |
| Content fit | `ui_content_fit`: all 5 fit × 3 horizontal × 3 vertical alignment × 2 directions × 2 target shapes = 180 cases; empty/invalid modes | No enormous/subnormal input-domain proof |
| Text | `ui_content_resources`: all 4 methods × 2 wraps × 2 fits × 3 paragraph alignments × 2 directions = 96 cases, actual SDL pixels, empty text, required-font Reset | LBRITE + short Latin text is not Unicode shaping/fallback/script conformance testing |
| Image/Vector | `ui_content_resources`: natural density/crop validation, required-handle Reset, final-size/DPI raster requests, failed loads, budgets, density-only failure/readiness, recovery, zero area | Element styles have separate tests; limited asset corpus, not all decoder failures |
| Clip/Layer/Painter | `ui_clip`, `ui_content`, `ui_layer_cache`: content clip/hits, singular-hit diagnostic, opacity pixels, axis-aligned and affine pixel checks, cache invalidation including local scrolling beneath a cached ancestor, root/per-layer budget refusal/release | No injected SDL allocation failure at every stage |
| AssetRegistry | `ui_asset_cache`: sharing, request-recency eviction, live-handle pinning, byte estimates, failing/null factories, retry, clear with external owner | Memory estimates omit allocator/font/GPU overhead; eviction is not a hard memory cap |
| SDL event adapter | `ui_sdl_input`: event-result combination/termination/formatting, supported key map, key metadata, mouse buttons, all four touch event types, wheel flip, unknown host event | Synthetic events do not establish real-device/platform behavior or multiple-device interaction |
| SDL errors | `sdl_errors`: std::exception catch, context/detail, consumed error state, null and valid resource | No process-wide failure injector |
| Rendering frame/session adapter | `ui_render_backend`: non-SDL recording context, lazy surface acquisition, clear/draw pixels, overlapping-frame rejection, double presentation/use-after-present rejection, exception cleanup/clip restoration, resized target reacquisition, reduced-resolution full-window presentation, observed window geometry | SDL dummy video driver; not AppHost/game smoke coverage, GPU behavior, or interactive fullscreen/multi-monitor verification |
| Affine/rounded painting | `ui_affine_shapes`: corner normalization, negative radii, rotation/reflection, transformed clipping, group alpha, rounded picking, singular no-paint, state restoration, opaque border/fill joins, anisotropic preparation density | Software/offscreen tests do not measure interactive performance or all transformed edge combinations |
| Text flow | `ui_text_flow`: combining/ZWJ boundaries, invalid UTF-8, all ellipsis positions, narrow output, line caps, both vertical progressions and three orientations, source-font preservation, rejected LCD/bidi/Tr combinations | Limited fonts/scripts; not CJK fallback, Unicode line-breaking conformance or publishing-quality typography |
| SVG styles | `ui_svg_styles`: styled pixels, immutable originals, cache reuse, no-paint, missing IDs, invalid stroke width | Limited SVG corpus; not full CSS cascade or every parser/decoder failure |
| Explicit track grid | `ui_track_grid`: offscreen spans, frozen panes, stable handles, revision validation, anchor preservation, estimated refinement, consecutive edits, empty-model pane validation | Not every span/frozen-pane/RTL/model-edit combination or adversarial population |
| Constraints | `ui_constraints`: parent/sibling anchors, required conflicts, missing keys, priorities, inequalities, resize, protected detach, width/baseline remeasurement, pre-attachment validation, RTL-only relations, bounded nonconvergence/recovery | No exhaustive constraint-graph/numeric conditioning assessment |

### Rendering/layout constituents

| Contract | Focused tests | Boundary |
|---|---|---|
| Viewport / preferred measurement | `ui_viewport`: optional authored versus shared-bootstrap sizing and invalid sizes, cross-platform DPI coordinate conventions, reflow zoom, all fixed-canvas fits, paint/input round trips, letterbox capture/release and touch routing, bounded padded measurement without arrangement, invalid/overflow extents | Synthetic input; no real monitor/window-manager behavior |
| Presentation settings/storage | `ui_presentation_settings`: layer precedence, absent versus false/zero, replacement preview, enum round trips, parse/schema/type failures, rejected-write preservation, signed session coordinates and isolated file replacement | No real user-directory writes, process crash/power-loss injection or concurrent writers |
| Breakpoints | `breakpoints`: half-open adjacency, 2D bounds, unknown offers, invalid/overlapping ranges, Keep/Set/Reset and failed-update preservation | No hysteresis or automatic subtree replacement |
| 3D math | `geometry3d`, `transform3d`: matrix/camera arithmetic, quaternion composition, 30 TRS/inverse sweeps, homogeneous division, inverse-transpose normals, singular/nonfinite rejection | No condition-number estimate, SIMD or renderer execution |
| Color semantics | `color_space`: all 256 byte-channel roundtrips, continuous transfer sweep, linear source-over, alpha-zero and invalid-channel rejection | SDR helpers, not a migration of SurfacePainter or ICC/HDR conformance |
| Renderer negotiation | `renderer_selection`, `ui_render_backend`: automatic/explicit selection, Vulkan policy, strict and allowed fallback, non-negotiable requirements, factory capability agreement | Synthetic candidates plus software factory; hardware negotiation has opt-in tests |
| Scene submission | `ui_scene_contracts`: valid/empty submissions, malformed triangles/indices, nonfinite geometry/model, invalid target and null mesh | No GPU scene execution |
| Scene ownership and mapping | `scene_viewport`, `scenes`: immutable mesh bounds, hierarchy updates, projection/picking, software depth/alpha/filtering | Selected pixels and transforms, not exhaustive model fidelity |
| Model importing | `model_import`: static glTF/GLB, hierarchy, materials, atomic scene publication and malformed/unsupported data | Static triangle subset; decoder and filesystem confinement are not a sandbox |
| Model file adapter | `model_import_files`: bare/absolute document paths, percent-decoded sibling files, traversal/network rejection | Isolated temporary files; no decoder fuzzing or symlink race guarantee |
| Paths | `vector_paths`: curve flattening, fill rules, closure, stroke and complexity limits, UI/software integration | Solid fills and round strokes, not the full SVG paint model |
| Shader contracts | `shader_contracts`: SPIR-V reflection, bindings, stage interfaces and rejected layouts | Native pipeline creation/reload requires `gpu_shaders` |
| Runtime handoff | `ui_completion_queue`, `runtime_transitions`: bounded delivery, throwing callbacks, rollback and recovery state | Does not assert rollback of arbitrary app side effects |
| Image realization | `ui_image_preparation`: pitch packing, all encoding/alpha pairs, metadata mismatch rejection, byte counts, invalid device ownership, Image/Text/Vector preparation/retry and retained ownership | GPU upload and atlas code compile; no device is created; metadata tests do not prove correct labeling of arbitrary source pixels |

## Assessment rules

`ui_callback_lifetimes` checks prompt capture release, delayed cancellation,
recursive self-disconnection, self-cancellation during an exception, and repeating
timer recovery. `ui_layer_failures` checks null/empty/nonfinite capture rejection,
painter-state and budget cleanup, retry, and successful cache reuse without SDL.

`playground_docs_check` compiles the GUIDE's additive C++ blocks as one generated
translation unit. It is a build-only contract check, not a program or runtime test.

A node appearing in a test establishes component reach, not complete branch,
property-combination, platform or failure coverage. Passing executables do not
supply a meaningful implementation-completion percentage. Test outcomes belong
in the run output, not a permanently stated passing status in this reference.

Arithmetic checks use independent oracles where practical: linear sums and
search for the extent index, sum-of-extents conservation for flex/flow, and
affine round-trip tolerances. Repeated-execution equality separately tests
determinism. Parameter sweeps are not formal proofs, and integer operators
retain normal C++ overflow/division preconditions.

A regression should assert an observable contract: final bounds, routed action,
handle validity, callback ordering, cache invalidation or selected pixels.
Success, rejection and recovery are distinct outcomes. A throwing callback may
have committed side effects; test preserved ownership and unattempted work,
not rollback that the API does not promise.

Treat numerical tolerances as part of the assertion. Do not use approximate
equality for cache keys, infer performance from wall-clock deadlines, or equate
offscreen pixel checks with interactive visual quality.

## Coverage boundaries

Renderer-specific tests are separate constituents: `scenes` checks scene ownership,
software depth/clipping/alpha pixels and SceneView preparation. Hardware
test `gpu_device` (Vulkan only) covers actual GPU pixels, groups, atlas preparation and
frame lifecycle. GPU-enabled builds include hardware tests and compiled shaders;
ordinary UI tests do not require a device. See the renderer contracts for limits.

Additional renderer/runtime constituents:

| Test | Contract |
|---|---|
| ui_completion_queue | Capacity/work budgets, FIFO, worker publication, owner-only/nonrecursive drain, failure-tail preservation, closed/expired sinks |
| runtime_transitions | Captured-state restoration, operation/restoration double failure, bounded recovery probation, completed/skipped work, update-clock rebasing |
| resource_values | Unique resource domains, overflow-safe target/upload byte limits |
| allocation_budget | Reservation cap, overflow rejection, failure preservation, in-flight lease ownership |
| gpu_timing | Injected native timestamp table, capacity, cancellation, failure quarantine, unavailable results, counter wrap, owner-thread rules |
| performance_monitor | Bounded CPU/GPU histories, absent measurements, availability states, hotkey preservation and invalid sample rejection |
| ui_layout_work | Linear ancestor visits, unchanged siblings, isolated boundary work, exception retry, in-pass mutation, detached queued IDs, exact two-offer leaf reuse and A/B/A virtual realization |
| ui_stack_baselines | Mixed button padding, every pair of cross alignments in both child orders and directions, partial/fallback baselines, collapsed/empty rows and invalid child rejection |
| ui_control_text_layout | Six real-font labels across button/checkbox/switch/select, four widths and four densities; raster/measurement agreement and render-only scaling stability |
| ui_overlays | Clipped owners, viewport placement/fallback, no-flow sizing, preview/commit/cancel, focus restoration, outside press/release consumption, hide/focus-loss cleanup and invalid patches |
| ui_theme | Four appearances, forced colors, authored/effective overrides, invalid metrics, explicit zero, reparenting and live composite geometry |
| ui_control_paint | Light/dark/high-contrast slider hover/drag/cancellation, pointer ownership, vertical endpoints/tiny bounds, full-width borderless option rows |
| ui_scroll_layers | Reserved gutters, clipped-content/chrome paint ordering, track and corner hit priority, drag cancellation, coupled axes, Auto removal and tiny viewports |
| text_pixels | Styled glyph bearings/crops, straight-alpha mixed runs, natural/explicit line spacing, patch reset and immutable cache distinction |
| scene_viewport | Camera/letterbox/input mapping, projection, stable alpha ordering, immutable meshes and cached world transforms |
| vector_paths | Curves, fill rules/holes, round strokes, bounded flattening, retained Path node and software pixels |
| shader_contracts | Malformed SPIR-V, reflected slots, stage linkage, type/count mismatches |

Performance-monitor checks include deterministic same-phase timing nesting and
per-root delta aggregation without double consumption. Content-resource variants
check that repeated preparation and foreground-color changes reuse text layout.
Content-resource checks also load the installed-source Material SVG icons, check
centered 24-unit geometry and inherited/disabled tint, and verify opt-out/reset.
Host transition sequencing remains tested with fake resources through the same
HostTransitions operations now used by AppHost and PresentationSession. Native
window sizing/rollback requires separate platform execution; these tests do not
exercise the desktop or modify user settings.

Hardware text tests include a vendored licensed color-glyph fixture when present;
the test asserts COLOR glyph support before comparing decoded/premultiplied pixels.
Coverage includes partial-alpha colored edges and opacity, bold, italic, outline,
underline/strikethrough, wrapped alignment and directional text. Italic/outline,
Twemoji's layer-only color fixture, Bungee's outlined-base color fixture and mixed
monochrome/color fallback runs require actual atlas preparation and compare full
CPU/GPU geometry and color with color-space-aware tolerances. Checked-in SDL_ttf
patches repair bitmap bearings/crops and CPU color-glyph alpha association.
GPU tests also exercise tracked target leases, cancellation, completed reuse and
native timestamp execution when the Vulkan device advertises that capability.
This is not exhaustive emoji/script/font/style conformance. Resource recovery
tests do not simulate a driver hang or physically removed device.

The inventory does not establish:

- exhaustive allocation-failure injection or interruption of every lifecycle path;
- all nested scrolling, focus, capture and multitouch interleavings;
- high-contention concurrency, thread-sanitizer or long-running memory-pressure behavior;
- equivalent results across build configurations, compilers, Linux/macOS and Windows;
- real-window high-DPI, display transitions or native accessibility behavior;
- complete font/script coverage, SVG/CSS conformance or full-domain floating-point correctness.

Record the platform, configuration and selected tests when reporting a run.
Keep the inventory aligned with source tests when contracts change. Use the
[cost model](REFERENCE.md#cost-model) to choose meaningful scale cases; do not turn
module tests into application benchmarks.

## Control composition

`ui_control_contracts` covers decimal editing, invalid drafts, custom validation,
external-value conflicts, explicit form submission, keyed choices, command
invocation, group policies, accordion bounds, meter/progress distinction, toolbar
focus and bounded notifications. `settings_view` exercises shared draft editing,
Apply/Save and future enum choices. `ui_number_stepper` covers readout layout,
adjustment bounds and notification contracts. Native interaction and screen-reader
acceptance remain separate from these deterministic checks.

`ui_control_regressions` covers publication failure from text capacity/invalid
codecs, dirty external refresh, Select label/help relationships, nonmodal edit
dismissal, composite toolbar Tab traversal, specialization invariants, independent
toast hover/focus pauses, event provenance, slider terminal states, meter threshold
semantics and focus-within boundaries.

## Rendering workloads

Build and run the maintained harnesses with the release preset. A CMake target
here is the executable to build; no install or private compile-command parser
is required.

```sh
cmake --preset release
cmake --build --preset release --target playground_ui_layout_workload playground_ui_host_workload
./build/release/bin/playground_ui_layout_workload --verify
./build/release/bin/playground_ui_layout_workload --verify --gpu
./build/release/bin/playground_ui_layout_workload --verify --full-layout
./build/release/bin/playground_ui_host_workload software
./build/release/bin/playground_ui_host_workload gpu
./build/release/bin/playground_ui_host_workload software --burst
./build/release/bin/playground_ui_host_workload gpu --burst
```

The layout workload uses actual Settings and Demo 2D views at 408×480 and
900×480, three fresh trees each. It reports first-layout time separately from
12 warmed scroll updates, actual measurements, text layouts and arrangements.
Assets remain cached within a process; first-layout time is not a cold-disk or
whole-startup measurement. --full-layout invalidates every node before each
update. --verify checks pixels against full layout, provisional-size round trips,
zero offset-only measurement and return to idle. --gpu runs the same comparisons
within Vulkan with a 1/1024 channel tolerance for RGBA16F rounding; software
pixels compare exactly. It does not compare software gamma arithmetic with GPU
arithmetic. Missing Vulkan support skips the GPU check.
CTest runs correctness checks, without timing thresholds.

The native workload uses isolated preferences, a non-resizable 408×480 window and 80 synthetic
scrollbar moves eight milliseconds apart; --burst queues them without spacing.
It waits for the final input to be painted and rejects changed window geometry
or a non-overflowing fixture. It reports enqueue-to-owner-dispatch and enqueue-to-UI-paint latency, submitted
frame counts, CPU poll durations and menu idle frames. These are not scanout or
input-to-photon measurements. Native tests require a desktop; software and GPU
runs must be sequential. Keep the machine idle, alternate baseline/candidate
order and retain raw output, source revisions, build type, OS, CPU and backend.
Do not compare timings from different geometry, fonts or build configurations.

These workloads do not automate OS title-bar dragging, IME, physical input,
screen readers, or all app transitions. Existing frame admission/activity tests
cover deferred/skipped submission and paint-time invalidation independently.

The recorded baseline/candidate comparison is in
[RENDERING_MEASUREMENTS.md](RENDERING_MEASUREMENTS.md).
