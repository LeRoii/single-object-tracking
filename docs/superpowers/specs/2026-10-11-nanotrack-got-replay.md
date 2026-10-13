# NanoTrack V3 GOT-10K 回放微调规格

## 目标

从上一轮选出的 `candidate_stage1_g00005000_e001_s02500.pth` 继续，仅训练 BAN head，
加入 GOT-10K 通用数据回放，通过两阶段、双随机种子的受控实验提高无人机车辆与行人跟踪
性能，同时限制 GOT-10K validation 和 UAV123 validation 的回退。

## 固定输入与隔离输出

- 初始化权重：`artifacts/nanotrack_conservative_three_stage/run/stage1/candidate_stage1_g00005000_e001_s02500.pth`
- 随机种子：`123456`、`20261011`
- GOT-10K 根目录：`/home/u20/dataset/GOT-10K`
- 新数据目录：`data/nanotrack_got_replay_v1/`
- 新产物目录：`artifacts/nanotrack_got_replay_v1/`
- 不修改前两轮数据、checkpoint、评测结果和报告。
- 不运行 `sot-v1.mp4` 视频回归，也不把它列为完成条件。

## GOT-10K 数据协议

- 只从官方 train 的 9,335 条序列构建训练和本地隐藏集；官方 val 180 条作为反复验证集；
  test 因只有第一帧标注，不用于本地计分。
- 在 train 内按 root class 分层固定留出 467 条隐藏序列：animal 169、person 114、
  vehicle 112、object part 49、passive motion object 23；剩余 8,868 条用于训练。
- 每条训练序列最多保留 48 帧：36 个确定性均匀锚点，加最多 12 个形变、尺度、运动、
  遮挡或覆盖变化困难帧；过滤 absence=1、cover<=0 和无效框。
- 输出 person、vehicle、other 三个互斥训练 manifest 和 crop511，并审计序列数、帧数、
  split 交集、引用文件和严格解码。

## 两阶段训练

两个阶段均冻结全部 backbone、neck 和所有 BN，只更新 BAN 分类/回归 head。

### Stage 1：通用回放适配

- 最多 4 个候选区间；参考区间为 10,000 个样本对。
- 每个区间的采样比例：GOT-10K 30%，UAV123 16.25%，VisDrone-SOT 22.5%，
  VisDrone-MOT 22.5%，UAVDT 8.75%。
- GOT person/vehicle/other 各占区间 10%。
- 参考 LR `2e-5 -> 8e-6`。

### Stage 2：无人机正常与困难样本细化

- 从 Stage 1 合格最佳 checkpoint 初始化，最多 3 个候选区间。
- 每区间：GOT normal 25%，无人机 normal 50%，无人机 hard 25%；类别和来源按已批准
  配额同比缩放。
- 参考 LR `8e-6 -> 3e-6`。

## 固定 batch 与 OOM 保护

- 按用户 2026-10-12 的明确要求，采用官方训练所用的固定 batch size 64；不再继续探测
  128 及以上候选。两个种子必须共用这一冻结值。
- 用 Stage 2 最重配置对 batch 64 执行至少 50 step 的真实
  forward/backward/optimizer 安全验证；要求无 OOM、NaN、Inf，峰值 reserved 不超过
  总显存 88%，且最小空闲显存不少于 768 MiB。
- 若正式阶段仍 OOM，从阶段起点以低一级 batch 32 重启，禁止在阶段中途改变 batch。
- 学习率按 `sqrt(batch/8)` 缩放且最大 4 倍；候选区间按样本数定义，并扩展到至少
  100 个 optimizer step，避免超大 batch 只有少量更新。

## 选模

- `T`：无人机 validation 目标宏平均，域内按 person 0.4、vehicle 0.4、overall 0.2，
  缺失项重新归一化后再跨域宏平均。
- `G`：GOT-10K val AO；同时报告 SR0.5、SR0.75。
- `U`：UAV123 validation overall AUC。
- 综合分数 `C = 0.70*T + 0.15*G + 0.15*U`。
- 阶段内替换要求 `C` 至少提升 0.002，且 G、U 相对阶段最佳各不下降超过 0.01。
- 最终晋级要求 T 相对初始化模型至少提升 0.005，G、U 各不低于初始化模型 0.01，
  person/vehicle 类别项不得下降超过 0.015。
- 两个种子都必须产出可评估候选；按 C 选择一个最终模型后，GOT train-hidden 只运行一次。

## 最终评测与完成条件

- 首先冻结最终 checkpoint，然后仅一次评测 GOT train-hidden。
- 运行 GOT-10K val、无人机 validation、UAV123 validation；UAV123 test 和
  VisDrone2019-SOT-val 作为已知回归集。
- 不执行任何视频回归。
- 记录所有命令、配置、哈希、自动 batch 证据、显存峰值、候选指标、OOM/中断、最终选择
  和限制；完整测试与 verifier 必须通过。

## v1 结果与校正试验（2026-10-12）

- v1 的两个种子均完成四个 Stage 1 候选，batch 64 稳定且无 OOM/NaN；但所有候选均未
  通过替换门槛，因此 Stage 2 未启动、GOT train-hidden 未访问。
- 审计发现生成配置只有一个 epoch，而旧训练器只按 epoch 更新 LR，导致 628 个 step
  全部使用 `5.656854e-5`，配置中的终点 LR 实际未生效。
- 保留 v1 全部证据，新增隔离的 `configs/nanotrack_got_replay_v2/` 与
  `artifacts/nanotrack_got_replay_v2/` 校正试验：batch 固定 64；LR 改为按 stage step
  对数衰减；Stage 1 使用未按 batch 放大的参考值 `2e-5 -> 8e-6`，Stage 2 使用
  `8e-6 -> 3e-6`。
- v2 沿用相同数据、种子、样本区间、选模门槛和隐藏集隔离规则；先评估第一个候选，只有
  趋势合理时才继续完整双种子试验。
