import importlib.util
import json
import unittest
from pathlib import Path


SOURCE = Path(__file__).resolve().parents[1] / "evaluate_human_occlusion_constraints.py"
SPEC = importlib.util.spec_from_file_location("human_constraints", SOURCE)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class HumanOcclusionConstraintTests(unittest.TestCase):
    def test_visible_and_occlusion_status_are_distinct(self):
        row = {"frame_id": "42", "tracking_phase": "TRACKING", "accepted": "1",
               "observation_valid": "1", "observation_present": "1"}
        self.assertTrue(MODULE.evaluate(
            {"id": "visible", "type": "visible_observation", "frame": 42},
            {42: row}, {})["passed"])
        row["tracking_phase"] = "SUSPECTED"
        row["accepted"] = "0"
        self.assertFalse(MODULE.evaluate(
            {"id": "visible", "type": "visible_observation", "frame": 42},
            {42: row}, {})["passed"])
        self.assertTrue(MODULE.evaluate(
            {"id": "occluded", "type": "occlusion_status", "frame": 42},
            {42: row}, {})["passed"])

    def test_reacquisition_requires_same_frame_identity_and_staging(self):
        check = {"id": "recovered", "type": "reacquisition", "first_frame": 169,
                 "last_frame": 169, "minimum_iou": 0.5}
        row = {"frame_id": "169", "reacquired": "1", "accepted": "1",
               "observation_valid": "1", "recovery_reason": "RECOVERY_STAGED",
               "tracking_x": "50", "tracking_y": "50", "tracking_width": "10",
               "tracking_height": "10"}
        truth = {169: {"bbox_xywh": [0, 0, 10, 10]}}
        self.assertFalse(MODULE.evaluate(check, {169: row}, truth)["passed"])
        row["tracking_x"] = row["tracking_y"] = "0"
        self.assertTrue(MODULE.evaluate(check, {169: row}, truth)["passed"])
        row["recovery_reason"] = "NONE"
        self.assertFalse(MODULE.evaluate(check, {169: row}, truth)["passed"])

    def test_late_reacquisition_does_not_meet_deadline(self):
        check = {"id": "recovered", "type": "reacquisition", "first_frame": 169,
                 "last_frame": 169, "minimum_iou": 0.5}
        rows = {frame: {"frame_id": str(frame), "reacquired": "1" if frame == 171 else "0"}
                for frame in (169, 170, 171)}
        self.assertFalse(MODULE.evaluate(check, rows, {169: {"bbox_xywh": [0, 0, 10, 10]}})["passed"])

    def test_car010_deadline_accepts_correct_early_reacquisition(self):
        constraints = json.loads((SOURCE.parent / "occ_video_1_human_constraints_v1.json").read_text())
        check = next(item for item in constraints["targets"]["car_010"]
                     if item["type"] == "reacquisition")
        self.assertEqual((check["first_frame"], check["last_frame"]), (149, 169))
        rows = {frame: {"frame_id": str(frame), "reacquired": "0"}
                for frame in range(149, 170)}
        rows[168].update({"reacquired": "1", "accepted": "1",
                          "observation_valid": "1", "recovery_reason": "RECOVERY_STAGED",
                          "tracking_x": "0", "tracking_y": "0", "tracking_width": "10",
                          "tracking_height": "10"})
        self.assertTrue(MODULE.evaluate(check, rows, {168: {"bbox_xywh": [0, 0, 10, 10]}})["passed"])


if __name__ == "__main__":
    unittest.main()
