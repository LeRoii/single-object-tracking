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


def synthetic_contract(schema=12):
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
        "identity": {"name": "synthetic", "frame_count": 11, "width": 320, "height": 240},
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


def mark_recovery(row):
    row.update(
        reacquired=True,
        motion_control_authorized=False,
        rect0_source="ACCEPTED_OBSERVATION",
        accepted_observation_path="search_reacquisition",
        template_action="STAGE_RECENT",
        template_transition_applied=True,
        template_transition_recovery=True,
        template_transition_result="APPLIED",
        template_probation_active=True,
        template_transition_reason="RECOVERY_CONFIRMED",
        template_transaction_id=1,
        template_slot_id=1,
        recovery_reason="RECOVERY_STAGED",
        recovery_confirmation_hits=2,
        recovery_conflict=False,
        candidate_source="RECENT",
    )


def passing_rows(schema=12):
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
                "observation_present": not searching,
                "trusted_for_state_update": not searching,
                "motion_update_valid": not searching,
                "control_source": "MOTION_SEARCH" if searching else "OBSERVATION",
                "control_valid": True,
                "motion_control_authorized": searching,
                "motion_prediction_confident": searching,
                "rect0_source": "MOTION_PREDICTION" if searching else "ACCEPTED_OBSERVATION",
                "accepted": not searching,
                "reacquired": frame_id == 7,
                "tracking_bbox": [10.0 + frame_id, 20.0, 10.0, 10.0],
            }
        )
    mark_recovery(rows[7])
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

    def test_car001_source_precedes_review_status_for_estimated_frames(self):
        with tempfile.TemporaryDirectory() as directory:
            expected = self.make_dataset(directory)
            status_path = Path(directory, "annotation_status.csv")
            with status_path.open(newline="", encoding="utf-8") as handle:
                rows = list(csv.DictReader(handle))
            with status_path.open("w", newline="", encoding="utf-8") as handle:
                writer = csv.DictWriter(
                    handle,
                    fieldnames=["frame_id", "status", "source", "review_status"],
                )
                writer.writeheader()
                for row in rows:
                    row["review_status"] = "confirmed"
                    writer.writerow(row)
            expected["hashes"]["annotation_status"] = sha256(status_path)
            with mock.patch.object(evaluator, "CANONICAL_IDENTITY", expected):
                truth = evaluator.load_truth(evaluator.load_contract(Path(directory)))
            self.assertFalse(truth[1]["observable"])

    def test_load_contract_selects_car002_and_preserves_sparse_truth_semantics(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            package = root / "ground_truth" / "car_002"
            labels = package / "labels"
            estimated = package / "estimated_labels"
            labels.mkdir(parents=True)
            estimated.mkdir()
            (root / "1.mp4").write_bytes(b"shared-video")
            write_json(
                package / "annotation_metadata.json",
                {
                    "draft": False,
                    "formal_evaluation_authorized": True,
                    "unresolved_review_count": 0,
                    "video": {"width": 100, "height": 50, "frame_count": 4},
                },
            )
            write_json(
                package / "events.json",
                {
                    "events": [
                        {
                            "type": "occlusion",
                            "full_occlusion_frames": [1, 1],
                            "re_emergence_frames": [2, 2],
                            "first_full_object_candidate_frame": 3,
                        },
                        {"type": "runtime_observation"},
                    ]
                },
            )
            with (package / "annotation_status.csv").open(
                    "w", newline="", encoding="utf-8") as handle:
                writer = csv.DictWriter(
                    handle,
                    fieldnames=["frame_id", "target_state", "review_status"],
                )
                writer.writeheader()
                writer.writerows(
                    [
                        {"frame_id": 0, "target_state": "visible",
                         "review_status": "manual_confirmed_visible"},
                        {"frame_id": 1, "target_state": "full_occlusion",
                         "review_status": "estimated_non_observable"},
                        {"frame_id": 2, "target_state": "partial_occlusion",
                         "review_status": "manual_confirmed_no_bbox"},
                        {"frame_id": 3, "target_state": "visible",
                         "review_status": "manual_confirmed_visible"},
                    ]
                )
            label_manifest = []
            estimate_manifest = []
            for frame_id in range(4):
                label = labels / "frame_{:06d}.txt".format(frame_id)
                estimate = estimated / "frame_{:06d}.txt".format(frame_id)
                label.write_text(
                    "0 0.5 0.5 0.2 0.4\n" if frame_id in {0, 3} else "",
                    encoding="utf-8",
                )
                estimate.write_text(
                    "0 0.5 0.5 0.2 0.4\n" if frame_id == 1 else "",
                    encoding="utf-8",
                )
                label_manifest.append(
                    "{}  labels/{}\n".format(sha256(label), label.name)
                )
                estimate_manifest.append(
                    "{}  estimated_labels/{}\n".format(sha256(estimate), estimate.name)
                )
            (package / "labels.sha256").write_text(
                "".join(label_manifest), encoding="utf-8"
            )
            (package / "estimated_labels.sha256").write_text(
                "".join(estimate_manifest), encoding="utf-8"
            )
            expected = {
                "name": "fixture_car_002",
                "target_id": "car_002",
                "annotation_root": "ground_truth/car_002",
                "frame_count": 4,
                "width": 100,
                "height": 50,
                "class_id": 0,
                "confirmed_package": True,
                "files": {
                    "video": "1.mp4",
                    "annotation_metadata": "annotation_metadata.json",
                    "occlusion_events": "events.json",
                    "annotation_status": "annotation_status.csv",
                    "labels_manifest": "labels.sha256",
                    "estimated_labels_manifest": "estimated_labels.sha256",
                },
                "hashes": {
                    "video": sha256(root / "1.mp4"),
                    "annotation_metadata": sha256(package / "annotation_metadata.json"),
                    "occlusion_events": sha256(package / "events.json"),
                    "annotation_status": sha256(package / "annotation_status.csv"),
                    "labels_manifest": sha256(package / "labels.sha256"),
                    "estimated_labels_manifest": sha256(
                        package / "estimated_labels.sha256"
                    ),
                },
            }
            with mock.patch.dict(
                    evaluator.CANONICAL_IDENTITIES, {"car_002": expected}):
                contract = evaluator.load_contract(root, "car_002")
                truth = evaluator.load_truth(contract)
            self.assertEqual(contract["event_frames"]["first_reliable_reappearance"], 3)
            self.assertEqual(contract["event_frames"]["reacquisition_deadline_exclusive"], 4)
            self.assertTrue(truth[0]["observable"])
            self.assertTrue(truth[1]["estimated"])
            self.assertFalse(truth[1]["observable"])
            self.assertIsNone(truth[2]["bbox"])
            self.assertTrue(truth[3]["observable"])
            rows = []
            last_box = truth[0]["bbox"]
            for item in truth:
                box = item["bbox"] or last_box
                last_box = box
                searching = item["frame_id"] in {1, 2}
                rows.append(
                    {
                        "schema_version": 12,
                        "frame_id": item["frame_id"],
                        "timestamp_ms": item["frame_id"] * 33,
                        "state": "OCCLUSION_HOLD" if searching else "TRACKING",
                        "tracking_phase": "SEARCHING" if searching else "TRACKING",
                        "observation_valid": not searching,
                        "observation_present": not searching,
                        "trusted_for_state_update": not searching,
                        "motion_update_valid": not searching,
                        "control_source": "MOTION_SEARCH" if searching else "OBSERVATION",
                        "control_valid": True,
                        "motion_control_authorized": searching,
                        "motion_prediction_confident": searching,
                        "rect0_source": "MOTION_PREDICTION" if searching else "ACCEPTED_OBSERVATION",
                        "accepted": not searching,
                        "reacquired": item["frame_id"] == 3,
                        "tracking_bbox": box,
                    }
                )
            mark_recovery(rows[3])
            self.assertEqual(evaluator.evaluate(contract, rows, "full")["verdict"], "PASS")

    def write_tracking_csv(self, path, rows):
        fields = [
            "schema_version",
            "frame_id",
            "timestamp_ms",
            "state",
            "tracking_phase",
            "observation_valid",
            "observation_present",
            "trusted_for_state_update",
            "motion_update_valid",
            "control_source",
            "control_valid",
            "accepted",
            "reacquired",
            "motion_control_authorized",
            "motion_prediction_confident",
            "rect0_source",
            "accepted_observation_path",
            "template_action",
            "template_transition_applied",
            "template_transition_recovery",
            "template_transition_result",
            "template_probation_active",
            "template_transition_reason",
            "template_transaction_id",
            "template_slot_id",
            "recovery_reason",
            "recovery_confirmation_hits",
            "recovery_conflict",
            "candidate_source",
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
                for key in (
                    "observation_valid", "observation_present", "trusted_for_state_update",
                    "motion_update_valid",
                    "control_valid", "accepted", "reacquired",
                    "motion_control_authorized", "motion_prediction_confident",
                    "template_transition_applied", "template_transition_recovery",
                    "template_probation_active", "recovery_conflict",
                ):
                    if key in row:
                        serialized[key] = "1" if row[key] else "0"
                writer.writerow(serialized)

    def test_tracking_csv_parser_rejects_duplicate_nan_and_partial_boxes(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory, "rows.csv")
            rows = passing_rows()[:3]
            self.write_tracking_csv(path, rows)
            parsed = evaluator.load_tracking_csv(path)
            self.assertEqual([row["frame_id"] for row in parsed], [0, 1, 2])
            self.assertIs(parsed[0]["observation_present"], True)
            self.assertIs(parsed[0]["trusted_for_state_update"], True)

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

    def test_holdout_ignores_inapplicable_occlusion_gate_but_keeps_global_reacquisition(self):
        contract = synthetic_contract()
        rows = passing_rows()
        for frame_id in range(11, 191):
            bbox = [10.0 + frame_id, 20.0, 10.0, 10.0]
            contract["truth"].append(
                {
                    "frame_id": frame_id,
                    "bbox": bbox,
                    "status": "visible",
                    "observable": True,
                }
            )
            rows.append(
                {
                    "schema_version": 12,
                    "frame_id": frame_id,
                    "timestamp_ms": frame_id * 33,
                    "state": "TRACKING",
                    "tracking_phase": "TRACKING",
                    "observation_valid": True,
                    "observation_present": True,
                    "trusted_for_state_update": True,
                    "motion_update_valid": True,
                    "control_source": "OBSERVATION",
                    "control_valid": True,
                    "accepted": True,
                    "reacquired": False,
                    "tracking_bbox": bbox,
                }
            )
        contract["identity"]["frame_count"] = 191

        passing = evaluator.evaluate(contract, rows, "holdout")

        self.assertEqual(passing["frame_range"], [190, 190])
        self.assertEqual(passing["verdict"], "PASS")
        self.assertFalse(passing["gates"]["full_occlusion_semantics"]["applicable"])
        self.assertIn("N/A", evaluator.render_markdown(passing))

        mark_recovery(rows[190])
        failing = evaluator.evaluate(contract, rows, "holdout")

        self.assertEqual(failing["verdict"], "FAIL")
        self.assertTrue(failing["gates"]["reacquisition"]["applicable"])
        self.assertFalse(failing["gates"]["reacquisition"]["passed"])
        self.assertEqual(failing["reacquisition"]["frames"], [7, 190])

    def test_evaluate_reports_visible_control_reacquisition_and_identity_failures(self):
        rows = passing_rows()
        rows[2]["tracking_bbox"] = [80.0, 20.0, 10.0, 10.0]
        rows[4]["control_valid"] = False
        rows[4]["control_source"] = "NONE"
        rows[5]["observation_valid"] = True
        rows[7]["reacquired"] = False
        mark_recovery(rows[9])
        report = evaluator.evaluate(synthetic_contract(), rows, "full")
        self.assertEqual(report["verdict"], "FAIL")
        self.assertFalse(report["gates"]["visible_boxes"]["passed"])
        self.assertFalse(report["gates"]["continuous_control"]["passed"])
        self.assertFalse(report["gates"]["full_occlusion_semantics"]["passed"])
        self.assertFalse(report["gates"]["identity_proxy"]["passed"])
        self.assertEqual(report["reacquisition"]["frames"], [9])
        self.assertIn(2, report["worst_frames"])

    def test_search_prediction_is_not_scored_as_observation_or_identity_claim(self):
        contract = synthetic_contract()
        for item in contract["truth"]:
            if item["frame_id"] >= 6:
                item["bbox"] = [30.0 + item["frame_id"] - 6, 20.0, 10.0, 10.0]
        rows = passing_rows()
        # Frame 6 is visible again, but remains an explicitly invalid search
        # prediction.  The two-hit commit at frame 7 is the first identity
        # claim and is allowed to atomically switch back to the truth box.
        rows[6]["tracking_bbox"] = [16.0, 20.0, 10.0, 10.0]
        for frame_id in range(7, 11):
            rows[frame_id]["tracking_bbox"] = [
                30.0 + frame_id - 6, 20.0, 10.0, 10.0
            ]

        report = evaluator.evaluate(contract, rows, "full")

        self.assertEqual(report["verdict"], "PASS")
        self.assertTrue(report["gates"]["visible_boxes"]["passed"])
        self.assertTrue(report["gates"]["prediction_continuity"]["passed"])
        self.assertTrue(report["gates"]["identity_proxy"]["passed"])
        frame6 = next(item for item in report["frames"] if item["frame_id"] == 6)
        self.assertTrue(frame6["ordinary_iou_allowed"])
        self.assertFalse(frame6["observation_valid"])
        self.assertLess(frame6["iou"], 0.10)

    def test_visible_mean_uses_absolute_floor_while_robustness_owns_relative_drop(self):
        rows = passing_rows()
        for row in rows:
            if row["observation_valid"]:
                row["tracking_bbox"][2] = 14.0
        passing = evaluator.evaluate(synthetic_contract(), rows, "full")
        self.assertTrue(passing["gates"]["visible_boxes"]["passed"])
        self.assertGreater(passing["strata"]["observation_visible"]["mean_iou"], 0.70)

        for row in rows:
            if row["observation_valid"]:
                row["tracking_bbox"][2] = 15.0
        failing = evaluator.evaluate(synthetic_contract(), rows, "full")
        self.assertFalse(failing["gates"]["visible_boxes"]["passed"])

    def test_visible_per_frame_floor_has_margin_away_from_half_iou_knife_edge(self):
        rows = passing_rows()
        rows[2]["tracking_bbox"][0] += 3.6
        passing = evaluator.evaluate(synthetic_contract(), rows, "full")
        self.assertGreater(passing["frames"][2]["iou"], 0.45)
        self.assertTrue(passing["gates"]["visible_boxes"]["passed"])

        rows[2]["tracking_bbox"][0] += 0.5
        failing = evaluator.evaluate(synthetic_contract(), rows, "full")
        self.assertLess(failing["frames"][2]["iou"], 0.45)
        self.assertFalse(failing["gates"]["visible_boxes"]["passed"])

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

    def test_candidate_rejects_archived_v10_and_missing_current_semantics(self):
        archived = passing_rows(schema=10)
        for row in archived:
            row.pop("observation_present")
            row.pop("trusted_for_state_update")
            row.pop("motion_update_valid")
        self.assertEqual(evaluator.evaluate(synthetic_contract(schema=10), archived, "full")["verdict"], "PASS")
        candidate = evaluator.evaluate(synthetic_contract(), archived, "full")
        self.assertEqual(candidate["verdict"], "FAIL")
        self.assertFalse(candidate["gates"]["tracking_schema"]["passed"])

        rows = passing_rows()
        rows[0].pop("trusted_for_state_update")
        self.assertFalse(evaluator.evaluate(synthetic_contract(), rows, "full")["gates"]["tracking_schema"]["passed"])

    def test_candidate_rejects_metadata_schema_mismatch(self):
        contract = synthetic_contract()
        contract["tracking_session_metadata"] = {"schema_version": 10}
        report = evaluator.evaluate(contract, passing_rows(), "full")
        self.assertFalse(report["gates"]["tracking_schema"]["passed"])

    def test_current_recovery_counts_only_trusted_tracking_with_applied_recovery_stage(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory, "rows.csv")
            self.write_tracking_csv(path, passing_rows())
            report = evaluator.evaluate(synthetic_contract(), evaluator.load_tracking_csv(path), "full")
        self.assertTrue(report["gates"]["reacquisition"]["passed"])
        self.assertEqual(report["reacquisition"]["frames"], [7])
        for changes in (
            {"state": "LOCAL_RECOVERY", "tracking_phase": "SEARCHING"},
            {"trusted_for_state_update": False},
            {"motion_update_valid": False},
            {"observation_present": False},
            {"observation_valid": False},
            {"accepted": False},
            {"state": "LOST"},
            {"control_valid": False},
            {"control_source": "MOTION_SEARCH"},
            {"motion_control_authorized": True},
            {"tracking_bbox": [17.0, 20.0, 0.0, 10.0]},
            {"tracking_bbox": [-1.0, 20.0, 10.0, 10.0]},
            {"tracking_bbox": [319.0, 20.0, 10.0, 10.0]},
            {"template_transition_applied": False},
            {"template_transition_recovery": False},
            {"template_transition_result": "TRACKER_REJECTED"},
            {"template_action": "COMMIT_RECENT", "recovery_reason": "RECOVERY_COMMITTED"},
            {"template_slot_id": 2},
            {"recovery_confirmation_hits": 1},
            {"recovery_conflict": True},
        ):
            with self.subTest(changes=changes):
                rows = passing_rows()
                rows[7].update(changes)
                report = evaluator.evaluate(synthetic_contract(), rows, "full")
                self.assertFalse(report["gates"]["reacquisition"]["passed"])
                self.assertEqual(report["gates"]["reacquisition"]["frames"], [7])
                self.assertEqual(report["reacquisition"]["frames"], [])

    def test_bare_recovery_flag_cannot_exempt_identity_jump(self):
        rows = passing_rows()
        rows[7]["reacquired"] = False
        rows[8]["reacquired"] = True
        rows[8]["tracking_bbox"][0] += 6.0
        report = evaluator.evaluate(synthetic_contract(), rows, "full")
        self.assertFalse(report["gates"]["reacquisition"]["passed"])
        self.assertEqual(report["reacquisition"]["frames"], [])
        self.assertIn(8, report["gates"]["identity_proxy"]["frames"])

    def test_lost_rejects_authorized_motion_and_claimed_observation(self):
        for changes in (
            {"control_valid": True, "control_source": "MOTION_SEARCH"},
            {"control_valid": True, "control_source": "OBSERVATION"},
            {"motion_control_authorized": True},
            {"observation_present": True},
            {"trusted_for_state_update": True, "accepted": True, "observation_valid": True},
        ):
            with self.subTest(changes=changes):
                rows = passing_rows()
                rows[0].update(
                    state="LOST", tracking_phase="SEARCHING", control_valid=False,
                    control_source="NONE", observation_present=False,
                    trusted_for_state_update=False, accepted=False, observation_valid=False,
                    motion_update_valid=False,
                )
                clean = evaluator.evaluate(synthetic_contract(), rows, "full")
                self.assertTrue(clean["gates"]["continuous_control"]["passed"])
                rows[0].update(changes)
                report = evaluator.evaluate(synthetic_contract(), rows, "full")
                self.assertEqual(report["gates"]["continuous_control"]["frames"], [0])

    def test_full_occlusion_attributes_false_observation_and_unauthorized_motion(self):
        for changes in (
            {"observation_present": True},
            {"accepted": True},
            {"trusted_for_state_update": True},
            {"motion_update_valid": True},
            {"motion_control_authorized": False},
            {"motion_prediction_confident": False},
            {"rect0_source": "RECENT_OBSERVATION"},
        ):
            with self.subTest(changes=changes):
                rows = passing_rows()
                rows[4].update(changes)
                report = evaluator.evaluate(synthetic_contract(), rows, "full")
                gate = report["gates"]["full_occlusion_semantics"]
                self.assertFalse(gate["passed"])
                self.assertEqual(gate["frames"], [4])

    def test_tentative_control_requires_present_untrusted_observation_and_preserves_identity(self):
        rows = passing_rows()
        tentative = rows[2]
        tentative.update(
            state="SUSPECTED", tracking_phase="SUSPECTED",
            observation_valid=False, trusted_for_state_update=False, accepted=False,
            motion_update_valid=False,
        )
        self.assertEqual(evaluator.evaluate(synthetic_contract(), rows, "full")["verdict"], "PASS")

        tentative["tracking_bbox"] = [80.0, 20.0, 10.0, 10.0]
        report = evaluator.evaluate(synthetic_contract(), rows, "full")
        self.assertFalse(report["gates"]["identity_proxy"]["passed"])
        self.assertIn(2, report["gates"]["identity_proxy"]["frames"])

        tentative["observation_present"] = False
        report = evaluator.evaluate(synthetic_contract(), rows, "full")
        self.assertFalse(report["gates"]["continuous_control"]["passed"])
        self.assertIn(2, report["gates"]["continuous_control"]["frames"])

    def test_render_markdown_reports_scope_verdict_and_worst_frames(self):
        rows = passing_rows()
        rows[2]["tracking_bbox"] = [80.0, 20.0, 10.0, 10.0]
        report = evaluator.evaluate(synthetic_contract(), rows, "development")
        rendered = evaluator.render_markdown(report)
        self.assertIn("# occ_video_1 evaluation", rendered)
        self.assertIn("development", rendered)
        self.assertIn("FAIL", rendered)
        self.assertIn("frame 2", rendered)

    def test_attach_provenance_requires_complete_hex_identities(self):
        report = {"schema_version": 1}
        enriched = evaluator.attach_provenance(
            report, "a" * 40, "b" * 64, "c" * 64
        )
        self.assertEqual(
            enriched["provenance"],
            {
                "source_commit": "a" * 40,
                "config_sha256": "b" * 64,
                "model_sha256": "c" * 64,
            },
        )
        self.assertNotIn("provenance", report)
        with self.assertRaisesRegex(ValueError, "provided together"):
            evaluator.attach_provenance(report, "a" * 40, None, "c" * 64)
        with self.assertRaisesRegex(ValueError, "source_commit"):
            evaluator.attach_provenance(report, "not-a-commit", "b" * 64, "c" * 64)


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
