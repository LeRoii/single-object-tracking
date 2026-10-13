import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
from pathlib import Path

import torch

import scripts.run_nanotrack_three_stage as runner_module


try:
    from scripts.run_nanotrack_three_stage import (
        ProcessResult,
        ThreeStageController,
        atomic_write_json,
        build_candidate_summary,
        early_stop_decision,
        primary_score,
        replacement_decision,
        validate_candidate_table,
    )
except (ImportError, ModuleNotFoundError) as error:
    _IMPORT_ERROR = error

    def _missing(*args, **kwargs):
        raise AssertionError("Task 5 controller API is missing: {}".format(_IMPORT_ERROR))

    ProcessResult = ThreeStageController = _missing
    atomic_write_json = build_candidate_summary = _missing
    early_stop_decision = primary_score = replacement_decision = _missing
    validate_candidate_table = _missing


def candidate(score=0.5, person=None, vehicle=None, domains=None, **extra):
    value = {
        "person_auc": score if person is None else person,
        "vehicle_auc": score if vehicle is None else vehicle,
        "overall_auc": score,
        "domain_overall": {"UAV123": score, "VisDrone-SOT": score}
        if domains is None
        else domains,
    }
    value.update(extra)
    return value


def require_runner_api(test_case, name):
    value = getattr(runner_module, name, None)
    if value is None:
        test_case.fail("Task 5 runner API is missing: {}".format(name))
    return value


class DecisionEngineTest(unittest.TestCase):
    def test_primary_score_renormalizes_only_present_terms(self):
        self.assertAlmostEqual(
            primary_score({"person_auc": 0.8, "vehicle_auc": None, "overall_auc": 0.5}),
            (0.4 * 0.8 + 0.2 * 0.5) / 0.6,
        )

    def test_primary_score_macro_averages_renormalized_domain_scores(self):
        g1250 = {
            "domains": {
                "UAV123": {"person_auc": 0.7063579184080451, "vehicle_auc": 0.6499015042920938, "overall_auc": 0.7077086289274795},
                "UAVDT": {"person_auc": None, "vehicle_auc": 0.7928997417367853, "overall_auc": 0.7928997417367853},
                "VisDrone-MOT": {"person_auc": 0.7155814200132832, "vehicle_auc": 0.7864274643261185, "overall_auc": 0.7532010659869217},
                "VisDrone-SOT": {"person_auc": None, "vehicle_auc": None, "overall_auc": 0.6952099257036777},
            }
        }
        g2500 = {
            "domains": {
                "UAV123": {"person_auc": 0.7106856909025644, "vehicle_auc": 0.6390661143168695, "overall_auc": 0.7057723432713922},
                "UAVDT": {"person_auc": None, "vehicle_auc": 0.7947916371490209, "overall_auc": 0.7947916371490209},
                "VisDrone-MOT": {"person_auc": 0.7241398140771533, "vehicle_auc": 0.787818299849762, "overall_auc": 0.7581548883251432},
                "VisDrone-SOT": {"person_auc": None, "vehicle_auc": None, "overall_auc": 0.7183776932032037},
            }
        }
        self.assertAlmostEqual(primary_score(g1250), 0.7308997323, places=9)
        self.assertAlmostEqual(primary_score(g2500), 0.7376596861, places=9)
        candidate_2500 = dict(g2500, domain_overall={"UAV123": 0.7057723432713922, "UAVDT": 0.7947916371490209, "VisDrone-MOT": 0.7581548883251432, "VisDrone-SOT": 0.7183776932032037})
        candidate_1250 = dict(g1250, domain_overall={"UAV123": 0.7077086289274795, "UAVDT": 0.7928997417367853, "VisDrone-MOT": 0.7532010659869217, "VisDrone-SOT": 0.6952099257036777})
        decision = replacement_decision(
            candidate_2500,
            candidate_1250,
            {"UAV123": 0.6696433493959986, "UAVDT": 0.7605841556547733,
             "VisDrone-MOT": 0.7368281705248185, "VisDrone-SOT": 0.68901223725477},
        )
        self.assertTrue(decision["qualified"], decision)
        self.assertAlmostEqual(decision["score_improvement"], 0.0067599538, places=9)
        self.assertAlmostEqual(
            primary_score({"person_auc": None, "vehicle_auc": 0.7, "overall_auc": 0.4}),
            (0.4 * 0.7 + 0.2 * 0.4) / 0.6,
        )

    def test_replacement_exact_boundaries_qualify(self):
        best = candidate(0.5, person=0.60, vehicle=0.60)
        proposal = candidate(
            0.545,
            person=0.59,
            vehicle=0.60,
            domains={"UAV123": 0.48, "VisDrone-SOT": 0.50},
        )
        decision = replacement_decision(
            proposal,
            best,
            {"UAV123": 0.50, "VisDrone-SOT": 0.50},
        )
        self.assertTrue(decision["qualified"], decision)

    def test_replacement_rejects_each_value_beyond_boundary(self):
        best = candidate(0.5, person=0.60, vehicle=0.60)
        baselines = {"UAV123": 0.50, "VisDrone-SOT": 0.50}
        cases = (
            candidate(0.544995, person=0.59, vehicle=0.60),
            candidate(0.60, person=0.589999, vehicle=0.62),
            candidate(0.60, person=0.62, vehicle=0.62,
                      domains={"UAV123": 0.479999, "VisDrone-SOT": 0.50}),
        )
        for proposal in cases:
            with self.subTest(proposal=proposal):
                self.assertFalse(
                    replacement_decision(proposal, best, baselines)["qualified"]
                )

    def test_early_stop_boundaries_and_stage_minimum(self):
        best = candidate(0.60)
        baselines = {"UAV123": 0.50, "VisDrone-SOT": 0.50}
        exact = candidate(
            0.58,
            person=0.58,
            vehicle=0.58,
            domains={"UAV123": 0.47, "VisDrone-SOT": 0.50},
        )
        self.assertFalse(
            early_stop_decision("stage2", 1, 0, exact, best, baselines)["should_stop"]
        )
        severe = candidate(
            0.579999,
            person=0.579999,
            vehicle=0.60,
            domains={"UAV123": 0.469999, "VisDrone-SOT": 0.50},
        )
        stage1 = early_stop_decision("stage1", 2, 0, severe, best, baselines)
        self.assertTrue(stage1["severe_regression"])
        self.assertFalse(stage1["should_stop"])
        self.assertTrue(stage1["minimum_deferred"])
        self.assertTrue(
            early_stop_decision("stage1", 6, 2, exact, best, baselines)["should_stop"]
        )
        self.assertTrue(
            early_stop_decision("stage2", 1, 0, severe, best, baselines)["should_stop"]
        )
        self.assertTrue(
            early_stop_decision("stage3", 1, 2, exact, best, baselines)["should_stop"]
        )


class CandidateSummaryTest(unittest.TestCase):
    def test_summary_is_domain_first_and_keeps_evaluator_hashes(self):
        with tempfile.TemporaryDirectory() as directory:
            checkpoint = Path(directory) / "candidate.pth"
            checkpoint.write_bytes(b"checkpoint")
            checkpoint_hash = hashlib.sha256(b"checkpoint").hexdigest()
            uav = {
                "snapshot_sha256": checkpoint_hash,
                "overall": {"success_auc": 0.6},
                "categories": {
                    "person": {"success_auc": 0.7},
                    "car": {"success_auc": 0.5},
                },
            }
            aerial = {
                "snapshot_sha256": checkpoint_hash,
                "panel_sha256": "panel-hash",
                "metrics": {
                    "sources": {
                        "VisDrone-MOT": {
                            "person_auc": 0.5,
                            "vehicle_auc": 0.7,
                            "overall_auc": 0.6,
                        },
                        "UAVDT": {
                            "person_auc": None,
                            "vehicle_auc": 0.8,
                            "overall_auc": 0.8,
                        },
                    }
                },
            }
            summary = build_candidate_summary(
                "stage1", 1250, 1250, checkpoint, uav, aerial,
                evaluator_hashes={"uav123": "u", "val_aerial": "a"},
            )
        self.assertEqual(summary["checkpoint_sha256"], checkpoint_hash)
        self.assertAlmostEqual(summary["person_auc"], 0.6)
        self.assertAlmostEqual(summary["vehicle_auc"], (0.5 + 0.7 + 0.8) / 3.0)
        self.assertAlmostEqual(summary["overall_auc"], (0.6 + 0.6 + 0.8) / 3.0)
        self.assertEqual(summary["evaluator_hashes"], {"uav123": "u", "val_aerial": "a"})
        self.assertAlmostEqual(summary["primary_score"], (0.6 + 0.6 + 0.8) / 3.0)


class StateValidationTest(unittest.TestCase):
    def test_duplicate_global_position_or_key_conflict_fails_closed(self):
        first = candidate(
            stage="stage1", stage_step=1250, global_step=1250,
            key="stage1:g00001250", checkpoint_sha256="a",
        )
        duplicate_position = candidate(
            stage="stage1", stage_step=2500, global_step=1250,
            key="stage1:g00002500", checkpoint_sha256="b",
        )
        with self.assertRaises(ValueError):
            validate_candidate_table({"candidates": [first, duplicate_position]})
        conflict = dict(first)
        conflict["checkpoint_sha256"] = "different"
        with self.assertRaises(ValueError):
            validate_candidate_table({"candidates": [first, conflict]})

    def test_atomic_json_replaces_complete_documents_without_temp_files(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "state.json"
            atomic_write_json(path, {"generation": 1, "payload": "old"})
            atomic_write_json(path, {"generation": 2, "payload": "new"})
            self.assertEqual(json.loads(path.read_text())["generation"], 2)
            self.assertEqual(list(Path(directory).glob("*.tmp")), [])


class FakeCommands:
    def __init__(self):
        self.calls = []

    def __call__(self, action, context):
        return [
            "fake-nanotrack",
            action,
            "--context-json",
            json.dumps(context, sort_keys=True),
        ]


class FakeRunner:
    def __init__(self):
        self.calls = []
        self.pid = 9000

    @staticmethod
    def _score(stage, stage_step):
        if stage == "stage1":
            return 0.51
        if stage == "stage2":
            return 0.52
        return 0.53

    def __call__(self, command, log_path):
        self.calls.append(list(command))
        self.pid += 1
        context = json.loads(command[3])
        action = command[1]
        if action == "train":
            Path(context["checkpoint_path"]).parent.mkdir(parents=True, exist_ok=True)
            torch.save(
                {
                    "stage": context["stage"],
                    "stage_step": context["stage_step"],
                    "global_step": context["global_step"],
                    "epoch": (context["stage_step"] - 1) // 2500,
                    "step_in_epoch": ((context["stage_step"] - 1) % 2500) + 1,
                    "state_dict": {},
                    "optimizer": {},
                },
                str(context["checkpoint_path"]),
            )
        elif action == "uav123":
            score = self._score(context["stage"], context["stage_step"])
            checkpoint_hash = context["checkpoint_sha256"]
            output = {
                "snapshot_sha256": checkpoint_hash,
                "overall": {"success_auc": score},
                "categories": {
                    "person": {"success_auc": score},
                    "car": {"success_auc": score},
                },
            }
            atomic_write_json(Path(context["output_path"]), output)
        elif action in ("val-aerial", "test-aerial-hidden"):
            score = self._score(context.get("stage", "stage3"), context.get("stage_step", 0))
            output = {
                "snapshot_sha256": context["checkpoint_sha256"],
                "panel_sha256": context["panel_sha256"],
                "metrics": {
                    "sources": {
                        "VisDrone-SOT": {
                            "person_auc": score,
                            "vehicle_auc": score,
                            "overall_auc": score,
                        }
                    }
                },
            }
            atomic_write_json(Path(context["output_path"]), output)
        Path(log_path).parent.mkdir(parents=True, exist_ok=True)
        return ProcessResult(
            pid=self.pid,
            start_time="2026-10-09T00:00:00Z",
            end_time="2026-10-09T00:00:01Z",
            exit_code=0,
            stdout="ok\n",
            stderr="",
        )


class FailOnceRunner(FakeRunner):
    def __init__(self, action, stage):
        super().__init__()
        self.fail_action = action
        self.fail_stage = stage
        self.failed = False

    def __call__(self, command, log_path):
        context = json.loads(command[3])
        if (not self.failed and command[1] == self.fail_action
                and context.get("stage") == self.fail_stage):
            self.failed = True
            self.calls.append(list(command))
            raise RuntimeError("simulated interruption")
        return super().__call__(command, log_path)


class ControllerEndToEndTest(unittest.TestCase):
    def _controller(self, root, commands, runner, limits=None):
        official = Path(root) / "nanotrackv3.pth"
        if not official.exists():
            official.write_bytes(b"official")
        return ThreeStageController(
            run_root=Path(root) / "run",
            official_checkpoint=official,
            official_metrics=candidate(0.5),
            official_baselines={"UAV123": 0.5, "VisDrone-SOT": 0.5},
            validation_panel_sha256="validation-panel",
            hidden_panel_sha256="hidden-panel",
            command_factory=commands,
            process_runner=runner,
            stage_candidate_limits=(
                {"stage1": 6, "stage2": 1, "stage3": 1}
                if limits is None else limits
            ),
        )

    def test_fake_end_to_end_flow_uses_selected_initializers_and_atomic_state(self):
        with tempfile.TemporaryDirectory() as directory:
            commands, runner = FakeCommands(), FakeRunner()
            controller = self._controller(directory, commands, runner)
            selection = controller.run()
            run_root = Path(directory) / "run"
            for name in (
                "run_state.json", "candidate_table.json", "selection.json",
                "interruptions.json",
            ):
                self.assertTrue((run_root / name).is_file(), name)
                json.loads((run_root / name).read_text())
            self.assertTrue(selection["selection_final"])
            table = json.loads((run_root / "candidate_table.json").read_text())
            stage1_best = selection["stages"]["stage1"]["checkpoint_sha256"]
            stage2_best = selection["stages"]["stage2"]["checkpoint_sha256"]
            stage2_first = next(c for c in table["candidates"] if c["stage"] == "stage2")
            stage3_first = next(c for c in table["candidates"] if c["stage"] == "stage3")
            self.assertEqual(stage2_first["initial_checkpoint_sha256"], stage1_best)
            self.assertEqual(stage3_first["initial_checkpoint_sha256"], stage2_best)
            stage1_last = [c for c in table["candidates"] if c["stage"] == "stage1"][-1]
            stage2_last = [c for c in table["candidates"] if c["stage"] == "stage2"][-1]
            self.assertEqual(stage2_first["input_checkpoint_sha256"], stage1_best)
            self.assertEqual(stage2_first["previous_global_step"], stage1_last["global_step"])
            self.assertEqual(stage3_first["input_checkpoint_sha256"], stage2_best)
            self.assertEqual(stage3_first["previous_global_step"], stage2_last["global_step"])
            positions = [c["global_step"] for c in table["candidates"]]
            self.assertEqual(positions, sorted(set(positions)))
            process_records = json.loads((run_root / "interruptions.json").read_text())["processes"]
            self.assertTrue(process_records)
            self.assertTrue(all(p["observed_exit"] and p["exit_code"] == 0 for p in process_records))
            self.assertFalse(list(run_root.rglob("*.tmp")))

    def test_each_later_candidate_continues_from_preceding_candidate(self):
        with tempfile.TemporaryDirectory() as directory:
            commands, runner = FakeCommands(), FakeRunner()
            self._controller(directory, commands, runner).run()
            table = json.loads(
                (Path(directory) / "run/candidate_table.json").read_text()
            )["candidates"]
            stage1 = [item for item in table if item["stage"] == "stage1"]
            official_hash = hashlib.sha256(b"official").hexdigest()
            self.assertEqual(stage1[0]["input_checkpoint_sha256"], official_hash)
            self.assertIsNone(stage1[0]["resume_checkpoint_path"])
            for previous, current in zip(stage1, stage1[1:]):
                self.assertEqual(
                    current["input_checkpoint_sha256"], previous["checkpoint_sha256"]
                )
                self.assertEqual(
                    current["resume_checkpoint_path"], previous["checkpoint_path"]
                )

    def test_production_command_factory_builds_real_bounded_commands(self):
        factory_type = require_runner_api(self, "NanoTrackCommandFactory")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            factory = factory_type(
                workspace=root,
                python_executable=Path(sys.executable),
                nanotrack_root=root / "nano",
                stage_configs={name: root / (name + ".yaml") for name in ("stage1", "stage2", "stage3")},
                stage_policies={"stage1": "head_only", "stage2": "tail2", "stage3": "head_only"},
                uav123_dataset_root=root / "uav",
                uav123_prepared_root=root / "prepared",
                validation_panel=root / "val.json",
                hidden_panel=root / "hidden.json",
                device="cuda:0",
            )
            context = {
                "stage": "stage2", "stage_step": 2500, "global_step": 10000,
                "previous_global_step": 8750,
                "checkpoint_path": str(root / "run/stage2/candidate.pth"),
                "input_checkpoint_path": str(root / "run/stage2/previous.pth"),
                "resume_checkpoint_path": str(root / "run/stage2/previous.pth"),
                "output_path": str(root / "run/validation/uav123/summary.json"),
                "panel_sha256": "panel",
            }
            train = factory("train", context)
            self.assertIn(str(root / "scripts/train_nanotrack_uav123.py"), train)
            self.assertEqual(train[train.index("--resume") + 1], context["resume_checkpoint_path"])
            self.assertEqual(train[train.index("--stop-after-global-step") + 1], "10000")
            self.assertNotIn("--initial-global-step", train)
            first = dict(context)
            first["resume_checkpoint_path"] = None
            first["previous_global_step"] = 7500
            first_train = factory("train", first)
            self.assertEqual(first_train[first_train.index("--initial-global-step") + 1], "7500")
            self.assertEqual(first_train[first_train.index("--initialize-from") + 1], first["input_checkpoint_path"])
            self.assertEqual(first_train[first_train.index("--stop-after-global-step") + 1], "10000")
            self.assertNotIn("--resume", first_train)
            uav = factory("uav123", context)
            self.assertIn(str(root / "scripts/evaluate_uav123_nanotrack.py"), uav)
            aerial = factory("val-aerial", context)
            self.assertIn(str(root / "scripts/evaluate_aerial_tracklets_nanotrack.py"), aerial)

    def test_checkpoint_metadata_must_match_requested_stage_and_position(self):
        validate_checkpoint = require_runner_api(self, "validate_candidate_checkpoint")
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "candidate.pth"
            torch.save(
                {"stage": "stage2", "stage_step": 1250, "global_step": 8750, "epoch": 0,
                 "step_in_epoch": 1250},
                str(path),
            )
            metadata = validate_checkpoint(path, "stage2", 1250, 8750)
            self.assertEqual(metadata["global_step"], 8750)
            with self.assertRaises(ValueError):
                validate_checkpoint(path, "stage1", 1250, 8750)
            with self.assertRaises(ValueError):
                validate_checkpoint(path, "stage2", 2500, 10000)

    def test_evaluator_panel_hash_must_match_requested_validation_panel(self):
        class WrongPanelRunner(FakeRunner):
            def __call__(self, command, log_path):
                result = super().__call__(command, log_path)
                if command[1] == "val-aerial":
                    context = json.loads(command[3])
                    output_path = Path(context["output_path"])
                    payload = json.loads(output_path.read_text())
                    payload["panel_sha256"] = "wrong-panel"
                    atomic_write_json(output_path, payload)
                return result

        with tempfile.TemporaryDirectory() as directory:
            controller = self._controller(directory, FakeCommands(), WrongPanelRunner())
            with self.assertRaisesRegex(ValueError, "panel hash"):
                controller.run()

    def test_resume_rehashes_evaluators_and_selection_provenance(self):
        with tempfile.TemporaryDirectory() as directory:
            commands, runner = FakeCommands(), FakeRunner()
            self._controller(directory, commands, runner).run()
            selection_path = Path(directory) / "run/selection.json"
            selection = json.loads(selection_path.read_text())
            selection["stages"]["stage3"]["checkpoint_sha256"] = "replaced checkpoint"
            atomic_write_json(selection_path, selection)
            with self.assertRaisesRegex(ValueError, "selection checkpoint"):
                self._controller(directory, commands, runner)

        with tempfile.TemporaryDirectory() as directory:
            commands, runner = FakeCommands(), FakeRunner()
            self._controller(directory, commands, runner).run()
            table = json.loads((Path(directory) / "run/candidate_table.json").read_text())
            candidate_row = table["candidates"][0]
            result_path = (
                Path(directory) / "run/validation" /
                "{}_{:s}".format(candidate_row["stage"], "g{:08d}".format(candidate_row["global_step"])) /
                "uav123" / "summary.json"
            )
            result_path.write_text("{}\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "evaluator result hash"):
                self._controller(directory, commands, runner)

    def test_hidden_success_is_reused_after_crash_before_final_state_commit(self):
        with tempfile.TemporaryDirectory() as directory:
            commands, runner = FakeCommands(), FakeRunner()
            controller = self._controller(directory, commands, runner)
            controller.run()
            original_atomic_write = runner_module.atomic_write_json
            failed = {"value": False}

            def fail_final_run_state(path, value):
                if Path(path).name == "run_state.json" and "hidden_evaluation" in value and not failed["value"]:
                    failed["value"] = True
                    raise OSError("simulated crash before hidden state commit")
                return original_atomic_write(path, value)

            with mock.patch.object(runner_module, "atomic_write_json", fail_final_run_state):
                with self.assertRaisesRegex(OSError, "simulated crash"):
                    controller.run_hidden_evaluation()
            hidden_count = sum(call[1] == "test-aerial-hidden" for call in runner.calls)
            resumed = self._controller(directory, commands, runner)
            result = resumed.run_hidden_evaluation()
            self.assertEqual(sum(call[1] == "test-aerial-hidden" for call in runner.calls), hidden_count)
            self.assertEqual(result["checkpoint_sha256"], resumed.selection["final_checkpoint_sha256"])

    def test_candidate_transaction_recovers_table_and_run_state_to_one_generation(self):
        with tempfile.TemporaryDirectory() as directory:
            commands, runner = FakeCommands(), FakeRunner()
            controller = self._controller(directory, commands, runner)
            original_atomic_write = runner_module.atomic_write_json
            failed = {"value": False}

            def fail_candidate_table(path, value):
                if (Path(path).name == "candidate_table.json" and value.get("candidates")
                        and not failed["value"]):
                    failed["value"] = True
                    raise OSError("simulated first candidate commit crash")
                return original_atomic_write(path, value)

            with mock.patch.object(runner_module, "atomic_write_json", fail_candidate_table):
                with self.assertRaisesRegex(OSError, "first candidate commit"):
                    controller.run()
            resumed = self._controller(directory, commands, runner)
            root = Path(directory) / "run"
            state = json.loads((root / "run_state.json").read_text())
            table = json.loads((root / "candidate_table.json").read_text())
            journal = json.loads((root / "state_journal.json").read_text())
            self.assertEqual(state["global_step"], table["candidates"][-1]["global_step"])
            self.assertEqual(state["generation"], table["generation"])
            self.assertEqual(state["generation"], journal["generation"])
            self.assertEqual(journal["status"], "committed")
            resumed.run()

    def test_subprocess_communicate_exception_records_identity_without_exit(self):
        class BrokenProcess:
            pid = 4321
            returncode = None

            def communicate(self):
                raise OSError("pipe broke")

        with tempfile.TemporaryDirectory() as directory:
            commands, fake_runner = FakeCommands(), FakeRunner()
            controller = ControllerEndToEndTest()._controller(directory, commands, fake_runner)
            controller.process_runner = runner_module.SubprocessRunner()
            with mock.patch.object(runner_module.subprocess, "Popen", return_value=BrokenProcess()):
                with self.assertRaises(runner_module.ProcessExecutionError) as raised:
                    controller._run_process("train", {"stage": "stage1"}, "broken")
            self.assertEqual(raised.exception.pid, 4321)
            self.assertTrue(raised.exception.start_time)
            self.assertFalse(raised.exception.observed_exit)
            self.assertIsNone(raised.exception.exit_code)
            event = controller.interruptions["events"][-1]
            self.assertEqual(event["pid"], 4321)
            self.assertEqual(event["start_time"], raised.exception.start_time)
            self.assertFalse(event["observed_exit"])
            self.assertIsNone(event["exit_code"])
            with mock.patch.object(runner_module.subprocess, "Popen", side_effect=OSError("exec failed")):
                with self.assertRaises(runner_module.ProcessExecutionError) as launch_error:
                    controller._run_process("train", {"stage": "stage1"}, "launch-failure")
            launch_event = controller.interruptions["events"][-1]
            self.assertIsNone(launch_error.exception.pid)
            self.assertTrue(launch_error.exception.start_time)
            self.assertFalse(launch_event["observed_exit"])
            self.assertIsNone(launch_event["exit_code"])

    def test_resume_skips_completed_candidates_and_rejects_hash_conflict(self):
        with tempfile.TemporaryDirectory() as directory:
            commands, runner = FakeCommands(), FakeRunner()
            self._controller(directory, commands, runner).run()
            original_calls = len(runner.calls)
            selection = self._controller(directory, commands, runner).run()
            self.assertTrue(selection["selection_final"])
            self.assertEqual(len(runner.calls), original_calls)
            table_path = Path(directory) / "run/candidate_table.json"
            table = json.loads(table_path.read_text())
            table["candidates"][0]["checkpoint_sha256"] = "conflict"
            atomic_write_json(table_path, table)
            with self.assertRaises(ValueError):
                self._controller(directory, commands, runner)

    def test_restart_after_interruption_does_not_repeat_completed_stage(self):
        with tempfile.TemporaryDirectory() as directory:
            commands = FakeCommands()
            runner = FailOnceRunner("train", "stage2")
            with self.assertRaises(RuntimeError):
                self._controller(directory, commands, runner).run()
            stage1_calls_before = sum(
                1 for call in runner.calls
                if json.loads(call[3]).get("stage") == "stage1"
            )
            self._controller(directory, commands, runner).run()
            stage1_calls_after = sum(
                1 for call in runner.calls
                if json.loads(call[3]).get("stage") == "stage1"
            )
            self.assertEqual(stage1_calls_after, stage1_calls_before)
            events = json.loads(
                (Path(directory) / "run/interruptions.json").read_text()
            )
            reasons = [event["reason"] for event in events["events"]]
            self.assertIn("process_runner_exception", reasons)
            self.assertIn("controller_restart", reasons)

    def test_restart_reuses_orphan_checkpoint_from_observed_success(self):
        with tempfile.TemporaryDirectory() as directory:
            commands = FakeCommands()
            runner = FailOnceRunner("uav123", "stage2")
            with self.assertRaises(RuntimeError):
                self._controller(directory, commands, runner).run()
            stage2_train_before = sum(
                1 for call in runner.calls
                if call[1] == "train" and json.loads(call[3]).get("stage") == "stage2"
            )
            self._controller(directory, commands, runner).run()
            stage2_train_after = sum(
                1 for call in runner.calls
                if call[1] == "train" and json.loads(call[3]).get("stage") == "stage2"
            )
            self.assertEqual(stage2_train_after, stage2_train_before)

    def test_resume_rejects_changed_run_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            commands, runner = FakeCommands(), FakeRunner()
            self._controller(directory, commands, runner).run()
            official = Path(directory) / "nanotrackv3.pth"
            with self.assertRaises(ValueError):
                ThreeStageController(
                    run_root=Path(directory) / "run",
                    official_checkpoint=official,
                    official_metrics=candidate(0.5),
                    official_baselines={"UAV123": 0.5, "VisDrone-SOT": 0.5},
                    validation_panel_sha256="validation-panel",
                    hidden_panel_sha256="different-hidden-panel",
                    command_factory=commands,
                    process_runner=runner,
                    stage_candidate_limits={"stage1": 6, "stage2": 1, "stage3": 1},
                )

    def test_resume_honors_persisted_early_stop_before_stage_finalization(self):
        with tempfile.TemporaryDirectory() as directory:
            commands, runner = FakeCommands(), FakeRunner()
            limits = {"stage1": 8, "stage2": 1, "stage3": 1}
            self._controller(directory, commands, runner, limits).run()
            root = Path(directory) / "run"
            table_path = root / "candidate_table.json"
            table = json.loads(table_path.read_text())
            table["candidates"] = [
                item for item in table["candidates"] if item["stage"] == "stage1"
            ]
            self.assertEqual(len(table["candidates"]), 6)
            atomic_write_json(table_path, table)
            selection_path = root / "selection.json"
            old_selection = json.loads(selection_path.read_text())
            atomic_write_json(
                selection_path,
                {
                    "format_version": 1,
                    "generation": old_selection["generation"],
                    "selection_final": False,
                    "stages": {},
                },
            )
            state_path = root / "run_state.json"
            state = json.loads(state_path.read_text())
            state["status"] = "running"
            state["current_stage"] = "stage1"
            state["global_step"] = table["candidates"][-1]["global_step"]
            state["stages"] = {"stage1": state["stages"]["stage1"]}
            state["stages"]["stage1"]["complete"] = False
            atomic_write_json(state_path, state)
            self._controller(directory, commands, runner, limits).run()
            resumed = json.loads(table_path.read_text())
            self.assertEqual(
                len([item for item in resumed["candidates"] if item["stage"] == "stage1"]),
                6,
            )

    def test_hidden_gate_rejects_pre_final_runs_once_and_fails_on_hash_mismatch(self):
        with tempfile.TemporaryDirectory() as directory:
            commands, runner = FakeCommands(), FakeRunner()
            controller = self._controller(directory, commands, runner)
            with self.assertRaises(RuntimeError):
                controller.run_hidden_evaluation()
            selection = controller.run()
            result = controller.run_hidden_evaluation()
            hidden_calls = [call for call in runner.calls if call[1] == "test-aerial-hidden"]
            self.assertEqual(len(hidden_calls), 1)
            self.assertEqual(controller.run_hidden_evaluation(), result)
            self.assertEqual(
                len([call for call in runner.calls if call[1] == "test-aerial-hidden"]), 1
            )
            with self.assertRaises(ValueError):
                controller.run_hidden_evaluation(checkpoint_sha256="wrong")
            with self.assertRaises(ValueError):
                controller.run_hidden_evaluation(panel_sha256="wrong")
            frozen = json.loads((Path(directory) / "run/selection.json").read_text())
            self.assertEqual(frozen, selection)
            self.assertNotIn("hidden", json.dumps(frozen).lower())

    def test_hidden_idempotent_reuse_verifies_result_hash(self):
        with tempfile.TemporaryDirectory() as directory:
            commands, runner = FakeCommands(), FakeRunner()
            controller = self._controller(directory, commands, runner)
            controller.run()
            record = controller.run_hidden_evaluation()
            Path(record["result_path"]).write_text("{}\n", encoding="utf-8")
            with self.assertRaises(ValueError):
                controller.run_hidden_evaluation()


class CliTest(unittest.TestCase):
    def test_direct_help_works_outside_workspace(self):
        script = Path(__file__).resolve().parents[1] / "scripts/run_nanotrack_three_stage.py"
        with tempfile.TemporaryDirectory() as directory:
            result = subprocess.run(
                [sys.executable, str(script), "--help"],
                cwd=directory,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                universal_newlines=True,
            )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("--run-root", result.stdout)
        self.assertIn("--nanotrack-command-config", result.stdout)


if __name__ == "__main__":
    unittest.main()
