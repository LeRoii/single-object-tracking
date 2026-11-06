# Visible Center State Estimation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reduce accepted-box center jitter with an auditable motion-aware bounded innovation blend while retaining one final bbox everywhere downstream.

**Architecture:** Extend the effective controller contract with a visible-only center blend alpha and target-relative correction bound. `LongTermController` combines the selected raw center with the existing motion prediction after scale stabilization, publishes diagnostics, and continues passing one final bbox to motion, runtime commit, templates, adapter output, and logs.

**Tech Stack:** C++17, CMake/CTest, shell/Python board-evidence verifier.

**Spec:** `docs/superpowers/specs/2026-11-06-visible-center-state-estimation-design.md`

## Global Constraints

- Visible center settings are smoothing alpha `0.35` and maximum correction ratio `0.25`.
- Generic and infrared compatibility settings are `1.0/0.0`; zero ratio disables correction limiting.
- The ratio limit is applied isotropically against `sqrt(final_width * final_height)` before alpha blending.
- Keep PSR, peak-margin, APCE, detector, template-selection, reacquisition, motion, and accepted-scale settings unchanged.
- Initialization and confirmed reacquisition use the raw confirmed center; only ordinary accepted tracking blends center.
- Keep one final bbox for controller state, motion correction, runtime commit, template staging, adapter output, and logs.
- Preserve raw selected measurements and expose prediction, innovation, applied correction, and limiting in debug telemetry.
- Keep tracking schema version `9`; do not add tracking CSV columns.
- Only `docs/progress/cross-compile.md` may be updated; do not modify coordinator or board-validation progress files.
- This plan ends at local RKNN-independent verification. Do not cross-compile, deploy, or operate remote processes without new explicit authorization.

## Review Focus

- Frame-edge predictions: rebuilding around a blended center must remain contained without invalid dimensions; Task 2 adds a clipped-edge regression.
- Near-zero target size: the correction limit must remain finite and positive for every valid bbox; Task 2 uses a one-pixel bbox fixture.
- Innovation exactly at the ratio boundary: it must not be reported as limited, while a just-over-boundary innovation must be; Task 2 pins both cases.
- Nonaccepted/aborted frames: no selected-center diagnostic or hidden correction may be committed; Task 2 checks controller state and Task 3 checks runtime commit count.
- Reacquisition after stale velocity: confirmed raw center must win once and reset motion; Task 2 adds a stale-prediction recovery regression.

---

### Task 1: Configuration and evidence contract

**Files:**
- Modify: `rk3566/nanotrack_v3/include/controller_config.hpp`
- Modify: `rk3566/nanotrack_v3/src/controller_config.cpp`
- Modify: `rk3566/nanotrack_v3/config/visible-default.conf`
- Modify: `rk3566/nanotrack_v3/include/tracking_log.hpp`
- Modify: `rk3566/nanotrack_v3/src/tracking_log.cpp`
- Modify: `rk3566/nanotrack_v3/scripts/verify_board.sh`
- Test: `rk3566/nanotrack_v3/tests/test_controller_support.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_tracking_log.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_verify_board_schema.sh`

**Interfaces:**
- Produces: `ControllerConfig::accepted_center_smoothing_alpha` (`float`) and `ControllerConfig::accepted_center_max_correction_ratio` (`float`).
- Produces: matching `TrackingSessionMetadata` fields and required JSON keys with identical names.
- Consumes: existing parser/default/validation/identity and metadata/verifier patterns for accepted-scale settings.

- [ ] **Step 1: Configure a clean host build and prove the current baseline**

Run:

```bash
cmake -S rk3566/nanotrack_v3 -B /tmp/nanotrack-visible-center-state-host -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF
cmake --build /tmp/nanotrack-visible-center-state-host -j4
ctest --test-dir /tmp/nanotrack-visible-center-state-host --output-on-failure
```

Expected: configuration/build succeed and all 18 current tests pass before implementation edits.

- [ ] **Step 2: Write failing configuration and metadata tests**

Extend `test_controller_support.cpp` to assert visible built-in and shipped values `0.35/0.25`, generic/infrared values `1.0/0.0`, file parsing, identity sensitivity, alpha validation for zero/negative/greater-than-one/non-finite values, and ratio validation for negative/non-finite values. Extend `test_tracking_log.cpp` and `test_verify_board_schema.sh` to require and range-check both effective fields, including missing and invalid fixtures.

- [ ] **Step 3: Run focused tests and verify RED**

Run:

```bash
cmake --build /tmp/nanotrack-visible-center-state-host -j4 --target test_controller_support test_tracking_log
ctest --test-dir /tmp/nanotrack-visible-center-state-host --output-on-failure -R 'controller_support|tracking_log|verify_board_schema'
```

Expected: compilation fails because the two fields do not exist and/or schema fixtures prove missing settings are accepted. Fix test syntax only until failures are exclusively due to the missing contract.

- [ ] **Step 4: Implement the minimal contract**

Add the two fields with generic defaults `1.0/0.0`; parse them; set visible defaults and shipped config to `0.35/0.25`; validate alpha finite in `(0,1]` and ratio finite `>=0`; append both to configuration identity. Copy them through `apply_controller_config_metadata()`, finite-check and serialize them in metadata JSON, and require them in `verify_board.sh` without changing schema version `9`.

- [ ] **Step 5: Run focused and full tests for GREEN**

Run the focused command from Step 3, then:

```bash
cmake --build /tmp/nanotrack-visible-center-state-host -j4
ctest --test-dir /tmp/nanotrack-visible-center-state-host --output-on-failure
```

Expected: focused tests and all 18 tests pass.

- [ ] **Step 6: Commit the contract**

```bash
git add rk3566/nanotrack_v3/include/controller_config.hpp \
  rk3566/nanotrack_v3/src/controller_config.cpp \
  rk3566/nanotrack_v3/config/visible-default.conf \
  rk3566/nanotrack_v3/include/tracking_log.hpp \
  rk3566/nanotrack_v3/src/tracking_log.cpp \
  rk3566/nanotrack_v3/scripts/verify_board.sh \
  rk3566/nanotrack_v3/tests/test_controller_support.cpp \
  rk3566/nanotrack_v3/tests/test_tracking_log.cpp \
  rk3566/nanotrack_v3/tests/test_verify_board_schema.sh
git commit -m "feat: define accepted center filter contract"
```

### Task 2: Motion-aware accepted-center state estimation

**Files:**
- Modify: `rk3566/nanotrack_v3/include/long_term_controller.hpp`
- Modify: `rk3566/nanotrack_v3/src/long_term_controller.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_long_term_controller.cpp`

**Interfaces:**
- Consumes: Task 1 center settings and `SearchPlan::motion_predicted_bbox`.
- Produces: `ControllerOutput::selected_observation_bbox` (`std::optional<BBox>`), `center_innovation_x/y` (`float`), `center_correction_x/y` (`float`), and `center_correction_limited` (`bool`).
- Produces: a private `stabilize_accepted_center(...)` helper returning the frame-safe final bbox after existing scale stabilization.

- [ ] **Step 1: Write failing center-filter controller tests**

Add tests asserting exact compatibility output, reduced alternating excursions, isotropic limit and limit flag behavior, exact/just-over boundary behavior, sustained-direction progress, one-pixel bbox finiteness, frame-edge containment, no correction from nonaccepted or aborted frames, and raw-center confirmed reacquisition despite stale prediction.

- [ ] **Step 2: Run the focused test and verify RED**

Run:

```bash
cmake --build /tmp/nanotrack-visible-center-state-host -j4 --target test_long_term_controller
ctest --test-dir /tmp/nanotrack-visible-center-state-host --output-on-failure -R long_term_controller
```

Expected: compilation fails because center diagnostics/helper do not exist or behavioral assertions show current direct raw-center output.

- [ ] **Step 3: Implement the minimal center estimator**

Extend `ControllerOutput` with the diagnostic fields. Refactor `stabilize_accepted_bbox()` so dimension filtering remains unchanged, then call `stabilize_accepted_center(const SearchPlan&, const BBox& raw, float final_width, float final_height, bool reacquired, ControllerOutput&)`. For ordinary accepted frames, calculate, isotropically limit, and alpha-blend the innovation exactly as specified; for compatibility mode or reacquisition, emit raw center. Use `centered_box()` for the final frame-safe bbox and populate diagnostics before motion correction.

- [ ] **Step 4: Run the focused test for GREEN**

Run the command from Step 2.

Expected: `long_term_controller` passes with no assertion output.

- [ ] **Step 5: Commit the estimator**

```bash
git add rk3566/nanotrack_v3/include/long_term_controller.hpp \
  rk3566/nanotrack_v3/src/long_term_controller.cpp \
  rk3566/nanotrack_v3/tests/test_long_term_controller.cpp
git commit -m "fix: stabilize accepted bbox centers"
```

### Task 3: Diagnostics and single-final-box runtime contract

**Files:**
- Modify: `rk3566/nanotrack_v3/src/debug_telemetry.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_debug_telemetry.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_long_term_runtime.cpp`

**Interfaces:**
- Consumes: Task 2 `ControllerOutput` diagnostics.
- Produces: `DEBUG_EVENT event=center_filter` on accepted frames with `raw_bbox`, `motion_bbox`, `innovation`, `correction`, `limited`, and `final_bbox` fields.
- Preserves: `LongTermRuntime` commits `result.output.bbox` unchanged and never commits an unaccepted box.

- [ ] **Step 1: Write failing telemetry and runtime tests**

Extend `test_debug_telemetry.cpp` to require one accepted-frame `center_filter` event with all fields and no such event for an unaccepted frame. Extend `test_long_term_runtime.cpp` with alternating-center observations and assert every tracker commit equals the corresponding stabilized `result.output.bbox`, while an unaccepted frame adds no commit.

- [ ] **Step 2: Run focused tests and verify RED**

Run:

```bash
cmake --build /tmp/nanotrack-visible-center-state-host -j4 --target test_debug_telemetry test_long_term_runtime
ctest --test-dir /tmp/nanotrack-visible-center-state-host --output-on-failure -R 'debug_telemetry|long_term_runtime'
```

Expected: telemetry test fails because no center event exists; runtime behavioral expectations must fail if any raw box bypasses the final output.

- [ ] **Step 3: Implement accepted-frame telemetry**

Emit the specified `center_filter` event from `DebugTelemetry::frame_events()` only when `output.accepted` and `selected_observation_bbox` are both true. Do not modify CSV headers, row serialization, or schema version.

- [ ] **Step 4: Run focused tests for GREEN**

Run the command from Step 2.

Expected: both tests pass.

- [ ] **Step 5: Run complete host verification**

```bash
cmake --build /tmp/nanotrack-visible-center-state-host -j4
ctest --test-dir /tmp/nanotrack-visible-center-state-host --output-on-failure
git diff --check
```

Expected: build succeeds, all 18 tests pass, and `git diff --check` has no output.

- [ ] **Step 6: Record the host-only milestone and commit**

Update only `docs/progress/cross-compile.md` with timestamp, branch/worktree/base/resulting commits, spec/plan hashes, RED/GREEN observations, exact verification commands/results, and the explicit evidence boundary that no cross-compilation, deployment, remote process operation, or pod test occurred.

```bash
git add rk3566/nanotrack_v3/src/debug_telemetry.cpp \
  rk3566/nanotrack_v3/tests/test_debug_telemetry.cpp \
  rk3566/nanotrack_v3/tests/test_long_term_runtime.cpp \
  docs/progress/cross-compile.md
git commit -m "test: expose accepted center corrections"
```

### Task 4: Final review and clean handoff

**Files:**
- Inspect only unless review finds a defect in Task 1-3 files.

**Interfaces:**
- Consumes: all implementation commits and the approved design.
- Produces: a clean branch with evidence-backed local verification and no remote side effects.

- [ ] **Step 1: Review the complete branch diff against the design**

Check compatibility bypass, vector-magnitude limiting, exact boundary behavior, frame clipping, recovery reset, nonaccepted/abort isolation, raw diagnostic retention, runtime commit identity, metadata/verifier coverage, unchanged CSV schema, and absence of unrelated changes.

- [ ] **Step 2: Re-run completion verification from final HEAD**

```bash
cmake --build /tmp/nanotrack-visible-center-state-host -j4
ctest --test-dir /tmp/nanotrack-visible-center-state-host --output-on-failure
git diff --check
git status --short
git log -5 --oneline
```

Expected: all 18 tests pass, formatting check is clean, and the worktree is clean.

- [ ] **Step 3: Report the evidence boundary**

Report exact commits and local results. State that reduced pod jitter remains unproven until separately authorized cross-compilation and a human pod trial.
