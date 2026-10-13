#!/usr/bin/env python3
"""Policy helpers for GOT replay batch calibration and training schedules."""

import json
import math
from pathlib import Path
from typing import Callable, Mapping, Sequence, Tuple


def _is_power_of_two(value: int) -> bool:
    return value > 0 and value & (value - 1) == 0


def batch_candidates(minimum: int = 16, maximum: int = 1024) -> Tuple[int, ...]:
    minimum = int(minimum)
    maximum = int(maximum)
    if not _is_power_of_two(minimum) or not _is_power_of_two(maximum) or minimum > maximum:
        raise ValueError("batch bounds must be ordered powers of two")
    values = []
    current = minimum
    while current <= maximum:
        values.append(current)
        current *= 2
    return tuple(values)


def accepted_memory_sample(
    sample: Mapping[str, object],
    maximum_reserved_ratio: float = 0.88,
    minimum_free_mib: float = 768.0,
    required_steps: int = 50,
) -> bool:
    try:
        total = float(sample["total_mib"])
        reserved = float(sample["peak_reserved_mib"])
        free = float(sample["minimum_free_mib"])
        completed = int(sample["completed_steps"])
    except (KeyError, TypeError, ValueError):
        return False
    if not all(math.isfinite(value) for value in (total, reserved, free)) or total <= 0:
        return False
    return (
        sample.get("oom") is False
        and sample.get("finite_losses") is True
        and completed >= int(required_steps)
        and reserved / total <= float(maximum_reserved_ratio)
        and free >= float(minimum_free_mib)
    )


def choose_largest_safe_batch(records: Sequence[Mapping[str, object]]) -> int:
    if not records:
        raise ValueError("batch calibration produced no records")
    previous = 0
    selected = None
    for record in records:
        batch = int(record["batch_size"])
        if batch <= previous:
            raise ValueError("batch calibration records must be strictly increasing")
        previous = batch
        if record.get("accepted") is not True:
            break
        selected = batch
    if selected is None:
        raise ValueError("no safe batch size was accepted")
    return selected


def scaled_learning_rate(
    reference_lr: float,
    batch_size: int,
    reference_batch: int = 8,
    maximum_scale: float = 4.0,
) -> float:
    lr = float(reference_lr)
    batch = int(batch_size)
    reference = int(reference_batch)
    if not math.isfinite(lr) or lr <= 0 or batch <= 0 or reference <= 0:
        raise ValueError("learning-rate scaling inputs must be positive and finite")
    scale = min(math.sqrt(batch / float(reference)), float(maximum_scale))
    return lr * scale


def candidate_interval(
    batch_size: int,
    reference_samples: int = 10000,
    minimum_steps: int = 100,
) -> dict:
    batch = int(batch_size)
    if batch <= 0 or reference_samples <= 0 or minimum_steps <= 0:
        raise ValueError("candidate interval inputs must be positive")
    steps = max(int(math.ceil(reference_samples / float(batch))), int(minimum_steps))
    return {"samples": steps * batch, "steps": steps}


def calibrate_batches(
    probe: Callable[[int], Mapping[str, object]],
    candidates: Sequence[int] = batch_candidates(),
) -> list:
    """Run increasing real probes and stop at the first rejected size."""
    records = []
    previous = 0
    for raw_batch in candidates:
        batch = int(raw_batch)
        if batch <= previous:
            raise ValueError("batch candidates must be strictly increasing")
        previous = batch
        record = dict(probe(batch))
        if int(record.get("batch_size", -1)) != batch:
            raise ValueError("probe returned a mismatched batch size")
        record["accepted"] = accepted_memory_sample(record)
        records.append(record)
        if not record["accepted"]:
            break
    return records


def freeze_calibration(records: Sequence[Mapping[str, object]], output: Path) -> dict:
    """Persist the one frozen batch/LR/candidate schedule shared by both seeds."""
    selected = choose_largest_safe_batch(records)
    frozen = {
        "schema_version": 1,
        "selected_batch_size": selected,
        "candidate_interval": candidate_interval(selected),
        "stage1_lr": {
            "start": scaled_learning_rate(2e-5, selected),
            "end": scaled_learning_rate(8e-6, selected),
        },
        "stage2_lr": {
            "start": scaled_learning_rate(8e-6, selected),
            "end": scaled_learning_rate(3e-6, selected),
        },
        "records": [dict(record) for record in records],
    }
    target = Path(output)
    target.parent.mkdir(parents=True, exist_ok=True)
    temporary = target.with_name(target.name + ".tmp")
    temporary.write_text(
        json.dumps(frozen, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    temporary.replace(target)
    return frozen
