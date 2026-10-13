#!/usr/bin/env python3
"""Run STMTrack's official model and pipeline on a regular video."""

import argparse
import os
import sys
import time
from pathlib import Path

import cv2
import numpy as np
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
    parser.add_argument("--max-frames", type=int, default=0)
    return parser.parse_args()


def main():
    args = parse_args()
    repo = Path(args.repo).resolve()
    os.chdir(str(repo))
    sys.path.insert(0, str(repo / "main"))
    sys.path.insert(0, str(repo))

    from videoanalyst.config.config import cfg as root_cfg
    from videoanalyst.config.config import specify_task
    from videoanalyst.model import builder as model_builder
    from videoanalyst.pipeline import builder as pipeline_builder
    from videoanalyst.utils import complete_path_wt_root_in_cfg

    root_cfg.merge_from_file(str(Path(args.config).resolve()))
    root_cfg.test.track.model.task_model.STMTrack.pretrain_model_path = str(Path(args.snapshot).resolve())
    resolved = complete_path_wt_root_in_cfg(root_cfg, str(repo)).test
    task, task_cfg = specify_task(resolved)
    if task != "track":
        raise RuntimeError(f"unexpected task {task}")
    model = model_builder.build("track", task_cfg.model)
    tracker = pipeline_builder.build("track", task_cfg.pipeline, model)
    device = torch.device("cuda:0" if torch.cuda.is_available() else "cpu")
    tracker.set_device(device)

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
    elapsed = 0.0
    while True:
        ok, frame = capture.read()
        if not ok or (args.max_frames and len(boxes) >= args.max_frames):
            break
        if not boxes:
            bbox = np.asarray(args.bbox, dtype=np.float32)
            tracker.init(frame, bbox)
        else:
            start = time.perf_counter()
            bbox = np.asarray(tracker.update(frame), dtype=np.float32)
            if device.type == "cuda":
                torch.cuda.synchronize()
            elapsed += time.perf_counter() - start
        boxes.append(bbox.tolist())
        x, y, w, h = (int(round(value)) for value in bbox)
        cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 255, 0), 2)
        writer.write(frame)

    capture.release()
    writer.release()
    bbox_output.write_text(
        "".join(",".join(f"{value:.6f}" for value in bbox) + "\n" for bbox in boxes),
        encoding="utf-8",
    )
    measured_frames = max(len(boxes) - 1, 0)
    speed = measured_frames / elapsed if elapsed else 0.0
    print(f"device={device} frames={len(boxes)} tracking_fps={speed:.2f}")


if __name__ == "__main__":
    main()
