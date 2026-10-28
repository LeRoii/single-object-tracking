# RK3566 Occlusion Hold and Detector Debug Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a configurable one-shot `OCCLUSION_HOLD` phase that freezes target scale and skips tracker/detector work for N complete frames, then expose every post-threshold/NMS detector candidate in the interactive Debug view without weakening control safety.

**Architecture:** Extend shared controller types and configuration first, add scale-freeze support to the motion estimator, then implement HOLD as an explicit controller override whose execution decision is carried in `SearchPlan`. Runtime honors that plan, drains but invalidates pre-HOLD detector work, and carries per-candidate diagnostics to rendering, telemetry, and schema-7 logs. Existing motion-path recovery rules stay unchanged; detector candidates still require NanoTrack anchor/recent verification and same-candidate 2-of-3 confirmation.

**Tech Stack:** C++17, CMake/CTest, OpenCV 4.10 for the board UI, RKNN Runtime 2.3.2 on RK3566, existing key-value controller configuration.

**Spec:** `docs/superpowers/specs/2026-10-28-occlusion-hold-detector-debug-design.md`

## Global Constraints

- Work only on `codex/debug-runtime-config` in `/home/u20/single-object-tracking/.worktrees/debug-runtime-config`; preserve unrelated and board-local user changes.
- Defaults are `occlusion_hold_frames=12`, `occlusion_trigger_consecutive_unreliable=2`, `occlusion_trigger_min_dimension_ratio=0.70`, and `occlusion_reference_window=8`.
- Frame `f` detects the trigger; frames `f+1..f+N` are the N complete skipped HOLD frames; frame `f+N+1` resumes recovery.
- One HOLD is allowed per loss episode and is re-armed only by accepted reacquisition or explicit reinitialization.
- HOLD may be entered only from `TRACKING`, `UNCERTAIN`, or `LOCAL_RECOVERY`, never from `ASSISTED_RECOVERY` or `LOST`.
- HOLD frames never call tracker inference or submit detector work, always keep `control_valid=false`, and preserve center motion while width/height remain frozen.
- A detector request invalidated by HOLD can be drained and displayed but can never later enter the candidate queue.
- “All detector results” means configured-class candidates after confidence filtering and NMS, not threshold-pre-NMS tensor cells.
- Invalid/non-finite detector boxes may produce text diagnostics but must never reach OpenCV rectangle conversion.
- Do not tighten existing motion-path recovery; detector-origin recovery still requires reliable agreeing anchor/recent observations and same-candidate 2-of-3 confirmation.
- Do not change models, detector thresholds/NMS, production ABI, `STATUS.md`, or `integration-contract.yaml`.
- Large videos, models, logs, screenshots, and build outputs remain outside Git; record paths, hashes, commands, and evidence boundaries only.

## Review Focus

- Trigger-frame and last-HOLD-frame off-by-one: Task 3 tests N=1 and N=12 and proves exactly N subsequent plans have `run_tracker=false`.
- A pre-HOLD detector batch arriving after HOLD exit: Task 4 tests permanent generation invalidation and diagnostic-only delivery.
- Natural scale motion versus suspected occlusion: Task 3 tests fast translation with stable size, threshold equality, just-above-threshold, and even-window medians.
- Invalid detector coordinates: Task 4 and Task 5 test that diagnostics survive while no drawable box is emitted.
- HOLD re-entry loops: Task 3 tests one trigger per loss episode and re-arm after accepted reacquisition/reinitialize.

---

### Task 1: Shared configuration, state, and diagnostics contracts

**Files:**
- Modify: `rk3566/nanotrack_v3/include/controller_config.hpp`
- Modify: `rk3566/nanotrack_v3/src/controller_config.cpp`
- Modify: `rk3566/nanotrack_v3/config/visible-default.conf`
- Modify: `rk3566/nanotrack_v3/config/infrared-default.conf`
- Modify: `rk3566/nanotrack_v3/include/long_term_types.hpp`
- Modify: `rk3566/nanotrack_v3/src/long_term_types.cpp`
- Modify: `rk3566/nanotrack_v3/include/long_term_controller.hpp`
- Test: `rk3566/nanotrack_v3/tests/test_controller_support.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_long_term_controller.cpp`

**Interfaces:**
- Consumes: Existing strict key-value config parsing and `TrackingMode` string conversion.
- Produces: Four `ControllerConfig` fields with the exact defaults above; `TrackingMode::kOcclusionHold`; `SearchPlan::run_tracker` (`bool`) and `SearchPlan::hold_remaining_frames` (`std::size_t`); `ControllerOutput::{occlusion_hold_triggered,occlusion_hold_remaining_frames,occlusion_reference_width,occlusion_reference_height,occlusion_width_ratio,occlusion_height_ratio}` using bool/count/optional-float types; and the following diagnostic contract for Tasks 4–5:

```cpp
enum class DetectorCandidateRejection {
  kNone, kInvalid, kFrameMismatch, kStale, kHoldInvalidated,
  kPositionRejected, kCapacityRejected
};

struct DetectorCandidateDiagnostic {
  DetectorCandidate candidate{};
  std::optional<float> center_ratio;
  std::optional<std::uint64_t> candidate_id;
  bool batch_eligible = false;
  bool position_gate_pass = false;
  bool queued = false;
  bool drawable = false;
  DetectorCandidateRejection rejection = DetectorCandidateRejection::kNone;
};
```

- [ ] **Step 1: Add failing configuration and type tests**

Assert default and file-loaded values equal `12`, `2`, `0.70F`, and `8`; missing keys retain structure defaults; limits reject hold frames outside `[1,300]`, consecutive count outside `[1,10]`, ratio outside `(0,1]`, and reference window outside `[3,60]`. Assert `to_string(TrackingMode::kOcclusionHold) == "OCCLUSION_HOLD"` and new plan/output structs preserve explicitly assigned values.

- [ ] **Step 2: Run the focused tests and observe RED**

Run:

```bash
cmake -S rk3566/nanotrack_v3 -B /tmp/nanotrack-occlusion-hold-build -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF
cmake --build /tmp/nanotrack-occlusion-hold-build --target test_controller_support test_long_term_controller -- -j2
ctest --test-dir /tmp/nanotrack-occlusion-hold-build -R '^(controller_support|long_term_controller)$' --output-on-failure
```

Expected: compile failures for missing config/state/diagnostic fields or assertion failures for missing parsing.

- [ ] **Step 3: Implement the shared contracts**

Add exact fields and validation to `ControllerConfig`; keep absent-key compatibility by relying on initialized defaults. Add `kOcclusionHold` and its string mapping. Define a diagnostic record containing candidate bbox/class/confidence, optional center ratio and candidate ID, `batch_eligible`, `position_gate_pass`, `queued`, `drawable`, and a finite enum rejection reason.

- [ ] **Step 4: Run focused tests and verify GREEN**

Run the Step 2 command. Expected: both tests pass.

- [ ] **Step 5: Commit Task 1**

```bash
git add rk3566/nanotrack_v3/include/controller_config.hpp rk3566/nanotrack_v3/src/controller_config.cpp rk3566/nanotrack_v3/config/visible-default.conf rk3566/nanotrack_v3/config/infrared-default.conf rk3566/nanotrack_v3/include/long_term_types.hpp rk3566/nanotrack_v3/src/long_term_types.cpp rk3566/nanotrack_v3/include/long_term_controller.hpp rk3566/nanotrack_v3/tests/test_controller_support.cpp rk3566/nanotrack_v3/tests/test_long_term_controller.cpp
git commit -m "feat(rk3566): define occlusion hold contracts"
```

### Task 2: Motion-estimator scale freeze

**Files:**
- Modify: `rk3566/nanotrack_v3/include/motion_estimator.hpp`
- Modify: `rk3566/nanotrack_v3/src/motion_estimator.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_controller_support.cpp`

**Interfaces:**
- Consumes: Existing `BBox`, frame clipping, alpha-beta center/scale prediction.
- Produces: `void MotionEstimator::freeze_scale(float width, float height)`, `bool MotionEstimator::scale_frozen() const`, and initialization behavior that clears freeze state.

- [ ] **Step 1: Add failing scale-freeze tests**

Create a motion sequence that learns non-zero center motion and negative width/height velocity, call `freeze_scale(40.0F, 30.0F)`, then assert multiple predictions move the center but retain width `40` and height `30`. Assert boundary clipping remains legal and `initialize()` clears `scale_frozen()`.

- [ ] **Step 2: Run the motion test and observe RED**

Run:

```bash
cmake --build /tmp/nanotrack-occlusion-hold-build --target test_controller_support -- -j2
/tmp/nanotrack-occlusion-hold-build/test_controller_support
```

Expected: compile failure because `freeze_scale()` and `scale_frozen()` do not exist.

- [ ] **Step 3: Implement scale freeze without resetting center motion**

`freeze_scale()` validates finite positive dimensions, replaces width/height, zeros only `velocity_width_` and `velocity_height_`, marks scale frozen, and clamps to frame. While frozen, `predict()` updates center but not width/height; `correct()` must not reintroduce scale velocity. `initialize()` clears the flag.

- [ ] **Step 4: Run the motion test and verify GREEN**

Run the Step 2 command. Expected: controller support tests pass.

- [ ] **Step 5: Commit Task 2**

```bash
git add rk3566/nanotrack_v3/include/motion_estimator.hpp rk3566/nanotrack_v3/src/motion_estimator.cpp rk3566/nanotrack_v3/tests/test_controller_support.cpp
git commit -m "feat(rk3566): freeze motion scale during occlusion"
```

### Task 3: One-shot OCCLUSION_HOLD controller state

**Files:**
- Modify: `rk3566/nanotrack_v3/include/long_term_controller.hpp`
- Modify: `rk3566/nanotrack_v3/src/long_term_controller.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_long_term_controller.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_long_term_replay.cpp`

**Interfaces:**
- Consumes: Task 1 config/types and Task 2 motion freeze API.
- Produces: Controller plans with exact HOLD timing and `run_tracker`; robust reliable-scale reference; one-shot loss-episode arming; accepted reacquisition/reinitialize reset behavior.

- [ ] **Step 1: Add failing trigger and robust-reference tests**

Add helpers that create reliable accepted boxes followed by weak/rejected observations. Assert: fewer than 8 reliable samples do not trigger; the even-window reference uses the mean of the two middle widths/heights; one unreliable frame does not trigger; the second with `min_dimension_ratio == 0.70` triggers; `0.7001` does not; stable-size fast translation does not trigger; no observation does not trigger.

- [ ] **Step 2: Add failing timing and state-boundary tests**

For `N=1` and `N=12`, assert trigger frame `f` still had `run_tracker=true`, plans `f+1..f+N` have `mode=kOcclusionHold`, `run_tracker=false`, no detector request, frozen dimensions, and invalid control; plan `f+N+1` resumes the time-derived recovery mode. Assert one loss episode cannot trigger twice, `ASSISTED_RECOVERY/LOST` cannot trigger, and accepted reacquisition or `initialize()` re-arms HOLD.

- [ ] **Step 3: Run controller/replay tests and observe RED**

Run:

```bash
cmake --build /tmp/nanotrack-occlusion-hold-build --target test_long_term_controller test_long_term_replay -- -j2
ctest --test-dir /tmp/nanotrack-occlusion-hold-build -R '^(long_term_controller|long_term_replay)$' --output-on-failure
```

Expected: HOLD timing/reference assertions fail.

- [ ] **Step 4: Implement controller HOLD state**

Maintain a deque of accepted reliable widths/heights, a consecutive-unreliable counter, `hold_remaining_frames_`, and `hold_used_for_loss_`. A frame without observations increments the non-reliable counter but cannot itself trigger because it has no shrink ratio. Trigger only after selection on frame `f`; freeze scale using the median reference; clear ordinary confirmations and pending/unlocked detector candidates. In `begin_frame()`, give remaining HOLD frames priority over timestamp-derived mode, decrement only after producing a complete HOLD frame, and bypass candidate activation/request scheduling. In HOLD `finish_frame()`, update frame/timestamp bookkeeping without ordinary confirmation/template actions. Accepted reacquisition re-arms HOLD, clears the old reference history, and seeds the new history with the accepted reliable bbox; explicit `initialize()` clears it without seeding. Preserve current motion-path and detector-path acceptance rules.

- [ ] **Step 5: Run controller/replay tests and verify GREEN**

Run the Step 3 command. Expected: both tests pass.

- [ ] **Step 6: Commit Task 3**

```bash
git add rk3566/nanotrack_v3/include/long_term_controller.hpp rk3566/nanotrack_v3/src/long_term_controller.cpp rk3566/nanotrack_v3/tests/test_long_term_controller.cpp rk3566/nanotrack_v3/tests/test_long_term_replay.cpp
git commit -m "feat(rk3566): add one-shot occlusion hold state"
```

### Task 4: Runtime skipping and permanently invalidated detector batches

**Files:**
- Modify: `rk3566/nanotrack_v3/include/long_term_types.hpp`
- Modify: `rk3566/nanotrack_v3/include/long_term_runtime.hpp`
- Modify: `rk3566/nanotrack_v3/src/long_term_runtime.cpp`
- Modify: `rk3566/nanotrack_v3/src/long_term_controller.cpp`
- Modify: `rk3566/nanotrack_v3/src/yolo_detector_adapter.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_long_term_runtime.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_yolo_detector_adapter.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_long_term_controller.cpp`

**Interfaces:**
- Consumes: Task 3 `SearchPlan::run_tracker` and HOLD state; Task 1 diagnostic record.
- Produces: `DetectorRequest::generation` and `DetectorBatch::generation` as `std::uint64_t`; `RuntimeFrameResult::tracker_skipped` as `bool`; and `ControllerOutput::detector_candidate_diagnostics` as `std::vector<DetectorCandidateDiagnostic>` for diagnostic-only late-batch delivery and rendering/logging.

- [ ] **Step 1: Add failing runtime call-count tests**

Extend the fake tracker/detector with infer/submit/poll counters. Assert each of the N HOLD frames calls poll once, tracker infer zero times, detector submit zero times, and returns `tracker_skipped=true`. Assert the trigger frame still calls infer once and reinitialization clears the skip state.

- [ ] **Step 2: Add failing detector-generation and diagnostic tests**

Submit generation `g` before HOLD, enter HOLD, then deliver its batch both during HOLD and after HOLD exit. Assert all post-NMS candidates remain in diagnostics with `batch_eligible=false` and reason `HOLD_INVALIDATED`, while queue size, active candidate, reacquisition, accepted, and `control_valid` remain false. Include a mixed batch containing position-pass, position-reject, top-N-excluded, and invalid/non-finite entries; every entry gets a diagnostic, but invalid entries have `drawable=false`.

- [ ] **Step 3: Run runtime/adapter/controller tests and observe RED**

Run:

```bash
cmake --build /tmp/nanotrack-occlusion-hold-build --target test_long_term_runtime test_yolo_detector_adapter test_long_term_controller -- -j2
ctest --test-dir /tmp/nanotrack-occlusion-hold-build -R '^(long_term_runtime|yolo_detector_adapter|long_term_controller)$' --output-on-failure
```

Expected: missing generation/diagnostic fields or call-count assertions fail.

- [ ] **Step 4: Implement runtime skip and detector eligibility generation**

Only call `tracker_.infer()` when `plan.run_tracker`. Continue non-blocking `poll()` every processed frame. Copy request generation into `DetectorBatch`; invalidate all pre-HOLD generations on entry. Build diagnostics before any batch-level stale/mismatch/eligibility early return, then independently perform control ingest. Never queue invalid, stale, mismatched, HOLD-invalidated, position-rejected, or post-top-N candidates.

- [ ] **Step 5: Run runtime/adapter/controller tests and verify GREEN**

Run the Step 3 command. Expected: all three tests pass.

- [ ] **Step 6: Commit Task 4**

```bash
git add rk3566/nanotrack_v3/include/long_term_types.hpp rk3566/nanotrack_v3/include/long_term_runtime.hpp rk3566/nanotrack_v3/src/long_term_runtime.cpp rk3566/nanotrack_v3/src/long_term_controller.cpp rk3566/nanotrack_v3/src/yolo_detector_adapter.cpp rk3566/nanotrack_v3/tests/test_long_term_runtime.cpp rk3566/nanotrack_v3/tests/test_yolo_detector_adapter.cpp rk3566/nanotrack_v3/tests/test_long_term_controller.cpp
git commit -m "feat(rk3566): suppress work during occlusion hold"
```

### Task 5: Detector-all-candidate overlay, telemetry, and schema-7 logs

**Files:**
- Modify: `rk3566/nanotrack_v3/include/interactive_debug_overlay.hpp`
- Modify: `rk3566/nanotrack_v3/src/interactive_debug_overlay.cpp`
- Modify: `rk3566/nanotrack_v3/src/main.cpp`
- Modify: `rk3566/nanotrack_v3/include/debug_telemetry.hpp`
- Modify: `rk3566/nanotrack_v3/src/debug_telemetry.cpp`
- Modify: `rk3566/nanotrack_v3/include/tracking_log.hpp`
- Modify: `rk3566/nanotrack_v3/src/tracking_log.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_verify_board_schema.sh`
- Modify: `RK3566_NANOTRACK_LONGTERM_CODE_FLOW.zh-CN.md`
- Modify: `rk3566/nanotrack_v3/README.md`
- Test: `rk3566/nanotrack_v3/tests/test_interactive_debug_overlay.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_debug_telemetry.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_tracking_log.cpp`

**Interfaces:**
- Consumes: Task 1/4 HOLD fields and complete `DetectorCandidateDiagnostic` vector.
- Produces: Drawable descriptors for every valid post-NMS candidate, text-only invalid diagnostics, HOLD UI/status, per-candidate events, schema version 7 CSV/metadata, and field documentation.

- [ ] **Step 1: Add failing overlay tests**

Construct one batch with PASS, `REJECT_POS`, top-N-not-queued, HOLD-invalidated, and invalid candidates. Assert every valid candidate yields a colored box/label with `cls/conf/d/src/age/queued`; invalid yields text only; the active probe remains orange and distinct; no raw detector descriptor is `ACCEPTED CTRL`. Assert HOLD frames show frozen motion plus `OCCLUSION HOLD remaining=N` and no anchor/recent boxes.

- [ ] **Step 2: Add failing telemetry and schema tests**

Assert `occlusion_hold_enter/tick/exit` events contain reference size, ratios, remaining frames, and skip flags. Assert one `detector_candidate` event per returned candidate with eligibility/rejection/queued fields. Update CSV/header/row/metadata assertions from schema 6 to schema 7 and cover all eight new HOLD columns.

- [ ] **Step 3: Run overlay/log/telemetry tests and observe RED**

Run:

```bash
cmake --build /tmp/nanotrack-occlusion-hold-build --target test_interactive_debug_overlay test_debug_telemetry test_tracking_log -- -j2
ctest --test-dir /tmp/nanotrack-occlusion-hold-build -R '^(interactive_debug_overlay|debug_telemetry|tracking_log|verify_board_schema)$' --output-on-failure
```

Expected: missing descriptors/events/schema fields fail.

- [ ] **Step 4: Implement overlay and safe drawing**

Map PASS to purple, `REJECT_POS` to gray, active probe to orange, and invalid to deep-red text only. Check `descriptor.drawable` before any `lround`/`cv::Rect`. Use compact indexed/multicolumn labels when candidate count exceeds available vertical rows; retain every diagnostic and keep full fields in logs.

- [ ] **Step 5: Implement telemetry, schema 7, and documentation**

Add the exact HOLD events/columns from the spec, one candidate event per diagnostic, metadata defaults, and Chinese field-source documentation. State that batch-frame boxes are asynchronous hypotheses and never direct control.

- [ ] **Step 6: Run focused tests and verify GREEN**

Run the Step 3 command. Expected: all focused tests pass.

- [ ] **Step 7: Commit Task 5**

```bash
git add rk3566/nanotrack_v3/include/interactive_debug_overlay.hpp rk3566/nanotrack_v3/src/interactive_debug_overlay.cpp rk3566/nanotrack_v3/src/main.cpp rk3566/nanotrack_v3/include/debug_telemetry.hpp rk3566/nanotrack_v3/src/debug_telemetry.cpp rk3566/nanotrack_v3/include/tracking_log.hpp rk3566/nanotrack_v3/src/tracking_log.cpp rk3566/nanotrack_v3/tests/test_verify_board_schema.sh RK3566_NANOTRACK_LONGTERM_CODE_FLOW.zh-CN.md rk3566/nanotrack_v3/README.md rk3566/nanotrack_v3/tests/test_interactive_debug_overlay.cpp rk3566/nanotrack_v3/tests/test_debug_telemetry.cpp rk3566/nanotrack_v3/tests/test_tracking_log.cpp
git commit -m "feat(rk3566): expose hold and detector diagnostics"
```

### Task 6: Full regression, RK3566 simple validation, and handoff record

**Files:**
- Modify: `docs/progress/board-validation.md`
- No large/generated artifacts added to Git.

**Interfaces:**
- Consumes: Tasks 1–5 complete feature and tests.
- Produces: Host regression evidence, native board binary/hash, bounded `girl_dance` HOLD and detector-overlay evidence, evidence boundaries, review result, pushed Debug branch commit.

- [ ] **Step 1: Configure a clean host build and run full CTest**

Run:

```bash
cmake -S rk3566/nanotrack_v3 -B /tmp/nanotrack-occlusion-hold-build -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF
cmake --build /tmp/nanotrack-occlusion-hold-build -- -j2
ctest --test-dir /tmp/nanotrack-occlusion-hold-build --output-on-failure
git diff --check
```

Expected: 100% tests pass and no whitespace errors.

- [ ] **Step 2: Request whole-feature code review**

Review from pre-feature commit `4f1a046` to current HEAD against the approved spec. Fix all Critical/Important findings with one TDD cycle; record deferred Minor findings explicitly.

- [ ] **Step 3: Synchronize only committed source changes to the board Debug worktree**

Before applying, record board branch/HEAD/status and preserve `config/interactive-debug.conf`, `log.txt`, models, videos, and build outputs. Confirm system time is corrected before incremental or clean build.

- [ ] **Step 4: Native-build and test on RK3566**

In `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry`, build with `-DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=ON`; run full CTest. If the known user-config `cli` assertion remains the only failure, also run `ctest -E '^cli$'` and report both results without overwriting the config.

- [ ] **Step 5: Perform bounded interactive `girl_dance` validation**

Use the approved NanoTrack models and current VisDrone9 detector identities. Verify one observed trigger has a trigger frame with anchor/recent, exactly N subsequent HOLD frames without them, frozen motion width/height, `control_valid=0`, and resumed recovery afterward. On a detector completion frame, capture all post-NMS candidates with source frame/age and PASS/REJECT/queued labels. If the video does not naturally produce one required condition, report it as unobserved rather than forcing a claim.

- [ ] **Step 6: Record identities and evidence boundaries**

Update `docs/progress/board-validation.md` with ISO timestamp, shared/board commits, binary/model/manifest/labels/video SHA-256, exact commands, test counts, retained log/screenshot paths, observed behavior, unobserved requirements, blockers, and `latest_handoff.available=false` unless the coordinator separately approves promotion.

- [ ] **Step 7: Commit, verify, and push the independent Debug branch**

```bash
git add docs/progress/board-validation.md
git commit -m "docs(board): record occlusion hold validation"
git status --short
git push origin codex/debug-runtime-config
git ls-remote --heads origin refs/heads/codex/debug-runtime-config
```

Expected: local and remote commit IDs match; `main` remains untouched.
