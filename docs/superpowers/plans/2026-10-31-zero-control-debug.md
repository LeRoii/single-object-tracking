# RK3566 Zero-Control Diagnostic Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build and stage an isolated A6 diagnostic executable that preserves the real tracking display box while always presenting a zero-offset control box to the host servo layer.

**Architecture:** Keep NanoTrack, long-term state transitions, detector behavior, and all real output boxes unchanged. Add one small pure helper for constructing a centered control box, apply it only to `track_box_result.rect[0]`, and enrich adapter logs with the existing observation and decision metrics.

**Tech Stack:** C++14, CMake/CTest, RK3566 AArch64 cross toolchain, SSH/SCP, shell hash verification.

**Spec:** `docs/superpowers/specs/2026-10-31-zero-control-debug-design.md`

## Global Constraints

- The input frame is fixed at 1920x1080 RGB with stride 5760.
- Only `track_box_result.rect[0]` may be replaced by the centered control box.
- `rect_show` and `track_box_result.rect[1..4]` must retain real algorithm results and existing semantics.
- Do not change NanoTrack inference, the retained dimension guard, controller decisions, detector behavior, model paths, command return semantics, or cancellation behavior.
- Build only platform A6 using `BUILD_JOBS=4 ./run.sh --platform A6`.
- Upload to a new `/app` directory and do not overwrite `/app/bin/viewlink_app` or an existing candidate.
- Compilation and upload are not development-board, tracking-quality, or real-gimbal acceptance evidence.

## Review Focus

- Initial tracking frame: control center is exactly `(960,540)` while the displayed initial box remains at the selected target.
- Reliable tracking frame: a valid real box must not leak into `rect[0]`, and `rect_show` must not be centered.
- Uncertain/lost frame: the predicted/final real display box remains visible while control offset stays zero.
- Edge-clipped or small boxes: centering preserves width/height after existing dimension handling and cannot move the real display box.
- Cancel/no-output commands: existing return/output semantics remain unchanged and no fabricated real box is introduced.

---

### Task 1: Pin the output-channel separation with a pure helper test

**Files:**
- Create: `.superpowers/sdd/2026-10-31-zero-control-debug/patched/control_output_safety.hpp`
- Create: `.superpowers/sdd/2026-10-31-zero-control-debug/tests/test_control_output_safety.cpp`

**Interfaces:**
- Consumes: a rectangle with integer `x`, `y`, `width`, and `height` fields.
- Produces: `template <typename Rect> Rect centered_control_box(const Rect&, int image_width, int image_height)`.

- [ ] **Step 1: Write the failing test**

  Assert that a real box `(137,211,83,59)` produces a separate control box centered exactly at `(960,540)`, retains `83x59`, and leaves the original/display box byte-for-byte unchanged. Add cases for odd/even dimensions and a clipped small box.

- [ ] **Step 2: Run the focused test and verify RED**

  Run: `g++ -std=c++14 -Wall -Wextra -pedantic .superpowers/sdd/2026-10-31-zero-control-debug/tests/test_control_output_safety.cpp -o /tmp/test_zero_control`

  Expected: compilation fails because `centered_control_box` is not yet defined.

- [ ] **Step 3: Implement the minimal pure helper**

  Return a copied rectangle whose width/height are preserved and whose `x/y` place its integer center at the image center; never mutate the input rectangle.

- [ ] **Step 4: Run the focused test and verify GREEN**

  Run the same compile command, then `/tmp/test_zero_control`.

  Expected: exit status 0.

### Task 2: Separate control output from real display output and add diagnostics

**Files:**
- Modify: `.superpowers/sdd/2026-10-30-rk3566-production-deployment-config/task-2-prepared/patched/native-lib_impl.cpp`
- Copy: `.superpowers/sdd/2026-10-31-zero-control-debug/patched/control_output_safety.hpp` into the WSL adapter include tree.

**Interfaces:**
- Consumes: `RuntimeFrameResult`, its selected output/metrics and per-template observations.
- Produces: `rect[0]=centered_control_box(real_box,1920,1080)`, unchanged real `rect[1..4]`, unchanged real `rect_show`, and structured `AI_TRACK_DIAG`/`AI_TRACK_OBS` logs.

- [ ] **Step 1: Add static adapter-contract checks**

  Verify source text has exactly one assignment path for centered `rect[0]`, assigns `rect_show` from the real final box, and never centers `rect_show` or `rect[1..4]`.

- [ ] **Step 2: Apply zero control on initialization and every tracking result**

  Use the helper for `rect[0]` on `STATE_INIT_TRACK` and all subsequent output-producing frames. Preserve the current initial/final/predicted assignments for all display and diagnostic rectangles.

- [ ] **Step 3: Add decision and observation logs**

  Log state/mode/quality/accepted/control-valid, real/control/predicted/final boxes, selected raw score, penalized peak, PSR, APCE, peak margin, center residual, area-log change and aspect-log change. Emit one observation line per anchor/recent observation with template kind, box and the five response metrics.

- [ ] **Step 4: Run focused, static, and host regression tests**

  Expected: helper test passes, adapter contract passes, `git diff --check` passes, and the existing RKNN-independent CTest suite remains 17/17.

### Task 3: Cross-compile and verify the A6 artifact

**Files:**
- Modify in WSL: `/home/wjm/viewpro_gimbal_zp/rk3566/gimbal_tracker/ai_alg/source/native-lib_impl.cpp`
- Create in WSL: `/home/wjm/viewpro_gimbal_zp/rk3566/gimbal_tracker/ai_alg/include/control_output_safety.hpp`
- Output in WSL: `/home/wjm/viewpro_gimbal_zp/rk3566/gimbal_tracker/build_A6/viewlink_app`

**Interfaces:**
- Consumes: tested adapter/helper and the already-synchronized retained dimension-guard source.
- Produces: AArch64 `viewlink_app` with recorded size and SHA-256.

- [ ] **Step 1: Copy only the tested adapter/helper into WSL and verify hashes**

- [ ] **Step 2: Execute the A6 build**

  Run: `BUILD_JOBS=4 ./run.sh --platform A6`

  Expected: exit status 0 and successful `libAI.a`/`viewlink_app` links.

- [ ] **Step 3: Verify the artifact**

  Record `file`, size, SHA-256, and confirm the new diagnostic log strings are present. Confirm the binary differs from the previous diagnostic binary.

### Task 4: Upload to an isolated board directory and record the handoff

**Files:**
- Create on board: `/app/nanotrack_b827_zero_control_diag_20261031/bin/viewlink_app`
- Modify: `docs/progress/cross-compile.md`

**Interfaces:**
- Consumes: the verified A6 binary and existing `/app/nanotrack_b827_candidate` models/configuration.
- Produces: an isolated executable ready for a later human-triggered run, plus a reproducible evidence record.

- [ ] **Step 1: Recheck board reachability and destination safety**

  Confirm the protected `/app/bin/viewlink_app` hash and the intended new destination; do not stop or start an existing process during upload.

- [ ] **Step 2: Upload without overwriting existing candidates**

  Create the new directory, transfer to a staging name, verify remote size/SHA-256, then rename within that new directory.

- [ ] **Step 3: Verify isolation**

  Recheck `/app/bin/viewlink_app` and existing candidate hashes are unchanged. Do not launch the new executable unless the user separately asks.

- [ ] **Step 4: Record exact evidence and commit owned changes**

  Add timestamp, branch/HEAD, source and binary identities, commands/results, deployment path, evidence boundaries, blockers and next action to `docs/progress/cross-compile.md`; stage only owned/related files and commit them together.
