import json
import math
import tempfile
import unittest
from pathlib import Path


try:
    from scripts.nanotrack_got_replay_training import (
        accepted_memory_sample,
        batch_candidates,
        candidate_interval,
        choose_largest_safe_batch,
        calibrate_batches,
        freeze_calibration,
        scaled_learning_rate,
    )
except ImportError as error:
    _IMPORT_ERROR = error
else:
    _IMPORT_ERROR = None


class GotReplayTrainingPolicyTest(unittest.TestCase):
    def setUp(self):
        if _IMPORT_ERROR is not None:
            self.fail("GOT replay training policy API is missing: {}".format(_IMPORT_ERROR))

    def test_batch_candidates_are_bounded_powers_of_two(self):
        self.assertEqual(batch_candidates(16, 1024),
                         (16, 32, 64, 128, 256, 512, 1024))
        with self.assertRaises(ValueError):
            batch_candidates(12, 1024)

    def test_memory_gate_requires_ratio_margin_and_finite_losses(self):
        accepted = {"batch_size": 128, "total_mib": 6144, "peak_reserved_mib": 5000,
                    "minimum_free_mib": 900, "finite_losses": True,
                    "completed_steps": 50, "oom": False}
        self.assertTrue(accepted_memory_sample(accepted))
        for key, value in (("peak_reserved_mib", 5500),
                           ("minimum_free_mib", 700),
                           ("finite_losses", False),
                           ("completed_steps", 49),
                           ("oom", True)):
            sample = dict(accepted)
            sample[key] = value
            self.assertFalse(accepted_memory_sample(sample), key)

    def test_largest_safe_batch_stops_at_first_failed_size(self):
        records = [
            {"batch_size": 16, "accepted": True},
            {"batch_size": 32, "accepted": True},
            {"batch_size": 64, "accepted": False},
            {"batch_size": 128, "accepted": True},
        ]
        self.assertEqual(choose_largest_safe_batch(records), 32)
        with self.assertRaises(ValueError):
            choose_largest_safe_batch([{"batch_size": 16, "accepted": False}])

    def test_lr_uses_sqrt_scaling_capped_at_four(self):
        self.assertAlmostEqual(scaled_learning_rate(2e-5, 8), 2e-5)
        self.assertAlmostEqual(scaled_learning_rate(2e-5, 32), 4e-5)
        self.assertAlmostEqual(scaled_learning_rate(2e-5, 512), 8e-5)

    def test_candidate_interval_preserves_samples_and_minimum_updates(self):
        self.assertEqual(candidate_interval(8), {"samples": 10000, "steps": 1250})
        self.assertEqual(candidate_interval(64), {"samples": 10048, "steps": 157})
        self.assertEqual(candidate_interval(256), {"samples": 25600, "steps": 100})
        for batch in (16, 32, 64, 128, 256, 512, 1024):
            result = candidate_interval(batch)
            self.assertEqual(result["samples"] % batch, 0)
            self.assertGreaterEqual(result["samples"], 10000)
            self.assertGreaterEqual(result["steps"], 100)

    def test_calibration_stops_at_first_rejected_candidate(self):
        visited = []

        def probe(batch_size):
            visited.append(batch_size)
            return {
                "batch_size": batch_size,
                "total_mib": 6144,
                "peak_reserved_mib": 4000 if batch_size <= 32 else 5600,
                "minimum_free_mib": 1000 if batch_size <= 32 else 500,
                "finite_losses": True,
                "completed_steps": 50,
                "oom": False,
            }

        records = calibrate_batches(probe, candidates=(16, 32, 64, 128))
        self.assertEqual(visited, [16, 32, 64])
        self.assertEqual([record["accepted"] for record in records],
                         [True, True, False])
        self.assertEqual(choose_largest_safe_batch(records), 32)

    def test_freeze_calibration_writes_reproducible_training_values(self):
        records = [
            {"batch_size": 16, "accepted": True},
            {"batch_size": 32, "accepted": True},
            {"batch_size": 64, "accepted": False},
        ]
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "calibration.json"
            frozen = freeze_calibration(records, output)
            persisted = json.loads(output.read_text(encoding="utf-8"))
        self.assertEqual(persisted, frozen)
        self.assertEqual(frozen["selected_batch_size"], 32)
        self.assertEqual(frozen["candidate_interval"],
                         {"samples": 10016, "steps": 313})
        self.assertAlmostEqual(frozen["stage1_lr"]["start"], 4e-5)
        self.assertAlmostEqual(frozen["stage2_lr"]["end"], 6e-6)


if __name__ == "__main__":
    unittest.main()
