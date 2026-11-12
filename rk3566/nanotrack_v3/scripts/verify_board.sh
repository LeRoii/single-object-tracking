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
expected_rknn_target=${EXPECTED_RKNN_TARGET:-rk3566}

case "$expected_rknn_target" in
  rk3566|rk3588) ;;
  *)
    echo "unsupported EXPECTED_RKNN_TARGET: $expected_rknn_target" >&2
    exit 2
    ;;
esac

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

python3 - "$artifact_dir" "$model_dir" "$expected_frames" "$frame_width" "$frame_height" "$detector_model" "$detector_manifest" "$detector_labels" "$expected_rknn_target" "${EXPECTED_TRACKING_SCHEMA:-13}" <<'PY'
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
expected_rknn_target = sys.argv[9]
EXPECTED_TRACKING_SCHEMA = int(sys.argv[10])
if EXPECTED_TRACKING_SCHEMA not in {10, 11, 12, 13}:
    raise SystemExit("expected tracking schema must be 10, 11, 12, or 13")
VALID_TARGET_MODES = {"FULL_OBJECT", "SUBTARGET"}
metadata = json.loads((artifact_dir / "session-metadata.json").read_text())
for key, low, high in [
    ("occlusion_hold_frames", 1, 300), ("occlusion_reference_window", 3, 60),
    ("occlusion_trigger_consecutive_unreliable", 1, 10),
    ("occlusion_probe_interval_frames", 1, 300),
    ("occlusion_gradual_consecutive_frames", 1, 60),
    ("occlusion_min_reference_samples", 1, 60),
    ("occlusion_suspicion_clear_frames", 1, 60),
    ("accepted_scale_median_window_frames", 1, 31),
    ("accepted_scale_direction_confirm_frames", 1, 31),
    ("template_bridge_min_stable_frames", 0, 60),
    ("template_bridge_max_duration_ms", 0, 1000),
    ("motion_search_confidence_ms", 1, 60000),
    ("search_probe_interval_ms", 1, 60000),
]:
    value = metadata.get(key)
    if isinstance(value, bool) or not isinstance(value, int) or not low <= value <= high:
        raise SystemExit(f"invalid metadata {key}")
if metadata["accepted_scale_median_window_frames"] % 2 == 0:
    raise SystemExit("invalid metadata accepted_scale_median_window_frames")
minimum_search_age = metadata.get("search_reacquisition_min_age_ms")
if (isinstance(minimum_search_age, bool) or
        not isinstance(minimum_search_age, int) or
        minimum_search_age < 0 or
        minimum_search_age > metadata["motion_search_confidence_ms"]):
    raise SystemExit("invalid metadata search_reacquisition_min_age_ms")
if (metadata["template_bridge_min_stable_frames"] > 0 and
        metadata["template_bridge_max_duration_ms"] == 0):
    raise SystemExit("enabled metadata template bridge requires a duration")
for key in ("occlusion_trigger_min_dimension_ratio", "occlusion_gradual_dimension_ratio",
            "occlusion_shrink_step_ratio", "occlusion_confirmation_iou",
            "max_accepted_scale_step_ratio",
            "accepted_scale_deadband_ratio", "accepted_scale_smoothing_alpha",
            "accepted_scale_output_hysteresis_px",
            "accepted_center_smoothing_alpha",
            "accepted_center_max_correction_ratio",
            "max_reacquisition_center_distance",
            "occlusion_peak_margin_drop", "occlusion_motion_residual_threshold",
            "occlusion_confirmation_center_distance", "occlusion_confirmation_max_area_log_change",
            "occlusion_confirmation_max_aspect_log_change"):
    value = metadata.get(key)
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
        raise SystemExit(f"invalid metadata {key}")
    if key in {"occlusion_peak_margin_drop", "max_reacquisition_center_distance",
               "accepted_scale_output_hysteresis_px",
               "accepted_center_max_correction_ratio"}:
        valid = value >= 0
    elif key == "accepted_scale_deadband_ratio":
        valid = 0 <= value < 1
    elif key in {"occlusion_trigger_min_dimension_ratio", "occlusion_gradual_dimension_ratio",
                 "occlusion_shrink_step_ratio", "occlusion_confirmation_iou",
                 "max_accepted_scale_step_ratio", "accepted_scale_smoothing_alpha",
                 "accepted_center_smoothing_alpha"}:
        valid = 0 < value <= 1
    else:
        valid = value > 0
    if not valid:
        raise SystemExit(f"out of range metadata {key}")
for key in ("motion_max_speed_diagonals_per_s",
            "motion_max_acceleration_diagonals_per_s2",
            "motion_scale_rate_limit_per_s"):
    value = metadata.get(key)
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value) or value <= 0:
        raise SystemExit(f"invalid metadata {key}")
if not isinstance(metadata.get("motion_search_enabled"), bool):
    raise SystemExit("invalid metadata motion_search_enabled")
if not isinstance(metadata.get("search_reacquisition_requires_detector"), bool):
    raise SystemExit("invalid metadata search_reacquisition_requires_detector")
if not isinstance(metadata.get("controller_config_identity"), str) or not metadata["controller_config_identity"]:
    raise SystemExit("invalid metadata controller_config_identity")

manifest = json.loads((artifact_dir / "manifest.json").read_text())
manifest_target = manifest.get("provenance", {}).get("target_platform")
if manifest_target != expected_rknn_target:
    raise SystemExit(
        f"tracking manifest target mismatch: expected {expected_rknn_target}, got {manifest_target}"
    )
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
    "accepted", "reacquired", "x", "y", "width", "height", "hypothesis_source",
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
    "occlusion_probe_anchor_score_gate", "occlusion_probe_anchor_center_gate", "occlusion_probe_anchor_area_gate", "occlusion_probe_anchor_aspect_gate", "occlusion_probe_recent_score_gate", "occlusion_probe_recent_center_gate", "occlusion_probe_recent_area_gate", "occlusion_probe_recent_aspect_gate",
    "occlusion_probe_rejection", "occlusion_probe_motion_x", "occlusion_probe_motion_y", "occlusion_probe_motion_width", "occlusion_probe_motion_height", "occlusion_probe_x", "occlusion_probe_y", "occlusion_probe_width", "occlusion_probe_height", "occlusion_probe_anchor_reliable", "occlusion_probe_recent_reliable", "occlusion_probe_template_gate", "occlusion_probe_cross_gate", "occlusion_probe_cross_iou", "occlusion_probe_cross_center", "occlusion_probe_cross_area", "occlusion_probe_cross_aspect",
    "occlusion_reference_source", "occlusion_suspicion_active", "occlusion_suspicion_started", "occlusion_suspicion_cleared", "occlusion_suspicion_frames", "occlusion_clean_frames", "occlusion_width_shrink_frames", "occlusion_height_shrink_frames", "occlusion_severe_evidence", "occlusion_gradual_evidence", "occlusion_template_evidence", "occlusion_peak_margin_evidence", "occlusion_motion_residual_evidence", "occlusion_trigger_reason", "occlusion_coast_age_frames", "occlusion_probe_type", "occlusion_probe_confirmation_hits", "occlusion_probe_confirmation_required", "motion_control_authorized",
    "target_class_status", "target_class_id", "target_class_reason",
    "detector_recovery_enabled",
    "tracking_phase", "observation_valid", "control_source",
    "accepted_observation_path",
    "tracking_x", "tracking_y", "tracking_width", "tracking_height",
    "display_x", "display_y", "display_width", "display_height",
    "control_x", "control_y", "control_width", "control_height",
    "motion_search_age_ms", "motion_prediction_confident",
    "motion_velocity_x", "motion_velocity_y", "motion_acceleration_x",
    "motion_acceleration_y", "motion_scale_velocity_width",
    "motion_scale_velocity_height", "search_hint_present",
    "search_hint_candidate_id", "search_hint_source_frame_id",
    "search_hint_source_timestamp_ms", "search_hint_projected_x",
    "search_hint_projected_y", "search_hint_projected_width",
    "search_hint_projected_height", "search_hint_generation",
    "search_hint_first_hit_frame", "search_hint_confirmation_count",
    "search_hint_used", "search_hint_rejection",
}
bbox_components = ("x", "y", "width", "height")
raw_components = (*bbox_components, "score", "penalized_peak", "psr", "apce", "peak_margin")
head_roles = ("recent", "safe", "staged")
v11_booleans = {
    "observation_present", "trusted_for_state_update", "safe_template_used",
    "safe_reused_recent", "template_transition_recovery", "template_transition_applied",
    "template_probation_active", "recovery_conflict",
    *(f"{role}_head_executed" for role in head_roles),
}
v11_baselines = ("trusted_baseline_psr", "trusted_baseline_apce", "trusted_baseline_peak_margin")
v11_deltas = ("response_psr_relative_delta", "response_apce_relative_delta",
              "response_peak_margin_absolute_delta")
v11_fields = v11_booleans | {
    "state_change_reason", "rect0_source", "active_template", "selected_template",
    "template_action", "template_transaction_id", "template_slot_id",
    "revoked_template_slot_id", "template_slot_revocation_reason",
    "template_transition_result", "template_transition_reason",
    "candidate_source", "candidate_id", "candidate_generation",
    "recovery_confirmation_hits", "recovery_probe_source", "recovery_probe_id",
    "recovery_probe_generation", "recovery_reason", *v11_baselines, *v11_deltas,
    *(f"{role}_decode_status" for role in head_roles),
    *(f"{role}_raw_{component}" for role in head_roles for component in raw_components),
    *(f"recovery_probe_{component}" for component in bbox_components),
}

def verify_recent_safe(row, index, last_transaction, staged_slot, reinitialized):
    def reject(reason):
        raise SystemExit(f"{reason} at row {index}")

    for key in v11_booleans:
        if row[key] not in {"0", "1"}:
            reject(f"invalid {key}")
    present = row["observation_present"] == "1"
    trusted = row["trusted_for_state_update"] == "1"
    accepted = row["accepted"] == "1"
    phase = row["tracking_phase"]
    if trusted != accepted or (trusted and not present):
        reject("inconsistent observation trust")
    if phase == "SUSPECTED" and (not present or trusted or row["recent_decode_status"] != "DECODED"):
        reject("invalid tentative recent observation")
    if phase == "SEARCHING" and present:
        reject("searching row reports an observation")
    if row["state"] == "SUSPECTED" and phase != "SUSPECTED":
        reject("suspected state outside suspected phase")
    if row["rect0_source"] == "INITIAL_ROI":
        if index != 0 or not accepted or row["state_change_reason"] != "INITIALIZED":
            reject("invalid initial rect0 source")
    else:
        expected_source = {"TRACKING": "ACCEPTED_OBSERVATION", "SUSPECTED": "RECENT_OBSERVATION",
                           "SEARCHING": "MOTION_PREDICTION"}[phase]
        if row["rect0_source"] != expected_source:
            reject("rect0 source disagrees with tracking phase")
    if row["state_change_reason"] not in {
        "NONE", "INITIALIZED", "RESPONSE_DEGRADED", "SUSPICION_ENTERED",
        "SUSPICION_CONTINUES", "SUSPICION_CLEARED", "SUSPICION_ESCALATED",
        "OBSERVATION_CONFIRMED", "OBSERVATION_LOST", "SEARCH_MODE_CHANGED",
        "RECOVERY_PREPARE_FAILED", "RECOVERY_COMMIT_REJECTED",
    }:
        reject("invalid state change reason")
    if row["recovery_reason"] not in {
        "NONE", "SEARCH_MIN_AGE", "PROBE_EXPIRED", "IDENTITY_CONFLICT",
        "RECOVERY_CONFIRMED", "AWAITING_CONFIRMATION", "NO_ELIGIBLE_CANDIDATE",
        "RECOVERY_STAGED", "RECOVERY_PREPARE_FAILED", "RECOVERY_COMMITTED",
        "RECOVERY_COMMIT_REJECTED", "RECOVERY_PROBATION_REJECTED",
    }:
        reject("invalid recovery reason")
    if row["active_template"] != "RECENT" or row["selected_template"] not in {"ANCHOR", "RECENT", "SAFE", "STAGED"}:
        reject("invalid template role")
    reuse = row["safe_reused_recent"] == "1"
    if reuse and (row["recent_head_executed"] != "1" or row["safe_head_executed"] == "1" or
                  row["safe_decode_status"] != row["recent_decode_status"] or
                  any(row["safe_raw_" + key] != row["recent_raw_" + key] for key in raw_components)):
        reject("inconsistent physical safe result reuse")
    if (row["safe_template_used"] == "1") != (reuse or row["safe_head_executed"] == "1"):
        reject("inconsistent safe-template use")
    if row["staged_head_executed"] == "1" and row["safe_template_used"] == "1":
        reject("staged and safe heads cannot run together")
    if row["tracker_skipped"] == "1" and any(row[f"{role}_head_executed"] == "1" for role in head_roles):
        reject("skipped tracker reports head execution")
    for role in head_roles:
        status = row[f"{role}_decode_status"]
        ran = row[f"{role}_head_executed"] == "1" or (role == "safe" and reuse)
        values = [row[f"{role}_raw_{key}"] for key in raw_components]
        if ran and status not in {"DECODED", "NO_DECODE"}:
            reject(f"missing raw {role} head result")
        if not ran and status != "NOT_RUN":
            reject(f"{role} result without execution")
        if status == "DECODED":
            if not all(values) or not all(math.isfinite(float(v)) for v in values):
                reject(f"missing or non-finite raw {role} fields")
            if any(float(v) <= 0 for v in values[2:4]):
                reject(f"nonpositive raw {role} geometry")
        elif any(values):
            reject(f"raw {role} fields without a decoded result")

    if accepted and row["rect0_source"] != "INITIAL_ROI":
        selected = row["selected_template"].lower()
        if selected not in head_roles or row[f"{selected}_decode_status"] != "DECODED":
            reject("accepted tracking lacks a decoded selected observation")

    reference = [row[key] for key in v11_baselines]
    deltas = [row[key] for key in v11_deltas]
    if any(reference) and not all(reference):
        reject("partial trusted response baseline")
    if any(deltas) and not all(reference):
        reject("relative response delta without trusted baseline")
    if any(value and not math.isfinite(float(value)) for value in reference + deltas):
        reject("non-finite trusted baseline or response delta")

    revoked_id = row["revoked_template_slot_id"]
    revocation_reason = row["template_slot_revocation_reason"]
    if revocation_reason == "NONE":
        if revoked_id:
            reject("revoked template slot without a reason")
    else:
        if revocation_reason not in {"FRAME_ABORTED", "FRAME_APPLICATION_FAILED", "REINITIALIZED"}:
            reject("invalid template slot revocation reason")
        if not revoked_id.isdigit() or staged_slot is None or int(revoked_id) != staged_slot[0]:
            reject("template revocation does not identify the live slot")
        if revocation_reason == "REINITIALIZED" and not reinitialized:
            reject("template revocation claims an unrecorded reinitialization")
        staged_slot = None
    if reinitialized and staged_slot is not None:
        reject("reinitialization omitted its live slot revocation")

    action, result = row["template_action"], row["template_transition_result"]
    applied = row["template_transition_applied"] == "1"
    recovery = row["template_transition_recovery"] == "1"
    probation = row["template_probation_active"] == "1"
    transaction_id, slot_id = row["template_transaction_id"], row["template_slot_id"]
    transition_reason = row["template_transition_reason"]
    recovery_reason = row["recovery_reason"]
    if (recovery_reason == "RECOVERY_COMMITTED") != (
            action == "COMMIT_RECENT" and recovery and applied):
        reject("recovery commit claim disagrees with its acknowledgement")
    if (recovery_reason == "RECOVERY_STAGED") != (
            action == "STAGE_RECENT" and recovery and applied):
        reject("recovery stage claim disagrees with its acknowledgement")
    if action == "NONE":
        if result != "NONE" or applied or recovery or transaction_id or slot_id or transition_reason != "NONE":
            reject("template acknowledgement without an action")
    else:
        if action not in {"STAGE_RECENT", "COMMIT_RECENT", "ROLLBACK_RECENT"}:
            reject("invalid template action")
        if result not in {"APPLIED", "PREPARE_FAILED", "TRACKER_REJECTED"} or applied != (result == "APPLIED"):
            reject("invalid template transition result")
        if not transaction_id.isdigit() or not slot_id.isdigit():
            reject("missing template transaction or slot ID")
        transaction_id, slot_id = int(transaction_id), int(slot_id)
        valid_reasons = {
            "STAGE_RECENT": {"RECOVERY_CONFIRMED" if recovery else "STABLE_TRACKING"},
            "COMMIT_RECENT": {"PROBATION_PASSED"},
            "ROLLBACK_RECENT": {"FRAME_INVALIDATED", "PROBATION_REJECTED"},
        }
        if transaction_id <= last_transaction or slot_id <= 0 or transition_reason not in valid_reasons[action]:
            reject("invalid template transaction identity or reason")
        if action == "STAGE_RECENT":
            if row["staged_head_executed"] == "1":
                reject("stage transition claims a staged head before preparation")
            if (staged_slot is not None or slot_id != transaction_id or
                    probation != applied or (applied and not accepted)):
                reject("inconsistent stage acknowledgement")
            if recovery and (applied != (row["reacquired"] == "1") or
                             (not applied and phase != "SEARCHING")):
                reject("invalid recovery stage semantics")
            if recovery and applied and row["accepted_observation_path"] != "search_reacquisition":
                reject("recovery stage lacks a confirmed search reacquisition")
            staged_slot = (slot_id, recovery) if applied else None
        else:
            if (slot_id >= transaction_id or (slot_id, recovery) != staged_slot or
                    probation or result == "PREPARE_FAILED"):
                reject("invalid template terminal acknowledgement")
            if action == "COMMIT_RECENT" and recovery and not applied and (
                    phase != "SEARCHING" or accepted or trusted or
                    recovery_reason != "RECOVERY_COMMIT_REJECTED"):
                reject("rejected recovery commit retained tracking authorization")
            staged_slot = None
        last_transaction = transaction_id
    if probation != (staged_slot is not None):
        reject("template probation disagrees with the live stage slot")

    for source_key, id_key, generation_key in (
        ("candidate_source", "candidate_id", "candidate_generation"),
        ("recovery_probe_source", "recovery_probe_id", "recovery_probe_generation"),
    ):
        source, candidate_id, generation = row[source_key], row[id_key], row[generation_key]
        if source == "NONE":
            if candidate_id or generation:
                reject("candidate ID without source")
        elif source not in {"RECENT", "SAFE", "DETECTOR"} or not candidate_id.isdigit() or int(candidate_id) <= 0 or not generation.isdigit():
            reject("invalid candidate key")
    probe_box = [row["recovery_probe_" + key] for key in bbox_components]
    if row["recovery_probe_source"] == "NONE":
        if any(probe_box):
            reject("probe box without candidate key")
    elif not all(probe_box) or not all(math.isfinite(float(v)) for v in probe_box) or any(float(v) <= 0 for v in probe_box[2:]):
        reject("invalid recovery probe box")
    hits = row["recovery_confirmation_hits"]
    if hits not in {"0", "1", "2"} or (hits != "0" and row["candidate_source"] == "NONE"):
        reject("invalid recovery confirmation hits")
    if row["recovery_conflict"] == "1" and (hits != "0" or row["reacquired"] == "1" or
                                             row["control_source"] == "OBSERVATION"):
        reject("conflicted candidate confirmed an observation")
    if row["accepted_observation_path"] == "search_reacquisition" and not (
            hits == "2" and row["candidate_source"] != "NONE" and row["recovery_conflict"] == "0" and
            action == "STAGE_RECENT" and applied and recovery and row["reacquired"] == "1"):
        reject("reacquisition lacks confirmed applied stage")
    return last_transaction, staged_slot

if EXPECTED_TRACKING_SCHEMA >= 11:
    required |= v11_fields
if EXPECTED_TRACKING_SCHEMA >= 12:
    required.add("motion_update_valid")
if EXPECTED_TRACKING_SCHEMA >= 13:
    required |= {
        "detector_request_purpose", "detector_batch_purpose", "display_box_source",
        "display_class_id", "display_correction_pending", "display_correction_applied",
        "display_confirmation_count", "display_refresh_interval_ms",
        "display_refresh_paused", "display_runtime_p95_ms",
    }
missing = required - set(rows[0])
if missing:
    raise SystemExit(f"CSV header is missing: {sorted(missing)}")
first_frame_id = int(rows[0]["frame_id"])
last_timestamp = None
last_transaction, staged_slot = 0, None
# Interactive reselection is recorded in session metadata even though the
# paused ROI replacement does not write a second CSV row for that frame.
reset_frames = {
    entry["frame_id"] for entry in metadata.get("target_class_initializations", [])
    if isinstance(entry, dict) and isinstance(entry.get("frame_id"), int)
    and entry.get("initialization_index", 0) > 1
} if EXPECTED_TRACKING_SCHEMA >= 11 else set()
for index, row in enumerate(rows):
    if row["schema_version"] != str(EXPECTED_TRACKING_SCHEMA):
        raise SystemExit(f"unexpected CSV schema version at row {index}")
    enums = {
        "state": {"TRACKING", "UNCERTAIN", "LOCAL_RECOVERY", "ASSISTED_RECOVERY", "LOST", "OCCLUSION_HOLD"},
        "quality": {"RELIABLE", "WEAK", "REJECTED"},
        "hypothesis_source": {"MOTION", "DETECTOR"},
        "detector_status": {"NONE", "ACCEPTED", "STALE", "FRAME_MISMATCH", "INVALID", "POSITION_REJECTED"},
        "target_class_status": {"FIXED", "ESTABLISHED", "UNKNOWN"},
        "target_class_reason": {"FIXED_OVERRIDE", "UNIQUE_MATCH", "NO_MATCH", "AMBIGUOUS", "DETECTOR_UNAVAILABLE", "DETECTOR_ERROR"},
        "occlusion_reference_source": {"NONE", "PARTIAL_STABLE_MEDIAN", "FULL_STABLE_MEDIAN"},
        "occlusion_trigger_reason": {"NONE", "SEVERE_WIDTH", "SEVERE_HEIGHT",
            "GRADUAL_WIDTH_TEMPLATE", "GRADUAL_WIDTH_PEAK", "GRADUAL_WIDTH_MOTION",
            "GRADUAL_HEIGHT_TEMPLATE", "GRADUAL_HEIGHT_PEAK", "GRADUAL_HEIGHT_MOTION"},
        "occlusion_probe_type": {"NONE", "SEARCH", "CONFIRM"},
        "occlusion_probe_rejection": {"NONE", "MISSING_TEMPLATE", "QUALITY_OR_MOTION_GEOMETRY",
            "TEMPLATE_AGREEMENT", "MISSING_CANDIDATE", "CONFIRM_GEOMETRY"},
        "tracking_phase": {"TRACKING", "SEARCHING"},
        "control_source": {"NONE", "OBSERVATION", "MOTION_SEARCH"},
        "accepted_observation_path": {"direct", "template_bridge", "short_confirmation",
            "local_or_deeper_recovery", "detector_recovery",
            "occlusion_probe_recovery", "search_reacquisition"},
    }
    if EXPECTED_TRACKING_SCHEMA >= 11:
        enums["state"].add("SUSPECTED")
        enums["tracking_phase"].add("SUSPECTED")
    for key, valid in enums.items():
        if row[key] not in valid:
            raise SystemExit(f"invalid {key} at row {index}")
    boolean_keys = {"accepted", "reacquired", "detector_configured", "detector_ready",
        "detector_requested", "detector_recovery_enabled", "occlusion_suspicion_active", "occlusion_suspicion_started",
        "occlusion_suspicion_cleared", "occlusion_severe_evidence", "occlusion_gradual_evidence",
        "occlusion_template_evidence", "occlusion_peak_margin_evidence",
        "occlusion_motion_residual_evidence", "motion_control_authorized",
        "observation_valid", "motion_prediction_confident", "search_hint_present"}
    for key in boolean_keys:
        if row[key] not in {"0", "1"}:
            raise SystemExit(f"invalid {key} at row {index}")
    observation_valid = row["observation_valid"] == "1"
    control_valid = row["control_valid"] == "1"
    if observation_valid != (row["accepted"] == "1"):
        raise SystemExit(f"observation/acceptance mismatch at row {index}")
    if row["tracking_phase"] == "TRACKING":
        if not observation_valid or not control_valid or row["control_source"] != "OBSERVATION":
            raise SystemExit(f"invalid tracking semantics at row {index}")
    elif observation_valid:
        raise SystemExit(f"searching row reports observation at row {index}")
    if (row["control_source"] == "NONE") != (not control_valid):
        raise SystemExit(f"control source/valid mismatch at row {index}")
    if row["control_source"] == "OBSERVATION" and row["tracking_phase"] not in {"TRACKING", "SUSPECTED"}:
        raise SystemExit(f"observation control outside tracking or suspicion at row {index}")
    if EXPECTED_TRACKING_SCHEMA >= 11:
        last_transaction, staged_slot = verify_recent_safe(
            row, index, last_transaction, staged_slot, int(row["frame_id"]) - 1 in reset_frames)
    if EXPECTED_TRACKING_SCHEMA >= 12:
        if row["motion_update_valid"] not in {"0", "1"}:
            raise SystemExit(f"invalid motion_update_valid at row {index}")
        if row["motion_update_valid"] == "1" and not (
                row["accepted"] == "1" and row["observation_present"] == "1" and
                row["trusted_for_state_update"] == "1" and
                row["tracking_phase"] == "TRACKING"):
            raise SystemExit(f"motion update without trusted tracking observation at row {index}")
    if EXPECTED_TRACKING_SCHEMA >= 13:
        if row["detector_request_purpose"] not in {"NONE", "SEARCH_RECOVERY", "INITIALIZATION", "DISPLAY_REFRESH"}:
            raise SystemExit(f"invalid detector request purpose at row {index}")
        if row["detector_batch_purpose"] not in {"NONE", "SEARCH_RECOVERY", "INITIALIZATION", "DISPLAY_REFRESH"}:
            raise SystemExit(f"invalid detector batch purpose at row {index}")
        if row["display_box_source"] not in {
                "CONTROLLER_FALLBACK", "TRACKER_PROPAGATED", "DETECTOR_INITIALIZED",
                "DETECTOR_CONFIRMED", "SUSPECTED_FROZEN", "SEARCH_PREDICTED",
                "REACQUISITION_BLEND"}:
            raise SystemExit(f"invalid display box source at row {index}")
        for key in ("display_correction_pending", "display_correction_applied", "display_refresh_paused"):
            if row[key] not in {"0", "1"}:
                raise SystemExit(f"invalid {key} at row {index}")
    if row["control_source"] == "MOTION_SEARCH":
        if row["tracking_phase"] != "SEARCHING" or row["motion_control_authorized"] != "1" or row["motion_prediction_confident"] != "1":
            raise SystemExit(f"invalid motion-search control at row {index}")
    optional_gates = [key for key in required if key.startswith("occlusion_probe_") and
                      (key.endswith("_gate") or key.endswith("_reliable") or
                       key in {"occlusion_probe_cross_iou", "occlusion_probe_cross_center",
                               "occlusion_probe_cross_area", "occlusion_probe_cross_aspect"})]
    for key in optional_gates:
        if row[key] not in {"", "0", "1"}:
            raise SystemExit(f"invalid {key} at row {index}")
    counts = ("occlusion_suspicion_frames", "occlusion_clean_frames",
        "occlusion_width_shrink_frames", "occlusion_height_shrink_frames",
        "occlusion_coast_age_frames", "occlusion_hold_remaining_frames",
        "occlusion_probe_confirmation_hits", "occlusion_probe_confirmation_required")
    for key in counts:
        if not row[key].isdigit():
            raise SystemExit(f"invalid {key} at row {index}")
    for key in ("occlusion_reference_width", "occlusion_reference_height",
                "occlusion_width_ratio", "occlusion_height_ratio",
                "occlusion_probe_motion_x", "occlusion_probe_motion_y",
                "occlusion_probe_motion_width", "occlusion_probe_motion_height",
                "occlusion_probe_x", "occlusion_probe_y",
                "occlusion_probe_width", "occlusion_probe_height"):
        if row[key] and not math.isfinite(float(row[key])):
            raise SystemExit(f"invalid {key} at row {index}")
    for key in ("occlusion_reference_width", "occlusion_reference_height",
                "occlusion_width_ratio", "occlusion_height_ratio",
                "occlusion_probe_motion_width", "occlusion_probe_motion_height",
                "occlusion_probe_width", "occlusion_probe_height"):
        if row[key] and float(row[key]) <= 0:
            raise SystemExit(f"nonpositive {key} at row {index}")
    for prefix in ("tracking_", "display_", "control_"):
        box = [row[prefix + suffix] for suffix in ("x", "y", "width", "height")]
        if not all(box):
            raise SystemExit(f"missing {prefix}box at row {index}")
        x, y, box_width, box_height = map(float, box)
        if not all(math.isfinite(value) for value in (x, y, box_width, box_height)) or box_width <= 0 or box_height <= 0:
            raise SystemExit(f"invalid {prefix}box at row {index}")
        if x < 0 or y < 0 or x + box_width > width + 1e-3 or y + box_height > height + 1e-3:
            raise SystemExit(f"out-of-frame {prefix}box at row {index}")
    for key in ("motion_velocity_x", "motion_velocity_y", "motion_acceleration_x",
                "motion_acceleration_y", "motion_scale_velocity_width",
                "motion_scale_velocity_height"):
        if not math.isfinite(float(row[key])):
            raise SystemExit(f"invalid {key} at row {index}")
    try:
        motion_age = int(row["motion_search_age_ms"])
    except ValueError:
        raise SystemExit(f"invalid motion_search_age_ms at row {index}")
    if motion_age < -1:
        raise SystemExit(f"invalid motion_search_age_ms at row {index}")
    hint_present = row["search_hint_present"] == "1"
    hint_keys = ("search_hint_candidate_id", "search_hint_source_frame_id",
                 "search_hint_source_timestamp_ms", "search_hint_projected_x",
                 "search_hint_projected_y", "search_hint_projected_width",
                 "search_hint_projected_height", "search_hint_generation",
                 "search_hint_first_hit_frame", "search_hint_confirmation_count",
                 "search_hint_used", "search_hint_rejection")
    if hint_present != all(bool(row[key]) for key in hint_keys):
        raise SystemExit(f"partial search hint at row {index}")
    if hint_present:
        if row["search_hint_used"] not in {"0", "1"} or row["search_hint_rejection"] not in {"NONE", "EXPIRED", "CONFLICT", "ABORTED"}:
            raise SystemExit(f"invalid search hint status at row {index}")
        confirmation_count = int(row["search_hint_confirmation_count"])
        if confirmation_count not in {1, 2}:
            raise SystemExit(f"invalid search hint confirmation count at row {index}")
        hint_box = [float(row["search_hint_projected_" + suffix]) for suffix in ("x", "y", "width", "height")]
        if not all(math.isfinite(value) for value in hint_box) or hint_box[2] <= 0 or hint_box[3] <= 0:
            raise SystemExit(f"invalid search hint box at row {index}")
    if EXPECTED_TRACKING_SCHEMA == 10 and row["accepted_observation_path"] == "search_reacquisition":
        if not hint_present or int(row["search_hint_confirmation_count"]) != 2 or row["search_hint_used"] != "1" or row["tracking_phase"] != "TRACKING":
            raise SystemExit(f"invalid search reacquisition at row {index}")
    for prefix in ("occlusion_probe_motion_", "occlusion_probe_"):
        box = [row[prefix + suffix] for suffix in ("x", "y", "width", "height")]
        if any(box) and not all(box):
            raise SystemExit(f"partial {prefix} box at row {index}")
    age = int(row["occlusion_coast_age_frames"])
    probe = row["occlusion_probe_type"]
    hits, required_hits = int(row["occlusion_probe_confirmation_hits"]), int(row["occlusion_probe_confirmation_required"])
    auth = row["motion_control_authorized"] == "1"
    hold = row["state"] == "OCCLUSION_HOLD"
    coast_budget = min(12, metadata["occlusion_hold_frames"])
    semantic_motion_search = (
        row["tracking_phase"] == "SEARCHING" and
        row["control_source"] == "MOTION_SEARCH" and
        row["motion_prediction_confident"] == "1" and
        row["control_valid"] == "1"
    )
    if (age > coast_budget or (hold and (age == 0 or not auth)) or
            (auth and not semantic_motion_search)):
        raise SystemExit(f"invalid coast authorization at row {index}")
    if age and not hold and row["reacquired"] != "1":
        raise SystemExit(f"coast age outside hold or recovery at row {index}")
    if hold and int(row["occlusion_hold_remaining_frames"]) != coast_budget - age + 1:
        raise SystemExit(f"invalid coast remaining at row {index}")
    if probe == "NONE":
        if hits or required_hits:
            raise SystemExit(f"probe counts without probe at row {index}")
    elif not age or required_hits != 2 or hits > 2 or (probe == "SEARCH" and hits > 1) or (not hold and row["reacquired"] != "1"):
        raise SystemExit(f"invalid probe state at row {index}")
    if probe == "SEARCH" and age % metadata["occlusion_probe_interval_frames"] != 0:
        raise SystemExit(f"invalid search age at row {index}")
    if probe == "CONFIRM" and (age < 2 or (age - 1) % metadata["occlusion_probe_interval_frames"] != 0):
        raise SystemExit(f"invalid confirm age at row {index}")
    cross = [row["occlusion_probe_cross_" + key] for key in ("iou", "center", "area", "aspect")]
    combined = row["occlusion_probe_cross_gate"]
    if any(cross) and (not all(cross) or combined != ("1" if all(v == "1" for v in cross) else "0")):
        raise SystemExit(f"inconsistent cross-frame gates at row {index}")
    if combined and not all(cross):
        raise SystemExit(f"missing cross-frame gate at row {index}")
    if age and row["reacquired"] == "1":
        required_gates = [
            "occlusion_probe_anchor_reliable", "occlusion_probe_recent_reliable",
            "occlusion_probe_template_gate", "occlusion_probe_cross_gate",
            *(f"occlusion_probe_{template}_{gate}_gate"
              for template in ("anchor", "recent")
              for gate in ("score", "center", "area", "aspect")),
            *(f"occlusion_probe_cross_{gate}" for gate in ("iou", "center", "area", "aspect")),
        ]
        if (row["state"] != "TRACKING" or row["accepted"] != "1" or
                row["control_valid"] != "1" or auth or probe != "CONFIRM" or
                hits != 2 or required_hits != 2 or row["occlusion_probe_rejection"] != "NONE" or
                any(row[key] != "1" for key in required_gates)):
            raise SystemExit(f"invalid coast reacquisition at row {index}")
        for prefix in ("occlusion_probe_motion_", "occlusion_probe_"):
            box = [row[prefix + suffix] for suffix in ("x", "y", "width", "height")]
            if not all(box):
                raise SystemExit(f"missing {prefix} recovery box at row {index}")
            x, y, box_width, box_height = map(float, box)
            if x < 0 or y < 0 or x + box_width > width + 1e-3 or y + box_height > height + 1e-3:
                raise SystemExit(f"invalid {prefix} recovery box at row {index}")
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
    ambiguous = row["detector_same_class_ambiguous"] == "1"
    persistent_conflict = EXPECTED_TRACKING_SCHEMA >= 11 and row["recovery_conflict"] == "1"
    if (same_class_candidates > 1 and not ambiguous) or (
            ambiguous and same_class_candidates <= 1 and not persistent_conflict):
        raise SystemExit(f"inconsistent same-class ambiguity at row {index}")
    if ambiguous and (row["detector_candidate_locked"] == "1" or
                      (EXPECTED_TRACKING_SCHEMA == 10 and control_valid) or
                      row["control_source"] == "OBSERVATION"):
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
        runtime_ms = float(row["runtime_ms"])
        pipeline_ms = float(row["pipeline_ms"])
        if not all(math.isfinite(value) for value in
                   (tracker_ms, runtime_ms, pipeline_ms)):
            raise SystemExit(f"non-finite latency at row {index}")
        if ((row["tracker_skipped"] == "1" and tracker_ms != 0) or
                (row["tracker_skipped"] == "0" and tracker_ms <= 0) or
                runtime_ms <= 0 or pipeline_ms <= 0):
            raise SystemExit(f"invalid latency at row {index}")
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
if EXPECTED_TRACKING_SCHEMA >= 11:
    if metadata.get("observation_valid_semantics") != "ACCEPTED_OBSERVATION" or metadata.get("rect0_bbox") != "tracking_bbox":
        raise SystemExit("missing observation_valid or rect0 schema semantics")
    companion = metadata.get("frame_json_file")
    if companion is not None:
        if not isinstance(companion, str) or not companion.endswith(".jsonl"):
            raise SystemExit("invalid frame JSON companion name")
        metadata_path = (artifact_dir / "session-metadata.json").resolve()
        companion_path = (metadata_path.parent / companion).resolve(strict=True)
        if any(companion_path.samefile(artifact_dir / artifact)
               for artifact in ("bboxes.csv", "session-metadata.json", "output.avi")):
            raise SystemExit("frame JSON companion aliases another artifact")
        with companion_path.open() as stream:
            frame_json = [json.loads(line) for line in stream]
        if len(frame_json) != len(rows):
            raise SystemExit("frame JSON companion count differs from CSV")
        shared_fields = v11_fields | {
            "schema_version", "frame_id", "timestamp_ms", "state", "tracking_phase",
            "observation_valid", "accepted", "reacquired", "control_valid", "control_source",
            "motion_control_authorized", "detector_same_class_ambiguous",
            *(f"{box}_{key}" for box in ("tracking", "display", "control") for key in bbox_components),
        }
        if EXPECTED_TRACKING_SCHEMA >= 12:
            shared_fields.add("motion_update_valid")
        for index, (row, frame) in enumerate(zip(rows, frame_json)):
            for key in shared_fields:
                value = frame.get(key)
                if isinstance(value, bool):
                    matches = row[key] == str(int(value))
                elif isinstance(value, int):
                    matches = row[key] and row[key] == str(value)
                elif isinstance(value, float):
                    matches = row[key] and float(row[key]) == value
                elif value is None:
                    matches = key in frame and row[key] == ""
                else:
                    matches = value == row[key]
                if not matches:
                    raise SystemExit(f"frame JSON companion mismatch for {key} at row {index}")
for key in {
    "schema_version", "manifest_hash", "controller_config_hash", "sensor",
    "calibrated", "detector_target_mode", "rknn_api_version",
    "rknn_driver_version", "occlusion_hold_frames",
    "occlusion_trigger_consecutive_unreliable",
    "occlusion_trigger_min_dimension_ratio", "occlusion_reference_window",
    "max_accepted_scale_step_ratio",
    "accepted_scale_deadband_ratio", "accepted_scale_smoothing_alpha",
    "accepted_scale_median_window_frames",
    "accepted_scale_direction_confirm_frames",
    "accepted_scale_output_hysteresis_px",
    "accepted_center_smoothing_alpha",
    "accepted_center_max_correction_ratio",
    "max_reacquisition_center_distance",
    "occlusion_probe_interval_frames", "occlusion_gradual_dimension_ratio", "occlusion_gradual_consecutive_frames", "occlusion_min_reference_samples", "occlusion_shrink_step_ratio", "occlusion_suspicion_clear_frames", "occlusion_peak_margin_drop", "occlusion_motion_residual_threshold", "occlusion_confirmation_iou", "occlusion_confirmation_center_distance", "occlusion_confirmation_max_area_log_change", "occlusion_confirmation_max_aspect_log_change",
    "mean_runtime_ms", "p50_runtime_ms", "p95_runtime_ms", "runtime_fps",
    "mean_pipeline_ms", "p50_pipeline_ms", "p95_pipeline_ms",
    "end_to_end_fps",
    "motion_search_enabled", "search_reacquisition_requires_detector",
    "search_reacquisition_min_age_ms", "motion_search_confidence_ms",
    "template_bridge_min_stable_frames",
    "template_bridge_max_duration_ms",
    "motion_max_speed_diagonals_per_s",
    "motion_max_acceleration_diagonals_per_s2",
    "motion_scale_rate_limit_per_s", "search_probe_interval_ms",
    "controller_config_identity",
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
    detector_target = declared["conversion"].get("target_platform")
    if detector_target != expected_rknn_target:
        raise SystemExit(
            f"detector manifest target mismatch: expected {expected_rknn_target}, got {detector_target}"
        )
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
    r"initializations=(\d+) calibrated=([01]) metadata=(\S+)",
    log,
)
if not match:
    raise SystemExit("summary metrics missing from run.log")
if int(match.group(1)) != expected - 1:
    raise SystemExit("summary tracked frame count mismatch")
if any(float(value) <= 0 for value in match.groups()[1:10]):
    raise SystemExit("invalid summary metrics")
if int(match.group(11)) < 1:
    raise SystemExit("summary initialization count must be positive")
if pathlib.Path(match.group(13)).name != "session-metadata.json":
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
