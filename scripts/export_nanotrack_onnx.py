#!/usr/bin/env python3
"""Export a NanoTrack checkpoint as fixed-shape ONNX components and compare outputs."""

import argparse
import hashlib
import json
import os
import sys
from pathlib import Path
from typing import Dict, List, Sequence

import numpy as np
import torch
import torch.nn as nn


class ExportHardSwish(nn.Module):
    def forward(self, value: torch.Tensor) -> torch.Tensor:
        return value * torch.clamp(value + 3.0, min=0.0, max=6.0) / 6.0


class ExportHardSigmoid(nn.Module):
    def forward(self, value: torch.Tensor) -> torch.Tensor:
        return torch.clamp(value + 3.0, min=0.0, max=6.0) / 6.0


def replace_hard_activations(module: nn.Module) -> nn.Module:
    for name, child in list(module.named_children()):
        if isinstance(child, nn.Hardswish):
            setattr(module, name, ExportHardSwish())
        elif isinstance(child, nn.Hardsigmoid):
            setattr(module, name, ExportHardSigmoid())
        else:
            replace_hard_activations(child)
    return module


def compare_outputs(
    reference: Sequence[np.ndarray], candidate: Sequence[np.ndarray]
) -> List[Dict[str, object]]:
    if len(reference) != len(candidate):
        raise ValueError("output counts differ")
    comparisons = []
    for expected, actual in zip(reference, candidate):
        if expected.shape != actual.shape:
            raise ValueError("output shapes differ: {} != {}".format(expected.shape, actual.shape))
        difference = np.abs(expected.astype(np.float64) - actual.astype(np.float64))
        comparisons.append(
            {
                "shape": list(expected.shape),
                "max_abs_error": float(difference.max()) if difference.size else 0.0,
                "mean_abs_error": float(difference.mean()) if difference.size else 0.0,
            }
        )
    return comparisons


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with Path(path).open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--nanotrack-root", type=Path, required=True)
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--snapshot", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--opset", type=int, default=11)
    parser.add_argument("--max-error", type=float, default=1e-4)
    args = parser.parse_args()

    nanotrack_root = args.nanotrack_root.resolve()
    config_path = args.config.resolve()
    snapshot_path = args.snapshot.resolve()
    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    sys.path.insert(0, str(nanotrack_root))
    os.chdir(str(nanotrack_root))

    import onnx
    import onnxruntime as ort
    import torch
    from nanotrack.core.config import cfg
    from nanotrack.models.model_builder import ModelBuilder
    from nanotrack.utils.model_load import load_pretrain

    cfg.merge_from_file(str(config_path))
    torch.manual_seed(123456)
    model = load_pretrain(ModelBuilder(), str(snapshot_path)).cpu().eval()
    replace_hard_activations(model)
    components = [
        {
            "name": "template_backbone_127",
            "module": model.backbone,
            "inputs": (torch.randn(1, 3, 127, 127),),
            "input_names": ["template"],
            "output_names": ["template_feature"],
        },
        {
            "name": "search_backbone_255",
            "module": model.backbone,
            "inputs": (torch.randn(1, 3, 255, 255),),
            "input_names": ["search"],
            "output_names": ["search_feature"],
        },
        {
            "name": "head",
            "module": model.ban_head,
            "inputs": (torch.randn(1, 96, 8, 8), torch.randn(1, 96, 16, 16)),
            "input_names": ["template_feature", "search_feature"],
            "output_names": ["classification", "localization"],
        },
    ]
    report = {
        "snapshot": str(snapshot_path),
        "snapshot_sha256": _sha256(snapshot_path),
        "opset": args.opset,
        "max_error_threshold": args.max_error,
        "components": {},
    }
    worst_error = 0.0
    with torch.no_grad():
        for component in components:
            path = output_dir / (component["name"] + ".onnx")
            torch.onnx.export(
                component["module"],
                component["inputs"],
                str(path),
                input_names=component["input_names"],
                output_names=component["output_names"],
                opset_version=args.opset,
                do_constant_folding=True,
            )
            onnx_model = onnx.load(str(path))
            onnx.checker.check_model(onnx_model)
            torch_outputs = component["module"](*component["inputs"])
            if not isinstance(torch_outputs, tuple):
                torch_outputs = (torch_outputs,)
            reference = [output.detach().cpu().numpy() for output in torch_outputs]
            session = ort.InferenceSession(str(path), providers=["CPUExecutionProvider"])
            feed = {
                name: tensor.detach().cpu().numpy()
                for name, tensor in zip(component["input_names"], component["inputs"])
            }
            candidate = session.run(component["output_names"], feed)
            comparisons = compare_outputs(reference, candidate)
            worst_error = max(
                worst_error, max(item["max_abs_error"] for item in comparisons)
            )
            report["components"][component["name"]] = {
                "path": str(path),
                "sha256": _sha256(path),
                "inputs": [list(tensor.shape) for tensor in component["inputs"]],
                "outputs": comparisons,
            }
            print(
                "exported {} max_abs_error={:.8g}".format(
                    component["name"], max(item["max_abs_error"] for item in comparisons)
                ),
                flush=True,
            )
    report["worst_max_abs_error"] = worst_error
    report["passed"] = worst_error <= args.max_error
    report_path = output_dir / "verification.json"
    report_path.write_text(json.dumps(report, indent=2, sort_keys=True), encoding="utf-8")
    print(json.dumps(report, indent=2, sort_keys=True))
    if not report["passed"]:
        raise RuntimeError("ONNX maximum error exceeds threshold")


if __name__ == "__main__":
    main()
