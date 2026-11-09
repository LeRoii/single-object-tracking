# RK3588 Debug Deployment Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build fresh RK3588-target NanoTrack and VisDrone9 RKNN models from frozen ONNX sources, deploy the latest integrated `main@1bf657ff730891ae0bea48340a046cbcbeaa87d4` to the NEARDI LKD3588, and verify its Debug workflow without consuming any RK3566-target model.

**Architecture:** Host-side conversion tooling becomes explicitly target-aware while preserving RK3566 behavior and tests. The host exports deterministic NanoTrack ONNX graphs, converts all four models for `rk3588`, packages source/models/config/data with hashes, and the RK3588 performs the native build, CTest, model-load smoke, canonical-video run, and interactive Debug check in a new isolated directory.

**Tech Stack:** Python 3.10, PyTorch 1.7.1, ONNX 1.13.1, ONNX Runtime 1.14.1, RKNN-Toolkit2 2.3.2, C++17, CMake 3.22.1, OpenCV 4.5.4, Ubuntu 22.04/AArch64, RKNN Runtime/driver reported by the RK3588.

**Spec:** `docs/superpowers/specs/2026-11-09-rk3588-debug-deployment-design.md`

## Global Constraints

- Source baseline is exactly `main@1bf657ff730891ae0bea48340a046cbcbeaa87d4`; the design/plan commits are documentation-only descendants.
- Do not copy, deploy, or load any RK3566-target `.rknn` file on the RK3588.
- Generate NanoTrack template/search/head and VisDrone9 detector with RKNN-Toolkit2 `2.3.2`, `target_platform="rk3588"`, and no quantization.
- NanoTrack source is `248663fde6bf7c40190cf10ee396d5662919ecd3`; official V3 weight SHA-256 is `4f0d25108385a67bdec5fa01ca37c74acb72343b53996dd645fe18b86e512e69`.
- VisDrone9 RKOPT ONNX SHA-256 is `b2948072f01da7fe3b686392a47fe742380c98775c4149111527b7ddf26a5cc8`; detector remains a Debug candidate and never directly authorizes control.
- New non-training runtime evidence uses only `data/occ_video_1`; frames 169--181 remain excluded from ordinary IoU claims.
- RK3588 deployment root is `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f`; never overwrite an existing path.
- RK3588 results are migration/Debug evidence, not the contract's RK3566 board evidence, detector promotion, production handoff, cross-compile proof, or pod acceptance.
- Large ONNX/RKNN/log/video artifacts remain outside Git; commit only code, tests, configuration, provenance, hashes, and reproduction commands.

## Review Focus

- A caller requests an unsupported target or a manifest target differs from the requested board: fail before conversion/deployment; Task 1 tests `rk3399pro` rejection and expected-target mismatch.
- A stale RK3566 model enters the RK3588 package: Task 3's package audit rejects every `.rknn` whose manifest is not `rk3588` and requires freshly generated hashes.
- RK3588 system runtime and the 2.3.2 compile header are ABI-incompatible: Task 4 records runtime/driver versions and requires real model initialization, not link success.
- The tiny first-frame ROI is rounded out of frame or decoded incorrectly: Task 5 derives `992,582,37,39` from the frozen 1920x1080 label and checks the initialization row and finite in-frame outputs.
- GUI Debug starts but cannot select/play/exit cleanly: Task 5 runs a bounded interactive session with X11 input and retains the raw log and exit evidence separately from non-interactive results.

---

### Task 1: Generalize conversion and manifest validation for RK3588

**Files:**
- Create: `rk3566/nanotrack_v3/tools/rknn_targets.py`
- Modify: `rk3566/nanotrack_v3/tools/convert_rknn.py`
- Modify: `rk3566/nanotrack_v3/tools/model_manifest.py`
- Modify: `rk3566/nanotrack_v3/tools/convert_yolov8_rk3566.py`
- Modify: `rk3566/nanotrack_v3/tools/verify_detector_manifest.py`
- Modify: `rk3566/nanotrack_v3/tests/test_model_manifest.py`
- Modify: `rk3566/nanotrack_v3/tests/test_detector_manifest.py`
- Test: `rk3566/nanotrack_v3/tests/test_rknn_targets.py`

**Interfaces:**
- Consumes: target string supplied explicitly as `rk3566` or `rk3588`.
- Produces: `SUPPORTED_RKNN_TARGETS: tuple[str, ...]`, `require_supported_target(target: str) -> str`, target-aware `convert_one(onnx_path: Path, rknn_path: Path, target: str)`, `build_detector_manifest(..., target_platform: str, ...)`, and `load_and_verify_manifest(..., expected_target: str | None = None)`.

- [ ] **Step 1: Write target-contract tests**

  Add tests asserting `require_supported_target("rk3566")` and `("rk3588")` pass, `rk3399pro` fails, tracker manifests accept both supported targets, detector manifests record the requested target, and verifier `expected_target="rk3588"` rejects a valid RK3566 manifest.

- [ ] **Step 2: Run focused tests and verify RED**

  Run: `/home/u20/single-object-tracking/.venvs/yolov8-visdrone/bin/python -m pytest -q rk3566/nanotrack_v3/tests/test_rknn_targets.py rk3566/nanotrack_v3/tests/test_model_manifest.py rk3566/nanotrack_v3/tests/test_detector_manifest.py`

  Expected: FAIL because shared target support and target-aware signatures do not exist.

- [ ] **Step 3: Implement the shared target contract and thread it through both converters**

  `convert_rknn.py --target` and `convert_yolov8_rk3566.py --target` must use the explicit argument for `RKNN.config(target_platform=...)` and manifest generation. Keep the detector script filename for compatibility; update its description so it no longer claims RK3566-only behavior.

- [ ] **Step 4: Implement explicit manifest target verification**

  Accept either supported target structurally; when `expected_target` is supplied, require exact equality. Converter self-verification must pass its requested target rather than relying on a default.

- [ ] **Step 5: Run focused and complete Python regressions**

  Run the focused command from Step 2, then `/home/u20/single-object-tracking/.venvs/yolov8-visdrone/bin/python -m pytest -q rk3566/nanotrack_v3/tests`.

  Expected: focused tests PASS; existing detector/Python regression suite PASS with no RK3566 behavior regression.

- [ ] **Step 6: Commit Task 1**

  Commit only the shared target helper, converter/verifier changes, and their tests.

### Task 2: Make board verification and Debug configuration target-aware

**Files:**
- Modify: `rk3566/nanotrack_v3/scripts/verify_board.sh`
- Modify: `rk3566/nanotrack_v3/tests/test_verify_board_schema.sh`
- Modify: `rk3566/nanotrack_v3/CMakeLists.txt`
- Create: `rk3566/nanotrack_v3/config/rk3588-interactive-debug.conf`
- Modify: `rk3566/nanotrack_v3/tests/test_cli.cpp`

**Interfaces:**
- Consumes: `EXPECTED_RKNN_TARGET` environment value, defaulting to `rk3566`, and the standard runtime config parser.
- Produces: board verifier requiring the manifest target to equal `EXPECTED_RKNN_TARGET`; RK3588 Debug config resolving video, models, controller config, and detector files relative to its own location.

- [ ] **Step 1: Add failing verifier and config tests**

  Extend the shell fixture to show an RK3588 manifest fails under the default and passes only with `EXPECTED_RKNN_TARGET=rk3588`. Add a CLI test loading `config/rk3588-interactive-debug.conf` and asserting `interactive=true`, canonical video `../data/occ_video_1/1.mp4`, tracker model directory `../models`, and detector release `yolov8n-visdrone9-rk3588-fp16-v1`.

- [ ] **Step 2: Run host CTest and verify RED**

  Run: `cmake -S rk3566/nanotrack_v3 -B /tmp/nanotrack-rk3588-plan-host -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF`, `cmake --build /tmp/nanotrack-rk3588-plan-host -- -j2`, and `ctest --test-dir /tmp/nanotrack-rk3588-plan-host --output-on-failure`.

  Expected: new target/config assertions fail before implementation.

- [ ] **Step 3: Implement verifier target input and RK3588 Debug config**

  Validate `EXPECTED_RKNN_TARGET` against the same two supported literal values in the shell verifier. Change only the CMake option description from RK3566-specific to generic RKNN/OpenCV wording; do not rename the option or source directory.

- [ ] **Step 4: Run clean host build, full CTest, and diff checks**

  Expected: complete RKNN-independent CTest passes and `git diff --check` is clean.

- [ ] **Step 5: Commit Task 2**

  Commit verifier, Debug config, CMake wording, and tests together.

### Task 3: Export ONNX and generate fresh RK3588 model package

**Files:**
- Generate outside Git: `/home/u20/single-object-tracking/artifacts/rk3588-debug-deploy/nanotrack/{template_backbone.onnx,search_backbone.onnx,head.onnx,export_metadata.json,template_backbone.rknn,search_backbone.rknn,head.rknn,manifest.json}`
- Generate outside Git: `/home/u20/single-object-tracking/artifacts/rk3588-debug-deploy/detector/{yolov8n-visdrone9-rk3588-fp16.rknn,detector-manifest.json,visdrone9.txt}`
- Generate outside Git: `/home/u20/single-object-tracking/artifacts/rk3588-debug-deploy/SHA256SUMS`

**Interfaces:**
- Consumes: official NanoTrack repo/config/weight and frozen VisDrone9 RKOPT ONNX/training provenance.
- Produces: four RK3588-target RKNN models plus two self-consistent manifests and a package checksum list.

- [ ] **Step 1: Revalidate all immutable inputs**

  Require NanoTrack repo HEAD `248663fde6bf7c40190cf10ee396d5662919ecd3`, weight hash `4f0d251...e512e69`, VisDrone9 ONNX hash `b294807...a5cc8`, and Toolkit2 version `2.3.2`. Stop before output creation on any mismatch.

- [ ] **Step 2: Export deterministic NanoTrack ONNX graphs**

  Run `tools/export_v3.py` with `/home/u20/single-object-tracking/.venvs/nanotrack/bin/python`, seed `20260929`, the frozen author repo/config/weight, and the dedicated Task 3 output directory.

  Expected: three ONNX checker passes and ONNX Runtime parity within `atol=rtol=1e-4`; record each ONNX SHA-256.

- [ ] **Step 3: Convert NanoTrack graphs for RK3588**

  Run `tools/convert_rknn.py --target rk3588` with source commit and weight hash.

  Expected: three new RKNN files; `manifest.json.provenance.target_platform == "rk3588"`; every model hash matches disk.

- [ ] **Step 4: Convert VisDrone9 detector for RK3588**

  Run the generalized detector converter with `--target rk3588`, output name `yolov8n-visdrone9-rk3588-fp16.rknn`, the frozen RKOPT ONNX, labels, source/training commit `4674fe6e003dfbc5f2250d3b39dd31faaf7a9877`, checkpoint, args, and dataset audit.

  Expected: manifest target `rk3588`, nine declared outputs, nine labels, FP16/no quantization, exact ONNX provenance.

- [ ] **Step 5: Audit the package against accidental RK3566 consumption**

  Verify all `.rknn` mtimes postdate this task, both manifests declare `rk3588`, hashes match, filenames contain no `rk3566`, and no tracked RK3566 `.rknn` path is copied into the package. Write `SHA256SUMS` only after the audit passes.

### Task 4: Transfer and build the isolated RK3588 Debug deployment

**Files:**
- Create on RK3588: `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/`
- Create on RK3588: `build/`, `models/`, `releases/yolov8n-visdrone9-rk3588-fp16-v1/models/`, `data/occ_video_1/`, and `runs/`
- Retain on RK3588: source archive, transfer checksum file, `configure.log`, `build.log`, `ctest.log`

**Interfaces:**
- Consumes: exact `main@1bf657f` source subtree, Task 3 models/manifests, RKNN 2.3.2 header, canonical dataset, and RK3588 system `/usr/lib/librknnrt.so` plus OpenCV 4.5.4.
- Produces: isolated native `build/nanotrack_v3` and `build/yolo_detector_smoke` with recorded hashes and test logs.

- [ ] **Step 1: Record RK3588 pre-deployment state**

  Capture hostname/model/kernel/time, compiler/CMake/OpenCV versions, `/usr/lib/librknnrt.so` hash, NPU device permissions, free disk, and whether the destination already exists. If it exists, choose a new suffixed directory; never overwrite it.

- [ ] **Step 2: Package and transfer exact inputs**

  Archive the committed `rk3566/nanotrack_v3` source after Tasks 1--2, recording both its base `1bf657f` and resulting task commit, then add the Task 3 generated RK3588 models, canonical `occ_video_1`, and the compile header as separate hashed inputs. Verify `SHA256SUMS` on the RK3588 before configuration.

- [ ] **Step 3: Configure and build natively**

  Configure with `BUILD_TESTING=ON`, `ENABLE_RKNN_RUNTIME=ON`, RKNN include directory from the transferred 2.3.2 header, `RKNN_LIBRARY=/usr/lib/librknnrt.so`, and system OpenCV 4.5.4. Build with `-j2`, retaining complete logs.

- [ ] **Step 4: Run complete CTest**

  Run `ctest --test-dir build --output-on-failure`.

  Expected: every configured test passes. Record executable SHA-256 and `ldd`; link success alone is not model/runtime evidence.

### Task 5: Prove model load, canonical-video runtime, and interactive Debug

**Files:**
- Create on RK3588: `runs/detector-smoke/`
- Create on RK3588: `runs/occ-smoke/{run.log,bboxes.csv,session-metadata.json,output.avi}`
- Create on RK3588: `runs/interactive-debug/{run.log,bboxes.csv,session-metadata.json}`

**Interfaces:**
- Consumes: Task 4 executables and freshly converted RK3588 models only.
- Produces: separate detector, non-interactive, and GUI Debug evidence with runtime/driver versions and artifact hashes.

- [ ] **Step 1: Run detector single-image smoke**

  Extract frame 0 from canonical `occ_video_1/1.mp4`, use the RK3588 detector model/manifest/labels, confidence `0.25`, NMS `0.45`, and VisDrone9 `car` class ID `2`. Require model initialization, inference completion, and retained annotated image/CSV; a zero-detection result may still prove execution but must not be described as detector quality success.

- [ ] **Step 2: Run bounded canonical-video smoke**

  Run `nanotrack_v3` on `occ_video_1/1.mp4` with bbox `992,582,37,39`, `--max-frames 60`, tracker model directory, visible controller config, RK3588 detector triplet, output video/boxes/metadata, and line-buffered logs.

  Expected: exit 0, initialization plus 60 processed rows, finite in-frame boxes, decodable output, non-empty API/driver versions, and no detector-direct control authorization.

- [ ] **Step 3: Run the board verifier for RK3588**

  Invoke `EXPECTED_RKNN_TARGET=rk3588 scripts/verify_board.sh` with the canonical input and generated artifacts.

  Expected: manifest target and file hashes match, row/frame counts are correct, safety fields are coherent, and the verifier exits 0.

- [ ] **Step 4: Run bounded interactive Debug**

  Launch `nanotrack_v3 --config config/rk3588-interactive-debug.conf` under the active RK3588 display, supply the same explicit bbox, save CSV/metadata/log, send play then quit through X11 input, and bound the run with a timeout.

  Expected: window creation succeeds, playback advances, structured Debug overlay/log fields appear, and quit exits normally. Record failures separately; do not substitute non-interactive success for GUI proof.

- [ ] **Step 5: Hash and summarize retained RK3588 evidence**

  Hash the source archive, four RKNN models, two manifests, labels, configs, executables, logs, CSV/JSON/video, and input data identities. Report exact commands and evidence boundaries.

### Task 6: Record the RK3588 migration milestone

**Files:**
- Modify: `docs/progress/board-validation.md`

**Interfaces:**
- Consumes: Tasks 1--5 commits and observed RK3588 artifacts.
- Produces: durable migration record and coordinator handoff request without changing public contract files.

- [ ] **Step 1: Update the owned progress file**

  Add an ISO 8601 `+08:00` entry with worktree, base/result commits, RK3588 identity, source/model/input/binary hashes, exact conversion/build/run commands, observed results, retained board paths, blockers, and next action.

- [ ] **Step 2: State the evidence boundary and Handoff**

  Explicitly state that RK3588 Debug migration does not satisfy the contract's RK3566 board gate, detector promotion, production handoff, cross-compile, long-stability, reacquisition-quality, calibration, or pod acceptance. Ask the coordinating chat to decide whether RK3588 should become a separately named target in the integration contract.

- [ ] **Step 3: Run final verification**

  Run focused Python tests, full `rk3566/nanotrack_v3/tests`, clean host CTest, `git diff --check`, confirm the worktree contains only intended tracked changes, and recheck the RK3588 evidence hashes.

- [ ] **Step 4: Commit the milestone**

  Commit code/tests/config/progress only; leave generated models and logs outside Git. Report the resulting commit for coordinator integration.
