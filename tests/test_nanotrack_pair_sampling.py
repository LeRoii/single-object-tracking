import hashlib
import importlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
import torch

from scripts import train_nanotrack_uav123 as training


ROOT = Path(__file__).resolve().parents[1]


def pair_api(test):
    try:
        return importlib.import_module("scripts.nanotrack_pair_sampling")
    except ImportError as error:
        test.fail("missing controlled pair-policy API: {}".format(error))


class PairSamplingTest(unittest.TestCase):
    def test_sparse_original_ids_exclude_far_frame_but_positions_keep_it(self):
        api = pair_api(self)
        self.assertEqual(api.eligible_search_frames([1, 50, 200], 50, 60,
                                                   "original_frame_id"), [1, 50])
        self.assertEqual(api.eligible_search_frames([1, 50, 200], 50, 60,
                                                   "list_position"), [1, 50, 200])

    def test_boundary_is_inclusive_and_template_remains_eligible_at_zero(self):
        api = pair_api(self)
        self.assertEqual(api.eligible_search_frames([1, 50, 110, 111], 50, 60,
                                                   "original_frame_id"), [1, 50, 110])
        self.assertEqual(api.eligible_search_frames([1, 50, 200], 200, 0,
                                                   "original_frame_id"), [200])
        self.assertEqual(api.eligible_search_frames([1, 50, 200], 1, 1,
                                                   "list_position"), [1, 50])

    def test_invalid_pair_arguments_fail_instead_of_silently_changing_sampling(self):
        api = pair_api(self)
        for frames, template, radius, mode in (
            ([], 1, 60, "original_frame_id"),
            ([1, 50], 2, 60, "original_frame_id"),
            ([1, 50], 1, -1, "original_frame_id"),
            ([1, 50], 1, 1.5, "original_frame_id"),
            ([50, 1], 1, 60, "original_frame_id"),
            ([1, 1], 1, 60, "original_frame_id"),
            ([1, 50], 1, 60, "unknown"),
        ):
            with self.subTest(frames=frames, mode=mode), self.assertRaises(ValueError):
                api.eligible_search_frames(frames, template, radius, mode)

    @staticmethod
    def fake_class():
        class SubDataset:
            def __init__(self, name):
                self.name = name
                self.frame_range = 60
                self.videos = ["video"]
                self.labels = {"video": {"00": {"frames": [1, 50, 200]}}}

            def get_positive_pair(self, index):
                return ("historical", self.name, index)

            def get_image_anno(self, video, track, frame):
                return video, track, frame
        return SubDataset

    def test_adapter_changes_only_matching_got_and_preserves_numpy_state_stream(self):
        api = pair_api(self)
        cls = self.fake_class()
        api.install_pair_policy(cls, {"GOT_PERSON_": "original_frame_id"})
        for name in ("UAV123_PERSON_NORMAL", "GOT_VEHICLE_NORMAL", "NOT_GOT_PERSON_"):
            self.assertEqual(cls(name).get_positive_pair(0), ("historical", name, 0))
        state = np.random.get_state()
        try:
            dataset = cls("GOT_PERSON_NORMAL")
            np.random.seed(23)
            pairs = [dataset.get_positive_pair(0) for _ in range(80)]
            after = np.random.randint(100000)
            np.random.seed(23)
            expected = []
            # Independent hand-derived candidate sets; share the trainer's stream.
            candidates = {1: [1, 50], 50: [1, 50], 200: [200]}
            for _ in range(80):
                np.random.choice(["00"])
                template = [1, 50, 200][np.random.randint(0, 3)]
                search = np.random.choice(candidates[template])
                expected.append((("video", "00", template), ("video", "00", search)))
            self.assertEqual(pairs, expected)
            self.assertEqual(after, np.random.randint(100000))
            self.assertEqual({pair[0][2] for pair in pairs}, {1, 50, 200})
        finally:
            np.random.set_state(state)

    def test_reinstall_replaces_policy_without_leaking_previous_scope(self):
        api = pair_api(self)
        cls = self.fake_class()
        api.install_pair_policy(cls, {"GOT_PERSON_": "original_frame_id"})
        api.install_pair_policy(cls, {"GOT_VEHICLE_": "original_frame_id"})
        self.assertEqual(cls("GOT_PERSON_NORMAL").get_positive_pair(0),
                         ("historical", "GOT_PERSON_NORMAL", 0))

    def test_unknown_and_aerial_policy_fail_before_installation(self):
        api = pair_api(self)
        cls = self.fake_class()
        for policy in ({}, [], {"GOT_": "position"}, {"UAV": "original_frame_id"},
                       {"": "original_frame_id"}, {"GOT": "original_frame_id"}):
            with self.subTest(policy=policy), self.assertRaises(ValueError):
                api.install_pair_policy(cls, policy)
        self.assertEqual(cls("GOT_NORMAL").get_positive_pair(0),
                         ("historical", "GOT_NORMAL", 0))

    def test_file_hash_binds_exact_policy_bytes_and_rejects_duplicate_keys(self):
        api = pair_api(self)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "policy.json"
            raw = b'{"GOT_": "original_frame_id"}\n'
            path.write_bytes(raw)
            identity = api.load_pair_policy(path)
            self.assertEqual(identity["sha256"], hashlib.sha256(raw).hexdigest())
            self.assertEqual(identity["policy"], {"GOT_": "original_frame_id"})
            self.assertEqual(identity["path"], str(path.resolve()))
            path.write_text('{"GOT_":"original_frame_id","GOT_":"original_frame_id"}')
            with self.assertRaises(ValueError):
                api.load_pair_policy(path)


class TrainerPairPolicyTest(unittest.TestCase):
    def test_main_installs_policy_and_records_provenance_before_dataset_construction(self):
        pair_api(self)
        from tests.test_nanotrack_training import FakePolicyModel
        cls = PairSamplingTest.fake_class()
        class DatasetConstructed(Exception):
            pass
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            policy = root / "policy.json"
            policy.write_text('{"GOT_":"original_frame_id"}')
            expected_hash = hashlib.sha256(policy.read_bytes()).hexdigest()
            def construct_dataset():
                self.assertNotEqual(cls("GOT_PERSON_NORMAL").get_positive_pair(0),
                                    ("historical", "GOT_PERSON_NORMAL", 0))
                provenance = json.loads((root / "run/pair-policy-provenance.json").read_text())
                self.assertEqual(provenance["pair_policy"]["sha256"], expected_hash)
                self.assertEqual(provenance["pair_policy"]["policy"], {"GOT_": "original_frame_id"})
                raise DatasetConstructed()
            cfg = SimpleNamespace(
                merge_from_file=lambda path: None, dump=lambda: "test-config",
                TRAIN=SimpleNamespace(BASE_LR=0.01, MOMENTUM=0.9, WEIGHT_DECAY=0.0),
                BACKBONE=SimpleNamespace(LAYERS_LR=0.1))
            modules = {
                "nanotrack.core.config": SimpleNamespace(cfg=cfg),
                "nanotrack.datasets.dataset": SimpleNamespace(BANDataset=construct_dataset, SubDataset=cls),
                "nanotrack.models.model_builder": SimpleNamespace(ModelBuilder=FakePolicyModel),
                "nanotrack.utils.model_load": SimpleNamespace(load_pretrain=lambda model, path: None),
            }
            argv = ["train", "--config", str(root / "config"), "--nanotrack-root", str(root),
                    "--pretrained", str(root / "weights"), "--output-dir", str(root / "run"),
                    "--policy", "head_only", "--pair-policy", str(policy)]
            original_path = list(sys.path)
            original_cwd = Path.cwd()
            try:
                with patch.dict(sys.modules, modules), patch.object(sys, "argv", argv), \
                        patch.object(torch.cuda, "is_available", return_value=True), \
                        patch.object(torch.nn.Module, "cuda", lambda self: self), \
                        patch.object(training, "_seed_everything"), self.assertRaises(DatasetConstructed):
                    training.main()
            finally:
                sys.path[:] = original_path
                import os
                os.chdir(str(original_cwd))

    def cli(self, *extra):
        return subprocess.run([
            sys.executable, "-B", str(ROOT / "scripts/train_nanotrack_uav123.py"),
            "--config", "/nonexistent/config", "--nanotrack-root", "/nonexistent/root",
            "--pretrained", "/nonexistent/model", "--output-dir", "/nonexistent/out",
        ] + list(extra), capture_output=True, text=True)

    def test_cli_rejects_invalid_policy_before_external_import_or_training(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "policy.json"
            for payload in ('{"GOT_":"unknown"}', '{"UAV":"original_frame_id"}', 'oops'):
                path.write_text(payload)
                result = self.cli("--pair-policy", str(path))
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("pair-policy", result.stderr)
                self.assertNotIn("unrecognized arguments", result.stderr)
                self.assertNotIn("No module named 'nanotrack'", result.stderr)

    def test_hash_is_recorded_in_periodic_checkpoints_and_restored(self):
        pair_api(self)
        model = torch.nn.Linear(2, 1)
        optimizer = torch.optim.SGD(model.parameters(), lr=0.01)
        with tempfile.TemporaryDirectory() as directory:
            paths = training.save_periodic_checkpoints(
                Path(directory), model, optimizer, stage="test", epoch=0,
                step_in_epoch=250, global_step=250, policy="head_only",
                metrics_position=250, candidate_checkpoint_steps=250,
                pair_policy_sha256="a" * 64,
            )
            for path in paths.values():
                self.assertEqual(torch.load(str(path), map_location="cpu")["pair_policy_sha256"],
                                 "a" * 64)
            progress = training.restore_training_checkpoint(
                model, optimizer, paths["resume"], pair_policy_sha256="a" * 64)
            self.assertEqual(progress["pair_policy_sha256"], "a" * 64)

    def test_resume_hash_mismatch_fails_before_model_optimizer_rng_or_metrics_changes(self):
        pair_api(self)
        model = torch.nn.Linear(2, 1)
        optimizer = torch.optim.SGD(model.parameters(), lr=0.01)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "resume.pth"
            training.save_training_checkpoint(
                path, model, optimizer, completed_epochs=0, step_in_epoch=0,
                global_step=1, mean_epoch_loss=1., seed=23, config="test",
                pair_policy_sha256="a" * 64)
            with torch.no_grad():
                model.weight.fill_(12.)
            metric = Path(directory) / "metrics.jsonl"
            metric.write_text('{"global_step": 2}\n')
            for requested in (None, "b" * 64):
                with self.subTest(requested=requested), self.assertRaisesRegex(ValueError, "pair-policy"):
                    training.restore_training_checkpoint(
                        model, optimizer, path, metrics_path=metric,
                        pair_policy_sha256=requested)
            self.assertTrue(torch.equal(model.weight, torch.tensor([[12., 12.]])))
            self.assertEqual(metric.read_text(), '{"global_step": 2}\n')

    def test_legacy_checkpoint_remains_compatible_but_cannot_resume_under_new_policy(self):
        pair_api(self)
        model = torch.nn.Linear(2, 1)
        optimizer = torch.optim.SGD(model.parameters(), lr=0.01)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "legacy.pth"
            torch.save({"model": model.state_dict(), "optimizer": optimizer.state_dict(),
                        "epoch": 0}, str(path))
            self.assertEqual(training.restore_training_checkpoint(model, optimizer, path)["epoch"], 0)
            with self.assertRaisesRegex(ValueError, "pair-policy"):
                training.restore_training_checkpoint(model, optimizer, path,
                                                      pair_policy_sha256="a" * 64)

    def test_no_policy_preserves_historical_metadata_shape(self):
        model = torch.nn.Linear(2, 1)
        optimizer = torch.optim.SGD(model.parameters(), lr=0.01)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "legacy.pth"
            saved = training.save_training_checkpoint(
                path, model, optimizer, completed_epochs=0, step_in_epoch=0,
                global_step=1, mean_epoch_loss=1., seed=23, config="test")
            self.assertNotIn("pair_policy_sha256", saved)
            self.assertNotIn("pair_policy_sha256",
                             training.restore_training_checkpoint(model, optimizer, path))

    def test_cli_resume_hash_mismatch_precedes_external_import(self):
        pair_api(self)
        with tempfile.TemporaryDirectory() as directory:
            policy = Path(directory) / "policy.json"
            policy.write_text('{"GOT_":"original_frame_id"}')
            resume = Path(directory) / "resume.pth"
            torch.save({"global_step": 250, "pair_policy_sha256": "a" * 64}, str(resume))
            result = self.cli("--pair-policy", str(policy), "--resume", str(resume))
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("pair-policy", result.stderr)
            self.assertNotIn("No module named 'nanotrack'", result.stderr)


class FramePairAuditTest(unittest.TestCase):
    def test_audit_exhaustively_counts_sparse_pairs_and_hashes_input(self):
        try:
            audit = importlib.import_module("scripts.audit_nanotrack_frame_pairs")
        except ImportError as error:
            self.fail("missing frame-pair audit: {}".format(error))
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            manifest = root / "train.json"
            manifest.write_text(json.dumps({"sequence": {"00": {
                "000001": [0, 0, 2, 2], "000050": [0, 0, 2, 2],
                "000200": [0, 0, 2, 2], "000300": [0, 0, 0, 0], "category": "person"}}}))
            policy = root / "policy.json"
            policy.write_text('{"GOT_":"original_frame_id"}\n')
            output = root / "audit"
            result = subprocess.run([
                sys.executable, "-B", str(ROOT / "scripts/audit_nanotrack_frame_pairs.py"),
                "--manifest", "GOT_PERSON_NORMAL=" + str(manifest), "--frame-range", "60",
                "--pair-policy", str(policy), "--output-dir", str(output),
            ], text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            report = json.loads((output / "audit.json").read_text())
            row = report["manifests"][0]
            self.assertEqual(row["manifest_path"], str(manifest.resolve()))
            self.assertEqual(row["manifest_sha256"], hashlib.sha256(manifest.read_bytes()).hexdigest())
            self.assertEqual((row["sequence_count"], row["track_count"], row["frame_count"]), (1, 1, 3))
            self.assertEqual(row["list_position"]["eligible_candidate_count"], 9)
            self.assertEqual(row["original_frame_id"]["eligible_candidate_count"], 5)
            # Old sorted gaps: 0,0,0,49,49,150,150,199,199.
            self.assertEqual(row["list_position"]["frame_gap"],
                             {"p50": 49., "p90": 199., "p95": 199., "max": 199})
            # New sorted gaps: 0,0,0,49,49.
            self.assertEqual(row["original_frame_id"]["frame_gap"],
                             {"p50": 0., "p90": 49., "p95": 49., "max": 49})
            self.assertAlmostEqual(row["list_position"]["same_frame_proportion"], 1. / 3)
            self.assertAlmostEqual(row["original_frame_id"]["same_frame_proportion"], 3. / 5)
            self.assertAlmostEqual(row["old_candidates_excluded_proportion"], 4. / 9)
            self.assertEqual(row["original_frame_id"]["candidates_per_template"],
                             {"min": 1, "max": 2, "mean": 5. / 3})
            self.assertAlmostEqual(row["original_frame_id"]["uniform_template_same_frame_probability"],
                                   2. / 3)
            self.assertEqual(report["pair_policy"]["sha256"], hashlib.sha256(policy.read_bytes()).hexdigest())
            self.assertEqual((output / "pair-policy.json").read_bytes(), policy.read_bytes())

    def test_audit_rejects_hidden_manifest_before_opening_it(self):
        try:
            audit = importlib.import_module("scripts.audit_nanotrack_frame_pairs")
        except ImportError as error:
            self.fail("missing frame-pair audit: {}".format(error))
        with self.assertRaisesRegex(ValueError, "hidden"):
            audit.audit_manifest("GOT_PERSON_NORMAL", Path("/missing/train-hidden/person.json"), 60)


if __name__ == "__main__":
    unittest.main()
