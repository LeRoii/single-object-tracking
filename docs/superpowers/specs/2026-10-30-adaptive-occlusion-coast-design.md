# RK3566 NanoTrack 自适应遮挡滑行设计

## 1. 背景与问题

当前长时控制器把遮挡处理实现为一次性的 `OCCLUSION_HOLD`：只有 NanoTrack
观测已经变为 `WEAK/REJECTED`，并且框相对最近可靠尺寸缩小到阈值以下时，才冻结
尺度并连续跳过若干帧的 NanoTrack 与 detector。默认参数为 12 个 HOLD 帧、连续
2 个不可靠观测、最小单边尺寸比例 0.70、8 帧可靠尺寸参考。

实际板端测试暴露出三类问题：

1. 部分遮挡后 NanoTrack 仍可能输出高响应和 `RELIABLE`，控制器继续接受逐渐缩小
   的局部框，约 8 帧后才进入 `UNCERTAIN`；遮挡触发过晚。
2. 逐渐缩小但仍被接受的框会进入 8 帧可靠尺寸历史，使参考尺寸也随错误框缩小，
   进一步削弱尺寸比例触发。
3. 固定 HOLD 期间完全不运行 NanoTrack。若目标在 12 帧结束前重新出现，控制器无法
   提前发现，导致重新出现时仍停留在 `UNCERTAIN/HOLD`。

测试观察同时表明，目标完全遮挡期间 motion prediction 比继续接受 NanoTrack
局部框更接近真实运动。因此本设计把 HOLD 改造成 motion 主导的遮挡滑行阶段，并
通过低频 NanoTrack probe 尽早发现目标重现。

## 2. 目标

- 将遮挡识别从“必须先变成 WEAK”中解耦，使明显遮挡可从 `TRACKING` 直接进入
  `OCCLUSION_HOLD`。
- 对渐进遮挡争取在 2～3 帧内识别，而不是等待约 8 帧质量下降。
- HOLD 最多仍持续 12 帧；期间 motion prediction 每帧输出并可驱动吊舱控制。
- HOLD 搜索阶段每 5 帧运行一次 NanoTrack probe；首次可靠命中后，下一帧立即进行
  确认 probe，成功时提前退出 HOLD。
- `UNCERTAIN` 保持每帧运行 NanoTrack，以快速处理相机抖动、模糊和偶发低质量帧。
- detector 不参与 `UNCERTAIN` 和 HOLD 的短遮挡恢复；只在后续
  `ASSISTED_RECOVERY/LOST` 提供候选。
- detector 框仍不能直接产生 `control_valid=true`。
- 对遮挡触发、motion 控制年龄、probe 调度和恢复判定提供可复现日志。

## 3. 非目标

- 本轮不训练遮挡分类网络、可见度网络或新 detector。
- 本轮不标定最终阈值；首版阈值是可配置的 Debug 初值，仍需有真值视频校准。
- 本轮不改变 detector 候选的类别、位置、NanoTrack 验证和同候选确认安全链。
- 本轮不宣称无真值视频可以证明重捕率、身份保持率或吊舱验收通过。
- 本轮不允许 motion coast 超过 12 帧继续授权控制。

## 4. 考虑过的方案

### 4.1 仅调整原有阈值

把连续不可靠帧从 2 改为 1、把尺寸比例 0.70 提高并缩短 HOLD。改动最小，但仍需
等待 NanoTrack 先降为 `WEAK/REJECTED`，不能解决高响应局部框和参考尺寸污染，且
误触发会明显增加。因此不采用。

### 4.2 motion 主导的自适应遮挡滑行（采用）

保留现有状态机骨架和 12 帧上限，增加独立遮挡证据；首次出现嫌疑即停止更新稳定
尺度参考。严重尺寸突变直接进入 HOLD，渐进缩小需要一个辅助证据。HOLD 每帧使用
motion，按 5 帧间隔 probe，并在首次命中后进入逐帧确认。该方案可解释、便于记录
和板端调参，且无需增加模型。

### 4.3 独立遮挡/可见度网络

理论上可以直接估计可见比例，但需要新标注、训练、RKNN 部署和额外算力。目前证据
不足以证明其收益超过规则方案，暂不采用。

## 5. 命名与兼容策略

内部继续保留 `TrackingMode::kOcclusionHold` 和日志字符串
`OCCLUSION_HOLD`，避免无必要地破坏既有 CSV、metadata、Debug 工具和测试。
其语义从“完全停止观察”升级为“最多 12 帧、motion 主导、低频 probe 的遮挡滑行”。

界面显示可改为 `OCCLUSION HOLD/COAST`，文档中简称 COAST。模式与
`hypothesis_source=MOTION` 共同表示控制来自 motion coast，不新增可被误解为真实
观测的 detector 控制来源。

日志 schema 需要升级，以增加 probe 和控制年龄字段；旧字段保留原有含义，版本号按
仓库现有 schema 规则递增。

## 6. 状态与转移

```text
TRACKING
  ├─ 普通质量下降 ────────────────> UNCERTAIN
  │                                  ├─ 可靠恢复/确认 ──> TRACKING
  │                                  └─ 遮挡证据成立 ──> OCCLUSION_HOLD
  └─ 严重或渐进遮挡证据成立 ──────> OCCLUSION_HOLD

OCCLUSION_HOLD（motion coast，最多 12 帧）
  ├─ 搜索 probe 每 5 帧一次
  ├─ 首次可靠命中 ──> 下一帧确认 probe
  ├─ 确认成功 ──────> TRACKING
  └─ 12 帧结束未恢复 -> 按 loss elapsed 进入 LOCAL_RECOVERY 或更晚状态
```

### 6.1 `UNCERTAIN`

- 每帧运行 NanoTrack，不运行 detector。
- `WEAK/REJECTED` 观测不修正 motion，也不更新稳定尺度历史。
- `RELIABLE` 且没有遮挡嫌疑时沿用现有确认规则。
- 遮挡证据成立时可以直接进入 HOLD，不必等 `uncertain_ms` 结束。

### 6.2 `OCCLUSION_HOLD`

- 触发帧仍执行一次完整 NanoTrack 推理，但一旦遮挡条件成立，该异常观测不得修正
  motion、提交 tracker bbox、更新模板或更新尺度历史；当帧最终输出改用冻结尺度的
  motion prediction。
- 触发帧就是 coast age 1。HOLD 总共最多 12 帧，而不是“触发帧 + 后续 12 帧”；
  因此不会产生第 13 个 motion 控制有效帧。
- 每帧计算 motion prediction，宽高固定为进入 HOLD 时冻结的稳定参考尺寸。
- HOLD 第 5、10 帧运行搜索 probe，其余帧不运行 NanoTrack。
- 某个搜索 probe 首次满足“恢复候选”后，下一帧无条件运行确认 probe，不等待下一个
  5 帧周期。
- 确认成功时立即用确认框重新初始化 motion，清理旧确认和 detector 候选，退出 HOLD。
- 确认失败时回到搜索阶段；下一次周期 probe 按原始 HOLD 帧号继续，例如第 6 帧确认
  失败后，下次搜索 probe 仍是第 10 帧。
- 第 12 帧结束仍未恢复时，motion coast 控制授权终止；下一帧恢复时间驱动的恢复模式。
- `loss_start_ms` 在触发帧建立。第 12 帧结束后的下一帧按实际 elapsed 进入
  `UNCERTAIN`、`LOCAL_RECOVERY` 或更晚状态；默认帧率下通常进入
  `LOCAL_RECOVERY`，但不以帧率假设覆盖时间戳计算。

## 7. 遮挡证据

### 7.1 稳定参考

控制器保留稳定尺寸参考和最近尺寸趋势。只有同时满足以下条件的已接受观测才可更新
参考历史：

- 质量为 `RELIABLE`；
- anchor/recent 不存在明显分歧；
- 相对 motion prediction 的几何残差在正常范围；
- 当前帧没有任何遮挡嫌疑。

首次出现遮挡嫌疑时立即冻结参考，后续观测不得把小框写回参考历史。显式重新框选或
确认成功的重捕会清空旧参考并从新框重新积累。

“首次遮挡嫌疑”定义为：任一尺寸相对上一稳定观测出现超过噪声容差的缩小，或者已
达到严重比例阈值。单独的 peak-margin 波动、模板分歧或 motion 残差不冻结尺寸参考，
避免没有尺度异常时永久停止更新。

嫌疑出现后进入内部 suspicion 计数阶段；它不新增公开 TrackingMode。若后续连续
2 帧尺寸回到噪声容差内、未满足严重/渐进触发且没有辅助证据，则解除嫌疑，从这些
正常帧重新继续参考更新。嫌疑期间的框先缓存在诊断历史中，不写入稳定参考。

在参考窗口尚未填满时，不允许以不完整历史触发“渐进遮挡”。严重突变至少需要 3 个
稳定样本；使用已有 3～7 个稳定样本的宽高独立中位数作为临时参考，并在日志标记
`reference_source=PARTIAL_STABLE_MEDIAN`。不足 3 个稳定样本时不触发 HOLD，继续走
普通 `UNCERTAIN`。达到完整窗口后标记 `reference_source=FULL_STABLE_MEDIAN`。

### 7.2 严重突变路径

若任一尺寸相对冻结稳定参考的比例小于等于严重阈值，直接触发 HOLD，不要求当前
NanoTrack 先变为 `WEAK/REJECTED`。

首版沿用 `0.70` 作为严重阈值。该路径仍需合法 bbox、至少 3 个稳定参考样本和非
detector hypothesis，避免 detector 候选或无效框触发 motion 控制。

### 7.3 渐进遮挡路径

连续 2 帧在同一尺寸方向缩小，并且该尺寸相对稳定参考的累计比例小于等于渐进阈值
（Debug 初值建议 `0.85`）时，若同时存在以下任一辅助证据，则触发 HOLD：

- anchor/recent 模板框分歧；
- `peak_margin` 相对稳定基线明显下降或降到配置阈值；
- NanoTrack 观测中心与 motion prediction 的归一化残差超过配置阈值。

首版采用明确的布尔规则，不使用难解释的加权总分。每个原始证据、连续计数、阈值和
最终触发原因都必须写入 Debug 日志，以便后续用真值视频标定。

宽、高分别维护缩小计数；任一轴满足连续性和累计比例即可成为渐进主证据。“缩小”
不是严格浮点递减，而是当前尺寸除以上一稳定/嫌疑尺寸小于等于
`occlusion_shrink_step_ratio`，Debug 初值为 `0.97`，即单步缩小至少约 3%。反向变化
或落入噪声带会重置对应轴计数。

peak-margin 基线取稳定参考窗口内“最终选中 NanoTrack 观测”的 peak margin 中位数；
下降量使用绝对差，首版 Debug 初值建议 `0.02`。motion residual 使用当前选中框中心
相对 motion prediction 的现有归一化定义，首版使用独立可配置阈值，避免改变普通
NanoTrack hard gate。

正常远离造成的宽高同步缓慢变化，如果没有辅助证据，不触发渐进遮挡。目标形变造成
单边变化时同样需要辅助证据，降低把真实形变误判为遮挡的风险。

## 8. Probe 与恢复

### 8.1 搜索 probe

- 使用 motion prediction 作为 NanoTrack 搜索中心。
- 使用冻结尺寸作为目标参考，不使用遮挡期间缩小的框。
- 同时运行 anchor/recent 模板。
- probe 输出只参与恢复判断，未确认前不得修正 motion、更新模板或更新尺度历史。

### 8.2 首版恢复条件

先保留相对严格的条件进行真实视频测试：

- anchor/recent 均为 `RELIABLE`；
- 两模板框 IoU 达到现有 `template_agreement_iou`；
- 选中框与 motion prediction 的中心和尺度变化通过几何门；
- 首次可靠搜索 probe 只建立候选；下一帧确认 probe 再次满足一致性后才恢复。
- confirm probe 还必须与首次 search-probe 候选执行跨帧 IoU、中心和尺度一致性检查；
  首版使用独立的 coast confirmation 配置，默认值可先对齐现有 detector confirmation
  几何门，但语义与 detector 候选完全分开。

恢复条件首版可以偏严格。日志必须分别给出质量、模板 IoU、motion 中心残差、尺度
变化和失败原因，后续只放宽被真实正例证明过严的条件，不一次性降低全部安全门。

## 9. Motion 控制授权

HOLD 的 12 个完整 coast 帧允许：

```text
mode=OCCLUSION_HOLD
hypothesis_source=MOTION
control_valid=true
bbox=motion_predicted_bbox（冻结宽高）
```

授权前提：

- HOLD 从已建立目标的正常 NanoTrack 跟踪路径进入；
- 进入时存在合法稳定尺寸参考和已初始化 motion；
- 不是 detector hypothesis、detector 候选或 UNKNOWN detector 类别直接触发；
- motion bbox 有限、尺寸为正、裁剪后仍合法；
- coast 年龄不超过 12 帧。

控制安全规则：

- `control_valid=true` 只表示允许使用有限时长的 motion coast，不表示有当前帧视觉
  观测；日志必须同时记录 mode/source/age。
- probe 未确认前不能以其框替换控制框。
- 第 12 帧结束后必须撤销 motion 控制授权；后续恢复状态保持
  `control_valid=false`，直到 NanoTrack 验证通过。
- detector 框在任何状态下都不能直接产生 `control_valid=true`。
- 显式重新框选、输入重置、非法 motion 或运行异常立即终止 coast 授权。

## 10. Detector 策略

- `TRACKING`、`UNCERTAIN` 和 `OCCLUSION_HOLD` 不提交恢复 detector 请求。
- 运行时可以继续非阻塞 poll 已提交工作，但 HOLD 入口会提升 eligibility generation；
  旧 batch 仅作诊断，不得入队或授权控制。
- 12 帧后若进入 `LOCAL_RECOVERY`，仍以 NanoTrack/motion 搜索为主。
- 只有到现有 `ASSISTED_RECOVERY/LOST` 时间门后才按 `detector_interval_ms` 请求检测。
- 目标类别 UNKNOWN 时继续禁用 detector 辅助恢复。

## 11. 配置建议

保留：

```ini
occlusion_hold_frames=12
occlusion_reference_window=8
occlusion_trigger_min_dimension_ratio=0.70
```

新增或明确：

```ini
occlusion_probe_interval_frames=5
occlusion_gradual_dimension_ratio=0.85
occlusion_gradual_consecutive_frames=2
occlusion_min_reference_samples=3
occlusion_shrink_step_ratio=0.97
occlusion_suspicion_clear_frames=2
occlusion_peak_margin_drop=0.02
occlusion_motion_residual_threshold=0.75
occlusion_confirmation_iou=0.50
occlusion_confirmation_center_distance=0.35
occlusion_confirmation_max_area_log_change=0.50
occlusion_confirmation_max_aspect_log_change=0.40
```

原 `occlusion_trigger_consecutive_unreliable` 不再作为进入 HOLD 的必要条件。为兼容旧
配置，可先保留解析和 metadata，但标记 deprecated；是否在后续 schema 移除由协调者
决定。

所有新阈值继续声明 `calibrated=false`，不能因板端能够触发就改成已标定。

## 12. 可观测性

每帧 CSV、metadata 和 `DEBUG_EVENT` 至少记录：

- 遮挡参考来源、参考宽高；
- 当前宽高比例、逐帧缩小方向和连续计数；
- severe、template disagreement、peak-margin drop、motion residual 四类证据；
- HOLD 触发原因；
- coast 当前帧号、剩余帧数、`control_valid`；
- 当前帧是否运行 probe、probe 类型（search/confirm）；
- probe 各恢复门结果和失败原因；
- motion bbox、probe bbox 和最终控制 bbox 的来源；
- HOLD 提前恢复或正常超时退出事件。

Debug overlay 应区分 motion 控制框、probe 观测框和 detector 候选框，避免把 probe 或
detector 框误认为已授权控制。

## 13. 测试策略

### 13.1 主机单元测试

- `TRACKING` 下高质量但严重单边缩小可直接触发 HOLD。
- 渐进缩小只有在连续 2 帧且至少一个辅助证据成立时触发。
- 正常远离、单帧抖动、稳定尺寸快速平移、目标合理形变不误触发。
- 首次嫌疑后稳定参考停止更新，错误小框不能污染参考。
- `UNCERTAIN` 每帧 `run_tracker=true`，且不请求 detector。
- 触发帧计为 HOLD 第 1 帧；第 1～12 帧 motion 控制有效，第 13 帧撤销 coast 授权。
- 搜索 probe 只出现在第 5、10 帧；首次命中后的下一帧运行确认 probe。
- 确认成功提前退出；确认失败回到原周期，不修正 motion。
- HOLD 期间 detector 不提交，旧 batch 永久无资格入队。
- raw detector、未确认 probe、非法 motion 都不能直接授权控制。

### 13.2 Replay/集成测试

构造包含“正常跟踪→渐进遮挡→完全遮挡→第 7 帧重现”的确定性序列，验证第 5 帧
probe 未命中、重现后的第 10/11 帧完成搜索和确认；再构造第 4 帧重现，验证第 5/6
帧恢复。测试控制框来自 motion，probe 未确认前不影响控制框。

### 13.3 RK3566 测试

- 使用现有 `girl_dance` 和 `/home/cat/Project/SOT/data/tracking-test-v1.mp4` 做无真值
  功能观察；明确它们不能证明精度。
- 后续用带真值遮挡视频统计：遮挡开始到 HOLD 触发帧数、重现到恢复帧数、误触发率、
  motion coast 中心误差、身份切换和控制连续性。
- 记录源码提交、二进制/模型/配置/视频 SHA-256、命令、日志和输出路径。
- 板端功能观察不能替代生产交叉编译或真实吊舱人工验收。

## 14. 验收边界

代码层通过要求包括状态机、probe 调度、12 帧授权上限、detector 禁止直接授权和日志
测试全部通过。板端功能层要求至少观察到一次 HOLD 触发、motion 控制、周期 probe 和
提前恢复或明确记录未观察。

在没有真值标注前，只能声明功能路径可运行，不能声明遮挡检测及时性、重捕准确率、
阈值、长时稳定性或吊舱控制效果验收通过。公共
`development_board_validation.latest_handoff.available` 仍由协调者根据完整门槛决定。
