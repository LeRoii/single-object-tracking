#!/usr/bin/env python3
"""Compare paired static-video display traces; this does not judge target identity."""

import argparse
import csv
import json
import math
from pathlib import Path


BOX_COLUMNS = (
    "tracking_x", "tracking_y", "tracking_width", "tracking_height",
    "display_x", "display_y", "display_width", "display_height",
)


def load(path):
    with path.open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    if not rows or not set(BOX_COLUMNS).issubset(rows[0]):
        raise ValueError(f"missing frame rows or box columns: {path}")
    result = {}
    for row in rows:
        frame = int(row["frame_id"])
        if frame in result:
            raise ValueError(f"duplicate frame_id={frame}: {path}")
        for key in BOX_COLUMNS:
            row[key] = float(row[key])
        result[frame] = row
    return result


def percentile(values, ratio):
    if not values:
        return None
    ordered = sorted(values)
    position = (len(ordered) - 1) * ratio
    low = math.floor(position)
    high = math.ceil(position)
    return ordered[low] + (ordered[high] - ordered[low]) * (position - low)


def center(row, prefix):
    return (row[f"{prefix}_x"] + 0.5 * row[f"{prefix}_width"],
            row[f"{prefix}_y"] + 0.5 * row[f"{prefix}_height"])


def metrics(rows, frames, warmup):
    center_acceleration = []
    size_acceleration = []
    center_step = []
    size_step = []
    accepted_frames = 0
    for frame in frames:
        current = rows[frame]
        if current["tracking_phase"].upper() == "TRACKING" and current["accepted"] == "1":
            accepted_frames += 1
        if frame < frames[0] + warmup or frame - 1 not in rows or frame - 2 not in rows:
            continue
        pair = [rows[frame - offset] for offset in (2, 1, 0)]
        if not all(row["tracking_phase"].upper() == "TRACKING" and
                   row["accepted"] == "1" for row in pair):
            continue
        centers = [center(row, "display") for row in pair]
        center_acceleration.append(math.hypot(
            centers[2][0] - 2 * centers[1][0] + centers[0][0],
            centers[2][1] - 2 * centers[1][1] + centers[0][1]))
        center_step.append(math.dist(centers[1], centers[2]))
        sizes = [(row["display_width"], row["display_height"]) for row in pair]
        size_acceleration.append(max(
            abs(sizes[2][axis] - 2 * sizes[1][axis] + sizes[0][axis]) /
            max(1.0, sizes[1][axis]) for axis in (0, 1)))
        size_step.append(max(
            abs(sizes[2][axis] - sizes[1][axis]) /
            max(1.0, sizes[1][axis]) for axis in (0, 1)))
    return {
        "accepted_tracking_frames": accepted_frames,
        "consecutive_tracking_triplets": len(center_acceleration),
        "center_second_difference_p95_px": percentile(center_acceleration, 0.95),
        "center_second_difference_p99_px": percentile(center_acceleration, 0.99),
        "center_step_max_px": max(center_step, default=None),
        "size_second_difference_p95_ratio": percentile(size_acceleration, 0.95),
        "size_second_difference_p99_ratio": percentile(size_acceleration, 0.99),
        "size_step_max_ratio": max(size_step, default=None),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("baseline", type=Path, help="CSV with stability disabled")
    parser.add_argument("candidate", type=Path, help="CSV with stability enabled")
    parser.add_argument("--warmup-frames", type=int, default=30)
    args = parser.parse_args()
    if args.warmup_frames < 0:
        parser.error("warmup frames must be nonnegative")
    baseline = load(args.baseline)
    candidate = load(args.candidate)
    frames = sorted(set(baseline) & set(candidate))
    if not frames:
        parser.error("no overlapping frame IDs")
    tracker_deltas = [max(abs(baseline[frame][field] - candidate[frame][field])
                          for field in BOX_COLUMNS[:4]) for frame in frames]
    phase_matches = sum(baseline[frame]["tracking_phase"] ==
                        candidate[frame]["tracking_phase"] and
                        baseline[frame]["accepted"] == candidate[frame]["accepted"]
                        for frame in frames)
    result = {
        "shared_frames": len(frames),
        "phase_and_acceptance_matches": phase_matches,
        "tracking_box_max_coordinate_delta_px": max(tracker_deltas),
        "comparable": set(baseline) == set(candidate) and
                      phase_matches == len(frames) and max(tracker_deltas) <= 0.01,
        "warmup_frames": args.warmup_frames,
        "baseline": metrics(baseline, frames, args.warmup_frames),
        "candidate": metrics(candidate, frames, args.warmup_frames),
        "evidence_boundary": "Frame smoothness only; inspect video to verify the box stays on the target.",
    }
    print(json.dumps(result, indent=2, ensure_ascii=False))
    if not result["comparable"]:
        raise SystemExit("traces differ in tracker state or boxes; paired display comparison is invalid")


if __name__ == "__main__":
    main()
