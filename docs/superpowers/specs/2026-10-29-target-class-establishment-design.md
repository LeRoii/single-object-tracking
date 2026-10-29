# RK3566 初始化目标类别建立设计

## 1. 背景与目的

当前交互式 Debug 程序在启动时通过 `detector_class_id` 固定 detector 类别。现有配置为 `0`，对应 VisDrone9 的 `person`。因此即使人工框选的是车辆或车辆局部区域，辅助恢复阶段仍只会提供行人候选，类别语义与初始化目标不一致。

本改动在每次人工初始化或重新框选时增加“目标类别建立”过程：在同一初始化帧上执行一次阻塞式全类别检测，通过初始化 ROI 与 detector 候选的几何关系确定目标类别。若不能得到唯一可信类别，则类别保持 `UNKNOWN`，禁用 detector 辅助恢复，但 NanoTrack、运动预测和交互调试仍可正常工作。

## 2. 目标与非目标

### 2.1 目标

- 初始化框可以是完整目标框，也可以是 detector 目标的局部区域或任意子目标。
- 用户确认 ROI 后，程序允许暂停等待一次 detector 推理完成。
- 每次重新框选均重新建立类别，并使旧 detector 请求、候选与确认状态失效。
- Debug 画面继续显示 detector 的全部检测结果；只有目标类别一致的候选可以进入恢复队列。
- 类别未知时 detector 可以继续运行和显示诊断结果，但任何 detector 候选均不得进入恢复验证。
- 保持安全约束：detector 框不能直接产生 `control_valid=true`；只有通过 NanoTrack anchor/recent 验证和既有确认规则的重捕获结果才能恢复控制输出。

### 2.2 非目标

- 不增加第二套 detector、ReID 网络或独立分类器。
- 不在跟踪过程中自动改变目标类别；类别只在人工初始化或重新框选时建立。
- 不把初始化阶段的 detector 框替换成人工 ROI；NanoTrack 仍以人工 ROI 初始化。
- 首版阈值只作为严格的工程默认值，标记为未标定，后续需用带真值数据重新标定。

## 3. 方案选择

采用“全类别检测 + 几何综合匹配”。对初始化帧运行一次全类别 detector，把 ROI 与每个候选的覆盖率、中心关系、IoU 和 detector 置信度组合评分，先在每个类别内选出最佳候选，再进行跨类别唯一性判断。

未采用以下方案：

- 只按 ROI 中心点选框：对局部子目标简单，但在重叠人车、车内人员或拥挤场景中易选错类别。
- 初始化后要求用户再点选类别：可靠但增加操作负担，也无法满足自动建立类别的目标。

## 4. 配置与兼容策略

保留现有 `detector_class_id` 配置项：

- `detector_class_id=-1`：自动建立类别，初始化时执行全类别检测。
- `detector_class_id>=0`：固定类别覆盖，跳过初始化检测，兼容明确知道类别的部署配置。

交互式 Debug 默认配置由 `0` 改为 `-1`。底层 YOLO 引擎始终以全类别模式运行，使全部结果都能用于显示和诊断；固定或自动得到的目标类别只在控制器候选入队前过滤。

RKNN engine 始终使用 `class_id=-1`。应用启动时必须依据已校验 manifest/labels 的 `class_count` 单独校验 `detector_class_id>=0`，越界仍是启动错误，不能降级为 UNKNOWN。固定类别且 detector 可用时，状态为 `FIXED / FIXED_OVERRIDE`；未配置 detector 时可保留固定类别语义，但类别门控不允许 detector 恢复。元数据分别记录 `requested_target_class_mode/id` 与 `engine_class_filter=-1`，不再让原 `detector.class_id` 同时表示用户目标类别和引擎输出过滤两种含义。

新增严格默认参数：

- `target_class_min_roi_coverage=0.50`，合法范围 `(0, 1]`。
- `target_class_max_center_ratio=0.60`，合法范围 `(0, 2]`。
- `target_class_ambiguity_margin=0.10`，合法范围 `[0, 1]`。

这些参数的配置解析、范围检查、元数据记录和帮助说明必须同步完成。

## 5. 初始化 ROI 与候选匹配

对初始化 ROI `R` 和 detector 候选 `D` 计算：

- `roi_coverage = area(R ∩ D) / area(R)`，用于支持局部目标 ROI。
- `iou = area(R ∩ D) / area(R ∪ D)`。
- `center_ratio = distance(center(R), center(D)) / diagonal(D)`。
- `center_inside`：ROI 中心是否位于候选框内。

ROI 和候选框先裁剪到图像范围再计算面积。ROI 本身非法时拒绝初始化；候选非法时只产生诊断，不参与匹配。类别 ID 的合法范围来自已验证 labels/manifest 的 `class_count`。“全部检测框”特指通过置信度门限和 NMS 后的全类别候选，不包含阈值前的网络 raw proposals。

候选满足以下任一条件才有资格参与匹配：

```text
(center_inside && center_ratio <= target_class_max_center_ratio)
OR roi_coverage >= target_class_min_roi_coverage
```

合格候选的综合得分为：

```text
center_term = clamp(
    1 - center_ratio / target_class_max_center_ratio,
    0,
    1)

match_score =
    0.55 * roi_coverage +
    0.20 * center_term +
    0.10 * iou +
    0.15 * detector_confidence
```

唯一性判断规则：

1. 每个类别只保留 `match_score` 最高的候选，避免同类别 NMS 残留框制造伪歧义。得分相同时依次按 detector confidence、候选面积、原始候选索引稳定决胜。
2. 没有合格类别时，结果为 `UNKNOWN / NO_MATCH`。
3. 只有一个合格类别时，建立该类别。
4. 两个及以上类别时，按最佳得分排序；跨类别同分时按 class ID 稳定排序，但因分差为 0 仍判为歧义。若第一名与第二名属于不同类别且分差严格小于 `target_class_ambiguity_margin`，结果为 `UNKNOWN / AMBIGUOUS`。
5. 否则建立第一名类别。

覆盖率、中心距离比等于门限按通过处理；最佳与次佳分差恰好等于歧义门限时建立第一名类别。浮点比较使用统一 epsilon，并为这些边界建立测试。输入框面积非法、类别 ID 越界或置信度非有限值的候选不得参与匹配，但仍应保留可诊断的拒绝原因。

## 6. 数据模型与生命周期

增加运行期类别状态：

```text
TargetClassStatus = FIXED | ESTABLISHED | UNKNOWN
TargetClassReason =
    FIXED_OVERRIDE |
    UNIQUE_MATCH |
    NO_MATCH |
    AMBIGUOUS |
    DETECTOR_UNAVAILABLE |
    DETECTOR_ERROR
```

运行期保存：

- 可选的 `target_class_id`。
- `detector_recovery_enabled`。
- 初始化匹配结果：选中候选索引、框、置信度、覆盖率、中心距离比、IoU、综合得分。
- 初始化帧全部候选的匹配诊断。

这里的 `detector_recovery_enabled` 仅表示目标类别是否允许恢复，不表示 detector worker 当前是否健康或 ready；逐帧可用性仍由现有 adapter 状态表达。若实现时命名容易混淆，应使用更明确的 `target_class_allows_recovery`。

控制器每次初始化时必须原子地替换上述类别状态，并清除旧恢复候选和 2/3 确认状态，同时推进 `detector_eligibility_generation`。类别在下一次人工重新初始化前保持不变。

adapter `request_id` 与 controller generation 是两个独立概念：

- adapter `request_id` 由 adapter 单调递增，用于唯一识别 worker 请求；初始化和普通异步请求都不得复用。
- controller `detector_eligibility_generation` 只判断异步恢复结果是否属于当前跟踪会话或当前 HOLD 世代。

两者不得混用。controller 不负责清理 adapter 的在途请求；runtime/adapter 负责 worker quiesce 与旧结果隔离。即使在同一 `frame_id/timestamp` 上连续重新框选，每次也必须得到新 request ID 和新的 eligibility generation。

## 7. 阻塞式检测与重新框选

在 `DetectorAdapter` 增加仅供初始化使用的阻塞检测接口。`detect_blocking()` 与 `submit()/poll()` 共用同一 adapter 状态机，但初始化请求具有独立的 `request_kind=INITIALIZATION` 和 adapter 内部单调递增的唯一 `request_id`。`YoloDetectorAdapter` 复用现有模型和工作线程，不加载第二份 RKNN 模型：

1. 阻塞调用取得 adapter 初始化独占权，取消尚未被 worker 取走的旧 `pending_`。
2. 若旧请求已经 `in_flight_`，等待其结束并丢弃其结果；随后清除旧 `completed_`。
3. 提交初始化请求，只等待且只消费相同 `request_id`、`request_kind` 的完成结果。
4. 普通 `submit()/poll()` 在阻塞调用期间不得消费初始化结果；首版虽然由主线程串行调用，adapter 仍必须靠请求类型和 ID 隔离结果所有权。
5. worker 在 `pending_→in_flight_`、推理成功、推理失败、停止和结果被消费后的每个状态转换均调用 `notify_all()`。
6. 条件变量等待期间释放互斥锁，`engine_->infer()` 期间不得持有 adapter 状态锁。

`NullDetectorAdapter` 返回 `DETECTOR_UNAVAILABLE`。固定类别模式不执行阻塞检测。如果旧在途请求或初始化请求发生 engine 异常，adapter 按现有策略转为 disabled，本次类别结果为 `UNKNOWN / DETECTOR_ERROR`，不得静默清除异常后继续使用状态未知的 RKNN engine。

人工初始化顺序：

1. 用户确认 ROI，视频保持暂停。
2. 清理或等待旧 detector 请求。
3. 自动模式执行一次全类别检测并建立类别；失败则设为 `UNKNOWN`。该初始化 batch 永远不得传入逐帧 `finish_frame()`。
4. 调用 `controller.initialize(ROI, TargetClassState)`，原子清理恢复状态并推进 eligibility generation；随后使用同一个原始人工 ROI 初始化 NanoTrack 和运动模型。
5. 显示类别结果并输出结构化日志。
6. 保持用户初始化前的暂停/连续播放状态，不因等待期间积压键盘事件而跳帧。

detector 推理失败不能阻止 NanoTrack 初始化；此时使用 `UNKNOWN / DETECTOR_ERROR`。模型加载或清单校验失败仍按现有启动错误策略处理，不在此处降级掩盖。

`LongTermRuntime::initialize()` 只允许在没有 open frame 时调用。提交阻塞推理前可刷新一次 `ESTABLISHING TARGET CLASS...` 画面，但推理期间不读取或积压播放控制键，避免完成后跳帧。

## 8. 恢复候选过滤与安全顺序

detector 每批结果先形成完整诊断记录。每个候选同时保存两个正交字段：

```text
class_disposition =
    UNCHECKED | MATCH | MISMATCH | TARGET_UNKNOWN | INVALID_CLASS

final_rejection =
    NONE | INVALID | GENERATION_INVALIDATED | FRAME_MISMATCH | STALE |
    HOLD_INVALIDATED | TARGET_CLASS_UNKNOWN | CLASS_MISMATCH |
    POSITION_REJECTED | CAPACITY_REJECTED
```

最终拒绝优先级固定为：非法框/置信度/类别 ID → generation → frame identity → stale → 当前 HOLD 隔离 → target unknown/class mismatch → position gate → capacity。后续类别判断不得覆盖前面的结构性错误；同帧触发 HOLD 时允许把原本可入队候选改为 `HOLD_INVALIDATED`，但保留 `class_disposition`。

类别门控规则：

- 类别状态为 `UNKNOWN`：标记 `TARGET_CLASS_UNKNOWN`，不得入队。
- 候选类别与目标类别不同：标记 `CLASS_MISMATCH`，不得入队。
- 类别匹配：继续执行既有 batch/frame 有效性、陈旧 generation、HOLD、位置跳变、容量限制、NanoTrack anchor/recent 验证和 2/3 确认规则。

通过身份和时效检查的 batch 即使因 UNKNOWN 或全部类别不匹配而零入队，batch 状态仍为 `ACCEPTED`；候选级原因表达为何没有入队，避免把预期类别抑制误报成 detector worker 失败。

所有 detector 原始框继续显示。`detector_recovery_enabled=false` 只禁止候选入队、激活 probe 和参加 anchor/recent 验证，不停止 Debug detector 请求；在 `ASSISTED_RECOVERY/LOST` 中仍按既有节流规则运行检测并显示全类别后处理结果。类别未知时所有候选均标记 `TARGET_UNKNOWN`，`candidate_id` 为空且 `queued=false`。未来若生产配置要关闭纯诊断推理，必须使用独立开关。

只有通过类别过滤并进入验证流程的候选才能显示为“活动验证候选”；类别不匹配或类别未知下的候选采用灰色及明确标签。任何 detector 原始框、初始化匹配框或活动验证框都不能直接令 `control_valid=true`。

## 9. Debug 显示、日志与遥测

增加结构化事件：

- `target_class_init_start`：初始化帧、ROI、类别模式、请求 ID。
- `target_class_candidate`：每个候选的类别、框、置信度、覆盖率、中心距离比、IoU、综合得分、资格和拒绝原因。
- `target_class_init_result`：状态、类别 ID/名称、原因、request ID、初始化序号、候选数、最佳/次佳类别和得分、是否启用 detector 恢复、耗时。

画面状态栏显示：

- `TARGET CLS car(2) AUTO`
- 或 `TARGET CLS UNKNOWN - DET RECOVERY OFF`

初始化阶段被选中的 detector 候选使用独立颜色和标签；类别不匹配、类别未知或仅诊断候选使用灰色。原有 NanoTrack anchor/recent、motion 和 detector 框颜色约定保持可区分。

类别名称必须从已验证 labels 清单解析；解析失败时显示 `cls=<id>`，不得猜测名称。

调试 CSV schema 从 7 升级到 8，至少记录：

- `target_class_status`
- `target_class_id`
- `target_class_reason`
- `detector_recovery_enabled`

`TargetClassState` 必须进入初始 `RuntimeFrameResult`、每个后续 `ControllerOutput/TrackingLogRow` 和 overlay，不能只存在于 `main.cpp` 局部变量。初始化候选诊断保存在初始化结果中，供刚完成框选的静止画面绘制。schema 8 的初始行和普通跟踪行均填写目标类别四字段。

每次初始化和重新框选生成一条带 `initialization_index` 的结构化初始化记录；metadata 保存初始化历史数组，而非只保存最后一次结果。CSV 继续保持“一解码帧一行”，重新框选不为同一 frame ID 再写重复帧行，而由结构化事件和 metadata history 记录。初始化 detector 的 request/complete/error/耗时采用独立计数，不混入逐帧 tracker/runtime/pipeline 延迟。

运行元数据还需记录类别模式、三个匹配阈值、初始化结果和类别标签清单身份。同步更新 `test_tracking_log.cpp`、`test_verify_board_schema.sh`、元数据 JSON 测试以及代码流程说明文档中的字段来源和含义。

## 10. 测试策略

按 TDD 先写失败测试，再实现最小代码使其通过。

### 10.1 纯匹配器测试

- 完整目标 ROI 建立正确类别。
- 局部/子目标 ROI 通过高覆盖率或中心关系建立正确类别。
- 无交集结果为 `NO_MATCH`。
- ROI 位于候选边缘的门限行为。
- 同类别重复框不产生歧义。
- 不同类别清晰胜出。
- 不同类别分差小于门限时结果为 `AMBIGUOUS`。
- 非法框、非法类别、NaN 置信度被拒绝。
- 覆盖率、中心距离比和歧义分差恰好等于门限时的边界行为。

### 10.2 Adapter 与运行期测试

- 阻塞初始化检测只返回对应 request ID 的结果。
- 旧异步结果在重新初始化后不能进入新会话。
- 连续重新框选不会死锁或复用旧类别。
- detector 错误和 unavailable 均得到明确未知原因。
- 固定类别模式不触发初始化 detector 推理。
- 自动类别建立成功后只允许该类别进入候选恢复。
- 未知类别仍能初始化并运行 NanoTrack，但 detector 恢复关闭。
- 阻塞推理异常后 adapter 进入 disabled，不能继续提交普通恢复请求。

### 10.3 控制器与安全回归

- 目标类别候选可继续进入既有验证链。
- 类别不匹配和未知类别候选均不可入队，但全部保留诊断。
- detector 原始框在任何情况下都不能直接设置 `control_valid=true`。
- 对自动成功、UNKNOWN 和 fixed override 分别断言：`tracker.initialize()` 与 controller 初始 bbox 收到的始终是人工 ROI，而不是所选 detector 候选框；初始化候选没有恢复 candidate ID，不入队，也不调用 `commit_bbox()`。
- 仅接收和绘制 detector 候选不得改变 `accepted/control_valid`；只有后续以 detector probe 为假设、NanoTrack anchor/recent 可靠、几何一致并满足既有 2/3 确认时才允许恢复控制。UNKNOWN 和 CLASS_MISMATCH 路径永远不能产生 detector probe。
- HOLD、motion、detector generation、位置门控、anchor/recent 和 2/3 确认测试全部保留并通过。
- overlay、结构化日志、CSV schema 8 和元数据字段均有测试。

完成后运行全部 host CTest，并执行 `git diff --check`。Host 通过不等价于开发板验收。

## 11. RK3566 板端验证范围

在开发板完整原生环境中进行：

- 干净构建、全部 CTest、可执行文件及模型/清单/标签 SHA-256 记录。
- `girl_dance` 人物 ROI：自动建立为 person，跟踪和遮挡流程可运行。
- 背景或无匹配 ROI：类别为 UNKNOWN，画面明确显示 detector recovery off，NanoTrack 可继续运行。
- 若板端存在合法的车辆样本，分别用整车框和车辆局部框验证建立为 car；若没有样本，明确记录为未观测，不能声称通过。
- 同一进程中重新框选不同类别，确认旧类别、旧 batch 和旧确认状态不继承。
- 核查 detector 框不能直接产生有效控制输出。

模型、视频、日志、构建产物不提交 Git，仅在 `docs/progress/board-validation.md` 记录路径、哈希、复现命令、观察结果和证据边界。

## 12. 交付与协调边界

- 开发分支：`codex/debug-target-class-init`。
- 隔离工作树：`/home/u20/single-object-tracking/.worktrees/debug-target-class-init`。
- 基线：`93be0fb1a8af151b6c20ca9a318c3bee40a9ad99`。
- 只更新开发板任务拥有的 `docs/progress/board-validation.md`，不直接修改公共状态或集成契约。
- 在板端验证形成实质证据前，不设置 `latest_handoff.available=true`，不声称生产交叉编译或真实吊舱验收完成。
