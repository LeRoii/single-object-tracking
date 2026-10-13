# RK3566 YOLOv8 Detector Integration Implementation Plan

> **For Codex:** Implement this plan task-by-task only after the user selects an execution approach. Use `superpowers:test-driven-development` for each code task, `superpowers:systematic-debugging` for unexpected failures, and `superpowers:verification-before-completion` before any completion claim.

**Goal:** Add a clean, optional YOLOv8n detector to `rk3566/nanotrack_v3` so the existing NanoTrack long-term controller can receive bounded, timestamped full-frame recovery candidates on a real RK3566 board.

**Architecture:** Keep detector inference behind the existing adapter boundary. A host-testable single-worker adapter owns copied frames and calls an injected engine; a pure C++ YOLOv8 DFL postprocessor decodes tensors; the board-only RKNN engine owns model/runtime resources and OpenCV preprocessing. Detector output remains a search hypothesis and can never bypass the controller's freshness, NanoTrack verification, or 2-of-3 recovery confirmation.

**Tech Stack:** C++17, CMake/CTest, OpenCV, RKNN Runtime 2.3.2, RKNN-Toolkit2 2.3.2, Python 3, JSON manifests, Bash deployment/verification scripts.

**Spec:** `docs/superpowers/specs/2026-10-13-rk3566-yolov8-detector-design.md`

## Global constraints

- Work only in `rk3566/nanotrack_v3`; do not modify or link `ancient-tracker/rknn_yolov8_detector-640384`.
- Do not copy any existing RK3588 detector model into the RK3566 project.
- Preserve detector-free CLI behavior and the existing NanoTrack model files.
- Keep all queues bounded to one pending request and one completed result.
- A detector candidate is never an accepted tracking result until the existing NanoTrack verification and 2-of-3 controller rule pass.
- Treat confidence/NMS values as uncalibrated bring-up defaults and write `calibrated=false` into metadata.
- Do not claim deployment from ONNX export, RKNN conversion, compilation, or model loading alone. Real board inference and retained outputs are required.
- The workspace currently does not expose a valid Git worktree. Instead of commits, append each RED/GREEN/refactor command and outcome to `docs/superpowers/plans/2026-10-13-rk3566-yolov8-detector-progress.md`. Do not initialize or repair Git without explicit user authorization.

## Public contracts to implement

```cpp
// include/detector_adapter.hpp
class DetectorAdapter {
 public:
  virtual ~DetectorAdapter() = default;
  virtual bool available() const = 0;
  virtual bool can_submit() const = 0;
  virtual void submit(const ImageView&, const DetectorRequest&) = 0;
  virtual std::optional<DetectorBatch> poll() = 0;
};
```

```cpp
// include/long_term_types.hpp
struct DetectorTiming {
  float preprocess_ms = 0.0F;
  float inference_ms = 0.0F;
  float postprocess_ms = 0.0F;
  float total_ms = 0.0F;
};

struct DetectorBatch {
  std::uint64_t source_frame_id = 0;
  std::int64_t source_timestamp_ms = 0;
  std::size_t raw_candidate_count = 0;
  std::size_t filtered_candidate_count = 0;
  DetectorTiming timing{};
  std::vector<DetectorCandidate> candidates;
};

struct DetectorDiagnostics {
  bool configured = false;
  bool ready = false;
  bool request_submitted = false;
  bool batch_received = false;
  std::uint64_t request_frame_id = 0;
  std::uint64_t batch_frame_id = 0;
  std::int64_t batch_age_ms = 0;
  std::size_t raw_candidate_count = 0;
  std::size_t filtered_candidate_count = 0;
  DetectorTiming timing{};
  std::string error;
};
```

```cpp
// include/yolo_detector_adapter.hpp
struct DetectorEngineResult {
  std::vector<DetectorCandidate> candidates;
  std::size_t raw_candidate_count = 0;
  std::size_t filtered_candidate_count = 0;
  DetectorTiming timing{};
};

class DetectorEngine {
 public:
  virtual ~DetectorEngine() = default;
  virtual DetectorEngineResult infer(const ImageView&, const DetectorRequest&) = 0;
};
```

`YoloDetectorAdapter` owns the copied BGR bytes and recreates an `ImageView` over that owned storage inside its worker. The engine interface may accept a view because the adapter guarantees that ownership until `infer()` returns.

## Review focus

1. Verify `begin_frame()` only records a detector request when `available() && can_submit()` is true; a slow request must not be overwritten by newer request identities.
2. Verify a result that completes after newer video frames is rejected by the existing 150 ms logical-video-time freshness rule, independent of wall-clock worker latency.
3. Verify copying a non-contiguous BGR source honors `stride_bytes`; no worker may retain the caller's frame pointer or ROI memory.
4. Verify destruction while inference is active cannot deadlock, detach, leak, or use freed state.
5. Verify 6-output and 9-output tensor contracts are explicit and deterministic; shape/order/class mismatches must fail instead of being guessed.
6. Verify same-class, high-confidence detections still pass NanoTrack anchor/recent verification and 2-of-3 confirmation before `control_valid=true`.

---

### Task 1: Create the execution evidence ledger and lock the detector contract

**Files:**

- Create: `docs/superpowers/plans/2026-10-13-rk3566-yolov8-detector-progress.md`
- Modify: `rk3566/nanotrack_v3/include/detector_adapter.hpp`
- Modify: `rk3566/nanotrack_v3/include/long_term_types.hpp`
- Modify: `rk3566/nanotrack_v3/include/long_term_runtime.hpp`
- Modify: `rk3566/nanotrack_v3/src/long_term_runtime.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_long_term_runtime.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_controller_support.cpp`

- [ ] Create the progress ledger with sections for environment, RED evidence, GREEN evidence, board evidence, artifacts, deviations, and unresolved risks. Record the current non-Git status.
- [ ] Add failing tests proving `NullDetectorAdapter::can_submit()` is false and a busy fake adapter does not cause `SearchPlan::request_detector` or advance the controller's detector schedule.
- [ ] Add a failing runtime test where an adapter accepts one request, remains busy over later frames, then returns the original batch; assert source identity is preserved and no second submission occurs.
- [ ] Add a failing runtime test that forces `poll()` to throw once; assert the frame contains the error, subsequent frames continue NanoTrack, and no detector request is made after the adapter reports unavailable.
- [ ] Run only the affected tests and record the expected RED output:

  ```bash
  cmake -S rk3566/nanotrack_v3 -B /tmp/nanotrack-yolo-host -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF
  cmake --build /tmp/nanotrack-yolo-host --target test_long_term_runtime test_controller_support -j2
  ctest --test-dir /tmp/nanotrack-yolo-host -R 'long_term_runtime|controller_support' --output-on-failure
  ```

- [ ] Add `can_submit()` to the base and null adapters. Pass `detector_.available() && detector_.can_submit()` into `LongTermController::begin_frame()`.
- [ ] Add `DetectorTiming`, expanded `DetectorBatch`, and `DetectorDiagnostics`. Replace the ambiguous `detector_ms` field in `RuntimeFrameResult` with diagnostics while retaining `runtime_ms` and `tracker_ms`.
- [ ] Make runtime diagnostics distinguish configured, ready, submitted, received, error, request frame, batch frame, age, counts, and engine timings. Do not use diagnostic fields for controller decisions.
- [ ] Run the targeted tests again and record GREEN output.
- [ ] Run all existing host tests to catch ABI/behavior regressions and record the result.
- [ ] Review the diff against public contracts above; document any necessary signature deviation before proceeding.

### Task 2: Implement the bounded asynchronous adapter with a fake engine

**Files:**

- Create: `rk3566/nanotrack_v3/include/yolo_detector_adapter.hpp`
- Create: `rk3566/nanotrack_v3/src/yolo_detector_adapter.cpp`
- Create: `rk3566/nanotrack_v3/tests/test_yolo_detector_adapter.cpp`
- Modify: `rk3566/nanotrack_v3/CMakeLists.txt`

- [ ] Write failing tests for construction with a valid fake engine, initial `available=true`, and `can_submit=true`.
- [ ] Write a failing stride-copy test using padded rows. Mutate/free the caller buffer immediately after `submit()` and assert the fake engine sees the original active pixels without padding corruption.
- [ ] Write failing tests that a second submit while pending, running, or completed-but-unpolled is rejected with `std::logic_error`; there must never be more than one copied frame plus one result.
- [ ] Write failing tests for non-blocking empty `poll()`, identity-preserving completed `poll()`, and `can_submit=true` only after the result is consumed.
- [ ] Write a failing exception test: fake `infer()` throws, the next `poll()` throws exactly once with the original stage text, `available=false`, and later `poll()` returns empty.
- [ ] Write a failing destructor test with fake inference blocked by a condition variable. Release it from the test, then assert destruction joins the worker and no work continues afterward.
- [ ] Run the new target and record RED output.
- [ ] Implement `DetectorEngine` and `YoloDetectorAdapter` with one joinable worker, one pending request, one completed result, a mutex, and a condition variable.
- [ ] Validate BGR8 dimensions, ROI, stride, and byte-count overflow before allocation. Copy rows using `width * channels`, not `stride_bytes` as active width.
- [ ] Ensure shutdown waits for in-flight inference but never holds the adapter mutex while calling the engine or joining the thread.
- [ ] Build and run the adapter test until GREEN, then run it under AddressSanitizer/UndefinedBehaviorSanitizer in a separate host build.
- [ ] Run the full host CTest suite and record outcomes in the ledger.

### Task 3: Implement and exhaustively test the pure YOLOv8 DFL postprocessor

**Files:**

- Create: `rk3566/nanotrack_v3/include/yolov8_postprocess.hpp`
- Create: `rk3566/nanotrack_v3/src/yolov8_postprocess.cpp`
- Create: `rk3566/nanotrack_v3/tests/test_yolov8_postprocess.cpp`
- Modify: `rk3566/nanotrack_v3/CMakeLists.txt`

**Required interface concepts:** immutable tensor views with explicit type, shape, data, scale, and zero point; three scale descriptors pairing box/class/optional score-sum tensors; explicit decoder options containing input dimensions, confidence, NMS, class filter, maximum candidates, and letterbox transform.

- [ ] Add failing literal-value tests for DFL softmax and box decoding on each of the three strides. Expected boxes must be hand-computed constants, not values produced by the implementation under test.
- [ ] Add failing tests proving equivalent FP32 and INT8 tensors decode to the same class, confidence, and box within declared tolerances.
- [ ] Add failing tests for both official 9-output semantics and supported 6-output semantics. Tensor roles must be bound explicitly by validated shapes, not incidental vector order.
- [ ] Add failing tests for confidence filtering, `class_id=-1`, one explicit class, invalid class IDs, per-class NMS, descending confidence order, and `max_candidates` truncation.
- [ ] Add failing tests for letterbox inverse mapping with horizontal and vertical padding, non-zero detector ROI offsets, clipping, zero-area rejection, and boxes fully outside the ROI.
- [ ] Add failing tests for unsupported tensor counts/types/layouts, inconsistent grids/classes/DFL lengths, null buffers, non-finite values, invalid quantization scales, and arithmetic overflow.
- [ ] Run the postprocessor target and record RED output.
- [ ] Implement the decoder as pure C++17 with no RKNN, OpenCV, global mutable state, or model-specific absolute paths.
- [ ] Keep allocation bounded and apply score filtering before DFL decoding where the output contract permits it.
- [ ] Run the postprocessor target until GREEN, then run with sanitizers.
- [ ] Run the complete host suite and record output.

### Task 4: Extend CLI, metadata, and schema-v3 tracking logs

**Files:**

- Modify: `rk3566/nanotrack_v3/include/cli.hpp`
- Modify: `rk3566/nanotrack_v3/src/cli.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_cli.cpp`
- Modify: `rk3566/nanotrack_v3/include/tracking_log.hpp`
- Modify: `rk3566/nanotrack_v3/src/tracking_log.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_tracking_log.cpp`
- Modify: `rk3566/nanotrack_v3/README.md`

- [ ] Add failing CLI tests for all detector parameters omitted, all three file parameters supplied, and every partial combination rejected.
- [ ] Add failing CLI tests for default class `-1`, conf `0.25`, NMS `0.45`; finite open interval `(0,1)` validation; and class IDs below `-1` rejected.
- [ ] Add failing regression tests showing existing detector-free commands parse unchanged.
- [ ] Add failing log tests for schema version 3 and exact detector columns: configured, ready, requested, request frame, worker/error status, batch frame/age, raw/filtered counts, verified candidate class/confidence, segmented timing, and total timing.
- [ ] Add failing metadata tests for detector file paths/hashes, class/conf/NMS, `calibrated=false`, tensor descriptions, API/driver versions, and request/complete/reject/error counters.
- [ ] Implement CLI validation without checking model internals; manifest and tensor validation belongs to engine startup.
- [ ] Implement CSV escaping/error-text handling so commas/newlines cannot corrupt rows.
- [ ] Increment the schema version exactly once and update README examples for enabled and disabled detector modes.
- [ ] Run targeted tests until GREEN, then the full host suite. Record commands and exact results.

### Task 5: Build a reproducible RK3566 model conversion and manifest pipeline

**Files:**

- Create: `rk3566/nanotrack_v3/tools/convert_yolov8_rk3566.py`
- Create: `rk3566/nanotrack_v3/tools/verify_detector_manifest.py`
- Create: `rk3566/nanotrack_v3/tests/test_detector_manifest.py`
- Create during execution: `rk3566/nanotrack_v3/models/coco80.txt`
- Create during execution: `rk3566/nanotrack_v3/models/detector-manifest.json`
- Create during execution: `rk3566/nanotrack_v3/models/yolov8n-rk3566-fp16.rknn`
- Retain outside source control or document location: downloaded official `yolov8n.onnx`
- Modify: `rk3566/nanotrack_v3/requirements-convert.txt`

- [ ] Pin conversion dependencies and record the exact Python executable, package versions, host OS, source URL, and official repository commit in the ledger.
- [ ] Add failing Python tests for manifest required keys, SHA-256 verification, `target_platform == "rk3566"`, `quantized == false`, RGB input, 640x640 shape, pad 114, labels hash, `calibrated=false`, and six/nine declared tensor roles.
- [ ] Add failing tests for wrong/missing hashes, RK3588 target, missing labels, unsupported tensor layout, and malformed JSON.
- [ ] Implement the verifier first and make its tests GREEN without importing RKNN.
- [ ] Implement conversion CLI with delayed `rknn.api` import so `--help` and manifest validation work even when RKNN Toolkit is unavailable.
- [ ] Before conversion, explicitly test the pinned Toolkit2 2.3.2 import. If it hangs or fails, stop this task and use systematic debugging; do not silently switch toolkit versions.
- [ ] Download the official optimized Rockchip YOLOv8n ONNX and COCO labels, then record URLs, commit, sizes, and SHA-256. Do not substitute an Ultralytics output with a different tensor contract.
- [ ] Convert with `target_platform='rk3566'`, `do_quantization=False`, mean 0, std 255, and a 640x640 RGB input. Save the full conversion log.
- [ ] Generate the manifest from explicit conversion inputs and inspect the RKNN artifact for RK3566 target evidence. Run the manifest verifier.
- [ ] Retain ONNX, RKNN, manifest, labels, conversion log, and their SHA-256 values. Mark conversion complete but board inference unverified.

### Task 6: Implement the board-only RKNN engine and executable integration

**Files:**

- Create: `rk3566/nanotrack_v3/include/yolo_rknn_engine.hpp`
- Create: `rk3566/nanotrack_v3/src/yolo_rknn_engine.cpp`
- Create: `rk3566/nanotrack_v3/src/yolo_detector_smoke.cpp`
- Modify: `rk3566/nanotrack_v3/src/main.cpp`
- Modify: `rk3566/nanotrack_v3/CMakeLists.txt`
- Modify: `rk3566/nanotrack_v3/scripts/deploy_board.sh`
- Modify: `rk3566/nanotrack_v3/scripts/verify_board.sh`
- Modify: `rk3566/nanotrack_v3/README.md`

- [ ] Add a board-testable engine contract check that loads model plus manifest and rejects target, hash, input shape/type/layout/color, output count/shape/role, or class-count mismatches before the first frame.
- [ ] Implement RAII wrappers for model bytes, `rknn_context`, queried attributes, per-call output buffers, and preprocessing buffers. Every RKNN return code must be checked and included in a stage-specific exception.
- [ ] Implement validated ROI crop, BGR-to-RGB conversion, aspect-preserving 640x640 resize, `(114,114,114)` padding, inference, tensor-view construction, pure postprocessor invocation, and output release on all paths.
- [ ] Do not set an RK3588 three-core mask and do not create a new context per request.
- [ ] Add `nanotrack_yolov8_postprocess` as an always-buildable host library and `nanotrack_yolo_detector` only under `ENABLE_RKNN_RUNTIME=ON`. Link no code or library from the old detector project.
- [ ] Wire `main.cpp` to construct `NullDetectorAdapter` when all detector options are absent and `YoloRknnEngine` plus `YoloDetectorAdapter` when present.
- [ ] Add a standalone `yolo_detector_smoke` executable accepting model, manifest, labels, image, thresholds, class filter, annotated-image path, and candidate-CSV path. It must print tensor attributes and fail if no output artifact is written.
- [ ] Update deployment to use a new isolated board directory, `/home/cat/Project/SOT/nanotrack_v3_longterm`, and copy detector artifacts without modifying `/home/cat/Project/SOT/nanotrack_v3`.
- [ ] Extend board verification to validate hashes and schema version, while keeping detector-disabled verification available.
- [ ] Perform a clean host logic build and a clean board-native build. Compilation alone is not a pass for this task; record the binaries and linked RKNN/OpenCV libraries for the next task.

### Task 7: Prove the full recovery data path in deterministic integration tests

**Files:**

- Modify: `rk3566/nanotrack_v3/tests/test_long_term_controller.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_long_term_runtime.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_long_term_replay.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_tracking_log.cpp`

- [ ] Add a failing replay where one high-confidence detector candidate arrives. Assert it becomes only a detector hypothesis and `control_valid` stays false.
- [ ] Add a failing replay with consistent candidate geometry and reliable NanoTrack verification at two of three confirmation instants. Assert control becomes valid only after confirmation.
- [ ] Add failing cases for stale logical timestamp, wrong source frame, invalid bbox, wrong class after filtering, empty batch, and a candidate inconsistent with anchor/recent verification.
- [ ] Add a slow-detector replay spanning multiple request intervals. Assert no newer request overwrites the in-flight identity and a stale completion is logged then discarded.
- [ ] Add a same-class distractor case. Assert detector confidence alone cannot select it as the recovered target.
- [ ] Make the tests GREEN using only the designed adapter/controller contracts; do not add detector-specific shortcuts to the controller.
- [ ] Run all CTest and Python manifest tests, plus sanitizer builds. Store exact totals and failures/skips in the ledger.

### Task 8: Run real RK3566 detector and long-term tracking acceptance

**Files/artifacts on board:**

- Deploy to: `/home/cat/Project/SOT/nanotrack_v3_longterm`
- Create: timestamped directory under `/home/cat/Project/SOT/nanotrack_v3_longterm/artifacts/`
- Update after results: `docs/superpowers/plans/2026-10-13-rk3566-yolov8-detector-progress.md`

- [ ] Record board identity, CPU/NPU platform, kernel, RKNN API/driver versions, free disk/RAM, model hashes, binary hashes, and command lines before testing.
- [ ] Clean-build on the board with `ENABLE_RKNN_RUNTIME=ON`; run the complete CTest suite and retain output.
- [ ] Run `yolo_detector_smoke` on an official image. Retain the input identity, annotated output, candidate CSV, tensor dump, stderr/stdout, and timing. Manually inspect that boxes/classes are plausible.
- [ ] Run a person video with `--detector-class-id 0`. Retain output video, schema-v3 CSV, metadata, and logs. Confirm detector requests and batches are distinct from NanoTrack verification/recovery events.
- [ ] Run a car disappearance/reappearance loop with `--detector-class-id 2`. Confirm `control_valid=false` while lost and it can become true only after the 2-of-3 rule. If recovery does not occur, retain evidence and report failure rather than tuning thresholds invisibly.
- [ ] Run the same baseline video with detector options omitted. Confirm legacy `NullDetectorAdapter` behavior and output schema remain valid.
- [ ] Run detector-enabled tracking for at least 10 minutes. Sample elapsed time, FPS timing boundary, RSS, thread count, queue state/counters, temperature, NPU information, and error counts. Check that memory and request/result counts do not show sustained growth.
- [ ] Copy or checksum all retained board artifacts, then run `scripts/verify_board.sh` against them.
- [ ] Record pass/fail separately for build, unit tests, model load, image inference, video inference, recovery correctness, detector-disabled regression, and stability. Do not impose or imply a detector performance threshold.

### Task 9: Final audit and handoff

**Files:**

- Modify: `rk3566/nanotrack_v3/README.md`
- Finalize: `docs/superpowers/plans/2026-10-13-rk3566-yolov8-detector-progress.md`
- Optionally create: `rk3566/nanotrack_v3/docs/detector-board-verification.md`

- [ ] Compare every implementation and artifact against the approved design's completion definition.
- [ ] Search for forbidden leftovers: hard-coded `/home/walker`, RK3588 target/core masks, detached threads, unbounded queues, stale copied binaries, unchecked RKNN calls, and detector boxes directly setting valid control.
- [ ] Verify README commands are executable from a clean checkout/deployment and distinguish host tests, conversion, board build, board inference, and long-term acceptance.
- [ ] Run final host and board verification commands fresh; use their current output, not earlier successful logs, for the completion claim.
- [ ] List known limitations explicitly: generic COCO detector, FP16 function-first build, uncalibrated thresholds, unsupported categories, same-class ambiguity without ReID, and measured board performance without a pass line.
- [ ] Report exact artifact paths and hashes, tests passed/failed, board measurements, and any deviations. If any required board item is missing, label the work incomplete.

## Self-review against the approved design

- Model provenance, RK3566 target, FP16 conversion, manifest, hashes, and `calibrated=false`: Tasks 5 and 8.
- Pure 6/9-output YOLOv8 DFL decoder, INT8-ready views, NMS/class/letterbox tests: Task 3.
- One-worker bounded ownership and failure-disable semantics: Task 2.
- `can_submit()` request identity fix and runtime diagnostics: Task 1.
- CLI optionality, schema v3, metadata and detector-free regression: Task 4.
- RAII RKNN/OpenCV engine and isolated integration: Task 6.
- No direct detector acceptance; freshness plus NanoTrack 2-of-3 confirmation: Task 7.
- Real image/video/recovery/10-minute board evidence: Task 8.
- No performance claim or threshold in this phase: Tasks 8 and 9.

No approved requirement is intentionally deferred. Detector model-quality optimization, INT8 calibration, custom data training, ReID, and product threshold calibration remain explicitly outside this plan.
