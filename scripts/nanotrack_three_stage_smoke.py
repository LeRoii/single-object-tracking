#!/usr/bin/env python3
"""Run bounded real-data CUDA smoke steps for the three NanoTrack policies."""

import argparse
import hashlib
import json
import math
import os
import subprocess
import sys
import time
from pathlib import Path


WORKSPACE = Path(__file__).resolve().parents[1]
NANOTRACK_ROOT = WORKSPACE / "repos/siamtrackers/NanoTrack"
OFFICIAL_CHECKPOINT = NANOTRACK_ROOT / "models/pretrained/nanotrackv3.pth"
VALIDATION_PANEL = WORKSPACE / "data/nanotrack_conservative_three_stage/val-aerial.json"
ARTIFACT_ROOT = WORKSPACE / "artifacts/nanotrack_conservative_three_stage/smoke"
STAGE_POLICIES = {"stage1": "head_only", "stage2": "tail2", "stage3": "head_only"}


def sha256_file(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def run_stage(stage, command):
    import numpy as np
    import torch
    from torch.nn.utils import clip_grad_norm_
    from torch.utils.data import DataLoader

    sys.path.insert(0, str(WORKSPACE))
    sys.path.insert(0, str(NANOTRACK_ROOT))
    from scripts import train_nanotrack_uav123 as trainer

    invocation_cwd = os.getcwd()
    os.chdir(str(NANOTRACK_ROOT))
    try:
        from nanotrack.core.config import cfg
        from nanotrack.datasets.dataset import BANDataset
        from nanotrack.models.model_builder import ModelBuilder
        from nanotrack.tracker.tracker_builder import build_tracker
        from nanotrack.utils.model_load import load_pretrain

        policy = STAGE_POLICIES[stage]
        config_path = WORKSPACE / "configs/nanotrack_v3_three_stage_{}_smoke.yaml".format(stage)
        output_dir = ARTIFACT_ROOT / stage
        output_dir.mkdir(parents=True, exist_ok=True)
        if not torch.cuda.is_available():
            raise RuntimeError("CUDA is unavailable; run with approved GPU access")
        if not OFFICIAL_CHECKPOINT.is_file():
            raise FileNotFoundError(str(OFFICIAL_CHECKPOINT))
        trainer._seed_everything(123456)
        cfg.merge_from_file(str(config_path))
        resolved_path = output_dir / "resolved_config.yaml"
        resolved_path.write_text(cfg.dump(), encoding="utf-8")

        start_time = time.perf_counter()
        model = load_pretrain(ModelBuilder(), str(OFFICIAL_CHECKPOINT)).cuda()
        trainer.configure_model_for_policy(model, policy, freeze_bn=True)
        optimizer = trainer.build_optimizer(
            model,
            head_lr=float(cfg.TRAIN.BASE_LR),
            backbone_lr=(float(cfg.TRAIN.BASE_LR) * float(cfg.BACKBONE.LAYERS_LR)
                         if policy == "tail2" else None),
            momentum=float(cfg.TRAIN.MOMENTUM),
            weight_decay=float(cfg.TRAIN.WEIGHT_DECAY),
        )
        trainable_names = [name for name, parameter in model.named_parameters()
                           if parameter.requires_grad]
        bn_modules = [(name, module) for name, module in model.named_modules()
                      if isinstance(module, torch.nn.modules.batchnorm._BatchNorm)]
        frozen_bn_modules = [name for name, module in bn_modules
                             if not module.training and
                             all(not parameter.requires_grad for parameter in module.parameters())]
        if not bn_modules or len(frozen_bn_modules) != len(bn_modules):
            raise RuntimeError("batch-normalization freeze audit failed")
        neck_frozen = (not model.neck.training and
                       all(not parameter.requires_grad for parameter in model.neck.parameters()))
        if not neck_frozen:
            raise RuntimeError("neck freeze audit failed")

        dataset = BANDataset()
        loader = DataLoader(dataset, batch_size=int(cfg.TRAIN.BATCH_SIZE),
                            num_workers=0, shuffle=False, pin_memory=True, drop_last=True)
        if len(loader) < 1:
            raise RuntimeError("smoke dataset did not produce a complete batch")
        batch = next(iter(loader))
        torch.cuda.synchronize()
        torch.cuda.reset_peak_memory_stats()
        optimizer.zero_grad()
        outputs = model(batch)
        losses = {key: float(outputs[key].detach().float().cpu().item())
                  for key in ("cls_loss", "loc_loss", "total_loss")}
        if not all(math.isfinite(value) for value in losses.values()):
            raise RuntimeError("non-finite smoke loss: {}".format(losses))
        outputs["total_loss"].backward()
        clip_grad_norm_(model.parameters(), float(cfg.TRAIN.GRAD_CLIP))
        optimizer.step()
        torch.cuda.synchronize()
        peak_allocated = torch.cuda.max_memory_allocated() / (1024.0 ** 2)
        peak_reserved = torch.cuda.max_memory_reserved() / (1024.0 ** 2)
        if peak_reserved >= 6144:
            raise RuntimeError("peak reserved memory {:.1f} MiB exceeds 6 GiB".format(peak_reserved))

        checkpoint_path = output_dir / "checkpoint_smoke.pth"
        payload = {
            "state_dict": {key: value.detach().cpu() for key, value in model.state_dict().items()},
            "smoke_metadata": {"stage": stage, "policy": policy,
                               "config_sha256": sha256_file(config_path)},
        }
        torch.save(payload, str(checkpoint_path))
        checkpoint_hash = sha256_file(checkpoint_path)
        reloaded_payload = torch.load(str(checkpoint_path), map_location="cpu")
        reloaded_model = ModelBuilder()
        reloaded_model.load_state_dict(reloaded_payload["state_dict"], strict=True)
        reloaded_model = reloaded_model.cuda().eval()

        from scripts.evaluate_aerial_tracklets_nanotrack import load_panel, run_sequence
        panel = load_panel(VALIDATION_PANEL)
        if panel.split != "validation":
            raise RuntimeError("smoke validation panel has unexpected split {}".format(panel.split))
        sequence = panel.sequences[0]
        tracker = build_tracker(reloaded_model)
        with torch.no_grad():
            validation_result = run_sequence(sequence, tracker)
        boxes = validation_result["predicted_xywh"]
        first_predicted = next((box for box in boxes[1:] if box is not None), None)
        if first_predicted is None or not all(math.isfinite(float(value)) for value in first_predicted):
            raise RuntimeError("held-out validation inference produced no finite bbox")
        elapsed = time.perf_counter() - start_time
        run = {
            "stage": stage,
            "policy": policy,
            "seed": 123456,
            "cuda_available": True,
            "gpu_name": torch.cuda.get_device_name(0),
            "command": list(command),
            "trainer_argv": [sys.executable, str(WORKSPACE / "scripts/train_nanotrack_uav123.py"),
                             "--config", str(config_path), "--policy", policy, "--stage", stage],
            "config_path": str(config_path),
            "config_sha256": sha256_file(config_path),
            "resolved_config_path": str(resolved_path),
            "official_checkpoint_path": str(OFFICIAL_CHECKPOINT),
            "official_checkpoint_sha256": sha256_file(OFFICIAL_CHECKPOINT),
            "forward_backward_steps": 1,
            "losses": {"cls": losses["cls_loss"], "loc": losses["loc_loss"], "total": losses["total_loss"]},
            "trainable_parameter_names": trainable_names,
            "frozen_bn_modules": frozen_bn_modules,
            "batch_norm_module_count": len(bn_modules),
            "neck_frozen": neck_frozen,
            "peak_allocated_mib": peak_allocated,
            "peak_reserved_mib": peak_reserved,
            "runtime_seconds": elapsed,
            "checkpoint_path": str(checkpoint_path),
            "checkpoint_sha256": checkpoint_hash,
            "checkpoint_reloaded_strictly": True,
            "panel_sha256": panel.sha256,
            "validation": {
                "split": panel.split,
                "panel_path": panel.path,
                "panel_sha256": panel.sha256,
                "sequence_name": sequence.name,
                "sequence_source": sequence.source,
                "sequence_source_video": sequence.source_video,
                "sequence_frames": validation_result["frame_count"],
                "tracked_frames": validation_result["frame_count"] - 1,
                "initialization_recorded": bool(validation_result["predicted_xywh"][0]),
                "bbox_xywh": [float(value) for value in first_predicted],
                "sequence_auc": float(validation_result["sequence_auc"]),
            },
        }
        (output_dir / "smoke_run.json").write_text(json.dumps(run, indent=2, sort_keys=True) + "\n",
                                                     encoding="utf-8")
        return run
    finally:
        os.chdir(invocation_cwd)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage", choices=("all", "stage1", "stage2", "stage3"), default="all")
    args = parser.parse_args()
    if args.stage == "all":
        runs = {}
        for stage in STAGE_POLICIES:
            command = [sys.executable, str(Path(__file__).resolve()), "--stage", stage]
            subprocess.run(command, cwd=str(WORKSPACE), check=True)
            runs[stage] = json.loads((ARTIFACT_ROOT / stage / "smoke_run.json").read_text(encoding="utf-8"))
        report = {"schema_version": 1, "runs": runs}
        ARTIFACT_ROOT.mkdir(parents=True, exist_ok=True)
        report_path = ARTIFACT_ROOT / "smoke_report.json"
        report_path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        print(str(report_path))
        return
    command = [sys.executable, str(Path(__file__).resolve()), "--stage", args.stage]
    run = run_stage(args.stage, command)
    print(json.dumps({"stage": args.stage, "losses": run["losses"],
                      "peak_reserved_mib": run["peak_reserved_mib"],
                      "checkpoint_sha256": run["checkpoint_sha256"],
                      "validation": run["validation"]}, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
