#!/usr/bin/env python3
"""Run the frozen nine-cell, two-seed NanoTrack screening matrix resumably."""
import argparse
import fcntl
import hashlib
import json
import math
import os
import pickle
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


def persisted_equivalent(left, right):
    """Compare persisted metrics/decisions without tolerating semantic changes."""
    if isinstance(left, bool) or isinstance(right, bool):
        return type(left) is type(right) and left == right
    if isinstance(left, (int, float)) and isinstance(right, (int, float)):
        return (math.isfinite(left) and math.isfinite(right)
                and math.isclose(left, right, rel_tol=1e-12, abs_tol=1e-12))
    if isinstance(left, dict) and isinstance(right, dict):
        return (left.keys() == right.keys()
                and all(persisted_equivalent(left[key], right[key]) for key in left))
    if isinstance(left, list) and isinstance(right, list):
        return (len(left) == len(right)
                and all(persisted_equivalent(a, b) for a, b in zip(left, right)))
    return type(left) is type(right) and left == right


def atomic_json(path, value):
    path = Path(path)
    content = (json.dumps(value, sort_keys=True, separators=(",", ":"), allow_nan=False) + "\n").encode()
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
    import torch
    header = torch.load(bound["initial_checkpoint"]["path"], map_location="cpu")
    if not isinstance(header, dict) or type(header.get("global_step")) is not int or header["global_step"] < 0:
        raise ValueError("initial checkpoint requires a nonnegative source global_step")
    source_header = {key: header.get(key) for key in
                     ("global_step", "stage_step", "stage", "epoch", "policy", "seed", "lr_schedule")}
    context = {"bound": bound, "evaluation_inputs": inputs, "python": artifact(sys.executable),
               "execution_backend": execution_backend,
               "initial_source_header": source_header,
               "scoring_version": SCORING_VERSION}
    context["sha256"] = json_hash(context)
    return manifest, context


def training_command(cell, seed, samples, destination, context, previous):
    inputs = context["evaluation_inputs"]
    command = [sys.executable, str(ROOT / "scripts" / SCRIPTS["train"]),
               "--config", cell["config_path"], "--nanotrack-root", inputs["nanotrack_root"],
               "--pretrained", cell["initial_checkpoint"], "--output-dir", str(destination),
               "--stop-after-global-step", str(context["initial_source_header"]["global_step"] + samples // cell["batch_size"]),
               "--policy", "head_only", "--pair-policy", cell["pair_policy_path"],
               "--stage", "factorial", "--lr-schedule", "sample", "--seed", str(seed),
               "--checkpoint-every-steps", "250",
               "--candidate-every-steps", str(2560 // cell["batch_size"])]
    command += ["--resume", previous["path"]] if previous else ["--initialize-from", cell["initial_checkpoint"], "--initial-global-step", str(context["initial_source_header"]["global_step"])]
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
            "seen_samples": samples, "stage_step": samples // cell["batch_size"],
            "global_step": context["initial_source_header"]["global_step"] + samples // cell["batch_size"],
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
    expected = {"global_step": identity["global_step"], "stage_step": identity["stage_step"],
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
    end = identity["stage_step"]
    start = end - 2560 // batch
    expected = {"schema_version": 1, "stage": "factorial", "seed": identity["seed"],
                "batch_size": batch, "lr_schedule": "sample", "full_sample_budget": 40960,
                "start_update": start, "end_update": end, "segment_updates": end - start,
                "start_global_step": context["initial_source_header"]["global_step"] + start,
                "end_global_step": identity["global_step"],
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
    for role in ("aerial", "uav", "got"):
        evidence = record["evaluations"][role]
        destination = Path(evidence["destination"])
        command, output = evaluation_commands(cell["config_path"], checkpoint["path"], destination, context)[role]
        check_evidence(evidence)
        if evidence["command"] != command or evidence["output"]["path"] != str(output):
            raise ValueError("evaluation command mismatch")
    metrics = combine(record["evaluations"], context["bound"]["config:" + record["cell"]], checkpoint)
    if (not persisted_equivalent(record["metrics"], metrics)
            or not persisted_equivalent(record["decision"], guardrail_decision(metrics, baseline))):
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


def validate_state_identity(state, manifest, context, output_root=None):
    if state.get("schema_version") != 2:
        raise ValueError("schema 2 is required; preserve schema 1 fake evidence in its original root and start a new output root")
    if state.get("context") != context or state.get("scoring_version") != SCORING_VERSION:
        raise ValueError("persisted experiment identity mismatch")
    if output_root is not None and state.get("output_root") != str(Path(output_root).resolve()):
        raise ValueError("persisted output-root identity mismatch")
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
    for attempt in state["attempts"]:
        root = Path(state["output_root"]).resolve()
        destination = Path(attempt["destination"]).resolve()
        if not any(str(destination).startswith(str(root / name) + "/") for name in ("baseline", "nodes")) or not destination.name.startswith("attempt") or not destination.name[7:].isdigit():
            raise ValueError("retained attempt outside experiment root")
        for execution in attempt["executions"]:
            if "destination" in execution and not (Path(execution["destination"]).resolve() == destination or str(Path(execution["destination"]).resolve()).startswith(str(destination) + "/")):
                raise ValueError("stage execution outside retained attempt")
        audit_commands(attempt)


def validate_baseline(state, manifest, context):
    baseline = state["baseline"]
    cell = manifest["cells"][CELLS[0]]
    config = context["bound"]["config:" + CELLS[0]]
    for role in ("aerial", "uav", "got"):
        evidence = baseline["evaluations"][role]
        command, output = evaluation_commands(cell["config_path"], cell["initial_checkpoint"], Path(evidence["destination"]), context)[role]
        check_evidence(evidence)
        if evidence["command"] != command or evidence["output"]["path"] != str(output):
            raise ValueError("baseline command mismatch")
    metrics = combine(baseline["evaluations"], config, context["bound"]["initial_checkpoint"])
    if not persisted_equivalent(metrics, baseline["metrics"]):
        raise ValueError("baseline metrics mismatch")
    return metrics


def audit_commands(value):
    """Inspect every retained/planned command, including failed and orphan attempts."""
    if isinstance(value, dict):
        for key, item in value.items():
            if key == "command":
                validate_command(item)
            else:
                audit_commands(item)
    elif isinstance(value, list):
        for item in value:
            audit_commands(item)


def inventory(destination):
    result = []
    for path in sorted(Path(destination).rglob("*")):
        if path.is_file():
            entry = artifact(path)
            entry["path"] = str(path.absolute())  # retain the recorded path, not a symlink's target
            entry["type"] = "symlink" if path.is_symlink() else "file"
            if path.is_symlink():
                entry["link_target"] = os.readlink(str(path))
            result.append(entry)
    return result


def check_recorded_inventory(execution):
    """Compare pre-recorded identities before parsing or adopting any stage output."""
    root = Path(os.path.abspath(execution["destination"]))
    for entry in execution.get("retained_inventory", []):
        path = Path(entry["path"])
        if not path.is_absolute():
            raise ValueError("retained stage inventory path must be absolute")
        try:
            Path(os.path.abspath(str(path))).relative_to(root)
        except ValueError:
            raise ValueError("retained stage inventory path escaped its destination")
        actual_type = "symlink" if path.is_symlink() else "file" if path.is_file() else "other"
        if actual_type != entry.get("type", "file"):
            raise ValueError("retained stage inventory type/symlink mismatch: " + str(path))
        if actual_type == "symlink" and os.readlink(str(path)) != entry.get("link_target"):
            raise ValueError("retained stage inventory symlink target mismatch: " + str(path))
        check_artifact(entry)


def audit_logs(destination):
    for path in Path(destination).rglob("*.log"):
        for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
            candidate = line[len("command="):] if line.startswith("command=") else line
            if candidate.startswith("["):
                try:
                    value = json.loads(candidate)
                except ValueError:
                    continue
                if isinstance(value, list) and value and isinstance(value[0], str):
                    validate_command(value)


def attempt_directories(output_root):
    return {str(p.resolve()) for name in ("baseline", "nodes")
            for p in (Path(output_root) / name).rglob("attempt[0-9]*") if p.is_dir()}


def invalidate_attempt(attempt, reason):
    attempt["status"] = "invalidated"
    attempt["invalidation_reason"] = reason
    attempt["integrity_audit"] = {
        "previous_inventory": attempt.get("retained_inventory", []),
        "recorded_stage_inventories": [
            {"role": execution["role"], "destination": execution["destination"],
             "inventory": execution["retained_inventory"]}
            for execution in attempt["executions"] if "retained_inventory" in execution],
        "observed_inventory": inventory(attempt["destination"]),
    }
    attempt.setdefault("integrity_audits", []).append(attempt["integrity_audit"])
    attempt["retained_inventory"] = attempt["integrity_audit"]["observed_inventory"]
    for execution in attempt["executions"]:
        if execution["status"] in ("planned", "running", "interrupted"):
            execution["status"] = "canceled"
            execution["reason"] = "parent_attempt_invalidated"


def reconcile_retained_attempts(state, output_root):
    known = {a["destination"] for a in state["attempts"]}
    for destination in sorted(attempt_directories(output_root) - known):
        audit_logs(destination)
        attempt = {"id": destination, "destination": destination, "kind": "orphan",
                   "executions": [], "status": "running"}
        invalidate_attempt(attempt, "orphan_directory_not_reused")
        state["attempts"].append(attempt)
    for attempt in state["attempts"]:
        audit_commands(attempt)
        audit_logs(attempt["destination"])
        if attempt["status"] == "invalidated" and inventory(attempt["destination"]) != attempt["retained_inventory"]:
            invalidate_attempt(attempt, "retained_invalidated_artifact_changed")


def plan_execution(role, destination, cell, context, snapshot=None, seed=None, samples=None, previous=None):
    destination = Path(destination)
    if role == "train":
        command = training_command(cell, seed, samples, destination / "training", context, previous)
        output = None
    else:
        command, output = evaluation_commands(cell["config_path"], snapshot, destination, context)[role]
    return {"role": role, "destination": str(destination), "command": command,
            "log_path": str(destination / (role + ".log")),
            "output_path": str(output) if output else None, "status": "planned"}


def allocate_attempt(state, root, cell_id, cell, context, seed=None, samples=None, previous=None):
    destination = _next_attempt(root)
    reserved = {a["destination"] for a in state["attempts"]}
    number = int(destination.name[7:])
    while str(destination) in reserved:
        number += 1
        destination = Path(root) / ("attempt%04d" % number)
    attempt = {"id": str(destination), "destination": str(destination),
               "kind": "baseline" if seed is None else "node", "cell": cell_id,
               "status": "planned", "executions": []}
    snapshot = cell["initial_checkpoint"]
    if seed is not None:
        identity = node_identity(cell_id, cell, seed, samples, context)
        attempt.update(identity=identity, predecessor=previous or context["bound"]["initial_checkpoint"])
        train = plan_execution("train", destination, cell, context, seed=seed, samples=samples, previous=previous)
        attempt["executions"].append(train)
        snapshot = str(destination / "training" / ("candidate_factorial_g%08d_e000_s%05d.pth" %
                       (identity["global_step"], identity["stage_step"])))
        attempt["checkpoint_path"] = snapshot
    for role in ("aerial", "uav", "got"):
        attempt["executions"].append(plan_execution(role, destination / "evaluation", cell, context, snapshot=snapshot))
    state["attempts"].append(attempt)
    return attempt


def inspect_execution(execution, attempt, cell, context):
    """Validate successful stage files, including an orphan success after a crash."""
    check_recorded_inventory(execution)
    log = Path(execution["log_path"])
    if not log.is_file() or "exit_code=0" not in log.read_text(encoding="utf-8"):
        raise ValueError("stage has no observed successful command exit")
    evidence = {"command": execution["command"], "command_sha256": json_hash(execution["command"]),
                "log": artifact(log), "destination": execution["destination"]}
    role = execution["role"]
    if role == "train":
        checkpoint = artifact(attempt["checkpoint_path"])
        check_checkpoint(checkpoint["path"], attempt["identity"])
        evidence["checkpoint"] = checkpoint
        evidence["telemetry"] = artifact(Path(checkpoint["path"]).parent / "segment-telemetry.json")
        previous = None if attempt["predecessor"] == context["bound"]["initial_checkpoint"] else attempt["predecessor"]
        validate_telemetry(evidence, cell, attempt["identity"], previous, context)
    else:
        evidence["output"] = artifact(execution["output_path"])
        output = read_json(evidence["output"]["path"])
        snapshot = context["bound"]["initial_checkpoint"] if attempt["kind"] == "baseline" else attempt["checkpoint"]
        if output.get("snapshot_sha256") != snapshot["sha256"] or output.get("config_sha256", cell["config_sha256"]) != cell["config_sha256"]:
            raise ValueError("stage output identity mismatch")
        if role == "got" and output.get("subset") != "val" or role == "uav" and output.get("split") != "val":
            raise ValueError("stage output split mismatch")
    return evidence


def check_execution(execution, attempt, cell, context):
    validate_command(execution["command"])
    role = execution["role"]
    if role == "train":
        ident = attempt["identity"]
        previous = None if attempt["predecessor"] == context["bound"]["initial_checkpoint"] else attempt["predecessor"]
        expected = training_command(cell, ident["seed"], ident["seen_samples"],
                                    Path(execution["destination"]) / "training", context, previous)
    else:
        snapshot = cell["initial_checkpoint"] if attempt["kind"] == "baseline" else attempt["checkpoint_path"]
        expected, output = evaluation_commands(cell["config_path"], snapshot, Path(execution["destination"]), context)[role]
        if execution["output_path"] != str(output):
            raise ValueError("planned stage output path mismatch")
    if execution["command"] != expected:
        raise ValueError("planned stage command mismatch")
    if execution["status"] == "succeeded":
        check_evidence(execution["evidence"])
        for key in ("checkpoint", "telemetry", "output"):
            if key in execution["evidence"]:
                check_artifact(execution["evidence"][key])
        actual = inspect_execution(execution, attempt, cell, context)
        if execution["evidence"] != actual:
            raise ValueError("retained stage evidence hash mismatch")


def verify_attempts(state, manifest, context, output_root, require_terminal=True):
    ids = [a["id"] for a in state["attempts"]]
    destinations = [a["destination"] for a in state["attempts"]]
    if len(ids) != len(set(ids)) or len(destinations) != len(set(destinations)):
        raise ValueError("duplicate retained attempt identity")
    if not attempt_directories(output_root).issubset(set(destinations)):
        raise ValueError("unrecorded orphan attempt directory")
    for attempt in state["attempts"]:
        audit_commands(attempt)
        audit_logs(attempt["destination"])
        for execution in attempt["executions"]:
            if execution["status"] == "invalidated" and "retained_inventory" in execution:
                audits = execution.get("integrity_audits", [])
                if not audits or any(a.get("recorded_inventory") != execution["retained_inventory"] for a in audits):
                    raise ValueError("invalidated stage lost its original recorded inventory")
        if require_terminal and attempt["status"] not in ("complete", "invalidated"):
            raise ValueError("unresolved pending attempt")
        if attempt["kind"] == "orphan" and attempt["status"] != "invalidated":
            raise ValueError("orphan attempt must have an audited invalidated status")
        if attempt["status"] == "invalidated":
            if not attempt.get("invalidation_reason") or "integrity_audit" not in attempt:
                raise ValueError("invalidated attempt requires an explicit audit")
        elif attempt["kind"] != "orphan":
            cell = manifest["cells"][attempt["cell"]]
            if attempt["kind"] == "node":
                identity = attempt["identity"]
                if identity != node_identity(attempt["cell"], cell, identity["seed"], identity["seen_samples"], context):
                    raise ValueError("retained node attempt identity mismatch")
            for execution in attempt["executions"]:
                check_execution(execution, attempt, cell, context)
                if require_terminal and execution["status"] not in ("succeeded", "invalidated", "canceled"):
                    raise ValueError("unresolved retained stage execution")
            if attempt["status"] == "complete":
                roles = [e["role"] for e in attempt["executions"] if e["status"] == "succeeded"]
                expected_roles = ["aerial", "got", "uav"] + (["train"] if attempt["kind"] == "node" else [])
                if sorted(roles) != sorted(expected_roles):
                    raise ValueError("complete attempt lacks exactly one success per stage")
        if attempt["status"] in ("complete", "invalidated") and inventory(attempt["destination"]) != attempt["retained_inventory"]:
            raise ValueError("retained attempt inventory/hash mismatch")
    by_id = {a["id"]: a for a in state["attempts"]}
    for record in ([state["baseline"]] if "baseline" in state else []) + state["candidates"]:
        attempt = by_id.get(record["attempt_id"])
        if attempt is None or attempt["status"] != "complete":
            raise ValueError("result has no completed retained attempt")
        successful = {e["role"]: e["evidence"] for e in attempt["executions"] if e["status"] == "succeeded"}
        if record["evaluations"] != {r: successful[r] for r in ("aerial", "uav", "got")}:
            raise ValueError("result evaluator evidence differs from stage journal")
        if attempt["kind"] == "node" and (record["training"] != successful["train"] or record["checkpoint"] != attempt["checkpoint"]["path"] or record["checkpoint_sha256"] != attempt["checkpoint"]["sha256"]):
            raise ValueError("result training evidence differs from stage journal")


class InvalidAttempt(ValueError):
    pass


def run_attempt(attempt, cell, context, state, table_path, runner):
    attempt["status"] = "running"
    atomic_json(table_path, state)
    roles = ("train", "aerial", "uav", "got") if attempt["kind"] == "node" else ("aerial", "uav", "got")
    results = {}
    for role in roles:
        execution = next(e for e in reversed(attempt["executions"]) if e["role"] == role)
        try:
            check_execution(execution, attempt, cell, context)
        except (ValueError, OSError, KeyError, EOFError, RuntimeError, pickle.UnpicklingError) as error:
            if execution["status"] == "succeeded":
                raise InvalidAttempt("retained successful stage is invalid: " + str(error))
            raise
        if execution["status"] in ("running", "interrupted"):
            try:
                execution["evidence"] = inspect_execution(execution, attempt, cell, context)
                execution["status"] = "succeeded"
                execution["recovery"] = "observed_success_after_interruption"
                atomic_json(table_path, state)
            except (ValueError, OSError, KeyError, EOFError, RuntimeError, pickle.UnpicklingError) as error:
                execution["status"] = "invalidated"
                execution["reason"] = "interrupted_stage_incomplete_or_invalid"
                execution.setdefault("retained_inventory", [])
                execution.setdefault("integrity_audits", []).append({
                    "reason": str(error), "recorded_inventory": execution["retained_inventory"],
                    "observed_inventory": inventory(execution["destination"]),
                })
                if role == "train":
                    raise InvalidAttempt("interrupted training has no valid completed checkpoint")
                number = sum(e["role"] == role for e in attempt["executions"]) + 1
                destination = Path(attempt["destination"]) / "evaluation" / ("retry-%s-%04d" % (role, number))
                snapshot = cell["initial_checkpoint"] if attempt["kind"] == "baseline" else attempt["checkpoint_path"]
                execution = plan_execution(role, destination, cell, context, snapshot=snapshot)
                attempt["executions"].append(execution)
                atomic_json(table_path, state)
        if execution["status"] == "planned":
            execution["status"] = "running"
            atomic_json(table_path, state)
            try:
                execute(execution["command"], Path(execution["log_path"]), runner)
                execution["evidence"] = inspect_execution(execution, attempt, cell, context)
                execution["status"] = "succeeded"
                if role == "train":
                    attempt["checkpoint"] = execution["evidence"]["checkpoint"]
                atomic_json(table_path, state)
            except BaseException as error:
                execution["status"] = "interrupted"
                execution["error"] = {"type": type(error).__name__, "message": str(error)}
                execution.setdefault("retained_inventory", inventory(execution["destination"]))
                atomic_json(table_path, state)
                raise
        if execution["status"] != "succeeded":
            raise InvalidAttempt("retained stage has no resumable success")
        results[role] = execution["evidence"]
        if role == "train":
            attempt["checkpoint"] = execution["evidence"]["checkpoint"]
    return results


def run_screening(manifest_path, output_root, runner=run_command, evaluation_inputs=None):
    output_root = Path(output_root).resolve()
    table_path = output_root / "candidate_table.json"
    existing = read_json(table_path) if table_path.is_file() else None
    if existing is not None and existing.get("schema_version") != 2:
        raise ValueError("schema 2 is required; preserve schema 1 fake evidence and use a new output root")
    backend = "subprocess" if runner is run_command else "fake_cpu_fixture"
    manifest, context = load_context(manifest_path, evaluation_inputs, backend)
    state = existing if existing is not None else {
        "schema_version": 2, "scoring_version": SCORING_VERSION, "context": context,
        "output_root": str(output_root),
        "candidates": [], "invalidated": [], "attempts": []}
    validate_state_identity(state, manifest, context, output_root)
    output_root.mkdir(parents=True, exist_ok=True)
    with (output_root / ".controller.lock").open("a+") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        if table_path.is_file():
            state = read_json(table_path)
            validate_state_identity(state, manifest, context, output_root)
        reconcile_retained_attempts(state, output_root)
        atomic_json(table_path, state)
        for cell_id, seed, samples in [(CELLS[0], None, None)] + [
                (c, s, n) for c in CELLS for s in SEEDS for n in NODES]:
            cell = manifest["cells"][cell_id]
            previous = None
            if seed is not None and samples != NODES[0]:
                predecessor = next(r for r in state["candidates"] if (r["cell"], r["seed"], r["seen_samples"]) == (cell_id, seed, samples - 2560))
                previous = {"path": predecessor["checkpoint"], "sha256": predecessor["checkpoint_sha256"]}
            records = [r for r in state["candidates"] if (r["cell"], r["seed"], r["seen_samples"]) == (cell_id, seed, samples)] if seed is not None else []
            record = records[0] if records else state.get("baseline") if seed is None else None
            attempt = next((a for a in reversed(state["attempts"]) if a["kind"] == ("baseline" if seed is None else "node") and a["cell"] == cell_id and (seed is None or (a["identity"]["seed"], a["identity"]["seen_samples"]) == (seed, samples)) and a["status"] != "invalidated"), None)
            if record is not None:
                try:
                    if attempt is None or attempt["status"] != "complete" or record["attempt_id"] != attempt["id"]:
                        raise ValueError("candidate has no matching completed attempt")
                    if inventory(attempt["destination"]) != attempt["retained_inventory"]:
                        raise ValueError("completed attempt inventory changed")
                    if seed is None:
                        validate_baseline(state, manifest, context)
                    else:
                        validate_record(record, cell, context, previous, state["baseline"]["metrics"])
                    continue
                except (ValueError, OSError, KeyError, EOFError, RuntimeError) as error:
                    if attempt is not None:
                        invalidate_attempt(attempt, str(error))
                    if seed is None:
                        state.pop("baseline", None)
                    else:
                        state["candidates"].remove(record)
                        state["invalidated"].append(record)
                    attempt = None
                    atomic_json(table_path, state)
            while True:
                if attempt is not None and seed is not None and attempt["predecessor"] != (previous or context["bound"]["initial_checkpoint"]):
                    invalidate_attempt(attempt, "predecessor_changed")
                    attempt = None
                if attempt is None:
                    root = output_root / "baseline" if seed is None else output_root / "nodes" / cell_id / str(seed) / str(samples)
                    attempt = allocate_attempt(state, root, cell_id, cell, context, seed, samples, previous)
                    atomic_json(table_path, state)  # durable plan exists before any command or directory creation
                try:
                    stages = run_attempt(attempt, cell, context, state, table_path, runner)
                    break
                except InvalidAttempt as error:
                    invalidate_attempt(attempt, str(error))
                    atomic_json(table_path, state)
                    attempt = None
            evaluations = {role: stages[role] for role in ("aerial", "uav", "got")}
            checkpoint = context["bound"]["initial_checkpoint"] if seed is None else attempt["checkpoint"]
            metrics = combine(evaluations, context["bound"]["config:" + cell_id], checkpoint)
            record = {"attempt_id": attempt["id"], "destination": attempt["destination"],
                      "evaluations": evaluations, "metrics": metrics}
            if seed is None:
                state["baseline"] = record
            else:
                identity = attempt["identity"]
                record.update(cell=cell_id, seed=seed, seen_samples=samples,
                              global_step=identity["global_step"], identity=identity,
                              checkpoint=checkpoint["path"], checkpoint_sha256=checkpoint["sha256"],
                              predecessor=attempt["predecessor"], training=stages["train"],
                              decision=guardrail_decision(metrics, state["baseline"]["metrics"]))
                state["candidates"].append(record)
                state["candidates"].sort(key=lambda r: (r["cell"], r["seed"], r["seen_samples"]))
            attempt["status"] = "complete"
            attempt["retained_inventory"] = inventory(attempt["destination"])
            atomic_json(table_path, state)
        verify_attempts(state, manifest, context, output_root)
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
