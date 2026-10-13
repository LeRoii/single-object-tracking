import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

import cv2
import numpy as np


try:
    from scripts.evaluate_got10k_nanotrack import (
        aggregate_got_metrics,
        got_frame_metrics,
        load_got_sequence,
        load_hidden_panel,
    )
except ImportError as error:
    _IMPORT_ERROR = error
else:
    _IMPORT_ERROR = None


class Got10kEvaluationTest(unittest.TestCase):
    def setUp(self):
        if _IMPORT_ERROR is not None:
            self.fail("GOT-10K evaluator API is missing: {}".format(_IMPORT_ERROR))

    def test_metrics_match_official_visible_post_initial_semantics(self):
        ground_truth = [(0, 0, 10, 10)] * 4
        predictions = [(99, 99, 1, 1), (0, 0, 10, 10),
                       (0, 0, 5, 10), (0, 0, 10, 10)]
        result = got_frame_metrics(ground_truth, predictions, [8, 8, 8, 0])
        self.assertEqual(result["scored_frame_count"], 2)
        self.assertEqual(result["ious"], [1.0, 0.5])
        self.assertAlmostEqual(result["ao"], 0.75)
        self.assertAlmostEqual(result["sr_0_5"], 0.5)
        self.assertAlmostEqual(result["sr_0_75"], 0.5)

    def test_aggregate_is_frame_weighted_like_official_got_toolkit(self):
        result = aggregate_got_metrics([
            {"name": "short", "ious": [1.0]},
            {"name": "long", "ious": [0.0, 0.0, 0.0]},
        ])
        self.assertEqual(result["sequence_count"], 2)
        self.assertEqual(result["scored_frame_count"], 4)
        self.assertAlmostEqual(result["ao"], 0.25)
        self.assertAlmostEqual(result["sr_0_5"], 0.25)
        self.assertAlmostEqual(result["sr_0_75"], 0.25)

    def test_sequence_loader_requires_aligned_boxes_cover_and_images(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "GOT-10k_Val_000001"
            path.mkdir()
            (path / "meta_info.ini").write_text(
                "[METAINFO]\nroot_class: vehicle\nresolution: (64, 48)\n")
            (path / "groundtruth.txt").write_text("1,2,10,20\n2,3,10,20\n")
            (path / "cover.label").write_text("8\n4\n")
            for index in (1, 2):
                self.assertTrue(cv2.imwrite(
                    str(path / "{:08d}.jpg".format(index)),
                    np.zeros((48, 64, 3), dtype=np.uint8)))
            sequence = load_got_sequence(path)
            self.assertEqual(sequence.name, path.name)
            self.assertEqual(sequence.root_class, "vehicle")
            self.assertEqual(sequence.cover, (8, 4))
            self.assertEqual(len(sequence.image_paths), 2)
            (path / "cover.label").write_text("8\n")
            with self.assertRaisesRegex(ValueError, "lengths differ"):
                load_got_sequence(path)

    def test_hidden_panel_is_denied_until_selection_is_final(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            sequence = base / "GOT-10k_Train_000001"
            sequence.mkdir()
            (sequence / "meta_info.ini").write_text("[METAINFO]\nroot_class: person\n")
            (sequence / "groundtruth.txt").write_text("1,2,10,20\n2,3,10,20\n")
            (sequence / "cover.label").write_text("8\n8\n")
            for index in (1, 2):
                cv2.imwrite(str(sequence / "{:08d}.jpg".format(index)),
                            np.zeros((32, 32, 3), dtype=np.uint8))
            inventory = base / "inventory.json"
            inventory.write_text(json.dumps({
                "splits": {"hidden": [sequence.name]},
                "sequences": {sequence.name: {"path": str(sequence), "root_class": "person"}},
            }))
            with self.assertRaisesRegex(ValueError, "selection is not final"):
                load_hidden_panel(inventory, {"selection_final": False})
            panel = load_hidden_panel(inventory, {"selection_final": True})
            self.assertEqual([item.name for item in panel], [sequence.name])

    def test_direct_cli_help_runs_outside_workspace(self):
        script = Path(__file__).resolve().parents[1] / "scripts/evaluate_got10k_nanotrack.py"
        completed = subprocess.run(
            [sys.executable, str(script), "--help"], cwd="/tmp",
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertIn("train-hidden", completed.stdout)


if __name__ == "__main__":
    unittest.main()
