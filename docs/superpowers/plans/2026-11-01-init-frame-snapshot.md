# NanoTrack Init Frame Snapshot Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Save one unannotated PNG containing the exact BGR frame passed to NanoTrack for every `STATE_INIT_TRACK`, without changing tracking or control behavior.

**Architecture:** Add a small no-throw adapter helper around OpenCV `imwrite`, call it only after `make_bgr_view()` and immediately before `LongTermRuntime::initialize()`, and log the image contract plus sanitized initialization box. Keep this diagnostic implementation in the production adapter and deploy it as a new isolated A6 candidate.

**Tech Stack:** C++14, OpenCV 4.x `imgcodecs`, CMake/CTest, RK3566 AArch64 cross toolchain, SSH/SCP, SHA-256 verification.

**Spec:** `docs/superpowers/specs/2026-11-01-init-frame-snapshot-design.md`

## Global Constraints

- Input remains RGB888, 1920x1080, stride 5760; the saved PNG is the converted BGR `cv::Mat` that NanoTrack receives.
- Save only on `STATE_INIT_TRACK`; do not save ordinary `STATE_DO_TRACK` frames.
- Snapshot failure must log an error and must not change `do_tracker_detector()` success/failure behavior.
- Do not modify `rect_show`, `track_box_result.rect[0..4]`, quality thresholds, prediction, detector behavior, model paths, or cancellation behavior.
- Build only A6 with `BUILD_JOBS=4 ./run.sh --platform A6`.
- Deploy to a new `/app` directory and do not overwrite `/app/bin/viewlink_app` or earlier candidates.
- Do not commit PNGs, binaries, logs, models, or other generated artifacts to Git.

## Review Focus

- The saved image must be the post-`COLOR_RGB2BGR` `context.bgr_frame`, not the source buffer reinterpreted with the wrong channel order.
- A missing/unwritable snapshot directory or `imwrite` exception must not abort initialization.
- Repeated initialization may overwrite the fixed diagnostic filename, while ordinary tracking frames must not write it.
- Logged width, height, channels and step must report `1920`, `1080`, `3` and `5760` for the valid production frame.
- The save call must happen before NanoTrack initialization so the file exactly represents the template-input frame.

---

### Task 1: Add and test the one-shot initialization snapshot

**Files:**
- Create in diagnostic staging: `init_frame_snapshot.hpp`
- Create in diagnostic staging: `init_frame_snapshot.cpp`
- Create in diagnostic staging: `test_init_frame_snapshot.cpp`
- Modify in diagnostic staging: `native-lib_impl.cpp`
- Modify in WSL: `/home/wjm/viewpro_gimbal_zp/rk3566/gimbal_tracker/ai_alg/CMakeLists.txt`
- Create in WSL: `/home/wjm/viewpro_gimbal_zp/rk3566/gimbal_tracker/ai_alg/include/init_frame_snapshot.hpp`
- Create in WSL: `/home/wjm/viewpro_gimbal_zp/rk3566/gimbal_tracker/ai_alg/source/init_frame_snapshot.cpp`
- Modify in WSL: `/home/wjm/viewpro_gimbal_zp/rk3566/gimbal_tracker/ai_alg/source/native-lib_impl.cpp`

**Interfaces:**
- Consumes: `const cv::Mat& bgr_frame`, snapshot path, sanitized `RECT_SIZE` initialization box.
- Produces: `bool save_init_frame_snapshot(const cv::Mat&, const std::string&, std::string*) noexcept`; `true` only when OpenCV reports a successful PNG write, otherwise `false` with a diagnostic message.

- [ ] **Step 1: Write the failing focused test**

  Test a known 2x2 BGR matrix written to a temporary PNG and read back unchanged; test an unwritable/nonexistent parent path returns `false` without throwing. Add a static ordering assertion that the adapter calls the helper inside `STATE_INIT_TRACK` after `make_bgr_view()` and before `runtime->initialize()` and nowhere in `STATE_DO_TRACK`.

- [ ] **Step 2: Run the focused test and verify RED**

  Compile/run with the host OpenCV 4 development flags already used by the repository. Expected: failure because `save_init_frame_snapshot` and its adapter call do not exist.

- [ ] **Step 3: Implement the minimal no-throw helper and adapter call**

  Use `cv::imwrite` without drawing or resizing. Log `AI_TRACK_SNAPSHOT` with success/failure, absolute path, `cols`, `rows`, `channels`, `step`, and sanitized initialization box. Use a fixed path in the new isolated deployment directory so a later initialization overwrites the previous PNG.

- [ ] **Step 4: Run focused and existing host tests**

  Expected: focused snapshot tests pass, adapter ordering check passes, `git diff --check` passes, and the existing RKNN-independent CTest suite remains green.

### Task 2: Cross-compile, deploy, and verify the diagnostic artifact

**Files:**
- Output in WSL: `/home/wjm/viewpro_gimbal_zp/rk3566/gimbal_tracker/build_A6/viewlink_app`
- Create on board: `/app/nanotrack_433deff_init_frame_diag_20261101/`
- Modify: `docs/progress/cross-compile.md`

**Interfaces:**
- Consumes: tested adapter snapshot files and the current `433deff` adaptive diagnostic source/model/config identity.
- Produces: an isolated AArch64 `viewlink_app`, a runtime snapshot at `/app/nanotrack_433deff_init_frame_diag_20261101/log/nanotrack_init_input.png`, and reproducible hashes/evidence.

- [ ] **Step 1: Transfer only the tested adapter files to WSL and verify SHA-256**

  Preserve unrelated WSL files and confirm the installed files match staging before building.

- [ ] **Step 2: Build and verify A6 output**

  Run `BUILD_JOBS=4 ./run.sh --platform A6`. Require exit status 0; record `file`, size, SHA-256, and confirm `AI_TRACK_SNAPSHOT` and the fixed PNG path are present in the binary.

- [ ] **Step 3: Deploy to the new board directory**

  Create only the new directory, copy the binary/config/model references needed by the existing launch pattern, verify remote SHA-256, and confirm protected `/app/bin/viewlink_app` plus earlier candidates are unchanged.

- [ ] **Step 4: Launch and perform one human-triggered initialization**

  Start with `XDG_RUNTIME_DIR=/var/run` and `LD_LIBRARY_PATH=/app/lib:/usr/lib`. After the user sends one tracking command, verify exactly one PNG exists, inspect its dimensions/color/content, and correlate its logged initialization box. Do not infer tracking-quality or pod acceptance from this check.

- [ ] **Step 5: Record evidence and commit**

  Update only `docs/progress/cross-compile.md` with timestamp, source/binary/snapshot paths and hashes, commands, observed result, evidence boundaries, blockers and next action. Commit the design, plan, progress update, and any repository-owned diagnostic code/documentation together; exclude generated artifacts.
