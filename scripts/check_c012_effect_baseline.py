#!/usr/bin/env python3
"""Reject RK3588 candidates whose recorded behavior is worse than C012."""

import argparse
import csv
import json
import math
import sys
from pathlib import Path


SCHEMA_VERSION = 1
REQUIRED_COLUMNS = {
    "frame_id", "tracking_phase", "control_valid", "accepted", "reacquired",
    "x", "y", "width", "height",
}


def _load_contract(path):
    try:
        contract = json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise ValueError("invalid C012 contract {}: {}".format(path, error)) from error
    if not isinstance(contract, dict) or contract.get("schema_version") != SCHEMA_VERSION:
        raise ValueError("C012 contract schema_version must be 1")
    if contract.get("candidate_id") != "C012":
        raise ValueError("C012 contract candidate_id mismatch")
    targets = contract.get("targets")
    if not isinstance(targets, dict) or set(targets) != {"car_001", "car_002"}:
        raise ValueError("C012 contract must define car_001 and car_002")
    return contract


def _load_rows(path, target):
    try:
        with Path(path).open(newline="", encoding="utf-8") as stream:
            reader = csv.DictReader(stream)
            if not reader.fieldnames or not REQUIRED_COLUMNS.issubset(reader.fieldnames):
                missing = sorted(REQUIRED_COLUMNS - set(reader.fieldnames or []))
                raise ValueError("{} CSV missing columns: {}".format(target, ", ".join(missing)))
            rows = list(reader)
    except OSError as error:
        raise ValueError("invalid {} CSV {}: {}".format(target, path, error)) from error
    if not rows:
        raise ValueError("{} CSV is empty".format(target))
    try:
        frame_ids = [int(row["frame_id"]) for row in rows]
    except (TypeError, ValueError) as error:
        raise ValueError("{} CSV has invalid frame_id".format(target)) from error
    if frame_ids != list(range(frame_ids[-1] + 1)):
        raise ValueError("{} CSV frame IDs must be contiguous from zero".format(target))
    return rows


def _bbox(row, target):
    try:
        values = [float(row[key]) for key in ("x", "y", "width", "height")]
    except (TypeError, ValueError) as error:
        raise ValueError("{} initial bbox is invalid".format(target)) from error
    if any(not math.isfinite(value) for value in values):
        raise ValueError("{} initial bbox is invalid".format(target))
    return values


def _check_initialization(rows, target, requirements):
    expected = requirements.get("initial_bbox")
    if (not isinstance(expected, list) or len(expected) != 4 or
            any(isinstance(value, bool) or not isinstance(value, (int, float))
                for value in expected)):
        raise ValueError("{} contract initial_bbox is invalid".format(target))
    actual = _bbox(rows[0], target)
    if any(abs(actual[index] - float(expected[index])) > 1e-4 for index in range(4)):
        raise ValueError(
            "{} initial bbox {} does not match C012 {}".format(target, actual, expected)
        )


def _truthy(row, key):
    return row[key] in {"1", "true", "True"}


def _check_car001(rows, requirements):
    target = "car_001"
    _check_initialization(rows, target, requirements)
    required_last = requirements.get("required_last_frame")
    if not isinstance(required_last, int) or len(rows) <= required_last:
        raise ValueError("car_001 must cover frame {}".format(required_last))
    control_end = requirements.get("control_valid_through_frame")
    invalid_control = [
        int(row["frame_id"]) for row in rows[:control_end + 1]
        if not _truthy(row, "control_valid")
    ]
    if invalid_control:
        raise ValueError("car_001 loses control at frames {}".format(invalid_control[:10]))
    reacquired = [int(row["frame_id"]) for row in rows if _truthy(row, "reacquired")]
    deadline = requirements.get("reacquisition_deadline_inclusive")
    if not reacquired or reacquired[0] > deadline:
        raise ValueError(
            "car_001 first reacquisition {} is worse than C012 frame {}".format(
                reacquired[0] if reacquired else None, deadline
            )
        )
    forbidden_start = requirements.get("forbid_search_from_frame")
    later_search = [
        int(row["frame_id"]) for row in rows
        if int(row["frame_id"]) >= forbidden_start and row["tracking_phase"] == "SEARCHING"
    ]
    if later_search:
        raise ValueError("car_001 searches after frame {}: {}".format(
            forbidden_start, later_search[:10]
        ))
    return {
        "initial_bbox": _bbox(rows[0], target),
        "last_frame": int(rows[-1]["frame_id"]),
        "first_reacquisition_frame": reacquired[0],
        "later_search_frames": later_search,
        "control_invalid_frames": invalid_control,
    }


def _check_car002(rows, requirements):
    target = "car_002"
    _check_initialization(rows, target, requirements)
    start, end = requirements.get("road_sign_continuity_frames", [None, None])
    if not isinstance(start, int) or not isinstance(end, int) or start > end or len(rows) <= end:
        raise ValueError("car_002 must cover the configured road-sign interval")
    failures = []
    for row in rows[start:end + 1]:
        if (row["tracking_phase"] != "TRACKING" or
                not _truthy(row, "accepted") or
                not _truthy(row, "control_valid")):
            failures.append(int(row["frame_id"]))
    if failures:
        raise ValueError("car_002 road-sign continuity regressed at frames {}".format(
            failures[:10]
        ))
    return {
        "initial_bbox": _bbox(rows[0], target),
        "last_frame": int(rows[-1]["frame_id"]),
        "road_sign_continuity_frames": [start, end],
        "road_sign_failures": failures,
    }


def evaluate(contract, car001_rows, car002_rows):
    targets = contract["targets"]
    return {
        "schema_version": SCHEMA_VERSION,
        "candidate_id": contract["candidate_id"],
        "verdict": "PASS",
        "car_001": _check_car001(car001_rows, targets["car_001"]),
        "car_002": _check_car002(car002_rows, targets["car_002"]),
    }


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--contract", required=True, type=Path)
    parser.add_argument("--car001-boxes", required=True, type=Path)
    parser.add_argument("--car002-boxes", required=True, type=Path)
    parser.add_argument("--output-json", type=Path)
    args = parser.parse_args(argv)
    try:
        contract = _load_contract(args.contract)
        result = evaluate(
            contract,
            _load_rows(args.car001_boxes, "car_001"),
            _load_rows(args.car002_boxes, "car_002"),
        )
    except ValueError as error:
        print("FAIL: {}".format(error), file=sys.stderr)
        return 1
    rendered = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.output_json:
        args.output_json.write_text(rendered, encoding="utf-8")
    print(rendered, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
