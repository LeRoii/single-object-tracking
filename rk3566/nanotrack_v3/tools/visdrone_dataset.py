#!/usr/bin/env python3
"""Convert and independently audit VisDrone DET annotations for YOLOv8."""

from __future__ import annotations

import json
import math
import os
import struct
from collections import Counter
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Sequence, Union


CLASS_NAMES = (
    "person",
    "bicycle",
    "car",
    "van",
    "truck",
    "tricycle",
    "awning-tricycle",
    "bus",
    "motor",
)

CATEGORY_TO_CLASS = {
    1: 0,
    2: 0,
    3: 1,
    4: 2,
    5: 3,
    6: 4,
    7: 5,
    8: 6,
    9: 7,
    10: 8,
}

SPLIT_DIRECTORIES = {
    "train": "VisDrone2019-DET-train",
    "val": "VisDrone2019-DET-val",
}

IMAGE_SUFFIXES = {".jpg", ".jpeg", ".png", ".bmp"}


@dataclass(frozen=True)
class RawAnnotation:
    x: float
    y: float
    width: float
    height: float
    score: float
    category: int
    truncation: int
    occlusion: int


@dataclass(frozen=True)
class ConvertedBox:
    class_id: int
    center_x: float
    center_y: float
    width: float
    height: float
    truncation: int
    occlusion: int
    clipped: bool


@dataclass(frozen=True)
class SkipReason:
    reason: str


BoxResult = Union[ConvertedBox, SkipReason]


def parse_annotation(line: str) -> RawAnnotation:
    fields = [field.strip() for field in line.strip().split(",")]
    if len(fields) != 8:
        raise ValueError("annotation must contain exactly eight comma-separated fields")
    try:
        geometry_and_score = tuple(float(field) for field in fields[:5])
        category, truncation, occlusion = (int(field) for field in fields[5:])
    except ValueError as error:
        raise ValueError("annotation contains a non-numeric field") from error
    if not all(math.isfinite(value) for value in geometry_and_score):
        raise ValueError("annotation values must be finite")
    return RawAnnotation(
        *geometry_and_score,
        category=category,
        truncation=truncation,
        occlusion=occlusion,
    )


def convert_box(
    annotation: RawAnnotation, image_width: int, image_height: int
) -> BoxResult:
    if image_width <= 0 or image_height <= 0:
        raise ValueError("image dimensions must be positive")
    if annotation.category in (0, 11):
        return SkipReason("ignored_category")
    class_id = CATEGORY_TO_CLASS.get(annotation.category)
    if class_id is None:
        return SkipReason("unsupported_category")
    if annotation.score <= 0:
        return SkipReason("non_positive_score")
    if annotation.width <= 0 or annotation.height <= 0:
        return SkipReason("non_positive_extent")

    original = (
        annotation.x,
        annotation.y,
        annotation.x + annotation.width,
        annotation.y + annotation.height,
    )
    left = min(float(image_width), max(0.0, original[0]))
    top = min(float(image_height), max(0.0, original[1]))
    right = min(float(image_width), max(0.0, original[2]))
    bottom = min(float(image_height), max(0.0, original[3]))
    if right <= left or bottom <= top:
        return SkipReason("outside_image")

    width = right - left
    height = bottom - top
    return ConvertedBox(
        class_id=class_id,
        center_x=((left + right) * 0.5) / image_width,
        center_y=((top + bottom) * 0.5) / image_height,
        width=width / image_width,
        height=height / image_height,
        truncation=annotation.truncation,
        occlusion=annotation.occlusion,
        clipped=(left, top, right, bottom) != original,
    )


def _jpeg_size(path: Path) -> tuple[int, int]:
    sof_markers = {
        0xC0,
        0xC1,
        0xC2,
        0xC3,
        0xC5,
        0xC6,
        0xC7,
        0xC9,
        0xCA,
        0xCB,
        0xCD,
        0xCE,
        0xCF,
    }
    with path.open("rb") as stream:
        if stream.read(2) != b"\xff\xd8":
            raise ValueError("JPEG SOI marker is missing")
        while True:
            byte = stream.read(1)
            while byte and byte != b"\xff":
                byte = stream.read(1)
            if not byte:
                break
            marker_byte = stream.read(1)
            while marker_byte == b"\xff":
                marker_byte = stream.read(1)
            if not marker_byte:
                break
            marker = marker_byte[0]
            if marker in (0xD8, 0xD9) or 0xD0 <= marker <= 0xD7:
                continue
            length_bytes = stream.read(2)
            if len(length_bytes) != 2:
                break
            segment_length = struct.unpack(">H", length_bytes)[0]
            if segment_length < 2:
                raise ValueError("JPEG segment length is invalid")
            if marker in sof_markers:
                payload = stream.read(segment_length - 2)
                if len(payload) < 5:
                    break
                height, width = struct.unpack(">HH", payload[1:5])
                if width <= 0 or height <= 0:
                    raise ValueError("JPEG dimensions must be positive")
                return width, height
            stream.seek(segment_length - 2, os.SEEK_CUR)
    raise ValueError("JPEG SOF marker is missing")


def read_image_size(path: Path) -> tuple[int, int]:
    with path.open("rb") as stream:
        header = stream.read(24)
    if header.startswith(b"\x89PNG\r\n\x1a\n") and len(header) >= 24:
        width, height = struct.unpack(">II", header[16:24])
        if width <= 0 or height <= 0:
            raise ValueError("PNG dimensions must be positive")
        return width, height
    if header.startswith(b"\xff\xd8"):
        return _jpeg_size(path)
    raise ValueError(f"unsupported image format: {path}")


def _write_text_atomic(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(text, encoding="utf-8")
    temporary.replace(path)


def write_json_atomic(path: Path, value: dict[str, Any]) -> None:
    _write_text_atomic(path, json.dumps(value, indent=2, sort_keys=True) + "\n")


def _relative_symlink(source: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    target = os.path.relpath(source.resolve(), destination.parent.resolve())
    if destination.is_symlink():
        if os.readlink(destination) == target:
            return
        raise ValueError(f"image link already points elsewhere: {destination}")
    if destination.exists():
        raise ValueError(f"image destination already exists: {destination}")
    destination.symlink_to(target)


def _format_box(box: ConvertedBox) -> str:
    return (
        f"{box.class_id} {box.center_x:.8f} {box.center_y:.8f} "
        f"{box.width:.8f} {box.height:.8f}"
    )


def convert_dataset(
    source_root: Path, output_root: Path, image_mode: str = "symlink"
) -> dict[str, Any]:
    source_root = Path(source_root)
    output_root = Path(output_root)
    if image_mode != "symlink":
        raise ValueError("image_mode must be symlink")

    total_classes: Counter[int] = Counter()
    total_skips: Counter[str] = Counter()
    total_converted = 0
    total_clipped = 0
    errors: list[dict[str, str]] = []
    split_reports: dict[str, Any] = {}

    for split, directory_name in SPLIT_DIRECTORIES.items():
        split_root = source_root / directory_name
        source_images = split_root / "images"
        source_annotations = split_root / "annotations"
        images = sorted(
            path
            for path in source_images.iterdir()
            if path.is_file() and path.suffix.lower() in IMAGE_SUFFIXES
        )
        split_classes: Counter[int] = Counter()
        split_skips: Counter[str] = Counter()
        split_converted = 0
        split_clipped = 0

        for image in images:
            annotation_path = source_annotations / f"{image.stem}.txt"
            destination_image = output_root / "images" / split / image.name
            destination_label = output_root / "labels" / split / f"{image.stem}.txt"
            try:
                if not annotation_path.is_file():
                    raise ValueError("source annotation is missing")
                width, height = read_image_size(image)
                converted_lines: list[str] = []
                for line_number, line in enumerate(
                    annotation_path.read_text(encoding="utf-8").splitlines(), start=1
                ):
                    if not line.strip():
                        continue
                    try:
                        result = convert_box(parse_annotation(line), width, height)
                    except ValueError as error:
                        reason = "malformed_annotation"
                        split_skips[reason] += 1
                        total_skips[reason] += 1
                        errors.append(
                            {
                                "path": str(annotation_path),
                                "line": str(line_number),
                                "reason": reason,
                                "detail": str(error),
                            }
                        )
                        continue
                    if isinstance(result, SkipReason):
                        split_skips[result.reason] += 1
                        total_skips[result.reason] += 1
                        continue
                    converted_lines.append(_format_box(result))
                    split_classes[result.class_id] += 1
                    total_classes[result.class_id] += 1
                    split_converted += 1
                    total_converted += 1
                    if result.clipped:
                        split_clipped += 1
                        total_clipped += 1
                _relative_symlink(image, destination_image)
                _write_text_atomic(
                    destination_label,
                    "\n".join(converted_lines) + ("\n" if converted_lines else ""),
                )
            except (OSError, UnicodeError, ValueError) as error:
                errors.append(
                    {"path": str(image), "reason": "conversion_error", "detail": str(error)}
                )

        split_reports[split] = {
            "images": len(images),
            "labels": sum(
                1 for path in (output_root / "labels" / split).glob("*.txt")
                if path.is_file()
            ),
            "converted": split_converted,
            "clipped": split_clipped,
            "class_counts": {str(key): split_classes[key] for key in sorted(split_classes)},
            "skipped": {key: split_skips[key] for key in sorted(split_skips)},
        }

    fatal_errors = [error for error in errors if error["reason"] == "conversion_error"]
    return {
        "status": "fail" if fatal_errors else "pass",
        "source_root": str(source_root.resolve()),
        "output_root": str(output_root.resolve()),
        "class_names": list(CLASS_NAMES),
        "splits": split_reports,
        "totals": {
            "converted": total_converted,
            "clipped": total_clipped,
            "class_counts": {str(key): total_classes[key] for key in sorted(total_classes)},
            "skipped": {key: total_skips[key] for key in sorted(total_skips)},
        },
        "errors": errors,
    }


def _audit_label(
    path: Path, class_count: int
) -> tuple[int, Counter[int], list[dict[str, str]]]:
    instances = 0
    classes: Counter[int] = Counter()
    errors: list[dict[str, str]] = []
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except (OSError, UnicodeError) as error:
        return 0, classes, [
            {"path": str(path), "reason": "unreadable_label", "detail": str(error)}
        ]
    for line_number, line in enumerate(lines, start=1):
        if not line.strip():
            continue
        fields = line.split()
        if len(fields) != 5:
            errors.append(
                {"path": str(path), "line": str(line_number), "reason": "malformed_label"}
            )
            continue
        try:
            class_id = int(fields[0])
            coordinates = tuple(float(field) for field in fields[1:])
        except ValueError:
            errors.append(
                {"path": str(path), "line": str(line_number), "reason": "malformed_label"}
            )
            continue
        if class_id < 0 or class_id >= class_count:
            errors.append(
                {
                    "path": str(path),
                    "line": str(line_number),
                    "reason": "class_out_of_range",
                }
            )
            continue
        if not all(math.isfinite(value) for value in coordinates):
            errors.append(
                {
                    "path": str(path),
                    "line": str(line_number),
                    "reason": "non_finite_coordinate",
                }
            )
            continue
        center_x, center_y, width, height = coordinates
        if not (
            0.0 <= center_x <= 1.0
            and 0.0 <= center_y <= 1.0
            and 0.0 < width <= 1.0
            and 0.0 < height <= 1.0
        ):
            errors.append(
                {
                    "path": str(path),
                    "line": str(line_number),
                    "reason": "coordinate_out_of_range",
                }
            )
            continue
        tolerance = 1e-7
        if (
            center_x - width * 0.5 < -tolerance
            or center_x + width * 0.5 > 1.0 + tolerance
            or center_y - height * 0.5 < -tolerance
            or center_y + height * 0.5 > 1.0 + tolerance
        ):
            errors.append(
                {"path": str(path), "line": str(line_number), "reason": "box_out_of_bounds"}
            )
            continue
        instances += 1
        classes[class_id] += 1
    return instances, classes, errors


def audit_dataset(
    dataset_root: Path, expected_names: Sequence[str]
) -> dict[str, Any]:
    dataset_root = Path(dataset_root)
    images_root = dataset_root / "images"
    labels_root = dataset_root / "labels"
    errors: list[dict[str, str]] = []
    class_counts: Counter[int] = Counter()
    image_count = 0
    label_count = 0
    instance_count = 0

    if tuple(expected_names) != CLASS_NAMES:
        errors.append(
            {
                "path": str(dataset_root),
                "reason": "class_names_mismatch",
                "detail": ",".join(expected_names),
            }
        )

    split_names = sorted(
        {path.name for path in images_root.iterdir() if path.is_dir()}
        if images_root.is_dir()
        else set()
    )
    if not split_names:
        errors.append({"path": str(images_root), "reason": "missing_images_root"})

    for split in split_names:
        image_dir = images_root / split
        label_dir = labels_root / split
        image_paths = sorted(
            path for path in image_dir.iterdir() if path.suffix.lower() in IMAGE_SUFFIXES
        )
        image_stems = {path.stem for path in image_paths}
        label_paths = sorted(label_dir.glob("*.txt")) if label_dir.is_dir() else []
        label_stems = {path.stem for path in label_paths}
        image_count += len(image_paths)
        label_count += len(label_paths)

        for image in image_paths:
            if image.is_symlink() and not image.exists():
                errors.append({"path": str(image), "reason": "broken_image_link"})
            elif not image.is_file():
                errors.append({"path": str(image), "reason": "unreadable_image"})
            if image.stem not in label_stems:
                errors.append(
                    {"path": str(label_dir / f"{image.stem}.txt"), "reason": "missing_label"}
                )

        for label in label_paths:
            if label.stem not in image_stems:
                errors.append(
                    {"path": str(label), "reason": "missing_image"}
                )
            instances, classes, label_errors = _audit_label(label, len(expected_names))
            instance_count += instances
            class_counts.update(classes)
            errors.extend(label_errors)

    return {
        "status": "pass" if not errors else "fail",
        "dataset_root": str(dataset_root.resolve()),
        "class_names": list(expected_names),
        "totals": {
            "images": image_count,
            "labels": label_count,
            "instances": instance_count,
        },
        "class_counts": {str(key): class_counts[key] for key in sorted(class_counts)},
        "errors": errors,
    }
