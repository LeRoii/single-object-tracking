import math
import sys
from pathlib import Path

import numpy as np
import onnx
import pytest
from onnx import TensorProto, helper


TOOLS_DIR = Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS_DIR))

from rkopt_validation import (  # noqa: E402
    Detection,
    compare_detections,
    decode_rkopt_outputs,
    validate_output_contract,
)


def output_shapes(class_count=9):
    shapes = []
    for grid in (80, 40, 20):
        shapes.extend(
            ([1, 64, grid, grid], [1, class_count, grid, grid], [1, 1, grid, grid])
        )
    return shapes


def synthetic_model(shapes=None):
    shapes = output_shapes() if shapes is None else shapes
    graph_outputs = [
        helper.make_tensor_value_info(f"output{index}", TensorProto.FLOAT, shape)
        for index, shape in enumerate(shapes)
    ]
    graph = helper.make_graph(
        [],
        "rkopt-contract",
        [helper.make_tensor_value_info("images", TensorProto.FLOAT, [1, 3, 640, 640])],
        graph_outputs,
    )
    return helper.make_model(graph, opset_imports=[helper.make_opsetid("", 12)])


def blank_outputs():
    arrays = []
    for grid in (80, 40, 20):
        arrays.extend(
            (
                np.zeros((1, 64, grid, grid), dtype=np.float32),
                np.zeros((1, 9, grid, grid), dtype=np.float32),
                np.zeros((1, 1, grid, grid), dtype=np.float32),
            )
        )
    return arrays


def test_nine_output_contract_accepts_fixed_640_model():
    report = validate_output_contract(synthetic_model())

    assert report.class_count == 9
    assert report.dfl_bins == 16
    assert report.strides == (8, 16, 32)
    assert report.roles == (
        "box_s8",
        "class_s8",
        "score_sum_s8",
        "box_s16",
        "class_s16",
        "score_sum_s16",
        "box_s32",
        "class_s32",
        "score_sum_s32",
    )


@pytest.mark.parametrize(
    ("mutate", "message"),
    [
        (lambda shapes: shapes[:-1], "nine outputs"),
        (lambda shapes: [shapes[1], shapes[0], *shapes[2:]], "output 0"),
        (
            lambda shapes: [
                shape if index not in (1, 4, 7) else [1, 10, shape[2], shape[3]]
                for index, shape in enumerate(shapes)
            ],
            "class count",
        ),
        (
            lambda shapes: [
                ["batch", *shape[1:]] if index == 0 else shape
                for index, shape in enumerate(shapes)
            ],
            "static integer dimensions",
        ),
    ],
)
def test_contract_rejects_missing_swapped_wrong_class_and_dynamic_batch(mutate, message):
    with pytest.raises(ValueError, match=message):
        validate_output_contract(synthetic_model(mutate(output_shapes())))


def test_dfl_decode_recovers_deterministic_target_and_rejects_nonfinite():
    outputs = blank_outputs()
    box, classes, score_sum = outputs[6:9]
    y, x = 2, 3
    box[:, :, y, x] = -20.0
    for side, distance in enumerate((1, 2, 3, 4)):
        box[0, side * 16 + distance, y, x] = 20.0
    classes[0, 4, y, x] = 0.9
    score_sum[0, 0, y, x] = 0.9

    detections = decode_rkopt_outputs(outputs, conf=0.25, nms=0.45)

    assert len(detections) == 1
    assert detections[0].class_id == 4
    assert detections[0].confidence == pytest.approx(0.9)
    assert detections[0].xyxy == pytest.approx((80.0, 16.0, 208.0, 208.0))

    outputs[0][0, 0, 0, 0] = math.nan
    with pytest.raises(ValueError, match="finite"):
        decode_rkopt_outputs(outputs, conf=0.25, nms=0.45)


def test_detection_matching_is_class_aware_and_reports_distribution():
    reference = [
        Detection((0.0, 0.0, 10.0, 10.0), 0.90, 0),
        Detection((20.0, 20.0, 30.0, 30.0), 0.80, 1),
        Detection((40.0, 40.0, 50.0, 50.0), 0.70, 2),
    ]
    candidate = [
        Detection((0.0, 0.0, 10.0, 10.0), 0.89, 0),
        Detection((20.0, 20.0, 30.0, 30.0), 0.77, 1),
        Detection((40.0, 40.0, 50.0, 50.0), 0.70, 1),
    ]

    report = compare_detections(reference, candidate, min_iou=0.9)

    assert report.reference_count == 3
    assert report.candidate_count == 3
    assert report.matched_count == 2
    assert report.matched_recall == pytest.approx(2 / 3)
    assert report.median_iou == pytest.approx(1.0)
    assert report.p95_confidence_delta == pytest.approx(0.029)
