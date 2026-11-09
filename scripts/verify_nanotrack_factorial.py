#!/usr/bin/env python3
"""Read-only verification of NanoTrack factorial screening evidence."""
import argparse
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
from scripts.run_nanotrack_factorial import (
    CELLS, NODES, SEEDS, load_context, read_json, summarize,
    validate_baseline, validate_record, validate_state_identity, verify_attempts,
)


def verify_experiment(manifest_path, output_root):
    """Raise ValueError for incomplete, altered or inconsistent retained evidence."""
    output_root = Path(output_root).resolve()
    state = read_json(output_root / "candidate_table.json")
    if state.get("schema_version") != 2:
        raise ValueError("schema 2 is required; schema 1 fake evidence must remain in its original root")
    manifest, context = load_context(manifest_path, state["context"]["evaluation_inputs"],
                                     state["context"]["execution_backend"])
    validate_state_identity(state, manifest, context, output_root)
    verify_attempts(state, manifest, context, output_root)
    expected = {(cell, seed, samples) for cell in CELLS for seed in SEEDS for samples in NODES}
    if {(r["cell"], r["seed"], r["seen_samples"]) for r in state["candidates"]} != expected:
        raise ValueError("incomplete matrix: exactly 72 nodes required")
    baseline = validate_baseline(state, manifest, context)
    for cell in CELLS:
        for seed in SEEDS:
            previous = None
            for samples in NODES:
                record = next(r for r in state["candidates"] if (r["cell"], r["seed"], r["seen_samples"]) == (cell, seed, samples))
                previous = validate_record(record, manifest["cells"][cell], context, previous, baseline)
    summary = read_json(output_root / "cell_summary.json")
    if summary != summarize(manifest, state):
        raise ValueError("cell summary/selection mismatch")
    return {"valid": True, "completed_nodes": 72, "scoring_version": context["scoring_version"],
            "execution_backend": context["execution_backend"],
            "state": summary["state"], "expansion_cells": summary["expansion_cells"],
            "promotion_authorized": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--output-root", required=True, type=Path)
    args = parser.parse_args()
    try:
        verdict = verify_experiment(args.manifest, args.output_root)
    except (ValueError, OSError, KeyError) as error:
        print(json.dumps({"valid": False, "error": str(error)}))
        raise SystemExit(1)
    print(json.dumps(verdict, sort_keys=True))


if __name__ == "__main__":
    main()
