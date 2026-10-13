import tempfile
import unittest
from pathlib import Path

import cv2
import numpy as np

from scripts.stream_tracking_video import run_tracking_video


class FakeTracker:
    def __init__(self):
        self.bbox = None

    def initialize(self, frame, bbox):
        self.bbox = list(bbox)

    def update(self, frame):
        self.bbox[0] += 1
        return list(self.bbox)


class RunTrackingVideoTest(unittest.TestCase):
    def test_streams_every_frame_and_records_one_box_per_frame(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            source = root / "source.mp4"
            output = root / "tracked.mp4"
            boxes = root / "boxes.txt"

            writer = cv2.VideoWriter(
                str(source), cv2.VideoWriter_fourcc(*"mp4v"), 10.0, (64, 48)
            )
            self.assertTrue(writer.isOpened())
            for value in (20, 40, 60):
                writer.write(np.full((48, 64, 3), value, dtype=np.uint8))
            writer.release()

            summary = run_tracking_video(
                video_path=source,
                output_path=output,
                bbox_output_path=boxes,
                initial_bbox=[5, 6, 10, 12],
                tracker=FakeTracker(),
            )

            capture = cv2.VideoCapture(str(output))
            decoded = 0
            while capture.read()[0]:
                decoded += 1
            capture.release()

            self.assertEqual(summary.frame_count, 3)
            self.assertEqual(decoded, 3)
            self.assertEqual(
                boxes.read_text(encoding="utf-8").splitlines(),
                ["5.000000,6.000000,10.000000,12.000000",
                 "6.000000,6.000000,10.000000,12.000000",
                 "7.000000,6.000000,10.000000,12.000000"],
            )


if __name__ == "__main__":
    unittest.main()
