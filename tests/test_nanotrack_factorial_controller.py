"""Exercise real controller state with a file-producing stand-in for GPU commands."""
import copy
import hashlib
import json
import math
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace

import torch
import yaml

try:
    from scripts.run_nanotrack_factorial import load_context, run_screening, validate_command
    from scripts.verify_nanotrack_factorial import verify_experiment
except ImportError as error:
    IMPORT_ERROR = error
else:
    IMPORT_ERROR = None

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, sort_keys=True) + "\n")


class FakeGPU:
    """Only the external command execution is replaced; JSON/checkpoints are real."""
    def __init__(self, scores=None, fail_at=None):
        self.commands = []
        self.scores = scores or {}
        self.fail_at = fail_at

    def __call__(self, command, log):
        self.commands.append(command)
        if len(self.commands) == self.fail_at:
            raise RuntimeError("simulated interruption")
        flags = dict(zip(command[2::2], command[3::2]))
        log.parent.mkdir(parents=True, exist_ok=True)
        log.write_text(json.dumps(command) + "\nexit_code=0\n")
        script = Path(command[1]).name
        if script == "train_nanotrack_uav123.py":
            step = int(flags["--stop-after-global-step"])
            destination = Path(flags["--output-dir"])
            destination.mkdir(parents=True, exist_ok=True)
            torch.save({"global_step": step, "stage_step": step,
                        "seed": int(flags["--seed"]), "stage": "factorial",
                        "policy": "head_only", "lr_schedule": flags["--lr-schedule"],
                        "pair_policy_sha256": sha(flags["--pair-policy"]),
                        "optimizer": {}, "model": {}},
                       destination / ("candidate_factorial_g%08d_e000_s%05d.pth" % (step, step)))
            batch = yaml.safe_load(Path(flags["--config"]).read_text())["TRAIN"]["BATCH_SIZE"]
            start = step - 2560 // batch
            predecessor = flags.get("--resume", flags.get("--initialize-from"))
            def lr(position):
                return math.exp(math.log(2e-5) * (1 - position * batch / 40960.0)
                                + math.log(8e-6) * position * batch / 40960.0)
            write_json(destination / "segment-telemetry.json", {
                "schema_version": 1, "stage": "factorial", "seed": int(flags["--seed"]),
                "batch_size": batch, "lr_schedule": "sample", "full_sample_budget": 40960,
                "start_update": start, "end_update": step, "segment_updates": step - start,
                "start_global_step": start, "end_global_step": step,
                "start_seen_samples": start * batch, "end_seen_samples": step * batch,
                "segment_samples": 2560, "initial_lr": lr(start), "final_lr": lr(step - 1),
                "terminal_next_lr": lr(step), "loss_finite": True, "loss_count": step - start,
                "wall_seconds": 0.01, "wall_boundary": "training_loop", "config_sha256": sha(flags["--config"]),
                "pair_policy_sha256": sha(flags["--pair-policy"]),
                "input_checkpoint_sha256": sha(predecessor),
                "cuda_memory": {"status": "unavailable", "reason": "fake_cpu_fixture"}})
            return
        snapshot = Path(flags["--snapshot"])
        baseline = snapshot.name == "initial.pth"
        cell = Path(flags["--config"]).stem
        value, got = (0.5, 0.6) if baseline else self.scores.get(cell, (0.51, 0.6))
        common = {"snapshot_sha256": sha(snapshot), "config_sha256": sha(flags["--config"])}
        if script == "evaluate_aerial_tracklets_nanotrack.py":
            common["metrics"] = {"sources": {
                name: {"person_auc": value if name == "VisDrone-MOT" else None,
                       "vehicle_auc": value, "overall_auc": value}
                for name in ("UAVDT", "VisDrone-MOT", "VisDrone-SOT")}}
            output = Path(flags["--output"])
        elif script == "evaluate_uav123_nanotrack.py":
            common.update({"split": "val", "overall": {"success_auc": value},
                           "categories": {name: {"success_auc": value}
                                          for name in ("person", "vehicle")}})
            output = Path(flags["--output-dir"]) / "summary.json"
        else:
            common.update({"subset": "val", "metrics": {"ao": got, "sr_0_5": got,
                                                          "sr_0_75": got}})
            output = Path(flags["--output"])
        write_json(output, common)


class FactorialControllerTest(unittest.TestCase):
    def setUp(self):
        if IMPORT_ERROR:
            self.fail("factorial controller API missing: %s" % IMPORT_ERROR)
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        manifest = json.loads((ROOT / "configs/nanotrack_factorial_v3/experiment-manifest.json").read_text())
        for name in manifest["input_paths"]:
            path = self.root / ("initial.pth" if name == "initial_checkpoint" else name + ".json")
            path.write_text(name)
            manifest["input_paths"][name] = str(path)
            manifest["input_hashes"][name] = sha(path)
        manifest["dataset_manifests"] = {}
        for cell_id, cell in manifest["cells"].items():
            config = yaml.safe_load(Path(cell["config_path"]).read_text())
            config_path = self.root / (cell_id + ".yaml")
            config_path.write_text(yaml.safe_dump(config))
            cell.update(config_path=str(config_path), config_sha256=sha(config_path),
                        input_hashes=manifest["input_hashes"].copy(),
                        initial_checkpoint=manifest["input_paths"]["initial_checkpoint"],
                        pair_policy_path=manifest["input_paths"]["pair_policy"])
        self.manifest_path = self.root / "manifest.json"
        write_json(self.manifest_path, manifest)
        self.output = self.root / "experiment"
        self.inputs = {"panel": self.root / "val-aerial.json", "uav_dataset": self.root / "uav",
                       "uav_prepared": self.root / "prepared", "got_dataset": self.root / "got",
                       "nanotrack_root": self.root / "nanotrack"}
        for key, path in self.inputs.items():
            if key == "panel":
                path.write_text('{"split":"val"}')
            else:
                path.mkdir()
        (self.inputs["uav_prepared"] / "splits.json").write_text('{}')
        (self.inputs["uav_dataset"] / "configSeqs.m").write_text('fixture')
        (self.inputs["got_dataset"] / "val_data/val").mkdir(parents=True)
        (self.inputs["got_dataset"] / "val_data/val/list.txt").write_text('fixture')
        (self.inputs["nanotrack_root"] / "module.py").write_text('# fixture')

    def run_fake(self, gpu=None):
        return run_screening(self.manifest_path, self.output, runner=gpu or FakeGPU(),
                             evaluation_inputs=self.inputs)

    def test_full_matrix_boundaries_commands_and_read_only_verification(self):
        gpu = FakeGPU()
        state = self.run_fake(gpu)
        self.assertEqual(len(state["candidates"]), 72)
        self.assertEqual(len([c for c in gpu.commands if "train_nanotrack_uav123.py" in c[1]]), 72)
        for batch, steps in ((8, [320, 640, 960, 1280]), (32, [80, 160, 240, 320]),
                             (64, [40, 80, 120, 160])):
            records = [r for r in state["candidates"] if r["cell"] == "b%d_g0" % batch and r["seed"] == 123456]
            self.assertEqual([r["global_step"] for r in records], steps)
            self.assertEqual([r["seen_samples"] for r in records], [2560, 5120, 7680, 10240])
            first = records[0]["training"]["command"]
            self.assertIn("--initialize-from", first)
            self.assertIn("--initial-global-step", first)
            self.assertNotIn("--resume", first)
            self.assertEqual(first[first.index("--lr-schedule") + 1], "sample")
            self.assertIn("--resume", records[1]["training"]["command"])
        before = {str(p): (sha(p), p.stat().st_mtime_ns) for p in self.output.rglob("*") if p.is_file()}
        verdict = verify_experiment(self.manifest_path, self.output)
        self.assertTrue(verdict["valid"], verdict)
        self.assertEqual(before, {str(p): (sha(p), p.stat().st_mtime_ns) for p in self.output.rglob("*") if p.is_file()})
        second = FakeGPU()
        self.run_fake(second)
        self.assertEqual(second.commands, [])
        self.assertEqual(before, {str(p): (sha(p), p.stat().st_mtime_ns) for p in self.output.rglob("*") if p.is_file()})

    def test_interruption_retains_valid_nodes_and_resumes_last_predecessor(self):
        with self.assertRaisesRegex(RuntimeError, "interruption"):
            self.run_fake(FakeGPU(fail_at=10))
        partial = json.loads((self.output / "candidate_table.json").read_text())
        self.assertEqual(len(partial["candidates"]), 1)
        checkpoint = partial["candidates"][0]["checkpoint"]
        retry = FakeGPU()
        state = self.run_fake(retry)
        self.assertEqual(len(state["candidates"]), 72)
        first_train = next(c for c in retry.commands if "train_nanotrack" in c[1])
        self.assertEqual(first_train[first_train.index("--resume") + 1], checkpoint)

    def test_frozen_input_or_persisted_identity_drift_is_rejected_without_writes(self):
        self.run_fake()
        table = self.output / "candidate_table.json"
        before = table.read_bytes()
        self.inputs["panel"].write_text('{"split":"val","revision":2}')
        with self.assertRaisesRegex(ValueError, "identity|hash|SHA"):
            self.run_fake()
        self.assertEqual(table.read_bytes(), before)

    def test_corrupt_result_reruns_suffix_but_preserves_valid_predecessor(self):
        state = self.run_fake()
        record = state["candidates"][1]
        Path(record["checkpoint"]).write_text('corrupted')
        gpu = FakeGPU()
        repaired = self.run_fake(gpu)
        trains = [c for c in gpu.commands if "train_nanotrack" in c[1]]
        self.assertEqual(len(trains), 3)
        self.assertEqual(trains[0][trains[0].index("--resume") + 1], state["candidates"][0]["checkpoint"])
        self.assertNotEqual(repaired["candidates"][1]["checkpoint"], record["checkpoint"])

    def test_guardrail_filter_cap_and_no_safe_diagnostic(self):
        scores = {"b32_g0": (0.7, 0.58), "b32_g10": (0.52, 0.6),
                  "b32_g30": (0.519, 0.6), "b64_g0": (0.519, 0.6),
                  "b64_g10": (0.519, 0.6)}
        self.run_fake(FakeGPU(scores))
        summary = json.loads((self.output / "cell_summary.json").read_text())
        self.assertEqual(summary["expansion_cells"], ["b32_g10", "b32_g30", "b64_g0"])
        self.assertFalse(summary["cells"]["b32_g0"]["safe"])
        self.assertIsNone(summary["cells"]["b32_g0"]["mean_target_score_v3"])
        self.assertFalse(summary["promotion_authorized"])
        self.output = self.root / "unsafe"
        self.run_fake(FakeGPU({"b%d_g%d" % (b, g): (0.52, 0.58)
                              for b in (8, 32, 64) for g in (0, 10, 30)}))
        summary = json.loads((self.output / "cell_summary.json").read_text())
        self.assertEqual(summary["state"], "no_safe_cell")
        self.assertEqual(summary["expansion_cells"], [])

    def test_verifier_rejects_missing_node_nonfinite_metric_and_wrong_version(self):
        state = self.run_fake()
        table = self.output / "candidate_table.json"
        for mutate in (lambda s: s["candidates"].pop(),
                       lambda s: s["candidates"][0]["metrics"].update(got_val_ao=float("nan")),
                       lambda s: s.update(scoring_version="old"),
                       lambda s: s["candidates"][0]["identity"].update(config_sha256="bad")):
            changed = copy.deepcopy(state)
            mutate(changed)
            write_json(table, changed)
            with self.assertRaises(ValueError):
                verify_experiment(self.manifest_path, self.output)

    def test_command_whitelist_rejects_hidden_video_and_extra_flags(self):
        for command in (["python", str(ROOT / "scripts/evaluate_got10k_nanotrack.py"), "--subset", "train-hidden"],
                        ["python", str(ROOT / "scripts/evaluate_uav123_nanotrack.py"), "--split", "test"],
                        ["python", str(ROOT / "scripts/sot-v1.mp4")],
                        ["python", str(ROOT / "scripts/train_nanotrack_uav123.py"), "--video", "file.mp4"]):
            with self.assertRaises(ValueError):
                validate_command(command)

    def test_manifest_quota_or_input_hash_drift_fails_before_output_creation(self):
        manifest = json.loads(self.manifest_path.read_text())
        manifest["cells"]["b8_g0"]["quotas"]["UAVDT_VEHICLE_NORMAL"] += 1
        write_json(self.manifest_path, manifest)
        with self.assertRaises(ValueError):
            self.run_fake()
        self.assertFalse(self.output.exists())

    def test_new_schedule_identity_cannot_be_omitted_from_nodes(self):
        state = self.run_fake()
        self.assertEqual(state["candidates"][0]["identity"]["lr_schedule"],
                         {"mode": "sample", "position": "pre_update_seen_samples",
                          "denominator_samples": 40960, "start_lr": 2e-5, "terminal_lr": 8e-6})

    def test_official_got_val_layout_is_bound_and_nonvalidation_panel_rejected(self):
        # The real evaluator reads val_data/val, not val directly.
        _, context = load_context(self.manifest_path, self.inputs)
        self.assertEqual(context["bound"]["evaluation:got_list"]["path"],
                         str(self.inputs["got_dataset"] / "val_data/val/list.txt"))
        self.inputs["panel"].write_text('{"split":"test"}')
        with self.assertRaises(ValueError):
            self.run_fake()
        self.assertFalse(self.output.exists())

    def test_archived_attempt_cannot_conceal_forbidden_access(self):
        # An archived invalid attempt still belongs to the retained evidence trail.
        state = self.run_fake()
        record = copy.deepcopy(state["candidates"][0])
        record["training"]["command"].extend(["--video", "sot-v1.mp4"])
        state["invalidated"].append(record)
        write_json(self.output / "candidate_table.json", state)
        with self.assertRaises(ValueError):
            self.run_fake()

    def test_telemetry_is_required_finite_and_bound_to_exact_samples(self):
        state = self.run_fake()
        record = next(r for r in state["candidates"] if r["cell"] == "b64_g0")
        self.assertIn("telemetry", record["training"])
        telemetry = record["training"]["telemetry"]
        original = read = json.loads(Path(telemetry["path"]).read_text())
        self.assertEqual(read["segment_updates"], 40)
        self.assertEqual(read["segment_samples"], 2560)
        for key, value in (("wall_seconds", float("nan")), ("end_seen_samples", 2561),
                           ("loss_finite", False), ("input_checkpoint_sha256", "wrong")):
            modified = dict(original)
            modified[key] = value
            write_json(Path(telemetry["path"]), modified)
            telemetry["sha256"] = sha(telemetry["path"])
            write_json(self.output / "candidate_table.json", state)
            with self.assertRaisesRegex(ValueError, "telemetry"):
                verify_experiment(self.manifest_path, self.output)
        Path(telemetry["path"]).unlink()
        with self.assertRaises((OSError, ValueError)):
            verify_experiment(self.manifest_path, self.output)


class SampleLearningRateTest(unittest.TestCase):
    def test_trainer_writes_atomic_telemetry_for_forty_update_segment(self):
        from scripts import train_nanotrack_uav123 as training
        self.assertTrue(hasattr(training, "save_sample_segment_telemetry"), "segment telemetry API missing")
        cfg = SimpleNamespace(TRAIN=SimpleNamespace(LR=SimpleNamespace(
            KWARGS=SimpleNamespace(start_lr=2e-5, end_lr=8e-6))))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "segment-telemetry.json"
            training.save_sample_segment_telemetry(
                path, cfg=cfg, stage="factorial", seed=123456, batch_size=64,
                start_update=40, end_update=80, start_global_step=40, end_global_step=80,
                full_sample_budget=40960, losses=[1.0] * 40, wall_seconds=1.25,
                config_sha256="config", pair_policy_sha256="pair", input_checkpoint_sha256="checkpoint",
                cuda_memory={"status": "unavailable", "reason": "fake_cpu_fixture"})
            telemetry = json.loads(path.read_text())
            self.assertEqual(telemetry["start_seen_samples"], 2560)
            self.assertEqual(telemetry["end_seen_samples"], 5120)
            self.assertEqual(telemetry["segment_updates"], 40)
            self.assertEqual(telemetry["loss_count"], 40)
            self.assertTrue(telemetry["loss_finite"])
            self.assertEqual(list(Path(directory).iterdir()), [path])

    def test_same_preupdate_sample_fraction_has_identical_lr_across_batches(self):
        from scripts import train_nanotrack_uav123 as training
        self.assertTrue(hasattr(training, "_head_lr_for_samples"), "sample schedule API missing")
        cfg = SimpleNamespace(TRAIN=SimpleNamespace(LR=SimpleNamespace(
            KWARGS=SimpleNamespace(start_lr=2e-5, end_lr=8e-6))))
        expected = {0: 2e-5, 2560: 1.888681581672778e-5,
                    10240: 1.5905414575341015e-5, 20480: 1.2649110640673518e-5,
                    40960: 8e-6}
        for samples, want in expected.items():
            values = [training._head_lr_for_samples(cfg, (samples // b) * b, 40960)
                      for b in (8, 32, 64)]
            self.assertEqual(values[0], values[1])
            self.assertEqual(values[1], values[2])
            self.assertAlmostEqual(values[0], want, places=15)
        for samples, total in ((-1, 40960), (40961, 40960), (0, 0)):
            with self.assertRaises(ValueError):
                training._head_lr_for_samples(cfg, samples, total)

    def test_sample_schedule_cli_and_checkpoint_roundtrip_preserve_identity(self):
        from scripts import train_nanotrack_uav123 as training
        result = subprocess.run([sys.executable, "-B", str(ROOT / "scripts/train_nanotrack_uav123.py"), "--help"],
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn("{epoch,step,sample}", result.stdout)
        model = torch.nn.Linear(1, 1)
        optimizer = torch.optim.SGD(model.parameters(), lr=2e-5)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "checkpoint.pth"
            training.save_training_checkpoint(path, model, optimizer, completed_epochs=0,
                                              step_in_epoch=40, global_step=40, mean_epoch_loss=1.0,
                                              seed=123456, config="fixture", stage="factorial",
                                              stage_step=40, policy="head_only", lr_schedule="sample")
            restored = training.restore_training_checkpoint(model, optimizer, path)
        self.assertEqual(restored["lr_schedule"], "sample")

if __name__ == "__main__":
    unittest.main()
