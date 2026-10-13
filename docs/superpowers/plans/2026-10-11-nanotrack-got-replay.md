# NanoTrack V3 GOT-10K 回放微调实施计划

> 执行方式：工作区 Git 元数据不可用，使用隔离数据/产物目录和 `.superpowers/sdd` ledger
> 记录 RED→GREEN、命令、裁决及验证证据。

**规格：** `docs/superpowers/specs/2026-10-11-nanotrack-got-replay.md`

## Task 1：GOT-10K 解析、分层隐藏集与帧采样

- 新建 `scripts/nanotrack_got_replay_data.py` 与 `tests/test_nanotrack_got_replay_data.py`。
- 先测试再实现目录发现、root class 解析、467 条分层隐藏集、8,868 条 train、有效帧过滤、
  36+12/最多 48 帧确定性采样和 person/vehicle/other 互斥分类。

## Task 2：GOT crop511、manifest、官方 val/隐藏评测面板

- 扩展数据脚本，生成 `data/nanotrack_got_replay_v1/`。
- 复用已验证 SiamFC crop 逻辑，完成引用、解码、split 和 BANDataset 审计。
- 新建 GOT OPE evaluator，输出 AO、SR0.5、SR0.75，并覆盖合成单元测试。

## Task 3：固定 batch 安全验证与样本尺度训练接口

- 保留已测试的候选 batch 决策、显存安全门槛、LR 缩放和样本区间到 step 换算接口；
  按用户最新要求将正式 batch 固定为 64，不继续探测 128 及以上候选。
- 扩展训练入口支持运行时 batch、动态候选步长和样本数元数据，保持精确恢复语义。
- 在真实 CUDA 上对 batch 64 做 50-step 安全验证，并冻结为两个种子共用的 batch。

## Task 4：两阶段双种子控制器与配置

- 新建两阶段配置、命令工厂、候选评测和事务化控制器。
- Stage 1/2 均严格 head-only；实现 C 分数、替换/停止/晋级门槛和幂等恢复测试。

## Task 5：基线、正式训练与在线验证

- 对初始化权重生成 GOT-val、无人机 validation、UAV123 validation 基线。
- 执行两个种子的 Stage 1、Stage 2；每个候选都评估并记录，发生 OOM 时按规格从阶段起点
  降一级 batch 重启。

## Task 6：冻结选模、一次隐藏评测、已知回归与报告

- 按 C 在合格候选中选最终模型；只运行一次 GOT train-hidden。
- 运行规定的已知回归，不运行 `sot-v1.mp4`。
- 生成中英文报告、完整 verifier、checkpoint reload 和全测试证据。

## Review Focus

- GOT train-hidden 不得进入训练、反复验证或选模。
- 固定 batch 64 的安全证据必须来自真实反向传播峰值，不能通过无意义显存预占伪造。
- 动态 batch 后候选边界、LR、恢复位置和数据配额必须以样本语义一致。
- 两阶段只能更新 BAN head；两个种子必须共用已冻结 batch。
- 最终隐藏评测只能发生在选择冻结后；不得生成或声称视频回归。
