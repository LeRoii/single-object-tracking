#!/usr/bin/env python3
"""Export the author's NanoTrack V3 into three fixed-shape ONNX graphs."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

import numpy as np
import onnx
import onnxruntime as ort
import torch

from model_manifest import sha256_file


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--weight", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--seed", type=int, default=20260929)
    parser.add_argument("--opset", type=int, default=14)
    parser.add_argument("--atol", type=float, default=1e-4)
    parser.add_argument("--rtol", type=float, default=1e-4)
    return parser.parse_args()


def tensor_meta(value_info: onnx.ValueInfoProto) -> dict:
    tensor_type = value_info.type.tensor_type
    dims = []
    for dim in tensor_type.shape.dim:
        dims.append(dim.dim_value if dim.HasField("dim_value") else dim.dim_param)
    dtype = onnx.TensorProto.DataType.Name(tensor_type.elem_type).lower()
    return {"name": value_info.name, "shape": dims, "dtype": dtype, "layout": "NCHW"}


def graph_meta(path: Path) -> dict:
    graph = onnx.load(str(path)).graph
    initializer_names = {item.name for item in graph.initializer}
    return {
        "file": path.name,
        "sha256": sha256_file(path),
        "inputs": [tensor_meta(item) for item in graph.input if item.name not in initializer_names],
        "outputs": [tensor_meta(item) for item in graph.output],
    }


def ort_run(path: Path, feeds: dict[str, np.ndarray]) -> list[np.ndarray]:
    session = ort.InferenceSession(str(path), providers=["CPUExecutionProvider"])
    return session.run(None, feeds)


def assert_close(name: str, actual: np.ndarray, expected: np.ndarray, atol: float, rtol: float) -> float:
    np.testing.assert_allclose(actual, expected, atol=atol, rtol=rtol)
    error = np.max(np.abs(actual - expected))
    return float(error)


def main() -> int:
    args = parse_args()
    repo = args.repo.resolve()
    config = args.config.resolve()
    weight = args.weight.resolve()
    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    source_commit = subprocess.check_output(
        ["git", "-C", str(repo), "rev-parse", "HEAD"], text=True
    ).strip()
    if not config.is_file() or not weight.is_file():
        raise FileNotFoundError("config and weight must be readable files")

    sys.path.insert(0, str(repo))
    old_cwd = Path.cwd()
    os.chdir(repo)
    try:
        from nanotrack.core.config import cfg
        from nanotrack.models.model_builder import ModelBuilder
        from nanotrack.utils.model_load import load_pretrain

        cfg.merge_from_file(str(config))
        cfg.CUDA = False
        torch.manual_seed(args.seed)
        np.random.seed(args.seed)
        model = load_pretrain(ModelBuilder(), str(weight)).cpu().eval()

        template = torch.randn(1, 3, 127, 127)
        search = torch.randn(1, 3, 255, 255)
        with torch.no_grad():
            template_feature = model.backbone(template)
            search_feature = model.backbone(search)
            torch_cls, torch_loc = model.ban_head(template_feature, search_feature)

        paths = {
            "template_backbone": output_dir / "template_backbone.onnx",
            "search_backbone": output_dir / "search_backbone.onnx",
            "head": output_dir / "head.onnx",
        }
        torch.onnx.export(
            model.backbone,
            template,
            str(paths["template_backbone"]),
            input_names=["image"],
            output_names=["feature"],
            opset_version=args.opset,
            do_constant_folding=True,
        )
        torch.onnx.export(
            model.backbone,
            search,
            str(paths["search_backbone"]),
            input_names=["image"],
            output_names=["feature"],
            opset_version=args.opset,
            do_constant_folding=True,
        )
        torch.onnx.export(
            model.ban_head,
            (template_feature, search_feature),
            str(paths["head"]),
            input_names=["template_feature", "search_feature"],
            output_names=["cls", "loc"],
            opset_version=args.opset,
            do_constant_folding=True,
        )

        for path in paths.values():
            onnx.checker.check_model(onnx.load(str(path)), full_check=True)

        ort_template = ort_run(paths["template_backbone"], {"image": template.numpy()})[0]
        ort_search = ort_run(paths["search_backbone"], {"image": search.numpy()})[0]
        ort_cls, ort_loc = ort_run(
            paths["head"],
            {"template_feature": ort_template, "search_feature": ort_search},
        )
        errors = {
            "template_backbone": assert_close(
                "template_backbone", ort_template, template_feature.numpy(), args.atol, args.rtol
            ),
            "search_backbone": assert_close(
                "search_backbone", ort_search, search_feature.numpy(), args.atol, args.rtol
            ),
            "head_cls": assert_close("head_cls", ort_cls, torch_cls.numpy(), args.atol, args.rtol),
            "head_loc": assert_close("head_loc", ort_loc, torch_loc.numpy(), args.atol, args.rtol),
        }

        metadata = {
            "source_commit": source_commit,
            "weight_sha256": sha256_file(weight),
            "seed": args.seed,
            "opset": args.opset,
            "torch_version": torch.__version__,
            "onnx_version": onnx.__version__,
            "onnxruntime_version": ort.__version__,
            "max_abs_error": errors,
            "graphs": {name: graph_meta(path) for name, path in paths.items()},
        }
        (output_dir / "export_metadata.json").write_text(
            json.dumps(metadata, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        print(json.dumps(metadata, indent=2, sort_keys=True))
    finally:
        os.chdir(old_cwd)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

