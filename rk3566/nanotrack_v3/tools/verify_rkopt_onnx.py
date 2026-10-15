#!/usr/bin/env python3
"""Verify a fixed-shape nine-output Rockchip YOLOv8 ONNX model."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any

import numpy as np

from rkopt_validation import validate_output_contract, validate_runtime_outputs


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _shape(value_info: Any, context: str) -> tuple[int, ...]:
    dimensions = []
    for dimension in value_info.type.tensor_type.shape.dim:
        if not dimension.HasField("dim_value") or dimension.dim_value <= 0:
            raise ValueError(f"{context} must have static positive dimensions")
        dimensions.append(int(dimension.dim_value))
    return tuple(dimensions)


def verify(path: Path) -> dict[str, Any]:
    import onnx
    import onnxruntime as ort
    from onnx import TensorProto, numpy_helper

    path = path.resolve()
    if not path.is_file():
        raise ValueError(f"ONNX file does not exist: {path}")
    model = onnx.load(str(path))
    onnx.checker.check_model(model)

    initializer_names = {item.name for item in model.graph.initializer}
    inputs = [item for item in model.graph.input if item.name not in initializer_names]
    if len(inputs) != 1:
        raise ValueError("RKOPT ONNX must expose exactly one runtime input")
    if inputs[0].type.tensor_type.elem_type != TensorProto.FLOAT:
        raise ValueError("RKOPT ONNX input must be float32")
    input_shape = _shape(inputs[0], "input")
    if input_shape != (1, 3, 640, 640):
        raise ValueError(f"input shape {input_shape} does not match (1, 3, 640, 640)")

    contract = validate_output_contract(model)
    for initializer in model.graph.initializer:
        array = numpy_helper.to_array(initializer)
        if np.issubdtype(array.dtype, np.floating) and not np.isfinite(array).all():
            raise ValueError(f"initializer {initializer.name} contains NaN or Inf")

    session = ort.InferenceSession(str(path), providers=["CPUExecutionProvider"])
    runtime_outputs = session.run(None, {inputs[0].name: np.zeros(input_shape, np.float32)})
    validate_runtime_outputs(runtime_outputs)
    opsets = {entry.domain or "ai.onnx": entry.version for entry in model.opset_import}
    return {
        "status": "pass",
        "onnx": str(path),
        "size_bytes": path.stat().st_size,
        "sha256": sha256_file(path),
        "opsets": opsets,
        "input": {"name": inputs[0].name, "shape": list(input_shape), "dtype": "float32"},
        "output_contract": contract.to_dict(),
        "runtime_provider": session.get_providers()[0],
        "runtime_finite_zero_input": True,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--onnx", required=True, type=Path)
    parser.add_argument("--report", type=Path)
    arguments = parser.parse_args()
    try:
        report = verify(arguments.onnx)
        if arguments.report:
            arguments.report.parent.mkdir(parents=True, exist_ok=True)
            arguments.report.write_text(
                json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
            )
    except (OSError, RuntimeError, ValueError) as error:
        parser.error(str(error))
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
