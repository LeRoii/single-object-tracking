# RK3566 遮挡等待状态与 Detector 全候选可视化设计

## 1. 背景与目的

当前长时跟踪控制器在目标渐进遮挡时存在两个相互强化的问题：

1. NanoTrack 在 `LOCAL_RECOVERY` 中仍逐帧推理。目标只剩局部可见区域时，anchor
   和 recent 模板可能共同输出逐渐收缩的原始观测框。虽然 `WEAK` 观测不会被
   `commit_bbox()`，Debug 窗口仍会显示这些原始框。
2. 运动估计器同时预测中心、宽度和高度。遮挡前最后几帧的已接受框如果持续变窄，
   会形成负的宽高速度；失去可靠观测后，运动预测框仍会继续收缩，并作为下一次
   NanoTrack 搜索的参考尺度，形成尺度坍缩正反馈。

此外，Detector 一批结果目前只在控制器内部筛选和排队，交互式 Debug 窗口只显示
当前活动候选，无法直接判断问题来自 detector 输出、位置硬门还是候选调度。

本设计增加独立的 `OCCLUSION_HOLD` 状态：在判断发生疑似尺度收缩遮挡、短时间内不可能
重捕时，暂停 NanoTrack 和 detector，保持中心运动预测但冻结目标尺度。等待期结束后
再恢复局部搜索，避免用无目标图像持续污染搜索参考和尺度趋势。同时在 detector 完成
推理时显示该批次全部可用检测结果及筛选原因。

## 2. 已选方案与备选方案

### 2.1 已选：独立 `OCCLUSION_HOLD` 状态

状态机显式增加 `OCCLUSION_HOLD`，使运行行为、日志、画面和测试都能明确表达：
当前不是算力不足或 detector 故障，而是控制器有意等待遮挡期结束。

优点：

- 状态语义清楚，便于板端调试和后续 RK3566 移植；
- 可直接验证 HOLD 期间 tracker/detector 调用次数为零；
- 可独立配置等待时间和触发阈值；
- 不会把“不搜索”和普通 `LOCAL_RECOVERY` 混在同一状态中。

代价：状态枚举、日志 schema、控制器、运行时和测试均需同步修改。

### 2.2 未选：在 `LOCAL_RECOVERY` 内增加 `hold_active`

代码改动较小，但同一个状态下有时运行 tracker、有时完全跳过，日志与问题定位容易
产生歧义，不适合当前以板端可观测性为重点的 Debug 版本。

### 2.3 未选：HOLD 期间周期性探测

周期运行 NanoTrack 或 detector 可以更早发现提前重现的目标，但会重新引入遮挡期
无效观测、尺度漂移和额外算力。本轮按用户确认的策略采用固定等待窗口，窗口内不做
tracker update/inference，也不发起 detector。

## 3. 状态机与时序

### 3.1 正常恢复流程

```text
TRACKING
   |
   | 非可靠观测开始
   v
UNCERTAIN -------- 普通丢失 --------> LOCAL_RECOVERY
   |
   | 连续非可靠 + 单尺度明显收缩
   v
OCCLUSION_HOLD
   |
   | hold_remaining_frames == 0
   v
LOCAL_RECOVERY ---- 超过本地恢复时间 ----> ASSISTED_RECOVERY ----> LOST
                                              |
                                              +-- detector 候选
                                                  -> NanoTrack 双模板验证
                                                  -> 同候选 2/3 确认
                                                  -> TRACKING
```

`OCCLUSION_HOLD` 是恢复路径中的临时覆盖状态。`loss_start_ms` 继续从首次未接受帧计时，
HOLD 不会重置损失时间。等待结束后按累计损失时间决定进入 `LOCAL_RECOVERY`、
`ASSISTED_RECOVERY` 或 `LOST`；如果配置了很长的 HOLD，允许结束后直接进入更后阶段。

### 3.2 进入 HOLD 的条件

默认配置：

```text
occlusion_hold_frames=12
occlusion_trigger_consecutive_unreliable=2
occlusion_trigger_min_dimension_ratio=0.70
occlusion_reference_window=8
```

控制器维护最近 `occlusion_reference_window` 个已接受且质量为 `RELIABLE` 的框，使用
宽度中位数和高度中位数组成稳健尺度参考，避免参考框被单帧异常或渐进遮挡快速拖小。
参考中心不取中位数；进入 HOLD 时中心仍来自当前 motion prediction。

每帧从 anchor/recent 中按现有规则选出观测后，计算：

```text
width_ratio  = selected_bbox.width  / reference_width
height_ratio = selected_bbox.height / reference_height
min_dimension_ratio = min(width_ratio, height_ratio)
```

满足以下全部条件时进入 HOLD：

1. 当前 plan 状态属于 `TRACKING`、`UNCERTAIN` 或 `LOCAL_RECOVERY`；
2. 当前尚未处于 detector 候选验证流程；
3. 本次 loss episode 尚未使用过 HOLD；
4. 已存在完整的稳健尺度参考窗口；
5. 连续 `occlusion_trigger_consecutive_unreliable` 帧没有可接受的 `RELIABLE` 结果；
6. 当前选中观测为 `WEAK` 或 `REJECTED`；
7. `min_dimension_ratio <= occlusion_trigger_min_dimension_ratio`。

仅出现快速位移但尺度正常时不会触发 HOLD；没有观测框时也不凭空判定尺度收缩，而是
沿用普通恢复流程。`ASSISTED_RECOVERY` 和 `LOST` 禁止首次进入 HOLD，避免已经启用
detector 后再次暂停。同一次 loss episode 只允许触发一次，重捕成功或人工重新框选后
才重新武装。进入 HOLD 后清空普通重捕确认历史和未锁定 detector 候选，避免等待结束后
消费过时假设。

无观测帧计入连续非可靠帧数，但由于没有尺度比例，该帧本身不能触发 HOLD。重捕成功后
清空旧参考窗口并用已接受的可靠重捕框开始新窗口；人工重新框选则清空窗口但不把人工框
冒充模型可靠观测。

触发发生在 `finish_frame()`，因此触发帧已经执行 NanoTrack 推理。若触发帧为 `f`、
配置 `occlusion_hold_frames=N`，则：

```text
f       : 完成 infer，根据结果进入 HOLD，并设置 remaining=N
f+1..f+N: 共 N 个完整 HOLD 帧，不运行 tracker/detector
f+N+1   : 恢复正常恢复阶段
```

最后一个 HOLD 帧即使处理后 remaining 递减为零，该帧输出状态仍为
`OCCLUSION_HOLD`。暂停交互播放不处理新视频帧，因此不消耗 remaining。

### 3.3 HOLD 期间行为

- `mode=OCCLUSION_HOLD`；
- `control_valid=false`、`accepted=false`；
- 不调用 `ShortTermTracker::infer()`，因此没有新的 anchor/recent 观测；
- 不提交 detector 请求；
- 如果存在进入 HOLD 前已经在途的 detector 请求，允许 runtime 将其 poll/drain，
  仅用于 Debug 显示和日志，不允许入队或改变控制状态；该请求在进入 HOLD 时即永久
  失去控制资格，即使 batch 到 HOLD 结束后才返回也不能重新变为可入队结果；
- 运动模型继续预测中心，但将宽高固定为进入 HOLD 时的稳健参考宽高；
- 进入 HOLD 时清零 `velocity_width` 和 `velocity_height`；退出 HOLD 后直到真正重捕前
  仍保持尺度冻结，避免普通恢复重新沿用旧的负尺度速度；
- 每处理一帧递减 `hold_remaining_frames`；手工重新框选会立即清除 HOLD 并重新初始化。

### 3.4 HOLD 结束后的恢复

HOLD 结束后先依据累计损失时间进入现有恢复阶段。只要尚未重捕，所有 motion 路径的
搜索参考框继续使用冻结尺度：

- `LOCAL_RECOVERY`：恢复 NanoTrack 推理，使用现有扩大搜索区域；
- `ASSISTED_RECOVERY/LOST`：允许 detector 请求，候选仍先经过位置硬门；
- 非 detector motion 路径保持现有规则：anchor-only 可靠结果可以进入普通 2/3
  确认；双模板均可靠但不一致时继续强制确认，不在本轮收紧为必须双模板一致；
- detector 路径继续要求 anchor/recent 双模板可靠且一致，并遵守 detector 几何门和
  同候选 2/3 确认；
- 只有重捕成功时，才用接受框重新初始化运动模型并解除尺度冻结。

本轮不增加 HOLD 提前退出探测，也不允许 `WEAK` 观测更新 motion、tracker state 或模板。

## 4. 运行时接口调整

在 `SearchPlan` 增加明确的执行许可，例如：

```text
run_tracker
request_detector
hold_remaining_frames
```

`LongTermRuntime::process_frame()` 仅在 `run_tracker=true` 时调用
`tracker_.infer()`。HOLD 帧向 `finish_frame()` 传入空观测集合。该设计比在具体
NanoTrack 实现内部静默跳过更清晰，也能由 mock tracker 单元测试验证调用次数。

控制器为每次 detector 请求分配内部 eligibility generation。进入 HOLD 时使此前
generation 永久失效。runtime 仍 poll/drain 对应 batch，并将候选交给诊断通道，但
控制 ingest 必须以 generation/请求身份判定不可入队，不能只依赖“batch 返回时是否仍
处于 HOLD”。

`MotionEstimator` 增加尺度冻结能力：

- 设置冻结宽高并清零宽高速度；
- 冻结时 `predict()` 只更新中心；
- 重捕后的 `initialize()` 清除冻结状态；
- 不改变中心速度的现有 alpha-beta 逻辑，但继续受 `motion_max_dt_ms` 限制。

## 5. Detector 全候选 Debug 可视化

### 5.1 “全部结果”的边界

这里的“全部检测结果”指：当前配置类别经过置信度阈值过滤和 NMS 后，由 detector
adapter 返回的全部 `DetectorCandidate`。不绘制 YOLO 输出网格中的阈值前原始预测，
也不显示被 NMS 抑制的框。

Detector batch 是异步结果，框属于 `source_frame_id`。结果在收到 batch 的当前画面上
绘制，并在标签中显示 `src` 和 `age_ms`，避免将旧帧框误认为当前帧精确位置。本轮不
跨多帧缓存 detector 框；逐帧 Debug 时可停在 batch 到达帧查看，日志保留全部字段。

### 5.2 每个候选的状态

控制器为 adapter 返回的每个候选生成 Debug 诊断项。诊断生成独立于控制入队流程，
不能因为 batch stale、frame mismatch、HOLD 作废或单个候选非法而提前丢失整批诊断：

```text
bbox
class_id
confidence
center_ratio
position_gate_pass
queued
candidate_id（仅入队后存在）
batch_eligible
rejection_reason
```

交互窗口显示：

| 颜色/标签 | 含义 |
|---|---|
| 紫色 `DET RAW PASS cls/conf/d` | 通过位置门的 detector 结果 |
| 灰色 `DET RAW REJECT_POS cls/conf/d` | 因位置跳变过大被拒绝 |
| 深红色文字 `DET RAW INVALID` | 框越界、非有限值或尺寸/置信度非法；不把非法坐标送给 OpenCV 绘框 |
| 橙色 `DET ACTIVE id/index` | 当前实际送入 NanoTrack 验证的候选/探针 |

所有 detector 原始框都只是 Debug 假设，不显示绿色控制含义。已有绿色
`ACCEPTED CTRL` 仍只来自 NanoTrack 验证及多帧确认通过后的控制输出。
`PASS` 只表示通过位置门，不等于已经入队；超过最终 top-N、batch 不可用于控制或
HOLD 前请求晚到时都必须显示 `queued=false` 和明确原因。候选数量较多时标签允许压缩
或分列，但不得静默遗漏诊断项；完整字段同时写入日志。

## 6. 日志与配置

控制器可见光和红外配置均增加四个严格解析字段：

```text
occlusion_hold_frames=12
occlusion_trigger_consecutive_unreliable=2
occlusion_trigger_min_dimension_ratio=0.70
occlusion_reference_window=8
```

建议约束：

- `occlusion_hold_frames`：`[1, 300]`；
- `occlusion_trigger_consecutive_unreliable`：`[1, 10]`；
- `occlusion_trigger_min_dimension_ratio`：`(0, 1]`；
- `occlusion_reference_window`：`[3, 60]`。

配置文件缺失这些新增字段时沿用结构默认值，保持旧配置可加载。偶数参考窗口的中位数
取中间两项平均；初始化框不冒充模型产生的可靠观测，人工重选时清空参考历史。

CSV schema 递增，新增：

```text
occlusion_hold_triggered
occlusion_hold_remaining_frames
occlusion_reference_width
occlusion_reference_height
occlusion_width_ratio
occlusion_height_ratio
tracker_skipped
detector_suppressed_by_hold
```

Debug 事件增加：

- `occlusion_hold_enter`：触发帧、连续非可靠计数、参考尺寸、观测尺寸和两个比例；
- `occlusion_hold_tick`：剩余帧数、motion 框、tracker/detector 跳过标记；
- `occlusion_hold_exit`：累计 loss 时长及下一状态；
- `detector_candidate`：每个 detector 结果的 bbox、类别、置信度、距离及筛选结论。

`RK3566_NANOTRACK_LONGTERM_CODE_FLOW.zh-CN.md` 同步说明新增状态、字段来源和排障
顺序。

## 7. 测试策略

### 7.1 主机单元测试

- 配置默认值、解析、缺失字段兼容策略及范围错误；
- 连续 1 帧非可靠不触发，连续 2 帧且单尺度比例等于 `0.70` 触发；
- 比例刚大于阈值不触发；快速位移但尺度正常不触发；无观测不触发收缩 HOLD；
- 参考尺度采用最近可靠框的中位数，不被单帧小框拖小；
- HOLD 精确持续配置帧数，期间 tracker infer 和 detector submit 均为零；
- 触发帧仍有一次 infer，`N=1`、`N=12`、帧号跳跃和暂停场景均验证无 off-by-one；
- 同一次 loss episode 只能进入一次 HOLD，重捕或人工重选后重新武装；
- `ASSISTED_RECOVERY`、`LOST` 不允许进入 HOLD；
- 在途 detector 只 drain 和显示，不入队、不产生 `control_valid`；
- HOLD 前 batch 在 HOLD 内或退出后才返回时都永久不可入队；
- HOLD 及后续恢复阶段宽高保持冻结，中心仍运动；
- 重捕成功后解除冻结并重新初始化 motion；
- 手工重新框选清除 HOLD；
- detector batch 中所有 post-NMS 候选均产生 Debug descriptor，PASS/REJECT/INVALID
  标记与控制器判断一致；
- 安全回归：任何 detector 原始框、HOLD motion 框和 `WEAK` 框都不能直接产生
  `control_valid=true`。

### 7.2 开发板简单验证

- 原生 clean build 和完整可执行程序链接；
- 运行完整 CTest；用户本地配置覆盖导致的既有 `cli` 差异继续单独报告；
- 使用 `girl_dance.mp4` 逐帧检查一次渐进遮挡：确认触发帧仍有 anchor/recent，
  随后 N 个完整 HOLD 帧中 anchor/recent 框消失、motion 框尺度固定、剩余帧递减，
  HOLD 结束后才恢复 tracker；
- 在 assisted recovery 的 detector batch 到达帧确认全部 post-NMS 候选均可见，
  并且位置拒绝框与活动候选颜色/标签不同；
- 保留日志、截图、源 commit、板端 commit、二进制/模型/输入 SHA-256 和精确命令。

这仍属于 Debug 功能和短程流程验证，不替代有标注遮挡数据上的重捕率、误重捕率、
恢复时延、资源稳定性或真实吊舱验收。

## 8. 明确不做的内容

- 不训练或替换 NanoTrack 模型；
- 不修改 detector 权重、类别阈值或 NMS 参数；
- 不引入 ReID、光流或分割模型；
- 不允许 detector 直接产生控制输出；
- 不在 HOLD 中周期探测或提前退出；
- 不把无标注 `girl_dance` 观察升级为定量重捕性能结论；
- 不更新协调者拥有的 `STATUS.md` 或 `integration-contract.yaml`。

## 9. 交付与协调边界

实现继续位于独立 Debug 分支。形成实质进展时，代码、测试、文档及
`docs/progress/board-validation.md` 证据同提交记录。模型、视频、日志、截图和构建
产物不进入 Git，只记录绝对路径、SHA-256、命令和观察结果。

在板端验证和协调者审查前，任务状态继续为 `functional_pass_promotion_held`，
`development_board_validation.latest_handoff.available=false`。交叉编译和真实吊舱测试
不属于本设计的完成条件。
