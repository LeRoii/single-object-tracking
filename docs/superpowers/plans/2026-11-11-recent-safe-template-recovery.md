# Recent-Safe Template Recovery Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace permanent anchor/recent matching with one active recent template plus a dormant previous-generation safe snapshot, add tentative `SUSPECTED` control, and allow NanoTrack recovery without mandatory detector confirmation.

**Architecture:** A standalone recent/safe template state owns transactional feature rotation while the controller owns tentative observation and bounded multi-source recovery state. Normal tracking executes recent only; staged probation executes staged+recent; suspected/search executes recent+safe. Runtime applies controller-requested template transitions and acknowledges them before the next frame, so tracker and controller cannot retain half-applied probation state.

**Tech Stack:** C++17, CMake/CTest, existing RKNN/OpenCV runtime, Python evaluator scripts only for offline evidence.

**Spec:** `docs/superpowers/specs/2026-11-11-recent-safe-template-recovery-design.md`

## Global Constraints

- Normal `TRACKING` runs one template head; at most two heads run in probation, `SUSPECTED`, or `SEARCHING`.
- `SUSPECTED` outputs the real recent observation through `rect[0]` and drives bounded control, but must not update templates, motion state, response baselines, or stable counters.
- NanoTrack-only recovery uses exactly 2-of-3 confirmation; detector is an optional hypothesis source and never directly authorizes control.
- Template feature rotation is `safe <- recent, recent <- staged`; failed preparation discards staged and preserves both committed features.
- Old `search_reacquisition_requires_detector` and `template_bridge_*` keys fail fast; the new config explicitly selects `recent_safe_v1` and `unified_candidates_v1`.
- Existing `car_001` evidence remains a regression gate; `car_002` draft cannot provide formal IoU or threshold calibration before human confirmation.
- Do not modify `docs/progress/STATUS.md` or `docs/progress/integration-contract.yaml`; only the board-validation owner may update `docs/progress/board-validation.md`.

## Review Focus

- A template feature extraction or transition callback throws: the next frame sees no staged template or controller probation state (Task 2 and Task 5 tests).
- A gradual viewpoint/scale transition weakens safe while recent remains temporally reliable: tracking continues and rolling trusted geometry, not initialization size, bounds scale (Task 3 tests).
- A stable-size occluder preserves raw score but degrades relative PSR/APCE/peak margin: the baseline freezes and the controller enters tentative `SUSPECTED` without learning the occluder (Task 3 tests).
- Recent, safe, and detector propose conflicting same-class objects: no source wins by detector confidence and recovery remains searching (Task 4 tests).
- Stale/gap/reset/generation changes arrive with partially confirmed candidates: every candidate and transition is revoked without replay on a later frame (Task 4 and Task 5 tests).

---

### Task 1: Versioned fail-fast controller configuration

**Files:**
- Modify: `rk3566/nanotrack_v3/include/controller_config.hpp`
- Modify: `rk3566/nanotrack_v3/src/controller_config.cpp`
- Create: `rk3566/nanotrack_v3/config/visible-recent-safe-v1.conf`
- Test: `rk3566/nanotrack_v3/tests/test_controller_support.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_cli.cpp`

**Interfaces:**
- Produces: `TemplatePolicy::kRecentSafeV1`, `RecoveryPolicy::kUnifiedCandidatesV1`, `ControllerConfig::template_policy`, and `ControllerConfig::recovery_policy`.
- Produces: validated candidate-only fields `suspicion_enter_ms`, `suspicion_clear_ms`,
  `response_psr_drop_ratio`, and `response_apce_drop_ratio`; the new profile remains
  `calibrated=false` until Task 6 selects or holds defaults from evidence.
- Produces: loader errors naming each deprecated key and the required replacement profile.
- Consumes: existing controller scalar gates; no behavior change outside config parsing in this task.

- [ ] **Step 1: Write failing parser tests**

Add tests asserting that a complete new profile loads only with
`template_policy=recent_safe_v1` and `recovery_policy=unified_candidates_v1`; missing policy keys and each deprecated key (`search_reacquisition_requires_detector`, `template_bridge_min_stable_frames`, `template_bridge_max_duration_ms`) throw with a migration-specific message. Assert historical config files remain byte-unchanged.

- [ ] **Step 2: Run the focused tests and verify RED**

Run: `cmake --build /tmp/nanotrack-occ-c010-host --target test_controller_support test_cli -j4 && ctest --test-dir /tmp/nanotrack-occ-c010-host -R '^(controller_support|cli)$' --output-on-failure`

Expected: FAIL because policy keys/enums and the new profile do not exist.

- [ ] **Step 3: Implement strict policy parsing**

Add the two enums and `to_string` helpers. `load_controller_config()` must track explicit policy-key presence, reject deprecated keys immediately, and require both new policy values for file-backed configs. `default_controller_config()` supplies the new policies for programmatic test fixtures. Keep old config files for historical binaries; create the new profile by copying still-applicable calibrated scalar values and omitting deprecated keys.

- [ ] **Step 4: Re-run focused tests and verify GREEN**

Run the Step 2 command. Expected: both tests PASS.

- [ ] **Step 5: Commit**

```bash
git add rk3566/nanotrack_v3/include/controller_config.hpp rk3566/nanotrack_v3/src/controller_config.cpp rk3566/nanotrack_v3/config/visible-recent-safe-v1.conf rk3566/nanotrack_v3/tests/test_controller_support.cpp rk3566/nanotrack_v3/tests/test_cli.cpp
git commit -m "feat: require recent-safe controller profile"
```

### Task 2: Transactional recent/safe template state

**Files:**
- Create: `rk3566/nanotrack_v3/include/recent_safe_templates.hpp`
- Create: `rk3566/nanotrack_v3/src/recent_safe_templates.cpp`
- Create: `rk3566/nanotrack_v3/tests/test_recent_safe_templates.cpp`
- Modify: `rk3566/nanotrack_v3/CMakeLists.txt`
- Modify: `rk3566/nanotrack_v3/include/long_term_types.hpp`
- Modify: `rk3566/nanotrack_v3/src/long_term_types.cpp`
- Modify: `rk3566/nanotrack_v3/include/short_term_tracker.hpp`
- Modify: `rk3566/nanotrack_v3/include/nanotrack_v3.hpp`
- Modify: `rk3566/nanotrack_v3/src/nanotrack_v3.cpp`

**Interfaces:**
- Produces: `TemplateKind::{kRecent,kSafe,kStaged}` and `TemplateRunMode::{kRecentOnly,kStagedAndRecent,kRecentAndSafe}`.
- Produces: `RecentSafeTemplates::initialize(Feature)`, `prepare(Feature,uint64_t)`, `commit(uint64_t) noexcept`, `abort(uint64_t) noexcept`, and const feature accessors.
- Produces: `ShortTermTracker::prepare_recent(image,bbox,transaction_id)`, `commit_recent(transaction_id) noexcept`, and `abort_recent(transaction_id) noexcept`.
- Consumes later: `SearchPlan::template_run_mode` from Task 3; until then tests exercise the standalone state.

- [ ] **Step 1: Write failing standalone state tests**

Cover initialization alias/reuse, independent staged storage, wrong transaction rejection before mutation, abort preserving recent/safe, commit performing `safe <- recent, recent <- staged`, repeated commit/abort idempotent rejection, and exception-before-prepare leaving state unchanged. Add a fake head counter asserting each `TemplateRunMode` requests exactly 1/2/2 feature evaluations and recent==safe may reuse one result.

- [ ] **Step 2: Run the new test and verify RED**

Run: `cmake -S rk3566/nanotrack_v3 -B /tmp/nanotrack-occ-c010-host -DENABLE_RKNN_RUNTIME=OFF -DBUILD_TESTING=ON && cmake --build /tmp/nanotrack-occ-c010-host --target test_recent_safe_templates -j4`

Expected: target or headers are missing.

- [ ] **Step 3: Implement the isolated template state and CMake target**

Store three independent `std::vector<float>` slots plus one pending transaction id. `prepare()` validates and copies/moves staged without touching committed slots; `commit()` uses only noexcept vector swaps; `abort()` clears staged. Put run-mode selection in an RKNN-independent helper so host tests count requested heads.

- [ ] **Step 4: Adapt NanoTrackV3 storage and inference**

Replace `anchor_feature_`, mutable recent backup, and `recent_matches_anchor_` with `RecentSafeTemplates`. Use the run mode to emit observations tagged recent/safe/staged, and replace the permanent initial-size clamp with limits relative to the last committed trusted bbox plus existing per-second scale-rate protection. Do not change controller selection yet.

- [ ] **Step 5: Run focused and full host tests**

Run: `cmake --build /tmp/nanotrack-occ-c010-host -j4 && ctest --test-dir /tmp/nanotrack-occ-c010-host -R '^(recent_safe_templates|long_term_runtime|long_term_controller)$' --output-on-failure`

Expected: all focused tests PASS; update compile-only fakes for the new interface without weakening their assertions.

- [ ] **Step 6: Commit**

```bash
git add rk3566/nanotrack_v3/CMakeLists.txt rk3566/nanotrack_v3/include/recent_safe_templates.hpp rk3566/nanotrack_v3/src/recent_safe_templates.cpp rk3566/nanotrack_v3/tests/test_recent_safe_templates.cpp rk3566/nanotrack_v3/include/long_term_types.hpp rk3566/nanotrack_v3/src/long_term_types.cpp rk3566/nanotrack_v3/include/short_term_tracker.hpp rk3566/nanotrack_v3/include/nanotrack_v3.hpp rk3566/nanotrack_v3/src/nanotrack_v3.cpp
git commit -m "feat: add transactional recent-safe templates"
```

### Task 3: Tentative SUSPECTED state and trusted response baseline

**Files:**
- Create: `rk3566/nanotrack_v3/include/response_baseline.hpp`
- Create: `rk3566/nanotrack_v3/src/response_baseline.cpp`
- Create: `rk3566/nanotrack_v3/tests/test_response_baseline.cpp`
- Modify: `rk3566/nanotrack_v3/include/long_term_types.hpp`
- Modify: `rk3566/nanotrack_v3/include/long_term_controller.hpp`
- Modify: `rk3566/nanotrack_v3/src/long_term_controller.cpp`
- Modify: `rk3566/nanotrack_v3/src/motion_estimator.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_long_term_controller.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_tracker_math.cpp`
- Modify: `rk3566/nanotrack_v3/CMakeLists.txt`

**Interfaces:**
- Produces: `TrackingMode::kSuspected`, `TrackingPhase::kSuspected`, `SearchPlan::template_run_mode`, and output booleans `observation_present` and `trusted_for_state_update`.
- Produces: `ResponseBaseline::observe_trusted(ResponseMetrics)`, `evaluate(ResponseMetrics)`, `freeze()`, and `reset_after_recovery()` using median windows sized by existing occlusion config.
- Consumes: recent/safe run modes and observations from Task 2.

- [ ] **Step 1: Write failing baseline tests**

Assert cold start before `occlusion_min_reference_samples`, median over exactly `occlusion_reference_window` trusted samples, frozen baseline ignoring suspected/search/recovery samples, PSR/APCE relative-drop flags, absolute peak-margin-drop flag, and post-recovery cold restart.

- [ ] **Step 2: Write failing controller tests**

Add fixtures for: stable-size high raw score with sustained relative degradation entering `kSuspected`; suspected output carrying recent tracking/control bbox with `accepted=false`, `observation_present=true`, `trusted_for_state_update=false`, and `control_valid=true`; no mutation of motion/template/baseline/stable counters; clean hysteresis returning to tracking; sustained/failed recent escalating to searching; control authorization expiring from the last trusted timestamp. Add gradual viewpoint/scale observations where safe weakens but recent remains continuous and accepted.

- [ ] **Step 3: Run focused tests and verify RED**

Run: `cmake --build /tmp/nanotrack-occ-c010-host --target test_response_baseline test_long_term_controller test_tracker_math -j4 && ctest --test-dir /tmp/nanotrack-occ-c010-host -R '^(response_baseline|long_term_controller|tracker_math)$' --output-on-failure`

Expected: missing state/types and failing permanent-size assumptions.

- [ ] **Step 4: Implement baseline and suspected semantics**

Update baselines only on trusted ordinary tracking frames. In suspected mode run recent+safe, select recent for `tracking_bbox`/`rect[0]`, constrain `control_bbox` by existing center/scale rate envelopes, and keep all learning writes frozen. Track last trusted timestamp separately from search start. Use test-provided relative-drop and duration values; keep the shipping profile marked calibration-pending until Task 6 evidence selects defaults.

- [ ] **Step 5: Replace permanent initialization scale clamp**

Reference scale gates to the last trusted bbox and existing accepted scale history. Preserve finite positive bounds, per-frame step control, and per-second rate limits; test long gradual scale change, sudden collapse, and frame-edge clipping.

- [ ] **Step 6: Run focused tests and commit**

Run the Step 3 command. Expected: PASS.

```bash
git add rk3566/nanotrack_v3/CMakeLists.txt rk3566/nanotrack_v3/include/response_baseline.hpp rk3566/nanotrack_v3/src/response_baseline.cpp rk3566/nanotrack_v3/tests/test_response_baseline.cpp rk3566/nanotrack_v3/include/long_term_types.hpp rk3566/nanotrack_v3/include/long_term_controller.hpp rk3566/nanotrack_v3/src/long_term_controller.cpp rk3566/nanotrack_v3/src/motion_estimator.cpp rk3566/nanotrack_v3/tests/test_long_term_controller.cpp rk3566/nanotrack_v3/tests/test_tracker_math.cpp
git commit -m "feat: add tentative suspected tracking state"
```

### Task 4: Bounded unified recovery candidates

**Files:**
- Create: `rk3566/nanotrack_v3/include/recovery_candidates.hpp`
- Create: `rk3566/nanotrack_v3/src/recovery_candidates.cpp`
- Create: `rk3566/nanotrack_v3/tests/test_recovery_candidates.cpp`
- Modify: `rk3566/nanotrack_v3/include/long_term_types.hpp`
- Modify: `rk3566/nanotrack_v3/include/long_term_controller.hpp`
- Modify: `rk3566/nanotrack_v3/src/long_term_controller.cpp`
- Modify: `rk3566/nanotrack_v3/CMakeLists.txt`
- Test: `rk3566/nanotrack_v3/tests/test_long_term_controller.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_long_term_replay.cpp`

**Interfaces:**
- Produces: `RecoveryCandidateSource::{kRecent,kSafe,kDetector}`, stable `RecoveryCandidateKey`, and `RecoveryCandidateTable::observe/expire/clear/select_probe`.
- Produces: capacity `2 + detector_max_candidates`, geometric duplicate merge, deterministic recent/safe/detector priority, and strict 2-of-3 confirmation.
- Consumes: trusted/suspected state and template observations from Tasks 2–3.

- [ ] **Step 1: Write failing table tests**

Test source-stable ids, duplicate merge, capacity, deterministic overflow, 2-of-3 with one gap, expiry, generation invalidation, reset/exception cleanup, recent-vs-detector conflict, safe-vs-recent agreement, and ambiguous same-class conflict retaining search.

- [ ] **Step 2: Run the new test and verify RED**

Run: `cmake --build /tmp/nanotrack-occ-c010-host --target test_recovery_candidates -j4`

Expected: target/types missing.

- [ ] **Step 3: Implement the standalone table**

Keep only bounded metadata and boxes; one probe is selected per frame while other entries age. Detector confidence may order detector entries only, never outrank a conflicting recent/safe identity by itself.

- [ ] **Step 4: Integrate all recovery sources in the controller**

Remove anchor-only search and mandatory-detector branches. Recent and safe observations can seed candidates after the configured minimum search age; detector ingestion adds candidates but cannot authorize control. Reuse one confirmation/commit path and preserve motion-search control until a candidate commits or authorization expires. Remove `template_bridge` state and its special acceptance path.

- [ ] **Step 5: Run controller/replay tests and commit**

Run: `cmake --build /tmp/nanotrack-occ-c010-host --target test_recovery_candidates test_long_term_controller test_long_term_replay -j4 && ctest --test-dir /tmp/nanotrack-occ-c010-host -R '^(recovery_candidates|long_term_controller|long_term_replay)$' --output-on-failure`

Expected: PASS, including NanoTrack-only recovery without any detector requests and no recovery for conflicting same-class candidates.

```bash
git add rk3566/nanotrack_v3/CMakeLists.txt rk3566/nanotrack_v3/include/recovery_candidates.hpp rk3566/nanotrack_v3/src/recovery_candidates.cpp rk3566/nanotrack_v3/tests/test_recovery_candidates.cpp rk3566/nanotrack_v3/include/long_term_types.hpp rk3566/nanotrack_v3/include/long_term_controller.hpp rk3566/nanotrack_v3/src/long_term_controller.cpp rk3566/nanotrack_v3/tests/test_long_term_controller.cpp rk3566/nanotrack_v3/tests/test_long_term_replay.cpp
git commit -m "feat: unify tracker and detector recovery candidates"
```

### Task 5: Runtime transaction acknowledgement and schema observability

**Files:**
- Modify: `rk3566/nanotrack_v3/include/long_term_controller.hpp`
- Modify: `rk3566/nanotrack_v3/src/long_term_controller.cpp`
- Modify: `rk3566/nanotrack_v3/include/long_term_runtime.hpp`
- Modify: `rk3566/nanotrack_v3/src/long_term_runtime.cpp`
- Modify: `rk3566/nanotrack_v3/include/tracking_log.hpp`
- Modify: `rk3566/nanotrack_v3/src/tracking_log.cpp`
- Modify: `rk3566/nanotrack_v3/src/debug_telemetry.cpp`
- Modify: `rk3566/nanotrack_v3/src/interactive_debug_overlay.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_long_term_runtime.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_tracking_log.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_debug_telemetry.cpp`
- Test: `rk3566/nanotrack_v3/tests/test_interactive_debug_overlay.cpp`

**Interfaces:**
- Produces: `TemplateTransitionRequest {transaction_id, action, bbox, recovery}` on output and `LongTermController::resolve_template_transition(transaction_id, applied) noexcept`.
- Consumes: transactional tracker methods from Task 2 and controller states/candidates from Tasks 3–4.
- Produces: next tracking schema version with recent/safe raw bboxes/metrics, head execution flags, baseline deltas, tentative/trusted flags, transition id/result, candidate source/id/hits/conflict, and state-change reason.

- [ ] **Step 1: Write failing runtime transaction tests**

Extend `FakeTracker` with injectable throws at prepare and with transaction logs. Assert prepare failure calls tracker abort and controller resolve(false), leaves no probation, and next frame runs; commit/abort are noexcept and acknowledged once; stale transaction ids cannot affect later frames; recovery prepare failure retains the previously committed templates and returns to safe search semantics without reporting a committed template.

- [ ] **Step 2: Write failing schema/overlay tests**

Pin exact enum strings and new CSV/JSON fields. Assert suspected overlay distinguishes `SUSPECTED / RECENT OBSERVATION` from accepted tracking and motion search; display smoothing remains separate; `rect[0]` source is traceable. Update verifier schema tests to reject missing or contradictory tentative/trusted fields.

- [ ] **Step 3: Run focused tests and verify RED**

Run: `cmake --build /tmp/nanotrack-occ-c010-host --target test_long_term_runtime test_tracking_log test_debug_telemetry test_interactive_debug_overlay -j4 && ctest --test-dir /tmp/nanotrack-occ-c010-host -R '^(long_term_runtime|tracking_log|debug_telemetry|interactive_debug_overlay|verify_board_schema)$' --output-on-failure`

Expected: FAIL on missing transaction and schema fields.

- [ ] **Step 4: Implement runtime acknowledgement**

Controller creates a pending transition without advancing template lifecycle flags. Runtime prepares/applies the tracker transition, then resolves the same id. Any exception aborts both sides before frame cleanup. Tracking acceptance may retain old committed templates if staging fails, but output must record `template_transition_applied=false` and must not claim probation/commit.

- [ ] **Step 5: Implement telemetry/schema and run full CTest**

Run: `cmake --build /tmp/nanotrack-occ-c010-host -j4 && ctest --test-dir /tmp/nanotrack-occ-c010-host --output-on-failure`

Expected: all tests PASS and `git diff --check` clean.

- [ ] **Step 6: Commit**

```bash
git add rk3566/nanotrack_v3/include/long_term_controller.hpp rk3566/nanotrack_v3/src/long_term_controller.cpp rk3566/nanotrack_v3/include/long_term_runtime.hpp rk3566/nanotrack_v3/src/long_term_runtime.cpp rk3566/nanotrack_v3/include/tracking_log.hpp rk3566/nanotrack_v3/src/tracking_log.cpp rk3566/nanotrack_v3/src/debug_telemetry.cpp rk3566/nanotrack_v3/src/interactive_debug_overlay.cpp rk3566/nanotrack_v3/tests/test_long_term_runtime.cpp rk3566/nanotrack_v3/tests/test_tracking_log.cpp rk3566/nanotrack_v3/tests/test_debug_telemetry.cpp rk3566/nanotrack_v3/tests/test_interactive_debug_overlay.cpp rk3566/nanotrack_v3/tests/test_verify_board_schema.sh
git commit -m "feat: make template transitions observable and atomic"
```

### Task 6: Regression fixtures, RK3588 candidate evidence, and handoff boundary

**Files:**
- Modify: `rk3566/nanotrack_v3/tests/test_long_term_replay.cpp`
- Modify: `rk3566/nanotrack_v3/config/rk3588-interactive-debug.conf`
- Modify: `rk3566/nanotrack_v3/README.md`
- Modify after observed evidence: `docs/progress/board-validation.md`
- Create after execution: `docs/evaluation/occ-video-1/recent-safe-candidate.json`

**Interfaces:**
- Consumes: all Tasks 1–5.
- Produces: host regression verdict, exact RK3588 source/binary/config/model hashes, runtime/head-call measurements, and retained outputs; it does not produce production or pod acceptance.

- [ ] **Step 1: Add pre-board regression fixtures**

Add replay fixtures for stable-size occlusion, gradual viewpoint transition, large-angle turn, same-class crossing, recent-only recovery, detector conflict, and expired motion control. Assert semantic state/control/candidate outcomes rather than video-specific frame ids.

- [ ] **Step 2: Run complete host verification**

Run: `cmake --build /tmp/nanotrack-occ-c010-host -j4 && ctest --test-dir /tmp/nanotrack-occ-c010-host --output-on-failure && git diff --check`

Expected: 0 failed tests and clean diff check.

- [ ] **Step 3: Build a new RK3588 Debug candidate without overwriting C012**

Use the board-native toolchain and a new immutable candidate root. Bind the new `visible-recent-safe-v1.conf`; verify source archive, binary, model, detector, config, and input SHA-256 before execution. Do not overwrite `/home/neardi/Project/SOT/occ-candidate-e865c78`.

- [ ] **Step 4: Run canonical car_001 and diagnostic car_002 separately**

Run `car_001` against its formal labels and existing verifier/evaluator. Run `car_002` only as qualitative/development evidence until the user confirms its draft labels. Retain logs, CSV, JSON, video, head-call counts, latency percentiles, memory, and exact commands. Reject any result that switches identity even if class and tracker scores are high.

- [ ] **Step 5: Select or hold candidate defaults**

Use generic fixtures plus confirmed observable development labels to choose relative-response defaults. If `car_002` remains unconfirmed or no setting passes both identity safety and recovery gates, keep the shipping profile calibration-pending and do not deploy it as the default interactive config.

- [ ] **Step 6: Record evidence and commit**

Update only `docs/progress/board-validation.md` with observed boundaries and the evaluation record. Explicitly separate host tests, board load/inference, algorithm acceptance, human label confirmation, and pod acceptance.

```bash
git add rk3566/nanotrack_v3/tests/test_long_term_replay.cpp rk3566/nanotrack_v3/config/rk3588-interactive-debug.conf rk3566/nanotrack_v3/README.md docs/evaluation/occ-video-1/recent-safe-candidate.json docs/progress/board-validation.md
git commit -m "test: validate recent-safe recovery on RK3588"
```

## Self-Review Result

- Spec coverage: configuration migration, template lifecycle, suspected control, scale migration, unified recovery, transactions, telemetry, host tests, and board evidence each map to a task.
- Step scan: every implementation task begins with a named failing test and ends with a focused verification plus commit.
- Type consistency: `TemplateRunMode`, `RecoveryCandidateSource`, transition ids, and tentative/trusted flags are defined once and consumed by later tasks.
- Review focus: all five high-risk conditions have explicit tests in Tasks 2–5.
- Proportion: the plan specifies interfaces and observable assertions, not implementation bodies; parameter calibration remains an evidence gate in Task 6.
