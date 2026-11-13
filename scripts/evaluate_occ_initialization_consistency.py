#!/usr/bin/env python3
"""Compare repeated occ_video_1 initializations with both confirmed targets."""

import argparse
import csv
import hashlib
import itertools
import json
import math
import statistics
from pathlib import Path

import evaluate_occ_video_1 as evaluator


BOX_KINDS = ("tracking", "display", "control")


def box(row, kind):
    values = [float(row[f"{kind}_{key}"]) for key in ("x", "y", "width", "height")]
    if not all(math.isfinite(value) for value in values) or values[2] <= 0 or values[3] <= 0:
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


def center(box_value):
    return (box_value[0] + 0.5 * box_value[2],
            box_value[1] + 0.5 * box_value[3])


def center_distance(first, second):
    return math.dist(center(first), center(second))


def center_summary(boxes, reference, frames):
    errors = [(frame, center_distance(boxes[frame], reference[frame]))
              for frame in frames]
    ordered = sorted(error for _, error in errors)
    worst_frame, worst_error = max(errors, key=lambda item: item[1])
    return {
        "frames": len(errors),
        "mean_px": statistics.mean(ordered),
        "p95_px": ordered[max(0, math.ceil(len(ordered) * 0.95) - 1)],
        "max_px": worst_error,
        "max_frame": worst_frame,
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
                metrics[kind][stratum] = {"iou": summary([
                    evaluator._iou(boxes[kind][frame], truth[frame]["bbox"])
                    for frame in frames
                ]), "center_error": center_summary(
                    boxes[kind], {frame: truth[frame]["bbox"] for frame in frames}, frames)}
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
    report = {"schema_version": 2, "runs": {}, "pairs": {}, "targets": {}}
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
                kind: {"iou": summary([evaluator._iou(runs[target, first][kind][frame],
                                                       runs[target, second][kind][frame])
                                       for frame in visible]),
                       "center_spread": center_summary(runs[target, first][kind],
                                                       runs[target, second][kind], visible)}
                for kind in BOX_KINDS
            }
        if len(labels) >= 2:
            report["targets"][target] = {}
            for kind in BOX_KINDS:
                pairs = list(itertools.combinations(labels, 2))
                worst_by_frame = {
                    frame: max(center_distance(runs[target, first][kind][frame],
                                               runs[target, second][kind][frame])
                               for first, second in pairs)
                    for frame in visible
                }
                ordered = sorted(worst_by_frame.values())
                worst_frame = max(worst_by_frame, key=worst_by_frame.get)
                report["targets"][target][kind] = {
                    "visible_frames": len(visible),
                    "max_pairwise_center_spread_mean_px": statistics.mean(ordered),
                    "max_pairwise_center_spread_p95_px": ordered[max(0, math.ceil(len(ordered) * 0.95) - 1)],
                    "max_pairwise_center_spread_max_px": worst_by_frame[worst_frame],
                    "max_pairwise_center_spread_max_frame": worst_frame,
                }
    args.output_json.parent.mkdir(parents=True, exist_ok=True)
    args.output_json.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    for name, result in report["runs"].items():
        metrics = result["metrics"]
        print(name, result["full_evaluator_verdict"], result["failed_gates"],
              "reacq", result["reacquisition_frames"],
              "tracking_iou", round(metrics["tracking"]["all_visible"]["iou"]["mean_iou"], 3),
              "tracking_center", round(metrics["tracking"]["all_visible"]["center_error"]["mean_px"], 2),
              "display_iou", round(metrics["display"]["all_visible"]["iou"]["mean_iou"], 3),
              "display_center", round(metrics["display"]["all_visible"]["center_error"]["mean_px"], 2),
              "no_bbox_false_obs", len(result["confirmed_no_bbox"]["false_observation_frames"]))
    for name, result in report["pairs"].items():
        print(name, "tracking_pair_center", round(result["tracking"]["center_spread"]["mean_px"], 2),
              "display_pair_center", round(result["display"]["center_spread"]["mean_px"], 2))
    for target, metrics in report["targets"].items():
        print(target, "tracking_spread_p95", round(metrics["tracking"]["max_pairwise_center_spread_p95_px"], 2),
              "display_spread_p95", round(metrics["display"]["max_pairwise_center_spread_p95_px"], 2))


if __name__ == "__main__":
    main()
