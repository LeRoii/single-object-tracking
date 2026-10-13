import unittest
from pathlib import Path


try:
    from scripts.run_nanotrack_got_replay import (
        build_metrics,
        candidate_training_command,
        composite_score,
        final_gate,
        hidden_evaluation_action,
        replacement_decision,
        target_score,
    )
except ImportError as error:
    _IMPORT_ERROR = error
else:
    _IMPORT_ERROR = None


def metrics(target=0.50, got=0.60, uav=0.48, person=0.49, vehicle=0.51):
    return {
        "domains": {
            "aerial_panel": {
                "person_auc": person,
                "vehicle_auc": vehicle,
                "overall_auc": target,
            },
            "uav123": {
                "person_auc": person + 0.01,
                "vehicle_auc": vehicle - 0.01,
                "overall_auc": target,
            },
        },
        "got_val_ao": got,
        "uav123_val_auc": uav,
    }


class GotReplayControllerTest(unittest.TestCase):
    def setUp(self):
        if _IMPORT_ERROR is not None:
            self.fail("GOT replay controller API is missing: {}".format(_IMPORT_ERROR))

    def test_target_score_normalizes_missing_categories_per_domain(self):
        candidate = metrics()
        candidate["domains"]["aerial_panel"]["person_auc"] = None
        expected_aerial = (0.4 * 0.51 + 0.2 * 0.50) / 0.6
        expected_uav = (0.4 * 0.50 + 0.4 * 0.50 + 0.2 * 0.50)
        self.assertAlmostEqual(target_score(candidate),
                               (expected_aerial + expected_uav) / 2.0)

    def test_composite_score_uses_70_15_15_weights(self):
        candidate = metrics(target=0.50, got=0.60, uav=0.40,
                            person=0.50, vehicle=0.50)
        self.assertAlmostEqual(composite_score(candidate),
                               0.70 * 0.50 + 0.15 * 0.60 + 0.15 * 0.40)

    def test_replacement_requires_gain_and_limits_generalization_drop(self):
        best = metrics(target=0.50, got=0.60, uav=0.48,
                       person=0.50, vehicle=0.50)
        improved = metrics(target=0.505, got=0.595, uav=0.475,
                           person=0.505, vehicle=0.505)
        self.assertTrue(replacement_decision(improved, best)["qualified"])
        too_small = metrics(target=0.501, got=0.60, uav=0.48,
                            person=0.501, vehicle=0.501)
        self.assertFalse(replacement_decision(too_small, best)["qualified"])
        got_drop = metrics(target=0.53, got=0.589, uav=0.48,
                           person=0.53, vehicle=0.53)
        self.assertFalse(replacement_decision(got_drop, best)["qualified"])

    def test_final_gate_enforces_target_generalization_and_categories(self):
        baseline = metrics(target=0.50, got=0.60, uav=0.48,
                           person=0.50, vehicle=0.50)
        passing = metrics(target=0.506, got=0.591, uav=0.471,
                          person=0.506, vehicle=0.506)
        self.assertTrue(final_gate(passing, baseline)["qualified"])
        category_drop = metrics(target=0.51, got=0.60, uav=0.48,
                                person=0.484, vehicle=0.53)
        self.assertFalse(final_gate(category_drop, baseline)["qualified"])

    def test_build_metrics_combines_three_evaluator_outputs(self):
        aerial = {"metrics": {"sources": {
            "UAVDT": {"person_auc": None, "vehicle_auc": 0.7,
                      "overall_auc": 0.7},
        }}}
        uav = {
            "overall": {"success_auc": 0.6},
            "categories": {
                "person": {"success_auc": 0.5},
                "vehicle": {"success_auc": 0.65},
            },
        }
        got = {"metrics": {"ao": 0.55, "sr_0_5": 0.52, "sr_0_75": 0.3}}
        combined = build_metrics(aerial, uav, got)
        self.assertEqual(combined["domains"]["UAVDT"]["vehicle_auc"], 0.7)
        self.assertEqual(combined["domains"]["UAV123"]["person_auc"], 0.5)
        self.assertEqual(combined["got_val_ao"], 0.55)
        self.assertEqual(combined["uav123_val_auc"], 0.6)
        self.assertIn("composite_score", combined)

    def test_training_command_uses_exact_boundaries_and_no_video(self):
        command = candidate_training_command(
            stage="stage1", seed=123456, candidate_index=2,
            interval_steps=313, config=Path("/tmp/stage1.yaml"),
            output_dir=Path("/tmp/run/stage1"),
            initial_checkpoint=Path("/tmp/initial.pth"),
            previous_candidate=Path("/tmp/candidate1.pth"),
            initial_global_step=0,
        )
        joined = " ".join(command)
        self.assertIn("--resume /tmp/candidate1.pth", joined)
        self.assertIn("--stop-after-global-step 626", joined)
        self.assertIn("--candidate-every-steps 313", joined)
        self.assertIn("--policy head_only", joined)
        self.assertIn("--lr-schedule step", joined)
        self.assertNotIn("video", joined.lower())

    def test_hidden_evaluation_gate_allows_exactly_one_invocation(self):
        selection = {"selection_final": True, "hidden_evaluation_started": False}
        self.assertEqual(hidden_evaluation_action(selection, False), "run")
        selection["hidden_evaluation_started"] = True
        with self.assertRaises(RuntimeError):
            hidden_evaluation_action(selection, False)
        self.assertEqual(hidden_evaluation_action(selection, True), "skip")
        with self.assertRaises(ValueError):
            hidden_evaluation_action({"selection_final": False}, False)


if __name__ == "__main__":
    unittest.main()
