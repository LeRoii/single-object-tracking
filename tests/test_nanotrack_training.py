import json
import random
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace

import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset

import scripts.train_nanotrack_uav123 as training


build_optimizer = training.build_optimizer
configure_model_for_epoch = training.configure_model_for_epoch
normalize_resume_position = training.normalize_resume_position
restore_training_checkpoint = training.restore_training_checkpoint
truncate_metrics_after_global_step = training.truncate_metrics_after_global_step


class FakeModel(nn.Module):
    def __init__(self):
        super().__init__()
        self.backbone = nn.Sequential(nn.Linear(4, 4), nn.BatchNorm1d(4))
        self.neck = nn.Linear(4, 4)
        self.ban_head = nn.Linear(4, 2)


class FakeBackbone(nn.Module):
    def __init__(self):
        super().__init__()
        self.stem = nn.Sequential(nn.Linear(4, 4), nn.BatchNorm1d(4))
        self.features = nn.Sequential(
            nn.Sequential(nn.Linear(4, 4), nn.BatchNorm1d(4), nn.ReLU()),
            nn.Sequential(nn.Linear(4, 4), nn.BatchNorm1d(4), nn.ReLU()),
            nn.Sequential(nn.Linear(4, 4), nn.BatchNorm1d(4), nn.ReLU()),
            nn.Sequential(nn.Linear(4, 4), nn.BatchNorm1d(4), nn.ReLU()),
        )

    def forward(self, value):
        value = self.stem(value)
        return self.features(value)


class FakePolicyModel(nn.Module):
    def __init__(self):
        super().__init__()
        self.backbone = FakeBackbone()
        self.neck = nn.Sequential(nn.Linear(4, 4), nn.BatchNorm1d(4))
        self.ban_head = nn.Sequential(
            nn.Linear(4, 4), nn.BatchNorm1d(4), nn.ReLU(), nn.Linear(4, 2)
        )

    def forward(self, value):
        return self.ban_head(self.neck(self.backbone(value)))


class StochasticIndexDataset(Dataset):
    def __init__(self, length):
        self.length = length
        self.constructor_draw = float(np.random.rand())

    def __len__(self):
        return self.length

    def __getitem__(self, index):
        return {
            "index": index,
            "value": (
                float(index)
                + random.random()
                + float(np.random.rand())
            ),
        }


def trainable_names(model):
    return {
        name for name, parameter in model.named_parameters() if parameter.requires_grad
    }


def require_api(test_case, name):
    test_case.assertTrue(hasattr(training, name), "missing training API: {}".format(name))
    return getattr(training, name)


class NanoTrackTrainingPolicyTest(unittest.TestCase):
    def test_step_lr_schedule_spans_stage_and_uses_log_interpolation(self):
        schedule = require_api(self, "_head_lr_for_step")
        cfg = SimpleNamespace(TRAIN=SimpleNamespace(LR=SimpleNamespace(
            KWARGS=SimpleNamespace(start_lr=2.0e-5, end_lr=8.0e-6)
        )))

        self.assertAlmostEqual(schedule(cfg, 0, 100), 2.0e-5)
        self.assertAlmostEqual(schedule(cfg, 99, 100), 8.0e-6)
        self.assertAlmostEqual(
            schedule(cfg, 49, 100),
            np.exp(np.log(2.0e-5) * (50.0 / 99.0)
                   + np.log(8.0e-6) * (49.0 / 99.0)),
        )
        with self.assertRaises(ValueError):
            schedule(cfg, 100, 100)

    def test_segment_contract_distinguishes_new_stage_from_exact_resume(self):
        validate_segment = require_api(self, "validate_segment_request")

        self.assertEqual(
            validate_segment(
                initial_global_step=7500,
                resume_global_step=None,
                stop_after_global_step=8750,
            ),
            7500,
        )
        self.assertEqual(
            validate_segment(
                initial_global_step=None,
                resume_global_step=8750,
                stop_after_global_step=10000,
            ),
            8750,
        )
        with self.assertRaisesRegex(ValueError, "mutually exclusive"):
            validate_segment(7500, 7500, 8750)
        with self.assertRaisesRegex(ValueError, "candidate boundary"):
            validate_segment(7500, None, 8000)
        with self.assertRaisesRegex(ValueError, "after segment start"):
            validate_segment(7500, None, 7500)

    def test_segment_contract_accepts_runtime_candidate_cadence(self):
        validate_segment = require_api(self, "validate_segment_request")
        self.assertEqual(validate_segment(0, None, 100, 100), 0)
        self.assertEqual(validate_segment(5000, None, 5313, 313), 5000)
        self.assertEqual(validate_segment(None, 5313, 5626, 313), 5313)
        with self.assertRaisesRegex(ValueError, "candidate boundary"):
            validate_segment(0, None, 125, 100)
        with self.assertRaisesRegex(ValueError, "candidate boundary"):
            validate_segment(5000, None, 5312, 313)

    def test_segment_boundary_stops_only_after_exact_candidate_was_saved(self):
        should_stop = require_api(self, "segment_boundary_reached")

        self.assertFalse(should_stop(8749, 8750, {}))
        with self.assertRaisesRegex(RuntimeError, "candidate checkpoint"):
            should_stop(8750, 8750, {"resume": Path("resume.pth")})
        self.assertTrue(
            should_stop(
                8750,
                8750,
                {"candidate": Path("candidate_stage2_g00008750.pth")},
            )
        )

    def test_stage_initialization_loads_weights_but_starts_fresh_optimizer(self):
        initialize = require_api(self, "initialize_training_stage")
        source = FakeModel()
        with torch.no_grad():
            source.ban_head.weight.fill_(7.0)
            source.backbone[0].weight.fill_(3.0)
        source_optimizer = torch.optim.SGD(source.parameters(), lr=0.1, momentum=0.9)
        source_optimizer.zero_grad()
        source.ban_head(torch.ones(1, 4)).sum().backward()
        source_optimizer.step()
        with tempfile.TemporaryDirectory() as directory:
            checkpoint_path = Path(directory) / "selected.pth"
            torch.save({
                "model": source.state_dict(),
                "stage": "stage1",
                "global_step": 7500,
                "optimizer": source_optimizer.state_dict(),
            }, str(checkpoint_path))
            target = FakeModel()
            training.configure_model_for_policy(target, "head_only")
            optimizer = build_optimizer(target, head_lr=0.01)
            progress = initialize(
                target, optimizer, checkpoint_path, stage="stage2",
                initial_global_step=7500,
            )
        self.assertEqual(progress["global_step"], 7500)
        self.assertEqual(progress["stage"], "stage2")
        self.assertTrue(torch.equal(target.ban_head.weight, source.ban_head.weight))
        self.assertEqual(len(optimizer.state), 0)

    def test_segment_request_is_candidate_bounded_and_global_step_monotonic(self):
        validate = require_api(self, "validate_segment_request")
        self.assertEqual(validate(7500, None, 8750), 7500)
        self.assertEqual(validate(None, 8750, 10000), 8750)
        for args, message in (
            ((7500, 7500, 8750), "mutually exclusive"),
            ((7500, None, 8000), "candidate boundary"),
            ((7500, None, 7500), "after segment start"),
        ):
            with self.subTest(args=args), self.assertRaisesRegex(ValueError, message):
                validate(*args)

    def test_segment_stops_only_with_the_requested_immutable_candidate(self):
        reached = require_api(self, "segment_boundary_reached")
        self.assertFalse(reached(8749, 8750, {}))
        with self.assertRaisesRegex(RuntimeError, "candidate checkpoint"):
            reached(8750, 8750, {"resume": Path("resume.pth")})
        self.assertTrue(reached(8750, 8750, {"candidate": Path("candidate.pth")}))

    def _run_toy_steps(self, model, optimizer, loader, count):
        consumed = []
        iterator = iter(loader)
        for _ in range(count):
            batch = next(iterator)
            indices = batch["index"].tolist()
            values = batch["value"].float().view(-1, 1)
            consumed.extend(
                (int(index), float(value))
                for index, value in zip(indices, batch["value"].tolist())
            )
            noise = (
                random.random() + float(np.random.rand()) + float(torch.rand(1))
            )
            optimizer.zero_grad()
            prediction = model(values)
            target = values * 0.25 + noise * 0.01
            ((prediction - target) ** 2).mean().backward()
            optimizer.step()
        del iterator
        return consumed

    def _new_toy_run(self, seed):
        random.seed(seed)
        np.random.seed(seed)
        torch.manual_seed(seed)
        model = nn.Linear(1, 1)
        optimizer = torch.optim.SGD(model.parameters(), lr=0.01, momentum=0.9)
        dataset = StochasticIndexDataset(12)
        return model, optimizer, dataset

    def test_mid_epoch_and_boundary_resume_match_uninterrupted_trajectory(self):
        build_loader = require_api(self, "build_deterministic_data_loader")
        sample_offset = require_api(self, "resume_sample_offset")
        seed = 314159
        batch_size = 2
        steps_per_epoch = 3
        total_steps = 6

        reference_model, reference_optimizer, reference_dataset = self._new_toy_run(seed)
        reference_loader = build_loader(
            reference_dataset,
            batch_size=batch_size,
            num_workers=0,
            seed=seed,
            start_sample=0,
            pin_memory=False,
        )
        reference_consumed = self._run_toy_steps(
            reference_model, reference_optimizer, reference_loader, total_steps
        )
        reference_parameters = {
            name: parameter.detach().clone()
            for name, parameter in reference_model.named_parameters()
        }
        reference_rng = (random.random(), float(np.random.rand()), torch.rand(3))

        for interrupted_step in (2, 3):
            first_model, first_optimizer, first_dataset = self._new_toy_run(seed)
            first_loader = build_loader(
                first_dataset,
                batch_size=batch_size,
                num_workers=0,
                seed=seed,
                start_sample=0,
                pin_memory=False,
            )
            consumed = self._run_toy_steps(
                first_model, first_optimizer, first_loader, interrupted_step
            )
            with tempfile.TemporaryDirectory() as temp_dir:
                checkpoint_path = Path(temp_dir) / "resume.pth"
                training.save_training_checkpoint(
                    checkpoint_path,
                    first_model,
                    first_optimizer,
                    completed_epochs=0,
                    step_in_epoch=interrupted_step,
                    global_step=interrupted_step,
                    mean_epoch_loss=0.0,
                    seed=seed,
                    config="toy",
                    stage="stage1",
                    policy="head_only",
                    metrics_position=interrupted_step,
                    atomic=True,
                )

                resumed_model, resumed_optimizer, resumed_dataset = self._new_toy_run(seed)
                progress = restore_training_checkpoint(
                    resumed_model,
                    resumed_optimizer,
                    checkpoint_path,
                    steps_per_epoch=steps_per_epoch,
                )
                offset = sample_offset(
                    progress["epoch"],
                    progress["step_in_epoch"],
                    steps_per_epoch,
                    batch_size,
                )
                resumed_loader = build_loader(
                    resumed_dataset,
                    batch_size=batch_size,
                    num_workers=0,
                    seed=seed,
                    start_sample=offset,
                    pin_memory=False,
                )
                consumed.extend(
                    self._run_toy_steps(
                        resumed_model,
                        resumed_optimizer,
                        resumed_loader,
                        total_steps - interrupted_step,
                    )
                )

            self.assertEqual([item[0] for item in consumed], list(range(12)))
            self.assertEqual(consumed, reference_consumed)
            for name, parameter in resumed_model.named_parameters():
                self.assertTrue(
                    torch.equal(parameter, reference_parameters[name]),
                    "{} differs after cut {}".format(name, interrupted_step),
                )
            self.assertEqual(random.random(), reference_rng[0])
            self.assertEqual(float(np.random.rand()), reference_rng[1])
            self.assertTrue(torch.equal(torch.rand(3), reference_rng[2]))

    def test_exact_resume_loader_rejects_uncheckpointed_worker_prefetch(self):
        build_loader = require_api(self, "build_deterministic_data_loader")
        seed = 271828
        random.seed(seed)
        np.random.seed(seed)
        torch.manual_seed(seed)
        dataset = StochasticIndexDataset(8)

        with self.assertRaisesRegex(ValueError, "num_workers=0"):
            build_loader(
                dataset,
                batch_size=2,
                num_workers=2,
                seed=seed,
                start_sample=2,
                pin_memory=False,
            )

    def test_legacy_joint_resume_configures_matching_optimizer_groups_before_load(self):
        configure_resume = require_api(self, "configure_legacy_model_for_resume")
        source = FakeModel()
        configure_model_for_epoch(source, epoch=3, backbone_train_epoch=3)
        source_optimizer = build_optimizer(
            source,
            base_lr=2.5e-4,
            backbone_lr_multiplier=0.05,
            momentum=0.9,
            weight_decay=1.0e-4,
        )
        sum(parameter.sum() for parameter in source.parameters()).backward()
        source_optimizer.step()

        with tempfile.TemporaryDirectory() as temp_dir:
            checkpoint_path = Path(temp_dir) / "legacy_joint.pth"
            torch.save(
                {
                    "epoch": 3,
                    "step_in_epoch": 7,
                    "global_step": 27,
                    "state_dict": source.state_dict(),
                    "optimizer": source_optimizer.state_dict(),
                },
                str(checkpoint_path),
            )
            restored = FakeModel()
            policy = configure_resume(restored, checkpoint_path, backbone_train_epoch=3)
            restored_optimizer = build_optimizer(
                restored,
                base_lr=2.5e-4,
                backbone_lr_multiplier=0.05,
                momentum=0.9,
                weight_decay=1.0e-4,
            )
            progress = restore_training_checkpoint(
                restored, restored_optimizer, checkpoint_path
            )

        self.assertEqual(policy, "legacy_joint")
        self.assertTrue(all(parameter.requires_grad for parameter in restored.backbone.parameters()))
        self.assertEqual(len(restored_optimizer.param_groups), 2)
        self.assertEqual(progress["global_step"], 27)

    def test_training_cli_exposes_explicit_policy_and_stage(self):
        result = subprocess.run(
            [sys.executable, str(Path(training.__file__).resolve()), "--help"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            universal_newlines=True,
        )

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("--policy {head_only,tail2}", result.stdout)
        self.assertIn("--stage STAGE", result.stdout)
        self.assertIn("--initial-global-step INITIAL_GLOBAL_STEP", result.stdout)
        self.assertIn("--stop-after-global-step STOP_AFTER_GLOBAL_STEP", result.stdout)
        self.assertIn("--candidate-every-steps CANDIDATE_EVERY_STEPS", result.stdout)
        self.assertIn("--lr-schedule {epoch,step}", result.stdout)

    def test_head_only_policy_has_exact_trainable_names_and_frozen_bn(self):
        configure = require_api(self, "configure_model_for_policy")
        model = FakePolicyModel()

        configure(model, "head_only", freeze_bn=True)
        model.train()

        self.assertEqual(
            trainable_names(model),
            {"ban_head.0.weight", "ban_head.0.bias", "ban_head.3.weight", "ban_head.3.bias"},
        )
        self.assertFalse(any(parameter.requires_grad for parameter in model.backbone.parameters()))
        self.assertFalse(any(parameter.requires_grad for parameter in model.neck.parameters()))
        for module in model.modules():
            if isinstance(module, nn.modules.batchnorm._BatchNorm):
                self.assertFalse(module.training)
                self.assertFalse(module.weight.requires_grad)
                self.assertFalse(module.bias.requires_grad)

    def test_tail2_policy_trains_only_head_and_last_two_backbone_blocks(self):
        configure = require_api(self, "configure_model_for_policy")
        model = FakePolicyModel()

        configure(model, "tail2", freeze_bn=True)
        model.train()

        self.assertEqual(
            trainable_names(model),
            {
                "backbone.features.2.0.weight",
                "backbone.features.2.0.bias",
                "backbone.features.3.0.weight",
                "backbone.features.3.0.bias",
                "ban_head.0.weight",
                "ban_head.0.bias",
                "ban_head.3.weight",
                "ban_head.3.bias",
            },
        )
        self.assertFalse(any(parameter.requires_grad for parameter in model.backbone.stem.parameters()))
        self.assertFalse(any(parameter.requires_grad for parameter in model.backbone.features[:2].parameters()))
        self.assertFalse(any(parameter.requires_grad for parameter in model.neck.parameters()))
        self.assertTrue(model.backbone.features[2].training)
        self.assertTrue(model.backbone.features[3].training)
        for module in model.modules():
            if isinstance(module, nn.modules.batchnorm._BatchNorm):
                self.assertFalse(module.training)
                self.assertFalse(module.weight.requires_grad)
                self.assertFalse(module.bias.requires_grad)

    def test_tail2_bn_statistics_stay_frozen_after_model_train_and_update(self):
        configure = require_api(self, "configure_model_for_policy")
        model = FakePolicyModel()
        configure(model, "tail2", freeze_bn=True)
        optimizer = build_optimizer(
            model,
            head_lr=2.0e-5,
            backbone_lr=1.0e-6,
            momentum=0.9,
            weight_decay=1.0e-4,
        )
        batch_norms = [
            module
            for module in model.modules()
            if isinstance(module, nn.modules.batchnorm._BatchNorm)
        ]
        running_means = [module.running_mean.clone() for module in batch_norms]
        running_vars = [module.running_var.clone() for module in batch_norms]

        model.train()
        self.assertTrue(all(not module.training for module in batch_norms))
        optimizer.zero_grad()
        model(torch.randn(8, 4)).sum().backward()
        optimizer.step()

        for module, expected_mean, expected_var in zip(
            batch_norms, running_means, running_vars
        ):
            self.assertTrue(torch.equal(module.running_mean, expected_mean))
            self.assertTrue(torch.equal(module.running_var, expected_var))

    def test_optimizer_groups_match_trainable_parameters_and_exact_lrs(self):
        configure = require_api(self, "configure_model_for_policy")
        model = FakePolicyModel()
        configure(model, "tail2")

        optimizer = build_optimizer(
            model,
            head_lr=2.0e-5,
            backbone_lr=1.0e-6,
            momentum=0.9,
            weight_decay=1.0e-4,
        )

        self.assertEqual([group["name"] for group in optimizer.param_groups], ["backbone", "head"])
        self.assertEqual([group["lr"] for group in optimizer.param_groups], [1.0e-6, 2.0e-5])
        optimized = [
            parameter
            for group in optimizer.param_groups
            for parameter in group["params"]
        ]
        self.assertEqual({id(parameter) for parameter in optimized}, {
            id(parameter) for parameter in model.parameters() if parameter.requires_grad
        })
        self.assertEqual(len(optimized), len({id(parameter) for parameter in optimized}))

    def test_optimizer_rejects_backbone_lr_for_head_only(self):
        configure = require_api(self, "configure_model_for_policy")
        model = FakePolicyModel()
        configure(model, "head_only")

        with self.assertRaisesRegex(ValueError, "backbone"):
            build_optimizer(
                model,
                head_lr=1.0e-4,
                backbone_lr=1.0e-6,
                momentum=0.9,
                weight_decay=1.0e-4,
            )

    def test_optimizer_rejects_missing_and_duplicate_trainable_parameters(self):
        configure = require_api(self, "configure_model_for_policy")
        missing = FakePolicyModel()
        missing.extra = nn.Linear(4, 4)
        configure(missing, "head_only")
        missing.extra.weight.requires_grad = True
        with self.assertRaisesRegex(ValueError, "missing"):
            build_optimizer(
                missing, head_lr=1.0e-4, momentum=0.9, weight_decay=1.0e-4
            )

        duplicate = FakePolicyModel()
        duplicate.ban_head.shared = duplicate.backbone.features[-1][0]
        configure(duplicate, "tail2")
        with self.assertRaisesRegex(ValueError, "duplicate"):
            build_optimizer(
                duplicate,
                head_lr=2.0e-5,
                backbone_lr=1.0e-6,
                momentum=0.9,
                weight_decay=1.0e-4,
            )

    def test_periodic_checkpoint_cadence_names_candidates_and_never_overwrites(self):
        save_periodic = require_api(self, "save_periodic_checkpoints")
        model = FakeModel()
        optimizer = torch.optim.SGD(model.parameters(), lr=0.1)
        with tempfile.TemporaryDirectory() as temp_dir:
            output_dir = Path(temp_dir)
            nothing = save_periodic(
                output_dir, model, optimizer, stage="stage2", epoch=1,
                step_in_epoch=249, global_step=249, policy="tail2",
                metrics_position=249,
            )
            resume_only = save_periodic(
                output_dir, model, optimizer, stage="stage2", epoch=1,
                step_in_epoch=250, global_step=250, policy="tail2",
                metrics_position=250,
            )
            both = save_periodic(
                output_dir, model, optimizer, stage="stage2", epoch=1,
                step_in_epoch=1250, global_step=1250, policy="tail2",
                metrics_position=1250,
            )
            candidate = output_dir / "candidate_stage2_g00001250_e001_s01250.pth"

            self.assertEqual(nothing, {})
            self.assertEqual(resume_only, {"resume": output_dir / "resume.pth"})
            self.assertEqual(
                both, {"resume": output_dir / "resume.pth", "candidate": candidate}
            )
            self.assertTrue(candidate.is_file())
            original = candidate.read_bytes()
            with self.assertRaises(FileExistsError):
                save_periodic(
                    output_dir, model, optimizer, stage="stage2", epoch=1,
                    step_in_epoch=1250, global_step=1250, policy="tail2",
                    metrics_position=1250,
                )
            self.assertEqual(candidate.read_bytes(), original)
            self.assertEqual(list(output_dir.glob("*.tmp")), [])

    def test_periodic_checkpoint_supports_runtime_candidate_and_resume_cadence(self):
        save_periodic = require_api(self, "save_periodic_checkpoints")
        model = FakeModel()
        optimizer = torch.optim.SGD(model.parameters(), lr=0.1)
        with tempfile.TemporaryDirectory() as temp_dir:
            output_dir = Path(temp_dir)
            saved = save_periodic(
                output_dir, model, optimizer, stage="stage1", epoch=0,
                step_in_epoch=100, global_step=100, policy="head_only",
                metrics_position=100, candidate_checkpoint_steps=100,
                resume_checkpoint_steps=25,
            )
            candidate = output_dir / "candidate_stage1_g00000100_e000_s00100.pth"
            self.assertEqual(saved, {
                "resume": output_dir / "resume.pth", "candidate": candidate})
            self.assertTrue(candidate.is_file())

    def test_candidate_cadence_is_stage_relative_when_global_step_is_offset(self):
        save_periodic = require_api(self, "save_periodic_checkpoints")
        model = FakeModel()
        optimizer = torch.optim.SGD(model.parameters(), lr=0.1)
        with tempfile.TemporaryDirectory() as directory:
            output_dir = Path(directory)
            saved = save_periodic(
                output_dir, model, optimizer, stage="stage1", epoch=0,
                step_in_epoch=313, global_step=5313, policy="head_only",
                metrics_position=5313, stage_step=313,
                candidate_checkpoint_steps=313,
            )
            self.assertEqual(
                saved["candidate"],
                output_dir / "candidate_stage1_g00005313_e000_s00313.pth",
            )

    def test_resume_checkpoint_metadata_and_python_numpy_torch_rng_restore(self):
        save_periodic = require_api(self, "save_periodic_checkpoints")
        model = FakeModel()
        optimizer = torch.optim.SGD(model.parameters(), lr=0.1)
        random.seed(17)
        np.random.seed(23)
        torch.manual_seed(29)
        with tempfile.TemporaryDirectory() as temp_dir:
            output_dir = Path(temp_dir)
            save_periodic(
                output_dir, model, optimizer, stage="stage1", epoch=2,
                step_in_epoch=250, global_step=750, policy="head_only",
                metrics_position=748, lr_schedule="step",
            )
            expected = (random.random(), float(np.random.rand()), torch.rand(3))
            random.seed(101)
            np.random.seed(103)
            torch.manual_seed(107)

            progress = restore_training_checkpoint(
                model, optimizer, output_dir / "resume.pth"
            )
            checkpoint = torch.load(str(output_dir / "resume.pth"), map_location="cpu")

        self.assertEqual(
            progress,
            {
                "stage": "stage1", "epoch": 2, "step_in_epoch": 250,
                "global_step": 750, "stage_step": 750,
                "policy": "head_only", "metrics_position": 748,
                "seed": 123456, "config": "", "lr_schedule": "step",
            },
        )
        self.assertIn("model", checkpoint)
        self.assertIn("rng_state", checkpoint)
        self.assertEqual(random.random(), expected[0])
        self.assertEqual(float(np.random.rand()), expected[1])
        self.assertTrue(torch.equal(torch.rand(3), expected[2]))

    def test_restore_truncates_metrics_and_boundary_resume_stays_monotonic(self):
        save_checkpoint = require_api(self, "save_training_checkpoint")
        append_metric = require_api(self, "append_metric_record")
        model = FakeModel()
        optimizer = torch.optim.SGD(model.parameters(), lr=0.1)
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            metrics_path = root / "train_metrics.jsonl"
            for step in range(6, 10):
                append_metric(metrics_path, {"global_step": step, "loss": step / 10.0})
            save_checkpoint(
                root / "resume.pth", model, optimizer, completed_epochs=2,
                step_in_epoch=4, global_step=8, mean_epoch_loss=0.2,
                seed=123456, config="config", stage="stage1",
                policy="head_only", metrics_position=8, atomic=True,
            )

            progress = restore_training_checkpoint(
                model, optimizer, root / "resume.pth", metrics_path=metrics_path,
                steps_per_epoch=4,
            )
            lines = [json.loads(line) for line in metrics_path.read_text().splitlines()]
            append_metric(metrics_path, {"global_step": 9, "loss": 0.9})
            with self.assertRaisesRegex(ValueError, "strictly increasing"):
                append_metric(metrics_path, {"global_step": 9, "loss": 0.9})

        self.assertEqual(progress["epoch"], 3)
        self.assertEqual(progress["step_in_epoch"], 0)
        self.assertEqual(progress["global_step"], 8)
        self.assertEqual([record["global_step"] for record in lines], [6, 7, 8])

    def test_resume_at_epoch_boundary_advances_to_next_epoch(self):
        self.assertEqual(normalize_resume_position(2, 2500, 2500), (3, 0))
        self.assertEqual(normalize_resume_position(2, 2499, 2500), (2, 2499))
        with self.assertRaises(ValueError):
            normalize_resume_position(2, 2501, 2500)

    def test_restore_training_checkpoint_restores_model_optimizer_and_progress(self):
        model = FakeModel()
        optimizer = torch.optim.SGD(model.parameters(), lr=0.1, momentum=0.9)
        loss = sum(parameter.sum() for parameter in model.parameters())
        loss.backward()
        optimizer.step()
        expected = {name: value.detach().clone() for name, value in model.state_dict().items()}
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "checkpoint.pth"
            torch.save(
                {
                    "epoch": 2,
                    "global_step": 17,
                    "step_in_epoch": 9,
                    "state_dict": model.state_dict(),
                    "optimizer": optimizer.state_dict(),
                },
                str(path),
            )
            restored_model = FakeModel()
            restored_optimizer = torch.optim.SGD(
                restored_model.parameters(), lr=0.01, momentum=0.9
            )

            progress = restore_training_checkpoint(
                restored_model, restored_optimizer, path
            )

        self.assertEqual(progress["epoch"], 2)
        self.assertEqual(progress["global_step"], 17)
        self.assertEqual(progress["step_in_epoch"], 9)
        for name, value in restored_model.state_dict().items():
            self.assertTrue(torch.equal(value, expected[name]), name)
        self.assertTrue(restored_optimizer.state_dict()["state"])

    def test_truncate_metrics_discards_records_newer_than_checkpoint(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "train_metrics.jsonl"
            path.write_text(
                "".join(
                    '{{"global_step": {}}}\n'.format(step)
                    for step in (1, 2, 3, 4)
                ),
                encoding="utf-8",
            )

            kept = truncate_metrics_after_global_step(path, 2)

            self.assertEqual(kept, 2)
            self.assertEqual(
                path.read_text(encoding="utf-8"),
                '{"global_step": 1}\n{"global_step": 2}\n',
            )

    def test_head_only_stage_freezes_backbone_and_unused_neck(self):
        model = FakeModel()

        configure_model_for_epoch(model, epoch=0, backbone_train_epoch=3)

        self.assertFalse(any(parameter.requires_grad for parameter in model.backbone.parameters()))
        self.assertFalse(any(parameter.requires_grad for parameter in model.neck.parameters()))
        self.assertTrue(all(parameter.requires_grad for parameter in model.ban_head.parameters()))
        self.assertFalse(model.backbone[1].training)

    def test_joint_stage_unfreezes_backbone_but_never_neck(self):
        model = FakeModel()

        configure_model_for_epoch(model, epoch=3, backbone_train_epoch=3)

        self.assertTrue(all(parameter.requires_grad for parameter in model.backbone.parameters()))
        self.assertFalse(any(parameter.requires_grad for parameter in model.neck.parameters()))
        self.assertTrue(model.backbone[1].training)

    def test_optimizer_has_head_and_low_lr_backbone_groups_only(self):
        model = FakeModel()
        configure_model_for_epoch(model, epoch=3, backbone_train_epoch=3)

        optimizer = build_optimizer(
            model, base_lr=2.5e-4, backbone_lr_multiplier=0.05, momentum=0.9, weight_decay=1e-4
        )

        self.assertEqual([group["name"] for group in optimizer.param_groups], ["backbone", "head"])
        self.assertAlmostEqual(optimizer.param_groups[0]["lr"], 1.25e-5)
        self.assertAlmostEqual(optimizer.param_groups[1]["lr"], 2.5e-4)
        optimized = {id(parameter) for group in optimizer.param_groups for parameter in group["params"]}
        self.assertFalse(any(id(parameter) in optimized for parameter in model.neck.parameters()))


if __name__ == "__main__":
    unittest.main()
