# NanoTrack 微调任务进度

- 任务状态：`ready_for_downstream_evaluation`
- 最近核对：`2026-10-25T00:23:04+08:00`。
- 核对基线：`bb297504070b3dfcf2fd4b4da05ebb1503e309db`（四任务进度集成提交）。
- 当前任务分支：`codex/nanotrack-factorial-v3`。
- 当前任务工作树：`/home/u20/single-object-tracking/.worktrees/nanotrack-factorial-v3`。
- 状态说明：既有 g5000 可交付下游评估；新实验类别优先评分策略已完成单元验证，尚未接入新控制器；batch/GOT 交叉对照、原始帧号采样及困难样本强化尚未形成实施完成证据。
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
- 历史控制器评分仍为 `0.70*T + 0.15*GOT + 0.15*UAV123`；新实验独立模块已实现 `T_v3`，尚未接入控制器，不能宣称新实验已运行。
- 当前外部 NanoTrack 数据加载器仍按筛选后的帧列表索引计算 `FRAME_RANGE`；原始帧号采样尚未落实。
- batch `{8,32,64}` × GOT `{0%,10%,30%}` 的新实验、困难样本归因与强化对照没有可交付的运行记录；不能声明 batch 64 或任何配比最优。
- 后续独立目标域留出集的可用性与隔离性尚需核查；已查看的历史隐藏集不能重新称为未触碰测试集。

## Next action

1. 已在 `codex/nanotrack-factorial-v3` 基于 `main@bb297504070b3dfcf2fd4b4da05ebb1503e309db`
   编写下一轮规格与实施计划：
   `docs/superpowers/specs/2026-10-24-nanotrack-factorial-v3.md`、
   `docs/superpowers/plans/2026-10-24-nanotrack-factorial-v3.md`；已获准开始实施，任务 1 评分策略已完成。
2. 下一步为 GOT 原始帧号配对与审计，再实施九单元格配置、可恢复控制器、
   验证失败诊断、等样本交叉实验、训练集困难样本对照和最终报告。计划阶段没有启动训练、
   隐藏集评测或 `sot-v1.mp4` 回归。
3. g5000 保持只读候选；下游 ONNX/RKNN/开发板链路不因新实验计划自动切换。

## 2026-10-25 任务 1：版本化类别优先评分策略

- 时间：`2026-10-25T00:23:04+08:00`。
- 分支/工作树：`codex/nanotrack-factorial-v3` /
  `/home/u20/single-object-tracking/.worktrees/nanotrack-factorial-v3`。
- 开始基线：`40c6b110d80a2aa5a1e2eb9e4e0191012773d2d3`；结果提交由
  `git log -1 --format=%H -- scripts/nanotrack_factorial_policy.py` 查询，提交后回报协调者。
- 输入契约：`scripts.run_nanotrack_got_replay.build_metrics` 的指标映射。本任务没有消费
  或生成模型/二进制；旧 replay 源码 SHA-256 仍为
  `d91c681a0af0fa6f2a19d78581f0b0a99da09127208aa52abe7de76f242ceb4b`。
- 新增输出均位于上述工作树：`scripts/nanotrack_factorial_policy.py`，SHA-256
  `eb1360c4a6bca3d9ec0e9aec96fd5c26a16224da9db01774445527459e7acb16`；
  `tests/test_nanotrack_factorial_policy.py`，SHA-256
  `6b0b9d14ea1d3ba2f3676b81ab279d4b780735ce5d337f7de70f0f60d5303c14`。
- 保留协调者预先完成的 `.gitignore` 四行修正，使 `.venvs`、`data`、`artifacts`、`repos`
  符号链接被忽略；没有提交符号链接或改写其指向的历史工件。

### 接口及行为

- 评分版本：`SCORING_VERSION = "nanotrack-category-first-v3"`。
- `target_score_v3(metrics)` 先分别跨有指标域计算 person/vehicle 宏平均，再计算四个
  必需域 UAV123/UAVDT/VisDrone-MOT/VisDrone-SOT 的 overall 宏平均；使用
  `0.4*P + 0.4*V + 0.2*D`，不读取缓存旧评分或帧数/轨迹数/训练频次。
- `guardrail_decision(candidate, baseline)` 返回 `qualified`、`reasons`、`checks`、
  `scoring_version` 和双方 `T_v3`；七个门分别约束 GOT/UAV123 下降不超过 0.01、
  person/vehicle 宏平均与其余三域 overall 下降不超过 0.015。边界允许 `1e-12` 浮点误差。
- `rank_key(metrics, baseline)` 供 `max` 或降序排序使用，合格返回 `(True, T_v3)`，
  不合格返回 `(False, -inf)`；调用者仍必须过滤不合格项，全部不合格时不得取其 max
  作为候选。GOT/UAV123 不再次进入排序分数。
- `final_promotion(candidate_by_seed, baseline)` 要求恰好三个种子的映射，三者逐一过门，
  并且三种子 `T_v3` 算术平均相对固定基线至少提高 0.005；结果包含
  `promoted`/`qualified`、原因、平均分、提升量及 `seed_decisions`。
- 必需域缺失、person/vehicle 任一宏平均完全缺失、非法/非有限评分输入、非法种子数量
  抛出 `ValueError`。旧域优先 `target_score`、`category_macro` 与
  `composite_score=0.70*T+0.15*GOT+0.15*UAV123` 未改动。

### 实际测试记录

以下命令均在任务工作树运行，解释器统一为
`/home/u20/single-object-tracking/.venvs/nanotrack/bin/python`：

```bash
/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B -m unittest tests.test_nanotrack_factorial_policy -v
/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B -m unittest tests.test_nanotrack_got_replay_controller tests.test_nanotrack_got_replay_reporting -v
/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B -m unittest discover -s tests -v
```

- RED：先新增测试、尚未编写策略模块时，第一条命令实测 `Ran 24 tests`、
  `FAILED (failures=22)`、exit 1。22 项均明确失败于
  `No module named 'scripts.nanotrack_factorial_policy'`；2 项旧评分兼容测试通过。
- GREEN：实现最小独立策略后重跑第一条，`Ran 24 tests in 0.015s`、`OK`、exit 0。
  覆盖类别权重、四域平均、缺失类别、频次无关、GOT/UAV 方向冲突、七门边界、
  三种子聚合门、非法/非有限输入与历史兼容。
- 历史控制器/报告：第二条实测 `Ran 11 tests in 0.072s`、`OK`、exit 0。
- 完整套件仅运行一次：第三条实测 `Ran 190 tests in 159.280s`、
  `FAILED (failures=4)`、exit 1、无 error。按顶层测试方法计，188 项通过、2 项失败；
  4 个失败记录含同一方法内的 3 个 stage 子测试，完整名称如下：
  - `test_nanotrack_three_stage_configs.FormalThreeStageConfigTest.test_formal_configs_match_quota_paths_and_training_contract (stage='stage1')`
  - `test_nanotrack_three_stage_configs.FormalThreeStageConfigTest.test_formal_configs_match_quota_paths_and_training_contract (stage='stage2')`
  - `test_nanotrack_three_stage_configs.FormalThreeStageConfigTest.test_formal_configs_match_quota_paths_and_training_contract (stage='stage3')`
  - `test_nanotrack_three_stage_configs.FormalThreeStageConfigTest.test_smoke_configs_are_bounded_and_keep_policy_representative_pools`
- 四项均比较历史 `/home/u20/single-object-tracking/artifacts/...` 与当前
  `.worktrees/nanotrack-factorial-v3/artifacts/...` 的配置路径，属于已报告的基线问题。
  已有符号链接使历史 smoke CLI/report 测试本轮通过，故没有照抄起始 5 失败/1 错误。
  **完整套件不是全绿**；本任务没有修改这些历史配置或测试。
- `git diff --check` 通过；旧 replay 文件无 Git diff，SHA-256 与基线一致。

### 边界、下一步和 Handoff

- 仅证明评分/排名/晋级策略及历史兼容的单元行为；未运行真实 GPU 训练、桌面数据集
  评测、隐藏集、`sot-v1.mp4`、ONNX/RKNN、开发板或吊舱验证，没有改变 g5000 身份。
- 下一步：任务 2 按原始帧号实施 GOT 配对策略；后续控制器消费本评分版本、逐种子
  `guardrail_decision` 和最终三种子聚合结果。
- Handoff：协调者集成本提交后可锁定新实验评分接口；历史 v1/v2 保持原语义，公共模型
  身份和板端 promotion gate 无需变更。不得把本单元里程碑视为交叉实验完成。

## 2026-10-25 航拍类别数量与评分方案复核

### 实测数据分布

在工件根目录 `/home/u20/single-object-tracking` 使用
`./.venvs/nanotrack/bin/python -B -` 只读解析四域 `normal/*.json`，只计数字帧键；
`hard/*.json` 与 normal 高度重叠，未相加形成虚假总量。观察结果如下：

| 类别 | 训练轨迹数 | normal 有效帧引用数 |
| --- | ---: | ---: |
| person | 1,718 | 52,198 |
| vehicle | 3,959 | 102,046 |
| bike | 823 | 20,878 |
| other | 25 | 5,141 |
| unknown | 70 | 60,359 |

- 四域 normal 总计 240,622 个帧引用，与历史准备摘要一致。
- person 由 UAV123 的 35 条/11,806 帧和 VisDrone-MOT 的 1,683 条/40,392 帧组成；
  UAVDT 的 1,037 条/24,888 帧均为 vehicle，VisDrone-SOT 的 70 条/60,359 帧均标为
  unknown。因此可确认“已识别类别中的行人较少”，但不能把 VisDrone-SOT 的 unknown
  推断成没有行人。
- 固定验证证据合并后，person 为 35 条序列、6,045 个计分帧、约 5 个独立源视频组；
  vehicle 为 57 条序列、9,086 个计分帧、约 7 个独立源视频组。连续帧不视为独立样本。
- 验证输入/结果身份：`data/nanotrack_conservative_three_stage/val-aerial.json`
  SHA-256 `8af84a37cc29bf1efc72a37adeae30b3ebf364c9c928e51b120077414f12b77f`；
  `data/uav123_nanotrack/annotations/val.json` SHA-256
  `116ea69d78c36f4a2422ea1ccbb034c81fba280522094e5e4c461bac0576f443`；
  基线 aerial 结果 SHA-256
  `d045f2c507a2c2f29411fc314dda9cc812d5c5c9899520ce5ca8feca0560d6c5`；
  基线 UAV123 summary SHA-256
  `6a36faec2bcca6e15655e2fcea0c242efbdb459123afa7ec43c1d03238fc8e15`。

### 评分结论与规格修订

- 数据频次不等于业务重要性。车辆和行人均为正式目标时，不应因 person 样本较少而按
  训练数量降低其评分权重；否则会进一步掩盖少数类别退化。
- 旧域优先 T 在每个域内使用 person 0.4、vehicle 0.4、overall 0.2，但缺失项重归一化后，
  四域平均中的直接名义权重约为 person 0.20、vehicle 0.3667、overall 0.4333；所以它并非
  全局 person/vehicle 等权。
- 新实验改用版本化的类别优先 `T_v3 = 0.4*P + 0.4*V + 0.2*D`：`P`、`V` 分别为
  可用域的 person/vehicle 宏平均，`D` 为四域 overall 宏平均。旧 `target_score` 和历史
  `composite_score` 保持不变，不重算、不改写 v1/v2 结论。
- 为控制较少独立源视频造成的方差，新实验同时报告 `P`、`V`、`D`、`T_v3` 的按源视频
  分组 bootstrap 95% 区间，并增加 UAVDT、VisDrone-MOT、VisDrone-SOT 逐域退化门；
  不能仅凭单个点估计宣布 person 改善。
- 已同步修订规格与实施计划。新实验只生成中文报告 `REPORT.zh-CN.md`，不新建/覆盖本轮
  `REPORT.md`；历史英文报告保留。本任务后续规格和计划正文统一使用中文。

### 证据边界和下一步

- 本次只读统计和文档修订没有启动训练、桌面推理、隐藏集、ONNX/RKNN、开发板或吊舱
  验证，也没有改变 g5000 模型身份。
- 下一步仍从实施计划任务 1 开始，先用测试锁定 `T_v3`、旧评分兼容性和逐域门槛；随后
  才实施 GOT 原始帧号配对与九单元格实验。
- 公共契约当前无需切换模型或状态；协调对话只需在集成本提交时知悉新实验评分版本和
  中文报告约束，待真实实验结果形成后再决定是否更新公共候选身份。

## 2026-10-24 交叉对照规格与计划

- 初始检查：主工作树为干净的 `main@bb297504070b3dfcf2fd4b4da05ebb1503e309db`，
  与 `origin/main` 一致；此前 NanoTrack 进度提交 `81b95123dc5678b924802482c88fb1ae3e9d5980`
  已通过 `707113b` 集成。没有在 `main` 上实施代码修改。
- 新工作树创建命令：

```bash
git worktree add .worktrees/nanotrack-factorial-v3 \
  -b codex/nanotrack-factorial-v3 bb297504070b3dfcf2fd4b4da05ebb1503e309db
```

- 已形成并自检的新规格与计划：
  `docs/superpowers/specs/2026-10-24-nanotrack-factorial-v3.md`、
  `docs/superpowers/plans/2026-10-24-nanotrack-factorial-v3.md`。
- 计划阶段没有修改评分/训练代码，没有启动 GPU 训练、评测、隐藏集或视频回归。
- 工作树基线全套测试命令：

```bash
/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B \
  -m unittest discover -s tests -v
```

  实测 `Ran 166 tests in 176.221s`，结果为 `FAILED (failures=5, errors=1)`。
  其余 160 项通过。五项失败来自 `test_nanotrack_three_stage_configs` 将已提交配置中
  `/home/u20/single-object-tracking/...` 的历史绝对路径与当前 worktree 根路径比较；一项报错
  来自同一测试直接调用 worktree 下不存在的 `.venvs/nanotrack/bin/python`。另一项同时要求
  worktree 中存在 Git 忽略的历史 smoke report。该组失败发生在任何 v3 实现之前，属于已知
  工作树基线兼容问题；不能把后续局部测试通过描述成“全套测试通过”。依照工作树流程，
  需要用户决定继续实施并保留该基线边界，或先单独修复工作树兼容性。

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
