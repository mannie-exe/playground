# Implementation ledger

This ledger coordinates implementation; `docs/` remains the intended design.
Update this file throughout each checkpoint with decisions, exact tested revision,
commands/results, review IDs, blockers and the next action. Keep run logs, captures
and temporary probes under ignored `build/runs/<checkpoint>/`, not project scripts.
Promote a tool only when it has a repeatable supported workflow and validation.

## Active checkpoint

**CP00: repository and build foundation.** Branch `dev`, PR
[#4](https://github.com/mannie-exe/playground/pull/4), base `main`. Starting revision
`6be253929479967d0ddedd7883cceaa8db1b4917`; preceding docs and volume fixture/tool
changes were local and are included in this checkpoint. No application feature
implementation or architecture-wide host rewrite is hidden in this cleanup.

Scope:

- Preserve the reviewed four-target design and acquired volume source/license locks.
- Archive obsolete runs/probes outside the checkout; retain dependency/source caches,
  downloaded source assets and the generated volume suite. Keep maintained asset tools.
- Introduce the reusable `playground_lib` aggregate without demo registration.
- Rename the desktop executable to `playground_demo`, give it an app-local entrypoint
  and identify legacy demo/host support separately until CP02 isolates AppHost.
- Reconcile CMake helpers, test linkage, editor commands, install/packaging and the
  contributor commands with the rename. Do not create nonfunctional voxel/Minecraft targets.
- Validate before/after, commit/push and update PR #4, then discuss checkpoint order.

### Evidence and current action

- Existing PR #4 is open on `dev`; `origin/dev` and the checkout initially agree.
- The starting CMake graph already had separate runtime/world/storage/scene/UI modules;
  `playground_app` bundled AppHost with concrete demos. Native dependency
  fetching is unconditional and is addressed in CP01, not claimed solved by an alias.
- Baseline build: `cmake --build --preset debug --target playground_tests --parallel 4`.
  First attempt failed because an SDL PCH was built with Apple Clang 2100.1.1.101,
  while the installed compiler is 2100.3.34.2. Removed the one generated stale PCH
  and reran successfully. Release had the same stale PCH and passed after regenerating
  its one generated PCH. No test expectation changed.
- Cleanup archived old review/validation/report directories, standalone probes and
  historical asset-authoring logs/notes under the local cache directory
  `~/Library/Caches/playground/cleanup-2026-10-13/`, with a move manifest. Source
  downloads, build caches, volume fixtures and unclassified authored images remain.
- Greptile CLI 3.6.2 is available and authenticated. A completed review is not
  automatically green: inspect findings and the reviewed commit before advancing.
- Implemented: `playground_lib` is an INTERFACE aggregate of portable modules;
  `playground_demo_support` owns legacy host/demo code; `apps/demo/main.cpp` builds
  `playground_demo`. Native SDL linkage remains explicit. Target helpers moved to
  `cmake/Targets.cmake`; macOS install repair resolves all registered executables.
- Validation on the CP00 working tree (before commit, based on `6be2539`):
  - Debug baseline and post-refactor `ctest --preset debug -LE hardware --parallel 4`:
    119/119 passed on each run. Logs: `build/runs/cp00/{baseline-tests,tests}.log`.
  - `cmake --preset debug` and build of `playground_demo playground_tests
    playground_docs_check` passed. Rebuilt/reran `world_state` after linking it through
    playground_lib; otool confirms no SDL dependency in that consumer.
  - `cmake --preset release` and build of `playground_demo playground_host_smoke_tests
    playground_world_state_tests playground_docs_check` passed. Focused `host_smoke` and `world_state` tests: 2/2 passed.
  - `python3 -m unittest discover -s tools/assets -p 'test_*.py' -v`: 7/7 passed.
    `python3 tools/assets/voxel_suite.py prepare` reverified all 1,600 existing bricks,
    hashes and spatial headers without changing source data.
  - Fresh Debug install relocated to a directory containing spaces: signature and
    library paths verified. Renamed entrypoint reached expected invalid-SDL-driver
    failure before creating preferences. This is loader/entrypoint evidence, not
    interactive UI acceptance; `host_smoke` supplies isolated host execution.
  - Editor database includes the moved entrypoint and explicit headers. Formatter
    dry-run, changed JSON parsing, 619 local doc links and whitespace checks passed.
  - Not run: GPU hardware suites, Windows/Linux builds or future native providers.
- Published `bbe9ee2116a6b87694cf9a8375aa7c742339bda2` to `origin/dev` and updated
  PR #4 to describe contracts plus the initial build split. Greptile CLI review
  `00a88ac9-464d-4572-a692-598fbf320ca0` compares that HEAD with main at
  `dc6846a47001b1e2db1423b1ec9aaf35bde7086f`. It completed with four P2 findings:
  Windows temporary-file cleanup, unchecked fixture interpretation, stale baseline
  character-motor references and ambiguous grid origin. All four are addressed in
  the follow-up at `6055eac`.
- Review regression evidence: added failure cases before fixing the tool; the nine
  test methods produced four failed and four errored subtests on the old code.
  After fixes, all nine methods pass, including Windows-style open-file deletion
  rules, checksum/size/timeout failures and malformed metadata. This models Windows
  sharing semantics on macOS; native Windows execution remains untested.
  Reverification of both source fixtures still reconstructs all 1,600 bricks.
- GridMapping now explicitly uses cell-corner-zero origin, with separate Vertex and
  CellCenter formulas and a signed/anisotropic NRRD conversion example. Character
  motors remain future across locomotion, navigation and camera references.
- Follow-up validation: nine Python tests, 620 local doc links and whitespace checks
  pass. C++/CMake are unchanged from the Debug/Release-tested initial commit.
- Refined proposal dependencies: shared products and the medical application are
  separate checkpoints; simulation does not require Jolt; module-specific composition
  attachments land with their providers rather than creating a dependency cycle.
- Published first review fixes at `6055eac9f7e41b5552d0097eb88dbd7d102ea5fb`.
  Review `fdb53e65-aef0-43fc-8a6c-342d600b7455` confirmed all four fixes and found
  a P1 mismatch: CTChest's supported 8-wide profile exceeds the old 32 MiB reader
  cap. A synthetic fixture with CTChest dimensions reproduced `manifest too large`
  after successful packing of 73,728 bricks; no external download was needed.
- The follow-up shares a 64 MiB encoded manifest cap between writer and reader,
  checks it before fixture publication and writes UTF-8/LF bytes consistently.
  Regression tests cover that complete profile and refusal without partial output.
  All 11 Python tests pass, including the complete 73,728-brick profile and writer/reader
  refusal checks. Local documentation links (620) and whitespace checks pass.
  Published as `d31b2dc63bc56f3eb76321a15e7b54c63bdef35a`. C++/CMake remain
  unchanged from `bbe9ee2`. Exact-HEAD review receipts are maintained in PR #4
  and ignored `build/runs/cp00/`, avoiding source commits solely for review status.
- Review `b8e4e883-2f65-4e97-9b87-79853fae80c0` of `d31b2dc` confirms the five
  prior fixes and identifies Windows newline translation in brick headers (P2).
  A regression using Windows text defaults reproduces twelve CRLF header failures.
  Header publication now writes UTF-8 bytes, matching the manifest's LF contract.
  All 12 fixture tests pass; native Windows execution remains a separate check.
- Review `ce4510c1-a75d-4345-9194-d48c4aa8fe1e` of `ab4bc15` confirms the six
  prior fixes and identifies detached-header payload resolution (P2). Moving only
  a header reproduced a false successful verification. Verification now requires
  its relative data path to resolve to the checked payload; moving both together
  remains valid. The regression covers both cases. All 13 fixture tests pass, and
  `prepare` reverified all 1,600 existing MRHead/CTChest bricks and source hashes.
  C++/CMake are unchanged; the earlier native validation remains applicable.
- User selected Jolt → volumes → networking/Minecraft after the three foundational
  checkpoints. Planning drafts #5–#20 are open in that order; links are below.
  The user subsequently requested **merge PR #4 once green, then stop**. Complete
  only the review fixes and current-HEAD CLI/GitHub gates needed for that merge;
  no further CP00 scope or later checkpoint development is authorized for now.
  The later drafts stay queued; no later implementation has started.

## Implementation checkpoints

The user selected the feature order below; IDs remain stable identifiers rather
than execution order. Draft PRs make each bounded scope reviewable before its
implementation. Dependencies are explicit. Each includes the relevant ideal
state docs, source refactor/wiring, failure tests and contributor/API documentation
updates. Reuse existing implementations after verifying their contracts rather than
rebuilding them from scratch. Explicitly future topics are excluded.

| ID | Draft PR | Complete PR scope | Dependencies | Observable completion |
|---|---|---|---|---|
| CP01 | [#5](https://github.com/mannie-exe/playground/pull/5) | Optional build/dependency graph and neutral library consumers | CP00 | Headless configuration avoids SDL/fonts/GPU fetch/link/init; selected modules and demo build independently; disabled providers absent from packaging |
| CP02 | [#6](https://github.com/mannie-exe/playground/pull/6) | Injected AppHost bootstrap, registry and instance/presentation lifetimes | CP01 | [#5](https://github.com/mannie-exe/playground/pull/5) | Mono-app and launcher use supplied factories; replacement rollback, suspension, view rebuild and late completions tested; no demo names in reusable host |
| CP03 | [#7](https://github.com/mannie-exe/playground/pull/7) | Asset/storage and registered saved composition | CP02 | [#6](https://github.com/mannie-exe/playground/pull/6) | Typed content locks, bounded preparation, render/camera/compiled-behavior attachments, restore/migration/error paths and durable checkpoints; module-specific attachments land with their providers |
| CP04 | [#9](https://github.com/mannie-exe/playground/pull/9) | Sampled-data storage, import, edits and bounded discovery | CP03 | [#7](https://github.com/mannie-exe/playground/pull/7) | VOX/NRRD fixtures, exact scalar/spatial metadata, revisioned chunks, finite/paged sources, edit/save/reopen and overload/cancellation tests |
| CP05a | [#10](https://github.com/mannie-exe/playground/pull/10) | Shared spatial products and streaming | CP04 | [#9](https://github.com/mannie-exe/playground/pull/9) | Bounded source/product demand, exposed-face and scalar-slice preparation, coherent revisions, cache churn/stale work and device retirement; no viewer UI dependency |
| CP05b | [#11](https://github.com/mannie-exe/playground/pull/11) | `playground_voxels` medical viewer | CP03, CP05a | Linked orthogonal views, physical crosshair, window/level and label edits; native samples match fixtures; save/reopen and bounded traversal verified |
| CP06 | [#8](https://github.com/mannie-exe/playground/pull/8) | Optional Jolt adapter and physical-object fixtures | CP02, CP03 | Pinned dependency, scoped bodies/queries, static geometry and convex dynamics, real contacts plus failure/retirement evidence; physics-disabled build works |
| CP07 | [#12](https://github.com/mannie-exe/playground/pull/12) | Physics/streaming/edit publication wiring | CP05a, CP06 | Exact collision readiness, shape replacement between steps, origin/coverage handling and pre-separated fragment activation under bounded traversal |
| CP08 | [#13](https://github.com/mannie-exe/playground/pull/13) | Small regional simulation service | CP04, CP05a | Deterministic propagation/diffusion fixture, coherent region steps, halo coverage, sleep/wake and expanding demand with finite budgets; persisted continuation |
| CP09 | [#14](https://github.com/mannie-exe/playground/pull/14) | TCP/UDP and HTTPS providers | CP02 | [#6](https://github.com/mannie-exe/playground/pull/6) | Asio/libcurl selection, bounded DNS/I/O/results, partial writes, datagram truncation, TLS/redirect rules, shutdown and native loopback tests |
| CP10 | [#18](https://github.com/mannie-exe/playground/pull/18) | Message transport and generic authority sessions | CP09 | [#14](https://github.com/mannie-exe/playground/pull/14) | GNS adapter, nonspatial document/presence and sequenced-value fixtures; reconnect, deduplication, filtered baselines and pressure tests |
| CP11 | [#19](https://github.com/mannie-exe/playground/pull/19) | Optional FMOD Core/Studio integration | CP02, CP03 | Supplied SDK version validation, clip/bank/event lifetime, spatial mapping, pause/device recovery; real native acceptance or explicit SDK blocker |
| CP12 | [#15](https://github.com/mannie-exe/playground/pull/15) | Minecraft 26.3 compatibility profile and offline pipeline | CP05a, CP09 | Complete pinned codecs/registries, safe fixture replay, local model/texture interpretation and chunk revision/product path; malformed-data/limit tests |
| CP13 | [#16](https://github.com/mannie-exe/playground/pull/16) | `playground_minecraft` online spectator and movement | CP12 | [#15](https://github.com/mannie-exe/playground/pull/15) | Real login/configuration, mouse look/WASD/vertical movement, corrections/acks, chunk feedback/unloads/dimensions and bounded fast traversal on controlled server |
| CP14 | [#17](https://github.com/mannie-exe/playground/pull/17) | Minecraft visible-content coverage | CP13 | [#16](https://github.com/mannie-exe/playground/pull/16) | Declared block/model/material/entity/block-entity/sky/weather coverage, resource compatibility and comparative captures; no claim of full rendering from terrain alone |
| CP15 | [#20](https://github.com/mannie-exe/playground/pull/20) | Distribution and cross-module acceptance | CP05b, CP07, CP08, CP10, CP11, CP14 | Four target packaging, combined contention/pause/shutdown, isolated roots, enabled/disabled matrices and final source-to-contract reconciliation |

Selected execution order: **CP01 → CP02 → CP03 → CP06 → CP04 → CP05a → CP05b →
CP07 → CP08 → CP09 → CP12 → CP13 → CP14 → CP10 → CP11 → CP15**. This puts the
requested Jolt, volume and Minecraft milestones first within their prerequisites.
CP05b does not block Minecraft, and CP08 does not require Jolt. Dataset and
physical-body composition attachments land in CP04 and CP06 respectively; audio
attachments land with CP11. CP10 is not required to implement Minecraft's wire protocol. The Minecraft
renderer may use app-owned rigid models and existing renderer primitives; general
skinning/shadows/editor frameworks stay excluded where marked future. If a required
visual class needs one, document the concrete conflict and discuss scope before
promoting a future engine feature into these checkpoints.

Known external evidence: exact 26.3 reference packet schemas are incomplete in the
inspected reference dataset; local class signatures are not wire conformance. The
FMOD checkpoint requires a compatible supplied SDK. Native GPU/OS/SDK acceptance
must be recorded separately from deterministic headless tests.

## PR and review workflow

Create planning-only draft PRs with their dependencies for the selected order. GitHub requires a branch difference: a pending checkpoint may carry
its own ledger scope/acceptance entry as a planning-only commit, clearly labelled
as unimplemented. Keep implementation sequential, update later bases after merges,
and never treat an empty planning PR as delivered code.

For each accepted checkpoint:

1. Branch from synchronized main (or explicitly documented draft dependency), and
   implement its complete scope with local validation. Record any real blocker.
2. Commit and push, then run `greptile review --branch main --json` against the correct
   base. Save output under `build/runs/<checkpoint>/`; record review ID and HEAD here.
3. Inspect findings, repair actionable issues, rerun affected checks and commit/push.
   Review the new HEAD again. A CLI exit code of zero means review completed, not
   that findings are resolved. Do not suppress issues to obtain a green result.
4. When local review is green, mark the draft ready for review. Wait for the
   automatically triggered GitHub review on that HEAD and required checks. Repeat
   the repair/validate/commit/push cycle until both are green.
5. Do a final diff and scope review. Rebase/merge as supported by repository policy,
   delete the checkpoint's remote/local branch, sync main and start the next scope.
   Never delete unrelated branches/worktrees or count a review of an older SHA.

Blocked review/auth/service runs remain blockers, not approval. Record evidence and
continue independent authorized work; do not merge without the agreed review gate.
The initial cleanup updates existing PR #4. Planning drafts may initially target
`dev` while CP00 is open; retarget/rebase them onto synchronized `main` after its
merge. A draft’s planning commit supplies reviewable scope, not completed work.
Do not advance implementation while a preceding checkpoint is held at its review gate.

## Queued draft scope: CP05b

**Planning only; implementation has not started.** This branch initially targets
`dev` because CP00 is still under review. Before implementation, synchronize with
main and completed prerequisites, reconcile this ledger and update the PR base.

- Scope: `playground_voxels` medical viewer.
- Dependencies: CP03, CP05a.
- Acceptance: Linked orthogonal views, physical crosshair, window/level and label edits; native samples match fixtures; save/reopen and bounded traversal verified.
- Owning contracts: `docs/apps/VOLUMES.md`, `docs/render/SPATIAL.md`, `docs/platform/SPATIAL_TESTING.md`.
- Include necessary source refactoring, wiring, failure tests and documentation
  reconciliation for this scope. Reuse verified existing implementations.
- Exclude explicitly future contracts and unrelated application features.
- Record actual revisions, commands, outcomes and provider/OS acceptance limits.
- Follow the CLI and GitHub review gates above before merging.
