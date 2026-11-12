# RK3588 Interactive One-Command Configuration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Run the final accepted RK3588 interactive tracker with one `--config` argument from the candidate root.

**Architecture:** Keep the existing runtime-config parser and encode the immutable board deployment paths and debug settings in the shipped RK3588 profile. Copy that exact profile to the candidate root and verify the real command reaches interactive readiness.

**Tech Stack:** C++17 CLI tests, strict key-value runtime config, RK3588/RKNN, OpenCV HighGUI.

**Spec:** `docs/superpowers/specs/2026-11-11-rk3588-interactive-one-command-design.md`

## Global Constraints

- Preserve `interactive=true`; do not add a fixed bbox.
- Use accepted C012 controller source `e865c78679247cb45ddbdbf2fb466e49930d5d12` and existing RK3588 model artifacts.
- Do not modify tracker/controller behavior or coordinator-owned progress files.
- A startup smoke proves configuration and model loading only, not mouse ROI or pod acceptance.

## Review Focus

- Running from the candidate root must find the config with the exact requested relative filename.
- Config-relative path resolution must not redirect the controller to `visible-default.conf`.
- Interactive mode must remain active with no preinitialized bbox.
- Detector model, manifest, and labels must remain a complete RK3588 triplet.
- Output paths must have an existing parent and not depend on shell cwd.

---

### Task 1: Update and deploy the interactive profile

**Files:**
- Modify: `rk3566/nanotrack_v3/config/rk3588-interactive-debug.conf`
- Modify: `rk3566/nanotrack_v3/tests/test_cli.cpp`
- Modify: `rk3566/nanotrack_v3/README.md`
- Modify: `docs/progress/board-validation.md`

**Interfaces:**
- Consumes: existing `load_runtime_config()` and `parse_options()` behavior.
- Produces: board-root `rk3588-interactive-debug.conf` and the exact one-argument launch command.

- [ ] **Step 1: Write the failing shipped-profile test**

Assert the profile remains interactive with no bbox, resolves the canonical video/tracker/detector paths, selects `visible-occlusion-calibrated.conf`, sets `max_frames=270`, and writes video/CSV/metadata to `/tmp/rk3588-interactive-debug.*`.

- [ ] **Step 2: Run RED**

Run: `cmake --build /tmp/nanotrack-occ-c010-host --target test_cli -j4 && ctest --test-dir /tmp/nanotrack-occ-c010-host -R '^cli$' --output-on-failure`

Expected: FAIL because the current shipped profile still selects `visible-default.conf` and omits outputs/frame limit.

- [ ] **Step 3: Update the profile and README**

Populate all non-ROI runtime values with the exact immutable RK3588 paths and document the two-line command plus mouse controls.

- [ ] **Step 4: Run GREEN and full host regression**

Run the focused command, all 19 CTests, and `git diff --check`.

Expected: focused and full suites PASS.

- [ ] **Step 5: Commit, deploy, and smoke-test**

Commit the profile/tests/docs, copy the committed profile to
`/home/neardi/Project/SOT/occ-candidate-e865c78/rk3588-interactive-debug.conf`,
verify SHA-256, and run the exact command under the active display with a bounded
timeout. Record only observed startup/model/config evidence in the owned progress
file and commit that evidence.
