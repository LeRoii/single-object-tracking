#!/usr/bin/env python3
"""Independently audit a generated VisDrone9 YOLO dataset view."""

from __future__ import annotations

import argparse
from pathlib import Path

from visdrone_dataset import CLASS_NAMES, audit_dataset, write_json_atomic


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dataset-root", required=True, type=Path)
    parser.add_argument("--report", required=True, type=Path)
    arguments = parser.parse_args()
    try:
        report = audit_dataset(arguments.dataset_root, CLASS_NAMES)
        write_json_atomic(arguments.report, report)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print(
        f"VisDrone9 audit {report['status']}: "
        f"images={report['totals']['images']} "
        f"instances={report['totals']['instances']} report={arguments.report}"
    )
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
