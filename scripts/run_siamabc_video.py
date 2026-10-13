#!/usr/bin/env python3
"""Stream a regular video through SiamABC without retaining all frames."""

import argparse
import os
import sys
from pathlib import Path

import cv2
import torch

from stream_tracking_video import run_tracking_video


class SiamABCAdapter:
    def __init__(self, repo, config_path, config_name, model_size, weights_path):
        repo = Path(repo).resolve()
        config_path = os.path.relpath(str(Path(config_path).resolve()), str(repo))
        weights_path = Path(weights_path).resolve()
        os.chdir(str(repo))
        sys.path.insert(0, str(repo))
        from realtime_test import get_tracker
        from core.utils.hydra import load_hydra_config_from_path

        config = load_hydra_config_from_path(
            config_path=config_path, config_name=config_name
        )
        config["model"]["model_size"] = "S" if model_size == "S_Tiny" else "M"
        self.tracker = get_tracker(config, str(weights_path))

    def initialize(self, frame, bbox):
        rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        self.tracker.initialize(rgb, bbox)

    def update(self, frame):
        rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        bbox, _ = self.tracker.update(rgb)
        torch.cuda.synchronize()
        return bbox


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", required=True)
    parser.add_argument("--config-path", required=True)
    parser.add_argument("--config-name", default="SiamABC_tracker")
    parser.add_argument("--model-size", default="S_Tiny")
    parser.add_argument("--weights", required=True)
    parser.add_argument("--video", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--bbox-output", required=True)
    parser.add_argument("--bbox", nargs=4, type=float, required=True)
    return parser.parse_args()


def main():
    args = parse_args()
    tracker = SiamABCAdapter(
        args.repo,
        args.config_path,
        args.config_name,
        args.model_size,
        args.weights,
    )
    summary = run_tracking_video(
        args.video, args.output, args.bbox_output, args.bbox, tracker
    )
    print(
        "device=cuda frames={} tracking_fps={:.2f}".format(
            summary.frame_count, summary.tracking_fps
        )
    )


if __name__ == "__main__":
    main()
