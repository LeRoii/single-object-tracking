#!/usr/bin/env python3
"""Split-aware one-pass evaluation for NanoTrack on UAV123_10fps."""

import argparse
import hashlib
import json
import math
import os
import sys
import time
from pathlib import Path
from typing import Dict, List, Optional, Sequence, Tuple

import cv2
import numpy as np

WORKSPACE_ROOT = Path(__file__).resolve().parents[1]
if str(WORKSPACE_ROOT) not in sys.path:
    sys.path.insert(0, str(WORKSPACE_ROOT))

from scripts.uav123_nanotrack import SequenceSpec, category_for, read_boxes


Box = Tuple[float, float, float, float]


def _iou_xywh(first: Box, second: Box) -> float:
    ax1, ay1, aw, ah = first
    bx1, by1, bw, bh = second
    ax2, ay2 = ax1 + max(0.0, aw), ay1 + max(0.0, ah)
    bx2, by2 = bx1 + max(0.0, bw), by1 + max(0.0, bh)
    intersection = max(0.0, min(ax2, bx2) - max(ax1, bx1)) * max(
        0.0, min(ay2, by2) - max(ay1, by1)
    )
    union = max(0.0, aw) * max(0.0, ah) + max(0.0, bw) * max(0.0, bh) - intersection
    return intersection / union if union > 0 else 0.0


def _center_distance(first: Box, second: Box) -> float:
    first_center = (first[0] + first[2] / 2.0, first[1] + first[3] / 2.0)
    second_center = (second[0] + second[2] / 2.0, second[1] + second[3] / 2.0)
    return math.hypot(first_center[0] - second_center[0], first_center[1] - second_center[1])


def _normalized_center_distance(prediction: Box, ground_truth: Box) -> float:
    pred_center = (prediction[0] + prediction[2] / 2.0, prediction[1] + prediction[3] / 2.0)
    gt_center = (ground_truth[0] + ground_truth[2] / 2.0, ground_truth[1] + ground_truth[3] / 2.0)
    width = max(ground_truth[2], 1e-12)
    height = max(ground_truth[3], 1e-12)
    return math.sqrt(
        ((pred_center[0] - gt_center[0]) / width) ** 2
        + ((pred_center[1] - gt_center[1]) / height) ** 2
    )


def evaluate_predictions(
    ground_truth: Sequence[Optional[Box]], predictions: Sequence[Box]
) -> Dict[str, float]:
    if len(ground_truth) != len(predictions):
        raise ValueError("ground-truth and prediction lengths differ")
    valid_pairs = [
        (prediction, target)
        for prediction, target in zip(predictions, ground_truth)
        if target is not None
    ]
    if not valid_pairs:
        raise ValueError("sequence has no valid ground-truth frames")
    overlaps = np.asarray([_iou_xywh(prediction, target) for prediction, target in valid_pairs])
    distances = np.asarray([_center_distance(prediction, target) for prediction, target in valid_pairs])
    normalized = np.asarray(
        [_normalized_center_distance(prediction, target) for prediction, target in valid_pairs]
    )
    success_thresholds = np.linspace(0.0, 1.0, 21)
    normalized_thresholds = np.linspace(0.0, 0.5, 51)
    success_curve = np.asarray([(overlaps > threshold).mean() for threshold in success_thresholds])
    normalized_curve = np.asarray(
        [(normalized <= threshold).mean() for threshold in normalized_thresholds]
    )
    return {
        "valid_frames": int(len(valid_pairs)),
        "success_auc": float(success_curve.mean()),
        "precision_20": float((distances <= 20.0).mean()),
        "normalized_precision_auc": float(normalized_curve.mean()),
        "mean_iou": float(overlaps.mean()),
        "zero_iou_frames": int((overlaps <= 0.0).sum()),
    }


def _aggregate(results: Sequence[Dict[str, object]]) -> Dict[str, float]:
    scalar_keys = ("success_auc", "precision_20", "normalized_precision_auc", "mean_iou")
    aggregate = {
        key: float(np.mean([result["metrics"][key] for result in results]))
        for key in scalar_keys
    }
    aggregate.update(
        {
            "sequences": len(results),
            "valid_frames": int(sum(result["metrics"]["valid_frames"] for result in results)),
            "zero_iou_frames": int(
                sum(result["metrics"]["zero_iou_frames"] for result in results)
            ),
        }
    )
    return aggregate


def summarize_sequences(results: Sequence[Dict[str, object]]) -> Dict[str, object]:
    if not results:
        raise ValueError("no sequence results")
    categories = sorted({str(result["category"]) for result in results})
    return {
        "overall": _aggregate(results),
        "categories": {
            category: _aggregate(
                [result for result in results if result["category"] == category]
            )
            for category in categories
        },
    }


def run_sequence(dataset_root: Path, spec: SequenceSpec, tracker: object) -> Dict[str, object]:
    root = Path(dataset_root)
    boxes = read_boxes(root / "anno" / "UAV123_10fps" / (spec.name + ".txt"))
    if len(boxes) != spec.frame_count:
        raise ValueError("{} annotation length mismatch".format(spec.name))
    if not boxes or boxes[0] is None:
        raise ValueError("{} has no valid first-frame initialization box".format(spec.name))

    image_root = root / "data_seq" / "UAV123_10fps" / spec.folder
    predictions: List[List[float]] = []
    start = time.perf_counter()
    for offset in range(spec.frame_count):
        frame_number = spec.start_frame + offset
        image_path = image_root / (
            ("{:0" + str(spec.zero_padding) + "d}.{}").format(frame_number, spec.extension)
        )
        image = cv2.imread(str(image_path))
        if image is None:
            raise RuntimeError("cannot decode {}".format(image_path))
        if offset == 0:
            initial = [float(value) for value in boxes[0]]
            tracker.init(image, initial)
            prediction = initial
        else:
            output = tracker.track(image)
            prediction = [float(value) for value in output["bbox"]]
        predictions.append(prediction)
    elapsed = time.perf_counter() - start
    metrics = evaluate_predictions(boxes, [tuple(box) for box in predictions])
    return {
        "name": spec.name,
        "folder": spec.folder,
        "category": category_for(spec.name),
        "frames": spec.frame_count,
        "elapsed_seconds": elapsed,
        "fps": spec.frame_count / elapsed if elapsed > 0 else 0.0,
        "predictions": predictions,
        "metrics": metrics,
    }


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with Path(path).open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True), encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dataset-root", type=Path, required=True)
    parser.add_argument("--prepared-root", type=Path, required=True)
    parser.add_argument("--nanotrack-root", type=Path, required=True)
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--snapshot", type=Path, required=True)
    parser.add_argument("--split", choices=("train", "val", "test"), required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--max-sequences", type=int, default=0)
    args = parser.parse_args()

    nanotrack_root = args.nanotrack_root.resolve()
    sys.path.insert(0, str(nanotrack_root))
    os.chdir(str(nanotrack_root))
    import torch
    from nanotrack.core.config import cfg
    from nanotrack.models.model_builder import ModelBuilder
    from nanotrack.tracker.tracker_builder import build_tracker
    from nanotrack.utils.model_load import load_pretrain

    if not torch.cuda.is_available():
        raise RuntimeError("CUDA is unavailable in this execution environment")
    cfg.merge_from_file(str(args.config.resolve()))
    model = ModelBuilder()
    load_pretrain(model, str(args.snapshot.resolve()))
    model = model.cuda().eval()

    split = json.loads((args.prepared_root / "splits.json").read_text(encoding="utf-8"))
    selected_names = split["logical_sequences"][args.split]
    if args.max_sequences > 0:
        selected_names = selected_names[: args.max_sequences]
    selected = set(selected_names)
    from scripts.uav123_nanotrack import parse_config_seqs

    specs = [
        spec
        for spec in parse_config_seqs(args.dataset_root / "configSeqs.m")
        if spec.name in selected
    ]
    output_dir = args.output_dir.resolve()
    prediction_dir = output_dir / "predictions"
    prediction_dir.mkdir(parents=True, exist_ok=True)
    sequence_results = []
    for index, spec in enumerate(specs, 1):
        result = run_sequence(args.dataset_root, spec, build_tracker(model))
        prediction_path = prediction_dir / (spec.name + ".txt")
        prediction_path.write_text(
            "\n".join(
                ",".join("{:.6f}".format(value) for value in box)
                for box in result["predictions"]
            )
            + "\n",
            encoding="utf-8",
        )
        compact = {key: value for key, value in result.items() if key != "predictions"}
        sequence_results.append(compact)
        _write_json(output_dir / "sequences" / (spec.name + ".json"), compact)
        print(
            "evaluated {}/{} {} AUC={:.4f} FPS={:.1f}".format(
                index,
                len(specs),
                spec.name,
                compact["metrics"]["success_auc"],
                compact["fps"],
            ),
            flush=True,
        )

    summary = summarize_sequences(sequence_results)
    summary.update(
        {
            "split": args.split,
            "config": str(args.config.resolve()),
            "snapshot": str(args.snapshot.resolve()),
            "snapshot_sha256": _sha256(args.snapshot),
            "sequence_names": [result["name"] for result in sequence_results],
            "mean_pipeline_fps": float(np.mean([result["fps"] for result in sequence_results])),
        }
    )
    _write_json(output_dir / "summary.json", summary)
    print(json.dumps(summary, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
