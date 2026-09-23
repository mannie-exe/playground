# Testing retained UI and its constituents

This document maps UI contracts to focused module tests and states the limits
of that coverage. [REFERENCE.md](REFERENCE.md) defines behavior; [GUIDE.md](GUIDE.md) teaches
usage. [CONTRIBUTING.md](../../CONTRIBUTING.md#testing) defines commands and test-authoring
rules. [tests/CMakeLists.txt](../../tests/CMakeLists.txt) owns test registration.
Author obligations are summarized in [CONTRACTS.md](CONTRACTS.md); painter,
device and scene boundaries are documented in [2D](../render/2D.md),
[GPU](../render/GPU.md) and [3D](../render/3D.md).

## Scope

Tests cover the UI module and its constituents: geometry, layout, retained
ownership, events, scheduling, SDL adapters, painting and resource support.
Complete programs, AppHost smoke tests and game rules are outside this scope.
Resource tests may read checked-in assets without treating their owning game
as a fixture.

CTest runs independent executables; assertions use runtime checks that survive
NDEBUG or compile-time static_assert. Build the selected targets before running
CTest. A successful application build verifies compilation of consumers, not
their runtime behavior.

## Contract inventory

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
| Button | `ui_button_states`, `ui_containers`: primary/secondary click, wrong release, drag out/back, cancel, disable mid-press, Space/Enter/repeat | No platform IME/editor controls, not every multitouch interleaving |
| ScrollView | `ui_scroll_variants`, `ui_containers`: all 3 axes × 3 scrollbar policies, offset clamping/invalid input, removal, overlay drag | No complete dedicated matrix of nested residual wheel propagation and scrollIntoView alignments |
| AdaptiveStack/Repeat | `ui_collections`, `ui_collection_failures`: mode switches, all three Repeat layouts, stable keyed reorder, duplicate keys, null factory, retry, empty model, refreshed model-backed child measurements | Not all partially throwing update/onAttach permutations |
| VirtualList/VirtualGrid | `ui_collections`, `ui_collection_failures`, `ui_virtual_list_scale`: fixed/estimated, horizontal/vertical, RTL, indexed scroll, focus pinning/removal, million-item bounded realization; update-then-erase | Not all variable-height anchor changes under arbitrary model edits or captured-pointer pinning combinations |
| Extent index | `ui_extent_index`: 100 updates, every prefix versus linear sum, boundary/search sweep versus linear lookup, zeros, invalid reset/update preservation | Extreme double-sum overflow/cancellation not exhaustively exercised; production virtual-list boundary also validates representable totals |
| Content fit | `ui_content_fit`: all 5 fit × 3 horizontal × 3 vertical alignment × 2 directions × 2 target shapes = 180 cases; empty/invalid modes | No enormous/subnormal input-domain proof |
| Text | `ui_content_resources`: all 4 methods × 2 wraps × 2 fits × 3 paragraph alignments × 2 directions = 96 cases, actual SDL pixels, empty text, required-font Reset | LBRITE + short Latin text is not Unicode shaping/fallback/script conformance testing |
| Image/Vector | `ui_content_resources`: natural density/crop validation, required-handle Reset, final-size/DPI raster requests, failed loads, budgets, density-only failure/readiness, recovery, zero area | Element styles have separate tests; limited asset corpus, not all decoder failures |
| Clip/Layer/Painter | `ui_clip`, `ui_content`, `ui_layer_cache`: content clip/hits, singular-hit diagnostic, opacity pixels, axis-aligned and affine pixel checks, cache invalidation, root/per-layer budget refusal/release | No injected SDL allocation failure at every stage |
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
| 3D math | `geometry3d`: matrix identity/composition, point versus direction, camera basis, projection near/far/aspect, invalid input, overflow and index bounds | Math foundation; not a 3D renderer or exhaustive numerical proof |
| Scene submission | `ui_scene_contracts`: valid/empty submissions, malformed triangles/indices, nonfinite geometry/model, invalid target and null mesh | No GPU scene execution |
| Image realization | `ui_image_preparation`: packed pitch/alpha, byte counts, invalid device ownership, generic identity/malformed outputs, Image/Text/Vector preparation wiring, failure/readiness/retry, retained draw ownership | GPU upload and atlas code compile; no device is created by these tests, no hardware execution claim |

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
