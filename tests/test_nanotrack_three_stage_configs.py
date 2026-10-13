import hashlib
import json
import math
import subprocess
import sys
import unittest
from pathlib import Path

import yaml

import scripts.run_nanotrack_three_stage as runner_module


ROOT = Path(__file__).resolve().parents[1]
CONFIG_DIR = ROOT / "configs"
ARTIFACT_DIR = ROOT / "artifacts/nanotrack_conservative_three_stage/smoke"
SOURCES = {
    "UAV123": ("uav123", 50),
    "VISDRONE_SOT": ("visdrone_sot", 100),
    "VISDRONE_MOT": ("visdrone_mot", 30),
    "UAVDT": ("uavdt", 30),
}
CATEGORIES = {
    "UAV123": ("person", "vehicle", "other", "bike"),
    "VISDRONE_SOT": ("unknown",),
    "VISDRONE_MOT": ("person", "vehicle", "bike"),
    "UAVDT": ("vehicle",),
}
QUOTAS = {
    "stage1": {
        "UAV123": {"person": 2500, "vehicle": 1500, "other": 750, "bike": 250},
        "VISDRONE_SOT": {"unknown": 7000},
        "VISDRONE_MOT": {"person": 3000, "vehicle": 1500, "bike": 500},
        "UAVDT": {"vehicle": 3000},
    },
    "stage2": {
        "UAV123": {"person": 2000, "vehicle": 1200, "other": 600, "bike": 200},
        "VISDRONE_SOT": {"unknown": 8000},
        "VISDRONE_MOT": {"person": 3000, "vehicle": 1500, "bike": 500},
        "UAVDT": {"vehicle": 3000},
    },
    "stage3": {
        "UAV123": {"person": 1000, "vehicle": 600, "other": 300, "bike": 100},
        "VISDRONE_SOT": {"unknown": 4000},
        "VISDRONE_MOT": {"person": 1500, "vehicle": 750, "bike": 250},
        "UAVDT": {"vehicle": 1500},
    },
}
HARD_QUOTAS = {
    "UAV123": {"person": 750, "vehicle": 750, "other": 250, "bike": 250},
    "VISDRONE_SOT": {"unknown": 3000},
    "VISDRONE_MOT": {"person": 2000, "vehicle": 2000, "bike": 500},
    "UAVDT": {"vehicle": 500},
}
STAGE_SETTINGS = {
    "stage1": (4, "head_only", 1e-4, 4e-5),
    "stage2": (3, "tail2", 2e-5, 1e-5),
    "stage3": (2, "head_only", 5e-6, 2e-6),
}


def read_config(stage, smoke=False):
    suffix = "_smoke" if smoke else ""
    path = CONFIG_DIR / "nanotrack_v3_three_stage_{}{}.yaml".format(stage, suffix)
    return yaml.safe_load(path.read_text(encoding="utf-8"))


class FormalThreeStageConfigTest(unittest.TestCase):
    def test_formal_configs_match_quota_paths_and_training_contract(self):
        for stage, (epochs, policy, start_lr, end_lr) in STAGE_SETTINGS.items():
            with self.subTest(stage=stage):
                cfg = read_config(stage)
                self.assertEqual(cfg["BACKBONE"]["TYPE"], "mobilenetv3_small_v3")
                self.assertEqual(cfg["BACKBONE"]["KWARGS"]["used_layers"], [4])
                self.assertEqual(cfg["BACKBONE"]["TRAIN_LAYERS"], ["features"])
                self.assertEqual(cfg["BACKBONE"]["TRAIN_EPOCH"], epochs + 1)
                self.assertEqual(cfg["BACKBONE"]["LAYERS_LR"], 0.05)
                self.assertEqual(cfg["TRAIN"]["BATCH_SIZE"], 8)
                self.assertEqual(cfg["TRAIN"]["NUM_WORKERS"], 0)
                self.assertEqual(cfg["TRAIN"]["EPOCH"], epochs)
                self.assertFalse(cfg["TRAIN"]["LR_WARMUP"]["WARMUP"])
                self.assertEqual(cfg["TRAIN"]["LR"]["TYPE"], "log")
                self.assertEqual(cfg["TRAIN"]["LR"]["KWARGS"],
                                 {"start_lr": start_lr, "end_lr": end_lr})
                self.assertEqual(cfg["TRAIN"]["BASE_LR"], start_lr)
                if stage == "stage2":
                    self.assertAlmostEqual(start_lr * cfg["BACKBONE"]["LAYERS_LR"], 1e-6)
                    self.assertAlmostEqual(end_lr * cfg["BACKBONE"]["LAYERS_LR"], 5e-7)
                self.assertEqual(cfg["CUDA"], True)
                self.assertEqual(cfg["DATASET"]["VIDEOS_PER_EPOCH"], 20000)
                self.assertEqual(cfg["META_ARC"], "nanotrack-v3-three-stage-{}-{}".format(stage, policy))
                self.assertEqual(cfg["TRAIN"]["SNAPSHOT_DIR"],
                                 str(ROOT / "artifacts/nanotrack_conservative_three_stage" / stage))
                self.assertEqual(cfg["DATASET"]["NAMES"], sorted(cfg["DATASET"]["NAMES"]))

                observed = {}
                for source, (folder, frame_range) in SOURCES.items():
                    for category, quota in QUOTAS[stage][source].items():
                        name = "{}_{}_NORMAL".format(source, category).upper()
                        item = cfg["DATASET"][name]
                        manifest = ROOT / "data/nanotrack_conservative_three_stage" / folder / "normal" / (category + ".json")
                        crop_root = ROOT / "data/nanotrack_conservative_three_stage" / folder / "crop511"
                        self.assertEqual(item["ROOT"], str(crop_root))
                        self.assertEqual(item["ANNO"], str(manifest))
                        self.assertEqual(item["FRAME_RANGE"], frame_range)
                        self.assertEqual(item["NUM_USE"], quota)
                        self.assertTrue(manifest.is_file())
                        self.assertGreater(len(json.loads(manifest.read_text(encoding="utf-8"))), 0)
                        self.assertTrue(crop_root.is_dir())
                        observed[name] = quota
                if stage == "stage3":
                    for source, (folder, frame_range) in SOURCES.items():
                        for category, quota in HARD_QUOTAS[source].items():
                            name = "{}_{}_HARD".format(source, category).upper()
                            item = cfg["DATASET"][name]
                            manifest = ROOT / "data/nanotrack_conservative_three_stage" / folder / "hard" / (category + ".json")
                            crop_root = ROOT / "data/nanotrack_conservative_three_stage" / folder / "crop511"
                            self.assertEqual(item["ROOT"], str(crop_root))
                            self.assertEqual(item["ANNO"], str(manifest))
                            self.assertEqual(item["FRAME_RANGE"], frame_range)
                            self.assertEqual(item["NUM_USE"], quota)
                            self.assertTrue(manifest.is_file())
                            self.assertGreater(len(json.loads(manifest.read_text(encoding="utf-8"))), 0)
                            observed[name] = quota
                self.assertEqual(set(cfg["DATASET"]["NAMES"]), set(observed))
                self.assertEqual(sum(observed.values()), 20000)

    def test_stage3_has_distinct_nonempty_normal_and_hard_pools(self):
        cfg = read_config("stage3")
        normal_total = 0
        hard_total = 0
        for source, (folder, frame_range) in SOURCES.items():
            for category, quota in QUOTAS["stage3"][source].items():
                normal = cfg["DATASET"]["{}_{}_NORMAL".format(source, category).upper()]
                hard = cfg["DATASET"]["{}_{}_HARD".format(source, category).upper()]
                self.assertTrue(normal["ANNO"].endswith("/normal/{}.json".format(category)))
                self.assertTrue(hard["ANNO"].endswith("/hard/{}.json".format(category)))
                self.assertNotEqual(normal["ANNO"], hard["ANNO"])
                self.assertEqual(normal["ROOT"], hard["ROOT"])
                self.assertEqual(normal["FRAME_RANGE"], frame_range)
                self.assertEqual(hard["FRAME_RANGE"], frame_range)
                self.assertEqual(hard["NUM_USE"], HARD_QUOTAS[source][category])
                normal_total += normal["NUM_USE"]
                hard_total += hard["NUM_USE"]
                for item in (normal, hard):
                    manifest = Path(item["ANNO"])
                    self.assertTrue(manifest.is_file())
                    self.assertGreater(len(json.loads(manifest.read_text(encoding="utf-8"))), 0)
        self.assertEqual(normal_total, 10000)
        self.assertEqual(hard_total, 10000)

    def test_smoke_configs_are_bounded_and_keep_policy_representative_pools(self):
        for stage, (_, policy, _, _) in STAGE_SETTINGS.items():
            cfg = read_config(stage, smoke=True)
            self.assertEqual(cfg["META_ARC"], "nanotrack-v3-three-stage-{}-{}-smoke".format(stage, policy))
            self.assertEqual(cfg["CUDA"], True)
            self.assertEqual(cfg["TRAIN"]["NUM_WORKERS"], 0)
            self.assertEqual(cfg["TRAIN"]["EPOCH"], 1)
            self.assertLessEqual(cfg["TRAIN"]["BATCH_SIZE"], 2)
            self.assertEqual(cfg["DATASET"]["VIDEOS_PER_EPOCH"], cfg["TRAIN"]["BATCH_SIZE"])
            self.assertEqual(sum(cfg["DATASET"][name]["NUM_USE"] for name in cfg["DATASET"]["NAMES"]),
                             cfg["DATASET"]["VIDEOS_PER_EPOCH"])
            self.assertEqual(cfg["TRAIN"]["SNAPSHOT_DIR"], str(ARTIFACT_DIR / stage))
            self.assertTrue(cfg["DATASET"]["NAMES"])
            self.assertTrue(all(cfg["DATASET"][name]["NUM_USE"] > 0 for name in cfg["DATASET"]["NAMES"]))
        self.assertTrue(any("_NORMAL" in name for name in read_config("stage3", True)["DATASET"]["NAMES"]))
        self.assertTrue(any("_HARD" in name for name in read_config("stage3", True)["DATASET"]["NAMES"]))

    def test_smoke_entry_has_policy_specific_cli_mapping(self):
        script = ROOT / "scripts/nanotrack_three_stage_smoke.py"
        result = subprocess.run([str(ROOT / ".venvs/nanotrack/bin/python"), str(script), "--help"],
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn("--stage", result.stdout)

    def test_smoke_report_records_real_cuda_training_reload_and_validation_inference(self):
        report_path = ARTIFACT_DIR / "smoke_report.json"
        self.assertTrue(report_path.is_file(), "run scripts/nanotrack_three_stage_smoke.py first")
        report = json.loads(report_path.read_text(encoding="utf-8"))
        self.assertEqual(report["schema_version"], 1)
        self.assertEqual(set(report["runs"]), set(STAGE_SETTINGS))
        for stage, run in report["runs"].items():
            self.assertTrue(run["cuda_available"])
            self.assertEqual(run["seed"], 123456)
            self.assertGreater(run["forward_backward_steps"], 0)
            self.assertTrue(all(math.isfinite(run["losses"][name]) for name in ("cls", "loc", "total")))
            self.assertLess(run["peak_reserved_mib"], 6144)
            self.assertGreater(run["peak_allocated_mib"], 0)
            self.assertEqual(len(run["checkpoint_sha256"]), 64)
            self.assertTrue(run["checkpoint_reloaded_strictly"])
            self.assertEqual(run["validation"]["split"], "validation")
            self.assertTrue(run["validation"]["initialization_recorded"])
            self.assertGreater(run["validation"]["tracked_frames"], 0)
            self.assertTrue(all(math.isfinite(value) for value in run["validation"]["bbox_xywh"]))
            self.assertTrue(all(len(run[key]) > 0 for key in ("trainable_parameter_names", "frozen_bn_modules")))
            self.assertTrue(run["neck_frozen"])
            for hash_key in ("config_sha256", "panel_sha256"):
                self.assertEqual(len(run[hash_key]), 64)
            self.assertEqual(run["policy"], STAGE_SETTINGS[stage][1])
            self.assertIn("--policy", run["trainer_argv"])
            self.assertEqual(run["trainer_argv"][run["trainer_argv"].index("--policy") + 1], STAGE_SETTINGS[stage][1])
            self.assertEqual(run["checkpoint_sha256"], hashlib.sha256(Path(run["checkpoint_path"]).read_bytes()).hexdigest())
            self.assertTrue(Path(run["config_path"]).is_file())
            self.assertEqual(run["config_sha256"], hashlib.sha256(Path(run["config_path"]).read_bytes()).hexdigest())
            self.assertTrue(Path(run["validation"]["panel_path"]).is_file())
            self.assertEqual(run["panel_sha256"], hashlib.sha256(Path(run["validation"]["panel_path"]).read_bytes()).hexdigest())

    def test_formal_command_config_drives_exact_production_training_argv(self):
        command_path = CONFIG_DIR / "nanotrack_v3_three_stage_commands.json"
        command_config = json.loads(command_path.read_text(encoding="utf-8"))
        factory, run_root, official_checkpoint, artifact_root = runner_module.load_nanotrack_command_config(command_path)
        artifact_root = Path(artifact_root)
        run_root = Path(run_root)
        self.assertEqual(run_root, artifact_root / "run")
        self.assertEqual(official_checkpoint, Path(command_config["official_checkpoint"]).resolve())
        self.assertEqual(command_config["seed"], 123456)
        self.assertEqual(command_config["stage_policies"],
                         {"stage1": "head_only", "stage2": "tail2", "stage3": "head_only"})
        for key in ("python_executable", "nanotrack_root", "official_checkpoint",
                    "uav123_dataset_root", "uav123_prepared_root", "validation_panel", "hidden_panel"):
            self.assertTrue(Path(command_config[key]).exists(), key)
        for index, stage in enumerate(("stage1", "stage2", "stage3"), 1):
            checkpoint = artifact_root / "run" / stage / "candidate_00001250.pth"
            if stage == "stage1":
                initializer = Path(command_config["official_checkpoint"]).resolve()
            elif stage == "stage2":
                initializer = artifact_root / "run/selection/stage1_selected.pth"
            else:
                initializer = artifact_root / "run/selection/cross_stage_best.pth"
            context = {
                "stage": stage,
                "stage_step": 1250,
                "global_step": index * 1250,
                "previous_global_step": (index - 1) * 1250,
                "checkpoint_path": str(checkpoint),
                "input_checkpoint_path": str(initializer),
                "resume_checkpoint_path": None,
            }
            argv = factory("train", context)
            self.assertEqual(argv[0], str(Path(command_config["python_executable"]).resolve()))
            self.assertEqual(argv[argv.index("--config") + 1], str(Path(command_config["stage_configs"][stage]).resolve()))
            self.assertEqual(argv[argv.index("--policy") + 1], command_config["stage_policies"][stage])
            self.assertEqual(argv[argv.index("--seed") + 1], "123456")
            self.assertEqual(argv[argv.index("--stage") + 1], stage)
            self.assertEqual(argv[argv.index("--nanotrack-root") + 1], str(Path(command_config["nanotrack_root"]).resolve()))
            self.assertEqual(argv[argv.index("--pretrained") + 1], str(Path(command_config["official_checkpoint"]).resolve()))
            self.assertEqual(argv[argv.index("--output-dir") + 1], str(checkpoint.parent))
            self.assertEqual(argv[argv.index("--stop-after-global-step") + 1], str(index * 1250))
            self.assertEqual(argv[argv.index("--initialize-from") + 1], str(initializer))
            self.assertEqual(argv[argv.index("--initial-global-step") + 1], str((index - 1) * 1250))
            self.assertNotIn("--resume", argv)
            Path(argv[argv.index("--output-dir") + 1]).resolve().relative_to(artifact_root)
        resume = dict(context)
        resume["stage"] = "stage2"
        resume["resume_checkpoint_path"] = str(artifact_root / "run/stage2/resume.pth")
        resumed = factory("train", resume)
        self.assertEqual(resumed[resumed.index("--resume") + 1], resume["resume_checkpoint_path"])
        self.assertNotIn("--initialize-from", resumed)
        evaluation_context = {
            "stage": "stage3",
            "checkpoint_path": str(artifact_root / "run/stage3/final.pth"),
            "final_checkpoint_path": str(artifact_root / "run/stage3/final.pth"),
            "output_path": str(artifact_root / "run/validation/aerial/result.json"),
            "panel_sha256": "validation-panel-hash",
        }
        aerial_argv = factory("val-aerial", evaluation_context)
        self.assertEqual(aerial_argv[aerial_argv.index("--config") + 1],
                         str(Path(command_config["stage_configs"]["stage3"]).resolve()))
        self.assertEqual(aerial_argv[aerial_argv.index("--output") + 1], evaluation_context["output_path"])
        self.assertEqual(aerial_argv[aerial_argv.index("--device") + 1], command_config["device"])
        self.assertEqual(aerial_argv[aerial_argv.index("--panel") + 1],
                         str(Path(command_config["validation_panel"]).resolve()))
        uav_context = dict(evaluation_context)
        uav_context["output_path"] = str(artifact_root / "run/validation/uav123/result.json")
        uav_argv = factory("uav123", uav_context)
        self.assertEqual(uav_argv[uav_argv.index("--output-dir") + 1], str(Path(uav_context["output_path"]).parent))
        hidden_context = dict(evaluation_context)
        hidden_argv = factory("test-aerial-hidden", hidden_context)
        self.assertEqual(hidden_argv[hidden_argv.index("--panel") + 1],
                         str(Path(command_config["hidden_panel"]).resolve()))
        outside = dict(context)
        outside["checkpoint_path"] = "/tmp/escape/candidate.pth"
        with self.assertRaisesRegex(ValueError, "outside artifact_root"):
            factory("train", outside)


if __name__ == "__main__":
    unittest.main()
