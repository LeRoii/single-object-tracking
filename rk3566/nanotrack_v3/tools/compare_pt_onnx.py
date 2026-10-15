#!/usr/bin/env python3
"""Compare Rockchip-optimized PyTorch and ONNX outputs on fixed images."""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from copy import deepcopy
from pathlib import Path
from typing import Any

import cv2
import numpy as np

from rkopt_validation import compare_detections, decode_rkopt_outputs


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def letterbox_tensor(path: Path, size: int = 640) -> tuple[np.ndarray, dict[str, Any]]:
    image = cv2.imread(str(path), cv2.IMREAD_COLOR)
    if image is None:
        raise ValueError(f"cannot decode image: {path}")
    height, width = image.shape[:2]
    scale = min(size / height, size / width)
    resized_width = int(round(width * scale))
    resized_height = int(round(height * scale))
    if (resized_width, resized_height) != (width, height):
        image = cv2.resize(
            image, (resized_width, resized_height), interpolation=cv2.INTER_LINEAR
        )
    horizontal = (size - resized_width) / 2.0
    vertical = (size - resized_height) / 2.0
    left, right = int(round(horizontal - 0.1)), int(round(horizontal + 0.1))
    top, bottom = int(round(vertical - 0.1)), int(round(vertical + 0.1))
    image = cv2.copyMakeBorder(
        image, top, bottom, left, right, cv2.BORDER_CONSTANT, value=(114, 114, 114)
    )
    if image.shape[:2] != (size, size):
        raise ValueError(f"letterbox produced unexpected shape {image.shape}")
    rgb = cv2.cvtColor(image, cv2.COLOR_BGR2RGB)
    tensor = np.ascontiguousarray(rgb.transpose(2, 0, 1)[None], dtype=np.float32)
    tensor /= 255.0
    return tensor, {
        "original_width": width,
        "original_height": height,
        "scale": scale,
        "pad_left": left,
        "pad_top": top,
    }


def _load_samples(path: Path, expected: int) -> list[Path]:
    lines = [line.strip() for line in path.read_text(encoding="utf-8").splitlines()]
    if any(not line for line in lines):
        raise ValueError("sample manifest contains an empty line")
    samples = [Path(line).expanduser().resolve() for line in lines]
    if len(samples) != expected:
        raise ValueError(f"sample manifest must contain exactly {expected} images")
    if len(set(samples)) != len(samples):
        raise ValueError("sample manifest contains duplicate images")
    missing = [str(item) for item in samples if not item.is_file()]
    if missing:
        raise ValueError(f"sample image does not exist: {missing[0]}")
    return samples


def _load_pt_model(repo: Path, checkpoint: Path):
    repo = repo.resolve()
    sys.path.insert(0, str(repo))
    import torch
    import ultralytics
    from ultralytics import YOLO
    from ultralytics.nn.modules.head import Detect

    package_path = Path(ultralytics.__file__).resolve()
    if repo not in package_path.parents:
        raise ValueError(f"ultralytics imported from {package_path}, expected {repo}")
    wrapped = YOLO(str(checkpoint))
    if list(wrapped.names.values()) != [
        "person",
        "bicycle",
        "car",
        "van",
        "truck",
        "tricycle",
        "awning-tricycle",
        "bus",
        "motor",
    ]:
        raise ValueError("PT checkpoint does not contain the expected VisDrone9 names")
    model = deepcopy(wrapped.model).cpu().eval().float().fuse()
    for module in model.modules():
        if isinstance(module, Detect):
            module.dynamic = False
            module.export = True
            module.format = "rknn"
    return torch, model


def compare(arguments: argparse.Namespace) -> dict[str, Any]:
    import onnxruntime as ort

    samples = _load_samples(arguments.samples, arguments.expected_samples)
    torch, model = _load_pt_model(arguments.repo, arguments.pt)
    session = ort.InferenceSession(
        str(arguments.onnx.resolve()), providers=["CPUExecutionProvider"]
    )
    if len(session.get_inputs()) != 1:
        raise ValueError("ONNX session must expose exactly one input")
    input_name = session.get_inputs()[0].name

    sample_reports = []
    all_ious: list[float] = []
    all_deltas: list[float] = []
    reference_total = 0
    candidate_total = 0
    matched_total = 0
    for path in samples:
        array, transform = letterbox_tensor(path)
        with torch.no_grad():
            raw_pt = model(torch.from_numpy(array))
        if not isinstance(raw_pt, (list, tuple)):
            raise ValueError("PT RKOPT model did not return a tensor sequence")
        pt_outputs = [item.detach().cpu().numpy() for item in raw_pt]
        onnx_outputs = session.run(None, {input_name: array})
        pt_detections = decode_rkopt_outputs(
            pt_outputs, arguments.conf, arguments.nms
        )
        onnx_detections = decode_rkopt_outputs(
            onnx_outputs, arguments.conf, arguments.nms
        )
        reference = [
            detection
            for detection in pt_detections
            if detection.confidence >= arguments.reference_conf
        ]
        parity = compare_detections(reference, onnx_detections, arguments.min_iou)
        reference_total += parity.reference_count
        candidate_total += parity.candidate_count
        matched_total += parity.matched_count
        all_ious.extend(parity.matched_ious)
        all_deltas.extend(parity.confidence_deltas)
        sample_reports.append(
            {
                "image": str(path),
                "transform": transform,
                "pt_reference_count": parity.reference_count,
                "onnx_candidate_count": parity.candidate_count,
                "matched_count": parity.matched_count,
                "matched_recall": parity.matched_recall,
                "median_iou": parity.median_iou,
                "p95_confidence_delta": parity.p95_confidence_delta,
            }
        )

    recall = matched_total / reference_total if reference_total else 1.0
    median_iou = float(np.median(all_ious)) if all_ious else 0.0
    p95_delta = float(np.percentile(all_deltas, 95)) if all_deltas else 0.0
    gates = {
        "matched_recall": recall >= arguments.required_recall,
        "median_iou": median_iou >= arguments.required_median_iou,
        "p95_confidence_delta": p95_delta <= arguments.max_p95_conf_delta,
    }
    return {
        "status": "pass" if all(gates.values()) else "fail",
        "pt": {
            "path": str(arguments.pt.resolve()),
            "sha256": sha256_file(arguments.pt),
        },
        "onnx": {
            "path": str(arguments.onnx.resolve()),
            "sha256": sha256_file(arguments.onnx),
            "provider": session.get_providers()[0],
        },
        "samples": {
            "path": str(arguments.samples.resolve()),
            "sha256": sha256_file(arguments.samples),
            "count": len(samples),
        },
        "settings": {
            "input_size": 640,
            "confidence": arguments.conf,
            "reference_confidence": arguments.reference_conf,
            "nms": arguments.nms,
            "minimum_match_iou": arguments.min_iou,
        },
        "thresholds": {
            "minimum_matched_recall": arguments.required_recall,
            "minimum_median_iou": arguments.required_median_iou,
            "maximum_p95_confidence_delta": arguments.max_p95_conf_delta,
        },
        "gates": gates,
        "summary": {
            "pt_reference_count": reference_total,
            "onnx_candidate_count": candidate_total,
            "matched_count": matched_total,
            "matched_recall": recall,
            "median_iou": median_iou,
            "p95_confidence_delta": p95_delta,
        },
        "per_sample": sample_reports,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", required=True, type=Path)
    parser.add_argument("--pt", required=True, type=Path)
    parser.add_argument("--onnx", required=True, type=Path)
    parser.add_argument("--samples", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--expected-samples", type=int, default=50)
    parser.add_argument("--conf", type=float, default=0.25)
    parser.add_argument("--reference-conf", type=float, default=0.35)
    parser.add_argument("--nms", type=float, default=0.45)
    parser.add_argument("--min-iou", type=float, default=0.90)
    parser.add_argument("--required-recall", type=float, default=0.98)
    parser.add_argument("--required-median-iou", type=float, default=0.99)
    parser.add_argument("--max-p95-conf-delta", type=float, default=0.02)
    arguments = parser.parse_args()
    try:
        report = compare(arguments)
        arguments.output.parent.mkdir(parents=True, exist_ok=True)
        arguments.output.write_text(
            json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
    except (OSError, RuntimeError, ValueError) as error:
        parser.error(str(error))
    print(json.dumps(report["summary"], indent=2, sort_keys=True))
    if report["status"] != "pass":
        print(f"parity gates failed; see {arguments.output}", file=sys.stderr)
        return 1
    print(f"parity gates passed; report: {arguments.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
