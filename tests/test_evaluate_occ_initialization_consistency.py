import sys
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import evaluate_occ_initialization_consistency as evaluator


class CenterMetricsTest(unittest.TestCase):
    def test_invalid_box_center_cannot_contaminate_metrics(self):
        row = {"frame_id": "7", "tracking_x": "nan", "tracking_y": "0",
               "tracking_width": "10", "tracking_height": "10"}
        with self.assertRaisesRegex(ValueError, "invalid tracking box on frame 7"):
            evaluator.box(row, "tracking")

    def test_center_summary_uses_box_centers_and_reports_worst_frame(self):
        predicted = [[0, 0, 10, 10], [0, 0, 10, 10], [0, 0, 10, 10]]
        reference = [[0, 0, 10, 10], [2, 0, 10, 10], [0, 4, 10, 10]]
        result = evaluator.center_summary(predicted, reference, [0, 1, 2])
        self.assertEqual(result["frames"], 3)
        self.assertAlmostEqual(result["mean_px"], 2)
        self.assertEqual(result["p95_px"], 4)
        self.assertEqual(result["max_px"], 4)
        self.assertEqual(result["max_frame"], 2)


if __name__ == "__main__":
    unittest.main()
