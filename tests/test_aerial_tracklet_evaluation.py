import hashlib
import importlib
import inspect
import json
import os
import subprocess
import sys
import tempfile
import types
import unittest
from pathlib import Path
from unittest import mock

import cv2
import numpy as np

from scripts.evaluate_uav123_nanotrack import evaluate_predictions as evaluate_uav123
from scripts.nanotrack_three_stage_data import AerialSequenceSpec


class MovingTracker:
    def __init__(self):
        self.box = None
        self.init_calls = 0
        self.track_calls = 0

    def init(self, image, box):
        self.init_calls += 1
        self.box = list(box)

    def track(self, image):
        self.track_calls += 1
        self.box[0] += 1.0
        return {"bbox": list(self.box)}


class AerialTrackletEvaluationTest(unittest.TestCase):
    def setUp(self):
        try:
            self.evaluator = importlib.import_module(
                "scripts.evaluate_aerial_tracklets_nanotrack"
            )
        except ModuleNotFoundError as error:
            self.fail("aerial tracklet evaluator is missing: {}".format(error))

    def test_perfect_and_disjoint_ope_endpoints(self):
        perfect = [(0.0, 0.0, 10.0, 10.0), (2.0, 0.0, 10.0, 10.0)]
        disjoint = [(100.0, 100.0, 10.0, 10.0)] * 2

        self.assertEqual(self.evaluator.ope_success_auc(perfect, perfect), 20.0 / 21.0)
        self.assertEqual(self.evaluator.ope_success_auc(perfect, disjoint), 0.0)
        self.assertEqual(
            self.evaluator.OPE_METADATA,
            {
                "threshold_start": 0.0,
                "threshold_end": 1.0,
                "threshold_step": 0.05,
                "thresholds": [index / 20.0 for index in range(21)],
                "comparator": ">",
                "auc_reduction": "mean",
                "perfect_endpoint_auc": 20.0 / 21.0,
            },
        )

    def test_ope_matches_uav123_for_all_inputs_including_perfect(self):
        ground_truth = [
            (0.0, 0.0, 10.0, 10.0),
            (0.0, 0.0, 10.0, 10.0),
            (0.0, 0.0, 10.0, 10.0),
        ]
        predictions = [
            (0.0, 0.0, 10.0, 10.0),
            (5.0, 0.0, 10.0, 10.0),
            (100.0, 100.0, 10.0, 10.0),
        ]

        for candidate in (predictions, ground_truth):
            aerial_auc = self.evaluator.ope_success_auc(ground_truth, candidate)
            uav_auc = evaluate_uav123(ground_truth, candidate)["success_auc"]
            self.assertEqual(abs(aerial_auc - uav_auc), 0.0)

    def test_missing_gt_is_tracked_through_but_excluded_from_scoring(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            paths = []
            for index in range(4):
                path = root / "{:04d}.jpg".format(index)
                cv2.imwrite(str(path), np.zeros((16, 24, 3), dtype=np.uint8))
                paths.append(str(path))
            spec = AerialSequenceSpec(
                name="track",
                source="domain-a",
                source_video="video-a",
                category="person",
                image_paths=tuple(paths),
                boxes_xywh=(None, (5.0, 6.0, 4.0, 5.0), None, (7.0, 6.0, 4.0, 5.0)),
                window_start=10,
                window_end=13,
            )
            tracker = MovingTracker()

            result = self.evaluator.run_sequence(spec, tracker)

        self.assertEqual(tracker.init_calls, 1)
        self.assertEqual(tracker.track_calls, 2)
        self.assertEqual(result["predicted_xywh"], [None, [5.0, 6.0, 4.0, 5.0], [6.0, 6.0, 4.0, 5.0], [7.0, 6.0, 4.0, 5.0]])
        self.assertEqual(result["scored_frame_mask"], [False, True, False, True])
        self.assertEqual(result["scored_frame_count"], 2)
        self.assertEqual(result["sequence_auc"], 20.0 / 21.0)

    def test_sequence_and_domain_macros_ignore_frame_and_domain_size(self):
        results = [
            {"name": "long", "source": "large", "category": "person", "sequence_auc": 1.0, "scored_frame_count": 1000},
            {"name": "short", "source": "large", "category": "person", "sequence_auc": 0.0, "scored_frame_count": 1},
            {"name": "only", "source": "small", "category": "person", "sequence_auc": 1.0, "scored_frame_count": 1},
        ]

        summary = self.evaluator.aggregate_domain_category_metrics(results)

        self.assertEqual(summary["sources"]["large"]["person_auc"], 0.5)
        self.assertEqual(summary["sources"]["large"]["overall_auc"], 0.5)
        self.assertEqual(summary["macro"]["overall_auc"], 0.75)

    def test_source_category_and_cross_domain_aggregation(self):
        results = [
            {"name": "a-person", "source": "a", "category": "person", "sequence_auc": 1.0, "scored_frame_count": 2},
            {"name": "a-vehicle", "source": "a", "category": "vehicle", "sequence_auc": 0.0, "scored_frame_count": 9},
            {"name": "b-vehicle", "source": "b", "category": "vehicle", "sequence_auc": 0.5, "scored_frame_count": 20},
        ]

        summary = self.evaluator.aggregate_domain_category_metrics(results)

        self.assertEqual(summary["sources"]["a"]["categories"], {"person": 1.0, "vehicle": 0.0})
        self.assertEqual(summary["sources"]["b"]["person_auc"], None)
        self.assertEqual(summary["macro"]["person_auc"], 1.0)
        self.assertEqual(summary["macro"]["vehicle_auc"], 0.25)
        self.assertEqual(summary["macro"]["overall_auc"], 0.5)

    def test_missing_category_weights_are_renormalized(self):
        score = self.evaluator.renormalized_weighted_score(
            {"person_auc": None, "vehicle_auc": 0.5, "overall_auc": 0.25},
            {"person_auc": 0.4, "vehicle_auc": 0.4, "overall_auc": 0.2},
        )

        self.assertAlmostEqual(score, (0.4 * 0.5 + 0.2 * 0.25) / 0.6)

    def test_panel_hash_loading_and_atomic_output_are_stable(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            image_path = root / "0001.jpg"
            cv2.imwrite(str(image_path), np.zeros((16, 24, 3), dtype=np.uint8))
            panel_path = root / "panel.json"
            panel_value = {
                "split": "validation",
                "members": ["one"],
                "sequences": [{
                    "name": "one", "source": "a", "source_video": "v", "category": "person",
                    "image_paths": [str(image_path)], "boxes_xywh": [[1, 2, 3, 4]],
                    "window_start": 1, "window_end": 1,
                }],
            }
            panel_path.write_text(json.dumps(panel_value, sort_keys=True), encoding="utf-8")
            expected_hash = hashlib.sha256(panel_path.read_bytes()).hexdigest()

            panel = self.evaluator.load_panel(panel_path, expected_hash)
            config = root / "config.yaml"
            snapshot = root / "snapshot.pth"
            config.write_bytes(b"config\n")
            snapshot.write_bytes(b"snapshot\n")
            value = self.evaluator.evaluate_panel(
                panel, MovingTracker, config, snapshot, "cpu"
            )
            self.assertIn("config_sha256", value)
            self.assertIn("snapshot_sha256", value)
            self.assertEqual(
                value["config_sha256"], hashlib.sha256(b"config\n").hexdigest()
            )
            self.assertEqual(
                value["snapshot_sha256"], hashlib.sha256(b"snapshot\n").hexdigest()
            )
            self.assertEqual(value["panel_sha256"], expected_hash)
            self.assertIn("ope", value)
            self.assertEqual(value["ope"], self.evaluator.OPE_METADATA)
            output = root / "result.json"
            self.evaluator.atomic_write_json(output, value)
            first = output.read_bytes()
            self.evaluator.atomic_write_json(output, value)
            second = output.read_bytes()

        self.assertEqual(panel.sha256, expected_hash)
        self.assertEqual(first, second)

    def test_panel_rejects_duplicate_malformed_empty_and_hash_mismatch(self):
        valid = {
            "split": "validation",
            "members": ["same", "same"],
            "sequences": [{
                "name": "same", "source": "a", "source_video": "v", "category": "person",
                "image_paths": ["a.jpg"], "boxes_xywh": [[0, 0, 3, 4]],
                "window_start": 1, "window_end": 1,
            }] * 2,
        }
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "panel.json"
            path.write_text(json.dumps(valid), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "duplicate"):
                self.evaluator.load_panel(path)
            valid["members"] = ["same"]
            valid["sequences"] = [dict(valid["sequences"][0], boxes_xywh=[[0, 0, -1, 4]])]
            path.write_text(json.dumps(valid), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "box"):
                self.evaluator.load_panel(path)
            valid["sequences"] = [dict(valid["sequences"][0], boxes_xywh=[None])]
            path.write_text(json.dumps(valid), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "scoreable"):
                self.evaluator.load_panel(path)
            with self.assertRaisesRegex(ValueError, "hash mismatch"):
                self.evaluator.load_panel(path, "0" * 64)

            valid["split"] = ""
            valid["sequences"] = [dict(valid["sequences"][0], boxes_xywh=[[0, 0, 3, 4]])]
            path.write_text(json.dumps(valid), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "split"):
                self.evaluator.load_panel(path)

    def test_panel_hash_and_json_are_derived_from_one_byte_buffer(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "panel.json"
            panel_value = {
                "split": "validation",
                "members": ["one"],
                "sequences": [{
                    "name": "one", "source": "a", "source_video": "v", "category": "person",
                    "image_paths": ["0001.jpg"], "boxes_xywh": [[1, 2, 3, 4]],
                    "window_start": 1, "window_end": 1,
                }],
            }
            original = json.dumps(panel_value).encode("utf-8")
            path.write_bytes(original)

            def mutate_after_hash(target):
                payload = Path(target).read_bytes()
                Path(target).write_bytes(b"not json")
                return hashlib.sha256(payload).hexdigest()

            with mock.patch.object(
                self.evaluator, "sha256_file", side_effect=mutate_after_hash
            ):
                try:
                    panel = self.evaluator.load_panel(path)
                except ValueError as error:
                    self.fail("panel hash and JSON used separate reads: {}".format(error))

        self.assertEqual(panel.sha256, hashlib.sha256(original).hexdigest())

    def test_tracker_factory_resolves_relative_inputs_before_temporary_chdir(self):
        captured = {}
        fake_torch = types.ModuleType("torch")
        fake_torch.cuda = types.SimpleNamespace(is_available=lambda: False)
        fake_torch.device = lambda value: value
        fake_config = types.ModuleType("nanotrack.core.config")
        fake_config.cfg = types.SimpleNamespace(
            merge_from_file=lambda path: captured.setdefault("config", path)
        )
        fake_builder = types.ModuleType("nanotrack.models.model_builder")

        class FakeModel:
            def to(self, device):
                return self

            def eval(self):
                return self

        fake_builder.ModelBuilder = FakeModel
        fake_tracker = types.ModuleType("nanotrack.tracker.tracker_builder")
        fake_tracker.build_tracker = lambda model: object()
        fake_load = types.ModuleType("nanotrack.utils.model_load")

        def load_pretrain(model, path):
            captured["snapshot"] = path
            return model

        fake_load.load_pretrain = load_pretrain
        modules = {
            "torch": fake_torch,
            "nanotrack": types.ModuleType("nanotrack"),
            "nanotrack.core": types.ModuleType("nanotrack.core"),
            "nanotrack.core.config": fake_config,
            "nanotrack.models": types.ModuleType("nanotrack.models"),
            "nanotrack.models.model_builder": fake_builder,
            "nanotrack.tracker": types.ModuleType("nanotrack.tracker"),
            "nanotrack.tracker.tracker_builder": fake_tracker,
            "nanotrack.utils": types.ModuleType("nanotrack.utils"),
            "nanotrack.utils.model_load": fake_load,
        }
        original_cwd = Path.cwd()
        try:
            with tempfile.TemporaryDirectory() as temp_dir:
                root = Path(temp_dir)
                invocation = root / "invocation"
                nanotrack = root / "nanotrack-repo"
                invocation.mkdir()
                nanotrack.mkdir()
                os.chdir(str(invocation))
                with mock.patch.dict(sys.modules, modules):
                    self.evaluator._tracker_factory(
                        Path("../nanotrack-repo"), Path("config.yaml"),
                        Path("snapshot.pth"), "cpu",
                    )
                self.assertEqual(Path.cwd(), invocation)
                self.assertEqual(captured["config"], str(invocation / "config.yaml"))
                self.assertEqual(captured["snapshot"], str(invocation / "snapshot.pth"))
        finally:
            os.chdir(str(original_cwd))

    def test_cli_resolves_paths_and_hashes_models_before_loading(self):
        self.assertIn("argv", inspect.signature(self.evaluator.main).parameters)
        events = []
        captured = {}
        panel = self.evaluator.AerialPanel("validation", tuple(), "panel-hash", "panel.json")

        def fake_hash(path):
            events.append(("hash", Path(path)))
            return "hash-" + Path(path).name

        def fake_factory(root, config, snapshot, device):
            events.append(("factory", Path(root), Path(config), Path(snapshot)))
            return lambda: object()

        def fake_evaluate(loaded_panel, factory, config, snapshot, device, **hashes):
            captured["evaluate"] = (Path(config), Path(snapshot), hashes)
            return {"metrics": {}}

        def fake_write(path, value):
            captured["output"] = Path(path)

        original_cwd = Path.cwd()
        try:
            with tempfile.TemporaryDirectory() as temp_dir:
                invocation = Path(temp_dir)
                os.chdir(str(invocation))
                with mock.patch.object(self.evaluator, "load_panel", return_value=panel), \
                     mock.patch.object(self.evaluator, "sha256_file", side_effect=fake_hash), \
                     mock.patch.object(self.evaluator, "_tracker_factory", side_effect=fake_factory), \
                     mock.patch.object(self.evaluator, "evaluate_panel", side_effect=fake_evaluate), \
                     mock.patch.object(self.evaluator, "atomic_write_json", side_effect=fake_write):
                    self.evaluator.main([
                        "--panel", "panel.json", "--nanotrack-root", "nano",
                        "--config", "config.yaml", "--snapshot", "snapshot.pth",
                        "--output", "out/result.json", "--device", "cpu",
                    ])

                self.assertEqual(
                    [event[0] for event in events], ["hash", "hash", "factory"]
                )
                factory_event = events[-1]
                self.assertEqual(factory_event[1], invocation / "nano")
                self.assertEqual(factory_event[2], invocation / "config.yaml")
                self.assertEqual(factory_event[3], invocation / "snapshot.pth")
                self.assertEqual(captured["output"], invocation / "out/result.json")
                self.assertEqual(
                    captured["evaluate"][2],
                    {
                        "config_sha256": "hash-config.yaml",
                        "snapshot_sha256": "hash-snapshot.pth",
                    },
                )
        finally:
            os.chdir(str(original_cwd))

    def test_missing_or_unreadable_image_fails(self):
        spec = AerialSequenceSpec(
            "missing", "a", "v", "person", ("/definitely/missing.jpg",),
            ((0.0, 0.0, 3.0, 4.0),), 1, 1,
        )
        with self.assertRaisesRegex(RuntimeError, "cannot decode"):
            self.evaluator.run_sequence(spec, MovingTracker())

    def test_direct_cli_help_works_outside_workspace(self):
        workspace = Path(__file__).resolve().parents[1]
        script = workspace / "scripts/evaluate_aerial_tracklets_nanotrack.py"
        with tempfile.TemporaryDirectory() as temp_dir:
            completed = subprocess.run(
                [sys.executable, str(script), "--help"], cwd=temp_dir,
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True,
            )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertIn("--panel", completed.stdout)
        self.assertIn("--device", completed.stdout)


if __name__ == "__main__":
    unittest.main()
