# Visible Scale Smoothing and Recovery Center Gate Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Smooth accepted visible-light box dimensions while preserving gradual real scale changes, and prevent large motion-relative center jumps from entering ordinary recovery confirmation.

**Architecture:** Extend `ControllerConfig` with three effective settings and keep compatibility defaults (`0`, `1`, `0`) for generic/infrared use. Apply deadband, the existing 5% outer clamp, and exponential smoothing only when an observation is accepted; apply the fixed center gate only before ordinary two-of-three recovery confirmation. Carry all three settings through built-in identity, session metadata JSON, and board-evidence validation.

**Tech Stack:** C++17, CMake/CTest, shell/Python board schema fixture

**Spec:** `docs/superpowers/specs/2026-11-04-visible-scale-smoothing-recovery-gate-design.md`

## Global Constraints

- Visible values are exactly `accepted_scale_deadband_ratio=0.02`, `accepted_scale_smoothing_alpha=0.10`, and `max_reacquisition_center_distance=0.75`.
- Keep visible quality thresholds at PSR `4.2/4.0` and peak margin `0.04/0.02`; retain `max_accepted_scale_step_ratio=0.05`.
- Generic and infrared behavior use deadband `0`, alpha `1`, and reacquisition-center sentinel `0`; infrared's existing unrestricted scale-step value stays unchanged.
- A dimension inside or exactly on the 2% deadband is held. Outside it, first clamp the target to plus/minus 5%, then move 10% of the current-to-clamped distance.
- Rebuild every accepted box around the raw observation center using the existing frame-safe centering helper.
- The fixed `0.75` gate applies only to ordinary motion recovery, including template-conflict confirmation. It does not scale with search size and does not alter detector or occlusion-hold confirmation.
- A failed fixed recovery gate clears pending ordinary confirmations and leaves the observation unaccepted.
- Do not add edge, partial-visibility, or out-of-view policy.
- Keep tracking schema version `9`; add the three settings as required effective metadata fields, matching the existing additive schema practice.
- Update only `docs/progress/cross-compile.md`; do not edit `docs/progress/STATUS.md`, `docs/progress/integration-contract.yaml`, or `docs/progress/board-validation.md`.
- This plan authorizes host implementation and host tests only. It does not authorize cross-compilation, remote writes, deployment, process control, or a pod trial.

## Review Focus

- A dimension exactly 2% from the accepted value must remain held, while a value just above 2% must begin a bounded smooth update; Task 2 pins both sides of the boundary.
- Very large growth and shrink observations must never move an accepted dimension more than 0.5% per visible frame and must retain the raw observed center; Task 2 tests both directions and runtime commit identity.
- Compatibility settings `0/1/0` must preserve the legacy unrestricted accepted box and disable only the new recovery gate; Tasks 1 and 2 test these sentinels explicitly.
- One valid ordinary recovery hit followed by a residual above `0.75` must be erased, so two new valid hits are required; Task 2 tests the queue reset and template-conflict path.
- Missing, non-finite, or out-of-range metadata settings must be rejected rather than silently accepted as board evidence; Task 3 adds positive and mutation fixtures for each field.

---

### Task 1: Configuration Contract, Defaults, and Identity

**Files:**
- Modify: `rk3566/nanotrack_v3/include/controller_config.hpp`
- Modify: `rk3566/nanotrack_v3/src/controller_config.cpp`
- Modify: `rk3566/nanotrack_v3/src/main.cpp`
- Modify: `rk3566/nanotrack_v3/config/visible-default.conf`
- Modify: `rk3566/nanotrack_v3/tests/test_controller_support.cpp`

**Interfaces:**
- Produces: `ControllerConfig::accepted_scale_deadband_ratio`, `ControllerConfig::accepted_scale_smoothing_alpha`, and `ControllerConfig::max_reacquisition_center_distance`, all `float`.
- Produces: `std::string controller_config_identity(const ControllerConfig& config)` in `controller_config.hpp`; `main.cpp` hashes this result for built-in defaults.
- Consumes: Existing strict `key=value` parser and `validate_controller_config(const ControllerConfig&)`.

- [ ] **Step 1: Add failing default, shipped-config, parser, validation, and identity tests**

  Extend `test_controller_support.cpp` with assertions that:

  - built-in and shipped visible configs expose `0.02F`, `0.10F`, and `0.75F`;
  - `ControllerConfig{}` and `default_controller_config("infrared")` expose `0.0F`, `1.0F`, and `0.0F`;
  - a temporary config parses explicit overrides for all three settings;
  - deadband accepts `[0,1)` and rejects negative, `1`, infinities, and NaN;
  - alpha accepts `(0,1]` and rejects `0`, negative, values above `1`, infinities, and NaN;
  - reacquisition distance accepts `0` and positive finite values, and rejects negative, infinities, and NaN;
  - changing each new setting changes `controller_config_identity`, proving built-in hashes cannot alias across effective settings.

- [ ] **Step 2: Run the focused test and verify the new contract fails**

  Run:

  ```bash
  cmake -S rk3566/nanotrack_v3 -B /tmp/nanotrack-visible-scale-recovery-host \
    -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF
  cmake --build /tmp/nanotrack-visible-scale-recovery-host \
    --target test_controller_support -j4
  ctest --test-dir /tmp/nanotrack-visible-scale-recovery-host \
    -R '^controller_support$' --output-on-failure
  ```

  Expected: build or test failure because the fields, parser keys, and identity API do not exist yet.

- [ ] **Step 3: Implement the configuration fields, parser, exact validation, and visible defaults**

  Add the three fields with compatibility defaults to `ControllerConfig`, parse the exact key names, and validate these ranges:

  - `accepted_scale_deadband_ratio`: finite and `0 <= value < 1`;
  - `accepted_scale_smoothing_alpha`: finite and `0 < value <= 1`;
  - `max_reacquisition_center_distance`: finite and `value >= 0`.

  Set only visible defaults to `0.02F`, `0.10F`, and `0.75F`. Add all three explicit values to `visible-default.conf`; leave the infrared file and its effective defaults unchanged.

- [ ] **Step 4: Move built-in identity composition into the tested support API**

  Implement `controller_config_identity(const ControllerConfig&) -> std::string` in `controller_config.cpp` using the existing ordered fields from `main.cpp` plus the three new fields. Remove the private `builtin_config_identity` from `main.cpp` and hash `controller_config_identity(config)` instead.

- [ ] **Step 5: Re-run the focused configuration test**

  Run the build and CTest commands from Step 2.

  Expected: `controller_support` passes.

- [ ] **Step 6: Commit the configuration contract**

  ```bash
  git add docs/superpowers/specs/2026-11-04-visible-scale-smoothing-recovery-gate-design.md \
    docs/superpowers/plans/2026-11-04-visible-scale-smoothing-recovery-gate.md \
    rk3566/nanotrack_v3/include/controller_config.hpp \
    rk3566/nanotrack_v3/src/controller_config.cpp \
    rk3566/nanotrack_v3/src/main.cpp \
    rk3566/nanotrack_v3/config/visible-default.conf \
    rk3566/nanotrack_v3/tests/test_controller_support.cpp
  git commit -m "feat: define visible scale smoothing settings"
  ```

### Task 2: Accepted Scale Smoothing and Ordinary Recovery Gate

**Files:**
- Modify: `rk3566/nanotrack_v3/src/long_term_controller.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_long_term_controller.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_long_term_runtime.cpp`

**Interfaces:**
- Consumes: The three `ControllerConfig` fields from Task 1.
- Produces: Updated private `LongTermController::stabilize_accepted_bbox(const BBox&) const` behavior; no public controller API change.
- Produces: Ordinary recovery admission rule using `EvaluatedObservation::center_residual`; detector and occlusion-hold paths remain separate.

- [ ] **Step 1: Replace the scale-limiter fixtures with failing deadband and smoothing fixtures**

  In `test_long_term_controller.cpp`, assert independently for width and height that:

  - raw changes of exactly 2% retain the current accepted dimensions;
  - changes just above 2% produce a nonzero update;
  - a large observation from width `100` is outer-clamped to `105` and smoothed to `100.5`, with the analogous shrink bounded to `99.5`;
  - every update preserves the raw observation center and remains in-frame through `centered_box`;
  - repeated persistent growth is monotonic, each step is at most 0.5% of the prior accepted dimension, and eventually stops within 2% of the observation;
  - compatibility values `deadband=0`, `alpha=1`, `max_accepted_scale_step_ratio=1` return the raw observed size.

- [ ] **Step 2: Add failing ordinary recovery-gate fixtures**

  Create controller fixtures where the existing search-scaled `max_center_distance` gate passes, then assert:

  - residual `<= 0.453` can contribute to and complete ordinary two-hit recovery;
  - residual `> 0.75` cannot enter confirmation;
  - a sequence good-hit, over-gate-hit, good-hit remains unaccepted until one more good hit, proving the failed gate clears the queue;
  - the same fixed gate applies when `force_confirmation` is caused by reliable anchor/recent template conflict;
  - `max_reacquisition_center_distance=0` preserves the legacy ordinary confirmation behavior.

- [ ] **Step 3: Update the runtime commit fixture to fail on the new stabilized values**

  In `test_long_term_runtime.cpp`, configure visible smoothing and assert the tracker commits exactly `result.output.bbox`, with a large width/height observation producing the smoothed 0.5% maximum step rather than the old 5% step.

- [ ] **Step 4: Run the focused controller and runtime tests and verify failure**

  Run:

  ```bash
  cmake --build /tmp/nanotrack-visible-scale-recovery-host \
    --target test_long_term_controller test_long_term_runtime -j4
  ctest --test-dir /tmp/nanotrack-visible-scale-recovery-host \
    -R '^(long_term_controller|long_term_runtime)$' --output-on-failure
  ```

  Expected: assertions fail because accepted dimensions still use the old direct 5% limiter and ordinary confirmations lack the fixed gate.

- [ ] **Step 5: Implement dimension deadband, outer clamp, and exponential smoothing**

  In `stabilize_accepted_bbox`, calculate each dimension from the current accepted dimension and raw dimension:

  1. return the current dimension when `abs(raw-current)/current <= deadband`;
  2. otherwise clamp raw to `current * (1 +/- max_accepted_scale_step_ratio)` and the frame dimension;
  3. return `current + alpha * (clamped-current)`.

  Use the raw observation center and existing `centered_box` exactly once after both dimensions are computed.

- [ ] **Step 6: Implement the fixed ordinary recovery admission rule**

  Before calling `add_confirmation` in the non-detector ordinary recovery branch, require:

  ```cpp
  config_.max_reacquisition_center_distance == 0.0F ||
      selected->center_residual <= config_.max_reacquisition_center_distance
  ```

  When false, clear `confirmations_` and do not accept. Do not reference `plan.search_scale`; do not place this gate in detector or occlusion-hold code.

- [ ] **Step 7: Re-run focused tests plus detector/occlusion regression tests**

  Run:

  ```bash
  cmake --build /tmp/nanotrack-visible-scale-recovery-host \
    --target test_long_term_controller test_long_term_runtime test_occlusion_evidence -j4
  ctest --test-dir /tmp/nanotrack-visible-scale-recovery-host \
    -R '^(long_term_controller|long_term_runtime|occlusion_evidence)$' \
    --output-on-failure
  ```

  Expected: all selected tests pass, including existing detector and occlusion-hold cases in `long_term_controller`.

- [ ] **Step 8: Commit controller behavior**

  ```bash
  git add rk3566/nanotrack_v3/src/long_term_controller.cpp \
    rk3566/nanotrack_v3/tests/test_long_term_controller.cpp \
    rk3566/nanotrack_v3/tests/test_long_term_runtime.cpp
  git commit -m "fix: smooth visible scale and gate recovery jumps"
  ```

### Task 3: Metadata, Board-Evidence Validation, and Host Verification

**Files:**
- Modify: `rk3566/nanotrack_v3/include/tracking_log.hpp`
- Modify: `rk3566/nanotrack_v3/src/tracking_log.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_tracking_log.cpp`
- Modify: `rk3566/nanotrack_v3/scripts/verify_board.sh`
- Modify: `rk3566/nanotrack_v3/tests/test_verify_board_schema.sh`
- Modify: `docs/progress/cross-compile.md`

**Interfaces:**
- Consumes: The Task 1 configuration fields.
- Produces: `TrackingSessionMetadata` fields with the same three names and effective values.
- Produces: JSON keys required and range-checked by `verify_board.sh` under tracking schema version `9`.

- [ ] **Step 1: Add failing metadata serialization tests**

  Extend `test_tracking_log.cpp` to set all three values through `apply_controller_config_metadata`, assert the struct receives them, and assert JSON contains:

  ```json
  "accepted_scale_deadband_ratio": 0.020000,
  "accepted_scale_smoothing_alpha": 0.100000,
  "max_reacquisition_center_distance": 0.750000
  ```

  Mutate each metadata member to NaN or infinity in separate cases and assert `tracking_session_metadata_json` throws.

- [ ] **Step 2: Add failing board-schema positive and mutation fixtures**

  Add the three valid keys to the metadata fixture in `test_verify_board_schema.sh`. Add mutations proving `verify_board.sh` rejects:

  - each missing key;
  - deadband below `0` or at/above `1`;
  - alpha at/below `0` or above `1`;
  - reacquisition distance below `0`;
  - a non-finite value for each validation class.

- [ ] **Step 3: Run metadata and verifier tests and verify failure**

  Run:

  ```bash
  cmake --build /tmp/nanotrack-visible-scale-recovery-host \
    --target test_tracking_log -j4
  ctest --test-dir /tmp/nanotrack-visible-scale-recovery-host \
    -R '^(tracking_log|verify_board_schema)$' --output-on-failure
  ```

  Expected: serialization and verifier assertions fail because the new keys are not emitted or required yet.

- [ ] **Step 4: Carry effective settings through metadata JSON**

  Add the three members to `TrackingSessionMetadata`, copy them in `apply_controller_config_metadata`, include them in the finite-value check, and serialize them adjacent to `max_accepted_scale_step_ratio`.

- [ ] **Step 5: Require and validate all three settings in board evidence**

  In both metadata-validation blocks in `verify_board.sh`, require numeric finite values and enforce the exact ranges from Task 1. Keep `0` valid only for deadband and reacquisition distance, and keep schema version `9`.

- [ ] **Step 6: Re-run metadata and board-schema tests**

  Run the build and CTest commands from Step 3.

  Expected: `tracking_log` and `verify_board_schema` pass.

- [ ] **Step 7: Run the complete RKNN-independent host suite**

  Run:

  ```bash
  cmake --build /tmp/nanotrack-visible-scale-recovery-host -j4
  ctest --test-dir /tmp/nanotrack-visible-scale-recovery-host \
    --output-on-failure
  git diff --check
  ```

  Expected: the build succeeds, CTest reports 100% passing, and `git diff --check` emits no output. This is host logic evidence only, not RK3566 or pod acceptance.

- [ ] **Step 8: Record the implementation milestone within task ownership**

  Append an ISO-8601 entry to `docs/progress/cross-compile.md` with the branch/worktree, starting commit, implementation commits, exact host commands and observed results, changed visible values, evidence boundary, and next action. State explicitly that no cross-compilation, remote write, deployment, process change, board run, or pod trial was performed.

- [ ] **Step 9: Commit observability, validation, and owned progress**

  ```bash
  git add rk3566/nanotrack_v3/include/tracking_log.hpp \
    rk3566/nanotrack_v3/src/tracking_log.cpp \
    rk3566/nanotrack_v3/tests/test_tracking_log.cpp \
    rk3566/nanotrack_v3/scripts/verify_board.sh \
    rk3566/nanotrack_v3/tests/test_verify_board_schema.sh \
    docs/progress/cross-compile.md
  git commit -m "test: validate visible stabilization metadata"
  ```

- [ ] **Step 10: Verify the committed tree without changing external state**

  Run:

  ```bash
  git status --short
  git branch --show-current
  git rev-parse HEAD
  ctest --test-dir /tmp/nanotrack-visible-scale-recovery-host \
    --output-on-failure
  ```

  Expected: clean status, branch `codex/cross-compile-main-b827`, a new implementation HEAD, and the complete host suite still passing. Stop here and request separate authorization before any cross-compilation, remote copy, deployment, process control, or pod testing.
