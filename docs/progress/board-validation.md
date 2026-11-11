# RK3566 开发板验证任务进度

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
