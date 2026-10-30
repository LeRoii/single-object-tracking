# RK3566 Production Deployment Config Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make production model paths configurable, rebuild the A6 candidate with the approved VisDrone9 functional detector, and stage a self-contained candidate for bounded board startup.

**Architecture:** Add a strict, testable deployment-config parser to the tracked NanoTrack core and call it from the WSL-only C ABI adapter. The adapter keeps only the deployment-config file location fixed; all tracker, controller, detector, manifest and labels paths come from that file. Deployment remains isolated under `/app/nanotrack_b827_candidate`.

**Tech Stack:** C++17, CMake/CTest, OpenCV, RKNN runtime, WSL AArch64 Buildroot toolchain, SSH/SCP.

**Spec:** `docs/superpowers/specs/2026-10-30-rk3566-production-deployment-config-design.md`

## Global Constraints

- Preserve the existing C ABI, RGB888 1920x1080 stride 5760 contract, output semantics and non-concurrent call assumption.
- Do not include CLI, HighGUI, DISPLAY, keyboard/mouse, debug telemetry, tracking logs or video/CSV output in production.
- Do not overwrite `/app/bin/viewlink_app`, `/app/viewlink_appnt3` or `/app/model`.
- Use the three u20 NanoTrack RKNN files only when their SHA-256 values match the spec.
- Use the approved VisDrone9 RKNN/manifest/labels identity from the spec only for functional startup.
- Model/config failure makes `tracker_detector_init()` return `false`; no silent fallback.
- Board startup must be bounded and must not run concurrently with an existing `viewlink_app` process.
- A build or bounded startup is not tracking, reacquisition or gimbal-pod acceptance.

## Review Focus

- Config with an unknown key must fail instead of silently ignoring a typo; Task 1 tests it.
- Config with a duplicated key must fail instead of letting the last value win; Task 1 tests it.
- Missing or empty required paths must fail before RKNN initialization; Task 1 tests both cases.
- A tracker directory missing one of the three RKNN files must fail validation; Task 1 tests it.
- A VisDrone manifest/labels mismatch must remain visible as initialization failure rather than falling back; Task 2 verifies exact hashes and startup logs.

---

### Task 1: Strict production deployment config parser

**Files:**
- Create: `rk3566/nanotrack_v3/include/production_deployment_config.hpp`
- Create: `rk3566/nanotrack_v3/src/production_deployment_config.cpp`
- Create: `rk3566/nanotrack_v3/tests/test_production_deployment_config.cpp`
- Modify: `rk3566/nanotrack_v3/CMakeLists.txt`
- Modify: `docs/progress/cross-compile.md`

**Interfaces:**
- Produces: `nanotrack::ProductionDeploymentConfig` with five string paths.
- Produces: `ProductionDeploymentConfig load_production_deployment_config(const std::string& path)`.
- Produces: `void validate_production_deployment_config(const ProductionDeploymentConfig&)`.

- [ ] **Step 1: Write failing parser tests**

Add tests named `loads_complete_config`, `rejects_unknown_key`, `rejects_duplicate_key`, `rejects_missing_key`, `rejects_empty_value`, `rejects_missing_tracker_model`, and `rejects_missing_detector_artifact`. Use a temporary directory with placeholder tracker/detector/config/manifest/labels files.

- [ ] **Step 2: Run the focused test and verify RED**

Run: `cmake -S rk3566/nanotrack_v3 -B /tmp/nanotrack-deployment-config -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF && cmake --build /tmp/nanotrack-deployment-config -j4 && ctest --test-dir /tmp/nanotrack-deployment-config -R production_deployment_config --output-on-failure`

Expected: build or test failure because the parser interface is not implemented.

- [ ] **Step 3: Implement the strict parser**

Parse one `key=value` pair per line with `#` comments and whitespace trimming. Require exactly `tracker_model_dir`, `controller_config_path`, `detector_model_path`, `detector_manifest_path`, and `detector_labels_path`; reject unknown, duplicate, missing and empty values. Validate that the tracker directory contains `template_backbone.rknn`, `search_backbone.rknn`, and `head.rknn`, and that all other configured paths are regular files.

- [ ] **Step 4: Run focused and full host tests**

Run the focused command from Step 2, then `ctest --test-dir /tmp/nanotrack-deployment-config --output-on-failure`.

Expected: focused test passes and the full suite reports zero failures.

- [ ] **Step 5: Commit Task 1**

```bash
git add rk3566/nanotrack_v3/include/production_deployment_config.hpp \
  rk3566/nanotrack_v3/src/production_deployment_config.cpp \
  rk3566/nanotrack_v3/tests/test_production_deployment_config.cpp \
  rk3566/nanotrack_v3/CMakeLists.txt docs/progress/cross-compile.md
git commit -m "feat: add production deployment config parser"
```

### Task 2: Wire WSL production adapter and rebuild A6

**Files:**
- Modify remotely: `ai_alg/source/native-lib_impl.cpp`
- Modify remotely: `ai_alg/CMakeLists.txt`
- Copy remotely: `ai_alg/nanotrack/include/production_deployment_config.hpp`
- Copy remotely: `ai_alg/nanotrack/source/production_deployment_config.cpp`
- Create remotely: `deployment_b827/config/production-deployment.conf`
- Update: `docs/progress/cross-compile.md`

**Interfaces:**
- Consumes: Task 1 parser API.
- Produces: A6 `viewlink_app` whose tracker/controller/detector paths come from `/app/nanotrack_b827_candidate/config/production-deployment.conf`.

- [ ] **Step 1: Extend the existing production staging check to require config loading and reject old hard-coded model constants**

Expected RED: current staged adapter still contains `kModelDir`, `kControllerConfig`, `kDetectorModel`, `kDetectorManifest`, and `kDetectorLabels`.

- [ ] **Step 2: Patch the adapter and CMake in the isolated staging area**

Keep only `kDeploymentConfig=/app/nanotrack_b827_candidate/config/production-deployment.conf`. Load the five paths before constructing `NanoTrackV3`, `ControllerConfig`, and `YoloRknnEngine`; add `production_deployment_config.cpp` to `libAI.a`.

- [ ] **Step 3: Stage exact model/config artifacts and verify SHA-256**

Copy the three NanoTrack models and approved VisDrone9 RKNN/manifest/labels plus `visible-default.conf` into the WSL deployment staging tree. Generate `production-deployment.conf` with the exact candidate paths from the spec. Require all seven artifact hashes to match before build/deployment.

- [ ] **Step 4: Sync the production whitelist to WSL and run A6 build**

Run: `BUILD_JOBS=4 ./run.sh --platform A6`

Expected: exit 0; `viewlink_app` is ARM aarch64; `libAI.a` contains `production_deployment_config.cpp.o`, `target_class.cpp.o`, and real C ABI symbols, with no debug/interactive objects.

- [ ] **Step 5: Record output identities and commit Task 2**

Record source commit, WSL backup/path, config/model hashes, command, artifact size/SHA-256, RKNN runtime risk and evidence boundary in `docs/progress/cross-compile.md`; the tracked parser code remains bound to the Task 1 commit.

```bash
git add docs/progress/cross-compile.md
git commit -m "docs: record configurable A6 candidate build"
```

### Task 3: Isolated board staging and bounded startup

**Files:**
- Output only: `/app/nanotrack_b827_candidate/**`
- Update: `docs/progress/cross-compile.md`

**Interfaces:**
- Consumes: Task 2 A6 binary and exact deployment tree.
- Produces: board-side file/hash inventory and bounded startup log, not an acceptance verdict.

- [ ] **Step 1: Read-only board preflight**

Confirm board identity, free space, no running `viewlink_app`, and unchanged existing `/app/bin/viewlink_app`, `/app/viewlink_appnt3`, and `/app/model` identities.

- [ ] **Step 2: Copy candidate tree without replacing existing application files**

Use a temporary sibling directory, verify all hashes, then rename it to `/app/nanotrack_b827_candidate`. If the existing candidate directory differs, preserve it as a timestamped backup before replacement.

- [ ] **Step 3: Run a bounded foreground smoke**

Run from `/app` with a short timeout and retained stdout/stderr log. Stop immediately on missing libraries, model-version errors, RKNN initialization errors, or hardware/resource conflict. Do not create an autostart service or replace production links.

- [ ] **Step 4: Verify post-run state and record evidence**

Confirm no candidate process remains, existing application files are unchanged, and record exact command, exit/timeout status, runtime/driver/model messages and retained log path.

- [ ] **Step 5: Commit Task 3 evidence**

```bash
git add docs/progress/cross-compile.md
git commit -m "docs: record isolated candidate startup"
```
