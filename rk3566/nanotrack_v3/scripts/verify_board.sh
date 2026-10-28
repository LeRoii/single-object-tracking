#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 5 && $# -ne 8 ]]; then
  echo "usage: $0 ARTIFACT_DIR MODEL_DIR EXPECTED_OUTPUT_FRAMES FRAME_WIDTH FRAME_HEIGHT [DETECTOR_MODEL DETECTOR_MANIFEST DETECTOR_LABELS]" >&2
  exit 2
fi

artifact_dir=$1
model_dir=$2
expected_frames=$3
frame_width=$4
frame_height=$5
detector_model=${6:-}
detector_manifest=${7:-}
detector_labels=${8:-}

for file in run.log bboxes.csv output.avi manifest.json session-metadata.json; do
  if [[ ! -s "$artifact_dir/$file" ]]; then
    echo "missing or empty artifact: $artifact_dir/$file" >&2
    exit 1
  fi
done

if [[ -n "$detector_model" ]]; then
  for file in "$detector_model" "$detector_manifest" "$detector_labels"; do
    if [[ ! -s "$file" ]]; then
      echo "missing or empty detector artifact: $file" >&2
      exit 1
    fi
  done
fi

for file in template_backbone.rknn search_backbone.rknn head.rknn; do
  if [[ ! -s "$model_dir/$file" ]]; then
    echo "missing or empty model: $model_dir/$file" >&2
    exit 1
  fi
done

if grep -Eqi 'GTK|selectROI|imshow|cannot open display|RKNN:.*(error|unsupported)|rknn_.*failed' "$artifact_dir/run.log"; then
  echo "runtime log contains a GUI/display or RKNN error" >&2
  exit 1
fi

python3 - "$artifact_dir" "$model_dir" "$expected_frames" "$frame_width" "$frame_height" "$detector_model" "$detector_manifest" "$detector_labels" <<'PY'
import csv
import hashlib
import json
import math
import pathlib
import re
import sys

artifact_dir = pathlib.Path(sys.argv[1])
model_dir = pathlib.Path(sys.argv[2])
expected = int(sys.argv[3])
width = int(sys.argv[4])
height = int(sys.argv[5])
detector_model = pathlib.Path(sys.argv[6]) if sys.argv[6] else None
detector_manifest_path = pathlib.Path(sys.argv[7]) if sys.argv[7] else None
detector_labels = pathlib.Path(sys.argv[8]) if sys.argv[8] else None
EXPECTED_TRACKING_SCHEMA = 7
VALID_TARGET_MODES = {"FULL_OBJECT", "SUBTARGET"}

manifest = json.loads((artifact_dir / "manifest.json").read_text())
for model in manifest["models"].values():
    path = model_dir / model["file"]
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if digest != model["sha256"]:
        raise SystemExit(f"model hash mismatch: {path}")

with (artifact_dir / "bboxes.csv").open(newline="") as stream:
    rows = list(csv.DictReader(stream))
if len(rows) != expected:
    raise SystemExit(f"CSV row count {len(rows)} != expected {expected}")
required = {
    "schema_version", "frame_id", "timestamp_ms", "state", "quality", "control_valid",
    "accepted", "reacquired", "x", "y", "width", "height",
    "anchor_score", "anchor_psr", "anchor_apce", "recent_score",
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
    "detector_preprocess_ms", "detector_inference_ms", "detector_postprocess_ms",
    "detector_total_ms", "tracker_ms", "runtime_ms", "pipeline_ms",
    "occlusion_hold_triggered", "occlusion_hold_remaining_frames",
    "occlusion_reference_width", "occlusion_reference_height",
    "occlusion_width_ratio", "occlusion_height_ratio", "tracker_skipped",
    "detector_suppressed_by_hold",
}
missing = required - set(rows[0])
if missing:
    raise SystemExit(f"CSV header is missing: {sorted(missing)}")
first_frame_id = int(rows[0]["frame_id"])
last_timestamp = None
for index, row in enumerate(rows):
    if row["schema_version"] != str(EXPECTED_TRACKING_SCHEMA):
        raise SystemExit(f"unexpected CSV schema version at row {index}")
    values = {key: float(row[key]) for key in {"x", "y", "width", "height"}}
    if int(row["frame_id"]) != first_frame_id + index:
        raise SystemExit(f"non-sequential frame id at row {index}")
    timestamp = int(row["timestamp_ms"])
    if last_timestamp is not None and timestamp <= last_timestamp:
        raise SystemExit(f"non-monotonic timestamp at row {index}")
    last_timestamp = timestamp
    if row["control_valid"] not in {"0", "1"}:
        raise SystemExit(f"invalid control_valid at row {index}")
    if row["detector_target_mode"] not in VALID_TARGET_MODES:
        raise SystemExit(f"invalid detector_target_mode at row {index}")
    if row["detector_candidate_locked"] not in {"0", "1"}:
        raise SystemExit(f"invalid detector_candidate_locked at row {index}")
    if row["detector_same_class_ambiguous"] not in {"0", "1"}:
        raise SystemExit(f"invalid detector_same_class_ambiguous at row {index}")
    same_class_candidates = int(row["detector_same_class_candidates"])
    if same_class_candidates < 0:
        raise SystemExit(f"invalid detector_same_class_candidates at row {index}")
    if (row["detector_same_class_ambiguous"] == "1") != (
            same_class_candidates > 1):
        raise SystemExit(f"inconsistent same-class ambiguity at row {index}")
    if row["detector_same_class_ambiguous"] == "1" and (
            row["detector_candidate_locked"] == "1" or
            row["control_valid"] == "1"):
        raise SystemExit(f"ambiguous same-class candidate controls at row {index}")
    for key in {"occlusion_hold_triggered", "tracker_skipped",
                "detector_suppressed_by_hold"}:
        if row[key] not in {"0", "1"}:
            raise SystemExit(f"invalid {key} at row {index}")
    if row["detector_suppressed_by_hold"] == "1" and row["tracker_skipped"] != "1":
        raise SystemExit(f"detector hold suppression without tracker skip at row {index}")
    if not all(math.isfinite(value) for value in values.values()):
        raise SystemExit(f"non-finite value at row {index}")
    if values["width"] <= 0 or values["height"] <= 0:
        raise SystemExit(f"non-positive box size at row {index}")
    if index > 0:
        tracker_ms = float(row["tracker_ms"])
        if ((row["tracker_skipped"] == "1" and tracker_ms != 0) or
                (row["tracker_skipped"] == "0" and tracker_ms <= 0) or
                float(row["runtime_ms"]) <= 0 or
                float(row["pipeline_ms"]) <= 0):
            raise SystemExit(f"non-positive latency at row {index}")
    if values["x"] < 0 or values["y"] < 0:
        raise SystemExit(f"negative box origin at row {index}")
    if values["x"] + values["width"] > width + 1e-3:
        raise SystemExit(f"box exceeds frame width at row {index}")
    if values["y"] + values["height"] > height + 1e-3:
        raise SystemExit(f"box exceeds frame height at row {index}")

metadata = json.loads((artifact_dir / "session-metadata.json").read_text())
if metadata.get("schema_version") != EXPECTED_TRACKING_SCHEMA:
    raise SystemExit(
        f"session metadata schema_version must be {EXPECTED_TRACKING_SCHEMA}"
    )
for key in {
    "schema_version", "manifest_hash", "controller_config_hash", "sensor",
    "calibrated", "detector_target_mode", "rknn_api_version",
    "rknn_driver_version", "occlusion_hold_frames",
    "occlusion_trigger_consecutive_unreliable",
    "occlusion_trigger_min_dimension_ratio", "occlusion_reference_window",
    "mean_runtime_ms", "p50_runtime_ms", "p95_runtime_ms", "runtime_fps",
    "mean_pipeline_ms", "p50_pipeline_ms", "p95_pipeline_ms",
    "end_to_end_fps",
}:
    if key not in metadata:
        raise SystemExit(f"session metadata missing {key}")
if metadata["detector_target_mode"] not in VALID_TARGET_MODES:
    raise SystemExit("session metadata detector_target_mode is invalid")
if metadata["tracked_frames"] != expected - 1:
    raise SystemExit("session metadata tracked frame count mismatch")

detector = metadata.get("detector")
if not isinstance(detector, dict):
    raise SystemExit("session metadata detector section is missing")
for key in {
    "enabled", "calibrated", "model_path", "model_sha256",
    "manifest_path", "manifest_sha256", "labels_path", "labels_sha256",
    "class_id", "confidence_threshold", "nms_threshold",
    "input_tensor_description", "output_tensor_descriptions",
    "rknn_api_version", "rknn_driver_version", "request_count",
    "complete_count", "reject_count", "error_count",
}:
    if key not in detector:
        raise SystemExit(f"session metadata detector missing {key}")
if detector["calibrated"] is not False:
    raise SystemExit("detector metadata must remain calibrated=false")
if detector["complete_count"] > detector["request_count"]:
    raise SystemExit("detector completions exceed requests")
if detector["reject_count"] > detector["complete_count"]:
    raise SystemExit("detector rejects exceed completions")

if detector_model is None:
    if detector["enabled"] is not False:
        raise SystemExit("detector-disabled run reports enabled detector")
    if any(row["detector_configured"] != "0" for row in rows):
        raise SystemExit("detector-disabled CSV reports configured detector")
else:
    if detector["enabled"] is not True:
        raise SystemExit("detector-enabled run reports disabled detector")
    if any(row["detector_configured"] != "1" for row in rows):
        raise SystemExit("detector-enabled CSV loses configured state")
    declared = json.loads(detector_manifest_path.read_text())
    if declared.get("schema_version") != 1:
        raise SystemExit("detector manifest schema_version must be 1")
    if declared.get("deployment_status") != "function_only_board_unverified":
        raise SystemExit("detector manifest deployment status is unexpected")
    if declared["conversion"].get("target_platform") != "rk3566":
        raise SystemExit("detector manifest target is not rk3566")
    if declared["conversion"].get("quantized") is not False:
        raise SystemExit("detector manifest must describe FP16 conversion")
    model_digest = hashlib.sha256(detector_model.read_bytes()).hexdigest()
    labels_digest = hashlib.sha256(detector_labels.read_bytes()).hexdigest()
    manifest_digest = hashlib.sha256(detector_manifest_path.read_bytes()).hexdigest()
    if model_digest != declared["artifact"]["sha256"]:
        raise SystemExit("detector model hash does not match detector manifest")
    if labels_digest != declared["labels"]["sha256"]:
        raise SystemExit("detector labels hash does not match detector manifest")
    if detector["model_sha256"] != model_digest:
        raise SystemExit("session detector model hash mismatch")
    if detector["labels_sha256"] != labels_digest:
        raise SystemExit("session detector labels hash mismatch")
    if detector["manifest_sha256"] != manifest_digest:
        raise SystemExit("session detector manifest hash mismatch")
    if not detector["input_tensor_description"] or len(detector["output_tensor_descriptions"]) not in {6, 9}:
        raise SystemExit("detector tensor descriptions are incomplete")
    if not detector["rknn_api_version"] or not detector["rknn_driver_version"]:
        raise SystemExit("detector RKNN versions are missing")

log = (artifact_dir / "run.log").read_text(errors="replace")
match = re.search(
    r"SUMMARY tracked_frames=(\d+) mean_tracker_ms=([0-9.]+) "
    r"mean_runtime_ms=([0-9.]+) p50_runtime_ms=([0-9.]+) "
    r"p95_runtime_ms=([0-9.]+) runtime_fps=([0-9.]+) "
    r"mean_pipeline_ms=([0-9.]+) p50_pipeline_ms=([0-9.]+) "
    r"p95_pipeline_ms=([0-9.]+) end_to_end_fps=([0-9.]+) "
    r"calibrated=([01]) metadata=(\S+)",
    log,
)
if not match:
    raise SystemExit("summary metrics missing from run.log")
if int(match.group(1)) != expected - 1:
    raise SystemExit("summary tracked frame count mismatch")
if any(float(value) <= 0 for value in match.groups()[1:10]):
    raise SystemExit("invalid summary metrics")
if pathlib.Path(match.group(12)).name != "session-metadata.json":
    raise SystemExit("summary metadata path does not name session-metadata.json")
PY

video_frames=$(ffprobe -v error -select_streams v:0 -count_frames \
  -show_entries stream=nb_read_frames -of default=nokey=1:noprint_wrappers=1 \
  "$artifact_dir/output.avi")
if [[ "$video_frames" != "$expected_frames" ]]; then
  echo "output video frame count $video_frames != expected $expected_frames" >&2
  exit 1
fi

echo "PASS board artifacts output_frames=$expected_frames tracked_frames=$((expected_frames - 1))"
