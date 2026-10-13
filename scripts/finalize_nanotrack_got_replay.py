#!/usr/bin/env python3
"""Run the one-time hidden panel and non-video regressions after frozen selection."""

import argparse
import json
import sys
from pathlib import Path


WORKSPACE = Path(__file__).resolve().parents[1]
if str(WORKSPACE) not in sys.path:
    sys.path.insert(0, str(WORKSPACE))

from scripts.run_nanotrack_got_replay import (
    NANOTRACK_ROOT,
    _atomic_json,
    _run_streamed,
    _sha256,
    _validated_existing_result,
    hidden_evaluation_action,
)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifact-root", type=Path,
                        default=WORKSPACE / "artifacts/nanotrack_got_replay_v1")
    args = parser.parse_args()
    root = args.artifact_root.resolve()
    selection_path = root / "run/selection.json"
    selection = json.loads(selection_path.read_text(encoding="utf-8"))
    selected = selection.get("selected")
    if not isinstance(selected, dict):
        raise ValueError("selection has no selected candidate")
    snapshot = Path(selected["checkpoint"]).resolve()
    config = Path(selected["metrics"]["config"]).resolve()
    expected_hash = str(selected["checkpoint_sha256"])
    if _sha256(snapshot) != expected_hash:
        raise ValueError("selected checkpoint hash changed")
    final_root = root / "final"
    logs = final_root / "logs"

    hidden_path = final_root / "got-train-hidden.json"
    action = hidden_evaluation_action(selection, hidden_path.is_file())
    if action == "run":
        selection["hidden_evaluation_started"] = True
        selection["hidden_evaluation_command"] = [
            sys.executable, str(WORKSPACE / "scripts/evaluate_got10k_nanotrack.py"),
            "--subset", "train-hidden", "--dataset-root", "/home/u20/dataset/GOT-10K",
            "--inventory", str(WORKSPACE / "data/nanotrack_got_replay_v1/got-train-split.json"),
            "--selection", str(selection_path),
            "--nanotrack-root", str(NANOTRACK_ROOT), "--config", str(config),
            "--snapshot", str(snapshot), "--output", str(hidden_path),
            "--device", "cuda:0",
        ]
        _atomic_json(selection_path, selection)
        _run_streamed(selection["hidden_evaluation_command"],
                      logs / "got-train-hidden.log")
    hidden = _validated_existing_result(hidden_path, snapshot)
    if hidden is None:
        raise RuntimeError("hidden evaluation did not commit a result")
    selection["hidden_evaluation_completed"] = True
    selection["hidden_evaluation_snapshot_sha256"] = expected_hash
    _atomic_json(selection_path, selection)

    uav_dir = final_root / "uav123-test"
    uav_path = uav_dir / "summary.json"
    uav = _validated_existing_result(uav_path, snapshot)
    if uav is None:
        command = [
            sys.executable, str(WORKSPACE / "scripts/evaluate_uav123_nanotrack.py"),
            "--dataset-root", "/home/u20/dataset/Dataset_UAV123_10fps/UAV123_10fps",
            "--prepared-root", str(WORKSPACE / "data/uav123_nanotrack"),
            "--nanotrack-root", str(NANOTRACK_ROOT), "--config", str(config),
            "--snapshot", str(snapshot), "--split", "test",
            "--output-dir", str(uav_dir),
        ]
        _run_streamed(command, logs / "uav123-test.log")
        uav = _validated_existing_result(uav_path, snapshot)

    vis_dir = final_root / "visdrone-sot-val"
    vis_path = vis_dir / "summary.json"
    vis = _validated_existing_result(vis_path, snapshot)
    if vis is None:
        command = [
            sys.executable, str(WORKSPACE / "scripts/evaluate_visdrone_sot_nanotrack.py"),
            "--dataset-root", "/home/u20/dataset/VisDrone2019-SOT",
            "--nanotrack-root", str(NANOTRACK_ROOT), "--config", str(config),
            "--snapshot", str(snapshot), "--split", "val",
            "--output-dir", str(vis_dir),
        ]
        _run_streamed(command, logs / "visdrone-sot-val.log")
        vis = _validated_existing_result(vis_path, snapshot)
    assert uav is not None and vis is not None
    summary = {
        "format_version": 1,
        "selected_key": selected["key"],
        "snapshot": str(snapshot),
        "snapshot_sha256": expected_hash,
        "selection_metrics": selected["metrics"],
        "got_train_hidden": hidden["metrics"],
        "uav123_test_auc": uav["overall"]["success_auc"],
        "visdrone_sot_val_auc": vis["overall"]["success_auc"],
        "video_regression_executed": False,
    }
    _atomic_json(final_root / "summary.json", summary)
    print(json.dumps(summary, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
