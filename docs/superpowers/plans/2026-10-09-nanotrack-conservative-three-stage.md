# NanoTrack V3 保守三阶段微调实施计划

> **执行要求：** 当前工作区不是 Git 仓库，无法创建 worktree 或提交。使用
> `superpowers:executing-plans` 在当前会话内执行，并在独立 ledger 中记录每个任务的
> RED→GREEN、命令、产物和裁决。

**目标：** 按已批准规格，从官方 NanoTrack V3 权重重新执行一次最多九 epoch 的
保守三阶段微调，完成新数据隔离、困难样本采样、阶段性选模、隐藏测试和回归报告。

**规格：** `docs/superpowers/specs/2026-10-09-nanotrack-conservative-three-stage.md`

**架构：** 新增一个纯数据模块，负责源视频级 80/10/10 分组、类别子 manifest、
困难样本池和航空评测面板；扩展现有训练入口以支持 Head-only、尾部两 block 解冻、
全程冻结 BN、半 epoch 候选 checkpoint 和评测回调；新增统一航空轨迹 evaluator 与
阶段控制器，按机器可读门槛选择/提前停止。所有数据和产物写入新的隔离目录。

**技术栈：** Python 3.7、PyTorch 1.7.1 CUDA 10.2、OpenCV、NumPy、YACS、
NanoTrack/PySOT dataset loader、unittest。

## 全局约束

- 种子 `123456`；源数据只读。
- batch 8；每 epoch 20,000 对；每半 epoch 10,000 对/1,250 step。
- Stage 1 最多 4 epoch，Stage 2 最多 3 epoch，Stage 3 最多 2 epoch。
- 训练从官方 `nanotrackv3.pth` 开始，不从上一轮 epoch 4 继续。
- UAV123 原 test 和 VisDrone-SOT 官方 val 只作已知回归，不参与新选模。
- `test-aerial-hidden` 在最终 checkpoint 确定前不得执行。
- 旧的 `data/nanotrack_four_dataset` 和 `artifacts/nanotrack_four_dataset_finetune`
  不得覆盖。
- 每个代码任务严格执行 TDD；最终完成必须有新鲜完整验证证据。

## 文件结构

- 创建 `scripts/nanotrack_three_stage_data.py`：split、类别 manifest、困难池和评测面板。
- 创建 `scripts/evaluate_aerial_tracklets_nanotrack.py`：连续航空轨迹 OPE evaluator。
- 创建 `scripts/run_nanotrack_three_stage.py`：阶段控制、候选评测、门槛和提前停止。
- 修改 `scripts/train_nanotrack_uav123.py`：显式 trainability policy、BN 冻结、
  候选 checkpoint 和可恢复阶段状态。
- 创建 `tests/test_nanotrack_three_stage_data.py`。
- 创建 `tests/test_aerial_tracklet_evaluation.py`。
- 修改 `tests/test_nanotrack_training.py`。
- 创建 `tests/test_nanotrack_three_stage_runner.py`。
- 创建 `configs/nanotrack_v3_three_stage_stage1.yaml`。
- 创建 `configs/nanotrack_v3_three_stage_stage2.yaml`。
- 创建 `configs/nanotrack_v3_three_stage_stage3.yaml`。
- 创建 `configs/nanotrack_v3_three_stage_smoke_*.yaml`。
- 生成 `data/nanotrack_conservative_three_stage/`。
- 生成 `artifacts/nanotrack_conservative_three_stage/`。

## Review Focus

- 同一源视频不得跨 train/validation/hidden-test。
- 类别 manifest 不得重复或丢失 tracklet，且配额必须与规格精确一致。
- 困难池不得引用 validation、hidden-test 或已知回归数据。
- Stage 2 只能更新 `backbone.features[-2:]` 与 BAN Head；所有 BN 参数和统计量冻结。
- 半 epoch 恢复后 global step、stage/epoch/step 位置必须单调且无重复指标。
- 候选替换和提前停止必须严格使用规格阈值，hidden-test 不能参与。
- 最终报告必须如实记录退化，不得用桌面结果宣称 RK3566 已验证。

---

### Task 1：确定性源视频分组与航空评测规格

**文件：**

- 创建：`scripts/nanotrack_three_stage_data.py`
- 创建：`tests/test_nanotrack_three_stage_data.py`

**接口：**

- `split_source_videos(tracklets, seed=123456) -> split map`
- `build_evaluation_panel(tracklets, split_map, split, limits) -> sequence specs`
- `AerialSequenceSpec` 保存 name、source、category、连续图像路径、逐帧可选 xywh。

**步骤：**

- [ ] 先写失败测试：86/56/30 个视频必须得到 70/8/8、46/5/5、24/3/3；
  同源所有 ID 同 split；三组交集为 0；输入顺序变化不改变结果。
- [ ] 写失败测试：validation 面板严格限制 MOT person/vehicle/bike 为 30/30/10，
  UAVDT vehicle 为 20；超过 300 帧时取中间连续窗口。
- [ ] 实现最小分组、面板 dataclass 和 JSON 序列化。
- [ ] 运行：`.venvs/nanotrack/bin/python -m unittest tests.test_nanotrack_three_stage_data -v`
  期望：全部通过。
- [ ] 运行完整 unittest；记录结果。

### Task 2：24 帧准备、类别子 manifest 与困难池

**文件：**

- 修改：`scripts/nanotrack_three_stage_data.py`
- 修改：`tests/test_nanotrack_three_stage_data.py`
- 生成：`data/nanotrack_conservative_three_stage/`

**接口：**

- 输入现有 UAV123 preparation 与三个官方无人机 train 源。
- 输出 split JSON、每 source/category 的 normal JSON、hard JSON、crop511 和 audit。

**步骤：**

- [ ] 先写失败测试：MOT 训练轨迹最多 24 帧且含首尾与近邻候选；validation/test
  不被 24 帧训练抽样破坏。
- [ ] 先写失败测试：类别子 manifest 互斥、并集等于训练 manifest；困难帧满足至少
  一条规格规则；困难轨迹至少两帧；非 train source 必须被拒绝。
- [ ] 实现 small/scale/motion/occlusion/distractor 标记和困难帧邻域扩展。
- [ ] 实现所有 normal/hard manifest、evaluation panel、审计与哈希输出。
- [ ] 用合成图执行 crop/manifest 集成测试并观察 GREEN。
- [ ] 在真实三数据源上准备隔离数据，允许复用已存在且解码为 511x511 的 crop。
- [ ] 全量检查 manifest 引用路径，抽样严格解码，并证明 split 交集为 0。
- [ ] 通过真实 `BANDataset` 分别加载每个 normal/hard 子池样本。

### Task 3：三阶段 trainability policy 与候选 checkpoint

**文件：**

- 修改：`scripts/train_nanotrack_uav123.py`
- 修改：`tests/test_nanotrack_training.py`

**接口：**

- 新增 policy：`head_only`、`tail2`。
- `configure_model_for_policy(model, policy, freeze_bn=True)`。
- checkpoint 记录 stage、epoch、step_in_epoch、global_step、policy 和 RNG 状态。
- 每 1,250 step 保存不可覆盖候选，每 250 step 保存 `resume.pth`。

**步骤：**

- [ ] 先写失败测试：Head-only 仅 BAN Head 可训练；tail2 仅 BAN Head 和
  `features[-2:]` 可训练；Neck 和全部 BN 参数冻结且 BN 为 eval。
- [ ] 先写失败测试：optimizer 仅含实际可训练参数，head/backbone LR 精确匹配。
- [ ] 先写失败测试：候选命名、阶段元数据、RNG 恢复、metrics 截断及 epoch 边界。
- [ ] 实现 policy、optimizer 和 checkpoint 扩展，保留旧调用兼容性。
- [ ] 为候选 checkpoint 添加原子临时文件后替换，防止中断产生半文件。
- [ ] 运行 focused tests，再运行完整 unittest。

### Task 4：航空轨迹 evaluator 与跨域评分

**文件：**

- 创建：`scripts/evaluate_aerial_tracklets_nanotrack.py`
- 创建：`tests/test_aerial_tracklet_evaluation.py`

**接口：**

- 输入 Task 1 panel JSON、NanoTrack config、snapshot。
- 输出逐轨迹预测、逐 source/category metrics 和宏平均 summary。
- 复用 UAV123 evaluator 的 OPE 定义。

**步骤：**

- [ ] 先写失败测试：完美预测 AUC=1、完全错位 AUC=0；缺失 GT 帧不中断跟踪且不
  计分；宏平均不受长序列帧数支配。
- [ ] 先写失败测试：person/vehicle 缺失域按现有项重新归一化，不把缺失当 0。
- [ ] 实现 panel loader、连续跟踪、summary 和 snapshot hash。
- [ ] 与 UAV123 evaluator 对同一组合成框交叉检查，AUC 差值为 0。
- [ ] 运行 focused 和完整 unittest。

### Task 5：阶段控制、选模与提前停止

**文件：**

- 创建：`scripts/run_nanotrack_three_stage.py`
- 创建：`tests/test_nanotrack_three_stage_runner.py`

**接口：**

- 控制器依次执行 stage1/2/3；每半 epoch 调用训练入口和两个 validation evaluator。
- 输出 `run_state.json`、`candidate_table.json`、`selection.json`、`interruptions.json`。
- 支持从已有 run state 幂等恢复；hidden-test 命令只能在 `selection_final=true` 后运行。

**步骤：**

- [ ] 先写失败测试：score 公式、`+0.005` 替换阈值、类别回退 0.01、域回退 0.02、
  patience=2、严重回退规则全部覆盖边界值。
- [ ] 先写失败测试：stage1 不得在 3 epoch 前停；stage2/3 可提前停；下一阶段必须
  从上一阶段选中 checkpoint 而非 last 初始化。
- [ ] 先写失败测试：hidden-test 在最终选择前调用必须失败；恢复后不得重评已完成候选。
- [ ] 实现纯函数 decision engine，再实现 subprocess orchestration 和原子 state 写入。
- [ ] 使用伪训练/伪 evaluator 完成端到端控制器测试。
- [ ] 运行 focused 和完整 unittest。

### Task 6：配置与三策略 CUDA smoke

**文件：**

- 创建：`configs/nanotrack_v3_three_stage_stage1.yaml`
- 创建：`configs/nanotrack_v3_three_stage_stage2.yaml`
- 创建：`configs/nanotrack_v3_three_stage_stage3.yaml`
- 创建：`configs/nanotrack_v3_three_stage_smoke_*.yaml`
- 生成：`artifacts/nanotrack_conservative_three_stage/smoke/`

**步骤：**

- [ ] 写配置测试，精确核对三个阶段的 source/category `NUM_USE` 总和、LR、epoch、
  batch 和 policy。
- [ ] 生成配置并确认每个阶段每轮恰好 20,000 对。
- [ ] 在 RTX 2060 上分别运行 Head-only、tail2、hard Head-only 的有限 CUDA
  forward/backward smoke。
- [ ] 记录可训练参数名、BN 状态、loss、峰值显存；要求全部 loss 有限且低于 6 GiB。
- [ ] 每个 smoke checkpoint 重新加载并在一条 held-out 序列上推理。

### Task 7：执行最多九 epoch 的正式训练与阶段选模

**文件/产物：**

- 生成：`artifacts/nanotrack_conservative_three_stage/stage1/`
- 生成：`artifacts/nanotrack_conservative_three_stage/stage2/`
- 生成：`artifacts/nanotrack_conservative_three_stage/stage3/`
- 生成：`artifacts/nanotrack_conservative_three_stage/validation/`

**步骤：**

- [ ] 先评测官方模型在 UAV123-val 与 `val-aerial` 的 baseline。
- [ ] 启动控制器；持续观察同一真实进程/状态文件，不能用无句柄的意向代替进度。
- [ ] 每半 epoch 核验候选 checkpoint、有限 loss、显存、两个 validation summary 和
  decision 记录。
- [ ] 按规则完成/提前停止 Stage 1，再以选中 checkpoint 初始化 Stage 2。
- [ ] 按规则完成/提前停止 Stage 2，再从 Stage 1/2 合格候选中的最佳初始化 Stage 3。
- [ ] 按规则完成/提前停止 Stage 3，冻结最终 selection；不得查看 hidden-test 后改选。
- [ ] 核验所有 metrics 的 stage/global step 唯一性和 checkpoint 可重载性。

### Task 8：隐藏测试、已知回归与最终报告

**文件/产物：**

- 生成：`artifacts/nanotrack_conservative_three_stage/final/`
- 创建：`artifacts/nanotrack_conservative_three_stage/REPORT.md`
- 创建：`artifacts/nanotrack_conservative_three_stage/verification.json`

**步骤：**

- [ ] selection 冻结后首次运行 `test-aerial-hidden`。
- [ ] 用官方模型、上一轮 epoch 4、本轮最佳模型运行 UAV123 test 和
  VisDrone-SOT 官方 val 相同 evaluator 对照。
- [ ] 在 `sot-v1.mp4` 两个固定初始框上生成本轮两个回归视频和逐帧框。
- [ ] 运行完整 unittest、数据 verifier、checkpoint reload、严格视频解码和 JSON 校验。
- [ ] 写报告：精确命令、哈希、数据覆盖、执行 epoch、提前停止理由、各域/类别指标、
  改善与退化、中断记录、桌面 FPS 边界和 RK3566 非证明声明。
- [ ] 按规格逐项完成验收审计；任何缺失保持目标 active。

## 完成条件

只有 Task 1–8 全部有当前状态证据、最终完整测试通过、报告中承诺的所有 checkpoint、
summary、视频和 verifier 实际存在且可读取时，才允许将目标标记为 complete。
