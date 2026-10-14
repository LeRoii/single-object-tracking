#!/usr/bin/env bash
set -euo pipefail

verify_script=$1
tmp_dir=$(mktemp -d /tmp/nanotrack-verify-schema.XXXXXX)
trap 'rm -rf "$tmp_dir"' EXIT
artifact_dir=$tmp_dir/artifacts
model_dir=$tmp_dir/models
mkdir -p "$artifact_dir" "$model_dir"

printf 'template' >"$model_dir/template_backbone.rknn"
printf 'search' >"$model_dir/search_backbone.rknn"
printf 'head' >"$model_dir/head.rknn"

python3 - "$artifact_dir" "$model_dir" <<'PY'
import csv
import hashlib
import json
import pathlib
import sys

artifact_dir = pathlib.Path(sys.argv[1])
model_dir = pathlib.Path(sys.argv[2])
models = {}
for name in ("template_backbone.rknn", "search_backbone.rknn", "head.rknn"):
    models[name] = {
        "file": name,
        "sha256": hashlib.sha256((model_dir / name).read_bytes()).hexdigest(),
    }
(artifact_dir / "manifest.json").write_text(json.dumps({"models": models}))

header = [
    "schema_version", "frame_id", "timestamp_ms", "state", "quality",
    "control_valid", "accepted", "reacquired", "x", "y", "width",
    "height", "anchor_score", "anchor_psr", "anchor_apce", "recent_score",
    "recent_psr", "recent_apce", "search_scale", "detector_status",
    "detector_configured", "detector_ready", "detector_requested",
    "detector_request_frame_id", "detector_worker_status", "detector_error",
    "detector_batch_frame_id", "detector_batch_age_ms",
    "detector_raw_candidate_count", "detector_filtered_candidate_count",
    "detector_verified_class_id", "detector_verified_confidence",
    "detector_target_mode", "detector_candidate_id", "detector_region_x",
    "detector_region_y", "detector_region_width", "detector_region_height",
    "detector_same_class_candidates", "detector_same_class_ambiguous",
    "detector_candidate_locked", "detector_probe_index",
    "detector_confirmation_attempts", "detector_confirmation_hits",
    "detector_preprocess_ms", "detector_inference_ms",
    "detector_postprocess_ms", "detector_total_ms", "tracker_ms",
    "runtime_ms", "pipeline_ms",
]
with (artifact_dir / "bboxes.csv").open("w", newline="") as stream:
    writer = csv.DictWriter(stream, fieldnames=header)
    writer.writeheader()
    for frame_id, timestamp_ms in ((0, 0), (1, 33)):
        row = {key: "" for key in header}
        row.update({
            "schema_version": "5", "frame_id": frame_id,
            "timestamp_ms": timestamp_ms, "state": "TRACKING",
            "quality": "RELIABLE", "control_valid": "1", "accepted": "1",
            "reacquired": "0", "x": "1", "y": "1", "width": "4",
            "height": "4", "search_scale": "1", "detector_status": "NONE",
            "detector_configured": "0", "detector_ready": "0",
            "detector_requested": "0", "detector_worker_status": "disabled",
            "detector_raw_candidate_count": "0",
            "detector_filtered_candidate_count": "0",
            "detector_target_mode": "FULL_OBJECT",
            "detector_same_class_candidates": "0",
            "detector_same_class_ambiguous": "0",
            "detector_candidate_locked": "0", "detector_probe_index": "0",
            "detector_confirmation_attempts": "0",
            "detector_confirmation_hits": "0", "tracker_ms": "1",
            "runtime_ms": "2", "pipeline_ms": "3",
        })
        writer.writerow(row)

metadata = {
    "schema_version": 5, "manifest_hash": "hash",
    "controller_config_hash": "config", "sensor": "visible",
    "calibrated": False, "detector_target_mode": "FULL_OBJECT",
    "rknn_api_version": "api", "rknn_driver_version": "driver",
    "tracked_frames": 1, "mean_runtime_ms": 2.0, "p50_runtime_ms": 2.0,
    "p95_runtime_ms": 2.0, "runtime_fps": 500.0,
    "mean_pipeline_ms": 3.0, "p50_pipeline_ms": 3.0,
    "p95_pipeline_ms": 3.0, "end_to_end_fps": 333.0,
    "detector": {
        "enabled": False, "calibrated": False, "model_path": "",
        "model_sha256": "", "manifest_path": "", "manifest_sha256": "",
        "labels_path": "", "labels_sha256": "", "class_id": -1,
        "confidence_threshold": 0.25, "nms_threshold": 0.45,
        "input_tensor_description": "", "output_tensor_descriptions": [],
        "rknn_api_version": "", "rknn_driver_version": "",
        "request_count": 0, "complete_count": 0, "reject_count": 0,
        "error_count": 0,
    },
}
(artifact_dir / "session-metadata.json").write_text(json.dumps(metadata))
(artifact_dir / "run.log").write_text(
    "SUMMARY tracked_frames=1 mean_tracker_ms=1 mean_runtime_ms=2 "
    "p50_runtime_ms=2 p95_runtime_ms=2 runtime_fps=500 "
    "mean_pipeline_ms=3 p50_pipeline_ms=3 p95_pipeline_ms=3 "
    "end_to_end_fps=333 calibrated=0 metadata=session-metadata.json\n"
)
PY

ffmpeg -loglevel error -f lavfi -i color=size=16x16:rate=1 \
  -frames:v 2 "$artifact_dir/output.avi"

bash "$verify_script" "$artifact_dir" "$model_dir" 2 16 16

