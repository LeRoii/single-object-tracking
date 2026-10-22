# NanoTrack 微调任务进度

- 任务状态：`ready_for_downstream_evaluation`
- 最近核对：`2026-10-22T21:39:23+08:00`
- 核对基线：`29b083e4e6ced2fd46eb7f9ecf821287b4cc401a`（协调规则提交）。
- 当前任务分支：`codex/nanotrack-progress-sync`。
- 当前任务工作树：`/home/u20/single-object-tracking/.worktrees/nanotrack-progress-sync`。
- 状态说明：既有 g5000 可交付下游评估；评分优化、batch/GOT 交叉对照、原始帧号采样及困难样本强化尚未形成实施完成证据。
- 责任边界：数据准备、训练、模型选择、桌面评测，以及向转换/板端链路交付冻结模型身份。

## 已观察证据

- 保守三阶段流程冻结选择为 Stage 1、global step 5000：
  `artifacts/nanotrack_conservative_three_stage/run/stage1/candidate_stage1_g00005000_e001_s02500.pth`。
- 冻结 checkpoint SHA-256：
  `40ebbe4296938bf5d53c8605ab05e729b306fc0bce65921a57ab6613030410d1`。
- `run/selection.json` 已记录 `selection_final=true`，SHA-256：
  `a4b7a0940911cf1e064d3bdf5a312f0c35d893c71ef63374fa8b41fb6e97cc6c`。
- 主要证据：
  `artifacts/nanotrack_conservative_three_stage/REPORT.zh-CN.md`。
- 后续 GOT-10K 回放没有候选通过晋级门，保留初始化基线；它没有替换上述已冻结的三阶段选择。证据见
  `artifacts/nanotrack_got_replay_v2/REPORT.zh-CN.md`。

## 证据边界

- 原模型选择面板和隐藏集属于居中窗口 V1 协议；协议复审没有隐式重跑隐藏集，也没有改变 g5000 的最终选择。
- 现有微调报告证明桌面训练和评测，不证明该 g5000 checkpoint 已完成 ONNX 导出、RKNN 转换或 RK3566 运行。
- `DEPLOYMENT_STATUS.md` 中较早的 RK3566 NanoTrack V3 结果绑定官方权重，不能替代微调 g5000 的板端证据。

## 下游交付要求

转换或板端任务消费本模型前，必须记录：

- checkpoint 路径、大小和 SHA-256；
- 对应配置及其 SHA-256；
- 导出脚本/源代码 commit；
- ONNX 输入输出契约和数值一致性结果；
- RKNN Toolkit 版本、目标平台、量化模式和生成模型 SHA-256。

## Blockers

- 尚未在集成契约中批准 g5000 取代当前板端 NanoTrack 模型。
- 尚无绑定 g5000 的 RKNN 模型与真实 RK3566 验收记录。
- 当前评分代码仍为 `0.70*T + 0.15*GOT + 0.15*UAV123`；不能把拟议的 T 主评分写成已上线。
- 当前外部 NanoTrack 数据加载器仍按筛选后的帧列表索引计算 `FRAME_RANGE`；原始帧号采样尚未落实。
- batch `{8,32,64}` × GOT `{0%,10%,30%}` 的新实验、困难样本归因与强化对照没有可交付的运行记录；不能声明 batch 64 或任何配比最优。
- 后续独立目标域留出集的可用性与隔离性尚需核查；已查看的历史隐藏集不能重新称为未触碰测试集。

## Next action

1. 协调对话先集成本次进度核对提交；保持 g5000 工件只读和当前板端模型不变。
2. 本任务下一研发步骤为冻结新版评分/采样协议及实验预算，再实施受控交叉对照；本次同步未启动训练、隐藏集评测或 `sot-v1.mp4` 回归。
3. 下游转换为另一条需协调确认的候选链路：由协调对话决定是否送 g5000 进入 ONNX/RKNN 和开发板原生 Debug 验证，不因本次文档核对自动启动。

## Handoff

建议的契约变更必须由协调对话审核后写入
`docs/progress/integration-contract.yaml`。本任务对话不得直接把候选状态改成已部署。

- **无需切换公共模型身份或放开 promotion gate**：checkpoint 与 selection 哈希已重新匹配当前契约，`onnx/rknn/board_validation` 应继续保持未验证状态。
- 建议协调对话在公共状态说明中区分“g5000 已冻结可供下游评估”和“下一轮优化仍待实施”。
- 可补充公共契约中的候选配置身份：`configs/nanotrack_v3_three_stage_stage1.yaml`，SHA-256 `cb3065982818a701a7ebb469c41b894b0a6c11e7aac6cb91dc42e5ed50547f2e`；它与已保存 UAV123 评测 provenance 一致，不是 RKNN 导出或板端配置验证。
- 本次只提交此任务文件；结果 commit 在提交后回报协调对话，也可用 `git log -1 --format=%H -- docs/progress/nanotrack-finetune.md` 查询。不要在本文件内虚构自身尚未产生的 commit ID。
- 开发板原生交互式 Debug 编译、调试、测试先于生产交叉编译；交叉编译仅消费协调者锁定的板端 handoff。真实吊舱部署和验收只接受人工反馈，本任务不声明通过。

## 2026-10-22 首次同步与重新核对

### Git 与只读检查

- 初始 cwd：`/home/u20/single-object-tracking`，分支 `main`，HEAD 为协调基线 `29b083e4e6ced2fd46eb7f9ecf821287b4cc401a`。
- `git status --short --branch` 输出仅 `## main`；无未提交修改。`git merge-base --is-ancestor 29b083e4e6ced2fd46eb7f9ecf821287b4cc401a HEAD` 返回 0，无需 fetch/merge/reset。
- 已依次完整阅读 `AGENTS.md`、`docs/progress/STATUS.md`、`docs/progress/integration-contract.yaml`、本任务文件。任务文件没有要求消费其他任务的新模型或板端结果。
- `.worktrees/` 已由 `.gitignore:15` 忽略；没有可用原生工作树工具。首次 `git worktree add` 因 Git 元数据只读失败，获授权后以同一命令成功创建独立分支/工作树，未修改主工作树文件：

```bash
git worktree add .worktrees/nanotrack-progress-sync -b codex/nanotrack-progress-sync 29b083e4e6ced2fd46eb7f9ecf821287b4cc401a
```

### 当前实测工件身份

以下路径相对于 **工件根目录 `/home/u20/single-object-tracking`**，不是新工作树。大文件没有复制进工作树或提交 Git。

| 工件 | 相对路径 | 本次 SHA-256 |
| --- | --- | --- |
| 冻结 checkpoint，3,468,939 bytes | `artifacts/nanotrack_conservative_three_stage/run/stage1/candidate_stage1_g00005000_e001_s02500.pth` | `40ebbe4296938bf5d53c8605ab05e729b306fc0bce65921a57ab6613030410d1` |
| 冻结选择 | `artifacts/nanotrack_conservative_three_stage/run/selection.json` | `a4b7a0940911cf1e064d3bdf5a312f0c35d893c71ef63374fa8b41fb6e97cc6c` |
| 原 Stage 1 配置 | `configs/nanotrack_v3_three_stage_stage1.yaml` | `cb3065982818a701a7ebb469c41b894b0a6c11e7aac6cb91dc42e5ed50547f2e` |
| 训练解析配置 | `artifacts/nanotrack_conservative_three_stage/run/stage1/resolved_config.yaml` | `e720aa6dc7685731c8ddee55c1d07f1f4c601e271219fdcc69e00f86ef49ddfb` |
| 三阶段中文报告 | `artifacts/nanotrack_conservative_three_stage/REPORT.zh-CN.md` | `4f74bfa993f6b7f7fd8c34ff7405d6345fc34fff0abbe1038ceef689475cfa7c` |
| GOT v2 选择状态 | `artifacts/nanotrack_got_replay_v2/run/selection.json` | `3c9c37ced8841b83f2ab4a6e3e29b52fbec5f4fd094945108d19c89bf3844c67` |
| GOT v2 候选表 | `artifacts/nanotrack_got_replay_v2/run/candidate_table.json` | `a61c7815c9f0a002069eea886a5c250ca5922b0f242a75b1f9874e1cd0ec7636` |
| GOT v2 历史验证记录 | `artifacts/nanotrack_got_replay_v2/verification.json` | `b9fae4a9e7f118717a2937480dcf0d691228577981069cb614b3b565afec7b63` |
| GOT v2 中文报告 | `artifacts/nanotrack_got_replay_v2/REPORT.zh-CN.md` | `51c9fc1dad19a1dfecc1168f587f80d4b10de500d0db0b99d7c67d3820bc5956` |
| GOT v2 Stage 1 配置 | `configs/nanotrack_got_replay_v2/stage1.yaml` | `3e209b1b46481df742b9d5349d0f9a1c76050e15bcc80f9608b857aa40238a4b` |

已运行 `sha256sum <上述路径>` 和 `stat -c '%s %n' <checkpoint路径>`。两种 Stage 1 配置用途不同，不以文件名相似推定内容相同。

### 实际重算与测试

在工件根目录使用 `./.venvs/nanotrack/bin/python -B -` 运行只读 JSON/哈希断言；核心可复现检查如下（没有调用会重写 verification.json 的验证器 main）：

```python
import json, hashlib
from pathlib import Path
from scripts.verify_nanotrack_got_replay import classify_selection_outcome
from scripts.run_nanotrack_got_replay import final_gate, composite_score, target_score
root = Path('/home/u20/single-object-tracking')
h = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
s = json.loads((root / 'artifacts/nanotrack_conservative_three_stage/run/selection.json').read_text())
assert s['selection_final'] is True
assert h(Path(s['final_checkpoint_path'])) == s['final_checkpoint_sha256']
r = root / 'artifacts/nanotrack_got_replay_v2'
sel = json.loads((r / 'run/selection.json').read_text())
t = json.loads((r / 'run/candidate_table.json').read_text())
print(classify_selection_outcome(sel, t, (r / 'final/summary.json').exists()))
for item in t['candidates']:
    assert h(Path(item['checkpoint'])) == item['checkpoint_sha256']
    assert not final_gate(item['metrics'], sel['baseline'])['qualified']
    assert abs(composite_score(item['metrics']) - item['metrics']['composite_score']) < 1e-12
best = max(t['candidates'], key=lambda x: composite_score(x['metrics']))
print(len(t['candidates']), t['seeds'])
print(best['key'], target_score(best['metrics']), composite_score(best['metrics']))
```

实测结果：三阶段 checkpoint 身份匹配；GOT v2 共 8 个候选哈希匹配且均未过 final gate；`outcome=no_candidate_passed`，`selected_key=None`，`hidden_evaluation_completed=False`。两个种子均为 `stage1_best=None`、`stage2_skipped=no qualified stage1 candidate`；`final/` 未发现结果条目。最佳但未晋级候选为 `seed20261011_stage1_c1`，T=`0.7432561249618961`、C=`0.7341685649057923`。这次重算利用已有指标，不是重新推理。

在任务工作树实际执行：

```bash
/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B -m unittest discover -s tests -p 'test_nanotrack_got*.py' -v
/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B -m unittest discover -s tests -p 'test_nanotrack_batch_calibration.py' -v
```

分别为 **33 tests / OK / exit 0** 和 **4 tests / OK / exit 0**。这里只重跑了 37 项相关单元测试，没有宣称整个项目测试通过，没有运行 GPU 训练或重新评测隐藏集。

### 新方案实施状态与证据边界

- 本次读取 `scripts/run_nanotrack_got_replay.py:80`，旧 `0.70/0.15/0.15` 评分仍存在，文件 SHA-256 为 `d91c681a0af0fa6f2a19d78581f0b0a99da09127208aa52abe7de76f242ceb4b`。相关单元测试仍明确检查旧权重；测试通过不代表新设计已实现。
- 本次读取 `/home/u20/single-object-tracking/repos/siamtrackers/NanoTrack/nanotrack/datasets/dataset.py:110`，仍按 `template_frame` 列表下标加减 `frame_range`。该外部源码不由本仓库 `git ls-files` 跟踪，当前文件 SHA-256 为 `cd6b925b12f2879d5979c2c1ee8229317ea4bcdc7ce6e8fface4f9d0f2d39373`；后续修改必须保存可复现补丁和源身份，不能只改外部目录。
- 在当前 `artifacts/nanotrack*`、`configs/nanotrack*` 清单中，GOT 回放仅查见 v1/v2；未发现新版交叉对照的完成工件。这是本地检查范围内的结论，不声称检查了其他对话所有工作树或外部机器。
- 历史报告、verification 中的性能/环境数据只作为历史记录引用；本次没有重跑完整数据审计、CUDA 推理、ONNX/RKNN 转换、开发板测试或吊舱验收，也没有证明未触碰隐藏数据的全系统访问历史。
- 研发待办：以 T 排序及固定基线退化约束；原始帧号采样审计/修正及新旧对照；batch/GOT 交叉对照；从验证失败提取问题类别但只从训练集补困难样本；独立目标域最终验证。上述均为待办，不作为完成证据。

## 后续更新模板

```text
时间：<ISO 8601 + timezone>
分支/工作树：<branch> / <absolute path>
基线/结果提交：<base commit> / <result commit>
输入：<path, SHA-256>
输出：<path, SHA-256>
验证：<command -> observed result>
边界/阻塞：<facts>
下一步：<one concrete action>
Handoff：<requested contract change or none>
```
