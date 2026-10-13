import tempfile
import unittest
from pathlib import Path

import cv2
import numpy as np
import yaml

from scripts.evaluate_visdrone_sot_nanotrack import (
    load_visdrone_sot_split,
    run_visdrone_sequence,
)


class MovingFakeTracker:
    def __init__(self):
        self.box = None

    def init(self, image, box):
        self.box = list(box)

    def track(self, image):
        self.box[0] += 1.0
        return {"bbox": list(self.box)}


class VisDroneSOTEvaluationTest(unittest.TestCase):
    def test_loads_official_val_layout_and_scores_sequence(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            base = root / "VisDrone2019-SOT-val"
            (base / "annotations").mkdir(parents=True)
            sequence_dir = base / "sequences/sequence_a"
            sequence_dir.mkdir(parents=True)
            (base / "annotations/sequence_a.txt").write_text(
                "5,6,10,12\n6,6,10,12\n7,6,10,12\n", encoding="utf-8"
            )
            for frame in range(1, 4):
                cv2.imwrite(
                    str(sequence_dir / "{:07d}.jpg".format(frame)),
                    np.zeros((32, 48, 3), dtype=np.uint8),
                )

            sequences = load_visdrone_sot_split(root, "val")
            result = run_visdrone_sequence(sequences[0], MovingFakeTracker())

        self.assertEqual(len(sequences), 1)
        self.assertEqual(result["name"], "sequence_a")
        self.assertAlmostEqual(result["metrics"]["success_auc"], 20.0 / 21.0)
        self.assertEqual(result["metrics"]["valid_frames"], 3)

    def test_four_dataset_full_config_has_requested_sampling_and_small_batch(self):
        workspace = Path(__file__).resolve().parents[1]
        config = yaml.safe_load(
            (workspace / "configs/nanotrack_v3_four_dataset_finetune.yaml").read_text(
                encoding="utf-8"
            )
        )

        self.assertEqual(
            config["DATASET"]["NAMES"],
            ["UAV123", "VISDRONE_SOT", "VISDRONE_MOT", "UAVDT"],
        )
        self.assertEqual(
            [config["DATASET"][name]["NUM_USE"] for name in config["DATASET"]["NAMES"]],
            [4000, 6000, 6000, 4000],
        )
        self.assertEqual(config["DATASET"]["VIDEOS_PER_EPOCH"], 20000)
        self.assertEqual(config["TRAIN"]["BATCH_SIZE"], 8)
        self.assertEqual(config["TRAIN"]["EPOCH"], 5)


if __name__ == "__main__":
    unittest.main()
