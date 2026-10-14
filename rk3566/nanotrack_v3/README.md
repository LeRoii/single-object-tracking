# NanoTrack V3 Long-term Tracker for RK3566

This project uses the existing three-model NanoTrack V3 RKNN deployment and
adds a timestamp-driven long-term controller around it. The controller keeps an
immutable anchor template and a cautiously updated recent template, predicts
motion, rejects unreliable observations, performs bounded regional recovery,
and exposes an asynchronous detector adapter. Detector arguments are optional;
when they are omitted, the executable uses `NullDetectorAdapter` and requires
no detector model.

The thresholds in `config/visible-default.conf` and
`config/infrared-default.conf` are bring-up defaults with `calibrated=false`.
They are not product thresholds and must be calibrated independently for the
two modalities.

## Clean-checkout host verification

From this directory, build and test the RKNN-independent logic with:

```bash
python3 -m venv .venv-host
.venv-host/bin/python -m pip install pytest==9.1.1
cmake -S . -B build-host -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF
cmake --build build-host -- -j2
ctest --test-dir build-host --output-on-failure
.venv-host/bin/python -m pytest -q \
  tests/test_model_manifest.py tests/test_detector_manifest.py
```

The generated `models/yolov8n-rk3566-fp16.rknn` is intentionally ignored by
Git. To reproduce it, use RKNN Toolkit2 2.3.2 on an x86-64 conversion host and
the optimized ONNX identity recorded in the manifest:

```bash
python3 -m venv .venv-rknn
.venv-rknn/bin/python -m pip install -r requirements-convert.txt
mkdir -p artifacts/model-source models
curl -fL \
  'https://ftrg.zbox.filez.com/v2/delivery/data/95f00b0fc900458ba134f8b180b3f7a1/examples/yolov8/yolov8n.onnx' \
  -o artifacts/model-source/yolov8n.onnx
printf '%s  %s\n' \
  '0c8716701f471067932b797eeb67c8e5db47c693c2557c881d7679ec12e21bc5' \
  'artifacts/model-source/yolov8n.onnx' | sha256sum -c -
.venv-rknn/bin/python tools/convert_yolov8_rk3566.py \
  --onnx artifacts/model-source/yolov8n.onnx \
  --output models/yolov8n-rk3566-fp16.rknn \
  --manifest models/detector-manifest.json \
  --labels models/coco80.txt \
  --source-url 'https://ftrg.zbox.filez.com/v2/delivery/data/95f00b0fc900458ba134f8b180b3f7a1/examples/yolov8/yolov8n.onnx' \
  --source-repository 'https://github.com/airockchip/rknn_model_zoo' \
  --source-commit 837db8e992815ff8647eadb709f07067f4bc3a71 \
  --output-count 9
```

Alternatively, restore the exact 8,097,993-byte RKNN artifact whose SHA-256 is
`4d53a9b1e12996556294be8e099fe2dc0815d5c9cfdc327edfe350832baf8cf6`.
In either case, run:

```bash
python3 tools/verify_detector_manifest.py \
  --manifest models/detector-manifest.json
```

The verifier checks the model/label hashes and the manifest's declared RK3566
FP16, RGB/NHWC 640x640 and 6/9-output contract. The board runtime separately
queries and validates the RKNN artifact's actual input/output tensor metadata.

## Board build

```bash
cmake -S . -B build -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=ON \
  -DOpenCV_DIR=/home/cat/ThirdParty/OpenCV4100/lib/cmake/opencv4 \
  -DRKNN_INCLUDE_DIR=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/include \
  -DRKNN_LIBRARY=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/aarch64/librknnrt.so
cmake --build build -- -j2
ctest --test-dir build --output-on-failure
```

Deployments are versioned below the isolated long-term root and never replace
the earlier `/home/cat/Project/SOT/nanotrack_v3` project or an existing release:

```bash
SSH_PORT=10022 SSH_CONTROL_PATH=/tmp/codex_rk_ctrl \
  ./scripts/deploy_board.sh cat@127.0.0.1
# default release:
# /home/cat/Project/SOT/nanotrack_v3_longterm/releases/rk3566-yolov8-detector
```

Set `DEPLOY_RELEASE_NAME` to create a second immutable release. The script
rejects an existing destination instead of updating it in place.

Before the full tracker is run, the detector can be checked independently:

```bash
export DETECTOR_TEST_IMAGE=/absolute/path/to/bus.jpg
test -s "$DETECTOR_TEST_IMAGE"
mkdir -p artifacts
./build/yolo_detector_smoke \
  --model models/yolov8n-rk3566-fp16.rknn \
  --manifest models/detector-manifest.json \
  --labels models/coco80.txt \
  --image "$DETECTOR_TEST_IMAGE" --conf 0.25 --nms 0.45 --class-id -1 \
  --output-image artifacts/yolo-smoke.jpg \
  --output-csv artifacts/yolo-smoke.csv
```

The smoke executable performs manifest, file-hash, target, input, output and
class-contract validation before inference. It writes both an annotated image
and a candidate CSV; an empty candidate list is valid, but missing output
artifacts are not.

## Headless run

Videos and images are deliberately not versioned. Set `TRACKER_TEST_VIDEO` to
an operator-supplied video, verify its identity separately, and choose the
first-frame box for that exact input before running:

```bash
export TRACKER_TEST_VIDEO=/absolute/path/to/input.mp4
test -s "$TRACKER_TEST_VIDEO"
mkdir -p artifacts
./build/nanotrack_v3 \
  --video "$TRACKER_TEST_VIDEO" \
  --bbox 244,161,74,70 \
  --model-dir models \
  --output-video artifacts/output.avi \
  --output-boxes artifacts/bboxes.csv
```

The legacy command remains valid and uses built-in visible-light defaults. A
configuration-explicit run is:

```bash
./build/nanotrack_v3 \
  --video "$TRACKER_TEST_VIDEO" \
  --bbox 244,161,74,70 \
  --model-dir models \
  --sensor visible \
  --controller-config config/visible-default.conf \
  --output-video artifacts/output-long-term.avi \
  --output-boxes artifacts/output-long-term.csv \
  --session-metadata artifacts/output-long-term.metadata.json
```

`--sensor infrared` must be paired with an infrared config. The program fails
if the CLI modality and config modality disagree.

To enable detector-assisted recovery, supply the model, manifest, and labels as
one all-or-none set. The thresholds below are uncalibrated bring-up defaults:

```bash
./build/nanotrack_v3 \
  --video "$TRACKER_TEST_VIDEO" \
  --bbox 244,161,74,70 \
  --model-dir models \
  --detector-model models/yolov8n-rk3566-fp16.rknn \
  --detector-manifest models/detector-manifest.json \
  --detector-labels models/coco80.txt \
  --detector-class-id 0 \
  --detector-conf 0.25 \
  --detector-nms 0.45 \
  --output-video artifacts/output-detector.avi \
  --output-boxes artifacts/output-detector.csv
```

Use class `-1` to retain every detector class. Confidence and NMS values must
be finite and strictly between zero and one. Supplying only part of the three
detector-file set is rejected before model startup.

For a retained run, verify source-coordinate boxes, schema-v3 metadata, model
hashes, detector state and output frame count with:

```bash
./scripts/verify_board.sh \
  artifacts/retained-run models EXPECTED_OUTPUT_FRAMES \
  SOURCE_FRAME_WIDTH SOURCE_FRAME_HEIGHT \
  models/yolov8n-rk3566-fp16.rknn \
  models/detector-manifest.json models/coco80.txt
```

`ARTIFACT_DIR` must contain `run.log`, `bboxes.csv`, `output.avi`,
`manifest.json`, and `session-metadata.json`. The width and height arguments
refer to the source/CSV coordinate system, not a downscaled preview video.

## Outputs and safety contract

The schema-v3 CSV contains one row for initialization and one row for each
processed frame. It records:

- monotonic timestamp, controller state, quality, `control_valid`, horizontal
  box, recovery and template actions;
- raw anchor/recent score, penalized peak, PSR, APCE and peak margin;
- template agreement, motion/shape residuals, search scale and hypothesis
  source;
- detector configured/ready/requested state, request and completed-batch frame
  identity, worker/error status, batch age, raw/filtered candidate counts,
  candidate class/confidence selected for NanoTrack verification, and segmented
  detector timing;
- tracker/runtime latency and full video-pipeline latency (decode, tracking,
  overlay, and encode).

Only a row with `control_valid=1` may drive a gimbal or downstream control
loop. `UNCERTAIN`, `LOCAL_RECOVERY`, `ASSISTED_RECOVERY`, and `LOST` rows can
contain a predicted box for visualization while remaining invalid for control.

If `--session-metadata` is omitted, metadata is written next to the CSV as
`OUTPUT.csv.metadata.json`. It records the model manifest fingerprint, RKNN
API/driver versions, timing fallback, threshold identity, calibration marker,
and latency summary. Schema-v3 metadata also has a detector section containing
artifact paths and SHA-256 identities, tensor descriptions, detector RKNN
API/driver versions, thresholds, `calibrated=false`, and request/completion/
rejection/error counters. Disabled runs retain that section with
`enabled=false` and zero counters so runs remain directly comparable.

The final console summary separates tracker latency, controller runtime latency,
and video-pipeline latency. `runtime_fps` excludes video I/O and rendering;
`end_to_end_fps` is derived from the per-frame pipeline measurement that includes
decode, tracking, overlay, and encode. Only measurements obtained on the actual
RK3566 board are board-performance evidence.

## Detector integration point

Implement `DetectorAdapter` from `include/detector_adapter.hpp`. Requests carry
`frame_id`, monotonic timestamp, ROI, and a maximum of three candidates.
Returned candidates never overwrite the tracker box directly: the controller
uses them as bounded search hypotheses and requires NanoTrack template
verification plus 2-of-3 confirmation. Results older than 150 ms or not tied
to a known request are rejected.

## Verified scope and known limitations

The current RK3566 evidence is documented in
`docs/detector-board-verification.md`. The board-native build, unit tests,
official-image inference, detector-enabled video inference, detector-disabled
regression and a 1267-second run completed with bounded one-request/one-result
queue behavior. These results do not establish product-quality long-term
recovery:

- the generic COCO detector, confidence/NMS values and controller thresholds
  remain uncalibrated;
- the tested person and car videos did not recover the target after the main
  long-term loss, even though detector candidates reached NanoTrack
  verification;
- same-class ambiguity has no ReID or target-specific appearance retrieval;
- the FP16 model is a function-first artifact, not an optimized INT8 release;
- the 1267-second run returned repeatedly to about 149 MB RSS, but periodic
  peaks rose to 446 MB and VmHWM reached 477 MB, so the video writer/buffering
  path still needs memory investigation;
- measured FPS values are observations without a performance pass threshold.

Only schema-v3 rows with `control_valid=1` are valid control outputs. Detector
confidence never directly authorizes control.
