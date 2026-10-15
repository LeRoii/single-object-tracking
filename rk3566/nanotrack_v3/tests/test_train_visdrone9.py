import json
import subprocess
import sys
from dataclasses import replace
from pathlib import Path

import pytest


TOOLS_DIR = Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS_DIR))

from train_visdrone9 import (  # noqa: E402
    CLASS_NAMES,
    RunIdentity,
    build_train_kwargs,
    probe_cuda,
    validate_resume_metadata,
)


def test_smoke_and_full_profiles_fix_the_approved_training_contract(tmp_path):
    common = {
        "data": tmp_path / "data.yaml",
        "project": tmp_path / "runs",
        "seed": 42,
    }

    smoke = build_train_kwargs("smoke", **common)
    full = build_train_kwargs("full", **common)

    for profile in (smoke, full):
        assert profile["batch"] == 8
        assert profile["imgsz"] == 640
        assert profile["patience"] == 20
        assert profile["seed"] == 42
        assert profile["amp"] is True
        assert profile["device"] == 0
        assert profile["deterministic"] is True
        assert profile["exist_ok"] is True
    assert smoke["epochs"] == 5
    assert smoke["name"] == "visdrone9-smoke"
    assert full["epochs"] == 100
    assert full["name"] == "visdrone9-full"


def test_unknown_training_mode_is_rejected(tmp_path):
    with pytest.raises(ValueError, match="mode"):
        build_train_kwargs("quick", tmp_path / "data.yaml", tmp_path / "runs")


class FakeTensor:
    def __init__(self, value, fails=False):
        self.value = value
        self.fails = fails

    def __mul__(self, factor):
        if self.fails:
            raise RuntimeError("CUDA kernel failed")
        return FakeTensor(self.value * factor)

    def item(self):
        return self.value


class FakeCuda:
    def __init__(self, available=True, tensor_fails=False):
        self.available = available
        self.tensor_fails = tensor_fails

    def is_available(self):
        return self.available

    def device_count(self):
        return 1 if self.available else 0

    def get_device_name(self, index):
        assert index == 0
        return "NVIDIA Test GPU"

    def synchronize(self, index):
        assert index == 0


class FakeCudnn:
    @staticmethod
    def version():
        return 90100


class FakeBackends:
    cudnn = FakeCudnn()


class FakeVersion:
    cuda = "12.1"


class FakeTorch:
    __version__ = "2.4.0"
    version = FakeVersion()
    backends = FakeBackends()

    def __init__(self, available=True, tensor_fails=False):
        self.cuda = FakeCuda(available, tensor_fails)
        self.tensor_fails = tensor_fails

    def tensor(self, values, device):
        assert values == [1.0]
        assert device == "cuda:0"
        return FakeTensor(values[0], self.tensor_fails)


def test_cuda_probe_rejects_framework_without_visible_gpu():
    with pytest.raises(RuntimeError, match="CUDA is not available"):
        probe_cuda(FakeTorch(available=False))


def test_cuda_probe_rejects_failed_tensor_execution():
    with pytest.raises(RuntimeError, match="CUDA tensor probe failed"):
        probe_cuda(FakeTorch(tensor_fails=True))


def test_cuda_probe_reports_versions_after_real_tensor_operation():
    report = probe_cuda(FakeTorch())

    assert report == {
        "torch_version": "2.4.0",
        "cuda_runtime": "12.1",
        "cudnn_version": 90100,
        "device_count": 1,
        "device_name": "NVIDIA Test GPU",
        "tensor_result": 2.0,
    }


def identity() -> RunIdentity:
    return RunIdentity(
        mode="full",
        repo_commit="4" * 40,
        dataset_audit_sha256="a" * 64,
        data_yaml_sha256="b" * 64,
        initial_weights_sha256="c" * 64,
        class_names=CLASS_NAMES,
        seed=42,
    )


def write_resume_fixture(tmp_path, value):
    run_dir = tmp_path / "run"
    checkpoint = run_dir / "weights" / "last.pt"
    checkpoint.parent.mkdir(parents=True)
    checkpoint.write_bytes(b"checkpoint")
    (run_dir / "run-identity.json").write_text(
        json.dumps(value.to_dict()), encoding="utf-8"
    )
    return checkpoint


def test_resume_identity_accepts_exact_match(tmp_path):
    expected = identity()
    checkpoint = write_resume_fixture(tmp_path, expected)

    validate_resume_metadata(checkpoint, expected)


@pytest.mark.parametrize(
    ("field", "value"),
    [
        ("mode", "smoke"),
        ("repo_commit", "5" * 40),
        ("dataset_audit_sha256", "d" * 64),
        ("data_yaml_sha256", "e" * 64),
        ("initial_weights_sha256", "f" * 64),
        ("class_names", ("person", "car")),
        ("seed", 7),
    ],
)
def test_resume_identity_rejects_every_training_input_mismatch(
    tmp_path, field, value
):
    expected = identity()
    checkpoint = write_resume_fixture(tmp_path, replace(expected, **{field: value}))

    with pytest.raises(ValueError, match=field):
        validate_resume_metadata(checkpoint, expected)


def test_help_does_not_import_training_dependencies():
    script = TOOLS_DIR / "train_visdrone9.py"
    result = subprocess.run(
        [sys.executable, "-S", str(script), "--help"],
        text=True,
        capture_output=True,
        check=False,
    )

    assert result.returncode == 0, result.stderr
    assert "--mode" in result.stdout
    assert "--dataset-audit" in result.stdout
