# Visible Scale Temporal Consistency and Output Hysteresis Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Stop high-frequency one-pixel width/height reversals while preserving gradual response to persistent real target-size changes.

**Architecture:** Extend the controller configuration/evidence contract with a five-sample accepted-scale median window, three-median direction confirmation, and 0.75-pixel emitted-size hysteresis. `LongTermController` keeps per-dimension temporal scale state, but still emits one final box that is shared by controller state, motion, runtime `commit_bbox()`, adapter output, and logs.

**Tech Stack:** C++17, CMake/CTest, shell/Python board-evidence verifier.

**Spec:** `docs/superpowers/specs/2026-11-05-visible-scale-temporal-hysteresis-design.md`

## Global Constraints

- Keep visible quality thresholds at PSR `4.2/4.0` and peak margin `0.04/0.02`.
- Keep `max_accepted_scale_step_ratio=0.05`, `accepted_scale_deadband_ratio=0.02`, and `accepted_scale_smoothing_alpha=0.10`.
- Visible temporal settings are median window `5`, direction confirmation `3`, and output hysteresis `0.75` pixel.
- Generic/infrared compatibility settings are `1/1/0`; unrestricted legacy acceptance must remain exact.
- Do not add edge/out-of-view handling, center smoothing, aspect-ratio locking, or separate tracker/display boxes.
- Keep metadata schema version `9`, following the existing schema-9 extension precedent, but require all three new effective fields.
- Only `docs/progress/cross-compile.md` may be updated; do not modify coordinator or board-validation progress files.
- This plan ends at local RKNN-independent verification. Do not cross-compile, deploy, or operate remote processes without a new explicit authorization.

## Review Focus

- Alternating `48/49` accepted observations: emitted height must not reverse at every integer boundary; Task 2 adds the regression.
- Exact `0.75`-pixel hysteresis boundary: equality must advance one pixel and a just-inside value must hold; Task 2 pins inclusive behavior.
- Short nonaccepted interval: it must neither add raw samples nor silently discard accepted history; Task 2 checks both sides of the gap.
- Reacquisition after stale direction evidence: it must clear the window/streak without jumping scale; Task 2 covers ordinary recovery and uses the common accept path for detector/occlusion behavior.
- Generic/infrared compatibility: `1/1/0` must return the raw accepted dimensions exactly; Tasks 1 and 2 test contract and behavior.

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
- Produces: `ControllerConfig::accepted_scale_median_window_frames` (`int`), `accepted_scale_direction_confirm_frames` (`int`), and `accepted_scale_output_hysteresis_px` (`float`).
- Produces: matching `TrackingSessionMetadata` fields and required JSON keys with identical names.
- Consumes: existing parser/default/validation/identity and metadata/verifier patterns for accepted-scale settings.

- [ ] **Step 1: Configure a clean host build and prove the current baseline**

Run:

```bash
cmake -S rk3566/nanotrack_v3 -B /tmp/nanotrack-visible-temporal-hysteresis-host -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF
cmake --build /tmp/nanotrack-visible-temporal-hysteresis-host -j4
ctest --test-dir /tmp/nanotrack-visible-temporal-hysteresis-host --output-on-failure
```

Expected: configuration/build succeed and all 18 current tests pass before edits.

- [ ] **Step 2: Write failing configuration and metadata tests**

Extend `test_controller_support.cpp` to assert visible built-in and shipped values `5/3/0.75`, generic/infrared values `1/1/0`, config-file parsing, configuration identity sensitivity, odd-window validation, confirmation-count validation, and negative/non-finite hysteresis rejection. Extend `test_tracking_log.cpp` and `test_verify_board_schema.sh` to require and range-check all three effective fields, including missing-field and invalid-value fixtures.

- [ ] **Step 3: Run the focused tests and verify RED**

Run:

```bash
cmake --build /tmp/nanotrack-visible-temporal-hysteresis-host -j4 --target test_controller_support test_tracking_log
ctest --test-dir /tmp/nanotrack-visible-temporal-hysteresis-host --output-on-failure -R 'controller_support|tracking_log|verify_board_schema'
```

Expected: C++ compilation fails because the three fields do not exist and/or the schema negative fixtures show the verifier incorrectly accepts missing fields. Fix test syntax only until failures are exclusively due to the missing contract.

- [ ] **Step 4: Implement the minimal configuration and evidence contract**

Add the three fields with generic defaults `1/1/0`; parse them; set visible defaults and shipped config to `5/3/0.75`; validate window odd and `[1,31]`, confirmation `[1,31]`, and finite hysteresis `>=0`; append them to configuration identity. Copy them through `apply_controller_config_metadata()`, finite-check and serialize them in `tracking_session_metadata_json()`, and require them in `verify_board.sh` without changing schema version `9`.

- [ ] **Step 5: Run focused and full tests for GREEN**

Run:

```bash
cmake --build /tmp/nanotrack-visible-temporal-hysteresis-host -j4
ctest --test-dir /tmp/nanotrack-visible-temporal-hysteresis-host --output-on-failure -R 'controller_support|tracking_log|verify_board_schema'
ctest --test-dir /tmp/nanotrack-visible-temporal-hysteresis-host --output-on-failure
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
git commit -m "feat: define temporal scale filter contract"
```

### Task 2: Stateful accepted-scale filtering and sticky emission

**Files:**
- Modify: `rk3566/nanotrack_v3/include/long_term_controller.hpp`
- Modify: `rk3566/nanotrack_v3/src/long_term_controller.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_long_term_controller.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_long_term_runtime.cpp`

**Interfaces:**
- Consumes: the three `ControllerConfig` fields from Task 1.
- Produces: private per-dimension state containing accepted samples, filtered accumulator, emitted dimension, last direction, and confirmation count.
- Produces: a non-const accepted-scale path that returns the one final emitted `BBox`; `LongTermRuntime` continues committing `result.output.bbox` unchanged.

- [ ] **Step 1: Write failing temporal-filter controller tests**

Add focused tests that configure `5/3/0.75` and assert:

- the first four accepted samples hold scale;
- three consecutive full-window medians in one direction are required before the accumulator moves;
- alternating `48/49` observations and alternating direction medians do not chatter;
- just-inside `0.75` holds and exact `0.75` advances at most one emitted pixel;
- sustained shrink and growth eventually move monotonically in the correct direction while preserving the raw center;
- a nonaccepted gap adds no sample but preserves earlier accepted history;
- `abort_frame()` adds no sample;
- ordinary reacquisition resets stale history and does not jump emitted scale;
- `1/1/0` compatibility emits the raw accepted box exactly.

- [ ] **Step 2: Extend the runtime commit regression**

Change `test_runtime_commits_stabilized_bbox()` to feed enough accepted frames to fill/confirm the temporal filter, then assert every tracker commit equals that frame's final emitted box. Include an alternating-height tail and assert committed height remains sticky; retain the existing assertion that unaccepted output cannot commit.

- [ ] **Step 3: Run controller/runtime tests and verify RED**

Run:

```bash
cmake --build /tmp/nanotrack-visible-temporal-hysteresis-host -j4 --target test_long_term_controller test_long_term_runtime
ctest --test-dir /tmp/nanotrack-visible-temporal-hysteresis-host --output-on-failure -R 'long_term_controller|long_term_runtime'
```

Expected: tests compile after Task 1 but fail because the current controller updates every accepted frame and has no temporal window, direction streak, or emitted-size hysteresis.

- [ ] **Step 4: Implement per-dimension temporal state**

In `LongTermController`, add a private `DimensionScaleState` with `std::deque<float> samples`, `float filtered`, `float emitted`, `int direction`, and `int confirmations`; keep separate width and height instances. Add private reset helpers for full initialization and evidence-only reacquisition reset. Change `stabilize_accepted_bbox()` to mutate a local next-state copy, compute the odd-window median, apply the existing deadband/clamp/alpha only after direction confirmation, and apply inclusive `0.75` hysteresis with at most one emitted pixel per accepted frame. Commit both next states only when the accepted output is formed successfully.

- [ ] **Step 5: Integrate reset/freeze semantics**

Seed both states from `initialize()`. In `accept_observation()`, clear temporal samples/direction before a `reacquired` observation is processed, but retain filtered/emitted size so recovery cannot jump. Keep nonaccepted, coast, and aborted paths free of temporal-state mutation. Continue rebuilding around the selected observation center and keep the existing common final box for motion correction and runtime commit.

- [ ] **Step 6: Run focused tests for GREEN**

Run:

```bash
cmake --build /tmp/nanotrack-visible-temporal-hysteresis-host -j4 --target test_long_term_controller test_long_term_runtime
ctest --test-dir /tmp/nanotrack-visible-temporal-hysteresis-host --output-on-failure -R 'long_term_controller|long_term_runtime'
```

Expected: both tests pass with no assertion output.

- [ ] **Step 7: Run the complete host verification**

Run:

```bash
cmake --build /tmp/nanotrack-visible-temporal-hysteresis-host -j4
ctest --test-dir /tmp/nanotrack-visible-temporal-hysteresis-host --output-on-failure
git diff --check
```

Expected: build succeeds, all 18 tests pass, and `git diff --check` has no output.

- [ ] **Step 8: Record the host-only milestone and commit**

Update only `docs/progress/cross-compile.md` with timestamp, branch/worktree/base/resulting commits when available, exact config/spec/plan hashes, RED/GREEN observations, verification commands/results, evidence limits, and the explicit statement that no WSL/pod operation occurred.

```bash
git add rk3566/nanotrack_v3/include/long_term_controller.hpp \
  rk3566/nanotrack_v3/src/long_term_controller.cpp \
  rk3566/nanotrack_v3/tests/test_long_term_controller.cpp \
  rk3566/nanotrack_v3/tests/test_long_term_runtime.cpp \
  docs/progress/cross-compile.md
git commit -m "fix: suppress accepted scale chatter"
```

### Task 3: Final review and clean handoff

**Files:**
- Inspect only unless review finds a defect in Task 1 or Task 2 files.

**Interfaces:**
- Consumes: both implementation commits and the approved design.
- Produces: a clean branch with evidence-backed local verification and no remote side effects.

- [ ] **Step 1: Review the complete branch diff against the design**

Check configuration compatibility, median ordering, exact hysteresis boundary, reset paths, exception/abort behavior, runtime commit identity, metadata/verifier coverage, and absence of unrelated changes.

- [ ] **Step 2: Re-run completion verification from the final HEAD**

```bash
cmake --build /tmp/nanotrack-visible-temporal-hysteresis-host -j4
ctest --test-dir /tmp/nanotrack-visible-temporal-hysteresis-host --output-on-failure
git diff --check
git status --short
git log -3 --oneline
```

Expected: all 18 tests pass, formatting check is clean, and the worktree is clean.

- [ ] **Step 3: Report the evidence boundary**

Report exact commits and host results. State that reduced pod jitter is still unproven until separately authorized A6 cross-compilation and human pod testing.
