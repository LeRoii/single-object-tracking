# RK3566 Debug Runtime Telemetry Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add bounded structured terminal telemetry to the interactive RK3566 debug executable without changing tracking or control behavior.

**Architecture:** A new pure formatting/state component converts existing runtime results into event lines and owns only telemetry history. The interactive `main.cpp` loop emits lifecycle and key-command events and delegates frame diagnostics to that component.

**Tech Stack:** C++17, OpenCV HighGUI, CMake/CTest

**Spec:** `docs/superpowers/specs/2026-10-23-debug-runtime-telemetry-design.md`

## Global Constraints

- Use `DEBUG_EVENT event=<name> key=value` one-line output.
- Continuous playback heartbeat interval is exactly 30 tracked frames.
- Manual stepping forces one heartbeat for that processed frame.
- Interactive debug logging does not enable output persistence.
- Detector candidates cannot directly authorize control.
- No controller, tracker, detector, or template-update decision may change.

## Review Focus

- The first processed frame must publish an initial state instead of being suppressed.
- A forced manual heartbeat must not reset or postpone the continuous 30-frame heartbeat cadence.
- Missing detector candidate IDs and regions must be emitted safely as `none`.
- Detector errors and rejected/stale batches must remain visible even when no heartbeat is due.
- Reinitialization must clear prior-target transition history without changing cumulative playback frame numbering.

---

### Task 1: Pure Debug Telemetry Component

**Files:**
- Create: `rk3566/nanotrack_v3/include/debug_telemetry.hpp`
- Create: `rk3566/nanotrack_v3/src/debug_telemetry.cpp`
- Create: `rk3566/nanotrack_v3/tests/test_debug_telemetry.cpp`
- Modify: `rk3566/nanotrack_v3/CMakeLists.txt`

**Interfaces:**
- Consumes: `RuntimeFrameResult`, pipeline latency, and a force-heartbeat flag.
- Produces: `DebugTelemetry(std::size_t)`, `reset()`, and `frame_events(const RuntimeFrameResult&, float, bool) -> std::vector<std::string>`.

- [ ] **Step 1: Write failing unit tests**

Test first-frame state output, unchanged-state suppression, control-only changes,
forced and 30-frame heartbeats, detector request/batch/error and verification
fields, reacquisition source, template actions, and reset.

- [ ] **Step 2: Run the focused test and verify RED**

Run: configure/build the `test_debug_telemetry` target and execute it.
Expected: build failure because `debug_telemetry.hpp` and implementation do not exist.

- [ ] **Step 3: Implement the minimal telemetry component**

Use fixed precision, explicit optional formatting, and state comparisons local
to the telemetry object. Never mutate the supplied runtime result.

- [ ] **Step 4: Run the focused test and verify GREEN**

Run: build and execute `test_debug_telemetry`.
Expected: `debug telemetry tests passed`, exit 0.

### Task 2: Interactive Loop Integration

**Files:**
- Modify: `rk3566/nanotrack_v3/src/main.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_interactive_control.cpp` only if a reusable command label interface is introduced.

**Interfaces:**
- Consumes: `DebugTelemetry::reset()` and `frame_events(...)` from Task 1.
- Produces: startup, ROI, playback command, frame-event, and shutdown terminal lines.

- [ ] **Step 1: Add a failing integration-facing test if command labeling requires production code**

Assert exact stable command labels through the public interactive-control API;
otherwise keep command printing local and rely on telemetry unit coverage plus
the board smoke run.

- [ ] **Step 2: Integrate startup, command, ROI, frame, and shutdown events**

Only call frame telemetry in interactive mode. Reset telemetry after each
successful target initialization. Track whether the current frame advance came
from an explicit step so it can force a heartbeat.

- [ ] **Step 3: Run focused and full host tests**

Run: CMake build, focused debug telemetry test, then `ctest --output-on-failure`.
Expected: all configured tests pass.

### Task 3: Development-board Verification and Handoff Record

**Files:**
- Modify: `docs/progress/board-validation.md`

**Interfaces:**
- Consumes: committed source identity from Tasks 1-2.
- Produces: reproducible board build/run commands, binary hash, observed output boundary, blockers, and coordinator handoff request.

- [ ] **Step 1: Synchronize the committed patch to the existing board repository without copying build products**

Preserve the board's unrelated `cmd.txt` and independent history.

- [ ] **Step 2: Clean-build against OpenCV 4.10 and RKNN 2.3.2**

Expected: native AArch64 build succeeds and the exact executable SHA-256 is recorded.

- [ ] **Step 3: Run the complete board CTest suite**

Expected: every configured test passes; report any pre-existing or new failure by name.

- [ ] **Step 4: Run a bounded interactive telemetry smoke test**

Observe startup and at least one interaction/frame event. Do not claim detector
reacquisition, recovery quality, memory plateau, or gimbal-pod acceptance unless
those conditions were actually exercised.

- [ ] **Step 5: Update the owned progress file and commit**

Record timestamp, branch/worktree/base/result commit, board source and binary
identity, commands, observed events, evidence boundary, blockers, next action,
and any requested public-contract update.
