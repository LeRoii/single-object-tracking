#!/usr/bin/env python3
"""Generate sample-exact NanoTrack GOT replay stage configurations."""

import argparse
import json
import math
from pathlib import Path
from typing import Mapping

import yaml


WORKSPACE = Path(__file__).resolve().parents[1]


STAGE_REFERENCE_QUOTAS = {
    "stage1": {
        "GOT_PERSON_NORMAL": 2000,
        "GOT_VEHICLE_NORMAL": 2000,
        "GOT_OTHER_NORMAL": 2000,
        "UAV123_PERSON_NORMAL": 1500,
        "UAV123_VEHICLE_NORMAL": 1000,
        "UAV123_OTHER_NORMAL": 500,
        "UAV123_BIKE_NORMAL": 250,
        "VISDRONE_SOT_UNKNOWN_NORMAL": 4500,
        "VISDRONE_MOT_PERSON_NORMAL": 2500,
        "VISDRONE_MOT_VEHICLE_NORMAL": 1500,
        "VISDRONE_MOT_BIKE_NORMAL": 500,
        "UAVDT_VEHICLE_NORMAL": 1750,
    },
    "stage2": {
        "GOT_PERSON_NORMAL": 1750,
        "GOT_VEHICLE_NORMAL": 1750,
        "GOT_OTHER_NORMAL": 1500,
        "UAV123_PERSON_NORMAL": 900,
        "UAV123_VEHICLE_NORMAL": 600,
        "UAV123_OTHER_NORMAL": 350,
        "UAV123_BIKE_NORMAL": 150,
        "VISDRONE_SOT_UNKNOWN_NORMAL": 3500,
        "VISDRONE_MOT_PERSON_NORMAL": 1500,
        "VISDRONE_MOT_VEHICLE_NORMAL": 1200,
        "VISDRONE_MOT_BIKE_NORMAL": 300,
        "UAVDT_VEHICLE_NORMAL": 1500,
        "UAV123_PERSON_HARD": 300,
        "UAV123_VEHICLE_HARD": 250,
        "UAV123_OTHER_HARD": 150,
        "UAV123_BIKE_HARD": 50,
        "VISDRONE_SOT_UNKNOWN_HARD": 1250,
        "VISDRONE_MOT_PERSON_HARD": 1000,
        "VISDRONE_MOT_VEHICLE_HARD": 800,
        "VISDRONE_MOT_BIKE_HARD": 200,
        "UAVDT_VEHICLE_HARD": 1000,
    },
}


def allocate_integer_quotas(weights: Mapping[str, float], total: int) -> dict:
    """Allocate an exact integer total by deterministic largest remainder."""
    target = int(total)
    if target <= 0 or not weights:
        raise ValueError("quota weights and total must be positive")
    normalized = {}
    for name, raw_value in weights.items():
        value = float(raw_value)
        if not math.isfinite(value) or value <= 0:
            raise ValueError("quota weights must be positive and finite")
        normalized[str(name)] = value
    weight_sum = sum(normalized.values())
    exact = {name: target * value / weight_sum for name, value in normalized.items()}
    allocated = {name: int(math.floor(value)) for name, value in exact.items()}
    remaining = target - sum(allocated.values())
    ranking = sorted(
        allocated,
        key=lambda name: (-(exact[name] - allocated[name]), name),
    )
    for name in ranking[:remaining]:
        allocated[name] += 1
    return {name: allocated[name] for name in sorted(allocated)}


def build_stage_quotas(stage: str, total_samples: int) -> dict:
    try:
        reference = STAGE_REFERENCE_QUOTAS[str(stage)]
    except KeyError:
        raise ValueError("unknown GOT replay stage {}".format(stage))
    return allocate_integer_quotas(reference, int(total_samples))


def _dataset_entry(name: str, num_use: int) -> dict:
    hardness = "hard" if name.endswith("_HARD") else "normal"
    if name.startswith("GOT_"):
        source, frame_range = "got", 100
        category = name[len("GOT_"):].rsplit("_", 1)[0].lower()
        base = WORKSPACE / "data/nanotrack_got_replay_v1/got"
    elif name.startswith("UAV123_"):
        source, frame_range = "uav123", 50
        category = name[len("UAV123_"):].rsplit("_", 1)[0].lower()
        base = WORKSPACE / "data/nanotrack_conservative_three_stage/uav123"
    elif name.startswith("UAVDT_"):
        source, frame_range = "uavdt", 30
        category = name[len("UAVDT_"):].rsplit("_", 1)[0].lower()
        base = WORKSPACE / "data/nanotrack_conservative_three_stage/uavdt"
    elif name.startswith("VISDRONE_SOT_"):
        source, frame_range = "visdrone_sot", 100
        category = name[len("VISDRONE_SOT_"):].rsplit("_", 1)[0].lower()
        base = WORKSPACE / "data/nanotrack_conservative_three_stage/visdrone_sot"
    elif name.startswith("VISDRONE_MOT_"):
        source, frame_range = "visdrone_mot", 30
        category = name[len("VISDRONE_MOT_"):].rsplit("_", 1)[0].lower()
        base = WORKSPACE / "data/nanotrack_conservative_three_stage/visdrone_mot"
    else:
        raise ValueError("unknown dataset quota name {}".format(name))
    del source
    return {
        "ROOT": str((base / "crop511").resolve()),
        "ANNO": str((base / hardness / (category + ".json")).resolve()),
        "FRAME_RANGE": frame_range,
        "NUM_USE": int(num_use),
    }


def build_stage_config(
    stage: str,
    batch_size: int,
    interval_samples: int,
    start_lr: float,
    end_lr: float,
    artifact_root: Path = WORKSPACE / "artifacts/nanotrack_got_replay_v1",
) -> dict:
    max_intervals = {"stage1": 4, "stage2": 3}
    if stage not in max_intervals:
        raise ValueError("unknown GOT replay stage {}".format(stage))
    batch = int(batch_size)
    interval = int(interval_samples)
    if batch <= 0 or interval <= 0 or interval % batch:
        raise ValueError("interval samples must be a positive multiple of batch size")
    total_samples = interval * max_intervals[stage]
    quotas = build_stage_quotas(stage, total_samples)
    config = {
        "META_ARC": "nanotrack-v3-got-replay-{}-head-only".format(stage),
        "BACKBONE": {
            "TYPE": "mobilenetv3_small_v3",
            "KWARGS": {"used_layers": [4]},
            "PRETRAINED": "",
            "TRAIN_LAYERS": ["features"],
            "TRAIN_EPOCH": 2,
            "LAYERS_LR": 0.05,
        },
        "ADJUST": {"ADJUST": True, "TYPE": "AdjustLayer",
                   "KWARGS": {"in_channels": 96, "out_channels": 96}},
        "BAN": {"BAN": True, "TYPE": "DepthwiseBAN",
                "KWARGS": {"in_channels": 96, "out_channels": 96}},
        "CUDA": True,
        "POINT": {"STRIDE": 16},
        "TRACK": {
            "TYPE": "NanoTracker", "WINDOW_INFLUENCE": 0.455,
            "PENALTY_K": 0.138, "LR": 0.348, "EXEMPLAR_SIZE": 127,
            "INSTANCE_SIZE": 255, "BASE_SIZE": 7, "CONTEXT_AMOUNT": 0.5,
            "OUTPUT_SIZE": 15,
        },
        "TRAIN": {
            "EPOCH": 1, "START_EPOCH": 0, "BATCH_SIZE": batch,
            "NUM_WORKERS": 0, "BASE_LR": float(start_lr),
            "CLS_WEIGHT": 1.0, "LOC_WEIGHT": 1.0, "NUM_CONVS": 4,
            "BASE_SIZE": 7, "OUTPUT_SIZE": 15, "PRINT_FREQ": 25,
            "GRAD_CLIP": 10.0, "LOG_GRADS": False, "RESUME": "",
            "PRETRAINED": "",
            "SNAPSHOT_DIR": str((Path(artifact_root) / "run" / stage).resolve()),
            "LR": {"TYPE": "log", "KWARGS": {
                "start_lr": float(start_lr), "end_lr": float(end_lr)}},
            "LR_WARMUP": {"WARMUP": False, "TYPE": "step", "EPOCH": 0,
                          "KWARGS": {"start_lr": float(start_lr),
                                     "end_lr": float(start_lr), "step": 1}},
        },
        "DATASET": {
            "NAMES": sorted(quotas),
            "VIDEOS_PER_EPOCH": total_samples,
            "TEMPLATE": {"SHIFT": 4, "SCALE": 0.05, "BLUR": 0.0,
                         "FLIP": 0.0, "COLOR": 1.0},
            "SEARCH": {"SHIFT": 64, "SCALE": 0.18, "BLUR": 0.2,
                       "FLIP": 0.0, "COLOR": 1.0},
            "NEG": 0.2,
            "GRAY": 0.0,
        },
    }
    for name, num_use in quotas.items():
        config["DATASET"][name] = _dataset_entry(name, num_use)
    return config


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--calibration", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path,
                        default=WORKSPACE / "configs/nanotrack_got_replay_v1")
    parser.add_argument(
        "--artifact-root", type=Path,
        default=WORKSPACE / "artifacts/nanotrack_got_replay_v1",
    )
    args = parser.parse_args()
    calibration = json.loads(args.calibration.read_text(encoding="utf-8"))
    batch = int(calibration["selected_batch_size"])
    interval_samples = int(calibration["candidate_interval"]["samples"])
    args.output_dir.mkdir(parents=True, exist_ok=True)
    for stage in ("stage1", "stage2"):
        lr = calibration[stage + "_lr"]
        config = build_stage_config(
            stage, batch, interval_samples, float(lr["start"]), float(lr["end"]),
            artifact_root=args.artifact_root,
        )
        output = args.output_dir / (stage + ".yaml")
        temporary = output.with_name(output.name + ".tmp")
        temporary.write_text(
            yaml.safe_dump(config, sort_keys=False, default_flow_style=False),
            encoding="utf-8",
        )
        temporary.replace(output)
        print(output.resolve())


if __name__ == "__main__":
    main()
