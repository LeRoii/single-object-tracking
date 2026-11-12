# RK3566 开发板验证任务进度

## 2026-11-12T15:48:10+08:00 2b08bc5 板端拒绝与冻结几何参考解耦

- 收窄提交 `2b08bc5bd4bd5c69bab6de2888db53972cc88de3` 已部署到独立根
  `/home/neardi/Project/SOT/occ-motion-trust-2b08bc5`，原生 RK3588 Debug 构建和 CTest
  `22/22` PASS，两目标程序 exit 0；但 full evaluator 仍 **FAIL**：car001 错误重捕
  `[84,102,127]`，car002 错误重捕 `[115,175,226]`，故该提交继续拒绝。
- 逐帧证据确认：即使仅冻结运动模型，观测质量的几何门仍以被冻结的运动预测为参考；随着
  NanoTrack 当前框继续运动，两者残差必然增长，最终由“主动冻结”自身制造 observation
  rejection 和 SEARCH。这违反“禁止运动更新先于确认遮挡，但当前 NanoTrack 框仍继续输出”
  的递进语义。
- 后续修正只解开这一处因果耦合：`motion_updates_held_` 且尚未进入 loss/search 时，观测的
  center/area/aspect 连续性改用上一已接受 `current_bbox_`，不再用有意冻结的 motion bbox；
  响应质量门和后续 SUSPECTED/SEARCH 递进保持不变，真正进入搜索后仍使用运动预测几何。
  单测通过人为拉开冻结预测与局部连续观测，确认不会发生自致拒绝。当前尚未提交或部署。

## 2026-11-12T15:43:51+08:00 d7ee7df 板端拒绝与运动信任边界收窄

- 精确提交 `d7ee7df5c6f3ab9805e7f29d08cf73e13e819564` 已部署到 RK3588 独立根
  `/home/neardi/Project/SOT/occ-motion-trust-d7ee7df`；源码归档主机/板端 SHA-256 均为
  `aa9137740e60a9d5ddd3508a401f4864235c9b2f3fc1b1bcdf9b97edace7b8fa`，二进制 SHA-256
  `e62028fa9f60aaf0585ef4710bae33c9556c02382454d060ecb6e2b1fffedde5`。原生 Debug、RKNN、
  `-Werror=switch` 构建与 CTest `22/22` PASS，两目标程序 exit 0，schema v12 板端 verifier
  均 PASS；这些功能结果不覆盖下述算法拒绝。
- 固定 ROI 双目标 full evaluator 均 **FAIL**。car001 出现错误重捕 `[104,158,194,236]`，
  car002 出现错误早期重捕 `[71]` 且未形成目标遮挡后的正式重捕；两个目标可见期质量均大幅
  回退。根因不是初始 ROI：首个单项 peak-margin 相对下降即同时冻结了运动、响应基线、稳定
  计数和模板推进，冻结参考无法适应正常外观变化，运动预测逐渐远离仍正确的 NanoTrack 观测，
  最终由自身制造几何拒绝和错误搜索。
- 本轮拒绝把上述结果升级为候选。修正边界收窄为用户原始要求：`motion_update_valid` 只控制
  运动模型校正；已接受观测原有的响应基线、stable 计数和 recent-safe 模板状态机恢复原行为，
  避免把“运动模型输入更严格”扩大成所有学习状态同时冻结。运动冻结/连续干净后恢复、schema
  v12 和方向启发式移除继续保留。
- 收窄后的主机 RKNN-off 构建、CTest `22/22`、evaluator 单测 `23/23`、`git diff --check`
  均 PASS；尚未提交或部署，下一步必须以新精确提交重新执行 RK3588 双目标回归。旧
  `d7ee7df` 根目录和失败报告保留，不覆盖。

## 2026-11-12T15:35:54+08:00 分级运动更新信任（主机验证）

- 分支 `codex/recent-safe-occlusion-recovery-v2`，基线
  `3f50eb205899e2b8e656d9c194de67f63478fea4`；结果为包含本条记录的当前提交。根据用户确认，
  本提交废止 `5fbcf83` 的“进入 SEARCH 时若方向相反则切换最后干净趋势”启发式，历史 RK3588
  结果继续保留为失败分析证据，不删除、不改写。
- 新策略将“当前 NanoTrack 框可继续输出/控制”和“允许更新运动模型”拆成独立语义：响应相对
  退化但仍被跟踪器接受时，`tracking_bbox/control_bbox` 继续采用当前观测，状态仍可保持
  `TRACKING`，但 `motion_update_valid=false`，运动位置/速度不再被该框校正，开环加速度和尺度
  速度清零；响应基线、稳定计数和 recent 模板推进同步冻结。只有连续干净观测达到既有
  `suspicion_clear_ms` 后，才用该干净时间段的首尾观测重新建立运动状态并恢复更新。
- 日志协议升为 schema v12，新增 `motion_update_valid`；CSV、JSONL、遥测、板端 verifier 与
  `occ_video_1` evaluator 同步校验“只有已接受的 TRACKING 观测才能更新运动模型”。这只是
  可观测性与安全不变量升级，不改变模型、检测器、遮挡阈值、重捕门或云台控制框语义。
- 主机 RKNN-off Debug 目录 `/tmp/nanotrack-motion-trust` 使用
  `-DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS=-Werror=switch` 完成构建；
  CTest `22/22` PASS，`python3 -m unittest tests.test_evaluate_occ_video_1` `23/23` PASS，
  `git diff --check` PASS。新增单测覆盖退化观测继续控制但冻结运动、保留干净速度并清零加速度、
  连续干净时间达标后才恢复更新。
- **边界 / Handoff**：当前仅为主机验证，尚未在 RK3588 上构建或复跑 car001/car002；不能据此
  声称视频效果改善或批准生产 handoff。下一步以精确提交部署到独立板端目录，固定双目标 ROI
  回归并与 C012、`b538667` 及 `5fbcf83` 保留结果比较。

## 2026-11-12T15:06:46+08:00 search 反向趋势修正 RK3588 双目标验证

- 分支 `codex/recent-safe-occlusion-recovery-v2`，源码提交
  `5fbcf8301ce52ad1c779541e30a35ad4153b2753`，基线
  `59c79e74a1dfc150d6257734598459f886d3da49`。源码归档在主机与板端 SHA-256 均为
  `6e73944baa06cf20ef24cca147da11c2a16f03be662c506bad1e7971d36609d5`，独立板端根为
  `/home/neardi/Project/SOT/occ-motion-opposite-5fbcf83`。原生 Debug、RKNN runtime、
  `-Werror=switch` 构建和 CTest `22/22` PASS；二进制 SHA-256
  `8766a6122b2d621bfeb95d4e32db8122cb3566fa9bd57c82aa2f8285de72762e`。
- 初版 `0a8304e9485dcb4b9754e37ca8a05300b03ad819` 对所有退化期都采用冻结趋势，car002
  方向改善但 car001 重捕从185延迟到194，故拒绝；其独立根
  `/home/neardi/Project/SOT/occ-motion-clean-0a8304e` 保留为失败证据。最终版只在进入 SEARCH
  时当前速度与最后干净趋势点积为负才替换速度并清零加速度；方向一致的 car001 保留原运动
  模型。
- 固定 ROI `car_001=992,582,37,39`、`car_002=970,517,38,38` 各运行0--270帧，均
  exit 0，AVI 均为1920x1080、30 FPS、271帧。非交互输出视频主框已改画
  `control_bbox`；`display_bbox`、交互调试叠框及生产显示路径未修改。
- car001 full evaluator **PASS**，重捕帧仍为185，visible/control/identity/prediction/
  full-occlusion 全部门通过。CSV、AVI、metadata、evaluator JSON SHA-256 分别为
  `f9161e18ee0fc6857459246262b59c2079025f51559e97595ede7091e8b08314`、
  `edf20ddcf711e329047ba4d70ce1c18d188a11ba93b57f4deafe091f0062dd03`、
  `575566a3b9e9645942d06342f4a65792c4cd9fc4bba46a6d5249e2c177c80ab6`、
  `dd9859784dfa2e4a00e4bd0e0d841264159aa6e04d5e42ecb4186033967e869f`。
- car002 在210帧仍是左下速度 `(-19.852020,+15.126831)`；遮挡物观测把216帧运动状态污染
  为速度 `(+21.519398,-8.891993)`、加速度 `(+146.120041,-109.989998)`。最终版在222帧
  SEARCH 切换为最后干净趋势 `(-15.818654,+16.426685)` 且加速度归零，并保持到234帧；
  预测中心相对正式重现目标的偏差由旧版约56 px降至约21 px，235帧重捕不变。full evaluator
  仍因既有213--216帧错误声明观测而 FAIL，但 visible/control/identity/prediction/reacquisition
  门均 PASS。CSV、AVI、metadata、evaluator JSON SHA-256 分别为
  `0a1d9288f555adb9561c98560f5de2e2d1f8ef0828b9295e18fbf75eceb80144`、
  `78fe73d1628d2b9b23f91d368175d3e907a02e847c48665a4ffd41186ec04765`、
  `63eb52962671321eb86e16bd4f159a22d2f6fe5ce08ede82a347ae74487b2eb7`、
  `7fd391078f91e8120c7a7f953274341ec5dd8c318547fc2727c61df0dc136637`。
- **边界 / Handoff**：本轮只修正进入 SEARCH 后采用反向污染趋势的问题，不修改阈值、配置、
  模型、模板、遮挡状态机或重捕确认。car001 不低于 `b538667` 基线，car002 已改善但并未解决
  213--216帧遮挡判定过晚；候选继续 HOLD，不构成正式生产 handoff 或真实吊舱验收。

## 2026-11-12T14:55:16+08:00 recent-safe v2 search 运动趋势修正（主机验证）

- 分支 `codex/recent-safe-occlusion-recovery-v2`，基线
  `59c79e74a1dfc150d6257734598459f886d3da49`。复核 RK3588 保留运行
  `/home/neardi/Project/SOT/occ-recent-safe-v2-b538667/runs/car_002/bboxes.csv`：目标在
  195--210 帧总体向左下运动，210 帧运动状态仍为
  `velocity=(-19.852020,+15.126831)`；211--216 帧遮挡物上的观测仍被判为可信并修正运动，
  到 216 帧已反向为 `velocity=(+21.519398,-8.891993)`、
  `acceleration=(+146.120041,-109.989998)`。217--221 帧虽不再更新可信状态，旧加速度仍被
  开环积分，222 帧进入 SEARCH 时进一步变为
  `velocity=(+39.388611,-23.931980)`，解释了预测框向右上偏离。
- 通用修正不改变遮挡判定、跟踪框、控制阈值、模板或模型：控制器保留最近一次“响应未出现
  相对退化”时的运动趋势；单项响应退化只冻结该 search 专用参考，不提前进入 SEARCH，连续
  `suspicion_clear_ms` 干净观测后才更新。真正进入 SEARCH 时，仅当当前速度与保留趋势方向
  相反才采用保留速度并将加速度清零；方向一致时保持原运动模型，避免无差别替换正常趋势。
- 后续非交互保存视频改为绘制 `control_bbox`；`display_bbox` 算法、交互叠框和生产
  `rect_show` 均未修改，等待真实吊舱测试后再决定显示平滑策略。
- 主机 RKNN-off 在独立目录 `/tmp/nanotrack-recent-safe-prediction` 完成 Debug 构建，CTest
  `22/22` PASS；新增测试覆盖保留趋势、反向退化观测和 SEARCH 清零加速度；视频主框切换经
  源码链路复核，`git diff --check` 通过。当前仅为主机验证，尚未部署或运行 RK3588，不能声称 car002 实际
  视频中的预测偏离已消除；下一步是提交源码后在独立板端目录原生构建，并以固定双目标 ROI
  复跑，C012 与 `b538667` 保留结果继续作为不得退化的对照。

## 2026-11-11T14:51:34+08:00 C009 holdout FAIL，停止调参与重复运行

- 冻结命令在 RK3588 上精确执行一次，输出 `1920x1080`/271 帧；首个正式
  `scope=holdout` evaluator 返回 FAIL。按冻结规则未执行 full-scope evaluator、第 2/3
  次重复或任何参数修改。
- 原遮挡后仍在帧 185 重捕，identity proxy、continuous control、prediction continuity
  PASS；但可见期 200--215 帧再次进入搜索，216 帧第二次重捕，不满足全序列
  唯一重捕。holdout 有效观测 mean IoU `0.671046 < 0.70`，all-visible `0.677702`。
- evaluator 还把不含全遮挡帧的 190--270 holdout scope 判为 full-occlusion gate FAIL，这是
  scope 适用性缺陷；排除它也不改变上述两个真实失败。
- RK3588 verifier 另在 CSV 帧 204 失败：`search_hint_present=1` + `CONFLICT`，但
  `search_hint_confirmation_count=0`，违反当前 schema 对 present hint 必须为 `1/2` 的约束。
- 保留根目录：`/home/neardi/Project/SOT/occ-candidate-05cf4d9/runs/C009-full-run1`。
  evaluator JSON/bboxes/video SHA-256 分别为
  `dae7168e3df422b42796ba156e70f7c1bfb67b0fb65596e63a3791a439343ffe`、
  `97f275ae1bf05eb345bb8335757ffbcaac063153d895c1f3ccd0808792f4c645`、
  `ac527664f99b1211d6ffa505d4bb65f9cc69b7795b7aaf870c97d3de30a63f36`。
  C009 结论为 `REJECT for release`；holdout 现已揭盲，后续候选不得声称仍为盲测。

## 2026-11-11T14:44:38+08:00 C009 development 平台 PASS 与冻结

- C009 RK3588 原生 Debug、`-Werror=switch`、CTest `19/19`；可执行/config SHA-256
  `3792ecde2f4b915d38496fa4fdbba6ecbe6854aae50897e9fb5e577d1d2a43a3`、
  `87b4b818e740a5a4137e4ab7339a26b5d70e93e5c7852e468dd26c8a73e06791`。
- 33/33 development 真实矩阵 PASS，重捕 `184--187`，最大 mean IoU 下降 `0.016618`；
  variable-time、detector delay/drop、generation-gap 的哈希 focused CTest 证据 PASS。
- 冻结前独立主机 CTest `19/19` 和 evaluator/robustness Python 单测 `18/18` PASS；
  全仓 Python discovery 另有 16 个模块因当前主机未安装 `cv2`/`torch` 而在导入阶段中止。
- 保留 nominal 在帧 185 重捕，evaluator/board verifier PASS；视频 `1920x1080`、190 帧。
  bboxes/video/evaluator JSON SHA-256 分别为
  `4d78dacdbd984f088aec6dff9139fa8e56de4a6e5687b1e3335273c191317098`、
  `0a5d887e20c3f5b556fc564c17abe90bd8a0a5073b0a795cdb490c34c19c34e3`、
  `8ea2ac37ba2238156f7cbd73f2dc1a4f1b5cb43f16c0d6634ad623d87d3bc504`。
- C009 已写入 `frozen-candidate.json`，状态 `NOT_RUN`；下一步只能执行其中精确 full
  command，不能再改 runtime/config。部分 190--193 status 文本暴露边界继续保留。当前仍无
  post-189 视频/bbox/runtime/evaluator 结果，也没有人眼 identity sign-off。

## 2026-11-11T14:26:43+08:00 C009 候选

- C009 恢复已通过 nominal/width+5 的 C007 runtime（probation=3），保留 400 ms、0.020
  和固定 `2/3` 几何显著规则；仅配套使用已提交的非刀刃 evaluator。
- 下一步优先重签 C007 保留 CSV，补跑缺失项并由 robustness aggregator 实际判定；尚未冻结，
  无 holdout/full 运行。

## 2026-11-11T14:26:10+08:00 C008 拒绝与视觉单帧绝对底线校正

- C008 板端 Debug/CTest `19/19`；probation 4/5 均 nominal PASS，但 width `+5%` 到帧 174
  才进入搜索，无法在 190 前完成两击，因此 C008 拒绝，完整矩阵未启动。
- probation=2 的唯一 C007 失败是帧 166 IoU `0.4916`、中心误差 `0.201`，不是丢失或身份
  错误。evaluator 单帧绝对 IoU 底线改为非贴线的 `0.45`；中心 `<=0.50`、mean `>=0.70`、
  identity、重捕、连续控制和矩阵相对下降 `<=0.03` 不变。合成测试锁定 0.46 PASS、0.43
  FAIL；evaluator/robustness 单测 `17/17` 通过。
- 下一候选恢复 probation=3，并重签/聚合已有真实矩阵报告；无 holdout/full 运行。

## 2026-11-11T14:21:16+08:00 C007 消歧通过与 C008 probation 内移

- C007 板端 Debug/CTest `19/19`；nominal 与关键 width `+5%` 均在帧 185 重捕。所有已执行
  实数 `+/-10%` 邻域 PASS，重捕范围 `184--187`。
- `recent_probation_frames=2` 在遮挡前 recent 模板提交后导致帧 166 视觉框门槛失败；该参数
  确实影响模板历史，不能当作无关抖动。C007 因而不冻结。
- C008 只将 probation 从 3 提到更保守的 4，并要求 `3/4/5` 平台；两击、消歧、运动控制和
  detector 隔离不变。尚无 C008 板端结果，无 holdout/full 运行。

## 2026-11-11T14:09:35+08:00 C006 平台结果与 C007 几何显著消歧

- C006 nominal 在帧 185 重捕；cadence `360/400/440 ms` 分别在 `184/185/187`，走廊
  `0.018/0.020/0.022` 均在 185 重捕。中心、width `-5%`、height `+/-5%` 通过。
- width `+5%` 的真实目标/邻车中心距离比为 `0.009615/0.017311`，但 count-only policy
  仍全部拒绝；同时 detector/template 分数更偏邻车，不能用分数消歧。
- C007 增加固定 `nearest <= 2/3 * runner-up` 几何显著规则。只解除最近候选的 ambiguity
  标志，仍需 anchor、recent veto、最小搜索龄与连续两击；相近候选继续拒绝。focused CTest
  `3/3` 通过，尚待全量测试、提交和 RK3588 验证。无 holdout/full 运行。

## 2026-11-11T13:50:36+08:00 C005 nominal 拒绝与 C006 最小候选

- C005 精确提交 `89215ab` 在 RK3588 原生 Debug 构建、`-Werror=switch`、CTest `19/19`
  通过，但 nominal 在 detector frame 182 的批次中 `0.015` 走廊把 121 个位置候选全部拒绝，
  因而无重捕；发现 nominal 失败后已立即停止其余 32 组矩阵。
- C006 保留已证明需要的 400 ms cadence，只把走廊恢复为 C004 的 `0.020`。这样以更早的新鲜
  可见帧解决 deadline/ambiguity，而不靠过紧位置阈值。尚无 C006 板端结果，无 holdout/full
  运行。

## 2026-11-11T13:47:27+08:00 C005 通用内点候选

- C005 仅把 detector cadence 从 `500` 内移到 `400 ms`，并把全帧对角线归一化走廊从
  `0.020` 收紧为 `0.015`；运行时 recent-veto-only、两击原子提交、同类歧义拒绝、
  generation/stale 隔离均不变。
- 选择依据是 C004 的真实邻域失败类别，不使用帧号或真值坐标：未来必须同时通过 cadence
  `360/440 ms` 和 corridor `0.0135/0.0165`，以及中心保持的初始化扰动，才允许冻结。
- 当前仅写入候选与台账，尚无 C005 板端结果；无 holdout/full 运行。

## 2026-11-11T13:46:23+08:00 C004 鲁棒性矩阵拒绝冻结

- RK3588 已执行 37 组 development-only 真实运行；临时视频每组评分后删除，保留 CSV、
  metadata、日志和报告。多数阈值 `+/-10%`、中心 `+/-2 px`、中心保持的高度 `+/-5%`
  均 PASS。
- 两个有效失败阻止冻结：`detector_interval_ms +10%` 无法在 190 前重捕，说明 nominal
  500 ms 卡在截止边缘；中心保持的 width `+5%` 在搜索后得到两个走廊内同类候选，安全地拒绝
  授权。width `-5%` 没有单帧质量失败且相对 mean IoU 只下降 `0.019565`，但被 evaluator
  与聚合器不一致的固定 `0.75` mean 门槛提前判死。
- evaluator 已以合成测试统一合同：单报告仍要求每有效观测 IoU `>=0.50`、中心误差
  `<=0.50`，绝对 mean floor 为 `0.70`；Task 8 聚合器继续要求所有变体 PASS 且相对 nominal
  下降 `<=0.03`。evaluator/robustness 单测 `16/16` 通过。
- C004 明确 `REJECT for freeze`；运行时 recent-veto-only 逻辑保留。下一候选只做通用的探测
  周期内移与归一化走廊收紧，再重跑有效邻域。没有 holdout/full 运行。

## 2026-11-11T13:25:34+08:00 C004 nominal PASS 与 provenance 接口补齐

- 同一 C004 产物经提交 `007f201ff998ec8b3d5f21ee134d406cc81d6870` 的 evaluator
  重算后 development 全门槛 PASS：帧 188 唯一重捕，有效观测 mean IoU `0.762083`，
  all-visible 诊断 mean IoU `0.741046`，搜索段 max speed/acceleration
  `370.984454/9331.818646`，均低于真值派生上限。报告 JSON/Markdown SHA-256 分别为
  `daad50c1a5be7361ba7d1f2b695d9a10a7ca0cf6ba66fc493159cdbf89a096d9`、
  `0336d518f7946a825a413820adc7e98ba87787e30d97223a8e2f218b54075f2c`。
- Task 8 聚合器要求报告内含源码、配置、模型身份；evaluator 新增成组 provenance 参数，拒绝
  缺项、非 40 位 commit 或非 64 位 SHA-256。evaluator/robustness 单测合计 `15/15` 与
  Python 语法检查通过。下一步重签 nominal 报告并执行真实扰动矩阵；当前仍未冻结。

## 2026-11-11T13:23:03+08:00 C004 重捕成功与 evaluator 语义分层修正

- C004 源提交 `0e19decf14d6fc674e78aa4c6249e12b70ba85b0` 已部署到 RK3588
  `/home/neardi/Project/SOT/occ-candidate-0e19dec`；源码归档 SHA-256
  `4eb692d72dae7f74093677b54313c7f0a7447ce3ba12eb253aa934c571f68082`。板端原生
  Debug、`-Werror=switch`、CTest `19/19` 通过；可执行文件 SHA-256
  `89e61bd1fb938be513f3b941af2e4354a682d952856de3566710b27119f28fa2`。
- 严格 `--max-frames 189` 的 `runs/C004-development` 运行完成，schema-10 board verifier
  PASS。帧 172--180 均为 `SEARCHING,false,MOTION_SEARCH`，控制连续；唯一两击重捕在帧
  188，随后帧 189 仍跟踪同一目标。旧 evaluator 报告显示重捕门槛、遮挡语义、连续控制均
  PASS，但把重捕前 `observation_valid=false` 的可见搜索预测当成视觉观测，并把原子
  `SEARCHING -> TRACKING` 切换计入同一运动段，因此其 FAIL 不能作为 schema-10 合同结论。
- evaluator 已用合成回归修正语义分层：搜索预测继续受估计轨迹、搜索段运动连续性和控制门槛
  约束；严格 IoU/中心误差只评价 `observation_valid=true`；原子重捕切换开启新的连续性段，
  但重捕观测仍必须具有真值重叠并经过 identity proxy。新增回归连同 evaluator 单测 `10/10`
  通过；尚待把这一提交精确传板并重算 C004 报告。
- 证据边界：运行程序、视频帧和 bbox 标签均未读取 190 以后；一次诊断命令的 `sed` 范围误显
  示了 `annotation_status.csv` 的 190--193 行状态文本。未运行 holdout/full evaluator，也未据此
  调整运行时代码或参数，但严格的“190 后元数据完全未暴露”边界已不再成立，后续冻结记录必须
  保留这一限制。

## 2026-11-11T13:11:43+08:00 C003 verifier PASS 与 C004 recent-veto-only 修正

- C003 在 RK3588 `/home/neardi/Project/SOT/occ-candidate-2389df0` 原生 Debug 构建、
  `-Werror=switch` 和 CTest `19/19` 通过；二进制 SHA-256
  `b2cad65b27fc18171ce36fadf8bdb63d4a970f16205ed934e3a4f17a01fc46da`。严格 0--189
  运行后，board verifier 首次 PASS，verify log SHA-256
  `f6564fb4dcfed79e3726f98d87cb3fb20a5bdbfa551023f269537cc304c585bd`。
- evaluator 仍与 C002 同一 FAIL：无重捕且 visible/identity gate 失败。逐字段证据显示窄走廊已把
  两批 position-valid/queued 数量从 3 降为 1；frame 187 唯一候选 center ratio `0.016328`，
  anchor 可靠，recent score `0.748582` 只达 weak，但两模板 IoU `0.911284`。当前实现错误要求
  recent 也必须 reliable，导致合法 anchor 无法建立 shadow first hit。
- C004 按既定证据政策修正为“anchor 唯一授权、recent 只否决”：可靠 anchor 必需；recent
  不存在或 weak 时不能授权也不能否决，只有 recent 自身 reliable 且与 anchor 几何冲突才 veto。
  detector identity/class/ambiguity、400 ms 最小搜索龄、0.02 corridor、两击一致性和第一击零提交
  均保持。
- 合成 detector 测试把第一击 recent 降为 weak 且保持高 IoU，确认仍只生成 shadow hint；第二击
  才原子提交。主机 CTest `19/19`、鲁棒性检查器单测 `4/4`、Python 语法检查和
  `git diff --check` 均通过。C003/C004 结果已只追加到 calibration ledger。
- C004 尚未提交、部署或运行；没有 holdout 证据，nominal 即使通过也尚未满足鲁棒性矩阵。

## 2026-11-11T13:07:57+08:00 C002 安全搜索结果与 C003 成熟窄走廊候选

- C002 在 RK3588 继续严格 `--max-frames 189`；二进制/config SHA-256 分别为
  `980545c9baeeceba21a9d0c10e7036c5eb3c73505c46b5c5adce694d1585cfd1`、
  `bb3513919fd4cc78ae3ae78b38057882ffa96d14bf22429a7f3120fd54e51901`。板端原生
  `-Werror=switch` 构建及 CTest `19/19` 通过，运行 SUMMARY mean runtime `9.338 ms`、
  mean pipeline `112.215 ms`、end-to-end `8.911 FPS`。
- evaluator development 仍 FAIL，但安全失败形态符合预期：full-occlusion semantics、
  continuous control、prediction continuity 全部 PASS，错误的 tracker-only 重捕已消失；两个
  detector batch 都保留三个同类候选，ambiguity gate 全部拒绝，因此 190 前无重捕。visible mean
  IoU `0.732761`；JSON SHA-256
  `f6585be2c7faf8b21000f0a99220771773020b08752abba0dcc41d236d8760cc`。
- C002 还暴露 verifier 不应要求 `hypothesis_source=MOTION`：搜索帧可以正在探测 detector ROI，
  但对外控制仍明确来自 `MOTION_SEARCH`。新增 DETECTOR-hypothesis 的合法 fixture 先 RED，再移除
  该错误耦合；phase/source/confidence/control_valid 仍全部必需。
- C003 将按整帧对角线归一化的 detector center gate 从 `0.15` 收紧为 `0.02`。前者在
  1920x1080 上约等于 330 px，而目标宽约 44 px；后者形成约一目标宽的通用 motion corridor。
  同时新增默认 0、候选 400 ms 的 `search_reacquisition_min_age_ms`，阻止短暂遮挡过渡立即种下
  重捕候选，且必须小于等于 motion confidence 期。
- parser、identity、metadata、verifier 与合成 controller 测试覆盖该最小搜索龄：龄前可靠观测
  不能产生 hint，龄后仍需两击才提交。C003 配置 SHA-256
  `89fbc1d23b2be17c8d6938f5f53383ec4b26a59383f4343dce80603335715706`；聚焦 4/4
  CTest 通过。后续即使 nominal 成功，也必须对两个实数门做 ±10% 邻域，不能直接冻结。
- C003 尚未提交、部署或运行；仍无 holdout 证据。

## 2026-11-11T13:01:15+08:00 C001 development 失败与 C002 detector-required 候选

- C001 在 RK3588 `/home/neardi/Project/SOT/occ-candidate-cbd38a5/runs/C001-development`
  严格以 `--max-frames 189` 完成，SUMMARY `tracked_frames=189`、mean runtime `9.148 ms`、
  mean pipeline `111.934 ms`、end-to-end `8.934 FPS`；没有读取 190--270。二进制 SHA-256
  `f58f22bb9e2bb6ba5821841d1402d3869e9978097d921d7bfb0cd5933199439d`，输入和所有
  RK3588 ONNX 派生模型/detector 哈希在运行前复核与 C000 一致。
- evaluator development 明确 FAIL：continuous control PASS，但 tracker-only shadow candidate
  在 171、183、186 三次错误重捕；full-occlusion semantics 于 172--173 失败，visible mean IoU
  `0.731970`，182--189 可见框与 183 以后 identity proxy 失败，运动速度/加速度也越过真值参考。
  JSON/Markdown SHA-256 为 `4aeb6e574db8c3fbbb466e176f9a373b559f725fefe1c419ad10bd1d9f290771`、
  `e616374ebb4b9432d2e5c31c1ffea14b6333ea1a5624e99637160034cb5dac15`。
- 同一真实输出暴露 verifier 将 `motion_control_authorized` 仍限定为旧
  `OCCLUSION_HOLD`，从而错误拒绝 frame 169 的 `UNCERTAIN / SEARCHING / MOTION_SEARCH`。
  新增 valid motion-search fixture 先复现 RED，再改为依据 schema 10 phase/source/confidence
  授权；旧 HOLD 的 age/budget 约束仍保留。
- C002 增加通用 `search_reacquisition_requires_detector=true`，默认 false 保持兼容；搜索态的
  非 detector 观测不再落入 legacy ordinary recovery，只能维持 motion control。detector 仍只是
  class/generation/position 安全的搜索种子，最终授权继续要求 anchor/recent 与两击几何一致，
  detector 本身不能直接控制。两击提交的 hint 同时明确记录 `used=true`。
- 配置解析、identity、metadata、verifier 与候选文件已同步，新候选配置 SHA-256
  `bb3513919fd4cc78ae3ae78b38057882ffa96d14bf22429a7f3120fd54e51901`；定向
  `controller_support|long_term_controller|long_term_runtime|tracking_log|verify_board_schema`
  为 `5/5` 通过。C001/C002 证据已只追加写入 calibration ledger。
- C002 尚未提交、部署或运行；没有 holdout 证据，不能宣称已重捕或已标定。

## 2026-11-11T12:53:27+08:00 development-only 鲁棒性聚合器与 RK3588 首次构建审计

- 在 `codex/occ-video-1-tracking`、Task 8 起点
  `2403c8618dc5f4234e399165e9168c5687aeb8e7` 新增纯报告聚合器
  `scripts/check_occ_tracking_robustness.py`。它不执行 tracker 或选择参数，只接受 schema 1
  candidate manifest 与已命名 evaluator reports；结果提交可用
  `git log -1 --format=%H -- scripts/check_occ_tracking_robustness.py` 查询。
- 聚合器要求每个实数参数 `-10%/nominal/+10%`、每个可调计数 `-1/nominal/+1`、初始框中心
  `±2 px`、尺度 `±5%`、变时间戳和 detector delay/drop/generation gap 全部齐备；所有报告必须
  严格为 development `0--189`、身份哈希匹配、所有 evaluator gate PASS、重捕 `<190`，且相对
  nominal 的可见 mean IoU 最大下降不超过 `0.03`。confirmation 至少两击、detector 不直接
  控制、generation/stale 隔离与异常 fail-closed 作为固定安全审计，不允许向下扰动。
- 单元测试先因脚本不存在 4/4 RED；实现后覆盖缺邻域、nominal-only 刀刃、错误 scope/holdout、
  重复报告、source/config/model 哈希不匹配、安全降级和 IoU 崩落，最终 4/4 PASS，
  `py_compile` 通过。完整主机 CTest `19/19`、0 失败、总时间 `4.24 sec`。
- 已把 `2403c86` 精确归档到 RK3588 `/home/neardi/Project/SOT/occ-candidate-2403c86`，归档两端
  SHA-256 `dc97c84662778a15ff85d36c17b73c1a4069dbfbd6cbf80bef17dcf1b7b4525a`；models、
  releases、data 均只链接到现有 ONNX 派生 RK3588 工件，复核哈希与 C000 一致。原生 Debug
  构建二进制为 `bb596ef7cb95c38232b724f3c4fb5d462d2ac8c0e48d1d2d25b0f5162211f207`，板端
  CTest `19/19`、0 失败、`6.40 sec`。
- 板端 `-Werror=switch` 复现发现 Task 6 新增的 `kSearchControlPrediction`/`kSearchHint` 未进入
  `main.cpp` 颜色 switch；根因是共享 overlay enum 已扩展而 Debug app 映射未同步。当前最小
  修复为两种语义增加独立颜色，避免主要 Debug 功能落到红色 fallback。修复提交后必须重新
  打包、原生构建和复跑 `-Werror=switch`，当前旧二进制不得用于 C001 数据运行。
- 尚未执行任何 0--189 candidate run，也没有读取 190--270；本条不是标定、鲁棒平台或板端
  功能通过结论。

## 2026-11-11T12:47:34+08:00 通用可见光遮挡候选配置与只追加标定账本

- 在 `codex/occ-video-1-tracking`、Task 7 起点
  `15f32fe8d52fee0b53e2c15d961b87f1c006ccd5` 新增通用
  `config/visible-occlusion-calibrated.conf`；结果提交可用
  `git log -1 --format=%H -- rk3566/nanotrack_v3/config/visible-occlusion-calibrated.conf` 查询。
- C001 保留 `visible-default.conf` 的已有视觉、遮挡、detector 和两击确认阈值，只显式启用
  `motion_search_enabled=true`，初始通用物理边界为 `2500 ms` 置信期、`1.0 diagonal/s`
  速度、`2.0 diagonal/s^2` 加速度、`0.50/s` 尺度变化和 `100 ms` probe 间隔；配置中没有
  视频名、帧号、真值坐标、桥梁或场景特例。`confirmation_required=2`、window 3 保持不变。
- 原 `visible-default.conf` 未修改，SHA-256 仍为
  `54dcc973feed11ad3e9e7838c8f35e86b0d587d5aa143863a625c849fa5dae19`；新候选为
  `495d5a0bc74d077cb333ae25affc512005b701fc432552f0994ed55607aab6c9`，controller identity
  测试确认两者不同且默认配置继续关闭 motion search。
- 新建只追加 `docs/evaluation/occ-video-1/calibration-ledger.md`，先冻结 C000 失败基线的源码、
  配置、RK3588 ONNX 派生模型、detector、输入与输出报告哈希，再登记 C001 的 parent、理由、
  变更参数和严格 `--max-frames 189` 待执行命令。C001 明确标为 `PENDING_NOT_RUN`；未知二进制、
  板端目录和报告写为 `PENDING`，没有从历史证据猜测。
- TDD RED 时两个测试分别因候选文件不存在失败；新增文件后，聚焦 `controller_support|cli`
  为 `2/2` 通过。新鲜完整构建及 CTest 为 `19/19` 通过、`0` 失败、总时间 `4.51 sec`，
  `git diff --check` 无输出。
- 本里程碑没有运行 C001、没有读取 190--270 holdout，也没有改变或重新生成 RK3588 模型；
  `calibrated=true` 仅表示候选配置接口，不能解释为真值标定已经完成。下一步 Task 8 先实现
  robustness manifest 校验器，再部署并只用 0--189 做候选与参数邻域验证。

## 2026-11-11T12:43:43+08:00 schema 10 搜索控制证据契约与 fail-closed 校验

- 在 `codex/occ-video-1-tracking`、Task 6 起点
  `936a5e77510786ec6631d94939f236492ae3e643` 将 tracking CSV 与 session metadata
  升级为 schema 10；结果提交可用
  `git log -1 --format=%H -- rk3566/nanotrack_v3/include/tracking_log.hpp` 查询。
- 每行现在直接序列化 controller/runtime 的 `tracking_phase`、`observation_valid`、
  `control_source`、真实 tracking/display/control 三类框、接受路径、搜索年龄与置信状态、
  速度/加速度/尺度速度，以及 shadow hint 的 source frame/timestamp、generation、projected
  bbox、确认计数、使用状态和拒绝原因；没有从旧 mode 或显示框反推这些语义。
- overlay 明确把搜索期云台框显示为 `SEARCHING / MOTION_SEARCH PREDICTION`，与
  `TRACKING / OBSERVATION` 分开；telemetry 以显式 phase/source 产生 `search_enter`、
  `search_expired`、`hint_first_hit`、`hint_reject` 和两击 `reacquired` 事件。
- 板端 verifier 现在要求 schema 10、连续帧/时间戳、三类有限且在画面内的框、通用运动搜索
  配置 identity、搜索控制置信状态和完整 hint 诊断；会拒绝 SEARCHING 冒充 observation、
  detector 直接授权、无置信 motion control、单击重捕、partial hint 以及 metadata/CSV schema
  不一致。旧模型/manifest/hash/runtime 检查继续保留。
- 新鲜验证命令重新配置并构建 `/tmp/nanotrack-occ-plan`，完整 CTest 为 `19/19` 通过、
  `0` 失败、总时间 `3.65 sec`；`git diff --check` 无输出。定向
  `tracking_log|debug_telemetry|interactive_debug_overlay|verify_board_schema` 为 `4/4` 通过。
- 本里程碑仍只有主机合成证据，没有改 ONNX/RKNN 模型，也没有同步或运行 RK3588/RK3566；
  不能宣称 `occ_video_1` 已达成 190 帧前重捕、阈值已由真值标定或真实云台已验收。下一步是
  Task 7 形成少量通用可见光候选配置，再在 0--189 development 段进行候选标定与扰动筛选。

## 2026-11-11T12:33:39+08:00 两击一致性重捕与模板 probation 原子提交

- 在 `codex/occ-video-1-tracking`、Task 5 起点
  `6b8c9f950b77b3b95ff3227d46d1c7d746fe7d9f` 将 tracker-seeded 与
  detector-seeded 重捕统一到一个 shadow confirmation candidate；结果提交可用
  `git log -1 --format=%H -- rk3566/nanotrack_v3/src/long_term_controller.cpp` 查询。
- 搜索中只有 anchor 可靠观测能建立/确认候选；recent 可以通过模板一致性门或否决，但
  recent-only 永远不能授权。第二击必须在 `confirmation_window` 内、identity/source 一致且满足
  IoU、中心、面积和宽高比门；同类邻近歧义、远框、模板冲突、窗口过期和 generation/source
  变化均不会提交。
- 第一击继续发布 motion control，不修改 committed bbox、motion 或模板；第二个合法击中才
  使用真实确认 tracker bbox 一次性重置 motion，清空 hint/候选队列，发布
  `TRACKING / OBSERVATION` 和单帧 `reacquired=true`，路径标记为
  `search_reacquisition`。后续普通跟踪不会重复发出 reacquired pulse。
- 原子提交帧只 `STAGE_RECENT`，不替换 anchor；后续普通可靠帧累计
  `recent_probation_frames` 后才 `COMMIT_RECENT`，期间 anchor/recent 不一致则
  `ROLLBACK_RECENT`，但 anchor 跟踪可继续。runtime 测试确认第一击零 bbox/template commit，
  第二击恰好一次 bbox commit 与一次 recent stage。
- RED 阶段首先因缺少 `kSearchReacquisition` 编译失败；GREEN 后聚焦
  `long_term_controller|long_term_runtime|long_term_replay` 为 `3/3` 通过。完整构建及 CTest
  为 `19/19` 通过、`0` 失败、总时间 `3.22 sec`，`git diff --check` 无输出。
- 本里程碑仍未在 RK3588/RK3566 或真实云台上运行；尚缺 schema 10 遥测、通用配置候选标定、
  冻结 holdout 与板端三次复跑，不能宣称 190 帧前重捕门已通过。

## 2026-11-11T12:27:57+08:00 SEARCHING detector 调度与 shadow hint 隔离

- 在 `codex/occ-video-1-tracking`、Task 4 起点
  `422cab44043786c755fb81a36063e55e3aa795a3` 完成候选路径的 search-wide detector
  调度：目标类别允许恢复且 detector ready 时，从第一个已进入 SEARCHING 的帧开始按
  `detector_interval_ms` 毫秒节流提交全画面请求，不再等待 ASSISTED/LOST；旧默认关闭候选时
  仍保留原调度与 coast 行为。结果提交可用
  `git log -1 --format=%H -- rk3566/nanotrack_v3/include/long_term_types.hpp` 查询。
- 异步候选保留 source frame/timestamp、generation 和 candidate identity，并先用 detector
  请求时与当前帧的有界 motion 位移投影到当前走廊，再执行类别、位置、多候选容量与歧义门。
  新增 `SearchHint` 诊断记录 projected bbox、首击帧、确认计数、是否已使用与拒绝原因。
- 一个合法 detector/anchor 首击只生成恰好供下一已处理帧使用的 shadow hint；hint 可改变
  NanoTrack 搜索 ROI，但未接受帧的 `tracking_bbox`/`control_bbox` 继续来自 motion，且不会
  correct/initialize motion、提交 bbox 或更新模板。第二击在本 Task 仅增加诊断或标记冲突，
  原子重捕提交留给 Task 5；中止帧、冲突、数值异常都会清除 hint/active candidate。
- runtime 测试确认 detector poll 异常仍保留 `MOTION_SEARCH` 控制，且 bbox commit、recent
  stage/commit/rollback 均不发生。既有 stale、frame/generation、class、position、capacity、
  same-class ambiguity 和 legacy HOLD 拒绝顺序继续由原测试覆盖。
- RED 阶段因 projected bbox、SearchHint 与 search-wide 调度接口缺失编译失败；GREEN 后聚焦
  `long_term_controller|long_term_runtime` 为 `2/2` 通过。完整构建及 CTest 为 `19/19`
  通过、`0` 失败、总时间 `3.48 sec`，`git diff --check` 无输出。
- 本里程碑仍仅为主机合成验证；没有板端同步、构建或视频运行，不能表述为 detector 已在
  RK3588 上帮助重捕或 `occ_video_1` 已通过。下一步是 Task 5 的两击一致性原子提交与 recent
  template probation。

## 2026-11-11T12:20:56+08:00 通用时间制有界 motion search 主机实现

- 在 `codex/occ-video-1-tracking`、Task 3 起点
  `339d5e0a50fef15af8f9666f2664678d35d91e99` 以 TDD 增加默认关闭的
  `motion_search_enabled`，以及毫秒置信期、按画面对角线归一化的速度/加速度上限、每秒尺度
  变化上限和搜索探测间隔。配置解析、验证和 identity 均覆盖这些字段；结果提交可用
  `git log -1 --format=%H -- rk3566/nanotrack_v3/src/motion_estimator.cpp` 查询。
- motion estimator 只从已接受视觉观测修正速度、加速度和尺度速度，支持 16/33/67 ms
  变帧率与丢帧时间步，所有导数有界、预测框有限且裁进画面。公开 snapshot 给出运动导数、
  search age 和时间置信状态；时间倒退、未初始化及非有限状态继续 fail closed。
- 候选开关启用后，第一个未接受观测立即进入 `SEARCHING`，保留最近接受尺度并以 motion
  prediction 同时发布 `tracking_bbox`/`control_bbox`，在置信期内明确发布
  `observation_valid=false`、`control_valid=true`、`control_source=MOTION_SEARCH`；详细模式在
  UNCERTAIN/LOCAL/ASSISTED 间变化不会中断授权。置信期后仍是搜索态但撤销控制；两次一致的
  NanoTrack 视觉确认恢复 `OBSERVATION` 控制。高质量但满足遮挡缩框证据的观测不会被提交，
  也不会走旧的固定帧数 coast 授权。
- 每个搜索 episode 只递增一次 detector eligibility generation，并清空确认、候选和旧请求；
  重复拒绝不会重复递增。旧默认 `motion_search_enabled=false`，因此既有 RK3566 配置的普通拒绝
  与 frame-count coast 行为保持不变。
- RED 阶段三个聚焦目标因新增字段和接口不存在而编译失败；GREEN 后命令
  `ctest --test-dir /tmp/nanotrack-occ-plan -R 'controller_support|long_term_controller|long_term_replay' --output-on-failure`
  为 `3/3` 通过，随后完整构建及 CTest 为 `19/19` 通过、`0` 失败、总时间 `3.29 sec`，
  `git diff --check` 无输出。
- 本里程碑仅是主机合成状态矩阵证据，未同步、编译或运行 RK3588/RK3566，未改变 ONNX/RKNN
  模型或工件哈希，也不能表述为 `occ_video_1` 已通过、阈值已完成真值标定或真实云台验收。
  下一步仍需实现搜索期 detector shadow hint、两击重捕、schema 10，再按冻结 evaluator 在
  RK3588 完整复跑。

## 2026-11-11T12:13:38+08:00 观测有效性与控制来源外部语义契约

- 在 `codex/occ-video-1-tracking`、Task 2 起点
  `73efebb0130e6c94ee097e2c4613e2b8179dc59f` 以 TDD 新增
  `TrackingPhase {TRACKING, SEARCHING, INVALID}`、
  `ControlSource {NONE, OBSERVATION, MOTION_SEARCH}`，并把 `tracking_phase`、
  `observation_valid`、`control_source` 放入 `SearchPlan` 和 `ControllerOutput`。所有完成的
  controller 输出在返回前经过同一语义校验器；结果提交以
  `git log -1 --format=%H -- rk3566/nanotrack_v3/include/long_term_types.hpp` 查询。
- 直接接受观测现在明确发布 `TRACKING / observation_valid=true / OBSERVATION`，且
  `tracking_bbox==control_bbox`，display smoothing 继续独立；既有授权 coast 明确发布
  `SEARCHING / observation_valid=false / MOTION_SEARCH`。旧默认配置下的普通拒绝暂时显式为
  `SEARCHING / false / NONE / control_valid=false`，没有在 Task 3 的 feature gate 和有界运动置信期
  完成前静默切换 RK3566 默认控制行为。
- RED 阶段 `test_long_term_controller` 因枚举、字段和校验接口不存在而编译失败；GREEN 后聚焦
  `tracker_math`、`long_term_controller`、`long_term_runtime` 3/3 通过，完整 RKNN-independent
  构建及 CTest `19/19` 通过，`git diff --check` 通过。本里程碑只建立共享 core 语义；尚未写
  schema 10 日志字段，也尚未使普通未接受帧持续驱动云台。下一步由 Task 3 在通用候选配置
  显式启用时间制有界 motion search。

## 2026-11-11T12:09:22+08:00 `occ_video_1` evaluator 与失败基线冻结

- 在分支 `codex/occ-video-1-tracking`、实现起点
  `e51a1d2d63c34077b4489e74675dcef03aba5681` 完成实施计划 Task 1；结果提交以
  `git log -1 --format=%H -- scripts/evaluate_occ_video_1.py` 查询。本里程碑新增独立
  `scripts/evaluate_occ_video_1.py`，runtime 不读取它或真值。evaluator 严格核对 canonical
  video/metadata/events/status/label-manifest 哈希，将 169--181 排除普通 IoU，分层报告可见框、
  估计轨迹、完全遮挡语义、控制连续性、重捕和身份代理；合法输入但硬门失败 exit 1，输入或
  身份错误 exit 2。schema 9 仅允许复现失败基线，候选 PASS 必须使用带明确
  `tracking_phase`、`observation_valid`、`control_source` 和 tracking box 的 schema 10。
- TDD 首轮在生产脚本不存在时 8/8 预期失败；实现后聚焦套件最终 9/9 通过。真实 RK3588
  baseline CSV 只读复制到 `/tmp/occ-baseline-full-20261111-bboxes.csv`，SHA-256
  `b2ba9d7328f0554d1806a36f817ece8a42a5cdd0e4275b9cae44f1b87271eb6f`，与板端记录一致；
  evaluator exit 1 并精确复现普通可见 mean IoU `0.5785649329`、0--168
  `0.6311449519`、182--270 `0.4787219754`，无合法重捕且遮挡/控制/schema/可见框/身份代理门
  失败。紧凑证据在 `docs/evaluation/occ-video-1/baseline-201991a.{json,md}`；完整可再生报告
  SHA-256 `6ed8b993bd825b38dc033515a286e51e6283906f7e1449689d6fc9128ee8ce6a`。
- 系统 `python3` 全量 discover 因缺 `cv2` 等既有环境依赖只运行 79 项并产生 16 个导入错误；
  改用项目 NanoTrack venv 后实际运行 286 项，除本工作树已知绝对主工作树路径/缺本地 venv/
  缺历史 smoke report 的 `test_nanotrack_three_stage_configs` 5 failures + 1 error 外，其余通过。
  本任务新增套件独立全绿；这些既有 linked-worktree 测试边界未被改写或冒充全套通过。
- 本里程碑没有修改 C++ runtime、配置阈值、模型、协调方 `STATUS.md`/契约或板端部署；原基线
  目录仍缺 `manifest.json`，因此继续是诊断 FAIL，不是 board verifier PASS。下一步按计划 Task 2
  先以失败测试引入 observation/control 来源的外部语义契约。

## 2026-11-11T11:18:21+08:00 `occ_video_1` 方案 2 获批并形成实施计划

- 用户已批准
  `docs/superpowers/specs/2026-11-11-occ-video-1-closed-loop-tracking-design.md` 的最终设计，包含
  SEARCHING 期间持续运动预测与云台控制、190 帧前两阶段重捕、detector shadow hint、模板
  probation 以及避免单视频过拟合的泛化护栏。批准不等于算法或板端验收已经完成。
- 实施计划写入
  `docs/superpowers/plans/2026-11-11-occ-video-1-closed-loop-tracking.md`，按 evaluator、外部语义、
  时间制运动搜索、搜索期 detector、两次确认重捕、schema 10、通用可见光候选配置、0--189
  标定/扰动平台、冻结 holdout 和 RK3588 三次完整复跑九个任务执行。计划明确第一次暴露
  190--270 前必须提交冻结记录，若盲测结果促成修改则该段永久降级为 regression。
- 本里程碑仍只提交设计/计划/任务进度，没有修改 runtime 算法、阈值、模型或协调方专属
  `STATUS.md`/`integration-contract.yaml`，也没有产生新的主机测试或开发板运行证据。起点为
  `codex/occ-video-1-tracking@98ee995e0ef67f51bfb6114754eb3f43d7e0e909`；结果提交以
  `git log -1 --format=%H -- docs/progress/board-validation.md` 查询。

## 2026-11-11T10:53:22+08:00 `occ_video_1` 完整基线与方案 2 设计待批准

- 新任务在隔离工作树
  `/home/u20/.codex/worktrees/rk3588-debug-deploy/single-object-tracking` 的独立分支
  `codex/occ-video-1-tracking` 开始，输入基线为已推送 RK3588 Debug 提交
  `201991a92bba95b542c97505c37053d1c4fe1da8`。本里程碑只包含诊断与设计，尚未修改算法；
  结果提交以 `git log -1 --format=%H -- docs/progress/board-validation.md` 查询。
- 用户确认完全遮挡不算永久丢失：172--180 帧仍输出运动预测框并持续驱动云台；用户选择
  “真值 evaluator + 状态机/运动/detector/NanoTrack/模板闭环重构”的方案 2，并把正确重捕
  硬门定为第 190 帧之前。设计在
  `docs/superpowers/specs/2026-11-11-occ-video-1-closed-loop-tracking-design.md`，仍需用户批准后
  才能写实现计划或代码。
- 在 RK3588 `192.168.1.102` 对当前候选运行完整 271 帧基线，实际核心命令为：

```bash
cd /home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f
stdbuf -oL -eL ./build/nanotrack_v3 \
  --video data/occ_video_1/1.mp4 --bbox 992,582,37,39 --model-dir models \
  --sensor visible --controller-config config/visible-default.conf --max-frames 270 \
  --detector-model releases/yolov8n-visdrone9-rk3588-fp16-v1/models/yolov8n-visdrone9-rk3588-fp16.rknn \
  --detector-manifest releases/yolov8n-visdrone9-rk3588-fp16-v1/models/detector-manifest.json \
  --detector-labels releases/yolov8n-visdrone9-rk3588-fp16-v1/models/visdrone9.txt \
  --detector-class-id 2 --detector-conf 0.25 --detector-nms 0.45 \
  --output-video runs/occ-baseline-full-20261111/output.avi \
  --output-boxes runs/occ-baseline-full-20261111/bboxes.csv \
  --session-metadata runs/occ-baseline-full-20261111/session-metadata.json \
  > runs/occ-baseline-full-20261111/run.log 2>&1
```

- 运行 exit 0，SUMMARY `tracked_frames=270`、mean runtime `9.196 ms`、runtime FPS
  `108.744`、mean pipeline `111.414 ms`、end-to-end FPS `8.976`。按 frozen truth 排除
  169--181 后，可评价 258 帧 mean IoU `0.579`；0--168 为 `0.631`，182--270 为
  `0.479`。遮挡状态从未触发、detector 从未请求；174--178 和 180--184
  `control_valid=0`；182 帧 IoU `0.303`，185 帧才恢复普通接受且 `reacquired=0`。
- 证据目录
  `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/runs/occ-baseline-full-20261111`；
  `bboxes.csv`、`output.avi`、`run.log`、`session-metadata.json` SHA-256 依次为
  `b2ba9d7328f0554d1806a36f817ece8a42a5cdd0e4275b9cae44f1b87271eb6f`、
  `0002caa118473ee41e2c015e29728445f157ac2b2f70996d179670fe8c2906e4`、
  `511e02d7e6aca3eac432ceb472bd54fb7c964ea1ef31d362b3fe632260c138cf`、
  `08f34c2452ad379eed26cd2e14e7c8cfd58960bc2ad450ff2b74e65005c8f9a8`。
  本次诊断目录未生成 `manifest.json`，故 `verify_board.sh` 明确报
  `missing or empty artifact`；它是失败基线，不计正式 board PASS。
- 独立 builder 审查的五项歧义已写入设计：一次 detector hit 只允许 shadow search hint；
  改动进入共享 core 但只由独立 Debug 配置启用；identity 自动代理门后仍需人工视频签署；
  本轮是离线帧序功能验收而非 30 Hz 实时门；最终运行必须同时通过 evaluator、run manifest 和
  `verify_board.sh`。未修改协调方专属 `STATUS.md` 或 `integration-contract.yaml`。
- 用户随后明确要求避免对单视频过拟合，把本视频视为典型遮挡场景并保留泛化能力。设计因此
  删除 `occ_video_1` 专用配置和允许过拟合的表述，改用通用
  `visible-occlusion-calibrated.conf` 候选；增加 190--270 冻结 holdout、合成状态机矩阵、
  参数 `±10%`/计数 `±1` 稳定平台、初始框中心 `±2 px`/尺度 `±5%` 扰动和时序/异步扰动门。
  `occ_video_1` 明确禁止进入任何后续模型训练或训练期选择。

## 2026-11-10T19:21:14+08:00 RK3588 独立 Debug 迁移与实板运行

- 本轮按用户确认的“方案 1”在隔离工作树
  `/home/u20/.codex/worktrees/rk3588-debug-deploy/single-object-tracking` 执行；主线输入为
  `main@1bf657ff730891ae0bea48340a046cbcbeaa87d4`，实现链为
  `3c7801897372f63f7b94755806ea2c53f4e03648`（转换目标参数化）、
  `493a3f8429bec0e85bb48fd15598218fb38ab6b2`（RK3588 Debug 配置与校验器）、
  `b1dc8450b3a679e9f41a22a3d9e3af25a527de39`（C++ runtime 接受 RK3588 detector
  manifest）和 `cb2d0fab019bf44edd55b3f947506b8b389bf19f`（校验当前 SUMMARY 格式）。
  工作树为 detached HEAD；本条进度的结果提交以
  `git log -1 --format=%H -- docs/progress/board-validation.md` 查询。未修改协调方专属的
  `docs/progress/STATUS.md` 或 `docs/progress/integration-contract.yaml`。
- 目标板最终地址为 `192.168.1.102`，hostname `LKD3588`，硬件
  `Rockchip RK3588 NEARDI LKD3588 Board`，AArch64 Ubuntu 22.04、Linux 6.1.84，
  GCC 11.4.0、CMake 3.22.1、OpenCV 4.5.4；RKNPU driver `0.9.8`。系统
  `/usr/lib/librknnrt.so` SHA-256 为
  `0f1c5db6c649c8705759308aa56ecffc3295c7eb465f9ec9170882f44004aa8d`，实际运行日志
  API 为 `2.1.0 (967d001cc8@2024-08-07T19:28:19)`。编译头从已连接 RK3566 的
  `/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/include/rknn_api.h` 取得，SHA-256
  `c48e11a6f41b451a5fd1e4ad774ea60252d3d94f78bee9b21ea3d21b21deba9a`；只使用该头
  编译，未覆盖 RK3588 系统 runtime。

### ONNX 到 RK3588 模型身份

- 严格未复制 RK3566 RKNN。NanoTrack 作者仓库 HEAD
  `248663fde6bf7c40190cf10ee396d5662919ecd3`，权重 SHA-256
  `4f0d25108385a67bdec5fa01ca37c74acb72343b53996dd645fe18b86e512e69`；确定性导出 seed
  `20260929`、opset 14，ONNX checker 和 ORT parity 均通过。计划指定的旧
  `nanotrack` venv 为 Torch 1.7.1、最高稳定 opset 12，实际会报
  `Unsupported ONNX opset version: 14`；因此实际导出使用与仓库历史 opset-14 制品一致的
  `/home/u20/single-object-tracking/.venvs/rknn-convert/bin/python`（Torch 2.4.0、ONNX
  1.16.1、ORT 1.23.2），未降低 opset 或跳过数值校验。
- 实际转换核心命令为：

```bash
/home/u20/single-object-tracking/.venvs/rknn-convert/bin/python \
  rk3566/nanotrack_v3/tools/export_v3.py \
  --repo /home/u20/single-object-tracking/repos/siamtrackers/NanoTrack \
  --config /home/u20/single-object-tracking/repos/siamtrackers/NanoTrack/models/config/configv3.yaml \
  --weight /home/u20/single-object-tracking/repos/siamtrackers/NanoTrack/models/pretrained/nanotrackv3.pth \
  --output-dir /home/u20/single-object-tracking/artifacts/rk3588-debug-deploy/nanotrack \
  --seed 20260929 --opset 14 --atol 1e-4 --rtol 1e-4

/home/u20/single-object-tracking/.venvs/rknn-convert/bin/python \
  rk3566/nanotrack_v3/tools/convert_rknn.py \
  --model-dir /home/u20/single-object-tracking/artifacts/rk3588-debug-deploy/nanotrack \
  --source-commit 248663fde6bf7c40190cf10ee396d5662919ecd3 \
  --weight-sha256 4f0d25108385a67bdec5fa01ca37c74acb72343b53996dd645fe18b86e512e69 \
  --target rk3588

/home/u20/single-object-tracking/.venvs/rknn-convert/bin/python \
  rk3566/nanotrack_v3/tools/convert_yolov8_rk3566.py \
  --onnx /home/u20/single-object-tracking/artifacts/yolov8-visdrone9/export/yolov8n-visdrone9-rkopt.onnx \
  --output /home/u20/single-object-tracking/artifacts/rk3588-debug-deploy/detector/yolov8n-visdrone9-rk3588-fp16.rknn \
  --manifest /home/u20/single-object-tracking/artifacts/rk3588-debug-deploy/detector/detector-manifest.json \
  --labels /home/u20/single-object-tracking/artifacts/rk3588-debug-deploy/detector/visdrone9.txt \
  --source-url local://yolov8n-visdrone9-rkopt.onnx \
  --source-repository https://github.com/airockchip/ultralytics_yolov8 \
  --source-commit 4674fe6e003dfbc5f2250d3b39dd31faaf7a9877 --target rk3588 \
  --output-count 9 \
  --training-checkpoint /home/u20/single-object-tracking/artifacts/yolov8-visdrone9/export/yolov8n-visdrone9-best.pt \
  --training-repository https://github.com/airockchip/ultralytics_yolov8 \
  --training-commit 4674fe6e003dfbc5f2250d3b39dd31faaf7a9877 \
  --training-config /home/u20/single-object-tracking/artifacts/yolov8-visdrone9/runs/visdrone9-full/args.yaml \
  --dataset-audit /home/u20/single-object-tracking/artifacts/yolov8-visdrone9/dataset-audit.json
```

- 新生成的 RK3588 template/search/head SHA-256 依次为
  `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55`、
  `d3cb0e3879df6a1272adb59bf501324f1f8f55b58309d3c4da4a731b600973ee`、
  `4da379f54093d79f02775e90bfea33ebee0837c6fc73a324499279b4c57807c5`；tracker
  manifest 为 `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`，
  `provenance.target_platform=rk3588`。VisDrone9 RK3588 model/manifest/labels 依次为
  `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c`、
  `ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9`、
  `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`；detector
  manifest 为 RK3588、FP16/no quantization、9 outputs、9 classes，源 RKOPT ONNX SHA-256
  `b2948072f01da7fe3b686392a47fe742380c98775c4149111527b7ddf26a5cc8`。主机模型包
  `/home/u20/single-object-tracking/artifacts/rk3588-debug-deploy/SHA256SUMS` 全部通过；审计还确认
  新包文件名不含 `rk3566`，且没有复制 tracked RK3566 RKNN 路径。

### 原生构建与实板运行

- 隔离部署根为
  `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f`，创建前确认不存在。canonical
  输入 `data/occ_video_1/1.mp4` SHA-256
  `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`，1920x1080、
  30 FPS、271 帧；顶层数据清单 SHA-256
  `872483add3bb9ae3ddc4172b689e224ccf3e65b51bd0ab07268ee02957c3254f`。
  source archive `inputs/source-cb2d0fa.tar.gz` SHA-256
  `171d587d0abc0fa91d4587bc7ad9200bbced198f69118470afd1c89f8fe25be6`。后续源码更新解包
  显式排除 `models/*`，避免源码仓库中的历史 manifest 覆盖生成模型。
- 板端实际构建命令：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
  -DENABLE_RKNN_RUNTIME=ON \
  -DRKNN_INCLUDE_DIR=/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/include \
  -DRKNN_LIBRARY=/usr/lib/librknnrt.so
cmake --build build -- -j2
ctest --test-dir build --output-on-failure
```

  原生 Debug 构建成功；最新完整 CTest `19/19` 通过。`build/nanotrack_v3` 和
  `build/yolo_detector_smoke` SHA-256 分别为
  `64c3839cc4666b48d160e3d7482237979fabf2b96ec51b80888cb7658021008a`、
  `2437181e5d77817137f1af362ddfbdc2181ad488f4ed4d667e4b08c407560e1e`；`ldd`
  确认二者使用板载 `librknnrt.so`，无缺失依赖。完整配置、构建、CTest 和 ldd 日志均留在部署根。
- 以下命令均在
  `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f` 执行；这是本轮保留证据所用的完整
  detector、bounded、verifier 和交互应用参数（交互键盘输入由临时 ctypes/XTest helper 等价执行
  `Space`、等待 4 秒、`Q`，其结果保留在 `runs/interactive-debug/x11-input.log`）：

```bash
ffmpeg -v error -y -i data/occ_video_1/1.mp4 -frames:v 1 \
  runs/detector-smoke/frame0.png
stdbuf -oL -eL ./build/yolo_detector_smoke \
  --model releases/yolov8n-visdrone9-rk3588-fp16-v1/models/yolov8n-visdrone9-rk3588-fp16.rknn \
  --manifest releases/yolov8n-visdrone9-rk3588-fp16-v1/models/detector-manifest.json \
  --labels releases/yolov8n-visdrone9-rk3588-fp16-v1/models/visdrone9.txt \
  --image runs/detector-smoke/frame0.png --conf 0.25 --nms 0.45 --class-id 2 \
  --output-image runs/detector-smoke/annotated.jpg \
  --output-csv runs/detector-smoke/detections.csv \
  > runs/detector-smoke/run.log 2>&1

stdbuf -oL -eL ./build/nanotrack_v3 \
  --video data/occ_video_1/1.mp4 --bbox 992,582,37,39 --model-dir models \
  --sensor visible --controller-config config/visible-default.conf --max-frames 60 \
  --detector-model releases/yolov8n-visdrone9-rk3588-fp16-v1/models/yolov8n-visdrone9-rk3588-fp16.rknn \
  --detector-manifest releases/yolov8n-visdrone9-rk3588-fp16-v1/models/detector-manifest.json \
  --detector-labels releases/yolov8n-visdrone9-rk3588-fp16-v1/models/visdrone9.txt \
  --detector-class-id 2 --detector-conf 0.25 --detector-nms 0.45 \
  --output-video runs/occ-smoke/output.avi --output-boxes runs/occ-smoke/bboxes.csv \
  --session-metadata runs/occ-smoke/session-metadata.json \
  > runs/occ-smoke/run.log 2>&1

EXPECTED_RKNN_TARGET=rk3588 scripts/verify_board.sh \
  runs/occ-smoke models 61 1920 1080 \
  releases/yolov8n-visdrone9-rk3588-fp16-v1/models/yolov8n-visdrone9-rk3588-fp16.rknn \
  releases/yolov8n-visdrone9-rk3588-fp16-v1/models/detector-manifest.json \
  releases/yolov8n-visdrone9-rk3588-fp16-v1/models/visdrone9.txt \
  > runs/occ-smoke/verify.log 2>&1

timeout --signal=INT 30s env DISPLAY=:0 XAUTHORITY=/home/neardi/.Xauthority \
  stdbuf -oL -eL ./build/nanotrack_v3 \
  --config config/rk3588-interactive-debug.conf --bbox 992,582,37,39 \
  --max-frames 120 --output-boxes runs/interactive-debug/bboxes.csv \
  --session-metadata runs/interactive-debug/session-metadata.json \
  > runs/interactive-debug/run.log 2>&1
```

- detector 单帧 smoke 使用 canonical frame 0、class 2 `car`、conf/NMS `0.25/0.45`。
  模型成功初始化并报告 `target=rk3588`、API 2.1.0、driver 0.9.8、9 outputs；推理结果
  `raw=346`、保留上限 100 个 car 候选，preprocess/inference/postprocess/total 为
  `8.745/53.909/12.492/75.146 ms`。这证明模型加载和推理完成，不是 detector 精度或阈值
  质量结论。证据在 `runs/detector-smoke/`；`run.log`、`detections.csv`、
  `annotated.jpg` SHA-256 依次为
  `cb4351742e38787f13e7887e47f884852ee4441893612d1c288ec9ef16661c1c`、
  `22d6a9e758ce791d2cb4f875053b5527c0efa3e5bf2b58f7339f692a6c5018d5`、
  `10b793e67844d041c67b04ca9543a1a3eb925c0041722def7da92bd1634c9c46`。
- bounded canonical smoke 使用 bbox `992,582,37,39`、`--max-frames 60`、class 2 和上述模型。
  初始化帧加 60 个跟踪帧，共 61 行数据/61 帧视频；SUMMARY 为
  `tracked_frames=60`、mean runtime `8.489 ms`、runtime FPS `117.799`、mean pipeline
  `113.422 ms`、end-to-end FPS `8.817`。执行上述完整
  `EXPECTED_RKNN_TARGET=rk3588 scripts/verify_board.sh` 命令得到
  `PASS board artifacts output_frames=61 tracked_frames=60`。61 行中 detector direct-control
  行为 0；本短段稳定跟踪中 detector request/complete/reject/error 均为 0，因此不证明 detector
  重捕。证据在 `runs/occ-smoke/`；log/CSV/metadata/video/verify SHA-256 依次为
  `cf043fa92c8c546e499ee0d2b43c6da9e92b47a8fcaf1a7cfbb45468cc9301b1`、
  `f8bdfbe3d84d793f6acac176de845e4edebd3cc36f5b476dc29276ae4abf0fe8`、
  `5515a66da7125d89ed126845e7023dc58a5ec20a050321c0029344c471a748a6`、
  `0c6c852c045d9d517095e94db7beade7d48bdde94bc963949ee0efe799c2be41`、
  `34296832c1129c5694da72a7f307e3d46bcf7d1aa0b6d9dc391da36e6cc55bab`。
- X11 Debug 在 active `:0` 会话运行 `--config config/rk3588-interactive-debug.conf`，显式同一 bbox，
  通过 XTest helper 发送 Space 播放、4 秒后 Q 退出；应用与输入 helper 均 exit 0。日志记录
  `interactive_ready`、`action=play_pause`、30/60/90/120 帧结构化 `frame_status`、
  `frame_limit_reached`、`action=quit` 和 `shutdown reason=user_quit`，最终 120 tracked frames。
  证据在 `runs/interactive-debug/`；log/CSV/metadata SHA-256 依次为
  `2cc348f9e0d5a74ec14759dea97340e5c2582ef4a07bed6c344093ecee75ada4`、
  `e18aef2ba86ad8fef9e4fa6d3fa1c076255190ec3ddc167b1ab4898a4a15c4d4`、
  `20c3d1d9aaa33b82c452d4d5054f4bb998c8ff5f6371f66a1ef919329d079c77`。
  总清单 `runs/EVIDENCE-SHA256SUMS` 含 40 项；环境快照在 `runs/BOARD-ENVIRONMENT.txt`。
- 首次 detector smoke 因 C++ runtime 仍硬编码 RK3566 manifest 而失败，原日志保留为
  `runs/detector-smoke/run-initial-rk3566-hardcode-failure.log`，随后由 `b1dc845` 修复并复测。
  二次源码归档解包曾只覆盖 tracker manifest 为 RK3566（RKNN 文件实测仍为新 RK3588 哈希），
  该次不一致运行保留于 `runs/occ-smoke-invalid-rk3566-manifest/`，不计入通过证据；恢复并逐文件
  `cmp` RK3588 包后重新运行，最终 `runs/occ-smoke/` 才是有效证据。

### 证据边界与 Handoff

- 本轮只证明在指定 LKD3588 上：四个从 ONNX 新生成的 RK3588 模型身份一致、原生 Debug 构建、
  detector 单图推理、canonical 视频 60 帧集成运行、板端 schema/safety 校验和真实 X11 交互播放/退出
  均通过。它不满足当前 integration contract 的 RK3566 board gate，也不构成 detector promotion、
  production handoff、生产交叉编译、长时稳定性、遮挡重现质量、阈值/性能校准或真实吊舱验收。
  `functional_pass_promotion_held` 和既有 RK3566 `latest_handoff.available=false` 不因本轮改变。
- 请求协调对话决定是否在 `integration-contract.yaml` 中把 RK3588 建立为与 RK3566 分离、明确命名的
  Debug target，并审核提交链 `3c78018..cb2d0fa` 后再决定是否集成。若建立新目标，还需分别冻结
  RK3588 runtime/API/driver、四模型 manifest/hash、canonical 路径及独立验收门；不得用本轮实验结果
  替换既有 RK3566 合同身份。

## 2026-11-08T22:52:30+08:00 统一非训练测试数据板端落盘核验

- 协调基线为 `main@9d0085f63e5e332db2142bfff71bfcef5f20b8c7`；任务工作树
  `/home/u20/single-object-tracking/.worktrees/board-validation-progress-sync`、分支
  `codex/board-validation-progress-sync` 在核验前干净并快进到该提交。本轮只核验用户已上传的
  数据并更新本任务文件，没有运行算法、修改源码、模型、公共 `STATUS.md` 或
  `integration-contract.yaml`。
- 板端实际路径为 `/home/cat/Project/SOT/data/occ_video_1`。在真实 RK3566 板端执行
  `sha256sum 1.mp4 annotation_metadata.json occlusion_events.json annotation_status.csv labels.sha256 checksums.sha256`，
  得到：视频 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`、
  元数据 `5641650435a1980694f982cf1fa6715ba959b1515938cee383c8db8ec4aeb076`、
  遮挡事件 `a43f016c96b34c8a0ec6125dcb944ba4fa07a79d99a46ce696bbb29841ded256`、
  逐帧状态 `e44fd23514d6871e72d628ad4843fc90bb4246f306322ee297c8a2aa75818036`、
  标签清单 `9ec01854884126350f93e1af3a4aee002d38ce7c2d50c71c78de0f1a8dc0a648`、
  顶层清单 `872483add3bb9ae3ddc4172b689e224ccf3e65b51bd0ab07268ee02957c3254f`，
  与 schema v2 集成契约逐项一致。
- 板端继续执行 `sha256sum -c --quiet --strict checksums.sha256` 和
  `sha256sum -c --quiet --strict labels.sha256`，两者均退出成功；`labels.sha256` 为
  271 项，`labels/` 实际 271 个 `frame_*.txt`，共 271 行有效 YOLO 记录，均为
  `class_id=0` 且每行 5 字段。`annotation_status.csv` 为连续 frame 0～270 的 271 条
  状态记录；169～181 的来源均为 `bidirectional_interpolation`。
- 板端 `ffprobe` 实测 `1.mp4` 为 1920×1080、`30/1` FPS、271 帧。解析元数据与事件记录
  确认目标为完整 `car`，169～171 部分遮挡、172～180 完全遮挡、181 首次可见、182
  首次可靠重现、184 稳定重现，遮挡物为人行天桥。169～181 是人工确认估计框，
  `ordinary_iou_metrics_allowed=false`，后续普通 IoU 统计不得包含这些帧。

### Handoff

- 请求协调对话采纳板端实际路径
  `/home/cat/Project/SOT/data/occ_video_1` 及上述哈希，补全公共契约的
  `test_data.canonical_dataset.target_environment_path`。后续新增开发板非训练算法证据只使用
  该数据；`sot-v1.mp4` 和 `girl_dance.mp4` 只保留历史输入身份，不再新增验证结论。
- 本轮仅证明冻结数据已完整落盘并与契约一致，不证明算法运行、遮挡恢复、精度、性能、
  阈值标定、生产交叉编译或吊舱验收。任务仍保持
  `functional_pass_promotion_held`，`development_board_validation.latest_handoff.available=false`。

## 2026-10-31T10:43:00+08:00 最终源码事件脉冲板端复测

- 被测源码 `codex/adaptive-occlusion-coast@d589215f1c0319f2686d6364edfec86a67a0c63e`，工作树干净；相对 `e3ec19615a0d931544fa9567f288be62a0935551` 的 `git diff --binary e3ec196 HEAD -- rk3566/nanotrack_v3` SHA-256 `64a0f8ab085a4e067618944b9e3899b0e7cedbf6df392d5e72b971efd57b01c7`。本进度提交结果身份见 Git 日志。
- 已校验源码 bundle 两端 SHA-256 `25d601866477794fe6d06223c09a22aff0f66db20ec79ba04d13ae6d8f444623`；新建独立板端 worktree `/home/cat/Project/SOT/adaptive-coast-d589215`，干净 detached HEAD `d589215f1c0319f2686d6364edfec86a67a0c63e`，源码子树对象 `4956f404de2ff6e4b63649191c09861ecb4caf83`。原工作区和此前证据未修改。本次复算模型、manifest、labels 和两视频 SHA-256，均与下两节记录相同；控制配置 `visible-default.conf` SHA-256 `2010c6cd16553595844a12f103f53412942289262d866b9fadba26b2821337ed`，Debug 配置 SHA-256 `e3fe5f927ba3a47d2829dce600c01634f7f2f405576865c2edea22e0256493f8`。板端时间 `2026-10-31T10:38:10+08:00`；Linux 4.19.232 AArch64，动态链接 OpenCV 4.10、RKNN runtime 2.3.2，运行日志 API 2.3.2、driver 0.8.2。
- 新证据根 `E=/home/cat/Project/SOT/validation-adaptive-coast-d589215`，源码 `S=/home/cat/Project/SOT/adaptive-coast-d589215/rk3566/nanotrack_v3`；`A`、`R` 与下两节相同。执行 `cmake -S "$S" -B "$E/build" -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=ON -DOpenCV_DIR=/home/cat/ThirdParty/OpenCV4100/lib/cmake/opencv4 -DRKNN_INCLUDE_DIR=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/include -DRKNN_LIBRARY=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/aarch64/librknnrt.so`、`cmake --build "$E/build" -- -j2`、`(cd "$E/build" && ctest --output-on-failure)`：CTest `17/17`、15.74 秒。二进制 SHA-256 `1d09aa599db01c663762f4185a9d7b9a36d2ed6c6c0bbd88a40fbba4ad0e0886`；`configure.log`、`build.log`、`ctest.log` 依次 `bfba39142461f7f1a5338c4de643441dd222e45f7f87fd82ca85be62c48796b5`、`a1c1cc90dc9833a484068d84cbc87d6f6e63ef909bc06ecc785be783ca5b7468`、`bdcf7286abba9ebbadd3cb80b7f8e30e60cf94c015f3739ab8c4729f197ccdd6`。

两次非交互实际命令与两节后的完整命令相同，仅 `E` 和 `S` 换成本节新目录；视频、ROI、模型、配置、帧数、阈值、尺寸不变，均 exit 0。为直接观察仅在交互模式输出的结构化事件，另执行：

```bash
timeout --signal=INT 90s env DISPLAY=:0 stdbuf -oL -eL "$E/build/nanotrack_v3" --interactive --video "$A/data/girl_dance.mp4" --bbox 196,160,184,310 --model-dir "$A/models" --controller-config "$S/config/visible-default.conf" --detector-model "$R/yolov8n-visdrone9-rk3566-fp16.rknn" --detector-manifest "$R/detector-manifest.json" --detector-labels "$R/visdrone9.txt" --max-frames 200 --output-boxes "$E/girl-interactive.csv" --session-metadata "$E/girl-interactive.json" > "$E/girl-interactive.log" 2>&1
```

交互窗口初始化后由 `/tmp/adaptive_xplay.py` 使用 X11/XTest 发送空格播放，达到 200 帧上限后发送 q 正常退出，exit 0；最终辅助脚本 SHA-256 `dba4ccf1bbeaf045b41084450c1664c768ca876a6798656502eab2c68d4b466a`。视频证据来自非交互运行；交互运行保存 CSV、metadata、原始结构化日志。

`girl_dance` 的触发/探测/控制轨迹不变：34 帧 suspicion_start，39 帧 `GRADUAL_WIDTH_PEAK` age 1 且运动控制有效；43 帧 age 5 SEARCH 命中一次，44 帧 age 6 CONFIRM 被质量或运动几何拒绝，48 帧 age 10 SEARCH 同样拒绝；50 帧 age 12，51 帧 LOCAL_RECOVERY 且控制无效。交互结构化日志中该次 suspicion_start 在 34 帧仅一次，51 帧 LOCAL_RECOVERY 的 `occlusion_suspicion_started=0`，没有重复事件；该帧仍保留 `FULL_STABLE_MEDIAN`、参考宽高 `227.833588/278.244293` 和 `GRADUAL_WIDTH_PEAK`。整段共有三次 suspicion_start（34、54、124），后两次出现在恢复为 TRACKING 之后，是后续新的 suspicion 起点；不能声称整段只有一次。全部 201 行 hypothesis 为 MOTION、恢复 detector 请求为零，未见 probe 或 detector 直接授权控制；成功 early recovery 未观察。

`tracking-test-v1` 同样 120 帧，初始类别确立 request/complete/error `1/1/0`，仍 UNKNOWN/NO_MATCH；121 行含 35 个 ASSISTED_RECOVERY 行，恢复 request/submit/complete `0/0/0`，候选队列、锁均为零，hypothesis 全为 MOTION，未见 detector 控制。无真值视频只支持定性状态/控制观察。

`girl.{avi,csv,json,log}` SHA-256 依次为 `aee8ea7743978bb3495f497e2efca8f19d2d5e22f6882f8c3e1dffe25cf38b4a`、`0d20556c753a09794f1cc763ec0ee0bcaf15000d0e5f3a4ac2edb219b32ee23d`、`c6b7126db37cf8c80971f25864ed305a156c8155ab062b7b27354c89c0019ba7`、`083987093b3b3924574a8c1c7cb7303523d8c282b988a8f4998f7d716cca612c`；`girl-interactive.{csv,json,log}` 依次为 `dc4dde141340764e579bb0d431e73bf4d9c3ac4642c4d9d66abdb829f2211e8c`、`c7d5819be9573dcdbd4a95cfd4021a0bc91ab7ab44b22745d50cb73a06abe370`、`5429d883c779c39699e6bdb0fef42f82c53564f3faeb11cddc9d3f7adba4e8d7`；`tracking.{avi,csv,json,log}` 依次为 `1d1d369364bd623d8a98d393f19108103c8356bee42dc3ae44859b0504604d93`、`617b555f612cd3ee42556a418770dbf03d5fa617faae05add47f69646a5b1607`、`847c1ff270d144bc75ae4ba0d2a8b59d25b205c0425e627728b7b9b1b4883da4`、`001cf43552312b7df7277f2c526a2530c1e9ae7487dc588fdf82815fd6f906ec`。

### Handoff

请求协调方采纳本次精确源码板端证据；promotion 继续 held、`latest_handoff.available=false`。detector 正例确立及确认重捕、成功 early recovery、长稳及其他晋级门未闭环；没有真实吊舱验收。

## 2026-10-31T10:29:51+08:00 UNKNOWN 恢复请求门控修复的板端复测

- 复测源码 `codex/adaptive-occlusion-coast@e3ec19615a0d931544fa9567f288be62a0935551`，父提交为上次证据 `9f38674`；相对上次被测源码 `3890c50bc6aff067027d63f0140d798f783d472e` 的 `git diff --binary 3890c50 e3ec196 -- rk3566/nanotrack_v3` SHA-256 `1638b4d3dcdcc855b44f2f9a3dae92374c5d469dd3eb3b30190b893fc7fd2972`。本进度提交的结果身份见 Git 日志。
- 板端独立工作树 `/home/cat/Project/SOT/adaptive-coast-e3ec196` 干净，HEAD 精确为 `e3ec19615a0d931544fa9567f288be62a0935551`，源码子树对象 `4fbd95d4c6c192449e6427a5558397c6be45a52f`；传输 bundle 两端 SHA-256 `eebaeefe98b9a654792e7e630a5cfc2aa475541d24f9523f1941e201e4b2fa64`。原有 checkout 和上次证据目录未修改。新证据根 `E=/home/cat/Project/SOT/validation-adaptive-coast-e3ec196`，源码 `S=/home/cat/Project/SOT/adaptive-coast-e3ec196/rk3566/nanotrack_v3`；`A`、`R`、模型及两视频路径与下节相同，所有模型、manifest、labels 和视频 SHA-256 均于本次复核并与下节一致。`$S/config/visible-default.conf` 为 `2010c6cd16553595844a12f103f53412942289262d866b9fadba26b2821337ed`；Debug 配置为 `e3fe5f927ba3a47d2829dce600c01634f7f2f405576865c2edea22e0256493f8`，本次未传 Debug 配置。
- 板端时间已核对，Linux 4.19.232 AArch64；实际动态链接 OpenCV 4.10 和 RKNN runtime 2.3.2，运行日志 API 2.3.2、driver 0.8.2。执行 `cmake -S "$S" -B "$E/build" -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=ON -DOpenCV_DIR=/home/cat/ThirdParty/OpenCV4100/lib/cmake/opencv4 -DRKNN_INCLUDE_DIR=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/include -DRKNN_LIBRARY=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/aarch64/librknnrt.so`、`cmake --build "$E/build" -- -j2`、`(cd "$E/build" && ctest --output-on-failure)`；完整 CTest `17/17`、15.53 秒。`$E/build/nanotrack_v3` SHA-256 `9c533b18fba5e0f25b56d1c2f48ecf3ede16382e8e675382f44c54e1af83d508`；`configure.log`、`build.log`、`ctest.log` 分别为 `47689096a5c49cb9b8a46010d15fcf6828abb2d43565f2fc5645bb427758b9b4`、`777378efb93222698a54207383f810851819b88908269cad228941ba6c6d7830`、`180d7ff6c69acf98eed4404e4bf0e33b286d306bae2976deb1e5dd17843cfd78`。

复测两条实际命令与下节两条完全相同，除 `E` 和 `S` 使用本节新目录；均 exit 0，输入、ROI、模型、配置、阈值、帧数和输出尺寸不变。非交互输出视频截图保留在新证据目录，不是 GUI 截屏。

- `tracking-test-v1`：初始分类独立请求/完成/错误为 `1/1/0`，六个候选均未匹配，结果仍 UNKNOWN/NO_MATCH、恢复关闭。121 行含初始化，状态 TRACKING 52、UNCERTAIN 10、LOCAL_RECOVERY 24、ASSISTED_RECOVERY 35；恢复 detector 计划请求/实际提交/完成均 `0/0/0`（metadata 恢复 request/complete/error 为 `0/0/0`），旧版同场景提交过 3 次。全部行候选队列 0、candidate ID 空、locked 0、hypothesis MOTION；69 个非控制有效帧中包括全部辅助恢复帧，无 detector 候选激活或 detector 直接控制。该样本复测支持旧 UNKNOWN 下额外请求问题已解决；不代表所有板端场景或 detector 正例恢复通过。无真值视频只作为定性观察。
- `girl_dance`：200 跟踪帧、初始化分类请求/完成 `1/1`、UNKNOWN，恢复 detector 请求/完成 `0/0`。第 34 帧 suspicion 开始；39 帧 `GRADUAL_WIDTH_PEAK` age 1 且运动控制有效；43 帧 age 5 SEARCH 命中一次，44 帧 age 6 CONFIRM 被质量或运动几何拒绝，48 帧 age 10 SEARCH 被拒绝；第 50 帧 age 12，51 帧 LOCAL_RECOVERY 且控制无效。全 201 行均为 MOTION，HOLD/UNCERTAIN 无 detector 请求，未见 probe 直接授权控制。与上轮观察的滑行节奏一致；成功 early recovery 仍未观察。

`tracking.{avi,csv,json,log}` SHA-256 依次为 `1d1d369364bd623d8a98d393f19108103c8356bee42dc3ae44859b0504604d93`、`6429112e5bee74086115123661894227dab7a38ca971875930c3e465b5e8d603`、`0fbef4559311b13983582f7d0604e0d410d86063774bdec8cfe0684eb86040c4`、`ad6929735b9f31a85ecc3ec20de63628f28253b126df6decb4abb975a1461229`；`girl.{avi,csv,json,log}` 依次为 `aee8ea7743978bb3495f497e2efca8f19d2d5e22f6882f8c3e1dffe25cf38b4a`、`57a0d294fa8a15126ff8dd876c429e5249d7f1aa79eb83526a36478b3bee9ab8`、`47aeeb8206789a7460e81b75244417ee84502b6e81aa259fee475ac63e60fa10`、`efe077d9eebefd4f896fe22d3dbd4f413e7455084a5ee3d1b7efdd5324fc8643`。`tracking-initial.png` 为 `e348eff93a1ccb09ca6c9c366f7c8dbe80d85d172d3a2bad85684bb3ec2a33be`；`girl-observation-01.png` 至 `05.png` 依次为 `19b3bb71b46afe492b895755db45ef35b5421b1b9b49728bc4ce222fddc74ef5`、`5a166c086b67fb64eb76248b2955942007a1af5df9650600b22660c133eb6156`、`fe7659b5509b33417ce300ea36bb52320c6310b6e673795acab8db7124b42787`、`c4380d238a8517edb1975a0f341649c32f75d1c90160c36a8b0f5404cf320dde`、`80533a8ad85ad2d91374ccec238a564a7a6ff16e03b405ed7cd98967b28e8321`。

### Handoff

请求协调方采纳本次旧问题复测证据。仍保持 promotion held 和 `latest_handoff.available=false`：detector 正例确立、detector 确认重捕、成功 early recovery、长稳及其他晋级门尚未闭环；没有真实吊舱验收。

## 2026-10-31T10:17:30+08:00 自适应遮挡滑行板端验证：恢复请求问题待处理

- 源码工作树 `/home/u20/single-object-tracking/.worktrees/adaptive-occlusion-coast`，分支 `codex/adaptive-occlusion-coast`，验证基线 `3890c50bc6aff067027d63f0140d798f783d472e`；与 `main` 的 merge-base 为 `b827c0a6d8916e9c0832c748e30722f6f2fe5563`。`git diff <merge-base> HEAD --binary` 的 SHA-256 为 `632162a38530bacaf541179e3a6f6f8920dfb6825d850680a60c807681580fd9`。本进度提交以该基线为父提交；结果提交身份见 Git 日志。
- 新建主机构建 `/tmp/nanotrack-adaptive-coast-final`：`cmake -S rk3566/nanotrack_v3 -B /tmp/nanotrack-adaptive-coast-final -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF`、`cmake --build /tmp/nanotrack-adaptive-coast-final -- -j2`、`ctest --test-dir /tmp/nanotrack-adaptive-coast-final --output-on-failure` 均成功，CTest `17/17`、1.63 秒；`git diff --check` 成功。主机测试不是板端场景验收。
- 板端既有 `/home/cat/Project/SOT/nanotrack_v3_longterm` 有修改的 `config/interactive-debug.conf` 和未跟踪 `cmd.txt`，未改动。将提交打包为 `/tmp/nanotrack-adaptive-3890c50.bundle`，两端 SHA-256 均为 `a8bbd83e3a19a0f16662994b7c28b24d4583b9616bf66130b66081e839f5d7a9`；独立裸库 `/home/cat/Project/SOT/adaptive-coast-3890c50.git` 的 worktree `/home/cat/Project/SOT/adaptive-coast-3890c50` 为干净 detached HEAD `3890c50bc6aff067027d63f0140d798f783d472e`，源码子树对象 `ec83cf87b265557b49800be729a42ba05aca1e1d` 与主机一致。板端时间 `2026-10-31T10:12:29+08:00`，Linux 4.19.232 AArch64；实际加载 OpenCV 4.10 和 RKNN API 2.3.2、driver 0.8.2。
- 板端构建及证据根目录 `E=/home/cat/Project/SOT/validation-adaptive-coast-3890c50`；源码目录 `S=/home/cat/Project/SOT/adaptive-coast-3890c50/rk3566/nanotrack_v3`。执行 `cmake -S "$S" -B "$E/build" -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=ON -DOpenCV_DIR=/home/cat/ThirdParty/OpenCV4100/lib/cmake/opencv4 -DRKNN_INCLUDE_DIR=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/include -DRKNN_LIBRARY=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/aarch64/librknnrt.so`、`cmake --build "$E/build" -- -j2`、`(cd "$E/build" && ctest --output-on-failure)`；成功，板端 CTest `17/17`、16.01 秒。`$E/build/nanotrack_v3` SHA-256 `7d96500d2832c87c033b8e1afdd95e381d7b71c11c8618540cf80a48b9d2d3c8`；`configure.log`、`build.log`、`ctest.log` SHA-256 分别为 `a7645ce858f8db6463f998e5c9ee150ca6e27ece8fd145c5cce402ae3bb48ae0`、`94a2aed5a4740944b0ccb29390c53440db542a6fc1392390285bf57a519cb89c`、`63cd9c5ec6d283711b0bde6d65d5eab4e4b7c58c8a29546ae4ea7d3d647a91ed`。
- 输入根 `A=/home/cat/Project/SOT/nanotrack_v3_longterm`，`R=$A/releases/yolov8n-visdrone9-rk3566-fp16-v1/models`。实测 `$A/models/{template_backbone.rknn,search_backbone.rknn,head.rknn,manifest.json}` SHA-256 依次为 `38b60334e8e0f0999bfb7ff9e943090546461f9307682653685237e24b2a7709`、`92125f1ef6254dade5786bb999faa5b9d9412ceb2b7987f6dbba82f959b7ef6a`、`2065bfca503cca9197e67de1a754efad930dfed4d2fc73881c77d9e850dcb600`、`441e93f2092afe94bac663f27adb1fe99848357356efd67c7473b0b506f854b2`；`$R/{yolov8n-visdrone9-rk3566-fp16.rknn,detector-manifest.json,visdrone9.txt}` 依次为 `3698bc9bea1f43a4bac2826a09e03be0439fddfbc5c5604d8ed0f92b36881df0`、`05e6d8c7c210b7f9d85b8fb712aa00b014f310e46e85f171ce1158afc90ba63e`、`41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`，与已记录身份一致。`$A/data/girl_dance.mp4` 为 `8090fcd5090adb62f21e79e799f0b0871d9c610d8cf8087b9fcdd3c7e9c2e9b9`；`/home/cat/Project/SOT/data/tracking-test-v1.mp4` 为 `aa30402f63cf9fdf01f96a5907ad1010ecdd09098ff0e8435815d9c3ddb82d07`。提交内 `$S/config/interactive-debug.conf` 为 `e3fe5f927ba3a47d2829dce600c01634f7f2f405576865c2edea22e0256493f8`；实际使用的 `$S/config/visible-default.conf` 为 `2010c6cd16553595844a12f103f53412942289262d866b9fadba26b2821337ed`，不同于历史旧配置，原因是本源码提交新增 coast 参数。

### 场景命令与观察

两次均使用下列非交互方式、源码默认 detector 类别和阈值（-1、0.25、0.45），未传 Debug 配置；显式传入上述模型和控制配置。视频、CSV、metadata、日志和提取截图保留在 `$E`，均为真实板端运行，未改阈值：

```bash
timeout --signal=INT 180s stdbuf -oL -eL "$E/build/nanotrack_v3" --video "$A/data/girl_dance.mp4" --bbox 196,160,184,310 --model-dir "$A/models" --controller-config "$S/config/visible-default.conf" --detector-model "$R/yolov8n-visdrone9-rk3566-fp16.rknn" --detector-manifest "$R/detector-manifest.json" --detector-labels "$R/visdrone9.txt" --max-frames 200 --output-size 428x240 --output-video "$E/girl.avi" --output-boxes "$E/girl.csv" --session-metadata "$E/girl.json" > "$E/girl.log" 2>&1
timeout --signal=INT 240s stdbuf -oL -eL "$E/build/nanotrack_v3" --video /home/cat/Project/SOT/data/tracking-test-v1.mp4 --bbox 1395,1075,240,125 --model-dir "$A/models" --controller-config "$S/config/visible-default.conf" --detector-model "$R/yolov8n-visdrone9-rk3566-fp16.rknn" --detector-manifest "$R/detector-manifest.json" --detector-labels "$R/visdrone9.txt" --max-frames 120 --output-size 960x540 --output-video "$E/tracking.avi" --output-boxes "$E/tracking.csv" --session-metadata "$E/tracking.json" > "$E/tracking.log" 2>&1
```

两次均 exit 0。`girl_dance` 200 跟踪帧：第 34 帧 suspicion 开始；第 39 帧触发 `GRADUAL_WIDTH_PEAK`，age 1，`motion_control_authorized=1`、`control_valid=1`。第 43 帧 age 5 SEARCH 命中一次，第 44 帧 age 6 CONFIRM 因 `QUALITY_OR_MOTION_GEOMETRY` 拒绝，第 48 帧 age 10 SEARCH 同样拒绝；第 50 帧 age 12，51 帧进入 LOCAL_RECOVERY 且控制无效，52 帧 MOTION 重捕。该次没有成功的 early probe recovery；12 个 HOLD 帧及两个 UNCERTAIN 帧均无 detector 请求，所有 201 行 hypothesis 均为 MOTION，未见 detector 或 probe 直接授权控制。初始化目标类别 UNKNOWN/NO_MATCH，故不能证明 detector 确认重捕。截图 `$E/girl-observation-01.png` 至 `05.png` 分别取输出帧 39、43、44、48、51；这是输出视频截图，不是交互 GUI 截屏。

路口视频首帧 `$E/tracking-first.png` SHA-256 `f7a460dc17e165adb2e8aba72498c448f0ab2fc8b6df75499cc1fccdbf054088`；人工检查后选取前景深色轿车 ROI `1395,1075,240,125`，保存的初始框截图 `$E/tracking-initial.png` SHA-256 `e348eff93a1ccb09ca6c9c366f7c8dbe80d85d172d3a2bad85684bb3ec2a33be`。120 跟踪帧中 TRACKING 52、UNCERTAIN 10、LOCAL_RECOVERY 24、ASSISTED_RECOVERY 35（计数含初始化行）；未触发 HOLD 或 coast probe。此视频无真值，以上仅为定性状态/控制观察，不是精度、身份、恢复率或阈值校准证据。

**待处理：UNKNOWN 下仍提交恢复 detector 请求。** 路口视频初始化 frame 0 为 UNKNOWN/NO_MATCH、`detector_recovery_enabled=0`；但 ASSISTED_RECOVERY 的 frame 86、101、117 有 `detector_requested=1`、worker `submitted`，87、102、118 分别完成，过滤候选数 8、9、10，批状态 `ACCEPTED`。这不是初始化诊断：源码 `long_term_controller.cpp` 的 `begin_frame` 请求条件未检查 target class，`long_term_runtime.cpp` 随后实际提交；`ingest_detector_batch` 对 UNKNOWN 候选拒绝入队。全 121 行 `detector_candidates_queued=0`、candidate ID 为空、locked=0、hypothesis_source=MOTION；上述提交与完成帧 `control_valid=0`，未观察到 detector 候选激活或直接控制。该请求仍消耗资源，且与 UNKNOWN 关闭 detector 辅助恢复的要求存在冲突，须作为阻塞项由源码 owner 修复或明确裁决；不将本轮记为功能全通过。

`girl.{avi,csv,json,log}` SHA-256 依次 `aee8ea7743978bb3495f497e2efca8f19d2d5e22f6882f8c3e1dffe25cf38b4a`、`14e65e17d80c075cf47dd37f6d4f72ba19c16528fb6d58846f39bdded8636afc`、`6caaf6f4a520ad36254dada95f68d439126798e79d6f274cf1257b7431be18f7`、`48aa466ef62e62f4bf43b74decacc1738f5cfea93d392692ea9c8b8d049e62fa`；`tracking.{avi,csv,json,log}` 依次 `1d1d369364bd623d8a98d393f19108103c8356bee42dc3ae44859b0504604d93`、`3be6ea849dca076bf9804426eaa0f00df2de23ea890bd3220bb8307b765f138a`、`225886a840ae974adcb6e4ab3a9a0b5b8ed671b81619ff2f46aac4db1c6236c0`、`a438364e1f96a736679bccb0bd1d11f67d5f3ad1547617371f3f6228cdd0f879`。

### Handoff

保留 `functional_pass_promotion_held` 的历史状态，不把本次源码候选标为功能全通过；`development_board_validation.latest_handoff.available=false`。请求协调方采纳本证据并安排 UNKNOWN 时请求门控问题的修复与板端复测。正例 detector 确立、detector 确认重捕、成功 early probe recovery、长稳及其他 promotion 门仍未通过；未执行真实吊舱测试。

- 任务状态：`functional_pass_promotion_held`
- 最近核对：`2026-10-26T19:53:37+08:00`
- 核对基线：`main@bb297504070b3dfcf2fd4b4da05ebb1503e309db`
- 任务工作树：`/home/u20/single-object-tracking/.worktrees/debug-runtime-config`
- 任务分支：`codex/debug-runtime-config`
- 责任边界：在交叉编译之前，使用具备完整编译环境的真实 RK3566 开发板，对交互式 Debug 版本执行原生编译、调试、加载/推理、集成视频、控制安全、时延、资源和稳定性测试。

本任务是开发阶段的板端验证，不消费交叉编译生产产物。交叉编译只在本任务冻结并移交最新可用结果后进行；真实吊舱测试由人完成。

## 2026-10-22 首次协调复核

- 主工作树 `/home/u20/single-object-tracking` 在 `main` 上，`HEAD` 正好是协调基线
  `29b083e4e6ced2fd46eb7f9ecf821287b4cc401a`；首次只读检查时没有已暂存、
  未暂存或未跟踪修改，`HEAD...29b083e` 的左右提交数为 `0/0`，因此没有执行
  merge、reset、覆盖或文件搬运。
- 本任务从该基线创建独立工作树和分支。历史控制器源提交
  `1552b7d14f67006768757dff039327253e3a60cb` 是当前基线祖先；交互式 Debug
  分支当前停在 `b98b1335c3d263f542fd52656c7d7c4d58b2bb77`，但它不是协调基线祖先，
  尚不能把两者合称为一份已集成、可移交的源码身份。
- `integration-contract.yaml` 锁定的 VisDrone9 RKNN/manifest/labels 大文件不在
  Git 工作树中，符合“大文件不入库”规则；本轮只能核对契约和历史报告中的哈希，
  不能把路径记录当作当前文件哈希复验。
- 尝试使用当前约定命令
  `ssh -p 10022 cat@127.0.0.1 'printf connected'` 连接开发板，实际返回
  `ssh: connect to host 127.0.0.1 port 10022: Connection refused`。用户随后恢复
  隧道；复测时 TCP 已成功连接到 `127.0.0.1:10022`，但 12 秒内未收到 SSH
  banner，返回 `Connection timed out during banner exchange`。这证明 Linux 端
  reverse-forward listener 已存在，但不能证明 Mac 到 `192.168.103.172:22` 的
  后半段可达。因此本轮仍未重新读取板端 Git、二进制、模型、运行时或保留工件，
  也没有生成新的板端 PASS。
- 对协调基线执行 RKNN-independent 主机回归：临时目录配置
  `cmake -S rk3566/nanotrack_v3 -B <tmp> -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF`，
  随后构建并运行 `ctest --test-dir <tmp> --output-on-failure`，结果为 `12/12`
  通过。这仅证明当前主线的主机逻辑基线可构建和测试，不是开发板证据。

本轮实际任务状态仍为 `functional_pass_promotion_held`。此前板端功能和控制安全
证据继续作为历史保留证据，但在反向隧道恢复并重新核对工件身份前，不升级为
“当前已复验”；`development_board_validation.latest_handoff.available=false` 仍然正确。

## 2026-10-22 隧道恢复后的当前板端复核

- 用户接通 Mac 与开发板的网线后，`ssh -p 10022 cat@127.0.0.1` 恢复。板端为
  LubanCat RK3566、AArch64、Debian 10、Linux `4.19.232`。原生工具链为 GCC
  `8.3.0`、CMake `3.13.4`、Git `2.20.1`；二进制动态链接
  `/home/cat/ThirdParty/OpenCV4100/lib/libopencv_world.so.410` 和 RKNN 2.3.2
  `librknnrt.so`，`ldd` 未报告缺失依赖。
- 板端仓库 `/home/cat/Project/SOT/nanotrack_v3_longterm` 位于
  `codex/rk3566-interactive-debug@d9f200ce26ade0f7664a9c853230f277c28da47b`，
  tracked tree 干净，仅有用户未跟踪文件 `cmd.txt`，本轮未修改。该板端仓库使用
  独立 Git 历史；逐文件对象比较表明，除板端额外的根 `.gitignore` 外，其 tracked
  源码与共享仓库 `b98b1335c3d263f542fd52656c7d7c4d58b2bb77:rk3566/nanotrack_v3`
  一致。
- 当前 Debug 二进制
  `/home/cat/Project/SOT/nanotrack_v3_longterm/build-interactive-debug-keyfix/nanotrack_v3`
  是 AArch64 PIE，SHA-256 为
  `c4436db4a7f2491483d5e66e4ce22a98feb710ae72dfe5203cb73793d043f8e9`。
  现有构建目录执行 `ctest --output-on-failure` 为 `13/13` 通过。
- 当前 NanoTrack 模型 SHA-256：template
  `38b60334e8e0f0999bfb7ff9e943090546461f9307682653685237e24b2a7709`、search
  `92125f1ef6254dade5786bb999faa5b9d9412ceb2b7987f6dbba82f959b7ef6a`、head
  `2065bfca503cca9197e67de1a754efad930dfed4d2fc73881c77d9e850dcb600`；
  tracker manifest 为
  `441e93f2092afe94bac663f27adb1fe99848357356efd67c7473b0b506f854b2`。
- VisDrone9 RKNN/manifest/labels 的板端 SHA-256 分别为
  `3698bc9bea1f43a4bac2826a09e03be0439fddfbc5c5604d8ed0f92b36881df0`、
  `05e6d8c7c210b7f9d85b8fb712aa00b014f310e46e85f171ce1158afc90ba63e`、
  `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`，
  与 `integration-contract.yaml` 完全一致；板端 manifest verifier 报告
  `target=rk3566 outputs=9 classes=9`。
- 使用 release 中样本 `0000330_00801_d_0000804.jpg` 执行当前 VisDrone9 person
  smoke：RKNN API `2.3.2 (429f97ae6b@2025-04-09T09:09:27)`、driver `0.8.2`，
  9 个输出张量与 manifest 一致；`raw=19`、person candidates `7`，preprocess /
  inference / postprocess / total 为 `26.833 / 177.154 / 23.459 / 227.446 ms`。
  输出位于 `/tmp/board-validation-detector-smoke.C97Y8A`，CSV SHA-256
  `499051ec07bbe03ecc6c7dbfa97f1df87c9882495fc52da9e0da51f72e4eee06`，
  标注图 SHA-256
  `3037104c175564fb411fab182381a255d9be9dc5aa54038c51de2d260821b7b9`；
  CSV 与历史 person-filter 证据一致。这是 `detector_function` 复核，不是精度或恢复验收。
- 用当前 Debug 二进制、上述 NanoTrack/VisDrone9 工件、class 0 和
  `girl_dance.mp4`（SHA-256
  `8090fcd5090adb62f21e79e799f0b0871d9c610d8cf8087b9fcdd3c7e9c2e9b9`）
  执行 3 tracked-frame 非交互烟测。结果 `mean_runtime_ms=33.154`、
  `mean_pipeline_ms=67.102`、`control_valid=true` 且无错误；4 帧 428x240 AVI、5 行
  CSV 和 metadata 保留在 `/tmp/board-validation-integrated-smoke.PFYuzG`，SHA-256
  分别为 `f0776f9ae6bf189e2595ce0d9c5ff03fcb8f5afddfd9a3eca16af3786c0bad51`、
  `17711c767032272324a3675cdbb0dae1f33a8b152491bf056301641b0a2f914a`、
  `938e3bcfa74ced584a8e907f7184b81237d3d3aa8d080be0b2bd9d63917e9737`。
  由于稳定
  跟踪期间 detector request/complete 为 `0/0`，此结果只证明 `model_load` 和短程
  `integrated_function`，不证明 detector 队列、重捕或长时稳定性。
- 板卡重启后 RTC 仍为 `2019-02-14`；首次登录时系统时间也为 2019，随后 `ntpd`
  已把系统时间纠正为 2026，且 `ntpq -pn` 有选中 peer，但 `timedatectl` 仍显示
  `System clock synchronized: no`、RTC 仍为 2019。任何原生增量构建前必须先确认
  系统时间已纠正，避免未来时间戳导致陈旧对象未重编译。

本轮板端实际验证命令的核心部分为：

```bash
cd /home/cat/Project/SOT/nanotrack_v3_longterm/build-interactive-debug-keyfix
ctest --output-on-failure

/home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/build/yolo_detector_smoke \
  --model /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/yolov8n-visdrone9-rk3566-fp16.rknn \
  --manifest /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/detector-manifest.json \
  --labels /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/visdrone9.txt \
  --image /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/samples/0000330_00801_d_0000804.jpg \
  --conf 0.25 --nms 0.45 --class-id 0 \
  --output-image /tmp/board-validation-detector-smoke.C97Y8A/person.jpg \
  --output-csv /tmp/board-validation-detector-smoke.C97Y8A/person.csv

/home/cat/Project/SOT/nanotrack_v3_longterm/build-interactive-debug-keyfix/nanotrack_v3 \
  --video /home/cat/Project/SOT/nanotrack_v3_longterm/data/girl_dance.mp4 \
  --bbox 244,161,74,70 \
  --model-dir /home/cat/Project/SOT/nanotrack_v3_longterm/models \
  --output-video /tmp/board-validation-integrated-smoke.PFYuzG/output.avi \
  --output-boxes /tmp/board-validation-integrated-smoke.PFYuzG/bboxes.csv \
  --max-frames 3 --output-size 428x240 --sensor visible \
  --controller-config /home/cat/Project/SOT/nanotrack_v3_longterm/config/visible-default.conf \
  --session-metadata /tmp/board-validation-integrated-smoke.PFYuzG/session-metadata.json \
  --detector-model /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/yolov8n-visdrone9-rk3566-fp16.rknn \
  --detector-manifest /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/detector-manifest.json \
  --detector-labels /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/visdrone9.txt \
  --detector-class-id 0 --detector-conf 0.25 --detector-nms 0.45
```

当前 live 复核确认了现有 Debug 产物的工件身份、模型加载、detector 单图功能、
短程集成功能和 13 项测试；没有改变 `functional_pass_promotion_held` 裁决，也没有
产生可供交叉编译消费的 latest handoff。

## 2026-10-23 Debug 运行配置板端验证

本轮只改造交互式 Debug 的启动参数入口，不修改跟踪、重捕、模板更新或控制授权
逻辑。经用户确认，采用独立的严格 `key=value` 运行配置和
`nanotrack_v3 --config FILE`，没有引入 YAML 依赖；原命令行继续兼容，显式 CLI
值覆盖配置值，配置中的相对路径以配置文件目录为基准。detector 仍只是候选来源，
不能直接产生 `control_valid=true`。

### 源码与板端身份

- 当前协调基线为
  `main@bb297504070b3dfcf2fd4b4da05ebb1503e309db`。独立分支
  `codex/debug-runtime-config` 在该基线上显式合并既有交互 Debug 候选
  `b98b1335c3d263f542fd52656c7d7c4d58b2bb77`；设计/计划提交为
  `f5cedeb2414473bb6bbf736b8026308688bcec24`，实现提交为
  `eb50d6788c477733697703d6b1e8b9f6e4800cd1`。
- 板端仓库使用独立历史。实现 patch SHA-256 为
  `863e2cd0fd37fd956d928faebea0ffbd3a8877b71f64f70b7951832560d7d4f4`；首次
  `git am --3way` 形成 `a45d563e7e18134400b66313b2db8d7c8f9f5f41` 时，三个新增文件因共享
  仓库子目录前缀被放入嵌套路径。未使用 reset；随后以可追溯提交
  `41aa1ffe703416ea442afa4582d5346d9475f254` 将它们移动到板端仓库根目录。
  最终 tracked 状态干净，仍只有既有用户文件 `cmd.txt` 未跟踪，本轮未修改。
- 共享提交子树与板端 `HEAD` 的 55 个构建相关条目（`CMakeLists.txt`、
  `config/`、`include/`、`src/`、C++ 测试、board schema 测试和 verifier）
  `git ls-tree` 指纹均为
  `9dc0ccb607672b0863f7805c1790db153c880ad2c68d95125cdfeb6ed8e840eb`。
  两个仓库的完整 tracked tree 不相同：板端独立历史不包含主线后增的训练文档、
  Python 训练工具和相应测试；因此这里只声明板端构建/测试输入等价，不声明完整
  仓库树等价。

### 配置、工件和运行环境

- 板端配置为
  `/home/cat/Project/SOT/nanotrack_v3_longterm/config/interactive-debug.conf`，
  SHA-256
  `9e57e3a7e8a13212b46a8b21daba08836c0ed41287f3cb76a73562b57e7551f1`。
  它启用交互模式、可见光 controller 和 VisDrone9 person candidate，不设置
  `bbox`，也不设置 video/CSV/metadata 输出路径；因此启动后暂停等待鼠标框选，
  调试输出默认不保存。
- 输入 `data/girl_dance.mp4` SHA-256 为
  `8090fcd5090adb62f21e79e799f0b0871d9c610d8cf8087b9fcdd3c7e9c2e9b9`。
  NanoTrack template/search/head/manifest SHA-256 依次为
  `38b60334e8e0f0999bfb7ff9e943090546461f9307682653685237e24b2a7709`、
  `92125f1ef6254dade5786bb999faa5b9d9412ceb2b7987f6dbba82f959b7ef6a`、
  `2065bfca503cca9197e67de1a754efad930dfed4d2fc73881c77d9e850dcb600`、
  `441e93f2092afe94bac663f27adb1fe99848357356efd67c7473b0b506f854b2`。
- VisDrone9 RKNN/manifest/labels SHA-256 依次为
  `3698bc9bea1f43a4bac2826a09e03be0439fddfbc5c5604d8ed0f92b36881df0`、
  `05e6d8c7c210b7f9d85b8fb712aa00b014f310e46e85f171ce1158afc90ba63e`、
  `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`，
  与公共契约一致；本配置使用候选不等于 canonical promotion。
- 本轮板端系统时间为 `2026-10-23T15:13:21+08:00` 且 NTP 同步为 yes；环境仍为
  LubanCat RK3566/AArch64、Linux `4.19.232`、GCC `8.3.0`、CMake `3.13.4`、
  Git `2.20.1`。后续重启后仍需重新检查 RTC/NTP。

### 原生编译与实测结果

首次新建的 `build-interactive-debug-config` 未显式设置 `OpenCV_DIR`，CMake 自动
选择系统 OpenCV 3.2；虽然编译和 13 项测试可通过，实际启动在 OpenCV 3.2 解析
detector JSON manifest 时失败。该目录是无效诊断产物，不能作为本轮结果。根因经
cache、`ldd` 和 manifest 首字节对照确认；manifest 本身以合法 JSON `{` 开头。

有效 clean build 使用另一个全新目录并显式绑定既有 OpenCV 4.10：

```bash
cmake -S /home/cat/Project/SOT/nanotrack_v3_longterm \
  -B /home/cat/Project/SOT/nanotrack_v3_longterm/build-interactive-debug-config-opencv4100 \
  -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=ON \
  -DOpenCV_DIR=/home/cat/ThirdParty/OpenCV4100/lib/cmake/opencv4 \
  -DRKNN_INCLUDE_DIR=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/include \
  -DRKNN_LIBRARY=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/aarch64/librknnrt.so
cmake --build /home/cat/Project/SOT/nanotrack_v3_longterm/build-interactive-debug-config-opencv4100 -- -j2
cd /home/cat/Project/SOT/nanotrack_v3_longterm/build-interactive-debug-config-opencv4100
ctest --output-on-failure
```

观察结果：原生编译成功，CTest `13/13` 通过；有效 AArch64 PIE 为
`/home/cat/Project/SOT/nanotrack_v3_longterm/build-interactive-debug-config-opencv4100/nanotrack_v3`，
SHA-256
`e469aa57a209c415f083a1149824d95b6dcd62c3381df1901f4c5a31acf7f33a`。
`ldd` 确认链接
`/home/cat/ThirdParty/OpenCV4100/lib/libopencv_world.so.410` 和 RKNN 2.3.2
`librknnrt.so`，未观察到缺失依赖。

实际简化启动烟测为：

```bash
cd /home/cat/Project/SOT/nanotrack_v3_longterm
timeout --signal=INT 15s env DISPLAY=:0 \
  ./build-interactive-debug-config-opencv4100/nanotrack_v3 \
  --config config/interactive-debug.conf
```

程序 15 秒内没有配置、路径、模型、manifest 或 GUI 启动错误，最终由 `timeout`
按预期结束并报告 `CONFIG_SMOKE_EXIT=124`。这证明实际板端二进制能够消费单一配置
参数并保持在交互运行状态；结合板端 `test_cli`，确认 shipped config 从任意 cwd
解析、默认无输出且等待鼠标 ROI。该有界启动没有人工框选、没有执行跟踪帧，也没有
证明 detector 推理、遮挡重捕、长时稳定性或跟踪质量。

### 本轮裁决与 Handoff

本轮层级为 `native_build + config_parse + interactive_startup` 功能通过，不改变总体
`functional_pass_promotion_held`。共享实现提交尚未合入 `main`，公共契约也尚未把
它锁定为下一轮 source candidate；因此
`development_board_validation.latest_handoff.available` 必须继续为 `false`，交叉编译
任务不得消费本结果。建议协调对话审查并集成
`eb50d6788c477733697703d6b1e8b9f6e4800cd1` 后，再决定是否将其设为下一轮板端候选。

## 2026-10-23 结构化 Debug 遥测板端验证

- 时间：`2026-10-23T22:39:00+08:00`。
- 共享分支/工作树：`codex/debug-runtime-config` /
  `/home/u20/single-object-tracking/.worktrees/debug-runtime-config`。
- 本功能基线为 `fb15ba310fa13a26741a22120bacf19b3a3d8aea`；纯遥测组件提交为
  `9289f07f528641ee014f8d1aa94e871acdea0ada`，交互主循环接入提交为
  `d868636f8332a58f7523ad0b941f258a7b07ffc5`。本进度文件的结果提交以
  `git log -1 --format=%H -- docs/progress/board-validation.md` 查询，避免在提交前虚构
  自身 commit ID。
- 共享源码到板端的 patch SHA-256 为
  `599fc21942fdef26319eb9d4f7538d62acf00999618b5c25cc73dee1d63b5dfa`。
  patch 只含 8 个 CMake/C++/测试文件，不含模型、视频、日志或构建产物。

### 板端隔离与源码身份

原板端 checkout `/home/cat/Project/SOT/nanotrack_v3_longterm` 在同步前已有用户状态：
`config/interactive-debug.conf` 将输入改为
`/home/cat/Project/SOT/nanotrack_v3_longterm/data/1.mp4`，另有未跟踪
`cmd.txt`。首次在该 checkout 应用源码并构建后，CTest 为 `13/14`，唯一失败是
`cli` 的 shipped-config 路径断言；根因就是上述配置修改，不是遥测代码。没有覆盖
或提交这两个用户文件；已精确反向应用本任务 patch，使原 checkout 恢复为仅这两项
用户状态。

正式验证改在板端独立工作树
`/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry`、分支
`codex/rk3566-debug-telemetry` 中进行，基线为
`41aa1ffe703416ea442afa4582d5346d9475f254`，板端结果提交为
`0f155ab7dcde33582534f1d87e61a3453167b825`，tracked 状态干净。该提交只增加
结构化终端遥测及其测试，不修改 `long_term_controller.cpp`、阈值、搜索计划、
模板决策或 detector 控制授权逻辑。

### 原生构建、测试和工件

板端系统时间已由 NTP 同步；环境为 LubanCat RK3566/AArch64、Linux
`4.19.232`、GCC `8.3.0`、CMake `3.13.4`、Git `2.20.1`。干净构建命令为：

```bash
cd /home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry
cmake -S . -B build-board \
  -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=ON \
  -DOpenCV_DIR=/home/cat/ThirdParty/OpenCV4100/lib/cmake/opencv4 \
  -DRKNN_INCLUDE_DIR=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/include \
  -DRKNN_LIBRARY=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/aarch64/librknnrt.so
cmake --build build-board -- -j2
(cd build-board && ctest --output-on-failure)
```

观察结果：原生编译成功，CTest `14/14` 通过，其中新增
`debug_telemetry` 测试覆盖首次状态、状态/控制变化、单步强制心跳、连续播放每
30 帧心跳、detector request/batch/error、候选 ID/区域缺失、同类干扰、锁定和
2/3 确认、重捕来源、模板动作及重新初始化 reset。有效 AArch64 PIE 为
`/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/build-board/nanotrack_v3`，
SHA-256 为
`11c70e81ac2beb6305c923bee5bbff61f0969d1d82e16548d9f7393b017f04dd`。
`ldd` 绑定 `/home/cat/ThirdParty/OpenCV4100/lib/libopencv_world.so.410` 与
`/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/aarch64/librknnrt.so`，没有
`not found`。

### 交互烟测

使用 canonical `config/interactive-debug.conf`，并通过显式 CLI 覆盖把大工件指向
原板端 checkout；没有把大文件复制到新工作树。核心启动参数为：

```bash
cd /home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry
timeout --signal=INT 35s env DISPLAY=:0 stdbuf -oL -eL \
  ./build-board/nanotrack_v3 --config config/interactive-debug.conf \
  --video /home/cat/Project/SOT/nanotrack_v3_longterm/data/girl_dance.mp4 \
  --bbox 244,161,74,70 \
  --model-dir /home/cat/Project/SOT/nanotrack_v3_longterm/models \
  --controller-config /home/cat/Project/SOT/nanotrack_v3_longterm/config/visible-default.conf \
  --detector-model /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/yolov8n-visdrone9-rk3566-fp16.rknn \
  --detector-manifest /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/detector-manifest.json \
  --detector-labels /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/visdrone9.txt \
  --max-frames 40
```

板端没有 `xdotool`，因此使用只存在 `/tmp` 的 ctypes/XTest 脚本向现有 X11
窗口依次发送 `R`、`Esc`、`N`、`Space`、`Space`、`Q`；未安装系统包。
脚本 SHA-256 为
`522a09537f16534e94bd44ceec1ae83ccfaccb89250fe05d2ae64eb245e283ec`。
日志 `/tmp/nanotrack-debug-telemetry-smoke.log` 共 47 行，SHA-256 为
`a4ced575095a1e4207dedeb22fab17ac09c36e525fc6b15aaa008f363661384d`。

实际观察到：

- 启动时打印视频、分辨率/FPS、模型目录、detector 开关/类别/模式、标定状态和
  `video=disabled boxes=disabled metadata=disabled`；调试默认仍不保存输出。
- `R -> Esc` 打印 `select_roi`、`roi_selection_started` 和
  `roi_selection_cancelled`；`N` 在第 1 帧立即打印完整 `frame_status`。
- 连续播放在第 30 帧打印周期心跳，并分别在第 30/33 帧打印
  `STAGE_RECENT`/`COMMIT_RECENT`；第 40 帧达到限制后由 `Q` 正常退出，退出原因为
  `user_quit`。
- 日志错误扫描为空；最终摘要为 40 tracked frames，mean runtime
  `31.909 ms`、mean pipeline `37.131 ms`。这是短程 Debug 烟测时序，不是正式
  性能基准或长稳资源结论。

本次沿用的 `girl_dance.mp4`、NanoTrack template/search/head/manifest 和
VisDrone9 detector RKNN/manifest/labels SHA-256 依次为
`8090fcd5090adb62f21e79e799f0b0871d9c610d8cf8087b9fcdd3c7e9c2e9b9`、
`38b60334e8e0f0999bfb7ff9e943090546461f9307682653685237e24b2a7709`、
`92125f1ef6254dade5786bb999faa5b9d9412ceb2b7987f6dbba82f959b7ef6a`、
`2065bfca503cca9197e67de1a754efad930dfed4d2fc73881c77d9e850dcb600`、
`441e93f2092afe94bac663f27adb1fe99848357356efd67c7473b0b506f854b2`、
`3698bc9bea1f43a4bac2826a09e03be0439fddfbc5c5604d8ed0f92b36881df0`、
`05e6d8c7c210b7f9d85b8fb712aa00b014f310e46e85f171ce1158afc90ba63e`、
`41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`。
RKNN API 为 `2.3.2`，driver 为 `0.8.2`，detector 实际加载 9 个输出。

### 裁决与 Handoff

本轮新增层级为 `native_build + interactive_debug_telemetry_smoke` 功能通过，且没有
改变 detector 候选不能直接产生 `control_valid=true` 的安全约束。稳定跟踪期间
没有触发 detector request、丢失或重捕，因此本结果不证明 detector 队列、同类
干扰、2/3 重捕、遮挡恢复、长时稳定、内存平台化或真实吊舱验收。

协调对话需要审查并集成 `codex/debug-runtime-config` 的最新结果提交，并在公共契约
中决定是否把它锁定为下一轮交互 Debug source candidate；这不是发布板端 latest
handoff。`development_board_validation.latest_handoff.available` 必须继续为
`false`，总体裁决仍为 `functional_pass_promotion_held`。

## 已观察证据

- VisDrone9 detector-only 验收：工件身份、manifest、模型加载、真实 tensor 布局、20 图推理、类别过滤和单进程资源检查为 `PASS`。
- 集成运行保留了人员、车辆、detector-disabled 和 1001 tracked-frame stability 证据。
- 所有保留运行的 detector 请求最终完成，最大队列 gap 不超过 1；未观察到 detector/RKNN error。
- 所有 `LOST` 行均保持 `control_valid=false`；detector-disabled 路径计数为零。
- 主要证据：
  `rk3566/nanotrack_v3/docs/visdrone9-detector-board-verification.md`。

## 开发板交付门结果

- 1001-frame stability 的 warm RSS 约为 111 MiB 且线程数稳定，但 VmHWM 在每个 30 秒采样点持续增长到 `160556 kB`。
- 人员运行有 detector hypothesis，但 confirmed reacquisition 为 0。
- 车辆运行的 4 次 reacquisition 都来自 motion，而不是 detector。
- 固定阈值下，VisDrone9 人员 usable hypotheses 少于 COCO 基线。
- 因此当前结论是“功能和控制安全通过，正式 promotion hold”；不是长时重捕产品验收通过。

## Blockers

- 协调基线尚未集成交互式 Debug 分支，也未在公共契约中锁定下一轮板端验证要使用
  的精确 source commit、NanoTrack 模型身份和 detector 选择。
- 板端 RTC 在重启后仍回退到 2019；NTP 纠正前执行增量构建存在时钟偏移风险。
- 隔离或修复视频 writer/buffer/allocator 内存路径，并证明 RSS 与 VmHWM 平台化。
- 用相同输入、时长、采样协议完成 COCO/VisDrone9 资源对比。
- 使用标注的遮挡/可靠重现数据证明 detector-confirmed recovery，而不是只统计候选或 motion reacquisition。
- 完成可见光/红外阈值标定和同类目标身份风险评估。

## Next action

1. 请协调对话先锁定下一轮板端验证的 source commit、NanoTrack 模型和 detector
   组合；在此之前不开始新的 promotion run，也不发布 latest handoff。
2. 锁定后先确认板端系统时间已由 NTP 纠正，再在完整原生环境中 clean-build
   交互式 Debug 版本，依次复测模型加载、推理、
   集成视频、控制安全、运行/端到端时延、同协议资源平台化及标注遮挡重捕。
3. 下一轮至少保留源 commit、板端原生编译二进制、模型/manifest/labels/输入哈希、
   完整命令、资源采样、verifier 结果和分层裁决。

## Handoff

任何 `PASS` 都必须注明层级：`model_load`、`detector_function`、`integrated_function`、`stability`、`recovery_quality` 或 `development_board_acceptance`。低层级通过不得升级表述为高层级通过。

当某一组合达到预定开发板门槛后，本任务应向协调对话移交精确 source commit、模型/manifest/labels 哈希、配置、板端验证证据和裁决。协调对话将其标记为最新 handoff，供交叉编译任务获取。此 handoff 不代表真实吊舱已通过测试。

当前需要协调对话更新公共契约的内容是“下一轮候选锁定”，不是发布通过的 handoff：
请明确批准的交互式 Debug source commit、NanoTrack 模型身份和 COCO/VisDrone9 detector
选择。`latest_handoff.available` 在新的真实板端验收达到门槛前必须继续保持 `false`。

## 2026-10-26 Debug 窗口 720p 初始尺寸验证

- 时间：`2026-10-26T19:53:37+08:00`。共享工作树/分支为
  `/home/u20/single-object-tracking/.worktrees/debug-runtime-config` /
  `codex/debug-runtime-config`，任务基线为
  `2f2f3fb57022d42855f20eca940eaafb3103720f`；结果提交可用
  `git log -1 --format=%H -- rk3566/nanotrack_v3/src/main.cpp` 查询。
- 变更仅在交互式 Debug 分支的 `cv::WINDOW_NORMAL` 窗口创建后调用
  `cv::resizeWindow(..., 1280, 720)`。输入帧、NanoTrack/detector 输入、框坐标、
  保存输出和生产 `ai_alg` 均未缩放或修改；窗口启动后仍允许用户调整大小。
- 主机侧先增加尺寸断言并观察到因常量不存在而编译失败，随后实现转绿；
  `test_interactive_control` 通过，RKNN-independent CTest 为 `14/14` 通过，
  `git diff --check` 通过。该主机构建目录不含 RKNN 可执行目标，因此完整程序由板端
  原生环境编译验证。
- 板端隔离工作树为
  `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry`，分支
  `codex/rk3566-debug-telemetry`，基线
  `0f155ab7dcde33582534f1d87e61a3453167b825`，结果提交
  `8802eabc4a56d3311de72e450d73967f218ab827`。系统时间为
  `2026-10-26T19:51:53+08:00` 且 NTP 已同步；`cmake --build build-board -- -j2`
  原生编译成功。
- 板端新二进制
  `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/build-board/nanotrack_v3`
  SHA-256 为
  `dbf1804ff2dda7a452d87149980a73dbd967e76744e7314f457a8192dc50fb71`。
  使用 `DISPLAY=:0`、`girl_dance.mp4`、既有 NanoTrack 模型和 VisDrone9 detector
  做有界启动后，`xwininfo -name "NanoTrack V3 RK3566 Debug"` 实测窗口内容区为
  `Width: 1280`、`Height: 720`；程序日志仍报告源视频和输出尺寸为 `856x480`，
  证明本次只改变窗口初始显示尺寸。启动日志和窗口查询分别保留在
  `/tmp/nanotrack-window-720p.log`、`/tmp/nanotrack-window-720p.xwininfo`，SHA-256
  分别为
  `e84814050a5698d4e0a896987c83a0c82e343cdc9260abb53ca70d0c3439403a`、
  `3cf58780fd121382d9e20b38ad63a537d7f9a9ef2ef4cf40fedaf81021e54c20`。
- 本次有界启动绑定的输入、NanoTrack template/search/head/manifest 和 VisDrone9
  detector RKNN/manifest/labels SHA-256 依次为
  `8090fcd5090adb62f21e79e799f0b0871d9c610d8cf8087b9fcdd3c7e9c2e9b9`、
  `38b60334e8e0f0999bfb7ff9e943090546461f9307682653685237e24b2a7709`、
  `92125f1ef6254dade5786bb999faa5b9d9412ceb2b7987f6dbba82f959b7ef6a`、
  `2065bfca503cca9197e67de1a754efad930dfed4d2fc73881c77d9e850dcb600`、
  `441e93f2092afe94bac663f27adb1fe99848357356efd67c7473b0b506f854b2`、
  `3698bc9bea1f43a4bac2826a09e03be0439fddfbc5c5604d8ed0f92b36881df0`、
  `05e6d8c7c210b7f9d85b8fb712aa00b014f310e46e85f171ce1158afc90ba63e`、
  `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`。
  二进制绑定 OpenCV 4.10 `libopencv_world.so.410`；启动日志实测 RKNN API
  `2.3.2 (429f97ae6b@2025-04-09T09:09:27)`、driver `0.8.2`。
- 实际启动、窗口查询和停止命令如下。程序由 `timeout` 设置 25 秒硬边界；查询到
  窗口后向本轮启动的进程发送 `SIGINT`，没有让其继续执行长时跟踪：

```bash
cd /home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry
timeout --signal=INT 25s env DISPLAY=:0 stdbuf -oL -eL \
  ./build-board/nanotrack_v3 --config config/interactive-debug.conf \
  --video /home/cat/Project/SOT/nanotrack_v3_longterm/data/girl_dance.mp4 \
  --bbox 244,161,74,70 \
  --model-dir /home/cat/Project/SOT/nanotrack_v3_longterm/models \
  --controller-config /home/cat/Project/SOT/nanotrack_v3_longterm/config/visible-default.conf \
  --detector-model /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/yolov8n-visdrone9-rk3566-fp16.rknn \
  --detector-manifest /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/detector-manifest.json \
  --detector-labels /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/visdrone9.txt \
  --max-frames 1 >/tmp/nanotrack-window-720p.log 2>&1 &
app_pid=$!
found=0
attempt=0
while [ "$attempt" -lt 15 ]; do
  if xwininfo -display :0 -name "NanoTrack V3 RK3566 Debug" \
      >/tmp/nanotrack-window-720p.xwininfo 2>/dev/null; then
    found=1
    break
  fi
  attempt=$((attempt + 1))
  sleep 1
done
test "$found" -eq 1
kill -INT "$app_pid"
wait "$app_pid" || true
```

- 板端 `interactive_control` 及其他 12 项测试通过；全量 CTest 为 `13/14`，唯一
  失败是 `cli` 对发行配置视频路径的断言。根因已由重复运行和差异核对确认：本轮
  开始前用户已把 `config/interactive-debug.conf` 的 `video` 从
  `../data/girl_dance.mp4` 改为 `/home/cat/Project/SOT/data/1.mp4`。该用户修改未被
  覆盖、暂存或提交，因此不能把本轮板端结果表述为全量 CTest 通过；它与窗口尺寸
  代码无重叠，窗口尺寸聚焦测试和实测均通过。

### 裁决与 Handoff

本轮仅形成 `native_build + interactive_window_geometry_smoke` 功能通过，不增加跟踪、
重捕、资源、长稳或真实吊舱证据，不改变 detector 不能直接产生
`control_valid=true` 的安全约束。协调对话可集成本次 Debug 功能提交和板端证据，
但无需修改 `integration-contract.yaml`；总体状态仍为
`functional_pass_promotion_held`，
`development_board_validation.latest_handoff.available` 继续为 `false`。

## 后续更新模板

```text
时间：<ISO 8601 + timezone>
板卡/运行时：<board, OS, RKNN API, driver, OpenCV>
集成身份：<source commit, executable/model/manifest/labels/input SHA-256>
命令：<exact board command>
结果：<frames, request/complete/error, reacquisition source, safety>
性能边界：<runtime vs end-to-end>
资源：<RSS, VmHWM, threads, temperature, duration>
verifier：<command -> observed result>
裁决/阻塞：<layered pass/fail/risk>
Handoff：<requested contract change or none>
```

## 2026-10-23 Debug 日志字段参考文档

- 时间：`2026-10-23T23:33:06+08:00`。
- 分支/工作树：`codex/debug-runtime-config` /
  `/home/u20/single-object-tracking/.worktrees/debug-runtime-config`。
- 基线：`e91af4c7d4943cd3ed8e4aba7d9d644cf891b4d2`；结果提交可用
  `git log -1 --format=%H -- RK3566_NANOTRACK_LONGTERM_CODE_FLOW.zh-CN.md`
  查询，避免在提交前记录不存在的 commit ID。
- 变更：在 `RK3566_NANOTRACK_LONGTERM_CODE_FLOW.zh-CN.md` 新增交互式 Debug
  日志章节，覆盖 `main.cpp` 和 `debug_telemetry.cpp` 当前输出的全部 20 种
  `DEBUG_EVENT`，说明触发条件、字段含义、源结构、枚举、打印节拍和排障顺序。
- 关键边界：明确这些日志只属于独立交互式 Debug 程序，不属于 ViewLink
  `ai_alg` 生产 ABI；再次明确 detector batch、候选框、可靠质量或候选锁定均不能
  直接产生 `control_valid=true`，必须经过 NanoTrack 验证和多帧确认。
- 一致性检查：从 `src/main.cpp` 的 `DEBUG_EVENT event=` 和
  `src/debug_telemetry.cpp` 的 `event_stream()` 提取事件名，与文档反查，观察结果为
  `documented_events=20 required_fields=17`；`git diff --check` 通过；现有主机
  CTest 回归仍为 `14/14` 通过。独立审查后又按源码修正了 2/3 确认示例、候选 ID
  重置范围、`FULL_OBJECT` 探针数量和选项结构类型名；针对这四点的 6 项文档断言
  由失败转为通过。
- 证据边界：本轮只更新说明文档，没有修改或重新编译程序，没有执行新的开发板、
  重捕、资源、长稳或吊舱测试；此前板端二进制、模型身份和验收结论均不改变。

### Handoff

协调对话只需集成本次文档提交；不需要修改 `integration-contract.yaml`，不改变
`functional_pass_promotion_held` 和
`development_board_validation.latest_handoff.available=false`。

## 2026-10-26 detector 位置硬门与 Debug 分色框简单板端验证

- 时间：`2026-10-26T21:25:05+08:00`。共享工作树/分支为
  `/home/u20/single-object-tracking/.worktrees/debug-runtime-config` /
  `codex/debug-runtime-config`，实现基线为
  `7d2fad2bbe4ebe67efe2f67aee492d755dbb3bf6`；实现提交为
  `c07f85080e1af02b53e9fa77f2b8bb0a5503b60e`、
  `5055e75af57c2a99e1d92a4eff13440e04f90759`、`1451537`、`02aef3c`。
  板端独立工作树/分支为
  `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry` /
  `codex/rk3566-debug-telemetry`，结果提交为
  `d845f056d41d5b1ab108b72375dbde59aab61f62`。
- 同步前板端已有用户修改 `config/interactive-debug.conf`（视频路径为
  `/home/cat/Project/SOT/data/1.mp4`）和未跟踪 `log.txt`；两者均未覆盖、未暂存、
  未提交。板端系统时间与 RTC 已正确到 2026，系统报告时钟已同步。
- 新增严格位置门：候选中心到本帧运动预测中心的距离除以画面对角线不得超过
  `detector_candidate_max_center_ratio=0.15`；`ASSISTED_RECOVERY` 与 `LOST`
  不放宽，通过者按距离升序、置信度降序排序。新增 `POSITION_REJECTED`、位置通过/
  拒绝计数和 schema 6 字段，且 detector 仍不能直接产生 `control_valid=true`。
- 交互窗口区分青色 motion、紫色 detector candidate、橙色 detector probe、蓝色
  anchor、黄色 recent、绿色 `ACCEPTED CTRL` 与红色 `REJECTED NO_CTRL`。第一次
  板端截图发现动态标签遮挡，随后通过共享提交 `02aef3c` 改为右侧固定纵向标签并
  重新原生链接。非交互输出视频的原单框渲染未改变。

主机 clean build 命令：

```bash
cmake -S rk3566/nanotrack_v3 -B /tmp/nanotrack-position-gate-clean-build \
  -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF
cmake --build /tmp/nanotrack-position-gate-clean-build -- -j2
ctest --test-dir /tmp/nanotrack-position-gate-clean-build --output-on-failure
```

结果 `15/15` 通过。新增 `interactive_debug_overlay` 测试覆盖六类框的绘制顺序、
`FULL_OBJECT` 候选/probe 同坐标仍保留不同标签，以及未确认假设不得生成
`ACCEPTED CTRL`。

板端为 LubanCat RK3566/AArch64、Linux `4.19.232`、GCC `8.3.0`、CMake
`3.13.4`、OpenCV 4.10、RKNN API `2.3.2` / driver `0.8.2`。原生命令：

```bash
cd /home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry
cmake -S . -B build-board -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=ON
cmake --build build-board -- -j2
cd build-board
ctest --output-on-failure
ctest --output-on-failure -E '^cli$'
```

完整 RKNN/OpenCV 可执行程序构建成功。全量 CTest 为 `14/15`；唯一失败的 `cli`
读取了必须保留的用户绝对视频路径覆盖，导致 shipped-profile 相对路径断言失败。
没有为凑通过而覆盖用户文件。排除这项已解释的本地配置差异，其余板端测试
`14/14` 通过；干净共享工作树的同一 `cli` 已包含在主机 `15/15` 中。

板端有界交互命令：

```bash
DISPLAY=:0 ./build-board/nanotrack_v3 \
  --config config/interactive-debug.conf \
  --video /home/cat/Project/SOT/nanotrack_v3_longterm/data/girl_dance.mp4 \
  --bbox 244,161,74,70 --max-frames 200
```

通过 X11 发送一次空格开始播放，帧 200 自动暂停后截图，再发送 `Q` 正常退出。日志
在帧 129 进入 `ASSISTED_RECOVERY`，帧 189 进入 `LOST`；帧 176 实际记录：

```text
DEBUG_EVENT event=detector_batch frame=176 source_frame=174 age_ms=67 raw=1 filtered=1 position_valid=0 position_rejected=1 status=POSITION_REJECTED total_ms=196.923
```

该候选没有后续 `detector_verify` 或 `reacquired`，不可靠状态始终
`control_valid=0`。帧 200 截图显示青色 `MOTION PRED`、蓝色 `NT ANCHOR`、黄色
`NT RECENT` 和红色 `REJECTED NO_CTRL`，标签不再互相遮挡；被位置门拒绝的候选
没有伪装成活动假设或绿色控制框。由于本次真实候选被硬门拒绝，截图不包含紫色/
橙色实例；它们及 `FULL_OBJECT` 重合情况由板端通过的纯数据 overlay 测试覆盖，
不能表述为七种颜色均完成了人工视觉验收。

| 工件 | 路径 | SHA-256 |
|---|---|---|
| Debug 可执行程序 | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/build-board/nanotrack_v3` | `30855451e2e7050ad208e70fb459e43cf61e459b962cfba8ac680111514c13ac` |
| template RKNN | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/models/template_backbone.rknn` | `38b60334e8e0f0999bfb7ff9e943090546461f9307682653685237e24b2a7709` |
| search RKNN | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/models/search_backbone.rknn` | `92125f1ef6254dade5786bb999faa5b9d9412ceb2b7987f6dbba82f959b7ef6a` |
| head RKNN | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/models/head.rknn` | `2065bfca503cca9197e67de1a754efad930dfed4d2fc73881c77d9e850dcb600` |
| tracker manifest | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/models/manifest.json` | `441e93f2092afe94bac663f27adb1fe99848357356efd67c7473b0b506f854b2` |
| VisDrone9 RKNN | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/yolov8n-visdrone9-rk3566-fp16.rknn` | `3698bc9bea1f43a4bac2826a09e03be0439fddfbc5c5604d8ed0f92b36881df0` |
| detector manifest | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/detector-manifest.json` | `05e6d8c7c210b7f9d85b8fb712aa00b014f310e46e85f171ce1158afc90ba63e` |
| VisDrone9 labels | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/visdrone9.txt` | `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992` |
| 输入视频 | `/home/cat/Project/SOT/nanotrack_v3_longterm/data/girl_dance.mp4` | `8090fcd5090adb62f21e79e799f0b0871d9c610d8cf8087b9fcdd3c7e9c2e9b9` |
| 帧 200 截图 | `/tmp/nanotrack-position-gate-debug-v2.png` | `53c8ace17b620759536e2f42a11b2b76b488073aca04684c36d60ebaad709efe` |
| 200 帧 Debug 日志 | `/tmp/nanotrack-position-gate-debug-v2.log` | `f70331b9f028b2ad361cc0dcd9debac6c3d32fe527f270f8fa466d9329e9805d` |

本轮只达到 `integrated_function_short_smoke` 与 Debug 可观测性验证，不是长时重捕
质量、阈值标定、资源平台化、完整七色人工视觉验收、真实吊舱或开发板发布门通过。
任务状态继续为 `functional_pass_promotion_held`，`latest_handoff.available=false`。

### Handoff

请协调对话在集成本任务提交后，把下一轮共享源码候选更新到该集成结果，并继续保持
detector 不能直接授权控制、VisDrone9 promotion hold 和 latest handoff 不可用；
是否把严格 `0.15` Debug 策略锁定为下一轮板端候选仍由协调者决定。

## 2026-10-27 独立审查修正：取消位置门前截断

- 时间：`2026-10-27T00:51:25+08:00`；分支/工作树仍为
  `codex/debug-runtime-config` /
  `/home/u20/single-object-tracking/.worktrees/debug-runtime-config`，审查起点为
  `59187142454acf9773423e42c8d829e3ba24a348`。
- 独立审查发现实际 RKNN detector 链路此前会在 YOLO 后处理中先按置信度截断为
  `detector_max_candidates=3`，三个远处高置信框可能在控制器位置门之前挤掉近处
  候选。这不满足“位置过滤、距离排序之后再截断”的完整顺序。
- 修正后 detector 请求允许后处理保留全部经过置信度过滤和 NMS 的候选，最终数量
  限制只在控制器完成 `0.15` 位置硬门和距离优先排序后执行。新增控制器请求与真实
  YOLO 后处理回归，覆盖第四个较低置信候选不会被上游提前丢弃。
- 同一审查指出 `FULL_OBJECT` 下候选框与 probe 框重合时，等线宽会使后画的橙色
  probe 覆盖紫色 candidate。现改为紫色 3 像素粗框、橙色 1 像素细框，并由纯数据
  overlay 测试锁定线宽关系。
- TDD 证据：新增断言先观察到控制器请求仍为 `3` 而失败、overlay 缺少
  `line_thickness` 而编译失败；实现后
  `long_term_controller`、`yolov8_postprocess`、`interactive_debug_overlay` 三项
  聚焦测试 `3/3` 通过。修正共享提交为
  `a6c23be`；主机重新构建后完整 CTest `15/15` 通过。
- 板端在保留用户修改 `config/interactive-debug.conf` 和未跟踪 `log.txt` 的前提下，
  应用同一源代码修正并原生重编译完整 RKNN/OpenCV 程序；板端独立提交为
  `5e620b1`。命令
  `cmake --build build-board -- -j2` 成功，随后
  `ctest --output-on-failure -E '^cli$'` 为 `14/14` 通过。这里排除的 `cli` 仍仅因
  已记录的用户视频路径覆盖，不把结果表述为板端全量 `15/15`。
- 新 Debug 可执行程序路径为
  `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/build-board/nanotrack_v3`，
  SHA-256 为
  `2b6a3836fb2ff46b38fa21a71a4c1610cab4914bef3f2ae101568a28531c229c`。
  本次复测是原生编译和测试级验证，没有再次运行视频，也没有新增重捕性能或人工
  七色视觉验收证据。
- 安全边界不变：detector 候选仍只是假设，不能直接产生 `control_valid=true`；
  NanoTrack 双模板、几何门和 2/3 确认规则均未绕过。任务状态仍为
  `functional_pass_promotion_held`，`latest_handoff.available=false`。

## 2026-10-28 遮挡等待状态与 Detector 全候选显示设计

- 时间：`2026-10-28T18:51:42+08:00`。
- 分支/工作树：`codex/debug-runtime-config` /
  `/home/u20/single-object-tracking/.worktrees/debug-runtime-config`；设计基线为
  `afc5f4942a6020b50058ecccd349cf1dc87288c2`。
- 与用户确认采用独立 `OCCLUSION_HOLD` 状态。默认触发条件为连续 2 帧非可靠且
  宽或高相对最近可靠尺度中位数缩小到 `0.70` 以下；HOLD 帧数通过
  `occlusion_hold_frames` 配置，默认 12。
- 触发帧仍执行 NanoTrack；随后完整 N 帧不运行 tracker、不发起 detector，只以
  冻结宽高的 motion 框预测中心且保持 `control_valid=false`。同一次 loss episode
  只允许触发一次，仅在 `TRACKING/UNCERTAIN/LOCAL_RECOVERY` 早期阶段允许触发。
- Detector batch 到达时计划显示全部经过类别阈值和 NMS 的候选，并区分位置门通过、
  位置拒绝、非法、最终入队和活动候选；所有原始 detector 框仍不能直接授权控制。
- 设计文档：
  `docs/superpowers/specs/2026-10-28-occlusion-hold-detector-debug-design.md`。
  独立实现者审查已用于消除 HOLD 首尾帧、重复触发、晚到 detector batch 和现有
  motion/detector 重捕规则边界的歧义。
- 用户批准设计后形成实施计划：
  `docs/superpowers/plans/2026-10-28-occlusion-hold-detector-debug.md`。计划按配置/类型、
  motion 尺度冻结、控制器状态、runtime 与 detector generation、全候选可视化/日志、
  主机与开发板验证六个可独立审查的任务执行，全部要求先写失败测试。
- 本次只形成设计，没有修改程序、编译开发板、运行视频或产生新的模型/二进制哈希。
  当前状态继续为 `functional_pass_promotion_held`，
  `development_board_validation.latest_handoff.available=false`。

### Handoff

协调对话暂不需要修改公共契约；待实现、主机测试和真实 RK3566 简单验证形成提交后，
再请求集成新的 Debug 源码候选。Detector 直接控制禁令和人工吊舱边界保持不变。

## 2026-10-28 OCCLUSION_HOLD 整体审查与板前主机门

- 时间：`2026-10-28T23:27:15+08:00`；工作树/分支为
  `/home/u20/single-object-tracking/.worktrees/debug-runtime-config` /
  `codex/debug-runtime-config`，审查和修正基线为
  `6e533c2785b1389c3613be38788468554f4133e4`。本节与相关测试、README
  修正同一提交，结果提交可用
  `git log -1 --format=%H -- docs/progress/board-validation.md` 查询。
- 整体功能审查范围为 `4f1a046..6e533c2`；审查门结论为没有未解决的
  Critical/Important 项。三个测试覆盖类 Minor 与三个 README 文案类 Minor 已在
  板端同步前处理，未因审查修改生产控制逻辑。
- 新增直接回归覆盖：HOLD 退出而尚未重捕时运动预测宽高仍保持冻结；活动 detector
  候选正在 NanoTrack 验证时不能进入 HOLD；HOLD 触发会清除在触发帧到达、通过位置门
  的晚到 detector 候选。此提交当时关于“清除旧 motion 确认”的公开行为断言并不能
  单独证明清理发生，因为固定 2/3 确认窗口会在 HOLD 结束前自然淘汰旧确认；该证据缺口
  由下方 `2026-10-28 HOLD 确认清理测试证据修正` 补齐。
- README 同步把控制无效状态列表补入 `OCCLUSION_HOLD`，把遗留
  `schema-v5` 改为当前 `schema-v7`，并收窄颜色说明：只有无拒绝原因的合法原始假设为
  紫色，位置/容量/stale/frame/HOLD 拒绝均为灰色，非法几何只显示深红文字。
- TDD/聚焦证据：新增候选清理断言首次运行时发现测试错误地读取触发帧末尾队列为
  `1`，实际设计语义是候选先经批次接收和位置门、随后由 HOLD 立即作废并把最终队列
  清为 `0`；断言改为同时验证 `ACCEPTED` 批次、位置通过计数、
  `HOLD_INVALIDATED` 诊断和最终零队列。随后
  `ctest -R '^(long_term_controller|long_term_replay)$'` 为 `2/2` 通过。
- 主机命令：

  ```bash
  cmake -S rk3566/nanotrack_v3 \
    -B /tmp/nanotrack-occlusion-hold-final-build \
    -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF
  cmake --build /tmp/nanotrack-occlusion-hold-final-build -- -j2
  ctest --test-dir /tmp/nanotrack-occlusion-hold-final-build --output-on-failure
  git diff --check 4f1a046..HEAD
  git diff --check
  ```

  观察结果：配置和完整构建成功，完整主机 CTest 为 `15/15` 通过、`0` 失败，
  总测试时间 `1.13 sec`；两项 whitespace 检查均无输出。
- 证据边界：本节仅证明 x86_64 主机、`ENABLE_RKNN_RUNTIME=OFF` 的构建/测试及
  源码审查状态；尚未连接 RK3566、未进行原生 RKNN 构建、模型加载、
  `girl_dance` 流程观察、二进制/模型/视频哈希或资源测量。不得把本节表述为板端验证、
  重捕性能、真实吊舱或发布门通过。
- 安全边界保持不变：detector 原始候选、HOLD motion 框和 WEAK 观测都不能直接产生
  `control_valid=true`；`latest_handoff.available=false`。板端仅允许同步本节对应的精确
  已提交源码，必须保留板端用户配置、日志、模型、视频和构建目录。

### Handoff

当前不请求协调对话修改 `STATUS.md` 或 `integration-contract.yaml`。完成真实 RK3566
原生构建、CTest 和有界 `girl_dance` 观察后，再在本文件追加板端工件身份、观察项、
未观察项和最终 Debug 分支提交。

## 2026-10-28 OCCLUSION_HOLD RK3566 原生构建与有界视频观察

- 时间：`2026-10-28T23:35:55+08:00`。共享源码提交为
  `578613dbe11a67c4241363e02e6554755258c178`，工作树/分支为
  `/home/u20/single-object-tracking/.worktrees/debug-runtime-config` /
  `codex/debug-runtime-config`。板端为 `lubancat`、Linux `4.19.232`，源码工作树/分支为
  `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry` /
  `codex/rk3566-debug-telemetry`，同步前提交 `5e620b1`，同步后的审计提交为
  `7ba0da0e0359c1ead4bd00656366a4cfbd463d69`。
- 同步前板端仍只有用户修改的 `config/interactive-debug.conf` 和未跟踪 `log.txt`；前者
  把视频改为 `/home/cat/Project/SOT/data/1.mp4`。二者均未覆盖、暂存或提交。系统时间为
  `2026-10-28T23:28:19+08:00`，时区 `Asia/Shanghai`，时钟已同步；根分区可用约
  `15G`。
- 板端源码仓库没有配置远程，且其仓库根对应共享仓库的 `rk3566/nanotrack_v3` 子目录。
  因此使用以下可审计同步方式：从两个精确共享提交生成相对路径补丁，传到板端 `/tmp`，
  校验 SHA-256，先执行 `git apply --check`，再应用并只暂存补丁列出的 31 个文件：

  ```bash
  git diff --relative=rk3566/nanotrack_v3 --binary \
    a6c23be..578613d -- rk3566/nanotrack_v3 \
    > /tmp/nanotrack-occlusion-hold-578613d.patch
  sha256sum /tmp/nanotrack-occlusion-hold-578613d.patch
  git apply --check /tmp/nanotrack-occlusion-hold-578613d.patch
  git apply /tmp/nanotrack-occlusion-hold-578613d.patch
  ```

  主机和板端补丁 SHA-256 均为
  `a481f5a69700bcb15a0935f626d009734ce27f59ce4627f6a6158e1cdb79b75d`；
  板端应用检查成功。提交后 `git status --short` 仍只显示上述用户配置和 `log.txt`。
- 板端工具链为 GCC `8.3.0`、CMake `3.13.4`、OpenCV `4.10.0`。首次按最简命令配置
  失败，明确提示必须设置 `RKNN_INCLUDE_DIR` 和 `RKNN_LIBRARY`；未改源码。随后复用
  板端既有验证路径，在新的独立构建目录完成原生配置和全量链接：

  ```bash
  cmake -S . -B build-board-occlusion-hold-578613d \
    -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=ON \
    -DOpenCV_DIR=/home/cat/ThirdParty/OpenCV4100/lib/cmake/opencv4 \
    -DRKNN_INCLUDE_DIR=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/include \
    -DRKNN_LIBRARY=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/aarch64/librknnrt.so
  cmake --build build-board-occlusion-hold-578613d -- -j2
  cd build-board-occlusion-hold-578613d
  ctest --output-on-failure
  ctest --output-on-failure -E '^cli$'
  ```

  构建成功。完整 CTest 为 `14/15`，唯一失败仍是用户绝对视频路径导致的既有 `cli`
  shipped-profile 断言；没有覆盖用户配置。排除该已解释差异后其余 `14/14` 全部通过，
  包括新增 HOLD/controller/runtime/overlay/telemetry/schema-7 回归。
- 有界板端命令：

  ```bash
  DISPLAY=:0 ./build-board-occlusion-hold-578613d/nanotrack_v3 \
    --config config/interactive-debug.conf \
    --video /home/cat/Project/SOT/nanotrack_v3_longterm/data/girl_dance.mp4 \
    --bbox 244,161,74,70 --max-frames 400
  ```

  通过本机 X11 向精确窗口发送 Space 开始连续播放，处理 307 帧后保留窗口 XWD 并发送
  Q 正常退出；因此这是 307 帧有界观察，不宣称达到配置上限 400。日志记录 RKNN API
  `2.3.2 (429f97ae6b@2025-04-09T09:09:27)`、driver `0.8.2`，最终摘要为
  `tracked_frames=307`、`mean_tracker_ms=45.596`、`mean_runtime_ms=45.846`、
  `runtime_fps=21.812`、`mean_pipeline_ms=50.029`、`end_to_end_fps=19.989`；这是单次
  Debug 观察值，不是性能门通过。
- 本次 `girl_dance` 自然流程没有触发 `occlusion_hold_enter/tick/exit`，三类事件计数均
  为 `0`。因此“触发帧仍有 anchor/recent、随后恰好 12 帧跳过推理、冻结尺度、
  `control_valid=0`、之后恢复搜索”在真实视频中均标记为**未观察到**，不使用人工伪造
  或单元测试代替板端视频证据。对应状态机边界仅有板端通过的自动化测试证据。
- Detector 在该 307 帧运行中完成 `12` 个 batch，记录 `7` 条逐候选事件。例如帧 237
  的 batch 对 `source_frame=234`、`age_ms=100` 的两个 filtered 候选逐一记录 bbox、
  class/confidence、center ratio、`eligible=1`、`position_pass=0`、`queued=0` 和
  `POSITION_REJECTED`；随后 batch 汇总为 `raw=10 filtered=2 position_rejected=2`。
  运行中没有 `reacquired` 事件；候选出现后的周期状态均保持
  `ASSISTED_RECOVERY/LOST`、`control_valid=0`。这证明本次返回的候选未直接授权控制，
  但由于所有候选均被位置门拒绝，未观察到 PASS/queued/active-probe 实例，也未在候选
  完成帧人工截图验证全部颜色；这些显示分支只有板端通过的纯数据 overlay 测试证据。

| 工件 | 板端绝对路径 | SHA-256 |
|---|---|---|
| Debug 可执行程序 | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/build-board-occlusion-hold-578613d/nanotrack_v3` | `62fe954c00fd4cce96e55db62a1f1687e037b53c5045070eef333dc02cf2e3f2` |
| template RKNN | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/models/template_backbone.rknn` | `38b60334e8e0f0999bfb7ff9e943090546461f9307682653685237e24b2a7709` |
| search RKNN | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/models/search_backbone.rknn` | `92125f1ef6254dade5786bb999faa5b9d9412ceb2b7987f6dbba82f959b7ef6a` |
| head RKNN | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/models/head.rknn` | `2065bfca503cca9197e67de1a754efad930dfed4d2fc73881c77d9e850dcb600` |
| tracker manifest | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/models/manifest.json` | `441e93f2092afe94bac663f27adb1fe99848357356efd67c7473b0b506f854b2` |
| VisDrone9 RKNN | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/yolov8n-visdrone9-rk3566-fp16.rknn` | `3698bc9bea1f43a4bac2826a09e03be0439fddfbc5c5604d8ed0f92b36881df0` |
| detector manifest | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/detector-manifest.json` | `05e6d8c7c210b7f9d85b8fb712aa00b014f310e46e85f171ce1158afc90ba63e` |
| VisDrone9 labels | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/visdrone9.txt` | `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992` |
| visible controller config | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/config/visible-default.conf` | `b9a128b78063056173626af1c7c7d2c580a59b295c45a4afc0ef8e2e04bff95b` |
| preserved user Debug config | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/config/interactive-debug.conf` | `c07da92f912f91d5c57d42f005017a309b874445e728079d3740148b9584ab6e` |
| 输入视频 | `/home/cat/Project/SOT/nanotrack_v3_longterm/data/girl_dance.mp4` | `8090fcd5090adb62f21e79e799f0b0871d9c610d8cf8087b9fcdd3c7e9c2e9b9` |
| 307 帧 Debug 日志 | `/tmp/nanotrack-occlusion-hold-578613d-girl-dance.log` | `9118fab42e1d512617689afb88efcaeae621994e49f61a415482158d88dd4bc0` |
| 退出前窗口 XWD | `/tmp/nanotrack-occlusion-hold-578613d-frame-limit.xwd` | `dff2ed7e978ee665e2151ae2bbc7519a56956e45fb3347e1f96d1d6b710519a5` |
| 同步补丁 | `/tmp/nanotrack-occlusion-hold-578613d.patch` | `a481f5a69700bcb15a0935f626d009734ce27f59ce4627f6a6158e1cdb79b75d` |

- 裁决：本轮达到“精确提交同步 + RK3566 原生完整链接 + 除已解释 CLI 本地配置差异外
  其余测试通过 + 无标注有界 detector 诊断观察”。不达到真实视频 HOLD 时序观察、
  detector PASS/入队/活动探针人工视觉验收、标注重捕率、资源长稳、发布门或真实吊舱
  验收。任务状态继续为 `functional_pass_promotion_held`，
  `development_board_validation.latest_handoff.available=false`。

### Handoff

协调对话可采纳本节锁定的共享源码提交和板端工件身份，但不应把它发布为公共
`latest_handoff`。公共契约需要协调者评估是否仅更新 Debug 源码候选；VisDrone9
promotion hold、detector 直接控制禁令、交叉编译前板端门和人工吊舱边界均保持不变。

## 2026-10-28 HOLD 确认清理测试证据修正

- 时间：`2026-10-28T23:41:18+08:00`；修正基线为
  `bc6103ddf1c327cc6c88b3121784de6a71fa31cc`。独立复核指出，上述旧测试在 HOLD 前
  已通过重捕路径清空确认，不能证明 HOLD 入口自身执行了 `confirmations_.clear()`。
- 新测试把 `occlusion_trigger_consecutive_unreliable` 设为 `3`：先以模板冲突留下恰好
  一个 motion 确认，再输入两个 WEAK 缩小框，第二个 WEAK 触发 HOLD。测试在触发前验证
  待确认数为 `1`，在 HOLD 入口后验证为 `0`；HOLD 结束后的第一个一致可靠观测只能建立
  新的第一次确认，保持 `accepted=false`、`control_valid=false`。
- 由于当前确认策略固定为 2/3，旧确认按帧号在 HOLD 结束前也会自然过期；仅验证退出后
  未接受不足以证明入口清理。为验证新断言的敏感性，临时且仅在主机工作树中禁用 HOLD
  入口的 `confirmations_.clear()`，重新构建运行 `test_long_term_controller`，观察到：

  ```text
  FAIL: HOLD entry clears the pending motion confirmation immediately
  1 test(s) failed
  ```

  随后立即恢复原生产代码；`git diff -- src/long_term_controller.cpp` 无输出。恢复后聚焦
  `long_term_controller|long_term_replay` 为 `2/2` 通过，完整主机 CTest 为 `15/15`
  通过、`0` 失败、总时间 `0.55 sec`，`git diff --check` 无输出。
- 本次提交只增加/修正测试与证据文档，不改变生产决策或板端二进制对应的生产源码。
  真实 RK3566 构建和 307 帧视频观察发生在本测试补强之前，本轮没有重新同步、编译或
  运行开发板；板端提交仍为 `7ba0da0e0359c1ead4bd00656366a4cfbd463d69`，此前二进制
  SHA-256 `62fe954c00fd4cce96e55db62a1f1687e037b53c5045070eef333dc02cf2e3f2`
  和全部板端证据边界不变，不能把新增主机测试表述为板端复测。
- 安全与发布裁决不变：detector/HOLD/WEAK 不能直接授权控制，
  `functional_pass_promotion_held` 与 `latest_handoff.available=false` 保持不变。

### Handoff

协调对话应采用本修正后的测试覆盖和证据措辞；公共契约无需因测试补强改变工件身份或
发布裁决。若未来要求在板端执行这个新增测试，必须另行形成真实板端复测证据。

## 2026-10-30T01:34:14+08:00 目标类别初始化 RK3566 板端验证

- 共享分支/工作树：`codex/debug-target-class-init` /
  `/home/u20/single-object-tracking/.worktrees/debug-target-class-init`；输入源码
  `85c7ba592577915576be018093524a8711915ce2`。本节结果提交可用
  `git log -1 --format=%H -- docs/progress/board-validation.md` 查询，不在提交前虚构自身哈希。
- 独立板端工作树 `/home/cat/Project/SOT/nanotrack_v3_longterm-target-class-85c7ba5`
  位于 `75d44431887f29f6813329a8acb102c9d9bed594`，复核时 tracked/untracked
  状态均干净。共享源码补丁 `/tmp/nanotrack-target-class-85c7ba5.patch` 的 SHA-256
  为 `93467e875b3def3ba4aedfc0062f6c482681892083e14101bea6956881acd64d`；
  前序部署核对的构建相关对象指纹匹配，本轮又复核了补丁哈希与板端 HEAD。
- 板卡为 LubanCat RK3566/AArch64、Debian 10、Linux `4.19.232`、GCC `8.3.0`、
  CMake `3.13.4`，OpenCV `4.10.0`；实际运行日志报告 RKNN API
  `2.3.2 (429f97ae6b@2025-04-09T09:09:27)`、driver `0.8.2`。
  `ldd` 指向 `/home/cat/ThirdParty/OpenCV4100/lib/libopencv_world.so.410` 和
  `/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/aarch64/librknnrt.so`，没有缺失依赖。

### 输入与输出身份

路径均在板端。`A=/home/cat/Project/SOT/nanotrack_v3_longterm`，
`B=/home/cat/Project/SOT/nanotrack_v3_longterm-target-class-85c7ba5`，
`R=$A/releases/yolov8n-visdrone9-rk3566-fp16-v1/models`。

| 工件 | 路径 | SHA-256 |
| --- | --- | --- |
| 原生可执行文件 | `$B/build-board-target-class-85c7ba5/nanotrack_v3` | `28ffadf45d023ea66bb99ec30e7a7b7efb755d6d0ad1edc4336940b19368d243` |
| NanoTrack template | `$A/models/template_backbone.rknn` | `38b60334e8e0f0999bfb7ff9e943090546461f9307682653685237e24b2a7709` |
| NanoTrack search | `$A/models/search_backbone.rknn` | `92125f1ef6254dade5786bb999faa5b9d9412ceb2b7987f6dbba82f959b7ef6a` |
| NanoTrack head | `$A/models/head.rknn` | `2065bfca503cca9197e67de1a754efad930dfed4d2fc73881c77d9e850dcb600` |
| NanoTrack manifest | `$A/models/manifest.json` | `441e93f2092afe94bac663f27adb1fe99848357356efd67c7473b0b506f854b2` |
| VisDrone9 detector RKNN | `$R/yolov8n-visdrone9-rk3566-fp16.rknn` | `3698bc9bea1f43a4bac2826a09e03be0439fddfbc5c5604d8ed0f92b36881df0` |
| Detector manifest | `$R/detector-manifest.json` | `05e6d8c7c210b7f9d85b8fb712aa00b014f310e46e85f171ce1158afc90ba63e` |
| Detector labels | `$R/visdrone9.txt` | `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992` |
| 输入视频 | `$A/data/girl_dance.mp4` | `8090fcd5090adb62f21e79e799f0b0871d9c610d8cf8087b9fcdd3c7e9c2e9b9` |
| Debug 配置 | `$B/config/interactive-debug.conf` | `e3fe5f927ba3a47d2829dce600c01634f7f2f405576865c2edea22e0256493f8` |
| 可见光控制配置 | `$B/config/visible-default.conf` | `b9a128b78063056173626af1c7c7d2c580a59b295c45a4afc0ef8e2e04bff95b` |

### 原生构建、CTest 与场景命令

前序代理在新的板端构建目录执行以下原生配置/构建；保留原始日志为
`/tmp/nanotrack-target-class-configure.log`、`/tmp/nanotrack-target-class-build.log`、
`/tmp/nanotrack-target-class-ctest.log`，其 SHA-256 分别为
`68557c3c81af073eac3527862eeb766f15c4d3b8a8f0d948ed2a426219296984`、
`6cb11a61d3fa9938bc957421b752fc65379022ef44622c9e5c49b842b563a947`、
`34604e9e286bbf61537d176b305b5dfe9db1f4755294382d766ce403b60d06fc`。

```bash
cmake -S "$B" -B "$B/build-board-target-class-85c7ba5" \
  -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=ON \
  -DOpenCV_DIR=/home/cat/ThirdParty/OpenCV4100/lib/cmake/opencv4 \
  -DRKNN_INCLUDE_DIR=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/include \
  -DRKNN_LIBRARY=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/aarch64/librknnrt.so
cmake --build "$B/build-board-target-class-85c7ba5" -- -j2
(cd "$B/build-board-target-class-85c7ba5" && ctest --output-on-failure)
```

配置/构建完成，原始 CTest `16/16` 通过、`5.18 s`；本轮从构建目录重跑
`ctest --output-on-failure`，`16/16` 通过、`5.10 s`。板端 CTest 3.13 不支持
`--test-dir`；一次从 `/home/cat` 使用该参数得到 `No tests were found`，未计入测试结果。

场景脚本 `/tmp/target_class_board_scenarios.py` SHA-256
`9153936f9c06fd14bebb9c210ba1681623119558c0fc0eb8dc0e1a53c2ff3dcb`，
GUI 辅助脚本 `/tmp/target_class_board_gui.py` SHA-256
`3558a257d7863e0a03b0acc38f0301274393d67c1b657504925b37955c2b47ac`。
实际命令为 `python3 /tmp/target_class_board_scenarios.py {full,local,background,reselect}`；
脚本启动上述可执行文件，传入 `--config $B/config/interactive-debug.conf`、
`--video $A/data/girl_dance.mp4`、`--model-dir $A/models`、
`--controller-config $B/config/visible-default.conf` 和三个 `$R` detector 工件；
各场景完整命令保留在
`/home/cat/Project/SOT/validation-target-class-85c7ba5/{full,local,background,reselect}.command`。
前三个命令使用 `--max-frames 3` 并通过 GUI 单帧步进，重选命令使用
`--max-frames 210`；脚本对等待和进程退出设有上限。所有场景 exit 0。

| 场景 | ROI（原视频坐标） | 板端观察 |
| --- | --- | --- |
| 人物全身大框 | `196,160,184,310` | `UNKNOWN/NO_MATCH`，仅一个 car 候选，非 person；恢复关闭 |
| 人物局部框 | `244,161,74,70` | `UNKNOWN/NO_MATCH`，仍仅 car 候选；恢复关闭 |
| 背景框 | `30,70,100,100` | `UNKNOWN/NO_MATCH`；恢复关闭 |
| 同进程重选 | 全身→局部→背景→全身 | 初始化与 request ID 均为 1–4，四次完成、零错误；四次均 UNKNOWN，随后跟踪 210 帧 |

在默认 detector confidence `0.25` 下，初始化 batch 原始数 `6`，过滤后仅一框：
`car(2)`、confidence `0.363`、bbox `484.21,163.14,98.73,175.73`，
与左侧人物全身 ROI 的覆盖率 `0`。用于诊断的有界命令额外覆盖
`--detector-conf 0.05`，日志 `$E/full-conf005.log`（`E` 为下述证据目录）记录左侧
人物对应的 `truck(4)` 候选 confidence `0.162` 被选中、右侧有两个低置信度 person
候选；`timeout --signal=INT 12s` 退出码 `124`，没有 metadata 文件。
这既不是默认配置结果，也不是正确的 person 确立或阈值优化通过。

诊断命令（`E=/home/cat/Project/SOT/validation-target-class-85c7ba5`）：

```bash
timeout --signal=INT 12s env DISPLAY=:0 stdbuf -oL -eL \
  "$B/build-board-target-class-85c7ba5/nanotrack_v3" \
  --config "$B/config/interactive-debug.conf" --video "$A/data/girl_dance.mp4" \
  --bbox 196,160,184,310 --model-dir "$A/models" \
  --controller-config "$B/config/visible-default.conf" \
  --detector-model "$R/yolov8n-visdrone9-rk3566-fp16.rknn" \
  --detector-manifest "$R/detector-manifest.json" --detector-labels "$R/visdrone9.txt" \
  --detector-conf 0.05 --max-frames 1 --session-metadata "$E/full-conf005.json" \
  > "$E/full-conf005.log" 2>&1
```

证据目录为 `/home/cat/Project/SOT/validation-target-class-85c7ba5`。
`full/local/background/reselect.log` SHA-256 依次为
`db5581161e5f7dde8736b7f732b7af96dbeca725237717cd8900fbbdef7fc9c`、
`3a23962e92eb971a26c6ec7c1a0a686550473069279f131ebab2643339e160bc`、
`5e12cea76d00490fecd7cb327241a454b27d91ca49e5d5c0357be8d77edf339f`、
`1a79b7b7e209f908f14c037f465b5440d8ba3e9a785930380821cdfb720b3e97`；
对应 `.json/.csv/.xwd` 也保留。`reselect.csv` 为 212 行（表头、唯一 frame 0、
210 跟踪帧），未重复写入初始化帧；`reselect.json` 的初始化 detector
`request_count=4, complete_count=4, error_count=0`，恢复 detector
`request_count=0, complete_count=0`。跟踪期出现 `control_valid=1` 的记录来自
NanoTrack `MOTION` 路径，未观察到 detector-only 控制授权；没有实际触发 detector
辅助恢复，也没有可观察的旧 batch 污染或新 eligibility generation 遥测。

另一次无预置 bbox 的 GUI 运行使用 `timeout --signal=INT 45s`，在
`interactive_ready initialized=0` 时截取 `prepaint-before-roi.xwd`，随后选取
`196,160,184,310` 并截取 `prepaint-after-roi.xwd`。前者可见首帧、操作提示及
`NO_TARGET`；后者可见 `UNKNOWN — DET RECOVERY OFF`、灰色拒绝 car 候选和绿色
tracker 控制框。白色初始化框的边缘被绿色框覆盖，无法从此截图独立确认白框层叠
效果；未观察到成功选择的候选。两个截图 SHA-256 分别为
`39e2ca4cf221fc9c05fdf88ffafe278b464dfe7f4c561506b68b31e5e51548f5`、
`3ac57070626ead0ab19a1c38d03b10ab31bdfa1eff26823ea1bb012e64f9d95e`；
`prepaint.log` SHA-256 为
`134037f819800ccba563b261cdb83cce2fa1fb2ab87f17994a64c037f465b544`。
GUI 启动命令与场景命令使用相同输入，使用
`timeout --signal=INT 45s`、不传 `--bbox`，显式设置
`--max-frames 1 --output-boxes $E/prepaint.csv --session-metadata $E/prepaint.json`。
随后依次运行
`python3 /tmp/target_class_board_gui.py capture $E/prepaint-before-roi.xwd`、
`python3 /tmp/target_class_board_gui.py roi 196,160,184,310`、
`python3 /tmp/target_class_board_gui.py capture $E/prepaint-after-roi.xwd`、
`python3 /tmp/target_class_board_gui.py key q` 并等待正常退出。

### 裁决、边界与 Handoff

本轮 **原生构建、板端 schema/CTest、模型加载和实际初始化推理路径通过**；
`girl_dance` 正例目标类别确立 **失败**：人物全身/局部均 UNKNOWN，默认阈值下
仅检出 car，降低阈值会把左侧人物误判为 truck。UNKNOWN 时恢复关闭、同进程
request ID 隔离与初始化帧不重复写出已观察。板端仅有 `girl_dance` 与
`sot-v1` 系列视频，未确认合法车辆样本，车辆全身/局部场景记为 **未观察**。
没有成功确立 person 或 vehicle，detector 辅助恢复未开启/未触发，不能以
`control_valid=1` 的 tracker 帧替代该门槛；亦无长稳、性能晋级或真实吊舱验收。
总体任务状态仍为 `functional_pass_promotion_held`，公共契约中的
`development_board_validation.latest_handoff.available=false` 必须保持。

请求协调对话采纳本证据提交，保持交叉编译 handoff 不可用，并决定后续 detector
类别误判/样本覆盖的独立修复与重测；不得把本次原生编译成功写成 target-class
功能通过或生产交付批准。完整任务报告在工作树
`.superpowers/sdd/2026-10-29-target-class-establishment/task-9-report.md`（不纳入 Git）。
## 2026-11-11T15:17:45+08:00 C010 post-holdout PARTIAL FAIL

- 分支 `codex/occ-video-1-tracking`，源码提交
  `033dc9058dfc4bf86d46fc74b1b472f689c6922b`，板端根目录
  `/home/neardi/Project/SOT/occ-candidate-033dc90`。源码归档、配置、板端可执行 SHA-256
  分别为 `82d41764a70fecbd6b48cf89571d05fb6c6f0d699d133e125e1347732690009b`、
  `83f1382e57942d44692c687c388133639a2cc94c91de3eff9a09ca688895d274`、
  `5802f441e8907007f54d1cacfa9fa4e0ba362f44786daa12689adde97daf7f51`。
- RK3588 原生 Debug、RKNN、`-Werror=switch` 构建成功，CTest `19/19`；完整运行
  verifier PASS，视频 1920x1080/271 帧，CSV 271 行。运行证据保留于
  `/home/neardi/Project/SOT/occ-candidate-033dc90/runs/C010-full-run1`。
- full evaluator FAIL，仅 `reacquisition` 门失败：帧 200 由 `template_bridge`
  正常接受，但 anchor 的 score-only 低谷持续到 201；单帧预算耗尽后 201--216
  再次 SEARCHING，重捕获帧为 `[185,217]`。其余门全部通过，valid-observation
  mean IoU `0.734841`，all-visible mean IoU `0.721368`。
- 证据边界：这是 C009 揭盲后的迭代，不是盲测；没有执行 C010 重复运行，也没有
  声称云台实机验收。下一步为通用、有界的 C011 两帧桥接设计，不降低可靠性或评估阈值。

## 2026-11-11T15:31:46+08:00 C012 canonical board acceptance PASS

- 分支 `codex/occ-video-1-tracking`，最终源码提交
  `e865c78679247cb45ddbdbf2fb466e49930d5d12`，板端根目录
  `/home/neardi/Project/SOT/occ-candidate-e865c78`。源码归档、配置、可执行 SHA-256
  分别为 `64896748bbedbe16a8bcea762087e3060db0f0a0ba4b7a64cbc113b56755ef69`、
  `82ce1e035e202f10a46f1aa2a21f4ed42d79839dca01b0629e1848c90919432c`、
  `e2a44c74bf6410dc79b5d07a5c134e27cd0096cdca16a81c67d2f27df521ab4d`。
- C011 两帧预算 full FAIL（重捕获 `[185,218]`），三帧诊断仅将失败平移到 203，
  因此停止按帧追参。C012 改为与帧率无关的 200 ms 短时滞回，不改变 tracker
  reliability、geometry 或 evaluator 阈值。
- RK3588 原生 Debug/RKNN/`-Werror=switch` 构建成功，CTest `19/19`。三次完整
  0--270 运行均 verifier PASS、full evaluator PASS，语义签名完全一致：唯一重捕获
  帧 185，模板桥接帧 200--206，190 以后无 SEARCHING；valid-observation mean IoU
  `0.732253`，all-visible mean IoU `0.725693`。
- 最终审查补充了 abort / motion prediction exception 立即撤销 bridge 的 fail-closed
  负例；修复后重新构建并重新执行三次。三次 evaluator JSON SHA-256 均为
  `8748b91ca59a8a91ecb013099d3aa43cb7868b1227c6123a5008c4f16f530aaf`；
  verifier log 均为
  `86808013d246f358f354b6101ef4e84470bb2cd29dfb95fa89482d31d735f02e`。
  49 帧复核视频保留在 run1，SHA-256
  `a3e76846fbf5b256a52226f21d041deec8b102c458fe3810c1f3410eb2f221c6`。
- 证据边界：`PASS_CANONICAL_BOARD_ACCEPTANCE` 仅表示 RK3588 上 canonical
  `occ_video_1` 后揭盲三次验收通过；人工复核视频状态仍为 PENDING，未声称真实云台
  pod 验收。

## 2026-11-11T16:10:00+08:00 RK3588 interactive one-command profile startup PASS

- 分支 `codex/occ-video-1-tracking`，任务基线
  `10c311e4576e67036ff30eb2538e9298aab5abfc`，配置/测试/文档提交
  `b3a7e4a`。本机配置为
  `rk3566/nanotrack_v3/config/rk3588-interactive-debug.conf`，板端部署路径为
  `/home/neardi/Project/SOT/occ-candidate-e865c78/rk3588-interactive-debug.conf`；两端
  SHA-256 均为
  `e7daafa4373f676b3e9b10984e48fa2ffe46cc6d9ec1c422dbe3d4e77e88544a`。
- 主机侧先修改 CLI 测试形成 3 个预期失败断言，再更新配置；聚焦 `cli` 测试通过，
  全量 CTest `19/19` 通过，`git diff --check` 通过。配置保留
  `interactive=true` 且不预置 bbox，绑定 C012 已验收的 RK3588 NanoTrack 模型、
  `visible-occlusion-calibrated.conf` 和 RK3588 VisDrone9 detector release；固定输出为
  `/tmp/rk3588-interactive-debug.avi`、`.csv`、`.json`。
- 板端在 `/home/neardi/Project/SOT/occ-candidate-e865c78` 执行
  `DISPLAY=:0 timeout --signal=INT --kill-after=2s 15s stdbuf -oL -eL ./build/nanotrack_v3 --config rk3588-interactive-debug.conf`。
  日志确认 RKNN API `2.1.0`、driver `0.9.8`、detector `target=rk3588 outputs=9`，
  随后出现 `runtime_ready`、三个输出均 enabled，以及
  `interactive_ready playing=0 initialized=0`；冒烟结束后无残留 `nanotrack_v3`
  进程。日志保留于板端 `/tmp/rk3588-interactive-config-smoke.log`。
- 证据边界：本次只验证新配置通过既有二进制加载并进入等待鼠标选框状态；未人工画框、
  未运行完整视频、未生成新的算法验收结论，也未声称真实云台 pod 验收。C012 的
  canonical acceptance 及其源码/可执行身份不因本次运行配置部署而改变。SSH 登录环境
  的 `DISPLAY` 为空，本次冒烟因此显式使用 `DISPLAY=:0`；用户给出的原样两行命令应在
  RK3588 图形桌面终端（已继承 `DISPLAY`）执行，不能把 SSH 冒烟误记为字面命令验收。

## 2026-11-11T22:21:04+08:00 occ_video_1 car_002 271-frame draft generated

- 分支 `codex/occ-video-1-tracking`，本任务执行基线
  `8545f97ac9539231e936dfcc2a322d8916e5f143`，候选工具提交 `1ccff5d`，分层草案打包器
  提交 `8545f97`，review contact-sheet 纳入包与哈希的修复提交
  `5d186f8705b5f8a1bababe51a11ffdd43a080935`，独立审查后的 provenance/validator/
  staged-replace 加固提交 `b1d9dc2`、`d33e848`、`e069bf6` 与 `db20a75`。这是主机侧标注草案生成，不是
  RK3588 新运行或板端验收。
- 原视频 `/home/u20/single-object-tracking/data/occ_video_1/1.mp4` SHA-256
  `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`，严格解码为
  1920x1080、30/1 fps、271 帧。用于辅助定位运行行为的保留证据 CSV/log/JSON/AVI
  SHA-256 依次为
  `ab5f05f2b278ae0216f404f8fee8130b53a396a582a361f0cd00e07fc13ae79f`、
  `269b80673b436db70e2f1d16be56898e69f64b78b114068a128fed7cba6d78b0`、
  `c588f6867043256266a838e2c95517f31bd334355dfbf402e6d53028ae6a8088`、
  `7146e5719363ff4a5611d15b758f67b6e3abc09a5997fd30c6c3358c8d2cce61`；运行证据只作
  诊断参考，不充当真值。
- 工作输入位于
  `artifacts/ground_truth/occ_video_1_car_002_work/`：`anchors.csv`、`candidates.csv`、
  `review-policy.csv` SHA-256 分别为
  `9a618cb492bf5d47b6d6688feb8cf2af7780b6c030438c78c1d027da377cf994`、
  `39c23fa800287843d5e20f3e528c796b7d44bb6315c6c10652c4c82465ca3821`、
  `e9bfa7ab37394b299ce5dc9c482878dc759b5936f84dd6ff78f1f847e6c4f3ac`。
  `distractors.json` SHA-256 为
  `dfcde77592f879ca85924c6155f2f0ffc49c5f1a1925ea6d77c50ac677723f23`。
  候选 review AVI SHA-256
  `ca40b981acb8dd8c4fae41cd75b157dcd77041105eb4e64baa63d81eb5233a67`，解码为
  1920x1080、30/1 fps、271 帧；三张 contact sheet 已逐张查看，SHA-256 为
  `dc3976f3da8d7be84740227b450e52d24e43acf4ce70736a737640e6c5ed2762`、
  `607796268c2b5031c857c6d34d264dd11aa52eaac1e3984541a57c0c79170116`、
  `4968b176b57957a2a8e212b06fa004a0421ba535e79ec6b0cf63439f2b982b94`。
- 草案位于 `artifacts/ground_truth/occ_video_1_car_002_draft/`，
  `checksums.sha256` SHA-256
  `be57dbaee44cfda1e2f925ea08f24a76edbf968c1159076a8c2c7f39303e1e27`，
  `annotation_metadata.json` SHA-256
  `2df9f8ccdb5c546133cac0e7e2bc26111a6aae3635938b9ba23a0ae4f7ce28b2`；连续两次重建
  获得相同两项哈希。
  三层均恰有 271 个连续文件：正式 `labels/` 非空 0，未确认候选
  `candidate_labels/` 非空 252，非可观测估计 `estimated_labels/` 非空 4；
  `frames.jsonl`、`annotation_status.csv` 均为 271 帧，`review_manifest.csv` 有 267 条
  待确认记录。状态计数为 assistant candidate 252、needs human review 15、estimated
  non-observable 4、manual confirmed 0。
- 身份复核结论：目标是桥前初始化白车，桥后为绿色车左侧白车；双向候选在 0--210、
  230--270 连续跟随该车。211--212 与 217--229 为高优先级人工复核区间；213--216
  标为 full occlusion，仅保存低置信度运动估计。运行在 259 帧取得的 detector 候选及
  262 帧恢复对应另一辆同类白车，已以 `car_002_distractor_001` 连续记录 259--264 帧
  （并保留 270 帧参照），不能作为目标真值。
- 实际生成命令为
  `/tmp/nanotrack-car002-tools/generate_bidirectional_candidates --video ... --anchors ...`
  与 `python3 rk3566/nanotrack_v3/tools/build_occ_car002_draft.py build ... --review-dir ...`；
  `validate --require-confirmed false` PASS，`validate --require-confirmed true` 按预期以
  `unresolved review rows: 267` FAIL。三个 layer manifest 与总 `checksums.sha256` 均经
  `sha256sum -c --quiet` PASS；Python 聚焦测试 8/8、annotation-tools CTest 20/20、
  既有主机 CTest 19/19 PASS，`git diff --check` PASS。
- 独立终审指出的四项问题已处理：人工确认必须通过独立 `confirmed_*` 框且保留原候选；
  validator 重新检查几何、枚举、派生字段、估计依据、逐层 YOLO 一致性、状态计数、
  metadata/video/episode 身份、状态与可见率关系、干扰物类型/ID 及 checksum 完整覆盖；
  独立 `validate` 默认绑定 canonical `occ_video_1` 固定 SHA-256，包内即使改绑到另一存在
  文件并提供其正确哈希也会被拒绝；
  `--replace-draft` 先在同级 staging 完整构建并通过
  validator，再以可恢复备份交换（不声称单次系统调用原子交换）；错误同类车补齐
  259--264 逐帧记录。测试覆盖多类语义篡改后重算哈希仍必须失败、结构无效 staging
  不得替换旧包，以及非法输入不得破坏旧草案。
- 证据边界：这是完整覆盖 0--270 的 assistant draft，不是人工确认真值。正式层为空，
  在用户逐帧确认并生成新版本/新哈希前，不授权 IoU、阈值标定、算法准确率、canonical
  dataset 晋级、板端验收或云台 pod 验收；没有修改 `car_001`、`STATUS.md` 或
  `integration-contract.yaml`。

## 2026-11-11T23:33:23+08:00 occ_video_1 car_002 confirmed package generated

- 分支 `codex/occ-video-1-tracking`，任务基线
  `100afb18d371707d9700b40a8f3931d5c22a931c`，结果代码与本记录位于同一 Git 提交。
  用户明确“整体接受”：接受 0--210、230--270 的候选框；确认遮挡区间；确认
  259--264 的候选框仍是正确目标，并与既有运行在 264 帧跳到错误车辆的负面证据严格
  区分。原 assistant draft 未作为本轮生成目的地，也未被重建或覆盖。
- 确认策略输入为
  `artifacts/ground_truth/occ_video_1_car_002_work/review-policy-confirmed.csv`，SHA-256
  `2fd9a8959ddb141292680fc4c5c30f7a0c3a682358788a0def55880eacde75a5`。0--210、
  230--270 共 252 帧使用 `manual_confirmed_visible`，候选框晋级到正式 `labels/`，同时
  保留同框的 candidate 层和候选来源；211--212、217--229 共 15 帧使用
  `manual_confirmed_no_bbox`，只确认遮挡状态与目标身份，正式层和候选层都明确为空；
  213--216 共 4 帧保留 `estimated_non_observable`，估计层不晋级为正式真值。
- 独立确认包位于
  `artifacts/ground_truth/occ_video_1_car_002_confirmed/`。`checksums.sha256` 与
  `annotation_metadata.json` SHA-256 分别为
  `4540ff280903a3e32d8b439f54ddca4497e855095da68ebdbe8fbea044371811`、
  `ddbd8846cd12466d82a833f2f335160a6e6dd1f6189963d268db1b1c54d03a63`；正式、候选、
  估计 manifest SHA-256 分别为
  `2f0e78ab1123cb035c87efa06171f1057c108f38b06fcc5c1ed3a3ec63fdbe98`、
  `c6d1093c16fd17c02c9b8ce1743a85bbc5316dacef76d08e368093a976a6c11e`、
  `62a2877bfc38bf4482e3e1cca1e81551272a677f81d932918723e557db3d24e4`。
  三层仍各有 271 个连续文件，非空计数依次为 252、252、4；
  `review_manifest.csv` 只有表头，未决计数为 0。连续两次生成得到相同 metadata 与总清单
  哈希，`validate --require-confirmed true` 及确认包 `sha256sum -c` 均 PASS。
- 打包器新增 confirmed-package 模式及严格语义：只有零未决项才可写入
  `draft=false`、`formal_evaluation_authorized=true`；`manual_confirmed_no_bbox` 必须身份
  certain、质量 unusable，且 formal/candidate/estimate 三种框都为空。实际执行 Python
  单元测试 10/10、含 annotation tool 的 CTest 20/20、既有主机 CTest 19/19，全部通过；
  `git diff --check` 通过。
- 复核旧 draft 时发现其
  `review_contact_sheets/contact-sheet-000.jpg` 当前缺失，旧总清单仍期望其 SHA-256
  `dc3976f3da8d7be84740227b450e52d24e43acf4ce70736a737640e6c5ed2762`，因此旧 draft
  当前独立校验失败。工作输入和新 confirmed 包都含有该文件且哈希匹配；为遵守“保留原
  draft、不覆盖”的边界，本轮未修改或修复旧 draft，并把此漂移作为外部证据异常记录。
- 证据边界：`formal_evaluation_authorized=true` 只授权新 confirmed 包中 252 个非空正式
  框参与 ordinary IoU；211--229 中无正式框的帧不能被当成零框误计 IoU，213--216 的
  估计框也必须排除。该确认不自动完成阈值标定、canonical dataset 晋级、RK3588 新运行、
  板端验收或真实云台 pod 验收；未修改协调者所有的 `STATUS.md` 与
  `integration-contract.yaml`。

## 2026-11-11T23:44:37+08:00 dual ground-truth sequences installed

- 分支 `codex/occ-video-1-tracking`，任务基线
  `4d38c28f31f125479bcc24c92231584a9cdde3e1`。按用户要求，将第二目标 confirmed 包从
  `artifacts/ground_truth/occ_video_1_car_002_confirmed/` 移至真实 canonical 数据目录
  `/home/u20/single-object-tracking/data/occ_video_1/ground_truth/car_002/`；原 artifacts
  confirmed 路径现已不存在。共享视频仍只有根目录 `1.mp4` 一份，不复制视频。
- 新增 `ground_truth/index.json` 与 `ground_truth/README.md`，SHA-256 分别为
  `42d08ea98642f81640842dd166ff8ba3518668441d430ce9931f99d1dc544177`、
  `2b2b9e10e7d70cecffc693f7044cde6a61128e8517188bf897bdcae6578dc6b9`；既有第一目标
  标注以逐文件相同的镜像放入 `ground_truth/car_001/`，根目录旧路径继续保留以兼容现有
  公共契约和旧命令。镜像 metadata/status/events/labels manifest 哈希仍分别为
  `5641650435a1980694f982cf1fa6715ba959b1515938cee383c8db8ec4aeb076`、
  `e44fd23514d6871e72d628ad4843fc90bb4246f306322ee297c8a2aa75818036`、
  `a43f016c96b34c8a0ec6125dcb944ba4fa07a79d99a46ce696bbb29841ded256`、
  `9ec01854884126350f93e1af3a4aee002d38ce7c2d50c71c78de0f1a8dc0a648`，271 项标签
  `sha256sum -c` 全部通过。
- 第二目标迁移后 `checksums.sha256` 仍为
  `4540ff280903a3e32d8b439f54ddca4497e855095da68ebdbe8fbea044371811`，严格 confirmed
  validator PASS；metadata/status/events/formal manifest/estimate manifest 哈希分别为
  `ddbd8846cd12466d82a833f2f335160a6e6dd1f6189963d268db1b1c54d03a63`、
  `212525642591c124320e93369f8748d82cc53092628beff051507f930946dec1`、
  `efc0652335fcf19c766def88bf50bb8defab1441fdc8fbb412b85d02135c31db`、
  `2f0e78ab1123cb035c87efa06171f1057c108f38b06fcc5c1ed3a3ec63fdbe98`、
  `62a2877bfc38bf4482e3e1cca1e81551272a677f81d932918723e557db3d24e4`。
- `scripts/evaluate_occ_video_1.py` 新增
  `--target-id {car_001,car_002}`，默认仍为 `car_001`。第二序列读取空正式标签、独立估计层
  与新遮挡事件：252 帧允许 ordinary IoU、4 帧仅作估计轨迹参考、15 帧无 bbox 且不参与
  几何评分。第一序列继续按 `source` 排除 169--181 共 13 个插值帧，避免被同一 CSV 的
  `review_status` 覆盖。双序列身份加载分别得到 271 帧；合成完美运行对两序列 full evaluator
  均为全部 gate PASS。Python evaluator 单元测试 16/16 通过。
- 后续同一运行 CSV 可分别执行：
  `python3 scripts/evaluate_occ_video_1.py --dataset-root /home/u20/single-object-tracking/data/occ_video_1 --target-id car_001 ...`
  与将 target 改为 `car_002` 的第二次评估。一次运行的初始化目标必须与所选真值目标一致，
  不能用初始化 `car_001` 的 CSV 对 `car_002` 结论作有效性声明。
- Handoff：请求协调对话把双序列目录、索引和上述新哈希纳入
  `integration-contract.yaml`，并决定板端数据复制路径；本任务按所有权规则未直接修改
  `STATUS.md` 或公共契约。此次仅完成主机数据安装和 evaluator 消费能力，不是新的 RK3588
  算法运行、阈值标定、板端验收或吊舱验收。

## 2026-11-12T02:38:54+08:00 RS001 recent/safe RK3588 原生验证 HOLD

- 分支 `codex/occ-video-1-tracking`，工作树
  `/home/u20/.codex/worktrees/rk3588-debug-deploy/single-object-tracking`，Task 6 基线
  `68b527659e8be39304ab805c9a6f85219c2af5ce`。板端源码先提交为
  `e027006339f0cdafb0b8d38c4dfa5e14abe66593`，证据与本条记录同提交
  （`test: validate recent-safe recovery on RK3588`）。
  `git archive` 两端 SHA-256 均为
  `fc8deb4fd4f96d28ab27be34feaf2133e6e8dc75937380b8304b913220af080e`；
  板端逐文件比对 314 个归档文件内容与执行权限通过。
- 实际板为 `LKD3588`，Linux 6.1.84 aarch64。独立候选根
  `/home/neardi/Project/SOT/occ-recent-safe-rs001`。RKNN API
  `2.1.0 (967d001cc8@2024-08-07T19:28:19)`、driver `0.9.8`；
  运行前用同一提交的 RKNN loader 逐一查询三份 tracker 模型和 detector。
  `build/nanotrack_v3` SHA-256
  `c4161c0a448351f4a6d84be60bc00ce8c7c6e98ed28134efb5069d7562821304`，
  `visible-recent-safe-v1.conf` SHA-256
  `7b038eed1dd052de5879bedbf465a2ded3cc79d5a6f0b662e39d9fd06fc15c34`（`calibrated=false`）。
  原生 CMake 使用 Debug、`ENABLE_RKNN_RUNTIME=ON`、`BUILD_TESTING=ON`、
  `-Werror=switch`，`cmake --build build --parallel 4` 和
  `ctest --test-dir build --output-on-failure` 通过 22/22（13.85 s）。
  主机 RKNN-off CTest 22/22、evaluator Python 19/19、语法检查和 `git diff --check` 通过。
- 两次各读取独立 ROI 和真值，`car_001=992,582,37,39` 使用 canonical 标注，
  `car_002=967,513,42,42` 使用已确认包（252 帧正式可观测框）。
  两次均运行 0–270 帧，1920×1080、30/1 fps，runtime exit 0，
  schema v11 verifier PASS，271 行 CSV 与实际 CLI 发布的 271 行 JSONL companion 均完整。
  模型、标注、输出哈希、完整 evaluator 门槛、源码与运行命令记录在
  [RS001 评估记录](../evaluation/occ-video-1/recent-safe-candidate.json)；
  大工件留在板端 `runs/car_001/` 与 `runs/car_002/`，每个目录有 `SHA256SUMS`。
- `car_001` full evaluator **FAIL**。172–173 帧未满足完全遮挡语义，174 帧开始搜索，
  `[182,190)` 内及整段均无重捕获，248–270 帧控制无效。最后可信帧 172，
  到首次失去控制经过 2534 ms。有效观测 mean IoU 0.764074（168 帧），
  全部可见帧 mean IoU 0.602621。候选冲突 76 帧。
- `car_002` full evaluator **FAIL**。70 帧进入搜索后无重捕获，145–270 帧控制无效；
  213–216 帧完全遮挡语义和估计轨迹连续性均失败，`[230,271)` 无重捕获。
  最后可信帧 69，到首次失去控制经过 2533 ms。有效观测 mean IoU 0.910586（70 帧），
  全部可见帧 mean IoU 0.302287。候选冲突 143 帧。两目标 identity proxy 均 PASS，
  其结论限于有权使用的可见观测，两次均未提交新恢复身份。
- recent/safe/staged 物理 head 调用分别为 `270/96/14` 与 `270/200/6`，
  safe 复用各 0 次，任何帧最多 2 次，未观察到模板事务应用失败。
  runtime p50/p95 分别为 7.82/9.97 ms 和 8.71/10.55 ms，pipeline p50/p95 为
  109.90/113.42 ms 和 111.02/113.78 ms。板端缺少 `/usr/bin/time`，以约 100 ms
  间隔采样进程 `/proc/<PID>/status`，观察到 VmHWM 237260 / 235476 kB。
- **Handoff**：RS001 保持 HOLD 和 `calibrated=false`。两次均按原配置一次运行，
  C012 图形启动仍使用原路径与配置；其二进制、交互配置和控制器配置 SHA-256
  前后一致。需复查 RS001 时，在候选根执行
  `cat runs/car_001/commands.txt runs/car_002/commands.txt` 查阅准确诊断命令，
  新诊断使用新的输出路径。证据限 RK3588 原生 Debug 的两目标算法结果。
  请求协调对话继续保留正式 handoff 门槛，生产交叉编译与人工吊舱验收未启动。

## 2026-11-12T02:53:15+08:00 RS001 v11 evaluator 复核与重新评分

- 在 `codex/occ-video-1-tracking` 的既有证据提交
  `fcab231098371b4e659282b1472b7faf243934e5` 上修复 evaluator。板端运行源码仍为
  `e027006339f0cdafb0b8d38c4dfa5e14abe66593`，重新评分脚本提交为
  `eb344708429949356c9f0161253bb326e0c16502`，脚本 SHA-256
  `57bdd5d6824c5517adaeeac1910152b52dbc1721589983cea548c6c4f5f1b7f4`。
  v11 重捕获必须来自可信跟踪观测和已应用的恢复阶段；`LOST` 禁止控制和观测声明；
  完全遮挡门直接核对观测缺失、可信标记和已获授权的运动预测。新增负例先失败，修复后
  evaluator Python 23/23、完整主机 CTest 22/22、Python 语法与 Git diff 检查通过。
- 用该提交的脚本在 RS001 分别重新评估保留的 `runs/car_001/bboxes.csv` 和
  `runs/car_002/bboxes.csv`，沿用各自正式真值及 session metadata。两次仍为 **FAIL**，
  每项 gate 的布尔结果和失败帧与原报告一致，均无重捕获或无效重捕获声明。
  更新后的 JSON SHA-256 分别为
  `b338b8b8ce5967d377bbdf15751a8ab36b63023151758e162937a34a88ddee19` 和
  `3e37c58e1387769203353dd81dcebbacf441ec55a269522b13deb1426fbd1846`，
  Markdown SHA-256 分别为
  `0de2acb648f2aa4fd42e2ffdcc583392bf28031b7a882b29402edfa4ebb011ae` 和
  `b2673e732693c3f43e15caf4abe9723ae638d0440e5808dc4c3d53aced6c21b0`。
  新 `SHA256SUMS` 均通过复核，非 evaluator 文件逐项哈希与旧清单一致；原报告及清单保留于
  两个 run 的 `evaluator-original-e027006/`。准确命令、原始及更新后的哈希见
  [RS001 评估记录](../evaluation/occ-video-1/recent-safe-candidate.json)。
- **Handoff**：保持 RS001 **HOLD**。本次仅对保留 CSV 重新评分，新增推理运行 0 次。

## 2026-11-12T09:56:47+08:00 latest HEAD RK3588 双目标视频运行与回传

- 分支 `codex/occ-video-1-tracking`，运行源码提交与任务基线均为
  `3ec5d56c72567b4e3eb6cab89cda149f51ce77f3`。源码通过 `git archive` 部署，归档在
  主机与板端核验的 SHA-256 均为
  `5d523e03c7d13a70b4065e9f450f8c79f1b0627d2f7a652610a8a68ba7114cc1`；独立板端根为
  `/home/neardi/Project/SOT/occ-latest-3ec5d56`，未覆盖 C012 或 RS001。
- 实际板为 `LKD3588`、Linux 6.1.84 aarch64，RKNN API
  `2.1.0 (967d001cc8@2024-08-07T19:28:19)`、driver `0.9.8`。原生 Debug 构建启用
  `ENABLE_RKNN_RUNTIME=ON`、`BUILD_TESTING=ON` 和 `-Werror=switch`，CTest 22/22 通过；
  新 `build/nanotrack_v3` SHA-256 为
  `8b30b04ce84dcfe4b4209a9aff7cf94a21697b78c35d2422fb2cee9396aebd38`。
- 输入仍为 `data/occ_video_1/1.mp4`，SHA-256
  `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；控制器配置
  `visible-recent-safe-v1.conf` SHA-256
  `7b038eed1dd052de5879bedbf465a2ded3cc79d5a6f0b662e39d9fd06fc15c34`
  （`calibrated=false`）。tracker manifest 和 RK3588 detector SHA-256 分别为
  `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`、
  `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c`。
- `car_001=992,582,37,39` 与 `car_002=967,513,42,42` 分别独立运行 0--270 帧，
  runtime 均 exit 0；schema v11 `verify_board.sh` 均 PASS，CSV 均为 271 数据行，输出视频
  均经 `ffprobe` 确认为 1920x1080、30/1 fps、271 帧。板端完整结果保留在
  `runs/car_001/` 与 `runs/car_002/`。
- 两段视频已回传至 `/home/u20/occ-video-1-latest-3ec5d56-results/`，并在 u20 端重新
  解码计数和复核哈希。`car_001-3ec5d56.avi` SHA-256 为
  `7d0438ea9da9dc08f5f7effa5d0084668c9c97091b8e818e4e2a5e11eb5aa1fd`；
  `car_002-3ec5d56.avi` SHA-256 为
  `dee808467725707228f5eda6045f21aa8388425bc3099855189acab28fc0e0ef`。
- **证据边界**：本次按用户要求完成最新程序的两目标单次运行、结构校验和视频交付，未执行
  full evaluator，也不改变 RS001 HOLD、`calibrated=false`、正式 handoff 门槛或人工吊舱
  验收状态；未修改 `STATUS.md` 或 `integration-contract.yaml`。

## 2026-11-12T13:07:25+08:00 recent-safe v2 阶段 1：单指标瞬降板端验证

- 分支 `codex/recent-safe-occlusion-recovery-v2`，任务基线
  `ef0c02130fba4aa900f85daf3341943bd408e634`，本阶段源码提交
  `c3d27bea3087f42ae60b8aa7e891bbc9a61fe605`。改动只调整 recent 观测判定：恰好一个
  响应指标失败时发布 tentative/SUSPECTED 观测，不更新可信运动、响应基线或模板；尺寸
  塌缩和多个绝对响应指标失败仍直接进入 SEARCH。主机 RKNN-off CTest 22/22、C012
  门禁 Python 单测 5/5 和 `git diff --check` 通过。
- 源码以 `git archive` 部署到实际板 `LKD3588` 的独立目录
  `/home/neardi/Project/SOT/occ-recent-safe-v2-c3d27be`，未覆盖 C012、RS001 或 latest。
  `source.tar` SHA-256 为
  `42741709a2dc50150c558cd4452b95dafbaa7eadfcbb1814b4613784aba915b3`。
  板端原生 Debug 构建启用 `ENABLE_RKNN_RUNTIME=ON`、`BUILD_TESTING=ON` 和
  `-Werror=switch`，CTest 22/22 通过；`build/nanotrack_v3` SHA-256 为
  `668aa05e6a7981f428afd3591d7436862575864afb39e13499dace806639ce52`。
- 使用固定 C012 A/B 初始框 `car_002=970,517,38,38`，同一视频、tracker 模型、detector
  和 `visible-recent-safe-v1.conf` 单次运行 0--270 帧。视频、配置、detector SHA-256
  分别为 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`、
  `7b038eed1dd052de5879bedbf465a2ded3cc79d5a6f0b662e39d9fd06fc15c34`、
  `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c`；三份 tracker
  RKNN SHA-256 记录在板端 `provenance/car002-inputs.sha256`。runtime exit 0，schema v11
  verifier PASS，CSV 和 1920x1080 AVI 均为 271 帧。
- 60--80 帧没有 SEARCH 且全部 `control_valid=1`。第 70 帧从 RS001 的直接 SEARCH 变为
  一帧 SUSPECTED，仍输出 NanoTrack 观测框并保持 observation control；第 71 帧恢复
  TRACKING。第 70 帧框为 `894.354431,618.288696,36.756348,37.964905`，与 C012 同帧
  `896.088928,618.415588,37,38` 接近。新 CSV、AVI 和 session metadata SHA-256 分别为
  `69fd729a37415b8c9071b4e9527c367e0b791e8fc13fcf4017399c1578b2fa67`、
  `6350da6ec20043f9bce4cce36dbefad108f4eb5745182d3812b12b741a6a9abd`、
  `a054f00d3d02b25042fb730a594299401a79705773a11319b91619d42f26bda7`；完整大工件留在
  板端 `runs/car_002/`。
- **证据边界 / Handoff**：阶段 1 的遮挡判定目标在 car002 上达到，但现有 C012 晋级脚本
  要求 60--80 每帧均 `tracking_phase=TRACKING` 且 `accepted=1`，因此会在有意冻结可信
  状态的第 70 帧报告失败。此处不修改或放宽协调者批准的门禁，不宣称候选已达到 C012
  全部晋级条件；C012 继续作为默认效果基线。尚未实施阶段 2 重捕获候选关联、阶段 3
  持续搜索控制，也未完成 car001 新运行、生产交叉编译或人工吊舱验收。

## 2026-11-12T13:38:53+08:00 recent-safe v2 三阶段 RK3588 双目标验证 HOLD

- 分支 `codex/recent-safe-occlusion-recovery-v2`。阶段 2 候选关联提交
  `d36ae22a200522314f7b8e0a4362e04e0cd97bc3`，恢复 probation 提交
  `5ce180dd4e14b04e9328150fd87bc38ca58e0c33`、`b18bb8241e39fc0f13886564d6e843eb584f0d0f`，
  阶段 3 持续有界搜索控制提交
  `b53866785d4a50855f3c2abf08ccb89f2aa69b9d`。后续实验
  `ffe8bd9a466b13de641a62b51928903e118759a7` 试图让持续单指标下降快速进入 SEARCH，
  但板端双目标均出现多次正常段误 SEARCH/误重捕获，已由
  `b8aa1006b66843a37f711e147ce4f92632279317` 明确撤销；当前分支树与已验证的
  `b538667` 相同。未修改阈值、配置或 ROI 扰动策略。
- 保留候选源码部署根为
  `/home/neardi/Project/SOT/occ-recent-safe-v2-b538667`，`source.tar` SHA-256
  `ed2f4f61014a4ef107eabe44d7a2d02b57ee2a42dc6a42f2582cedf2b36c8ed2`。
  RK3588 原生 Debug CTest 22/22 通过；主机 RKNN-off CTest 22/22、C012 门禁单测 5/5
  与 `git diff --check` 通过。板端 `build/nanotrack_v3` SHA-256 为
  `c97945d117dc6d611f222477308dfe4a7a07d2067b0c306872c091d3b557c3c2`；配置 SHA-256
  仍为 `7b038eed1dd052de5879bedbf465a2ded3cc79d5a6f0b662e39d9fd06fc15c34`
  （`calibrated=false`）。视频、detector 和三份 tracker 模型哈希均与 RS001 相同，精确清单
  位于板端 `provenance/final-inputs.sha256`。
- 最终固定 A/B ROI 运行：`car_001=992,582,37,39`、`car_002=970,517,38,38`，各
  0--270 帧。两次 runtime exit 0、schema v11 verifier PASS、AVI 1920x1080 共 271 帧；
  两次均全程 `control_valid=1` 且无 `LOST`。car001 在 185 帧重捕获，186 帧只进入一帧
  SUSPECTED，187--189 由 staged 承接并提交 recent，190--270 无 SEARCH。car002 在
  60--80 无 SEARCH，第 70 帧为单帧 SUSPECTED 且 observation control 有效；后续搜索在
  运动置信度衰减后仍保持有界 `MOTION_SEARCH` 控制。
- car001 CSV/AVI/session metadata SHA-256 分别为
  `82e23ad08e80d976ed072187ea1e0e2344a220595f2a925e3e33ce302792c89a`、
  `4f40eeb1b35b5a23d4814213a6bdeabde6587d7b5ea4f72a481bf7d4a23c0a89`、
  `e5db2fd2225e580c00a0ece936aa747780cd081ce6d2e5532809060e1cb39944`；car002 对应为
  `2c03b7de9803922e83fc75ab46e09ba08211005c7f8ff6d09eebb607abda427e`、
  `cde0c9267243483c1319b7c65454b761d1cb55575a7abdd54d3043b780b6eb75`、
  `49dbc0385e5d8da53a34e2264f72a379520a1d5448c2a7882bb8416843a8c07b`。
  大工件和完整日志保留在板端 `runs/car_001/`、`runs/car_002/`。
- full evaluator：car001 **PASS**，有效观测 mean IoU `0.752494`，重捕获恰为 185，
  evaluator JSON SHA-256
  `7b945e043c0c9c5f6c485b5f20beb5f00e361f7aa77c12cd2a75a804a5a67c3e`；car002
  可见框、连续控制、identity proxy、预测连续性和 235 帧重捕获均 PASS，但 213--216
  完全遮挡仍声明观测，`full_occlusion_semantics` FAIL，JSON SHA-256
  `886b4aead8f8595bd721c57ae155159969c8cea09ae51b7531d3757dc62101bf`。
  现有 C012 双目标脚本另因 car002 第 70 帧有意为 `accepted=0` 的 SUSPECTED 状态而在该
  单帧 FAIL；视觉框和控制未中断，但未经协调者批准不放宽该门禁。
- **结论 / Handoff**：相对 RS001，car001 的永久身份冲突、恢复后立即回滚及搜索控制失效
  已修复；car002 路牌段不再丢失，且后段可重捕获。候选仍为 **HOLD**，C012 保持正式效果
  基线和默认部署。未解决项是 car002 完全遮挡的提前识别，以及 C012 门禁是否应允许
  `SUSPECTED + observation control + accepted=0` 的单帧冻结语义；不得用已否决的单指标
  快速升级规则解决。未启动生产交叉编译或人工吊舱验收，未修改协调者所有的
  `STATUS.md` 或 `integration-contract.yaml`。
