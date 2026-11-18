#!/usr/bin/env python3
"""Check human-confirmed frame deadlines without rewriting canonical truth."""

import argparse
import csv
import hashlib
import json
import math
from pathlib import Path


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def flag(row, key):
    value = row.get(key)
    if value not in {"0", "1"}:
        raise ValueError(f"frame {row.get('frame_id')} has invalid {key}: {value!r}")
    return value == "1"


def box(row):
    values = [float(row[f"tracking_{name}"]) for name in ("x", "y", "width", "height")]
    if not all(math.isfinite(value) for value in values) or min(values[2:]) <= 0:
        raise ValueError(f"frame {row.get('frame_id')} has invalid tracking box")
    return values


def iou(first, second):
    left = max(first[0], second[0])
    top = max(first[1], second[1])
    right = min(first[0] + first[2], second[0] + second[2])
    bottom = min(first[1] + first[3], second[1] + second[3])
    intersection = max(0.0, right - left) * max(0.0, bottom - top)
    union = first[2] * first[3] + second[2] * second[3] - intersection
    return intersection / union if union > 0 else 0.0


def evaluate(check, rows, truth):
    kind = check["type"]
    if kind == "visible_observation":
        frame = check["frame"]
        row = rows[frame]
        passed = (row["tracking_phase"] == "TRACKING" and flag(row, "accepted")
                  and flag(row, "observation_valid") and flag(row, "observation_present"))
        return {"id": check["id"], "passed": passed, "frame": frame,
                "phase": row["tracking_phase"], "accepted": flag(row, "accepted")}
    if kind == "occlusion_status":
        frame = check["frame"]
        row = rows[frame]
        passed = row["tracking_phase"] in {"SUSPECTED", "SEARCHING"} and not flag(row, "accepted")
        return {"id": check["id"], "passed": passed, "frame": frame,
                "phase": row["tracking_phase"], "accepted": flag(row, "accepted")}
    if kind == "reacquisition":
        first, last = check["first_frame"], check["last_frame"]
        minimum_iou = check["minimum_iou"]
        if first > last or not 0 <= minimum_iou <= 1:
            raise ValueError(f"invalid reacquisition constraint {check['id']}")
        events = []
        for frame in range(first, last + 1):
            row = rows[frame]
            if not flag(row, "reacquired"):
                continue
            truth_box = truth[frame].get("bbox_xywh")
            overlap = iou(box(row), truth_box) if truth_box is not None else None
            events.append({"frame": frame, "iou": overlap,
                           "accepted": flag(row, "accepted"),
                           "observation_valid": flag(row, "observation_valid"),
                           "recovery_reason": row.get("recovery_reason")})
        passed = any(event["accepted"] and event["observation_valid"]
                     and event["recovery_reason"] == "RECOVERY_STAGED"
                     and event["iou"] is not None and event["iou"] >= minimum_iou
                     for event in events)
        return {"id": check["id"], "passed": passed, "deadline_inclusive": last,
                "events_in_window": events,
                "all_reacquisition_frames": [frame for frame, row in rows.items()
                                             if flag(row, "reacquired")]}
    raise ValueError(f"unknown constraint type {kind!r}")


def load_csv(path):
    with path.open(newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))
    frames = {int(row["frame_id"]): row for row in rows}
    if len(frames) != len(rows) or sorted(frames) != list(range(len(rows))):
        raise ValueError(f"non-contiguous frame IDs in {path}")
    return frames


def load_truth(path):
    entries = [json.loads(line) for line in path.read_text().splitlines() if line.strip()]
    frames = {entry["frame_id"]: entry for entry in entries}
    if len(frames) != len(entries) or sorted(frames) != list(range(len(entries))):
        raise ValueError(f"non-contiguous truth frame IDs in {path}")
    return frames


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--constraints", type=Path, required=True)
    parser.add_argument("--dataset-root", type=Path, required=True)
    parser.add_argument("--runs-root", type=Path, required=True)
    parser.add_argument("--run-template", default="output-{mode}-full/{target}/frames.csv")
    parser.add_argument("--output-json", type=Path, required=True)
    args = parser.parse_args()
    specification = json.loads(args.constraints.read_text())
    if specification.get("schema_version") != 1:
        raise ValueError("unsupported constraint schema")
    report = {"schema_version": 1, "constraints_sha256": sha256(args.constraints),
              "runs": [], "all_passed": True}
    for target, checks in specification["targets"].items():
        truth_path = args.dataset_root / "ground_truth" / target / "frames.jsonl"
        truth = load_truth(truth_path)
        for mode in ("gt", "det"):
            path = args.runs_root / args.run_template.format(mode=mode, target=target)
            rows = load_csv(path)
            if len(rows) != len(truth):
                raise ValueError(f"run/truth frame count differs for {target} {mode}")
            results = [evaluate(check, rows, truth) for check in checks]
            passed = all(result["passed"] for result in results)
            report["all_passed"] &= passed
            report["runs"].append({"target": target, "mode": mode, "passed": passed,
                                   "frames_csv": str(path), "frames_sha256": sha256(path),
                                   "truth_sha256": sha256(truth_path), "checks": results})
    args.output_json.parent.mkdir(parents=True, exist_ok=True)
    args.output_json.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"all_passed": report["all_passed"], "runs": [
        {"target": run["target"], "mode": run["mode"], "passed": run["passed"],
         "checks": {check["id"]: check["passed"] for check in run["checks"]}}
        for run in report["runs"]]}, indent=2))


if __name__ == "__main__":
    main()
