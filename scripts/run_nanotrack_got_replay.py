#!/usr/bin/env python3
"""Two-stage NanoTrack GOT replay controller and selection policy."""

import argparse
import hashlib
import json
import math
import os
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Mapping, Optional


EPSILON = 1e-12
TARGET_WEIGHTS = {
    "person_auc": 0.4,
    "vehicle_auc": 0.4,
    "overall_auc": 0.2,
}
WORKSPACE = Path(__file__).resolve().parents[1]
NANOTRACK_ROOT = WORKSPACE / "repos/siamtrackers/NanoTrack"
TRAINER = WORKSPACE / "scripts/train_nanotrack_uav123.py"
INITIAL_CHECKPOINT = (
    WORKSPACE / "artifacts/nanotrack_conservative_three_stage/run/stage1/"
    "candidate_stage1_g00005000_e001_s02500.pth"
)


def _finite(value: object, label: str) -> float:
    number = float(value)
    if not math.isfinite(number):
        raise ValueError("{} must be finite".format(label))
    return number


def _optional(value: object, label: str) -> Optional[float]:
    if value is None:
        return None
    return _finite(value, label)


def target_score(metrics: Mapping[str, object]) -> float:
    domains = metrics.get("domains")
    if not isinstance(domains, Mapping) or not domains:
        raise ValueError("target metrics require at least one domain")
    domain_scores = []
    for domain, raw_values in sorted(domains.items()):
        if not isinstance(raw_values, Mapping):
            raise ValueError("domain {} metrics must be a mapping".format(domain))
        numerator = 0.0
        denominator = 0.0
        for name, weight in TARGET_WEIGHTS.items():
            value = _optional(raw_values.get(name), "{}.{}".format(domain, name))
            if value is not None:
                numerator += weight * value
                denominator += weight
        if denominator <= 0:
            raise ValueError("domain {} has no target score terms".format(domain))
        domain_scores.append(numerator / denominator)
    return sum(domain_scores) / len(domain_scores)


def category_macro(metrics: Mapping[str, object], category: str) -> Optional[float]:
    key = category + "_auc"
    domains = metrics.get("domains")
    if not isinstance(domains, Mapping):
        raise ValueError("target metrics require domains")
    values = []
    for domain, raw_values in domains.items():
        if not isinstance(raw_values, Mapping):
            raise ValueError("domain {} metrics must be a mapping".format(domain))
        value = _optional(raw_values.get(key), "{}.{}".format(domain, key))
        if value is not None:
            values.append(value)
    return sum(values) / len(values) if values else None


def composite_score(metrics: Mapping[str, object]) -> float:
    return (
        0.70 * target_score(metrics)
        + 0.15 * _finite(metrics["got_val_ao"], "got_val_ao")
        + 0.15 * _finite(metrics["uav123_val_auc"], "uav123_val_auc")
    )


def replacement_decision(
    candidate: Mapping[str, object], stage_best: Mapping[str, object]
) -> dict:
    score = composite_score(candidate)
    best_score = composite_score(stage_best)
    reasons = []
    if score + EPSILON < best_score + 0.002:
        reasons.append("composite_improvement_below_0.002")
    for key in ("got_val_ao", "uav123_val_auc"):
        current = _finite(candidate[key], key)
        reference = _finite(stage_best[key], key)
        if reference - current > 0.01 + EPSILON:
            reasons.append("{}_drop_over_0.01".format(key))
    return {
        "qualified": not reasons,
        "candidate_score": score,
        "stage_best_score": best_score,
        "improvement": score - best_score,
        "reasons": reasons,
    }


def final_gate(candidate: Mapping[str, object], baseline: Mapping[str, object]) -> dict:
    reasons = []
    candidate_target = target_score(candidate)
    baseline_target = target_score(baseline)
    if candidate_target + EPSILON < baseline_target + 0.005:
        reasons.append("target_improvement_below_0.005")
    for key in ("got_val_ao", "uav123_val_auc"):
        current = _finite(candidate[key], key)
        reference = _finite(baseline[key], key)
        if reference - current > 0.01 + EPSILON:
            reasons.append("{}_drop_over_0.01".format(key))
    for category in ("person", "vehicle"):
        current = category_macro(candidate, category)
        reference = category_macro(baseline, category)
        if current is not None and reference is not None:
            if reference - current > 0.015 + EPSILON:
                reasons.append("{}_macro_drop_over_0.015".format(category))
    return {
        "qualified": not reasons,
        "target_score": candidate_target,
        "baseline_target_score": baseline_target,
        "target_improvement": candidate_target - baseline_target,
        "composite_score": composite_score(candidate),
        "reasons": reasons,
    }


def hidden_evaluation_action(
    selection: Mapping[str, object], result_exists: bool
) -> str:
    if selection.get("selection_final") is not True:
        raise ValueError("selection is not final; hidden evaluation is forbidden")
    if result_exists:
        return "skip"
    if selection.get("hidden_evaluation_started") is True:
        raise RuntimeError(
            "hidden evaluation was already invoked but has no committed result"
        )
    return "run"


def build_metrics(
    aerial_result: Mapping[str, object],
    uav123_result: Mapping[str, object],
    got_result: Mapping[str, object],
) -> dict:
    aerial_metrics = aerial_result.get("metrics")
    if not isinstance(aerial_metrics, Mapping):
        raise ValueError("aerial result lacks metrics")
    sources = aerial_metrics.get("sources")
    if not isinstance(sources, Mapping) or not sources:
        raise ValueError("aerial result lacks source metrics")
    domains = {}
    for source, raw_values in sorted(sources.items()):
        if not isinstance(raw_values, Mapping):
            raise ValueError("aerial source {} metrics must be a mapping".format(source))
        domains[str(source)] = {
            "person_auc": _optional(raw_values.get("person_auc"),
                                    "{}.person_auc".format(source)),
            "vehicle_auc": _optional(raw_values.get("vehicle_auc"),
                                     "{}.vehicle_auc".format(source)),
            "overall_auc": _finite(raw_values["overall_auc"],
                                   "{}.overall_auc".format(source)),
        }
    overall = uav123_result.get("overall")
    categories = uav123_result.get("categories", {})
    if not isinstance(overall, Mapping) or not isinstance(categories, Mapping):
        raise ValueError("UAV123 result lacks overall/categories metrics")

    def uav_category(name):
        value = categories.get(name)
        if not isinstance(value, Mapping):
            return None
        return _optional(value.get("success_auc"), "UAV123.{}.success_auc".format(name))

    domains["UAV123"] = {
        "person_auc": uav_category("person"),
        "vehicle_auc": uav_category("vehicle"),
        "overall_auc": _finite(overall["success_auc"], "UAV123.overall.success_auc"),
    }
    got_metrics = got_result.get("metrics")
    if not isinstance(got_metrics, Mapping):
        raise ValueError("GOT result lacks metrics")
    combined = {
        "domains": domains,
        "got_val_ao": _finite(got_metrics["ao"], "GOT.ao"),
        "got_val_sr_0_5": _finite(got_metrics["sr_0_5"], "GOT.sr_0_5"),
        "got_val_sr_0_75": _finite(got_metrics["sr_0_75"], "GOT.sr_0_75"),
        "uav123_val_auc": domains["UAV123"]["overall_auc"],
    }
    combined["target_score"] = target_score(combined)
    combined["composite_score"] = composite_score(combined)
    combined["person_macro"] = category_macro(combined, "person")
    combined["vehicle_macro"] = category_macro(combined, "vehicle")
    return combined


def candidate_training_command(
    stage: str,
    seed: int,
    candidate_index: int,
    interval_steps: int,
    config: Path,
    output_dir: Path,
    initial_checkpoint: Path,
    previous_candidate: Optional[Path],
    initial_global_step: int,
) -> list:
    if stage not in ("stage1", "stage2"):
        raise ValueError("unknown GOT replay stage {}".format(stage))
    index = int(candidate_index)
    interval = int(interval_steps)
    initial_step = int(initial_global_step)
    if index <= 0 or interval <= 0 or initial_step < 0:
        raise ValueError("candidate position inputs are invalid")
    stop = initial_step + index * interval
    command = [
        sys.executable,
        str(TRAINER),
        "--config", str(Path(config).resolve()),
        "--nanotrack-root", str(NANOTRACK_ROOT),
        "--pretrained", str(Path(initial_checkpoint).resolve()),
        "--output-dir", str(Path(output_dir).resolve()),
        "--policy", "head_only",
        "--stage", stage,
        "--seed", str(int(seed)),
        "--lr-schedule", "step",
        "--candidate-every-steps", str(interval),
        "--stop-after-global-step", str(stop),
    ]
    if previous_candidate is None:
        command.extend([
            "--initialize-from", str(Path(initial_checkpoint).resolve()),
            "--initial-global-step", str(initial_step),
        ])
    else:
        command.extend(["--resume", str(Path(previous_candidate).resolve())])
    return command


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with Path(path).open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _atomic_json(path: Path, value: object) -> None:
    target = Path(path)
    target.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(
        prefix="." + target.name + ".", suffix=".tmp", dir=str(target.parent)
    )
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as handle:
            json.dump(value, handle, indent=2, sort_keys=True)
            handle.write("\n")
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary, str(target))
    except BaseException:
        try:
            os.unlink(temporary)
        except OSError:
            pass
        raise


def _run_streamed(command: list, log_path: Path) -> None:
    log_path.parent.mkdir(parents=True, exist_ok=True)
    print("RUN " + " ".join(command), flush=True)
    with log_path.open("w", encoding="utf-8") as log:
        log.write("command=" + json.dumps(command) + "\n")
        log.flush()
        process = subprocess.Popen(
            command, cwd=str(WORKSPACE), stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, universal_newlines=True, bufsize=1,
        )
        assert process.stdout is not None
        for line in process.stdout:
            print(line, end="", flush=True)
            log.write(line)
        return_code = process.wait()
        log.write("exit_code={}\n".format(return_code))
        log.flush()
        os.fsync(log.fileno())
    if return_code:
        raise RuntimeError("command failed with exit {}: {}".format(
            return_code, " ".join(command)))


def _validated_existing_result(path: Path, snapshot: Path) -> Optional[dict]:
    if not path.is_file():
        return None
    result = json.loads(path.read_text(encoding="utf-8"))
    if result.get("snapshot_sha256") != _sha256(snapshot):
        raise ValueError("existing evaluation snapshot hash mismatch: {}".format(path))
    return result


def evaluate_snapshot(
    key: str, config: Path, snapshot: Path, output_root: Path
) -> dict:
    destination = output_root / key
    destination.mkdir(parents=True, exist_ok=True)
    logs = output_root.parent / "logs"
    aerial_path = destination / "aerial.json"
    uav_dir = destination / "uav123"
    uav_path = uav_dir / "summary.json"
    got_path = destination / "got-val.json"
    aerial = _validated_existing_result(aerial_path, snapshot)
    if aerial is None:
        _run_streamed([
            sys.executable, str(WORKSPACE / "scripts/evaluate_aerial_tracklets_nanotrack.py"),
            "--panel", str(WORKSPACE / "data/nanotrack_conservative_three_stage/val-aerial.json"),
            "--nanotrack-root", str(NANOTRACK_ROOT),
            "--config", str(config), "--snapshot", str(snapshot),
            "--output", str(aerial_path), "--device", "cuda:0",
        ], logs / (key + "_aerial.log"))
        aerial = _validated_existing_result(aerial_path, snapshot)
    uav = _validated_existing_result(uav_path, snapshot)
    if uav is None:
        _run_streamed([
            sys.executable, str(WORKSPACE / "scripts/evaluate_uav123_nanotrack.py"),
            "--dataset-root", "/home/u20/dataset/Dataset_UAV123_10fps/UAV123_10fps",
            "--prepared-root", str(WORKSPACE / "data/uav123_nanotrack"),
            "--nanotrack-root", str(NANOTRACK_ROOT),
            "--config", str(config), "--snapshot", str(snapshot),
            "--split", "val", "--output-dir", str(uav_dir),
        ], logs / (key + "_uav123.log"))
        uav = _validated_existing_result(uav_path, snapshot)
    got = _validated_existing_result(got_path, snapshot)
    if got is None:
        _run_streamed([
            sys.executable, str(WORKSPACE / "scripts/evaluate_got10k_nanotrack.py"),
            "--subset", "val", "--dataset-root", "/home/u20/dataset/GOT-10K",
            "--nanotrack-root", str(NANOTRACK_ROOT),
            "--config", str(config), "--snapshot", str(snapshot),
            "--output", str(got_path), "--device", "cuda:0",
        ], logs / (key + "_got-val.log"))
        got = _validated_existing_result(got_path, snapshot)
    assert aerial is not None and uav is not None and got is not None
    combined = build_metrics(aerial, uav, got)
    combined.update({
        "key": key,
        "snapshot": str(snapshot.resolve()),
        "snapshot_sha256": _sha256(snapshot),
        "config": str(config.resolve()),
        "config_sha256": _sha256(config),
    })
    _atomic_json(destination / "metrics.json", combined)
    print("METRICS {} T={:.6f} G={:.6f} U={:.6f} C={:.6f}".format(
        key, combined["target_score"], combined["got_val_ao"],
        combined["uav123_val_auc"], combined["composite_score"]), flush=True)
    return combined


def _find_candidate(directory: Path, stage: str, global_step: int) -> Optional[Path]:
    matches = sorted(directory.glob(
        "candidate_{}_g{:08d}_*.pth".format(stage, int(global_step))))
    if len(matches) > 1:
        raise ValueError("multiple candidates at {} step {}".format(stage, global_step))
    return matches[0] if matches else None


def _checkpoint_global_step(path: Path) -> int:
    import torch
    checkpoint = torch.load(str(path), map_location="cpu")
    if not isinstance(checkpoint, Mapping):
        raise ValueError("checkpoint must be a mapping")
    return int(checkpoint.get("global_step", 0))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--calibration", type=Path, required=True)
    parser.add_argument("--config-dir", type=Path,
                        default=WORKSPACE / "configs/nanotrack_got_replay_v1")
    parser.add_argument("--artifact-root", type=Path,
                        default=WORKSPACE / "artifacts/nanotrack_got_replay_v1")
    parser.add_argument("--initial-checkpoint", type=Path, default=INITIAL_CHECKPOINT)
    args = parser.parse_args()
    calibration = json.loads(args.calibration.read_text(encoding="utf-8"))
    interval = int(calibration["candidate_interval"]["steps"])
    configs = {stage: (args.config_dir / (stage + ".yaml")).resolve()
               for stage in ("stage1", "stage2")}
    for path in list(configs.values()) + [args.initial_checkpoint]:
        if not path.is_file():
            raise FileNotFoundError(str(path))
    root = args.artifact_root.resolve()
    run_root = root / "run"
    validation_root = run_root / "validation"
    state_path = run_root / "candidate_table.json"
    state = {"format_version": 1, "candidates": [], "seeds": {}}
    if state_path.is_file():
        state = json.loads(state_path.read_text(encoding="utf-8"))
    baseline = evaluate_snapshot(
        "baseline", configs["stage1"], args.initial_checkpoint.resolve(),
        root / "baseline",
    )
    initial_step = _checkpoint_global_step(args.initial_checkpoint)

    def persisted_record(key):
        return next((item for item in state["candidates"] if item["key"] == key), None)

    for seed in (123456, 20261011):
        seed_state = state["seeds"].setdefault(str(seed), {})
        stage1_best_metrics = baseline
        stage1_best_checkpoint = args.initial_checkpoint.resolve()
        stage1_best_global = initial_step
        previous = None
        stage_dir = run_root / str(seed) / "stage1"
        for index in range(1, 5):
            global_step = initial_step + index * interval
            key = "seed{}_stage1_c{}".format(seed, index)
            candidate = _find_candidate(stage_dir, "stage1", global_step)
            if candidate is None:
                command = candidate_training_command(
                    "stage1", seed, index, interval, configs["stage1"], stage_dir,
                    args.initial_checkpoint, previous, initial_step,
                )
                _run_streamed(command, run_root / "logs" / (key + "_train.log"))
                candidate = _find_candidate(stage_dir, "stage1", global_step)
            if candidate is None:
                raise RuntimeError("training produced no candidate for {}".format(key))
            previous = candidate
            record = persisted_record(key)
            if record is None:
                metrics = evaluate_snapshot(key, configs["stage1"], candidate,
                                            validation_root)
                decision = replacement_decision(metrics, stage1_best_metrics)
                record = {
                    "key": key, "seed": seed, "stage": "stage1", "index": index,
                    "global_step": global_step, "checkpoint": str(candidate.resolve()),
                    "checkpoint_sha256": _sha256(candidate), "metrics": metrics,
                    "replacement": decision,
                }
                state["candidates"].append(record)
                _atomic_json(state_path, state)
            if record["replacement"]["qualified"]:
                stage1_best_metrics = record["metrics"]
                stage1_best_checkpoint = Path(record["checkpoint"])
                stage1_best_global = int(record["global_step"])
        seed_state["stage1_best"] = (
            None if stage1_best_checkpoint == args.initial_checkpoint.resolve()
            else str(stage1_best_checkpoint)
        )
        if seed_state["stage1_best"] is None:
            seed_state["stage2_skipped"] = "no qualified stage1 candidate"
            _atomic_json(state_path, state)
            continue

        stage2_best_metrics = stage1_best_metrics
        stage2_best_checkpoint = stage1_best_checkpoint
        previous = None
        stage_dir = run_root / str(seed) / "stage2"
        for index in range(1, 4):
            global_step = stage1_best_global + index * interval
            key = "seed{}_stage2_c{}".format(seed, index)
            candidate = _find_candidate(stage_dir, "stage2", global_step)
            if candidate is None:
                command = candidate_training_command(
                    "stage2", seed, index, interval, configs["stage2"], stage_dir,
                    stage1_best_checkpoint, previous, stage1_best_global,
                )
                _run_streamed(command, run_root / "logs" / (key + "_train.log"))
                candidate = _find_candidate(stage_dir, "stage2", global_step)
            if candidate is None:
                raise RuntimeError("training produced no candidate for {}".format(key))
            previous = candidate
            record = persisted_record(key)
            if record is None:
                metrics = evaluate_snapshot(key, configs["stage2"], candidate,
                                            validation_root)
                decision = replacement_decision(metrics, stage2_best_metrics)
                record = {
                    "key": key, "seed": seed, "stage": "stage2", "index": index,
                    "global_step": global_step, "checkpoint": str(candidate.resolve()),
                    "checkpoint_sha256": _sha256(candidate), "metrics": metrics,
                    "replacement": decision,
                }
                state["candidates"].append(record)
                _atomic_json(state_path, state)
            if record["replacement"]["qualified"]:
                stage2_best_metrics = record["metrics"]
                stage2_best_checkpoint = Path(record["checkpoint"])
        seed_state["stage2_best"] = str(stage2_best_checkpoint)
        _atomic_json(state_path, state)

    eligible = []
    for record in state["candidates"]:
        gate = final_gate(record["metrics"], baseline)
        record["final_gate"] = gate
        if gate["qualified"]:
            eligible.append(record)
    eligible.sort(key=lambda item: (
        float(item["metrics"]["composite_score"]), -int(item["seed"]),
        item["stage"], -int(item["index"])), reverse=True)
    selection = {
        "format_version": 1,
        "selection_final": bool(eligible),
        "baseline": baseline,
        "selected": eligible[0] if eligible else None,
        "eligible_keys": [item["key"] for item in eligible],
        "hidden_evaluation_started": False,
    }
    _atomic_json(state_path, state)
    _atomic_json(run_root / "selection.json", selection)
    if not eligible:
        raise RuntimeError("no candidate passed the final advancement gate")
    print("SELECTION {} C={:.6f}".format(
        eligible[0]["key"], eligible[0]["metrics"]["composite_score"]), flush=True)


if __name__ == "__main__":
    main()
