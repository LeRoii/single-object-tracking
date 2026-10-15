#!/usr/bin/env python3
"""Convert Rockchip's optimized YOLOv8 ONNX model for RK3566."""

from __future__ import annotations

import argparse
import json
import platform
import sys
from importlib.metadata import PackageNotFoundError, version
from pathlib import Path
from typing import Any, Dict

from verify_detector_manifest import (
    ManifestError,
    load_and_verify_manifest,
    sha256_file,
)


PINNED_TOOLKIT_VERSION = "2.3.2"


def _label_names(path: Path) -> list[str]:
    try:
        names = [line for line in path.read_text(encoding="utf-8").splitlines() if line]
    except (OSError, UnicodeError) as error:
        raise ValueError(f"cannot read labels file {path}: {error}") from error
    if not names:
        raise ValueError("labels file must contain at least one non-empty line")
    return names


def _output_tensor(role: str, channels: int, grid: int) -> Dict[str, Any]:
    return {
        "name": role,
        "role": role,
        "shape": [1, channels, grid, grid],
        "dtype": "float32",
        "layout": "NCHW",
    }


def _training_artifact(path: Path) -> Dict[str, Any]:
    path = Path(path)
    if not path.is_file():
        raise ValueError(f"training provenance file does not exist: {path}")
    return {
        "file": path.name,
        "size_bytes": path.stat().st_size,
        "sha256": sha256_file(path),
    }


def build_detector_manifest(
    *,
    onnx_path: Path,
    rknn_path: Path,
    labels_path: Path,
    source_url: str,
    source_repository: str,
    source_commit: str,
    toolkit_version: str,
    python_executable: str,
    host_os: str,
    output_count: int,
    training_checkpoint: Path | None = None,
    training_repository: str | None = None,
    training_commit: str | None = None,
    training_config: Path | None = None,
    dataset_audit: Path | None = None,
) -> Dict[str, Any]:
    onnx_path = Path(onnx_path)
    rknn_path = Path(rknn_path)
    labels_path = Path(labels_path)
    labels = _label_names(labels_path)
    if output_count not in (6, 9):
        raise ValueError("output_count must be 6 or 9")
    outputs = []
    for stride, grid in ((8, 80), (16, 40), (32, 20)):
        outputs.extend(
            (
                _output_tensor(f"box_s{stride}", 64, grid),
                _output_tensor(f"class_s{stride}", len(labels), grid),
            )
        )
        if output_count == 9:
            outputs.append(_output_tensor(f"score_sum_s{stride}", 1, grid))
    training_values = (
        training_checkpoint,
        training_repository,
        training_commit,
        training_config,
        dataset_audit,
    )
    if any(value is not None for value in training_values) and not all(
        value is not None for value in training_values
    ):
        raise ValueError("training provenance arguments must be provided together")
    if training_commit is not None and len(training_commit) != 40:
        raise ValueError("training commit must contain the full 40-character commit")

    manifest = {
        "schema_version": 1,
        "artifact": {
            "file": rknn_path.name,
            "size_bytes": rknn_path.stat().st_size,
            "sha256": sha256_file(rknn_path),
        },
        "labels": {
            "file": labels_path.name,
            "count": len(labels),
            "sha256": sha256_file(labels_path),
        },
        "provenance": {
            "source_file": onnx_path.name,
            "source_size_bytes": onnx_path.stat().st_size,
            "source_url": source_url,
            "source_repository": source_repository,
            "source_commit": source_commit,
            "source_sha256": sha256_file(onnx_path),
            "toolkit_version": toolkit_version,
            "python_executable": python_executable,
            "host_os": host_os,
        },
        "conversion": {
            "target_platform": "rk3566",
            "quantized": False,
            "mean_values": [[0.0, 0.0, 0.0]],
            "std_values": [[255.0, 255.0, 255.0]],
        },
        "input": {
            "name": "images",
            "shape": [1, 640, 640, 3],
            "dtype": "uint8",
            "layout": "NHWC",
            "color_order": "RGB",
            "pad_value": 114,
        },
        "outputs": outputs,
        "class_count": len(labels),
        "dfl_bins": 16,
        "calibrated": False,
        "deployment_status": "function_only_board_unverified",
    }
    if training_checkpoint is not None:
        manifest["training"] = {
            "checkpoint": _training_artifact(training_checkpoint),
            "repository": {
                "url": training_repository,
                "commit": training_commit,
            },
            "config": _training_artifact(training_config),
            "dataset_audit": _training_artifact(dataset_audit),
        }
    return manifest


def _check_rknn_result(stage: str, result: int) -> None:
    if result != 0:
        raise RuntimeError(f"RKNN {stage} failed with code {result}")


def _convert(arguments: argparse.Namespace) -> None:
    for path, name in ((arguments.onnx, "ONNX"), (arguments.labels, "labels")):
        if not path.is_file():
            raise ValueError(f"{name} file does not exist: {path}")
    training_values = (
        arguments.training_checkpoint,
        arguments.training_repository,
        arguments.training_commit,
        arguments.training_config,
        arguments.dataset_audit,
    )
    if any(value is not None for value in training_values) and not all(
        value is not None for value in training_values
    ):
        raise ValueError("training provenance arguments must be provided together")
    for path, name in (
        (arguments.training_checkpoint, "training checkpoint"),
        (arguments.training_config, "training config"),
        (arguments.dataset_audit, "dataset audit"),
    ):
        if path is not None and not path.is_file():
            raise ValueError(f"{name} file does not exist: {path}")
    if arguments.output.parent != arguments.manifest.parent or arguments.labels.parent != arguments.manifest.parent:
        raise ValueError("output, labels, and manifest must share one directory")
    if len(arguments.source_commit) != 40:
        raise ValueError("--source-commit must contain the full 40-character commit")

    try:
        installed_version = version("rknn-toolkit2")
    except PackageNotFoundError as error:
        raise RuntimeError("rknn-toolkit2 is not installed in this Python environment") from error
    if installed_version != PINNED_TOOLKIT_VERSION:
        raise RuntimeError(
            f"rknn-toolkit2 {PINNED_TOOLKIT_VERSION} is required, got {installed_version}"
        )

    # Deliberately delayed: --help and --verify-manifest do not require Toolkit2.
    from rknn.api import RKNN

    rknn = RKNN(verbose=True)
    try:
        _check_rknn_result(
            "config",
            rknn.config(
                mean_values=[[0.0, 0.0, 0.0]],
                std_values=[[255.0, 255.0, 255.0]],
                target_platform="rk3566",
            ),
        )
        _check_rknn_result("load_onnx", rknn.load_onnx(model=str(arguments.onnx)))
        _check_rknn_result("build", rknn.build(do_quantization=False))
        _check_rknn_result("export_rknn", rknn.export_rknn(str(arguments.output)))
    finally:
        rknn.release()

    manifest = build_detector_manifest(
        onnx_path=arguments.onnx,
        rknn_path=arguments.output,
        labels_path=arguments.labels,
        source_url=arguments.source_url,
        source_repository=arguments.source_repository,
        source_commit=arguments.source_commit,
        toolkit_version=installed_version,
        python_executable=sys.executable,
        host_os=platform.platform(),
        output_count=arguments.output_count,
        training_checkpoint=arguments.training_checkpoint,
        training_repository=arguments.training_repository,
        training_commit=arguments.training_commit,
        training_config=arguments.training_config,
        dataset_audit=arguments.dataset_audit,
    )
    arguments.manifest.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    load_and_verify_manifest(arguments.manifest)


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--verify-manifest",
        type=Path,
        help="validate an existing manifest and exit without importing RKNN",
    )
    parser.add_argument("--onnx", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--labels", type=Path)
    parser.add_argument("--source-url")
    parser.add_argument("--source-repository")
    parser.add_argument("--source-commit")
    parser.add_argument("--output-count", type=int, choices=(6, 9), default=9)
    parser.add_argument("--training-checkpoint", type=Path)
    parser.add_argument("--training-repository")
    parser.add_argument("--training-commit")
    parser.add_argument("--training-config", type=Path)
    parser.add_argument("--dataset-audit", type=Path)
    return parser


def main() -> int:
    parser = _parser()
    arguments = parser.parse_args()
    try:
        if arguments.verify_manifest is not None:
            load_and_verify_manifest(arguments.verify_manifest)
            print(f"detector manifest verified: {arguments.verify_manifest}")
            return 0
        required = (
            "onnx",
            "output",
            "manifest",
            "labels",
            "source_url",
            "source_repository",
            "source_commit",
        )
        missing = [f"--{name.replace('_', '-')}" for name in required if not getattr(arguments, name)]
        if missing:
            parser.error("conversion requires " + ", ".join(missing))
        _convert(arguments)
    except (ManifestError, OSError, RuntimeError, ValueError) as error:
        parser.error(str(error))
    print(f"RK3566 detector converted: {arguments.output}")
    print(f"detector manifest: {arguments.manifest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
