#!/usr/bin/env python3
"""Prepare deterministic GOT-10K replay data for NanoTrack V3 fine-tuning."""

import configparser
import argparse
import json
import math
import os
import random
import sys
import tempfile
from collections import Counter, defaultdict
from concurrent.futures import ProcessPoolExecutor
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Mapping, Sequence, Tuple

WORKSPACE_ROOT = Path(__file__).resolve().parents[1]
if str(WORKSPACE_ROOT) not in sys.path:
    sys.path.insert(0, str(WORKSPACE_ROOT))

from scripts.nanotrack_three_stage_data import prepare_crop_group


DEFAULT_HIDDEN_QUOTAS = {
    "animal": 169,
    "person": 114,
    "vehicle": 112,
    "object part": 49,
    "passive motion object": 23,
}
KNOWN_ROOT_CLASSES = tuple(sorted(DEFAULT_HIDDEN_QUOTAS))


@dataclass(frozen=True)
class GotFrame:
    number: int
    image_path: str
    bbox_xywh: Tuple[float, float, float, float]
    absence: int
    cover: int


@dataclass(frozen=True)
class GotSequence:
    name: str
    path: Path
    root_class: str
    frames: Tuple[GotFrame, ...]


def discover_train_sequences(dataset_root: Path) -> Tuple[Path, ...]:
    """Discover the extracted official train sequences beneath split folders."""
    root = Path(dataset_root).resolve() / "train_data"
    found = []
    for path in root.glob("GOT-10k_Train_split_*/GOT-10k_Train_*"):
        if path.is_dir() and (path / "meta_info.ini").is_file():
            found.append(path.resolve())
    found.sort(key=lambda path: path.name)
    names = [path.name for path in found]
    if len(names) != len(set(names)):
        raise ValueError("duplicate GOT-10K train sequence name")
    return tuple(found)


def _read_int_labels(path: Path) -> Tuple[int, ...]:
    return tuple(int(float(line.strip())) for line in path.read_text(encoding="utf-8").splitlines()
                 if line.strip())


def _read_boxes(path: Path) -> Tuple[Tuple[float, float, float, float], ...]:
    boxes = []
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        values = tuple(float(value.strip()) for value in line.split(","))
        if len(values) != 4:
            raise ValueError("invalid GOT-10K groundtruth row in {}".format(path))
        boxes.append(values)
    return tuple(boxes)


def load_got_sequence(path: Path) -> GotSequence:
    """Load one sequence and retain only visible, covered, valid annotated frames."""
    root = Path(path).resolve()
    parser = configparser.ConfigParser()
    if not parser.read(str(root / "meta_info.ini"), encoding="utf-8"):
        raise ValueError("cannot read GOT-10K metadata: {}".format(root))
    try:
        root_class = parser.get("METAINFO", "root_class").strip().lower()
    except (configparser.Error, KeyError) as error:
        raise ValueError("missing root_class in {}: {}".format(root, error))
    if root_class not in KNOWN_ROOT_CLASSES:
        raise ValueError("unknown GOT-10K root class: {}".format(root_class))
    boxes = _read_boxes(root / "groundtruth.txt")
    absence = _read_int_labels(root / "absence.label")
    cover = _read_int_labels(root / "cover.label")
    if not (len(boxes) == len(absence) == len(cover)):
        raise ValueError("GOT-10K annotation lengths differ for {}".format(root.name))
    frames = []
    for index, (box, missing, visible) in enumerate(zip(boxes, absence, cover), 1):
        x, y, width, height = box
        if missing or visible <= 0 or not all(math.isfinite(value) for value in box):
            continue
        if width <= 0 or height <= 0:
            continue
        image_path = root / "{:08d}.jpg".format(index)
        if not image_path.is_file():
            raise FileNotFoundError(str(image_path))
        frames.append(GotFrame(index, str(image_path), box, int(missing), int(visible)))
    if len(frames) < 2:
        raise ValueError("GOT-10K sequence has fewer than two usable frames: {}".format(root.name))
    return GotSequence(root.name, root, root_class, tuple(frames))


def read_root_class(path: Path) -> str:
    parser = configparser.ConfigParser()
    metadata = Path(path) / "meta_info.ini"
    if not parser.read(str(metadata), encoding="utf-8"):
        raise ValueError("cannot read GOT-10K metadata: {}".format(metadata))
    root_class = parser.get("METAINFO", "root_class").strip().lower()
    if root_class not in KNOWN_ROOT_CLASSES:
        raise ValueError("unknown GOT-10K root class: {}".format(root_class))
    return root_class


def usable_frame_count(path: Path) -> int:
    root = Path(path)
    boxes = _read_boxes(root / "groundtruth.txt")
    absence = _read_int_labels(root / "absence.label")
    cover = _read_int_labels(root / "cover.label")
    if not (len(boxes) == len(absence) == len(cover)):
        raise ValueError("GOT-10K annotation lengths differ for {}".format(root.name))
    return sum(
        (not missing) and visible > 0 and all(math.isfinite(value) for value in box)
        and box[2] > 0 and box[3] > 0
        for box, missing, visible in zip(boxes, absence, cover)
    )


def stratified_train_hidden(
    sequences: Sequence[GotSequence],
    hidden_quotas: Mapping[str, int] = DEFAULT_HIDDEN_QUOTAS,
    seed: int = 123456,
) -> Dict[str, Tuple[str, ...]]:
    """Return stable, root-class-stratified train and one-shot hidden names."""
    by_class = {}  # type: Dict[str, list]
    names = set()
    for sequence in sequences:
        if sequence.name in names:
            raise ValueError("duplicate GOT-10K sequence: {}".format(sequence.name))
        names.add(sequence.name)
        by_class.setdefault(sequence.root_class, []).append(sequence.name)
    rng = random.Random(int(seed))
    hidden = []
    train = []
    for root_class in sorted(hidden_quotas):
        group = sorted(by_class.pop(root_class, []))
        quota = int(hidden_quotas[root_class])
        if quota < 0 or len(group) <= quota:
            raise ValueError("insufficient {} sequences for hidden quota {}".format(root_class, quota))
        rng.shuffle(group)
        hidden.extend(group[:quota])
        train.extend(group[quota:])
    if by_class:
        raise ValueError("unaccounted GOT-10K root classes: {}".format(sorted(by_class)))
    return {"train": tuple(sorted(train)), "hidden": tuple(sorted(hidden))}


def training_pool(root_class: str) -> str:
    root = str(root_class).strip().lower()
    if root not in KNOWN_ROOT_CLASSES:
        raise ValueError("unknown GOT-10K root class: {}".format(root))
    return root if root in ("person", "vehicle") else "other"


def _hardness(frames: Sequence[GotFrame], index: int) -> float:
    frame = frames[index]
    x, y, width, height = frame.bbox_xywh
    area = width * height
    score = max(0, 8 - frame.cover) * 4.0
    for neighbor_index in (index - 1, index + 1):
        if neighbor_index < 0 or neighbor_index >= len(frames):
            continue
        nx, ny, nw, nh = frames[neighbor_index].bbox_xywh
        neighbor_area = nw * nh
        ratio = max(area, neighbor_area) / max(min(area, neighbor_area), 1e-12)
        score += abs(math.log(ratio))
        center_motion = math.hypot((x + width / 2) - (nx + nw / 2),
                                   (y + height / 2) - (ny + nh / 2))
        score += center_motion / max(math.hypot(nw, nh), 1e-12)
    return score


def select_training_frames(
    sequence: GotSequence,
    maximum: int = 48,
    uniform_count: int = 36,
    hard_count: int = 12,
) -> Tuple[GotFrame, ...]:
    """Select uniform temporal coverage plus the strongest difficult observations."""
    frames = tuple(sorted(sequence.frames, key=lambda frame: frame.number))
    if maximum <= 1 or uniform_count <= 1 or hard_count < 0:
        raise ValueError("invalid GOT-10K frame sampling limits")
    if uniform_count + hard_count > maximum:
        raise ValueError("uniform_count + hard_count exceeds maximum")
    if len(frames) <= maximum:
        return frames
    uniform = {
        int(round(slot * (len(frames) - 1) / float(uniform_count - 1)))
        for slot in range(uniform_count)
    }
    ranked = sorted(range(len(frames)), key=lambda index: (-_hardness(frames, index), index))
    chosen = set(uniform)
    additions = 0
    if hard_count:
        for index in ranked:
            if index in chosen:
                continue
            chosen.add(index)
            additions += 1
            if additions >= hard_count or len(chosen) >= maximum:
                break
    return tuple(frames[index] for index in sorted(chosen))


def _atomic_json(path: Path, value: object) -> None:
    destination = Path(path).resolve()
    destination.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(
        prefix="." + destination.name + ".", suffix=".tmp", dir=str(destination.parent)
    )
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as handle:
            json.dump(value, handle, indent=2, sort_keys=True)
            handle.write("\n")
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary, str(destination))
    except BaseException:
        try:
            os.unlink(temporary)
        except OSError:
            pass
        raise


def build_split_inventory(
    dataset_root: Path,
    output_path: Path,
    hidden_quotas: Mapping[str, int] = DEFAULT_HIDDEN_QUOTAS,
    seed: int = 123456,
    validate_eligibility: bool = False,
) -> Dict[str, object]:
    paths = discover_train_sequences(dataset_root)
    raw_sequences = tuple(
        GotSequence(path.name, path, read_root_class(path), tuple()) for path in paths
    )
    excluded = {}
    sequences = []
    for position, sequence in enumerate(raw_sequences, 1):
        if validate_eligibility and usable_frame_count(sequence.path) < 2:
            excluded[sequence.name] = "fewer_than_two_usable_frames"
        else:
            sequences.append(sequence)
        if validate_eligibility and position % 1000 == 0:
            print("validated GOT eligibility {}/{}; excluded {}".format(
                position, len(raw_sequences), len(excluded)), flush=True)
    sequences = tuple(sequences)
    splits = stratified_train_hidden(sequences, hidden_quotas, seed)
    class_counts = Counter(sequence.root_class for sequence in sequences)
    raw_class_counts = Counter(sequence.root_class for sequence in raw_sequences)
    result = {
        "format_version": 1,
        "dataset_root": str(Path(dataset_root).resolve()),
        "seed": int(seed),
        "raw_sequence_count": len(raw_sequences),
        "sequence_count": len(sequences),
        "root_class_counts": {key: class_counts[key] for key in sorted(class_counts)},
        "raw_root_class_counts": {
            key: raw_class_counts[key] for key in sorted(raw_class_counts)
        },
        "eligibility_validated": bool(validate_eligibility),
        "excluded_sequences": dict(sorted(excluded.items())),
        "hidden_quotas": {key: int(hidden_quotas[key]) for key in sorted(hidden_quotas)},
        "splits": {key: list(splits[key]) for key in ("train", "hidden")},
        "sequences": {
            sequence.name: {"path": str(sequence.path), "root_class": sequence.root_class,
                            "pool": training_pool(sequence.root_class)}
            for sequence in sequences
        },
        "intersection": sorted(set(splits["train"]) & set(splits["hidden"])),
    }
    if result["intersection"]:
        raise ValueError("GOT-10K train/hidden intersection is not empty")
    _atomic_json(output_path, result)
    return result


def _training_manifest(sequence: GotSequence, frames: Sequence[GotFrame]) -> Dict[str, object]:
    return {
        sequence.name: {
            "00": {
                "{:06d}".format(frame.number): [
                    frame.bbox_xywh[0],
                    frame.bbox_xywh[1],
                    frame.bbox_xywh[0] + frame.bbox_xywh[2],
                    frame.bbox_xywh[1] + frame.bbox_xywh[3],
                ]
                for frame in frames
            }
        }
    }


def prepare_got_training_data(
    inventory_path: Path,
    output_root: Path,
    workers: int = 8,
    maximum: int = 48,
    uniform_count: int = 36,
    hard_count: int = 12,
) -> Dict[str, object]:
    """Materialize three replay manifests and restart-safe 511 crops."""
    inventory_file = Path(inventory_path).resolve()
    inventory = json.loads(inventory_file.read_text(encoding="utf-8"))
    output = Path(output_root).resolve()
    got_root = output / "got"
    pool_manifests = {"person": {}, "vehicle": {}, "other": {}}
    selected_counts = Counter()
    jobs = []
    train_names = list(inventory["splits"]["train"])
    for position, name in enumerate(train_names, 1):
        record = inventory["sequences"][name]
        sequence = load_got_sequence(Path(record["path"]))
        selected = select_training_frames(
            sequence, maximum=maximum, uniform_count=uniform_count, hard_count=hard_count
        )
        pool = training_pool(sequence.root_class)
        pool_manifests[pool].update(_training_manifest(sequence, selected))
        selected_counts[pool] += len(selected)
        for frame in selected:
            destination = (got_root / "crop511" / sequence.name /
                           ("{:06d}.00.x.jpg".format(frame.number)))
            jobs.append((frame.image_path, frame.bbox_xywh, str(destination), False))
        if position % 500 == 0:
            print("indexed GOT train {}/{} sequences, {} selected frames".format(
                position, len(train_names), len(jobs)), flush=True)

    counts = Counter()
    grouped_jobs = [[job] for job in jobs]
    if int(workers) <= 1:
        for position, group in enumerate(grouped_jobs, 1):
            counts.update(prepare_crop_group(group))
            if position % 2000 == 0:
                print("GOT crops {}/{}".format(position, len(grouped_jobs)), flush=True)
    else:
        with ProcessPoolExecutor(max_workers=int(workers)) as executor:
            for position, result in enumerate(
                    executor.map(prepare_crop_group, grouped_jobs, chunksize=16), 1):
                counts.update(result)
                if position % 2000 == 0:
                    print("GOT crops {}/{}".format(position, len(grouped_jobs)), flush=True)

    manifest_hashes = {}
    for pool, manifest in sorted(pool_manifests.items()):
        path = got_root / "normal" / (pool + ".json")
        _atomic_json(path, manifest)
        import hashlib
        manifest_hashes[pool] = hashlib.sha256(path.read_bytes()).hexdigest()
    all_names = [name for manifest in pool_manifests.values() for name in manifest]
    if len(all_names) != len(set(all_names)) or set(all_names) != set(train_names):
        raise ValueError("GOT-10K pool manifests are not an exclusive exhaustive partition")
    summary = {
        "format_version": 1,
        "inventory_path": str(inventory_file),
        "sequence_count": len(train_names),
        "pool_sequence_counts": {
            pool: len(pool_manifests[pool]) for pool in sorted(pool_manifests)
        },
        "pool_frame_counts": {pool: selected_counts[pool] for pool in sorted(pool_manifests)},
        "selected_frame_count": len(jobs),
        "crop_counts": dict(counts),
        "crop_root": str(got_root / "crop511"),
        "manifest_hashes": manifest_hashes,
        "sampling": {"maximum": int(maximum), "uniform_count": int(uniform_count),
                     "hard_count": int(hard_count)},
        "partition_verified": True,
    }
    _atomic_json(output / "prepare-summary.json", summary)
    return summary


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dataset-root", type=Path, default=Path("/home/u20/dataset/GOT-10K"))
    parser.add_argument("--output", type=Path, default=(
        Path(__file__).resolve().parents[1] / "data/nanotrack_got_replay_v1/got-train-split.json"
    ))
    parser.add_argument("--prepare-training", action="store_true")
    parser.add_argument("--validate-eligibility", action="store_true")
    parser.add_argument("--workers", type=int, default=min(16, len(os.sched_getaffinity(0))))
    args = parser.parse_args()
    result = build_split_inventory(
        args.dataset_root, args.output, validate_eligibility=args.validate_eligibility
    )
    print(json.dumps({
        "output": str(args.output.resolve()),
        "sequence_count": result["sequence_count"],
        "root_class_counts": result["root_class_counts"],
        "train": len(result["splits"]["train"]),
        "hidden": len(result["splits"]["hidden"]),
    }, indent=2, sort_keys=True))
    if args.prepare_training:
        prepared = prepare_got_training_data(
            args.output, args.output.resolve().parent, workers=args.workers
        )
        print(json.dumps(prepared, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
