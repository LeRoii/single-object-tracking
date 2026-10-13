#!/usr/bin/env python3
"""Evaluate NanoTrack with local GOT-10K AO, SR0.5 and SR0.75 semantics."""

import argparse
import ast
import configparser
import hashlib
import json
import math
import os
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Mapping, Optional, Sequence, Tuple

import cv2


WORKSPACE_ROOT = Path(__file__).resolve().parents[1]
if str(WORKSPACE_ROOT) not in sys.path:
    sys.path.insert(0, str(WORKSPACE_ROOT))


Box = Tuple[float, float, float, float]


@dataclass(frozen=True)
class GotEvaluationSequence:
    name: str
    root_class: str
    image_paths: Tuple[str, ...]
    boxes_xywh: Tuple[Box, ...]
    cover: Tuple[int, ...]
    resolution: Optional[Tuple[int, int]]


def _read_boxes(path: Path) -> Tuple[Box, ...]:
    result = []
    for line in Path(path).read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        box = tuple(float(value.strip()) for value in line.split(","))
        if len(box) != 4 or not all(math.isfinite(value) for value in box):
            raise ValueError("invalid GOT-10K box in {}".format(path))
        result.append(box)
    return tuple(result)  # type: ignore


def _read_cover(path: Path) -> Tuple[int, ...]:
    return tuple(int(float(line.strip())) for line in Path(path).read_text(
        encoding="utf-8").splitlines() if line.strip())


def load_got_sequence(path: Path) -> GotEvaluationSequence:
    root = Path(path).resolve()
    parser = configparser.ConfigParser()
    if not parser.read(str(root / "meta_info.ini"), encoding="utf-8"):
        raise ValueError("cannot read GOT-10K metadata: {}".format(root))
    root_class = parser.get("METAINFO", "root_class").strip().lower()
    resolution = None
    if parser.has_option("METAINFO", "resolution"):
        value = ast.literal_eval(parser.get("METAINFO", "resolution"))
        if (not isinstance(value, tuple) or len(value) != 2 or
                int(value[0]) <= 0 or int(value[1]) <= 0):
            raise ValueError("invalid resolution in {}".format(root))
        resolution = (int(value[0]), int(value[1]))
    boxes = _read_boxes(root / "groundtruth.txt")
    cover = _read_cover(root / "cover.label")
    if len(boxes) != len(cover) or len(boxes) < 2:
        raise ValueError("GOT-10K image/box/cover lengths differ for {}".format(root.name))
    images = tuple(str(root / "{:08d}.jpg".format(index))
                   for index in range(1, len(boxes) + 1))
    missing = [path for path in images if not Path(path).is_file()]
    if missing:
        raise FileNotFoundError(missing[0])
    return GotEvaluationSequence(
        root.name, root_class, images, boxes, cover, resolution
    )


def _bounded_iou(left: Box, right: Box,
                 resolution: Optional[Tuple[int, int]] = None) -> float:
    def clip(box):
        x, y, width, height = box
        if resolution is not None:
            bound_width, bound_height = resolution
            x = min(max(x, 0.0), float(bound_width))
            y = min(max(y, 0.0), float(bound_height))
            width = min(max(width, 0.0), float(bound_width) - x)
            height = min(max(height, 0.0), float(bound_height) - y)
        return x, y, max(width, 0.0), max(height, 0.0)
    ax, ay, aw, ah = clip(left)
    bx, by, bw, bh = clip(right)
    intersection_width = max(0.0, min(ax + aw, bx + bw) - max(ax, bx))
    intersection_height = max(0.0, min(ay + ah, by + bh) - max(ay, by))
    intersection = intersection_width * intersection_height
    union = aw * ah + bw * bh - intersection
    return min(max(intersection / (union + sys.float_info.epsilon), 0.0), 1.0)


def got_frame_metrics(
    ground_truth: Sequence[Box],
    predictions: Sequence[Box],
    cover: Sequence[int],
    resolution: Optional[Tuple[int, int]] = None,
) -> Dict[str, object]:
    if not (len(ground_truth) == len(predictions) == len(cover)):
        raise ValueError("GOT-10K metric input lengths differ")
    ious = [
        _bounded_iou(tuple(ground_truth[index]), tuple(predictions[index]), resolution)
        for index in range(1, len(ground_truth)) if int(cover[index]) > 0
    ]
    if not ious:
        raise ValueError("GOT-10K sequence has no visible post-initial frames")
    return {
        "scored_frame_count": len(ious),
        "ious": ious,
        "ao": sum(ious) / float(len(ious)),
        "sr_0_5": sum(value > 0.5 for value in ious) / float(len(ious)),
        "sr_0_75": sum(value > 0.75 for value in ious) / float(len(ious)),
    }


def aggregate_got_metrics(sequence_results: Sequence[Mapping[str, object]]) -> Dict[str, object]:
    if not sequence_results:
        raise ValueError("no GOT-10K sequence results")
    names = set()
    ious = []
    for result in sequence_results:
        name = str(result["name"])
        if name in names:
            raise ValueError("duplicate GOT-10K result {}".format(name))
        names.add(name)
        values = [float(value) for value in result["ious"]]
        if not values or not all(math.isfinite(value) and 0.0 <= value <= 1.0
                                 for value in values):
            raise ValueError("invalid GOT-10K IoUs for {}".format(name))
        ious.extend(values)
    return {
        "sequence_count": len(sequence_results),
        "scored_frame_count": len(ious),
        "ao": sum(ious) / float(len(ious)),
        "sr_0_5": sum(value > 0.5 for value in ious) / float(len(ious)),
        "sr_0_75": sum(value > 0.75 for value in ious) / float(len(ious)),
    }


def load_official_val_panel(dataset_root: Path) -> Tuple[GotEvaluationSequence, ...]:
    val_root = Path(dataset_root).resolve() / "val_data/val"
    names = [line.strip() for line in (val_root / "list.txt").read_text(
        encoding="utf-8").splitlines() if line.strip()]
    if not names:
        raise ValueError("GOT-10K val list is empty")
    return tuple(load_got_sequence(val_root / name) for name in names)


def load_hidden_panel(
    inventory_path: Path, selection: Mapping[str, object]
) -> Tuple[GotEvaluationSequence, ...]:
    if selection.get("selection_final") is not True:
        raise ValueError("selection is not final; GOT train-hidden access denied")
    inventory = json.loads(Path(inventory_path).read_text(encoding="utf-8"))
    names = list(inventory["splits"]["hidden"])
    return tuple(load_got_sequence(Path(inventory["sequences"][name]["path"]))
                 for name in names)


def run_sequence(sequence: GotEvaluationSequence, tracker: object) -> Dict[str, object]:
    predictions = [list(sequence.boxes_xywh[0])]
    first = cv2.imread(sequence.image_paths[0])
    if first is None:
        raise RuntimeError("cannot decode {}".format(sequence.image_paths[0]))
    tracker.init(first, predictions[0])
    for index, path in enumerate(sequence.image_paths[1:], 1):
        image = cv2.imread(path)
        if image is None:
            raise RuntimeError("cannot decode {}".format(path))
        output = tracker.track(image)
        if not isinstance(output, Mapping) or "bbox" not in output:
            raise ValueError("{} frame {} tracker output has no bbox".format(
                sequence.name, index))
        box = tuple(float(value) for value in output["bbox"])
        if len(box) != 4 or not all(math.isfinite(value) for value in box):
            raise ValueError("{} frame {} tracker bbox is invalid".format(
                sequence.name, index))
        predictions.append(list(box))
    metrics = got_frame_metrics(
        sequence.boxes_xywh, predictions, sequence.cover, sequence.resolution
    )
    return {
        "name": sequence.name,
        "root_class": sequence.root_class,
        "frame_count": len(sequence.image_paths),
        "scored_frame_count": metrics["scored_frame_count"],
        "ious": metrics["ious"],
        "ao": metrics["ao"],
        "sr_0_5": metrics["sr_0_5"],
        "sr_0_75": metrics["sr_0_75"],
        "predicted_xywh": predictions,
    }


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with Path(path).open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _atomic_json(path: Path, value: object) -> None:
    output = Path(path).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(
        prefix="." + output.name + ".", suffix=".tmp", dir=str(output.parent))
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as handle:
            json.dump(value, handle, indent=2, sort_keys=True)
            handle.write("\n")
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary, str(output))
    except BaseException:
        try:
            os.unlink(temporary)
        except OSError:
            pass
        raise


def _tracker_factory(nanotrack_root: Path, config: Path, snapshot: Path, device: str):
    root = Path(nanotrack_root).resolve()
    sys.path.insert(0, str(root))
    invocation = os.getcwd()
    try:
        os.chdir(str(root))
        import torch
        from nanotrack.core.config import cfg
        from nanotrack.models.model_builder import ModelBuilder
        from nanotrack.tracker.tracker_builder import build_tracker
        from nanotrack.utils.model_load import load_pretrain
        cfg.merge_from_file(str(Path(config).resolve()))
        if device.startswith("cuda") and not torch.cuda.is_available():
            raise RuntimeError("CUDA is unavailable in this execution environment")
        model = load_pretrain(ModelBuilder(), str(Path(snapshot).resolve()))
        model = model.to(torch.device(device)).eval()
    finally:
        os.chdir(invocation)
    return lambda: build_tracker(model)


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--subset", choices=("val", "train-hidden"), required=True)
    parser.add_argument("--dataset-root", type=Path,
                        default=Path("/home/u20/dataset/GOT-10K"))
    parser.add_argument("--inventory", type=Path)
    parser.add_argument("--selection", type=Path)
    parser.add_argument("--nanotrack-root", type=Path, required=True)
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--snapshot", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--device", default="cuda:0")
    args = parser.parse_args(argv)
    if args.subset == "val":
        panel = load_official_val_panel(args.dataset_root)
    else:
        if args.inventory is None or args.selection is None:
            parser.error("train-hidden requires --inventory and --selection")
        selection = json.loads(args.selection.read_text(encoding="utf-8"))
        panel = load_hidden_panel(args.inventory, selection)
    factory = _tracker_factory(
        args.nanotrack_root, args.config, args.snapshot, args.device)
    results = []
    for index, sequence in enumerate(panel, 1):
        result = run_sequence(sequence, factory())
        results.append(result)
        print("GOT {} {}/{} {} AO={:.6f}".format(
            args.subset, index, len(panel), sequence.name, result["ao"]), flush=True)
    report = {
        "format_version": 1,
        "subset": args.subset,
        "snapshot": str(args.snapshot.resolve()),
        "snapshot_sha256": _sha256(args.snapshot),
        "config": str(args.config.resolve()),
        "config_sha256": _sha256(args.config),
        "metric_semantics": {
            "skip_initial": True,
            "visible_cover_gt": 0,
            "sr_comparator": ">",
            "aggregation": "concatenate visible post-initial frame IoUs",
        },
        "metrics": aggregate_got_metrics(results),
        "sequences": results,
    }
    _atomic_json(args.output, report)
    print(json.dumps(report["metrics"], indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
