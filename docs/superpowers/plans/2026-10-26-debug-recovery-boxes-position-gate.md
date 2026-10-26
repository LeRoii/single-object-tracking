# RK3566 Debug Recovery Boxes and Position Gate Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Distinguish every recovery/debug box by color and label while strictly rejecting detector candidates whose centers jump more than 15% of the frame diagonal from the motion prediction.

**Architecture:** Preserve the motion prediction separately from the detector-overridden search hypothesis, then apply a configurable hard position gate before candidates enter the controller queue. Expose the gate evidence through controller output, telemetry, CSV/metadata, and a pure C++ debug-overlay descriptor layer; OpenCV rendering consumes those descriptors only in interactive Debug mode.

**Tech Stack:** C++17, CMake/CTest, OpenCV HighGUI on RK3566

**Spec:** `docs/superpowers/specs/2026-10-26-debug-recovery-boxes-position-gate-design.md`

## Global Constraints

- Default `detector_candidate_max_center_ratio` is exactly `0.15` and is not relaxed in `LOST`.
- Distance is candidate-center to motion-prediction-center divided by the frame diagonal.
- Candidates over the threshold never enter the controller queue or reach NanoTrack verification.
- Surviving candidates are ordered by distance ascending, then confidence descending, and capped by `detector_max_candidates`.
- Position gating must not add an area/IoU admission gate.
- Detector candidates never directly authorize `control_valid=true`; dual-template verification and 2/3 confirmation remain mandatory.
- Multi-color overlays affect interactive Debug only; non-interactive video and production `ai_alg` remain unchanged.
- Large models, videos, logs, screenshots, and binaries remain outside Git.

## Review Focus

- A candidate exactly at ratio `0.15` must pass, while a value just above it must be rejected (Task 1 boundary test).
- A higher-confidence far candidate must not outrank a lower-confidence near candidate (Task 1 ordering test).
- Valid detector boxes all rejected only by position must report `POSITION_REJECTED`, not `INVALID` (Task 1 status test).
- `FULL_OBJECT` candidate and probe share coordinates but must remain separately labeled in Debug descriptors (Task 3 overlay test).
- A rejected/unconfirmed hypothesis must never be drawn or logged as `ACCEPTED CTRL`, and must keep `control_valid=false` (Tasks 1 and 3 safety tests).

---

### Task 1: Add the strict detector candidate position gate

**Files:**
- Modify: `rk3566/nanotrack_v3/include/controller_config.hpp`
- Modify: `rk3566/nanotrack_v3/src/controller_config.cpp`
- Modify: `rk3566/nanotrack_v3/config/visible-default.conf`
- Modify: `rk3566/nanotrack_v3/config/infrared-default.conf`
- Modify: `rk3566/nanotrack_v3/include/long_term_types.hpp`
- Modify: `rk3566/nanotrack_v3/src/long_term_types.cpp`
- Modify: `rk3566/nanotrack_v3/include/long_term_controller.hpp`
- Modify: `rk3566/nanotrack_v3/src/long_term_controller.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_controller_support.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_long_term_controller.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_long_term_replay.cpp`

**Interfaces:**
- Produces: `ControllerConfig::detector_candidate_max_center_ratio` defaulting to `0.15F`.
- Produces: `SearchPlan::motion_predicted_bbox` and `SearchPlan::detector_candidate_center_ratio`.
- Produces: `ControllerOutput::detector_position_valid_count` and `ControllerOutput::detector_position_rejected_count`.
- Produces: `DetectorBatchStatus::kPositionRejected` serialized as `POSITION_REJECTED`.
- Consumes: existing motion prediction, frame dimensions, detector candidate queue, and 2/3 safety policy.

- [ ] **Step 1: Add failing configuration tests**

Assert that built-in and shipped visible/infrared configs expose `0.15F`; a config value of `NaN`, negative, or greater than `1.0` is rejected; an exact `0.15` value parses successfully.

- [ ] **Step 2: Run the focused configuration test and observe RED**

Run: `cmake --build /tmp/nanotrack-debug-telemetry-build --target test_controller_support -- -j2 && /tmp/nanotrack-debug-telemetry-build/test_controller_support`

Expected: compile or assertion failure because the new config member/key does not exist.

- [ ] **Step 3: Implement and validate the configuration field**

Add the field, strict parser key, `[0,1]` finite validation, and `0.15` entries in both shipped controller configs.

- [ ] **Step 4: Verify configuration GREEN**

Run the focused configuration target and executable; expect PASS.

- [ ] **Step 5: Add failing controller/replay tests for admission, boundary, ordering, and safety**

Cover these exact cases:

- center ratio exactly `0.15` enters the queue;
- a value just greater than `0.15` is excluded;
- all otherwise-valid candidates over the limit yield `POSITION_REJECTED`, zero queued candidates, and no detector hypothesis on the following frame;
- a near `0.70` confidence candidate is tried before a farther `0.99` confidence candidate;
- mixed near/far candidates report both position-valid and position-rejected counts;
- the same hard limit applies after transition to `LOST`;
- rejected candidates never set `accepted`, `reacquired`, or `control_valid`.

- [ ] **Step 6: Run controller/replay tests and observe RED**

Build and run `test_long_term_controller` and `test_long_term_replay`; expected failure is missing fields/status and current confidence-only admission.

- [ ] **Step 7: Implement the position gate**

Preserve the motion prediction before any detector override. Introduce a private ingest-result structure carrying status and counts. Apply validity check → center-ratio gate → distance/confidence sort → cap → queue. Store the ratio on queued/active candidates and surface it in `SearchPlan`.

`detector_position_valid_count` counts candidates that pass the hard gate before the existing maximum-candidate cap; `detector_position_rejected_count` counts otherwise-valid candidates rejected only by the hard gate.

- [ ] **Step 8: Verify focused GREEN and commit Task 1**

Run the three focused tests plus `git diff --check`; stage only Task 1 files and commit `feat(rk3566): gate detector candidates by position`.

---

### Task 2: Make position-gate evidence durable in telemetry and logs

**Files:**
- Modify: `rk3566/nanotrack_v3/include/tracking_log.hpp`
- Modify: `rk3566/nanotrack_v3/src/tracking_log.cpp`
- Modify: `rk3566/nanotrack_v3/src/debug_telemetry.cpp`
- Modify: `rk3566/nanotrack_v3/src/main.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_tracking_log.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_debug_telemetry.cpp`

**Interfaces:**
- Consumes: Task 1 motion prediction, candidate ratio, gate counts, status, and config field.
- Produces: tracking schema version `6` and CSV columns `motion_predicted_*`, `detector_candidate_center_ratio`, `detector_position_valid_count`, and `detector_position_rejected_count`.
- Produces: metadata field `detector_candidate_max_center_ratio` and Debug event fields `position_valid`, `position_rejected`, and `center_ratio`.

- [ ] **Step 1: Add failing CSV, metadata, and Debug telemetry tests**

Assert schema version `6`, header/data column parity, exact mapping from `RuntimeFrameResult`, metadata threshold output, `detector_batch ... position_valid=N position_rejected=N status=POSITION_REJECTED`, and `detector_verify ... center_ratio=...`.

- [ ] **Step 2: Run logging tests and observe RED**

Build and run `test_tracking_log` and `test_debug_telemetry`; expect missing fields and old schema/status text.

- [ ] **Step 3: Implement durable evidence fields**

Extend row mapping, CSV header/encoding, metadata JSON, Debug events, and built-in config identity. Keep existing fields and ordering stable except for appended schema-6 columns.

- [ ] **Step 4: Verify focused GREEN and commit Task 2**

Run both tests plus `git diff --check`; commit `feat(rk3566): log detector position gating`.

---

### Task 3: Add testable multi-color interactive Debug overlays and board evidence

**Files:**
- Create: `rk3566/nanotrack_v3/include/interactive_debug_overlay.hpp`
- Create: `rk3566/nanotrack_v3/src/interactive_debug_overlay.cpp`
- Create: `rk3566/nanotrack_v3/tests/test_interactive_debug_overlay.cpp`
- Modify: `rk3566/nanotrack_v3/CMakeLists.txt`
- Modify: `rk3566/nanotrack_v3/src/main.cpp`
- Modify: `rk3566/nanotrack_v3/README.md`
- Modify: `RK3566_NANOTRACK_LONGTERM_CODE_FLOW.zh-CN.md`
- Modify: `docs/progress/board-validation.md`

**Interfaces:**
- Consumes: complete `RuntimeFrameResult` and Task 1/2 candidate evidence.
- Produces: `DebugBoxKind`, `DebugBoxDescriptor`, `InteractiveDebugOverlay`, and `make_interactive_debug_overlay(const RuntimeFrameResult&)`.
- Produces: interactive-only color mapping and labels defined by the design spec.

- [ ] **Step 1: Add the failing pure-data overlay test and CMake target**

Construct one result containing motion prediction, detector candidate/probe, anchor/recent observations, and accepted output. Assert descriptor order, coordinates, labels, candidate ID/confidence/distance, and accepted status. Add separate tests for `FULL_OBJECT` coordinate overlap and rejected `NO_CTRL` output.

- [ ] **Step 2: Run the overlay test and observe RED**

Configure/build the host test tree and run `test_interactive_debug_overlay`; expected failure is missing API/files.

- [ ] **Step 3: Implement the OpenCV-independent descriptor layer**

Return descriptors in this order: motion prediction, detector candidate, detector probe, anchor, recent, accepted. Generate red `REJECTED NO_CTRL` status without manufacturing a result box.

- [ ] **Step 4: Verify descriptor GREEN**

Build/run the focused overlay test; expect PASS.

- [ ] **Step 5: Replace the interactive renderer with descriptor-driven drawing**

Keep non-interactive `render_annotated()` unchanged. Store the last `RuntimeFrameResult` for the interactive window, map kinds to cyan `(255,255,0)`, magenta `(255,0,255)`, orange `(0,165,255)`, blue `(255,0,0)`, yellow `(0,255,255)`, green `(0,255,0)`, and red `(0,0,255)` in OpenCV BGR order, draw labels and a compact legend, and preserve 1280×720 resizable-window behavior.

- [ ] **Step 6: Update user/debug documentation**

Document the new config key, `POSITION_REJECTED`, new telemetry fields, colors/labels, schema version, strict-policy tradeoff, and the rule that displayed hypotheses are not control-authorized results.

- [ ] **Step 7: Run full host verification**

Run a clean RKNN-independent configure/build, complete CTest, focused overlay/log/controller tests, and `git diff --check`; expect zero failures.

- [ ] **Step 8: Perform simple RK3566 board verification**

In the isolated board Debug worktree, preserve unrelated user changes, apply only committed task patches, verify board time, native-build with OpenCV 4.10/RKNN 2.3.2, run CTest, and do a bounded interactive video run. Retain outside Git the log and screenshot proving labels/colors plus any `POSITION_REJECTED` evidence; record exact commands, identities, hashes, and evidence limits.

- [ ] **Step 9: Record the milestone and commit Task 3**

Update only `docs/progress/board-validation.md`; stage the named overlay/docs/progress files, inspect `git diff --cached --name-status`, and commit `feat(rk3566): distinguish recovery debug boxes`.

- [ ] **Step 10: Review and publish the Debug branch**

Request a fresh whole-range review, fix Critical/Important findings with focused RED→GREEN tests, rerun final verification, and push `codex/debug-runtime-config`. Do not merge to `main` or update the coordinator-owned public contract.
