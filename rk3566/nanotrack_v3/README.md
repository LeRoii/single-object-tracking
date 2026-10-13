# NanoTrack V3 Long-term Tracker for RK3566

This project uses the existing three-model NanoTrack V3 RKNN deployment and
adds a timestamp-driven long-term controller around it. The controller keeps an
immutable anchor template and a cautiously updated recent template, predicts
motion, rejects unreliable observations, performs bounded regional recovery,
and exposes an asynchronous detector adapter. The default executable uses
`NullDetectorAdapter`; no detector model is required.

The thresholds in `config/visible-default.conf` and
`config/infrared-default.conf` are bring-up defaults with `calibrated=false`.
They are not product thresholds and must be calibrated independently for the
two modalities.

## Board build

```bash
cmake -S . -B build -DBUILD_TESTING=OFF -DENABLE_RKNN_RUNTIME=ON \
  -DOpenCV_DIR=/home/cat/ThirdParty/OpenCV4100/lib/cmake/opencv4 \
  -DRKNN_INCLUDE_DIR=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/include \
  -DRKNN_LIBRARY=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/aarch64/librknnrt.so
cmake --build build -- -j2
```

## Headless run

```bash
./build/nanotrack_v3 \
  --video data/girl_dance.mp4 \
  --bbox 244,161,74,70 \
  --model-dir models \
  --output-video artifacts/output.avi \
  --output-boxes artifacts/bboxes.csv
```

The legacy command remains valid and uses built-in visible-light defaults. A
configuration-explicit run is:

```bash
./build/nanotrack_v3 \
  --video data/girl_dance.mp4 \
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

## Outputs and safety contract

The CSV contains one row for initialization and one row for each processed
frame. It records:

- monotonic timestamp, controller state, quality, `control_valid`, horizontal
  box, recovery and template actions;
- raw anchor/recent score, penalized peak, PSR, APCE and peak margin;
- template agreement, motion/shape residuals, search scale and hypothesis
  source;
- detector request/result status, tracker/runtime latency, and full video-pipeline
  latency (decode, tracking, overlay, and encode).

Only a row with `control_valid=1` may drive a gimbal or downstream control
loop. `UNCERTAIN`, `LOCAL_RECOVERY`, `ASSISTED_RECOVERY`, and `LOST` rows can
contain a predicted box for visualization while remaining invalid for control.

If `--session-metadata` is omitted, metadata is written next to the CSV as
`OUTPUT.csv.metadata.json`. It records the model manifest fingerprint, RKNN
API/driver versions, timing fallback, threshold identity, calibration marker,
and latency summary.

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
