#!/usr/bin/env python3
"""Evaluate NanoTrack output against the frozen occ_video_1 truth contract."""

import argparse
import csv
import hashlib
import json
import math
import sys
from pathlib import Path


REPORT_SCHEMA_VERSION = 1
REQUIRED_TRACKING_SCHEMA = 12
MIN_OBSERVATION_VISIBLE_MEAN_IOU = 0.70
MIN_OBSERVATION_VISIBLE_FRAME_IOU = 0.45
CANONICAL_IDENTITY = {
    "name": "occ_video_1_car_001",
    "frame_count": 271,
    "width": 1920,
    "height": 1080,
    "class_id": 0,
    "hashes": {
        "video": "eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d",
        "annotation_metadata": "5641650435a1980694f982cf1fa6715ba959b1515938cee383c8db8ec4aeb076",
        "occlusion_events": "a43f016c96b34c8a0ec6125dcb944ba4fa07a79d99a46ce696bbb29841ded256",
        "annotation_status": "e44fd23514d6871e72d628ad4843fc90bb4246f306322ee297c8a2aa75818036",
        "labels_manifest": "9ec01854884126350f93e1af3a4aee002d38ce7c2d50c71c78de0f1a8dc0a648",
    },
}
CANONICAL_IDENTITIES = {
    "car_001": CANONICAL_IDENTITY,
    "car_002": {
        "name": "occ_video_1_car_002",
        "target_id": "car_002",
        "annotation_root": "ground_truth/car_002",
        "frame_count": 271,
        "width": 1920,
        "height": 1080,
        "class_id": 0,
        "files": {
            "video": "1.mp4",
            "annotation_metadata": "annotation_metadata.json",
            "occlusion_events": "events.json",
            "annotation_status": "annotation_status.csv",
            "labels_manifest": "labels.sha256",
            "estimated_labels_manifest": "estimated_labels.sha256",
            "package_checksums": "checksums.sha256",
        },
        "hashes": {
            "video": "eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d",
            "annotation_metadata": "ddbd8846cd12466d82a833f2f335160a6e6dd1f6189963d268db1b1c54d03a63",
            "occlusion_events": "efc0652335fcf19c766def88bf50bb8defab1441fdc8fbb412b85d02135c31db",
            "annotation_status": "212525642591c124320e93369f8748d82cc53092628beff051507f930946dec1",
            "labels_manifest": "2f0e78ab1123cb035c87efa06171f1057c108f38b06fcc5c1ed3a3ec63fdbe98",
            "estimated_labels_manifest": "62a2877bfc38bf4482e3e1cca1e81551272a677f81d932918723e557db3d24e4",
            "package_checksums": "4540ff280903a3e32d8b439f54ddca4497e855095da68ebdbe8fbea044371811",
        },
        "confirmed_package": True,
    },
}


def _sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _require_file(path, description):
    path = Path(path)
    if not path.is_file():
        raise ValueError("missing {}: {}".format(description, path))
    return path


def _require_hash(path, expected, description):
    actual = _sha256(_require_file(path, description))
    if actual != expected:
        raise ValueError(
            "{} SHA-256 mismatch: expected {}, observed {}".format(
                description, expected, actual
            )
        )
    return actual


def _load_json(path, description):
    try:
        value = json.loads(_require_file(path, description).read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise ValueError("invalid {}: {}".format(description, error)) from error
    if not isinstance(value, dict):
        raise ValueError("{} must be a JSON object".format(description))
    return value


def _event_frames(events, frame_count):
    event_list = events.get("events", [events])
    if not isinstance(event_list, list):
        raise ValueError("occlusion events must be a list")
    candidates = [
        item for item in event_list
        if isinstance(item, dict) and item.get("type") in {"full_occlusion", "occlusion"}
    ]
    if not candidates and len(event_list) == 1 and isinstance(event_list[0], dict):
        candidates = event_list
    if len(candidates) != 1:
        raise ValueError("occlusion events must contain exactly one annotation event")
    event = candidates[0]

    def pair(primary, start_key, end_key):
        if primary in event:
            value = event[primary]
            if not isinstance(value, list) or len(value) != 2:
                raise ValueError("{} must be a two-frame range".format(primary))
            return int(value[0]), int(value[1])
        return int(event[start_key]), int(event[end_key])

    if "estimated_bbox_frames" in event or "estimated_bbox_start_frame" in event:
        estimated_start, estimated_end = pair(
            "estimated_bbox_frames", "estimated_bbox_start_frame", "estimated_bbox_end_frame"
        )
    else:
        estimated_start, estimated_end = pair(
            "full_occlusion_frames", "first_full_frame", "last_full_frame"
        )
    full_start, full_end = pair(
        "full_occlusion_frames", "first_full_frame", "last_full_frame"
    )
    first_reliable = int(
        event.get("first_reliable_reappearance_frame",
                  event.get("first_full_object_candidate_frame"))
    )
    result = {
        "estimated_start": estimated_start,
        "estimated_end": estimated_end,
        "full_start": full_start,
        "full_end": full_end,
        "first_reliable_reappearance": first_reliable,
        "stable_reappearance": int(event.get("stable_reappearance_frame", first_reliable)),
        "reacquisition_deadline_exclusive": int(
            event.get(
                "reacquisition_deadline_exclusive",
                frame_count if "first_full_object_candidate_frame" in event else 190,
            )
        ),
    }
    if not (
        0 <= estimated_start <= full_start <= full_end <= estimated_end
        < result["first_reliable_reappearance"]
        <= result["stable_reappearance"]
    ):
        raise ValueError("occlusion event frame order is invalid")
    return result


def _manifest_paths(root, manifest, frame_count, layer):
    entries = []
    for line_number, line in enumerate(manifest.read_text(encoding="utf-8").splitlines(), 1):
        pieces = line.split()
        if len(pieces) != 2 or len(pieces[0]) != 64:
            raise ValueError("invalid {} manifest line {}".format(layer, line_number))
        relative = Path(pieces[1])
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError("unsafe {} manifest path at line {}".format(layer, line_number))
        label_path = root / relative
        _require_hash(label_path, pieces[0], "{} {}".format(layer, relative))
        entries.append(label_path)
    expected_names = ["frame_{:06d}.txt".format(index) for index in range(frame_count)]
    if [path.name for path in entries] != expected_names:
        raise ValueError("{} manifest frame IDs are not continuous".format(layer))
    return entries


def _verify_package_checksums(root, manifest):
    expected_paths = []
    for line_number, line in enumerate(manifest.read_text(encoding="utf-8").splitlines(), 1):
        pieces = line.split("  ", 1)
        if len(pieces) != 2 or len(pieces[0]) != 64:
            raise ValueError("invalid package checksum line {}".format(line_number))
        relative = Path(pieces[1])
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError("unsafe package checksum path at line {}".format(line_number))
        _require_hash(root / relative, pieces[0], "package file {}".format(relative))
        expected_paths.append(relative.as_posix())
    actual_paths = sorted(
        path.relative_to(root).as_posix() for path in root.rglob("*")
        if path.is_file() and path != manifest
    )
    if expected_paths != actual_paths:
        raise ValueError("package checksum coverage mismatch")


def load_contract(root, target_id="car_001"):
    """Load and strictly verify the frozen dataset identity and annotations."""
    root = Path(root)
    if not root.is_dir():
        raise ValueError("dataset root is not a directory: {}".format(root))
    if target_id not in CANONICAL_IDENTITIES:
        raise ValueError("unknown target_id: {}".format(target_id))
    expected_identity = (
        CANONICAL_IDENTITY if target_id == "car_001" else CANONICAL_IDENTITIES[target_id]
    )
    annotation_root = root / expected_identity.get("annotation_root", ".")
    files = expected_identity.get("files", {})
    paths = {
        "video": root / files.get("video", "1.mp4"),
        "annotation_metadata": annotation_root / files.get(
            "annotation_metadata", "annotation_metadata.json"
        ),
        "occlusion_events": annotation_root / files.get(
            "occlusion_events", "occlusion_events.json"
        ),
        "annotation_status": annotation_root / files.get(
            "annotation_status", "annotation_status.csv"
        ),
        "labels_manifest": annotation_root / files.get("labels_manifest", "labels.sha256"),
    }
    for optional in ("estimated_labels_manifest", "package_checksums"):
        if optional in files:
            paths[optional] = annotation_root / files[optional]
    observed = {
        key: _require_hash(
            paths[key], expected_identity["hashes"][key], key.replace("_", " ")
        )
        for key in paths
    }
    metadata = _load_json(paths["annotation_metadata"], "annotation metadata")
    events = _load_json(paths["occlusion_events"], "occlusion events")
    video = metadata.get("video", {})
    width = int(video.get("width", expected_identity["width"]))
    height = int(video.get("height", expected_identity["height"]))
    frame_count = int(video.get("frame_count", expected_identity["frame_count"]))
    if (width, height, frame_count) != (
        int(expected_identity["width"]),
        int(expected_identity["height"]),
        int(expected_identity["frame_count"]),
    ):
        raise ValueError("metadata dimensions/frame count do not match canonical identity")
    if expected_identity.get("confirmed_package") and (
            metadata.get("draft") is not False or
            metadata.get("formal_evaluation_authorized") is not True or
            metadata.get("unresolved_review_count") != 0):
        raise ValueError("target package is not confirmed for formal evaluation")

    status_rows = []
    with paths["annotation_status"].open(newline="", encoding="utf-8") as handle:
        for raw in csv.DictReader(handle):
            try:
                frame_id = int(raw["frame_id"])
            except (KeyError, TypeError, ValueError) as error:
                raise ValueError("annotation status has invalid frame_id") from error
            status_rows.append(
                {
                    "frame_id": frame_id,
                    "status": raw.get(
                        "target_state", raw.get("state", raw.get("status", ""))
                    ).strip(),
                    "source": raw.get("source", raw.get("review_status", "")).strip(),
                }
            )
    expected_frames = list(range(frame_count))
    if [row["frame_id"] for row in status_rows] != expected_frames:
        raise ValueError("annotation status frame IDs are not continuous")

    manifest_entries = _manifest_paths(
        annotation_root, paths["labels_manifest"], frame_count, "labels"
    )
    estimated_entries = None
    if "estimated_labels_manifest" in paths:
        estimated_entries = _manifest_paths(
            annotation_root, paths["estimated_labels_manifest"], frame_count,
            "estimated labels",
        )
    if "package_checksums" in paths:
        _verify_package_checksums(annotation_root, paths["package_checksums"])

    identity = dict(expected_identity)
    identity["hashes"] = observed
    identity["target_id"] = target_id
    return {
        "root": root,
        "annotation_root": annotation_root,
        "identity": identity,
        "metadata": metadata,
        "events": events,
        "event_frames": _event_frames(events, frame_count),
        "status": status_rows,
        "label_paths": manifest_entries,
        "estimated_label_paths": estimated_entries,
        "required_tracking_schema": REQUIRED_TRACKING_SCHEMA,
    }


def load_truth(contract):
    """Read verified YOLO labels as pixel XYWH boxes and attach visibility semantics."""
    if "truth" in contract:
        return list(contract["truth"])
    identity = contract["identity"]
    width = float(identity["width"])
    height = float(identity["height"])
    class_id = int(identity["class_id"])
    def read_box(path, allow_empty):
        pieces = path.read_text(encoding="utf-8").strip().split()
        if not pieces and allow_empty:
            return None
        if len(pieces) != 5:
            raise ValueError("label {} must contain exactly five fields".format(path))
        try:
            observed_class = int(pieces[0])
            cx, cy, box_width, box_height = map(float, pieces[1:])
        except ValueError as error:
            raise ValueError("label {} has invalid numeric fields".format(path)) from error
        if observed_class != class_id:
            raise ValueError("label {} has unexpected class ID".format(path))
        if not all(math.isfinite(value) for value in (cx, cy, box_width, box_height)):
            raise ValueError("label {} has non-finite coordinates".format(path))
        if not (0.0 <= cx <= 1.0 and 0.0 <= cy <= 1.0 and 0.0 < box_width <= 1.0 and 0.0 < box_height <= 1.0):
            raise ValueError("label {} lies outside normalized YOLO bounds".format(path))
        pixel_width = box_width * width
        pixel_height = box_height * height
        bbox = [
            cx * width - 0.5 * pixel_width,
            cy * height - 0.5 * pixel_height,
            pixel_width,
            pixel_height,
        ]
        return bbox

    truth = []
    estimated_paths = contract.get("estimated_label_paths")
    if estimated_paths is None:
        estimated_paths = [None] * len(contract["label_paths"])
    for status, path, estimated_path in zip(
            contract["status"], contract["label_paths"], estimated_paths):
        source = status["source"]
        formal_box = read_box(
            path,
            allow_empty=source in {"manual_confirmed_no_bbox", "estimated_non_observable"},
        )
        estimated_box = read_box(estimated_path, allow_empty=True) if estimated_path else None
        if formal_box is not None and estimated_box is not None:
            raise ValueError("formal and estimated labels overlap at frame {}".format(
                status["frame_id"]
            ))
        if source == "manual_confirmed_visible" and formal_box is None:
            raise ValueError("confirmed visible frame lacks a formal label")
        if source == "manual_confirmed_no_bbox" and (
                formal_box is not None or estimated_box is not None):
            raise ValueError("confirmed no-bbox frame contains geometry")
        if source == "estimated_non_observable" and estimated_box is None:
            raise ValueError("estimated frame lacks an estimated label")
        bbox = formal_box if formal_box is not None else estimated_box
        state = status["status"]
        truth.append(
            {
                "frame_id": status["frame_id"],
                "bbox": bbox,
                "status": state,
                "observable": (
                    formal_box is not None and
                    source not in {"bidirectional_interpolation", "estimated_non_observable"}
                ),
                "estimated": estimated_box is not None,
            }
        )
    return truth


def _parse_bool(value, field, frame_id):
    if isinstance(value, bool):
        return value
    text = str(value).strip().lower()
    if text in ("1", "true"):
        return True
    if text in ("0", "false"):
        return False
    raise ValueError("frame {} has invalid {}".format(frame_id, field))


def _parse_float(value, field, frame_id, allow_empty=False):
    text = str(value).strip()
    if not text and allow_empty:
        return None
    try:
        number = float(text)
    except ValueError as error:
        raise ValueError("frame {} has invalid {}".format(frame_id, field)) from error
    if not math.isfinite(number):
        raise ValueError("frame {} {} must be finite".format(frame_id, field))
    return number


def load_tracking_csv(path):
    """Parse candidate tracking rows and archived rows used for diagnostics."""
    path = _require_file(path, "tracking CSV")
    rows = []
    with path.open(newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        required = {
            "schema_version", "frame_id", "timestamp_ms", "state", "control_valid",
            "accepted", "reacquired",
        }
        if not reader.fieldnames or not required.issubset(reader.fieldnames):
            missing = sorted(required.difference(reader.fieldnames or []))
            raise ValueError("tracking CSV missing columns: {}".format(", ".join(missing)))
        tracking_box_fields = ("tracking_x", "tracking_y", "tracking_width", "tracking_height")
        legacy_box_fields = ("x", "y", "width", "height")
        if all(name in reader.fieldnames for name in tracking_box_fields):
            box_fields = tracking_box_fields
            box_source = "tracking"
        elif all(name in reader.fieldnames for name in legacy_box_fields):
            box_fields = legacy_box_fields
            box_source = "legacy_display"
        else:
            raise ValueError(
                "tracking CSV missing a complete tracking box column group"
            )
        for raw in reader:
            try:
                frame_id = int(raw["frame_id"])
                schema_version = int(raw["schema_version"])
                timestamp_ms = int(raw["timestamp_ms"])
            except (TypeError, ValueError) as error:
                raise ValueError("tracking CSV has invalid integer identity") from error
            values = [raw.get(name, "") for name in box_fields]
            present = [bool(str(value).strip()) for value in values]
            if any(present) and not all(present):
                raise ValueError("frame {} has a partial tracking box".format(frame_id))
            if not all(present):
                raise ValueError("frame {} has no tracking box".format(frame_id))
            bbox = [
                _parse_float(value, "tracking box", frame_id) for value in values
            ]
            if bbox[2] <= 0.0 or bbox[3] <= 0.0:
                raise ValueError("frame {} tracking box must have positive size".format(frame_id))
            semantic_flags = (
                "observation_valid", "observation_present", "trusted_for_state_update",
                "motion_update_valid",
                "motion_control_authorized", "motion_prediction_confident",
                "template_transition_recovery", "template_transition_applied",
                "template_probation_active", "recovery_conflict",
            )
            flags = {}
            for field in semantic_flags:
                value = (raw.get(field) or "").strip()
                flags[field] = _parse_bool(value, field, frame_id) if value else None
            recovery_numbers = {}
            for field in ("template_transaction_id", "template_slot_id", "recovery_confirmation_hits"):
                value = (raw.get(field) or "").strip()
                recovery_numbers[field] = int(value) if value.isascii() and value.isdecimal() else None
            rows.append(
                {
                    "schema_version": schema_version,
                    "frame_id": frame_id,
                    "timestamp_ms": timestamp_ms,
                    "state": raw["state"].strip(),
                    "tracking_phase": raw.get("tracking_phase", "").strip() or None,
                    **flags,
                    **{
                        field: (raw.get(field) or "").strip() or None
                        for field in (
                            "rect0_source", "accepted_observation_path", "template_action",
                            "template_transition_result", "template_transition_reason",
                            "recovery_reason", "candidate_source",
                        )
                    },
                    **recovery_numbers,
                    "control_source": raw.get("control_source", "").strip() or None,
                    "control_valid": _parse_bool(raw["control_valid"], "control_valid", frame_id),
                    "accepted": _parse_bool(raw["accepted"], "accepted", frame_id),
                    "reacquired": _parse_bool(raw["reacquired"], "reacquired", frame_id),
                    "tracking_bbox": bbox,
                    "tracking_box_source": box_source,
                }
            )
    if not rows:
        raise ValueError("tracking CSV has no rows")
    first = rows[0]["frame_id"]
    if [row["frame_id"] for row in rows] != list(range(first, first + len(rows))):
        raise ValueError("tracking CSV frame IDs are not strictly continuous")
    if any(
        current["timestamp_ms"] <= previous["timestamp_ms"]
        for previous, current in zip(rows, rows[1:])
    ):
        raise ValueError("tracking CSV timestamps are not strictly increasing")
    return rows


def _bbox_center(box):
    return box[0] + 0.5 * box[2], box[1] + 0.5 * box[3]


def _center_distance(first, second):
    first_center = _bbox_center(first)
    second_center = _bbox_center(second)
    return math.hypot(first_center[0] - second_center[0], first_center[1] - second_center[1])


def _iou(first, second):
    first_right, first_bottom = first[0] + first[2], first[1] + first[3]
    second_right, second_bottom = second[0] + second[2], second[1] + second[3]
    overlap = max(0.0, min(first_right, second_right) - max(first[0], second[0])) * max(
        0.0, min(first_bottom, second_bottom) - max(first[1], second[1])
    )
    union = first[2] * first[3] + second[2] * second[3] - overlap
    return overlap / union if union > 0.0 else 0.0


def _percentile(values, quantile):
    if not values:
        return 0.0
    ordered = sorted(values)
    position = (len(ordered) - 1) * quantile
    lower = int(math.floor(position))
    upper = int(math.ceil(position))
    if lower == upper:
        return ordered[lower]
    fraction = position - lower
    return ordered[lower] * (1.0 - fraction) + ordered[upper] * fraction


def _kinematics(items, box_key):
    velocities = []
    velocity_records = []
    for previous, current in zip(items, items[1:]):
        elapsed = (current["timestamp_ms"] - previous["timestamp_ms"]) / 1000.0
        if elapsed <= 0.0:
            continue
        p = _bbox_center(previous[box_key])
        c = _bbox_center(current[box_key])
        vx, vy = (c[0] - p[0]) / elapsed, (c[1] - p[1]) / elapsed
        speed = math.hypot(vx, vy)
        velocities.append(speed)
        velocity_records.append((current["timestamp_ms"], vx, vy))
    accelerations = []
    for previous, current in zip(velocity_records, velocity_records[1:]):
        elapsed = (current[0] - previous[0]) / 1000.0
        if elapsed > 0.0:
            accelerations.append(
                math.hypot(current[1] - previous[1], current[2] - previous[2]) / elapsed
            )
    return velocities, accelerations


def _gate(passed, detail, frames=None, applicable=True):
    return {
        "passed": bool(passed),
        "applicable": bool(applicable),
        "detail": str(detail),
        "frames": sorted(set(frames or [])),
    }


def _continuity_segments(rows, reacquisition_frames):
    """Split output rows at semantic control handoffs.

    Search motion and accepted observations are each required to be smooth
    internally.  The atomic SEARCHING -> TRACKING reacquisition handoff is not
    a motion-estimator sample and therefore starts a new segment.
    """
    segments = []
    current = []
    previous_phase = None
    for row in rows:
        phase = row.get("tracking_phase")
        if current and (phase != previous_phase or row["frame_id"] in reacquisition_frames):
            segments.append(current)
            current = []
        current.append(
            {"timestamp_ms": row["timestamp_ms"], "bbox": row["tracking_bbox"]}
        )
        previous_phase = phase
    if current:
        segments.append(current)
    return segments


def _is_current_reacquisition(row, frame_width, frame_height):
    # A recovery stage is acknowledged on the reacquisition frame. The
    # template commit is a separate acknowledgement after probation.
    transaction_id = row.get("template_transaction_id")
    bbox = row["tracking_bbox"]
    return (
        row["schema_version"] == REQUIRED_TRACKING_SCHEMA
        and row["state"] == row.get("tracking_phase") == "TRACKING"
        and all(row.get(field) is True for field in (
            "observation_present", "accepted", "observation_valid",
            "trusted_for_state_update", "motion_update_valid", "control_valid",
            "template_transition_recovery", "template_transition_applied",
            "template_probation_active",
        ))
        and row.get("control_source") == "OBSERVATION"
        and row.get("motion_control_authorized") is False
        and row.get("rect0_source") == "ACCEPTED_OBSERVATION"
        and row.get("tracking_box_source", "tracking") == "tracking"
        and len(bbox) == 4 and all(math.isfinite(value) for value in bbox)
        and bbox[0] >= 0.0 and bbox[1] >= 0.0 and bbox[2] > 0.0 and bbox[3] > 0.0
        and bbox[0] + bbox[2] <= frame_width + 1e-3
        and bbox[1] + bbox[3] <= frame_height + 1e-3
        and row.get("accepted_observation_path") == "search_reacquisition"
        and row.get("template_action") == "STAGE_RECENT"
        and row.get("template_transition_result") == "APPLIED"
        and row.get("template_transition_reason") == "RECOVERY_CONFIRMED"
        and isinstance(transaction_id, int) and transaction_id > 0
        and row.get("template_slot_id") == transaction_id
        and row.get("recovery_reason") == "RECOVERY_STAGED"
        and row.get("recovery_confirmation_hits") == 2
        and row.get("candidate_source") in {"RECENT", "SAFE", "DETECTOR"}
        and row.get("recovery_conflict") is False
    )


def evaluate(contract, rows, scope):
    """Evaluate already-loaded rows without exposing truth to the runtime."""
    if scope not in ("development", "holdout", "full"):
        raise ValueError("unknown evaluation scope: {}".format(scope))
    truth = load_truth(contract)
    if not truth:
        raise ValueError("truth is empty")
    truth_by_frame = {item["frame_id"]: item for item in truth}
    if len(truth_by_frame) != len(truth):
        raise ValueError("truth contains duplicate frame IDs")
    final_frame = max(truth_by_frame)
    scope_start, scope_end = {
        "development": (0, min(189, final_frame)),
        "holdout": (190, final_frame),
        "full": (0, final_frame),
    }[scope]
    if scope_start > scope_end:
        raise ValueError("{} scope is empty for this contract".format(scope))
    rows_by_frame = {row["frame_id"]: row for row in rows}
    if len(rows_by_frame) != len(rows):
        raise ValueError("tracking rows contain duplicate frame IDs")
    required_frames = list(range(scope_start, scope_end + 1))
    missing = [frame for frame in required_frames if frame not in rows_by_frame]
    if missing:
        raise ValueError("tracking rows do not cover scope; first missing frame {}".format(missing[0]))
    scoped_rows = [rows_by_frame[frame] for frame in required_frames]
    events = contract["event_frames"]
    estimated_frames = set(range(events["estimated_start"], events["estimated_end"] + 1))
    full_frames = set(range(events["full_start"], events["full_end"] + 1))

    frame_reports = []
    visible_ious = []
    visible_center_errors = []
    observation_visible_ious = []
    observation_visible_center_errors = []
    estimated_center_errors = []
    pre_occlusion_ious = []
    post_reappearance_ious = []
    for row in scoped_rows:
        frame_id = row["frame_id"]
        target = truth_by_frame[frame_id]
        prediction = row["tracking_bbox"]
        if target["bbox"] is None:
            center_error = None
        else:
            diagonal = max(math.hypot(target["bbox"][2], target["bbox"][3]), 1e-12)
            center_error_px = _center_distance(prediction, target["bbox"])
            center_error = center_error_px / diagonal
        ordinary = target["observable"] and frame_id not in estimated_frames
        overlap = _iou(prediction, target["bbox"]) if ordinary else None
        if ordinary:
            visible_ious.append(overlap)
            visible_center_errors.append(center_error)
            if row.get("observation_valid") is True:
                observation_visible_ious.append(overlap)
                observation_visible_center_errors.append(center_error)
            if frame_id < events["estimated_start"]:
                pre_occlusion_ious.append(overlap)
            if frame_id >= events["first_reliable_reappearance"]:
                post_reappearance_ious.append(overlap)
        if frame_id in estimated_frames and center_error is not None:
            estimated_center_errors.append(center_error)
        frame_reports.append(
            {
                "frame_id": frame_id,
                "truth_status": target["status"],
                "ordinary_iou_allowed": ordinary,
                "iou": overlap,
                "normalized_center_error": center_error,
                "tracking_phase": row.get("tracking_phase"),
                "observation_valid": row.get("observation_valid"),
                "control_source": row.get("control_source"),
                "control_valid": bool(row["control_valid"]),
                "accepted": bool(row["accepted"]),
                "reacquired": bool(row["reacquired"]),
            }
        )

    required_schema = int(contract.get("required_tracking_schema", REQUIRED_TRACKING_SCHEMA))
    schema_versions = sorted({int(row["schema_version"]) for row in rows})
    semantic_fields = ["tracking_phase", "observation_valid", "control_source"]
    recent_safe = required_schema == REQUIRED_TRACKING_SCHEMA
    if recent_safe:
        semantic_fields.extend((
            "observation_present", "trusted_for_state_update", "motion_update_valid",
        ))
    semantic_columns_present = all(
        row.get(field) is not None for row in rows for field in semantic_fields
    )
    explicit_tracking_boxes = all(
        row.get("tracking_box_source", "tracking") == "tracking" for row in rows
    )
    metadata = contract.get("tracking_session_metadata")
    schema_ok = (
        schema_versions == [required_schema]
        and semantic_columns_present
        and explicit_tracking_boxes
        and (metadata is None or metadata.get("schema_version") == required_schema)
    )
    if schema_versions == [9]:
        schema_detail = "legacy schema 9 is diagnostic-only; schema {} semantics are required".format(
            required_schema
        )
    else:
        schema_detail = "observed schema {}; required {} with semantic columns".format(
            schema_versions, required_schema
        )

    visible_failures = [
        report["frame_id"] for report in frame_reports
        if report["ordinary_iou_allowed"]
        and report["observation_valid"] is True
        and (
            report["iou"] < MIN_OBSERVATION_VISIBLE_FRAME_IOU
            or report["normalized_center_error"] > 0.50
        )
    ]
    mean_iou = sum(visible_ious) / len(visible_ious) if visible_ious else 0.0
    observation_mean_iou = (
        sum(observation_visible_ious) / len(observation_visible_ious)
        if observation_visible_ious else 0.0
    )
    visible_ok = (
        bool(observation_visible_ious)
        and not visible_failures
        and observation_mean_iou >= MIN_OBSERVATION_VISIBLE_MEAN_IOU
    )

    semantic_failures = []
    for frame_id in sorted(full_frames.intersection(required_frames)):
        row = rows_by_frame[frame_id]
        if not (
            row.get("tracking_phase") == "SEARCHING"
            and row.get("observation_valid") is False
            and row["control_valid"] is True
            and row.get("control_source") == "MOTION_SEARCH"
            and (not recent_safe or (
                row["state"] != "LOST"
                and all(row.get(field) is False for field in (
                    "observation_present", "accepted", "trusted_for_state_update",
                    "motion_update_valid",
                ))
                and row.get("rect0_source") == "MOTION_PREDICTION"
                and row.get("motion_control_authorized") is True
                and row.get("motion_prediction_confident") is True
            ))
        ):
            semantic_failures.append(frame_id)

    control_failures = [
        row["frame_id"] for row in scoped_rows
        if row["frame_id"] != 0 and not row["control_valid"]
    ]
    source_failures = [
        row["frame_id"] for row in scoped_rows
        if semantic_columns_present and (
            (row.get("observation_valid") is True and not row["accepted"])
            or (row.get("control_source") == "NONE" and row["control_valid"])
            or (row.get("control_source") == "MOTION_SEARCH" and row.get("observation_valid") is True)
        )
    ]
    if recent_safe and semantic_columns_present:
        for row in scoped_rows:
            phase = row["tracking_phase"]
            present = row["observation_present"]
            trusted = row["trusted_for_state_update"]
            motion_update_valid = row["motion_update_valid"]
            if (
                trusted != row["accepted"]
                or row["observation_valid"] != trusted
                or (motion_update_valid and (
                    not trusted or not present or phase != "TRACKING"
                ))
                or row["control_valid"] != (row["control_source"] != "NONE")
                or phase not in {"TRACKING", "SUSPECTED", "SEARCHING"}
                or (phase == "TRACKING" and (
                    not present or not trusted or row["control_source"] != "OBSERVATION"
                ))
                or (phase == "SUSPECTED" and (
                    not present or trusted or row["control_source"] not in {"OBSERVATION", "NONE"}
                ))
                or (phase == "SEARCHING" and (
                    present or trusted or row["control_source"] not in {"MOTION_SEARCH", "NONE"}
                ))
            ):
                source_failures.append(row["frame_id"])
    if recent_safe:
        source_failures.extend(
            row["frame_id"] for row in scoped_rows
            if row["state"] == "LOST" and (
                row.get("tracking_phase") != "SEARCHING"
                or row.get("control_source") != "NONE"
                or row.get("motion_control_authorized") is True
                or any(row.get(field) is not False for field in (
                    "control_valid", "observation_present", "trusted_for_state_update",
                    "accepted", "observation_valid", "motion_update_valid",
                ))
            )
        )

    claimed_reacquisitions = [row for row in rows if row["reacquired"]]
    frame_width = contract["identity"].get("width", math.inf)
    frame_height = contract["identity"].get("height", math.inf)
    invalid_reacquisitions = sorted(
        row["frame_id"] for row in claimed_reacquisitions
        if recent_safe and not _is_current_reacquisition(row, frame_width, frame_height)
    )
    valid_reacquisitions = {row["frame_id"] for row in claimed_reacquisitions}.difference(
        invalid_reacquisitions
    )
    reacquisition_frames = sorted(valid_reacquisitions)
    first_reliable = int(events["first_reliable_reappearance"])
    deadline = int(events.get("reacquisition_deadline_exclusive", 190))
    reacquisition_ok = (
        not invalid_reacquisitions
        and len(reacquisition_frames) == 1
        and first_reliable <= reacquisition_frames[0] < deadline
    )
    reacquisition_detail = "exactly one reacquisition in [{}, {}) observed {}".format(
        first_reliable, deadline, reacquisition_frames
    )
    if recent_safe:
        reacquisition_detail += ". Invalid schema claims {}".format(invalid_reacquisitions)

    truth_motion_items = []
    for frame_id in required_frames:
        row = rows_by_frame[frame_id]
        target = truth_by_frame[frame_id]
        if (target["observable"] and target["bbox"] is not None and
                frame_id not in estimated_frames):
            truth_motion_items.append(
                {"timestamp_ms": row["timestamp_ms"], "bbox": target["bbox"]}
            )
    predicted_velocity = []
    predicted_acceleration = []
    for segment in _continuity_segments(scoped_rows, valid_reacquisitions):
        velocities, accelerations = _kinematics(segment, "bbox")
        predicted_velocity.extend(velocities)
        predicted_acceleration.extend(accelerations)
    truth_velocity, truth_acceleration = _kinematics(truth_motion_items, "bbox")
    velocity_limit = max(2.0 * _percentile(truth_velocity, 0.99), 1e-6)
    acceleration_limit = max(2.0 * _percentile(truth_acceleration, 0.99), 1e-6)
    trajectory_center_ok = all(value <= 1.0 for value in estimated_center_errors)
    trajectory_motion_ok = (
        (not predicted_velocity or max(predicted_velocity) <= velocity_limit)
        and (not predicted_acceleration or max(predicted_acceleration) <= acceleration_limit)
    )

    reference_steps = []
    for previous, current in zip(truth, truth[1:]):
        if (previous["observable"] and current["observable"] and
                previous["bbox"] is not None and current["bbox"] is not None):
            reference_steps.append(_center_distance(previous["bbox"], current["bbox"]))
    jump_limit = max(5.0, 3.0 * _percentile(reference_steps, 0.99))
    identity_failures = []
    for previous, current in zip(scoped_rows, scoped_rows[1:]):
        current_truth = truth_by_frame[current["frame_id"]]
        if (
            current_truth["observable"]
            and current_truth["bbox"] is not None
            and current["frame_id"] not in estimated_frames
            and (
                current.get("observation_valid") is True
                or (recent_safe and current.get("observation_present") is True
                    and current.get("control_source") == "OBSERVATION" and current["control_valid"])
            )
        ):
            if (
                (
                    current["frame_id"] not in valid_reacquisitions
                    and _center_distance(
                        previous["tracking_bbox"], current["tracking_bbox"]
                    ) > jump_limit
                )
                or _iou(current["tracking_bbox"], current_truth["bbox"]) < 0.10
            ):
                identity_failures.append(current["frame_id"])

    gates = {
        "tracking_schema": _gate(schema_ok, schema_detail),
        "visible_boxes": _gate(
            visible_ok,
            "valid-observation mean IoU {:.6f} (floor {:.2f}); per-frame IoU >={:.2f} and center error <=0.50; all-visible mean IoU {:.6f}".format(
                observation_mean_iou, MIN_OBSERVATION_VISIBLE_MEAN_IOU,
                MIN_OBSERVATION_VISIBLE_FRAME_IOU, mean_iou
            ),
            visible_failures,
        ),
        "full_occlusion_semantics": _gate(
            not semantic_failures,
            "full occlusion must publish SEARCHING/no observation/MOTION_SEARCH control",
            semantic_failures,
            applicable=bool(full_frames.intersection(required_frames)),
        ),
        "prediction_continuity": _gate(
            trajectory_center_ok and trajectory_motion_ok,
            "estimated center <=1 diagonal; max speed {:.6f}/{:.6f}, max acceleration {:.6f}/{:.6f}".format(
                max(predicted_velocity or [0.0]), velocity_limit,
                max(predicted_acceleration or [0.0]), acceleration_limit,
            ),
            [
                report["frame_id"] for report in frame_reports
                if report["frame_id"] in estimated_frames
                and report["normalized_center_error"] is not None
                and report["normalized_center_error"] > 1.0
            ],
        ),
        "reacquisition": _gate(
            reacquisition_ok,
            reacquisition_detail,
            (reacquisition_frames + invalid_reacquisitions) if not reacquisition_ok else [],
        ),
        "continuous_control": _gate(
            not control_failures and not source_failures,
            "post-initialization control and source invariants",
            sorted(set(control_failures + source_failures)),
        ),
        "identity_proxy": _gate(
            not identity_failures,
            "no corridor jump or near-zero visible truth overlap; jump limit {:.6f}px".format(jump_limit),
            identity_failures,
        ),
    }
    failed_frames = set()
    for gate in gates.values():
        if gate["applicable"] and not gate["passed"]:
            failed_frames.update(gate["frames"])
    visible_ranked = sorted(
        (
            (report["iou"], report["frame_id"])
            for report in frame_reports if report["ordinary_iou_allowed"]
        ),
        key=lambda value: (value[0], value[1]),
    )
    worst_frames = sorted(failed_frames.union(frame for _, frame in visible_ranked[:10]))
    verdict = "PASS" if all(
        not gate["applicable"] or gate["passed"] for gate in gates.values()
    ) else "FAIL"
    return {
        "schema_version": REPORT_SCHEMA_VERSION,
        "dataset": contract["identity"],
        "scope": scope,
        "frame_range": [scope_start, scope_end],
        "verdict": verdict,
        "gates": gates,
        "strata": {
            "visible": {
                "frames": len(visible_ious),
                "mean_iou": mean_iou,
                "min_iou": min(visible_ious) if visible_ious else None,
                "max_normalized_center_error": max(visible_center_errors) if visible_center_errors else None,
            },
            "observation_visible": {
                "frames": len(observation_visible_ious),
                "mean_iou": observation_mean_iou,
                "min_iou": min(observation_visible_ious) if observation_visible_ious else None,
                "max_normalized_center_error": (
                    max(observation_visible_center_errors)
                    if observation_visible_center_errors else None
                ),
            },
            "pre_occlusion_visible": {
                "frames": len(pre_occlusion_ious),
                "mean_iou": (
                    sum(pre_occlusion_ious) / len(pre_occlusion_ious)
                    if pre_occlusion_ious else None
                ),
            },
            "post_reappearance_visible": {
                "frames": len(post_reappearance_ious),
                "mean_iou": (
                    sum(post_reappearance_ious) / len(post_reappearance_ious)
                    if post_reappearance_ious else None
                ),
            },
            "estimated_trajectory": {
                "frames": len(estimated_center_errors),
                "max_normalized_center_error": max(estimated_center_errors) if estimated_center_errors else None,
                "max_speed_px_per_s": max(predicted_velocity or [0.0]),
                "max_acceleration_px_per_s2": max(predicted_acceleration or [0.0]),
            },
            "full_occlusion": {
                "frames": len(full_frames.intersection(required_frames)),
                "semantic_failures": semantic_failures,
            },
        },
        "reacquisition": {
            "frames": reacquisition_frames,
            **({"invalid_claim_frames": invalid_reacquisitions} if recent_safe else {}),
            "first_frame": reacquisition_frames[0] if reacquisition_frames else None,
            "first_reliable_reappearance_frame": first_reliable,
            "deadline_exclusive": deadline,
        },
        "identity_proxy": {
            "jump_limit_px": jump_limit,
            "violation_frames": identity_failures,
            "human_review_required": True,
        },
        "worst_frames": worst_frames,
        "frames": frame_reports,
    }


def render_markdown(report):
    lines = [
        "# occ_video_1 evaluation",
        "",
        "- Scope: `{}`".format(report["scope"]),
        "- Verdict: **{}**".format(report["verdict"]),
        "- Frame range: `{}--{}`".format(*report["frame_range"]),
        "- Visible mean IoU: `{:.6f}`".format(report["strata"]["visible"]["mean_iou"]),
        "- Reacquisition frames: `{}`".format(report["reacquisition"]["frames"]),
        "",
        "## Gates",
        "",
    ]
    for name, gate in report["gates"].items():
        status = "N/A" if not gate.get("applicable", True) else (
            "PASS" if gate["passed"] else "FAIL"
        )
        lines.append(
            "- `{}`: **{}** — {}".format(name, status, gate["detail"])
        )
    lines.extend(["", "## Worst frames", ""])
    if report["worst_frames"]:
        lines.extend("- frame {}".format(frame) for frame in report["worst_frames"])
    else:
        lines.append("- none")
    lines.extend(
        [
            "",
            "> Identity is checked automatically only by truth overlap and motion continuity. "
            "Human review of the distractor window remains required.",
            "",
        ]
    )
    return "\n".join(lines)


def attach_provenance(report, source_commit=None, config_sha256=None,
                      model_sha256=None):
    values = (source_commit, config_sha256, model_sha256)
    if all(value is None for value in values):
        return dict(report)
    if any(value is None for value in values):
        raise ValueError("provenance identities must be provided together")
    normalized = {}
    for name, value, length in (
        ("source_commit", source_commit, 40),
        ("config_sha256", config_sha256, 64),
        ("model_sha256", model_sha256, 64),
    ):
        if not isinstance(value, str) or len(value) != length:
            raise ValueError("{} must be a {}-character hexadecimal identity".format(name, length))
        try:
            int(value, 16)
        except ValueError as error:
            raise ValueError("{} must be hexadecimal".format(name)) from error
        normalized[name] = value.lower()
    enriched = dict(report)
    enriched["provenance"] = normalized
    return enriched


def _write_report(path, content):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dataset-root", type=Path, required=True)
    parser.add_argument(
        "--target-id", choices=tuple(sorted(CANONICAL_IDENTITIES)), default="car_001"
    )
    parser.add_argument("--boxes", type=Path, required=True)
    parser.add_argument("--session-metadata", type=Path)
    parser.add_argument("--scope", choices=("development", "holdout", "full"), required=True)
    parser.add_argument("--output-json", type=Path, required=True)
    parser.add_argument("--output-markdown", type=Path, required=True)
    parser.add_argument("--source-commit")
    parser.add_argument("--config-sha256")
    parser.add_argument("--model-sha256")
    args = parser.parse_args(argv)
    try:
        contract = load_contract(args.dataset_root, args.target_id)
        rows = load_tracking_csv(args.boxes)
        if args.session_metadata:
            metadata = _load_json(args.session_metadata, "session metadata")
            contract["tracking_session_metadata"] = metadata
            contract["tracking_session_metadata_sha256"] = _sha256(args.session_metadata)
        report = attach_provenance(
            evaluate(contract, rows, args.scope),
            args.source_commit,
            args.config_sha256,
            args.model_sha256,
        )
        _write_report(
            args.output_json,
            json.dumps(report, indent=2, sort_keys=True, allow_nan=False) + "\n",
        )
        _write_report(args.output_markdown, render_markdown(report))
    except (OSError, ValueError, KeyError) as error:
        print("ERROR: {}".format(error), file=sys.stderr)
        return 2
    print("{} {}".format(report["verdict"], args.output_json))
    return 0 if report["verdict"] == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
