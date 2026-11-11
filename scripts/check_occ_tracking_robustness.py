#!/usr/bin/env python3
"""Validate a development-only occ tracking robustness plateau manifest."""

import argparse
import json
import math
import sys
from pathlib import Path


SCHEMA_VERSION = 1
REQUIRED_GATES = {
    "tracking_schema", "visible_boxes", "full_occlusion_semantics",
    "prediction_continuity", "reacquisition", "continuous_control",
    "identity_proxy",
}


def _object(value, description):
    if not isinstance(value, dict):
        raise ValueError("{} must be an object".format(description))
    return value


def _load_json(path, description):
    path = Path(path)
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise ValueError("invalid {} {}: {}".format(description, path, error)) from error
    return _object(value, description)


def _nonempty_names(value, description):
    if not isinstance(value, list) or not value or any(
        not isinstance(item, str) or not item for item in value
    ) or len(set(value)) != len(value):
        raise ValueError("{} must contain unique nonempty names".format(description))
    return value


def _require_coverage(manifest, records):
    nominal = [item for item in records if item["category"] == "nominal"]
    if len(nominal) != 1 or nominal[0]["variant"] != "nominal":
        raise ValueError("missing unique nominal report")

    for parameter in _nonempty_names(manifest.get("real_parameters"), "real_parameters"):
        observed = {
            item["variant"] for item in records
            if item["category"] == "real" and item.get("parameter") == parameter
        }
        for required in ("-10%", "+10%"):
            if required not in observed:
                raise ValueError("missing {} neighbor {}".format(parameter, required))
    for parameter in _nonempty_names(manifest.get("count_parameters"), "count_parameters"):
        observed = {
            item["variant"] for item in records
            if item["category"] == "count" and item.get("parameter") == parameter
        }
        for required in ("-1", "+1"):
            if required not in observed:
                raise ValueError("missing {} neighbor {}".format(parameter, required))
    for category, key in (("initialization", "initialization_variants"),
                          ("fault", "fault_variants")):
        required = set(_nonempty_names(manifest.get(key), key))
        observed = {item["variant"] for item in records if item["category"] == category}
        missing = sorted(required - observed)
        if missing:
            raise ValueError("missing {} variants: {}".format(category, ", ".join(missing)))
        unexpected = sorted(observed - required)
        if unexpected:
            raise ValueError("unexpected {} variants: {}".format(category, ", ".join(unexpected)))
    return nominal[0]


def _validate_safety(manifest):
    safety = _object(manifest.get("safety_audit"), "safety_audit")
    if isinstance(safety.get("confirmation_required"), bool) or not isinstance(
        safety.get("confirmation_required"), int
    ) or safety["confirmation_required"] < 2:
        raise ValueError("confirmation_required safety invariant is not satisfied")
    if safety.get("detector_direct_control") is not False:
        raise ValueError("detector direct control safety invariant is not satisfied")
    for key in ("generation_stale_isolation", "fail_closed_faults"):
        if safety.get(key) is not True:
            raise ValueError("{} safety invariant is not satisfied".format(key))
    if not isinstance(safety.get("evidence"), str) or not safety["evidence"]:
        raise ValueError("safety audit evidence is missing")
    return safety


def evaluate_manifest(manifest, manifest_path=None):
    manifest = _object(manifest, "manifest")
    if manifest.get("schema_version") != SCHEMA_VERSION:
        raise ValueError("manifest schema_version must be 1")
    candidate = _object(manifest.get("candidate"), "candidate")
    for key in ("id", "source_commit", "model_sha256", "nominal_config_sha256"):
        if not isinstance(candidate.get(key), str) or not candidate[key]:
            raise ValueError("candidate {} is missing".format(key))
    records = manifest.get("reports")
    if not isinstance(records, list) or not records:
        raise ValueError("reports must be a nonempty list")
    labels = [item.get("label") for item in records if isinstance(item, dict)]
    raw_paths = [item.get("path") for item in records if isinstance(item, dict)]
    if len(labels) != len(records) or len(set(labels)) != len(labels):
        raise ValueError("duplicate or invalid report label")
    if len(raw_paths) != len(records) or len(set(raw_paths)) != len(raw_paths):
        raise ValueError("duplicate or invalid report path")
    nominal_record = _require_coverage(manifest, records)
    safety = _validate_safety(manifest)

    base = Path(manifest_path).resolve().parent if manifest_path else Path.cwd()
    loaded = []
    for item in records:
        for key in ("label", "category", "variant", "path", "config_sha256"):
            if not isinstance(item.get(key), str) or not item[key]:
                raise ValueError("report record has invalid {}".format(key))
        report_path = Path(item["path"])
        if not report_path.is_absolute():
            report_path = base / report_path
        payload = _load_json(report_path, "evaluator report")
        if payload.get("schema_version") != 1:
            raise ValueError("report {} schema mismatch".format(item["label"]))
        if payload.get("scope") != "development" or payload.get("frame_range") != [0, 189]:
            raise ValueError("report {} must be development-only 0--189".format(item["label"]))
        provenance = _object(payload.get("provenance"), "report provenance")
        for key in ("source_commit", "model_sha256"):
            if provenance.get(key) != candidate[key]:
                raise ValueError("report {} {} mismatch".format(item["label"], key))
        if provenance.get("config_sha256") != item["config_sha256"]:
            raise ValueError("report {} config_sha256 mismatch".format(item["label"]))
        if item is nominal_record and item["config_sha256"] != candidate["nominal_config_sha256"]:
            raise ValueError("nominal config_sha256 mismatch")
        gates = _object(payload.get("gates"), "report gates")
        if not REQUIRED_GATES.issubset(gates) or any(
            not isinstance(gates[name], dict) or gates[name].get("passed") is not True
            for name in REQUIRED_GATES
        ) or payload.get("verdict") != "PASS":
            raise ValueError("robustness plateau report {} is not PASS".format(item["label"]))
        reacquisition = _object(payload.get("reacquisition"), "reacquisition")
        first_frame = reacquisition.get("first_frame")
        if isinstance(first_frame, bool) or not isinstance(first_frame, int) or first_frame >= 190:
            raise ValueError("report {} reacquisition is not before 190".format(item["label"]))
        try:
            mean_iou = float(payload["strata"]["visible"]["mean_iou"])
        except (KeyError, TypeError, ValueError) as error:
            raise ValueError("report {} visible mean IoU is invalid".format(item["label"])) from error
        if not math.isfinite(mean_iou):
            raise ValueError("report {} visible mean IoU is invalid".format(item["label"]))
        loaded.append({"record": item, "report": payload, "mean_iou": mean_iou})

    nominal = next(item for item in loaded if item["record"] is nominal_record)
    max_drop = max(nominal["mean_iou"] - item["mean_iou"] for item in loaded)
    if max_drop > 0.03 + 1e-12:
        raise ValueError("visible mean IoU drop {:.6f} exceeds 0.03".format(max_drop))
    return {
        "schema_version": SCHEMA_VERSION,
        "candidate": candidate,
        "verdict": "PASS",
        "report_count": len(loaded),
        "nominal_visible_mean_iou": nominal["mean_iou"],
        "max_visible_mean_iou_drop": max_drop,
        "latest_reacquisition_frame": max(
            item["report"]["reacquisition"]["first_frame"] for item in loaded
        ),
        "safety_audit": safety,
        "reports": [
            {
                "label": item["record"]["label"],
                "category": item["record"]["category"],
                "variant": item["record"]["variant"],
                "visible_mean_iou": item["mean_iou"],
                "reacquisition_frame": item["report"]["reacquisition"]["first_frame"],
            }
            for item in loaded
        ],
    }


def render_markdown(result):
    lines = [
        "# occ tracking robustness plateau", "",
        "- Candidate: `{}`".format(result["candidate"]["id"]),
        "- Verdict: **{}**".format(result["verdict"]),
        "- Reports: `{}`".format(result["report_count"]),
        "- Nominal visible mean IoU: `{:.6f}`".format(result["nominal_visible_mean_iou"]),
        "- Maximum visible mean IoU drop: `{:.6f}`".format(result["max_visible_mean_iou_drop"]),
        "- Latest reacquisition frame: `{}`".format(result["latest_reacquisition_frame"]),
        "", "## Perturbations", "",
    ]
    lines.extend(
        "- `{}`: IoU `{:.6f}`, reacquisition `{}`".format(
            item["label"], item["visible_mean_iou"], item["reacquisition_frame"]
        ) for item in result["reports"]
    )
    lines.append("")
    return "\n".join(lines)


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--output-json", required=True, type=Path)
    parser.add_argument("--output-markdown", required=True, type=Path)
    args = parser.parse_args(argv)
    try:
        manifest = _load_json(args.manifest, "candidate manifest")
        result = evaluate_manifest(manifest, args.manifest)
        args.output_json.parent.mkdir(parents=True, exist_ok=True)
        args.output_markdown.parent.mkdir(parents=True, exist_ok=True)
        args.output_json.write_text(
            json.dumps(result, indent=2, sort_keys=True, allow_nan=False) + "\n",
            encoding="utf-8",
        )
        args.output_markdown.write_text(render_markdown(result), encoding="utf-8")
    except ValueError as error:
        print("robustness error: {}".format(error), file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
