# Contributing / working on playground

This is a learning project. Prefer a small, understandable change with a focused
check over a new framework that obscures the thing being studied.

## Build and source boundaries

Use C++23 and the checked-in CMake presets. CMake 4.4 or later and Ninja are
currently required by the project; on Windows use the existing developer-shell
environment so MSVC and the Windows SDK can be discovered.

```powershell
cmake --preset debug
cmake --build --preset debug
cmake --install build/debug
```

Install assembles `dist/debug` with runtime dependencies and assets. It does not
install development tests. `release` is a separate configure/build preset.
Do not check in build products, compile_commands.json, downloaded dependencies,
or editor-specific absolute paths. Do not change clangd/editor settings just to
silence a source error.

### Headers, implementations, and editor commands

Name complete property values `SomethingProps` and their partial updates
`SomethingPatch`, not `SomethingPropsPatch`. Preserve established acronyms in
type and file names (`UIRoot`, `UIServices`, `SVGDocument`, `SDLResource`), and
in compound function names such as `getSVGDocument`. Namespaces/directories
remain lowercase (`ui`, `sdl`).

Use `Node::settings/setSettings/applySettingsPatch` for common groups, named
base groups (`contentProps`, `buttonProps`, `gridProps`) to avoid inheritance
hiding, and `props/setProps/applyPatch` for component-specific values. Do not
invent mutators for immutable construction configuration. Generic Keep/Set/Reset
operations live in `support/Patch.hpp`, not the UI module.

Layout changes should test invalidation visits, arrangement reuse, A/B/A offers,
mutation during traversal and exception retry. Only opt into multiple measurement
offers when cached results cannot leave constraint-specific child state behind.
Counters are the performance oracle in tests; optional timings are observational.

Keep value/props/patch types, templates, constant-evaluation functions, and small
accessors in headers. Put substantial non-template algorithms, resource work,
and orchestration in matching `src/<area>/<Name>.cpp` files. Public headers must
be self-contained; implementation-only helpers/dependencies belong in `.cpp` or
private `src` headers. Keep default arguments on declarations, not definitions.

Register implementation files in `cmake/Modules.cmake` (or the application target
for app-specific code). Link the module providing an API, not just its external
dependencies: `playground_math`, `playground_runtime`, `playground_assets`,
`playground_rendering`, `playground_scene`, `playground_layout`, `playground_ui_core`, `playground_constraints`,
`playground_ui_resources`, or `playground_sdl`. Use PUBLIC requirements for public
headers, PRIVATE for implementation-only dependencies. Template and constexpr
definitions stay visible where consumers instantiate/evaluate them.

After adding a `.cpp`, configure and build the affected target. Every project
target updates the root `compile_commands.json`, preserving the build database's
source commands and adding explicit commands for project headers. Matching `.cpp`
commands take precedence; headers without one use the application context.
`cmake/GenerateEditorCommands.cmake` discovers new headers on each build; configure
alone does not refresh this editor database. Do not enable a competing raw-database
copy or hard-code dependency include paths in `.clangd`. If the editor retains an
old header command after rebuilding, use **clangd: Restart language server**.

| Area | Responsibility |
|---|---|
| `include/math`, `include/layout` | Own values, geometry, layout rules; no SDL dependencies |
| Core `include/ui` | Retained ownership, props, layout, routing, runtime services |
| `include/platform/sdl` | SDL conversion/input/painting/session boundary |
| `include/platform` | Window presentation/viewport values and typed settings/document-storage contracts; native implementation stays out of UI nodes |
| `include/rendering`, `include/scene` | Backend/frame/resource preparation and separate scene submission contracts; no SDL types |
| `include/ui/content` | Current resource-backed content implementations (some use SDL) |
| `include/app`, app directories | Host orchestration and app-specific composition/model |

See [UI reference](docs/ui/REFERENCE.md) for behavioral contracts and
[MinesweeperUI.hpp](include/minesweeper/MinesweeperUI.hpp) for a real composition.
UI docs live in `docs/ui`: REFERENCE is the catalog, GUIDE teaches composition,
CONTRACTS defines author/runtime obligations, and TESTING records coverage.
Renderer contracts live separately in `docs/render/{GPU,2D,3D}.md`; distinguish
implemented APIs from requirements for a future backend. Keep UI contracts
backend-agnostic and place native adapter wiring in the rendering documents.
`docs/render/CONTRACTS.md` defines capability negotiation, coordinate/color rules
and the required implementation sequence. `SHADERS.md` covers optional offline
tools. Software-only builds must not fetch a shader compiler. Adding a shader
requires explicit input dependencies, stage, resource bindings and an install rule;
report compilation and device execution separately from C++ build success.
Shared image contracts belong to `rendering/PaintImage.hpp`, not UI. Changes to
pixels must preserve or explicitly convert color encoding and alpha association.
Never relabel legacy encoded-space blending as linear-light composition.
Window policy, settings precedence/schema and filesystem guarantees live in
`docs/platform/{WINDOWING,SETTINGS}.md`. Settings parsers are private dependencies
(currently toml++); do not leak parser nodes into app/UI public interfaces. Keep
requested preferences distinct from observed OS state. Changes to persistent fields
must specify defaults/units, validation, versioning and when they take effect.
Keep window metadata, sizing policy, and presentation requests separate. Content-sized
apps should expose their root's measurement, not duplicate layout arithmetic in
window config. Use WindowMetrics at the session boundary and project color values
outside native adapters; do not reintroduce compatibility-only size/color wrappers.
The application includes SDL_main.h only in src/main.cpp for SDL's platform
entry-point setup. SDL.h does not include it implicitly; do not put this
implementation-bearing header in shared public headers.
Preserve explicit deferred requirements rather than
quietly presenting them as implemented or deleting them from the design.

## Testing

Asset definitions live in `include/assets`; native acquisition stays in
`platform/sdl`. C++ registration freezes before consumers or workers read it.
See [asset ownership](docs/platform/ASSETS.md). App view constructors receive
model/resources explicitly; rebuilding nodes must not reset application state.
Do not add document apps, disk watchers or executable discovery implicitly.

The `assets` and `runtime` test groups cover catalog/decoder/reconstruction and
bounded execution. Their aggregates are `playground_assets_tests` and
`playground_runtime_tests`. Use deterministic barriers/stop-aware gates, not sleeps.
Test admission refusal, supersession, cancellation, shutdown and failed publication.
These are constituent tests, not game-rule tests.

Runtime input/timing/lifetime contracts are in `docs/platform/RUNTIME.md`.
Ownership changes must test held actions as well as fresh events: editor/modal
claims, neutral reacquisition, per-pointer capture, global overrides and pending
fixed-tick edges. Run `input_actions`, `ui_navigation_routing`, `ui_sdl_input` and
`ui_text_field`; ownership policy must not require another device event to cancel
an active action. Camera boundaries are in `docs/render/CAMERAS.md`; networking
direction (not an implemented transport) is in `docs/platform/NETWORKING.md`.

For idle/wake changes, follow `docs/platform/ACTIVITY.md` and run `runtime_activity`,
`event_wake`, `ui_completions`, `ui_window_services` and `performance_reports`.
Check lost/filtered notifications, stale endpoints, timer cancellation, pending
work during presentation and snapshot invalidation. Test deadlines with supplied
times, not wall-clock performance thresholds. Painter fast paths require opt-in
`gpu_device` pixel comparisons against the general path, including transforms,
fractional clips, borders and opacity layers. Report hardware execution separately.
Keep input and clocks independent of SDL; translation/device ownership belongs in
`platform/sdl`, and controllers manipulate authored scene/model state, not a backend.
Use synthetic events and explicit elapsed times. Cover BeforeUI/AfterUI consumption,
held/released/canceled distinctions, zero-tick frames, bounded catch-up, pause,
stale activation delivery, and failed deferred callbacks. Test interpolation and
time conservation with numeric tolerances, not wall-clock sleeps. Runtime tests
may drive a tiny scene model without becoming full application/game tests.

We use **CTest to discover/run tests**, with small C++ test executables. CTest is
not an assertion library. New focused tests can use
[`tests/support/Test.hpp`](tests/support/Test.hpp): `require` throws on failure and
`run` reports failures with a nonzero exit code. GoogleTest is not a dependency;
introduce it only if fixtures, parameterization or richer reporting justify it.

`PLAYGROUND_BUILD_TESTS` defaults ON; turn it OFF for an application-only build.
Changing this option requires configure, not install.
Automated testing covers UI and its constituents, plus rendering and scene
contracts: math, layout, runtime, SDL adapters and resource support. Do not add app/game
rule tests or executable smoke tests yet. Demo and Minesweeper remain production
consumers, not test fixtures; generic resource tests may reuse checked-in assets.
UI viewport/presentation settings are constituents: test their pure mapping,
schema/merge and failure contracts, not full AppHost startup. Use an in-memory
FileStore or a fresh test-owned directory; never real user preferences. Ordinary
tests must not change desktop displays, fullscreen modes, or OS settings.

Host transition ordering may be tested through the narrow operations in
`app/HostTransitions.hpp`, which AppHost also calls. Use fake apps/backends to
verify ownership, rollback, command suppression and recovery without constructing
AppHost or touching user settings. Native submit failures use GPUCommandAPI's
consuming submit boundary in opt-in GPU tests, never a forced driver reset.

### Commands

```powershell
cmake --preset debug -DPLAYGROUND_BUILD_TESTS=ON
cmake --build --preset debug --target playground_tests

# Fast development pass: omit explicitly labeled stress tests.
ctest --test-dir build/debug -LE stress --output-on-failure

# One feature, or one module. CTest selects tests; it does not build them.
ctest --test-dir build/debug -R '^ui_clip$' --output-on-failure
ctest --test-dir build/debug -L ui -LE stress --output-on-failure

# Inspect discovery, then run the full set including scale checks.
ctest --test-dir build/debug -N
ctest --test-dir build/debug --output-on-failure
```

Build-only aggregate targets also exist: `playground_rendering_tests`, `playground_math_tests`,
`playground_layout_tests` and `playground_ui_tests`.
Use `cmake --build --preset debug --target playground_docs_check` after changing
the UI guide or its APIs. This extracts its additive C++ blocks into the build
tree and compiles them together; it does not add/install an example program or
establish runtime correctness. Keep fragments additive and compile-valid.
Individual test executables use `playground_<test-name>_tests`.
The UI aggregate includes its stress executable; `-LE stress` controls execution.

Release checks should use `cmake --preset release`, build the desired target with
`--preset release`, and run `ctest --test-dir build/release ...`. Do not use plain
`assert` for required test outcomes: it disappears under NDEBUG. Current tests
use throwing runtime checks or compile-time static_assert; use the focused helper
for new tests.

### What belongs in a test

| Kind / label | Scope | Example |
|---|---|---|
| `unit` | One contract, no real window, predictable input | Patch Reset, grid placement, clipping/hits, routed UI action |
| `integration;sdl` | Small real adapter/resource interaction, preferably offscreen | Text preparation, layer cache pixels, SDL input translation |
| `stress` | Explicitly heavier population/scale behavior | Million-item source with bounded realized nodes |

Label by module (`math`, `layout`, `ui`) and useful feature (`input`,
`cache`, `collections`, etc.). A label may belong to more than one test; it is not
a dependency declaration. Timeouts catch hangs, not performance regressions.
Renderer tests use the `rendering` module label and link playground_rendering;
hardware tests must be explicitly opt-in, not silently run as ordinary UI tests.
Use `-DPLAYGROUND_GPU_TESTS=ON` with compiled SPIR-V shaders to enable `gpu_device`
and `gpu_shaders`/`gpu_materials` (Vulkan only). These are renderer-module readback/lifetime
checks, not program smoke tests. Unsupported drivers return skip code 77; a
supported device failing an assertion is a failure, not a skip. Report shader
compilation and each tested driver separately. Software-only builds remain valid.
Do not assert wall-clock speed in ordinary unit tests.

Texture/material changes should run texture_materials, software_materials, animation, material_assets,
model_import and (opt-in Vulkan) gpu_materials. The latter reads back PBR output;
it is a renderer constituent test, not a Demo 3D application smoke test.
Use synthetic clips to test interpolation, identity and stale targets independently
of downloaded assets. Keep asset authors, source URLs, license files and hashes in
assets/demo3d/README.md; tests must not fetch the network. Preserve numerical
texture channels and account for all mip levels. Shader compilation is not proof
of visual correctness.
Compact storage/import changes additionally run texture_storage, mesh_preparation
and ktx_texture. KTX integration tests reuse fixtures from the pinned dependency;
do not download fixtures during test execution. Keep exact vertex reindexing
separate from lossy welding or triangle-order changes. Report payload bytes,
admission estimates and actual allocator/process measurements distinctly.

For a bug fix, first express the failure at the smallest useful boundary. Test
observable results: bounds, routed action, handle validity, dirty/cache behavior,
or a carefully selected pixel. Avoid asserting private field layout or exact
whole-frame screenshots for unrelated features.

Use deterministic data. Do not require the network, an interactive window, sleeps,
a user's home directory, or modifying real assets. Resource tests may read the
checked-in assets using a target-specific source-root definition.
Resource-backed Text/Vector/SDL adapter tests link playground_sdl; direct
Font/AssetRegistry/SVG/text-flow tests can link playground_ui_resources.
ConstraintLayout tests link playground_constraints, which compiles the
Kiwi-backed solver; Kiwi, utf8proc, and pugixml are implementation dependencies.
Geometry/layout values alone do not need those dependencies.
Compiled 3D camera/matrix tests link playground_math; scene-contract tests link
playground_scene. Ordinary UI tests must not require a GPU. GPU resource code is
compiled with playground_sdl; deterministic tests cover CPU upload preparation and
fake backend realization. Real-device uploads, atlas execution and drawing
need separately identified hardware verification, not a passing CPU test claim.
Unicode vertical-orientation data is versioned with its
license in docs/licenses/UNICODE.txt. Release all
resource handles before shutting down their SDL/TTF lifetime guards.

### UI regression checklist

These retain the design's acceptance criteria, not a claim that every fault has
already been exhaustively injected. Choose the relevant rows when changing a feature:

| Boundary | Important cases |
|---|---|
| Values/patches | Invalid numbers/ranges; Keep versus false/zero/Reset/nullopt; half-open bounds |
| Ownership | Stale handles, root destruction, cyclic/incompatible reparenting, deferred self-removal |
| Layout | Nested insets; capped growth/shrink minima; baseline; wrap remeasurement; spans; RTL; empty/oversized content |
| Input | Press-drag-release, unrelated release, disable mid-press, focus/capture cleanup, clipped/transformed hits |
| Cache validity | Same props with changed constraints/density/direction; layout-only layer changes; failed rebuild stays dirty |
| Collections | Duplicate keys; reorder preserves identity; anchor preservation; bounded live nodes; focus/capture pinning |
| Resources/callbacks | Load/allocation failure; throwing notifications; stale asynchronous completion; no dangling ownership |
| Painting | Affine/rounded clip and opacity correctness, singular transforms, cache budget refusal; damage rendering only when implemented |

Property-based testing is useful for finite nonnegative output, repeated-measure
determinism, and space conservation where constraints allow it. Use explicit
numeric tolerances, not approximate hash-map equality. Inspect runtime counters
for measurement/cache hits/arrangement/preparation/realization; broader queue,
raster, byte-budget and PerformanceMonitor integration remains an extension.

### Add a focused test

For example, `tests/ui/example_contract.cpp` can contain:

```cpp
#include <support/Test.hpp>
#include <math/Geometry2D.hpp>

int main() {
  return playground::test::run([] {
    const auto bounds = playground::math::rect(0, 0, 20, 10);
    playground::test::require(bounds.contains({0, 0}), "leading edge included");
    playground::test::require(!bounds.contains({20, 5}), "trailing edge excluded");
  });
}
```

Register it in [tests/CMakeLists.txt](tests/CMakeLists.txt):

```cmake
playground_add_test(example_contract
    SOURCE ui/example_contract.cpp
    MODULE ui
    LABELS unit geometry)
```

The helper sets C++23, include paths, a 60-second default timeout, CTest registration,
and module/all-test build aggregation. UI tests link playground_ui_core and layout
tests link playground_layout automatically. Add `LIBRARIES playground_sdl` for
compiled SDL adapters/content, or `SDL3::SDL3` for direct SDL-only use; add
`TIMEOUT` for genuinely longer tests. Ensure ordinary
test DLLs can be found through the project's build output arrangement, not a new
global PATH workaround.

A passing offscreen module test does not validate a complete program or every
platform's windowing/accessibility behavior. Program-level testing is outside
the current scope; building the application still verifies that its consumers compile.

## Style and change checklist

- Follow adjacent class layout. Separate authored props, runtime state, and derived
  caches with whitespace; order ownership/lifetime dependencies intentionally.
- Prefer explicit typed props/patches and ownership. Do not hide resource ownership
  behind a raw pointer or introduce a second authoritative copy of derived bounds.
- Keep required includes direct. Keep SDL types/conversions at adapter boundaries.
- Group includes as standard library, external dependencies, then local/project
  headers, separated by blank lines. Conditional/platform includes remain within
  their original preprocessor branches. Check header self-containment rather
  than relying on an implementation file's first include to hide missing inputs.
- Keep worker publication bounded and owner-thread application explicit. A shared
  handle preserves lifetime, not thread safety or GPU completion. Resource-domain
  compatibility and completion fences solve different problems.
- Keep backend creation policy in RenderBackendProps and retain it across recovery.
  Allocation refusal is not device loss. New frame failures must use the matching
  RenderOperation category; decoding/validation errors must not request recovery.
- Publish immutable GPU images only after their initializing submission succeeds.
  Writable pooled attachments and their mutable lease bookkeeping remain internal.
- For shader changes, test reflected ABI rejection and failed-reload preservation,
  then run native readback tests. For imports, test malformed/budget-exceeding input
  and atomic scene publication separately from successful parsing.
- Match existing formatting/editor conventions; do not add a competing formatter
  configuration as part of an unrelated change.
- Keep each test focused on one feature or tightly related contract. Existing broad
  tests can be split incrementally; do not add every regression to one giant main.
- Update reference semantics/defaults and compile-checked examples with API changes.
- Build affected targets, run the relevant tests, and use `git diff --check`.
- Report what was actually verified, along with platform or deferred limitations.

### Accessibility and Unicode dependencies

`cmake/Accessibility.cmake` imports the official checksum-pinned AccessKit C
0.23.1 desktop binaries. Native adapters are private to playground_sdl; core UI
headers must not expose AccessKit types. ICU uc/i18n/data are implementation-only
dependencies of playground_ui_core. MSVC x64/ARM64 use pinned ICU 78.3 archives;
other desktop toolchains require an installed ICU >=78 development package.
Windows installation copies the ICU DLLs and both dependencies' licenses.
The prebuilt AccessKit package does not require a local Rust build.

Appearance observation adds AppKit on macOS and `dbus-1` development headers via
pkg-config on Linux. SDL supplies light/dark; native adapters supply contrast.
Core palettes and popup layout remain SDL-independent. Test with injected
SystemAppearance values; do not toggle a user's OS preferences automatically.
Run ui_theme, ui_overlays and ui_control_text_layout for appearance/overlay changes.
Wrap regressions should include fractional density and actual fonts, not just
synthetic text sizes. Popup dismissal must test the complete pointer sequence.

Use `ui_text_edit`, `ui_text_field`, `ui_accessibility`, `ui_control_variants`,
`ui_sdl_input`, `ui_window_services` and `ui_presentation_settings` for focused interaction checks.
Tests use injected clipboard functions; never overwrite the user's clipboard.
Keep passwords out of snapshots and diagnostics. Native callbacks must use owned
snapshots/requests and never mutate Nodes or throw through a C ABI. A native
adapter compiling does not establish Narrator/NVDA, VoiceOver or Orca acceptance;
report that separately. The Demo2D controls gallery is for manual exploration,
not a program smoke test.

### Native rendering dependency maintenance

SDL and SDL_ttf are pinned and receive checked-in configure-time patches under
`cmake/patches`; `cmake/SDLTimestamps.cmake` integrates the Vulkan timestamp
extension. Never fix only the downloaded `_deps` copy. Dependency upgrades must
review patch guards, repeat configuration to check idempotence, and run the
focused `gpu_timing`/`text_pixels` tests plus opt-in native GPU tests. Run
`gpu_timestamp_native` for profiling changes: command-buffer reuse and runtime
profiling toggles must work with GPU debugging both disabled and enabled. The timestamp
table is a versioned project extension, not an upstream SDL API. CPU timing and
GPU execution/completion measurements must remain labeled separately.

For telemetry changes also run `performance_monitor` and `performance_reports`.
Use deterministic duration samples to check aggregation, missing values, size and
domain grouping, collection generations, loss-counter deltas and bounded histories.
Reporting must not wait for GPU results or equate command intervals with GPU frame
time/utilization. Native timing tests cover execution separately from aggregation.

Resource tests must distinguish recording, submitted, completed, canceled and
ambiguous-failure states. Use injected timestamp tables and pure budget/recovery
tests for deterministic failures; do not reset a real driver to test device loss.
For native callbacks, declare sampled pooled images through `GPURecordingContext`
and let the owning frame submit. A failed/unknown completion must not make a
target or query slot reusable. Only native execution verifies timestamp support;
ordinary unit tests do not claim hardware coverage.

### Pre-commit verification

Format authored C++ with the checked-in `.clang-format`; exclude generated tables
and dependency sources. It retains LLVM formatting and groups standard library,
external dependencies, then project headers, with blank lines between groups.
Matching implementation headers stay in the project group. Macro-configured
implementation headers and conditional includes must retain their preprocessor
ordering. Preserve semantic grouping of props, runtime state and derived caches;
formatting cannot infer those groups for you.

The editor and CLI both use `--style=file`; LLVM 22.1.8 is the currently verified
formatter/clangd pair. Use `clang-format --dry-run --Werror --style=file <files>`
to check and `clang-format -i --style=file <files>` to apply. `.clangd` prefers
angle brackets for public project include paths; it does not add search paths.
The generated compilation database remains the authority for those paths.
Review include-cleaner suggestions rather than bulk-removing headers: formatter
specializations, overload providers and SDL_main.h can matter without an obvious
direct symbol use. Experiments remain separate targets and need not be reformatted
as part of an engine-only change.

After rebuilding the compile database, a source/header can be checked with
`clangd --check=path/to/file.hpp --compile-commands-dir=. --tweaks=`. The empty
tweaks list disables refactoring-tweak probes, not parsing or diagnostics. It
avoids unrelated Windows URI errors in those probes. This does not replace a
real compiler build. Treat clang-tidy include-cleaner output as review suggestions,
not permission to remove a dependency or rely on transitive includes blindly.

Before a broad checkpoint, build Debug and Release, run their focused module
tests, compile guide snippets, and inspect `git diff --check` plus untracked files.
Installation verifies packaging separately from execution. Keep GPU hardware,
interactive behavior and non-Windows checks explicitly separate from CPU/offscreen
success. No commit/staging or external publishing is implied by verification.
