#!/usr/bin/env python3
"""Convert the three freshly exported NanoTrack V3 ONNX graphs to RKNN."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from rknn.api import RKNN

from model_manifest import build_manifest, sha256_file, write_manifest


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--model-dir", type=Path, required=True)
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--weight-sha256", required=True)
    parser.add_argument("--target", default="rk3566", choices=["rk3566"])
    return parser.parse_args()


def checked(step: str, status: int) -> None:
    if status != 0:
        raise RuntimeError(f"RKNN {step} failed with status {status}")


def convert_one(onnx_path: Path, rknn_path: Path) -> None:
    rknn = RKNN(verbose=True)
    try:
        checked("config", rknn.config(target_platform="rk3566"))
        checked("load_onnx", rknn.load_onnx(model=str(onnx_path)))
        checked("build", rknn.build(do_quantization=False))
        checked("export_rknn", rknn.export_rknn(str(rknn_path)))
    finally:
        rknn.release()


def runtime_model(file_name: str, sha256: str, inputs: list[dict], outputs: list[dict]) -> dict:
    return {"file": file_name, "sha256": sha256, "inputs": inputs, "outputs": outputs}


def tensor(name: str, shape: list[int], dtype: str, layout: str) -> dict:
    return {"name": name, "shape": shape, "dtype": dtype, "layout": layout}


def nchw_shape_to_nhwc(shape: list[int]) -> list[int]:
    if len(shape) != 4:
        raise ValueError(f"expected a four-dimensional NCHW shape, got {shape}")
    return [shape[0], shape[2], shape[3], shape[1]]


def main() -> int:
    args = parse_args()
    model_dir = args.model_dir.resolve()
    export_meta = json.loads((model_dir / "export_metadata.json").read_text(encoding="utf-8"))
    graph_meta = export_meta["graphs"]

    output_names = {
        "template_backbone": "template_backbone.rknn",
        "search_backbone": "search_backbone.rknn",
        "head": "head.rknn",
    }
    for name, output_name in output_names.items():
        convert_one(model_dir / graph_meta[name]["file"], model_dir / output_name)

    template_shape = graph_meta["template_backbone"]["outputs"][0]["shape"]
    search_shape = graph_meta["search_backbone"]["outputs"][0]["shape"]
    cls_shape = graph_meta["head"]["outputs"][0]["shape"]
    loc_shape = graph_meta["head"]["outputs"][1]["shape"]

    template = runtime_model(
        output_names["template_backbone"],
        sha256_file(model_dir / output_names["template_backbone"]),
        [tensor("image", [1, 127, 127, 3], "uint8", "NHWC")],
        [tensor("feature", template_shape, "float32", "NCHW")],
    )
    search = runtime_model(
        output_names["search_backbone"],
        sha256_file(model_dir / output_names["search_backbone"]),
        [tensor("image", [1, 255, 255, 3], "uint8", "NHWC")],
        [tensor("feature", search_shape, "float32", "NCHW")],
    )
    head = runtime_model(
        output_names["head"],
        sha256_file(model_dir / output_names["head"]),
        [
            tensor("template_feature", nchw_shape_to_nhwc(template_shape), "float32", "NHWC"),
            tensor("search_feature", nchw_shape_to_nhwc(search_shape), "float32", "NHWC"),
        ],
        [tensor("cls", cls_shape, "float32", "NCHW"), tensor("loc", loc_shape, "float32", "NCHW")],
    )
    provenance = {
        "source_commit": args.source_commit,
        "weight_sha256": args.weight_sha256,
        "toolkit_version": "2.3.2",
        "target_platform": args.target,
        "quantized": False,
    }
    manifest = build_manifest(template, search, head, provenance)
    write_manifest(model_dir / "manifest.json", manifest)
    print(json.dumps(manifest, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
