#!/usr/bin/env python3
"""Headless runner using SiamMask's official demo inference functions."""

import argparse
import sys
import time
from pathlib import Path
from types import SimpleNamespace

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
    parser.add_argument("--max-frames", type=int, default=120)
    return parser.parse_args()


def main():
    args = parse_args()
    repo = Path(args.repo).resolve()
    experiment = repo / "experiments" / "siammask_sharp"
    sys.path.insert(0, str(experiment))
    sys.path.insert(0, str(repo))

    from custom import Custom
    from tools.test import siamese_init, siamese_track
    from utils.config_helper import load_config
    from utils.load_helper import load_pretrain

    config_args = SimpleNamespace(config=args.config, arch="")
    cfg = load_config(config_args)
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    model = Custom(anchors=cfg["anchors"])
    model = load_pretrain(model, args.snapshot).eval().to(device)

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
    with torch.no_grad():
        while len(boxes) < args.max_frames:
            ok, frame = capture.read()
            if not ok:
                break
            if not boxes:
                x, y, w, h = args.bbox
                state = siamese_init(
                    frame,
                    np.array([x + w / 2, y + h / 2]),
                    np.array([w, h]),
                    model,
                    cfg["hp"],
                    device=device,
                )
                bbox = [x, y, w, h]
            else:
                start = time.perf_counter()
                state = siamese_track(
                    state,
                    frame,
                    mask_enable=True,
                    refine_enable=True,
                    device=device,
                )
                if device.type == "cuda":
                    torch.cuda.synchronize()
                elapsed += time.perf_counter() - start
                polygon = np.asarray(state["ploygon"], dtype=np.int32).reshape(-1, 2)
                bbox = list(cv2.boundingRect(polygon))
                mask = state["mask"] > state["p"].seg_thr
                frame[:, :, 2] = np.where(mask, 255, frame[:, :, 2])
                cv2.polylines(frame, [polygon.reshape(-1, 1, 2)], True, (0, 255, 0), 2)
            boxes.append(bbox)
            writer.write(frame)

    capture.release()
    writer.release()
    bbox_output.write_text(
        "".join(",".join(str(value) for value in bbox) + "\n" for bbox in boxes),
        encoding="utf-8",
    )
    measured_frames = max(len(boxes) - 1, 0)
    speed = measured_frames / elapsed if elapsed else 0.0
    print(f"device={device} frames={len(boxes)} tracking_fps={speed:.2f}")


if __name__ == "__main__":
    main()
