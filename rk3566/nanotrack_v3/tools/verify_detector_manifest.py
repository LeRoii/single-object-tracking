#!/usr/bin/env python3
"""Validate the RK3566 YOLOv8 detector manifest and artifact identities."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any, Dict, Iterable, Optional


class ManifestError(ValueError):
    """The detector manifest or one of its referenced artifacts is invalid."""


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _require_mapping(value: Any, name: str) -> Dict[str, Any]:
    if not isinstance(value, dict):
        raise ManifestError(f"{name} must be a JSON object")
    return value


def _require_keys(value: Dict[str, Any], keys: Iterable[str], name: str) -> None:
    for key in keys:
        if key not in value:
            raise ManifestError(f"{name} is missing required key: {key}")


def _require_sha256(value: Any, name: str) -> str:
    if not isinstance(value, str) or len(value) != 64:
        raise ManifestError(f"{name} must be 64 hexadecimal characters")
    try:
        int(value, 16)
    except ValueError as error:
        raise ManifestError(f"{name} must be hexadecimal") from error
    return value.lower()


def _artifact_path(manifest_path: Path, declared: Any, override: Optional[Path], name: str) -> Path:
    if override is not None:
        return Path(override)
    if not isinstance(declared, str) or not declared:
        raise ManifestError(f"{name} file must be a non-empty path")
    return manifest_path.parent / declared


def _verify_file(path: Path, expected_hash: Any, name: str, size: Optional[Any] = None) -> None:
    if not path.is_file():
        raise ManifestError(f"{name} file does not exist: {path}")
    expected = _require_sha256(expected_hash, f"{name} SHA-256")
    actual = sha256_file(path)
    if actual != expected:
        raise ManifestError(
            f"{name} SHA-256 mismatch: expected {expected}, actual {actual}"
        )
    if size is not None:
        if not isinstance(size, int) or isinstance(size, bool) or size < 0:
            raise ManifestError(f"{name} size_bytes must be a non-negative integer")
        actual_size = path.stat().st_size
        if actual_size != size:
            raise ManifestError(
                f"{name} size mismatch: expected {size}, actual {actual_size}"
            )


def _validate_provenance(provenance: Dict[str, Any]) -> None:
    required = (
        "source_file",
        "source_size_bytes",
        "source_url",
        "source_repository",
        "source_commit",
        "source_sha256",
        "toolkit_version",
        "python_executable",
        "host_os",
    )
    _require_keys(provenance, required, "provenance")
    for key in required:
        if key == "source_size_bytes":
            continue
        if not isinstance(provenance[key], str) or not provenance[key]:
            raise ManifestError(f"provenance.{key} must be a non-empty string")
    if (
        not isinstance(provenance["source_size_bytes"], int)
        or isinstance(provenance["source_size_bytes"], bool)
        or provenance["source_size_bytes"] <= 0
    ):
        raise ManifestError("provenance.source_size_bytes must be a positive integer")
    _require_sha256(provenance["source_sha256"], "provenance.source_sha256")


def _validate_input(input_tensor: Dict[str, Any]) -> None:
    _require_keys(
        input_tensor,
        ("name", "shape", "dtype", "layout", "color_order", "pad_value"),
        "input",
    )
    if input_tensor["shape"] != [1, 640, 640, 3]:
        raise ManifestError("input shape must be [1, 640, 640, 3]")
    if input_tensor["dtype"] != "uint8":
        raise ManifestError("input dtype must be uint8")
    if input_tensor["layout"] != "NHWC":
        raise ManifestError("input layout must be NHWC")
    if input_tensor["color_order"] != "RGB":
        raise ManifestError("input color_order must be RGB")
    if input_tensor["pad_value"] != 114:
        raise ManifestError("input pad_value must be 114")


def _expected_roles(include_score_sum: bool) -> list[str]:
    roles = []
    for stride in (8, 16, 32):
        roles.extend((f"box_s{stride}", f"class_s{stride}"))
        if include_score_sum:
            roles.append(f"score_sum_s{stride}")
    return roles


def _validate_outputs(outputs: Any, class_count: int, dfl_bins: int) -> None:
    if not isinstance(outputs, list) or len(outputs) not in (6, 9):
        raise ManifestError("outputs must declare exactly six or nine tensors")
    roles = []
    for index, value in enumerate(outputs):
        tensor = _require_mapping(value, f"outputs[{index}]")
        _require_keys(tensor, ("name", "role", "shape", "dtype", "layout"), f"outputs[{index}]")
        role = tensor["role"]
        if not isinstance(role, str):
            raise ManifestError(f"outputs[{index}].role must be a string")
        roles.append(role)
        if tensor["layout"] != "NCHW":
            raise ManifestError("output layout must be NCHW")
        if tensor["dtype"] not in ("float32", "int8"):
            raise ManifestError("output dtype must be float32 or int8")

    expected = _expected_roles(len(outputs) == 9)
    if roles != expected:
        raise ManifestError(f"output roles must be exactly: {', '.join(expected)}")

    for tensor in outputs:
        role = tensor["role"]
        stride = int(role.rsplit("s", 1)[1])
        grid = 640 // stride
        if role.startswith("box_"):
            channels = 4 * dfl_bins
        elif role.startswith("class_"):
            channels = class_count
        else:
            channels = 1
        expected_shape = [1, channels, grid, grid]
        if tensor["shape"] != expected_shape:
            raise ManifestError(
                f"{role} shape must be {expected_shape}, got {tensor['shape']}"
            )


def load_and_verify_manifest(
    manifest_path: Path,
    model_path: Optional[Path] = None,
    labels_path: Optional[Path] = None,
) -> Dict[str, Any]:
    manifest_path = Path(manifest_path)
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise ManifestError(f"cannot read manifest JSON {manifest_path}: {error}") from error
    manifest = _require_mapping(manifest, "manifest")
    _require_keys(
        manifest,
        (
            "schema_version",
            "artifact",
            "labels",
            "provenance",
            "conversion",
            "input",
            "outputs",
            "class_count",
            "dfl_bins",
            "calibrated",
            "deployment_status",
        ),
        "manifest",
    )
    if manifest["schema_version"] != 1:
        raise ManifestError("schema_version must be 1")
    if manifest["calibrated"] is not False:
        raise ManifestError("calibrated must be false for bring-up thresholds")
    if manifest["deployment_status"] != "function_only_board_unverified":
        raise ManifestError(
            "deployment_status must be function_only_board_unverified"
        )

    artifact = _require_mapping(manifest["artifact"], "artifact")
    labels = _require_mapping(manifest["labels"], "labels")
    provenance = _require_mapping(manifest["provenance"], "provenance")
    conversion = _require_mapping(manifest["conversion"], "conversion")
    input_tensor = _require_mapping(manifest["input"], "input")
    _require_keys(artifact, ("file", "size_bytes", "sha256"), "artifact")
    _require_keys(labels, ("file", "count", "sha256"), "labels")
    _require_keys(
        conversion,
        ("target_platform", "quantized", "mean_values", "std_values"),
        "conversion",
    )

    _validate_provenance(provenance)
    if conversion["target_platform"] != "rk3566":
        raise ManifestError("conversion.target_platform must be rk3566")
    if conversion["quantized"] is not False:
        raise ManifestError("conversion.quantized must be false")
    if conversion["mean_values"] != [[0.0, 0.0, 0.0]]:
        raise ManifestError("conversion.mean_values must be [[0, 0, 0]]")
    if conversion["std_values"] != [[255.0, 255.0, 255.0]]:
        raise ManifestError("conversion.std_values must be [[255, 255, 255]]")
    _validate_input(input_tensor)

    class_count = manifest["class_count"]
    dfl_bins = manifest["dfl_bins"]
    if not isinstance(class_count, int) or isinstance(class_count, bool) or class_count <= 0:
        raise ManifestError("class_count must be a positive integer")
    if not isinstance(dfl_bins, int) or isinstance(dfl_bins, bool) or dfl_bins <= 1:
        raise ManifestError("dfl_bins must be an integer greater than one")
    if labels["count"] != class_count:
        raise ManifestError("labels count must equal class_count")
    _validate_outputs(manifest["outputs"], class_count, dfl_bins)

    resolved_model = _artifact_path(manifest_path, artifact["file"], model_path, "model")
    resolved_labels = _artifact_path(manifest_path, labels["file"], labels_path, "labels")
    _verify_file(resolved_model, artifact["sha256"], "model", artifact["size_bytes"])
    _verify_file(resolved_labels, labels["sha256"], "labels")
    try:
        label_count = len(
            [line for line in resolved_labels.read_text(encoding="utf-8").splitlines() if line]
        )
    except (OSError, UnicodeError) as error:
        raise ManifestError(f"cannot read labels file {resolved_labels}: {error}") from error
    if label_count != class_count:
        raise ManifestError(
            f"labels file contains {label_count} non-empty lines, expected {class_count}"
        )
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--model", type=Path, help="override manifest-relative model path")
    parser.add_argument("--labels", type=Path, help="override manifest-relative labels path")
    arguments = parser.parse_args()
    try:
        manifest = load_and_verify_manifest(
            arguments.manifest, arguments.model, arguments.labels
        )
    except ManifestError as error:
        parser.error(str(error))
    print(
        "detector manifest verified: "
        f"target={manifest['conversion']['target_platform']} "
        f"outputs={len(manifest['outputs'])} "
        f"classes={manifest['class_count']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
