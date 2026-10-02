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
