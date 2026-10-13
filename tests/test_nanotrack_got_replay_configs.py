import tempfile
import unittest
from pathlib import Path


try:
    from scripts.generate_nanotrack_got_replay_configs import (
        allocate_integer_quotas,
        build_stage_config,
        build_stage_quotas,
    )
except ImportError as error:
    _IMPORT_ERROR = error
else:
    _IMPORT_ERROR = None


class GotReplayConfigTest(unittest.TestCase):
    def setUp(self):
        if _IMPORT_ERROR is not None:
            self.fail("GOT replay config API is missing: {}".format(_IMPORT_ERROR))

    def test_largest_remainder_allocation_is_exact_and_deterministic(self):
        quotas = allocate_integer_quotas({"b": 1, "a": 1, "c": 1}, 10)
        self.assertEqual(quotas, {"a": 4, "b": 3, "c": 3})
        self.assertEqual(sum(quotas.values()), 10)

    def test_stage1_preserves_source_and_got_category_mix(self):
        quotas = build_stage_quotas("stage1", total_samples=40000)
        self.assertEqual(sum(quotas.values()), 40000)
        got = {key: value for key, value in quotas.items() if key.startswith("GOT_")}
        self.assertEqual(got, {
            "GOT_OTHER_NORMAL": 4000,
            "GOT_PERSON_NORMAL": 4000,
            "GOT_VEHICLE_NORMAL": 4000,
        })
        self.assertEqual(quotas["UAVDT_VEHICLE_NORMAL"], 3500)

    def test_stage2_preserves_normal_hard_and_got_mix(self):
        quotas = build_stage_quotas("stage2", total_samples=30000)
        self.assertEqual(sum(quotas.values()), 30000)
        got_total = sum(value for key, value in quotas.items() if key.startswith("GOT_"))
        hard_total = sum(value for key, value in quotas.items() if key.endswith("_HARD"))
        aerial_normal = 30000 - got_total - hard_total
        self.assertEqual((got_total, aerial_normal, hard_total), (7500, 15000, 7500))
        self.assertEqual(
            [quotas[name] for name in
             ("GOT_PERSON_NORMAL", "GOT_VEHICLE_NORMAL", "GOT_OTHER_NORMAL")],
            [2625, 2625, 2250],
        )

    def test_unknown_stage_is_rejected(self):
        with self.assertRaises(ValueError):
            build_stage_quotas("stage3", 10000)

    def test_stage_config_uses_frozen_batch_and_exact_stage_length(self):
        with tempfile.TemporaryDirectory() as directory:
            artifact_root = Path(directory) / "isolated-artifacts"
            config = build_stage_config(
                "stage2", batch_size=32, interval_samples=10016,
                start_lr=1.6e-5, end_lr=6e-6,
                artifact_root=artifact_root,
            )
        self.assertEqual(config["TRAIN"]["BATCH_SIZE"], 32)
        self.assertEqual(config["TRAIN"]["NUM_WORKERS"], 0)
        self.assertEqual(config["TRAIN"]["EPOCH"], 1)
        self.assertEqual(config["BACKBONE"]["TRAIN_EPOCH"], 2)
        self.assertEqual(config["DATASET"]["VIDEOS_PER_EPOCH"], 30048)
        names = config["DATASET"]["NAMES"]
        self.assertEqual(names, sorted(names))
        self.assertEqual(
            sum(config["DATASET"][name]["NUM_USE"] for name in names), 30048)
        self.assertTrue(all(config["DATASET"][name]["ANNO"].startswith("/home/u20/")
                            for name in names))
        self.assertEqual(config["TRAIN"]["LR"]["KWARGS"],
                         {"start_lr": 1.6e-5, "end_lr": 6e-6})
        self.assertEqual(
            config["TRAIN"]["SNAPSHOT_DIR"],
            str((artifact_root / "run/stage2").resolve()),
        )


if __name__ == "__main__":
    unittest.main()
