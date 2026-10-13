#!/usr/bin/env python3
"""Calibrate the largest safe NanoTrack batch with real CUDA training steps."""

import argparse
import gc
import json
import math
import os
import subprocess
import sys
import tempfile
import time
from pathlib import Path


WORKSPACE = Path(__file__).resolve().parents[1]
NANOTRACK_ROOT = WORKSPACE / "repos/siamtrackers/NanoTrack"
DEFAULT_INITIAL = (
    WORKSPACE / "artifacts/nanotrack_conservative_three_stage/run/stage1/"
    "candidate_stage1_g00005000_e001_s02500.pth"
)

if str(WORKSPACE) not in sys.path:
    sys.path.insert(0, str(WORKSPACE))

from scripts.generate_nanotrack_got_replay_configs import build_stage_config
from scripts.nanotrack_got_replay_training import (
    batch_candidates,
    calibrate_batches,
    candidate_interval,
    freeze_calibration,
)


def resolve_cli_path(path: Path, invocation_cwd: Path) -> Path:
    """Resolve a CLI path against the directory where the command was invoked."""
    candidate = Path(path)
    if candidate.is_absolute():
        return candidate
    return (Path(invocation_cwd) / candidate).resolve()


def cuda_free_mib(cuda, command_runner=subprocess.check_output) -> float:
    """Read device-0 free memory across old and new PyTorch releases."""
    native = getattr(cuda, "mem_get_info", None)
    if native is not None:
        free_bytes, _ = native()
        return float(free_bytes) / 1024.0 ** 2
    output = command_runner([
        "nvidia-smi", "--query-gpu=memory.free",
        "--format=csv,noheader,nounits", "--id=0",
    ])
    try:
        value = float(output.decode("utf-8").strip()
                      if isinstance(output, bytes) else str(output).strip())
    except (TypeError, ValueError, UnicodeError) as error:
        raise RuntimeError("cannot parse nvidia-smi free memory: {!r}".format(output)) from error
    if not math.isfinite(value) or value < 0:
        raise RuntimeError("nvidia-smi returned invalid free memory: {!r}".format(output))
    return value


def _write_probe_config(batch_size: int, directory: Path) -> Path:
    import yaml

    interval = candidate_interval(batch_size)["samples"]
    config = build_stage_config("stage2", batch_size, interval, 8e-6, 3e-6)
    path = directory / "probe_batch_{}.yaml".format(batch_size)
    path.write_text(yaml.safe_dump(config, sort_keys=False), encoding="utf-8")
    return path


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pretrained", type=Path, default=DEFAULT_INITIAL)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--steps", type=int, default=50)
    parser.add_argument("--minimum-batch", type=int, default=16)
    parser.add_argument("--maximum-batch", type=int, default=1024)
    parser.add_argument("--seed", type=int, default=123456)
    args = parser.parse_args()
    if args.steps < 50:
        raise ValueError("calibration requires at least 50 real optimizer steps")
    if not args.pretrained.is_file():
        raise FileNotFoundError(str(args.pretrained))

    import torch
    from torch.nn.utils import clip_grad_norm_
    from torch.utils.data import DataLoader

    from scripts import train_nanotrack_uav123 as trainer

    sys.path.insert(0, str(NANOTRACK_ROOT))
    invocation_cwd = Path.cwd()
    output_path = resolve_cli_path(args.output, invocation_cwd)
    os.chdir(str(NANOTRACK_ROOT))
    try:
        from nanotrack.core.config import cfg
        from nanotrack.datasets.dataset import BANDataset
        from nanotrack.models.model_builder import ModelBuilder
        from nanotrack.utils.model_load import load_pretrain

        if not torch.cuda.is_available():
            raise RuntimeError("CUDA is unavailable; run with approved host GPU access")
        trainer._seed_everything(args.seed)
        with tempfile.TemporaryDirectory(prefix="nanotrack-batch-calibration-") as temp:
            temp_path = Path(temp)
            first_config = _write_probe_config(args.minimum_batch, temp_path)
            cfg.merge_from_file(str(first_config))
            dataset = BANDataset()

            def probe(batch_size):
                trainer._seed_everything(args.seed)
                loader = DataLoader(
                    dataset, batch_size=int(batch_size), shuffle=False,
                    num_workers=0, pin_memory=True, drop_last=True,
                )
                iterator = iter(loader)
                model = None
                optimizer = None
                completed = 0
                finite = True
                oom = False
                error = None
                losses = []
                minimum_free = float("inf")
                started = time.perf_counter()
                try:
                    model = load_pretrain(ModelBuilder(), str(args.pretrained)).cuda()
                    trainer.configure_model_for_policy(model, "head_only", freeze_bn=True)
                    optimizer = trainer.build_optimizer(
                        model, head_lr=8e-6,
                        momentum=float(cfg.TRAIN.MOMENTUM),
                        weight_decay=float(cfg.TRAIN.WEIGHT_DECAY),
                    )
                    torch.cuda.empty_cache()
                    torch.cuda.reset_peak_memory_stats()
                    total_bytes = torch.cuda.get_device_properties(0).total_memory
                    for _ in range(args.steps):
                        try:
                            batch = next(iterator)
                        except StopIteration:
                            iterator = iter(loader)
                            batch = next(iterator)
                        optimizer.zero_grad(set_to_none=True)
                        outputs = model(batch)
                        loss = outputs["total_loss"]
                        if not torch.isfinite(loss):
                            finite = False
                            break
                        loss.backward()
                        clip_grad_norm_(model.parameters(), float(cfg.TRAIN.GRAD_CLIP))
                        optimizer.step()
                        completed += 1
                        losses.append(float(loss.detach().cpu()))
                        torch.cuda.synchronize()
                        minimum_free = min(minimum_free, cuda_free_mib(torch.cuda))
                    torch.cuda.synchronize()
                except RuntimeError as caught:
                    error = str(caught)
                    if "out of memory" in error.lower():
                        oom = True
                    else:
                        raise
                finally:
                    peak_allocated = torch.cuda.max_memory_allocated() / 1024.0 ** 2
                    peak_reserved = torch.cuda.max_memory_reserved() / 1024.0 ** 2
                    total_mib = torch.cuda.get_device_properties(0).total_memory / 1024.0 ** 2
                    del iterator, loader, optimizer, model
                    gc.collect()
                    torch.cuda.empty_cache()
                if not math.isfinite(minimum_free):
                    minimum_free = cuda_free_mib(torch.cuda)
                record = {
                    "batch_size": int(batch_size),
                    "total_mib": total_mib,
                    "peak_allocated_mib": peak_allocated,
                    "peak_reserved_mib": peak_reserved,
                    "minimum_free_mib": minimum_free,
                    "finite_losses": finite,
                    "completed_steps": completed,
                    "requested_steps": int(args.steps),
                    "oom": oom,
                    "error": error,
                    "mean_loss": sum(losses) / len(losses) if losses else None,
                    "elapsed_seconds": time.perf_counter() - started,
                    "gpu_name": torch.cuda.get_device_name(0),
                }
                print(json.dumps(record, sort_keys=True), flush=True)
                return record

            records = calibrate_batches(
                probe,
                batch_candidates(args.minimum_batch, args.maximum_batch),
            )
            frozen = freeze_calibration(records, output_path)
            print(json.dumps(frozen, indent=2, sort_keys=True), flush=True)
    finally:
        os.chdir(str(invocation_cwd))


if __name__ == "__main__":
    main()
