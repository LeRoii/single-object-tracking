#!/usr/bin/env python3
"""Shared validation for RKNN conversion targets supported by this project."""

from __future__ import annotations


SUPPORTED_RKNN_TARGETS = ("rk3566", "rk3588")


def require_supported_target(target: str) -> str:
    if target not in SUPPORTED_RKNN_TARGETS:
        raise ValueError(f"unsupported RKNN target: {target}")
    return target
