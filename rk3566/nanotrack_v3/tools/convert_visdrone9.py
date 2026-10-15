#!/usr/bin/env python3
"""Convert VisDrone2019-DET train/val splits into an audited YOLO9 view."""

from __future__ import annotations

import argparse
from pathlib import Path

from visdrone_dataset import convert_dataset, write_json_atomic


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", required=True, type=Path)
    parser.add_argument("--output-root", required=True, type=Path)
    parser.add_argument("--report", required=True, type=Path)
    parser.add_argument("--image-mode", choices=("symlink",), default="symlink")
    arguments = parser.parse_args()
    try:
        report = convert_dataset(
            arguments.source_root, arguments.output_root, arguments.image_mode
        )
        write_json_atomic(arguments.report, report)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print(
        f"VisDrone9 conversion {report['status']}: "
        f"images={sum(split['images'] for split in report['splits'].values())} "
        f"instances={report['totals']['converted']} report={arguments.report}"
    )
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
