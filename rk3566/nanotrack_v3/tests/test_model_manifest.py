import sys
from pathlib import Path

import pytest


TOOLS_DIR = Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS_DIR))

from model_manifest import build_manifest, sha256_file  # noqa: E402


def tensor(name, shape, dtype="float32", layout="NCHW"):
    return {"name": name, "shape": shape, "dtype": dtype, "layout": layout}


def valid_metadata():
    template = {
        "file": "template_backbone.rknn",
        "sha256": "1" * 64,
        "inputs": [tensor("input", [1, 127, 127, 3], "uint8", "NHWC")],
        "outputs": [tensor("template_feature", [1, 96, 8, 8])],
    }
    search = {
        "file": "search_backbone.rknn",
        "sha256": "2" * 64,
        "inputs": [tensor("input", [1, 255, 255, 3], "uint8", "NHWC")],
        "outputs": [tensor("search_feature", [1, 96, 16, 16])],
    }
    head = {
        "file": "head.rknn",
        "sha256": "3" * 64,
        "inputs": [
            tensor("template_feature", [1, 8, 8, 96], "float32", "NHWC"),
            tensor("search_feature", [1, 16, 16, 96], "float32", "NHWC"),
        ],
        "outputs": [
            tensor("cls", [1, 2, 16, 16]),
            tensor("loc", [1, 4, 16, 16]),
        ],
    }
    return template, search, head


def test_build_manifest_records_exact_provenance_and_interfaces():
    template, search, head = valid_metadata()
    provenance = {
        "source_commit": "248663fde6bf7c40190cf10ee396d5662919ecd3",
        "weight_sha256": "4f0d25108385a67bdec5fa01ca37c74acb72343b53996dd645fe18b86e512e69",
        "toolkit_version": "2.3.2",
        "target_platform": "rk3566",
    }

    manifest = build_manifest(template, search, head, provenance)

    assert manifest["schema_version"] == 1
    assert manifest["provenance"] == provenance
    assert manifest["models"]["template_backbone"] == template
    assert manifest["models"]["search_backbone"] == search
    assert manifest["models"]["head"] == head


def test_build_manifest_rejects_head_template_shape_mismatch():
    template, search, head = valid_metadata()
    head["inputs"][0]["shape"] = [1, 8, 8, 48]

    with pytest.raises(ValueError, match="template feature shape"):
        build_manifest(
            template,
            search,
            head,
            {
                "source_commit": "a" * 40,
                "weight_sha256": "b" * 64,
                "toolkit_version": "2.3.2",
                "target_platform": "rk3566",
            },
        )


def test_build_manifest_rejects_non_rk3566_target():
    template, search, head = valid_metadata()

    with pytest.raises(ValueError, match="target_platform"):
        build_manifest(
            template,
            search,
            head,
            {
                "source_commit": "a" * 40,
                "weight_sha256": "b" * 64,
                "toolkit_version": "2.3.2",
                "target_platform": "rk3588",
            },
        )


def test_sha256_file_hashes_file_bytes(tmp_path):
    fixture = tmp_path / "fixture.bin"
    fixture.write_bytes(b"nanotrack-v3\n")

    assert sha256_file(fixture) == "ba9187e8cf1328293bc33715bf1e13281b088c0386b4a5ee0fc5802f6f9092ff"
