#!/usr/bin/env python3
"""Verify committed evidence for the NanoTrack GOT replay experiment."""

import argparse
import json
import sys
from pathlib import Path

import yaml

WORKSPACE = Path(__file__).resolve().parents[1]
if str(WORKSPACE) not in sys.path:
    sys.path.insert(0, str(WORKSPACE))

from scripts.nanotrack_got_replay_training import accepted_memory_sample
from scripts.run_nanotrack_got_replay import _atomic_json, _sha256


def classify_selection_outcome(selection, table, final_exists):
    """Validate selected and no-selection terminal states without touching hidden data."""
    candidates = table.get("candidates", [])
    if selection.get("selection_final") is True:
        selected = selection.get("selected")
        if not isinstance(selected, dict):
            raise ValueError("final selection is missing selected candidate")
        if selected.get("final_gate", {}).get("qualified") is not True:
            raise ValueError("selected checkpoint did not pass final gate")
        if selection.get("hidden_evaluation_completed") is not True or not final_exists:
            raise ValueError("hidden evaluation is incomplete")
        return {
            "outcome": "selected",
            "selected_key": selected["key"],
            "hidden_evaluation_completed": True,
        }
    if selection.get("selected") is not None or selection.get("eligible_keys"):
        raise ValueError("no-selection outcome contains selected/eligible candidates")
    if selection.get("hidden_evaluation_started") is not False or final_exists:
        raise ValueError("hidden evaluation is forbidden without a selected model")
    if any(item.get("final_gate", {}).get("qualified") is True for item in candidates):
        raise ValueError("qualified candidate is inconsistent with no-selection outcome")
    if len(candidates) != 8:
        raise ValueError("no-selection outcome requires all eight Stage-1 candidates")
    expected_seeds = {"123456", "20261011"}
    seed_state = table.get("seeds", {})
    if set(seed_state) != expected_seeds:
        raise ValueError("no-selection outcome requires both seed states")
    for seed in expected_seeds:
        state = seed_state[seed]
        if state.get("stage1_best") is not None:
            raise ValueError("no-selection seed unexpectedly has a Stage-1 best")
        if state.get("stage2_skipped") != "no qualified stage1 candidate":
            raise ValueError("Stage 2 skip reason is missing")
    return {
        "outcome": "no_candidate_passed",
        "selected_key": None,
        "hidden_evaluation_completed": False,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifact-root", type=Path,
                        default=WORKSPACE / "artifacts/nanotrack_got_replay_v1")
    parser.add_argument("--config-dir", type=Path,
                        default=WORKSPACE / "configs/nanotrack_got_replay_v1")
    args = parser.parse_args()
    root = args.artifact_root.resolve()
    data_root = WORKSPACE / "data/nanotrack_got_replay_v1"
    inventory = json.loads((data_root / "got-train-split.json").read_text())
    prep = json.loads((data_root / "prepare-summary.json").read_text())
    train = set(inventory["splits"]["train"])
    hidden = set(inventory["splits"]["hidden"])
    if train & hidden or len(hidden) != 467 or len(train) != 8852:
        raise ValueError("GOT split cardinality/disjointness audit failed")
    observed_names = set()
    observed_frames = 0
    pool_counts = {}
    for pool in ("person", "vehicle", "other"):
        manifest_path = data_root / "got/normal" / (pool + ".json")
        manifest = json.loads(manifest_path.read_text())
        names = set(manifest)
        if observed_names & names:
            raise ValueError("GOT manifests overlap")
        observed_names.update(names)
        pool_counts[pool] = len(names)
        observed_frames += sum(len(tracks["00"]) for tracks in manifest.values())
        if _sha256(manifest_path) != prep["manifest_hashes"][pool]:
            raise ValueError("manifest hash changed: {}".format(pool))
    if observed_names != train or pool_counts != prep["pool_sequence_counts"]:
        raise ValueError("GOT manifests are not an exact training partition")
    if observed_frames != int(prep["selected_frame_count"]):
        raise ValueError("manifest frame count differs from preparation summary")
    crop_count = sum(1 for _ in (data_root / "got/crop511").rglob("*.x.jpg"))
    if crop_count != observed_frames:
        raise ValueError("crop count {} differs from {}".format(crop_count, observed_frames))

    calibration = json.loads((root / "calibration.json").read_text())
    selected_batch = int(calibration["selected_batch_size"])
    accepted = [record for record in calibration["records"]
                if int(record["batch_size"]) == selected_batch]
    if len(accepted) != 1 or not accepted_memory_sample(accepted[0]):
        raise ValueError("selected calibration record is not safe")
    if not all(record["accepted"] == accepted_memory_sample(record)
               for record in calibration["records"]):
        raise ValueError("calibration accepted flags do not match policy")
    configs = {}
    for stage, intervals in (("stage1", 4), ("stage2", 3)):
        path = args.config_dir.resolve() / (stage + ".yaml")
        config = yaml.safe_load(path.read_text())
        if int(config["TRAIN"]["BATCH_SIZE"]) != selected_batch:
            raise ValueError("{} batch does not match calibration".format(stage))
        expected = int(calibration["candidate_interval"]["samples"]) * intervals
        if int(config["DATASET"]["VIDEOS_PER_EPOCH"]) != expected:
            raise ValueError("{} sample total mismatch".format(stage))
        if sum(int(config["DATASET"][name]["NUM_USE"])
               for name in config["DATASET"]["NAMES"]) != expected:
            raise ValueError("{} quotas do not close".format(stage))
        expected_lr = calibration[stage + "_lr"]
        observed_lr = config["TRAIN"]["LR"]["KWARGS"]
        if float(observed_lr["start_lr"]) != float(expected_lr["start"]):
            raise ValueError("{} start LR does not match calibration".format(stage))
        if float(observed_lr["end_lr"]) != float(expected_lr["end"]):
            raise ValueError("{} end LR does not match calibration".format(stage))
        configs[stage] = _sha256(path)

    table = json.loads((root / "run/candidate_table.json").read_text())
    seeds = {int(item["seed"]) for item in table["candidates"]}
    if seeds != {123456, 20261011}:
        raise ValueError("both required seeds did not produce candidates")
    for item in table["candidates"]:
        checkpoint = Path(item["checkpoint"])
        if _sha256(checkpoint) != item["checkpoint_sha256"]:
            raise ValueError("candidate checkpoint hash changed")
        if calibration.get("correction", {}).get("lr_schedule") == "step_log":
            import torch
            payload = torch.load(str(checkpoint), map_location="cpu")
            if payload.get("lr_schedule") != "step":
                raise ValueError("candidate did not persist step LR schedule")
            if payload.get("policy") != "head_only":
                raise ValueError("candidate is not head-only")
    selection = json.loads((root / "run/selection.json").read_text())
    final_path = root / "final/summary.json"
    outcome = classify_selection_outcome(selection, table, final_path.is_file())
    selected = selection.get("selected")
    if outcome["outcome"] == "selected":
        final = json.loads(final_path.read_text())
        if final.get("video_regression_executed") is not False:
            raise ValueError("video regression boundary was violated")
        if final["snapshot_sha256"] != selected["checkpoint_sha256"]:
            raise ValueError("final summary is not bound to selected checkpoint")
    for name in ("REPORT.md", "REPORT.zh-CN.md"):
        if not (root / name).is_file():
            raise FileNotFoundError(str(root / name))
    verification = {
        "format_version": 1,
        "verified": True,
        "train_sequences": len(train),
        "hidden_sequences": len(hidden),
        "selected_frames_and_crops": crop_count,
        "selected_batch_size": selected_batch,
        "candidate_count": len(table["candidates"]),
        "seeds": sorted(seeds),
        "outcome": outcome["outcome"],
        "selected_key": outcome["selected_key"],
        "selected_checkpoint_sha256": (
            selected["checkpoint_sha256"] if selected is not None else None
        ),
        "config_sha256": configs,
        "hidden_evaluation_completed": outcome["hidden_evaluation_completed"],
        "video_regression_executed": False,
    }
    _atomic_json(root / "verification.json", verification)
    print(json.dumps(verification, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
