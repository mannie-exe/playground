# UI scrolling measurements — 2026-10-01

The retained UI's scroll invalidation and provisional measurement caused the
reproduced stalls. These measurements support local arrangement plus bounded
host event dispatch; they do not establish a native live-resize fix.

## Method

Apple M4 Pro, 48 GiB RAM, macOS 26.4 (25E246), arm64, Apple Clang 21.0.0,
CMake 4.4.3, Release. Baseline production sources are commit b97efb9. All variants
use the same font assets and maintained [workloads](TESTING.md#rendering-workloads)
from this change. No competing builds/tests ran during measurement.

| Variant | Production change from baseline |
|---|---|
| Baseline | Existing scroll measurement, ancestor invalidation and unbounded event drain |
| Bounded | Host drain limited to 64 events/four milliseconds |
| Pure | Measurement leaves committed scroll geometry unchanged; gutter resolution happens during arrangement |
| Local | Pure plus local arrangement, position-only child-layout reuse and cached scroll geometry |
| Combined | Local plus bounded dispatch |

Each layout case has three fresh trees and 12 warmed updates. Native Settings
runs use a non-resizable 408×480 window, 80 moves eight milliseconds apart,
three sequential runs per variant/backend, alternating variant order. The
harness rejects changed geometry or missing overflow. All accepted runs retained
408×480, painted the final move and produced zero menu frames in the final
one-second idle interval. A separate burst queues all 80 moves without spacing.

## Results

Median warm layout milliseconds per update, at 408×480. Measurement counts cover
12 updates; these count actual node measurements, not unique nodes or allocations.

| Variant | Settings ms | Demo 2D ms | Settings measurements | Demo 2D measurements |
|---|---:|---:|---:|---:|
| Baseline | 17.889 | 2.021 | 275,076 | 26,904 |
| Bounded | 18.161 | 2.001 | 275,076 | 26,904 |
| Pure | 1.798 | 1.195 | 26,364 | 17,160 |
| Local | 0.0205 | 0.0084 | 0 | 0 |
| Combined | 0.0214 | 0.0081 | 0 | 0 |

At 900×480, combined layout measured 0.0205 ms for Settings and 0.0085 ms for
Demo 2D, versus 11.052 ms and 1.617 ms at baseline. Combined warmed scrolling
also performed zero text layouts at both sizes. First-layout times remain
separate in harness output; assets are cached within each process.

Native enqueue-to-UI-paint p95 milliseconds: median of three run percentiles.
Paint samples count the latest dispatched move observed by each paint; discarded
intermediate visual states are not counted as painted inputs. All 80 events were
dispatched. These numbers measure CPU UI recording, not GPU completion or scanout.

| Variant | Software p95 ms | GPU p95 ms | Moves observed by paint, out of 80 |
|---|---:|---:|---:|
| Baseline | 813.841 | 800.034 | 6–7 |
| Bounded | 997.898 | 943.237 | 80 |
| Pure | 5.017 | 4.861 | 80 |
| Local | 1.397 | 0.829 | 80 |
| Combined | 1.393 | 0.918 | 80 |

Combined enqueue-to-dispatch p95 was 0.254 ms software and 0.271 ms GPU.
Worst observed enqueue-to-paint was 5.786 ms software and 4.894 ms GPU.

Bounding alone increases backlog while each event still performs expensive
layout. Under the unspaced burst, Local painted once after 32.8/28.8 ms
(software/GPU); Combined painted eight intermediate/final states, completing in
51.6/60.6 ms. This is the fairness tradeoff: extra presentation work improves
progress during a queue flood but does not minimize total drain time. Baseline
painted once after approximately 1.48 seconds.

## Correctness and limits

Both backends compare retained output against forced full layout, including
provisional measurement round trips. Software pixels match exactly; GPU channels
use a 1/1024 tolerance for RGBA16F rounding. A deliberately changed scroll offset
must fail the pixel-equivalence check. Baseline provisional measurement changed
live offsets; Pure, Local and Combined preserve them. Constituent tests cover
nested residual wheel input, hit geometry, queued local arrangements, cached
ancestor pixels, content shrink, resize and deferred frame acknowledgement.

Raw accepted runs, variant patches and source hashes are retained locally under
build/review/ui-ab/controlled-v3. Earlier exploratory runs are not included in
these results. Workload commands are in [TESTING.md](TESTING.md#rendering-workloads).

No Windows, Linux or AMD64 host was measured. Native OS title-bar dragging,
physical input, input-to-photon latency, allocator-call counts and complete
menu/app transition latency are not covered. SDL's blocking native resize loop
still needs a separately verified callback integration; see
[activity contracts](../platform/ACTIVITY.md#input-fairness).

## Validation

```sh
cmake --build --preset debug --target playground playground_tests playground_docs_check playground_ui_host_workload
MVK_CONFIG_LOG_LEVEL=2 ctest --preset debug --output-on-failure
SDL_VIDEODRIVER=playground-no-such-driver ctest --preset debug -R '^host_smoke$'
SDL_VIDEODRIVER=dummy ctest --preset debug -R '^host_smoke$'
cmake --install build/debug
```

The complete suite passed 107/107. After the Greptile headless-startup correction,
the native host check passed normally and both unavailable/dummy driver checks
returned CTest's configured skip result. Changed C++ files passed clang-format
and clangd checks using CMake's generated compilation database.
