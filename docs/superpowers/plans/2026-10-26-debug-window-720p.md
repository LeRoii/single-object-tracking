# RK3566 Debug Window 720p Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Open the standalone interactive RK3566 Debug window at 1280×720 without changing algorithm or output coordinates.

**Architecture:** Define the default window dimensions beside the interactive-control API and consume them from `main.cpp` immediately after `cv::namedWindow()`. Keep the window resizable and leave all frame data untouched.

**Tech Stack:** C++17, OpenCV HighGUI, CMake/CTest

**Spec:** `docs/superpowers/specs/2026-10-26-debug-window-720p-design.md`

## Global Constraints

- Initial window size is exactly `1280×720`.
- The change applies only to interactive Debug mode.
- Do not resize input frames, tracking coordinates, detector inputs, or saved outputs.
- Do not add CLI/configuration surface or modify production `ai_alg`.
- Use only simple verification; no long-term/recovery acceptance run.

## Review Focus

- The window remains `cv::WINDOW_NORMAL` so the user can resize it after startup.
- `cv::resizeWindow()` occurs only after the named window exists.
- Width and height are not transposed.
- Non-interactive execution does not call HighGUI window sizing.
- ROI and tracking continue to use the original frame, not a resized image.

---

### Task 1: Set and verify the interactive window default size

**Files:**
- Modify: `rk3566/nanotrack_v3/include/interactive_control.hpp`
- Modify: `rk3566/nanotrack_v3/src/main.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_interactive_control.cpp`
- Modify: `docs/progress/board-validation.md`

**Interfaces:**
- Produces: `kInteractiveWindowWidth=1280` and `kInteractiveWindowHeight=720`
- Consumes: existing `cv::namedWindow()` lifecycle in interactive Debug mode

- [x] **Step 1: Add a failing unit test**

Assert that the public interactive window width and height constants exist and equal `1280` and `720`.

- [x] **Step 2: Run the focused test and observe RED**

Build `test_interactive_control`; expected failure is missing window-size constants.

- [x] **Step 3: Add the constants and resize the created window**

Define the constants in `interactive_control.hpp`, then call `cv::resizeWindow()` immediately after `cv::namedWindow()` inside the existing `options.interactive` branch.

- [x] **Step 4: Verify GREEN and run the full host suite**

Run the focused test, full CTest, and `git diff --check`; expect all to pass.

- [x] **Step 5: Perform simple board verification when reachable**

Apply the committed source delta to the isolated board Debug worktree, rebuild natively, run CTest, and perform only a bounded interactive startup check. If the tunnel is unavailable, record that exact blocker without claiming board verification.

- [x] **Step 6: Record evidence and commit**

Update only `docs/progress/board-validation.md`, stage the named code/test/design/plan files, inspect the staged file list, and commit the result.
