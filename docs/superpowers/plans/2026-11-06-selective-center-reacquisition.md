# Selective Center Reacquisition Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Prevent short tracking/uncertain confirmation from bypassing accepted-center smoothing while retaining exact reset behavior for true recovery.

**Architecture:** Keep the existing confirmation and quality gates. Change only the classification of a confirmed non-detector observation: tracking/uncertain confirmation follows ordinary correction, while local-or-deeper, detector, and occlusion recovery retain reinitialization. Add an accepted-path enum to `ControllerOutput` and debug telemetry without changing configuration or CSV schema.

**Tech Stack:** C++17, CMake, CTest, existing fake tracker/detector controller tests

**Spec:** `docs/superpowers/specs/2026-11-06-selective-center-reacquisition-design.md`

## Global Constraints

- Keep visible center settings exactly `accepted_center_smoothing_alpha=0.35` and `accepted_center_max_correction_ratio=0.25`.
- Do not change PSR, peak-margin, APCE, template, detector, recovery-time, scale, or servo settings.
- Preserve raw-center reset and motion reinitialization for local-or-deeper, detector, and occlusion-probe recovery.
- Do not add adaptive catch-up in this plan.
- Maintain one final box for controller state, motion, runtime commit, and output.
- Modify only this task's source/tests and `docs/progress/cross-compile.md`; do not modify coordinator or board-validation progress files.

## Review Focus

- A confirmation forced while the plan remains `kTracking` must be short-confirmation, not a true reset; Task 1 tests it through the same classification predicate.
- The exact `uncertain_ms` boundary must classify `kLocalRecovery` as a true reset; Task 1 tests timestamps immediately on the local-recovery side.
- Detector confirmation must override a tracking/uncertain mode and remain true recovery; Task 2 retains and strengthens its existing controller assertion.
- Occlusion search/confirm recovery must remain a raw-center reset; Task 2 adds path assertions to the existing probe test.
- Generic/infrared `1.0/0.0` direct acceptance must remain exact; the full controller suite in Task 2 retains the existing compatibility regression.

---

### Task 1: Separate short confirmation from true recovery

**Files:**
- Modify: `rk3566/nanotrack_v3/src/long_term_controller.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_long_term_controller.cpp`

**Interfaces:**
- Consumes: existing `SearchPlan::mode`, `loss_start_ms_`, `add_confirmation()`, and `ControllerOutput::reacquired`.
- Produces: short confirmation in `kTracking`/`kUncertain` returns `reacquired=false` and uses ordinary scale/center/motion correction; local-or-deeper confirmation remains `reacquired=true`.

- [ ] **Step 1: Write failing controller behavior tests**

Add focused cases that create an unreliable gap, satisfy the existing confirmation requirement in `kUncertain`, and assert the accepted result is blended/limited, `reacquired=false`, and followed by ordinary direct tracking. Extend the existing stale-motion recovery case so its timestamp reaches `kLocalRecovery` and assert it still emits the confirmed raw center with `reacquired=true`.

- [ ] **Step 2: Run the focused test and verify RED**

Run: `cmake --build /tmp/nanotrack-visible-center-state-host -j4 && ctest --test-dir /tmp/nanotrack-visible-center-state-host --output-on-failure -R long_term_controller`

Expected: `long_term_controller` fails because current uncertain confirmation reports reacquired and snaps to the raw center.

- [ ] **Step 3: Implement the minimal classification change**

In the existing non-detector confirmation branch, set true reacquisition only when `plan.mode` is `kLocalRecovery`, `kAssistedRecovery`, or `kLost`; tracking/uncertain confirmation remains accepted but passes `reacquired=false` to `accept_observation()`. Leave detector and occlusion call sites unchanged.

- [ ] **Step 4: Run focused and full controller/runtime tests**

Run: `cmake --build /tmp/nanotrack-visible-center-state-host -j4 && ctest --test-dir /tmp/nanotrack-visible-center-state-host --output-on-failure -R 'long_term_controller|long_term_runtime'`

Expected: `2/2` tests pass.

- [ ] **Step 5: Commit Task 1**

Commit: `fix: preserve smoothing after short uncertainty`

### Task 2: Expose the accepted observation path

**Files:**
- Modify: `rk3566/nanotrack_v3/include/long_term_controller.hpp`
- Modify: `rk3566/nanotrack_v3/src/long_term_controller.cpp`
- Modify: `rk3566/nanotrack_v3/src/debug_telemetry.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_long_term_controller.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_debug_telemetry.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_long_term_runtime.cpp`
- Modify: `docs/progress/cross-compile.md`

**Interfaces:**
- Consumes: Task 1's short-confirmation versus true-recovery classification.
- Produces: `AcceptedObservationPath` with direct, short-confirmation, local-or-deeper, detector, and occlusion-probe values; `ControllerOutput::accepted_observation_path`; accepted `center_filter` telemetry fields `acceptance_path` and `reacquired`.

- [ ] **Step 1: Write failing path and telemetry tests**

Assert direct, short-confirmation, local-or-deeper, detector, and occlusion accepted results expose distinct enum values. Extend the accepted `center_filter` event assertion with stable lowercase path text and `reacquired=0/1`; rejected frames must still emit no center event. Keep runtime's exact final-box commit assertion.

- [ ] **Step 2: Run focused tests and verify RED**

Run: `cmake --build /tmp/nanotrack-visible-center-state-host -j4`

Expected: compilation fails because `AcceptedObservationPath` and `accepted_observation_path` do not yet exist.

- [ ] **Step 3: Implement path reporting**

Define `enum class AcceptedObservationPath` in `long_term_controller.hpp`, default output to direct, assign the path at each existing accepted call site, and add a local telemetry string conversion in `debug_telemetry.cpp`. Do not change controller decisions beyond Task 1.

- [ ] **Step 4: Run focused tests**

Run: `cmake --build /tmp/nanotrack-visible-center-state-host -j4 && ctest --test-dir /tmp/nanotrack-visible-center-state-host --output-on-failure -R 'long_term_controller|long_term_runtime|debug_telemetry'`

Expected: `3/3` tests pass.

- [ ] **Step 5: Run complete verification**

Run: `ctest --test-dir /tmp/nanotrack-visible-center-state-host --output-on-failure && git diff --check`

Expected: `18/18` tests pass with zero failures and format check has no output.

- [ ] **Step 6: Record evidence and commit Task 2**

Update only `docs/progress/cross-compile.md` with timestamp, branch/worktree, source commits, RED/GREEN commands and results, evidence boundary, and next A6 action.

Commit: `test: expose selective center acceptance path`
