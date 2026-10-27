"""Exercise real controller state with a file-producing stand-in for GPU commands."""
import copy
import hashlib
import inspect
import json
import math
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
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
        table = next((p / "candidate_table.json" for p in log.parents
                      if (p / "candidate_table.json").is_file()), None)
        if table is None:
            raise AssertionError("command launched before durable attempt journal")
        persisted = json.loads(table.read_text())
        if not any(a["status"] == "running" and e["status"] == "running" and e["command"] == command
                   for a in persisted["attempts"] for e in a["executions"]):
            raise AssertionError("command launched before durable running-stage record")
        if len(self.commands) == self.fail_at:
            raise RuntimeError("simulated interruption")
        flags = dict(zip(command[2::2], command[3::2]))
        log.parent.mkdir(parents=True, exist_ok=True)
        log.write_text(json.dumps(command) + "\nexit_code=0\n")
        script = Path(command[1]).name
        if script == "train_nanotrack_uav123.py":
            step = int(flags["--stop-after-global-step"])
            stage_step = step - 5000
            destination = Path(flags["--output-dir"])
            destination.mkdir(parents=True, exist_ok=True)
            torch.save({"global_step": step, "stage_step": stage_step,
                        "seed": int(flags["--seed"]), "stage": "factorial",
                        "policy": "head_only", "lr_schedule": flags["--lr-schedule"],
                        "pair_policy_sha256": sha(flags["--pair-policy"]),
                        "optimizer": {}, "model": {"weight": torch.tensor([0.0])}},
                       destination / ("candidate_factorial_g%08d_e000_s%05d.pth" % (step, stage_step)))
            batch = yaml.safe_load(Path(flags["--config"]).read_text())["TRAIN"]["BATCH_SIZE"]
            start = stage_step - 2560 // batch
            predecessor = flags.get("--resume", flags.get("--initialize-from"))
            def lr(position):
                return math.exp(math.log(2e-5) * (1 - position * batch / 40960.0)
                                + math.log(8e-6) * position * batch / 40960.0)
            write_json(destination / "segment-telemetry.json", {
                "schema_version": 1, "stage": "factorial", "seed": int(flags["--seed"]),
                "batch_size": batch, "lr_schedule": "sample", "full_sample_budget": 40960,
                "start_update": start, "end_update": stage_step, "segment_updates": stage_step - start,
                "start_global_step": 5000 + start, "end_global_step": step,
                "start_seen_samples": start * batch, "end_seen_samples": stage_step * batch,
                "segment_samples": 2560, "initial_lr": lr(start), "final_lr": lr(stage_step - 1),
                "terminal_next_lr": lr(stage_step), "loss_finite": True, "loss_count": stage_step - start,
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
            if name == "initial_checkpoint":
                torch.save({"global_step": 5000, "stage_step": 5000, "model": {}}, path)
            if name == "pair_policy":
                path.write_text('{"GOT_":"original_frame_id"}')
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

    def partial_state(self):
        with self.assertRaisesRegex(RuntimeError, "interruption"):
            self.run_fake(FakeGPU(fail_at=8))
        return json.loads((self.output / "candidate_table.json").read_text())

    def transition_fixture(self, fail_at=8):
        manifest, current = load_context(self.manifest_path, self.inputs, "fake_cpu_fixture")
        old = copy.deepcopy(current)
        for key, digest in (("code:run_nanotrack_factorial.py", "a" * 64),
                            ("code:nanotrack_factorial_policy.py", "b" * 64)):
            old["bound"][key]["sha256"] = digest
        old["sha256"] = self.context_digest(old)
        with patch("scripts.run_nanotrack_factorial.load_context", return_value=(manifest, old)):
            with self.assertRaisesRegex(RuntimeError, "interruption"):
                self.run_fake(FakeGPU(fail_at=fail_at))
        state = json.loads((self.output / "candidate_table.json").read_text())
        transition = {"schema_version": 1, "old_context_sha256": old["sha256"],
                      "new_context_sha256": current["sha256"], "reason": "reviewed restart fix",
                      "allowed_code_changes": {
                          key: {"old_sha256": old["bound"][key]["sha256"],
                                "new_sha256": current["bound"][key]["sha256"]}
                          for key in ("code:run_nanotrack_factorial.py",
                                      "code:nanotrack_factorial_policy.py")}}
        path = self.root / "transition.json"
        write_json(path, transition)
        return state, current, path

    @staticmethod
    def context_digest(context):
        body = {k: v for k, v in context.items() if k != "sha256"}
        return hashlib.sha256(json.dumps(body, sort_keys=True, allow_nan=False).encode()).hexdigest()

    def run_transition(self, path, gpu=None, current=None):
        self.assertIn("context_transition", inspect.signature(run_screening).parameters,
                      "controller lacks explicit opt-in context transition")
        if current is None:
            return run_screening(self.manifest_path, self.output, runner=gpu or FakeGPU(),
                                 evaluation_inputs=self.inputs, context_transition=path)
        manifest = json.loads(self.manifest_path.read_text())
        with patch("scripts.run_nanotrack_factorial.load_context", return_value=(manifest, current)):
            return self.run_transition(path, gpu)

    def assert_transition_rejected(self, path, current=None):
        table = self.output / "candidate_table.json"
        before = table.read_bytes()
        gpu = FakeGPU()
        with self.assertRaises((ValueError, OSError)):
            self.run_transition(path, gpu, current)
        self.assertEqual(before, table.read_bytes())
        self.assertEqual(gpu.commands, [])

    def test_context_transition_requires_opt_in_without_writes_or_commands(self):
        self.transition_fixture()
        self.assert_transition_rejected(None)

    def test_context_transition_rejects_malformed_manifest_and_hashes(self):
        _, _, path = self.transition_fixture()
        original = json.loads(path.read_text())
        mutations = [lambda m: m.update(schema_version=2),
                     lambda m: m.update(reason=" "),
                     lambda m: m.update(old_context_sha256="0" * 64),
                     lambda m: m.update(new_context_sha256="bad"),
                     lambda m: m["allowed_code_changes"].pop("code:nanotrack_factorial_policy.py"),
                     lambda m: m["allowed_code_changes"].update(extra={}),
                     lambda m: m["allowed_code_changes"]["code:run_nanotrack_factorial.py"].update(old_sha256="bad"),
                     lambda m: m["allowed_code_changes"]["code:nanotrack_factorial_policy.py"].update(new_sha256="0" * 64)]
        for mutate in mutations:
            changed = copy.deepcopy(original)
            mutate(changed)
            write_json(path, changed)
            with self.subTest(manifest=changed):
                self.assert_transition_rejected(path)

    def test_context_transition_rejects_every_extra_context_difference(self):
        _, current, path = self.transition_fixture()
        original = json.loads(path.read_text())
        mutations = [lambda c: c["bound"]["code:run_nanotrack_factorial.py"].update(path="/other/controller.py"),
                     lambda c: c["bound"]["code:train_nanotrack_uav123.py"].update(sha256="c" * 64),
                     lambda c: c["bound"]["code:evaluate_got10k_nanotrack.py"].update(sha256="c" * 64),
                     lambda c: c["bound"]["config:b32_g0"].update(sha256="c" * 64),
                     lambda c: c["bound"]["pair_policy"].update(sha256="c" * 64),
                     lambda c: c["bound"]["pair_audit"].update(sha256="c" * 64),
                     lambda c: c["bound"]["evaluation:got_list"].update(sha256="c" * 64),
                     lambda c: c["bound"]["initial_checkpoint"].update(sha256="c" * 64),
                     lambda c: c["evaluation_inputs"].update(panel="/other/panel.json"),
                     lambda c: c["initial_source_header"].update(global_step=5001),
                     lambda c: c["python"].update(sha256="c" * 64),
                     lambda c: c.update(execution_backend="subprocess"),
                     lambda c: c.update(extra="unapproved")]
        for mutate in mutations:
            changed = copy.deepcopy(current)
            mutate(changed)
            changed["sha256"] = self.context_digest(changed)
            proposal = copy.deepcopy(original)
            proposal["new_context_sha256"] = changed["sha256"]
            write_json(path, proposal)
            with self.subTest(context=changed):
                self.assert_transition_rejected(path, changed)

    def test_context_transition_is_durable_under_lock_before_reconciliation(self):
        import fcntl
        from scripts.run_nanotrack_factorial import reconcile_retained_attempts
        state, current, path = self.transition_fixture()
        table = self.output / "candidate_table.json"
        def reconcile(persisted, root):
            retained = json.loads(table.read_text())
            self.assertEqual(len(retained["context_transitions"]), 1)
            self.assertEqual(retained["context"], state["context"])
            self.assertEqual(retained["candidates"], state["candidates"])
            self.assertEqual(retained["attempts"], state["attempts"])
            entry = retained["context_transitions"][0]
            self.assertEqual(entry["new_context"], current)
            self.assertEqual(entry["manifest"]["path"], str(path.resolve()))
            self.assertEqual(entry["manifest"]["sha256"], sha(path))
            self.assertEqual(entry["manifest"]["size"], path.stat().st_size)
            self.assertEqual(entry["manifest_content"], json.loads(path.read_text()))
            with (self.output / ".controller.lock").open("a+") as lock:
                with self.assertRaises(BlockingIOError):
                    fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            raise RuntimeError("observed durable transition")
        with patch("scripts.run_nanotrack_factorial.reconcile_retained_attempts", side_effect=reconcile):
            with self.assertRaisesRegex(RuntimeError, "durable transition"):
                self.run_transition(path)

    def test_context_transition_preserves_history_and_retries_old_nonterminal_attempt(self):
        old, current, path = self.transition_fixture(fail_at=9)
        interrupted = old["attempts"][-1]
        before_files = {str(p): sha(p) for p in Path(interrupted["destination"]).rglob("*") if p.is_file()}
        gpu = FakeGPU()
        state = self.run_transition(path, gpu)
        self.assertEqual(state["context"], old["context"])
        self.assertEqual(state["baseline"], old["baseline"])
        self.assertEqual(state["candidates"][0], old["candidates"][0])
        self.assertEqual(state["attempts"][:2], old["attempts"][:2])
        abandoned = next(a for a in state["attempts"] if a["id"] == interrupted["id"])
        self.assertEqual(abandoned["identity"], interrupted["identity"])
        self.assertEqual(abandoned["status"], "invalidated")
        self.assertIn("context", abandoned["invalidation_reason"])
        self.assertTrue(abandoned["retained_inventory"])
        self.assertEqual(before_files, {str(p): sha(p) for p in Path(interrupted["destination"]).rglob("*") if p.is_file()})
        first = gpu.commands[0]
        self.assertIn("train_nanotrack", first[1])
        self.assertNotIn(interrupted["destination"], first[first.index("--output-dir") + 1])
        self.assertEqual(first[first.index("--resume") + 1], old["candidates"][0]["checkpoint"])
        self.assertEqual(state["candidates"][1]["identity"]["context_sha256"], current["sha256"])
        self.assertEqual(state["candidates"][1]["identity"]["scoring_source_sha256"], current["bound"]["code:nanotrack_factorial_policy.py"]["sha256"])
        self.assertTrue(verify_experiment(self.manifest_path, self.output)["valid"])
        before = (self.output / "candidate_table.json").read_bytes()
        resumed_gpu = FakeGPU()
        self.run_fake(resumed_gpu)
        self.assertEqual(resumed_gpu.commands, [])
        self.assertEqual(before, (self.output / "candidate_table.json").read_bytes())

    def test_context_transition_retries_planned_old_attempt_and_preserves_invalidated(self):
        from scripts.run_nanotrack_factorial import allocate_attempt, invalidate_attempt
        state, _, path = self.transition_fixture()
        archived_attempt = state["attempts"][-1]
        invalidate_attempt(archived_attempt, "historical interruption")
        original_archived = copy.deepcopy(archived_attempt)
        cell_id = archived_attempt["cell"]
        cell = json.loads(self.manifest_path.read_text())["cells"][cell_id]
        attempt = allocate_attempt(state, Path(archived_attempt["destination"]).parent,
                                   cell_id, cell, state["context"], 123456, 5120,
                                   archived_attempt["predecessor"])
        archived = copy.deepcopy(state["candidates"][0])
        state["invalidated"].append(archived)
        write_json(self.output / "candidate_table.json", state)
        gpu = FakeGPU(fail_at=1)
        with self.assertRaisesRegex(RuntimeError, "interruption"):
            self.run_transition(path, gpu)
        retained = json.loads((self.output / "candidate_table.json").read_text())
        self.assertEqual(retained["invalidated"], [archived])
        self.assertEqual(retained["attempts"][2], original_archived)
        self.assertEqual(retained["attempts"][3]["status"], "invalidated")
        self.assertNotEqual(retained["attempts"][-1]["id"], attempt["id"])

    def test_context_transition_revalidates_manifest_and_rejects_duplicates_forks_unknown_contexts(self):
        _, current, path = self.transition_fixture()
        with self.assertRaisesRegex(RuntimeError, "interruption"):
            self.run_transition(path, FakeGPU(fail_at=1))
        table = self.output / "candidate_table.json"
        adopted = json.loads(table.read_text())
        original_manifest = path.read_bytes()
        path.write_bytes(original_manifest + b" ")
        self.assert_transition_rejected(None)
        path.write_bytes(original_manifest)
        self.assert_transition_rejected(path)
        mutations = [lambda s: s["context_transitions"].append(copy.deepcopy(s["context_transitions"][0])),
                     lambda s: s["context_transitions"][0].update(old_context_sha256="f" * 64),
                     lambda s: s["context_transitions"][0]["new_context"].update(sha256="f" * 64),
                     lambda s: s["context"].update(sha256="f" * 64),
                     lambda s: s["candidates"][0]["identity"].update(context_sha256="f" * 64),
                     lambda s: s["attempts"][1]["identity"].update(context_sha256="f" * 64)]
        for mutate in mutations:
            changed = copy.deepcopy(adopted)
            mutate(changed)
            write_json(table, changed)
            with self.subTest(state=changed):
                self.assert_transition_rejected(None)
        write_json(table, adopted)
        newer = copy.deepcopy(current)
        for key in ("code:run_nanotrack_factorial.py", "code:nanotrack_factorial_policy.py"):
            newer["bound"][key]["sha256"] = "e" * 64
        newer["sha256"] = self.context_digest(newer)
        proposal = json.loads(path.read_text())
        proposal.update(old_context_sha256=current["sha256"], new_context_sha256=newer["sha256"])
        for key, hashes in proposal["allowed_code_changes"].items():
            hashes.update(old_sha256=current["bound"][key]["sha256"], new_sha256="e" * 64)
        second_path = self.root / "second-transition.json"
        write_json(second_path, proposal)
        self.assert_transition_rejected(second_path, newer)

    def test_context_transition_rejects_invalid_runtime_context_hash(self):
        _, current, path = self.transition_fixture()
        current["sha256"] = "f" * 64
        proposal = json.loads(path.read_text())
        proposal["new_context_sha256"] = current["sha256"]
        write_json(path, proposal)
        self.assert_transition_rejected(path, current)

    def test_context_transition_rejects_candidate_attempt_provenance_relabeling(self):
        from scripts.run_nanotrack_factorial import validate_state_identity
        _, current, path = self.transition_fixture()
        with self.assertRaisesRegex(RuntimeError, "interruption"):
            self.run_transition(path, FakeGPU(fail_at=1))
        adopted = json.loads((self.output / "candidate_table.json").read_text())
        manifest = json.loads(self.manifest_path.read_text())
        for location in ("candidate", "attempt"):
            changed = copy.deepcopy(adopted)
            record = changed["candidates"][0]
            attempt = next(a for a in changed["attempts"] if a["id"] == record["attempt_id"])
            identity = record["identity"] if location == "candidate" else attempt["identity"]
            identity.update(context_sha256=current["sha256"],
                            scoring_source_sha256=current["bound"]["code:nanotrack_factorial_policy.py"]["sha256"])
            with self.subTest(location=location), self.assertRaises(ValueError):
                validate_state_identity(changed, manifest, current, self.output)

    def test_full_matrix_boundaries_commands_and_read_only_verification(self):
        gpu = FakeGPU()
        state = self.run_fake(gpu)
        self.assertEqual(len(state["candidates"]), 72)
        self.assertEqual(len([c for c in gpu.commands if "train_nanotrack_uav123.py" in c[1]]), 72)
        for batch, steps in ((8, [320, 640, 960, 1280]), (32, [80, 160, 240, 320]),
                             (64, [40, 80, 120, 160])):
            records = [r for r in state["candidates"] if r["cell"] == "b%d_g0" % batch and r["seed"] == 123456]
            self.assertEqual([r["global_step"] for r in records], [5000 + s for s in steps])
            self.assertEqual([r["identity"]["stage_step"] for r in records], steps)
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
        checkpoint = partial["attempts"][-1]["checkpoint"]["path"]
        retry = FakeGPU()
        state = self.run_fake(retry)
        self.assertEqual(len(state["candidates"]), 72)
        first_train = next(c for c in retry.commands if "train_nanotrack" in c[1])
        self.assertEqual(first_train[first_train.index("--resume") + 1], checkpoint)

    def test_restart_reuses_complete_attempt_with_one_ulp_persisted_floats(self):
        state = self.partial_state()
        self.assertEqual(state["schema_version"], 2)
        record = state["candidates"][0]
        # Adjacent binary64 values, representing legacy aggregation rounding.
        state["baseline"]["metrics"]["person_macro"] = 0.5000000000000001
        record["metrics"]["got_val_ao"] = 0.6000000000000001
        record["decision"]["checks"]["vehicle_macro"]["candidate"] = 0.5100000000000001
        write_json(self.output / "candidate_table.json", state)
        retained = {str(p): (sha(p), p.stat().st_mtime_ns)
                    for p in Path(record["destination"]).rglob("*") if p.is_file()}
        retry = FakeGPU(fail_at=1)
        with self.assertRaisesRegex(RuntimeError, "interruption"):
            self.run_fake(retry)
        resumed = json.loads((self.output / "candidate_table.json").read_text())
        self.assertEqual(resumed["candidates"], [record])
        self.assertEqual(resumed["invalidated"], [])
        attempt = next(a for a in resumed["attempts"] if a["id"] == record["attempt_id"])
        self.assertEqual(attempt["status"], "complete")
        self.assertIn("baseline", resumed, "one-ULP baseline drift must not invalidate it")
        self.assertEqual(resumed["baseline"], state["baseline"])
        self.assertNotIn(str(Path(record["checkpoint"]).parent), retry.commands[0])
        self.assertEqual(retained, {str(p): (sha(p), p.stat().st_mtime_ns)
                                    for p in Path(record["destination"]).rglob("*") if p.is_file()})

    def test_persisted_semantic_changes_are_rejected_by_record_revalidation(self):
        from scripts.run_nanotrack_factorial import validate_record
        state = self.partial_state()
        manifest, context = load_context(self.manifest_path, self.inputs, "fake_cpu_fixture")
        record = state["candidates"][0]
        mutations = {
            "qualification": lambda r: r["decision"].update(qualified=False),
            "qualification_integer": lambda r: r["decision"].update(qualified=1),
            "check_boolean_integer": lambda r: r["decision"]["checks"]["vehicle_macro"].update(passed=1),
            "reason_text": lambda r: r["decision"]["reasons"].append("changed_reason"),
            "missing_decision_key": lambda r: r["decision"].pop("target_score_v3"),
            "missing_metric_key": lambda r: r["metrics"].pop("got_val_ao"),
            "material_metric": lambda r: r["metrics"].update(got_val_ao=0.600001),
            "material_decision": lambda r: r["decision"].update(target_score_v3=0.510001),
        }
        for name, mutate in mutations.items():
            changed = copy.deepcopy(record)
            mutate(changed)
            with self.subTest(change=name), self.assertRaisesRegex(ValueError, "persisted metrics"):
                validate_record(changed, manifest["cells"][record["cell"]], context,
                                None, state["baseline"]["metrics"])

    def test_frozen_input_or_persisted_identity_drift_is_rejected_without_writes(self):
        self.partial_state()
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
        state = self.partial_state()
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
        state = self.partial_state()
        record = copy.deepcopy(state["candidates"][0])
        record["training"]["command"].extend(["--video", "sot-v1.mp4"])
        state["invalidated"].append(record)
        write_json(self.output / "candidate_table.json", state)
        with self.assertRaises(ValueError):
            self.run_fake()

    def test_telemetry_is_required_finite_and_bound_to_exact_samples(self):
        state = self.partial_state()
        record = state["candidates"][0]
        self.assertIn("telemetry", record["training"])
        telemetry = record["training"]["telemetry"]
        original = read = json.loads(Path(telemetry["path"]).read_text())
        self.assertEqual(read["segment_updates"], 80)
        self.assertEqual(read["segment_samples"], 2560)
        for key, value in (("wall_seconds", float("nan")), ("end_seen_samples", 2561),
                           ("loss_finite", False), ("input_checkpoint_sha256", "wrong")):
            modified = dict(original)
            modified[key] = value
            write_json(Path(telemetry["path"]), modified)
            telemetry["sha256"] = sha(telemetry["path"])
            attempt = next(a for a in state["attempts"] if a["id"] == record["attempt_id"])
            next(e for e in attempt["executions"] if e["role"] == "train")["evidence"]["telemetry"]["sha256"] = telemetry["sha256"]
            write_json(self.output / "candidate_table.json", state)
            with self.assertRaisesRegex(ValueError, "telemetry"):
                verify_experiment(self.manifest_path, self.output)
        Path(telemetry["path"]).unlink()
        with self.assertRaises((OSError, ValueError)):
            verify_experiment(self.manifest_path, self.output)

    def test_generated_commands_pass_real_trainer_preflight_for_all_batches(self):
        from scripts.run_nanotrack_factorial import training_command
        manifest, context = load_context(self.manifest_path, self.inputs)
        for batch in (8, 32, 64):
            cell = manifest["cells"]["b%d_g0" % batch]
            command = training_command(cell, 123456, 2560, self.root / "never-created", context, None)
            result = subprocess.run(command + ["--validate-only"], stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, universal_newlines=True)
            self.assertEqual(result.returncode, 0, result.stdout)
            self.assertIn('"start_global_step": 5000', result.stdout)
            self.assertFalse((self.root / "never-created").exists())

    def test_evaluation_interruptions_reuse_every_completed_stage(self):
        # Calls 8/9/10: node 2 trained, then zero/one/two evaluator successes.
        for fail_at in (9, 10, 11):
            self.output = self.root / ("interrupt%d" % fail_at)
            gpu = FakeGPU(fail_at=fail_at)
            with self.assertRaisesRegex(RuntimeError, "interruption"):
                self.run_fake(gpu)
            partial = json.loads((self.output / "candidate_table.json").read_text())
            self.assertIn("attempts", partial)
            self.assertEqual(len(partial["attempts"]), 3)
            succeeded = [c for c in gpu.commands[:-1]]
            retry = FakeGPU(fail_at=13 - fail_at)
            with self.assertRaisesRegex(RuntimeError, "interruption"):
                self.run_fake(retry)
            state = json.loads((self.output / "candidate_table.json").read_text())
            self.assertEqual(len(state["candidates"]), 2)
            self.assertEqual(len([c for c in retry.commands if "train_nanotrack" in c[1]]), 1)
            self.assertFalse(any(c in succeeded for c in retry.commands))
            with self.assertRaisesRegex(ValueError, "pending"):
                verify_experiment(self.manifest_path, self.output)

    def test_orphan_and_interrupted_attempts_are_audited_and_all_commands_checked(self):
        with self.assertRaises(RuntimeError):
            self.run_fake(FakeGPU(fail_at=9))
        state = json.loads((self.output / "candidate_table.json").read_text())
        self.assertIn("attempts", state)
        self.assertEqual(state["attempts"][-1]["status"], "running")
        checkpoint = Path(state["attempts"][-1]["checkpoint"]["path"])
        checkpoint.write_text("corrupt interrupted checkpoint")
        self.run_fake()
        state = json.loads((self.output / "candidate_table.json").read_text())
        self.assertTrue(any(a["status"] == "invalidated" for a in state["attempts"]))
        orphan = self.output / "nodes/orphan/attempt0001"
        orphan.mkdir(parents=True)
        (orphan / "partial.pth").write_text("unowned partial")
        with self.assertRaises(ValueError):
            verify_experiment(self.manifest_path, self.output)
        self.run_fake()
        self.assertTrue(verify_experiment(self.manifest_path, self.output)["valid"])
        state = json.loads((self.output / "candidate_table.json").read_text())
        orphan_record = next(a for a in state["attempts"] if a["destination"] == str(orphan))
        self.assertEqual(orphan_record["status"], "invalidated")
        orphan_record["executions"].append({"role": "got", "command": ["python", str(ROOT / "scripts/evaluate_got10k_nanotrack.py"), "--subset", "train-hidden"], "status": "planned"})
        write_json(self.output / "candidate_table.json", state)
        with self.assertRaises(ValueError):
            verify_experiment(self.manifest_path, self.output)

    def test_schema_one_state_is_rejected_without_automatic_relabeling(self):
        self.output.mkdir()
        path = self.output / "candidate_table.json"
        write_json(path, {"schema_version": 1, "candidates": []})
        before = path.read_bytes()
        with self.assertRaisesRegex(ValueError, "schema.*1|schema.*2"):
            self.run_fake()
        self.assertEqual(path.read_bytes(), before)

    def test_completed_training_is_recovered_after_success_before_state_commit(self):
        class ExitAfterTrainingSuccess(FakeGPU):
            def __call__(self, command, log):
                super().__call__(command, log)
                if len(self.commands) == 8:
                    raise RuntimeError("interruption after observed training success")
        with self.assertRaisesRegex(RuntimeError, "after observed"):
            self.run_fake(ExitAfterTrainingSuccess())
        retry = FakeGPU(fail_at=4)
        with self.assertRaisesRegex(RuntimeError, "interruption"):
            self.run_fake(retry)
        self.assertTrue(all("evaluate_" in c[1] for c in retry.commands[:3]))
        state = json.loads((self.output / "candidate_table.json").read_text())
        self.assertEqual(len(state["candidates"]), 2)
        attempt = state["attempts"][2]
        training = next(e for e in attempt["executions"] if e["role"] == "train")
        self.assertEqual(training["recovery"], "observed_success_after_interruption")

    def test_retry_allocates_unique_attempt_after_training_interrupted_before_any_output(self):
        partial = self.partial_state()
        abandoned = partial["attempts"][-1]["destination"]
        self.assertFalse(Path(abandoned).exists())
        retry = FakeGPU(fail_at=5)
        with self.assertRaisesRegex(RuntimeError, "interruption"):
            self.run_fake(retry)
        state = json.loads((self.output / "candidate_table.json").read_text())
        ids = [a["id"] for a in state["attempts"]]
        self.assertEqual(len(ids), len(set(ids)), "allocated-but-uncreated attempts must reserve their identity")
        self.assertEqual(state["attempts"][2]["status"], "invalidated")

    def test_changed_loadable_interrupted_model_is_not_adopted_and_original_hash_is_retained(self):
        class ExitAfterTraining(FakeGPU):
            def __call__(self, command, log):
                super().__call__(command, log)
                if len(self.commands) == 8:
                    raise RuntimeError("after training output")
        with self.assertRaises(RuntimeError):
            self.run_fake(ExitAfterTraining())
        state = json.loads((self.output / "candidate_table.json").read_text())
        attempt = state["attempts"][-1]
        execution = next(e for e in attempt["executions"] if e["role"] == "train")
        original_inventory = copy.deepcopy(execution["retained_inventory"])
        checkpoint = Path(attempt["checkpoint_path"])
        original_sha = sha(checkpoint)
        payload = torch.load(checkpoint, map_location="cpu")
        payload["model"]["weight"].add_(1.0)  # all metadata, keys, shapes and types remain valid
        torch.save(payload, checkpoint)
        self.assertNotEqual(sha(checkpoint), original_sha)
        retry = FakeGPU(fail_at=2)
        real_load = torch.load
        def reject_changed_checkpoint_load(path, *args, **kwargs):
            if Path(path) == checkpoint:
                self.fail("changed interrupted checkpoint reached deserialization")
            return real_load(path, *args, **kwargs)
        with patch.object(torch, "load", side_effect=reject_changed_checkpoint_load):
            with self.assertRaises(RuntimeError):
                self.run_fake(retry)
        self.assertIn("train_nanotrack_uav123.py", retry.commands[0][1])
        state = json.loads((self.output / "candidate_table.json").read_text())
        rejected = next(a for a in state["attempts"] if a["id"] == attempt["id"])
        self.assertEqual(rejected["status"], "invalidated")
        retained = next(e for e in rejected["executions"] if e["role"] == "train")
        self.assertEqual(retained["retained_inventory"], original_inventory)
        self.assertEqual(next(r["sha256"] for r in retained["retained_inventory"]
                              if r["path"] == str(checkpoint)), original_sha)
        completed = self.run_fake()
        self.assertTrue(all(r["checkpoint"] != str(checkpoint) for r in completed["candidates"]))
        self.assertTrue(verify_experiment(self.manifest_path, self.output)["valid"])

    def test_changed_valid_interrupted_evaluator_is_retried_without_retraining(self):
        class ExitAfterAerial(FakeGPU):
            def __call__(self, command, log):
                super().__call__(command, log)
                if len(self.commands) == 9:
                    raise RuntimeError("after aerial output")
        with self.assertRaises(RuntimeError):
            self.run_fake(ExitAfterAerial())
        state = json.loads((self.output / "candidate_table.json").read_text())
        attempt = state["attempts"][-1]
        execution = next(e for e in attempt["executions"] if e["role"] == "aerial")
        original_inventory = copy.deepcopy(execution["retained_inventory"])
        output = Path(execution["output_path"])
        value = json.loads(output.read_text())
        value["metrics"]["sources"]["UAVDT"]["overall_auc"] = 0.52
        write_json(output, value)
        retry = FakeGPU(fail_at=2)
        from scripts import run_nanotrack_factorial as controller
        real_read = controller.read_json
        def reject_changed_evaluator_parse(path):
            if Path(path) == output:
                self.fail("changed interrupted evaluator reached JSON parsing")
            return real_read(path)
        with patch.object(controller, "read_json", side_effect=reject_changed_evaluator_parse):
            with self.assertRaises(RuntimeError):
                self.run_fake(retry)
        self.assertIn("evaluate_aerial_tracklets_nanotrack.py", retry.commands[0][1])
        self.assertNotIn(str(output), retry.commands[0])
        self.assertFalse(any("train_nanotrack" in c[1] for c in retry.commands))
        state = json.loads((self.output / "candidate_table.json").read_text())
        rejected = next(e for e in state["attempts"][-1]["executions"]
                        if e["role"] == "aerial" and e["status"] == "invalidated")
        self.assertEqual(rejected["retained_inventory"], original_inventory)

    def test_recorded_file_cannot_be_replaced_by_equal_bytes_symlink(self):
        from scripts import run_nanotrack_factorial as controller
        self.assertTrue(hasattr(controller, "check_recorded_inventory"))
        destination = self.root / "stage"
        destination.mkdir()
        original = destination / "output.bin"
        replacement = self.root / "replacement.bin"
        original.write_bytes(b"same bytes")
        replacement.write_bytes(b"same bytes")
        execution = {"destination": str(destination), "retained_inventory": controller.inventory(destination)}
        original.unlink()
        original.symlink_to(replacement)
        with self.assertRaisesRegex(ValueError, "type|symlink"):
            controller.check_recorded_inventory(execution)


class PersistedEquivalenceTest(unittest.TestCase):
    def equivalent(self, left, right):
        from scripts import run_nanotrack_factorial as controller
        self.assertTrue(hasattr(controller, "persisted_equivalent"),
                        "persisted float equivalence is missing")
        return controller.persisted_equivalent(left, right)

    def test_nested_finite_numbers_allow_only_the_requested_tolerance(self):
        for left, right in ((0.6, 0.6000000000000001), (0.0, 1e-12),
                            (1000000.0, 1000000.0000005), (1, 1.0)):
            with self.subTest(left=left, right=right):
                self.assertTrue(self.equivalent({"v": [left]}, {"v": [right]}))
        for left, right in ((0.0, 1.0001e-12), (0.6, 0.600001),
                            (1000000.0, 1000000.000002)):
            with self.subTest(left=left, right=right):
                self.assertFalse(self.equivalent({"v": [left]}, {"v": [right]}))

    def test_structure_reason_order_and_scalar_types_remain_exact(self):
        for left, right in ((True, 1), (False, 0.0), (1, True),
                            ("1", 1), (None, 0), (None, ""),
                            ({"a": 1}, {"a": 1, "b": 2}),
                            ([1], [1, 2]), (["a", "b"], ["b", "a"]),
                            ([1], {"0": 1}), ("reason", "changed_reason")):
            with self.subTest(left=left, right=right):
                self.assertFalse(self.equivalent(left, right))
        self.assertTrue(self.equivalent({"qualified": True, "reasons": [], "other": None},
                                        {"other": None, "reasons": [], "qualified": True}))

    def test_nonfinite_numbers_are_never_equivalent_even_to_themselves(self):
        for value in (float("nan"), float("inf"), -float("inf")):
            for other in (value, 0.5):
                with self.subTest(value=value, other=other):
                    self.assertFalse(self.equivalent({"v": [value]}, {"v": [other]}))


class SampleLearningRateTest(unittest.TestCase):
    def test_frozen_g5000_initialization_preserves_source_offset_on_cpu(self):
        code = '''
import sys
from pathlib import Path
sys.path.insert(0, "/home/u20/single-object-tracking/repos/siamtrackers/NanoTrack")
from nanotrack.core.config import cfg
cfg.merge_from_file("configs/nanotrack_factorial_v3/b8_g0.yaml")
from nanotrack.models.model_builder import ModelBuilder
from scripts.train_nanotrack_uav123 import initialize_training_stage
import torch
model = ModelBuilder()
optimizer = torch.optim.SGD(model.parameters(), lr=2e-5)
path = Path("/home/u20/single-object-tracking/artifacts/nanotrack_conservative_three_stage/run/stage1/candidate_stage1_g00005000_e001_s02500.pth")
try:
    initialize_training_stage(model, optimizer, path, "factorial", 0)
except ValueError:
    pass
else:
    raise AssertionError("backwards initializer was accepted")
progress = initialize_training_stage(model, optimizer, path, "factorial", 5000)
assert progress["global_step"] == 5000 and progress["stage_step"] == 0
assert not optimizer.state
print("CPU frozen g5000 initialization: global=5000 stage=0 fresh_optimizer=true")
'''
        result = subprocess.run([sys.executable, "-B", "-c", code], cwd=str(ROOT),
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn("global=5000 stage=0 fresh_optimizer=true", result.stdout)
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
