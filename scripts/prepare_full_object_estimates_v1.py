#!/usr/bin/env python3
"""Build the reviewed, approximate full-object geometry overlay for occ_video_1.

The numbers below are visual estimates, never tracker output. Historical source
labels and packages remain immutable. Re-run only against the named video and
truth package identities, then review the emitted diff before changing a version.
"""

import argparse
import hashlib
import json
from pathlib import Path

from evaluate_occ_video_1 import load_contract, load_truth

VIDEO_SHA256 = "eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d"
BASE = {
    **{f"car_{i:03d}": None for i in range(1, 11)},
    "car_003": "car_003_visible_unboxed_v3",
    "car_004": "car_004_full_estimated_v4",
    "car_009": "car_009_visible_unboxed_v2",
    "car_010": "car_010_visible_unboxed_v3",
}


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def build(root):
    records = {target: [] for target in BASE}
    source = {target: load_contract(root, target, version) for target, version in BASE.items()}
    truth = {target: load_truth(contract) for target, contract in source.items()}

    def add(target, frame, box, method, visible=None):
        records[target].append({
            "frame_id": frame,
            "estimated_full_bbox_xywh": [round(float(value), 2) for value in box],
            "visible_bbox_xywh": visible,
            "method": method,
        })

    # Existing human-confirmed interpolation already represents the full car.
    for frame in (169, 170, 171, 181):
        add("car_001", frame, truth["car_001"][frame]["bbox"],
            "existing bidirectional full-object interpolation; hidden extent uncertain")

    # car_002: partly visible bridge passage bracketed by full-object labels 210/230.
    before, after = truth["car_002"][210]["bbox"], truth["car_002"][230]["bbox"]
    for frame in (211, 212, *range(217, 230)):
        ratio = (frame - 210) / 20
        box = [before[i] * (1 - ratio) + after[i] * ratio for i in range(4)]
        add("car_002", frame, box,
            "linear bridge-passage estimate from confirmed frames 210 and 230; hidden edge uncertain")

    # car_003: source labels 124-129 shrink to the roof as the lower car is hidden.
    for frame in range(124, 131):
        visible = truth["car_003"][frame]["bbox"]
        roof_y = visible[1] if visible else 888.0
        add("car_003", frame,
            [803.5 + (frame - 124) * 1.5, roof_y, 55, 46],
            "visual roof and pre-bridge full-size estimate; hidden edge uncertain", visible)

    # car_004: carry over v4's 127-129 full estimates and adjacent roof motion.
    boxes = {
        126: [731.5, 874.8, 52, 53], 127: [730, 876.5, 53, 52.5],
        128: [730, 879, 52, 51], 129: [730, 879, 52, 51],
        130: [728, 881.5, 52, 51], 131: [725, 884, 52, 51],
        132: [723, 886, 52, 51],
    }
    existing = {int(key): value for key, value in json.loads(
        (source["car_004"]["annotation_root"] / "estimated_full_object_boxes.json")
        .read_text(encoding="utf-8"))["frames"].items()}
    for frame, box in boxes.items():
        if frame in existing and box != existing[frame]["estimated_full_bbox_xywh"]:
            raise ValueError("v4 full-object estimate changed at frame {}".format(frame))
        visible = (existing[frame]["visible_bbox_xywh"] if frame in existing
                   else truth["car_004"][frame]["bbox"])
        add("car_004", frame, box,
            "existing v4 visual estimate" if frame in existing else
            "visual roof progression and frame-125 full-object extent; hidden edge uncertain",
            visible)

    # car_009: grey car remains partially visible through the bridge.
    before, after = truth["car_009"][114]["bbox"], truth["car_009"][121]["bbox"]
    for frame in range(115, 121):
        ratio = (frame - 114) / 7
        box = [before[i] * (1 - ratio) + after[i] * ratio for i in range(4)]
        add("car_009", frame, box,
            "linear between confirmed frames 114 and 121; partly hidden outline")

    # car_010: yellow car roof/right edge move behind bridge; uncertainty is high.
    for frame in range(149, 158):
        step = frame - 148
        add("car_010", frame, [982.2 + 2.45 * step, 440.3 + 1.25 * step, 31, 36],
            "visual roof/right-edge motion from frames 148-157; hidden extent highly uncertain")

    targets = {}
    for target, contract in source.items():
        identity = contract["identity"]
        hash_field = ("package_checksums" if "package_checksums" in identity["hashes"]
                      else "annotation_metadata")
        targets[target] = {
            "base_truth_version": BASE[target],
            "base_hash_field": hash_field,
            "base_sha256": identity["hashes"][hash_field],
            "frames": sorted(records[target], key=lambda item: item["frame_id"]),
        }
    return {
        "schema_version": 1,
        "version": "full_object_estimates_v1",
        "video_sha256": VIDEO_SHA256,
        "bbox_format": "xywh_pixels",
        "scoring": {"ordinary_iou_allowed": False,
                    "max_center_distance_in_target_diagonals": 1.0},
        "policy": "Partial-occlusion full-object boxes are estimates; visible-part geometry is preserved. Estimated frames use only a coarse center-distance gate, never ordinary IoU. Full occlusion stays unboxed.",
        "targets": targets,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dataset-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if sha256(args.dataset_root / "1.mp4") != VIDEO_SHA256:
        raise ValueError("input video SHA-256 mismatch")
    payload = build(args.dataset_root)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n",
                           encoding="utf-8")
    print(args.output, {target: len(value["frames"])
                        for target, value in payload["targets"].items()})


if __name__ == "__main__":
    main()
