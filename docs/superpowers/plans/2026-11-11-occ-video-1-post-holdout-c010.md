# occ_video_1 Post-Holdout C010 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Eliminate C009's visible-period false search without weakening the real occlusion transition, repair exposed evidence contracts, and verify the complete 271-frame behavior on RK3588.

**Architecture:** Add a bounded temporal dual-template bridge to the controller, keep search recovery detector-seeded, and make hint/evaluator contracts accurately represent absent or inapplicable evidence. Run C010 end to end before deciding whether a separate geometry-correction design is required.

**Tech Stack:** C++17, CMake/CTest, Python 3 unittest, RKNN Runtime on RK3588, OpenCV/ffprobe.

**Spec:** `docs/superpowers/specs/2026-11-11-occ-video-1-post-holdout-c010-design.md`

## Global Constraints

- No runtime frame IDs, truth boxes, pixel offsets, or dataset coordinates.
- `rect[0]`/`tracking_bbox` remains the real tracker or predicted box; display smoothing stays separate.
- Detector candidates never directly authorize control; search recovery remains NanoTrack-verified and two-hit.
- Existing RK3566 configurations remain unchanged because the bridge defaults disabled.
- All new non-training evidence uses only canonical `occ_video_1` with verified hashes.
- Update only `docs/progress/board-validation.md` among coordination files.
- Post-holdout evidence is labeled as such and is never called blind validation.

## Review Focus

- A single reliable-recent frame after mature dual-template tracking bridges once; a second consecutive frame enters search.
- Recent-only evidence without a preceding dual-reliable streak must not bridge the real occlusion onset.
- Geometry disagreement or a hard anchor geometry failure must never bridge.
- A pre-min-age locked detector candidate must not fabricate a partial hint.
- Scope-inapplicable gates must not affect verdict, while applicable global reacquisition failures still do.

---

### Task 1: Scope-aware evaluator gates

**Files:**
- Modify: `scripts/evaluate_occ_video_1.py`
- Modify: `tests/test_evaluate_occ_video_1.py`

**Interfaces:**
- Produces: gate objects with `applicable: bool`; verdict reduces only applicable gates.
- Preserves: global reacquisition evaluation over all supplied rows.

- [ ] **Step 1: Write failing evaluator tests**

Add a holdout-only assertion that `full_occlusion_semantics.applicable` is false
and cannot fail the verdict, plus a holdout fixture with a second reacquisition
that still fails the global reacquisition gate.

- [ ] **Step 2: Run RED**

Run: `python3 -m unittest tests.test_evaluate_occ_video_1 -v`

Expected: FAIL because gates have no applicability semantics and the empty
full-occlusion intersection currently fails.

- [ ] **Step 3: Implement applicability**

Extend `_gate(passed, detail, frames=None, applicable=True)` and exclude
non-applicable gates from verdict and worst-frame failure reduction. Render
`N/A` distinctly in Markdown.

- [ ] **Step 4: Run GREEN and regression**

Run: `python3 -m unittest tests.test_evaluate_occ_video_1 -v`

Expected: all evaluator tests PASS.

- [ ] **Step 5: Commit**

Commit: `fix: make occlusion evaluator gates scope-aware`

### Task 2: Complete search-hint identity

**Files:**
- Modify: `rk3566/nanotrack_v3/src/long_term_controller.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_long_term_controller.cpp`
- Modify: `rk3566/nanotrack_v3/tests/fixtures/valid_board_run/bboxes.csv` only if the fixture generator contract requires it.

**Interfaces:**
- Consumes: optional `SearchPlan::search_hint`.
- Produces: no `ControllerOutput::search_hint` for a conflict with no real hint;
  preserves and marks a real hint as `CONFLICT`.

- [ ] **Step 1: Write failing controller tests**

Cover a locked detector candidate before minimum search age with no plan hint,
and a real pending hint that conflicts. Assert absence in the first case and
complete identity plus `CONFLICT` in the second.

- [ ] **Step 2: Run RED**

Run: `cmake --build /tmp/nanotrack-occ-c010-host --target test_long_term_controller -j2 && ctest --test-dir /tmp/nanotrack-occ-c010-host -R '^long_term_controller$' --output-on-failure`

Expected: the absent-hint case FAILS because a zero-valued optional hint is emitted.

- [ ] **Step 3: Implement the source fix**

Only copy and reject `plan.search_hint` when it exists. Keep detector diagnostics
as the evidence channel for a locked candidate that has not created a hint.

- [ ] **Step 4: Run GREEN and complete CTest**

Run the focused command, then `ctest --test-dir /tmp/nanotrack-occ-c010-host --output-on-failure`.

Expected: focused test and all 19 CTests PASS.

- [ ] **Step 5: Commit**

Commit: `fix: never publish an empty search hint`

### Task 3: Bounded temporal dual-template bridge

**Files:**
- Modify: `rk3566/nanotrack_v3/include/controller_config.hpp`
- Modify: `rk3566/nanotrack_v3/src/controller_config.cpp`
- Modify: `rk3566/nanotrack_v3/include/long_term_types.hpp`
- Modify: `rk3566/nanotrack_v3/include/long_term_controller.hpp`
- Modify: `rk3566/nanotrack_v3/src/long_term_controller.cpp`
- Modify: `rk3566/nanotrack_v3/src/long_term_types.cpp`
- Modify: `rk3566/nanotrack_v3/src/main.cpp` or existing session-metadata publisher.
- Modify: `rk3566/nanotrack_v3/config/visible-occlusion-calibrated.conf`
- Modify: controller/config/log/verifier tests under `rk3566/nanotrack_v3/tests/`.

**Interfaces:**
- Produces: `template_bridge_min_stable_frames` config and metadata identity;
  `AcceptedObservationPath::kTemplateBridge`; bounded controller state
  `dual_template_reliable_streak_`.
- Preserves: existing direct, short-confirmation, detector, occlusion-probe, and
  search-reacquisition semantics.

- [ ] **Step 1: Write failing behavior and contract tests**

Add literal synthetic sequences for: mature streak then one anchor score-only
failure bridges; the next consecutive failure searches; no prior streak
searches; geometry disagreement searches; hard geometry failure searches;
probation/search/reinitialization reset the streak; config parsing/identity,
metadata, accepted-path serialization, and verifier accept the new path.

- [ ] **Step 2: Run RED**

Run focused controller/config/log/verifier CTests.

Expected: compile or assertion failures for the missing config, path, state, and behavior.

- [ ] **Step 3: Implement minimal bridge**

Implement exactly the eligibility and reset rules in the spec. Set default `0`
and visible C010 value `3`. Do not change reliability thresholds.

- [ ] **Step 4: Run GREEN and full host regressions**

Run all 19 CTests, evaluator/robustness Python tests, `git diff --check`, and
the repository Python discovery. Record any existing environment-only import
errors separately; do not call them passing.

Expected: CTest 19/19 and task-relevant Python tests PASS.

- [ ] **Step 5: Commit**

Commit: `fix: bridge isolated template quality dips`

### Task 4: RK3588 C010 complete-video evidence

**Files:**
- Modify: `docs/evaluation/occ-video-1/calibration-ledger.md`
- Create: `docs/evaluation/occ-video-1/c010-result.json`
- Modify: `docs/evaluation/occ-video-1/rk3588-final-summary.md`
- Modify: `docs/progress/board-validation.md`

**Interfaces:**
- Consumes: exact committed C010 source, config identity, RK3588 tracker models,
  RK3588 detector model/manifest/labels, canonical dataset.
- Produces: hashed board build/test/run/verifier/evaluator evidence and a clear
  PASS or next-design decision.

- [ ] **Step 1: Package and verify identities**

Transfer a committed source archive to a new immutable board root, verify SHA-256,
native-configure Debug with RKNN enabled and `-Werror=switch`, build, and run all CTests.

Expected: build succeeds and CTest 19/19 PASS.

- [ ] **Step 2: Run the complete video once**

Run exactly frames 0--270 with the same tracker/detector artifacts and retain
CSV, video, log, metadata, manifest, verifier output, and full evaluator reports.

Expected: verifier PASS, 271 rows/frames, and full evaluator PASS.

- [ ] **Step 3: Diagnose rather than weaken if still red**

If false search is gone but visible geometry still fails, record C010 as partial
and create a C011 design from the new per-frame evidence. Do not lower evaluator
gates or mark the goal complete.

- [ ] **Step 4: Repeat a passing candidate three times**

Only after the first complete run passes, execute two more exact runs and compare
semantic events and evaluator gates.

Expected: all three runs PASS with one reacquisition in `[182,190)` and no later search.

- [ ] **Step 5: Produce review artifact and commit evidence**

Create a compact frame/video review artifact covering pre-occlusion, 172--180,
reappearance, and the former 200--216 failure interval. Record commands, hashes,
evidence boundaries, and post-holdout status in the owned progress file and commit.

Expected: repository contains reproducible compact evidence; large artifacts remain on board.
