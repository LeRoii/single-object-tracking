#!/usr/bin/env python3
"""Conservative three-stage NanoTrack controller and hidden-evaluation gate.

The decision helpers are pure.  ``ThreeStageController`` accepts a command
factory and process runner so its complete state machine can be exercised
without CUDA, while the default runner records real process identities and
captured output.
"""

import argparse
import hashlib
import json
import math
import os
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Dict, List, Mapping, Optional, Sequence

import torch


WORKSPACE = Path(__file__).resolve().parents[1]
if str(WORKSPACE) not in sys.path:
    sys.path.insert(0, str(WORKSPACE))

EPSILON = 1e-12
SCORE_WEIGHTS = {"person_auc": 0.4, "vehicle_auc": 0.4, "overall_auc": 0.2}
STAGE_ORDER = ("stage1", "stage2", "stage3")
DEFAULT_STAGE_CANDIDATES = {"stage1": 8, "stage2": 6, "stage3": 4}
CANDIDATE_STEPS = 1250
STAGE1_MINIMUM_CANDIDATES = 6


class ProcessResult:
    def __init__(
        self,
        pid: int,
        start_time: str,
        end_time: str,
        exit_code: int,
        stdout: str,
        stderr: str,
    ) -> None:
        self.pid = int(pid)
        self.start_time = str(start_time)
        self.end_time = str(end_time)
        self.exit_code = int(exit_code)
        self.stdout = str(stdout)
        self.stderr = str(stderr)


class ProcessExecutionError(RuntimeError):
    def __init__(self, message: str, pid: Optional[int], start_time: str, end_time: str) -> None:
        super().__init__(message)
        self.pid = None if pid is None else int(pid)
        self.start_time = str(start_time)
        self.end_time = str(end_time)
        self.exit_code = None
        self.observed_exit = False


def _utc_now() -> str:
    import datetime

    return datetime.datetime.utcnow().replace(microsecond=0).isoformat() + "Z"


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with Path(path).open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def validate_candidate_checkpoint(path: Path, stage: str, stage_step: int, global_step: int) -> Dict[str, object]:
    checkpoint = torch.load(str(path), map_location="cpu")
    if checkpoint.get("stage") != stage:
        raise ValueError("candidate checkpoint stage does not match request")
    if int(checkpoint.get("global_step", -1)) != int(global_step):
        raise ValueError("candidate checkpoint global_step does not match request")
    if int(checkpoint.get("stage_step", -1)) != int(stage_step):
        raise ValueError("candidate checkpoint stage_step does not match request")
    return checkpoint


def atomic_write_json(path: Path, value: object) -> None:
    output = Path(path)
    output.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(
        prefix="." + output.name + ".", suffix=".tmp", dir=str(output.parent)
    )
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as handle:
            json.dump(value, handle, indent=2, sort_keys=True)
            handle.write("\n")
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary, str(output))
    except BaseException:
        try:
            os.unlink(temporary)
        except OSError:
            pass
        raise


def _finite_optional(metrics: Mapping[str, object], key: str) -> Optional[float]:
    value = metrics.get(key)
    if value is None:
        return None
    number = float(value)
    if not math.isfinite(number):
        raise ValueError("{} must be finite when present".format(key))
    return number


def primary_score(metrics: Mapping[str, object]) -> float:
    domains = metrics.get("domains")
    if domains is not None:
        if not isinstance(domains, Mapping) or not domains:
            raise ValueError("candidate domains must be a nonempty mapping")
        domain_scores = []
        for domain, values in domains.items():
            if not isinstance(values, Mapping):
                raise ValueError("domain {} metrics must be a mapping".format(domain))
            numerator = 0.0
            denominator = 0.0
            for key, weight in SCORE_WEIGHTS.items():
                value = _finite_optional(values, key)
                if value is not None:
                    numerator += weight * value
                    denominator += weight
            if denominator <= 0.0:
                raise ValueError("domain {} has no score terms".format(domain))
            domain_scores.append(numerator / denominator)
        return sum(domain_scores) / float(len(domain_scores))
    numerator = 0.0
    denominator = 0.0
    for key, weight in SCORE_WEIGHTS.items():
        value = _finite_optional(metrics, key)
        if value is not None:
            numerator += weight * value
            denominator += weight
    if denominator <= 0.0:
        raise ValueError("candidate has no score terms")
    return numerator / denominator


def _drop_exceeds(current: float, reference: float, limit: float) -> bool:
    return reference - current > limit + EPSILON


def replacement_decision(
    candidate: Mapping[str, object],
    stage_best: Mapping[str, object],
    official_baselines: Mapping[str, float],
) -> Dict[str, object]:
    candidate_score = primary_score(candidate)
    best_score = primary_score(stage_best)
    reasons = []  # type: List[str]
    if candidate_score + EPSILON < best_score + 0.005:
        reasons.append("score_improvement_below_0.005")
    for key in ("person_auc", "vehicle_auc"):
        current = _finite_optional(candidate, key)
        reference = _finite_optional(stage_best, key)
        if current is not None and reference is not None and _drop_exceeds(current, reference, 0.01):
            reasons.append("{}_drop_over_0.01".format(key))
    domains = candidate.get("domain_overall")
    if not isinstance(domains, Mapping):
        raise ValueError("candidate domain_overall must be a mapping")
    for domain, baseline in official_baselines.items():
        if domain not in domains:
            raise ValueError("candidate lacks official validation domain {}".format(domain))
        value = float(domains[domain])
        if not math.isfinite(value):
            raise ValueError("domain {} metric must be finite".format(domain))
        if _drop_exceeds(value, float(baseline), 0.02):
            reasons.append("domain_{}_drop_over_0.02".format(domain))
    return {
        "qualified": not reasons,
        "candidate_score": candidate_score,
        "stage_best_score": best_score,
        "score_improvement": candidate_score - best_score,
        "reasons": reasons,
    }


def early_stop_decision(
    stage: str,
    completed_candidate_points: int,
    consecutive_without_replacement: int,
    candidate: Mapping[str, object],
    stage_best: Mapping[str, object],
    official_baselines: Mapping[str, float],
) -> Dict[str, object]:
    if stage not in STAGE_ORDER:
        raise ValueError("unknown stage {}".format(stage))
    reasons = []  # type: List[str]
    for key in ("person_auc", "vehicle_auc"):
        current = _finite_optional(candidate, key)
        reference = _finite_optional(stage_best, key)
        if current is not None and reference is not None and _drop_exceeds(current, reference, 0.02):
            reasons.append("{}_severe_drop_over_0.02".format(key))
    domains = candidate.get("domain_overall")
    if not isinstance(domains, Mapping):
        raise ValueError("candidate domain_overall must be a mapping")
    for domain, baseline in official_baselines.items():
        if domain not in domains:
            raise ValueError("candidate lacks official validation domain {}".format(domain))
        if _drop_exceeds(float(domains[domain]), float(baseline), 0.03):
            reasons.append("domain_{}_severe_drop_over_0.03".format(domain))
    severe = bool(reasons)
    patience = int(consecutive_without_replacement) >= 2
    if patience:
        reasons.append("patience_2")
    trigger = severe or patience
    minimum_deferred = stage == "stage1" and completed_candidate_points < STAGE1_MINIMUM_CANDIDATES and trigger
    return {
        "should_stop": trigger and not minimum_deferred,
        "severe_regression": severe,
        "patience_exhausted": patience,
        "minimum_deferred": minimum_deferred,
        "reasons": reasons,
    }


def _auc(summary: Mapping[str, object], category: str) -> Optional[float]:
    categories = summary.get("categories", {})
    if not isinstance(categories, Mapping):
        return None
    aliases = (category,) if category != "vehicle" else ("vehicle", "car", "truck")
    values = []
    for alias in aliases:
        item = categories.get(alias)
        if isinstance(item, Mapping) and item.get("success_auc") is not None:
            values.append(float(item["success_auc"]))
    return sum(values) / float(len(values)) if values else None


def build_candidate_summary(
    stage: str,
    stage_step: int,
    global_step: int,
    checkpoint_path: Path,
    uav123_result: Mapping[str, object],
    aerial_result: Mapping[str, object],
    evaluator_hashes: Mapping[str, str],
) -> Dict[str, object]:
    checkpoint = Path(checkpoint_path).resolve()
    checkpoint_hash = sha256_file(checkpoint)
    for name, result in (("UAV123", uav123_result), ("val-aerial", aerial_result)):
        if result.get("snapshot_sha256") != checkpoint_hash:
            raise ValueError("{} snapshot hash does not match candidate".format(name))
    sources = aerial_result.get("metrics", {}).get("sources", {})
    if not isinstance(sources, Mapping) or not sources:
        raise ValueError("aerial evaluator has no source metrics")
    uav_overall = float(uav123_result["overall"]["success_auc"])
    domain_values = {
        "UAV123": {
            "person_auc": _auc(uav123_result, "person"),
            "vehicle_auc": _auc(uav123_result, "vehicle"),
            "overall_auc": uav_overall,
        }
    }
    for source, value in sources.items():
        domain_values[str(source)] = {
            "person_auc": value.get("person_auc"),
            "vehicle_auc": value.get("vehicle_auc"),
            "overall_auc": float(value["overall_auc"]),
        }
    macro = {}
    for key in ("person_auc", "vehicle_auc", "overall_auc"):
        present = [float(value[key]) for value in domain_values.values() if value.get(key) is not None]
        macro[key] = sum(present) / float(len(present)) if present else None
    summary = {
        "key": "{}:g{:08d}".format(stage, int(global_step)),
        "stage": stage,
        "stage_step": int(stage_step),
        "global_step": int(global_step),
        "checkpoint_path": str(checkpoint),
        "checkpoint_sha256": checkpoint_hash,
        "person_auc": macro["person_auc"],
        "vehicle_auc": macro["vehicle_auc"],
        "overall_auc": macro["overall_auc"],
        "domain_overall": {
            domain: float(value["overall_auc"]) for domain, value in domain_values.items()
        },
        "domains": domain_values,
        "evaluator_hashes": dict(evaluator_hashes),
        "validation_panel_sha256": aerial_result.get("panel_sha256"),
    }
    summary["primary_score"] = primary_score(summary)
    return summary


def validate_candidate_table(table: Mapping[str, object]) -> None:
    candidates = table.get("candidates", [])
    if not isinstance(candidates, list):
        raise ValueError("candidate table candidates must be a list")
    keys = {}  # type: Dict[str, Mapping[str, object]]
    positions = {}  # type: Dict[int, str]
    previous = -1
    for candidate in candidates:
        key = str(candidate["key"])
        global_step = int(candidate["global_step"])
        if global_step <= previous:
            raise ValueError("candidate global positions must be strictly monotonic")
        previous = global_step
        if key in keys:
            if dict(keys[key]) != dict(candidate):
                raise ValueError("candidate key conflict: {}".format(key))
            raise ValueError("duplicate candidate key: {}".format(key))
        if global_step in positions:
            raise ValueError("duplicate candidate global position {}".format(global_step))
        keys[key] = candidate
        positions[global_step] = key


def _read_json(path: Path) -> object:
    return json.loads(Path(path).read_text(encoding="utf-8"))


class SubprocessRunner:
    """Run an explicit argv, capturing the real pid, exit, stdout and stderr."""

    def __call__(self, command: Sequence[str], log_path: Path) -> ProcessResult:
        if not command or not all(isinstance(item, str) and item for item in command):
            raise ValueError("subprocess command must be a non-empty explicit string array")
        start = _utc_now()
        try:
            process = subprocess.Popen(
                list(command), stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                universal_newlines=True,
            )
        except BaseException as error:
            raise ProcessExecutionError(
                "process launch failed: {}".format(error), None, start, _utc_now()
            )
        try:
            stdout, stderr = process.communicate()
        except BaseException as error:
            end = _utc_now()
            raise ProcessExecutionError(
                "process communicate failed: {}".format(error), process.pid, start, end
            )
        end = _utc_now()
        log = Path(log_path)
        log.parent.mkdir(parents=True, exist_ok=True)
        with log.open("w", encoding="utf-8") as handle:
            handle.write("command={}\nexit_code={}\n--- stdout ---\n{}\n--- stderr ---\n{}".format(
                json.dumps(list(command)), process.returncode, stdout, stderr
            ))
            handle.flush()
            os.fsync(handle.fileno())
        return ProcessResult(process.pid, start, end, process.returncode, stdout, stderr)


class ThreeStageController:
    def __init__(
        self,
        run_root: Path,
        official_checkpoint: Path,
        official_metrics: Mapping[str, object],
        official_baselines: Mapping[str, float],
        validation_panel_sha256: str,
        hidden_panel_sha256: str,
        command_factory,
        process_runner=None,
        stage_candidate_limits: Optional[Mapping[str, int]] = None,
    ) -> None:
        self.run_root = Path(run_root).resolve()
        self.run_root.mkdir(parents=True, exist_ok=True)
        self.official_checkpoint = Path(official_checkpoint).resolve()
        if not self.official_checkpoint.is_file():
            raise FileNotFoundError(str(self.official_checkpoint))
        self.official_hash = sha256_file(self.official_checkpoint)
        self.official_metrics = dict(official_metrics)
        self.official_metrics["checkpoint_path"] = str(self.official_checkpoint)
        self.official_metrics["checkpoint_sha256"] = self.official_hash
        self.official_metrics["primary_score"] = primary_score(self.official_metrics)
        self.official_baselines = {str(key): float(value) for key, value in official_baselines.items()}
        self.validation_panel_sha256 = str(validation_panel_sha256)
        self.hidden_panel_sha256 = str(hidden_panel_sha256)
        self.command_factory = command_factory
        self.process_runner = process_runner or SubprocessRunner()
        self.stage_limits = dict(DEFAULT_STAGE_CANDIDATES)
        if stage_candidate_limits is not None:
            self.stage_limits.update({key: int(value) for key, value in stage_candidate_limits.items()})
        if set(self.stage_limits) != set(STAGE_ORDER) or any(value <= 0 for value in self.stage_limits.values()):
            raise ValueError("all three stage candidate limits must be positive")
        self.identity = {
            "official_checkpoint_sha256": self.official_hash,
            "official_metrics": {
                key: value for key, value in self.official_metrics.items()
                if key not in ("checkpoint_path", "primary_score")
            },
            "official_baselines": self.official_baselines,
            "validation_panel_sha256": self.validation_panel_sha256,
            "hidden_panel_sha256": self.hidden_panel_sha256,
            "stage_candidate_limits": self.stage_limits,
        }
        self.paths = {
            "run": self.run_root / "run_state.json",
            "table": self.run_root / "candidate_table.json",
            "selection": self.run_root / "selection.json",
            "interruptions": self.run_root / "interruptions.json",
        }
        self.journal_path = self.run_root / "state_journal.json"
        self._load_or_initialize()

    def _load_or_initialize(self) -> None:
        if self.journal_path.exists():
            journal = _read_json(self.journal_path)
            if journal.get("status") == "pending":
                documents = journal.get("documents")
                if not isinstance(documents, Mapping) or set(documents) != set(self.paths):
                    raise ValueError("pending state journal is incomplete")
                for name, value in documents.items():
                    atomic_write_json(self.paths[name], value)
                journal["status"] = "committed"
                atomic_write_json(self.journal_path, journal)
        existing = [path.exists() for path in self.paths.values()]
        if any(existing) and not all(existing):
            raise ValueError("partial controller state fails closed")
        if not any(existing):
            self.run_state = {
                "format_version": 1,
                "run_identity": self.identity,
                "status": "ready",
                "current_stage": "stage1",
                "global_step": 0,
                "generation": 0,
                "stages": {},
            }
            self.table = {"format_version": 1, "generation": 0, "candidates": []}
            self.selection = {
                "format_version": 1,
                "generation": 0,
                "selection_final": False,
                "stages": {},
            }
            self.interruptions = {"format_version": 1, "generation": 0, "processes": [], "events": []}
            self._persist_all()
            return
        self.run_state = _read_json(self.paths["run"])
        self.table = _read_json(self.paths["table"])
        self.selection = _read_json(self.paths["selection"])
        self.interruptions = _read_json(self.paths["interruptions"])
        generations = {
            int(document.get("generation", -1))
            for document in (self.run_state, self.table, self.selection, self.interruptions)
        }
        journal = _read_json(self.journal_path) if self.journal_path.exists() else {}
        if len(generations) != 1 or next(iter(generations)) != int(journal.get("generation", -2)):
            raise ValueError("controller state generation mismatch")
        if self.run_state.get("run_identity") != self.identity:
            raise ValueError("controller inputs do not match immutable run identity")
        validate_candidate_table(self.table)
        for candidate in self.table["candidates"]:
            checkpoint = Path(candidate["checkpoint_path"])
            if not checkpoint.is_file() or sha256_file(checkpoint) != candidate["checkpoint_sha256"]:
                raise ValueError("completed candidate checkpoint hash conflict: {}".format(candidate["key"]))
        if self.table["candidates"]:
            table_step = int(self.table["candidates"][-1]["global_step"])
            if int(self.run_state["global_step"]) != table_step:
                raise ValueError("run state and candidate table global positions conflict")
        self._validate_resume_artifacts()
        if not self.selection.get("selection_final"):
            self.interruptions["events"].append({
                "time": _utc_now(), "reason": "controller_restart", "observed_state": self.run_state["status"]
            })
            atomic_write_json(self.paths["interruptions"], self.interruptions)

    def _validate_resume_artifacts(self) -> None:
        candidates = self.table.get("candidates", [])
        stage_counts = {stage: 0 for stage in STAGE_ORDER}
        stage_failures = {stage: 0 for stage in STAGE_ORDER}
        stage_bests = {}
        stage_initializers = {}
        for candidate in candidates:
            root = self.run_root / "validation" / "{}_g{:08d}".format(
                candidate["stage"], int(candidate["global_step"])
            )
            paths = {
                "uav123": root / "uav123" / "summary.json",
                "val_aerial": root / "val-aerial.json",
            }
            for name, path in paths.items():
                if not path.is_file() or sha256_file(path) != candidate["evaluator_hashes"].get(name):
                    raise ValueError("evaluator result hash conflict: {}".format(name))
            uav = _read_json(paths["uav123"])
            aerial = _read_json(paths["val_aerial"])
            if aerial.get("panel_sha256") != self.validation_panel_sha256:
                raise ValueError("evaluator validation panel hash conflict")
            recomputed = build_candidate_summary(
                candidate["stage"], int(candidate["stage_step"]), int(candidate["global_step"]),
                Path(candidate["checkpoint_path"]), uav, aerial,
                candidate["evaluator_hashes"],
            )
            for key in (
                "person_auc", "vehicle_auc", "overall_auc", "domain_overall",
                "domains", "primary_score", "checkpoint_sha256", "validation_panel_sha256",
            ):
                if recomputed.get(key) != candidate.get(key):
                    raise ValueError("candidate summary does not match evaluator artifacts: {}".format(key))
            validate_candidate_checkpoint(
                Path(candidate["checkpoint_path"]), candidate["stage"],
                int(candidate["stage_step"]), int(candidate["global_step"]),
            )
            stage = candidate["stage"]
            if stage not in stage_bests:
                if stage == "stage1":
                    initializer = self.official_metrics
                elif stage == "stage2":
                    initializer = self.selection.get("stages", {}).get("stage1")
                else:
                    selected = self.selection.get("stages", {})
                    if "stage1" not in selected or "stage2" not in selected:
                        raise ValueError("stage3 candidate lacks frozen stage1/stage2 selections")
                    initializer = max((selected["stage1"], selected["stage2"]), key=primary_score)
                if initializer is None:
                    raise ValueError("{} candidate lacks selected prior-stage checkpoint".format(stage))
                stage_initializers[stage] = dict(initializer)
                stage_bests[stage] = dict(initializer)
            initializer = stage_initializers[stage]
            if stage_counts[stage] == 0:
                expected_input = initializer
            else:
                expected_input = next(
                    item for item in reversed(candidates[:candidates.index(candidate)])
                    if item.get("stage") == stage
                )
            if (candidate.get("initial_checkpoint_sha256") != initializer.get("checkpoint_sha256")
                    or candidate.get("input_checkpoint_sha256") != expected_input.get("checkpoint_sha256")):
                raise ValueError("candidate checkpoint initialization provenance mismatch")
            expected_resume = None if stage_counts[stage] == 0 else expected_input.get("checkpoint_path")
            if candidate.get("resume_checkpoint_path") != expected_resume:
                raise ValueError("candidate same-stage resume path does not match preceding candidate")
            stage_counts[stage] += 1
            best = stage_bests[stage]
            expected_replacement = replacement_decision(candidate, best, self.official_baselines)
            if candidate.get("replacement_decision") != expected_replacement:
                raise ValueError("candidate replacement decision does not match validation metrics")
            qualified = bool(expected_replacement["qualified"])
            if candidate.get("qualified_replacement") != qualified:
                raise ValueError("candidate replacement flag does not match decision")
            if qualified:
                best = candidate
                stage_bests[stage] = candidate
                stage_failures[stage] = 0
            else:
                stage_failures[stage] += 1
            expected_stop = early_stop_decision(
                stage, stage_counts[stage], stage_failures[stage], candidate,
                best, self.official_baselines,
            )
            if candidate.get("early_stop_decision") != expected_stop:
                raise ValueError("candidate early-stop decision does not match validation metrics")
        for name, selected in self.selection.get("stages", {}).items():
            path = Path(selected["checkpoint_path"])
            if not path.is_file() or sha256_file(path) != selected.get("checkpoint_sha256"):
                raise ValueError("selection checkpoint provenance/hash conflict: {}".format(name))
            allowed_sources = {
                "stage1": ("stage1",),
                "stage2": ("stage1", "stage2"),
                "stage3": ("stage1", "stage2", "stage3"),
            }[name]
            source = next((
                candidate for candidate in candidates
                if candidate.get("stage") in allowed_sources
                and candidate.get("checkpoint_sha256") == selected.get("checkpoint_sha256")
            ), None)
            if source is None:
                if selected.get("checkpoint_sha256") != self.official_hash:
                    raise ValueError("selection checkpoint provenance is missing: {}".format(name))
                source = self.official_metrics
            for key in ("person_auc", "vehicle_auc", "overall_auc", "domain_overall", "primary_score"):
                if selected.get(key) != source.get(key):
                    raise ValueError("selection metrics do not match checkpoint provenance: {}".format(name))
        if self.selection.get("selection_final"):
            path = Path(self.selection["final_checkpoint_path"])
            if not path.is_file() or sha256_file(path) != self.selection.get("final_checkpoint_sha256"):
                raise ValueError("final selection checkpoint provenance/hash conflict")
            selected_final = self.selection.get("stages", {}).get("stage3", {})
            if (self.selection["final_checkpoint_path"] != selected_final.get("checkpoint_path")
                    or self.selection["final_checkpoint_sha256"] != selected_final.get("checkpoint_sha256")
                    or self.selection.get("final_primary_score") != primary_score(selected_final)):
                raise ValueError("final selection does not match frozen stage3 selection")

    def _persist_all(self) -> None:
        validate_candidate_table(self.table)
        current = max(
            int(document.get("generation", 0))
            for document in (self.run_state, self.table, self.selection, self.interruptions)
        )
        generation = current + 1
        for document in (self.run_state, self.table, self.selection, self.interruptions):
            document["generation"] = generation
        documents = {
            "run": self.run_state,
            "table": self.table,
            "selection": self.selection,
            "interruptions": self.interruptions,
        }
        journal = {"generation": generation, "status": "pending", "documents": documents}
        atomic_write_json(self.journal_path, journal)
        for name, value in documents.items():
            atomic_write_json(self.paths[name], value)
        journal["status"] = "committed"
        atomic_write_json(self.journal_path, journal)

    def _command_for(self, action: str, context: Mapping[str, object]) -> List[str]:
        command = self.command_factory(action, dict(context))
        if not isinstance(command, (list, tuple)) or not command or not all(
            isinstance(item, str) and item for item in command
        ):
            raise ValueError("command factory must return an explicit argv array")
        return list(command)

    def _has_observed_success(self, action: str, context: Mapping[str, object]) -> bool:
        command = self._command_for(action, context)
        expected_artifact = context.get("checkpoint_path", context.get("output_path"))
        for process in self.interruptions["processes"]:
            if not (
                process.get("action") == action
                and process.get("command") == command
                and process.get("observed_exit") is True
                and int(process.get("exit_code", -1)) == 0
            ):
                continue
            if expected_artifact is not None:
                path = Path(str(expected_artifact))
                if (
                    process.get("artifact_path") != str(path)
                    or not path.is_file()
                    or process.get("artifact_sha256") != sha256_file(path)
                ):
                    continue
            return True
        return False

    def _record_reuse(self, action: str, path: Path) -> None:
        self.interruptions["events"].append({
            "time": _utc_now(), "reason": "verified_completed_artifact_reuse",
            "action": action, "path": str(path),
        })
        atomic_write_json(self.paths["interruptions"], self.interruptions)

    def _run_process(self, action: str, context: Mapping[str, object], label: str) -> ProcessResult:
        command = self._command_for(action, context)
        log_path = self.run_root / "logs" / (label + ".log")
        try:
            result = self.process_runner(list(command), log_path)
        except BaseException as error:
            process_event = {
                "time": _utc_now(), "reason": "process_runner_exception",
                "action": action, "detail": repr(error),
                "pid": getattr(error, "pid", None),
                "start_time": getattr(error, "start_time", _utc_now()),
                "end_time": getattr(error, "end_time", _utc_now()),
                "exit_code": getattr(error, "exit_code", None),
                "observed_exit": bool(getattr(error, "observed_exit", False)),
            }
            self.interruptions["events"].append({
                **process_event,
            })
            atomic_write_json(self.paths["interruptions"], self.interruptions)
            raise
        log_path.parent.mkdir(parents=True, exist_ok=True)
        with log_path.open("w", encoding="utf-8") as handle:
            handle.write("command={}\nexit_code={}\n--- stdout ---\n{}\n--- stderr ---\n{}".format(
                json.dumps(command), result.exit_code, result.stdout, result.stderr
            ))
            handle.flush()
            os.fsync(handle.fileno())
        record = {
            "action": action,
            "command": list(command),
            "pid": result.pid,
            "start_time": result.start_time,
            "end_time": result.end_time,
            "exit_code": result.exit_code,
            "stdout": result.stdout,
            "stderr": result.stderr,
            "log_path": str(log_path),
            "observed_exit": True,
        }
        artifact_value = context.get("checkpoint_path", context.get("output_path"))
        if artifact_value is not None:
            artifact_path = Path(str(artifact_value))
            if artifact_path.is_file():
                record["artifact_path"] = str(artifact_path)
                record["artifact_sha256"] = sha256_file(artifact_path)
        self.interruptions["processes"].append(record)
        atomic_write_json(self.paths["interruptions"], self.interruptions)
        if result.exit_code != 0:
            self.interruptions["events"].append({
                "time": _utc_now(), "reason": "nonzero_process_exit",
                "action": action, "exit_code": result.exit_code,
            })
            atomic_write_json(self.paths["interruptions"], self.interruptions)
            raise RuntimeError("{} exited {}".format(action, result.exit_code))
        return result

    def _stage_initializer(self, stage: str) -> Mapping[str, object]:
        if stage == "stage1":
            return self.official_metrics
        if stage == "stage2":
            return self.selection["stages"]["stage1"]
        eligible = [self.selection["stages"][name] for name in ("stage1", "stage2")]
        return max(eligible, key=primary_score)

    def _candidate_checkpoint_path(self, stage: str, stage_step: int, global_step: int) -> Path:
        epoch_zero = (stage_step - 1) // (2 * CANDIDATE_STEPS)
        step_in_epoch = stage_step - epoch_zero * 2 * CANDIDATE_STEPS
        return self.run_root / stage / "candidate_{}_g{:08d}_e{:03d}_s{:05d}.pth".format(
            stage, global_step, epoch_zero, step_in_epoch
        )

    def _run_candidate(
        self, stage: str, index: int, initializer: Mapping[str, object]
    ) -> Dict[str, object]:
        stage_step = index * CANDIDATE_STEPS
        global_step = int(self.run_state["global_step"]) + CANDIDATE_STEPS
        checkpoint = self._candidate_checkpoint_path(stage, stage_step, global_step)
        preceding = [
            item for item in self.table["candidates"] if item.get("stage") == stage
        ]
        resume_checkpoint = preceding[-1] if preceding else None
        input_checkpoint = resume_checkpoint or initializer
        base = {
            "stage": stage,
            "stage_step": stage_step,
            "global_step": global_step,
            "checkpoint_path": str(checkpoint),
            "initial_checkpoint_path": initializer["checkpoint_path"],
            "initial_checkpoint_sha256": initializer["checkpoint_sha256"],
            "input_checkpoint_path": input_checkpoint["checkpoint_path"],
            "input_checkpoint_sha256": input_checkpoint["checkpoint_sha256"],
            "resume_checkpoint_path": None if resume_checkpoint is None else resume_checkpoint["checkpoint_path"],
            "previous_global_step": global_step - CANDIDATE_STEPS,
        }
        if checkpoint.exists():
            if not self._has_observed_success("train", base):
                raise ValueError("orphan checkpoint lacks an observed successful train process")
            self._record_reuse("train", checkpoint)
        else:
            self._run_process("train", base, "{}_g{:08d}_train".format(stage, global_step))
        if not checkpoint.is_file():
            raise RuntimeError("training exited without candidate checkpoint {}".format(checkpoint))
        validate_candidate_checkpoint(checkpoint, stage, stage_step, global_step)
        checkpoint_hash = sha256_file(checkpoint)
        validation_root = self.run_root / "validation" / "{}_g{:08d}".format(stage, global_step)
        uav_path = validation_root / "uav123" / "summary.json"
        aerial_path = validation_root / "val-aerial.json"
        evaluation = dict(base)
        evaluation["checkpoint_sha256"] = checkpoint_hash
        evaluation["output_path"] = str(uav_path)
        if uav_path.exists():
            if not self._has_observed_success("uav123", evaluation):
                raise ValueError("orphan UAV123 result lacks an observed successful process")
            self._record_reuse("uav123", uav_path)
        else:
            self._run_process("uav123", evaluation, "{}_g{:08d}_uav123".format(stage, global_step))
        evaluation["output_path"] = str(aerial_path)
        evaluation["panel_sha256"] = self.validation_panel_sha256
        if aerial_path.exists():
            if not self._has_observed_success("val-aerial", evaluation):
                raise ValueError("orphan aerial result lacks an observed successful process")
            self._record_reuse("val-aerial", aerial_path)
        else:
            self._run_process("val-aerial", evaluation, "{}_g{:08d}_val_aerial".format(stage, global_step))
        uav_bytes = uav_path.read_bytes()
        aerial_bytes = aerial_path.read_bytes()
        aerial_payload = json.loads(aerial_bytes.decode("utf-8"))
        if aerial_payload.get("panel_sha256") != self.validation_panel_sha256:
            raise ValueError("val-aerial evaluator panel hash does not match configured panel")
        summary = build_candidate_summary(
            stage, stage_step, global_step, checkpoint,
            json.loads(uav_bytes.decode("utf-8")), aerial_payload,
            evaluator_hashes={
                "uav123": hashlib.sha256(uav_bytes).hexdigest(),
                "val_aerial": hashlib.sha256(aerial_bytes).hexdigest(),
            },
        )
        summary["initial_checkpoint_path"] = initializer["checkpoint_path"]
        summary["initial_checkpoint_sha256"] = initializer["checkpoint_sha256"]
        summary["input_checkpoint_path"] = base["input_checkpoint_path"]
        summary["input_checkpoint_sha256"] = base["input_checkpoint_sha256"]
        summary["resume_checkpoint_path"] = base["resume_checkpoint_path"]
        summary["previous_global_step"] = base["previous_global_step"]
        return summary

    def run(self) -> Dict[str, object]:
        if self.selection.get("selection_final"):
            return dict(self.selection)
        for stage in STAGE_ORDER:
            if stage in self.selection["stages"]:
                continue
            initializer = self._stage_initializer(stage)
            stage_best = dict(initializer)
            failures = 0
            completed = 0
            self.run_state["current_stage"] = stage
            self.run_state["status"] = "running"
            self._persist_all()
            existing = [candidate for candidate in self.table["candidates"] if candidate["stage"] == stage]
            persisted_stop = False
            for stored in existing:
                completed += 1
                failures = 0 if stored.get("qualified_replacement") else failures + 1
                if stored.get("qualified_replacement"):
                    stage_best = stored
                if stored.get("early_stop_decision", {}).get("should_stop"):
                    persisted_stop = True
            first_index = self.stage_limits[stage] + 1 if persisted_stop else completed + 1
            for index in range(first_index, self.stage_limits[stage] + 1):
                candidate = self._run_candidate(stage, index, initializer)
                replacement = replacement_decision(candidate, stage_best, self.official_baselines)
                candidate["replacement_decision"] = replacement
                candidate["qualified_replacement"] = bool(replacement["qualified"])
                if replacement["qualified"]:
                    stage_best = candidate
                    failures = 0
                else:
                    failures += 1
                completed += 1
                stop = early_stop_decision(
                    stage, completed, failures, candidate, stage_best, self.official_baselines
                )
                candidate["early_stop_decision"] = stop
                self.table["candidates"].append(candidate)
                self.run_state["global_step"] = candidate["global_step"]
                self.run_state["stages"][stage] = {
                    "completed_candidate_points": completed,
                    "consecutive_without_replacement": failures,
                    "selected_checkpoint_sha256": stage_best["checkpoint_sha256"],
                    "stop_decision": stop,
                }
                self._persist_all()
                if stop["should_stop"]:
                    break
            selected = dict(stage_best)
            selected["stage_completed_candidate_points"] = completed
            self.selection["stages"][stage] = selected
            self.run_state["stages"][stage]["complete"] = True
            self._persist_all()
        final = dict(self.selection["stages"]["stage3"])
        self.selection["final_checkpoint_path"] = final["checkpoint_path"]
        self.selection["final_checkpoint_sha256"] = final["checkpoint_sha256"]
        self.selection["final_primary_score"] = primary_score(final)
        self.selection["selection_final"] = True
        self.run_state["status"] = "selection_final"
        self.run_state["current_stage"] = None
        self._persist_all()
        return dict(self.selection)

    def run_hidden_evaluation(
        self,
        checkpoint_sha256: Optional[str] = None,
        panel_sha256: Optional[str] = None,
    ) -> Dict[str, object]:
        if not self.selection.get("selection_final"):
            raise RuntimeError("test-aerial-hidden is forbidden before immutable final selection")
        expected_checkpoint = str(self.selection["final_checkpoint_sha256"])
        expected_panel = self.hidden_panel_sha256
        requested_checkpoint = expected_checkpoint if checkpoint_sha256 is None else str(checkpoint_sha256)
        requested_panel = expected_panel if panel_sha256 is None else str(panel_sha256)
        if requested_checkpoint != expected_checkpoint:
            raise ValueError("hidden checkpoint hash does not match final selection")
        if requested_panel != expected_panel:
            raise ValueError("hidden panel hash mismatch")
        existing = self.run_state.get("hidden_evaluation")
        if existing is not None:
            if (existing["checkpoint_sha256"], existing["panel_sha256"]) != (
                requested_checkpoint, requested_panel
            ):
                raise ValueError("hidden evaluation identity conflict")
            result_path = Path(existing["result_path"])
            if not result_path.is_file():
                raise ValueError("recorded hidden result is missing")
            payload = result_path.read_bytes()
            if hashlib.sha256(payload).hexdigest() != existing["result_sha256"]:
                raise ValueError("recorded hidden result hash mismatch")
            result = json.loads(payload.decode("utf-8"))
            if result != existing["result"]:
                raise ValueError("recorded hidden result content mismatch")
            if result.get("snapshot_sha256") != requested_checkpoint:
                raise ValueError("recorded hidden output checkpoint hash mismatch")
            if result.get("panel_sha256") != requested_panel:
                raise ValueError("recorded hidden output panel hash mismatch")
            return dict(existing)
        output_path = self.run_root / "hidden" / "test-aerial-hidden.json"
        context = {
            "checkpoint_path": self.selection["final_checkpoint_path"],
            "checkpoint_sha256": requested_checkpoint,
            "panel_sha256": requested_panel,
            "output_path": str(output_path),
        }
        if output_path.exists():
            if not self._has_observed_success("test-aerial-hidden", context):
                raise ValueError("orphan hidden result lacks an observed successful process")
            self._record_reuse("test-aerial-hidden", output_path)
        else:
            self._run_process("test-aerial-hidden", context, "test_aerial_hidden")
        payload = output_path.read_bytes()
        result = json.loads(payload.decode("utf-8"))
        if result.get("snapshot_sha256") != requested_checkpoint:
            raise ValueError("hidden output checkpoint hash mismatch")
        if result.get("panel_sha256") != requested_panel:
            raise ValueError("hidden output panel hash mismatch")
        record = {
            "checkpoint_sha256": requested_checkpoint,
            "panel_sha256": requested_panel,
            "result_path": str(output_path),
            "result_sha256": hashlib.sha256(payload).hexdigest(),
            "result": result,
        }
        self.run_state["hidden_evaluation"] = record
        atomic_write_json(self.paths["run"], self.run_state)
        return dict(record)


def _main_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run-root", type=Path)
    parser.add_argument("--official-checkpoint", type=Path)
    parser.add_argument("--official-metrics", type=Path, required=True)
    parser.add_argument("--official-baselines", type=Path, required=True)
    parser.add_argument("--validation-panel-sha256", required=True)
    parser.add_argument("--hidden-panel-sha256", required=True)
    command_source = parser.add_mutually_exclusive_group(required=True)
    command_source.add_argument(
        "--command-plan", type=Path,
        help="JSON action-to-explicit-argv templates; use {context_key} placeholders",
    )
    command_source.add_argument(
        "--nanotrack-command-config", type=Path,
        help="JSON paths and stage policies for the built-in NanoTrack command factory",
    )
    parser.add_argument("--hidden-only", action="store_true")
    return parser


class JsonCommandFactory:
    def __init__(self, plan: Mapping[str, object]) -> None:
        self.plan = dict(plan)

    def __call__(self, action: str, context: Mapping[str, object]) -> List[str]:
        template = self.plan.get(action)
        if not isinstance(template, list):
            raise ValueError("command plan lacks explicit argv for {}".format(action))
        return [str(item).format(**context) for item in template]


class NanoTrackCommandFactory:
    """Build the real bounded training and evaluator argv contracts."""

    def __init__(
        self,
        workspace: Path,
        python_executable: Path,
        nanotrack_root: Path,
        stage_configs: Mapping[str, Path],
        stage_policies: Mapping[str, str],
        uav123_dataset_root: Path,
        uav123_prepared_root: Path,
        validation_panel: Path,
        hidden_panel: Path,
        device: str = "cuda:0",
        official_checkpoint: Optional[Path] = None,
        seed: int = 123456,
        artifact_root: Optional[Path] = None,
    ) -> None:
        self.workspace = Path(workspace).resolve()
        self.python = str(Path(python_executable).resolve())
        self.nanotrack_root = str(Path(nanotrack_root).resolve())
        self.configs = {key: str(Path(value).resolve()) for key, value in stage_configs.items()}
        self.policies = dict(stage_policies)
        self.uav_dataset = str(Path(uav123_dataset_root).resolve())
        self.uav_prepared = str(Path(uav123_prepared_root).resolve())
        self.validation_panel = str(Path(validation_panel).resolve())
        self.hidden_panel = str(Path(hidden_panel).resolve())
        self.device = str(device)
        self.seed = int(seed)
        self.artifact_root = None if artifact_root is None else Path(artifact_root).resolve()
        self.official_checkpoint = str(Path(
            official_checkpoint
            or (self.workspace / "repos/siamtrackers/NanoTrack/models/pretrained/nanotrackv3.pth")
        ).resolve())

    def _require_artifact_path(self, path: Path) -> Path:
        resolved = Path(path).resolve()
        if self.artifact_root is not None:
            try:
                resolved.relative_to(self.artifact_root)
            except ValueError:
                raise ValueError("NanoTrack output path is outside artifact_root: {}".format(resolved))
        return resolved

    def __call__(self, action: str, context: Mapping[str, object]) -> List[str]:
        stage = str(context.get("stage", "stage3"))
        config = self.configs[stage]
        snapshot = str(context.get("checkpoint_path", context.get("final_checkpoint_path", "")))
        if action == "train":
            checkpoint_path = self._require_artifact_path(Path(str(context["checkpoint_path"])))
            argv = [
                self.python, str(self.workspace / "scripts/train_nanotrack_uav123.py"),
                "--config", config, "--nanotrack-root", self.nanotrack_root,
                "--pretrained", self.official_checkpoint,
                "--output-dir", str(checkpoint_path.parent),
                "--stage", stage, "--policy", self.policies[stage],
                "--seed", str(self.seed),
                "--stop-after-global-step", str(int(context["global_step"])),
            ]
            resume = context.get("resume_checkpoint_path")
            if resume:
                argv.extend(["--resume", str(resume)])
            else:
                argv.extend([
                    "--initialize-from", str(context["input_checkpoint_path"]),
                    "--initial-global-step", str(int(context["previous_global_step"])),
                ])
            return argv
        if action == "uav123":
            output_dir = self._require_artifact_path(Path(str(context["output_path"])).parent)
            return [
                self.python, str(self.workspace / "scripts/evaluate_uav123_nanotrack.py"),
                "--dataset-root", self.uav_dataset, "--prepared-root", self.uav_prepared,
                "--nanotrack-root", self.nanotrack_root, "--config", config,
                "--snapshot", snapshot, "--split", "val",
                "--output-dir", str(output_dir),
            ]
        if action in ("val-aerial", "test-aerial-hidden"):
            panel = self.hidden_panel if action == "test-aerial-hidden" else self.validation_panel
            output_path = self._require_artifact_path(Path(str(context["output_path"])))
            return [
                self.python, str(self.workspace / "scripts/evaluate_aerial_tracklets_nanotrack.py"),
                "--panel", panel, "--nanotrack-root", self.nanotrack_root,
                "--config", config, "--snapshot", snapshot,
                "--output", str(output_path), "--device", self.device,
                "--expected-panel-sha256", str(context["panel_sha256"]),
            ]
        raise ValueError("unsupported NanoTrack action {}".format(action))


def load_nanotrack_command_config(
    config_path: Path, cli_official_checkpoint: Optional[Path] = None
):
    """Load the formal built-in command config and bind all generated outputs."""
    source = Path(config_path).resolve()
    config = _read_json(source)

    def config_path_value(value: object) -> Path:
        path = Path(str(value))
        return (path if path.is_absolute() else source.parent / path).resolve()

    required = (
        "workspace", "python_executable", "nanotrack_root", "stage_configs",
        "stage_policies", "uav123_dataset_root", "uav123_prepared_root",
        "validation_panel", "hidden_panel", "official_checkpoint",
        "artifact_root", "run_root", "seed",
    )
    missing = [key for key in required if key not in config]
    if missing:
        raise ValueError("NanoTrack command config is missing {}".format(", ".join(missing)))
    if set(config["stage_configs"]) != set(STAGE_ORDER):
        raise ValueError("NanoTrack command config must specify all stage configs")
    expected_policies = {"stage1": "head_only", "stage2": "tail2", "stage3": "head_only"}
    if config["stage_policies"] != expected_policies:
        raise ValueError("NanoTrack command config stage_policies do not match the approved policies")
    artifact_root = config_path_value(config["artifact_root"])
    run_root = config_path_value(config["run_root"])
    try:
        run_root.relative_to(artifact_root)
    except ValueError:
        raise ValueError("NanoTrack run_root must be inside artifact_root")
    official_checkpoint = config_path_value(config["official_checkpoint"])
    if cli_official_checkpoint is not None and Path(cli_official_checkpoint).resolve() != official_checkpoint:
        raise ValueError("--official-checkpoint conflicts with NanoTrack command config")
    factory = NanoTrackCommandFactory(
        workspace=config_path_value(config["workspace"]),
        python_executable=config_path_value(config["python_executable"]),
        nanotrack_root=config_path_value(config["nanotrack_root"]),
        stage_configs={stage: config_path_value(config["stage_configs"][stage]) for stage in STAGE_ORDER},
        stage_policies=config["stage_policies"],
        uav123_dataset_root=config_path_value(config["uav123_dataset_root"]),
        uav123_prepared_root=config_path_value(config["uav123_prepared_root"]),
        validation_panel=config_path_value(config["validation_panel"]),
        hidden_panel=config_path_value(config["hidden_panel"]),
        device=config.get("device", "cuda:0"),
        official_checkpoint=official_checkpoint,
        seed=int(config["seed"]),
        artifact_root=artifact_root,
    )
    return factory, run_root, official_checkpoint, artifact_root


def main(argv: Optional[Sequence[str]] = None) -> None:
    args = _main_parser().parse_args(argv)
    metrics = _read_json(args.official_metrics)
    baselines = _read_json(args.official_baselines)
    if args.command_plan is not None:
        if args.run_root is None or args.official_checkpoint is None:
            raise ValueError("--command-plan requires --run-root and --official-checkpoint")
        run_root = args.run_root.resolve()
        official_checkpoint = args.official_checkpoint.resolve()
        command_factory = JsonCommandFactory(_read_json(args.command_plan))
    else:
        command_factory, configured_run_root, official_checkpoint, artifact_root = (
            load_nanotrack_command_config(args.nanotrack_command_config, args.official_checkpoint)
        )
        if args.run_root is not None and args.run_root.resolve() != configured_run_root:
            raise ValueError("--run-root conflicts with NanoTrack command config")
        run_root = configured_run_root
        try:
            run_root.relative_to(artifact_root)
        except ValueError:
            raise ValueError("--run-root must be inside NanoTrack artifact_root")
    controller = ThreeStageController(
        run_root, official_checkpoint, metrics, baselines,
        args.validation_panel_sha256, args.hidden_panel_sha256,
        command_factory, SubprocessRunner(),
    )
    result = controller.run_hidden_evaluation() if args.hidden_only else controller.run()
    print(json.dumps(result, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
