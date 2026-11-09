#!/usr/bin/env python3
"""Audit all eligible ordered frame pairs in existing GOT training manifests."""

import argparse
from collections import Counter
import hashlib
import json
import math
from pathlib import Path

try:
    from scripts.nanotrack_pair_sampling import eligible_search_frames, load_pair_policy
except ModuleNotFoundError:  # Direct script invocation from outside the repository root.
    from nanotrack_pair_sampling import eligible_search_frames, load_pair_policy


def _frame_is_valid(bbox):
    # Match upstream SubDataset._filter_zero before extracting numeric frame keys.
    if isinstance(bbox, dict):
        return True
    if len(bbox) == 4:
        width, height = bbox[2] - bbox[0], bbox[3] - bbox[1]
    else:
        width, height = bbox
    return width > 0 and height > 0


def _quantiles(histogram):
    count = sum(histogram.values())
    ordered = sorted(histogram.items())
    def value_at(index):
        cumulative = 0
        for value, frequency in ordered:
            cumulative += frequency
            if index < cumulative:
                return value
    def percentile(fraction):
        rank = (count - 1) * fraction
        lower, upper = math.floor(rank), math.ceil(rank)
        return value_at(lower) + (value_at(upper) - value_at(lower)) * (rank - lower)
    if not count:
        return {"p50": None, "p90": None, "p95": None, "max": None}
    return {"p50": percentile(.5), "p90": percentile(.9), "p95": percentile(.95),
            "max": ordered[-1][0]}


def audit_manifest(name, path, frame_range):
    """Exhaustive counts, with no images, training, or hidden-set reads."""
    path = Path(path)
    if "hidden" in str(path).lower() or "hidden" in str(path.resolve()).lower():
        raise ValueError("hidden manifests are forbidden in the training pair audit")
    if not name.startswith("GOT_"):
        raise ValueError("audit datasets must start with GOT_")
    raw = path.read_bytes()
    labels = json.loads(raw.decode("utf-8"))
    modes = ("list_position", "original_frame_id")
    histograms = {mode: Counter() for mode in modes}
    counts = {mode: [] for mode in modes}
    same_probabilities = {mode: [] for mode in modes}
    sequence_count = track_count = frame_count = excluded = 0
    for tracks in labels.values():
        sequence_used = False
        for track in tracks.values():
            frames = sorted(int(key) for key, bbox in track.items()
                            if key.isdigit() and _frame_is_valid(bbox))
            if not frames:
                continue
            sequence_used = True
            track_count += 1
            frame_count += len(frames)
            for template in frames:
                candidates = {}
                for mode in modes:
                    search = eligible_search_frames(frames, template, frame_range, mode)
                    candidates[mode] = search
                    counts[mode].append(len(search))
                    histograms[mode].update(abs(frame - template) for frame in search)
                    same_probabilities[mode].append(1. / len(search))
                excluded += len(set(candidates["list_position"]) - set(candidates["original_frame_id"]))
        sequence_count += int(sequence_used)
    result = {"name": name, "manifest_path": str(path.resolve()),
              "manifest_sha256": hashlib.sha256(raw).hexdigest(), "frame_range": frame_range,
              "sequence_count": sequence_count, "track_count": track_count, "frame_count": frame_count}
    for mode in modes:
        histogram = histograms[mode]
        total = sum(histogram.values())
        count = counts[mode]
        result[mode] = {
            "eligible_candidate_count": total,
            "candidates_per_template": {"min": min(count) if count else None,
                                        "max": max(count) if count else None,
                                        "mean": total / frame_count if frame_count else None},
            "frame_gap": _quantiles(histogram),
            "same_frame_proportion": histogram[0] / total if total else None,
            "uniform_template_same_frame_probability": (
                math.fsum(same_probabilities[mode]) / frame_count if frame_count else None),
        }
    old_count = result["list_position"]["eligible_candidate_count"]
    result["old_candidates_excluded_count"] = excluded
    result["old_candidates_excluded_proportion"] = excluded / old_count if old_count else None
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", action="append", required=True, metavar="GOT_NAME=PATH")
    parser.add_argument("--pair-policy", type=Path, required=True)
    parser.add_argument("--frame-range", type=int, default=100)
    parser.add_argument("--output-dir", type=Path, default=(
        Path(__file__).resolve().parents[1] / "artifacts/nanotrack_factorial_v3/pair-audit"))
    args = parser.parse_args()
    if args.frame_range < 0:
        raise ValueError("frame_range must be non-negative")
    identity = load_pair_policy(args.pair_policy)
    manifests = []
    for item in args.manifest:
        name, path = item.split("=", 1)
        if not any(name.startswith(prefix) for prefix in identity["policy"]):
            raise ValueError("audit manifest is not matched by pair-policy: {}".format(name))
        manifests.append(audit_manifest(name, Path(path), args.frame_range))
    report = {
        "schema_version": 1, "pair_policy": identity,
        "measurement_boundary": (
            "Exhaustive ordered candidate pairs, equally weighted for gap percentiles "
            "and same_frame_proportion; percentile interpolation is linear. "
            "uniform_template_same_frame_probability separately weights templates equally "
            "within each manifest, then search candidates uniformly; no dataset quota weights."),
        "filtering": "Numeric frame keys after upstream nonpositive-bbox filtering; no image reads.",
        "manifests": manifests,
    }
    args.output_dir.mkdir(parents=True, exist_ok=True)
    (args.output_dir / "pair-policy.json").write_bytes(Path(identity["path"]).read_bytes())
    output = args.output_dir / "audit.json"
    output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(str(output.resolve()))


if __name__ == "__main__":
    main()
