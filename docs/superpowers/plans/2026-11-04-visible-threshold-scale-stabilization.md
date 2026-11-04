# Visible Threshold and Scale Stabilization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reduce false visible-light loss transitions and prevent enlarged recovery crops from causing abrupt final width/height changes.

**Architecture:** Keep raw NanoTrack observations unchanged, but make `LongTermController` the single owner of the accepted scale. Visible configuration supplies staged quality thresholds and a 5% per-accepted-frame dimension limit; rejected/recovery prediction freezes the last accepted dimensions while retaining center motion. The runtime already commits `ControllerOutput::bbox` back to NanoTrack, so stabilizing that one box keeps output, motion state, and the next tracker reference consistent.

**Tech Stack:** C++17, CMake/CTest, existing standalone assertion-based tests.

**Spec:** `docs/superpowers/specs/2026-11-04-visible-threshold-scale-stabilization-design.md`

## Global Constraints

- Change visible defaults to PSR `4.2/4.0`, peak margin `0.04/0.02`, and `max_accepted_scale_step_ratio=0.05`.
- Keep `calibrated=false`, infrared thresholds, detector behavior, raw NanoTrack dimension guard, and two-of-three reliable recovery confirmation unchanged.
- Use one stabilized accepted box for controller output, motion correction/reinitialization, current controller state, and runtime tracker commit.
- Freeze dimensions, not center prediction, after an unaccepted observation.
- Do not cross-compile, deploy, start/stop remote processes, or claim RK3566/pod acceptance.
- Do not modify `docs/progress/STATUS.md`, `docs/progress/integration-contract.yaml`, or `docs/progress/board-validation.md`.
- Do not create a Git commit without separate user authorization.

## Review Focus

- Frame-edge targets: limited dimensions must remain valid and fully inside the frame while retaining the requested center when possible; Task 2 tests this.
- Repeated recovery prediction: frozen width/height must not drift or collapse even when prior accepted frames created scale velocity; Task 2 tests this.
- Reacquisition: confirmation observations may use enlarged crops, but the accepted box must apply the same 5% limit and unfreeze cleanly; Task 2 tests this.
- Genuine gradual scale change: consecutive reliable frames must accumulate multiple 5% steps instead of permanently freezing scale; Task 2 tests this.
- Configuration provenance: invalid/non-finite limits must be rejected and the effective value must appear in metadata and built-in identity; Task 1 tests the parser and metadata.

---

### Task 1: Visible Thresholds and Observable Scale-Limit Configuration

**Files:**
- Modify: `rk3566/nanotrack_v3/include/controller_config.hpp`
- Modify: `rk3566/nanotrack_v3/src/controller_config.cpp`
- Modify: `rk3566/nanotrack_v3/config/visible-default.conf`
- Modify: `rk3566/nanotrack_v3/include/tracking_log.hpp`
- Modify: `rk3566/nanotrack_v3/src/tracking_log.cpp`
- Modify: `rk3566/nanotrack_v3/src/main.cpp`
- Modify: `rk3566/nanotrack_v3/scripts/verify_board.sh`
- Modify: `rk3566/nanotrack_v3/tests/test_controller_support.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_tracking_log.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_verify_board_schema.sh`

**Interfaces:**
- Produces: `ControllerConfig::max_accepted_scale_step_ratio` as a finite `(0,1]` fraction.
- Produces: `TrackingSessionMetadata::max_accepted_scale_step_ratio` and JSON field of the same name.
- Visible defaults/config: `4.2`, `4.0`, `0.04`, `0.02`, `0.05`.
- Generic and infrared defaults: existing quality values and unrestricted scale ratio `1.0`.

- [x] **Step 1: Write failing configuration and metadata tests**

  Update `test_controller_support.cpp` to assert the exact visible candidate values, unchanged infrared values, shipped-config parity, parsing of `max_accepted_scale_step_ratio`, and rejection of zero, values above one, infinity, and NaN. Update `test_tracking_log.cpp` to assert that applying controller metadata copies `0.05` and JSON emits `"max_accepted_scale_step_ratio": 0.050000`.

- [x] **Step 2: Run focused tests and verify RED**

  Run:

  ```bash
  cmake -S rk3566/nanotrack_v3 -B /tmp/nanotrack-visible-scale-stabilization -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF
  cmake --build /tmp/nanotrack-visible-scale-stabilization --target test_controller_support test_tracking_log -- -j2
  ```

  Expected: compilation fails because the new controller and metadata fields do not yet exist, proving the tests exercise the missing behavior.

- [x] **Step 3: Implement the minimal configuration path**

  Add `max_accepted_scale_step_ratio` with generic default `1.0F`; parse and validate it in `(0,1]`; set visible built-in default to `0.05F`; update visible PSR/peak-margin defaults and `visible-default.conf`; copy and serialize the scale ratio in session metadata; include it in `builtin_config_identity()` so a built-in configuration hash changes with the effective behavior.

- [x] **Step 4: Run focused tests and verify GREEN**

  Run:

  ```bash
  cmake --build /tmp/nanotrack-visible-scale-stabilization --target test_controller_support test_tracking_log -- -j2
  ctest --test-dir /tmp/nanotrack-visible-scale-stabilization -R '^(controller_support|tracking_log)$' --output-on-failure
  ```

  Expected: both focused tests pass.

### Task 2: Controller-Owned Accepted Scale and Loss Freeze

**Files:**
- Modify: `rk3566/nanotrack_v3/include/long_term_controller.hpp`
- Modify: `rk3566/nanotrack_v3/src/long_term_controller.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_long_term_controller.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_long_term_runtime.cpp`

**Interfaces:**
- Consumes: `ControllerConfig::max_accepted_scale_step_ratio` from Task 1.
- Produces: private `BBox stabilize_accepted_bbox(const BBox& observation) const` or an equivalently focused helper.
- Preserves: `LongTermRuntime` commits the exact stabilized `ControllerOutput::bbox` through the existing `ShortTermTracker::commit_bbox(const BBox&)` interface.

- [x] **Step 1: Write the failing accepted-scale tests**

  In `test_long_term_controller.cpp`, create a controller with a `0.05` limit and assert that a reliable observation with a doubled width/height preserves its observed center but emits at most `1.05x` dimensions; a second reliable frame accumulates another `1.05x`; a shrinking observation emits no less than `0.95x`; and a target near each frame edge remains a valid in-frame box.

- [x] **Step 2: Run the focused controller test and verify RED**

  Run:

  ```bash
  cmake --build /tmp/nanotrack-visible-scale-stabilization --target test_long_term_controller -- -j2
  ctest --test-dir /tmp/nanotrack-visible-scale-stabilization -R '^long_term_controller$' --output-on-failure
  ```

  Expected: assertions fail because accepted observations currently replace the full output scale without the 5% limit.

- [x] **Step 3: Implement accepted-scale limiting**

  Add the focused helper to clamp width and height independently around `current_bbox_`, rebuild the box around the observation center using the existing frame-safe centering behavior, and make `accept_observation()` use the stabilized box before motion correction/reinitialization and before assigning `current_bbox_`.

- [x] **Step 4: Run the focused controller test and verify GREEN**

  Run the Step 2 commands again. Expected: the new accepted-scale cases pass without breaking the existing controller cases.

- [x] **Step 5: Write failing loss-freeze and reacquisition tests**

  Add controller tests that first create nonzero scale velocity, then reject observations and assert: the first rejected output returns to the last accepted dimensions; `MotionEstimator::scale_frozen()` is true; repeated `UNCERTAIN`/recovery predictions retain exactly those dimensions while center prediction may change; two reliable, geometrically consistent recovery hits are still required; and the accepted recovery box uses the observed center, the 5% dimension limit, and an unfrozen motion state. Add a runtime assertion that `FakeTracker::committed_bbox` exactly equals the stabilized output after acceptance.

- [x] **Step 6: Run focused controller/runtime tests and verify RED**

  Run:

  ```bash
  cmake --build /tmp/nanotrack-visible-scale-stabilization --target test_long_term_controller test_long_term_runtime -- -j2
  ctest --test-dir /tmp/nanotrack-visible-scale-stabilization -R '^(long_term_controller|long_term_runtime)$' --output-on-failure
  ```

  Expected: the loss-freeze assertions fail because ordinary rejected paths currently retain scale velocity until a special occlusion-hold trigger.

- [x] **Step 7: Implement loss-scale freezing**

  On the first ordinary unaccepted result, freeze motion scale to `current_bbox_` dimensions, zero scale velocity through the existing `MotionEstimator::freeze_scale()`, and publish `motion_.current_bbox()` so the predicted center is retained with the last accepted dimensions. Leave the already-frozen path idempotent. Recovery acceptance continues through `accept_observation()`, whose reinitialization unfreezes the stabilized scale.

- [x] **Step 8: Run focused tests and verify GREEN**

  Run the Step 6 commands again. Expected: controller and runtime focused tests pass.

### Task 3: Full Host Regression and Evidence Update

**Files:**
- Modify after successful verification only: `docs/progress/cross-compile.md`

**Interfaces:**
- Consumes: Tasks 1-2 working tree changes.
- Produces: exact host commands/results and evidence boundaries in the task-owned progress file; no board or pod acceptance statement.

- [x] **Step 1: Run formatting and full host verification**

  Run:

  ```bash
  git diff --check
  cmake --build /tmp/nanotrack-visible-scale-stabilization -- -j2
  ctest --test-dir /tmp/nanotrack-visible-scale-stabilization --output-on-failure
  ```

  Expected: clean diff, successful host build, and all CTest cases passing.

- [x] **Step 2: Review the complete diff against the design**

  Confirm exact visible values, unchanged infrared defaults, `calibrated=false`, internal/output bbox consistency, no detector changes, and no edits to coordination-owned progress files.

- [x] **Step 3: Update only the owned progress file**

  Append an ISO-8601 entry to `docs/progress/cross-compile.md` recording the source baseline, changed behavior, exact verification commands/results, retained log identities, and the boundary that host tests do not prove RK3566 or pod acceptance.

- [x] **Step 4: Re-run final verification after documentation**

  Run:

  ```bash
  git diff --check
  ctest --test-dir /tmp/nanotrack-visible-scale-stabilization --output-on-failure
  git status --short
  ```

  Expected: clean diff, all tests passing, and only the planned uncommitted files listed.
