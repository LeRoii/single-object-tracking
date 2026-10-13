#!/usr/bin/env python3
"""Run LightTrack continuously on a regular video without VOT resets."""

import argparse
import os
import sys
from pathlib import Path

import cv2
import numpy as np
import torch

from stream_tracking_video import run_tracking_video


class LightTrackAdapter:
    def __init__(self, repo, arch, snapshot, stride, even, path_name):
        repo = Path(repo).resolve()
        os.chdir(str(repo))
        sys.path.insert(0, str(repo / "lib"))
        sys.path.insert(0, str(repo))

        import models.models as models
        from easydict import EasyDict as edict
        from tracker.lighttrack import Lighttrack
        from utils.utils import cxy_wh_2_rect, load_pretrain

        info = edict()
        info.arch = arch
        info.dataset = "VOT2019"
        info.epoch_test = False
        info.stride = stride
        self.tracker = Lighttrack(info, even=even)
        self.network = models.__dict__[arch](path_name, stride=stride)
        self.network = load_pretrain(self.network, str(Path(snapshot).resolve()))
        self.network.eval().cuda()
        self.state = None
        self.cxy_wh_2_rect = cxy_wh_2_rect

    def initialize(self, frame, bbox):
        x, y, w, h = bbox
        target_pos = np.asarray([x + w / 2.0, y + h / 2.0])
        target_size = np.asarray([w, h])
        rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        self.state = self.tracker.init(rgb, target_pos, target_size, self.network)

    def update(self, frame):
        rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        self.state = self.tracker.track(self.state, rgb)
        torch.cuda.synchronize()
        return self.cxy_wh_2_rect(
            self.state["target_pos"], self.state["target_sz"]
        )


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", required=True)
    parser.add_argument("--arch", default="LightTrackM_Subnet")
    parser.add_argument("--snapshot", required=True)
    parser.add_argument("--stride", type=int, default=16)
    parser.add_argument("--even", type=int, default=0)
    parser.add_argument("--path-name", required=True)
    parser.add_argument("--video", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--bbox-output", required=True)
    parser.add_argument("--bbox", nargs=4, type=float, required=True)
    return parser.parse_args()


def main():
    args = parse_args()
    tracker = LightTrackAdapter(
        args.repo,
        args.arch,
        args.snapshot,
        args.stride,
        args.even,
        args.path_name,
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
