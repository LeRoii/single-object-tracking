import tempfile
import unittest
from pathlib import Path
import subprocess
import sys

import cv2
import numpy as np

from scripts.evaluate_uav123_nanotrack import evaluate_predictions, run_sequence, summarize_sequences
from scripts.uav123_nanotrack import SequenceSpec


class FakeTracker:
    def __init__(self):
        self.box = None
        self.track_calls = 0

    def init(self, image, box):
        self.box = list(box)

    def track(self, image):
        self.track_calls += 1
        self.box[0] += 1.0
        return {"bbox": list(self.box)}


class UAV123EvaluationTest(unittest.TestCase):
    def test_cli_help_runs_when_invoked_as_a_script(self):
        workspace = Path(__file__).resolve().parents[1]
        completed = subprocess.run(
            [sys.executable, str(workspace / "scripts/evaluate_uav123_nanotrack.py"), "--help"],
            cwd=str(workspace),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            universal_newlines=True,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertIn("--snapshot", completed.stdout)

    def test_perfect_predictions_score_one_and_ignore_absent_frames(self):
        ground_truth = [(0.0, 0.0, 20.0, 10.0), None, (5.0, 2.0, 20.0, 10.0)]
        predictions = [(0.0, 0.0, 20.0, 10.0), (999.0, 999.0, 1.0, 1.0), (5.0, 2.0, 20.0, 10.0)]

        metrics = evaluate_predictions(ground_truth, predictions)

        self.assertEqual(metrics["valid_frames"], 2)
        self.assertAlmostEqual(metrics["success_auc"], 20.0 / 21.0)
        self.assertAlmostEqual(metrics["precision_20"], 1.0)
        self.assertAlmostEqual(metrics["normalized_precision_auc"], 1.0)
        self.assertEqual(metrics["zero_iou_frames"], 0)

    def test_disjoint_predictions_have_zero_overlap_and_precision(self):
        ground_truth = [(0.0, 0.0, 10.0, 10.0)]
        predictions = [(100.0, 100.0, 10.0, 10.0)]

        metrics = evaluate_predictions(ground_truth, predictions)

        self.assertAlmostEqual(metrics["mean_iou"], 0.0)
        self.assertAlmostEqual(metrics["success_auc"], 0.0)
        self.assertAlmostEqual(metrics["precision_20"], 0.0)
        self.assertEqual(metrics["zero_iou_frames"], 1)

    def test_summary_averages_sequences_and_reports_categories(self):
        results = [
            {"name": "car1", "category": "vehicle", "metrics": {"success_auc": 1.0, "precision_20": 1.0, "normalized_precision_auc": 1.0, "mean_iou": 1.0, "zero_iou_frames": 0, "valid_frames": 10}},
            {"name": "person1", "category": "person", "metrics": {"success_auc": 0.5, "precision_20": 0.5, "normalized_precision_auc": 0.5, "mean_iou": 0.5, "zero_iou_frames": 2, "valid_frames": 8}},
        ]

        summary = summarize_sequences(results)

        self.assertAlmostEqual(summary["overall"]["success_auc"], 0.75)
        self.assertAlmostEqual(summary["categories"]["vehicle"]["success_auc"], 1.0)
        self.assertAlmostEqual(summary["categories"]["person"]["success_auc"], 0.5)
        self.assertEqual(summary["overall"]["sequences"], 2)

    def test_run_sequence_tracks_through_absent_frames_without_reset(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            image_dir = root / "data_seq/UAV123_10fps/car1"
            annotation_dir = root / "anno/UAV123_10fps"
            image_dir.mkdir(parents=True)
            annotation_dir.mkdir(parents=True)
            annotation_dir.joinpath("car1.txt").write_text(
                "5,6,10,12\nNaN,NaN,NaN,NaN\n7,6,10,12\n", encoding="utf-8"
            )
            for frame in range(1, 4):
                cv2.imwrite(
                    str(image_dir / "{:06d}.jpg".format(frame)),
                    np.zeros((32, 48, 3), dtype=np.uint8),
                )
            spec = SequenceSpec("car1", "car1", 1, 3, 6, "jpg")
            tracker = FakeTracker()

            result = run_sequence(root, spec, tracker)

        self.assertEqual(tracker.track_calls, 2)
        self.assertEqual(result["predictions"], [[5.0, 6.0, 10.0, 12.0], [6.0, 6.0, 10.0, 12.0], [7.0, 6.0, 10.0, 12.0]])
        self.assertEqual(result["metrics"]["valid_frames"], 2)
        self.assertEqual(result["metrics"]["zero_iou_frames"], 0)


if __name__ == "__main__":
    unittest.main()
