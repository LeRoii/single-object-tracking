import hashlib
import json
import subprocess
import sys
from pathlib import Path

import pytest


TOOLS_DIR = Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS_DIR))

from verify_detector_manifest import ManifestError, load_and_verify_manifest  # noqa: E402
from convert_yolov8_rk3566 import build_detector_manifest  # noqa: E402


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def output(role, channels, grid):
    return {
        "name": role,
        "role": role,
        "shape": [1, channels, grid, grid],
        "dtype": "int8",
        "layout": "NCHW",
    }


def valid_manifest(model_bytes=b"rk3566 detector", labels_bytes=b"person\ncar\n"):
    outputs = []
    for stride, grid in ((8, 80), (16, 40), (32, 20)):
        outputs.extend(
            [
                output(f"box_s{stride}", 64, grid),
                output(f"class_s{stride}", 2, grid),
                output(f"score_sum_s{stride}", 1, grid),
            ]
        )
    return {
        "schema_version": 1,
        "artifact": {
            "file": "yolov8n-rk3566-fp16.rknn",
            "size_bytes": len(model_bytes),
            "sha256": sha256(model_bytes),
        },
        "labels": {
            "file": "coco80.txt",
            "count": 2,
            "sha256": sha256(labels_bytes),
        },
        "provenance": {
            "source_file": "yolov8n.onnx",
            "source_size_bytes": len(model_bytes),
            "source_url": "https://example.invalid/yolov8n.onnx",
            "source_repository": "https://github.com/airockchip/rknn_model_zoo",
            "source_commit": "a" * 40,
            "source_sha256": "b" * 64,
            "toolkit_version": "2.3.2",
            "python_executable": "/opt/rknn/bin/python",
            "host_os": "Ubuntu 22.04",
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
        "class_count": 2,
        "dfl_bins": 16,
        "calibrated": False,
        "deployment_status": "function_only_board_unverified",
    }


def write_fixture(tmp_path, manifest=None):
    model_bytes = b"rk3566 detector"
    labels_bytes = b"person\ncar\n"
    (tmp_path / "yolov8n-rk3566-fp16.rknn").write_bytes(model_bytes)
    (tmp_path / "coco80.txt").write_bytes(labels_bytes)
    data = valid_manifest(model_bytes, labels_bytes) if manifest is None else manifest
    path = tmp_path / "detector-manifest.json"
    path.write_text(json.dumps(data), encoding="utf-8")
    return path


def test_valid_nine_output_manifest_verifies_files_and_contract(tmp_path):
    manifest = load_and_verify_manifest(write_fixture(tmp_path))

    assert manifest["conversion"]["target_platform"] == "rk3566"
    assert manifest["conversion"]["quantized"] is False
    assert manifest["input"] == {
        "name": "images",
        "shape": [1, 640, 640, 3],
        "dtype": "uint8",
        "layout": "NHWC",
        "color_order": "RGB",
        "pad_value": 114,
    }
    assert manifest["calibrated"] is False
    assert len(manifest["outputs"]) == 9


def test_six_output_manifest_without_score_sum_is_supported(tmp_path):
    manifest = valid_manifest()
    manifest["outputs"] = [
        tensor for tensor in manifest["outputs"] if not tensor["role"].startswith("score_sum")
    ]

    loaded = load_and_verify_manifest(write_fixture(tmp_path, manifest))

    assert [tensor["role"] for tensor in loaded["outputs"]] == [
        "box_s8",
        "class_s8",
        "box_s16",
        "class_s16",
        "box_s32",
        "class_s32",
    ]


@pytest.mark.parametrize(
    ("field", "value", "message"),
    [
        (("conversion", "target_platform"), "rk3588", "target_platform"),
        (("conversion", "quantized"), True, "quantized"),
        (("input", "color_order"), "BGR", "color_order"),
        (("input", "shape"), [1, 384, 640, 3], "shape"),
        (("input", "pad_value"), 0, "pad_value"),
        (("input", "layout"), "NCHW", "input layout"),
        (("calibrated",), True, "calibrated"),
    ],
)
def test_fixed_rk3566_conversion_contract_is_enforced(tmp_path, field, value, message):
    manifest = valid_manifest()
    target = manifest
    for name in field[:-1]:
        target = target[name]
    target[field[-1]] = value

    with pytest.raises(ManifestError, match=message):
        load_and_verify_manifest(write_fixture(tmp_path, manifest))


def test_required_top_level_key_is_enforced(tmp_path):
    manifest = valid_manifest()
    del manifest["provenance"]

    with pytest.raises(ManifestError, match="provenance"):
        load_and_verify_manifest(write_fixture(tmp_path, manifest))


def test_function_only_deployment_status_is_required(tmp_path):
    manifest = valid_manifest()
    del manifest["deployment_status"]

    with pytest.raises(ManifestError, match="deployment_status"):
        load_and_verify_manifest(write_fixture(tmp_path, manifest))


def test_onnx_file_identity_is_required(tmp_path):
    manifest = valid_manifest()
    del manifest["provenance"]["source_file"]

    with pytest.raises(ManifestError, match="source_file"):
        load_and_verify_manifest(write_fixture(tmp_path, manifest))


def test_model_hash_mismatch_is_rejected(tmp_path):
    path = write_fixture(tmp_path)
    manifest = json.loads(path.read_text(encoding="utf-8"))
    manifest["artifact"]["sha256"] = "0" * 64
    path.write_text(json.dumps(manifest), encoding="utf-8")

    with pytest.raises(ManifestError, match="model SHA-256"):
        load_and_verify_manifest(path)


def test_missing_model_is_rejected(tmp_path):
    path = write_fixture(tmp_path)
    (tmp_path / "yolov8n-rk3566-fp16.rknn").unlink()

    with pytest.raises(ManifestError, match="model file"):
        load_and_verify_manifest(path)


@pytest.mark.parametrize("case", ["wrong_hash", "missing_file"])
def test_labels_identity_is_enforced(tmp_path, case):
    path = write_fixture(tmp_path)
    if case == "wrong_hash":
        manifest = json.loads(path.read_text(encoding="utf-8"))
        manifest["labels"]["sha256"] = "0" * 64
        path.write_text(json.dumps(manifest), encoding="utf-8")
    else:
        (tmp_path / "coco80.txt").unlink()

    with pytest.raises(ManifestError, match="labels"):
        load_and_verify_manifest(path)


def test_unsupported_output_layout_is_rejected(tmp_path):
    manifest = valid_manifest()
    manifest["outputs"][0]["layout"] = "NHWC"

    with pytest.raises(ManifestError, match="output layout"):
        load_and_verify_manifest(write_fixture(tmp_path, manifest))


def test_output_roles_must_be_exact_six_or_nine_tensor_contract(tmp_path):
    manifest = valid_manifest()
    manifest["outputs"][0]["role"] = "unknown"

    with pytest.raises(ManifestError, match="output roles"):
        load_and_verify_manifest(write_fixture(tmp_path, manifest))


def test_malformed_json_is_rejected(tmp_path):
    path = tmp_path / "detector-manifest.json"
    path.write_text("{not-json", encoding="utf-8")

    with pytest.raises(ManifestError, match="JSON"):
        load_and_verify_manifest(path)


def test_conversion_manifest_records_reproducible_inputs(tmp_path):
    onnx = tmp_path / "yolov8n.onnx"
    model = tmp_path / "yolov8n-rk3566-fp16.rknn"
    labels = tmp_path / "coco80.txt"
    onnx.write_bytes(b"official onnx")
    model.write_bytes(b"converted rknn")
    labels.write_bytes(b"person\ncar\n")

    manifest = build_detector_manifest(
        onnx_path=onnx,
        rknn_path=model,
        labels_path=labels,
        source_url="https://github.com/airockchip/rknn_model_zoo/raw/a/model.onnx",
        source_repository="https://github.com/airockchip/rknn_model_zoo",
        source_commit="a" * 40,
        toolkit_version="2.3.2",
        python_executable="/opt/rknn/bin/python",
        host_os="Ubuntu 22.04",
        output_count=9,
    )

    assert manifest["artifact"]["sha256"] == sha256(b"converted rknn")
    assert manifest["provenance"]["source_sha256"] == sha256(b"official onnx")
    assert manifest["provenance"]["source_file"] == "yolov8n.onnx"
    assert manifest["provenance"]["source_size_bytes"] == len(b"official onnx")
    assert manifest["provenance"]["toolkit_version"] == "2.3.2"
    assert manifest["conversion"] == {
        "target_platform": "rk3566",
        "quantized": False,
        "mean_values": [[0.0, 0.0, 0.0]],
        "std_values": [[255.0, 255.0, 255.0]],
    }
    assert len(manifest["outputs"]) == 9
    assert manifest["calibrated"] is False
    assert manifest["deployment_status"] == "function_only_board_unverified"


def test_conversion_help_and_manifest_check_do_not_import_rknn(tmp_path):
    script = TOOLS_DIR / "convert_yolov8_rk3566.py"
    help_result = subprocess.run(
        [sys.executable, "-S", str(script), "--help"],
        text=True,
        capture_output=True,
        check=False,
    )
    assert help_result.returncode == 0, help_result.stderr
    assert "RK3566" in help_result.stdout

    manifest_path = write_fixture(tmp_path)
    verify_result = subprocess.run(
        [sys.executable, "-S", str(script), "--verify-manifest", str(manifest_path)],
        text=True,
        capture_output=True,
        check=False,
    )
    assert verify_result.returncode == 0, verify_result.stderr
    assert "verified" in verify_result.stdout
