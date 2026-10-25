#!/usr/bin/env python3
"""Generate immutable, sample-exact NanoTrack factorial configurations."""

import argparse
import copy
import hashlib
import json
from fractions import Fraction
from pathlib import Path

import yaml


ASSET_ROOT = Path("/home/u20/single-object-tracking")
CONFIG_DIR = Path(__file__).resolve().parents[1] / "configs/nanotrack_factorial_v3"
EXPECTED_HASHES = {
    "pair_policy": "e33eb9423b08879b6b6f9068711e1617298a6fceb4a5ca4241f6a5bd45338024",
    "pair_audit": "69b4d5d253ef40894f34a3648af31872e5676a40ce8d8a2f98a44cf04adb1ca2",
    "initial_checkpoint": "40ebbe4296938bf5d53c8605ab05e729b306fc0bce65921a57ab6613030410d1",
    "source_config": "cb3065982818a701a7ebb469c41b894b0a6c11e7aac6cb91dc42e5ed50547f2e",
}
INPUT_RELATIVE_PATHS = {
    "pair_policy": "artifacts/nanotrack_factorial_v3/pair-audit/pair-policy.json",
    "pair_audit": "artifacts/nanotrack_factorial_v3/pair-audit/audit.json",
    "initial_checkpoint": "artifacts/nanotrack_conservative_three_stage/run/stage1/candidate_stage1_g00005000_e001_s02500.pth",
    "source_config": "configs/nanotrack_v3_three_stage_stage1.yaml",
}
AERIAL_WEIGHTS = {
    "UAV123": 65, "UAVDT": 90, "VISDRONE_MOT": 90, "VISDRONE_SOT": 35,
}
DOMAIN_CATEGORIES = {
    "UAV123": {
        "UAV123_BIKE_NORMAL": 1, "UAV123_OTHER_NORMAL": 3,
        "UAV123_PERSON_NORMAL": 10, "UAV123_VEHICLE_NORMAL": 6,
    },
    "UAVDT": {"UAVDT_VEHICLE_NORMAL": 1},
    "VISDRONE_MOT": {
        "VISDRONE_MOT_BIKE_NORMAL": 1,
        "VISDRONE_MOT_PERSON_NORMAL": 6,
        "VISDRONE_MOT_VEHICLE_NORMAL": 3,
    },
    "VISDRONE_SOT": {"VISDRONE_SOT_UNKNOWN_NORMAL": 1},
}
GOT_NAMES = ("GOT_OTHER_NORMAL", "GOT_PERSON_NORMAL", "GOT_VEHICLE_NORMAL")


def _sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _allocate(weights, total):
    """Largest remainders with exact rational arithmetic and name tie-breaking."""
    denominator = sum(weights.values())
    exact = {name: Fraction(total * weight, denominator)
             for name, weight in weights.items()}
    result = {name: int(value) for name, value in exact.items()}
    remaining = total - sum(result.values())
    ranked = sorted(result, key=lambda name: (-(exact[name] - result[name]), name))
    for name in ranked[:remaining]:
        result[name] += 1
    return dict(sorted(result.items()))


def _quotas(got_percent):
    got_total = 40960 * got_percent // 100
    domain_totals = _allocate(AERIAL_WEIGHTS, 40960 - got_total)
    quotas = {}
    if got_total:
        quotas.update(_allocate(dict.fromkeys(GOT_NAMES, 1), got_total))
    for domain, total in domain_totals.items():
        quotas.update(_allocate(DOMAIN_CATEGORIES[domain], total))
    return dict(sorted(quotas.items()))


def _input_paths(asset_root):
    paths = {name: asset_root / relative
             for name, relative in INPUT_RELATIVE_PATHS.items()}
    for name, path in paths.items():
        observed = _sha256(path)
        if observed != EXPECTED_HASHES[name]:
            raise ValueError("{} SHA-256 mismatch: {}".format(name, observed))
    return paths


def _validate_frozen_mix(source):
    data = source["DATASET"]
    for domain in ("UAV123", "VISDRONE_MOT"):
        weights = DOMAIN_CATEGORIES[domain]
        observed = {name: data[name]["NUM_USE"] for name in weights}
        ratios = {name: observed[name] // weights[name] for name in weights}
        if any(observed[name] != ratios[name] * weights[name] for name in weights) or len(set(ratios.values())) != 1:
            raise ValueError("frozen Stage 1 {} category mix changed".format(domain))


def _got_entry(asset_root, name, num_use):
    category = name[len("GOT_"):-len("_NORMAL")].lower()
    base = asset_root / "data/nanotrack_got_replay_v1/got"
    return {
        "ROOT": str(base / "crop511"),
        "ANNO": str(base / "normal" / (category + ".json")),
        "FRAME_RANGE": 100,
        "NUM_USE": num_use,
    }


def _config(source, asset_root, batch, got_percent):
    key = "b{}_g{}".format(batch, got_percent)
    config = copy.deepcopy(source)
    config["META_ARC"] = "nanotrack-v3-factorial-{}-head-only".format(key)
    config["BACKBONE"]["TRAIN_EPOCH"] = 2
    train = config["TRAIN"]
    train["EPOCH"] = 1
    train["START_EPOCH"] = 0
    train["BATCH_SIZE"] = batch
    train["BASE_LR"] = 2e-5
    train["SNAPSHOT_DIR"] = str(asset_root / "artifacts/nanotrack_factorial_v3/run" / key)
    train["LR"] = {"TYPE": "log", "KWARGS": {"start_lr": 2e-5, "end_lr": 8e-6}}
    train["LR_WARMUP"] = {"WARMUP": False, "TYPE": "step", "EPOCH": 0,
                          "KWARGS": {"start_lr": 2e-5, "end_lr": 2e-5, "step": 1}}
    quotas = _quotas(got_percent)
    data = config["DATASET"]
    for name in list(data):
        if name.endswith("_NORMAL"):
            del data[name]
    data["NAMES"] = list(quotas)
    data["VIDEOS_PER_EPOCH"] = 40960
    for name, num_use in quotas.items():
        if name.startswith("GOT_"):
            data[name] = _got_entry(asset_root, name, num_use)
        else:
            data[name] = copy.deepcopy(source["DATASET"][name])
            data[name]["NUM_USE"] = num_use
        for field in ("ROOT", "ANNO"):
            path = Path(data[name][field])
            if not path.is_absolute() or not str(path).startswith(str(asset_root / "data") + "/"):
                raise ValueError("dataset path outside asset root: {}".format(path))
    return config, quotas


def generate_configs(output_dir=CONFIG_DIR, asset_root=ASSET_ROOT):
    """Verify frozen inputs, then create or verify all ten immutable files."""
    asset_root = Path(asset_root).resolve()
    output_dir = Path(output_dir).resolve()
    paths = _input_paths(asset_root)
    source = yaml.safe_load(paths["source_config"].read_text(encoding="utf-8"))
    _validate_frozen_mix(source)
    artifact_root = asset_root / "artifacts/nanotrack_factorial_v3"
    manifest = {
        "schema_version": 1,
        "experiment": "nanotrack-factorial-v3",
        "scoring_version": "nanotrack-category-first-v3",
        "asset_root": str(asset_root),
        "output_root": str(artifact_root),
        "source_config": str(paths["source_config"]),
        "input_paths": {name: str(path) for name, path in paths.items()},
        "input_hashes": EXPECTED_HASHES.copy(),
        "screening_seeds": [123456, 20261011],
        "confirmation_seeds": [123456, 20261011, 20261024],
        "cells": {},
    }
    generated = {}
    dataset_hashes = {}
    for batch in (8, 32, 64):
        for got_percent in (0, 10, 30):
            key = "b{}_g{}".format(batch, got_percent)
            config, quotas = _config(source, asset_root, batch, got_percent)
            content = yaml.safe_dump(config, sort_keys=False).encode("utf-8")
            generated[key + ".yaml"] = content
            for name in quotas:
                annotation = Path(config["DATASET"][name]["ANNO"])
                if name not in dataset_hashes:
                    dataset_hashes[name] = {"path": str(annotation), "sha256": _sha256(annotation)}
            manifest["cells"][key] = {
                "batch_size": batch,
                "got_percent": got_percent,
                "quotas": quotas,
                "full_samples": 40960,
                "screening_samples": 10240,
                "evaluation_interval_samples": 2560,
                "full_updates": 40960 // batch,
                "screening_updates": 10240 // batch,
                "evaluation_interval_updates": 2560 // batch,
                "learning_rate": {"start": 2e-5, "end": 8e-6,
                                  "schedule": "log_by_samples"},
                "seeds": [123456, 20261011, 20261024],
                "input_hashes": EXPECTED_HASHES.copy(),
                "pair_policy_path": str(paths["pair_policy"]),
                "initial_checkpoint": str(paths["initial_checkpoint"]),
                "config_path": str(output_dir / (key + ".yaml")),
                "config_sha256": hashlib.sha256(content).hexdigest(),
                "output_dir": str(artifact_root / "run" / key),
            }
    manifest["dataset_manifests"] = dict(sorted(dataset_hashes.items()))
    generated["experiment-manifest.json"] = (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode("utf-8")
    for name, content in generated.items():
        target = output_dir / name
        if target.exists() and target.read_bytes() != content:
            raise ValueError("refusing to replace existing configuration: {}".format(target))
    output_dir.mkdir(parents=True, exist_ok=True)
    for name, content in generated.items():
        target = output_dir / name
        if not target.exists():
            with target.open("xb") as stream:
                stream.write(content)
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=CONFIG_DIR)
    parser.add_argument("--asset-root", type=Path, default=ASSET_ROOT)
    args = parser.parse_args()
    manifest = generate_configs(args.output_dir, args.asset_root)
    print("{}: {} cells".format(args.output_dir.resolve(), len(manifest["cells"])))


if __name__ == "__main__":
    main()
