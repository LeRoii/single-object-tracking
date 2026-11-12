import csv
import json
import subprocess
import sys
import tempfile
import unittest
from copy import deepcopy
from pathlib import Path

try:
    from scripts import check_c012_effect_baseline as baseline
except ImportError as error:
    baseline = None
    IMPORT_ERROR = error
else:
    IMPORT_ERROR = None


ROOT = Path(__file__).resolve().parents[1]


def contract():
    return {
        "schema_version": 1,
        "candidate_id": "C012",
        "targets": {
            "car_001": {
                "initial_bbox": [992, 582, 37, 39],
                "required_last_frame": 270,
                "reacquisition_deadline_inclusive": 185,
                "forbid_search_from_frame": 190,
                "control_valid_through_frame": 270,
            },
            "car_002": {
                "initial_bbox": [970, 517, 38, 38],
                "road_sign_continuity_frames": [60, 80],
            },
        },
    }


def rows(target):
    last = 270 if target == "car_001" else 241
    roi = contract()["targets"][target]["initial_bbox"]
    result = []
    for frame_id in range(last + 1):
        result.append({
            "frame_id": str(frame_id),
            "tracking_phase": "SEARCHING" if target == "car_001" and 169 <= frame_id < 185 else "TRACKING",
            "control_valid": "1",
            "accepted": "0" if target == "car_001" and 169 <= frame_id < 185 else "1",
            "reacquired": "1" if target == "car_001" and frame_id == 185 else "0",
            "x": str(roi[0]), "y": str(roi[1]),
            "width": str(roi[2]), "height": str(roi[3]),
        })
    return result


def write_csv(path, records):
    with Path(path).open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(records[0]))
        writer.writeheader()
        writer.writerows(records)


class C012EffectBaselineTest(unittest.TestCase):
    def setUp(self):
        if IMPORT_ERROR:
            self.fail("C012 baseline checker missing: {}".format(IMPORT_ERROR))

    def test_c012_behavior_passes(self):
        result = baseline.evaluate(contract(), rows("car_001"), rows("car_002"))
        self.assertEqual(result["verdict"], "PASS")
        self.assertEqual(result["car_001"]["first_reacquisition_frame"], 185)
        self.assertEqual(result["car_002"]["road_sign_failures"], [])

    def test_car001_late_reacquisition_search_and_control_regressions_fail(self):
        late = rows("car_001")
        late[185]["reacquired"] = "0"
        late[186]["reacquired"] = "1"
        with self.assertRaisesRegex(ValueError, "worse than C012"):
            baseline.evaluate(contract(), late, rows("car_002"))

        later_search = rows("car_001")
        later_search[200]["tracking_phase"] = "SEARCHING"
        with self.assertRaisesRegex(ValueError, "searches after"):
            baseline.evaluate(contract(), later_search, rows("car_002"))

        invalid_control = rows("car_001")
        invalid_control[248]["control_valid"] = "0"
        with self.assertRaisesRegex(ValueError, "loses control"):
            baseline.evaluate(contract(), invalid_control, rows("car_002"))

    def test_car002_road_sign_and_initialization_regressions_fail(self):
        road_sign = rows("car_002")
        road_sign[70]["tracking_phase"] = "SEARCHING"
        road_sign[70]["accepted"] = "0"
        with self.assertRaisesRegex(ValueError, "road-sign continuity"):
            baseline.evaluate(contract(), rows("car_001"), road_sign)

        wrong_roi = rows("car_002")
        wrong_roi[0]["x"] = "967"
        wrong_roi[0]["y"] = "513"
        wrong_roi[0]["width"] = "42"
        wrong_roi[0]["height"] = "42"
        with self.assertRaisesRegex(ValueError, "does not match C012"):
            baseline.evaluate(contract(), rows("car_001"), wrong_roi)

    def test_frame_gaps_and_short_runs_fail(self):
        gap = rows("car_002")
        del gap[70]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "gap.csv"
            write_csv(path, gap)
            with self.assertRaisesRegex(ValueError, "contiguous"):
                baseline._load_rows(path, "car_002")

        short = rows("car_002")[:70]
        with self.assertRaisesRegex(ValueError, "must cover"):
            baseline.evaluate(contract(), rows("car_001"), short)

    def test_cli_writes_passing_json(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            contract_path = root / "contract.json"
            car001 = root / "car001.csv"
            car002 = root / "car002.csv"
            output = root / "result.json"
            contract_path.write_text(json.dumps(contract()), encoding="utf-8")
            write_csv(car001, rows("car_001"))
            write_csv(car002, rows("car_002"))
            completed = subprocess.run(
                [sys.executable, str(ROOT / "scripts/check_c012_effect_baseline.py"),
                 "--contract", str(contract_path), "--car001-boxes", str(car001),
                 "--car002-boxes", str(car002), "--output-json", str(output)],
                check=False, capture_output=True, text=True,
            )
            self.assertEqual(completed.returncode, 0, completed.stderr)
            self.assertEqual(json.loads(output.read_text())["verdict"], "PASS")


if __name__ == "__main__":
    unittest.main()
