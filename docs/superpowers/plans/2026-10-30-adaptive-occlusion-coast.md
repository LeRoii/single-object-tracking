# RK3566 NanoTrack Adaptive Occlusion Coast Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the blind fixed occlusion HOLD with a 12-frame motion-controlled coast that detects severe or gradual occlusion early, probes NanoTrack every five frames, and exits early after a consistent two-probe recovery.

**Architecture:** Keep the public `OCCLUSION_HOLD` mode for compatibility, but move scale-reference and occlusion-evidence bookkeeping into a pure `OcclusionEvidenceTracker`. `LongTermController` owns state transitions, the inclusive 1–12 coast age, motion control authorization, and probe confirmation; runtime continues to execute the controller plan without allowing detector candidates to authorize control. Telemetry/schema changes make every trigger and probe gate observable before RK3566 validation.

**Tech Stack:** C++17, CMake/CTest, existing NanoTrack/RKNN runtime interfaces, OpenCV 4.10 on RK3566, strict `key=value` configuration, schema-versioned CSV/JSON telemetry.

**Spec:** `docs/superpowers/specs/2026-10-30-adaptive-occlusion-coast-design.md`

## Global Constraints

- Work only on `codex/adaptive-occlusion-coast` in `/home/u20/single-object-tracking/.worktrees/adaptive-occlusion-coast`; preserve unrelated worktrees and user files.
- Keep `TrackingMode::kOcclusionHold` and the string `OCCLUSION_HOLD`; its new behavior is motion coast, not a new public mode.
- The trigger frame is coast age 1; ages 1–12 may return `control_valid=true` from `hypothesis_source=MOTION`; age 13 may not.
- Search probes run at ages 5 and 10. A successful search probe schedules a confirm probe on the immediately following frame.
- `UNCERTAIN` runs NanoTrack every frame and never submits recovery detector work.
- HOLD/COAST never submits detector work. Pre-HOLD detector generations remain permanently ineligible.
- An unconfirmed probe cannot correct motion, update templates/reference history, replace the control bbox, or authorize control by itself.
- Detector candidates never directly produce `control_valid=true`; detector recovery retains class/position gates, anchor/recent verification, and same-candidate confirmation.
- Defaults are uncalibrated Debug values: hold `12`, probe interval `5`, gradual ratio `0.85`, consecutive gradual frames `2`, minimum reference samples `3`, shrink step ratio `0.97`, suspicion clear frames `2`, peak-margin drop `0.02`, motion residual `0.75`, and coast confirmation geometry `0.50/0.35/0.50/0.40`.
- Do not modify `docs/progress/STATUS.md` or `docs/progress/integration-contract.yaml`; this task owns only `docs/progress/board-validation.md`.
- Models, videos, logs, screenshots, build trees, and binaries remain outside Git; record paths, SHA-256, commands, and evidence boundaries only.
- Host tests/builds do not constitute RK3566 evidence. RK3566 functional observation does not constitute threshold calibration, production cross-compilation, or human gimbal-pod acceptance.

## Review Focus

- A target legitimately moving away or camera zoom changing both dimensions must not trigger gradual occlusion without an auxiliary anomaly; Task 2 adds uniform-scale and camera-like scale fixtures.
- A one-frame size spike followed by normal frames must clear suspicion after exactly two clean frames and resume stable-reference updates; Task 2 pins the freeze/unfreeze boundary.
- Trigger age, probes at 5/10, immediate confirms at 6/11, and revocation after age 12 must have no off-by-one control authorization; Tasks 3–4 test all boundaries.
- Explicit reinitialization/reselection during suspicion, coast, or pending confirmation must clear every old reference/candidate/counter; Tasks 3–5 test same-frame and next-frame resets.
- Dropped/irregular timestamps must not change frame-counted probe/control limits, while post-coast recovery mode still follows elapsed milliseconds; Tasks 3–5 mix frame IDs and timestamp gaps.

---

### Task 1: Configuration and shared coast contracts

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
- Consumes: Existing strict controller config parsing, `TrackingMode`, `SearchPlan`, and `ControllerOutput`.
- Produces: Exact configuration fields below; `OcclusionReferenceSource`, `OcclusionTriggerReason`, and `OcclusionProbeType` enums with `to_string(...)`; plan/output diagnostics used by Tasks 2–6.

- [ ] **Step 1: Add failing config-default, file-parse, and validation tests**

Assert these exact `ControllerConfig` fields/defaults and shipped visible/infrared values:

```cpp
int occlusion_probe_interval_frames = 5;
float occlusion_gradual_dimension_ratio = 0.85F;
int occlusion_gradual_consecutive_frames = 2;
int occlusion_min_reference_samples = 3;
float occlusion_shrink_step_ratio = 0.97F;
int occlusion_suspicion_clear_frames = 2;
float occlusion_peak_margin_drop = 0.02F;
float occlusion_motion_residual_threshold = 0.75F;
float occlusion_confirmation_iou = 0.50F;
float occlusion_confirmation_center_distance = 0.35F;
float occlusion_confirmation_max_area_log_change = 0.50F;
float occlusion_confirmation_max_aspect_log_change = 0.40F;
```

Validate: probe interval `[1,300]`; gradual consecutive/clear/min-reference counts `[1,60]`; all ratios in `(0,1]`; peak drop `>=0` and finite; motion residual `>0` and finite; confirmation IoU `(0,1]`; remaining confirmation deltas `>0` and finite. Retain parsing/metadata compatibility for `occlusion_trigger_consecutive_unreliable` but stop describing it as the coast trigger.

- [ ] **Step 2: Add failing shared-type tests**

Define and assert string conversions for:

```cpp
enum class OcclusionReferenceSource { kNone, kPartialStableMedian, kFullStableMedian };
enum class OcclusionTriggerReason { kNone, kSevereWidth, kSevereHeight,
  kGradualWidthTemplate, kGradualWidthPeak, kGradualWidthMotion,
  kGradualHeightTemplate, kGradualHeightPeak, kGradualHeightMotion };
enum class OcclusionProbeType { kNone, kSearch, kConfirm };
```

Extend `SearchPlan` with `occlusion_coast_age_frames`, `occlusion_probe_type`, and `motion_control_authorized`. Extend `ControllerOutput` with reference source, suspicion/clean/shrink counters, evidence booleans, trigger reason, coast age, probe type, probe confirmation hits/required, and motion-control authorization. Tests assign non-default values and prove copies preserve them.

- [ ] **Step 3: Run focused tests and observe RED**

Run:

```bash
cmake -S rk3566/nanotrack_v3 -B /tmp/nanotrack-adaptive-coast-build -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF
cmake --build /tmp/nanotrack-adaptive-coast-build --target test_controller_support test_long_term_controller -- -j2
ctest --test-dir /tmp/nanotrack-adaptive-coast-build -R '^(controller_support|long_term_controller)$' --output-on-failure
```

Expected: compile/assertion failures for missing config fields/enums/plan-output fields.

- [ ] **Step 4: Implement config and shared contracts**

Add exact fields, parser keys, validation, shipped values, enum conversions, and zero/none defaults. Do not change controller behavior in this task.

- [ ] **Step 5: Re-run focused and full host tests**

Run the Step 3 command, then:

```bash
cmake --build /tmp/nanotrack-adaptive-coast-build -- -j2
ctest --test-dir /tmp/nanotrack-adaptive-coast-build --output-on-failure
```

Expected: focused tests and the existing full suite pass.

- [ ] **Step 6: Commit Task 1**

```bash
git add rk3566/nanotrack_v3/include/controller_config.hpp rk3566/nanotrack_v3/src/controller_config.cpp rk3566/nanotrack_v3/config/visible-default.conf rk3566/nanotrack_v3/config/infrared-default.conf rk3566/nanotrack_v3/include/long_term_types.hpp rk3566/nanotrack_v3/src/long_term_types.cpp rk3566/nanotrack_v3/include/long_term_controller.hpp rk3566/nanotrack_v3/tests/test_controller_support.cpp rk3566/nanotrack_v3/tests/test_long_term_controller.cpp
git commit -m "feat(rk3566): define adaptive occlusion coast contracts"
```

### Task 2: Pure occlusion-evidence and stable-reference tracker

**Files:**
- Create: `rk3566/nanotrack_v3/include/occlusion_evidence.hpp`
- Create: `rk3566/nanotrack_v3/src/occlusion_evidence.cpp`
- Create: `rk3566/nanotrack_v3/tests/test_occlusion_evidence.cpp`
- Modify: `rk3566/nanotrack_v3/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 1 `ControllerConfig`, `BBox`, `ResponseMetrics`, reference/trigger enums.
- Produces:

```cpp
struct OcclusionEvidenceInput {
  BBox observation_bbox;
  BBox motion_bbox;
  ResponseMetrics selected_metrics;
  bool reliable_accepted;
  bool template_disagreement;
  bool detector_hypothesis;
};

struct OcclusionEvidenceDecision {
  bool suspicion_active;
  bool suspicion_started;
  bool suspicion_cleared;
  bool severe_evidence;
  bool gradual_evidence;
  bool template_evidence;
  bool peak_margin_evidence;
  bool motion_residual_evidence;
  bool triggered;
  OcclusionReferenceSource reference_source;
  OcclusionTriggerReason trigger_reason;
  std::optional<float> reference_width, reference_height;
  std::optional<float> width_ratio, height_ratio;
  int width_shrink_frames, height_shrink_frames, clean_frames;
};

class OcclusionEvidenceTracker {
 public:
  explicit OcclusionEvidenceTracker(const ControllerConfig& config);
  void reset();
  OcclusionEvidenceDecision observe(const OcclusionEvidenceInput& input);
};
```

`observe()` owns stable width/height/peak-margin histories, suspicion freeze/clear, partial/full medians, and evidence decisions. It does not own tracking modes, motion correction, templates, or control authorization.

- [ ] **Step 1: Add failing reference and suspicion tests**

Test names/assertions:

- `partial_reference_requires_three_stable_samples`: 0–2 samples produce `kNone`; the third produces `kPartialStableMedian`; the eighth produces `kFullStableMedian` with independent even-window medians.
- `first_real_shrink_freezes_reference`: a `0.96` step starts suspicion and later smaller reliable boxes do not change the frozen median.
- `two_clean_frames_clear_suspicion`: one clean frame stays frozen; exactly two clear suspicion and resume stable history from clean observations.
- `single_noise_band_change_does_not_start_suspicion`: ratios greater than `0.97` do not freeze history.
- `reset_clears_reference_suspicion_and_counters`.

- [ ] **Step 2: Add failing severe/gradual evidence tests**

Cover width and height independently:

- Severe ratio exactly `0.70` triggers even when `reliable_accepted=true`; `0.7001` does not.
- Gradual ratio `<=0.85` plus two `<=0.97` same-axis steps does not trigger without an auxiliary signal.
- Each of template disagreement, absolute peak-margin median drop `>=0.02`, and normalized motion residual `>=0.75` independently completes gradual trigger and yields the exact reason enum.
- Uniform width/height shrink representing target moving away does not trigger without an auxiliary signal.
- Stable-size fast translation and camera-like uniform scaling inside the noise/trend rules do not trigger.
- Detector hypotheses never trigger or update stable history.
- Non-finite/invalid boxes or metrics throw `std::invalid_argument` without mutating state.

- [ ] **Step 3: Run the new test and observe RED**

Run:

```bash
cmake --build /tmp/nanotrack-adaptive-coast-build --target test_occlusion_evidence -- -j2
/tmp/nanotrack-adaptive-coast-build/test_occlusion_evidence
```

Expected: target/source/test missing or assertions fail.

- [ ] **Step 4: Implement the pure tracker**

Use independent deques for stable widths, heights, and selected peak margins; compute medians using the existing even-window convention. Calculate motion residual with the controller's normalized-center-distance convention, moved to a shared helper only if needed. Evaluate severe before gradual; deterministic reason priority is width before height, then template before peak before motion. When suspicion starts, snapshot the reference and exclude the current observation from stable history. On two clean frames, clear frozen state and seed new stable history in chronological order.

- [ ] **Step 5: Run focused and full tests**

Run Step 3, then full CTest. Expected: all pass and `git diff --check` is clean.

- [ ] **Step 6: Commit Task 2**

```bash
git add rk3566/nanotrack_v3/include/occlusion_evidence.hpp rk3566/nanotrack_v3/src/occlusion_evidence.cpp rk3566/nanotrack_v3/tests/test_occlusion_evidence.cpp rk3566/nanotrack_v3/CMakeLists.txt
git commit -m "feat(rk3566): detect adaptive occlusion evidence"
```

### Task 3: Inclusive 12-frame motion coast and control authorization

**Files:**
- Modify: `rk3566/nanotrack_v3/include/long_term_controller.hpp`
- Modify: `rk3566/nanotrack_v3/src/long_term_controller.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_long_term_controller.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_long_term_replay.cpp`

**Interfaces:**
- Consumes: Task 2 `OcclusionEvidenceTracker::observe/reset`, Task 1 plan/output fields.
- Produces: Controller transition where the trigger frame is coast age 1, uses frozen-scale motion output, and authorizes exactly ages 1–12; `begin_frame()` plans non-probe coast frames with `run_tracker=false` and no detector request.

- [ ] **Step 1: Add failing direct-trigger and commit-order tests**

Build 8 stable accepted observations, then a `RELIABLE` severe-width observation. Assert on the trigger frame:

- input plan mode was `TRACKING`, output mode is `OCCLUSION_HOLD`;
- `occlusion_coast_age_frames==1`, trigger reason is severe width;
- output bbox equals frozen-scale motion prediction, not the abnormal NanoTrack bbox;
- `accepted=false`, `control_valid=true`, `motion_control_authorized=true`, source is motion;
- motion position/velocity, recent template, stable reference, and template-update state were not corrected by the abnormal observation;
- `loss_start_ms` equals trigger timestamp; detector generation increments and pending candidates/confirmations clear.

Repeat for gradual trigger from `TRACKING` and `UNCERTAIN`. Assert ordinary one-frame low quality still enters `UNCERTAIN` with `run_tracker=true` next frame.

- [ ] **Step 2: Add failing 1–12 boundary, timestamp, and reset tests**

Assert ages 1 through 12 each output frozen-size motion and `control_valid=true`; age 13 has no coast authorization and returns the elapsed-time mode. Use irregular timestamp gaps to prove age/probe limits are frame-counted while age-13 mode is timestamp-derived. Explicit initialize/reselect at trigger, age 6, and pending loss clears evidence, coast age, frozen scale, loss clock, candidates, and authorization.

Add invalid-motion fixtures (non-finite/out-of-frame after safe clipping cannot occur silently): invalid prediction must revoke coast control, produce `control_valid=false`, and leave detector unable to authorize control.

- [ ] **Step 3: Run controller/replay tests and observe RED**

```bash
cmake --build /tmp/nanotrack-adaptive-coast-build --target test_long_term_controller test_long_term_replay -- -j2
ctest --test-dir /tmp/nanotrack-adaptive-coast-build -R '^(long_term_controller|long_term_replay)$' --output-on-failure
```

Expected: new transition/age/control assertions fail.

- [ ] **Step 4: Integrate evidence before acceptance side effects**

Construct the evidence input after anchor/recent selection but before `motion_.correct`, `record_reliable_scale`, or template actions. If evidence triggers, suppress normal acceptance side effects, freeze the evidence reference, set age 1, initialize the loss clock, invalidate detector eligibility, and return motion control output. Replace the old `consecutive_unreliable + size ratio` trigger; retain deprecated config serialization only.

For subsequent non-probe ages, `begin_frame()` returns motion prediction, frozen scale, `run_tracker=false`, no detector request, and authorization true. After age 12, unfreeze only when confirmed recovery or explicit initialize occurs; recovery searches must continue using the frozen reference rather than accepting a collapsed scale.

- [ ] **Step 5: Run focused and full tests**

Run Step 3 and full CTest. Expected: all pass.

- [ ] **Step 6: Commit Task 3**

```bash
git add rk3566/nanotrack_v3/include/long_term_controller.hpp rk3566/nanotrack_v3/src/long_term_controller.cpp rk3566/nanotrack_v3/tests/test_long_term_controller.cpp rk3566/nanotrack_v3/tests/test_long_term_replay.cpp
git commit -m "feat(rk3566): authorize bounded motion coast"
```

### Task 4: Five-frame NanoTrack probes and early recovery

**Files:**
- Modify: `rk3566/nanotrack_v3/include/long_term_controller.hpp`
- Modify: `rk3566/nanotrack_v3/src/long_term_controller.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_long_term_controller.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_long_term_replay.cpp`

**Interfaces:**
- Consumes: Task 3 inclusive coast age and Task 1 `OcclusionProbeType`/confirmation config.
- Produces: Search probes at ages 5/10, immediate confirm probes, a stored `CoastProbeCandidate {frame_id, bbox}`, strict same-target confirmation, and early `TRACKING` recovery.

- [ ] **Step 1: Add failing probe-schedule tests**

Assert ages 1–12 produce:

```text
1-4 NONE/run_tracker=false
5   SEARCH/run_tracker=true
6-9 NONE/run_tracker=false (when search 5 misses)
10  SEARCH/run_tracker=true
11-12 NONE/run_tracker=false (when search 10 misses)
```

When age-5 search succeeds, age 6 is `CONFIRM/run_tracker=true`; when confirm fails, ages 7–9 are none and age 10 is search. When age-10 search succeeds, age 11 is confirm. Detector request remains false on every age.

- [ ] **Step 2: Add failing probe-isolation and recovery tests**

For search probes, assert anchor/recent must both be reliable, template IoU passes, and selected bbox passes motion center/scale gates. A successful search stores one candidate but output remains the motion bbox with motion control authorized; it cannot correct motion, update recent/anchor template, update reference, or set `accepted=true`.

For confirm probes, assert the new bbox passes the same per-frame gates plus cross-frame candidate gates using exact defaults `0.50/0.35/0.50/0.40`. Two different nearby targets fail. A valid confirm:

- sets accepted/reacquired/control valid;
- outputs the confirmed NanoTrack bbox;
- reinitializes motion and stable reference;
- returns `TRACKING` immediately and clears coast/probe/detector state.

Test reinitialization between search and confirm, confirmation at age 11, and a search result arriving with mismatched frame/token; all stale candidates are discarded safely.

- [ ] **Step 3: Run focused tests and observe RED**

Run the Task 3 focused command. Expected: schedule and recovery assertions fail.

- [ ] **Step 4: Implement probe scheduling and confirmation**

In `begin_frame()`, compute probe type before the generic HOLD early return. Search when `age % interval == 0`; pending confirmation overrides interval on the next frame. Both use motion prediction as center and frozen reference as target size/search basis.

In `finish_frame()`, evaluate probe observations without entering ordinary acceptance/template-update code. Store only the selected search candidate. Confirm against both motion and stored candidate; on failure clear only the candidate and stay in coast. On success execute one explicit reacquisition path shared with ordinary validated recovery.

- [ ] **Step 5: Run focused and full tests**

Expected: controller/replay and full CTest pass.

- [ ] **Step 6: Commit Task 4**

```bash
git add rk3566/nanotrack_v3/include/long_term_controller.hpp rk3566/nanotrack_v3/src/long_term_controller.cpp rk3566/nanotrack_v3/tests/test_long_term_controller.cpp rk3566/nanotrack_v3/tests/test_long_term_replay.cpp
git commit -m "feat(rk3566): probe during occlusion coast"
```

### Task 5: Runtime execution and detector-safety integration

**Files:**
- Modify: `rk3566/nanotrack_v3/src/long_term_runtime.cpp`
- Modify: `rk3566/nanotrack_v3/include/long_term_runtime.hpp` only if new observable counters are required
- Test: `rk3566/nanotrack_v3/tests/test_long_term_runtime.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_yolo_detector_adapter.cpp`

**Interfaces:**
- Consumes: Controller plans from Tasks 3–4.
- Produces: Runtime call counts and result fields proving non-probe coast skips NanoTrack, probe frames run it, detector submit stays zero, and late batches remain diagnostic-only.

- [ ] **Step 1: Add failing runtime call-count tests**

Drive a deterministic trigger through ages 1–12. Assert tracker inference occurs on the trigger, ages 5/10, and an immediate confirm only; all other coast ages set `tracker_skipped=true`. Assert detector submit count is zero throughout `UNCERTAIN` and coast while poll remains non-blocking.

- [ ] **Step 2: Add failing safety/reset/error tests**

- Deliver a pre-HOLD detector batch during ages 5, 12, and after age 12: diagnostics survive with `HOLD_INVALIDATED`, but queue/active candidate/reacquisition/control never derive from it.
- Throw tracker inference on search/confirm probe: frame cleanup leaves no open-frame poison; motion control is revoked for that failed frame and no candidate survives.
- Reinitialize in the same decoded frame after a pending confirm: old probe and detector identities cannot enter the new session.
- Verify `UNCERTAIN` still invokes tracker every processed frame with irregular timestamp gaps.

- [ ] **Step 3: Run runtime/adapter tests and observe RED**

```bash
cmake --build /tmp/nanotrack-adaptive-coast-build --target test_long_term_runtime test_yolo_detector_adapter -- -j2
ctest --test-dir /tmp/nanotrack-adaptive-coast-build -R '^(long_term_runtime|yolo_detector_adapter)$' --output-on-failure
```

Expected: new coast/probe call-count and cleanup assertions fail.

- [ ] **Step 4: Implement minimal runtime integration**

Continue honoring `plan.run_tracker`; no special runtime-owned scheduler is allowed. Preserve detector poll for diagnostics and submit only when `plan.request_detector`. If tracker throws on a control-authorized coast probe, abort the frame and surface the error rather than emitting an unverified valid control result.

- [ ] **Step 5: Run focused and full tests**

Expected: focused and full CTest pass.

- [ ] **Step 6: Commit Task 5**

```bash
git add rk3566/nanotrack_v3/src/long_term_runtime.cpp rk3566/nanotrack_v3/include/long_term_runtime.hpp rk3566/nanotrack_v3/tests/test_long_term_runtime.cpp rk3566/nanotrack_v3/tests/test_yolo_detector_adapter.cpp
git commit -m "test(rk3566): integrate adaptive coast runtime safety"
```

### Task 6: Schema, telemetry, overlay, and operator documentation

**Files:**
- Modify: `rk3566/nanotrack_v3/include/tracking_log.hpp`
- Modify: `rk3566/nanotrack_v3/src/tracking_log.cpp`
- Modify: `rk3566/nanotrack_v3/include/debug_telemetry.hpp` if event state is needed
- Modify: `rk3566/nanotrack_v3/src/debug_telemetry.cpp`
- Modify: `rk3566/nanotrack_v3/include/interactive_debug_overlay.hpp` if descriptors change
- Modify: `rk3566/nanotrack_v3/src/interactive_debug_overlay.cpp`
- Modify: `rk3566/nanotrack_v3/scripts/verify_board.sh`
- Modify: `rk3566/nanotrack_v3/tests/test_tracking_log.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_debug_telemetry.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_interactive_debug_overlay.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_verify_board_schema.sh`
- Modify: `rk3566/nanotrack_v3/README.md`
- Modify: `RK3566_NANOTRACK_LONGTERM_CODE_FLOW.zh-CN.md`

**Interfaces:**
- Consumes: Task 1 output diagnostics and Tasks 3–5 observed behavior.
- Produces: Schema 9 CSV/metadata, structured trigger/probe events, distinct motion/probe overlays, verifier requirements, and Chinese operator field documentation.

- [ ] **Step 1: Add failing schema and serialization tests**

Upgrade schema 8 to 9. Require columns/metadata for reference source, suspicion state/counters, severe/gradual/template/peak/motion evidence, trigger reason, coast age, probe type, confirmation hits/required, and motion-control authorization. Test enum strings, optionals, non-finite rejection, trigger age 1, search 5/10, confirm 6/11, recovery, and timeout exit.

- [ ] **Step 2: Add failing event and overlay tests**

Events:

- `occlusion_suspicion_start/clear`
- `occlusion_coast_enter/tick/exit`
- `occlusion_probe_search`
- `occlusion_probe_confirm`
- `occlusion_probe_rejected`
- `occlusion_reacquired`

Each event includes frame, coast age, reference source/size, evidence gates, motion/probe/control boxes where applicable, and control authorization. Overlay tests require motion control to remain green and labeled `CTRL MOTION COAST`; search/confirm probe boxes use distinct non-green colors and gate labels; detector candidates retain their existing colors and can never be labeled accepted control.

- [ ] **Step 3: Run focused tests and observe RED**

```bash
cmake --build /tmp/nanotrack-adaptive-coast-build --target test_tracking_log test_debug_telemetry test_interactive_debug_overlay -- -j2
ctest --test-dir /tmp/nanotrack-adaptive-coast-build -R '^(tracking_log|debug_telemetry|interactive_debug_overlay|verify_board_schema)$' --output-on-failure
```

Expected: schema/event/overlay assertions fail.

- [ ] **Step 4: Implement serialization, events, overlay, and verifier**

Populate presentation only from `RuntimeFrameResult`/`ControllerOutput`; do not recompute trigger or recovery decisions in logging/UI code. Keep invalid boxes text-only. Update `verify_board.sh` and its fixture to schema 9.

- [ ] **Step 5: Update operator documentation**

Document every new field's meaning and source, inclusive 12-frame semantics, probe schedule, control source, detector exclusions, and `calibrated=false`. Clearly state that `control_valid=true` in HOLD is bounded motion prediction, not a current-frame visual observation.

- [ ] **Step 6: Run focused and full tests**

Expected: focused and full CTest pass; `git diff --check` passes.

- [ ] **Step 7: Commit Task 6**

```bash
git add rk3566/nanotrack_v3/include/tracking_log.hpp rk3566/nanotrack_v3/src/tracking_log.cpp rk3566/nanotrack_v3/include/debug_telemetry.hpp rk3566/nanotrack_v3/src/debug_telemetry.cpp rk3566/nanotrack_v3/include/interactive_debug_overlay.hpp rk3566/nanotrack_v3/src/interactive_debug_overlay.cpp rk3566/nanotrack_v3/scripts/verify_board.sh rk3566/nanotrack_v3/tests/test_tracking_log.cpp rk3566/nanotrack_v3/tests/test_debug_telemetry.cpp rk3566/nanotrack_v3/tests/test_interactive_debug_overlay.cpp rk3566/nanotrack_v3/tests/test_verify_board_schema.sh rk3566/nanotrack_v3/README.md RK3566_NANOTRACK_LONGTERM_CODE_FLOW.zh-CN.md
git commit -m "feat(rk3566): expose adaptive coast diagnostics"
```

### Task 7: Full regression, RK3566 validation, and owned handoff record

**Files:**
- Modify: `docs/progress/board-validation.md`
- No large/generated artifact enters Git.

**Interfaces:**
- Consumes: Tasks 1–6 committed source and exact approved model/config identities.
- Produces: Clean host evidence, isolated RK3566 native build/binary hash, bounded functional observations for both available videos, evidence boundaries, and an owned progress commit. It does not change the public contract.

- [ ] **Step 1: Run a fresh clean host build and full suite**

```bash
cmake -S rk3566/nanotrack_v3 -B /tmp/nanotrack-adaptive-coast-final -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF
cmake --build /tmp/nanotrack-adaptive-coast-final -- -j2
ctest --test-dir /tmp/nanotrack-adaptive-coast-final --output-on-failure
git diff --check
```

Expected: configure/build succeeds, all tests pass, no whitespace errors.

- [ ] **Step 2: Review source and artifact identities before board sync**

Record branch HEAD, merge base, source patch SHA-256, NanoTrack template/search/head/manifest SHA-256, detector RKNN/manifest/labels SHA-256, both video SHA-256 values, and controller/Debug config SHA-256. Stop and report any mismatch against the coordinator-approved identities; do not silently substitute artifacts.

- [ ] **Step 3: Create an isolated board worktree and native build**

Do not modify the board's existing user checkout. Apply the exact committed patch into a new board worktree under `/home/cat/Project/SOT/`, confirm clean status, correct board time if necessary, and configure a fresh build with `ENABLE_RKNN_RUNTIME=ON`, OpenCV 4.10, and RKNN Runtime 2.3.2. Run full board CTest and hash the AArch64 executable.

- [ ] **Step 4: Run bounded `girl_dance` functional observation**

Use the approved initial ROI and current models. Retain command/log/CSV/metadata/screenshots. Attempt to observe:

-遮挡 suspicion and trigger reason;
- trigger frame as age 1 with motion control;
- search probes at age 5/10 and any immediate confirm;
- early recovery or explicit timeout;
- no detector request during UNCERTAIN/HOLD;
- no detector/probe direct control authorization.

If the video does not naturally produce a condition, record it as unobserved rather than forcing thresholds or claiming PASS.

- [ ] **Step 5: Run bounded `tracking-test-v1.mp4` functional observation**

Use `/home/cat/Project/SOT/data/tracking-test-v1.mp4`. Select documented person/vehicle ROIs only after inspecting the frame. This video has no ground truth, so report qualitative state/control/probe behavior only; do not report accuracy, recovery rate, identity correctness, or threshold calibration.

- [ ] **Step 6: Update only the owned board progress file**

Append an ISO-8601 `+08:00` section to `docs/progress/board-validation.md` with exact commits, paths, hashes, commands, observed results, unobserved requirements, blockers, next action, and Handoff. Keep `functional_pass_promotion_held` and `latest_handoff.available=false` unless the coordinator separately approves every promotion gate. Do not modify `STATUS.md` or `integration-contract.yaml`.

- [ ] **Step 7: Commit Task 7 and verify the branch**

```bash
git add docs/progress/board-validation.md
git commit -m "docs(board): record adaptive occlusion coast validation"
git status --short --untracked-files=all
```

Expected: clean worktree; commit contains only the owned progress file. Report the commit to the coordinating chat. No real gimbal-pod acceptance claim is permitted without explicit human evidence.
