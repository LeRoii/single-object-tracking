#!/usr/bin/env python3
"""Validate and decode Rockchip-optimized YOLOv8 detector outputs."""

from __future__ import annotations

from dataclasses import asdict, dataclass
from typing import Any, Sequence

import numpy as np


INPUT_SIZE = 640
STRIDES = (8, 16, 32)
DFL_BINS = 16
CLASS_COUNT = 9


@dataclass(frozen=True)
class ContractReport:
    class_count: int
    dfl_bins: int
    strides: tuple[int, ...]
    roles: tuple[str, ...]
    shapes: tuple[tuple[int, ...], ...]

    def to_dict(self) -> dict[str, Any]:
        return asdict(self)


@dataclass(frozen=True)
class Detection:
    xyxy: tuple[float, float, float, float]
    confidence: float
    class_id: int

    def to_dict(self) -> dict[str, Any]:
        return asdict(self)


@dataclass(frozen=True)
class ParityReport:
    reference_count: int
    candidate_count: int
    matched_count: int
    matched_recall: float
    median_iou: float
    p95_confidence_delta: float
    matched_ious: tuple[float, ...]
    confidence_deltas: tuple[float, ...]

    def to_dict(self) -> dict[str, Any]:
        return asdict(self)


def _static_shape(value_info: Any, index: int) -> tuple[int, ...]:
    tensor_type = value_info.type.tensor_type
    if tensor_type.elem_type == 0:
        raise ValueError(f"output {index} does not declare a tensor type")
    dimensions: list[int] = []
    for dimension in tensor_type.shape.dim:
        if not dimension.HasField("dim_value") or dimension.dim_value <= 0:
            raise ValueError(
                f"output {index} must have static integer dimensions"
            )
        dimensions.append(int(dimension.dim_value))
    if len(dimensions) != 4:
        raise ValueError(f"output {index} must be rank-4 NCHW")
    return tuple(dimensions)


def validate_output_contract(model: Any) -> ContractReport:
    """Return the fixed nine-output RKOPT contract or raise ``ValueError``."""

    outputs = list(model.graph.output)
    if len(outputs) != 9:
        raise ValueError("RKOPT detector must expose exactly nine outputs")

    shapes = tuple(_static_shape(value, index) for index, value in enumerate(outputs))
    roles: list[str] = []
    class_counts: set[int] = set()
    for scale_index, (stride, grid) in enumerate(zip(STRIDES, (80, 40, 20))):
        expected = (
            (1, 4 * DFL_BINS, grid, grid),
            (1, CLASS_COUNT, grid, grid),
            (1, 1, grid, grid),
        )
        scale_roles = (f"box_s{stride}", f"class_s{stride}", f"score_sum_s{stride}")
        for role_index, (wanted_shape, role) in enumerate(zip(expected, scale_roles)):
            output_index = scale_index * 3 + role_index
            actual_shape = shapes[output_index]
            if role_index == 1:
                class_counts.add(actual_shape[1])
                if (
                    actual_shape[0] == 1
                    and actual_shape[2:] == (grid, grid)
                    and actual_shape[1] != CLASS_COUNT
                ):
                    raise ValueError(f"class count must be {CLASS_COUNT}")
            if actual_shape != wanted_shape:
                raise ValueError(
                    f"output {output_index} ({role}) shape {actual_shape} "
                    f"does not match {wanted_shape}"
                )
            roles.append(role)
    if class_counts != {CLASS_COUNT}:
        raise ValueError(f"class count must be {CLASS_COUNT}")

    return ContractReport(CLASS_COUNT, DFL_BINS, STRIDES, tuple(roles), shapes)


def validate_runtime_outputs(outputs: Sequence[np.ndarray]) -> list[np.ndarray]:
    if len(outputs) != 9:
        raise ValueError("RKOPT detector must return exactly nine outputs")
    arrays = [np.asarray(output) for output in outputs]
    expected_shapes = []
    for grid in (80, 40, 20):
        expected_shapes.extend(
            ((1, 64, grid, grid), (1, CLASS_COUNT, grid, grid), (1, 1, grid, grid))
        )
    for index, (array, expected) in enumerate(zip(arrays, expected_shapes)):
        if array.shape != expected:
            raise ValueError(f"output {index} shape {array.shape} does not match {expected}")
        if not np.issubdtype(array.dtype, np.floating):
            raise ValueError(f"output {index} must contain floating-point values")
        if not np.isfinite(array).all():
            raise ValueError(f"output {index} values must be finite")
    return arrays


def _dfl_distance(box: np.ndarray, side: int, y: int, x: int) -> float:
    logits = box[0, side * DFL_BINS : (side + 1) * DFL_BINS, y, x].astype(
        np.float64
    )
    logits -= logits.max()
    probabilities = np.exp(logits)
    probabilities /= probabilities.sum()
    return float(probabilities @ np.arange(DFL_BINS, dtype=np.float64))


def _iou(first: Detection, second: Detection) -> float:
    left = max(first.xyxy[0], second.xyxy[0])
    top = max(first.xyxy[1], second.xyxy[1])
    right = min(first.xyxy[2], second.xyxy[2])
    bottom = min(first.xyxy[3], second.xyxy[3])
    intersection = max(0.0, right - left) * max(0.0, bottom - top)
    first_area = max(0.0, first.xyxy[2] - first.xyxy[0]) * max(
        0.0, first.xyxy[3] - first.xyxy[1]
    )
    second_area = max(0.0, second.xyxy[2] - second.xyxy[0]) * max(
        0.0, second.xyxy[3] - second.xyxy[1]
    )
    union = first_area + second_area - intersection
    return intersection / union if union > 0.0 else 0.0


def decode_rkopt_outputs(
    outputs: Sequence[np.ndarray],
    conf: float,
    nms: float,
    *,
    max_detections: int = 300,
) -> list[Detection]:
    """Decode fixed 640x640 RKOPT outputs in model-input coordinates."""

    if not np.isfinite(conf) or not 0.0 < conf < 1.0:
        raise ValueError("conf must be finite and in (0,1)")
    if not np.isfinite(nms) or not 0.0 < nms < 1.0:
        raise ValueError("nms must be finite and in (0,1)")
    if max_detections <= 0:
        raise ValueError("max_detections must be positive")
    arrays = validate_runtime_outputs(outputs)

    candidates: list[Detection] = []
    for scale_index, stride in enumerate(STRIDES):
        box, classes, score_sum = arrays[scale_index * 3 : scale_index * 3 + 3]
        if (score_sum < 0.0).any():
            raise ValueError("score-sum values must be non-negative")
        if (classes < 0.0).any() or (classes > 1.0).any():
            raise ValueError("class confidence values must be in [0,1]")
        mask = score_sum[0, 0] >= conf
        for y, x in np.argwhere(mask):
            class_id = int(np.argmax(classes[0, :, y, x]))
            confidence = float(classes[0, class_id, y, x])
            if confidence < conf:
                continue
            left, top, right, bottom = (
                _dfl_distance(box, side, int(y), int(x)) for side in range(4)
            )
            center_x = (float(x) + 0.5) * stride
            center_y = (float(y) + 0.5) * stride
            candidates.append(
                Detection(
                    (
                        center_x - left * stride,
                        center_y - top * stride,
                        center_x + right * stride,
                        center_y + bottom * stride,
                    ),
                    confidence,
                    class_id,
                )
            )

    candidates.sort(key=lambda item: item.confidence, reverse=True)
    kept: list[Detection] = []
    for candidate in candidates:
        if any(
            candidate.class_id == accepted.class_id and _iou(candidate, accepted) > nms
            for accepted in kept
        ):
            continue
        kept.append(candidate)
        if len(kept) == max_detections:
            break
    return kept


def compare_detections(
    reference: Sequence[Detection],
    candidate: Sequence[Detection],
    min_iou: float,
) -> ParityReport:
    """Greedily match detections of the same class by descending reference score."""

    if not np.isfinite(min_iou) or not 0.0 < min_iou <= 1.0:
        raise ValueError("min_iou must be finite and in (0,1]")
    available = set(range(len(candidate)))
    matched_ious: list[float] = []
    confidence_deltas: list[float] = []
    ordered_reference = sorted(reference, key=lambda item: item.confidence, reverse=True)
    for expected in ordered_reference:
        compatible = [
            (index, _iou(expected, candidate[index]))
            for index in available
            if candidate[index].class_id == expected.class_id
        ]
        compatible = [item for item in compatible if item[1] >= min_iou]
        if not compatible:
            continue
        selected, overlap = max(compatible, key=lambda item: item[1])
        available.remove(selected)
        matched_ious.append(overlap)
        confidence_deltas.append(
            abs(expected.confidence - candidate[selected].confidence)
        )

    matched_count = len(matched_ious)
    recall = matched_count / len(reference) if reference else 1.0
    median_iou = float(np.median(matched_ious)) if matched_ious else 0.0
    p95_delta = (
        float(np.percentile(confidence_deltas, 95)) if confidence_deltas else 0.0
    )
    return ParityReport(
        len(reference),
        len(candidate),
        matched_count,
        recall,
        median_iou,
        p95_delta,
        tuple(matched_ious),
        tuple(confidence_deltas),
    )
