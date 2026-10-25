#!/usr/bin/env python3
"""Run the frozen nine-cell, two-seed NanoTrack screening matrix resumably."""
import argparse
import fcntl
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
import sys
import tempfile

import yaml

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
from scripts.generate_nanotrack_factorial_configs import _quotas
from scripts.nanotrack_factorial_policy import SCORING_VERSION, guardrail_decision
from scripts.run_nanotrack_got_replay import build_metrics

NODES = (2560, 5120, 7680, 10240)
SAMPLE_SCHEDULE = {"mode": "sample", "position": "pre_update_seen_samples",
                   "denominator_samples": 40960, "start_lr": 2e-5, "terminal_lr": 8e-6}
SEEDS = (123456, 20261011)
CELLS = tuple(sorted("b%d_g%d" % (b, g) for b in (8, 32, 64) for g in (0, 10, 30)))
SCRIPTS = {
    "train": "train_nanotrack_uav123.py", "aerial": "evaluate_aerial_tracklets_nanotrack.py",
    "uav": "evaluate_uav123_nanotrack.py", "got": "evaluate_got10k_nanotrack.py",
}
FLAGS = {
    "train": {"--config", "--nanotrack-root", "--pretrained", "--output-dir", "--resume",
              "--initialize-from", "--initial-global-step", "--stop-after-global-step", "--policy",
              "--pair-policy", "--stage", "--lr-schedule", "--seed", "--checkpoint-every-steps",
              "--candidate-every-steps"},
    "aerial": {"--panel", "--nanotrack-root", "--config", "--snapshot", "--output", "--device"},
    "uav": {"--dataset-root", "--prepared-root", "--nanotrack-root", "--config", "--snapshot",
            "--split", "--output-dir"},
    "got": {"--subset", "--dataset-root", "--nanotrack-root", "--config", "--snapshot", "--output", "--device"},
}


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def json_hash(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, allow_nan=False).encode()).hexdigest()


def read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))


def atomic_json(path, value):
    path = Path(path)
    content = (json.dumps(value, indent=2, sort_keys=True, allow_nan=False) + "\n").encode()
    if path.is_file() and path.read_bytes() == content:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(prefix="." + path.name, dir=str(path.parent))
    try:
        with os.fdopen(descriptor, "wb") as handle:
            handle.write(content)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary, path)
        directory = os.open(str(path.parent), os.O_RDONLY)
        try:
            os.fsync(directory)
        finally:
            os.close(directory)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def artifact(path):
    return {"path": str(Path(path).resolve()), "sha256": sha256(path)}


def check_artifact(record):
    if sha256(record["path"]) != record["sha256"]:
        raise ValueError("artifact SHA-256 mismatch: " + record["path"])


def validate_command(command):
    if len(command) < 2 or len(command) % 2:
        raise ValueError("malformed command")
    for token in command:
        if any(term in str(token).lower() for term in ("hidden", "sot-v1", ".mp4", "--video")):
            raise ValueError("forbidden input in command")
    roles = [role for role, script in SCRIPTS.items() if command[1] == str(ROOT / "scripts" / script)]
    if len(roles) != 1:
        raise ValueError("script outside command whitelist")
    role = roles[0]
    pairs = list(zip(command[2::2], command[3::2]))
    if len({key for key, _ in pairs}) != len(pairs) or any(key not in FLAGS[role] for key, _ in pairs):
        raise ValueError("flag outside command whitelist")
    options = dict(pairs)
    if role == "got" and options.get("--subset") != "val":
        raise ValueError("only GOT val is permitted")
    if role == "uav" and options.get("--split") != "val":
        raise ValueError("only UAV val is permitted")
    return role


def run_command(command, log_path):
    validate_command(command)
    log_path.parent.mkdir(parents=True, exist_ok=True)
    with log_path.open("w", encoding="utf-8") as log:
        log.write("command=" + json.dumps(command) + "\n")
        log.flush()
        process = subprocess.run(command, cwd=str(ROOT), stdout=log, stderr=subprocess.STDOUT)
        log.write("exit_code=%d\n" % process.returncode)
        log.flush()
        os.fsync(log.fileno())
    if process.returncode:
        raise RuntimeError("command failed; see " + str(log_path))


def default_inputs(manifest):
    assets = Path(manifest["asset_root"])
    return {"panel": assets / "data/nanotrack_conservative_three_stage/val-aerial.json",
            "uav_dataset": Path("/home/u20/dataset/Dataset_UAV123_10fps/UAV123_10fps"),
            "uav_prepared": assets / "data/uav123_nanotrack",
            "got_dataset": Path("/home/u20/dataset/GOT-10K"),
            "nanotrack_root": assets / "repos/siamtrackers/NanoTrack"}


def load_context(manifest_path, evaluation_inputs=None, execution_backend="subprocess"):
    """Read and validate every frozen input before creating output directories."""
    manifest = read_json(manifest_path)
    if manifest.get("scoring_version") != SCORING_VERSION or tuple(manifest.get("screening_seeds", [])) != SEEDS:
        raise ValueError("scoring version or screening seeds mismatch")
    if sorted(manifest["cells"]) != list(CELLS):
        raise ValueError("matrix must contain exactly nine cells")
    inputs = {k: str(Path(v).resolve()) for k, v in (evaluation_inputs or default_inputs(manifest)).items()}
    if read_json(inputs["panel"]).get("split") not in ("val", "validation"):
        raise ValueError("only the validation aerial panel is permitted")
    bound = {"manifest": artifact(manifest_path)}
    for key, path in manifest["input_paths"].items():
        bound[key] = artifact(path)
        if bound[key]["sha256"] != manifest["input_hashes"][key]:
            raise ValueError("frozen input SHA mismatch: " + key)
    for name, record in manifest["dataset_manifests"].items():
        check_artifact(record)
        bound["dataset:" + name] = record
    for cell_id, cell in manifest["cells"].items():
        batch, got = cell["batch_size"], cell["got_percent"]
        if cell_id != "b%d_g%d" % (batch, got) or batch not in (8, 32, 64) or got not in (0, 10, 30):
            raise ValueError("invalid cell identity")
        exact = {"full_samples": 40960, "screening_samples": 10240,
                 "evaluation_interval_samples": 2560, "full_updates": 40960 // batch,
                 "screening_updates": 10240 // batch, "evaluation_interval_updates": 2560 // batch}
        if any(cell.get(k) != v for k, v in exact.items()) or cell["quotas"] != _quotas(got):
            raise ValueError("sample/update/quota mismatch: " + cell_id)
        if cell["input_hashes"] != manifest["input_hashes"] or cell["initial_checkpoint"] != manifest["input_paths"]["initial_checkpoint"] or cell["pair_policy_path"] != manifest["input_paths"]["pair_policy"]:
            raise ValueError("cell frozen input identity mismatch")
        bound["config:" + cell_id] = artifact(cell["config_path"])
        if bound["config:" + cell_id]["sha256"] != cell["config_sha256"]:
            raise ValueError("config SHA mismatch")
        cfg = yaml.safe_load(Path(cell["config_path"]).read_text())
        quotas = {n: cfg["DATASET"][n]["NUM_USE"] for n in cfg["DATASET"]["NAMES"]}
        if quotas != cell["quotas"] or cfg["DATASET"]["VIDEOS_PER_EPOCH"] != 40960 or cfg["TRAIN"]["BATCH_SIZE"] != batch or cfg["TRAIN"]["EPOCH"] != 1 or cfg["TRAIN"]["NUM_WORKERS"] != 0:
            raise ValueError("config sample or loader mismatch")
        if cfg["TRAIN"]["LR"]["KWARGS"] != {"start_lr": 2e-5, "end_lr": 8e-6} or cfg["TRAIN"]["LR_WARMUP"]["WARMUP"]:
            raise ValueError("sample-aligned LR mismatch")
    code = list(SCRIPTS.values()) + ["run_nanotrack_factorial.py", "verify_nanotrack_factorial.py",
            "nanotrack_factorial_policy.py", "nanotrack_pair_sampling.py", "run_nanotrack_got_replay.py",
            "generate_nanotrack_factorial_configs.py", "nanotrack_three_stage_data.py", "uav123_nanotrack.py"]
    for script in code:
        bound["code:" + script] = artifact(ROOT / "scripts" / script)
    for path in sorted(Path(inputs["nanotrack_root"]).rglob("*.py")):
        bound["upstream:" + str(path.relative_to(inputs["nanotrack_root"]))] = artifact(path)
    for key, path in {"panel": Path(inputs["panel"]),
                      "uav_split": Path(inputs["uav_prepared"]) / "splits.json",
                      "uav_config": Path(inputs["uav_dataset"]) / "configSeqs.m",
                      "got_list": Path(inputs["got_dataset"]) / "val_data/val/list.txt"}.items():
        bound["evaluation:" + key] = artifact(path)
    if execution_backend not in ("subprocess", "fake_cpu_fixture"):
        raise ValueError("unknown execution backend")
    context = {"bound": bound, "evaluation_inputs": inputs, "python": artifact(sys.executable),
               "execution_backend": execution_backend,
               "scoring_version": SCORING_VERSION}
    context["sha256"] = json_hash(context)
    return manifest, context


def training_command(cell, seed, samples, destination, context, previous):
    inputs = context["evaluation_inputs"]
    command = [sys.executable, str(ROOT / "scripts" / SCRIPTS["train"]),
               "--config", cell["config_path"], "--nanotrack-root", inputs["nanotrack_root"],
               "--pretrained", cell["initial_checkpoint"], "--output-dir", str(destination),
               "--stop-after-global-step", str(samples // cell["batch_size"]),
               "--policy", "head_only", "--pair-policy", cell["pair_policy_path"],
               "--stage", "factorial", "--lr-schedule", "sample", "--seed", str(seed),
               "--checkpoint-every-steps", str(2560 // cell["batch_size"]),
               "--candidate-every-steps", str(2560 // cell["batch_size"])]
    command += ["--resume", previous["path"]] if previous else ["--initialize-from", cell["initial_checkpoint"], "--initial-global-step", "0"]
    return command


def evaluation_commands(config, snapshot, destination, context):
    inputs = context["evaluation_inputs"]
    common = ["--nanotrack-root", inputs["nanotrack_root"], "--config", str(config), "--snapshot", str(snapshot)]
    result = {}
    for role in ("aerial", "uav", "got"):
        command = [sys.executable, str(ROOT / "scripts" / SCRIPTS[role])]
        if role == "aerial":
            output = destination / "aerial.json"
            command += ["--panel", inputs["panel"]] + common + ["--output", str(output), "--device", "cuda:0"]
        elif role == "uav":
            output = destination / "uav123/summary.json"
            command += ["--dataset-root", inputs["uav_dataset"], "--prepared-root", inputs["uav_prepared"]] + common + ["--split", "val", "--output-dir", str(output.parent)]
        else:
            output = destination / "got-val.json"
            command += ["--subset", "val", "--dataset-root", inputs["got_dataset"]] + common + ["--output", str(output), "--device", "cuda:0"]
        result[role] = (command, output)
    return result


def execute(command, log, runner):
    validate_command(command)
    runner(command, log)
    return {"command": command, "command_sha256": json_hash(command), "log": artifact(log)}


def combine(evaluations, config, checkpoint):
    outputs = {}
    for role, evidence in evaluations.items():
        check_artifact(evidence["output"])
        value = read_json(evidence["output"]["path"])
        if value.get("snapshot_sha256") != checkpoint["sha256"]:
            raise ValueError("evaluation snapshot SHA mismatch")
        if value.get("config_sha256", config["sha256"]) != config["sha256"]:
            raise ValueError("evaluation config SHA mismatch")
        if role == "got" and value.get("subset") != "val" or role == "uav" and value.get("split") != "val":
            raise ValueError("evaluation subset mismatch")
        outputs[role] = value
    metrics = build_metrics(outputs["aerial"], outputs["uav"], outputs["got"])
    for key in ("target_score", "composite_score"):
        metrics.pop(key, None)
    guardrail_decision(metrics, metrics)
    json_hash(metrics)
    return metrics


def evaluate(config, checkpoint, destination, context, runner):
    evaluations = {}
    for role, (command, output) in evaluation_commands(config["path"], checkpoint["path"], destination, context).items():
        evidence = execute(command, destination / (role + ".log"), runner)
        evidence["output"] = artifact(output)
        evaluations[role] = evidence
    return evaluations, combine(evaluations, config, checkpoint)


def node_identity(cell_id, cell, seed, samples, context):
    return {"context_sha256": context["sha256"], "cell": cell_id, "seed": seed,
            "seen_samples": samples, "global_step": samples // cell["batch_size"],
            "config_sha256": cell["config_sha256"],
            "initial_checkpoint_sha256": context["bound"]["initial_checkpoint"]["sha256"],
            "pair_policy_sha256": context["bound"]["pair_policy"]["sha256"],
            "pair_audit_sha256": context["bound"]["pair_audit"]["sha256"],
            "lr_schedule": SAMPLE_SCHEDULE.copy(),
            "scoring_version": SCORING_VERSION,
            "scoring_source_sha256": context["bound"]["code:nanotrack_factorial_policy.py"]["sha256"]}


def check_evidence(evidence):
    validate_command(evidence["command"])
    if json_hash(evidence["command"]) != evidence["command_sha256"]:
        raise ValueError("command hash mismatch")
    check_artifact(evidence["log"])


def check_checkpoint(path, identity):
    import torch
    payload = torch.load(str(path), map_location="cpu")
    expected = {"global_step": identity["global_step"], "stage_step": identity["global_step"],
                "seed": identity["seed"], "stage": "factorial", "policy": "head_only",
                "lr_schedule": "sample", "pair_policy_sha256": identity["pair_policy_sha256"]}
    if any(payload.get(k) != v for k, v in expected.items()) or "optimizer" not in payload or "model" not in payload:
        raise ValueError("checkpoint metadata mismatch")


def validate_telemetry(evidence, cell, identity, previous, context):
    if "telemetry" not in evidence:
        raise ValueError("missing segment telemetry")
    check_artifact(evidence["telemetry"])
    value = read_json(evidence["telemetry"]["path"])
    batch = cell["batch_size"]
    end = identity["global_step"]
    start = end - 2560 // batch
    expected = {"schema_version": 1, "stage": "factorial", "seed": identity["seed"],
                "batch_size": batch, "lr_schedule": "sample", "full_sample_budget": 40960,
                "start_update": start, "end_update": end, "segment_updates": end - start,
                "start_global_step": start, "end_global_step": end,
                "start_seen_samples": identity["seen_samples"] - 2560,
                "end_seen_samples": identity["seen_samples"], "segment_samples": 2560,
                "loss_finite": True, "loss_count": end - start, "wall_boundary": "training_loop",
                "config_sha256": cell["config_sha256"],
                "pair_policy_sha256": identity["pair_policy_sha256"],
                "input_checkpoint_sha256": (previous or context["bound"]["initial_checkpoint"])["sha256"]}
    if any(value.get(k) != v for k, v in expected.items()):
        raise ValueError("segment telemetry identity/sample mismatch")
    for key, position in (("initial_lr", start), ("final_lr", end - 1), ("terminal_next_lr", end)):
        ratio = position * batch / 40960.0
        want = math.exp(math.log(2e-5) * (1 - ratio) + math.log(8e-6) * ratio)
        actual = value.get(key)
        if not isinstance(actual, (int, float)) or not math.isfinite(actual) or not math.isclose(actual, want, rel_tol=1e-12):
            raise ValueError("segment telemetry LR mismatch")
    wall = value.get("wall_seconds")
    if not isinstance(wall, (int, float)) or not math.isfinite(wall) or wall < 0:
        raise ValueError("segment telemetry wall time must be finite")
    memory = value.get("cuda_memory", {})
    if context["execution_backend"] == "fake_cpu_fixture":
        if memory != {"status": "unavailable", "reason": "fake_cpu_fixture"}:
            raise ValueError("fake segment telemetry memory must be explicitly unavailable")
    else:
        allocated, reserved = memory.get("peak_allocated_bytes"), memory.get("peak_reserved_bytes")
        if memory.get("status") != "observed" or not isinstance(allocated, int) or not isinstance(reserved, int) or allocated <= 0 or reserved < allocated:
            raise ValueError("real segment telemetry requires observed CUDA memory")
    return value


def validate_record(record, cell, context, previous, baseline):
    checkpoint = {"path": record["checkpoint"], "sha256": record["checkpoint_sha256"]}
    check_artifact(checkpoint)
    check_checkpoint(checkpoint["path"], record["identity"])
    expected_predecessor = previous or context["bound"]["initial_checkpoint"]
    if record["predecessor"] != expected_predecessor:
        raise ValueError("checkpoint predecessor mismatch")
    check_evidence(record["training"])
    validate_telemetry(record["training"], cell, record["identity"], previous, context)
    expected_command = training_command(cell, record["seed"], record["seen_samples"], Path(checkpoint["path"]).parent, context, previous)
    if record["training"]["command"] != expected_command:
        raise ValueError("training command mismatch")
    destination = Path(record["training"]["log"]["path"]).parent / "evaluation"
    expected = evaluation_commands(cell["config_path"], checkpoint["path"], destination, context)
    for role, (command, output) in expected.items():
        evidence = record["evaluations"][role]
        check_evidence(evidence)
        if evidence["command"] != command or evidence["output"]["path"] != str(output):
            raise ValueError("evaluation command mismatch")
    metrics = combine(record["evaluations"], context["bound"]["config:" + record["cell"]], checkpoint)
    if record["metrics"] != metrics or record["decision"] != guardrail_decision(metrics, baseline):
        raise ValueError("persisted metrics or scoring decision mismatch")
    return checkpoint


def summarize(manifest, state):
    cells = {}
    for cell in CELLS:
        records = [r for r in state["candidates"] if r["cell"] == cell and r["seen_samples"] == 10240]
        complete = sorted(r["seed"] for r in records) == list(SEEDS)
        safe = complete and all(r["decision"]["qualified"] for r in records)
        cells[cell] = {"complete": complete, "safe": safe,
                       "mean_target_score_v3": sum(r["decision"]["target_score_v3"] for r in records) / 2 if safe else None}
    ranked = sorted((c for c in CELLS if cells[c]["safe"]), key=lambda c: (-cells[c]["mean_target_score_v3"], c))
    expansion = [c for c in ranked if cells[c]["mean_target_score_v3"] + 0.002 + 1e-12 >= cells[ranked[0]]["mean_target_score_v3"]][:3] if ranked else []
    complete = len(state["candidates"]) == 72
    return {"schema_version": 1, "scoring_version": SCORING_VERSION,
            "state": ("screening_complete" if ranked else "no_safe_cell") if complete else "screening_incomplete",
            "cells": cells, "expansion_cells": expansion if complete else [],
            "promotion_authorized": False, "candidate_table_sha256": json_hash(state)}


def _next_attempt(root):
    number = 1
    while (root / ("attempt%04d" % number)).exists():
        number += 1
    return root / ("attempt%04d" % number)


def validate_state_identity(state, manifest, context):
    if state.get("schema_version") != 1 or state.get("context") != context or state.get("scoring_version") != SCORING_VERSION:
        raise ValueError("persisted experiment identity mismatch")
    seen = set()
    for record in state["candidates"]:
        key = (record["cell"], record["seed"], record["seen_samples"])
        if key in seen or key[0] not in CELLS or key[1] not in SEEDS or key[2] not in NODES:
            raise ValueError("duplicate or unexpected matrix node")
        seen.add(key)
        if record["identity"] != node_identity(key[0], manifest["cells"][key[0]], key[1], key[2], context) or record["global_step"] != record["identity"]["global_step"]:
            raise ValueError("persisted node identity mismatch")
    # Invalidated artifacts are retained for audit; their commands must remain in scope.
    for record in state["invalidated"]:
        validate_command(record["training"]["command"])
        for evidence in record["evaluations"].values():
            validate_command(evidence["command"])


def validate_baseline(state, manifest, context):
    baseline = state["baseline"]
    cell = manifest["cells"][CELLS[0]]
    config = context["bound"]["config:" + CELLS[0]]
    expected = evaluation_commands(cell["config_path"], cell["initial_checkpoint"], Path(baseline["destination"]), context)
    for role, (command, output) in expected.items():
        evidence = baseline["evaluations"][role]
        check_evidence(evidence)
        if evidence["command"] != command or evidence["output"]["path"] != str(output):
            raise ValueError("baseline command mismatch")
    metrics = combine(baseline["evaluations"], config, context["bound"]["initial_checkpoint"])
    if metrics != baseline["metrics"]:
        raise ValueError("baseline metrics mismatch")
    return metrics


def run_screening(manifest_path, output_root, runner=run_command, evaluation_inputs=None):
    backend = "subprocess" if runner is run_command else "fake_cpu_fixture"
    manifest, context = load_context(manifest_path, evaluation_inputs, backend)
    output_root = Path(output_root).resolve()
    table_path = output_root / "candidate_table.json"
    state = read_json(table_path) if table_path.is_file() else {
        "schema_version": 1, "scoring_version": SCORING_VERSION, "context": context,
        "candidates": [], "invalidated": []}
    validate_state_identity(state, manifest, context)
    output_root.mkdir(parents=True, exist_ok=True)
    with (output_root / ".controller.lock").open("a+") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        if table_path.is_file():
            state = read_json(table_path)
            validate_state_identity(state, manifest, context)
        if "baseline" not in state:
            destination = _next_attempt(output_root / "baseline")
            cell = manifest["cells"][CELLS[0]]
            evaluations, metrics = evaluate(context["bound"]["config:" + CELLS[0]],
                                            context["bound"]["initial_checkpoint"], destination, context, runner)
            state["baseline"] = {"destination": str(destination), "evaluations": evaluations, "metrics": metrics}
            atomic_json(table_path, state)
        baseline = validate_baseline(state, manifest, context)
        for cell_id in CELLS:
            cell = manifest["cells"][cell_id]
            for seed in SEEDS:
                previous = None
                invalid_suffix = False
                for samples in NODES:
                    record = next((r for r in state["candidates"] if (r["cell"], r["seed"], r["seen_samples"]) == (cell_id, seed, samples)), None)
                    if record and not invalid_suffix:
                        try:
                            previous = validate_record(record, cell, context, previous, baseline)
                            continue
                        except (ValueError, OSError, KeyError, EOFError, RuntimeError):
                            invalid_suffix = True
                    if record:
                        state["candidates"].remove(record)
                        state["invalidated"].append(record)
                    identity = node_identity(cell_id, cell, seed, samples, context)
                    destination = _next_attempt(output_root / "nodes" / cell_id / str(seed) / str(samples))
                    train_dir = destination / "training"
                    command = training_command(cell, seed, samples, train_dir, context, previous)
                    training = execute(command, destination / "training.log", runner)
                    matches = list(train_dir.glob("candidate_factorial_g%08d_*.pth" % identity["global_step"]))
                    if len(matches) != 1:
                        raise ValueError("training must produce exactly one boundary checkpoint")
                    check_checkpoint(matches[0], identity)
                    training["telemetry"] = artifact(train_dir / "segment-telemetry.json")
                    validate_telemetry(training, cell, identity, previous, context)
                    checkpoint = artifact(matches[0])
                    evaluations, metrics = evaluate(context["bound"]["config:" + cell_id], checkpoint, destination / "evaluation", context, runner)
                    record = {"cell": cell_id, "seed": seed, "seen_samples": samples,
                              "global_step": identity["global_step"], "identity": identity,
                              "checkpoint": checkpoint["path"], "checkpoint_sha256": checkpoint["sha256"],
                              "predecessor": previous or context["bound"]["initial_checkpoint"],
                              "training": training, "evaluations": evaluations, "metrics": metrics,
                              "decision": guardrail_decision(metrics, baseline)}
                    state["candidates"].append(record)
                    state["candidates"].sort(key=lambda r: (r["cell"], r["seed"], r["seen_samples"]))
                    atomic_json(table_path, state)
                    previous = checkpoint
        atomic_json(output_root / "cell_summary.json", summarize(manifest, state))
    return state


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=ROOT / "configs/nanotrack_factorial_v3/experiment-manifest.json")
    parser.add_argument("--output-root", type=Path)
    args = parser.parse_args()
    manifest = read_json(args.manifest)
    state = run_screening(args.manifest, args.output_root or Path(manifest["output_root"]) / "screening")
    print(json.dumps({"completed_nodes": len(state["candidates"]), "scoring_version": SCORING_VERSION}))


if __name__ == "__main__":
    main()
