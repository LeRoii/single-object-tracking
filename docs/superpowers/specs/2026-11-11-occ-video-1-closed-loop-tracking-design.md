# `occ_video_1` 真值闭环跟踪设计

## 目的

以 `data/occ_video_1` 为唯一的本轮功能验收基准，联合调整可见目标跟踪、遮挡判断、
运动预测、搜索控制、detector 调度、NanoTrack 重捕验证和模板生命周期，使白色轿车在
完整 271 帧中保持正确身份和连续轨迹。完全遮挡不是永久丢失，而是仍可驱动云台的
`SEARCHING` 阶段；目标重新出现后必须在第 190 帧之前正确重捕。

本轮允许为了该视频调整算法流程和阈值，但禁止读取当前帧号后输出预存框、使用真值框作为
运行时输入，或把 detector 候选直接当作已确认目标。最终程序必须只依赖视频、模型、配置和
自身历史状态。

实现落在共享 runtime/core，避免维护 RK3588 专用算法分叉；但新行为和标定值先由独立的
`occ_video_1` Debug 配置启用，既有 RK3566 默认配置、production handoff 和宿主 ABI 不在本轮
自动切换。协调方审核通过前，本轮结果仍是独立 RK3588 Debug 候选。

## 已确认的产品语义

- 172--180 帧完全遮挡期间继续输出运动预测框，保持轨迹连续。
- 搜索期间仍需驱动云台；因此“没有当前视觉观测”不等于 `control_valid=false`。
- 宿主必须能区分视觉确认框和搜索预测框，不能把预测框标成已经重新看见目标。
- 目标 181 帧部分重现、182 帧首次可靠重现、184 帧稳定重现；允许验证身份，但必须在
  190 帧之前切回已确认跟踪。
- 相邻车道存在另一辆白车，重捕不能发生身份切换。

## 真值与评价边界

canonical 输入为 `/home/u20/single-object-tracking/data/occ_video_1/1.mp4`，SHA-256
`eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`，1920x1080、
30 FPS、271 帧。评价严格遵守数据契约：

- 0--168、182--270 帧是普通可见帧，可计算 IoU、中心误差和身份连续性；
- 169--181 帧为人工确认的双向插值估计，不属于可观测真值，禁止混入普通 IoU；
- 169--181 帧只用于评价预测轨迹的中心偏差、速度/加速度连续性和重现端点衔接；
- 172--180 帧的视觉真值语义是 `full_occlusion`，不得声称存在真实观测；
- 重捕延迟从 `first_reliable_reappearance_frame=182` 计算。

## 当前实板基线

2026-11-11 在 RK3588 `192.168.1.102` 上运行现有 `201991a` 部署链的完整 271 帧诊断，
证据目录为
`/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/runs/occ-baseline-full-20261111`。
主要制品 SHA-256：

- `bboxes.csv`: `b2ba9d7328f0554d1806a36f817ece8a42a5cdd0e4275b9cae44f1b87271eb6f`
- `output.avi`: `0002caa118473ee41e2c015e29728445f157ac2b2f70996d179670fe8c2906e4`
- `run.log`: `511e02d7e6aca3eac432ceb472bd54fb7c964ea1ef31d362b3fe632260c138cf`
- `session-metadata.json`: `08f34c2452ad379eed26cd2e14e7c8cfd58960bc2ad450ff2b74e65005c8f9a8`

基线在可评价 258 帧上的 mean IoU 为 `0.579`；遮挡后 182--270 为 `0.479`。
遮挡状态从未触发、detector 从未请求；174--178 和 180--184 的 `control_valid=0`；182 帧
IoU `0.303`，185 帧才恢复普通接受，且未报告 `reacquired`。这说明需要流程重构，不能只做
单一阈值搜索。该目录缺少 verifier 所需的 run manifest，因此它是算法诊断证据，不冒充正式
board-verifier PASS。

## 方案比较

### 方案 1：只搜索阈值

扫描质量、中心距离、遮挡和 detector 阈值，改动小，但无法修复当前“遮挡状态未进入、
detector 未调度、搜索控制失效”的流程缺口，不采用。

### 方案 2：真值驱动的闭环算法重构（已选）

先建立独立 evaluator，再以失败帧驱动状态机、运动预测、detector 和模板流程的迭代。算法
不读取真值，但每个候选版本都由相同真值协议评分。它能同时解决可见帧精度、遮挡轨迹、
持续云台控制和可靠重捕，采用此方案。

### 方案 3：优先重新训练模型

训练可能改善表征，但当前主要故障是控制状态和调度，训练周期长且不能保证修复流程问题。
仅当方案 2 在现有模型上达到流程正确但仍达不到框质量门时，才升级为后续阶段。

## 体系结构

### 1. 独立真值 evaluator

新增 `scripts/evaluate_occ_video_1.py`，输入 canonical 数据根和运行 CSV，输出版本化 JSON
与 Markdown 摘要，并以非零退出码表示硬门失败。evaluator 必须：

- 校验视频/metadata/events/status/labels 身份和帧号连续性；
- 将普通可见、估计遮挡、完全遮挡和重现窗口分层计算，禁止混算；
- 报告逐帧 IoU、中心误差、状态、框来源、控制来源、身份切换、搜索进入和重捕帧；
- 计算遮挡估计段的中心偏差及相邻帧速度/加速度突变；
- 输出最差帧清单，供下一轮算法改动定位；
- 不向 runtime 提供真值或最优参数，杜绝评测泄漏。

### 2. 观测状态与控制状态解耦

保留现有 `control_valid` 的宿主控制含义，但增加明确的观测/控制来源：

- 正常跟踪：`observation_valid=true`、`control_source=OBSERVATION`；
- 搜索预测：`observation_valid=false`、`control_source=MOTION_SEARCH`、
  `control_valid=true`；
- 无可用运动状态或异常：`observation_valid=false`、`control_valid=false`。

内部可继续兼容 `TrackingMode::kOcclusionHold`，但 CSV、metadata、Debug overlay 和事件必须
明确发布 `tracking_phase=SEARCHING`，不再把它描述为静态 HOLD。预测搜索框仍写入
`tracking_bbox/control_bbox`，并以 `hypothesis_source=MOTION` 标识。

### 3. 搜索进入条件

现有触发过度依赖尺寸收缩，桥面纹理仍能产生高分类分数，导致遮挡漏检。新逻辑使用多证据
状态机：

- 一旦当前观测未被接受但运动状态仍有效，立即进入 `SEARCHING` 并保持运动控制；
- 对“分数看似可靠但目标外观已改变”的情况，使用模板一致性、peak margin 退化、中心残差、
  尺度/宽高变化的持久组合证据；
- 单一瞬时异常只进入 suspicion，不冻结模板或误触发重捕；
- 进入搜索后冻结 anchor/recent 模板更新，清除旧 detector 批次并递增 generation；
- 不依赖帧号、桥的位置或预存目标轨迹。

### 4. 运动预测与云台控制

搜索期间由置信度加权 alpha-beta/有界加速度模型输出轨迹：

- 只用进入搜索前的已接受观测估计速度和可选加速度；
- 遮挡期间不使用未确认 NanoTrack 框纠正主状态；
- 对速度、加速度和尺度变化设置物理连续性上限，避免框突然跳到相邻车辆；
- 每帧输出预测框并保持 `control_valid=true`、`motion_control_authorized=true`；
- 预测框始终裁剪在画面内，尺寸从最后可靠尺度缓慢外推或保持，不追随遮挡物纹理；
- 本轮配置的运动搜索置信期必须覆盖从遮挡进入到第 190 帧，期间持续驱动且不进入另一套扫描
  语义；搜索走廊随预测不确定度有界扩张，但搜索 ROI 本身不能充当身份确认依据；
- 任何数值异常、时间倒退或无初始化状态必须 fail closed。超过本轮运动搜索置信期仍未重现时
  如何持续扫描属于后续独立设计，本轮不得用未经身份门约束的扫描候选自动重捕。

### 5. 搜索探测与 detector 调度

搜索阶段同时使用两条候选通道：

- NanoTrack 周期探测以预测框为中心，逐级扩大搜索区域；
- detector 在搜索阶段允许调度，不再被 coast/HOLD 无条件抑制，但仍受 generation、stale、
  class 和几何门控制。

detector 只产生候选，不能直接授权控制或更新模板。单次候选即使通过一次 anchor probe，也
只能写入独立的 shadow search hint，用于限定下一帧搜索 ROI；它不得直接修改已提交运动状态、
`control_bbox` 或宿主命令。只有第二次一致确认才允许原子提交重捕状态。相邻同类车通过预测
走廊、中心距离、运动方向、尺度、anchor 模板和跨帧一致性联合排除。异步 detector 结果必须
绑定源帧并外推到当前帧后再参与验证；确认失败时丢弃 shadow hint，不需要回滚宿主状态。

### 6. 两阶段重捕与模板生命周期

首个合法候选只进入 `REACQUIRE_CANDIDATE`：它只缩小/偏置下一帧 tracker/detector 搜索区域，
宿主仍由原运动预测框驱动，`observation_valid` 仍为 false，不更新模板。只有连续两帧满足
以下条件才原子提交确认框、运动状态和宿主控制框：

- 候选均位于运动预测走廊；
- 两帧候选相互满足 IoU/中心/面积/宽高比一致性；
- anchor NanoTrack 质量门通过；recent 模板若与 anchor 冲突，不得单独授权；
- 同类多候选不存在未消解的身份歧义。

确认后设置 `reacquired=true`、切回 `TRACKING`、用确认框重置运动状态，并先进入 recent
template probation；probation 通过后才替换 recent，anchor 在本视频中保持冻结。确认失败则
回到纯运动搜索，不能污染模板或改变身份。

## 验收门

所有门同时通过才可声称本视频目标完成：

1. **身份门**：0--270 无目标身份切换，尤其不得切到重现处右侧白车。
2. **可见框门**：0--168、182--270 每帧 IoU `>=0.50`，mean IoU `>=0.75`；每帧归一化
   中心误差 `<=0.50`（以真值框对角线归一化）。
3. **遮挡语义门**：172--180 全部为 `tracking_phase=SEARCHING`、
   `observation_valid=false`、`control_valid=true`、`control_source=MOTION_SEARCH`。
4. **预测连续门**：169--181 不计普通 IoU；预测中心相对估计轨迹的误差不超过一个估计框
   对角线，且无超出可见段 99 百分位两倍的速度或加速度突变。
5. **重捕门**：首个可靠重现帧为 182；确认重捕帧必须 `<190`，只产生一次合法
   `reacquired` 事件，重捕后持续跟踪到 270。
6. **控制门**：初始化后除明确的数值/输入故障外，整段 `control_valid=true`；搜索控制来源
   必须可追踪，不能伪装为观测控制。
7. **安全回归门**：detector 候选不能直接授权控制；stale/generation/异常恢复、同类歧义、
   模板回滚和 schema verifier 测试全部通过。
8. **实板门**：最终候选必须在 RK3588 上用同一模型/配置跑完整 271 帧，evaluator PASS，
   正式 run manifest 和 `verify_board.sh` PASS，输出视频可解码且逐帧数一致；主机合成测试
   不能替代实板结果。

canonical truth 只标注目标车，没有给相邻白车独立 ID/轨迹，因此身份门分两层：evaluator
自动检查预测走廊、中心连续性、禁止的大幅横向跳变和目标真值 IoU；随后人工逐帧观看最终
视频的 160--200 帧并签署“未切换到相邻白车”。在补充 distractor 真值前，自动代理指标不能
单独冒充完整身份真值。

本轮实板验收是确定性的离线文件顺序回放，固定输入、初始框、模型、配置、线程/队列策略并
至少重复三次。三次必须各自通过全部硬门，重捕帧跨度不超过 1 帧且每次都 `<190`。除最早与
最晚重捕帧形成的至多两帧转换窗口外，状态/phase、观测/控制来源、`control_valid` 和 frame
count 等离散语义逐帧相同；转换窗口只允许由合法重捕时刻造成的
`SEARCHING/MOTION_SEARCH` 与 `TRACKING/OBSERVATION`、`observation_valid`、`accepted`、
`reacquired` 差异，`control_valid` 在所有运行中仍必须为 true。可见框中心跨次差异 p95
`<=2 px`、最大 `<=5 px`，mean IoU 跨次差异 `<=0.01`。测时、线程完成时刻和原始
日志/CSV/metadata 整文件哈希不要求跨次相等；每次制品仍分别记录 SHA-256 以固定身份。
30 Hz live-source deadline 不是本轮 PASS 门；实际 runtime、pipeline FPS、detector 延迟和队列
年龄必须单独报告，不得把离线功能通过写成实时通过。

mean IoU 门用于推动全段质量，每帧 `0.50` 门防止均值掩盖局部脱靶。若人工复核发现标签边界
本身与可见整车不一致，只能修订并重新冻结真值制品，不得在 evaluator 中为算法加例外。

## 迭代顺序

1. 固化 evaluator 和现有 RK3588 baseline 报告；
2. 先修复搜索控制语义，保证遮挡期间轨迹与云台连续；
3. 修复遮挡/搜索进入，证明 detector 和 probe 可达；
4. 调整运动模型，使 169--181 预测连续并靠近重现端点；
5. 实现 detector/NanoTrack 两阶段重捕和模板 probation；
6. 在固定协议下做小规模参数搜索，每轮保留 config、程序/model/hash 和报告；
7. 只有流程正确仍不足时，才评估模型微调；
8. 在主机测试全绿后部署 RK3588，完整运行、评测并人工观看最终视频。

## 非目标与证据边界

- 本轮允许针对 `occ_video_1` 过拟合参数，但不允许帧号/真值轨迹硬编码。
- 超出本视频重捕窗口的长期扫描策略不在本轮实现；若以后实现，必须另行定义扫描区域与身份
  证据的独立关系，不能把扫描中心本身当作运动身份门。
- 通过该单视频不证明跨场景泛化、红外、长时稳定、生产交叉编译或真实吊舱验收。
- 本轮不修改协调方专属 `docs/progress/STATUS.md` 和
  `docs/progress/integration-contract.yaml`；需要的新 schema/门槛由本任务 Handoff 请求协调方采纳。
- 大型视频、RKNN、运行日志和搜索结果留在板端/制品目录，Git 只提交代码、配置、报告、
  provenance 和哈希。
