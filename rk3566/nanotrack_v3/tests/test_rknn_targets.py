import importlib
import sys
from pathlib import Path

import pytest


TOOLS_DIR = Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS_DIR))

@pytest.mark.parametrize("target", ["rk3566", "rk3588"])
def test_supported_target_is_returned_unchanged(target):
    require_supported_target = importlib.import_module(
        "rknn_targets"
    ).require_supported_target

    assert require_supported_target(target) == target


def test_unsupported_target_is_rejected_before_conversion():
    require_supported_target = importlib.import_module(
        "rknn_targets"
    ).require_supported_target

    with pytest.raises(ValueError, match="unsupported RKNN target.*rk3399pro"):
        require_supported_target("rk3399pro")
