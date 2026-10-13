"""Source-video split and fixed aerial evaluation panel data interfaces."""

import json
import random
import argparse
import hashlib
import math
import os
import sys
from collections import Counter, defaultdict
from concurrent.futures import ProcessPoolExecutor
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Mapping, Optional, Sequence, Tuple

import cv2

# Direct script execution puts only ``scripts/`` on sys.path.  Add the
# workspace before importing sibling modules through the public scripts package.
WORKSPACE_ROOT = Path(__file__).resolve().parents[1]
if str(WORKSPACE_ROOT) not in sys.path:
    sys.path.insert(0, str(WORKSPACE_ROOT))

from scripts.aerial_sot_nanotrack import (
    FrameRecord, Tracklet, _image_index, _read_csv_rows, _valid_box,
    _visdrone_category, _write_json,
    parse_visdrone_sot_train, parse_visdrone_mot_train, parse_uavdt_train,
)
from scripts.uav123_nanotrack import parse_config_seqs, read_boxes, category_for, make_search_crop


Box = Tuple[float, float, float, float]


@dataclass(frozen=True)
class AerialSequenceSpec:
    name: str
    source: str
    source_video: str
    category: str
    image_paths: Tuple[str, ...]
    boxes_xywh: Tuple[Optional[Box], ...]
    window_start: int
    window_end: int

    def to_dict(self) -> Dict[str, object]:
        return {
            "name": self.name,
            "source": self.source,
            "source_video": self.source_video,
            "category": self.category,
            "image_paths": list(self.image_paths),
            "boxes_xywh": [list(box) if box is not None else None for box in self.boxes_xywh],
            "window_start": self.window_start,
            "window_end": self.window_end,
        }


def split_source_videos(tracklets: Mapping[str, Sequence[object]], seed: int = 123456) -> Dict[str, Dict[str, Tuple[str, ...]]]:
    """Split sorted unique video IDs separately for each dataset source."""
    split_map = {}
    for source in sorted(tracklets):
        video_ids = sorted({tracklet.source_video for tracklet in tracklets[source]})
        holdout = max(1, len(video_ids) // 10)
        if len(video_ids) < 2 * holdout:
            raise ValueError("{} has too few source videos for disjoint splits".format(source))
        random.Random(seed).shuffle(video_ids)
        train_end = len(video_ids) - 2 * holdout
        split_map[source] = {
            "train": tuple(video_ids[:train_end]),
            "validation": tuple(video_ids[train_end:train_end + holdout]),
            "hidden-test": tuple(video_ids[train_end + holdout:]),
        }
    return split_map


def build_evaluation_panel(
    tracklets: Mapping[str, Sequence[object]],
    split_map: Mapping[str, Mapping[str, Sequence[str]]],
    split: str,
    limits: Mapping[str, Mapping[str, int]],
) -> Tuple[AerialSequenceSpec, ...]:
    """Select a fixed panel using full input tracklets and centered frame windows."""
    panel = []
    panel_names = set()
    for source in sorted(tracklets):
        selected_videos = set(split_map.get(source, {}).get(split, ()))
        selected_counts = {}
        for tracklet in sorted(tracklets[source], key=lambda item: item.name):
            if tracklet.source_video not in selected_videos:
                continue
            if tracklet.name in panel_names:
                raise ValueError("duplicate panel sequence name: {}".format(tracklet.name))
            panel_names.add(tracklet.name)
            category_limit = limits.get(source)
            if category_limit is not None:
                maximum = category_limit.get(tracklet.category, 0)
                if selected_counts.get(tracklet.category, 0) >= maximum:
                    continue
            frames = sorted(tracklet.frames, key=lambda frame: frame.frame_number)
            if not frames:
                raise ValueError("{} has no evaluation frames".format(tracklet.name))
            numbers = [frame.frame_number for frame in frames]
            if any(right != left + 1 for left, right in zip(numbers, numbers[1:])):
                raise ValueError("{} evaluation frames are not consecutive".format(tracklet.name))
            frame_cap = 300 if source in ("UAVDT", "VisDrone-MOT") else None
            start = (len(frames) - frame_cap) // 2 if frame_cap and len(frames) > frame_cap else 0
            window = frames[start:start + frame_cap] if frame_cap else frames
            panel.append(AerialSequenceSpec(
                name=tracklet.name,
                source=source,
                source_video=tracklet.source_video,
                category=tracklet.category,
                image_paths=tuple(frame.image_path for frame in window),
                boxes_xywh=tuple(frame.bbox_xywh for frame in window),
                window_start=window[0].frame_number,
                window_end=window[-1].frame_number,
            ))
            selected_counts[tracklet.category] = selected_counts.get(tracklet.category, 0) + 1
    return tuple(panel)


def serialize_evaluation_panel(panel: Sequence[AerialSequenceSpec], split: str) -> str:
    """Serialize stable panel membership, paths, boxes, and frame windows."""
    return json.dumps(
        {"split": split, "members": [spec.name for spec in panel],
         "sequences": [spec.to_dict() for spec in panel]},
        indent=2,
        sort_keys=True,
    )


def sample_training_frames(frames, maximum=24):
    """Keep temporal endpoints, closest adjacent pair, thirds, then uniform anchors."""
    ordered = tuple(sorted(frames, key=lambda frame: frame.frame_number))
    if len(ordered) <= maximum:
        return ordered
    if maximum < 6:
        raise ValueError("at least six slots are required for temporal anchors")
    numbers = [frame.frame_number for frame in ordered]
    near = min(range(len(ordered) - 1), key=lambda i: (numbers[i + 1] - numbers[i], i))
    chosen = {0, len(ordered) - 1, near, near + 1}
    for fraction in (1.0 / 3, 2.0 / 3):
        target = numbers[0] + fraction * (numbers[-1] - numbers[0])
        chosen.add(min(range(len(numbers)), key=lambda i: (abs(numbers[i] - target), i)))
    # Fill remaining slots uniformly across the entire duration. Resolve collisions
    # against unused observations so late temporal positions cannot be starved.
    remaining = maximum - len(chosen)
    for j in range(1, remaining + 1):
        target = numbers[0] + j * (numbers[-1] - numbers[0]) / float(remaining + 1)
        chosen.add(min((i for i in range(len(numbers)) if i not in chosen),
                       key=lambda i: (abs(numbers[i] - target), i)))
    return tuple(ordered[i] for i in sorted(chosen))


def consecutive_evaluation_tracklet(tracklet, image_index):
    observations = {frame.frame_number: frame for frame in tracklet.frames}
    first, last = min(observations), max(observations)
    frames = tuple(FrameRecord(number, str(image_index[number]),
                               observations[number].bbox_xywh if number in observations else None)
                   for number in range(first, last + 1))
    return Tracklet(tracklet.name, tracklet.source_video, tracklet.track_id, tracklet.category, frames)


def category_tracklets(tracklets, train_videos):
    groups = defaultdict(list)
    names = set()
    for track in tracklets:
        if track.source_video not in train_videos:
            raise ValueError("non-train source video: " + track.source_video)
        if track.name in names:
            raise ValueError("duplicate training tracklet: " + track.name)
        names.add(track.name)
        groups[track.category].append(track)
    return dict(groups)


def has_nearby_distractor(box, target_id, class_id, observations):
    x, y, w, h = box
    cx, cy = x + w / 2, y + h / 2
    radius = 2 * math.hypot(w, h)
    return any(str(other_id) != str(target_id) and other_class == class_id
               and math.hypot(other[0] + other[2] / 2 - cx,
                              other[1] + other[3] / 2 - cy) <= radius
               for other_id, other_class, other in observations)


def hard_tracklet(tracklet, image_size, extra_reasons=None):
    """Mark sampled observations, then retain each mark and visible neighbors."""
    frames = sorted(tracklet.frames, key=lambda frame: frame.frame_number)
    reasons = defaultdict(set)
    image_area = image_size[0] * image_size[1]
    areas = [frame.bbox_xywh[2] * frame.bbox_xywh[3] for frame in frames]
    for i, frame in enumerate(frames):
        if areas[i] / float(image_area) < 0.01:
            reasons[i].add("small")
        reasons[i].update((extra_reasons or {}).get(frame.frame_number, ()))
        if i:
            a, b = frames[i - 1].bbox_xywh, frame.bbox_xywh
            if math.hypot(b[0] + b[2] / 2 - a[0] - a[2] / 2,
                          b[1] + b[3] / 2 - a[1] - a[3] / 2) > 0.5 * math.hypot(a[2], a[3]):
                reasons[i - 1].add("motion")
                reasons[i].add("motion")
    if areas and max(areas) / min(areas) >= 2:
        for i in range(len(frames)):
            reasons[i].add("scale")
    marked = {i for i, values in reasons.items() if values}
    if not marked or len(frames) < 2:
        return None, {}
    retained = marked | {j for i in marked for j in (i - 1, i + 1) if 0 <= j < len(frames)}
    flags = {frames[i].frame_number: sorted(reasons[i]) if reasons[i] else ["neighbor"]
             for i in sorted(retained)}
    return Tracklet(tracklet.name, tracklet.source_video, tracklet.track_id, tracklet.category,
                    tuple(frames[i] for i in sorted(retained))), flags


def _manifest(tracklets):
    return {t.name: {"00": {"{:06d}".format(f.frame_number):
            [f.bbox_xywh[0], f.bbox_xywh[1], f.bbox_xywh[0] + f.bbox_xywh[2],
             f.bbox_xywh[1] + f.bbox_xywh[3]] for f in t.frames}} for t in tracklets}


def _strict_crop(path):
    image = cv2.imread(str(path))
    if image is None or image.shape != (511, 511, 3):
        raise ValueError("invalid 511 crop: " + str(path))


def prepare_crop_group(jobs):
    """Decode one source frame once for all target crops, verifying every output."""
    # ProcessPoolExecutor already provides process-level parallelism.  Leaving
    # OpenCV at its host-wide thread count multiplies that count in every
    # worker (6 processes x 6 OpenCV threads on this host) and hurts throughput.
    cv2.setNumThreads(1)
    counts, pending = Counter(), []
    if not jobs:
        return {}
    source = jobs[0][0]
    for job in jobs:
        if job[0] != source:
            raise ValueError("crop group mixes source images")
        path = Path(job[2])
        if path.is_file() and not job[3]:
            try:
                _strict_crop(path)
                counts["skipped"] += 1
                continue
            except ValueError:
                pass
        pending.append(job)
    if pending:
        image = cv2.imread(str(source))
        if image is None:
            raise RuntimeError("cannot decode " + str(source))
        for _, box, destination, _ in pending:
            path = Path(destination)
            path.parent.mkdir(parents=True, exist_ok=True)
            crop = make_search_crop(image, box, out_size=511)
            if not cv2.imwrite(str(path), crop, [int(cv2.IMWRITE_JPEG_QUALITY), 95]):
                raise RuntimeError("cannot write " + str(path))
            _strict_crop(path)
            counts["written"] += 1
    return dict(counts)


def prepare_source_pools(source, tracklets, train_videos, output_root, workers=8,
                         reuse_root=None, difficulty=None, required_categories=None):
    root = Path(output_root).resolve()
    normal = category_tracklets(tracklets, set(train_videos))
    hard = defaultdict(list)
    reasons, provenance, dimensions = {}, {}, {}
    for track in tracklets:
        if track.source_video not in dimensions:
            image = cv2.imread(track.frames[0].image_path)
            if image is None:
                raise ValueError("cannot decode " + track.frames[0].image_path)
            dimensions[track.source_video] = image.shape[:2]
        selected, flags = hard_tracklet(track, dimensions[track.source_video],
                                        (difficulty or {}).get(track.name))
        if selected:
            hard[track.category].append(selected)
            reasons[track.name] = flags
        provenance[track.name] = {"source_video": track.source_video, "category": track.category,
                                  "track_id": track.track_id,
                                  "frames": {str(f.frame_number): {"image_path": f.image_path,
                                             "bbox_xywh": f.bbox_xywh} for f in track.frames}}
    for category in required_categories or normal:
        if not hard.get(category):
            raise ValueError("required hard subpool is empty: {}/{}".format(source, category))
    manifest = _manifest(tracklets)
    old = {}
    if reuse_root:
        reuse_root = Path(reuse_root)
        old_path = reuse_root / ("annotations/train.json" if source == "UAV123" else "train.json")
        if old_path.is_file():
            old = json.loads(old_path.read_text())
    jobs, paths, reused = [], [], []
    for track in tracklets:
        for frame in track.frames:
            key = "{:06d}".format(frame.frame_number)
            path = root / "crop511" / track.name / (key + ".00.x.jpg")
            paths.append(path)
            previous = old.get(track.name, {}).get("00", {}).get(key)
            candidate = reuse_root / "crop511" / track.name / path.name if reuse_root else None
            if candidate and candidate.is_file() and previous == manifest[track.name]["00"][key]:
                path.parent.mkdir(parents=True, exist_ok=True)
                if not path.exists():
                    path.symlink_to(candidate.resolve())
                if path.resolve() != candidate.resolve():
                    _strict_crop(path)
                reused.append(path)
            else:
                jobs.append((frame.image_path, frame.bbox_xywh, str(path), False))
    # Dataset roots may be on a spinning disk. Read consecutive source images,
    # grouping targets on the same image, instead of seeking through each track.
    groups = defaultdict(list)
    for job in jobs:
        groups[job[0]].append(job)
    grouped_jobs = [groups[key] for key in sorted(groups)]
    counts = Counter()
    if workers <= 1:
        for group in grouped_jobs:
            counts.update(prepare_crop_group(group))
    else:
        with ProcessPoolExecutor(max_workers=workers) as executor:
            progress = 0
            for result in executor.map(prepare_crop_group, grouped_jobs, chunksize=4):
                before = progress
                counts.update(result)
                progress += sum(result.values())
                if progress // 2000 > before // 2000:
                    print("{} crops {}/{}".format(source, progress, len(jobs)), flush=True)
    if not all(path.is_file() for path in paths):
        raise ValueError("missing referenced crop")
    sample = random.Random(123456).sample(reused, min(512, len(reused)))
    for path in sample:
        _strict_crop(path)
    pools = {"source": source, "crop_root": str(root / "crop511"), "normal": {}, "hard": {}}
    for kind, groups in (("normal", normal), ("hard", hard)):
        for category, items in sorted(groups.items()):
            path = root / kind / (category + ".json")
            _write_json(path, _manifest(items))
            pools[kind][category] = {"annotation": str(path), "tracklets": len(items),
                                     "frames": sum(len(t.frames) for t in items),
                                     "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
    _write_json(root / "train.json", manifest)
    audit = {"source": source, "train_videos": sorted(train_videos), "pools": pools,
             "hard_scope": ("24-frame sampled train observations; neighbors in sampled tracklet"
                            if source in ("VisDrone-MOT", "UAVDT") else
                            "full normal training observations; nearest visible neighbors"),
             "hard_reasons": reasons, "provenance": provenance, "image_sizes_hw": dimensions,
             "crop_verification": {"referenced": len(paths), "reused": len(reused),
                                    "new_or_existing_strict_decoded": len(jobs),
                                    "source_image_groups": len(grouped_jobs),
                                    "reuse_sample_strict_decoded": len(sample), "writes": dict(counts)},
             "category_partition_union_equal": set().union(*(set(t.name for t in ts)
                                                           for ts in normal.values())) == set(manifest)}
    _write_json(root / "audit.json", audit)
    _write_json(root / "pools.json", pools)
    return pools


def mot_difficulty(tracklets, annotation_root, source):
    """Use raw same-frame/class annotations, including targets too short for training."""
    groups = defaultdict(list)
    for track in tracklets:
        groups[track.source_video].append(track)
    result = {}
    for video, tracks in sorted(groups.items()):
        wanted = {f.frame_number for t in tracks for f in t.frames}
        context, metadata = defaultdict(list), {}
        path = Path(annotation_root) / (video + (".txt" if source == "VisDrone-MOT" else "_gt_whole.txt"))
        for row in _read_csv_rows(path):
            number, target = int(row[0]), str(int(row[1]))
            if number not in wanted or int(target) <= 0:
                continue
            class_id = int(float(row[7] if source == "VisDrone-MOT" else row[8]))
            if source == "VisDrone-MOT":
                if int(float(row[6])) != 1 or _visdrone_category(class_id) == "ignore":
                    continue
            elif class_id not in (1, 2, 3):
                continue
            try:
                box = _valid_box(row[2:6])
            except ValueError:
                continue
            context[number].append((target, class_id, box))
            flags = []
            if source == "VisDrone-MOT":
                if int(float(row[8])) > 0:
                    flags.append("truncation")
                if int(float(row[9])) > 0:
                    flags.append("occlusion")
            metadata[(number, target)] = (class_id, flags)
        for track in tracks:
            flags_by_frame = {}
            for frame in track.frames:
                class_id, flags = metadata[(frame.frame_number, track.track_id)]
                flags = list(flags)
                if has_nearby_distractor(frame.bbox_xywh, track.track_id, class_id,
                                         context[frame.frame_number]):
                    flags.append("distractor")
                if flags:
                    flags_by_frame[frame.frame_number] = sorted(flags)
            result[track.name] = flags_by_frame
    return result


def verify_bandataset(pools, nanotrack_root):
    """Load and sample each actual source/category manifest through official BANDataset."""
    import numpy as np
    sys.path.insert(0, str(Path(nanotrack_root).resolve()))
    from nanotrack.core.config import cfg
    from nanotrack.datasets.dataset import BANDataset
    from yacs.config import CfgNode
    cfg.defrost()
    cfg.TRAIN.EPOCH = 1
    cfg.DATASET.VIDEOS_PER_EPOCH = 1
    cfg.DATASET.NEG = 0.0
    result = []
    for source, source_pools in sorted(pools.items()):
        for kind in ("normal", "hard"):
            for category, pool in sorted(source_pools[kind].items()):
                cfg.DATASET.NAMES = ("PREPARATION_CHECK",)
                cfg.DATASET.PREPARATION_CHECK = CfgNode({
                    "ROOT": source_pools["crop_root"], "ANNO": pool["annotation"],
                    "FRAME_RANGE": 100, "NUM_USE": 1})
                random.seed(123456)
                np.random.seed(123456)
                dataset = BANDataset()
                item = dataset[0]
                shapes = {key: list(value.shape) for key, value in item.items()}
                if shapes["template"] != [3, 127, 127] or shapes["search"] != [3, 255, 255]:
                    raise ValueError("unexpected BANDataset image shapes: " + str(shapes))
                if not all(np.isfinite(value).all() for value in item.values()):
                    raise ValueError("non-finite BANDataset sample")
                result.append({"source": source, "pool": kind, "category": category,
                               "shapes": shapes, "loaded_tracklets": dataset.all_dataset[0].num,
                               "finite": True})
    return result


def build_three_stage_data(workspace, dataset_root, output_root, workers=8, check_bandataset=True):
    workspace, dataset_root, output = Path(workspace).resolve(), Path(dataset_root).resolve(), Path(output_root).resolve()
    if output != workspace / "data/nanotrack_conservative_three_stage":
        raise ValueError("real preparation output must be data/nanotrack_conservative_three_stage")
    roots = {"VisDrone-SOT": dataset_root / "VisDrone2019-SOT",
             "VisDrone-MOT": dataset_root / "VisDrone2019-MOT", "UAVDT": dataset_root / "UAVDT"}
    print("Parsing full unsampled source tracks", flush=True)
    full = {"VisDrone-SOT": parse_visdrone_sot_train(roots["VisDrone-SOT"]),
            "VisDrone-MOT": parse_visdrone_mot_train(roots["VisDrone-MOT"], max_frames=0),
            "UAVDT": parse_uavdt_train(roots["UAVDT"], max_frames=0)}
    splits = split_source_videos(full, seed=123456)
    expected = {"VisDrone-SOT": (70, 8, 8), "VisDrone-MOT": (46, 5, 5), "UAVDT": (24, 3, 3)}
    for source in full:
        actual = tuple(len(splits[source][part]) for part in ("train", "validation", "hidden-test"))
        if actual != expected[source]:
            raise ValueError("unexpected source-video counts: {} {}".format(source, actual))
    # Evaluation is materialized from full tracks before any training sampler runs.
    print("Building consecutive evaluation streams before training sampling", flush=True)
    evaluation = {}
    for source, tracks in full.items():
        image_indices, streams = {}, []
        for track in tracks:
            if track.source_video in splits[source]["train"]:
                continue
            if track.source_video not in image_indices:
                image_indices[track.source_video] = _image_index(Path(track.frames[0].image_path).parent)
            streams.append(consecutive_evaluation_tracklet(track, image_indices[track.source_video]))
        evaluation[source] = streams
    output.mkdir(parents=True, exist_ok=True)
    limits = {"VisDrone-MOT": {"person": 30, "vehicle": 30, "bike": 10}, "UAVDT": {"vehicle": 20}}
    panels = {}
    for split, name in (("validation", "val-aerial"), ("hidden-test", "test-aerial-hidden")):
        panel = build_evaluation_panel(evaluation, splits, split, limits)
        path = output / (name + ".json")
        path.write_text(serialize_evaluation_panel(panel, split), encoding="utf-8")
        panels[name] = {"path": str(path), "sequences": len(panel),
                        "categories": dict(Counter(s.source + "/" + s.category for s in panel)),
                        "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
    del evaluation
    # Preserve the pre-existing physical-folder split and all UAV123 training frames.
    uav_root = dataset_root / "Dataset_UAV123_10fps/UAV123_10fps"
    prepared_uav = workspace / "data/uav123_nanotrack"
    fixed = json.loads((prepared_uav / "splits.json").read_text())
    specs = parse_config_seqs(uav_root / "configSeqs.m")
    uav_tracks = []
    for spec in specs:
        if spec.name not in fixed["logical_sequences"]["train"]:
            continue
        boxes = read_boxes(uav_root / "anno/UAV123_10fps" / (spec.name + ".txt"))
        frames = tuple(FrameRecord(spec.start_frame + i,
            str(uav_root / "data_seq/UAV123_10fps" / spec.folder /
                (str(spec.start_frame + i).zfill(spec.zero_padding) + "." + spec.extension)), box)
                       for i, box in enumerate(boxes) if box is not None)
        uav_tracks.append(Tracklet(spec.name, spec.folder, "00", category_for(spec.name), frames))
    splits["UAV123"] = {key: tuple(fixed["physical_folders"][old]) for key, old in
                         (("train", "train"), ("validation", "val"), ("hidden-test", "test"))}
    full["UAV123"] = uav_tracks
    _write_json(output / "source-video-splits.json", {"seed": 123456, "sources": splits,
                                                     "uav123_fixed_split": str(prepared_uav / "splits.json")})
    intersections = {}
    for source, groups in splits.items():
        intersections[source] = {a + "__" + b: sorted(set(groups[a]) & set(groups[b]))
                                for a, b in (("train", "validation"), ("train", "hidden-test"),
                                             ("validation", "hidden-test"))}
        if any(intersections[source].values()):
            raise ValueError("source split leakage")
    slugs = {"VisDrone-SOT": "visdrone_sot", "VisDrone-MOT": "visdrone_mot", "UAVDT": "uavdt", "UAV123": "uav123"}
    required = {"VisDrone-SOT": ("unknown",), "VisDrone-MOT": ("person", "vehicle", "bike"),
                "UAVDT": ("vehicle",), "UAV123": ("person", "vehicle", "other", "bike")}
    pools, category_counts = {}, {}
    for source, tracks in full.items():
        category_counts[source] = {split: dict(Counter(t.category for t in tracks if t.source_video in groups))
                                  for split, groups in splits[source].items()}
        selected = [t for t in tracks if t.source_video in splits[source]["train"]]
        if source in ("VisDrone-MOT", "UAVDT"):
            selected = [Tracklet(t.name, t.source_video, t.track_id, t.category,
                                  sample_training_frames(t.frames)) for t in selected]
        difficulty = {}
        if source in ("VisDrone-MOT", "UAVDT"):
            annotation_root = roots[source] / ("VisDrone2019-MOT-train/annotations" if source == "VisDrone-MOT"
                                                else "UAV-benchmark-MOTD_v1.0/GT")
            difficulty = mot_difficulty(selected, annotation_root, source)
        reuse = prepared_uav if source == "UAV123" else workspace / "data/nanotrack_four_dataset" / slugs[source]
        print("Preparing {}: {} train tracks, {} frames".format(source, len(selected), sum(len(t.frames) for t in selected)), flush=True)
        pools[source] = prepare_source_pools(source, selected, splits[source]["train"],
            output / slugs[source], workers, reuse, difficulty, required[source])
        print("Completed {} pools".format(source), flush=True)
    bandataset = verify_bandataset(pools, workspace / "repos/siamtrackers/NanoTrack") if check_bandataset else []
    summary = {"seed": 123456, "pools": pools, "panels": panels, "pairwise_intersections": intersections,
               "category_counts": category_counts, "bandataset": bandataset,
               "limitations": ["SOT sequence-level attributes are not treated as per-frame occlusion or truncation labels; hard flags use geometry.",
                                "UAV123 holdout category counts are recorded in existing splits.json; only train tracks are loaded here."],
               "sampling": {"VisDrone-MOT": 24, "UAVDT": 24, "VisDrone-SOT": "full", "UAV123": "full"},
               "hard_rules": {"small_area_fraction_lt": 0.01, "scale_max_min_gte": 2,
                              "motion_previous_diagonal_gt": 0.5, "distractor_current_diagonal_lte": 2,
                              "distractor_class": "exact raw class ID; valid same-frame target",
                              "hard_marks": "sampled MOT/UAVDT observations; full SOT/UAV123 observations"}}
    _write_json(output / "prepare-summary.json", summary)
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workspace", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--dataset-root", type=Path, default=Path("/home/u20/dataset"))
    parser.add_argument("--workers", type=int, default=min(16, len(os.sched_getaffinity(0))))
    args = parser.parse_args()
    result = build_three_stage_data(args.workspace, args.dataset_root,
                                   args.workspace / "data/nanotrack_conservative_three_stage", args.workers)
    print(json.dumps({"panels": result["panels"], "bandataset_checks": len(result["bandataset"])}, indent=2))


if __name__ == "__main__":
    main()
