#!/usr/bin/env python3
"""Shared streaming video loop for tracker-specific adapters."""

import time
from collections import namedtuple
from pathlib import Path

import cv2


TrackingSummary = namedtuple("TrackingSummary", "frame_count tracking_fps")


def run_tracking_video(
    video_path,
    output_path,
    bbox_output_path,
    initial_bbox,
    tracker,
):
    """Track one object without loading the complete video into memory."""
    video_path = Path(video_path)
    output_path = Path(output_path)
    bbox_output_path = Path(bbox_output_path)

    capture = cv2.VideoCapture(str(video_path))
    if not capture.isOpened():
        raise RuntimeError("failed to open input video {}".format(video_path))

    fps = capture.get(cv2.CAP_PROP_FPS) or 30.0
    width = int(capture.get(cv2.CAP_PROP_FRAME_WIDTH))
    height = int(capture.get(cv2.CAP_PROP_FRAME_HEIGHT))
    output_path.parent.mkdir(parents=True, exist_ok=True)
    bbox_output_path.parent.mkdir(parents=True, exist_ok=True)
    writer = cv2.VideoWriter(
        str(output_path), cv2.VideoWriter_fourcc(*"mp4v"), fps, (width, height)
    )
    if not writer.isOpened():
        capture.release()
        raise RuntimeError("failed to open output video {}".format(output_path))

    boxes = []
    elapsed = 0.0
    try:
        while True:
            ok, frame = capture.read()
            if not ok:
                break
            if not boxes:
                bbox = [float(value) for value in initial_bbox]
                tracker.initialize(frame, bbox)
            else:
                start = time.perf_counter()
                bbox = [float(value) for value in tracker.update(frame)]
                elapsed += time.perf_counter() - start

            boxes.append(bbox)
            x, y, w, h = (int(round(value)) for value in bbox)
            cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 255, 0), 4)
            writer.write(frame)
    finally:
        capture.release()
        writer.release()

    bbox_output_path.write_text(
        "".join(
            ",".join("{:.6f}".format(value) for value in bbox) + "\n"
            for bbox in boxes
        ),
        encoding="utf-8",
    )
    measured_frames = max(len(boxes) - 1, 0)
    tracking_fps = measured_frames / elapsed if elapsed else 0.0
    return TrackingSummary(len(boxes), tracking_fps)
