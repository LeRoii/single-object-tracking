#!/usr/bin/env python3
"""Generate English and Chinese reports for the NanoTrack GOT replay run."""

import argparse
import json
from pathlib import Path


WORKSPACE = Path(__file__).resolve().parents[1]


def _f(value):
    return "{:.6f}".format(float(value))


def _candidate_rows(records, yes="yes", no="no"):
    rows = []
    for item in records:
        metrics = item["metrics"]
        gate = item.get("final_gate", {})
        rows.append(
            "| {key} | {t} | {g} | {u} | {c} | {replace} | {gate} |".format(
                key=item["key"], t=_f(metrics["target_score"]),
                g=_f(metrics["got_val_ao"]), u=_f(metrics["uav123_val_auc"]),
                c=_f(metrics["composite_score"]),
                replace=yes if item["replacement"]["qualified"] else no,
                gate=yes if gate.get("qualified") else no,
            )
        )
    return "\n".join(rows)


def render_reports(prep, calibration, candidates, selection, final=None):
    """Render the auditable no-selection terminal outcome in both languages."""
    selected = selection.get("selected")
    if selected is not None:
        raise ValueError("render_reports currently expects a no-selection outcome")
    if selection.get("selection_final") is not False:
        raise ValueError("no-selection report requires selection_final=false")
    if selection.get("eligible_keys"):
        raise ValueError("no-selection report cannot contain eligible candidates")
    if selection.get("hidden_evaluation_started") is not False or final is not None:
        raise ValueError("hidden/final evaluation is invalid without a selected model")
    records = candidates["candidates"]
    if not records:
        raise ValueError("no-selection report requires evaluated candidates")
    baseline = selection["baseline"]
    best = max(records, key=lambda item: float(item["metrics"]["composite_score"]))
    rows = _candidate_rows(records)
    accepted = [item for item in calibration["records"] if item["accepted"]]
    peak = next(item for item in accepted
                if int(item["batch_size"]) == int(calibration["selected_batch_size"]))
    english = f"""# NanoTrack V3 GOT-10K Replay Fine-tuning Report

## Scope and outcome

No candidate passed the frozen advancement gates, so the initialization baseline is retained.
Stage 2 was not started, GOT-10K train-hidden was not accessed, and no final known-set or
video regression was run. The best observed but non-qualified candidate was `{best['key']}`:
T {_f(best['metrics']['target_score'])}, GOT-val AO {_f(best['metrics']['got_val_ao'])},
UAV123-val AUC {_f(best['metrics']['uav123_val_auc'])}, C
{_f(best['metrics']['composite_score'])}. Baseline C was
{_f(baseline['composite_score'])}; the observed C change was
{_f(float(best['metrics']['composite_score']) - float(baseline['composite_score']))}, below
the required +0.002 stage replacement threshold.

## Data, batch, and schedule

The eligible GOT-10K train pool contains {prep['sequence_count']} sequences and
{prep['selected_frame_count']} selected frames. The mutually exclusive person, vehicle and
other sequence counts are `{prep['pool_sequence_counts']}`. The local hidden split contains
467 sequences and is not referenced by training manifests.

Real forward/backward calibration fixed batch {calibration['selected_batch_size']}. Peak
reserved memory was {peak['peak_reserved_mib']:.1f} MiB and minimum observed free memory
was {peak['minimum_free_mib']:.1f} MiB across {peak['completed_steps']} optimizer steps.
Both seeds used the same batch and {calibration['candidate_interval']['samples']}-sample
candidate interval. The corrective run used exact per-step logarithmic LR decay; Stage 1
ran from 2e-5 to 8e-6.

## Candidate history

| Candidate | T | GOT-val AO | UAV123-val AUC | C | Stage replacement | Final gate |
| --- | ---: | ---: | ---: | ---: | --- | --- |
{rows}

## Boundaries and limitations

All evidence was produced on the desktop RTX 2060 CUDA environment. It does not establish
RK3566/RKNN conversion, board latency, memory, temperature, power, or board-side accuracy.
Because selection did not freeze a qualifying candidate, accessing the hidden set would
have violated the protocol; it therefore remains untouched.
"""
    chinese_rows = _candidate_rows(records, yes="是", no="否")
    chinese = f"""# NanoTrack V3 GOT-10K 回放微调报告

## 范围与结论

没有候选模型通过冻结的晋级门槛，因此保留初始化基线模型。Stage 2 未启动，
GOT-10K train-hidden 未访问，也没有执行最终已知集或视频回归。观测到的最佳但未合格
候选为 `{best['key']}`：T {_f(best['metrics']['target_score'])}、GOT-val AO
{_f(best['metrics']['got_val_ao'])}、UAV123-val AUC
{_f(best['metrics']['uav123_val_auc'])}、C {_f(best['metrics']['composite_score'])}。
基线 C 为 {_f(baseline['composite_score'])}，观测到的 C 变化为
{_f(float(best['metrics']['composite_score']) - float(baseline['composite_score']))}，
低于阶段替换要求的 +0.002。

## 数据、batch 与学习率调度

GOT-10K 可用训练池包含 {prep['sequence_count']} 条序列、
{prep['selected_frame_count']} 个入选帧；person、vehicle、other 三个互斥池的序列数为
`{prep['pool_sequence_counts']}`。本地隐藏集包含 467 条序列，不出现在训练 manifest 中。

真实前向、反向和优化器更新标定固定 batch={calibration['selected_batch_size']}；连续
{peak['completed_steps']} 步的峰值 reserved 显存为 {peak['peak_reserved_mib']:.1f} MiB，
观测到的最小空闲显存为 {peak['minimum_free_mib']:.1f} MiB。两个随机种子共用相同
batch 和 {calibration['candidate_interval']['samples']} 样本候选区间。校正实验采用精确的
逐 step 对数学习率衰减，Stage 1 从 2e-5 衰减到 8e-6。

## 候选模型历史

| 候选模型 | T | GOT-val AO | UAV123-val AUC | C | 阶段替换 | 最终门槛 |
| --- | ---: | ---: | ---: | ---: | --- | --- |
{chinese_rows}

## 证据边界与限制

全部证据来自桌面 RTX 2060 CUDA 环境，不能证明 RK3566/RKNN 转换、板端时延、内存、
温度、功耗或板端精度。由于没有冻结合格候选，此时访问隐藏集会违反协议，因此隐藏集
保持未访问状态。
"""
    return english, chinese


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifact-root", type=Path,
                        default=WORKSPACE / "artifacts/nanotrack_got_replay_v1")
    args = parser.parse_args()
    root = args.artifact_root.resolve()
    prep = json.loads((WORKSPACE / "data/nanotrack_got_replay_v1/prepare-summary.json").read_text())
    calibration = json.loads((root / "calibration.json").read_text())
    candidates = json.loads((root / "run/candidate_table.json").read_text())
    selection = json.loads((root / "run/selection.json").read_text())
    selected = selection["selected"]
    if selected is None:
        english, chinese = render_reports(
            prep, calibration, candidates, selection, final=None
        )
        (root / "REPORT.md").write_text(english, encoding="utf-8")
        (root / "REPORT.zh-CN.md").write_text(chinese, encoding="utf-8")
        print(root / "REPORT.md")
        print(root / "REPORT.zh-CN.md")
        return
    final = json.loads((root / "final/summary.json").read_text())
    baseline = selection["baseline"]
    rows = _candidate_rows(candidates["candidates"])
    chinese_rows = _candidate_rows(candidates["candidates"], yes="是", no="否")
    accepted = [item for item in calibration["records"] if item["accepted"]]
    peak = next(item for item in accepted
                if int(item["batch_size"]) == int(calibration["selected_batch_size"]))
    english = f"""# NanoTrack V3 GOT-10K Replay Fine-tuning Report

## Scope and outcome

The frozen model is `{selected['key']}` with checkpoint SHA-256
`{selected['checkpoint_sha256']}`. Selection was completed before the one-time
GOT-10K train-hidden evaluation. No `sot-v1.mp4` or other video regression was run.

The selected model changed target score T from {_f(baseline['target_score'])} to
{_f(selected['metrics']['target_score'])}, GOT-val AO from
{_f(baseline['got_val_ao'])} to {_f(selected['metrics']['got_val_ao'])}, and
UAV123-val AUC from {_f(baseline['uav123_val_auc'])} to
{_f(selected['metrics']['uav123_val_auc'])}. Its composite C score is
{_f(selected['metrics']['composite_score'])}.

## Data and batch calibration

The eligible GOT-10K train pool contains {prep['sequence_count']} sequences and
{prep['selected_frame_count']} selected frames. The mutually exclusive person,
vehicle and other sequence counts are `{prep['pool_sequence_counts']}`. The local
hidden split contains 467 sequences and is not referenced by training manifests.

Real forward/backward calibration selected batch {calibration['selected_batch_size']}.
At that batch, peak reserved memory was {peak['peak_reserved_mib']:.1f} MiB and
minimum observed free memory was {peak['minimum_free_mib']:.1f} MiB across
{peak['completed_steps']} optimizer steps. Both seeds used the same frozen batch,
learning rates and {calibration['candidate_interval']['samples']}-sample candidate interval.

## Candidate history

| Candidate | T | GOT-val AO | UAV123-val AUC | C | Stage replacement | Final gate |
| --- | ---: | ---: | ---: | ---: | --- | --- |
{rows}

## Post-selection evaluation

- GOT-10K train-hidden: AO {_f(final['got_train_hidden']['ao'])}, SR0.5
  {_f(final['got_train_hidden']['sr_0_5'])}, SR0.75
  {_f(final['got_train_hidden']['sr_0_75'])}.
- UAV123 known test regression: success AUC {_f(final['uav123_test_auc'])}.
- Official VisDrone2019-SOT-val regression: success AUC
  {_f(final['visdrone_sot_val_auc'])}.

## Boundaries and limitations

All training and inference evidence in this report was produced on the desktop RTX
2060 CUDA environment. It does not establish RK3566/RKNN conversion, board latency,
memory, temperature, power, or board-side accuracy. UAV123 test and official
VisDrone-SOT-val are known regression sets, while GOT train-hidden was accessed once
only after selection was frozen.
"""
    chinese = f"""# NanoTrack V3 GOT-10K 回放微调报告

## 范围与结论

最终冻结模型为 `{selected['key']}`，检查点 SHA-256 为
`{selected['checkpoint_sha256']}`。模型选择完成后才执行一次 GOT-10K train-hidden
评测。本轮没有运行 `sot-v1.mp4`，也没有执行其他视频回归。

相对初始化模型，目标域得分 T 从 {_f(baseline['target_score'])} 变为
{_f(selected['metrics']['target_score'])}，GOT-val AO 从
{_f(baseline['got_val_ao'])} 变为 {_f(selected['metrics']['got_val_ao'])}，
UAV123-val AUC 从 {_f(baseline['uav123_val_auc'])} 变为
{_f(selected['metrics']['uav123_val_auc'])}；最终综合分 C 为
{_f(selected['metrics']['composite_score'])}。

## 数据与 batch 标定

GOT-10K 可用训练池包含 {prep['sequence_count']} 条序列、
{prep['selected_frame_count']} 个入选帧。person、vehicle、other 三个互斥池的序列数为
`{prep['pool_sequence_counts']}`。本地隐藏集包含 467 条序列，不出现在训练 manifest 中。

真实前向、反向和优化器更新标定选择 batch={calibration['selected_batch_size']}。
该 batch 连续 {peak['completed_steps']} 步的峰值 reserved 显存为
{peak['peak_reserved_mib']:.1f} MiB，观测到的最小空闲显存为
{peak['minimum_free_mib']:.1f} MiB。两个随机种子共用冻结后的 batch、学习率和
{calibration['candidate_interval']['samples']} 样本候选区间。

## 候选模型历史

| 候选模型 | T | GOT-val AO | UAV123-val AUC | C | 阶段替换 | 最终门槛 |
| --- | ---: | ---: | ---: | ---: | --- | --- |
{chinese_rows}

## 选模后的评测

- GOT-10K train-hidden：AO {_f(final['got_train_hidden']['ao'])}，SR0.5
  {_f(final['got_train_hidden']['sr_0_5'])}，SR0.75
  {_f(final['got_train_hidden']['sr_0_75'])}。
- UAV123 已知测试回归：成功率 AUC {_f(final['uav123_test_auc'])}。
- 官方 VisDrone2019-SOT-val 回归：成功率 AUC
  {_f(final['visdrone_sot_val_auc'])}。

## 证据边界与限制

本报告中的训练和推理证据来自桌面 RTX 2060 CUDA 环境，不能证明 RK3566/RKNN
转换、板端时延、内存、温度、功耗或板端精度。UAV123 test 和官方
VisDrone-SOT-val 属于已知回归集；GOT train-hidden 只在选择冻结后访问一次。
"""
    (root / "REPORT.md").write_text(english, encoding="utf-8")
    (root / "REPORT.zh-CN.md").write_text(chinese, encoding="utf-8")
    print(root / "REPORT.md")
    print(root / "REPORT.zh-CN.md")


if __name__ == "__main__":
    main()
