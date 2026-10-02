# Scene verification workloads

## Deterministic contracts

Camera path sampling depends only on explicit time; wrap seams, finite values,
priority ties, source removal and interrupted transitions are tested without SDL.
Scene-view property equality preserves prepared output. Camera/light changes
invalidate output, not layout; layout properties invalidate measurement.

GPU resource tests render repeated frames with working sets above retention
budgets. Warm camera motion must produce zero unchanged mesh/texture uploads.
Shared resources deduplicate across objects and active views. Expired views,
policy pressure, failure and device replacement preserve ownership/accounting.
Impossible plans fail before native upload and cannot trigger an eviction loop.
Opaque/blended variants retain alpha/color correctness through cache reuse.

## Native workflows

Exercise menu -> material/scene -> mouse lock -> movement -> unlock -> settings
-> menu, including focus loss and window resizing. Relative input is viewport
scoped and never survives app/window teardown. Run timed and infinite Bistro
benchmarks using the same app state machine and path used by the launcher.
Finite runs complete only after measurement and bounded completion collection;
cancellation and invalidation report an explicit outcome.

## Measurements

Compare Debug and Release with identical content, camera samples, pixel extent,
renderer, graphics settings and budgets. Separate cold loading, warm-up, steady
traversal and first frame after idle. Report median/p95/p99/max CPU phases,
completed GPU intervals, input dispatch delay, resource uploads and managed peaks.
Compare baseline, retention enlarged above the working set, and corrected active
ownership to distinguish faster computation from eliminated repeated work.

Small deterministic regressions run through CTest. Real-asset sustained runs are
explicit developer workloads with bounded duration and machine-readable output;
wall-time results are not portable pass/fail assertions. Generated reports,
profiles and screenshots remain outside version control.

## Commands

Build deterministic checks and explicit native workloads:

```sh
cmake --preset debug
cmake --build --preset debug --parallel 6
ctest --preset debug
```

A target is the executable or group that CMake should build. The normal build
includes the following workload executable; to build only it:

```sh
cmake --build --preset debug --target playground_scene_host_workload --parallel 6
./build/debug/bin/playground_scene_host_workload material 5
./build/debug/bin/playground_scene_host_workload bistro 5
./build/debug/bin/playground_scene_host_workload chess 5
./build/debug/bin/playground_scene_host_workload camera
./build/debug/bin/playground_scene_host_workload benchmark 5
./build/debug/bin/playground_scene_host_workload benchmark 15
./build/debug/bin/playground_scene_host_workload benchmark 0
./build/debug/bin/playground_scene_host_workload infinite
./build/debug/bin/playground_scene_host_workload interrupted
```

Material/Bistro/Chess workloads allow 30 seconds for preparation before measuring;
measurement starts after scene upload and requires rendered samples. Scene
workloads inject held look intent to exercise camera redraws. Camera mode
checks native lock/unlock, relative motion, held movement across Settings,
focus cancellation and app exit. Early exit cannot pass unfinished checks. Keep
that window focused; focus enforcement is part of the contract.
Benchmark mode uses the launcher's full-loop warm-up and emits `BenchmarkJSON`.
Zero duration runs until quit. `infinite` verifies two periodic reports and then
automatically cancels; interruption reasons distinguish cancellation, focus,
Settings, window, graphics, device and quality changes. `interrupted` verifies
that focus loss during loading remains invalid after the scene arrives. Only an
explicit restart clears interruption. Interruptions produce invalid results,
not a score.
Each invocation uses temporary settings and requests a nonresizable 960×720
window; incompatible window-manager placement fails explicitly. Drawable pixels
still depend on display density and remain part of result identity. User settings
are untouched.

Replace `debug` with `release` in configure/build commands and executable paths
for optimized measurements. Do not run competing builds or GPU tests during a
measurement. Vulkan/MoltenVK and a native desktop are required. Hardware unit
checks skip with code 77 only when the Vulkan backend is unavailable; explicit
workload launch failures are errors. Outputs may be redirected under ignored
`build/`; no generated measurement files are source artifacts.

Percentiles use at most the latest 65,536 phase samples; counts, totals and maxima
cover the whole run. Managed memory peaks cover the host process since startup,
not physical residency or per-scene allocation deltas. CPU iterations without a
render retain absent render/present phases. GPU intervals are admitted by their
originating frame IDs, not completion arrival time.
