#!/usr/bin/env python3
"""Freeze the eight reviewed point tracks as car_003 through car_010 truth."""

import argparse
import csv
import hashlib
import json
from pathlib import Path

VIDEO_SHA256 = "eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d"
VISIBILITY_SHA256 = "d3b918ac4473d8ee256f23a5fc725724b057c228ab21136008a0e5e2cdd4adce"


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def intervals(frames):
    result = []
    for frame in sorted(frames):
        if result and result[-1][1] == frame - 1:
            result[-1][1] = frame
        else:
            result.append([frame, frame])
    return result


def freeze(dataset_root, output_root):
    dataset_root = Path(dataset_root)
    output_root = Path(output_root)
    ground_truth = dataset_root / "ground_truth"
    visibility_path = ground_truth / "eight_point_draft/human_confirmed_visibility.csv"
    if sha256(dataset_root / "1.mp4") != VIDEO_SHA256:
        raise ValueError("video identity changed")
    if sha256(visibility_path) != VISIBILITY_SHA256:
        raise ValueError("human visibility confirmation changed")
    with visibility_path.open(newline="", encoding="utf-8") as handle:
        visibility_rows = list(csv.DictReader(handle))
    visibility = {}
    for row in visibility_rows:
        key = (row["target_id"], int(row["frame_id"]))
        if key in visibility or row["visibility_status"] not in {"full_occlusion", "out_of_view"}:
            raise ValueError("duplicate or invalid visibility row")
        visibility[key] = row["visibility_status"]
    if len(visibility) != 175:
        raise ValueError("visibility row count changed")

    output_root.mkdir(parents=True, exist_ok=True)
    for point in range(1, 9):
        source_id = f"point_{point:02d}"
        target_id = f"car_{point + 2:03d}"
        source = ground_truth / source_id
        source_metadata = json.loads((source / "annotation_metadata.json").read_text())
        source_rows = [json.loads(line) for line in (source / "frames.jsonl").read_text().splitlines()]
        if len(source_rows) != 271 or [row["frame_id"] for row in source_rows] != list(range(271)):
            raise ValueError(f"{source_id}: invalid frame coverage")
        destination = output_root / target_id
        if destination.exists():
            raise FileExistsError(destination)
        labels = destination / "labels"
        labels.mkdir(parents=True)
        statuses = []
        records = []
        label_manifest = []
        occluded = []
        outside = []
        visible_count = 0
        for frame, source_row in enumerate(source_rows):
            box = source_row["bbox_xywh"]
            confirmed = visibility.get((source_id, frame))
            if box is None:
                if confirmed is None:
                    raise ValueError(f"{source_id} frame {frame}: unreviewed empty box")
                state = confirmed
                review = "manual_confirmed_no_bbox"
                (occluded if state == "full_occlusion" else outside).append(frame)
            else:
                if confirmed is not None:
                    raise ValueError(f"{source_id} frame {frame}: visibility conflicts with bbox")
                state = "visible"
                review = "manual_confirmed_visible"
                visible_count += 1
            label_source = source / "labels" / f"frame_{frame:06d}.txt"
            label = labels / label_source.name
            content = label_source.read_bytes()
            if bool(content.strip()) != (box is not None):
                raise ValueError(f"{source_id} frame {frame}: box-label mismatch")
            label.write_bytes(content)
            label_manifest.append(f"{sha256(label)}  labels/{label.name}\n")
            statuses.append({"frame_id": frame, "target_state": state,
                             "review_status": review, "identity_status": "certain",
                             "annotation_quality": "reliable" if box else "not_observable"})
            records.append({"frame_id": frame, "bbox_xywh": box, "target_state": state,
                            "review_status": review, "ordinary_iou_allowed": box is not None})
        (destination / "labels.sha256").write_text("".join(label_manifest))
        with (destination / "annotation_status.csv").open("w", newline="") as handle:
            writer = csv.DictWriter(handle, fieldnames=list(statuses[0]))
            writer.writeheader()
            writer.writerows(statuses)
        with (destination / "frames.jsonl").open("w") as handle:
            for record in records:
                handle.write(json.dumps(record, sort_keys=True) + "\n")
        events = []
        for start, end in intervals(occluded):
            first_after = next((frame for frame in range(end + 1, 271)
                                if statuses[frame]["target_state"] == "visible"), None)
            if any(statuses[frame]["target_state"] == "out_of_view"
                   for frame in range(end + 1, first_after or 271)):
                first_after = None
            events.append({"type": "full_occlusion", "frames": [start, end],
                           "first_visible_after": first_after})
        for start, end in intervals(outside):
            events.append({"type": "out_of_view", "frames": [start, end]})
        (destination / "events.json").write_text(json.dumps({"events": events}, indent=2) + "\n")
        metadata = {
            "ground_truth_schema_version": "2.0-multi-event", "draft": False,
            "formal_evaluation_authorized": True, "unresolved_review_count": 0,
            "target_id": target_id, "source_point_id": source_id,
            "initial_point_xy": source_metadata["initial_point_xy"],
            "video": {"path": str(dataset_root / "1.mp4"), "sha256": VIDEO_SHA256,
                      "width": 1920, "height": 1080, "fps_num": 30, "fps_den": 1,
                      "frame_count": 271, "frame_id_origin": 0},
            "review_status_counts": {"manual_confirmed_visible": visible_count,
                                     "manual_confirmed_no_bbox": len(occluded) + len(outside),
                                     "needs_human_review": 0},
            "visibility_counts": {"visible": visible_count,
                                  "full_occlusion": len(occluded), "out_of_view": len(outside)},
            "source_draft_manifest_sha256": sha256(source / "checksums.sha256"),
            "visibility_confirmation_sha256": VISIBILITY_SHA256,
        }
        (destination / "annotation_metadata.json").write_text(json.dumps(metadata, indent=2, sort_keys=True) + "\n")
        (destination / "README.md").write_text(
            f"# {target_id} confirmed truth\n\n"
            f"Frame-0 point {source_metadata['initial_point_xy']} maps from {source_id}. "
            "Visible labels are full-object human-accepted boxes. Full occlusion and "
            "out-of-view frames have empty labels; there are no observable truth boxes "
            "in these frames. Prediction boxes are runtime output, not truth.\n")
        files = sorted((path for path in destination.rglob("*") if path.is_file()),
                       key=lambda path: path.relative_to(destination).as_posix())
        (destination / "checksums.sha256").write_text("".join(
            f"{sha256(path)}  {path.relative_to(destination)}\n" for path in files))
        print(target_id, metadata["visibility_counts"], sha256(destination / "checksums.sha256"))
    if set(visibility) != {(f"point_{point:02d}", frame)
                           for point in range(1, 9)
                           for frame in range(271)
                           if source_frame_empty(ground_truth / f"point_{point:02d}", frame)}:
        raise ValueError("visibility confirmation did not cover precisely all empty frames")


def source_frame_empty(source, frame):
    return not (source / "labels" / f"frame_{frame:06d}.txt").read_text().strip()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dataset-root", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    args = parser.parse_args()
    freeze(args.dataset_root, args.output_root)
