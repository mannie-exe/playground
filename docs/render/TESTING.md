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

### Locomotion and camera acceptance

| Area | Required cases |
|---|---|
| Steered | Camera-relative immediate travel versus body-relative curved travel; lateral/backward and diagonal input; preserved analog magnitude |
| Strafe / Tank | Aim-facing lateral/reverse travel; explicit tank turning and reverse movement without camera-driven facing |
| Response | Rate bounds, half-life response across time subdivisions, angular wrap, zero delta and rejected invalid settings |
| Perspective | Threshold hysteresis, fractional wheel input, forced modes, reversal mid-transition, retained look and per-view self-hiding |
| Director | A moving selected source does not restart a blend; moving-destination hand-offs complete; interruption starts from displayed pose |
| Target lifetime | Removal, generation reuse, teleport reset and missing-target fallback |
| Input | Coalesced versus separate mouse events yield the same turn; pointer delta applies once; stick rate is frame-rate independent |
| Ownership | UI scroll does not zoom; engagement does not also activate gameplay; per-channel takeover, drift rejection and neutral rearming |
| Settings | Transactional Apply/Save/Revert, independent inversion, dead-zone validation, requested/effective app restrictions |
| Obstruction hook | Absent provider reports Unavailable and leaves unconstrained placement; no collision-safety claim |

Compare equal-duration input traces across frame and fixed-tick schedules,
including zero-tick and catch-up frames. Verify no stale movement after focus,
Settings, device disconnect, ownership change or app exit. Contextual profile
overrides preserve eligible input while changing its movement/facing policy. Preserve analog input
below full deflection and avoid normalizing it to full speed. Camera motion,
profile switches and view-local visibility must not cause unchanged mesh/texture
uploads or layout remeasurement.

Physics contact, swept-volume correctness and collision-index performance checks
belong to the deferred obstruction/physics implementation, not these acceptance
criteria. No new native command is implied by this matrix.

## Native workflows

Exercise menu -> material/scene -> mouse lock -> movement -> unlock -> settings
-> menu, including focus loss and window resizing. Relative input is viewport
scoped and never survives app/window teardown. Run timed and infinite Bistro
benchmarks using the same app state machine and path used by the launcher.
Finite runs complete only after measurement and bounded completion collection;
cancellation and invalidation report an explicit outcome.

Exercise mouse/keyboard, assigned gamepad and mixed-device navigation through the
same app adapter. SDL virtual gamepads supply repeatable axes, buttons and device
removal; native focus/cursor behavior still needs desktop verification. Test held
stick/capture-button focus loss, explicit re-engagement, controller noise during
mouse use, cinematic takeover/return and switching perspective while turning.
Record source ownership, cancellation reason, requested/effective perspective and
desired/actual headings alongside frame timing; a screenshot alone cannot establish
correct input consumption or hand-off.

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
