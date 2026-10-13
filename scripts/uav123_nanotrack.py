#!/usr/bin/env python3
"""UAV123_10fps metadata and NanoTrack preprocessing utilities."""

import argparse
import json
import math
import random
import re
from concurrent.futures import ProcessPoolExecutor
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence, Tuple

import cv2
import numpy as np


@dataclass(frozen=True)
class SequenceSpec:
    name: str
    folder: str
    start_frame: int
    end_frame: int
    zero_padding: int
    extension: str

    @property
    def frame_count(self) -> int:
        return self.end_frame - self.start_frame + 1


_CONFIG_PATTERN = re.compile(
    r"struct\('name','(?P<name>[^']+)',"
    r"'path','[^']*\\(?P<folder>[^\\']+)\\',"
    r"'startFrame',(?P<start>\d+),"
    r"'endFrame',(?P<end>\d+),"
    r"'nz',(?P<nz>\d+),"
    r"'ext','(?P<ext>[^']+)'"
)


def parse_config_seqs(path: Path) -> List[SequenceSpec]:
    text = Path(path).read_text(encoding="utf-8", errors="replace")
    specs = [
        SequenceSpec(
            name=match.group("name"),
            folder=match.group("folder"),
            start_frame=int(match.group("start")),
            end_frame=int(match.group("end")),
            zero_padding=int(match.group("nz")),
            extension=match.group("ext"),
        )
        for match in _CONFIG_PATTERN.finditer(text)
    ]
    if not specs:
        raise ValueError("no UAV123 sequence definitions found in {}".format(path))
    names = [spec.name for spec in specs]
    if len(set(names)) != len(names):
        raise ValueError("duplicate logical sequence names in {}".format(path))
    return specs


def read_boxes(path: Path) -> List[Optional[Tuple[float, float, float, float]]]:
    boxes = []
    for line_number, raw_line in enumerate(
        Path(path).read_text(encoding="utf-8", errors="replace").splitlines(), 1
    ):
        line = raw_line.strip()
        if not line:
            continue
        parts = [part.strip() for part in re.split(r"[,\t]", line)]
        if len(parts) != 4:
            raise ValueError("{}:{} expected 4 values".format(path, line_number))
        try:
            values = tuple(float(part) for part in parts)
        except ValueError as exc:
            raise ValueError("{}:{} invalid numeric value".format(path, line_number)) from exc
        if not all(math.isfinite(value) for value in values) or values[2] <= 0 or values[3] <= 0:
            boxes.append(None)
        else:
            boxes.append(values)
    return boxes


def category_for(name: str) -> str:
    base = re.sub(r"_\d+$", "", name.lower())
    if base.startswith(("car", "truck")):
        return "vehicle"
    if base.startswith(("person", "group")):
        return "person"
    if base.startswith("bike"):
        return "bike"
    return "other"


def grouped_split(
    specs: Sequence[SequenceSpec],
    seed: int = 123456,
    train_ratio: float = 0.70,
    val_ratio: float = 0.15,
) -> Dict[str, object]:
    folder_specs: Dict[str, List[SequenceSpec]] = {}
    for spec in specs:
        folder_specs.setdefault(spec.folder, []).append(spec)

    categories: Dict[str, List[str]] = {}
    for folder, grouped_specs in folder_specs.items():
        category = category_for(grouped_specs[0].name)
        categories.setdefault(category, []).append(folder)

    rng = random.Random(seed)
    physical = {"train": [], "val": [], "test": []}
    for category in sorted(categories):
        folders = sorted(categories[category])
        rng.shuffle(folders)
        train_count = int(round(len(folders) * train_ratio))
        val_count = int(round(len(folders) * val_ratio))
        if train_count + val_count > len(folders):
            val_count = max(0, len(folders) - train_count)
        physical["train"].extend(folders[:train_count])
        physical["val"].extend(folders[train_count : train_count + val_count])
        physical["test"].extend(folders[train_count + val_count :])

    logical = {split_name: [] for split_name in physical}
    folder_to_split = {
        folder: split_name
        for split_name, folders in physical.items()
        for folder in folders
    }
    for spec in specs:
        logical[folder_to_split[spec.folder]].append(spec.name)

    return {
        "seed": seed,
        "ratios": {"train": train_ratio, "val": val_ratio, "test": 1.0 - train_ratio - val_ratio},
        "physical_folders": {key: sorted(value) for key, value in physical.items()},
        "logical_sequences": {key: sorted(value) for key, value in logical.items()},
        "sequence_categories": {spec.name: category_for(spec.name) for spec in specs},
    }


def audit_dataset(dataset_root: Path, specs: Sequence[SequenceSpec]) -> Dict[str, object]:
    root = Path(dataset_root)
    annotation_root = root / "anno" / "UAV123_10fps"
    image_root = root / "data_seq" / "UAV123_10fps"
    total = valid = invalid = 0
    length_mismatches = []
    missing_frames = []
    category_counts: Dict[str, Dict[str, int]] = {}

    for spec in specs:
        boxes = read_boxes(annotation_root / (spec.name + ".txt"))
        if len(boxes) != spec.frame_count:
            length_mismatches.append(
                {"name": spec.name, "actual": len(boxes), "expected": spec.frame_count}
            )
        category = category_for(spec.name)
        counts = category_counts.setdefault(
            category, {"logical_sequences": 0, "annotations": 0, "valid": 0, "invalid": 0}
        )
        counts["logical_sequences"] += 1
        counts["annotations"] += len(boxes)
        total += len(boxes)
        for offset, box in enumerate(boxes):
            frame_number = spec.start_frame + offset
            frame_path = image_root / spec.folder / (
                ("{:0" + str(spec.zero_padding) + "d}.{}").format(frame_number, spec.extension)
            )
            if not frame_path.is_file():
                missing_frames.append(str(frame_path))
            if box is None:
                invalid += 1
                counts["invalid"] += 1
            else:
                valid += 1
                counts["valid"] += 1

    return {
        "dataset_root": str(root.resolve()),
        "logical_sequences": len(specs),
        "physical_folders": len({spec.folder for spec in specs}),
        "annotations_total": total,
        "valid_boxes": valid,
        "absent_or_invalid_boxes": invalid,
        "length_mismatches": length_mismatches,
        "missing_frames": missing_frames,
        "category_counts": category_counts,
    }


def _crop_hwc(image: np.ndarray, bbox: Sequence[float], out_size: int, padding: Sequence[float]) -> np.ndarray:
    scale_x = (out_size - 1) / (bbox[2] - bbox[0])
    scale_y = (out_size - 1) / (bbox[3] - bbox[1])
    mapping = np.array(
        [[scale_x, 0.0, -scale_x * bbox[0]], [0.0, scale_y, -scale_y * bbox[1]]],
        dtype=np.float32,
    )
    return cv2.warpAffine(
        image,
        mapping,
        (out_size, out_size),
        borderMode=cv2.BORDER_CONSTANT,
        borderValue=tuple(float(value) for value in padding),
    )


def make_search_crop(
    image: np.ndarray,
    xywh: Sequence[float],
    out_size: int = 511,
    exemplar_size: int = 127,
    context_amount: float = 0.5,
) -> np.ndarray:
    x, y, width, height = [float(value) for value in xywh]
    center = (x + (width - 1.0) / 2.0, y + (height - 1.0) / 2.0)
    context = context_amount * (width + height)
    size_z = math.sqrt((width + context) * (height + context))
    scale_z = exemplar_size / size_z
    search_padding = (out_size - exemplar_size) / 2.0 / scale_z
    size_x = size_z + 2.0 * search_padding
    crop_box = (
        center[0] - size_x / 2.0,
        center[1] - size_x / 2.0,
        center[0] + size_x / 2.0,
        center[1] + size_x / 2.0,
    )
    padding = np.mean(image, axis=(0, 1))
    return _crop_hwc(image, crop_box, out_size, padding)


def write_split_annotations(
    dataset_root: Path,
    specs: Sequence[SequenceSpec],
    logical_names: Iterable[str],
    output_path: Path,
) -> Dict[str, object]:
    root = Path(dataset_root)
    annotation_root = root / "anno" / "UAV123_10fps"
    selected = set(logical_names)
    output: Dict[str, object] = {}
    for spec in specs:
        if spec.name not in selected:
            continue
        boxes = read_boxes(annotation_root / (spec.name + ".txt"))
        frames = {}
        for offset, box in enumerate(boxes):
            if box is None:
                continue
            frame_number = spec.start_frame + offset
            x, y, width, height = box
            frames["{:06d}".format(frame_number)] = [x, y, x + width, y + height]
        if frames:
            output[spec.name] = {"00": frames}
    path = Path(output_path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(output, indent=2, sort_keys=True), encoding="utf-8")
    return output


def _crop_job(job: Tuple[str, Tuple[float, float, float, float], str, bool]) -> Tuple[str, str]:
    source_text, box, destination_text, overwrite = job
    source = Path(source_text)
    destination = Path(destination_text)
    if destination.is_file() and not overwrite:
        existing = cv2.imread(str(destination))
        if existing is not None and existing.shape[:2] == (511, 511):
            return "skipped", str(destination)
    image = cv2.imread(str(source))
    if image is None:
        return "failed", "cannot decode {}".format(source)
    crop = make_search_crop(image, box, out_size=511)
    destination.parent.mkdir(parents=True, exist_ok=True)
    if not cv2.imwrite(str(destination), crop, [int(cv2.IMWRITE_JPEG_QUALITY), 95]):
        return "failed", "cannot write {}".format(destination)
    return "written", str(destination)


def prepare_crops(
    dataset_root: Path,
    specs: Sequence[SequenceSpec],
    output_root: Path,
    workers: int = 1,
    overwrite: bool = False,
) -> Dict[str, object]:
    root = Path(dataset_root)
    output = Path(output_root)
    annotation_root = root / "anno" / "UAV123_10fps"
    image_root = root / "data_seq" / "UAV123_10fps"
    jobs = []
    for spec in specs:
        boxes = read_boxes(annotation_root / (spec.name + ".txt"))
        for offset, box in enumerate(boxes):
            if box is None:
                continue
            frame_number = spec.start_frame + offset
            source = image_root / spec.folder / (
                ("{:0" + str(spec.zero_padding) + "d}.{}").format(frame_number, spec.extension)
            )
            destination = output / "crop511" / spec.name / (
                "{:06d}.00.x.jpg".format(frame_number)
            )
            jobs.append((str(source), box, str(destination), overwrite))

    counts = {"requested": len(jobs), "written": 0, "skipped": 0, "failed": 0}
    failures = []
    if workers <= 1:
        results = map(_crop_job, jobs)
    else:
        executor = ProcessPoolExecutor(max_workers=workers)
        results = executor.map(_crop_job, jobs, chunksize=16)
    try:
        for index, (status, detail) in enumerate(results, 1):
            counts[status] += 1
            if status == "failed":
                failures.append(detail)
            if index % 1000 == 0 or index == len(jobs):
                print(
                    "prepared {}/{} crops (written={}, skipped={}, failed={})".format(
                        index, len(jobs), counts["written"], counts["skipped"], counts["failed"]
                    ),
                    flush=True,
                )
    finally:
        if workers > 1:
            executor.shutdown(wait=True)
    counts["failures"] = failures
    return counts


def _write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True), encoding="utf-8")


def build_dataset(
    dataset_root: Path,
    output_root: Path,
    workers: int = 1,
    seed: int = 123456,
    overwrite: bool = False,
) -> Dict[str, object]:
    dataset_root = Path(dataset_root)
    output_root = Path(output_root)
    output_root.mkdir(parents=True, exist_ok=True)
    specs = parse_config_seqs(dataset_root / "configSeqs.m")
    audit = audit_dataset(dataset_root, specs)
    _write_json(output_root / "audit_report.json", audit)
    if audit["length_mismatches"] or audit["missing_frames"]:
        raise ValueError("dataset audit failed; see {}".format(output_root / "audit_report.json"))

    split = grouped_split(specs, seed=seed)
    _write_json(output_root / "splits.json", split)
    for split_name, logical_names in split["logical_sequences"].items():
        write_split_annotations(
            dataset_root,
            specs,
            logical_names,
            output_root / "annotations" / (split_name + ".json"),
        )

    crop_summary = prepare_crops(
        dataset_root, specs, output_root, workers=workers, overwrite=overwrite
    )
    summary = {
        "dataset_root": str(dataset_root.resolve()),
        "output_root": str(output_root.resolve()),
        "seed": seed,
        "audit": audit,
        "split_counts": {
            split_name: {
                "physical_folders": len(split["physical_folders"][split_name]),
                "logical_sequences": len(split["logical_sequences"][split_name]),
            }
            for split_name in ("train", "val", "test")
        },
        "crop_summary": crop_summary,
    }
    _write_json(output_root / "prepare_summary.json", summary)
    if crop_summary["failed"]:
        raise RuntimeError("{} crops failed; see prepare_summary.json".format(crop_summary["failed"]))
    return summary


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dataset-root", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    parser.add_argument("--workers", type=int, default=8)
    parser.add_argument("--seed", type=int, default=123456)
    parser.add_argument("--overwrite", action="store_true")
    args = parser.parse_args()
    summary = build_dataset(
        args.dataset_root,
        args.output_root,
        workers=args.workers,
        seed=args.seed,
        overwrite=args.overwrite,
    )
    print(json.dumps(summary, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
