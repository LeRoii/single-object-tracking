#!/usr/bin/env python3
"""Build and validate the model-interface manifest used by host and board."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
from typing import Any, Dict


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _require_model(model: Dict[str, Any], name: str, inputs: int, outputs: int) -> None:
    if len(model.get("inputs", [])) != inputs:
        raise ValueError(f"{name} must have {inputs} input tensor(s)")
    if len(model.get("outputs", [])) != outputs:
        raise ValueError(f"{name} must have {outputs} output tensor(s)")
    file_hash = model.get("sha256", "")
    if len(file_hash) != 64:
        raise ValueError(f"{name} sha256 must contain 64 hexadecimal characters")
    try:
        int(file_hash, 16)
    except ValueError as exc:
        raise ValueError(f"{name} sha256 is not hexadecimal") from exc


def _feature_shapes_match(output: Dict[str, Any], head_input: Dict[str, Any]) -> bool:
    """Compare one feature tensor by logical N/C/H/W axes, not stored axis order."""
    output_shape = output.get("shape")
    input_shape = head_input.get("shape")
    if not isinstance(output_shape, list) or not isinstance(input_shape, list):
        return False
    if output.get("layout") == head_input.get("layout"):
        return output_shape == input_shape
    if output.get("layout") == "NCHW" and head_input.get("layout") == "NHWC":
        return len(output_shape) == 4 and input_shape == [
            output_shape[0], output_shape[2], output_shape[3], output_shape[1]
        ]
    if output.get("layout") == "NHWC" and head_input.get("layout") == "NCHW":
        return len(output_shape) == 4 and input_shape == [
            output_shape[0], output_shape[3], output_shape[1], output_shape[2]
        ]
    return False


def build_manifest(
    template: Dict[str, Any],
    search: Dict[str, Any],
    head: Dict[str, Any],
    provenance: Dict[str, Any],
) -> Dict[str, Any]:
    if provenance.get("target_platform") != "rk3566":
        raise ValueError("target_platform must be rk3566")

    _require_model(template, "template_backbone", inputs=1, outputs=1)
    _require_model(search, "search_backbone", inputs=1, outputs=1)
    _require_model(head, "head", inputs=2, outputs=2)

    if not _feature_shapes_match(template["outputs"][0], head["inputs"][0]):
        raise ValueError("head template feature shape does not match template backbone output")
    if not _feature_shapes_match(search["outputs"][0], head["inputs"][1]):
        raise ValueError("head search feature shape does not match search backbone output")

    return {
        "schema_version": 1,
        "provenance": dict(provenance),
        "models": {
            "template_backbone": template,
            "search_backbone": search,
            "head": head,
        },
    }


def write_manifest(path: Path, manifest: Dict[str, Any]) -> None:
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
