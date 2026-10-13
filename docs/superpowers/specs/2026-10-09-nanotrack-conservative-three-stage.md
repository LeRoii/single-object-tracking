# NanoTrack V3 保守三阶段微调规格

## 目标

从官方 NanoTrack V3 权重重新开始，使用 UAV123、VisDrone-SOT、
VisDrone-MOT 和 UAVDT 执行一次最多九个 epoch 的保守三阶段微调。
本次实验的主要目标不是追求最低训练 loss，而是在无人机车辆与行人场景上
提高跨数据域稳定性，并降低上一轮出现的验证集提升、独立测试集退化问题。

## 已有证据与设计约束

- UAV123-only 实验在 Head-only epoch 3 达到最佳；全 Backbone 解冻后，
  行人验证 AUC 从 0.7221 降到约 0.605。
- 四数据集实验的 epoch 4 在 UAV123 validation 达到最高分，但 epoch 5
  随即回落；选出的 epoch 4 在 UAV123 test 上 overall AUC 从官方基线的
  0.4821 降到 0.4394，person AUC 从 0.6141 降到 0.5228。
- 同一个四数据集模型在 VisDrone-SOT validation 上从 0.5872 提升到
  0.6302，说明无人机域数据有效，问题主要是更新范围、采样平衡和选模稳定性，
  而不是应该放弃这些数据。
- 当前没有 GOT-10k、LaSOT 或 TrackingNet 等通用数据回放，因此不得再次
  全量更新 Backbone；官方权重承担通用匹配能力的锚点作用。
- 单卡为 RTX 2060 6 GiB。batch size 固定为 8；已有实测峰值远低于 6 GiB。
- 随机种子固定为 `123456`。源数据目录保持只读。

## 输出隔离

新实验不得覆盖前两次实验：

- 新准备数据：`data/nanotrack_conservative_three_stage/`
- 新实验产物：`artifacts/nanotrack_conservative_three_stage/`
- 新配置：`configs/nanotrack_v3_three_stage_*.yaml`
- 新工具和测试使用独立文件名；现有评测脚本可复用但不得改变既有结果。

## 数据重新划分

### UAV123

继续使用已经按物理源文件夹分组的固定划分：

- train：85 个逻辑序列；
- validation：17 个逻辑序列；
- 原 test：21 个逻辑序列，仅作为已知回归集，不再宣称是下一轮的完全无偏测试集。

### 三个新增无人机数据源

对 VisDrone-SOT 官方 train、VisDrone-MOT 官方 train、UAVDT 官方 MOT train
重新按 `source_video` 做固定近似 80/10/10 分组。实现必须先对视频 ID 排序，
再用种子 `123456` 的独立随机生成器打乱；validation 和 hidden-test 的视频数均为
`max(1, floor(0.1 * N))`，其余全部归入 train。按当前数据量，固定计数为：

| 来源 | Train 视频 | Validation 视频 | Hidden-test 视频 |
|---|---:|---:|---:|
| VisDrone-SOT | 70 | 8 | 8 |
| VisDrone-MOT | 46 | 5 | 5 |
| UAVDT | 24 | 3 | 3 |

分组语义为：

- 80%：新训练集；
- 10%：`val-aerial`，参与 checkpoint 选择；
- 10%：`test-aerial-hidden`，训练和选模期间禁止评测，只在最终模型确定后运行一次。

同一源视频产生的所有 target ID 和分段轨迹必须处在同一 split，并输出视频级
split JSON、类别计数和交集审计。VisDrone 官方 val、
UAV123 test 作为已知回归集；VisDrone 官方 test-dev 不参与训练。

`val-aerial` 的重复评测面板固定为：8 条 VisDrone-SOT validation 序列、
VisDrone-MOT validation 中按固定顺序选取最多 30 条 person、30 条 vehicle、
10 条 bike 轨迹，以及 UAVDT validation 中最多 20 条 vehicle 轨迹。MOT 轨迹
评测使用分段后未做 24 帧训练抽样的连续有效标注帧，单轨迹最多评测连续 300 帧；
超过 300 帧时取以轨迹中点为中心的连续窗口。`test-aerial-hidden` 最终评测使用
全部 8 条 VisDrone-SOT hidden-test 序列，并对 MOT/UAVDT hidden-test 使用同样的
类别上限和 300 帧窗口规则。面板成员和窗口起止帧必须写入 JSON，候选之间不变。

### MOT 轨迹密度

VisDrone-MOT 和 UAVDT 的合格轨迹仍要求至少 30 个有效标注帧、长缺口处分段，
但每条轨迹从当前最多 12 帧提高到最多 24 个确定性均匀帧。必须同时保留近邻、
中距离和长距离候选，而不是只保留间距过大的样本。所有新 manifest 必须验证
crop 存在且可通过 NanoTrack `BANDataset` 解码。

## 训练采样

每个 epoch 抽取 20,000 个训练样本对，batch size 8。

### 阶段一和正常样本池

阶段一 source 权重：

| 来源 | 每 epoch 样本数 | 比例 |
|---|---:|---:|
| UAV123 | 5,000 | 25% |
| VisDrone-SOT | 7,000 | 35% |
| VisDrone-MOT | 5,000 | 25% |
| UAVDT | 3,000 | 15% |

具有可靠类别的数据拆成独立 manifest，以 `NUM_USE` 实现分层采样：

- UAV123 5,000：person 2,500、vehicle 1,500、other 750、bike 250；
- VisDrone-MOT 5,000：person 3,000、vehicle 1,500、bike 500；
- UAVDT 3,000：vehicle 3,000；
- VisDrone-SOT 7,000：由于官方 SOT 标注不含可靠类别，保持 source-level 采样。

阶段二正常 source 权重调整为 UAV123/VisDrone-SOT/VisDrone-MOT/UAVDT =
20/40/25/15，对应 4,000/8,000/5,000/3,000。UAV123 的 4,000 为
person 2,000、vehicle 1,200、other 600、bike 200；VisDrone-MOT 仍为
person 3,000、vehicle 1,500、bike 500。

### 困难样本池

困难样本只能从训练 split 构建，不能从任何 validation/test/regression 集回流。
以下任一条件可标记为困难帧或困难轨迹：

- 原图中目标框面积低于图像面积 1%；
- 轨迹内目标面积最大值/最小值不低于 2；
- 相邻有效标注间中心位移超过前一目标框对角线的 0.5 倍；
- VisDrone 标注为截断或遮挡；
- MOT 视频中存在邻近同类别干扰目标。

困难 manifest 保留困难帧及其最近的前后可见帧，每条可训练轨迹至少两个帧。
阶段三每 epoch 的 20,000 对由 10,000 正常池和 10,000 困难池组成；困难池中
优先保证 person 与 vehicle 接近 1:1，并限制纯 UAVDT vehicle 不超过困难池 15%。

阶段三正常池固定为：UAV123 2,000（person/vehicle/other/bike =
1,000/600/300/100）、VisDrone-SOT 4,000、VisDrone-MOT 2,500
（person/vehicle/bike = 1,500/750/250）、UAVDT vehicle 1,500。
困难池固定为：VisDrone-SOT 3,000、VisDrone-MOT person/vehicle/bike =
2,000/2,000/500、UAV123 person/vehicle/other/bike = 750/750/250/250、
UAVDT vehicle 500。若某个困难子池为空，准备步骤必须失败，不得静默把配额转给
其他类别。

## 三阶段训练

所有阶段均从上一个阶段按验证规则选出的最佳 checkpoint 开始，而不是默认使用
最后一个 checkpoint。Neck 始终冻结且不进入优化器。

### 阶段一：Head 稳定适配

- 最大 4 epoch；最少完成 3 epoch。
- Backbone 全部冻结，所有 Backbone BN 固定为 eval。
- 只训练 BAN 分类头和回归头。
- Head LR 从 `1e-4` 对数下降到 `4e-5`。
- 使用 25/35/25/15 source 权重。

### 阶段二：选择性域适配

- 从阶段一最佳 checkpoint 初始化，最大 3 epoch。
- 只解冻 `model.backbone.features` 最后两个 inverted-residual block；更早层冻结。
- 所有 BatchNorm 的参数和 running statistics 始终冻结。
- Head LR 从 `2e-5` 下降到 `1e-5`。
- Backbone 尾部 LR 从 `1e-6` 下降到 `5e-7`。
- 使用 20/40/25/15 source 权重。
- 若第一轮已触发严重回退，可直接结束阶段二并恢复阶段一最佳模型。

### 阶段三：困难样本收敛

- 从阶段一、二所有合格 checkpoint 中的最佳模型初始化，最大 2 epoch。
- Backbone 重新全部冻结，BN 固定为 eval。
- 只训练 BAN Head，LR 从 `5e-6` 下降到 `2e-6`。
- 每轮 50% 正常样本、50% 困难样本。

最大训练预算为 9 epoch、180,000 个样本对和 22,500 个 optimizer step；提前停止
后允许实际只执行 5 至 8 个 epoch。

## Checkpoint、恢复和评测频率

- 每 250 optimizer step 覆盖写入可恢复 checkpoint。
- 每 10,000 个样本对，即每半个 epoch，额外保存不可覆盖的候选 checkpoint。
- 每个候选 checkpoint 完成 UAV123 validation 与 `val-aerial` 评测。
- 每个阶段独立保存训练指标、resolved config、显存峰值和 checkpoint 哈希。
- 中断恢复必须保持 global step 单调、删除 checkpoint 之后的多余指标，并恢复当前
  阶段、阶段内 epoch 和半 epoch 位置。

## 选模和提前停止

每个验证域先按序列宏平均，再在数据域之间宏平均，避免长视频或单一数据集支配。
主分数为：

```text
score = 0.4 * person_AUC + 0.4 * vehicle_AUC + 0.2 * overall_AUC
```

对于不含某类别的验证域，只对该域存在的项重新归一化，不能把缺失类别当作 0。

候选 checkpoint 只有同时满足下列条件才可以替换当前最佳模型：

- score 至少提高 0.005；
- person 和 vehicle AUC 相对当前阶段最佳均不得下降超过 0.01；
- 任一验证域相对官方基线不得下降超过 0.02。

提前停止条件：

- 连续两个半 epoch 评测点没有合格提升；或
- person/vehicle AUC 相对当前阶段最佳下降超过 0.02；或
- 任一验证域整体 AUC 下降超过 0.03。

阶段一至少运行 3 epoch 后才允许按 patience 停止。阶段二、三可提前结束。

## 最终评测

确定最终 checkpoint 后，按以下顺序评测且不再反向调整模型：

1. `test-aerial-hidden`，仅运行一次；
2. UAV123 原 test，作为已知回归集；
3. VisDrone-SOT 官方 validation，作为已知回归集；
4. 固定 `sot-v1.mp4` 两目标视频回归。

对官方权重、上一轮四数据集 epoch 4 和本轮最佳模型使用相同 evaluator。报告必须
同时展示改善和退化，不因本轮没有超过基线而改变选模规则。

## 验收标准

- 数据 split 审计证明所有源视频级交集为 0。
- 四个训练源均贡献非零样本；配置中的 source 和类别采样数与规格一致。
- 正常池和困难池均能通过 `BANDataset` 实际解码 511x511 模板/搜索图。
- 阶段一 Backbone 全冻结；阶段二仅最后两个 block 可训练；阶段三再次全冻结；
  所有阶段 BN 均为 eval 且参数不可训练。
- CUDA smoke 覆盖三个阶段的 trainability policy，loss 有限且显存低于 6 GiB。
- 每个实际执行的候选 checkpoint 有验证汇总；提前停止决定有机器可读记录。
- 最终 checkpoint 可重载并完成三个评测集合与双目标视频回归。
- 完整测试套件通过，最终报告包含命令、哈希、数据覆盖、阶段指标、选模理由、
  中断记录和限制。
- 桌面 GPU 结果不得描述为 RK3566/RKNN 板端证明。
