#!/usr/bin/env python3
"""Evaluate NanoTrack on a fixed consecutive aerial-tracklet panel."""

import argparse
import hashlib
import json
import math
import os
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Dict, List, Mapping, Optional, Sequence, Tuple

import cv2


WORKSPACE_ROOT = Path(__file__).resolve().parents[1]
if str(WORKSPACE_ROOT) not in sys.path:
    sys.path.insert(0, str(WORKSPACE_ROOT))

from scripts.evaluate_uav123_nanotrack import evaluate_predictions
from scripts.nanotrack_three_stage_data import AerialSequenceSpec


Box = Tuple[float, float, float, float]
DEFAULT_SCORE_WEIGHTS = {
    "person_auc": 0.4,
    "vehicle_auc": 0.4,
    "overall_auc": 0.2,
}
OPE_METADATA = {
    "threshold_start": 0.0,
    "threshold_end": 1.0,
    "threshold_step": 0.05,
    "thresholds": [index / 20.0 for index in range(21)],
    "comparator": ">",
    "auc_reduction": "mean",
    "perfect_endpoint_auc": 20.0 / 21.0,
}


@dataclass(frozen=True)
class AerialPanel:
    split: str
    sequences: Tuple[AerialSequenceSpec, ...]
    sha256: str
    path: str


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with Path(path).open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _box(value: object, context: str) -> Optional[Box]:
    if value is None:
        return None
    if not isinstance(value, (list, tuple)) or len(value) != 4:
        raise ValueError("{} box must contain four numbers".format(context))
    try:
        box = tuple(float(item) for item in value)
    except (TypeError, ValueError):
        raise ValueError("{} box must contain four numbers".format(context))
    if not all(math.isfinite(item) for item in box):
        raise ValueError("{} box must be finite".format(context))
    if box[2] <= 0.0 or box[3] <= 0.0:
        raise ValueError("{} box width and height must be positive".format(context))
    return box  # type: ignore


def load_panel(path: Path, expected_sha256: Optional[str] = None) -> AerialPanel:
    panel_path = Path(path).resolve()
    try:
        payload = panel_path.read_bytes()
    except OSError as error:
        raise ValueError("cannot read panel {}: {}".format(panel_path, error))
    actual_hash = hashlib.sha256(payload).hexdigest()
    if expected_sha256 is not None and actual_hash.lower() != expected_sha256.lower():
        raise ValueError(
            "panel hash mismatch: expected {}, got {}".format(expected_sha256, actual_hash)
        )
    try:
        value = json.loads(payload.decode("utf-8"))
    except (UnicodeDecodeError, ValueError) as error:
        raise ValueError("cannot read panel {}: {}".format(panel_path, error))
    if (not isinstance(value, dict) or not isinstance(value.get("split"), str)
            or not value["split"].strip()):
        raise ValueError("panel must contain a nonempty string split")
    raw_sequences = value.get("sequences")
    if not isinstance(raw_sequences, list) or not raw_sequences:
        raise ValueError("panel has no sequences")

    sequences = []
    names = set()
    scoreable = 0
    for sequence_index, raw in enumerate(raw_sequences):
        if not isinstance(raw, dict):
            raise ValueError("sequence {} must be an object".format(sequence_index))
        name = raw.get("name")
        if not isinstance(name, str) or not name:
            raise ValueError("sequence {} has no valid name".format(sequence_index))
        if name in names:
            raise ValueError("duplicate sequence name {}".format(name))
        names.add(name)
        image_paths = raw.get("image_paths")
        boxes = raw.get("boxes_xywh")
        if not isinstance(image_paths, list) or not image_paths:
            raise ValueError("{} has no image paths".format(name))
        if not all(isinstance(item, str) and item for item in image_paths):
            raise ValueError("{} has a malformed image path".format(name))
        if not isinstance(boxes, list) or len(boxes) != len(image_paths):
            raise ValueError("{} image/box lengths differ".format(name))
        parsed_boxes = tuple(
            _box(value, "{} frame {}".format(name, index))
            for index, value in enumerate(boxes)
        )
        scoreable += sum(box is not None for box in parsed_boxes)
        resolved_paths = tuple(
            str((panel_path.parent / item).resolve()) if not Path(item).is_absolute() else item
            for item in image_paths
        )
        try:
            source = raw["source"]
            source_video = raw["source_video"]
            category = raw["category"]
            window_start = int(raw["window_start"])
            window_end = int(raw["window_end"])
        except (KeyError, TypeError, ValueError) as error:
            raise ValueError("{} has malformed metadata: {}".format(name, error))
        if not all(isinstance(item, str) and item for item in (source, source_video, category)):
            raise ValueError("{} has malformed source/category metadata".format(name))
        if window_end - window_start + 1 != len(image_paths):
            raise ValueError("{} frame window is not consecutive".format(name))
        sequences.append(
            AerialSequenceSpec(
                name=name,
                source=source,
                source_video=source_video,
                category=category,
                image_paths=resolved_paths,
                boxes_xywh=parsed_boxes,
                window_start=window_start,
                window_end=window_end,
            )
        )
    members = value.get("members")
    if members is not None and members != [sequence.name for sequence in sequences]:
        raise ValueError("panel members do not match sequence names")
    if scoreable == 0:
        raise ValueError("panel has no scoreable frames")
    return AerialPanel(value["split"], tuple(sequences), actual_hash, str(panel_path))


def ope_success_auc(
    ground_truth: Sequence[Optional[Box]], predictions: Sequence[Optional[Box]]
) -> float:
    if len(ground_truth) != len(predictions):
        raise ValueError("ground-truth and prediction lengths differ")
    scored_gt = []
    scored_predictions = []
    for index, (target, prediction) in enumerate(zip(ground_truth, predictions)):
        if target is None:
            continue
        if prediction is None:
            raise ValueError("scoreable frame {} has no prediction".format(index))
        scored_gt.append(_box(target, "ground truth frame {}".format(index)))
        scored_predictions.append(_box(prediction, "prediction frame {}".format(index)))
    if not scored_gt:
        raise ValueError("sequence has no scoreable frames")
    return float(evaluate_predictions(scored_gt, scored_predictions)["success_auc"])


def run_sequence(spec: AerialSequenceSpec, tracker: object) -> Dict[str, object]:
    if len(spec.image_paths) != len(spec.boxes_xywh) or not spec.image_paths:
        raise ValueError("{} image/box lengths differ".format(spec.name))
    predictions = []  # type: List[Optional[List[float]]]
    initialized = False
    for index, (image_text, target) in enumerate(zip(spec.image_paths, spec.boxes_xywh)):
        image = cv2.imread(str(image_text))
        if image is None:
            raise RuntimeError("cannot decode {}".format(image_text))
        if not initialized:
            if target is None:
                predictions.append(None)
                continue
            prediction_box = _box(target, "{} initialization".format(spec.name))
            prediction = [float(value) for value in prediction_box]  # type: ignore
            tracker.init(image, prediction)
            initialized = True
        else:
            output = tracker.track(image)
            if not isinstance(output, dict) or "bbox" not in output:
                raise ValueError("{} frame {} tracker output has no bbox".format(spec.name, index))
            parsed = _box(output["bbox"], "{} prediction frame {}".format(spec.name, index))
            prediction = [float(value) for value in parsed]  # type: ignore
        predictions.append(prediction)
    if not initialized:
        raise ValueError("{} has no scoreable frames".format(spec.name))
    mask = [target is not None and prediction is not None
            for target, prediction in zip(spec.boxes_xywh, predictions)]
    auc = ope_success_auc(spec.boxes_xywh, predictions)
    return {
        "name": spec.name,
        "source": spec.source,
        "source_video": spec.source_video,
        "category": spec.category,
        "window_start": spec.window_start,
        "window_end": spec.window_end,
        "frame_count": len(spec.image_paths),
        "predicted_xywh": predictions,
        "scored_frame_mask": mask,
        "scored_frame_count": int(sum(mask)),
        "sequence_auc": auc,
    }


def renormalized_weighted_score(
    metrics: Mapping[str, Optional[float]], weights: Mapping[str, float]
) -> float:
    weighted = 0.0
    present_weight = 0.0
    for key, weight_value in weights.items():
        weight = float(weight_value)
        if not math.isfinite(weight) or weight < 0.0:
            raise ValueError("weight {} must be finite and non-negative".format(key))
        value = metrics.get(key)
        if value is None:
            continue
        number = float(value)
        if not math.isfinite(number):
            raise ValueError("metric {} must be finite".format(key))
        weighted += weight * number
        present_weight += weight
    if present_weight <= 0.0:
        raise ValueError("no weighted metrics are present")
    return weighted / present_weight


def aggregate_domain_category_metrics(
    results: Sequence[Mapping[str, object]],
    score_weights: Optional[Mapping[str, float]] = None,
) -> Dict[str, object]:
    if not results:
        raise ValueError("no sequence results")
    weights = dict(DEFAULT_SCORE_WEIGHTS if score_weights is None else score_weights)
    grouped = {}  # type: Dict[str, List[Mapping[str, object]]]
    names = set()
    for result in results:
        name = str(result["name"])
        if name in names:
            raise ValueError("duplicate sequence result {}".format(name))
        names.add(name)
        auc = float(result["sequence_auc"])
        if not math.isfinite(auc) or auc < 0.0 or auc > 1.0:
            raise ValueError("{} has invalid sequence AUC".format(name))
        grouped.setdefault(str(result["source"]), []).append(result)

    sources = {}
    for source in sorted(grouped):
        source_results = grouped[source]
        categories = {}
        for category in sorted({str(result["category"]) for result in source_results}):
            values = [float(result["sequence_auc"]) for result in source_results
                      if str(result["category"]) == category]
            categories[category] = sum(values) / float(len(values))
        source_summary = {
            "sequence_count": len(source_results),
            "scored_frame_count": int(sum(int(result["scored_frame_count"])
                                          for result in source_results)),
            "categories": categories,
            "person_auc": categories.get("person"),
            "vehicle_auc": categories.get("vehicle"),
            "overall_auc": sum(float(result["sequence_auc"]) for result in source_results)
                           / float(len(source_results)),
        }
        source_summary["controller_score"] = renormalized_weighted_score(
            source_summary, weights
        )
        sources[source] = source_summary

    macro = {}
    for key in ("person_auc", "vehicle_auc", "overall_auc"):
        present = [summary[key] for summary in sources.values() if summary[key] is not None]
        macro[key] = sum(present) / float(len(present)) if present else None
    macro["controller_score"] = sum(
        float(summary["controller_score"]) for summary in sources.values()
    ) / float(len(sources))
    macro["source_count"] = len(sources)
    return {"sources": sources, "macro": macro}


def evaluate_panel(
    panel: AerialPanel,
    tracker_factory: Callable[[], object],
    config_path: Path,
    snapshot_path: Path,
    device: str,
    config_sha256: Optional[str] = None,
    snapshot_sha256: Optional[str] = None,
) -> Dict[str, object]:
    config = Path(config_path).resolve()
    snapshot = Path(snapshot_path).resolve()
    config_hash = sha256_file(config) if config_sha256 is None else config_sha256
    snapshot_hash = sha256_file(snapshot) if snapshot_sha256 is None else snapshot_sha256
    sequence_results = [run_sequence(spec, tracker_factory()) for spec in panel.sequences]
    summary = aggregate_domain_category_metrics(sequence_results)
    return {
        "format_version": 1,
        "panel": panel.path,
        "panel_split": panel.split,
        "panel_sha256": panel.sha256,
        "config": str(config),
        "config_sha256": config_hash,
        "snapshot": str(snapshot),
        "snapshot_sha256": snapshot_hash,
        "device": device,
        "ope": dict(OPE_METADATA),
        "sequences": sequence_results,
        "metrics": summary,
    }


def atomic_write_json(path: Path, value: object) -> None:
    output = Path(path)
    output.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(
        prefix="." + output.name + ".", suffix=".tmp", dir=str(output.parent)
    )
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
    config_path = Path(config).resolve()
    snapshot_path = Path(snapshot).resolve()
    sys.path.insert(0, str(root))
    invocation_cwd = os.getcwd()
    try:
        os.chdir(str(root))
        import torch
        from nanotrack.core.config import cfg
        from nanotrack.models.model_builder import ModelBuilder
        from nanotrack.tracker.tracker_builder import build_tracker
        from nanotrack.utils.model_load import load_pretrain

        cfg.merge_from_file(str(config_path))
        model = load_pretrain(ModelBuilder(), str(snapshot_path))
        if device.startswith("cuda") and not torch.cuda.is_available():
            raise RuntimeError("CUDA is unavailable in this execution environment")
        model = model.to(torch.device(device)).eval()
    finally:
        os.chdir(invocation_cwd)
    return lambda: build_tracker(model)


def main(argv: Optional[Sequence[str]] = None) -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--panel", type=Path, required=True)
    parser.add_argument("--nanotrack-root", type=Path, required=True)
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--snapshot", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--device", default="cuda:0")
    parser.add_argument("--expected-panel-sha256")
    args = parser.parse_args(argv)

    invocation_root = Path.cwd()

    def invocation_path(value: Path) -> Path:
        path = Path(value)
        return path.resolve() if path.is_absolute() else (invocation_root / path).resolve()

    panel_path = invocation_path(args.panel)
    nanotrack_root = invocation_path(args.nanotrack_root)
    config_path = invocation_path(args.config)
    snapshot_path = invocation_path(args.snapshot)
    output_path = invocation_path(args.output)
    panel = load_panel(panel_path, args.expected_panel_sha256)
    config_hash = sha256_file(config_path)
    snapshot_hash = sha256_file(snapshot_path)
    factory = _tracker_factory(
        nanotrack_root, config_path, snapshot_path, args.device
    )
    result = evaluate_panel(
        panel, factory, config_path, snapshot_path, args.device,
        config_sha256=config_hash, snapshot_sha256=snapshot_hash,
    )
    atomic_write_json(output_path, result)
    print(json.dumps(result["metrics"], indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
