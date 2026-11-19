#!/usr/bin/env python3
"""Stage the user-confirmed car_009 visibility correction without inventing boxes."""

import argparse
import csv
import hashlib
import json
import shutil
from pathlib import Path


VERSION = "car_009_visible_unboxed_v2"
FRAMES = tuple(range(115, 121))


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def prepare(source_root, output_root):
    source_root = source_root.resolve()
    target = output_root / "ground_truth_versions" / VERSION
    if output_root.exists():
        raise ValueError("refusing to overwrite existing output root: {}".format(output_root))
    target.parent.mkdir(parents=True)
    shutil.copytree(source_root / "ground_truth" / "car_009", target)
    (output_root / "1.mp4").symlink_to(source_root / "1.mp4")
    (output_root / "ground_truth").symlink_to(source_root / "ground_truth")

    path = target / "frames.jsonl"
    lines = path.read_text(encoding="utf-8").splitlines()
    if len(lines) != 271:
        raise ValueError("unexpected frame count")
    for frame_id in FRAMES:
        entry = json.loads(lines[frame_id])
        if (entry.get("frame_id") != frame_id or
                entry.get("target_state") != "full_occlusion" or
                entry.get("review_status") != "manual_confirmed_no_bbox" or
                entry.get("bbox_xywh") is not None):
            raise ValueError("unexpected source truth at frame {}".format(frame_id))
        entry["target_state"] = "visible"
        entry["review_status"] = "user_confirmed_visible_unboxed"
        lines[frame_id] = json.dumps(entry, sort_keys=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")

    path = target / "annotation_status.csv"
    with path.open(newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        fields = reader.fieldnames
        rows = list(reader)
    if fields is None or len(rows) != 271:
        raise ValueError("unexpected annotation status")
    for frame_id in FRAMES:
        row = rows[frame_id]
        if (row["frame_id"] != str(frame_id) or
                row["target_state"] != "full_occlusion" or
                row["review_status"] != "manual_confirmed_no_bbox"):
            raise ValueError("unexpected source status at frame {}".format(frame_id))
        row.update(target_state="visible",
                   review_status="user_confirmed_visible_unboxed",
                   annotation_quality="unboxed")
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)

    path = target / "events.json"
    events = json.loads(path.read_text(encoding="utf-8"))
    original_count = len(events["events"])
    events["events"] = [event for event in events["events"]
                        if event.get("frames") != [115, 120]]
    if len(events["events"]) != original_count - 1:
        raise ValueError("expected full occlusion event is missing")
    path.write_text(json.dumps(events, ensure_ascii=False, indent=2) + "\n",
                    encoding="utf-8")

    path = target / "annotation_metadata.json"
    metadata = json.loads(path.read_text(encoding="utf-8"))
    if metadata["visibility_counts"] != {
            "full_occlusion": 12, "out_of_view": 0, "visible": 259}:
        raise ValueError("unexpected source visibility counts")
    metadata["visibility_counts"] = {
        "full_occlusion": 6, "out_of_view": 0, "visible": 265}
    metadata["review_status_counts"] = {
        "manual_confirmed_no_bbox": 6,
        "manual_confirmed_visible": 259,
        "user_confirmed_visible_unboxed": 6,
        "needs_human_review": 0,
    }
    metadata["correction_history"] = [{
        "frames": [115, 120],
        "previous_state": "full_occlusion",
        "new_state": "visible",
        "visibility_authority": "user confirmation in conversation 2026-11-18",
        "bbox_status": "unboxed; no measured visible extent supplied",
        "source_package_sha256": sha256(
            source_root / "ground_truth" / "car_009" / "checksums.sha256"),
    }]
    path.write_text(json.dumps(metadata, ensure_ascii=False, indent=2) + "\n",
                    encoding="utf-8")

    with (target / "README.md").open("a", encoding="utf-8") as handle:
        handle.write("\n## Versioned frames 115–120 visibility correction\n\n")
        handle.write("The user confirmed that these frames are partially visible. "
                     "No measured visible boxes were supplied, so they remain empty "
                     "and are excluded from ordinary IoU. The original package "
                     "remains unchanged.\n")

    checksums = target / "checksums.sha256"
    paths = [line.split("  ", 1)[1] for line in checksums.read_text().splitlines()]
    checksums.write_text("".join(
        "{}  {}\n".format(sha256(target / relative), relative)
        for relative in paths), encoding="utf-8")
    return target, sha256(checksums)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    args = parser.parse_args()
    target, manifest_sha = prepare(args.source_root, args.output_root)
    print(target)
    print("package checksums SHA-256", manifest_sha)


if __name__ == "__main__":
    main()
