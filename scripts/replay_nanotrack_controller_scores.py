#!/usr/bin/env python3
"""Read-only corrected-score replay for the frozen three-stage run."""

import argparse
import hashlib
import json
import sys
from pathlib import Path
from typing import Dict, Mapping, Tuple

WORKSPACE = Path(__file__).resolve().parents[1]
if str(WORKSPACE) not in sys.path:
    sys.path.insert(0, str(WORKSPACE))

from scripts.run_nanotrack_three_stage import (  # noqa: E402
    early_stop_decision,
    primary_score,
    replacement_decision,
)


def _read_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _legacy_metrics(candidate: Mapping[str, object]) -> Dict[str, object]:
    return {key: value for key, value in candidate.items() if key != "domains"}


def _simulate(candidates, initializer, baselines, legacy=False):
    stage_best: Dict[str, Mapping[str, object]] = {}
    failures: Dict[str, int] = {}
    completed: Dict[str, int] = {}
    decisions = []
    stops = {}
    for candidate in candidates:
        stage = str(candidate["stage"])
        if stage not in stage_best:
            stage_best[stage] = initializer(stage, legacy)
            failures[stage] = 0
            completed[stage] = 0
        proposal = _legacy_metrics(candidate) if legacy else candidate
        best = _legacy_metrics(stage_best[stage]) if legacy else stage_best[stage]
        replacement = replacement_decision(proposal, best, baselines)
        if replacement["qualified"]:
            stage_best[stage] = candidate
            failures[stage] = 0
        else:
            failures[stage] += 1
        completed[stage] += 1
        stop = early_stop_decision(
            stage, completed[stage], failures[stage], proposal,
            _legacy_metrics(stage_best[stage]) if legacy else stage_best[stage], baselines,
        )
        decisions.append({
            "key": candidate["key"],
            "stage": stage,
            "global_step": int(candidate["global_step"]),
            "primary_score": primary_score(proposal),
            "stage_best_before": best.get("key"),
            "replacement": replacement,
            "early_stop": stop,
        })
        if stop["should_stop"]:
            stops[stage] = {
                "global_step": int(candidate["global_step"]),
                "candidate_points": completed[stage],
                "decision": stop,
            }
    selected = dict(stage_best)
    return decisions, stops, selected


def build_replay(artifact_root: Path) -> Dict[str, object]:
    run_root = artifact_root / "run"
    baseline_root = artifact_root / "baseline"
    final_root = artifact_root / "final"
    table_path = run_root / "candidate_table.json"
    official_path = baseline_root / "official_metrics.json"
    baselines_path = baseline_root / "official_baselines.json"
    selection_path = run_root / "selection.json"
    state_path = run_root / "run_state.json"
    journal_path = run_root / "state_journal.json"
    val_panel = WORKSPACE / "data/nanotrack_conservative_three_stage/val-aerial.json"
    hidden_panel = WORKSPACE / "data/nanotrack_conservative_three_stage/test-aerial-hidden.json"
    source_paths = (table_path, official_path, baselines_path, selection_path,
                    state_path, journal_path, val_panel, hidden_panel)
    sources = {
        str(path.relative_to(WORKSPACE)): {"sha256": _sha256(path), "bytes": path.stat().st_size}
        for path in source_paths
    }

    candidates = _read_json(table_path)["candidates"]
    official = _read_json(official_path)
    baselines = _read_json(baselines_path)
    selection = _read_json(selection_path)
    state = _read_json(state_path)

    def initializer(stage, legacy):
        if stage == "stage1":
            return official
        if stage == "stage2":
            return selection["stages"]["stage1"]
        previous = (selection["stages"]["stage1"], selection["stages"]["stage2"])
        score = lambda candidate: primary_score(_legacy_metrics(candidate)) if legacy else primary_score(candidate)
        return max(previous, key=score)

    legacy_decisions, legacy_stops, legacy_selected = _simulate(
        candidates, initializer, baselines, legacy=True
    )
    corrected_decisions, corrected_stops, corrected_selected = _simulate(
        candidates, initializer, baselines, legacy=False
    )
    recorded_stops = {}
    for stage, value in state["stages"].items():
        stage_candidates = [candidate for candidate in candidates if candidate["stage"] == stage]
        recorded_stops[stage] = {
            "global_step": int(stage_candidates[-1]["global_step"]),
            "candidate_points": int(value["completed_candidate_points"]),
            "decision": value["stop_decision"],
        }
    recorded_final = selection["stages"]["stage3"]
    corrected_final = max(corrected_selected.values(), key=primary_score)
    keys_changed = []
    legacy_by_key = {item["key"]: item for item in legacy_decisions}
    corrected_by_key = {item["key"]: item for item in corrected_decisions}
    for candidate in candidates:
        old = legacy_by_key[candidate["key"]]["replacement"]["qualified"]
        new = corrected_by_key[candidate["key"]]["replacement"]["qualified"]
        if old != new:
            keys_changed.append({"key": candidate["key"], "legacy_qualified": old,
                                 "corrected_qualified": new})

    return {
        "format_version": 1,
        "purpose": "read-only replay of frozen candidates using domain-first controller scoring",
        "scoring": "renormalize 0.4 person / 0.4 vehicle / 0.2 overall within each domain, then macro-average domains",
        "source_artifacts": sources,
        "legacy_decisions": legacy_decisions,
        "corrected_decisions": corrected_decisions,
        "changed_replacement_decisions": keys_changed,
        "corrected_selection": {
            "selected_stage1": corrected_selected["stage1"]["key"],
            "selected_stage2": corrected_selected["stage2"]["key"],
            "selected_stage3": corrected_selected["stage3"]["key"],
            "final_key": corrected_final["key"],
            "final_primary_score": primary_score(corrected_final),
            "matches_frozen_selection": corrected_final["key"] == recorded_final["key"],
        },
        "stage_stops": {
            stage: {
                "recorded": recorded_stops[stage],
                "legacy_replay": legacy_stops.get(stage),
                "corrected_replay": corrected_stops.get(stage),
                "unchanged": corrected_stops.get(stage) == recorded_stops[stage],
            }
            for stage in sorted(recorded_stops)
        },
        "g1250_score": corrected_by_key["stage1:g00001250"]["primary_score"],
        "g2500_score": corrected_by_key["stage1:g00002500"]["primary_score"],
        "g2500_gain_vs_g1250": (
            corrected_by_key["stage1:g00002500"]["primary_score"]
            - corrected_by_key["stage1:g00001250"]["primary_score"]
        ),
        "hidden_evaluation_rerun": False,
        "historical_artifacts_mutated": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifact-root", type=Path, default=WORKSPACE / "artifacts/nanotrack_conservative_three_stage")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report = build_replay(args.artifact_root.resolve())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps({
        "output": str(args.output.resolve()),
        "g1250_score": report["g1250_score"],
        "g2500_score": report["g2500_score"],
        "g2500_gain_vs_g1250": report["g2500_gain_vs_g1250"],
        "changed_replacement_decisions": report["changed_replacement_decisions"],
        "corrected_selection": report["corrected_selection"],
        "stage_stops_unchanged": all(item["unchanged"] for item in report["stage_stops"].values()),
    }, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
