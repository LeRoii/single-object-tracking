# Detector Recapture Candidate Lock Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add safe whole-object and subtarget detector recovery with same-candidate 2/3 confirmation and complete audit fields.

**Architecture:** Extend the existing controller state machine rather than changing the detector. Detector candidates become identified controller hypotheses; an active candidate is probed, locked by agreeing NanoTrack templates, and confirmed before control resumes.

**Tech Stack:** C++17, CMake/CTest, existing NanoTrack and detector adapter interfaces.

**Spec:** `docs/superpowers/specs/2026-10-14-detector-recapture-lock.md`

## Global Constraints

- Do not modify or reimplement YOLOv8 detector inference.
- A detector result alone can never produce `control_valid=true`.
- `full_object` remains the backward-compatible default.
- Thresholds remain `calibrated=false` until labeled data exists.
- Develop in `codex/detector-recapture-debug`, not `main`.

## Review Focus

- Two different same-class candidates must not combine into one 2/3 confirmation.
- A batch with multiple candidates of one class must not establish any lock
  until target-specific identity evidence exists.
- A new detector batch must not replace a candidate already under confirmation.
- Subtarget mode must never change target geometry to the detector region geometry.
- One failed frame inside a three-frame confirmation window must still permit two valid hits.
- Excessive position, area, or aspect drift must leave control invalid.

---

### Task 1: Target mode and recovery configuration

**Files:**
- Modify: `rk3566/nanotrack_v3/include/long_term_types.hpp`
- Modify: `rk3566/nanotrack_v3/include/controller_config.hpp`
- Modify: `rk3566/nanotrack_v3/src/long_term_types.cpp`
- Modify: `rk3566/nanotrack_v3/src/controller_config.cpp`
- Modify: `rk3566/nanotrack_v3/config/visible-default.conf`
- Modify: `rk3566/nanotrack_v3/config/infrared-default.conf`
- Test: `rk3566/nanotrack_v3/tests/test_controller_support.cpp`

**Interfaces:**
- Produces: `DetectorTargetMode`, its string conversion, and validated detector recovery thresholds in `ControllerConfig`.

- [ ] Write configuration parsing and rejection tests for `full_object`, `subtarget`, and invalid values.
- [ ] Run `test_controller_support` and observe failure from missing fields.
- [ ] Add the enum, parser, validation, and uncalibrated defaults.
- [ ] Run `test_controller_support` and the complete CTest suite.
- [ ] Commit the task.

### Task 2: Candidate lock and dual target semantics

**Files:**
- Modify: `rk3566/nanotrack_v3/include/long_term_controller.hpp`
- Modify: `rk3566/nanotrack_v3/src/long_term_controller.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_long_term_controller.cpp`

**Interfaces:**
- Consumes: `DetectorTargetMode` and recovery thresholds from Task 1.
- Produces: identified detector `SearchPlan` fields and candidate-lock diagnostics in `ControllerOutput`.

- [ ] Add failing tests for whole-object hypotheses, subtarget geometry preservation, same-candidate locking, 2/3 with one miss, batch replacement protection, and strict drift rejection.
- [ ] Run `test_long_term_controller` and observe the expected failures.
- [ ] Implement queued candidate ids, subtarget probes, active locking, and strict detector-origin confirmation.
- [ ] Run `test_long_term_controller` and the complete CTest suite.
- [ ] Commit the task.

### Task 3: Audit log and regression replay

**Files:**
- Modify: `rk3566/nanotrack_v3/include/tracking_log.hpp`
- Modify: `rk3566/nanotrack_v3/src/tracking_log.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_tracking_log.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_long_term_replay.cpp`

**Interfaces:**
- Consumes: SearchPlan and ControllerOutput diagnostics from Task 2.
- Produces: schema-v5 CSV fields for candidate identity, same-class ambiguity,
  and confirmation state.

- [ ] Add failing log and replay assertions for candidate identity and no cross-candidate reacquisition.
- [ ] Run the focused tests and observe failure from missing schema fields.
- [x] Add schema-v5 serialization and replay coverage.
- [ ] Run the focused tests and complete CTest suite.
- [ ] Commit the task.

### Task 4: Operator documentation and outstanding validation

**Files:**
- Modify: `rk3566/nanotrack_v3/README.md`
- Create: `rk3566/nanotrack_v3/docs/detector-recapture-todo.md`

**Interfaces:**
- Consumes: configuration and log field names from Tasks 1-3.
- Produces: board-facing mode instructions and a bounded TODO list.

- [ ] Document both target modes and the unchanged control safety rule.
- [ ] Record missing annotations, threshold calibration, `girl_dance` cases, board measurement, auto-mode, and optional ReID as TODOs.
- [ ] Run the complete CTest suite and inspect `git diff --check`.
- [ ] Commit the task.

