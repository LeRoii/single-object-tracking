# RK3566 Debug 恢复框分色与 detector 位置硬门设计

## 目的

交互式 Debug 窗口必须明确区分运动预测、detector 原始候选、detector 探针、
NanoTrack anchor/recent 观测和最终接受框，避免把未确认假设误认为有效跟踪结果。
同时在 detector 候选进入控制器队列前增加严格位置硬门，现阶段优先防止框大范围
跳动，接受因此漏掉远距离重现目标的代价。

## 已选方案与备选

采用“相对运动预测中心的画面对角线归一化距离硬门”。候选中心与本帧 motion
预测中心的欧氏距离除以画面对角线，必须不大于
`detector_candidate_max_center_ratio`，默认 `0.15`。通过位置门的候选按中心距离
从近到远排序，距离相同再按 detector 置信度从高到低排序。

未采用的方案：

- 相对最后有效框硬门：更严格，但不利用已有运动趋势，快速运动时误拒更多。
- IoU 硬门：整人/整车 detector 框与任意局部初始化框不一定重叠，和现有子目标
  使用方式冲突。
- 仅按距离软排序：仍可能把远距离候选送入验证，不能满足当前“严格禁止”要求。

## detector 候选位置门

### 参考框与公式

`begin_frame()` 在 detector 覆盖搜索假设之前保存 motion 预测框：

```text
motion_predicted_bbox
```

候选位置距离定义为：

```text
candidate_center_ratio =
    distance(center(candidate), center(motion_predicted_bbox)) /
    hypot(frame_width, frame_height)
```

候选处理顺序：

1. 保留现有有限数值、画面边界、正尺寸和置信度合法性检查。
2. 计算 `candidate_center_ratio`。
3. 超过 `detector_candidate_max_center_ratio` 的候选直接拒绝，不进入候选队列。
4. 剩余候选按距离升序、置信度降序排序。
5. 最多保留 `detector_max_candidates` 个候选。

该规则在 `ASSISTED_RECOVERY` 和 `LOST` 中使用相同的严格阈值，不随丢失时间放宽。
它只约束中心位置，不以面积或 IoU 拒绝候选，以保留“局部初始化框—完整人/车框”
之间的尺度兼容性。既有 NanoTrack 双模板、几何门和 2/3 确认继续生效。

### 配置与状态

在 `ControllerConfig`、可见光/红外默认配置和严格配置解析中增加：

```text
detector_candidate_max_center_ratio=0.15
```

配置必须为有限数，范围为 `[0, 1]`。位置合法候选存在但全部被硬门拒绝时，批次
状态使用新增的 `POSITION_REJECTED`，不混同于框本身非法的 `INVALID`。

输出和日志增加：

- 本批次通过基础合法性检查的数量；
- 因位置硬门被拒绝的数量；
- 当前验证候选的 `candidate_center_ratio`；
- 配置阈值 `detector_candidate_max_center_ratio`。

CSV schema 随新增字段递增版本。detector 原始候选仍不能直接产生
`control_valid=true`。

## 交互 Debug 框与标签

只改变交互式 Debug 窗口，不改变非交互输出视频和生产 `ai_alg`。交互渲染直接消费
本帧 `RuntimeFrameResult`，按以下语义绘制：

| 颜色 | 标签 | 数据来源 |
|---|---|---|
| 青色，BGR `(255,255,0)` | `MOTION PRED` | `SearchPlan::motion_predicted_bbox` |
| 紫色，BGR `(255,0,255)` | `DET CAND id/conf/d` | detector 原始活动候选区域、ID、置信度和归一化距离 |
| 橙色，BGR `(0,165,255)` | `DET PROBE index` | detector 路径实际送给 NanoTrack 的 `predicted_bbox` |
| 蓝色，BGR `(255,0,0)` | `NT ANCHOR score` | anchor `TrackerObservation::bbox/raw_score` |
| 黄色，BGR `(0,255,255)` | `NT RECENT score` | recent `TrackerObservation::bbox/raw_score` |
| 绿色粗框，BGR `(0,255,0)` | `ACCEPTED CTRL` | 已接受且 `control_valid=true` 的 `ControllerOutput::bbox` |
| 红色状态标签，BGR `(0,0,255)` | `REJECTED NO_CTRL` | 本帧没有有效控制输出；不额外伪造一个结果框 |

绘制顺序从预测/候选到观测再到最终接受框，保证最终框位于最上层。`FULL_OBJECT`
下原始候选和 probe 坐标相同时保留两种标签，并通过线宽和标签位置区分。窗口底部
增加紧凑图例。窗口缩放发生在绘制之后，不改变算法坐标。

为避免把绘制规则埋在 `main.cpp`，新增不依赖 OpenCV 的纯数据层：它从
`RuntimeFrameResult` 生成待绘制框描述（种类、框、标签）。`main.cpp` 只负责颜色
映射和 OpenCV 绘制。这样主机测试可以验证每种框的数据来源和标签，不需要 GUI。

## 测试与板端验证

- 配置测试：默认值、文件解析、越界/非有限值拒绝。
- 控制器测试：近候选入队、远候选得到 `POSITION_REJECTED`、混合候选只保留近框、
  距离优先于置信度、严格门在 `LOST` 不放宽。
- 安全回归：远候选不能锁定、不能重捕、不能产生 `control_valid=true`；detector
  仍需双模板验证和 2/3 确认。
- Debug 描述测试：六类框/状态标签的数据源、坐标、ID、置信度、距离和绘制顺序。
- 主机运行完整 CTest；开发板原生构建、CTest，并用短视频有界运行观察分色标签和
  位置拒绝日志。不执行长时、遮挡重捕或真实吊舱验收。

## 明确边界

- 当前严格门会主动牺牲画面远端重现目标的找回能力，这是本轮确认的策略。
- 本轮不增加随时间扩张的门限，不做 ReID，不改变 detector 模型或阈值标定结论。
- 不允许 detector 框绕过 NanoTrack 验证和 2/3 确认。
- 模型、视频、日志和构建产物不提交 Git；板端证据只记录路径、哈希和实测结果。
