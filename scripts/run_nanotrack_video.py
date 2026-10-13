#!/usr/bin/env python3
"""Headless video runner built from NanoTrack's official demo API."""

import argparse
import sys
import time
from pathlib import Path

import cv2
import torch


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--snapshot", required=True)
    parser.add_argument("--video", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--bbox-output", required=True)
    parser.add_argument("--bbox", nargs=4, type=float, required=True, metavar=("X", "Y", "W", "H"))
    return parser.parse_args()


def main():
    args = parse_args()
    repo = Path(args.repo).resolve()
    sys.path.insert(0, str(repo))

    from nanotrack.core.config import cfg
    from nanotrack.models.model_builder import ModelBuilder
    from nanotrack.tracker.tracker_builder import build_tracker
    from nanotrack.utils.model_load import load_pretrain

    cfg.merge_from_file(args.config)
    cfg.CUDA = bool(cfg.CUDA and torch.cuda.is_available())
    device = torch.device("cuda" if cfg.CUDA else "cpu")
    model = load_pretrain(ModelBuilder(), args.snapshot).eval().to(device)
    tracker = build_tracker(model)

    capture = cv2.VideoCapture(args.video)
    fps = capture.get(cv2.CAP_PROP_FPS) or 30.0
    width = int(capture.get(cv2.CAP_PROP_FRAME_WIDTH))
    height = int(capture.get(cv2.CAP_PROP_FRAME_HEIGHT))
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    bbox_output = Path(args.bbox_output)
    bbox_output.parent.mkdir(parents=True, exist_ok=True)
    writer = cv2.VideoWriter(str(output), cv2.VideoWriter_fourcc(*"mp4v"), fps, (width, height))
    if not writer.isOpened():
        raise RuntimeError(f"failed to open output video {output}")

    boxes = []
    frame_count = 0
    tracking_time = 0.0
    with torch.no_grad():
        while True:
            ok, frame = capture.read()
            if not ok:
                break
            if frame_count == 0:
                bbox = list(args.bbox)
                tracker.init(frame, bbox)
            else:
                start = time.perf_counter()
                bbox = tracker.track(frame)["bbox"]
                if cfg.CUDA:
                    torch.cuda.synchronize()
                tracking_time += time.perf_counter() - start
            boxes.append(bbox)
            x, y, w, h = (int(round(value)) for value in bbox)
            cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 255, 0), 2)
            writer.write(frame)
            frame_count += 1

    capture.release()
    writer.release()
    bbox_output.write_text(
        "".join(",".join(f"{value:.6f}" for value in bbox) + "\n" for bbox in boxes),
        encoding="utf-8",
    )
    measured_frames = max(frame_count - 1, 0)
    speed = measured_frames / tracking_time if tracking_time else 0.0
    print(f"device={device} frames={frame_count} tracking_fps={speed:.2f}")


if __name__ == "__main__":
    main()
