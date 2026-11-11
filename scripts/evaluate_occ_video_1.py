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
REQUIRED_TRACKING_SCHEMA = 10
CANONICAL_IDENTITY = {
    "name": "occ_video_1",
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


def _event_frames(events):
    event = events.get("events", [events])
    if not isinstance(event, list) or len(event) != 1 or not isinstance(event[0], dict):
        raise ValueError("occlusion events must contain exactly one event")
    event = event[0]

    def pair(primary, start_key, end_key):
        if primary in event:
            value = event[primary]
            if not isinstance(value, list) or len(value) != 2:
                raise ValueError("{} must be a two-frame range".format(primary))
            return int(value[0]), int(value[1])
        return int(event[start_key]), int(event[end_key])

    estimated_start, estimated_end = pair(
        "estimated_bbox_frames", "estimated_bbox_start_frame", "estimated_bbox_end_frame"
    )
    full_start, full_end = pair(
        "full_occlusion_frames", "first_full_frame", "last_full_frame"
    )
    result = {
        "estimated_start": estimated_start,
        "estimated_end": estimated_end,
        "full_start": full_start,
        "full_end": full_end,
        "first_reliable_reappearance": int(event["first_reliable_reappearance_frame"]),
        "stable_reappearance": int(event["stable_reappearance_frame"]),
        "reacquisition_deadline_exclusive": 190,
    }
    if not (
        0 <= estimated_start <= full_start <= full_end <= estimated_end
        < result["first_reliable_reappearance"]
        <= result["stable_reappearance"]
    ):
        raise ValueError("occlusion event frame order is invalid")
    return result


def load_contract(root):
    """Load and strictly verify the frozen dataset identity and annotations."""
    root = Path(root)
    if not root.is_dir():
        raise ValueError("dataset root is not a directory: {}".format(root))
    paths = {
        "video": root / "1.mp4",
        "annotation_metadata": root / "annotation_metadata.json",
        "occlusion_events": root / "occlusion_events.json",
        "annotation_status": root / "annotation_status.csv",
        "labels_manifest": root / "labels.sha256",
    }
    observed = {
        key: _require_hash(paths[key], CANONICAL_IDENTITY["hashes"][key], key.replace("_", " "))
        for key in paths
    }
    metadata = _load_json(paths["annotation_metadata"], "annotation metadata")
    events = _load_json(paths["occlusion_events"], "occlusion events")
    video = metadata.get("video", {})
    width = int(video.get("width", CANONICAL_IDENTITY["width"]))
    height = int(video.get("height", CANONICAL_IDENTITY["height"]))
    frame_count = int(video.get("frame_count", CANONICAL_IDENTITY["frame_count"]))
    if (width, height, frame_count) != (
        int(CANONICAL_IDENTITY["width"]),
        int(CANONICAL_IDENTITY["height"]),
        int(CANONICAL_IDENTITY["frame_count"]),
    ):
        raise ValueError("metadata dimensions/frame count do not match canonical identity")

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
                    "status": raw.get("state", raw.get("status", "")).strip(),
                    "source": raw.get("source", "").strip(),
                }
            )
    expected_frames = list(range(frame_count))
    if [row["frame_id"] for row in status_rows] != expected_frames:
        raise ValueError("annotation status frame IDs are not continuous")

    manifest_entries = []
    for line_number, line in enumerate(
        paths["labels_manifest"].read_text(encoding="utf-8").splitlines(), 1
    ):
        pieces = line.split()
        if len(pieces) != 2 or len(pieces[0]) != 64:
            raise ValueError("invalid labels manifest line {}".format(line_number))
        relative = Path(pieces[1])
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError("unsafe labels manifest path at line {}".format(line_number))
        label_path = root / relative
        _require_hash(label_path, pieces[0], "label {}".format(relative))
        manifest_entries.append(label_path)
    expected_names = ["frame_{:06d}.txt".format(index) for index in expected_frames]
    if [path.name for path in manifest_entries] != expected_names:
        raise ValueError("label manifest frame IDs are not continuous")

    identity = dict(CANONICAL_IDENTITY)
    identity["hashes"] = observed
    return {
        "root": root,
        "identity": identity,
        "metadata": metadata,
        "events": events,
        "event_frames": _event_frames(events),
        "status": status_rows,
        "label_paths": manifest_entries,
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
    truth = []
    for status, path in zip(contract["status"], contract["label_paths"]):
        pieces = path.read_text(encoding="utf-8").strip().split()
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
        state = status["status"]
        truth.append(
            {
                "frame_id": status["frame_id"],
                "bbox": bbox,
                "status": state,
                "observable": status["source"] != "bidirectional_interpolation",
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
    """Parse schema-9/10 tracking rows into a stable evaluator representation."""
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
            rows.append(
                {
                    "schema_version": schema_version,
                    "frame_id": frame_id,
                    "timestamp_ms": timestamp_ms,
                    "state": raw["state"].strip(),
                    "tracking_phase": raw.get("tracking_phase", "").strip() or None,
                    "observation_valid": (
                        _parse_bool(raw["observation_valid"], "observation_valid", frame_id)
                        if str(raw.get("observation_valid", "")).strip() else None
                    ),
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


def _gate(passed, detail, frames=None):
    return {
        "passed": bool(passed),
        "detail": str(detail),
        "frames": sorted(set(frames or [])),
    }


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
    estimated_center_errors = []
    pre_occlusion_ious = []
    post_reappearance_ious = []
    for row in scoped_rows:
        frame_id = row["frame_id"]
        target = truth_by_frame[frame_id]
        prediction = row["tracking_bbox"]
        diagonal = max(math.hypot(target["bbox"][2], target["bbox"][3]), 1e-12)
        center_error_px = _center_distance(prediction, target["bbox"])
        center_error = center_error_px / diagonal
        ordinary = target["observable"] and frame_id not in estimated_frames
        overlap = _iou(prediction, target["bbox"]) if ordinary else None
        if ordinary:
            visible_ious.append(overlap)
            visible_center_errors.append(center_error)
            if frame_id < events["estimated_start"]:
                pre_occlusion_ious.append(overlap)
            if frame_id >= events["first_reliable_reappearance"]:
                post_reappearance_ious.append(overlap)
        if frame_id in estimated_frames:
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
    schema_versions = sorted({int(row["schema_version"]) for row in scoped_rows})
    semantic_columns_present = all(
        row.get("tracking_phase") is not None
        and row.get("observation_valid") is not None
        and row.get("control_source") is not None
        for row in scoped_rows
    )
    explicit_tracking_boxes = all(
        row.get("tracking_box_source", "tracking") == "tracking" for row in scoped_rows
    )
    schema_ok = (
        schema_versions == [required_schema]
        and semantic_columns_present
        and explicit_tracking_boxes
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
        and (report["iou"] < 0.50 or report["normalized_center_error"] > 0.50)
    ]
    mean_iou = sum(visible_ious) / len(visible_ious) if visible_ious else 0.0
    visible_ok = bool(visible_ious) and not visible_failures and mean_iou >= 0.75

    semantic_failures = []
    for frame_id in sorted(full_frames.intersection(required_frames)):
        row = rows_by_frame[frame_id]
        if not (
            row.get("tracking_phase") == "SEARCHING"
            and row.get("observation_valid") is False
            and row["control_valid"] is True
            and row.get("control_source") == "MOTION_SEARCH"
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

    all_motion_items = []
    truth_motion_items = []
    for frame_id in required_frames:
        row = rows_by_frame[frame_id]
        all_motion_items.append(
            {"timestamp_ms": row["timestamp_ms"], "bbox": row["tracking_bbox"]}
        )
        target = truth_by_frame[frame_id]
        if target["observable"] and frame_id not in estimated_frames:
            truth_motion_items.append(
                {"timestamp_ms": row["timestamp_ms"], "bbox": target["bbox"]}
            )
    predicted_velocity, predicted_acceleration = _kinematics(all_motion_items, "bbox")
    truth_velocity, truth_acceleration = _kinematics(truth_motion_items, "bbox")
    velocity_limit = max(2.0 * _percentile(truth_velocity, 0.99), 1e-6)
    acceleration_limit = max(2.0 * _percentile(truth_acceleration, 0.99), 1e-6)
    trajectory_center_ok = all(value <= 1.0 for value in estimated_center_errors)
    trajectory_motion_ok = (
        (not predicted_velocity or max(predicted_velocity) <= velocity_limit)
        and (not predicted_acceleration or max(predicted_acceleration) <= acceleration_limit)
    )

    reacquisition_frames = sorted(row["frame_id"] for row in rows if row["reacquired"])
    first_reliable = int(events["first_reliable_reappearance"])
    deadline = int(events.get("reacquisition_deadline_exclusive", 190))
    reacquisition_ok = (
        len(reacquisition_frames) == 1
        and first_reliable <= reacquisition_frames[0] < deadline
    )

    reference_steps = []
    for previous, current in zip(truth, truth[1:]):
        if previous["observable"] and current["observable"]:
            reference_steps.append(_center_distance(previous["bbox"], current["bbox"]))
    jump_limit = max(5.0, 3.0 * _percentile(reference_steps, 0.99))
    identity_failures = []
    for previous, current in zip(scoped_rows, scoped_rows[1:]):
        current_truth = truth_by_frame[current["frame_id"]]
        if current_truth["observable"] and current["frame_id"] not in estimated_frames:
            if (
                _center_distance(previous["tracking_bbox"], current["tracking_bbox"]) > jump_limit
                or _iou(current["tracking_bbox"], current_truth["bbox"]) < 0.10
            ):
                identity_failures.append(current["frame_id"])

    gates = {
        "tracking_schema": _gate(schema_ok, schema_detail),
        "visible_boxes": _gate(
            visible_ok,
            "mean IoU {:.6f}; per-frame IoU >=0.50 and center error <=0.50".format(mean_iou),
            visible_failures,
        ),
        "full_occlusion_semantics": _gate(
            not semantic_failures and bool(full_frames.intersection(required_frames)),
            "full occlusion must publish SEARCHING/no observation/MOTION_SEARCH control",
            semantic_failures,
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
                and report["normalized_center_error"] > 1.0
            ],
        ),
        "reacquisition": _gate(
            reacquisition_ok,
            "exactly one reacquisition in [{}, {}) observed {}".format(
                first_reliable, deadline, reacquisition_frames
            ),
            reacquisition_frames if not reacquisition_ok else [],
        ),
        "continuous_control": _gate(
            not control_failures and not source_failures,
            "post-initialization control and source invariants",
            control_failures + source_failures,
        ),
        "identity_proxy": _gate(
            not identity_failures,
            "no corridor jump or near-zero visible truth overlap; jump limit {:.6f}px".format(jump_limit),
            identity_failures,
        ),
    }
    failed_frames = set()
    for gate in gates.values():
        failed_frames.update(gate["frames"])
    visible_ranked = sorted(
        (
            (report["iou"], report["frame_id"])
            for report in frame_reports if report["ordinary_iou_allowed"]
        ),
        key=lambda value: (value[0], value[1]),
    )
    worst_frames = sorted(failed_frames.union(frame for _, frame in visible_ranked[:10]))
    verdict = "PASS" if all(gate["passed"] for gate in gates.values()) else "FAIL"
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
        lines.append(
            "- `{}`: **{}** — {}".format(name, "PASS" if gate["passed"] else "FAIL", gate["detail"])
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


def _write_report(path, content):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dataset-root", type=Path, required=True)
    parser.add_argument("--boxes", type=Path, required=True)
    parser.add_argument("--session-metadata", type=Path)
    parser.add_argument("--scope", choices=("development", "holdout", "full"), required=True)
    parser.add_argument("--output-json", type=Path, required=True)
    parser.add_argument("--output-markdown", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        contract = load_contract(args.dataset_root)
        rows = load_tracking_csv(args.boxes)
        if args.session_metadata:
            metadata = _load_json(args.session_metadata, "session metadata")
            contract["tracking_session_metadata"] = metadata
            contract["tracking_session_metadata_sha256"] = _sha256(args.session_metadata)
        report = evaluate(contract, rows, args.scope)
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
