#!/usr/bin/env bash
set -euo pipefail

verify_script=$1
export EXPECTED_TRACKING_SCHEMA=10
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
(artifact_dir / "manifest.json").write_text(json.dumps({
    "provenance": {"target_platform": "rk3566"},
    "models": models,
}))

header = [
    "schema_version", "frame_id", "timestamp_ms", "state", "quality",
    "control_valid", "accepted", "reacquired", "x", "y", "width", "hypothesis_source",
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
    "occlusion_probe_anchor_score_gate", "occlusion_probe_anchor_center_gate", "occlusion_probe_anchor_area_gate", "occlusion_probe_anchor_aspect_gate", "occlusion_probe_recent_score_gate", "occlusion_probe_recent_center_gate", "occlusion_probe_recent_area_gate", "occlusion_probe_recent_aspect_gate",
    "occlusion_probe_rejection", "occlusion_probe_motion_x", "occlusion_probe_motion_y", "occlusion_probe_motion_width", "occlusion_probe_motion_height", "occlusion_probe_x", "occlusion_probe_y", "occlusion_probe_width", "occlusion_probe_height", "occlusion_probe_anchor_reliable", "occlusion_probe_recent_reliable", "occlusion_probe_template_gate", "occlusion_probe_cross_gate", "occlusion_probe_cross_iou", "occlusion_probe_cross_center", "occlusion_probe_cross_area", "occlusion_probe_cross_aspect",
    "occlusion_reference_source", "occlusion_suspicion_active", "occlusion_suspicion_started", "occlusion_suspicion_cleared", "occlusion_suspicion_frames", "occlusion_clean_frames", "occlusion_width_shrink_frames", "occlusion_height_shrink_frames", "occlusion_severe_evidence", "occlusion_gradual_evidence", "occlusion_template_evidence", "occlusion_peak_margin_evidence", "occlusion_motion_residual_evidence", "occlusion_trigger_reason", "occlusion_coast_age_frames", "occlusion_probe_type", "occlusion_probe_confirmation_hits", "occlusion_probe_confirmation_required", "motion_control_authorized",
    "target_class_status", "target_class_id", "target_class_reason",
    "detector_recovery_enabled",
    "tracking_phase", "observation_valid", "control_source", "accepted_observation_path",
    "tracking_x", "tracking_y", "tracking_width", "tracking_height",
    "display_x", "display_y", "display_width", "display_height",
    "control_x", "control_y", "control_width", "control_height",
    "motion_search_age_ms", "motion_prediction_confident",
    "motion_velocity_x", "motion_velocity_y", "motion_acceleration_x", "motion_acceleration_y",
    "motion_scale_velocity_width", "motion_scale_velocity_height",
    "search_hint_present", "search_hint_candidate_id", "search_hint_source_frame_id",
    "search_hint_source_timestamp_ms", "search_hint_projected_x", "search_hint_projected_y",
    "search_hint_projected_width", "search_hint_projected_height", "search_hint_generation",
    "search_hint_first_hit_frame", "search_hint_confirmation_count", "search_hint_used",
    "search_hint_rejection",
]
with (artifact_dir / "bboxes.csv").open("w", newline="") as stream:
    writer = csv.DictWriter(stream, fieldnames=header)
    writer.writeheader()
    for frame_id, timestamp_ms in ((0, 0), (1, 33), (2, 66)):
        row = {key: "" for key in header}
        row.update({
            "schema_version": "10", "frame_id": frame_id,
            "timestamp_ms": timestamp_ms, "state": "TRACKING",
            "quality": "RELIABLE", "control_valid": "1", "accepted": "1",
            "hypothesis_source": "MOTION",
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
            "occlusion_probe_rejection": "NONE",
            "occlusion_reference_source": "NONE", "occlusion_suspicion_active": "0", "occlusion_suspicion_started": "0", "occlusion_suspicion_cleared": "0", "occlusion_suspicion_frames": "0", "occlusion_clean_frames": "0", "occlusion_width_shrink_frames": "0", "occlusion_height_shrink_frames": "0", "occlusion_severe_evidence": "0", "occlusion_gradual_evidence": "0", "occlusion_template_evidence": "0", "occlusion_peak_margin_evidence": "0", "occlusion_motion_residual_evidence": "0", "occlusion_trigger_reason": "NONE", "occlusion_coast_age_frames": "0", "occlusion_probe_type": "NONE", "occlusion_probe_confirmation_hits": "0", "occlusion_probe_confirmation_required": "0", "motion_control_authorized": "0",
            "target_class_status": "UNKNOWN", "target_class_id": "",
            "target_class_reason": "DETECTOR_UNAVAILABLE",
            "detector_recovery_enabled": "0",
            "tracking_phase": "TRACKING", "observation_valid": "1",
            "control_source": "OBSERVATION", "accepted_observation_path": "direct",
            "tracking_x": "1", "tracking_y": "1", "tracking_width": "4", "tracking_height": "4",
            "display_x": "1", "display_y": "1", "display_width": "4", "display_height": "4",
            "control_x": "1", "control_y": "1", "control_width": "4", "control_height": "4",
            "motion_search_age_ms": "-1", "motion_prediction_confident": "0",
            "motion_velocity_x": "0", "motion_velocity_y": "0",
            "motion_acceleration_x": "0", "motion_acceleration_y": "0",
            "motion_scale_velocity_width": "0", "motion_scale_velocity_height": "0",
            "search_hint_present": "0",
            "tracker_ms": "1",
            "runtime_ms": "2", "pipeline_ms": "3",
        })
        if frame_id == 1:
            row["accepted_observation_path"] = "template_bridge"
        if frame_id == 2:
            row.update({
                "state": "OCCLUSION_HOLD", "quality": "REJECTED",
                "control_valid": "1", "accepted": "0",
                "motion_control_authorized": "1",
                "tracking_phase": "SEARCHING", "observation_valid": "0",
                "control_source": "MOTION_SEARCH", "motion_prediction_confident": "1",
                "motion_search_age_ms": "66",
                "occlusion_coast_age_frames": "2",
                "occlusion_reference_source": "FULL_STABLE_MEDIAN",
                "occlusion_trigger_reason": "SEVERE_WIDTH",
                "occlusion_probe_rejection": "NONE",
                "occlusion_hold_remaining_frames": "11",
                "occlusion_reference_width": "4",
                "occlusion_reference_height": "4",
                "tracker_skipped": "1",
                "detector_suppressed_by_hold": "1",
                "tracker_ms": "0",
            })
        writer.writerow(row)

metadata = {
    "schema_version": 10, "manifest_hash": "hash",
    "controller_config_hash": "config", "sensor": "visible",
    "calibrated": False, "detector_target_mode": "FULL_OBJECT",
    "occlusion_hold_frames": 12,
    "occlusion_trigger_consecutive_unreliable": 2,
    "occlusion_trigger_min_dimension_ratio": 0.70,
    "occlusion_reference_window": 8,
    "max_accepted_scale_step_ratio": 0.05,
    "accepted_scale_deadband_ratio": 0.02,
    "accepted_scale_smoothing_alpha": 0.10,
    "accepted_scale_median_window_frames": 5,
    "accepted_scale_direction_confirm_frames": 3,
    "accepted_scale_output_hysteresis_px": 0.75,
    "accepted_center_smoothing_alpha": 0.35,
    "accepted_center_max_correction_ratio": 0.25,
    "max_reacquisition_center_distance": 0.75,
    "motion_search_enabled": True,
    "search_reacquisition_requires_detector": True,
    "search_reacquisition_min_age_ms": 400,
    "template_bridge_min_stable_frames": 3,
    "template_bridge_max_duration_ms": 200,
    "motion_search_confidence_ms": 1000,
    "motion_max_speed_diagonals_per_s": 1.0,
    "motion_max_acceleration_diagonals_per_s2": 2.0,
    "motion_scale_rate_limit_per_s": 0.5,
    "search_probe_interval_ms": 100,
    "controller_config_identity": "test-generic-motion-search",
    "occlusion_probe_interval_frames": 5, "occlusion_gradual_dimension_ratio": 1, "occlusion_gradual_consecutive_frames": 1, "occlusion_min_reference_samples": 1, "occlusion_shrink_step_ratio": 1, "occlusion_suspicion_clear_frames": 1, "occlusion_peak_margin_drop": 1, "occlusion_motion_residual_threshold": 1, "occlusion_confirmation_iou": 1, "occlusion_confirmation_center_distance": 1, "occlusion_confirmation_max_area_log_change": 1, "occlusion_confirmation_max_aspect_log_change": 1,
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
    "end_to_end_fps=333 initializations=1 calibrated=0 "
    "metadata=session-metadata.json\n"
)
PY

ffmpeg -loglevel error -f lavfi -i color=size=16x16:rate=1 \
  -frames:v 3 "$artifact_dir/output.avi"

bash "$verify_script" "$artifact_dir" "$model_dir" 3 16 16

python3 - "$artifact_dir/manifest.json" <<'PY'
import json, pathlib, sys
path = pathlib.Path(sys.argv[1])
manifest = json.loads(path.read_text())
manifest["provenance"]["target_platform"] = "rk3588"
path.write_text(json.dumps(manifest))
PY
if bash "$verify_script" "$artifact_dir" "$model_dir" 3 16 16 >/dev/null 2>&1; then
  echo "verify_board accepted RK3588 manifest under default RK3566 target" >&2
  exit 1
fi
EXPECTED_RKNN_TARGET=rk3588 \
  bash "$verify_script" "$artifact_dir" "$model_dir" 3 16 16
if EXPECTED_RKNN_TARGET=rk3399pro \
    bash "$verify_script" "$artifact_dir" "$model_dir" 3 16 16 >/dev/null 2>&1; then
  echo "verify_board accepted unsupported EXPECTED_RKNN_TARGET" >&2
  exit 1
fi
python3 - "$artifact_dir/manifest.json" <<'PY'
import json, pathlib, sys
path = pathlib.Path(sys.argv[1])
manifest = json.loads(path.read_text())
manifest["provenance"]["target_platform"] = "rk3566"
path.write_text(json.dumps(manifest))
PY

cp "$artifact_dir/bboxes.csv" "$artifact_dir/bboxes.valid.csv"
cp "$artifact_dir/session-metadata.json" "$artifact_dir/metadata.base.json"
review_failures=0
for review_case in budget_valid budget_age13 motion_search_valid recovery_valid recovery_bad_cross recovery_bad_template recovery_bad_geometry; do
  cp "$artifact_dir/bboxes.valid.csv" "$artifact_dir/bboxes.csv"
  cp "$artifact_dir/metadata.base.json" "$artifact_dir/session-metadata.json"
  python3 - "$artifact_dir" "$review_case" <<'PY'
import csv, json, pathlib, sys
path, case = pathlib.Path(sys.argv[1]), sys.argv[2]
csv_path, metadata_path = path / "bboxes.csv", path / "session-metadata.json"
with csv_path.open(newline="") as stream:
    rows = list(csv.DictReader(stream))
fields = list(rows[0])
with metadata_path.open() as stream:
    metadata = json.load(stream)
if case.startswith("budget_"):
    metadata["occlusion_hold_frames"] = 13
    if case == "budget_age13":
        rows[2]["occlusion_coast_age_frames"] = "13"
        rows[2]["occlusion_hold_remaining_frames"] = "1"
elif case == "motion_search_valid":
    rows[1].update({
        "state": "UNCERTAIN", "quality": "REJECTED", "accepted": "0",
        "tracking_phase": "SEARCHING", "observation_valid": "0",
        "control_source": "MOTION_SEARCH", "motion_control_authorized": "1",
        "motion_prediction_confident": "1", "motion_search_age_ms": "33",
        "hypothesis_source": "DETECTOR"})
else:
    row = rows[2]
    row.update({"state": "TRACKING", "quality": "RELIABLE", "accepted": "1",
        "reacquired": "1", "control_valid": "1", "motion_control_authorized": "0",
        "tracking_phase": "TRACKING", "observation_valid": "1",
        "control_source": "OBSERVATION", "accepted_observation_path": "occlusion_probe_recovery",
        "motion_prediction_confident": "0",
        "occlusion_coast_age_frames": "6", "occlusion_hold_remaining_frames": "0",
        "occlusion_probe_type": "CONFIRM", "occlusion_probe_confirmation_hits": "2",
        "occlusion_probe_confirmation_required": "2", "occlusion_probe_rejection": "NONE",
        "tracker_skipped": "0", "detector_suppressed_by_hold": "0", "tracker_ms": "1"})
    for prefix in ("occlusion_probe_motion_", "occlusion_probe_"):
        for suffix, value in zip(("x", "y", "width", "height"), ("1", "1", "4", "4")):
            row[prefix + suffix] = value
    for template in ("anchor", "recent"):
        row["occlusion_probe_" + template + "_reliable"] = "1"
        for gate in ("score", "center", "area", "aspect"):
            row["occlusion_probe_" + template + "_" + gate + "_gate"] = "1"
    for gate in ("gate", "iou", "center", "area", "aspect"):
        row["occlusion_probe_cross_" + gate] = "1"
    row["occlusion_probe_template_gate"] = "1"
    if case == "recovery_bad_cross":
        row["occlusion_probe_cross_iou"] = row["occlusion_probe_cross_gate"] = "0"
        row["occlusion_probe_rejection"] = "CONFIRM_GEOMETRY"
    if case == "recovery_bad_template":
        row["occlusion_probe_recent_score_gate"] = "0"
    if case == "recovery_bad_geometry":
        row["occlusion_probe_width"] = ""
with csv_path.open("w", newline="") as stream:
    writer = csv.DictWriter(stream, fieldnames=fields)
    writer.writeheader()
    writer.writerows(rows)
with metadata_path.open("w") as stream:
    json.dump(metadata, stream)
PY
  if bash "$verify_script" "$artifact_dir" "$model_dir" 3 16 16 >/dev/null 2>&1; then
    if [[ "$review_case" != *_valid ]]; then
      echo "verify_board accepted $review_case" >&2
      review_failures=$((review_failures + 1))
    fi
  elif [[ "$review_case" == *_valid ]]; then
    echo "verify_board rejected $review_case" >&2
    review_failures=$((review_failures + 1))
  fi
done
cp "$artifact_dir/bboxes.valid.csv" "$artifact_dir/bboxes.csv"
cp "$artifact_dir/metadata.base.json" "$artifact_dir/session-metadata.json"
if [[ "$review_failures" != 0 ]]; then exit 1; fi
for invalid_case in unskipped_zero nan_tracker inf_runtime inf_pipeline skipped_nonzero; do
  cp "$artifact_dir/bboxes.valid.csv" "$artifact_dir/bboxes.csv"
  python3 - "$artifact_dir/bboxes.csv" "$invalid_case" <<'PY'
import csv
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
invalid_case = sys.argv[2]
with path.open(newline="") as stream:
    rows = list(csv.DictReader(stream))
    fields = list(rows[0])
if invalid_case == "unskipped_zero":
    rows[1]["tracker_skipped"] = "0"
    rows[1]["tracker_ms"] = "0"
elif invalid_case == "nan_tracker":
    rows[1]["tracker_skipped"] = "0"
    rows[1]["tracker_ms"] = "nan"
elif invalid_case == "inf_runtime":
    rows[1]["runtime_ms"] = "inf"
elif invalid_case == "inf_pipeline":
    rows[1]["pipeline_ms"] = "+inf"
elif invalid_case == "skipped_nonzero":
    rows[2]["tracker_skipped"] = "1"
    rows[2]["tracker_ms"] = "1"
with path.open("w", newline="") as stream:
    writer = csv.DictWriter(stream, fieldnames=fields)
    writer.writeheader()
    writer.writerows(rows)
PY
  if bash "$verify_script" "$artifact_dir" "$model_dir" 3 16 16 \
      >/dev/null 2>&1; then
    echo "verify_board accepted invalid latency case: $invalid_case" >&2
    exit 1
  fi
done

for invalid_case in age13 banana reference bad_bool nan_gate nan_ratio no_auth bad_probe bad_hits; do
  cp "$artifact_dir/bboxes.valid.csv" "$artifact_dir/bboxes.csv"
  python3 - "$artifact_dir/bboxes.csv" "$invalid_case" <<'PY'
import csv, sys
path, case = sys.argv[1:]
with open(path, newline="") as stream:
    rows = list(csv.DictReader(stream))
fields = list(rows[0])
key, value = {
    "age13": ("occlusion_coast_age_frames", "13"),
    "banana": ("occlusion_probe_type", "BANANA"),
    "reference": ("occlusion_reference_source", "BAD"),
    "bad_bool": ("occlusion_severe_evidence", "2"),
    "nan_gate": ("occlusion_probe_cross_gate", "nan"),
    "nan_ratio": ("occlusion_width_ratio", "nan"),
    "no_auth": ("motion_control_authorized", "0"),
    "bad_probe": ("occlusion_probe_type", "SEARCH"),
    "bad_hits": ("occlusion_probe_confirmation_hits", "3"),
}[case]
rows[2][key] = value
with open(path, "w", newline="") as stream:
    writer = csv.DictWriter(stream, fieldnames=fields)
    writer.writeheader()
    writer.writerows(rows)
PY
  if bash "$verify_script" "$artifact_dir" "$model_dir" 3 16 16 >/dev/null 2>&1; then
    echo "verify_board accepted invalid schema case: $invalid_case" >&2
    exit 1
  fi
done
for invalid_case in searching_observation detector_control motion_unconfident partial_hint reacquired_one_hit; do
  cp "$artifact_dir/bboxes.valid.csv" "$artifact_dir/bboxes.csv"
  python3 - "$artifact_dir/bboxes.csv" "$invalid_case" <<'PY'
import csv, sys
path, case = sys.argv[1:]
with open(path, newline="") as stream:
    rows = list(csv.DictReader(stream))
fields = list(rows[0])
if case == "searching_observation":
    rows[2].update({"observation_valid": "1", "accepted": "1",
                    "control_source": "OBSERVATION"})
elif case == "detector_control":
    rows[1]["control_source"] = "DETECTOR"
elif case == "motion_unconfident":
    rows[2]["motion_prediction_confident"] = "0"
elif case == "partial_hint":
    rows[1].update({"search_hint_present": "1", "search_hint_candidate_id": "7"})
else:
    rows[1].update({
        "reacquired": "1", "accepted_observation_path": "search_reacquisition",
        "search_hint_present": "1", "search_hint_candidate_id": "7",
        "search_hint_source_frame_id": "0", "search_hint_source_timestamp_ms": "0",
        "search_hint_projected_x": "1", "search_hint_projected_y": "1",
        "search_hint_projected_width": "4", "search_hint_projected_height": "4",
        "search_hint_generation": "1", "search_hint_first_hit_frame": "0",
        "search_hint_confirmation_count": "1", "search_hint_used": "1",
        "search_hint_rejection": "NONE"})
with open(path, "w", newline="") as stream:
    writer = csv.DictWriter(stream, fieldnames=fields)
    writer.writeheader()
    writer.writerows(rows)
PY
  if bash "$verify_script" "$artifact_dir" "$model_dir" 3 16 16 >/dev/null 2>&1; then
    echo "verify_board accepted invalid semantic case: $invalid_case" >&2
    exit 1
  fi
done
cp "$artifact_dir/bboxes.valid.csv" "$artifact_dir/bboxes.csv"
cp "$artifact_dir/session-metadata.json" "$artifact_dir/metadata.valid.json"
for invalid_case in schema_mismatch nan_threshold interval0 missing_scale_step invalid_scale_step \
    missing_deadband deadband_negative deadband_one deadband_inf \
    missing_alpha alpha_zero alpha_large alpha_nan \
    missing_median median_even median_large missing_direction direction_zero direction_large \
    missing_output_hysteresis output_hysteresis_negative output_hysteresis_inf \
    missing_center_alpha center_alpha_zero center_alpha_large center_alpha_nan \
    missing_center_ratio center_ratio_negative center_ratio_inf \
    missing_reacquisition reacquisition_negative reacquisition_inf bridge_duration_zero; do
  cp "$artifact_dir/metadata.valid.json" "$artifact_dir/session-metadata.json"
  python3 - "$artifact_dir/session-metadata.json" "$invalid_case" <<'PY'
import json, sys
path, case = sys.argv[1:]
with open(path) as stream: data = json.load(stream)
if case == "schema_mismatch":
    data["schema_version"] = 9
elif case == "nan_threshold":
    data["occlusion_peak_margin_drop"] = float("nan")
elif case == "interval0":
    data["occlusion_probe_interval_frames"] = 0
elif case == "missing_scale_step":
    del data["max_accepted_scale_step_ratio"]
elif case == "invalid_scale_step":
    data["max_accepted_scale_step_ratio"] = 1.01
elif case == "missing_deadband":
    del data["accepted_scale_deadband_ratio"]
elif case == "deadband_negative":
    data["accepted_scale_deadband_ratio"] = -0.01
elif case == "deadband_one":
    data["accepted_scale_deadband_ratio"] = 1.0
elif case == "deadband_inf":
    data["accepted_scale_deadband_ratio"] = float("inf")
elif case == "missing_alpha":
    del data["accepted_scale_smoothing_alpha"]
elif case == "alpha_zero":
    data["accepted_scale_smoothing_alpha"] = 0.0
elif case == "alpha_large":
    data["accepted_scale_smoothing_alpha"] = 1.01
elif case == "alpha_nan":
    data["accepted_scale_smoothing_alpha"] = float("nan")
elif case == "missing_median":
    del data["accepted_scale_median_window_frames"]
elif case == "median_even":
    data["accepted_scale_median_window_frames"] = 2
elif case == "median_large":
    data["accepted_scale_median_window_frames"] = 33
elif case == "missing_direction":
    del data["accepted_scale_direction_confirm_frames"]
elif case == "direction_zero":
    data["accepted_scale_direction_confirm_frames"] = 0
elif case == "direction_large":
    data["accepted_scale_direction_confirm_frames"] = 32
elif case == "missing_output_hysteresis":
    del data["accepted_scale_output_hysteresis_px"]
elif case == "output_hysteresis_negative":
    data["accepted_scale_output_hysteresis_px"] = -0.01
elif case == "output_hysteresis_inf":
    data["accepted_scale_output_hysteresis_px"] = float("inf")
elif case == "missing_center_alpha":
    del data["accepted_center_smoothing_alpha"]
elif case == "center_alpha_zero":
    data["accepted_center_smoothing_alpha"] = 0.0
elif case == "center_alpha_large":
    data["accepted_center_smoothing_alpha"] = 1.01
elif case == "center_alpha_nan":
    data["accepted_center_smoothing_alpha"] = float("nan")
elif case == "missing_center_ratio":
    del data["accepted_center_max_correction_ratio"]
elif case == "center_ratio_negative":
    data["accepted_center_max_correction_ratio"] = -0.01
elif case == "center_ratio_inf":
    data["accepted_center_max_correction_ratio"] = float("inf")
elif case == "missing_reacquisition":
    del data["max_reacquisition_center_distance"]
elif case == "reacquisition_negative":
    data["max_reacquisition_center_distance"] = -0.01
elif case == "bridge_duration_zero":
    data["template_bridge_max_duration_ms"] = 0
else:
    data["max_reacquisition_center_distance"] = float("inf")
with open(path, "w") as stream: json.dump(data, stream)
PY
  if bash "$verify_script" "$artifact_dir" "$model_dir" 3 16 16 >/dev/null 2>&1; then
    echo "verify_board accepted invalid metadata case: $invalid_case" >&2
    exit 1
  fi
done
cp "$artifact_dir/metadata.valid.json" "$artifact_dir/session-metadata.json"


unset EXPECTED_TRACKING_SCHEMA
if bash "$verify_script" "$artifact_dir" "$model_dir" 3 16 16 >/dev/null 2>&1; then
  echo "verify_board accepted historical v10 without explicit selection" >&2
  exit 1
fi

for schema_case in valid no_decode missing_safe_raw no_decode_with_raw missing_trust tentative_trusted \
                   ambiguous_observation ambiguous_unauthorized applied_without_id wrong_slot \
                   failed_commit_probation rejected_claimed_committed recovery_without_identity \
                   accepted_no_decode live_slot_replaced live_slot_disappeared valid_commit valid_rejected_recovery_commit \
                   valid_rollback valid_failed_rollback companion_cross_directory companion_alias json_mismatch json_missing_field; do
  python3 - "$artifact_dir" "$schema_case" <<'PY'
import csv
import json
import pathlib
import sys

root, case = pathlib.Path(sys.argv[1]), sys.argv[2]
with (root / "bboxes.valid.csv").open(newline="") as stream:
    rows = list(csv.DictReader(stream))
metadata = json.loads((root / "metadata.valid.json").read_text())
metadata.update(schema_version=11, observation_valid_semantics="ACCEPTED_OBSERVATION",
                rect0_bbox="tracking_bbox", frame_json_file="bboxes.csv.jsonl")
roles = ("recent", "safe", "staged")
geometry = ("x", "y", "width", "height")
raw = (*geometry, "score", "penalized_peak", "psr", "apce", "peak_margin")
roles_and_raw = {f"{role}_raw_{key}": "" for role in roles for key in raw}
defaults = {
    "schema_version": "11", "state_change_reason": "NONE", "rect0_source": "ACCEPTED_OBSERVATION",
    "active_template": "RECENT", "selected_template": "RECENT",
    "safe_template_used": "0", "safe_reused_recent": "0",
    "trusted_baseline_psr": "", "trusted_baseline_apce": "", "trusted_baseline_peak_margin": "",
    "response_psr_relative_delta": "", "response_apce_relative_delta": "",
    "response_peak_margin_absolute_delta": "", "template_action": "NONE",
    "template_transaction_id": "", "template_slot_id": "",
    "template_transition_recovery": "0", "template_transition_applied": "0",
    "template_transition_result": "NONE", "template_transition_reason": "NONE",
    "template_probation_active": "0", "candidate_source": "NONE", "candidate_id": "",
    "candidate_generation": "", "recovery_confirmation_hits": "0", "recovery_conflict": "0",
    "recovery_probe_source": "NONE", "recovery_probe_id": "", "recovery_probe_generation": "",
    "recovery_reason": "NONE",
    **{f"recovery_probe_{key}": "" for key in geometry},
    **{f"{role}_head_executed": "0" for role in roles},
    **{f"{role}_decode_status": "NOT_RUN" for role in roles},
    **roles_and_raw,
}
for row in rows:
    row.update(defaults, observation_present=row["accepted"], trusted_for_state_update=row["accepted"])
rows[0].update(rect0_source="INITIAL_ROI", state_change_reason="INITIALIZED")
rows[1].update(state="SUSPECTED", tracking_phase="SUSPECTED", observation_present="1",
               trusted_for_state_update="0", observation_valid="0", accepted="0",
               accepted_observation_path="direct", rect0_source="RECENT_OBSERVATION",
               state_change_reason="RESPONSE_DEGRADED", recent_head_executed="1",
               recent_decode_status="DECODED", safe_head_executed="1", safe_template_used="1",
               safe_decode_status="DECODED")
for role in ("recent", "safe"):
    rows[1].update({f"{role}_raw_{name}": value for name, value in zip(
        raw, ("1", "1", "4", "4", "0.9", "0.8", "8", "12", "0.15"))})
rows[2].update(rect0_source="MOTION_PREDICTION", recovery_conflict="1",
               candidate_source="DETECTOR", candidate_id="3", candidate_generation="2",
               recovery_reason="IDENTITY_CONFLICT", detector_same_class_ambiguous="1",
               detector_same_class_candidates="1")
if case in {"no_decode", "no_decode_with_raw"}:
    rows[1]["safe_decode_status"] = "NO_DECODE"
    if case == "no_decode":
        rows[1].update({f"safe_raw_{key}": "" for key in raw})
elif case == "missing_safe_raw":
    rows[1]["safe_raw_psr"] = ""
elif case == "missing_trust":
    for row in rows:
        del row["trusted_for_state_update"]
elif case == "tentative_trusted":
    rows[1]["trusted_for_state_update"] = "1"
elif case == "ambiguous_observation":
    rows[2]["control_source"] = "OBSERVATION"
elif case == "ambiguous_unauthorized":
    rows[2]["motion_control_authorized"] = "0"
elif case == "applied_without_id":
    rows[1]["template_transition_applied"] = "1"
elif case in {"accepted_no_decode", "recovery_without_identity"}:
    rows[1].update(state="TRACKING", tracking_phase="TRACKING", accepted="1",
                   observation_valid="1", trusted_for_state_update="1",
                   rect0_source="ACCEPTED_OBSERVATION")
    if case == "accepted_no_decode":
        rows[1].update(roles_and_raw, recent_decode_status="NO_DECODE", safe_decode_status="NO_DECODE")
    else:
        rows[1].update(reacquired="1", template_action="STAGE_RECENT",
                       template_transaction_id="5", template_slot_id="5",
                       template_transition_recovery="1", template_transition_applied="1",
                       template_transition_result="APPLIED", template_probation_active="1",
                       template_transition_reason="RECOVERY_CONFIRMED",
                       recovery_reason="RECOVERY_STAGED", accepted_observation_path="search_reacquisition")
elif case in {"valid_commit", "wrong_slot", "failed_commit_probation", "rejected_claimed_committed",
              "live_slot_replaced", "live_slot_disappeared", "valid_rollback", "valid_failed_rollback",
              "valid_rejected_recovery_commit"}:
    rows[1].update(state="TRACKING", tracking_phase="TRACKING",
                   accepted="1", observation_valid="1", trusted_for_state_update="1",
                   rect0_source="ACCEPTED_OBSERVATION", template_action="STAGE_RECENT",
                   template_transaction_id="5", template_slot_id="5",
                   template_transition_result="APPLIED", template_transition_applied="1",
                   template_transition_reason="STABLE_TRACKING", template_probation_active="1")
    rows[2].update(template_action="COMMIT_RECENT", template_transaction_id="6",
                   template_slot_id="5", template_transition_result="APPLIED",
                   template_transition_applied="1", template_transition_reason="PROBATION_PASSED",
                   template_probation_active="0", state="TRACKING", tracking_phase="TRACKING",
                   observation_present="1", observation_valid="1", accepted="1",
                   trusted_for_state_update="1", control_source="OBSERVATION",
                   rect0_source="ACCEPTED_OBSERVATION", motion_control_authorized="0",
                   occlusion_coast_age_frames="0", occlusion_hold_remaining_frames="0",
                   recovery_conflict="0", detector_same_class_ambiguous="0",
                   recovery_reason="NONE", candidate_source="NONE", candidate_id="",
                   candidate_generation="")
    rows[2].update(recent_head_executed="1", recent_decode_status="DECODED", tracker_skipped="0",
                   detector_suppressed_by_hold="0", tracker_ms="1", detector_same_class_candidates="0")
    rows[2].update({f"recent_raw_{key}": rows[1][f"recent_raw_{key}"] for key in raw})
    if case == "wrong_slot":
        rows[2]["template_slot_id"] = "4"
    if case in {"valid_rollback", "valid_failed_rollback"}:
        rows[2].update(template_action="ROLLBACK_RECENT", template_transition_reason="FRAME_INVALIDATED")
        if case == "valid_failed_rollback":
            rows[2].update(template_transition_result="TRACKER_REJECTED", template_transition_applied="0")
    if case in {"rejected_claimed_committed", "valid_rejected_recovery_commit"}:
        rows[1].update(template_transition_recovery="1", template_transition_reason="RECOVERY_CONFIRMED",
                       candidate_source="RECENT", candidate_id="1", candidate_generation="2",
                       recovery_confirmation_hits="2", recovery_reason="RECOVERY_STAGED",
                       accepted_observation_path="search_reacquisition", reacquired="1")
        rows[2].update(template_transition_result="TRACKER_REJECTED", template_transition_applied="0",
                       template_transition_recovery="1", recovery_reason="RECOVERY_COMMIT_REJECTED",
                       state="UNCERTAIN", tracking_phase="SEARCHING", accepted="0", observation_valid="0",
                       observation_present="0", trusted_for_state_update="0", control_source="MOTION_SEARCH",
                       motion_control_authorized="1", rect0_source="MOTION_PREDICTION")
        if case == "rejected_claimed_committed":
            rows[2]["recovery_reason"] = "RECOVERY_COMMITTED"
    if case == "live_slot_replaced":
        rows[2].update(template_action="STAGE_RECENT", template_slot_id="6",
                       template_transition_reason="STABLE_TRACKING", template_probation_active="1")
    if case == "live_slot_disappeared":
        rows[2].update(template_action="NONE", template_slot_id="", template_transaction_id="",
                       template_transition_reason="NONE", template_transition_result="NONE",
                       template_transition_applied="0")
    if case == "failed_commit_probation":
        rows[2].update(template_transition_result="TRACKER_REJECTED",
                       template_transition_applied="0", template_probation_active="1")

shared = set(defaults) | {
    "frame_id", "timestamp_ms", "state", "tracking_phase", "observation_valid", "accepted",
    "reacquired", "control_valid", "control_source", "motion_control_authorized",
    "observation_present", "trusted_for_state_update", "detector_same_class_ambiguous",
    *(f"{box}_{key}" for box in ("tracking", "display", "control") for key in geometry),
}
frames = [{key: row[key] or None for key in shared if key in row} for row in rows]
if case == "json_mismatch":
    frames[1]["safe_raw_psr"] = 20.0
if case == "json_missing_field":
    del frames[1]["template_slot_id"]
with (root / "bboxes.csv").open("w", newline="") as stream:
    writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
    writer.writeheader()
    writer.writerows(rows)
json_path = root / "bboxes.csv.jsonl"
if case == "companion_cross_directory":
    json_path = root.parent / "companion-dir" / "frames.jsonl"
    json_path.parent.mkdir()
    metadata["frame_json_file"] = "../companion-dir/frames.jsonl"
elif case == "companion_alias":
    alias = root / "alias.jsonl"
    alias.symlink_to(root / "bboxes.csv")
    metadata["frame_json_file"] = alias.name
json_path.write_text("".join(json.dumps(frame) + "\n" for frame in frames))
(root / "session-metadata.json").write_text(json.dumps(metadata))
PY
  case "$schema_case" in
    valid|no_decode|valid_commit|valid_rollback|valid_failed_rollback|valid_rejected_recovery_commit|companion_cross_directory)
      bash "$verify_script" "$artifact_dir" "$model_dir" 3 16 16 ;;
    *)
      if bash "$verify_script" "$artifact_dir" "$model_dir" 3 16 16 >/dev/null 2>&1; then
        echo "verify_board accepted invalid v11 case: $schema_case" >&2
        exit 1
      fi ;;
  esac
done
