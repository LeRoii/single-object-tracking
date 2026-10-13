#!/usr/bin/env python3
"""Single-GPU NanoTrack fine-tuning entry point for prepared SOT datasets."""

import argparse
import json
import math
import os
import random
import re
import sys
import tempfile
import time
from pathlib import Path
from types import MethodType
from typing import Any, Dict, List, Optional

import numpy as np
import torch
import torch.nn as nn
from torch.nn.utils import clip_grad_norm_
from torch.utils.data import DataLoader, Dataset, Sampler


POLICIES = ("head_only", "tail2")
RESUME_CHECKPOINT_STEPS = 250
CANDIDATE_CHECKPOINT_STEPS = 1250


def validate_segment_request(
    initial_global_step: Optional[int],
    resume_global_step: Optional[int],
    stop_after_global_step: Optional[int],
    candidate_checkpoint_steps: int = CANDIDATE_CHECKPOINT_STEPS,
) -> int:
    """Validate a bounded segment and return its exact starting global step."""
    if initial_global_step is not None and resume_global_step is not None:
        raise ValueError("initial-global-step and resume are mutually exclusive")
    start = int(resume_global_step if resume_global_step is not None else initial_global_step or 0)
    if stop_after_global_step is None:
        return start
    stop = int(stop_after_global_step)
    if stop <= start:
        raise ValueError("stop-after-global-step must be after segment start")
    cadence = int(candidate_checkpoint_steps)
    if cadence <= 0:
        raise ValueError("candidate checkpoint cadence must be positive")
    if (stop - start) % cadence:
        raise ValueError("stop-after-global-step must end at a candidate boundary")
    return start


def segment_boundary_reached(
    global_step: int, stop_after_global_step: Optional[int], saved_paths: Dict[str, Path]
) -> bool:
    if stop_after_global_step is None or global_step < stop_after_global_step:
        return False
    if global_step > stop_after_global_step:
        raise RuntimeError("training crossed requested segment boundary")
    if not saved_paths.get("candidate"):
        raise RuntimeError("requested boundary reached without candidate checkpoint")
    return True


def initialize_training_stage(
    model: nn.Module,
    optimizer: torch.optim.Optimizer,
    path: Path,
    stage: str,
    initial_global_step: int,
) -> Dict[str, Any]:
    """Initialize a new stage from selected weights with a fresh optimizer."""
    checkpoint = torch.load(str(path), map_location="cpu")
    if not isinstance(checkpoint, dict):
        raise ValueError("stage initializer must contain a checkpoint mapping")
    model_state = checkpoint.get("model", checkpoint.get("state_dict"))
    raw_state = model_state is None
    if raw_state:
        model_state = checkpoint
    if not isinstance(model_state, dict):
        raise ValueError("stage initializer is missing model weights")
    if model_state and all(str(key).startswith("module.") for key in model_state):
        model_state = {str(key)[len("module."):]: value for key, value in model_state.items()}
    source_step = int(checkpoint.get("global_step", 0))
    if source_step < 0 or int(initial_global_step) < source_step:
        raise ValueError("stage initializer would move the run global_step backwards")
    if raw_state:
        result = model.load_state_dict(model_state, strict=False)
        if len(model_state) == len(result.unexpected_keys):
            prefixed = {"features." + key: value for key, value in model_state.items()}
            result = model.load_state_dict(prefixed, strict=False)
        if len(model_state) == len(result.unexpected_keys):
            raise ValueError("stage initializer weights do not match the NanoTrack model")
    else:
        model.load_state_dict(model_state, strict=True)
    if optimizer.state:
        raise ValueError("new stage optimizer must be fresh")
    return {
        "stage": stage,
        "global_step": int(initial_global_step),
        "stage_step": 0,
        "epoch": 0,
        "step_in_epoch": 0,
        "seed": None,
        "config": None,
        "metrics_position": int(initial_global_step),
    }


class _DeterministicSampleDataset(Dataset):
    """Make NanoTrack's Python/NumPy augmentation a pure function of index."""

    def __init__(self, dataset: Dataset, seed: int):
        self.dataset = dataset
        self.seed = int(seed)

    def __len__(self) -> int:
        return len(self.dataset)

    def __getitem__(self, index: int):
        python_state = random.getstate()
        numpy_state = np.random.get_state()
        sample_seed = (self.seed * 1000003 + int(index)) & 0xFFFFFFFF
        try:
            random.seed(sample_seed)
            np.random.seed(sample_seed)
            return self.dataset[index]
        finally:
            random.setstate(python_state)
            np.random.set_state(numpy_state)


class _ResumeSequentialSampler(Sampler):
    def __init__(self, data_source: Dataset, start_sample: int):
        super().__init__(data_source)
        self.data_source = data_source
        self.start_sample = int(start_sample)

    def __iter__(self):
        return iter(range(self.start_sample, len(self.data_source)))

    def __len__(self) -> int:
        return len(self.data_source) - self.start_sample


def resume_sample_offset(
    epoch: int, step_in_epoch: int, steps_per_epoch: int, batch_size: int
) -> int:
    if epoch < 0 or step_in_epoch < 0:
        raise ValueError("resume epoch and step must be non-negative")
    if steps_per_epoch <= 0 or batch_size <= 0:
        raise ValueError("steps_per_epoch and batch_size must be positive")
    if step_in_epoch >= steps_per_epoch:
        raise ValueError("normalize epoch-boundary progress before computing sample offset")
    return (epoch * steps_per_epoch + step_in_epoch) * batch_size


def build_deterministic_data_loader(
    dataset: Dataset,
    batch_size: int,
    num_workers: int,
    seed: int,
    start_sample: int,
    pin_memory: bool = True,
) -> DataLoader:
    if batch_size <= 0:
        raise ValueError("batch_size must be positive")
    if num_workers != 0:
        raise ValueError(
            "exact-resume deterministic loading requires num_workers=0; "
            "worker prefetch state is not checkpointed"
        )
    if start_sample < 0 or start_sample > len(dataset):
        raise ValueError("start_sample is outside the dataset")
    if start_sample % batch_size != 0:
        raise ValueError("start_sample must be aligned to a complete batch")
    deterministic_dataset = _DeterministicSampleDataset(dataset, seed)
    sampler = _ResumeSequentialSampler(deterministic_dataset, start_sample)
    loader_generator = torch.Generator()
    loader_generator.manual_seed(int(seed))
    return DataLoader(
        deterministic_dataset,
        batch_size=batch_size,
        sampler=sampler,
        num_workers=num_workers,
        pin_memory=pin_memory,
        drop_last=True,
        generator=loader_generator,
    )


def _tail_blocks(model: nn.Module) -> List[nn.Module]:
    features = getattr(model.backbone, "features", None)
    if features is None:
        raise ValueError("tail2 policy requires model.backbone.features")
    blocks = list(features.children())
    if len(blocks) < 2:
        raise ValueError("tail2 policy requires at least two backbone feature blocks")
    return blocks[-2:]


def _set_requires_grad(module: nn.Module, enabled: bool) -> None:
    for parameter in module.parameters():
        parameter.requires_grad = enabled


def _enforce_policy_modes(model: nn.Module) -> None:
    policy = getattr(model, "_nanotrack_trainability_policy", None)
    if policy not in POLICIES:
        return
    root_training = bool(model.training)
    model.backbone.eval()
    if policy == "tail2":
        for block in _tail_blocks(model):
            block.train(root_training)
    if hasattr(model, "neck"):
        model.neck.eval()
    model.ban_head.train(root_training)
    if bool(getattr(model, "_nanotrack_freeze_bn", False)):
        for module in model.modules():
            if isinstance(module, nn.modules.batchnorm._BatchNorm):
                module.eval()
                if module.affine:
                    module.weight.requires_grad = False
                    module.bias.requires_grad = False


def _policy_aware_train(self: nn.Module, mode: bool = True) -> nn.Module:
    self._nanotrack_original_train(mode)
    _enforce_policy_modes(self)
    return self


def _install_policy_train_enforcement(model: nn.Module) -> None:
    if bool(getattr(model, "_nanotrack_policy_train_installed", False)):
        return
    model._nanotrack_original_train = model.train
    model.train = MethodType(_policy_aware_train, model)
    model._nanotrack_policy_train_installed = True


def _remove_policy_train_enforcement(model: nn.Module) -> None:
    if not bool(getattr(model, "_nanotrack_policy_train_installed", False)):
        return
    model.train = model._nanotrack_original_train
    del model._nanotrack_original_train
    del model._nanotrack_policy_train_installed
    if hasattr(model, "_nanotrack_trainability_policy"):
        del model._nanotrack_trainability_policy
    if hasattr(model, "_nanotrack_freeze_bn"):
        del model._nanotrack_freeze_bn


def configure_model_for_policy(
    model: nn.Module, policy: str, freeze_bn: bool = True
) -> None:
    if policy not in POLICIES:
        raise ValueError("unsupported trainability policy: {}".format(policy))
    if not hasattr(model, "backbone") or not hasattr(model, "ban_head"):
        raise ValueError("model must expose backbone and ban_head modules")

    for parameter in model.parameters():
        parameter.requires_grad = False
    _set_requires_grad(model.ban_head, True)
    if policy == "tail2":
        for block in _tail_blocks(model):
            _set_requires_grad(block, True)
    if hasattr(model, "neck"):
        _set_requires_grad(model.neck, False)

    model._nanotrack_trainability_policy = policy
    model._nanotrack_freeze_bn = bool(freeze_bn)
    _install_policy_train_enforcement(model)
    model.train(model.training)


def configure_model_for_epoch(model: nn.Module, epoch: int, backbone_train_epoch: int) -> None:
    _remove_policy_train_enforcement(model)
    train_backbone = epoch >= backbone_train_epoch
    for parameter in model.backbone.parameters():
        parameter.requires_grad = train_backbone
    model.backbone.train(train_backbone)
    for module in model.backbone.modules():
        if isinstance(module, nn.BatchNorm2d) or isinstance(module, nn.BatchNorm1d):
            module.train(train_backbone)

    if hasattr(model, "neck"):
        for parameter in model.neck.parameters():
            parameter.requires_grad = False
        model.neck.eval()

    for parameter in model.ban_head.parameters():
        parameter.requires_grad = True
    model.ban_head.train(True)


def configure_legacy_model_for_resume(
    model: nn.Module, path: Path, backbone_train_epoch: int
) -> str:
    checkpoint = torch.load(str(path), map_location="cpu")
    optimizer_state = checkpoint.get("optimizer")
    if not isinstance(optimizer_state, dict):
        raise ValueError("resume checkpoint is missing optimizer")
    group_count = len(optimizer_state.get("param_groups", []))
    recorded_policy = checkpoint.get("policy")
    if recorded_policy == "legacy_joint":
        policy = "legacy_joint"
    elif recorded_policy == "legacy_head":
        policy = "legacy_head"
    elif recorded_policy is not None:
        raise ValueError(
            "checkpoint policy {} is not a legacy policy".format(recorded_policy)
        )
    elif group_count == 2 and int(checkpoint.get("epoch", 0)) >= backbone_train_epoch:
        policy = "legacy_joint"
    elif group_count == 1:
        policy = "legacy_head"
    elif group_count == 2:
        raise ValueError(
            "old head-only checkpoint contains frozen backbone optimizer parameters"
        )
    else:
        raise ValueError(
            "legacy checkpoint has unsupported optimizer group count {}".format(
                group_count
            )
        )
    configure_model_for_epoch(
        model,
        epoch=backbone_train_epoch if policy == "legacy_joint" else 0,
        backbone_train_epoch=backbone_train_epoch,
    )
    return policy


def build_optimizer(
    model: nn.Module,
    base_lr: Optional[float] = None,
    backbone_lr_multiplier: Optional[float] = None,
    momentum: float = 0.9,
    weight_decay: float = 0.0,
    head_lr: Optional[float] = None,
    backbone_lr: Optional[float] = None,
) -> torch.optim.Optimizer:
    resolved_head_lr = head_lr if head_lr is not None else base_lr
    if resolved_head_lr is None:
        raise ValueError("head_lr or base_lr is required")
    if resolved_head_lr < 0:
        raise ValueError("head learning rate must be non-negative")

    backbone_parameters = [
        parameter for parameter in model.backbone.parameters() if parameter.requires_grad
    ]
    head_parameters = [
        parameter for parameter in model.ban_head.parameters() if parameter.requires_grad
    ]
    grouped_parameters = backbone_parameters + head_parameters
    grouped_ids = [id(parameter) for parameter in grouped_parameters]
    if len(grouped_ids) != len(set(grouped_ids)):
        raise ValueError("duplicate trainable parameter across optimizer groups")
    trainable_ids = {
        id(parameter) for parameter in model.parameters() if parameter.requires_grad
    }
    grouped_id_set = set(grouped_ids)
    missing = trainable_ids - grouped_id_set
    if missing:
        raise ValueError("missing {} trainable parameter(s) from optimizer groups".format(len(missing)))
    if grouped_id_set - trainable_ids:
        raise ValueError("optimizer groups contain frozen parameters")
    if not head_parameters:
        raise ValueError("BAN head has no trainable parameters")

    parameter_groups = []
    if backbone_parameters:
        resolved_backbone_lr = backbone_lr
        if resolved_backbone_lr is None:
            if base_lr is None or backbone_lr_multiplier is None:
                raise ValueError("backbone_lr is required when backbone parameters are trainable")
            resolved_backbone_lr = base_lr * backbone_lr_multiplier
        if resolved_backbone_lr < 0:
            raise ValueError("backbone learning rate must be non-negative")
        multiplier = (
            resolved_backbone_lr / resolved_head_lr if resolved_head_lr else 0.0
        )
        parameter_groups.append(
            {
                "name": "backbone",
                "params": backbone_parameters,
                "lr": resolved_backbone_lr,
                "lr_multiplier": multiplier,
            }
        )
    elif backbone_lr is not None:
        raise ValueError("backbone_lr is invalid when no backbone parameter is trainable")
    parameter_groups.append(
        {
            "name": "head",
            "params": head_parameters,
            "lr": resolved_head_lr,
            "lr_multiplier": 1.0,
        }
    )
    return torch.optim.SGD(
        parameter_groups,
        lr=resolved_head_lr,
        momentum=momentum,
        weight_decay=weight_decay,
    )


def restore_training_checkpoint(
    model: nn.Module,
    optimizer: torch.optim.Optimizer,
    path: Path,
    metrics_path: Optional[Path] = None,
    steps_per_epoch: Optional[int] = None,
) -> Dict[str, Any]:
    checkpoint = torch.load(str(path), map_location="cpu")
    required = {"epoch", "optimizer"}
    missing = sorted(required - set(checkpoint))
    if missing:
        raise ValueError("resume checkpoint is missing {}".format(", ".join(missing)))
    model_state = checkpoint.get("model", checkpoint.get("state_dict"))
    if model_state is None:
        raise ValueError("resume checkpoint is missing model/state_dict")
    model.load_state_dict(model_state)
    optimizer.load_state_dict(checkpoint["optimizer"])
    device = next(model.parameters()).device
    for state in optimizer.state.values():
        for key, value in list(state.items()):
            if torch.is_tensor(value):
                state[key] = value.to(device)
    progress = {
        "stage": checkpoint.get("stage"),
        "stage_step": int(checkpoint.get("stage_step", checkpoint.get("global_step", 0))),
        "epoch": int(checkpoint["epoch"]),
        "step_in_epoch": int(checkpoint.get("step_in_epoch", 0)),
        "global_step": int(checkpoint.get("global_step", 0)),
        "policy": checkpoint.get("policy"),
        "metrics_position": int(
            checkpoint.get("metrics_position", checkpoint.get("global_step", 0))
        ),
        "seed": checkpoint.get("seed"),
        "config": checkpoint.get("config"),
        "lr_schedule": checkpoint.get("lr_schedule"),
    }
    for key in ("epoch", "step_in_epoch", "global_step", "metrics_position"):
        if progress[key] < 0:
            raise ValueError("checkpoint {} must be non-negative".format(key))
    if progress["metrics_position"] > progress["global_step"]:
        raise ValueError("metrics_position cannot exceed global_step")
    if steps_per_epoch is not None:
        progress["epoch"], progress["step_in_epoch"] = normalize_resume_position(
            progress["epoch"], progress["step_in_epoch"], steps_per_epoch
        )
    if metrics_path is not None:
        truncate_metrics_after_global_step(metrics_path, progress["metrics_position"])
    if "rng_state" in checkpoint:
        _restore_rng_state(checkpoint["rng_state"])
    return progress


def _capture_rng_state() -> Dict[str, Any]:
    state = {
        "python": random.getstate(),
        "numpy": np.random.get_state(),
        "torch_cpu": torch.get_rng_state(),
    }
    if torch.cuda.is_available():
        state["torch_cuda"] = torch.cuda.get_rng_state_all()
    return state


def _restore_rng_state(state: Dict[str, Any]) -> None:
    required = {"python", "numpy", "torch_cpu"}
    missing = sorted(required - set(state))
    if missing:
        raise ValueError("checkpoint RNG state is missing {}".format(", ".join(missing)))
    random.setstate(state["python"])
    np.random.set_state(state["numpy"])
    torch.set_rng_state(state["torch_cpu"])
    if "torch_cuda" in state and torch.cuda.is_available():
        torch.cuda.set_rng_state_all(state["torch_cuda"])


def _atomic_torch_save(value: Dict[str, Any], path: Path, immutable: bool) -> None:
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    if immutable and path.exists():
        raise FileExistsError("candidate checkpoint already exists: {}".format(path))
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=".{}-".format(path.name), suffix=".tmp", dir=str(path.parent)
    )
    os.close(descriptor)
    temporary_path = Path(temporary_name)
    try:
        torch.save(value, str(temporary_path))
        if immutable:
            os.link(str(temporary_path), str(path))
            temporary_path.unlink()
        else:
            os.replace(str(temporary_path), str(path))
    finally:
        if temporary_path.exists():
            temporary_path.unlink()


def _checkpoint_payload(
    model: nn.Module,
    optimizer: torch.optim.Optimizer,
    completed_epochs: int,
    step_in_epoch: int,
    global_step: int,
    mean_epoch_loss: float,
    seed: int,
    config: str,
    stage: Optional[str],
    policy: Optional[str],
    metrics_position: Optional[int],
    stage_step: Optional[int],
    lr_schedule: str = "epoch",
) -> Dict[str, Any]:
    model_state = model.state_dict()
    return {
        "model": model_state,
        "state_dict": model_state,
        "optimizer": optimizer.state_dict(),
        "stage": stage,
        "epoch": completed_epochs,
        "step_in_epoch": step_in_epoch,
        "global_step": global_step,
        "stage_step": global_step if stage_step is None else int(stage_step),
        "policy": policy,
        "rng_state": _capture_rng_state(),
        "metrics_position": global_step if metrics_position is None else metrics_position,
        "mean_epoch_loss": mean_epoch_loss,
        "seed": seed,
        "config": config,
        "lr_schedule": lr_schedule,
    }


def truncate_metrics_after_global_step(path: Path, global_step: int) -> int:
    """Keep metrics committed no later than the checkpoint being resumed."""
    if not path.exists():
        return 0
    kept: List[str] = []
    with path.open("r", encoding="utf-8") as handle:
        for line in handle:
            if not line.strip():
                continue
            record = json.loads(line)
            if int(record["global_step"]) <= global_step:
                kept.append(line if line.endswith("\n") else line + "\n")
    with path.open("w", encoding="utf-8") as handle:
        handle.writelines(kept)
    return len(kept)


def append_metric_record(path: Path, value: Dict[str, object]) -> None:
    if "global_step" not in value:
        raise ValueError("metric record requires global_step")
    global_step = int(value["global_step"])
    last_step = None
    if path.exists():
        with path.open("rb") as handle:
            handle.seek(0, os.SEEK_END)
            position = handle.tell()
            tail = b""
            while position > 0:
                size = min(4096, position)
                position -= size
                handle.seek(position)
                tail = handle.read(size) + tail
                stripped = tail.rstrip()
                newline = stripped.rfind(b"\n")
                if newline >= 0 or position == 0:
                    line = stripped[newline + 1 :] if newline >= 0 else stripped
                    if line:
                        last_step = int(json.loads(line.decode("utf-8"))["global_step"])
                    break
    if last_step is not None and global_step <= last_step:
        raise ValueError("metric global_step must be strictly increasing")
    with path.open("a", encoding="utf-8") as handle:
        handle.write(json.dumps(value, sort_keys=True) + "\n")


def normalize_resume_position(
    completed_epochs: int, step_in_epoch: int, steps_per_epoch: int
):
    if step_in_epoch < 0 or step_in_epoch > steps_per_epoch:
        raise ValueError("step_in_epoch must be between zero and steps_per_epoch")
    if step_in_epoch == steps_per_epoch:
        return completed_epochs + 1, 0
    return completed_epochs, step_in_epoch


def save_training_checkpoint(
    path: Path,
    model: nn.Module,
    optimizer: torch.optim.Optimizer,
    completed_epochs: int,
    step_in_epoch: int,
    global_step: int,
    mean_epoch_loss: float,
    seed: int,
    config: str,
    stage: Optional[str] = None,
    policy: Optional[str] = None,
    metrics_position: Optional[int] = None,
    atomic: bool = False,
    immutable: bool = False,
    stage_step: Optional[int] = None,
    lr_schedule: str = "epoch",
) -> Dict[str, object]:
    if completed_epochs < 0 or step_in_epoch < 0 or global_step < 0:
        raise ValueError("checkpoint progress values must be non-negative")
    if metrics_position is not None and (
        metrics_position < 0 or metrics_position > global_step
    ):
        raise ValueError("metrics_position must be between zero and global_step")
    checkpoint = _checkpoint_payload(
        model,
        optimizer,
        completed_epochs,
        step_in_epoch,
        global_step,
        mean_epoch_loss,
        seed,
        config,
        stage,
        policy,
        metrics_position,
        stage_step,
        lr_schedule,
    )
    if atomic or immutable:
        _atomic_torch_save(checkpoint, path, immutable=immutable)
    else:
        torch.save(checkpoint, str(path))
    return checkpoint


def _candidate_checkpoint_path(
    output_dir: Path, stage: str, epoch: int, step_in_epoch: int, global_step: int
) -> Path:
    if not re.match(r"^[A-Za-z0-9_-]+$", stage):
        raise ValueError("stage must contain only letters, digits, underscores, or hyphens")
    return output_dir / "candidate_{}_g{:08d}_e{:03d}_s{:05d}.pth".format(
        stage, global_step, epoch, step_in_epoch
    )


def save_periodic_checkpoints(
    output_dir: Path,
    model: nn.Module,
    optimizer: torch.optim.Optimizer,
    stage: str,
    epoch: int,
    step_in_epoch: int,
    global_step: int,
    policy: str,
    metrics_position: int,
    mean_epoch_loss: float = float("nan"),
    seed: int = 123456,
    config: str = "",
    stage_step: Optional[int] = None,
    candidate_checkpoint_steps: int = CANDIDATE_CHECKPOINT_STEPS,
    resume_checkpoint_steps: int = RESUME_CHECKPOINT_STEPS,
    lr_schedule: str = "epoch",
) -> Dict[str, Path]:
    if global_step <= 0:
        raise ValueError("global_step must be positive for periodic checkpoints")
    candidate_cadence = int(candidate_checkpoint_steps)
    resume_cadence = int(resume_checkpoint_steps)
    if candidate_cadence <= 0 or resume_cadence <= 0:
        raise ValueError("checkpoint cadences must be positive")
    paths = {}
    candidate_position = global_step if stage_step is None else int(stage_step)
    if candidate_position <= 0:
        raise ValueError("candidate checkpoint position must be positive")
    candidate_path = None
    if candidate_position % candidate_cadence == 0:
        candidate_path = _candidate_checkpoint_path(
            Path(output_dir), stage, epoch, step_in_epoch, global_step
        )
        if candidate_path.exists():
            raise FileExistsError(
                "candidate checkpoint already exists: {}".format(candidate_path)
            )
    common = {
        "model": model,
        "optimizer": optimizer,
        "completed_epochs": epoch,
        "step_in_epoch": step_in_epoch,
        "global_step": global_step,
        "mean_epoch_loss": mean_epoch_loss,
        "seed": seed,
        "config": config,
        "stage": stage,
        "policy": policy,
        "metrics_position": metrics_position,
        "stage_step": stage_step,
        "lr_schedule": lr_schedule,
    }
    if global_step % resume_cadence == 0:
        resume_path = Path(output_dir) / "resume.pth"
        save_training_checkpoint(resume_path, atomic=True, **common)
        paths["resume"] = resume_path
    if candidate_path is not None:
        save_training_checkpoint(candidate_path, atomic=True, immutable=True, **common)
        paths["candidate"] = candidate_path
    return paths


def _head_lr_for_epoch(cfg: object, epoch: int) -> float:
    warmup_enabled = bool(cfg.TRAIN.LR_WARMUP.WARMUP)
    warmup_epochs = int(cfg.TRAIN.LR_WARMUP.EPOCH) if warmup_enabled else 0
    if epoch < warmup_epochs:
        start = float(cfg.TRAIN.LR_WARMUP.KWARGS.start_lr)
        end = float(cfg.TRAIN.LR_WARMUP.KWARGS.end_lr)
        if warmup_epochs <= 1:
            return start
        ratio = epoch / float(warmup_epochs - 1)
        return start + ratio * (end - start)
    normal_epochs = int(cfg.TRAIN.EPOCH) - warmup_epochs
    start = float(cfg.TRAIN.LR.KWARGS.start_lr)
    end = float(cfg.TRAIN.LR.KWARGS.end_lr)
    if normal_epochs <= 1:
        return start
    normal_epoch = epoch - warmup_epochs
    ratio = normal_epoch / float(normal_epochs - 1)
    return math.exp(math.log(start) * (1.0 - ratio) + math.log(end) * ratio)


def _head_lr_for_step(cfg: object, stage_step: int, total_steps: int) -> float:
    """Return the log-interpolated LR for one exact stage update position."""
    position = int(stage_step)
    count = int(total_steps)
    if count <= 0 or position < 0 or position >= count:
        raise ValueError("stage_step must be within the step LR schedule")
    start = float(cfg.TRAIN.LR.KWARGS.start_lr)
    end = float(cfg.TRAIN.LR.KWARGS.end_lr)
    if start <= 0 or end <= 0:
        raise ValueError("step LR endpoints must be positive")
    if count == 1:
        return start
    ratio = position / float(count - 1)
    return math.exp(math.log(start) * (1.0 - ratio) + math.log(end) * ratio)


def _set_learning_rates(optimizer: torch.optim.Optimizer, head_lr: float) -> None:
    for group in optimizer.param_groups:
        group["lr"] = head_lr * float(group["lr_multiplier"])


def _seed_everything(seed: int) -> None:
    random.seed(seed)
    os.environ["PYTHONHASHSEED"] = str(seed)
    np.random.seed(seed)
    torch.manual_seed(seed)
    torch.cuda.manual_seed_all(seed)
    torch.backends.cudnn.benchmark = False
    torch.backends.cudnn.deterministic = True


def _append_jsonl(path: Path, value: Dict[str, object]) -> None:
    append_metric_record(path, value)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--nanotrack-root", type=Path, required=True)
    parser.add_argument("--pretrained", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--resume", type=Path)
    parser.add_argument("--initialize-from", type=Path)
    parser.add_argument("--initial-global-step", type=int)
    parser.add_argument("--stop-after-global-step", type=int)
    parser.add_argument("--policy", choices=POLICIES)
    parser.add_argument("--stage", default="legacy")
    parser.add_argument("--lr-schedule", choices=("epoch", "step"), default="epoch")
    parser.add_argument(
        "--checkpoint-every-steps", type=int, default=RESUME_CHECKPOINT_STEPS
    )
    parser.add_argument(
        "--candidate-every-steps", type=int, default=CANDIDATE_CHECKPOINT_STEPS
    )
    parser.add_argument("--seed", type=int, default=123456)
    args = parser.parse_args()
    if args.resume is not None and args.initialize_from is not None:
        raise ValueError("resume and initialize-from are mutually exclusive")
    if args.initial_global_step is not None and args.initialize_from is None:
        raise ValueError("initial-global-step requires initialize-from weights")
    resume_global_step = None
    if args.resume is not None:
        resume_header = torch.load(str(args.resume.resolve()), map_location="cpu")
        if not isinstance(resume_header, dict) or "global_step" not in resume_header:
            raise ValueError("resume checkpoint is missing global_step")
        resume_global_step = int(resume_header["global_step"])
    start_global_step = validate_segment_request(
        args.initial_global_step,
        resume_global_step,
        args.stop_after_global_step,
        args.candidate_every_steps,
    )
    if args.checkpoint_every_steps != RESUME_CHECKPOINT_STEPS:
        raise ValueError(
            "checkpoint-every-steps must be {} for recoverable training".format(
                RESUME_CHECKPOINT_STEPS
            )
        )

    config_path = args.config.resolve()
    nanotrack_root = args.nanotrack_root.resolve()
    pretrained_path = args.pretrained.resolve()
    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    sys.path.insert(0, str(nanotrack_root))
    os.chdir(str(nanotrack_root))

    from nanotrack.core.config import cfg
    from nanotrack.datasets.dataset import BANDataset
    from nanotrack.models.model_builder import ModelBuilder
    from nanotrack.utils.model_load import load_pretrain

    cfg.merge_from_file(str(config_path))
    if not torch.cuda.is_available():
        raise RuntimeError("CUDA is unavailable in this execution environment")
    _seed_everything(args.seed)
    (output_dir / "resolved_config.yaml").write_text(cfg.dump(), encoding="utf-8")

    model = ModelBuilder()
    load_pretrain(model, str(pretrained_path))
    model = model.cuda().train()
    if args.policy is None:
        if args.resume is None:
            configure_model_for_epoch(model, 0, int(cfg.BACKBONE.TRAIN_EPOCH))
        else:
            configure_legacy_model_for_resume(
                model,
                args.resume.resolve(),
                backbone_train_epoch=int(cfg.BACKBONE.TRAIN_EPOCH),
            )
        optimizer = build_optimizer(
            model,
            base_lr=float(cfg.TRAIN.BASE_LR),
            backbone_lr_multiplier=float(cfg.BACKBONE.LAYERS_LR),
            momentum=float(cfg.TRAIN.MOMENTUM),
            weight_decay=float(cfg.TRAIN.WEIGHT_DECAY),
        )
    else:
        configure_model_for_policy(model, args.policy, freeze_bn=True)
        optimizer = build_optimizer(
            model,
            head_lr=float(cfg.TRAIN.BASE_LR),
            backbone_lr=(
                float(cfg.TRAIN.BASE_LR) * float(cfg.BACKBONE.LAYERS_LR)
                if args.policy == "tail2"
                else None
            ),
            momentum=float(cfg.TRAIN.MOMENTUM),
            weight_decay=float(cfg.TRAIN.WEIGHT_DECAY),
        )

    dataset = BANDataset()
    batch_size = int(cfg.TRAIN.BATCH_SIZE)
    steps_per_epoch = int(cfg.DATASET.VIDEOS_PER_EPOCH) // batch_size
    if steps_per_epoch <= 0:
        raise ValueError("VIDEOS_PER_EPOCH must be at least one batch")
    start_epoch = int(cfg.TRAIN.START_EPOCH)
    global_step = 0
    resume_step = 0
    if args.resume is not None:
        progress = restore_training_checkpoint(
            model,
            optimizer,
            args.resume.resolve(),
            steps_per_epoch=steps_per_epoch,
        )
        if args.policy is not None and progress["policy"] not in (None, args.policy):
            raise ValueError(
                "resume policy {} does not match requested policy {}".format(
                    progress["policy"], args.policy
                )
            )
        if args.policy is not None and progress["seed"] != args.seed:
            raise ValueError(
                "resume seed {} does not match requested seed {}".format(
                    progress["seed"], args.seed
                )
            )
        if args.policy is not None and progress["config"] != cfg.dump():
            raise ValueError(
                "resume config does not match the current exact-resume config"
            )
        if progress["stage"] not in (None, args.stage):
            raise ValueError(
                "resume stage {} does not match requested stage {}".format(
                    progress["stage"], args.stage
                )
            )
        if progress["lr_schedule"] not in (None, args.lr_schedule):
            raise ValueError(
                "resume LR schedule {} does not match requested LR schedule {}".format(
                    progress["lr_schedule"], args.lr_schedule
                )
            )
        start_epoch = max(start_epoch, progress["epoch"])
        global_step = progress["global_step"]
        stage_step = progress["stage_step"]
        if args.stop_after_global_step is not None and global_step >= args.stop_after_global_step:
            raise ValueError("stop-after-global-step must be after segment start")
        resume_step = progress["step_in_epoch"] if start_epoch == progress["epoch"] else 0
        print(
            "resumed={} completed_epochs={} step_in_epoch={} global_step={}".format(
                args.resume.resolve(), start_epoch, resume_step, global_step
            ),
            flush=True,
        )
    elif args.initialize_from is not None:
        progress = initialize_training_stage(
            model, optimizer, args.initialize_from.resolve(), args.stage,
            start_global_step,
        )
        global_step = progress["global_step"]
        stage_step = 0
        start_epoch, resume_step = normalize_resume_position(
            int(cfg.TRAIN.START_EPOCH), 0, steps_per_epoch
        )
    else:
        global_step = start_global_step
        stage_step = 0
        start_epoch, resume_step = normalize_resume_position(
            start_epoch, resume_step, steps_per_epoch
        )
    start_sample = resume_sample_offset(
        start_epoch, resume_step, steps_per_epoch, batch_size
    )
    if args.policy is not None:
        loader = build_deterministic_data_loader(
            dataset,
            batch_size=batch_size,
            num_workers=int(cfg.TRAIN.NUM_WORKERS),
            seed=args.seed,
            start_sample=start_sample,
            pin_memory=True,
        )
    else:
        loader = DataLoader(
            dataset,
            batch_size=batch_size,
            sampler=_ResumeSequentialSampler(dataset, start_sample),
            num_workers=int(cfg.TRAIN.NUM_WORKERS),
            pin_memory=True,
            drop_last=True,
        )
    iterator = iter(loader)
    metrics_path = output_dir / "train_metrics.jsonl"
    if metrics_path.exists() and args.resume is None:
        metrics_path.unlink()
    elif args.resume is not None:
        kept = truncate_metrics_after_global_step(
            metrics_path, progress["metrics_position"]
        )
        print(
            "metrics_truncated_to_position={} kept={}".format(
                progress["metrics_position"], kept
            ),
            flush=True,
        )
    torch.cuda.reset_peak_memory_stats()
    for epoch in range(start_epoch, int(cfg.TRAIN.EPOCH)):
        model.train()
        if args.policy is None:
            configure_model_for_epoch(model, epoch, int(cfg.BACKBONE.TRAIN_EPOCH))
            optimized_ids = {
                id(parameter)
                for group in optimizer.param_groups
                for parameter in group["params"]
            }
            trainable_ids = {
                id(parameter)
                for parameter in model.parameters()
                if parameter.requires_grad
            }
            if optimized_ids != trainable_ids:
                optimizer = build_optimizer(
                    model,
                    base_lr=float(cfg.TRAIN.BASE_LR),
                    backbone_lr_multiplier=float(cfg.BACKBONE.LAYERS_LR),
                    momentum=float(cfg.TRAIN.MOMENTUM),
                    weight_decay=float(cfg.TRAIN.WEIGHT_DECAY),
                )
        else:
            configure_model_for_policy(model, args.policy, freeze_bn=True)
        epoch_head_lr = _head_lr_for_epoch(cfg, epoch)
        epoch_start = time.perf_counter()
        epoch_losses: List[float] = []
        first_step = resume_step if epoch == start_epoch else 0
        for step in range(first_step, steps_per_epoch):
            head_lr = (
                _head_lr_for_step(
                    cfg, stage_step, int(cfg.TRAIN.EPOCH) * steps_per_epoch
                )
                if args.lr_schedule == "step"
                else epoch_head_lr
            )
            _set_learning_rates(optimizer, head_lr)
            data = next(iterator)
            optimizer.zero_grad()
            outputs = model(data)
            loss = outputs["total_loss"]
            if not torch.isfinite(loss):
                raise RuntimeError("non-finite loss at epoch {} step {}".format(epoch + 1, step + 1))
            loss.backward()
            clip_grad_norm_(model.parameters(), float(cfg.TRAIN.GRAD_CLIP))
            optimizer.step()
            global_step += 1
            stage_step += 1
            record = {
                "epoch": epoch + 1,
                "step": step + 1,
                "global_step": global_step,
                "head_lr": head_lr,
                "backbone_lr": head_lr * float(cfg.BACKBONE.LAYERS_LR),
                "backbone_trainable": (
                    args.policy == "tail2"
                    if args.policy is not None
                    else epoch >= int(cfg.BACKBONE.TRAIN_EPOCH)
                ),
                "total_loss": float(loss.detach().cpu()),
                "cls_loss": float(outputs["cls_loss"].detach().cpu()),
                "loc_loss": float(outputs["loc_loss"].detach().cpu()),
            }
            epoch_losses.append(record["total_loss"])
            _append_jsonl(metrics_path, record)
            active_policy = args.policy or (
                "legacy_joint"
                if epoch >= int(cfg.BACKBONE.TRAIN_EPOCH)
                else "legacy_head"
            )
            saved_paths = save_periodic_checkpoints(
                output_dir,
                model,
                optimizer,
                stage=args.stage,
                epoch=epoch,
                step_in_epoch=step + 1,
                global_step=global_step,
                policy=active_policy,
                metrics_position=global_step,
                mean_epoch_loss=float(np.mean(epoch_losses)),
                seed=args.seed,
                config=cfg.dump(),
                stage_step=stage_step,
                candidate_checkpoint_steps=args.candidate_every_steps,
                resume_checkpoint_steps=args.checkpoint_every_steps,
                lr_schedule=args.lr_schedule,
            )
            if (step + 1) % int(cfg.TRAIN.PRINT_FREQ) == 0 or step + 1 == steps_per_epoch:
                print(
                    "epoch={}/{} step={}/{} loss={:.4f} cls={:.4f} loc={:.4f} head_lr={:.7f} backbone={} peak_MiB={:.0f}".format(
                        epoch + 1,
                        int(cfg.TRAIN.EPOCH),
                        step + 1,
                        steps_per_epoch,
                        record["total_loss"],
                        record["cls_loss"],
                        record["loc_loss"],
                        head_lr,
                        record["backbone_trainable"],
                        torch.cuda.max_memory_reserved() / 1024 ** 2,
                    ),
                    flush=True,
                )
            if segment_boundary_reached(
                global_step, args.stop_after_global_step, saved_paths
            ):
                print("segment_complete global_step={}".format(global_step), flush=True)
                return
        checkpoint_path = output_dir / "checkpoint_e{}.pth".format(epoch + 1)
        checkpoint = save_training_checkpoint(
            checkpoint_path,
            model,
            optimizer,
            completed_epochs=epoch + 1,
            step_in_epoch=0,
            global_step=global_step,
            mean_epoch_loss=float(np.mean(epoch_losses)),
            seed=args.seed,
            config=cfg.dump(),
            stage=args.stage,
            policy=args.policy or (
                "legacy_joint"
                if epoch >= int(cfg.BACKBONE.TRAIN_EPOCH)
                else "legacy_head"
            ),
            metrics_position=global_step,
            stage_step=stage_step,
            atomic=True,
            lr_schedule=args.lr_schedule,
        )
        save_training_checkpoint(
            output_dir / "last.pth",
            model,
            optimizer,
            completed_epochs=epoch + 1,
            step_in_epoch=0,
            global_step=global_step,
            mean_epoch_loss=float(np.mean(epoch_losses)),
            seed=args.seed,
            config=cfg.dump(),
            stage=args.stage,
            policy=checkpoint["policy"],
            metrics_position=global_step,
            stage_step=stage_step,
            atomic=True,
            lr_schedule=args.lr_schedule,
        )
        save_training_checkpoint(
            output_dir / "resume.pth",
            model,
            optimizer,
            completed_epochs=epoch + 1,
            step_in_epoch=0,
            global_step=global_step,
            mean_epoch_loss=float(np.mean(epoch_losses)),
            seed=args.seed,
            config=cfg.dump(),
            stage=args.stage,
            policy=checkpoint["policy"],
            metrics_position=global_step,
            stage_step=stage_step,
            atomic=True,
            lr_schedule=args.lr_schedule,
        )
        print(
            "saved={} mean_loss={:.4f} elapsed_seconds={:.1f}".format(
                checkpoint_path, checkpoint["mean_epoch_loss"], time.perf_counter() - epoch_start
            ),
            flush=True,
        )
        resume_step = 0
    if args.stop_after_global_step is not None and global_step < args.stop_after_global_step:
        raise RuntimeError(
            "training ended at global_step {} before requested stop {}".format(
                global_step, args.stop_after_global_step
            )
        )


if __name__ == "__main__":
    main()
