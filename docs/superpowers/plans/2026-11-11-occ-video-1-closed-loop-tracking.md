# `occ_video_1` Closed-Loop Tracking Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the shared NanoTrack runtime track the canonical white car through all 271 frames, publish continuous motion-search control during full occlusion, and confirm the same target before frame 190 without fitting behavior to frame numbers or truth at runtime.

**Architecture:** A truth-only Python evaluator remains outside the runtime. The C++ core separates observation validity, tracking phase, and control source; a bounded motion model owns host control while searching; tracker and detector candidates remain shadow hypotheses until two consecutive identity-consistent confirmations atomically commit reacquisition. A generic visible-light Debug configuration enables the candidate behavior while existing defaults and the RK3566 production contract stay unchanged.

**Tech Stack:** Python 3.10/unittest, C++17, CMake/CTest, OpenCV, NanoTrack V3, asynchronous YOLO detector adapter, RKNN Runtime on the LKD3588.

**Spec:** `docs/superpowers/specs/2026-11-11-occ-video-1-closed-loop-tracking-design.md`

## Global Constraints

- Start from `codex/occ-video-1-tracking@98ee995e0ef67f51bfb6114754eb3f43d7e0e909`; preserve unrelated changes and stop if the baseline or worktree changes unexpectedly.
- Runtime code must never read `frame_id` to recognize this video, load truth labels, or consume evaluator output. All decisions use timestamps, normalized geometry, model responses, and prior runtime state.
- `data/occ_video_1` remains evaluation-only and must not enter training, fine-tuning, template precomputation, or parameter-ranking feedback after candidate freeze.
- Candidate development and parameter selection use frames 0--189 only. Before the first run that exposes frames 190--270, write and commit a freeze record containing source/config/model hashes and the exact command.
- If the first frozen candidate's 190--270 result causes any code or parameter change, mark that segment permanently exposed; it remains regression evidence, not a restored blind holdout.
- Keep the old internal `TrackingMode` values for compatibility where useful, but publish the new external semantics. Existing `visible-default.conf`, RK3566 defaults, production handoff, and host ABI are not switched automatically.
- Detector output never directly changes the committed motion state, `control_bbox`, template, or host control. A single hit may only create a shadow hint; two identity-consistent confirmations are mandatory.
- During a valid search hypothesis, `control_valid=true`, `control_source=MOTION_SEARCH`, `observation_valid=false`, and `control_bbox` equals the bounded motion prediction. Numeric/time/input faults remain fail-closed.
- Large videos, RKNNs, logs, CSVs, rendered videos, and parameter sweeps stay outside Git. Commit only code, tests, generic config, compact reports/manifests, hashes, and reproduction commands.
- Do not modify coordinator-owned `docs/progress/STATUS.md` or `docs/progress/integration-contract.yaml`. Record requested contract changes only in `docs/progress/board-validation.md` Handoff.
- RK3588 results are separate Debug evidence; they do not satisfy the frozen RK3566 board gate, production cross-compilation, or human pod acceptance.

## Review Focus

- A high-score tracker response on the bridge texture must not be published as a valid observation. Tasks 3 and 4 test persistent multi-evidence suspicion and immediate search control after rejection.
- Motion search must remain smooth and controllable without allowing an unbounded prediction to authorize control forever. Task 3 uses timestamp-based confidence age and physical velocity/acceleration/scale bounds, including irregular timestamps and time reversal.
- An asynchronous detector result may arrive late, from an old generation, or near the neighboring white car. Tasks 4 and 5 test source-frame projection, stale/generation rejection, ambiguity rejection, and shadow-only first hits.
- Reacquisition must be atomic: before the second valid confirmation, committed motion/control/template state is unchanged; after it, motion resets once, `reacquired` pulses once, and recent-template probation begins.
- Telemetry must not conflate “not visible” with “cannot control.” Task 6 upgrades schema and verifier checks for `tracking_phase`, `observation_valid`, and `control_source` across CSV, JSON, overlay, and events.
- Passing one exact configuration is insufficient. Task 8 perturbs every tunable threshold/count and initialization/timing inputs, and rejects knife-edge candidates.
- Board success requires three complete runs, evaluator PASS, board verifier PASS, decodable 271-frame output, stable reacquisition timing, and a human identity review of frames 160--200.

---

### Task 1: Build the independent canonical evaluator

**Files:**
- Create: `scripts/evaluate_occ_video_1.py`
- Create: `tests/test_evaluate_occ_video_1.py`
- Create: `tests/fixtures/occ_video_1_eval/README.md`
- Create: `docs/evaluation/occ-video-1/baseline-201991a.json`
- Create: `docs/evaluation/occ-video-1/baseline-201991a.md`

**Interfaces:**
- Consumes: `--dataset-root`, `--boxes`, optional `--session-metadata`, `--scope {development,full,holdout}`, `--output-json`, and `--output-markdown`.
- Produces: schema-1 report with input hashes, visible/estimated/full-occlusion/reappearance strata, per-frame metrics, worst frames, state/control violations, first valid reacquisition frame, identity-proxy violations, and hard-gate verdict. Exit 0 means every gate in the selected scope passed; exit 1 means valid inputs with failed gates; exit 2 means malformed or mismatched inputs.
- Pure test seams: `load_contract(root)`, `load_truth(contract)`, `load_tracking_csv(path)`, `evaluate(contract, rows, scope)`, and `render_markdown(report)`.

- [ ] **Step 1: Write failing evaluator unit tests**

  Cover canonical hash/271-label/continuous-frame validation; YOLO-to-XYWH conversion; exclusion of frames 169--181 from ordinary IoU; separate estimated-trajectory center/velocity/acceleration metrics; full-occlusion SEARCHING semantics; `<190` single reacquisition; all-post-init control continuity; visible per-frame and mean gates; identity-proxy jump detection; malformed schema, duplicate/missing rows, NaN, and partial boxes. Use small synthetic objects through pure functions rather than copying the canonical video into Git.

- [ ] **Step 2: Run the focused test and verify RED**

  Run: `python3 -m unittest -v tests.test_evaluate_occ_video_1`

  Expected: FAIL because the evaluator module and report schema do not exist.

- [ ] **Step 3: Implement strict identity loading and metric strata**

  Freeze the canonical video/metadata/events/status/label-manifest hashes from the spec, verify all files before scoring, and emit finite JSON (`allow_nan=False`). Keep `development` limited to 0--189, `holdout` limited to 190--270, and `full` at 0--270. Never let the evaluator emit runtime configuration suggestions.

- [ ] **Step 4: Add a CLI fixture and failure-code tests**

  Build temporary canonical-shaped datasets in the unit test and inject their expected hash contract through the pure `evaluate` seam; separately test argparse/output/exit-code behavior without weakening production hash checks.

- [ ] **Step 5: Reproduce and freeze the existing RK3588 baseline report**

  Copy only the compact report derived from the retained board CSV; bind source commit `201991a92bba95b542c97505c37053d1c4fe1da8`, the four existing evidence hashes, and the missing-manifest boundary. Assert the report reproduces mean visible IoU about `0.579`, pre-occlusion about `0.631`, post-occlusion about `0.479`, control gaps, and no legal reacquisition.

- [ ] **Step 6: Run tests and commit Task 1**

  Run the focused unittest, `python3 -m py_compile scripts/evaluate_occ_video_1.py`, and `git diff --check`. Commit evaluator, tests, and compact baseline reports together.

### Task 2: Introduce explicit tracking-phase and control-source contracts

**Files:**
- Modify: `rk3566/nanotrack_v3/include/long_term_types.hpp`
- Modify: `rk3566/nanotrack_v3/src/long_term_types.cpp`
- Modify: `rk3566/nanotrack_v3/include/long_term_controller.hpp`
- Modify: `rk3566/nanotrack_v3/tests/test_long_term_controller.cpp`

**Interfaces:**
- Add: `enum class TrackingPhase { kTracking, kSearching, kInvalid }`.
- Add: `enum class ControlSource { kNone, kObservation, kMotionSearch }` plus `to_string` overloads.
- Extend `SearchPlan` and `ControllerOutput` with `tracking_phase`, `observation_valid`, and `control_source`.
- Preserve: `control_valid` remains the host authorization bit; `TrackingMode` remains the detailed internal recovery state.

- [ ] **Step 1: Add failing contract tests**

  Assert initialization/accepted tracking publishes `TRACKING,true,OBSERVATION`; rejected observations with valid motion publish `SEARCHING,false,MOTION_SEARCH`; exceptions/aborted frames cannot publish control; and every output satisfies `(observation_valid => accepted)` and `(control_source==NONE => !control_valid)`.

- [ ] **Step 2: Run focused CTest and verify RED**

  Configure a clean RKNN-independent build under `/tmp/nanotrack-occ-plan`, build `test_long_term_controller`, and run `ctest --test-dir /tmp/nanotrack-occ-plan -R long_term_controller --output-on-failure`.

  Expected: compile/test failure because the new enums and fields do not exist.

- [ ] **Step 3: Implement types and centralized output invariants**

  Add a private controller helper that assigns the three external semantic fields together and validates them before return. Do not infer observation validity later from `control_valid` or `accepted` in logging code.

- [ ] **Step 4: Keep default behavior compatibility explicit**

  Existing configurations may continue their old detailed mode transitions, but every returned output must use the new unambiguous semantic triplet. Add assertions that direct tracking still uses the real `tracking_bbox/control_bbox` and display smoothing remains independent.

- [ ] **Step 5: Run focused tests and commit Task 2**

  Build and run `long_term_controller`, `long_term_runtime`, and `tracker_math`; commit only the semantic types/controller tests and implementation.

### Task 3: Make motion search continuous, bounded, and time-based

**Files:**
- Modify: `rk3566/nanotrack_v3/include/controller_config.hpp`
- Modify: `rk3566/nanotrack_v3/src/controller_config.cpp`
- Modify: `rk3566/nanotrack_v3/include/motion_estimator.hpp`
- Modify: `rk3566/nanotrack_v3/src/motion_estimator.cpp`
- Modify: `rk3566/nanotrack_v3/include/long_term_controller.hpp`
- Modify: `rk3566/nanotrack_v3/src/long_term_controller.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_controller_support.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_long_term_controller.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_long_term_replay.cpp`

**Interfaces:**
- Add generic candidate settings: `motion_search_enabled`, `motion_search_confidence_ms`, `motion_max_speed_diagonals_per_s`, `motion_max_acceleration_diagonals_per_s2`, `motion_scale_rate_limit_per_s`, and `search_probe_interval_ms`.
- Add motion snapshot diagnostics: bounded velocity, bounded acceleration, search age, and `prediction_confident(timestamp_ms)`.
- Preserve legacy frame-count coast fields for parsing/metadata compatibility, but the new candidate path uses timestamps and never enlarges authorization from a frame counter.

- [ ] **Step 1: Add failing motion and controller tests**

  Feed accepted observations with constant velocity, acceleration, scale change, variable 16/33/67 ms steps, dropped frames, a rejected interval, and time reversal. Assert smooth extrapolation, finite/clipped boxes, bounded derivatives, scale freeze/rate limit, valid search control through the configured confidence interval, and fail-closed output after expiry or invalid time.

- [ ] **Step 2: Verify RED with focused tests**

  Run `ctest --test-dir /tmp/nanotrack-occ-plan -R 'controller_support|long_term_controller|long_term_replay' --output-on-failure`.

- [ ] **Step 3: Implement a confidence-weighted alpha-beta/acceleration model**

  Estimate velocity and optional acceleration only from accepted observations. Normalize physical caps by frame diagonal and elapsed seconds; apply acceleration and scale-rate bounds before projection; never correct from rejected tracker boxes or detector hints. Time reversal, non-finite state, or an uninitialized predictor throws and is caught by the controller as no authorization.

- [ ] **Step 4: Replace candidate coast authorization with SEARCHING motion control**

  With `motion_search_enabled=true`, the first unaccepted observation immediately starts `SEARCHING`; every confident prediction sets `tracking_bbox=control_bbox=motion prediction`, `observation_valid=false`, `control_valid=true`, and `control_source=MOTION_SEARCH`. Keep `display_bbox` separately presentational. Do not terminate control merely because `kUncertain`, `kLocalRecovery`, or `kAssistedRecovery` changes.

- [ ] **Step 5: Freeze templates and invalidate old detector work on search entry**

  Increment eligibility generation once per search episode, clear pending confirmations/hints, prevent recent-template stage/commit, and preserve the anchor. Repeated rejected frames must not repeatedly increment the generation.

- [ ] **Step 6: Run the synthetic state matrix subset and commit Task 3**

  Cover no-occlusion short low-score recovery, partial/full occlusion, long gap past confidence, left/right motion, acceleration, scale change, invalid timestamps, and numeric exceptions. Commit config parser/identity, motion/controller behavior, and tests together.

### Task 4: Schedule detector and tracker probes during search without direct control

**Files:**
- Modify: `rk3566/nanotrack_v3/include/long_term_controller.hpp`
- Modify: `rk3566/nanotrack_v3/src/long_term_controller.cpp`
- Modify: `rk3566/nanotrack_v3/include/long_term_types.hpp`
- Modify: `rk3566/nanotrack_v3/src/long_term_types.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_long_term_controller.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_long_term_runtime.cpp`

**Interfaces:**
- Add: `SearchHint`/diagnostics carrying candidate ID, source frame/timestamp, projected bbox, generation, first-hit frame, confirmation count, and rejection reason.
- Change detector scheduling predicate from “assisted/lost only” to “SEARCHING and eligible,” still rate-limited by `detector_interval_ms` and target-class authorization.
- Project eligible detector candidates from source timestamp into the current motion corridor before normalized geometry checks.

- [ ] **Step 1: Add failing search-scheduling tests**

  Assert the first search frame requests detector when ready; the interval is timestamp-based; HOLD/search no longer blanket-rejects the matching generation; stale, wrong-frame, wrong-generation, wrong-class, outside-corridor, and same-class ambiguity results remain rejected; detector exceptions leave motion control intact.

- [ ] **Step 2: Add failing shadow-hint isolation tests**

  After one legal detector/anchor hit, assert `SearchPlan` may bias the next tracker ROI but the committed motion bbox, `control_bbox`, `control_source`, template state, and host command remain unchanged. Aborting a frame or receiving a conflicting next hit must discard the hint without rollback work.

- [ ] **Step 3: Run focused tests and verify RED**

  Run controller and runtime CTests; expected failures identify the current assisted-only scheduling and hold invalidation paths.

- [ ] **Step 4: Implement search-wide scheduling and source-time projection**

  Submit full-frame detector requests only while target class is established and search is active. Retain request ID/generation/source provenance; project the candidate according to bounded motion elapsed time before comparing it with the current corridor.

- [ ] **Step 5: Implement a shadow-only hint lifecycle**

  The first hit creates an immutable hint for exactly the next processed frame. It may select/limit a search ROI but cannot call `motion_.correct/initialize`, change committed boxes, or update templates. Make “hint used,” “hint expired,” and rejection reasons observable for tests and later logging.

- [ ] **Step 6: Run safety regressions and commit Task 4**

  Include queue capacity, multiple same-class candidates, stale/generation gaps, detector unavailable/error, frame abort, and late completion. Commit the scheduling/hint change only after controller/runtime tests pass.

### Task 5: Commit reacquisition atomically after two identity-consistent hits

**Files:**
- Modify: `rk3566/nanotrack_v3/include/long_term_controller.hpp`
- Modify: `rk3566/nanotrack_v3/src/long_term_controller.cpp`
- Modify: `rk3566/nanotrack_v3/src/long_term_runtime.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_long_term_controller.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_long_term_runtime.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_long_term_replay.cpp`

**Interfaces:**
- Reuse `confirmation_required=2` and `confirmation_window` as the generic two-hit contract; safety validation continues to reject values below two.
- Add `AcceptedObservationPath::kSearchReacquisition` if existing detector/coast values cannot faithfully represent tracker- or detector-seeded shadow confirmation.
- A successful commit emits one `reacquired` pulse, resets motion to the confirmed tracker bbox, clears search hint/generation queues, returns `TRACKING/OBSERVATION`, and stages recent-template probation without replacing anchor.

- [ ] **Step 1: Write failing two-hit atomicity tests**

  Cover tracker-seeded and detector-seeded candidates, consistent and conflicting second hits, gaps within/outside the window, anchor/recent disagreement, neighboring same-class ambiguity, and a recent-only hit. Assert one hit never changes control/template/motion and two valid hits commit exactly once.

- [ ] **Step 2: Write failing template lifecycle tests**

  Assert anchor stays frozen through search/reacquisition; recent is staged only after commit; probation requires the configured reliable frames; disagreement rolls back; later ordinary tracking cannot emit another reacquired event.

- [ ] **Step 3: Verify RED with focused CTests**

  Run controller, runtime, and replay tests; expected failures expose current paths where short confirmation is not marked reacquired or detector state owns the committed search center too early.

- [ ] **Step 4: Implement one candidate-confirmation state machine**

  Unify tracker and detector-seeded confirmation around a shadow candidate record. Require corridor, cross-frame IoU/center/area/aspect, anchor quality, template agreement, and resolved same-class identity at both hits. `recent` may veto but never authorize by itself.

- [ ] **Step 5: Implement atomic commit and probation**

  Apply confirmed bbox, motion reset, control source switch, queue cleanup, and probation state only after every gate passes. Keep all pre-commit search frames under motion control. On any failed gate, discard the candidate and continue pure motion search.

- [ ] **Step 6: Run the complete synthetic state matrix and commit Task 5**

  Add combinations of direction/speed/acceleration/scale, partial/full/no occlusion, near/far same-class distractors, stale/generation gaps, detector faults, and template conflict. Commit only after all focused tests pass.

### Task 6: Publish schema-10 semantics through logs, telemetry, overlay, and verifier

**Files:**
- Modify: `rk3566/nanotrack_v3/include/tracking_log.hpp`
- Modify: `rk3566/nanotrack_v3/src/tracking_log.cpp`
- Modify: `rk3566/nanotrack_v3/include/debug_telemetry.hpp`
- Modify: `rk3566/nanotrack_v3/src/debug_telemetry.cpp`
- Modify: `rk3566/nanotrack_v3/include/interactive_debug_overlay.hpp`
- Modify: `rk3566/nanotrack_v3/src/interactive_debug_overlay.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_tracking_log.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_debug_telemetry.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_interactive_debug_overlay.cpp`
- Modify: `rk3566/nanotrack_v3/scripts/verify_board.sh`
- Modify: `rk3566/nanotrack_v3/tests/test_verify_board_schema.sh`

**Interfaces:**
- Increment `kTrackingSchemaVersion` from 9 to 10.
- Add CSV/metadata fields: `tracking_phase`, `observation_valid`, `control_source`, search age/confidence, motion velocity/acceleration diagnostics, and shadow-hint provenance/status.
- Overlay shows `SEARCHING / MOTION_SEARCH` distinctly from `TRACKING / OBSERVATION`; telemetry emits search-enter, hint-first-hit, hint-reject, reacquired, and search-expired events.

- [ ] **Step 1: Add failing serialization/presentation/verifier tests**

  Require every schema-10 row to satisfy semantic invariants; reject a SEARCHING row labeled as observation, detector control, invalid motion control, a reacquired row without the second confirmation, partial diagnostic groups, or metadata/CSV schema mismatch.

- [ ] **Step 2: Run logging/telemetry/overlay/schema tests and verify RED**

  Run the four relevant CTests plus `verify_board_schema`; expected failures show schema 9 and missing semantic fields.

- [ ] **Step 3: Serialize from controller output, not reconstructed guesses**

  Copy the new fields directly from `ControllerOutput/SearchPlan`. Update metadata with all new generic config values and their controller-config identity. Keep old detailed state and coast diagnostics for traceability.

- [ ] **Step 4: Update Debug display and event transitions**

  Present the predicted control box during search and label it as prediction, never as a visual observation. Event transitions key on `tracking_phase/control_source`, not only `TrackingMode::kOcclusionHold`.

- [ ] **Step 5: Strengthen the board verifier**

  Require schema 10, exact row/frame continuity, finite in-frame boxes, valid semantic combinations, no detector-direct authorization, and coherent two-hit reacquisition. Continue model/manifest/hash/runtime checks.

- [ ] **Step 6: Run complete RKNN-independent CTest and commit Task 6**

  Reconfigure and rebuild `/tmp/nanotrack-occ-plan`, run all CTests, and run `git diff --check`. Commit schema, verifier, presentation, and tests together.

### Task 7: Add a generic calibrated visible-light candidate configuration

**Files:**
- Create: `rk3566/nanotrack_v3/config/visible-occlusion-calibrated.conf`
- Modify: `rk3566/nanotrack_v3/tests/test_controller_support.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_cli.cpp`
- Create: `docs/evaluation/occ-video-1/calibration-ledger.md`

**Interfaces:**
- The filename and keys contain no video, frame, bridge, or scene-specific identifiers.
- Existing `visible-default.conf` remains byte-for-byte behavior-compatible unless a separately justified parser default must be added; new behavior is explicitly enabled only by the calibrated candidate file.
- The ledger records candidate ID, parent, rationale, source/config/model hashes, 0--189 command/report, changed parameters, and decision without storing large outputs.

- [ ] **Step 1: Add failing config/CLI tests**

  Assert the new file parses as `sensor=visible`, `calibrated=true`, enables motion search, retains two-hit safety, uses finite valid bounds, and has a distinct config identity. Assert default config behavior is unchanged.

- [ ] **Step 2: Verify RED, then add a conservative initial candidate**

  Begin from existing visible thresholds and the generic time/geometry limits introduced in Tasks 3--5. Do not insert frame numbers, truth coordinates, or values chosen from holdout feedback.

- [ ] **Step 3: Create the immutable calibration-ledger format**

  Record the failed `201991a` baseline first. Each subsequent 0--189 candidate gets an append-only entry before another candidate is tried; include exact board path and artifact hashes.

- [ ] **Step 4: Run parser/CLI tests and commit Task 7**

  Commit candidate config, tests, and ledger skeleton before parameter iteration begins.

### Task 8: Calibrate only on 0--189 and prove a stable generalization plateau

**Files:**
- Create: `scripts/check_occ_tracking_robustness.py`
- Create: `tests/test_check_occ_tracking_robustness.py`
- Modify append-only: `docs/evaluation/occ-video-1/calibration-ledger.md`
- Create at freeze: `docs/evaluation/occ-video-1/frozen-candidate.json`

**Interfaces:**
- Consumes a candidate manifest plus a set of evaluator reports named by perturbation; it does not execute the tracker or choose parameters.
- Produces schema-1 robustness JSON/Markdown and fails unless every required real threshold has `-10%/nominal/+10%`, every tunable count has valid `-1/nominal/+1`, all initialization perturbations exist, all safety invariants pass, reacquisition is `<190`, and development mean IoU drop is at most `0.03`.
- Fixed safety invariants (`confirmation_required>=2`, no detector control, generation/stale isolation, fail-closed faults) are audited but never perturbed downward.

- [ ] **Step 1: Write failing robustness-manifest tests**

  Reject missing neighbors, knife-edge nominal-only PASS, mislabeled scopes, duplicate reports, holdout-contaminated reports, source/config/model hash mismatch, and safety-invariant perturbation. Pass a complete synthetic plateau.

- [ ] **Step 2: Implement the report aggregator and verify focused tests**

  Run: `python3 -m unittest -v tests.test_check_occ_tracking_robustness` and `python3 -m py_compile scripts/check_occ_tracking_robustness.py`.

- [ ] **Step 3: Deploy candidate code/config to the isolated RK3588 tree**

  Transfer an exact committed source archive without replacing the existing RK3588 ONNX-derived models. Verify source, executable, model, manifest, labels, video, and config SHA-256 before every recorded run; rebuild Debug natively and run complete CTest.

- [ ] **Step 4: Iterate by failure class on frames 0--189 only**

  Run with `--max-frames 189`; use evaluator development reports to address, in order: control semantics, search entry, motion continuity, detector/probe reachability, two-hit identity, then visible box quality. Change the smallest coherent algorithm/config set per candidate and append its evidence before the next attempt. Never query 190--270 during this stage.

- [ ] **Step 5: Run the full generic robustness matrix**

  Execute threshold/count neighbors, initial bbox center `±2 px`, width/height `±5%`, variable timestamps, detector delay/drop/generation gaps, and the C++ synthetic state matrix. Require every safety/identity/control/reacquisition gate and the `0.03` development-IoU tolerance; choose the simplest candidate in the interior of the passing plateau, not the highest single score.

- [ ] **Step 6: Freeze before exposing holdout**

  Write `frozen-candidate.json` with timestamp, branch/commit, executable/config/model/manifest/labels hashes, exact 0--189 evidence paths and reports, robustness verdict, and the exact future full-run command. Commit this record. After this commit, do not change the frozen candidate before Task 9's first holdout run.

### Task 9: Perform the one-time holdout and full RK3588 acceptance runs

**Files:**
- Retain outside Git on RK3588: three full run directories, evaluator reports, manifests, verifier logs, and videos.
- Create: `docs/evaluation/occ-video-1/rk3588-final-summary.md`
- Create: `docs/evaluation/occ-video-1/rk3588-final-artifacts.sha256`
- Modify: `docs/progress/board-validation.md`

**Interfaces:**
- Each run directory contains `run.log`, `bboxes.csv`, `session-metadata.json`, `output.avi`, `manifest.json`, `verify.log`, evaluator JSON/Markdown, and per-run SHA-256.
- The summary declares holdout status exactly once, three-run parity metrics, performance separately from functional verdict, and human identity-review status.

- [ ] **Step 1: Run the frozen candidate once over all 271 frames**

  Use the exact command and hashes committed in Task 8. Evaluate `holdout` first and durably record its result without code/config changes, then evaluate `full`. If the holdout fails, mark it exposed and failed before any later work; do not silently rerun or retune under a blind-holdout label.

- [ ] **Step 2: Run board verification and media checks for run 1**

  Require evaluator hard gates, schema-10 verifier, 271 CSV/video frames, decodable 1920x1080 video, exact input/model/config identity, and no GUI/RKNN errors. Record runtime/pipeline/detector timing as non-gating performance evidence.

- [ ] **Step 3: Run two more exact repeats**

  Require all three full reports to pass; reacquisition frame spread `<=1`; every run reacquires before 190; outside the transition window discrete semantics match; control stays valid in the transition; visible center p95 difference `<=2 px`, max `<=5 px`; mean IoU spread `<=0.01`.

- [ ] **Step 4: Complete human identity review**

  Present frames/video 160--200 for explicit human confirmation that the tracker did not switch to the neighboring white car. Automatic corridor/continuity checks are necessary but cannot substitute for this sign-off because no distractor truth ID exists.

- [ ] **Step 5: Run complete final verification**

  On host: evaluator/robustness unittests, complete Python tests affected by the change, clean RKNN-independent build, complete CTest, `git diff --check`, and clean status. On RK3588: native Debug build, complete CTest, three evaluator PASS results, three `verify_board.sh` PASS results, and artifact SHA verification.

- [ ] **Step 6: Record the board milestone and Handoff**

  Update only `docs/progress/board-validation.md` with ISO timestamp, branch/base/result commits, source/executable/config/model/data hashes, exact commands, observed metrics, holdout exposure status, retained paths, human review result, blockers, and evidence boundaries. Request the coordinator to adopt schema 10 and the distinct RK3588 Debug candidate; do not mark RK3566 `latest_handoff.available=true`.

- [ ] **Step 7: Commit Task 9 and prepare branch delivery**

  Commit compact final summary/hash/progress records. Then use `superpowers:verification-before-completion`, `superpowers:requesting-code-review`, and `superpowers:finishing-a-development-branch`; push the existing `codex/occ-video-1-tracking` branch only after review findings are resolved.

## Plan Self-Review

- Every runtime behavior is driven by timestamps, normalized geometry, model evidence, and history; no planned production interface contains canonical frame numbers or truth data.
- The evaluator, development scope, holdout scope, and full final acceptance are separate, so the same labels cannot silently become runtime inputs or unrestricted tuning feedback.
- Search control, detector scheduling, shadow hints, two-hit commit, and template probation are separate commits with focused RED/GREEN tests, which keeps safety regressions attributable.
- The plan preserves detector direct-control prohibition while meeting the user's requirement that the host and gimbal continue receiving a reasonable motion box throughout full occlusion.
- Parameter perturbation and initialization/timing matrices are required before freeze; a single exact configuration cannot satisfy Task 8.
- Final claims remain bounded to the LKD3588 Debug candidate and this representative scene; RK3566 production handoff, statistical cross-scene generalization, 30 Hz live performance, and pod acceptance remain outside the claim.
