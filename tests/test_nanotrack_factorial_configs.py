"""Exact, independently checked expectations for the nine factorial cells."""

import json
import tempfile
import unittest
from pathlib import Path

import yaml

from scripts.generate_nanotrack_factorial_configs import generate_configs


ASSET_ROOT = Path("/home/u20/single-object-tracking")
EXPECTED = {
    0: {
        "UAV123_BIKE_NORMAL": 476, "UAV123_OTHER_NORMAL": 1426,
        "UAV123_PERSON_NORMAL": 4754, "UAV123_VEHICLE_NORMAL": 2852,
        "UAVDT_VEHICLE_NORMAL": 13166,
        "VISDRONE_MOT_BIKE_NORMAL": 1317,
        "VISDRONE_MOT_PERSON_NORMAL": 7899,
        "VISDRONE_MOT_VEHICLE_NORMAL": 3950,
        "VISDRONE_SOT_UNKNOWN_NORMAL": 5120,
    },
    10: {
        "GOT_OTHER_NORMAL": 1366, "GOT_PERSON_NORMAL": 1365,
        "GOT_VEHICLE_NORMAL": 1365,
        "UAV123_BIKE_NORMAL": 428, "UAV123_OTHER_NORMAL": 1284,
        "UAV123_PERSON_NORMAL": 4279, "UAV123_VEHICLE_NORMAL": 2567,
        "UAVDT_VEHICLE_NORMAL": 11849,
        "VISDRONE_MOT_BIKE_NORMAL": 1185,
        "VISDRONE_MOT_PERSON_NORMAL": 7109,
        "VISDRONE_MOT_VEHICLE_NORMAL": 3555,
        "VISDRONE_SOT_UNKNOWN_NORMAL": 4608,
    },
    30: {
        "GOT_OTHER_NORMAL": 4096, "GOT_PERSON_NORMAL": 4096,
        "GOT_VEHICLE_NORMAL": 4096,
        "UAV123_BIKE_NORMAL": 333, "UAV123_OTHER_NORMAL": 998,
        "UAV123_PERSON_NORMAL": 3328, "UAV123_VEHICLE_NORMAL": 1997,
        "UAVDT_VEHICLE_NORMAL": 9216,
        "VISDRONE_MOT_BIKE_NORMAL": 922,
        "VISDRONE_MOT_PERSON_NORMAL": 5529,
        "VISDRONE_MOT_VEHICLE_NORMAL": 2765,
        "VISDRONE_SOT_UNKNOWN_NORMAL": 3584,
    },
}


class FactorialConfigTest(unittest.TestCase):
    def test_nine_saved_configs_have_exact_quotas_and_sample_clock(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            generate_configs(output, ASSET_ROOT)
            manifest = json.loads((output / "experiment-manifest.json").read_text())
            self.assertEqual(len(manifest["cells"]), 9)
            self.assertEqual(manifest["screening_seeds"], [123456, 20261011])
            self.assertEqual(manifest["confirmation_seeds"], [123456, 20261011, 20261024])
            self.assertEqual(manifest["scoring_version"], "nanotrack-category-first-v3")
            self.assertEqual(manifest["input_hashes"]["pair_policy"],
                             "e33eb9423b08879b6b6f9068711e1617298a6fceb4a5ca4241f6a5bd45338024")
            self.assertEqual(manifest["input_hashes"]["pair_audit"],
                             "69b4d5d253ef40894f34a3648af31872e5676a40ce8d8a2f98a44cf04adb1ca2")
            self.assertEqual(manifest["input_hashes"]["initial_checkpoint"],
                             "40ebbe4296938bf5d53c8605ab05e729b306fc0bce65921a57ab6613030410d1")
            self.assertEqual(manifest["input_hashes"]["source_config"],
                             "cb3065982818a701a7ebb469c41b894b0a6c11e7aac6cb91dc42e5ed50547f2e")
            self.assertEqual(manifest["source_config"], str(ASSET_ROOT / "configs/nanotrack_v3_three_stage_stage1.yaml"))
            self.assertEqual(manifest["output_root"], str(ASSET_ROOT / "artifacts/nanotrack_factorial_v3"))
            for batch, updates, interval in ((8, 5120, 320), (32, 1280, 80), (64, 640, 40)):
                for got in (0, 10, 30):
                    key = "b{}_g{}".format(batch, got)
                    with self.subTest(cell=key):
                        cell = manifest["cells"][key]
                        path = output / (key + ".yaml")
                        config = yaml.safe_load(path.read_text())
                        quotas = {name: config["DATASET"][name]["NUM_USE"]
                                  for name in config["DATASET"]["NAMES"]}
                        self.assertEqual(quotas, EXPECTED[got])
                        self.assertEqual(cell["quotas"], EXPECTED[got])
                        self.assertEqual(sum(quotas.values()), 40960)
                        self.assertEqual(config["DATASET"]["VIDEOS_PER_EPOCH"], 40960)
                        self.assertEqual(config["TRAIN"]["BATCH_SIZE"], batch)
                        self.assertEqual(cell["full_updates"], updates)
                        self.assertEqual(cell["evaluation_interval_updates"], interval)
                        self.assertEqual(cell["screening_updates"], updates // 4)
                        self.assertEqual(cell["full_samples"], 40960)
                        self.assertEqual(cell["screening_samples"], 10240)
                        self.assertEqual(cell["evaluation_interval_samples"], 2560)
                        self.assertEqual(config["TRAIN"]["LR"]["KWARGS"],
                                         {"start_lr": 2e-5, "end_lr": 8e-6})
                        self.assertEqual(config["TRAIN"]["BASE_LR"], 2e-5)
                        self.assertEqual(cell["learning_rate"],
                                         {"start": 2e-5, "end": 8e-6, "schedule": "log_by_samples"})
                        self.assertEqual(cell["input_hashes"], manifest["input_hashes"])
                        self.assertEqual(cell["pair_policy_path"], manifest["input_paths"]["pair_policy"])
                        self.assertEqual(cell["initial_checkpoint"], manifest["input_paths"]["initial_checkpoint"])
                        self.assertEqual(cell["config_sha256"], __import__("hashlib").sha256(path.read_bytes()).hexdigest())
                        self.assertEqual(cell["output_dir"], str(ASSET_ROOT / "artifacts/nanotrack_factorial_v3/run" / key))
                        for name in quotas:
                            entry = config["DATASET"][name]
                            self.assertTrue(entry["ROOT"].startswith(str(ASSET_ROOT / "data") + "/"))
                            self.assertTrue(entry["ANNO"].startswith(str(ASSET_ROOT / "data") + "/"))
                        self.assertFalse(got == 0 and any(name.startswith("GOT_") for name in config["DATASET"]))

    def test_existing_generated_file_cannot_be_silently_replaced(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            generate_configs(output, ASSET_ROOT)
            target = output / "b8_g0.yaml"
            target.write_text("changed\n")
            with self.assertRaises(ValueError):
                generate_configs(output, ASSET_ROOT)


if __name__ == "__main__":
    unittest.main()
