#!/usr/bin/env python3
"""One-pass NanoTrack evaluation on the official VisDrone-SOT splits."""

import argparse
import hashlib
import json
import os
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Sequence, Tuple

import cv2
import numpy as np

WORKSPACE_ROOT = Path(__file__).resolve().parents[1]
if str(WORKSPACE_ROOT) not in sys.path:
    sys.path.insert(0, str(WORKSPACE_ROOT))

from scripts.evaluate_uav123_nanotrack import evaluate_predictions, summarize_sequences
from scripts.uav123_nanotrack import read_boxes


Box = Tuple[float, float, float, float]


@dataclass(frozen=True)
class VisDroneSequence:
    name: str
    image_paths: Tuple[str, ...]
    boxes: Tuple[Box, ...]


def _split_root(root: Path, split: str) -> Path:
    if split == "val":
        return Path(root) / "VisDrone2019-SOT-val"
    if split == "test-dev":
        return (
            Path(root)
            / "VisDrone2019-SOT-test-dev"
            / "VisDrone2018-SOT-test-dev"
        )
    raise ValueError("unsupported VisDrone-SOT split {}".format(split))


def load_visdrone_sot_split(root: Path, split: str = "val") -> List[VisDroneSequence]:
    base = _split_root(root, split)
    annotation_root = base / "annotations"
    sequence_root = base / "sequences"
    if not annotation_root.is_dir() or not sequence_root.is_dir():
        raise FileNotFoundError("missing VisDrone-SOT {} layout under {}".format(split, base))
    sequences = []
    for annotation_path in sorted(annotation_root.glob("*.txt")):
        name = annotation_path.stem
        boxes = read_boxes(annotation_path)
        if any(box is None for box in boxes):
            raise ValueError("{} contains invalid SOT boxes".format(annotation_path))
        image_paths = tuple(str(path) for path in sorted((sequence_root / name).glob("*.jpg")))
        if len(image_paths) != len(boxes):
            raise ValueError(
                "{} image/annotation mismatch: {} vs {}".format(
                    name, len(image_paths), len(boxes)
                )
            )
        sequences.append(
            VisDroneSequence(
                name=name,
                image_paths=image_paths,
                boxes=tuple(box for box in boxes if box is not None),
            )
        )
    if not sequences:
        raise ValueError("no VisDrone-SOT sequences found in {}".format(base))
    return sequences


def run_visdrone_sequence(sequence: VisDroneSequence, tracker: object) -> Dict[str, object]:
    predictions: List[List[float]] = []
    start = time.perf_counter()
    for index, image_text in enumerate(sequence.image_paths):
        image = cv2.imread(image_text)
        if image is None:
            raise RuntimeError("cannot decode {}".format(image_text))
        if index == 0:
            prediction = [float(value) for value in sequence.boxes[0]]
            tracker.init(image, prediction)
        else:
            prediction = [float(value) for value in tracker.track(image)["bbox"]]
        predictions.append(prediction)
    elapsed = time.perf_counter() - start
    metrics = evaluate_predictions(sequence.boxes, [tuple(box) for box in predictions])
    return {
        "name": sequence.name,
        "category": "all",
        "frames": len(sequence.image_paths),
        "elapsed_seconds": elapsed,
        "fps": len(sequence.image_paths) / elapsed if elapsed > 0 else 0.0,
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
    parser.add_argument("--nanotrack-root", type=Path, required=True)
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--snapshot", type=Path, required=True)
    parser.add_argument("--split", choices=("val", "test-dev"), default="val")
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

    sequences = load_visdrone_sot_split(args.dataset_root, args.split)
    if args.max_sequences > 0:
        sequences = sequences[: args.max_sequences]
    output_dir = args.output_dir.resolve()
    results = []
    for index, sequence in enumerate(sequences, 1):
        result = run_visdrone_sequence(sequence, build_tracker(model))
        predictions = result.pop("predictions")
        (output_dir / "predictions").mkdir(parents=True, exist_ok=True)
        (output_dir / "predictions" / (sequence.name + ".txt")).write_text(
            "\n".join(",".join("{:.6f}".format(value) for value in box) for box in predictions)
            + "\n",
            encoding="utf-8",
        )
        results.append(result)
        _write_json(output_dir / "sequences" / (sequence.name + ".json"), result)
        print(
            "evaluated {}/{} {} AUC={:.4f} FPS={:.1f}".format(
                index,
                len(sequences),
                sequence.name,
                result["metrics"]["success_auc"],
                result["fps"],
            ),
            flush=True,
        )
    summary = summarize_sequences(results)
    summary.update(
        {
            "dataset": "VisDrone-SOT",
            "split": args.split,
            "snapshot": str(args.snapshot.resolve()),
            "snapshot_sha256": _sha256(args.snapshot),
            "sequence_names": [result["name"] for result in results],
            "mean_pipeline_fps": float(np.mean([result["fps"] for result in results])),
        }
    )
    _write_json(output_dir / "summary.json", summary)
    print(json.dumps(summary, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
