# NanoTrack V3 交叉对照实验实施计划

> **供执行代理使用：** 必须使用 `superpowers:subagent-driven-development`（推荐）或
> `superpowers:executing-plans`，逐项实施本计划。步骤使用复选框（`- [ ]`）跟踪。

**目标：** 实现并执行可复现的 batch `{8,32,64}` × GOT `{0%,10%,30%}` NanoTrack
微调实验，修正 GOT 帧对语义，诊断验证失败，并在不污染验证集或隐藏集的前提下测试仅来自
训练集的困难样本强化。

**架构：** 保留历史 v1/v2 行为，新增带版本的 v3 策略、配置、控制、诊断、验证和报告模块。
训练继续使用官方 NanoTrack checkout，但由主仓库跟踪的运行时帧对策略适配器仅为 GOT 提供
原始帧 ID 配对，避免把外部仓库的未提交修改纳入结果。

**技术栈：** Python 3.7、PyTorch 1.7.1/CUDA 10.2、NumPy、OpenCV、YAML、
`unittest`、官方 NanoTrack V3 checkout。

**规格：** `docs/superpowers/specs/2026-10-24-nanotrack-factorial-v3.md`

## 全局约束

- 在从 `main@bb297504070b3dfcf2fd4b4da05ebb1503e309db` 创建的
  `codex/nanotrack-factorial-v3` 上工作，禁止在 `main` 实施。
- 不修改历史 GOT v1/v2 工件，也不重新解释其选模规则。
- 不使用验证/隐藏序列训练；最终合格模型冻结前不访问 GOT train-hidden；不运行
  `sot-v1.mp4`。
- 生成的模型、日志、图片、视频和实验 JSON 均留在 Git 外；Git 只提交路径、哈希、命令、
  报告和验证摘要。
- 只修改 NanoTrack 微调代码/文档和 `docs/progress/nanotrack-finetune.md`；不修改公共协调
  文件或其他任务文件。
- 每项行为修改遵循 RED → GREEN；每个实质里程碑把代码/文档和任务进度文件一同提交。
- 完整交叉对照以等样本实验为主要证据。等更新数诊断必须单独标注，绝不混入主排名。
- 本计划以及本任务后续新增或修订的规格、计划正文统一使用中文；命令、路径和代码标识符
  保留原文。
- 本轮只生成中文报告 `REPORT.zh-CN.md`；保留历史英文报告，但不新建或覆盖
  `REPORT.md`。

## 审查重点

- 新增 v3 后，历史 v2 验证或回放必须保留原 `0.70/0.15/0.15` 语义；任务 1 增加明确
  的兼容性测试。
- 新 `T_v3` 必须在类别层真正保持 person/vehicle 各 0.4，不按帧数或训练数量加权；旧的
  域优先 `target_score` 保持不变，任务 1 同时测试两种语义。
- 对 `[1, 50, 200]` 这类稀疏、不连续帧 ID，GOT 必须按原始 ID 间隔过滤，而历史航拍配对
  仍按列表位置；任务 2 测试两条路径。
- GOT 0% 不得包含 GOT 数据项，每个 batch/比例配置必须精确闭合到 40,960 个样本；任务 3
  测试全部九个单元格。
- 恢复运行不得重复已完成训练/评测，且必须拒绝 checkpoint/config/pair-policy 哈希不匹配；
  任务 4 测试中断恢复和身份不匹配。
- 验证序列 ID 及同一源视频的别名/分段不得进入困难训练清单；任务 5 测试源组隔离。

---

## 执行前检查

- 版本化代码根目录为
  `/home/u20/single-object-tracking/.worktrees/nanotrack-factorial-v3`。
- 大工件根目录仍为 `/home/u20/single-object-tracking`。任务 1 前仅为缺失的 `.venvs`、
  `data`、`artifacts`、`repos` 创建被 Git 忽略的工作树符号链接；使用前逐一用
  `readlink -f` 验证。禁止在工作树之间复制生成工件。
- 在台账记录已观察的基线测试结果：共 `166` 项，因历史绝对路径/工作树假设出现 `5` 项
  失败和 `1` 项错误。用户必须选择接受此已知边界继续，或先修复兼容性；执行者不得把该
  基线静默标成全绿。
- 任何训练命令前重新核对规格中的冻结 checkpoint 和配置哈希。

---

### 任务 1：版本化的类别优先评分策略

**文件：**
- 新建：`scripts/nanotrack_factorial_policy.py`
- 新建：`tests/test_nanotrack_factorial_policy.py`
- 修改：`docs/progress/nanotrack-finetune.md`

**接口：**
- 输入：`scripts.run_nanotrack_got_replay.build_metrics` 产生的指标映射。
- 输出：`target_score_v3(metrics) -> float`、`guardrail_decision(candidate, baseline) -> dict`、
  `rank_key(metrics, baseline) -> tuple`、`final_promotion(candidate_by_seed, baseline) -> dict`，
  以及后续状态文件记录的评分版本字符串。

- [ ] **步骤 1：编写失败的策略测试。** 覆盖 `P`/`V` 各 0.4、`D` 为四域总体宏平均、
  缺失类别只在有该指标的域间宏平均、禁止按帧数加权、UAV123/GOT 方向冲突时只按
  `T_v3` 排序、每个固定基线门槛、三种子 `T_v3 >= baseline + 0.005`、非有限输入；并断言
  旧 `target_score` 与 `composite_score` 仍保留历史语义。
- [ ] **步骤 2：运行** `/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B -m unittest tests.test_nanotrack_factorial_policy -v`。
  **预期：** v3 策略模块尚不存在，导入失败。
- [ ] **步骤 3：实现最小策略模块。** 新增类别优先 `target_score_v3`，复用指标读取但不修改
  旧评分函数；加入 person、vehicle、GOT、UAV123 及逐航拍域退化门。
- [ ] **步骤 4：运行新测试和历史控制器/报告测试。** **预期：** 全部通过，旧 v2 数值仍可
  按原语义复现。
- [ ] **步骤 5：在本任务进度文件记录策略状态/证据并提交**
  `feat: add category-first NanoTrack selection policy`。

### 任务 2：原始帧 ID 配对策略与审计

**文件：**
- 新建：`scripts/nanotrack_pair_sampling.py`
- 新建：`scripts/audit_nanotrack_frame_pairs.py`
- 新建：`tests/test_nanotrack_pair_sampling.py`
- 修改：`scripts/train_nanotrack_uav123.py`
- 修改：`docs/progress/nanotrack-finetune.md`

**接口：**
- 输入：具有 `name`、`frame_range`、`labels`、`videos`、`get_image_anno` 的
  `SubDataset` 实例，以及把数据集前缀映射到 `original_frame_id` 的 JSON 配对策略。
- 输出：`eligible_search_frames(frames, template_id, frame_range, mode) -> list`、
  `install_pair_policy(subdataset_class, policy) -> None`、命令行参数 `--pair-policy`，以及
  任务 4 溯源使用的审计 JSON。

- [ ] **步骤 1：编写失败的单元/集成测试。** 断言帧 `[1,50,200]`、范围 60、模板 50
  在原始 ID 模式得到 `[1,50]`，在历史位置模式得到全部三帧；只为 GOT 安装策略后，伪造
  航拍数据集仍使用原方法；非法/未知策略必须在训练前失败。
- [ ] **步骤 2：运行聚焦测试。** **预期：** 缺少功能导致导入/命令行失败。
- [ ] **步骤 3：实现帧对选择和运行时适配器，并扩展训练器命令行。** checkpoint 记录
  pair-policy SHA-256；恢复时哈希不同则拒绝。
- [ ] **步骤 4：新增并运行审计命令**，读取既有 GOT 清单，写入
  `artifacts/nanotrack_factorial_v3/pair-audit/`；验证 P50/P90/P95/max、同帧比例、旧候选
  被排除比例、清单哈希、序列/帧数。
- [ ] **步骤 5：运行训练恢复测试和全部帧对测试。** **预期：** 全部通过，且
  `repos/siamtrackers/NanoTrack` 下无文件被修改。
- [ ] **步骤 6：在本任务进度文件记录审计路径/哈希并提交**
  `feat: use original frame ids for GOT pair sampling`。

### 任务 3：九单元格精确样本配置生成器

**文件：**
- 新建：`scripts/generate_nanotrack_factorial_configs.py`
- 新建：`tests/test_nanotrack_factorial_configs.py`
- 在 `configs/nanotrack_factorial_v3/` 下生成配置
- 修改：`docs/progress/nanotrack-finetune.md`

**接口：**
- 输入：batch 水平 `(8,32,64)`、GOT 比例 `(0.0,0.1,0.3)`、总样本 40,960、间隔
  2,560、学习率 `2e-5 -> 8e-6`，以及任务 2 的 pair-policy 路径/哈希。
- 输出：九份不可变 YAML 配置和 `experiment-manifest.json`，其中包含精确配额、更新数、
  样本数、种子、输入哈希和输出根目录。

- [ ] **步骤 1：为九个单元格编写失败的表驱动测试。** 人工核对总量、GOT 0% 无 GOT
  条目、GOT 类别配额在整数舍入内等分、航拍比例归一化、评测间隔可整除、不同 batch 的
  学习率一致，以及绝对路径均以实验工件根目录为根。
- [ ] **步骤 2：运行聚焦测试。** **预期：** 生成器导入失败。
- [ ] **步骤 3：实现配额/配置/清单生成。** 完整计划保持 40,960 样本，控制器可先在
  10,240 样本处停止。
- [ ] **步骤 4：生成配置，并为每份配置真实构造 `BANDataset`、抽取样本，不执行优化。**
  **预期：** 九份均可加载；每个解码样本张量有限且形状正确。
- [ ] **步骤 5：运行配置、数据加载器和历史 GOT 配置测试。** **预期：** 全部通过，v1/v2
  生成器不变。
- [ ] **步骤 6：在本任务进度文件记录清单/配置哈希并提交**
  `feat: generate NanoTrack batch GOT factorial configs`。

### 任务 4：可恢复的交叉实验控制器与验证器

**文件：**
- 新建：`scripts/run_nanotrack_factorial.py`
- 新建：`scripts/verify_nanotrack_factorial.py`
- 新建：`tests/test_nanotrack_factorial_controller.py`
- 修改：`docs/progress/nanotrack-finetune.md`

**接口：**
- 输入：任务 1 策略、任务 2 pair-policy/审计、任务 3 清单/配置、既有训练器和三个桌面
  评测器。
- 输出：事务式 `candidate_table.json`、`cell_summary.json`、筛选/扩展决定、精确命令/日志、
  checkpoint/评测哈希和只读验证结果。

- [ ] **步骤 1：编写失败的伪端到端测试。** 覆盖九单元格 × 两种子 × 四节点、样本到更新
  边界、中断复用、身份不匹配拒绝、无隐藏集/视频命令、约束过滤、最多三个扩展单元格、无
  安全组合时的诊断状态，以及 `T_v3` 评分版本绑定。
- [ ] **步骤 2：运行聚焦测试。** **预期：** 控制器导入失败。
- [ ] **步骤 3：复用现有评测器原语、采用新状态/模式实现控制器。** 每项持久化结果绑定
  代码/config/checkpoint/pair-policy/评分策略哈希；已完成节点幂等复用。
- [ ] **步骤 4：实现验证器。** 在不重写证据的前提下，验证完整预期矩阵、精确配额/样本数、
  两种子覆盖、有限指标、工件哈希、评分版本以及未访问隐藏集/视频。
- [ ] **步骤 5：运行伪控制器测试、历史控制器测试和验证器命令行帮助/测试。** **预期：**
  全部通过。
- [ ] **步骤 6：更新本任务进度文件并提交**
  `feat: add resumable NanoTrack factorial controller`。

### 任务 5：验证失败诊断与仅训练集困难样本池

**文件：**
- 新建：`scripts/analyze_nanotrack_failures.py`
- 新建：`scripts/build_nanotrack_failure_matched_pool.py`
- 新建：`tests/test_nanotrack_failure_analysis.py`
- 修改：`docs/progress/nanotrack-finetune.md`

**接口：**
- 输入：现有基线/候选逐序列预测、验证 GT、已准备的训练 split/分组元数据和既有困难原因
  标注。
- 输出：逐序列诊断 JSON、数量受限的联系图、歧义队列，以及带源组不相交溯源的仅训练集
  匹配困难样本清单。

- [ ] **步骤 1：编写失败的合成测试。** 覆盖持续漂移、零 IoU 连续段、尺度/运动几何、边界
  截断、证据不足的遮挡/干扰物分类，以及训练清单拒绝验证源别名。
- [ ] **步骤 2：运行聚焦测试。** **预期：** 分析模块缺失。
- [ ] **步骤 3：实现几何诊断和联系图生成。** 没有证据时不推断遮挡或身份切换，输出
  `ambiguous_requires_human_review`。
- [ ] **步骤 4：根据现有训练标注/原因构造仅训练集样本池**，并与验证/隐藏面板做精确的
  源视频不相交检查。
- [ ] **步骤 5：在现有 g5000/v2 结果上运行工具。** 检查生成的联系图；只有进入队列的
  身份/遮挡歧义需要人工协助，其余明确工作继续自动执行。
- [ ] **步骤 6：运行聚焦测试和溯源验证器，在本任务进度文件记录路径/哈希并提交**
  `feat: diagnose NanoTrack failures without validation leakage`。

### 任务 6：执行初筛与确认实验

**文件：**
- 仅生成：`artifacts/nanotrack_factorial_v3/`
- 修改：`docs/progress/nanotrack-finetune.md`

**接口：**
- 输入：任务 1–4 和冻结的 g5000 身份。
- 输出：实测九单元格/两种子初筛结果、最多三个单元格/完整预算/第三种子确认结果，以及包含
  不确定性和计时证据的 batch/GOT 决定。

- [ ] **步骤 1：重新检查 `nvidia-smi`、CUDA/PyTorch 可用性、checkpoint/config/data 哈希、
  剩余磁盘和是否存在其他训练进程。** 记录精确输出；身份不匹配立即停止。
- [ ] **步骤 2：用修正后的帧对策略，对 batch 8、32、64 分别执行真实前向/反向探针。**
  **预期：** 各完成 50 个有限值步骤，或明确记录失败单元格；禁止静默降级。
- [ ] **步骤 3：运行 18 条初筛轨迹**到 10,240 样本，并评测四个固定节点。中断后恢复，
  不从头重跑。
- [ ] **步骤 4：验证完整初筛矩阵**，在继续训练前冻结扩展决定。
- [ ] **步骤 5：把选中单元格继续到 40,960 样本并运行种子 `20261024`；评测固定节点，
  对 `P`、`V`、`D`、`T_v3` 做按源视频分组 bootstrap。** 若无安全单元格，只运行已声明
  的诊断续跑并保留 g5000。
- [ ] **步骤 6：若 batch 结论仍有因果歧义且满足规格条件，运行单独标注的等更新数诊断。**
  其指标绝不加入主排名。
- [ ] **步骤 7：在本任务进度文件记录命令、中断、指标、哈希、阻塞和选定 batch/GOT 状态；
  提交** `docs: record NanoTrack factorial experiment evidence`。

### 任务 7：困难样本受控跟进、中文报告与最终验证

**文件：**
- 新建：`scripts/report_nanotrack_factorial.py`
- 新建：`tests/test_nanotrack_factorial_reporting.py`
- 在 Git 外生成：`artifacts/nanotrack_factorial_v3/REPORT.zh-CN.md`、验证 JSON 和候选工件
- 修改：`docs/progress/nanotrack-finetune.md`

**接口：**
- 输入：确认后的交叉实验配方、任务 5 匹配困难样本池、任务 1 最终门槛和任务 6 全部证据。
- 输出：无困难强化与困难强化的受控对照结果、最终候选或明确不晋级结论、中文报告和完整
  证据验证结果。

- [ ] **步骤 1：编写失败的报告/验证器测试。** 断言只生成中文报告、不新建或覆盖本轮
  `REPORT.md`、冻结身份、主要证据与等更新数证据分离、无合格最终选择时不存在隐藏集结果，
  以及桌面/RKNN/开发板边界明确。
- [ ] **步骤 2：运行聚焦测试。** **预期：** 报告模块缺失。
- [ ] **步骤 3：实现中文报告和最终验证生成。** 报告必须保留失败/中断尝试和全部协议偏差，
  并列出类别数量、独立源视频组数及 `P`/`V` 置信区间。
- [ ] **步骤 4：用相同起点、预算、种子、节点和评测器，分别运行无困难强化与匹配困难
  强化配方；按预先声明的 `T_v3` 规则冻结更好的安全结果。**
- [ ] **步骤 5：应用最终晋级门。** 只有通过后才冻结 checkpoint 身份并恰好一次调用 GOT
  train-hidden；否则保留 g5000，隐藏集保持未访问。
- [ ] **步骤 6：运行完整仓库 Python 测试套件和实验验证器。** 按名称记录每项失败；局部
  测试不得称为完整通过。
- [ ] **步骤 7：生成中文报告，对全部保留工件计算哈希，在同一提交更新本任务进度文件并
  提交** `docs: finalize NanoTrack factorial fine-tuning evidence`。
