#!/usr/bin/env python3
"""Convert a video into the minimal VOT layout used by LightTrack's test script."""

import argparse
from pathlib import Path

import cv2


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--video", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--name", default="fear-demo")
    parser.add_argument("--bbox", nargs=4, type=float, required=True, metavar=("X", "Y", "W", "H"))
    parser.add_argument("--frames", type=int, default=120)
    return parser.parse_args()


def main():
    args = parse_args()
    root = Path(args.output)
    sequence = root / args.name
    color = sequence / "color"
    color.mkdir(parents=True, exist_ok=True)

    capture = cv2.VideoCapture(args.video)
    written = 0
    while written < args.frames:
        ok, frame = capture.read()
        if not ok:
            break
        written += 1
        output_path = color / f"{written:08d}.jpg"
        if not cv2.imwrite(str(output_path), frame):
            raise RuntimeError(f"failed to write {output_path}")
    capture.release()

    if written < 2:
        raise RuntimeError(f"video yielded only {written} frame(s)")

    bbox_line = ",".join(str(value) for value in args.bbox)
    (root / "list.txt").write_text(args.name + "\n", encoding="utf-8")
    (sequence / "groundtruth.txt").write_text(
        (bbox_line + "\n") * written,
        encoding="utf-8",
    )
    print(f"prepared {written} frames in {sequence}")


if __name__ == "__main__":
    main()
