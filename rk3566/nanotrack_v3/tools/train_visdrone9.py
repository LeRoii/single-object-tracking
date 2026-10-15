#!/usr/bin/env python3
"""Run reproducible smoke or full VisDrone9 training with Rockchip YOLOv8."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any, Literal


ROCKCHIP_REPOSITORY = "https://github.com/airockchip/ultralytics_yolov8"
ROCKCHIP_COMMIT = "4674fe6e003dfbc5f2250d3b39dd31faaf7a9877"
CLASS_NAMES = (
    "person",
    "bicycle",
    "car",
    "van",
    "truck",
    "tricycle",
    "awning-tricycle",
    "bus",
    "motor",
)


@dataclass(frozen=True)
class RunIdentity:
    mode: str
    repo_commit: str
    dataset_audit_sha256: str
    data_yaml_sha256: str
    initial_weights_sha256: str
    class_names: tuple[str, ...]
    seed: int

    def to_dict(self) -> dict[str, Any]:
        value = asdict(self)
        value["class_names"] = list(self.class_names)
        return value

    @classmethod
    def from_dict(cls, value: dict[str, Any]) -> "RunIdentity":
        required = {
            "mode",
            "repo_commit",
            "dataset_audit_sha256",
            "data_yaml_sha256",
            "initial_weights_sha256",
            "class_names",
            "seed",
        }
        missing = required.difference(value)
        if missing:
            raise ValueError(
                "run identity is missing: " + ", ".join(sorted(missing))
            )
        return cls(
            mode=str(value["mode"]),
            repo_commit=str(value["repo_commit"]),
            dataset_audit_sha256=str(value["dataset_audit_sha256"]),
            data_yaml_sha256=str(value["data_yaml_sha256"]),
            initial_weights_sha256=str(value["initial_weights_sha256"]),
            class_names=tuple(str(name) for name in value["class_names"]),
            seed=int(value["seed"]),
        )


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def build_train_kwargs(
    mode: Literal["smoke", "full"],
    data: Path,
    project: Path,
    seed: int = 42,
) -> dict[str, object]:
    if mode not in ("smoke", "full"):
        raise ValueError("mode must be smoke or full")
    return {
        "data": str(Path(data).resolve()),
        "epochs": 5 if mode == "smoke" else 100,
        "batch": 8,
        "imgsz": 640,
        "patience": 20,
        "seed": seed,
        "amp": True,
        "device": 0,
        "deterministic": True,
        "workers": 4,
        "project": str(Path(project).resolve()),
        "name": f"visdrone9-{mode}",
        "exist_ok": True,
        "save": True,
        "plots": True,
    }


def probe_cuda(torch_module: Any) -> dict[str, Any]:
    if not torch_module.cuda.is_available() or torch_module.cuda.device_count() < 1:
        raise RuntimeError("CUDA is not available to PyTorch")
    try:
        tensor = torch_module.tensor([1.0], device="cuda:0")
        result = (tensor * 2.0).item()
        torch_module.cuda.synchronize(0)
    except Exception as error:
        raise RuntimeError(f"CUDA tensor probe failed: {error}") from error
    if result != 2.0:
        raise RuntimeError(f"CUDA tensor probe returned {result}, expected 2.0")
    return {
        "torch_version": str(torch_module.__version__),
        "cuda_runtime": str(torch_module.version.cuda),
        "cudnn_version": torch_module.backends.cudnn.version(),
        "device_count": int(torch_module.cuda.device_count()),
        "device_name": str(torch_module.cuda.get_device_name(0)),
        "tensor_result": float(result),
    }


def validate_resume_metadata(checkpoint: Path, expected: RunIdentity) -> None:
    checkpoint = Path(checkpoint)
    if not checkpoint.is_file():
        raise ValueError(f"resume checkpoint does not exist: {checkpoint}")
    identity_path = checkpoint.parent.parent / "run-identity.json"
    try:
        value = json.loads(identity_path.read_text(encoding="utf-8"))
        actual = RunIdentity.from_dict(value)
    except (OSError, UnicodeError, json.JSONDecodeError, TypeError) as error:
        raise ValueError(f"cannot read resume identity {identity_path}: {error}") from error
    for field in RunIdentity.__dataclass_fields__:
        if getattr(actual, field) != getattr(expected, field):
            raise ValueError(
                f"resume identity mismatch for {field}: "
                f"{getattr(actual, field)!r} != {getattr(expected, field)!r}"
            )


def _write_json_atomic(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(
        json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    temporary.replace(path)


def _repository_commit(repository: Path) -> str:
    if not (repository / "ultralytics" / "__init__.py").is_file():
        raise ValueError(f"Rockchip YOLOv8 checkout is incomplete: {repository}")
    result = subprocess.run(
        ["git", "-C", str(repository), "rev-parse", "HEAD"],
        text=True,
        capture_output=True,
        check=False,
    )
    if result.returncode != 0:
        raise ValueError(f"cannot read Rockchip checkout commit: {result.stderr.strip()}")
    commit = result.stdout.strip()
    if commit != ROCKCHIP_COMMIT:
        raise ValueError(
            f"Rockchip checkout commit {commit} does not match {ROCKCHIP_COMMIT}"
        )
    return commit


def _load_dataset_audit(path: Path) -> dict[str, Any]:
    try:
        audit = json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise ValueError(f"cannot read dataset audit {path}: {error}") from error
    if audit.get("status") != "pass" or audit.get("class_names") != list(CLASS_NAMES):
        raise ValueError("dataset audit is not a passing VisDrone9 audit")
    if audit.get("totals", {}).get("images") != 7019:
        raise ValueError("dataset audit image count must be 7019")
    return audit


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", required=True, choices=("smoke", "full"))
    parser.add_argument("--repo", required=True, type=Path)
    parser.add_argument("--weights", required=True, type=Path)
    parser.add_argument("--data", required=True, type=Path)
    parser.add_argument("--dataset-audit", required=True, type=Path)
    parser.add_argument("--project", required=True, type=Path)
    parser.add_argument("--resume", type=Path)
    parser.add_argument("--seed", type=int, default=42)
    return parser


def main() -> int:
    arguments = _parser().parse_args()
    try:
        for path, description in (
            (arguments.weights, "initial weights"),
            (arguments.data, "data YAML"),
            (arguments.dataset_audit, "dataset audit"),
        ):
            if not path.is_file():
                raise ValueError(f"{description} does not exist: {path}")
        repo_commit = _repository_commit(arguments.repo)
        _load_dataset_audit(arguments.dataset_audit)
        identity = RunIdentity(
            mode=arguments.mode,
            repo_commit=repo_commit,
            dataset_audit_sha256=sha256_file(arguments.dataset_audit),
            data_yaml_sha256=sha256_file(arguments.data),
            initial_weights_sha256=sha256_file(arguments.weights),
            class_names=CLASS_NAMES,
            seed=arguments.seed,
        )
        kwargs = build_train_kwargs(
            arguments.mode, arguments.data, arguments.project, arguments.seed
        )
        run_dir = Path(str(kwargs["project"])) / str(kwargs["name"])
        if arguments.resume is not None:
            validate_resume_metadata(arguments.resume, identity)
        elif run_dir.exists():
            raise ValueError(
                f"training run directory already exists; use --resume or move it: {run_dir}"
            )

        # Delayed imports keep --help and unit tests independent of the training stack.
        import torch

        cuda_report = probe_cuda(torch)
        if arguments.resume is None:
            _write_json_atomic(run_dir / "run-identity.json", identity.to_dict())
            _write_json_atomic(run_dir / "cuda-environment.json", cuda_report)

        repository = str(arguments.repo.resolve())
        if repository not in sys.path:
            sys.path.insert(0, repository)
        from ultralytics import YOLO

        if arguments.resume is not None:
            model = YOLO(str(arguments.resume.resolve()))
            model.train(resume=True)
        else:
            model = YOLO(str(arguments.weights.resolve()))
            model.train(**kwargs)
    except (OSError, RuntimeError, ValueError) as error:
        print(f"training setup failed: {error}", file=sys.stderr)
        return 2
    print(f"VisDrone9 {arguments.mode} training completed: {run_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
