# NanoTrack recent 单活跃与 safe 快照恢复设计

## 意图

修正当前固定 `anchor` / 自适应 `recent` 双模板在稳定尺寸遮挡、过顶、大角度转弯和
同类车辆恢复中的结构性矛盾：固定初始化外观不能作为长期身份真值，detector 的同类别
候选也不能成为重捕获的必要条件。设计必须保持云台控制连续、限制模板污染，并避免引入
常驻多模板库和逐模板线性推理开销。

## 已确认的产品行为

- 正常跟踪只使用一个活跃 `recent` 模板。
- 保留一个休眠 `safe` 快照用于短时校验、回滚和搜索恢复，但不参与每帧正常推理。
- `safe` 不是永久初始化模板，而是上一代已确认的 `recent`。
- 进入 `SUSPECTED` 后先继续使用 NanoTrack recent 观测框对外输出和驱动云台。
- 搜索期 NanoTrack 可独立完成重捕获；detector 只是可选候选源，不能成为必要条件，
  也不能以类别正确替代目标身份确认。
- 使用时间和相对历史统计定义门槛，不针对本视频的 211、217、245 或 264 帧调参。

## 方案比较与选择

1. **固定 anchor + recent（现状，排除）**：具备独立参考和回滚保护，但初始化外观会
   随视角/形变过时；当前多条恢复路径把 anchor 可靠或双模板一致作为硬门。
2. **recent-only 且无备份（排除）**：推理最省、形变适应自然，但模板污染后会形成
   自我确认，无遮挡前快照可用于回滚或恢复。
3. **recent 单活跃 + 休眠 safe 快照（已选）**：正常阶段一次模板 head；只在模板
   probation、`SUSPECTED` 和 `SEARCHING` 中临时启用 safe，保持有界安全冗余。
4. **多视角模板库（暂不采用）**：覆盖形变更全面，但选择、淘汰和多模板推理复杂度
   超过当前问题所需。

## 模板生命周期

### 初始化

用户初始化时提取一份模板特征，同时赋给 `recent` 和 `safe`。两者内容相同期间只执行
一次 head，并复用结果。

### 正常 recent 更新

沿用“稳定后才更新”和 probation 思路，但不再用永久 anchor 做硬门：

1. 达到现有稳定时长/更新间隔后，从当前已接受观测框提取到独立 `staged` 槽；提取
   失败不得改变 recent 或 safe；
2. 当前 recent 和旧 safe 均保持不变，因此 probation 期间存储 staged、recent、safe
   三份特征，但每帧最多只执行 staged 与 recent 两次 head，safe 仍休眠；
3. probation 期间临时运行 staged recent 与当前 recent，检查连续帧质量、框时序一致性、
   运动/尺度连续性和是否存在遮挡怀疑；
4. probation 失败只丢弃 staged，recent 与 safe 不变；
5. probation 成功使用不抛异常的交换原子完成 `safe <- recent, recent <- staged`。

因此 safe 始终落后 recent 一个已确认外观阶段，能够随过顶和转弯逐步迁移，又不会在同一
帧同时被可疑观测污染。safe 与 recent 外观或响应不同只作为证据，不自动否决 recent。

### 冻结边界

`SUSPECTED`、`SEARCHING`、detector 候选验证和 recovery probation 禁止**普通自适应**
stage/commit/refresh。统一重捕获事务可以在提交帧建立 recovery staged，并在 recovery
probation 后受控 commit/rollback。异常、重置和帧中止必须丢弃未提交 staged，保留最后
一份已提交 recent 与 safe。

模板操作使用显式事务：stage 特征提取和所有验证在任何 controller 状态写入之前完成；
成功后 tracker 先以强异常保证应用不抛交换，controller 再提交同一 transaction id 的
状态。失败时两侧都 abort 同一 transaction，下一帧不得看见半提交 probation、模板或
候选。需要抽出可注入的 template/head 后端，使主机单测能验证 head 调用次数和每个异常点。

## 状态和数据流

### TRACKING

- 每帧只运行 recent；recent 可靠且未进入可疑状态时，观测框可直接成为 tracking/control
  框并更新运动模型。尺度保护以最后可信几何为滚动参考，复用每秒尺度变化率和防塌缩
  绝对边界；不得继续把所有 head 输出永久钳在初始化宽高的 `0.75..1.33`。
- safe 不推理；仅在 probation 中临时启用。
- 不因 safe 与 recent 外观差异把正常形变判为丢失。

### SUSPECTED

`SUSPECTED` 是显式状态，不等于真正丢失。稳定尺寸目标可由相对历史异常进入该状态，
不再要求宽高明显缩小。第一阶段只复用已有响应统计：PSR、APCE、peak margin 相对最近
稳定基线的持续退化，以及响应/运动一致性；不得使用 raw score 单项触发。

- 新增 tentative observation 语义：`tracking_bbox` 和 `rect[0]` 保持真实 recent 观测，
  `control_bbox` 是经过位移/速率包络约束的 recent 观测，`control_valid=true`；
- `accepted=false`、`trusted_for_state_update=false`。可疑观测不得写回模板、运动速度/
  加速度、响应基线或稳定计数，运动模型保留影子预测；`observation_present` 与
  `trusted_for_state_update` 分开，不能再用 `observation_valid == accepted` 表示两种含义；
- 控制授权时限从最后一帧可信观测开始计时，而不是到进入 SEARCHING 才开始；
- 临时运行 safe 产生诊断/确认观测，但 safe 不拥有强制否决权；
- 连续干净观测后回到 `TRACKING`；异常继续或 recent 质量崩溃则进入 `SEARCHING`。

`SUSPECTED` 的进入与退出使用毫秒持续时间和迟滞，具体默认值必须先由通用测试夹具与两
个 episode 的草案真值评估，而不是从单个失败帧反推。

相对响应基线使用最近 `occlusion_reference_window` 个可信 `TRACKING` 帧的逐指标中位数，
至少 `occlusion_min_reference_samples` 个样本后才生效；只允许可信普通跟踪帧更新，在
`SUSPECTED`、`SEARCHING`、detector 验证和 recovery probation 中冻结。恢复后先积累新的
最小样本窗口再启用相对门，避免可疑帧把基线自适应吞掉。PSR/APCE 使用相对历史下降，
peak margin 保留绝对下降证据；具体阈值必须由独立夹具和开发集标定。

### SEARCHING

- 对外继续输出有界运动预测框并保持云台搜索控制，只要运动预测仍在授权时限内；
- recent 和 safe 均可产生 NanoTrack 搜索候选；detector 可并行提供额外候选；
- 候选来源不决定控制授权，所有候选进入统一时序确认；
- NanoTrack 候选不要求 detector 同时命中；detector 候选必须由 NanoTrack验证，且
  detector 类别、置信度或单帧位置门永远不能直接授权控制。

## 统一重捕获确认

候选表容量有明确上限：recent 1 个、safe 1 个、detector 至多沿用
`detector_max_candidates` 个；几何重复候选先合并，溢出时按 recent、safe、detector 的
确定性优先级淘汰。candidate key 包含来源和稳定 id，gap、stale、异常、重置和 eligibility
generation 变化立即清理。每帧仍只选择一个搜索中心执行验证，其余候选保留有界年龄。
候选至少满足：

- NanoTrack 响应质量通过多指标门，不使用 raw score 单项；
- 在有界窗口内严格达到现有合同的 2-of-3 时序确认；
- 候选自身中心、尺度和宽高比跨帧连续；
- 与最后可靠轨迹的关系作为软/分级证据，不能只围绕可能已漂移的运动预测框设硬门；
- 有同类竞争且身份无法区分时保持搜索，不选择最高 detector 置信度目标；
- safe 命中可增强身份置信，但 safe 不命中不能单独否决经连续 recent 观测确认的合法
  视角变化。

recent/safe/detector 对同一位置形成来源冲突或出现多个同类竞争时保持 `SEARCHING`；不得
仅按 detector 置信度裁决。没有 ReID 时，若目标缺席且只有一个高度相似替代物，系统无法
证明物理身份，设计明确选择保守不恢复，而不声称解决不可观测身份问题。

NanoTrack-only、safe-assisted 和 detector-seeded 三条路径最终使用同一重捕获提交与
probation 逻辑。提交帧 stages 新 recent，后续普通跟踪帧确认后才完成模板迁移；失败则
回滚到提交前 recent/safe 并恢复搜索。

## 形变与遮挡的区分

- **形变/视角变化**：recent 质量和位置连续，外观变化可逐帧解释，运动/尺度平滑；即使
  safe 响应下降，也继续跟踪并允许 probation 后更新 recent。
- **遮挡怀疑**：响应统计相对历史突变或持续退化，观测与影子运动不一致，无法形成稳定
  的新外观序列；进入 `SUSPECTED` 并冻结学习。
- **真正搜索**：recent 质量崩溃或可疑证据持续超过时限；进入 `SEARCHING`，但对算法
  和宿主仍是搜索状态，不等同永久丢失。

## 配置与兼容性

- 用户已选择 fail-fast 迁移。新版配置必须显式声明
  `template_policy=recent_safe_v1` 和 `recovery_policy=unified_candidates_v1`；缺失或值不匹配
  时拒绝启动，不能使用隐式默认值进入新安全语义。
- 废弃“detector 必须确认 NanoTrack 搜索命中”的行为；新版 parser 遇到旧键
  `search_reacquisition_requires_detector` 必须报出明确迁移错误。
- `template_bridge_*` 的旧固定 anchor 特例由统一 probation/恢复路径取代；新版 parser
  遇到 `template_bridge_min_stable_frames` 或 `template_bridge_max_duration_ms` 同样 fail-fast。
- 新增独立配置 `visible-recent-safe-v1.conf`，不原地改写或重新解释历史配置；旧配置只与
  对应旧二进制共同保留为历史复现输入。
- telemetry/schema 版本递增。保留历史字段读取兼容时，也必须新增明确的
  `active_template=RECENT`、`safe_template_used`、`candidate_source` 和状态转换原因。

## 必须补充的观测字段

- recent/safe 原始 bbox 和完整响应指标；
- 稳定响应基线及各指标相对变化；
- recent/safe 是否实际执行 head、是否复用结果；
- staged、commit、rollback、safe refresh 的帧和原因；
- `SUSPECTED` 进入、持续、清除或升级原因；
- 每个恢复候选的来源、candidate id、连续命中、几何/身份争议和最终拒绝原因；
- tracking、display、control 三种框继续分离，`rect[0]` 保持真实观测或搜索预测框语义。

## 验证策略

### 单元和合成序列

- 正常跟踪及 recent==safe 时每帧只运行一次 head；
- recent 更新 probation 临时双 head，成功后旧 recent 成为 safe，失败则完整回滚；
- 渐进过顶/转弯导致 safe 变弱时，连续可靠 recent 不被否决；
- 稳定尺寸且 raw score 高、但多项相对响应持续退化时进入 `SUSPECTED`；
- `SUSPECTED` 继续输出 recent 控制框，同时冻结模板和运动观测更新；
- NanoTrack-only 候选无需 detector，满足统一时序门后恢复；
- detector-only、单帧同类候选和身份争议候选不得授权控制；
- abort、异常、重置和 stale/gap 路径均不泄漏 staged 模板或旧候选状态。

### 真实数据

- 现有 `car_001` canonical episode 必须保持既有可见区间精度、控制连续性和遮挡恢复
  安全门，不得只验证本次第二目标。
- `car_002` 271 帧草案只能用于开发诊断；人工确认前不报告正式 IoU 或阈值标定结论。
- 至少增加过顶、大角度转弯、同类交叉三个独立片段后才能评价泛化；本视频通过不能证明
  全场景晋级。
- RK3588 验证分别报告每帧 head 调用次数、tracker/runtime/pipeline 延迟和内存，不用
  构建成功替代板端行为证据。

## 非目标

- 不实现多模板库或专用 ReID 网络；
- 不让 detector 直接控制云台；
- 不用第二目标草案反向调出帧号专用参数；
- 不在本设计中修改公共数据契约、生产交叉编译或真实吊舱验收结论。
