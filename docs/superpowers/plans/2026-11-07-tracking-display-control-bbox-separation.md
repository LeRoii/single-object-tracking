# Tracking, Display, and Control BBox Separation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Separate NanoTrack feedback state, visible overlay, and gimbal control boxes so display smoothing cannot make the tracker fall behind gimbal-induced image motion.

**Architecture:** `LongTermController` publishes three explicit boxes. The runtime commits and stages templates from `tracking_bbox`; compatibility `bbox` remains the display box. The WSL production adapter maps `control_bbox` to `rect[0]` on every active tracking frame, while `control_valid` remains diagnostic.

**Tech Stack:** C++17, CMake/CTest, RK3566 A6 cross-compilation

**Spec:** `docs/superpowers/specs/2026-11-07-tracking-display-control-bbox-separation-design.md`

## Global Constraints

- Preserve accepted dimension stabilization and existing threshold values.
- Keep `ControllerOutput::bbox` as a compatibility alias for `display_bbox`.
- Do not deploy or claim real-gimbal acceptance.
- Modify only `docs/progress/cross-compile.md` among task progress files.

## Review Focus

- Direct reliable observations moving 30-45 pixels per frame must update tracking state without the display correction ceiling.
- Display output must retain the existing center smoothing and bounded correction diagnostics.
- Runtime tracker commit and recent-template staging must both use tracking state.
- Rejected frames must publish their motion-predicted tracking box to `rect[0]` while retaining `control_valid=false` in diagnostics.
- Occlusion-hold authorized predictions must publish coherent tracking, display, and control boxes.

---

### Task 1: Controller output and runtime feedback separation

**Files:**
- Modify: `rk3566/nanotrack_v3/include/long_term_controller.hpp`
- Modify: `rk3566/nanotrack_v3/src/long_term_controller.cpp`
- Modify: `rk3566/nanotrack_v3/src/long_term_runtime.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_long_term_controller.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_long_term_runtime.cpp`

**Interfaces:**
- Produces: `ControllerOutput::{tracking_bbox,display_bbox,control_bbox}`.
- Preserves: `ControllerOutput::bbox == ControllerOutput::display_bbox`.

- [ ] Add tests asserting raw-center tracking/control boxes, smoothed display/compatibility boxes, motion state following tracking state, and runtime commit/staging from tracking state.
- [ ] Run focused tests and verify they fail because the three fields and feedback mapping do not exist.
- [ ] Add the three output fields and implement the minimum controller/runtime separation.
- [ ] Run focused tests and the complete RKNN-independent CTest suite.
- [ ] Commit the controller, runtime, tests, design, and plan.

### Task 2: Production adapter synchronization and A6 build

**Files:**
- Modify in WSL: `ai_alg/source/native-lib_impl.cpp`
- Modify in WSL by exact Git copies: controller header/source and runtime source
- Modify: `docs/progress/cross-compile.md`

**Interfaces:**
- Consumes: Task 1 `ControllerOutput` fields.
- Produces: `rect[0]=control_bbox` on every active tracking frame and `rect[1]/rect_show=display_bbox`.

- [ ] Update the WSL adapter mapping while retaining the prior centered assignment as a comment.
- [ ] Synchronize exact Task 1 Git sources to WSL and verify SHA-256 equality.
- [ ] Cross-compile A6 and record binary/library hashes and observed build result.
- [ ] Update only `docs/progress/cross-compile.md` with source hashes, commands, results, and evidence boundaries.
- [ ] Commit the production synchronization record.

### Task 3: Integrate into main

**Files:**
- Merge branch history; do not rewrite task history.

**Interfaces:**
- Consumes: verified commits from Tasks 1-2 and the existing independent visible-threshold commit on `main`.
- Produces: a merged `main` retaining both histories.

- [ ] Review the complete branch diff and resolve any overlap without dropping the `main` threshold calibration.
- [ ] Merge `codex/cross-compile-main-b827` into `main` locally.
- [ ] Run the complete RKNN-independent CTest suite on the merged result.
- [ ] Report the resulting main commit and evidence boundary; preserve the task worktree unless separately authorized for cleanup.
