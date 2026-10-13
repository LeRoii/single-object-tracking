#!/usr/bin/env python3
"""Stream a regular video through FEAR without loading it all into RAM."""

import argparse
import os
import sys
from pathlib import Path

import cv2
import torch

from stream_tracking_video import run_tracking_video


class FearAdapter:
    def __init__(self, repo, config_path, config_name, weights_path):
        repo = Path(repo).resolve()
        config_path = os.path.relpath(str(Path(config_path).resolve()), str(repo))
        weights_path = Path(weights_path).resolve()
        os.chdir(str(repo))
        sys.path.insert(0, str(repo))
        from demo_video import get_tracker

        self.tracker = get_tracker(
            config_path=config_path,
            config_name=config_name,
            weights_path=str(weights_path),
        )

    def initialize(self, frame, bbox):
        self.tracker.initialize(cv2.cvtColor(frame, cv2.COLOR_BGR2RGB), bbox)

    def update(self, frame):
        bbox = self.tracker.update(cv2.cvtColor(frame, cv2.COLOR_BGR2RGB))["bbox"]
        torch.cuda.synchronize()
        return bbox


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", required=True)
    parser.add_argument("--config-path", required=True)
    parser.add_argument("--config-name", default="fear_tracker")
    parser.add_argument("--weights", required=True)
    parser.add_argument("--video", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--bbox-output", required=True)
    parser.add_argument("--bbox", nargs=4, type=float, required=True)
    return parser.parse_args()


def main():
    args = parse_args()
    tracker = FearAdapter(args.repo, args.config_path, args.config_name, args.weights)
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
