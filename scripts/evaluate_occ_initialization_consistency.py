#!/usr/bin/env python3
"""Compare repeated occ_video_1 initializations with both confirmed targets."""

import argparse
import csv
import hashlib
import itertools
import json
import statistics
from pathlib import Path

import evaluate_occ_video_1 as evaluator


BOX_KINDS = ("tracking", "display", "control")


def box(row, kind):
    values = [float(row[f"{kind}_{key}"]) for key in ("x", "y", "width", "height")]
    if values[2] <= 0 or values[3] <= 0:
        raise ValueError(f"invalid {kind} box on frame {row['frame_id']}")
    return values


def summary(values):
    return {
        "frames": len(values),
        "mean_iou": statistics.mean(values),
        "min_iou": min(values),
        "p10_iou": sorted(values)[max(0, int(len(values) * 0.1) - 1)],
        "below_0_50": sum(value < 0.5 for value in values),
    }


def read_run(path, roi, truth, contract):
    with path.open(newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))
    expected_frames = contract["identity"]["frame_count"]
    if len(rows) != expected_frames or [int(row["frame_id"]) for row in rows] != list(range(expected_frames)):
        raise ValueError(f"{path}: expected continuous frames 0..{expected_frames - 1}")
    first = box(rows[0], "tracking")
    if any(abs(value - expected) > 0.01 for value, expected in zip(first, roi)):
        raise ValueError(f"{path}: initial tracker box {first} does not match ROI {roi}")
    parsed = evaluator.load_tracking_csv(path)
    full = evaluator.evaluate(contract, parsed, "full")
    boxes = {kind: [box(row, kind) for row in rows] for kind in BOX_KINDS}
    visible = [frame for frame, item in truth.items() if item["observable"]]
    no_bbox = [frame for frame, item in truth.items() if item["bbox"] is None]
    strata = {"all_visible": visible,
              "fast_motion_48_58": [frame for frame in visible if 48 <= frame <= 58],
              "late_visible_230_270": [frame for frame in visible if 230 <= frame <= 270]}
    metrics = {}
    for kind in BOX_KINDS:
        metrics[kind] = {}
        for stratum, frames in strata.items():
            if frames:
                metrics[kind][stratum] = summary([
                    evaluator._iou(boxes[kind][frame], truth[frame]["bbox"])
                    for frame in frames
                ])
    return rows, boxes, {
        "csv": str(path.resolve()),
        "csv_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "roi": roi,
        "full_evaluator_verdict": full["verdict"],
        "failed_gates": [name for name, gate in full["gates"].items()
                         if gate["applicable"] and not gate["passed"]],
        "reacquisition_frames": full["reacquisition"]["frames"],
        "confirmed_no_bbox": {
            "frames": len(no_bbox),
            "false_observation_frames": [frame for frame in no_bbox
                                         if parsed[frame].get("observation_valid") is True],
            "false_observation_control_frames": [frame for frame in no_bbox
                                                 if parsed[frame].get("control_source") == "OBSERVATION"
                                                 and parsed[frame]["control_valid"]],
        },
        "metrics": metrics,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dataset-root", type=Path, required=True)
    parser.add_argument("--run", nargs=4, action="append", required=True,
                        metavar=("TARGET", "LABEL", "X,Y,W,H", "CSV"))
    parser.add_argument("--output-json", type=Path, required=True)
    args = parser.parse_args()
    contracts = {}
    truths = {}
    runs = {}
    report = {"schema_version": 1, "runs": {}, "pairs": {}}
    for target, label, roi_text, path_text in args.run:
        if target not in evaluator.CANONICAL_IDENTITIES or (target, label) in runs:
            parser.error(f"invalid or repeated target/label: {target}/{label}")
        if target not in contracts:
            contracts[target] = evaluator.load_contract(args.dataset_root, target)
            truths[target] = {item["frame_id"]: item for item in evaluator.load_truth(contracts[target])}
        try:
            roi = [float(value) for value in roi_text.split(",")]
            if len(roi) != 4 or roi[2] <= 0 or roi[3] <= 0:
                raise ValueError()
        except ValueError:
            parser.error(f"invalid ROI: {roi_text}")
        rows, boxes, result = read_run(Path(path_text), roi, truths[target], contracts[target])
        runs[target, label] = boxes
        report["runs"][f"{target}/{label}"] = result
    for target in sorted(contracts):
        labels = sorted(label for run_target, label in runs if run_target == target)
        visible = [frame for frame, item in truths[target].items() if item["observable"]]
        for first, second in itertools.combinations(labels, 2):
            key = f"{target}/{first}__{second}"
            report["pairs"][key] = {
                kind: summary([evaluator._iou(runs[target, first][kind][frame],
                                               runs[target, second][kind][frame])
                               for frame in visible])
                for kind in BOX_KINDS
            }
    args.output_json.parent.mkdir(parents=True, exist_ok=True)
    args.output_json.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    for name, result in report["runs"].items():
        metrics = result["metrics"]
        print(name, result["full_evaluator_verdict"], result["failed_gates"],
              "reacq", result["reacquisition_frames"],
              "tracking", round(metrics["tracking"]["all_visible"]["mean_iou"], 3),
              "display", round(metrics["display"]["all_visible"]["mean_iou"], 3),
              "no_bbox_false_obs", len(result["confirmed_no_bbox"]["false_observation_frames"]))
    for name, result in report["pairs"].items():
        print(name, "tracking_pair", round(result["tracking"]["mean_iou"], 3),
              "display_pair", round(result["display"]["mean_iou"], 3))


if __name__ == "__main__":
    main()
