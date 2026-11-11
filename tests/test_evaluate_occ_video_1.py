import csv
import hashlib
import json
import math
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


try:
    from scripts import evaluate_occ_video_1 as evaluator
except ImportError as error:
    evaluator = None
    IMPORT_ERROR = error
else:
    IMPORT_ERROR = None


ROOT = Path(__file__).resolve().parents[1]


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write_json(path, value):
    Path(path).write_text(json.dumps(value, sort_keys=True) + "\n", encoding="utf-8")


def synthetic_contract(schema=10):
    truth = []
    for frame_id in range(11):
        status = "visible"
        observable = True
        if 3 <= frame_id <= 5:
            status = "full_occlusion" if frame_id >= 4 else "partial_occlusion"
            observable = False
        truth.append(
            {
                "frame_id": frame_id,
                "bbox": [10.0 + frame_id, 20.0, 10.0, 10.0],
                "status": status,
                "observable": observable,
            }
        )
    return {
        "identity": {"name": "synthetic", "frame_count": 11},
        "truth": truth,
        "event_frames": {
            "estimated_start": 3,
            "estimated_end": 5,
            "full_start": 4,
            "full_end": 5,
            "first_reliable_reappearance": 6,
            "stable_reappearance": 7,
            "reacquisition_deadline_exclusive": 190,
        },
        "required_tracking_schema": schema,
    }


def passing_rows(schema=10):
    rows = []
    for frame_id in range(11):
        searching = 3 <= frame_id <= 6
        rows.append(
            {
                "schema_version": schema,
                "frame_id": frame_id,
                "timestamp_ms": frame_id * 33,
                "state": "OCCLUSION_HOLD" if searching else "TRACKING",
                "tracking_phase": "SEARCHING" if searching else "TRACKING",
                "observation_valid": not searching,
                "control_source": "MOTION_SEARCH" if searching else "OBSERVATION",
                "control_valid": True,
                "accepted": not searching,
                "reacquired": frame_id == 7,
                "tracking_bbox": [10.0 + frame_id, 20.0, 10.0, 10.0],
            }
        )
    rows[6]["accepted"] = False
    rows[7]["accepted"] = True
    rows[7]["observation_valid"] = True
    rows[7]["tracking_phase"] = "TRACKING"
    rows[7]["control_source"] = "OBSERVATION"
    return rows


class OccVideoEvaluatorTest(unittest.TestCase):
    def setUp(self):
        if IMPORT_ERROR:
            self.fail("occ_video_1 evaluator API missing: {}".format(IMPORT_ERROR))

    def make_dataset(self, root, frame_count=3):
        root = Path(root)
        labels = root / "labels"
        labels.mkdir(parents=True, exist_ok=True)
        (root / "1.mp4").write_bytes(b"synthetic-video")
        write_json(
            root / "annotation_metadata.json",
            {
                "video": {"width": 100, "height": 50, "frame_count": frame_count},
                "target": {"class_id": 0, "class_name": "car"},
            },
        )
        write_json(
            root / "occlusion_events.json",
            {
                "estimated_bbox_frames": [1, 1],
                "full_occlusion_frames": [1, 1],
                "first_reliable_reappearance_frame": 2,
                "stable_reappearance_frame": 2,
            },
        )
        with (root / "annotation_status.csv").open("w", newline="", encoding="utf-8") as handle:
            writer = csv.DictWriter(handle, fieldnames=["frame_id", "status", "source"])
            writer.writeheader()
            for frame_id in range(frame_count):
                writer.writerow(
                    {
                        "frame_id": frame_id,
                        "status": "full_occlusion" if frame_id == 1 else "visible",
                        "source": "bidirectional_interpolation" if frame_id == 1 else "manual",
                    }
                )
        manifest_lines = []
        for frame_id in range(frame_count):
            path = labels / "frame_{:06d}.txt".format(frame_id)
            path.write_text("0 0.5 0.5 0.2 0.4\n", encoding="utf-8")
            manifest_lines.append("{}  labels/{}\n".format(sha256(path), path.name))
        (root / "labels.sha256").write_text("".join(manifest_lines), encoding="utf-8")
        return {
            "name": "fixture",
            "frame_count": frame_count,
            "width": 100,
            "height": 50,
            "class_id": 0,
            "hashes": {
                "video": sha256(root / "1.mp4"),
                "annotation_metadata": sha256(root / "annotation_metadata.json"),
                "occlusion_events": sha256(root / "occlusion_events.json"),
                "annotation_status": sha256(root / "annotation_status.csv"),
                "labels_manifest": sha256(root / "labels.sha256"),
            },
        }

    def test_load_contract_verifies_hashes_labels_and_frame_continuity(self):
        with tempfile.TemporaryDirectory() as directory:
            expected = self.make_dataset(directory)
            with mock.patch.object(evaluator, "CANONICAL_IDENTITY", expected):
                contract = evaluator.load_contract(Path(directory))
                truth = evaluator.load_truth(contract)
            self.assertEqual([row["frame_id"] for row in truth], [0, 1, 2])
            self.assertEqual(truth[0]["bbox"], [40.0, 15.0, 20.0, 20.0])
            self.assertTrue(truth[0]["observable"])
            self.assertFalse(truth[1]["observable"])
            self.assertEqual(contract["identity"]["hashes"], expected["hashes"])

    def test_load_contract_rejects_changed_input_and_noncontinuous_status(self):
        with tempfile.TemporaryDirectory() as directory:
            expected = self.make_dataset(directory)
            Path(directory, "1.mp4").write_bytes(b"changed")
            with mock.patch.object(evaluator, "CANONICAL_IDENTITY", expected):
                with self.assertRaisesRegex(ValueError, "video.*SHA-256"):
                    evaluator.load_contract(Path(directory))
            self.make_dataset(directory)
            status = Path(directory, "annotation_status.csv")
            lines = status.read_text(encoding="utf-8").splitlines()
            status.write_text("\n".join(lines[:2] + lines[3:]) + "\n", encoding="utf-8")
            expected["hashes"]["annotation_status"] = sha256(status)
            with mock.patch.object(evaluator, "CANONICAL_IDENTITY", expected):
                with self.assertRaisesRegex(ValueError, "continuous"):
                    evaluator.load_contract(Path(directory))

    def write_tracking_csv(self, path, rows):
        fields = [
            "schema_version",
            "frame_id",
            "timestamp_ms",
            "state",
            "tracking_phase",
            "observation_valid",
            "control_source",
            "control_valid",
            "accepted",
            "reacquired",
            "tracking_x",
            "tracking_y",
            "tracking_width",
            "tracking_height",
        ]
        with Path(path).open("w", newline="", encoding="utf-8") as handle:
            writer = csv.DictWriter(handle, fieldnames=fields)
            writer.writeheader()
            for row in rows:
                box = row["tracking_bbox"]
                serialized = {key: row[key] for key in fields if key in row}
                serialized.update(
                    tracking_x=box[0], tracking_y=box[1],
                    tracking_width=box[2], tracking_height=box[3]
                )
                for key in ("observation_valid", "control_valid", "accepted", "reacquired"):
                    serialized[key] = "1" if row[key] else "0"
                writer.writerow(serialized)

    def test_tracking_csv_parser_rejects_duplicate_nan_and_partial_boxes(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory, "rows.csv")
            rows = passing_rows()[:3]
            self.write_tracking_csv(path, rows)
            parsed = evaluator.load_tracking_csv(path)
            self.assertEqual([row["frame_id"] for row in parsed], [0, 1, 2])

            duplicate = rows + [dict(rows[-1])]
            self.write_tracking_csv(path, duplicate)
            with self.assertRaisesRegex(ValueError, "strictly continuous"):
                evaluator.load_tracking_csv(path)

            self.write_tracking_csv(path, rows)
            text = path.read_text(encoding="utf-8").replace("12.0,20.0", "nan,20.0")
            path.write_text(text, encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "finite"):
                evaluator.load_tracking_csv(path)

            self.write_tracking_csv(path, rows)
            text = path.read_text(encoding="utf-8").replace("12.0,20.0,10.0,10.0", "12.0,,10.0,10.0")
            path.write_text(text, encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "partial tracking box"):
                evaluator.load_tracking_csv(path)

    def test_tracking_csv_parser_reads_legacy_schema9_display_box_for_diagnostics(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory, "legacy.csv")
            path.write_text(
                "schema_version,frame_id,timestamp_ms,state,control_valid,accepted,reacquired,x,y,width,height\n"
                "9,0,0,TRACKING,1,1,0,12.5,20.0,10.0,11.0\n",
                encoding="utf-8",
            )
            try:
                rows = evaluator.load_tracking_csv(path)
            except ValueError as error:
                self.fail("legacy schema-9 diagnostic box was rejected: {}".format(error))
            self.assertEqual(rows[0]["tracking_bbox"], [12.5, 20.0, 10.0, 11.0])
            self.assertIsNone(rows[0]["tracking_phase"])

    def test_evaluate_separates_visible_and_estimated_frames_and_passes_semantics(self):
        report = evaluator.evaluate(synthetic_contract(), passing_rows(), "full")
        self.assertEqual(report["verdict"], "PASS")
        self.assertEqual(report["strata"]["visible"]["frames"], 8)
        self.assertEqual(report["strata"]["estimated_trajectory"]["frames"], 3)
        self.assertEqual(report["strata"]["visible"]["mean_iou"], 1.0)
        self.assertIn("pre_occlusion_visible", report["strata"])
        self.assertIn("post_reappearance_visible", report["strata"])
        self.assertEqual(report["strata"]["pre_occlusion_visible"], {"frames": 3, "mean_iou": 1.0})
        self.assertEqual(report["strata"]["post_reappearance_visible"], {"frames": 5, "mean_iou": 1.0})
        self.assertEqual(report["reacquisition"]["frames"], [7])
        self.assertEqual(report["reacquisition"]["first_frame"], 7)
        self.assertTrue(report["gates"]["full_occlusion_semantics"]["passed"])
        self.assertTrue(report["gates"]["prediction_continuity"]["passed"])
        self.assertEqual(
            [frame["frame_id"] for frame in report["frames"] if frame["ordinary_iou_allowed"]],
            [0, 1, 2, 6, 7, 8, 9, 10],
        )

    def test_evaluate_reports_visible_control_reacquisition_and_identity_failures(self):
        rows = passing_rows()
        rows[2]["tracking_bbox"] = [80.0, 20.0, 10.0, 10.0]
        rows[4]["control_valid"] = False
        rows[4]["control_source"] = "NONE"
        rows[5]["observation_valid"] = True
        rows[7]["reacquired"] = False
        rows[9]["reacquired"] = True
        report = evaluator.evaluate(synthetic_contract(), rows, "full")
        self.assertEqual(report["verdict"], "FAIL")
        self.assertFalse(report["gates"]["visible_boxes"]["passed"])
        self.assertFalse(report["gates"]["continuous_control"]["passed"])
        self.assertFalse(report["gates"]["full_occlusion_semantics"]["passed"])
        self.assertFalse(report["gates"]["identity_proxy"]["passed"])
        self.assertEqual(report["reacquisition"]["frames"], [9])
        self.assertIn(2, report["worst_frames"])

    def test_schema9_is_diagnostic_only_and_missing_new_semantics_cannot_pass(self):
        rows = passing_rows(schema=9)
        for row in rows:
            row.pop("tracking_phase")
            row.pop("observation_valid")
            row.pop("control_source")
        report = evaluator.evaluate(synthetic_contract(), rows, "full")
        self.assertEqual(report["verdict"], "FAIL")
        self.assertFalse(report["gates"]["tracking_schema"]["passed"])
        self.assertIn("legacy schema 9", report["gates"]["tracking_schema"]["detail"])
        self.assertGreater(report["strata"]["visible"]["mean_iou"], 0.99)

    def test_render_markdown_reports_scope_verdict_and_worst_frames(self):
        rows = passing_rows()
        rows[2]["tracking_bbox"] = [80.0, 20.0, 10.0, 10.0]
        report = evaluator.evaluate(synthetic_contract(), rows, "development")
        rendered = evaluator.render_markdown(report)
        self.assertIn("# occ_video_1 evaluation", rendered)
        self.assertIn("development", rendered)
        self.assertIn("FAIL", rendered)
        self.assertIn("frame 2", rendered)


class OccVideoEvaluatorCliTest(unittest.TestCase):
    def setUp(self):
        if IMPORT_ERROR:
            self.fail("occ_video_1 evaluator API missing: {}".format(IMPORT_ERROR))

    def test_cli_returns_two_for_identity_error_without_outputs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            boxes = root / "boxes.csv"
            boxes.write_text("schema_version,frame_id\n10,0\n", encoding="utf-8")
            output_json = root / "report.json"
            output_markdown = root / "report.md"
            result = subprocess.run(
                [
                    sys.executable,
                    str(ROOT / "scripts/evaluate_occ_video_1.py"),
                    "--dataset-root", str(root / "missing"),
                    "--boxes", str(boxes),
                    "--scope", "full",
                    "--output-json", str(output_json),
                    "--output-markdown", str(output_markdown),
                ],
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(result.returncode, 2)
            self.assertIn("ERROR", result.stderr)
            self.assertFalse(output_json.exists())
            self.assertFalse(output_markdown.exists())


if __name__ == "__main__":
    unittest.main()
