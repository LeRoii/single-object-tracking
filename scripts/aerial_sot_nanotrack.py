#!/usr/bin/env python3
"""Prepare VisDrone and UAVDT aerial tracking data for NanoTrack training."""

import argparse
import csv
import json
import sys
from concurrent.futures import ProcessPoolExecutor
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Sequence, Tuple, Union

import cv2

cv2.setNumThreads(1)

WORKSPACE_ROOT = Path(__file__).resolve().parents[1]
if str(WORKSPACE_ROOT) not in sys.path:
    sys.path.insert(0, str(WORKSPACE_ROOT))

from scripts.uav123_nanotrack import make_search_crop


Box = Tuple[float, float, float, float]


@dataclass(frozen=True)
class FrameRecord:
    frame_number: int
    image_path: str
    bbox_xywh: Box


@dataclass(frozen=True)
class Tracklet:
    name: str
    source_video: str
    track_id: str
    category: str
    frames: Tuple[FrameRecord, ...]


FrameInput = Union[FrameRecord, Tuple[int, str, Box]]


def _frame_record(value: FrameInput) -> FrameRecord:
    if isinstance(value, FrameRecord):
        return value
    frame_number, image_path, bbox = value
    return FrameRecord(int(frame_number), str(image_path), tuple(float(v) for v in bbox))


def _evenly_subsample(frames: Sequence[FrameRecord], maximum: int) -> Tuple[FrameRecord, ...]:
    if maximum <= 0 or len(frames) <= maximum:
        return tuple(frames)
    indices = [int(round(index * (len(frames) - 1) / float(maximum - 1))) for index in range(maximum)]
    return tuple(frames[index] for index in indices)


def split_and_subsample(
    frames: Iterable[FrameInput],
    min_frames: int = 30,
    max_gap: int = 10,
    max_frames: int = 60,
) -> List[Tuple[FrameRecord, ...]]:
    ordered = sorted((_frame_record(frame) for frame in frames), key=lambda frame: frame.frame_number)
    segments: List[List[FrameRecord]] = []
    current: List[FrameRecord] = []
    for frame in ordered:
        if current and frame.frame_number - current[-1].frame_number > max_gap:
            segments.append(current)
            current = []
        current.append(frame)
    if current:
        segments.append(current)
    return [
        _evenly_subsample(segment, max_frames)
        for segment in segments
        if len(segment) >= min_frames
    ]


def _read_csv_rows(path: Path) -> Iterable[List[str]]:
    with Path(path).open("r", encoding="utf-8-sig", errors="replace", newline="") as handle:
        for row in csv.reader(handle):
            if row and any(value.strip() for value in row):
                yield [value.strip() for value in row]


def _valid_box(values: Sequence[str]) -> Box:
    if len(values) != 4:
        raise ValueError("expected four bbox values")
    box = tuple(float(value) for value in values)
    if box[2] <= 0 or box[3] <= 0:
        raise ValueError("bbox width and height must be positive")
    return box  # type: ignore[return-value]


def _image_index(sequence_dir: Path) -> Dict[int, Path]:
    images: Dict[int, Path] = {}
    for path in sorted(Path(sequence_dir).glob("*.jpg")):
        stem = path.stem
        digits = stem[3:] if stem.lower().startswith("img") else stem
        if digits.isdigit():
            images[int(digits)] = path
    return images


def parse_visdrone_sot_train(root: Path) -> List[Tracklet]:
    root = Path(root)
    bases = [
        ("p1", root / "VisDrone2019-SOT-train_part1/VisDrone2019-SOT-train"),
        ("p2", root / "VisDrone2019-SOT-train_part2/VisDrone2019-SOT-train"),
    ]
    tracklets: List[Tracklet] = []
    for part, base in bases:
        annotation_root = base / "annotations"
        sequence_root = base / "sequences"
        if not annotation_root.is_dir() or not sequence_root.is_dir():
            raise FileNotFoundError("missing VisDrone-SOT train root {}".format(base))
        for annotation_path in sorted(annotation_root.glob("*.txt")):
            source_video = annotation_path.stem
            images = _image_index(sequence_root / source_video)
            records: List[FrameRecord] = []
            for offset, row in enumerate(_read_csv_rows(annotation_path), 1):
                try:
                    bbox = _valid_box(row)
                except (ValueError, TypeError):
                    continue
                image_path = images.get(offset)
                if image_path is None:
                    raise FileNotFoundError("missing frame {} for {}".format(offset, source_video))
                records.append(FrameRecord(offset, str(image_path), bbox))
            if records:
                tracklets.append(
                    Tracklet(
                        name="visdrone_sot_{}__{}".format(part, source_video),
                        source_video=source_video,
                        track_id="00",
                        category="unknown",
                        frames=tuple(records),
                    )
                )
    return tracklets


def _visdrone_category(category_id: int) -> str:
    if category_id in (1, 2):
        return "person"
    if category_id in (4, 5, 6, 9):
        return "vehicle"
    if category_id in (3, 7, 8, 10):
        return "bike"
    return "ignore"


def parse_visdrone_mot_train(
    root: Path,
    min_frames: int = 30,
    max_gap: int = 10,
    max_frames: int = 60,
) -> List[Tracklet]:
    base = Path(root) / "VisDrone2019-MOT-train"
    annotation_root = base / "annotations"
    sequence_root = base / "sequences"
    if not annotation_root.is_dir() or not sequence_root.is_dir():
        raise FileNotFoundError("missing VisDrone-MOT train root {}".format(base))
    tracklets: List[Tracklet] = []
    for annotation_path in sorted(annotation_root.glob("*.txt")):
        source_video = annotation_path.stem
        images = _image_index(sequence_root / source_video)
        tracks: Dict[int, List[FrameRecord]] = {}
        categories: Dict[int, str] = {}
        for row in _read_csv_rows(annotation_path):
            if len(row) < 10:
                raise ValueError("{} has a row with fewer than 10 columns".format(annotation_path))
            frame_number, target_id = int(row[0]), int(row[1])
            score, category_id = int(float(row[6])), int(float(row[7]))
            category = _visdrone_category(category_id)
            if target_id <= 0 or score != 1 or category == "ignore":
                continue
            try:
                bbox = _valid_box(row[2:6])
            except ValueError:
                continue
            image_path = images.get(frame_number)
            if image_path is None:
                raise FileNotFoundError("missing frame {} for {}".format(frame_number, source_video))
            tracks.setdefault(target_id, []).append(FrameRecord(frame_number, str(image_path), bbox))
            categories.setdefault(target_id, category)
        for target_id in sorted(tracks):
            segments = split_and_subsample(tracks[target_id], min_frames, max_gap, max_frames)
            for segment_index, segment in enumerate(segments):
                tracklets.append(
                    Tracklet(
                        name="visdrone_mot__{}__id{:06d}__seg{:02d}".format(
                            source_video, target_id, segment_index
                        ),
                        source_video=source_video,
                        track_id=str(target_id),
                        category=categories[target_id],
                        frames=segment,
                    )
                )
    return tracklets


def parse_uavdt_train(
    root: Path,
    min_frames: int = 30,
    max_gap: int = 10,
    max_frames: int = 60,
) -> List[Tracklet]:
    root = Path(root)
    train_list_root = root / "M_attr/train"
    gt_root = root / "UAV-benchmark-MOTD_v1.0/GT"
    image_root = root / "UAV-benchmark-M"
    if not train_list_root.is_dir() or not gt_root.is_dir() or not image_root.is_dir():
        raise FileNotFoundError("incomplete UAVDT train layout under {}".format(root))
    tracklets: List[Tracklet] = []
    for attr_path in sorted(train_list_root.glob("*_attr.txt")):
        source_video = attr_path.name[: -len("_attr.txt")]
        gt_path = gt_root / (source_video + "_gt_whole.txt")
        images = _image_index(image_root / source_video)
        tracks: Dict[int, List[FrameRecord]] = {}
        categories: Dict[int, int] = {}
        for row in _read_csv_rows(gt_path):
            if len(row) < 9:
                raise ValueError("{} has a row with fewer than 9 columns".format(gt_path))
            frame_number, target_id = int(row[0]), int(row[1])
            category_id = int(float(row[8]))
            if target_id <= 0 or category_id not in (1, 2, 3):
                continue
            try:
                bbox = _valid_box(row[2:6])
            except ValueError:
                continue
            image_path = images.get(frame_number)
            if image_path is None:
                raise FileNotFoundError("missing frame {} for {}".format(frame_number, source_video))
            tracks.setdefault(target_id, []).append(FrameRecord(frame_number, str(image_path), bbox))
            categories.setdefault(target_id, category_id)
        for target_id in sorted(tracks):
            segments = split_and_subsample(tracks[target_id], min_frames, max_gap, max_frames)
            for segment_index, segment in enumerate(segments):
                tracklets.append(
                    Tracklet(
                        name="uavdt__{}__id{:06d}__seg{:02d}".format(
                            source_video, target_id, segment_index
                        ),
                        source_video=source_video,
                        track_id=str(target_id),
                        category="vehicle",
                        frames=segment,
                    )
                )
    return tracklets


def _write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True), encoding="utf-8")


def _prepare_crop_job(job: Tuple[str, Box, str, bool]) -> Tuple[str, str]:
    source_text, bbox, destination_text, overwrite = job
    source = Path(source_text)
    destination = Path(destination_text)
    if destination.is_file() and not overwrite:
        existing = cv2.imread(str(destination))
        if existing is not None and existing.shape[:2] == (511, 511):
            return "skipped", str(destination)
    image = cv2.imread(str(source))
    if image is None:
        return "failed", "cannot decode {}".format(source)
    crop = make_search_crop(image, bbox, out_size=511)
    destination.parent.mkdir(parents=True, exist_ok=True)
    if not cv2.imwrite(str(destination), crop, [int(cv2.IMWRITE_JPEG_QUALITY), 95]):
        return "failed", "cannot write {}".format(destination)
    return "written", str(destination)


def prepare_tracklets(
    dataset_name: str,
    tracklets: Sequence[Tracklet],
    output_root: Path,
    workers: int = 8,
    overwrite: bool = False,
) -> Dict[str, object]:
    output_root = Path(output_root)
    tracklets = list(tracklets)
    if not tracklets:
        raise ValueError("{} has no eligible tracklets".format(dataset_name))
    names = [tracklet.name for tracklet in tracklets]
    if len(set(names)) != len(names):
        raise ValueError("duplicate tracklet names in {}".format(dataset_name))
    missing = sorted(
        {
            frame.image_path
            for tracklet in tracklets
            for frame in tracklet.frames
            if not Path(frame.image_path).is_file()
        }
    )
    if missing:
        raise FileNotFoundError(
            "missing source images for {}: {}".format(dataset_name, missing[:5])
        )

    annotations: Dict[str, Dict[str, Dict[str, List[float]]]] = {}
    jobs: List[Tuple[str, Box, str, bool]] = []
    categories: Dict[str, int] = {}
    for tracklet in tracklets:
        categories[tracklet.category] = categories.get(tracklet.category, 0) + 1
        frames: Dict[str, List[float]] = {}
        for frame in tracklet.frames:
            frame_key = "{:06d}".format(frame.frame_number)
            x, y, width, height = frame.bbox_xywh
            frames[frame_key] = [x, y, x + width, y + height]
            destination = output_root / "crop511" / tracklet.name / (
                frame_key + ".00.x.jpg"
            )
            jobs.append((frame.image_path, frame.bbox_xywh, str(destination), overwrite))
        annotations[tracklet.name] = {"00": frames}

    audit = {
        "dataset": dataset_name,
        "tracklets": len(tracklets),
        "frames": len(jobs),
        "source_videos": len({tracklet.source_video for tracklet in tracklets}),
        "categories": categories,
        "min_tracklet_frames": min(len(tracklet.frames) for tracklet in tracklets),
        "max_tracklet_frames": max(len(tracklet.frames) for tracklet in tracklets),
        "missing_source_images": [],
    }
    _write_json(output_root / "audit.json", audit)

    counts: Dict[str, object] = {
        "requested": len(jobs),
        "written": 0,
        "skipped": 0,
        "failed": 0,
        "failures": [],
    }
    executor = None
    if workers <= 1:
        results = map(_prepare_crop_job, jobs)
    else:
        executor = ProcessPoolExecutor(max_workers=workers)
        results = executor.map(_prepare_crop_job, jobs, chunksize=16)
    try:
        for index, (status, detail) in enumerate(results, 1):
            counts[status] = int(counts[status]) + 1
            if status == "failed":
                counts["failures"].append(detail)  # type: ignore[union-attr]
            if index % 1000 == 0 or index == len(jobs):
                print(
                    "{} prepared {}/{} crops (written={}, skipped={}, failed={})".format(
                        dataset_name,
                        index,
                        len(jobs),
                        counts["written"],
                        counts["skipped"],
                        counts["failed"],
                    ),
                    flush=True,
                )
    finally:
        if executor is not None:
            executor.shutdown(wait=True)
    if counts["failed"]:
        _write_json(output_root / "prepare_failures.json", counts)
        raise RuntimeError("{} crops failed for {}".format(counts["failed"], dataset_name))

    _write_json(output_root / "train.json", annotations)
    summary = dict(audit)
    summary["output_root"] = str(output_root.resolve())
    summary["crop_summary"] = counts
    _write_json(output_root / "prepare_summary.json", summary)
    return summary


def build_aerial_datasets(
    visdrone_sot_root: Path,
    visdrone_mot_root: Path,
    uavdt_root: Path,
    output_root: Path,
    workers: int = 8,
    overwrite: bool = False,
    mot_max_frames: int = 60,
) -> Dict[str, object]:
    parsed = {
        "visdrone_sot": parse_visdrone_sot_train(visdrone_sot_root),
        "visdrone_mot": parse_visdrone_mot_train(
            visdrone_mot_root, max_frames=mot_max_frames
        ),
        "uavdt": parse_uavdt_train(uavdt_root, max_frames=mot_max_frames),
    }
    summaries = {
        name: prepare_tracklets(
            name,
            tracklets,
            Path(output_root) / name,
            workers=workers,
            overwrite=overwrite,
        )
        for name, tracklets in parsed.items()
    }
    combined = {
        "sources": summaries,
        "tracklets": sum(int(summary["tracklets"]) for summary in summaries.values()),
        "frames": sum(int(summary["frames"]) for summary in summaries.values()),
    }
    _write_json(Path(output_root) / "prepare_summary.json", combined)
    return combined


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--visdrone-sot-root", type=Path, required=True)
    parser.add_argument("--visdrone-mot-root", type=Path, required=True)
    parser.add_argument("--uavdt-root", type=Path, required=True)
    parser.add_argument("--output-root", type=Path)
    parser.add_argument("--workers", type=int, default=8)
    parser.add_argument("--mot-max-frames", type=int, default=60)
    parser.add_argument("--overwrite", action="store_true")
    parser.add_argument("--audit-only", action="store_true")
    args = parser.parse_args()
    values = {
        "visdrone_sot": parse_visdrone_sot_train(args.visdrone_sot_root),
        "visdrone_mot": parse_visdrone_mot_train(
            args.visdrone_mot_root, max_frames=args.mot_max_frames
        ),
        "uavdt": parse_uavdt_train(
            args.uavdt_root, max_frames=args.mot_max_frames
        ),
    }
    summary = {
        name: {
            "tracklets": len(tracklets),
            "frames": sum(len(tracklet.frames) for tracklet in tracklets),
            "categories": {
                category: sum(1 for tracklet in tracklets if tracklet.category == category)
                for category in sorted({tracklet.category for tracklet in tracklets})
            },
        }
        for name, tracklets in values.items()
    }
    if args.audit_only:
        print(json.dumps(summary, indent=2, sort_keys=True))
        return
    if args.output_root is None:
        parser.error("--output-root is required unless --audit-only is used")
    prepared = {
        name: prepare_tracklets(
            name,
            tracklets,
            args.output_root / name,
            workers=args.workers,
            overwrite=args.overwrite,
        )
        for name, tracklets in values.items()
    }
    combined = {
        "sources": prepared,
        "tracklets": sum(int(value["tracklets"]) for value in prepared.values()),
        "frames": sum(int(value["frames"]) for value in prepared.values()),
    }
    _write_json(args.output_root / "prepare_summary.json", combined)
    print(json.dumps(combined, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
