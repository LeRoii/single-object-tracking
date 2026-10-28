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
    "runtime_ms", "pipeline_ms", "occlusion_hold_triggered",
    "occlusion_hold_remaining_frames", "occlusion_reference_width",
    "occlusion_reference_height", "occlusion_width_ratio",
    "occlusion_height_ratio", "tracker_skipped",
    "detector_suppressed_by_hold",
]
with (artifact_dir / "bboxes.csv").open("w", newline="") as stream:
    writer = csv.DictWriter(stream, fieldnames=header)
    writer.writeheader()
    for frame_id, timestamp_ms in ((0, 0), (1, 33), (2, 66)):
        row = {key: "" for key in header}
        row.update({
            "schema_version": "7", "frame_id": frame_id,
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
            "detector_confirmation_hits": "0",
            "occlusion_hold_triggered": "0",
            "occlusion_hold_remaining_frames": "0",
            "tracker_skipped": "0",
            "detector_suppressed_by_hold": "0",
            "tracker_ms": "1",
            "runtime_ms": "2", "pipeline_ms": "3",
        })
        if frame_id == 2:
            row.update({
                "state": "OCCLUSION_HOLD", "quality": "REJECTED",
                "control_valid": "0", "accepted": "0",
                "occlusion_hold_remaining_frames": "11",
                "occlusion_reference_width": "4",
                "occlusion_reference_height": "4",
                "tracker_skipped": "1",
                "detector_suppressed_by_hold": "1",
                "tracker_ms": "0",
            })
        writer.writerow(row)

metadata = {
    "schema_version": 7, "manifest_hash": "hash",
    "controller_config_hash": "config", "sensor": "visible",
    "calibrated": False, "detector_target_mode": "FULL_OBJECT",
    "occlusion_hold_frames": 12,
    "occlusion_trigger_consecutive_unreliable": 2,
    "occlusion_trigger_min_dimension_ratio": 0.70,
    "occlusion_reference_window": 8,
    "rknn_api_version": "api", "rknn_driver_version": "driver",
    "tracked_frames": 2, "mean_runtime_ms": 2.0, "p50_runtime_ms": 2.0,
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
    "SUMMARY tracked_frames=2 mean_tracker_ms=1 mean_runtime_ms=2 "
    "p50_runtime_ms=2 p95_runtime_ms=2 runtime_fps=500 "
    "mean_pipeline_ms=3 p50_pipeline_ms=3 p95_pipeline_ms=3 "
    "end_to_end_fps=333 calibrated=0 metadata=session-metadata.json\n"
)
PY

ffmpeg -loglevel error -f lavfi -i color=size=16x16:rate=1 \
  -frames:v 3 "$artifact_dir/output.avi"

bash "$verify_script" "$artifact_dir" "$model_dir" 3 16 16

python3 - "$artifact_dir/bboxes.csv" <<'PY'
import csv
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
with path.open(newline="") as stream:
    rows = list(csv.DictReader(stream))
    fields = list(rows[0])
rows[1]["tracker_skipped"] = "0"
rows[1]["tracker_ms"] = "0"
with path.open("w", newline="") as stream:
    writer = csv.DictWriter(stream, fieldnames=fields)
    writer.writeheader()
    writer.writerows(rows)
PY
if bash "$verify_script" "$artifact_dir" "$model_dir" 3 16 16 \
    >/dev/null 2>&1; then
  echo "verify_board accepted zero tracker latency without skip flag" >&2
  exit 1
fi

