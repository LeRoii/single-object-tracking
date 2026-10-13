import subprocess
import sys
import unittest
from pathlib import Path


class GotReplayReportingTest(unittest.TestCase):
    def test_verifier_cli_accepts_isolated_config_directory(self):
        script = Path(__file__).resolve().parents[1] / "scripts/verify_nanotrack_got_replay.py"
        result = subprocess.run(
            [sys.executable, str(script), "--help"],
            capture_output=True, text=True, check=True,
        )
        self.assertIn("--config-dir", result.stdout)

    def test_no_candidate_outcome_is_valid_without_hidden_evaluation(self):
        from scripts.verify_nanotrack_got_replay import classify_selection_outcome

        table = {
            "candidates": [
                {
                    "key": "seed{}_stage1_c{}".format(seed, index),
                    "seed": seed,
                    "final_gate": {"qualified": False},
                }
                for seed in (123456, 20261011)
                for index in range(1, 5)
            ],
            "seeds": {
                "123456": {"stage1_best": None, "stage2_skipped": "no qualified stage1 candidate"},
                "20261011": {"stage1_best": None, "stage2_skipped": "no qualified stage1 candidate"},
            },
        }
        selection = {
            "selection_final": False,
            "selected": None,
            "eligible_keys": [],
            "hidden_evaluation_started": False,
        }

        outcome = classify_selection_outcome(selection, table, final_exists=False)

        self.assertEqual(outcome["outcome"], "no_candidate_passed")
        self.assertFalse(outcome["hidden_evaluation_completed"])
        self.assertIsNone(outcome["selected_key"])

    def test_no_candidate_outcome_rejects_hidden_access(self):
        from scripts.verify_nanotrack_got_replay import classify_selection_outcome

        selection = {
            "selection_final": False,
            "selected": None,
            "eligible_keys": [],
            "hidden_evaluation_started": True,
        }
        table = {"candidates": [], "seeds": {}}

        with self.assertRaisesRegex(ValueError, "hidden evaluation"):
            classify_selection_outcome(selection, table, final_exists=False)

    def test_no_candidate_report_retains_baseline_and_states_hidden_boundary(self):
        from scripts.report_nanotrack_got_replay import render_reports

        prep = {
            "sequence_count": 8852,
            "selected_frame_count": 424790,
            "pool_sequence_counts": {"person": 1, "vehicle": 2, "other": 3},
        }
        calibration = {
            "selected_batch_size": 64,
            "candidate_interval": {"samples": 10048},
            "records": [{
                "accepted": True, "batch_size": 64, "peak_reserved_mib": 508.0,
                "minimum_free_mib": 4107.0, "completed_steps": 50,
            }],
        }
        candidate = {
            "key": "seed20261011_stage1_c1",
            "metrics": {
                "target_score": 0.743256,
                "got_val_ao": 0.696074,
                "uav123_val_auc": 0.729855,
                "composite_score": 0.734169,
            },
            "replacement": {"qualified": False},
            "final_gate": {"qualified": False},
        }
        candidates = {"candidates": [candidate]}
        selection = {
            "baseline": {
                "target_score": 0.743261,
                "got_val_ao": 0.694762,
                "uav123_val_auc": 0.725587,
                "composite_score": 0.733335,
            },
            "selection_final": False,
            "selected": None,
            "eligible_keys": [],
            "hidden_evaluation_started": False,
        }

        english, chinese = render_reports(
            prep, calibration, candidates, selection, final=None
        )

        self.assertIn("baseline is retained", english)
        self.assertIn("was not accessed", english)
        self.assertIn("保留初始化基线模型", chinese)
        self.assertIn("未访问", chinese)
        self.assertNotIn("| no |", chinese)


if __name__ == "__main__":
    unittest.main()
