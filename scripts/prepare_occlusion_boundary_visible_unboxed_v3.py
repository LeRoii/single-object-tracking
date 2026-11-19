#!/usr/bin/env python3
"""Stage user-confirmed partial-visibility boundaries as versioned truth."""

import argparse
import csv
import hashlib
import json
import shutil
from collections import Counter
from pathlib import Path


CORRECTIONS = {
    "car_003": ("ground_truth/car_003", (130,), (130, 138)),
    "car_004": ("ground_truth_versions/car_004_visible_unboxed_v2",
                (131, 132), (131, 136)),
    "car_010": ("ground_truth_versions/car_010_visible_unboxed_v2",
                (156, 157), (156, 168)),
}


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def prepare(source_root, output_root):
    source_root = source_root.resolve()
    if output_root.exists():
        raise ValueError("refusing to overwrite existing output root: {}".format(output_root))
    versions = output_root / "ground_truth_versions"
    versions.mkdir(parents=True)
    (output_root / "1.mp4").symlink_to(source_root / "1.mp4")
    (output_root / "ground_truth").symlink_to(source_root / "ground_truth")
    results = {}
    for target_id, (source_relative, frames, event_range) in CORRECTIONS.items():
        source = source_root / source_relative
        version = target_id + "_visible_unboxed_v3"
        target = versions / version
        shutil.copytree(source, target)

        frame_path = target / "frames.jsonl"
        entries = [json.loads(line) for line in frame_path.read_text().splitlines()]
        if len(entries) != 271:
            raise ValueError("unexpected frame count in {}".format(source))
        for frame_id in frames:
            entry = entries[frame_id]
            if (entry.get("frame_id") != frame_id or
                    entry.get("target_state") != "full_occlusion" or
                    entry.get("review_status") != "manual_confirmed_no_bbox" or
                    entry.get("bbox_xywh") is not None or
                    entry.get("ordinary_iou_allowed") is not False):
                raise ValueError("unexpected source truth at {} frame {}".format(
                    target_id, frame_id))
            entry["target_state"] = "visible"
            entry["review_status"] = "user_confirmed_visible_unboxed"
        frame_path.write_text("".join(json.dumps(entry, sort_keys=True) + "\n"
                                      for entry in entries), encoding="utf-8")

        status_path = target / "annotation_status.csv"
        with status_path.open(newline="", encoding="utf-8") as handle:
            reader = csv.DictReader(handle)
            fields = reader.fieldnames
            rows = list(reader)
        if fields is None or len(rows) != 271:
            raise ValueError("unexpected annotation status in {}".format(source))
        for frame_id in frames:
            row = rows[frame_id]
            if (row["frame_id"] != str(frame_id) or
                    row["target_state"] != "full_occlusion" or
                    row["review_status"] != "manual_confirmed_no_bbox"):
                raise ValueError("unexpected source status at {} frame {}".format(
                    target_id, frame_id))
            row.update(target_state="visible",
                       review_status="user_confirmed_visible_unboxed",
                       annotation_quality="unboxed")
        with status_path.open("w", newline="", encoding="utf-8") as handle:
            writer = csv.DictWriter(handle, fieldnames=fields, lineterminator="\n")
            writer.writeheader()
            writer.writerows(rows)

        events_path = target / "events.json"
        events = json.loads(events_path.read_text(encoding="utf-8"))
        matches = [event for event in events["events"]
                   if event.get("type") == "full_occlusion" and
                   event.get("frames") == list(event_range)]
        if len(matches) != 1:
            raise ValueError("expected full occlusion event missing in {}".format(source))
        matches[0]["frames"][0] = max(frames) + 1
        events_path.write_text(json.dumps(events, ensure_ascii=False, indent=2) + "\n",
                               encoding="utf-8")

        metadata_path = target / "annotation_metadata.json"
        metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
        state_counts = Counter(entry["target_state"] for entry in entries)
        metadata["visibility_counts"] = {
            state: state_counts[state]
            for state in ("visible", "full_occlusion", "out_of_view")
        }
        metadata["review_status_counts"] = dict(Counter(
            entry["review_status"] for entry in entries))
        metadata.setdefault("correction_history", []).append({
            "frames": list(frames),
            "previous_state": "full_occlusion",
            "new_state": "visible",
            "visibility_authority": "user confirmation in conversation 2026-11-18",
            "bbox_status": "unboxed; no measured visible extent supplied",
            "source_package_sha256": sha256(source / "checksums.sha256"),
        })
        metadata_path.write_text(json.dumps(metadata, ensure_ascii=False,
                                            indent=2) + "\n", encoding="utf-8")
        with (target / "README.md").open("a", encoding="utf-8") as handle:
            handle.write("\n## Versioned partial-visibility boundary correction\n\n")
            handle.write("The user confirmed frames {} are partly visible. No "
                         "measured visible boxes were supplied, so they remain "
                         "unboxed and excluded from ordinary IoU. The source "
                         "package remains unchanged.\n".format(
                             ", ".join(map(str, frames))))

        checksums = target / "checksums.sha256"
        paths = [line.split("  ", 1)[1] for line in checksums.read_text().splitlines()]
        checksums.write_text("".join(
            "{}  {}\n".format(sha256(target / relative), relative)
            for relative in paths), encoding="utf-8")
        results[version] = sha256(checksums)
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    args = parser.parse_args()
    for name, digest in prepare(args.source_root, args.output_root).items():
        print(name, digest)


if __name__ == "__main__":
    main()
