# NanoTrack 微调任务进度

- 任务状态：`ready_for_downstream_evaluation`
- 最近核对：`2026-10-27T20:16:18+08:00`。
- 核对基线：`20fcb80c810e7ec7ce0eb2891691f88cfbd001df`（context 过渡实现开始基线）。
- 当前任务分支：`codex/nanotrack-factorial-v3`。
- 当前任务工作树：`/home/u20/single-object-tracking/.worktrees/nanotrack-factorial-v3`。
- 状态说明：既有 g5000 可交付下游评估；任务 6 主矩阵断电前完成基线及 44/72 节点。首次恢复因映射顺序引发一 ULP 误差，错误失效一个已完成节点后保护停止；当前原始 journal 保留 43 个候选及 1 个失效记录。本轮仅修复确定性与持久化浮点复核，未恢复训练，不能据部分结果选择最优 batch/GOT 或切换模型。
- 责任边界：数据准备、训练、模型选择、桌面评测，以及向转换/板端链路交付冻结模型身份。

## 已观察证据

- 保守三阶段流程冻结选择为 Stage 1、global step 5000：
  `artifacts/nanotrack_conservative_three_stage/run/stage1/candidate_stage1_g00005000_e001_s02500.pth`。
- 冻结 checkpoint SHA-256：
  `40ebbe4296938bf5d53c8605ab05e729b306fc0bce65921a57ab6613030410d1`。
- `run/selection.json` 已记录 `selection_final=true`，SHA-256：
  `a4b7a0940911cf1e064d3bdf5a312f0c35d893c71ef63374fa8b41fb6e97cc6c`。
- 主要证据：
  `artifacts/nanotrack_conservative_three_stage/REPORT.zh-CN.md`。
- 后续 GOT-10K 回放没有候选通过晋级门，保留初始化基线；它没有替换上述已冻结的三阶段选择。证据见
  `artifacts/nanotrack_got_replay_v2/REPORT.zh-CN.md`。

## 证据边界

- 原模型选择面板和隐藏集属于居中窗口 V1 协议；协议复审没有隐式重跑隐藏集，也没有改变 g5000 的最终选择。
- 现有微调报告证明桌面训练和评测，不证明该 g5000 checkpoint 已完成 ONNX 导出、RKNN 转换或 RK3566 运行。
- `DEPLOYMENT_STATUS.md` 中较早的 RK3566 NanoTrack V3 结果绑定官方权重，不能替代微调 g5000 的板端证据。

## 下游交付要求

转换或板端任务消费本模型前，必须记录：

- checkpoint 路径、大小和 SHA-256；
- 对应配置及其 SHA-256；
- 导出脚本/源代码 commit；
- ONNX 输入输出契约和数值一致性结果；
- RKNN Toolkit 版本、目标平台、量化模式和生成模型 SHA-256。

## Blockers

- 任务 6 恢复仍受独立复审门阻塞：控制器和评分源码 SHA 属于 context，修复后不能直接消费旧 journal。恢复修复已通过独立复审，显式 context 过渡机制已完成本轮实现和 CPU 回归，尚待独立复审；真实 transition manifest 尚未生成，不得手改旧 journal、重标失效记录或直接重启训练。
- 尚未在集成契约中批准 g5000 取代当前板端 NanoTrack 模型。
- 尚无绑定 g5000 的 RKNN 模型与真实 RK3566 验收记录。
- 历史控制器评分仍为 `0.70*T + 0.15*GOT + 0.15*UAV123`；新控制器已接入 `T_v3`，目前只有伪端到端结果，不能宣称新实验已实际训练或产生性能提升。
- 外部 NanoTrack 数据加载器保留历史列表索引语义；受控训练入口已可用显式 `--pair-policy` 在构造数据集前仅为匹配的 GOT 子池安装原始帧号策略，尚未用于真实新训练。
- batch `{8,32,64}` 已各完成 50 步 CUDA 容量探测，但九格主矩阵和困难样本强化对照尚未完成；探测不是等样本性能比较，不能声明 batch 64 或任何配比最优。
- 后续独立目标域留出集的可用性与隔离性尚需核查；已查看的历史隐藏集不能重新称为未触碰测试集。

## Next action

0. 当前优先事项：独立复审本轮显式 opt-in 的 context 过渡实现；批准后才按最终源码身份另行生成真实 manifest 和恢复 runner。旧 attempt、保护停止记录及断电证据保持原状；过渡实现获批前不执行 GPU 训练恢复。
1. 已在 `codex/nanotrack-factorial-v3` 基于 `main@bb297504070b3dfcf2fd4b4da05ebb1503e309db`
   编写下一轮规格与实施计划：
   `docs/superpowers/specs/2026-10-24-nanotrack-factorial-v3.md`、
   `docs/superpowers/plans/2026-10-24-nanotrack-factorial-v3.md`；已获准开始实施，任务 1 评分策略和任务 2 配对策略及审计已完成局部验证。
2. 九单元格精确样本配置和可恢复控制器已实现；验证失败诊断与训练集困难池已通过独立复审，
   任务 6 已完成三档真实 CUDA 探测，下一步执行等样本交叉实验、训练集困难样本对照和
   最终报告。没有启动隐藏集评测或 `sot-v1.mp4` 回归。
3. g5000 保持只读候选；下游 ONNX/RKNN/开发板链路不因新实验计划自动切换。

## 2026-10-27 context 过渡任务 1：实现和验证（待独立复审）

- 时间：`2026-10-27T20:16:18+08:00`；分支/工作树为
  `codex/nanotrack-factorial-v3` /
  `/home/u20/single-object-tracking/.worktrees/nanotrack-factorial-v3`。
  开始 HEAD `20fcb80c810e7ec7ce0eb2891691f88cfbd001df`，初始 tracked worktree 干净。
  输入为该提交冻结的英文 design/plan，以及已复审修复 `7b8c3caf04f795aee486cb41947de63503969b2f`。
  本提交仅包含控制器、控制器测试和本 owner 文件；结果 commit 提交后回报，可用
  `git log -1 --format=%H -- scripts/run_nanotrack_factorial.py` 查询。
- 新增 `run_screening(..., context_transition=None)` 与 `--context-transition PATH`。
  原始 `state.context` 不变；schema-1 manifest 只允许 controller/policy 两个源码 SHA
  真正变化，路径及其余完整字段逐项受同一上下文哈希约束。完整新 context 和 manifest
  绝对路径/SHA/size/content 写入 `context_transitions`，每次恢复重新核对。
- 协调者明确裁定：本次恢复只授权一个过渡；保留列表/链式数据结构，但第二次登记、
  duplicate、fork、未知 context 全部失败关闭，未来升级必须重新设计和复审。
  迁移校验及原子提交均在控制器锁内，接受记录先于 reconciliation 或任何外部命令；
  拒绝时 journal 字节不变且 runner 调用为零。
- 完成节点和 retained attempt 按自身 `identity.context_sha256` 验证，并精确核对候选与
  对应 attempt 的身份一致；旧 context 的 planned/running/interrupted 节点保留原身份、
  工件与清单，显式失效并分配唯一新 attempt。新节点绑定当前 context/policy SHA。
  baseline 始终使用原 context。只读 verifier 源码保持不变；其 record 验证消费的命令、
  模型/配置/数据/评测身份、source header、Python/backend 全是过渡强制保持一致的字段。
- 严格 TDD：初始 8 项迁移测试在生产未改时实测 **20 failures / 0 errors**（2.822 s），
  均为缺少显式接口；初次 GREEN **8 tests / OK**（15.681 s）。自检另增候选/attempt
  来源重标回归，先观察 **1 test / 2 failures / 0 errors**（0.535 s），最小修正后
  聚焦 **9 tests / OK**（16.920 s）；最终同一聚焦命令再次为 **9 tests / OK / exit 0**
  （16.138 s），日志 `/tmp/nanotrack-context-transition-focused-final.log` SHA
  `0fe3e8a992cdaed6e6de5e4d53c072c01daad7f715bed93e468f27dcbc8fa24c`。
  最终六模块历史回归 **102 tests / OK / exit 0**
  （104.889 s），覆盖最终全部迁移断言及既有恢复、命令和工件完整性检查。
- 实际相关命令（解释器 `/home/u20/single-object-tracking/.venvs/nanotrack/bin/python`）：
  `TMPDIR=/dev/shm <python> -B -m unittest tests.test_nanotrack_factorial_policy
  tests.test_nanotrack_factorial_controller tests.test_nanotrack_got_replay_controller
  tests.test_nanotrack_got_replay_reporting tests.test_nanotrack_three_stage_runner
  tests.test_nanotrack_factorial_configs -v`。日志
  `/tmp/nanotrack-context-transition-historical-final.log` SHA
  `6f89bee3b1f7fbc19d9ad2d1a65fad77ca34a88991e8ae45834ee89226972907`。
  更早的一次历史回归在源码仍调整时出现 1 项 context identity error（101 tests），
  已保留为非最终日志；以冻结源码后的上述 102 项通过结果为准。
- 本轮唯一完整 discover：`TMPDIR=/dev/shm <python> -B -m unittest discover -s tests -v`，
  **276 tests / failures=4 / errors=0 / exit 1**（108.235 s）。仍是历史
  `test_nanotrack_three_stage_configs.FormalThreeStageConfigTest` 的
  `test_formal_configs_match_quota_paths_and_training_contract` 中 stage1/stage2/stage3
  三个 subtest，以及 `test_smoke_configs_are_bounded_and_keep_policy_representative_pools`，
  全部为主资产根与 linked worktree 绝对路径比较；完整套件不是全绿。
  日志 `/tmp/nanotrack-context-transition-discover.log` SHA
  `66a81608cf48213fcab7bb689f5d81acd53192911b274994a2db0a9d7f113010`。
- 输出源码 SHA：`scripts/run_nanotrack_factorial.py`
  `3875154b61fd1d4a4f3a2f1736230f70cf2c4f020a027e9587cdd831e982a161`；测试
  `tests/test_nanotrack_factorial_controller.py`
  `96289f45a9f25ef67704aa8b4efa870334118f1a76d13e6a3b4f1624d27ee7aa`。
  policy/verifier SHA 分别仍为
  `5a6c1d6e310510d234f38c6536004f93ae9402d623d432a33936498b08e5f410` /
  `c920a15b32396ab9797f51c2d17df611478277f00e65d2edb6f9040d6f824208`。
- 边界：只操作临时合成 fixture，未修改真实 journal/attempt/工件，未生成真实 transition
  manifest，未运行 GPU 训练、隐藏集、`sot-v1.mp4`、ONNX/RKNN、板端或吊舱工作。
  `git diff --check` 与真实 CLI `--help` 通过。完整实施报告为
  `.superpowers/sdd/2026-10-27-nanotrack-context-transition/task-1-report.md`。
  下一步/Handoff：协调者组织独立复审后，才生成最终源码绑定的真实迁移清单并讨论恢复；
  不请求改变公共模型或 promotion gate，不修改公共 STATUS/契约及其他 owner 文件。

## 2026-10-27 任务 6 恢复设计：逐 attempt context 与显式过渡

- 时间：`2026-10-27T20:03:43+08:00`；基线提交
  `7b8c3caf04f795aee486cb41947de63503969b2f`。该恢复修复已经独立任务复审：spec 和质量均
  `Approved`，无 Critical/Important/Minor；评审边界是只读 diff/report，未重复执行训练或
  工件保存过程。
- 进一步核对确认：旧 journal context SHA 为
  `4f55202fd2da582bd850217c2ca19c378a76c8fcff89c4442fa80aa6b741d14f`，其中旧 controller /
  policy SHA 分别为 `925a7965765ca83b5ecde8bd7065044046ee162d30a5bc02a445e551d21e66ec` /
  `eb1360c4a6bca3d9ec0e9aec96fd5c26a16224da9db01774445527459e7acb16`；修复后对应 SHA 为
  `da27394102b157445b1896c6d82304bc0d8980fa045599751d5dea1347194c69` /
  `5a6c1d6e310510d234f38c6536004f93ae9402d623d432a33936498b08e5f410`。其他已核对 code
  identity 未变化。exact context 门正确阻止直接接管，不能通过放宽整体 context 比较绕过。
- 已选择“保留原 context、追加严格链式 context registry、按每个 attempt 的
  `context_sha256` 验证”方案；拒绝把历史身份批量改成新源码，也拒绝在旧含缺陷 commit 上
  继续训练。显式 schema-1 transition manifest 只允许 controller/policy 两个 SHA 字段变化，
  自身路径/SHA/size 持久化并在每次恢复复核。旧 context 下未完成 attempt 只能保留并失效，
  不得跨 context 续跑；新 attempt 使用新 context。
- 英文设计与计划分别为
  `docs/superpowers/specs/2026-10-27-nanotrack-context-transition-design.md` 和
  `docs/superpowers/plans/2026-10-27-nanotrack-context-transition.md`。本里程碑只冻结设计和
  TDD/复审要求，不修改 journal，不生成真实 transition manifest，不启动训练。下一步由新
  实施子代理完成单一任务并提交，再由独立子代理复审；只有最终源码身份确定后才生成真实
  manifest 和新的恢复 runner。

## 2026-10-27 任务 6 恢复修复：固定聚合顺序与有限浮点复核

- 时间：`2026-10-27T20:00:40+08:00`；分支/工作树为
  `codex/nanotrack-factorial-v3` /
  `/home/u20/single-object-tracking/.worktrees/nanotrack-factorial-v3`。
  开始 HEAD `4aea962256d0ac9cb008ac829995315ce6fc09cb`，tracked worktree 干净。
  本提交包含生产修复、回归与本 owner 记录；结果提交在提交后回报，可用
  `git log -1 --format=%H -- scripts/run_nanotrack_factorial.py` 查询。
- 断电边界为基线及 44/72 个完整节点；`b64_g30 / seed 20261011 / 2560 /
  attempt0001` 只有 12/40 条训练指标，没有有效 checkpoint/telemetry。首次恢复误失效
  `b32_g0 / seed 123456 / 2560 / attempt0001`；保护停止的 attempt0002 只有 1 条
  指标。当前 journal 为 43 个候选、1 个失效记录；没有删除或重标任何历史 attempt。
- 原因经未改生产代码的只读重算复现：已完成 attempt 的 inventory 逐字节相同，metrics
  精确相等，qualified/reasons 相同。只有 vehicle guardrail 的 baseline 从持久化
  `0.7562493078245914` 变成重算 `0.7562493078245915`，drop 从
  `0.016996178816342633` 变成 `0.016996178816342744`。`build_metrics` 先插入 aerial
  再插入 UAV123，JSON sort_keys 改变域顺序，旧 category sum 依赖输入迭代顺序，
  严格字典比较把一 ULP 舍入差误判为结果变更。
- 最小修复：policy 遍历冻结 `AERIAL_DOMAINS`；保留必需域/类别校验。控制器仅对
  persisted baseline metrics、candidate metrics/decision 使用递归等价比较，有限数值
  使用 `math.isclose(rel_tol=1e-12, abs_tol=1e-12)`；字典键集合、列表顺序/长度和
  非数值保持精确，bool 在 int 前处理，NaN/Inf 永不等价。路径、哈希、清单、源码
  context、qualification bool 和原因文本仍精确校验；不重写历史浮点或决策。

### 保留工件与日志身份

以下路径相对工件根 `/home/u20/single-object-tracking/artifacts/nanotrack_factorial_v3/`，
本轮全部只读；完整节点证据清单另见本次 SDD report。

| 路径 | SHA-256 |
| --- | --- |
| `screening/candidate_table.json` | `3f388abb0a2032a7993ed4292f322e9423e08eca4b38adb39306d8c3851a662e` |
| `task6/candidate_table-post-false-resume-20261027.json` | `3f388abb0a2032a7993ed4292f322e9423e08eca4b38adb39306d8c3851a662e` |
| `task6/screening-resume-20261027-controller.log` | `9df2def8bb8792272f9ff5b735ed635cc36d3c2c9d5ffe7c30a3543b470ed497` |
| `task6/screening-resume-20261027-supervisor.log` | `4927fae09a73262fd70f9773a3428e8f828bc1e094f03c693c87d50d3910bd9d` |
| `task6-screening-resume-20261027.py` | `3eaee3b44e1ef25ad9b48e061252a66707e4c01f5b7f6465cf48b82b90e87181` |
| `screening/nodes/b32_g0/123456/2560/attempt0001/training/candidate_factorial_g00005080_e000_s00080.pth` | `5f5eeeefa0b9c99169d36ee3988f331263ae32250232c06f5c1fb53ccf095e5d` |
| `screening/nodes/b32_g0/123456/2560/attempt0001/training/segment-telemetry.json` | `9db52938d947724f251c6853724a1c4d32abc03ec4f96f2fa15036d00ff32593` |
| `screening/nodes/b64_g30/20261011/2560/attempt0001/training/train_metrics.jsonl` | `1decb6e7c57b51a971f679c4d8c664c2c8f62038906b58873a8140a37d3078cf` |
| `screening/nodes/b32_g0/123456/2560/attempt0002/training/train_metrics.jsonl` | `3a90e15f4cda918c489c4166884de46e9ba2276bdc5eb34d247bd232699d8b21` |

### 实际测试与边界

- 严格 TDD：先新增 6 个回归方法，生产未改时最终 RED 为 **6 tests / 32 failures /
  0 errors / exit 1**（6.924 s；含 subtests）；最小修复后同命令 **6 tests / OK /
  exit 0**（7.771 s）。初次 RED 的缺 baseline 断言曾产生 KeyError，已改为明确
  assertIn 并重新观察无 error 的功能 RED，不把测试拼写/断言错误当功能证据。
- 聚焦命令：`/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B -m unittest
  tests.test_nanotrack_factorial_policy.FactorialPolicyTest.test_domain_order_and_sorted_json_preserve_exact_score_and_decision
  tests.test_nanotrack_factorial_controller.FactorialControllerTest.test_restart_reuses_complete_attempt_with_one_ulp_persisted_floats
  tests.test_nanotrack_factorial_controller.FactorialControllerTest.test_persisted_semantic_changes_are_rejected_by_record_revalidation
  tests.test_nanotrack_factorial_controller.PersistedEquivalenceTest -v`。
  日志 `/tmp/nanotrack-task6-resume-fix-red-final.log` SHA
  `4d9257ee4ae6f59296f9be401ba16995f2abe377ddda3c0c3a3a8c8be23feeb8`；GREEN 日志
  `/tmp/nanotrack-task6-resume-fix-green.log` SHA
  `dd1eacc15be5e17d13e30b26606e2a05309b87a83ac58e7433e83cec87667889`。
- 相关历史套件：`TMPDIR=/dev/shm` 加同一解释器，`-B -m unittest
  tests.test_nanotrack_factorial_policy tests.test_nanotrack_factorial_controller
  tests.test_nanotrack_got_replay_controller tests.test_nanotrack_got_replay_reporting
  tests.test_nanotrack_three_stage_runner tests.test_nanotrack_factorial_configs -v`，
  六模块实测 **93 tests / OK / exit 0**（85.729 s）。最终日志
  `/tmp/nanotrack-task6-resume-fix-historical-final.log` SHA
  `8edf3f800df5a21c31c245970bc173ecc49fcd75faa4e5c153d01097e78273ed`。首次磁盘临时目录
  运行因刷盘缓慢主动中断，保留 `/tmp/nanotrack-task6-resume-fix-historical.log`，不计为
  完成证据；改为 `TMPDIR=/dev/shm` 运行相同断言，日志使用新路径。
- 唯一完整 discover 命令：`TMPDIR=/dev/shm
  /home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B -m unittest discover -s tests -v`，
  **267 tests / failures=4 / errors=0 / exit 1**（94.335 s）。仍为历史
  `test_nanotrack_three_stage_configs.FormalThreeStageConfigTest` 的
  `test_formal_configs_match_quota_paths_and_training_contract` 中 stage1/stage2/stage3
  三个 subtest，以及 `test_smoke_configs_are_bounded_and_keep_policy_representative_pools`。
  四条均是主资产根与 linked worktree 的绝对路径比较，本轮未改对应配置/测试；完整套件
  **不是全绿**。日志 `/tmp/nanotrack-task6-resume-fix-discover.log`，SHA
  `41a35c313f24f7325b6ff094f5a2c7892395af00dc1834276dcceb643e333d0e`。
- 提交前 `git diff --check` 通过，重新计算上述 journal/恢复日志/runner 哈希仍与表格
  逐字节一致。生产 policy/controller SHA 分别为
  `5a6c1d6e310510d234f38c6536004f93ae9402d623d432a33936498b08e5f410` /
  `da27394102b157445b1896c6d82304bc0d8980fa045599751d5dea1347194c69`。
- 以上仅为 CPU 回归、伪命令驱动的真实控制器与只读旧工件重算。本轮没有恢复训练、
  新推理、hidden 内容、`sot-v1.mp4`、导出/转换、开发板或吊舱测试，没有性能结论。
  `candidate_table.json`、全部 attempt、公共 STATUS/契约及其他 owner 文件均未修改。
- 下一安全动作/Handoff：独立复审本修复后，由协调者另行设计和独立复审显式 opt-in、
  保留审计的 context 过渡及历史误失效处理。本轮保留 exact source/context 身份门，
  不引入过渡机制；Task 6 仍未完成，不因本修复允许直接恢复旧 journal。
  完整实施报告：`.superpowers/sdd/2026-10-24-nanotrack-factorial-v3/task-6-resume-fix-report.md`。

## 2026-10-26 任务 6：schema-2 主矩阵已启动（运行中）

- 时间：`2026-10-26T14:11:05+08:00`；分支/工作树不变。预检/探测里程碑提交
  `e73c91c64c8388e54a31cbf58fc09dfd7f94325d` 已完成，仅修改本 owner 文件。
  控制器在该提交的干净工作树启动；源码/context 身份与前一节完全一致。
- 实际启动时间 `2026-10-26T14:10:34.499820+08:00`；宿主 supervisor PID `2557095`，
  controller PID `2557104`，初始 aerial baseline evaluator PID `2557114`。
  宿主 `ps` 和 `nvidia-smi --query-compute-apps=pid,process_name,used_memory --format=csv`
  实际观察该 evaluator 正在 CUDA 运行，占用约 964 MiB；不是只创建目录或加载模型。
  此时尚无已完成候选节点，不能将正在进行的基线验证称为筛选完成。
- 生成 supervisor 以独立会话持续运行，日志写文件；没有需要保留的交互工具 session。
  启动记录绝对路径
  `/home/u20/single-object-tracking/artifacts/nanotrack_factorial_v3/task6/screening-launch.json`，
  SHA `4bf880766ba36e65e83862969214c094fbaac77c21f430d4f24dfccc3dd94258`；
  该记录绑定 source commit、context、探测摘要及精确启动命令/PID。
- 实际命令：

```bash
/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B /home/u20/single-object-tracking/artifacts/nanotrack_factorial_v3/task6-screening.py launch
```

该生成脚本实际调用原生产控制器，未改配置、配额或训练代码：

```bash
/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B scripts/run_nanotrack_factorial.py \
  --manifest /home/u20/single-object-tracking/.worktrees/nanotrack-factorial-v3/configs/nanotrack_factorial_v3/experiment-manifest.json \
  --output-root /home/u20/single-object-tracking/artifacts/nanotrack_factorial_v3/screening
```

- 运行根 `/home/u20/single-object-tracking/artifacts/nanotrack_factorial_v3/screening/`；
  原子状态 `candidate_table.json`（schema 2）。supervisor/controller 日志位于工件根
  `task6/screening-supervisor.log`、`task6/screening-controller.log`；当前 baseline 原始命令
  日志为 `screening/baseline/attempt0001/evaluation/aerial.log`。运行中文件尚在增长，不冻结
  临时日志/状态的 SHA。进程退出时 supervisor 写 `task6/screening-process-result.json`。
- 下一步：等待 18 条轨迹各到 10240 样本、四节点共 72 候选完成，再执行只读验证器并冻结
  `cell_summary.json` 的扩展决策；其后报告确认控制器缺口。若进程中断，保留当前根和全部
  attempt，在确认原进程已退出后重调上述相同生产命令恢复，不新建同身份训练。
  当前不执行确认、equal-update、hidden 或困难池分支，Task 6 仍未完成/未勾选。

## 2026-10-26 任务 6：冻结输入预检与三档真实 CUDA 探测

- 时间：`2026-10-26T14:08:13+08:00`；分支/工作树为
  `codex/nanotrack-factorial-v3` /
  `/home/u20/single-object-tracking/.worktrees/nanotrack-factorial-v3`。
  初始 HEAD `145f1c6cf7b30b53fd6dfbe8d54dcce85775ed5e`，初始工作树干净；本里程碑仅提交
  此 owner 文件，结果 commit 在提交后回报，不预写尚未产生的 commit ID。
- 已阅读 STATUS、集成契约、owner 进度、任务 6 brief、冻结规格以及任务 3/4 的实际清单、
  控制器、训练器和验证器。没有消费其他 owner 的新模型或板端 handoff。
- 受限环境 `nvidia-smi` 实测无法连接驱动；同机宿主执行成功，故此为设备访问边界。
  宿主实测 driver `580.178.04`、RTX 2060 6144 MiB；PyTorch `1.7.1`、CUDA `10.2`、
  cuDNN `7605`、`cuda_available=true`、一张设备，初始 allocated=0。未观察到其他 CUDA
  compute 进程；桌面图形进程占用约 225 MiB。磁盘可用约 306 GB。
- 冻结 manifest SHA `d8385c25de4fce516545ee7e7b45c30f9aa7d76af8256a0afa965d93abee99cd`；
  g5000 SHA `40ebbe4296938bf5d53c8605ab05e729b306fc0bce65921a57ab6613030410d1`；
  来源配置 SHA `cb3065982818a701a7ebb469c41b894b0a6c11e7aac6cb91dc42e5ed50547f2e`；
  pair-policy SHA `e33eb9423b08879b6b6f9068711e1617298a6fceb4a5ca4241f6a5bd45338024`；
  pair-audit SHA `69b4d5d253ef40894f34a3648af31872e5676a40ce8d8a2f98a44cf04adb1ca2`。
  `load_context` 实际绑定/核对 143 个配置、训练清单、评测列表和源码文件，context SHA
  `4f55202fd2da582bd850217c2ca19c378a76c8fcff89c4442fa80aa6b741d14f`。
  控制器 SHA `925a7965765ca83b5ecde8bd7065044046ee162d30a5bc02a445e551d21e66ec`，
  训练器 SHA `4651e28992f2e5b9384683986183c6d0f38c8bb8f3c2d9a165defe06120f2c55`。

### 实际命令与测量

工件根为 `/home/u20/single-object-tracking/artifacts/nanotrack_factorial_v3/`；以下均从任务
worktree 调用，GPU 探测使用宿主执行。生成的 runner/verifier 位于工件根，不修改生产代码。

```bash
/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B /home/u20/single-object-tracking/artifacts/nanotrack_factorial_v3/task6-execution.py preflight
/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B /home/u20/single-object-tracking/artifacts/nanotrack_factorial_v3/task6-execution.py probes
/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B /home/u20/single-object-tracking/artifacts/nanotrack_factorial_v3/task6-verify-probes.py
```

三档均使用冻结的 `b{8,32,64}_g30.yaml`、seed `123456`、GOT 原始帧号配对、`head_only`、
冻结 BN、新优化器和 sample LR，从 g5000 各执行 50 次完整 forward/backward/optimizer
update，global `5000 -> 5050`。单独 verifier 对实际 `train_metrics.jsonl` 逐项重读，核验
恰好 50 条连续更新、三类 loss 全有限以及原生 CUDA telemetry；**三档均 exit 0，无 OOM**。

| batch | 样本数 | 训练循环秒 | 进程墙钟秒 | peak allocated/reserved MiB | trainer PID |
| --- | ---: | ---: | ---: | ---: | ---: |
| 8 | 400 | 39.586 | 54.162 | 61.69 / 96 | 2552184 |
| 32 | 1600 | 163.724 | 169.023 | 236.40 / 250 | 2552670 |
| 64 | 3200 | 201.596 | 206.987 | 468.59 / 508 | 2554075 |

训练循环口径包括数据取样/增强、CUDA 训练和循环内保存，不含模型/数据集构造；进程口径
另含初始化和退出。三档样本数不同，探测时间不能直接作为主实验等样本 batch 结论。
固定单进程确定性数据加载和 head-only 策略造成较低显存使用，不因此增加未声明 batch。

- 生成 wrapper 起初错误查找 `metrics.jsonl`，而真实 trainer 输出 `train_metrics.jsonl`，
  因而全部 trainer 成功后 wrapper 自身 exit 1。原 runner、原始 result/log/checkpoint 均保留，
  没有重训或覆盖；新增单独 verifier 读取真实文件，实测 exit 0。此为工件收集路径错误，
  不是训练失败或生产代码缺陷。预检绑定的原 runner SHA 已恢复并由 verifier 实际核对。
- 预检 JSON `task6/preflight.json` SHA
  `e682500a106a4a3912ace4a4e9e3f086483cf8470c61fbafb6e1ecd7fdd32307`；
  完整验证摘要 `task6/verified-probe-summary.json` SHA
  `a0cfd16ee7ae373b1bb29797b4f1f6d819cca8a91149307e156e5099414bd1cc`。
  摘要绑定每档精确命令、PID、原始 result、metrics、telemetry 和 checkpoint SHA；
  绝对输出目录为 `task6/probes/b{8,32,64}_g30/attempt0001/`。
- 各档 probe checkpoint 文件名均为
  `candidate_factorial_probe_g00005050_e000_s00050.pth`；batch 8/32/64 SHA 依次为
  `b86f1a883b7561516eb25af97ad9a45cd8c1128ba50fe6d45e9dc6911832eb94`、
  `c549c78426a609e29f7134c5d65a48d8e2679464b18cf4cd0f7fa0476af2f79a`、
  `7b55d3b89e0ded5d23e4fe63437928f8bbec6d3afc166bc0f6e95127d6ca81fc`。
  这些只是容量探测工件，不进入主矩阵或候选排名。

### 已冻结的后续口径与边界

- 协调者在筛选结果出现前裁定，确认阶段的共同四节点为
  `10240/20480/30720/40960` 样本：筛选终点加三段等长扩展；各格/三个种子共用相同节点，
  不能逐种子选择训练时长。若此解释错误，确认调度/评测需重跑，筛选仍有效。
- 现有 schema-2 控制器只实现筛选；先完成并验证 72 个节点及冻结 expansion，随后报告
  可恢复确认路径缺口，由协调者安排单独经测试的修复，不使用无 journal 的手工续训。
- 本阶段只证明真实桌面 CUDA 容量与冻结输入预检；尚无完整主矩阵、确认、分组 bootstrap
  或 batch/GOT 最优结论。不将任务 5 hard pool 加入主矩阵；没有访问 GOT train-hidden、
  历史隐藏图像/框/预测/指标，没有运行 `sot-v1.mp4`、转换、板端或吊舱工作。
  g5000、公共 STATUS/集成契约和其他 owner 文件不变；SDD Task 6 保持未勾选。
- 下一步：提交此里程碑后立即运行原 schema-2 控制器，18 条轨迹各至 10,240 样本，
  每条固定评测 `2560/5120/7680/10240`；中断时保留 attempt 并使用同一根恢复。
  任务实施报告：`.superpowers/sdd/2026-10-24-nanotrack-factorial-v3/task-6-report.md`。

## 2026-10-26 任务 5 复审批准与 JSON EOF 加固

- 时间：`2026-10-26T09:59:54+08:00`；分支/工作树仍为
  `codex/nanotrack-factorial-v3` /
  `/home/u20/single-object-tracking/.worktrees/nanotrack-factorial-v3`，初始干净 HEAD
  `2e8daf260b991d2246966f4abf1d7167e7804a2c`。
- 同范围独立复审对 `2e8daf2` 给出 `APPROVED`，原 Major 均已关闭。唯一 Minor 为隐藏
  成员 JSON 解析器尚未拒绝顶层对象后的非空白内容。新增精确回归先实测 1 failure，
  最小修改要求顶层 JSON 对象之后仅允许空白直至 EOF；测试覆盖追加 `{}`、`[]`、垃圾
  文本均拒绝，合法尾随空白仍接受。
- RED 日志 `/tmp/nanotrack-task5-eof-red.log`，SHA
  `4748c1e8274f08ef508ec81822358fd7a26e81f253937b5bbaf7356a74672d5e`。
  运行 `python -B -m unittest tests.test_nanotrack_failure_analysis -v`，实测 **25/25 OK**
  （9.818 s），日志 `/tmp/nanotrack-task5-eof-focused.log`，SHA
  `57083d2ee08ad31df6f9b5edc99698ec7cac4b5be659f91e4d0833a3ef86ab59`。本次仅为 EOF
  检查增补，按范围复跑聚焦测试与真实池验证；上一回合全套 260 项及同四条历史路径失败
  的边界继续保留，不称本次重新运行了全套。
- builder SHA 更新为 `7cd0538c2f115b181727faf6505255ce06ad813ee0d99f8dfca0717d4d81c15b`，
  因其身份在池 inputs 中，已保留旧池为 `training-only-matched-pool-approved-2e8daf2.json`
  并重新生成当前池，不能复用旧 builder 身份。analysis、歧义队列、联系图无需重生成。
  当前池 SHA 为 `4ec7720d7874a1893c82381243bbafe89de9470fc80ca202c8cb3724fc466c30`；
  analysis SHA 仍为 `c2479f0547a5636332f37bd624be1d9302342ccb9d272235439758a4b9796f77`，
  歧义队列 SHA 仍为 `f2b259d1703eff064c62426672d68d48fccefe3c36f9e692178dca9f850917af`。
- 计数及访问边界不变：1,024 条原因-目标-帧成员关系，830 个目标轨迹帧观察，502 个源
  图像帧；不训练、不读取隐藏图像/框值/预测/指标，不切换公共模型身份。内部 SDD ledger
  在本次验证完成后勾选任务 5；下一步任务 6 的真实交叉对照，公共契约无需变化。

## 2026-10-26 任务 5 修复回合 1：来源图像、传递依赖和隐藏成员表绑定（已获复审批准）

- 时间：`2026-10-26T09:51:36+08:00`；分支/工作树仍为
  `codex/nanotrack-factorial-v3` /
  `/home/u20/single-object-tracking/.worktrees/nanotrack-factorial-v3`。
  开始 HEAD `7a8213743ce8d23d3666c3964cc14a0e1cc49456`，初始工作树干净。
  首轮评审 `NOT APPROVED` 成立：原池只相信声明的 video 身份，没有解析实际图像父目录和
  帧号；诊断依赖没有传递验证；历史隐藏面板成员表没有独立绑定。以下为修复后的证据。
- 每个候选训练图像先 resolve 到实际文件，按四域各自实际目录布局提取精确 video 文件夹
  和数字/img数字帧号，并与 audit 声明的 video/frame、规范化源组逐项一致；未知布局、
  替换成验证图像、帧号错配和指向其他视频的符号链接均失败关闭。输出保留解析后的图像身份。
- 只读池 verifier 在使用请求和重建池之前，校验 diagnosis 文件 SHA，并调用
  `verify_inputs(diagnosis['inputs'])` 传递核验其全部冻结依赖。分析器把每个显式语义事件的
  annotation 身份纳入顶层 inputs，去重排序；修改该来源会在改写输出前被拒绝。
- 池升级为 schema 2，并显式绑定
  `/home/u20/single-object-tracking/data/nanotrack_conservative_three_stage/test-aerial-hidden.json`，
  SHA `a014b957689636d26df302cb80aac6696f6fab5b45f6eea0bd6957e91f314054`。只解析
  `split`、`members` 和序列 `name/source/source_video`；其余值仅做词法跳过，不反序列化
  图像路径数组或框数组，不打开隐藏图像/预测/指标。整文件 SHA 必须读取 manifest 的字节，
  这不等于读取它引用的隐藏数据文件或执行隐藏评测。
- 历史面板是 hidden-test 划分的抽样子集，父任务据实际成员表批准按“子集一致”验证，
  不要求面板覆盖全划分。逐项要求面板组属于同域权威 hidden-test 列表，保护两者并集；
  未知域、域外组、错误 split 和缺失/不一致成员均失败关闭。实际覆盖：UAVDT 1/3、
  VisDrone-MOT 2/5、VisDrone-SOT 8/8、UAV123 0/15（该 aerial 面板不含 UAV123）。
  UAVDT split-only=`M1304,M1306`；MOT split-only=`uav0000352_05980_v,
  uav0000357_00920_v,uav0000363_00001_v`；UAV123 15 组均由划分表保护。未入面板只是历史
  抽样未选中，不是划分不一致或泄漏事件；全部精确组列表记录在池 hidden_membership.coverage。
- 实际重新生成的池计数明确区分：**1,024 条原因-目标-帧成员关系、830 个唯一目标轨迹帧
  观察（key=`source,track,frame_id`）、502 个唯一源图像帧（唯一 resolved image path）**；
  72 条轨迹、9 个规范化源组、额外排除 538 条训练轨迹的结果不变。不能把 830 称作不同
  源图像数量，也不能把同源图像内多个目标视为独立场景。

### 修复后工件及命令

生成目录仍为 `/home/u20/single-object-tracking/artifacts/nanotrack_factorial_v3/failure-analysis`。
初次提交工件已保留为 `analysis-prereview-7a82137.json`、
`ambiguity-queue-prereview-7a82137.json`、`training-only-matched-pool-prereview-7a82137.json`；
它们不能通过 schema 2 的新 provenance 验证，不作为当前可训练池。

| 当前输出 | SHA-256 |
| --- | --- |
| `analysis.json` | `c2479f0547a5636332f37bd624be1d9302342ccb9d272235439758a4b9796f77` |
| `ambiguity-queue.json` | `f2b259d1703eff064c62426672d68d48fccefe3c36f9e692178dca9f850917af` |
| `training-only-matched-pool.json` | `5834eea63933e008d655f6b649803f7ab783d65e8d0269bdea22814b5a7d1fb1` |

分析器按初次实施节相同命令重新执行；池生成实际命令增加显式隐藏成员表参数：

```bash
/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B scripts/build_nanotrack_failure_matched_pool.py \
  --root /home/u20/single-object-tracking \
  --analysis /home/u20/single-object-tracking/artifacts/nanotrack_factorial_v3/failure-analysis/analysis.json \
  --output /home/u20/single-object-tracking/artifacts/nanotrack_factorial_v3/failure-analysis/training-only-matched-pool.json \
  --historical-hidden-panel /home/u20/single-object-tracking/data/nanotrack_conservative_three_stage/test-aerial-hidden.json \
  --cap-per-request 128
```

### 修复验证

- 精确缺陷先写回归：前两类四项实测 **4 failures**（0 errors），日志
  `/tmp/nanotrack-task5-fix1-red.log`，SHA
  `e26620139c25072b6d60b1c1da09373b1bc4ad982d6a7620abb95bb189cc6f02`；最小修复后
  同四项 **4/4 OK**（1.298 s）。隐藏成员接口先因缺失新显式参数而 RED（2 errors），
  日志 `/tmp/nanotrack-task5-fix1-red-hidden.log`，SHA
  `d0c8358d495dca17a968d99439b6f7553daeb8f1a3ddf3e899d44ac53c427bc1`。
- 最终聚焦 **24/24 OK**（10.848 s），日志
  `/tmp/nanotrack-task5-fix1-focused.log`，SHA
  `39dec82f1950b71b7f832870d269bc49df3fa98f51c8809abb0614ebd88c2eb6`。
  与初次实施相同六个相关历史模块 **103/103 OK**（4.668 s），日志
  `/tmp/nanotrack-task5-fix1-historical.log`，SHA
  `dc52e5ecaa050eb4446a02d46773a4b24a47520aa90339210979b4c109970f27`。
- 本修复回合完整 discover：**260 项，4 failures，0 errors**（89.027 s），仍是历史
  `test_formal_configs_match_quota_paths_and_training_contract` 的 stage1/2/3 与
  `test_smoke_configs_are_bounded_and_keep_policy_representative_pools` 四条路径失败。
  日志 `/tmp/nanotrack-task5-fix1-full.log`，SHA
  `f8a7bdd0a16d92f7841f3c084584a805496aebd0b8bf99522a4150fb733b01c2`。没有宣称全绿。
- 新分析仍为 196 条轨迹、24 张联系图及 26 条歧义队列；语义原因边界不变。未启动 GPU
  训练、重新推理、隐藏评测、视频回归或板端/吊舱工作，公共模型身份不变。
  下一步为同范围独立复审；任务 5 保持未勾选，无需更新公共契约。
- 分析器与池生成器均已同输入重复执行；三个最终 JSON 哈希与本节表格逐字节一致，24 张
  联系图 SHA 亦与原清单逐项一致。实际只读 verifier 再次返回 `verified=true, members=1024,
  canonical_train_groups=9, excluded_training_tracks=538`；计数由再生成池重新计算，非沿用旧报告。

## 2026-10-26 任务 5 初次实施（独立评审未通过；以上方修复回合为准）

- 时间：`2026-10-26T09:35:39+08:00`；分支/工作树为
  `codex/nanotrack-factorial-v3` /
  `/home/u20/single-object-tracking/.worktrees/nanotrack-factorial-v3`。
  初始 HEAD 为 `c72a800f8ba0b13057cfd6914c8e26568e336cde`，`git status --short` 无输出。
  本节是实施者验证，任务 5 checkbox 暂不勾选；不替代独立评审。
- 新增 `scripts/analyze_nanotrack_failures.py`、
  `scripts/build_nanotrack_failure_matched_pool.py` 及 16 项测试。诊断记录逐帧 IoU、零交集段、
  持续漂移段、预测/GT 面积比、相对初始尺度、目标大小、位移及真实图像尺寸/边界线索；
  保留带 SHA 的图像身份。冻结面板中存在 null GT，按原 scoring mask 保持不计分，且打断
  漂移/零交集区间；没有补造框或把缺失 GT 推断为遮挡。

### 实际输入与模型身份

以下工件均位于绝对根目录 `/home/u20/single-object-tracking`，生成 JSON 完整记录每项输入的
绝对路径/SHA-256；这里只列关键身份。

| 输入 | SHA-256 |
| --- | --- |
| `data/nanotrack_conservative_three_stage/val-aerial.json` | `8af84a37cc29bf1efc72a37adeae30b3ebf364c9c928e51b120077414f12b77f` |
| `artifacts/nanotrack_got_replay_v2/baseline/baseline/aerial.json` | `d045f2c507a2c2f29411fc314dda9cc812d5c5c9899520ce5ca8feca0560d6c5` |
| `artifacts/nanotrack_got_replay_v2/run/candidate_table.json` | `a61c7815c9f0a002069eea886a5c250ca5922b0f242a75b1f9874e1cd0ec7636` |
| `artifacts/nanotrack_got_replay_v2/run/validation/seed20261011_stage1_c1/aerial.json` | `e159c5ec1505b04831601cd388333921ab9332373119a43fb08695a9963e4c38` |
| `data/nanotrack_conservative_three_stage/source-video-splits.json` | `9b26aa8346a3693be2e7e000d141dce2bc3611ca7bc2112f6fd639c030a96513` |

- g5000 的 checkpoint SHA 仍为
  `40ebbe4296938bf5d53c8605ab05e729b306fc0bce65921a57ab6613030410d1`；本次读取的是既有
  v2 baseline 预测，其实际推理配置为 `configs/nanotrack_got_replay_v1/stage1.yaml`，SHA
  `cb505635e81420fc9786c614de7a6eb848aa555c89de98f34522009ddeca7181`，没有以文件名相似
  假定它使用当前 v3 配置。
- 依据已保存 v2 `composite_score` 的最大值确定历史最佳为
  `seed20261011_stage1_c1`：checkpoint
  `artifacts/nanotrack_got_replay_v2/run/20261011/stage1/candidate_stage1_g00005157_e000_s00157.pth`，
  SHA `337a19f2d217d0bce50c526bd3a9106a9351045ffaa528b328205c2a8ded5498`；
  `final_gate.qualified=false`，明确为未晋级历史候选。本次不以 v3 规则改写 v2 选择。
  其实际配置 SHA 为 `3e209b1b46481df742b9d5349d0f9a1c76050e15bcc80f9608b857aa40238a4b`。

### 观察结果与人工帮助边界

- 三个现有 aerial 域各 98 条序列 × 两个模型，共 **196 条序列-模型轨迹、31,878 个帧记录**；
  其中 12 个为原面板不计分 null GT。几何线索按序列-模型计：scale_change 106、
  boundary_truncation 60、tiny_target 10、rapid_motion 4；sustained_drift 12、zero_overlap 10。
  这些线索可以重叠，不是独立失效原因计数，也不证明 v2 优于基线。
- 只在同一帧 IoU < 0.5 时出现的几何线索/有来源事件才形成训练请求，防止把跟踪正常的
  小目标或尺度变化直接当作失败来源。共 9 个 source/category/reason 请求。
- **26 个序列-模型歧义条目、15 条唯一序列**需要人工判断遮挡/相似目标/身份问题：g5000
  11 条、v2 15 条。队列保存原帧 ID；自动诊断未声称任何一次遮挡或身份切换成立。其余
  几何分析与训练样本筛选可继续自动执行，不需要人工逐帧标注整个训练集。
- 生成并核验 24 张联系图，逐图绑定源图哈希和联系图 SHA。实施者实际查看 00、04、12，
  父任务另查看 00、04：00 可见多人邻近场景中预测与 GT 渐失重叠，图像不足以确认身份
  切换；04 可见车辆框的重叠降至零及尺度变化，仍不足以定性遮挡；12 是夜间路口车辆
  场景，预测框相对 GT 扩张/偏移。均保留几何结论及歧义，不强行赋予语义原因。

### 训练池及防泄漏核验

- 先以 `source-video-splits.json` 的精确 train/validation/hidden-test 列表验证成员，再加
  保守别名排除：规范化路径、大小写、扩展名和片段后缀；VisDrone MOT/SOT 共享父 uav ID，
  UAV123 合并编号片段和 `_s` 变体。未知身份失败关闭；没有读取隐藏集预测、指标或原始帧。
- 额外排除原先精确 train 中 **538 条轨迹、29 个精确源视频**：VisDrone-MOT 515 条、
  VisDrone-SOT 18 条、UAV123 5 条。输出同时记录精确 ID 和规范化父 ID，未修改/重标
  历史划分，不把这些排除解释成已证实的数据泄漏事件。
- 从四域训练 audit 的 `hard_reasons` 与原 prepared train annotation 重新筛选，而非把
  既有 hard 池统一当成同一种困难。每个请求最多 128 条 reason-frame 成员，确定性排序。
  最终 **1,024 条原因-目标-帧成员关系、830 个唯一目标轨迹帧观察、502 个唯一源图像帧、72 条轨迹、9 个规范化源组**；
  MOT 896 条、SOT 128 条，person 256、vehicle 384、bike 256、unknown 128。
  UAVDT 的 boundary 请求没有对应 audited truncation 样本，明确记录 available=0，未伪造。
- 审计的 small/scale/motion 规则与诊断的阈值并不完全相同；此池匹配的是已有困难原因层，
  不声称数值严重程度相同。重复 reason-frame 不应被当成独立样本；任务 7 仍需冻结困难
  采样比例与按源组分布，并与无强化分支作同预算对照。当前池不加入任务 6 主矩阵。
- 只读 verifier 重新校验文件哈希、精确训练成员、来源框/帧及规范化源组不相交，再重算
  确定性选择；实际返回 `verified=true, members=1024, canonical_train_groups=9,
  excluded_training_tracks=538`。合成测试覆盖验证别名、隐藏源组排除和未知来源失败关闭。

### 输出与复现

输出目录：`/home/u20/single-object-tracking/artifacts/nanotrack_factorial_v3/failure-analysis`。

| 输出 | SHA-256 |
| --- | --- |
| `analysis.json` | `6acd7e5a720cbe489205289fec26dc7eed5b7063dfda69b66fe696f24f7575c9` |
| `ambiguity-queue.json` | `f2b259d1703eff064c62426672d68d48fccefe3c36f9e692178dca9f850917af` |
| `training-only-matched-pool.json` | `46b396c7f80175d517b22d4a83e8cb42ff46961fb4e39a1760a7d1a8c6ad9a3f` |

实际执行（工作目录为本任务 worktree）：

```bash
/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B scripts/analyze_nanotrack_failures.py \
  --root /home/u20/single-object-tracking \
  --output-dir /home/u20/single-object-tracking/artifacts/nanotrack_factorial_v3/failure-analysis --sheet-limit 24
/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B scripts/build_nanotrack_failure_matched_pool.py \
  --root /home/u20/single-object-tracking \
  --analysis /home/u20/single-object-tracking/artifacts/nanotrack_factorial_v3/failure-analysis/analysis.json \
  --output /home/u20/single-object-tracking/artifacts/nanotrack_factorial_v3/failure-analysis/training-only-matched-pool.json \
  --cap-per-request 128
/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B scripts/build_nanotrack_failure_matched_pool.py \
  --verify /home/u20/single-object-tracking/artifacts/nanotrack_factorial_v3/failure-analysis/training-only-matched-pool.json
```

- 最终相同输入的两个 CLI 均重复执行，analysis、queue、pool SHA 均与上表逐字节一致；
  24 张联系图 SHA 亦逐项核对。初始未按低 IoU 条件筛请求的试运行 JSON 保留为
  `analysis-pilot-all-geometry.json` / `training-only-matched-pool-pilot-all-geometry.json`，
  它们是已废弃的试运行口径，不作为当前池或训练证据。

### 测试及证据限制

- RED：先观察分析模块缺失；新增语义事件跨序列拒绝、可选 diagnosis 哈希绑定、null GT
  保留及低 IoU 条件训练请求时分别观察预期失败，再修正。首次测试曾引用不可用 PIL，
  随后改用项目既有 OpenCV；这项导入失败不计作功能 RED。
- 聚焦 `python -B -m unittest tests.test_nanotrack_failure_analysis -v`：16/16 通过。
  相关历史/评分/配对/数据/控制器六模块：103/103 通过（169.242 s）。最初历史命令中
  两个模块名拼写错误，已改用实际存在的 `test_nanotrack_three_stage_runner` 与
  `test_nanotrack_got_replay_controller` 重跑；不是生产实现错误。
- 本轮唯一完整 discover：`TMPDIR=/dev/shm ... -m unittest discover -s tests -v`，
  **252 项、4 failures、0 errors**（89.950 s），仍为历史
  `test_formal_configs_match_quota_paths_and_training_contract` 的 stage1/2/3 三个 subtest 及
  `test_smoke_configs_are_bounded_and_keep_policy_representative_pools` 的 worktree 绝对路径
  不兼容。日志 `/tmp/nanotrack-task5-full-suite.log`，SHA
  `8f1fbefb36ef43d19151deb8dcb3e4468d1e3ce7e32fe3ad96a9ca5bbad56eed`。没有宣称全套全绿。
- 已保存的 g5000 UAV123 baseline 目录只有 summary，成对诊断限于有逐帧结果的 UAVDT、
  VisDrone-MOT、VisDrone-SOT；没有补造 UAV123 基线预测。没有启动 GPU 训练、重新推理、
  隐藏集评测、视频回归、模型导出/转换、板端或吊舱测试。
- 实施报告：`.superpowers/sdd/2026-10-24-nanotrack-factorial-v3/task-5-report.md`。
  下一步为独立评审，再执行任务 6；公共 g5000 身份不变，无需更新公共契约。

## 2026-10-26 任务 4 评审修复回合 2：中断工件身份先验校验

- 时间：`2026-10-26T09:17:30+08:00`；分支/工作树仍为
  `codex/nanotrack-factorial-v3` /
  `/home/u20/single-object-tracking/.worktrees/nanotrack-factorial-v3`。
  开始基线为修复回合 1 提交 `a1f69eab0e0972cf426a7a3d8d15be8ec57ec441`；开始时仅有
  实施者在额度中断前留下的两个未提交文件，父任务先只读核对差异、进程与测试日志，未发现
  并发测试或半提交。
- 二次复审确认生产命令 cadence、来源 global offset 和普通评测中断恢复已经闭合，但复现
  一个剩余 Major：命令已成功、状态尚未提交时，journal 已保存原工件清单；若 checkpoint
  被替换成元数据仍合法、可以反序列化但 SHA 已改变的文件，旧恢复逻辑会采用新哈希。
- 修正后，任何中断阶段在解析 JSON 或反序列化 checkpoint 前，先逐项比较先前持久化的
  绝对路径、文件类型、SHA-256；符号链接还比较 link target，并要求所有路径位于该阶段
  destination 内。原始 `retained_inventory` 不再被恢复失败分支覆盖；失效审计同时保留
  原始清单和当前观察清单。该规则同样覆盖训练 checkpoint/遥测、评测 JSON 和日志。
- 新增三个回归：修改 checkpoint 的合法 tensor 内容但保留全部元数据；修改合法评测 JSON；
  用同字节符号链接替换普通文件。测试还 patch 了解析入口，确认身份不一致在 `torch.load`
  或 JSON parse 前即被拒绝。失败 attempt 保留原 SHA 并进入显式 invalidated 审计，后续使用
  新 attempt；有效训练或既有评测仍不重复。
- 本轮父任务重新实测：
  - 三项完整性回归：`3 tests / OK`（10.308 s）；实施者冻结日志
    `/tmp/nanotrack-task4-fix2-targeted-final.log` 为 `3 tests / OK`（10.146 s），SHA-256
    `e0006ba6eba9b0a26018bacbebd935908182ff76ffc170db57b6da907c1b43c6`。
  - `TMPDIR=/dev/shm ... -m unittest tests.test_nanotrack_factorial_controller -v`：
    `25 tests / OK`（81.037 s）；实施者冻结日志 SHA-256
    `df84c4c83f19d1bf7326e20fc9ef409b937aad0a9b91995a4b3da162b5be22af`。
  - 六个相关历史模块：`87 tests / OK`（2.891 s）；实施者冻结日志 SHA-256
    `3063b112d4feb92cadb09e31de54e6f17a964c675dcaa8c7e3ce0b94cd2a4bfa`。
  修复回合 1 已在源码冻结时运行完整 discover：233 项中只有 4 条既有工作树绝对路径失败、
  无 errors；本回合仅改控制器及其测试，按修复范围重跑上述 25+87 项，不把全套称为全绿。
- 当前 SHA-256：`scripts/run_nanotrack_factorial.py`
  `925a7965765ca83b5ecde8bd7065044046ee162d30a5bc02a445e551d21e66ec`；
  `tests/test_nanotrack_factorial_controller.py`
  `0bb5808df23d15c7688ef1f5bab6cc5c64f94a03324f7873d7160558897b9464`。
- 边界/下一步：没有启动真实 GPU 训练/评测，没有访问隐藏集或视频回归，没有产生新模型、
  ONNX/RKNN、板端或吊舱证据。修复提交为
  `5a8660fbe806d3c5f473a46f692b6892575e5f06`。同一独立评审者在磁盘临时目录复跑三项
  完整性回归，实测 `3 tests / OK`（136.625 s）；另复跑实际训练器三种 batch 预检与
  冻结 g5000 CPU 初始化，`2 tests / OK`，最终结论 `APPROVED`，无其他发现。
  任务 4 至此完成；下一步执行任务 5 的验证失败诊断和仅训练集困难样本池。公共模型身份
  不变，不请求更新公共契约。

## 2026-10-25 任务 4 评审修复回合 1：生产命令契约与逐阶段恢复

- 时间：`2026-10-25T16:33:50+08:00`；分支/工作树仍为
  `codex/nanotrack-factorial-v3` /
  `/home/u20/single-object-tracking/.worktrees/nanotrack-factorial-v3`。
  开始基线 `3cc2cf4eadafb8afd1ad5548421b0f5ddf96db1c`，初始工作树干净。
  首轮实现只有伪命令成功，未穿过真实 trainer 的 cadence/初始化检查；评审指出的三项
  Major 均成立。下面是修正后的新证据，不将前一轮伪通过解释为生产训练已经可运行。
- 修正 1：恢复 checkpoint cadence 固定为 trainer 支持的 `250`；候选 cadence 仍为
  batch 8/32/64 的 `320/80/40`。新增 trainer `--validate-only`，经过真实参数解析、
  pair-policy、checkpoint/candidate 边界及来源 checkpoint 检查后退出，不创建输出目录，
  不导入 NanoTrack 模型或调用 CUDA。实际九格来源配置中三种 batch 的首段预检均 exit 0。
- 修正 2：context 绑定实际 g5000 header（`global_step=5000`、`stage_step=5000`、
  `stage=stage1`、`epoch=1`、`policy=head_only`、`seed=123456`）。新训练阶段起点是
  global=5000、stage=0；节点 global 为 `5000+stage_updates`，seen samples 只由
  stage updates × batch 推出。首节点 stop 分别为 `5320/5080/5040`；LR 和遥测的样本
  比例不计入来源的 5000 次更新。实际冻结权重在真实 NanoTrack CPU 模型上严格加载成功，
  新优化器为空；同一来源以 initial-global-step=0 初始化确实被拒绝。
- 修正 3：`candidate_table.json` 升为 schema 2。baseline/node attempt 及全部计划命令
  在发起命令前持久化；训练 checkpoint/遥测及每个评测器成功后分别原子提交阶段状态。
  评测中断后仅重跑缺失/失败的评测，使用新重试路径，复用已验证的训练和其他评测结果；
  输出已成功但尚未来得及提交状态的阶段可按已观察的日志、哈希和元数据恢复。
  attempt ID 同时保留文件系统和 journal 中已分配的编号，即便中断前尚未生成目录也不重用。
- 所有保留 attempt（包括 canceled/invalidated/未登记孤儿）都有记录；未知 attempt 目录
  只进入有原因、有文件清单的 invalidated 审计。验证器检查全部计划/执行命令及保留日志、
  已成功阶段的哈希和身份、终态文件清单及结果/journal 一致性；未闭环 pending attempt
  阻止完整验证通过。损坏的保留 checkpoint 在反序列化前核对已记录 SHA；被废弃工件后续
  再变化会导致验证失败，或由控制器显式追加新的完整性审计，不能静默忽略。
- schema 1 兼容策略：不自动迁移、不改标签、不覆盖旧证据；明确拒绝并要求保留原目录、
  使用新的输出根。schema 1 缺少真实 global offset 和完整阶段 journal，不能补造它们。
  本项目尚无真实新实验输出，因此没有生产状态迁移或重训问题。
- RED：新增真实预检/阶段恢复/孤儿审计/schema 回归先观察 `4 failures`；补测“已分配但未
  创建目录”再观察 `5 != 4` 的 ID 冲突；保留 checkpoint 损坏回归暴露反序列化前未核对
  哈希，均已修正。伪执行器现在在每次命令开始时读取磁盘状态，要求对应 running 阶段已
  持久化。旧版耗时重放在源码修正后被终止/取代，其日志不充当最终验证证据。
- 最终局部验证：
  `TMPDIR=/dev/shm /home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B -m unittest tests.test_nanotrack_factorial_controller -v > /tmp/nanotrack-task4-fix-controller-final.log 2>&1`，
  实测 `22 tests / OK`（70.606 s）；相关历史回归实测 `87 tests / OK`（3.541 s），
  日志 `/tmp/nanotrack-task4-fix-historical.log`。此外磁盘 `/tmp` 上的已分配 ID/孤儿/损坏
  重放验证为 `2 tests / OK`（259.446 s），日志 `/tmp/nanotrack-task4-fix-edge.log`。
  为减少事务测试磁盘刷写，最终完整/局部套件只将测试临时工件放在内存临时目录，自动清理；
  不修改源码或测试断言，原子文件操作仍实际执行。
- 真实来源命令预检日志 `/tmp/nanotrack-task4-fix-command-validation.log` 记录三种 batch
  均 validated=true，绑定 143 个输入文件；context SHA-256
  `d0156c770f20ad3f4cac53ba1f15aeeae5a16a27e6fea63ff9ece26d0fd0ee33`。
  CLI help 均 exit 0。冻结 manifest SHA 仍为
  `d8385c25de4fce516545ee7e7b45c30f9aa7d76af8256a0afa965d93abee99cd`，
  g5000 权重 SHA 仍为 `40ebbe4296938bf5d53c8605ab05e729b306fc0bce65921a57ab6613030410d1`。
- 本轮源码 SHA-256：控制器
  `5bfd6af6514344d3ccbd088b1eb75326e1cc5c1ae8b19c7f7535862b937c15f0`；验证器
  `c920a15b32396ab9797f51c2d17df611478277f00e65d2edb6f9040d6f824208`；训练器
  `4651e28992f2e5b9384683986183c6d0f38c8bb8f3c2d9a165defe06120f2c55`；新控制器测试
  `0eca501d24d789347943ad721fabbee7a71fd4a0e5adfa42740ae1c831f242e2`。
- 源码冻结后完整 discover 本修复回合仅运行一次：
  `TMPDIR=/dev/shm /home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B -m unittest discover -s tests -v > /tmp/nanotrack-task4-fix-discover.log 2>&1`，
  实测 `233 tests / failures=4 / exit 1`（74.724 s），无 errors。四条仍为历史
  `test_nanotrack_three_stage_configs.FormalThreeStageConfigTest` 的
  `test_formal_configs_match_quota_paths_and_training_contract` 中 stage1/stage2/stage3
  三个 subtest，以及 `test_smoke_configs_are_bounded_and_keep_policy_representative_pools`，
  均是主资产根与 linked worktree 的绝对路径断言；没有新增失败，不能宣称全套通过。
  日志 SHA-256 `6ca29401963cc019b1b3c05b2e2e6f2fd3abd3d5cd8e7bd52b0e13d1a0f87d17`。
- 边界/下一步：以上是预检、CPU 权重初始化与伪端到端证据，没有真实 GPU 训练/评测，
  没有隐藏集/视频回归或模型改选；等待独立复审后再执行任务 5/6。结果提交在提交后回报，
  可通过 `git log -1 --format=%H -- scripts/run_nanotrack_factorial.py` 查询。
  Handoff：消费新 schema 2 和 source-offset/阶段遥测契约；不请求改变公共模型契约，
  不修改 STATUS、公共契约或其他 owner 文件。

## 2026-10-25 任务 4：可恢复控制器、精确样本 LR 和只读验证器

- 时间：`2026-10-25T16:00:05+08:00`；分支 `codex/nanotrack-factorial-v3`，工作树
  `/home/u20/single-object-tracking/.worktrees/nanotrack-factorial-v3`，开始基线
  `97825eefbd269d8fb5fa9391e3e4a0e312b7587a`，开始时工作树干净；结果提交在提交后回报，
  可用 `git log -1 --format=%H -- scripts/run_nanotrack_factorial.py` 查询。
- 新增 `scripts/run_nanotrack_factorial.py` 和 `scripts/verify_nanotrack_factorial.py`；
  初筛严格为九格 × 两种子 × `2560/5120/7680/10240` 样本节点。batch 8/32/64
  对应间隔 `320/80/40` 次更新；首次从冻结 g5000 初始化并新建优化器，后续从已验证的
  前一 checkpoint 精确恢复。原子 JSON 状态绑定配置、源码、初始化/输出 checkpoint、
  pair-policy/audit、评分策略、命令、日志、评测输出和段遥测的哈希。
  有效节点重复调用不重跑；损坏节点及同种子后续节点使用新 attempt 目录重跑，保留旧工件及
  invalidated 记录。输入或已持久化身份漂移在修改状态前失败。
- 两个种子在 10240 样本都过 guardrail 后才计算格子的平均 `T_v3`；扩展集合取最佳格及
  距最佳不超过 `0.002` 的安全格，最多三个，并列按格名。无安全格写 `no_safe_cell`；
  初筛结果始终 `promotion_authorized=false`。只读验证器复核完整矩阵、配额、样本/更新、
  两种子、有限指标、全部有效工件哈希、评分版本和确定性选择；拒绝隐藏集/视频参数。
- 实现时发现历史 `step` LR 使用 `step/(total_steps-1)`，会使不同 batch 在相同样本边界
  的 LR 有细小差别。按本轮接口裁定新增兼容选项 `--lr-schedule sample`：更新前已见样本
  `s=stage_step*batch_size`，预算 `N=40960`，使用
  `exp(log(start)*(1-s/N)+log(end)*s/N)`。相同更新前样本边界获得完全相同的浮点 LR；
  `s=N` 是末端值，不额外执行更新。旧 `epoch/step` 语义不变；checkpoint 与节点身份
  显式绑定 `sample` 及该约定。
- 新训练入口在每个 sample 段边界原子写 `segment-telemetry.json`：实际 batch、段起止
  样本/更新、首/末/下一边界 LR、loss 有限性/数量、训练循环墙钟时间、CUDA 峰值
  allocated/reserved bytes，以及配置、pair-policy、输入 checkpoint 哈希。
  batch64 的 40-update 段也生成该文件。真实执行必须有实测 CUDA 内存；测试明确标记
  `execution_backend=fake_cpu_fixture`，内存为带原因的 unavailable，不能当作真实训练证据。
- 冻结输入仍为任务 3 manifest
  `/home/u20/single-object-tracking/.worktrees/nanotrack-factorial-v3/configs/nanotrack_factorial_v3/experiment-manifest.json`
  SHA-256 `d8385c25de4fce516545ee7e7b45c30f9aa7d76af8256a0afa965d93abee99cd`；
  初始模型 SHA-256 `40ebbe4296938bf5d53c8605ab05e729b306fc0bce65921a57ab6613030410d1`，
  pair-policy SHA-256 `e33eb9423b08879b6b6f9068711e1617298a6fceb4a5ca4241f6a5bd45338024`，
  audit SHA-256 `69b4d5d253ef40894f34a3648af31872e5676a40ce8d8a2f98a44cf04adb1ca2`。
  只读真实输入 preflight 已成功读取九格和 143 个绑定文件，确认 GOT 官方验证输入实际位于
  `/home/u20/dataset/GOT-10K/val_data/val/list.txt`。
- 本轮输出源码 SHA-256：`run_nanotrack_factorial.py`
  `61e1159ed0e10c71a890cff539b8445b08038e9a5e763855dacb8e95b9e96296`；
  `verify_nanotrack_factorial.py`
  `47017a2d2c125c91c1b3a115fc82279b1e4685cf14ddd20618c5afda1b739b16`；
  `train_nanotrack_uav123.py`
  `7ea57630ad8e33bdfc71d7a7640dca71a91f1b225b70c115511f32eae40b48f2`。
- TDD 先观察 8 项因控制器缺失而失败；样本 LR 新接口/CLI 先观察 2 项失败，遥测接口与
  节点缺遥测再观察 2 项失败后实现。最终命令：
  `/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B -m unittest tests.test_nanotrack_factorial_controller -v > /tmp/nanotrack-task4-controller.log 2>&1`，
  实测 `15 tests / OK`（166.843 s）；覆盖 72 节点、更新边界、中断恢复、损坏后缀恢复、
  同输入重复调用字节/mtime 不变、验证器只读、版本/指标/身份篡改、样本 LR 与
  batch64 段遥测等。相关历史测试共 `87 tests / OK`（3.114 s），日志
  `/tmp/nanotrack-task4-historical.log`；其中历史 CLI 帮助断言随新增合法选项更新。
- 完整 discover 本轮仅运行一次：
  `/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B -m unittest discover -s tests -v > /tmp/nanotrack-task4-discover.log 2>&1`，
  实测 `226 tests / failures=4 / exit 1`（355.675 s），无 errors。四条均为已知历史
  `test_nanotrack_three_stage_configs.FormalThreeStageConfigTest` 中正式配置的
  `stage1/stage2/stage3` 路径断言和 smoke 配置路径断言，均比较主资产根与 linked worktree
  绝对路径；没有新增失败，完整套件并非全绿。该日志 SHA-256
  `9a37271c01f2d8737fedb0725f6a185585e25635f1563692391479895eb467a3`。
- 边界：本轮没有启动真实 GPU 训练/评测，没有隐藏集、视频回归、ONNX/RKNN、板端或吊舱
  验证；没有新模型结果。仅实现初筛控制器和扩展决策，40,960 样本/第三种子确认及性能
  bootstrap 仍属于任务 6。原始图像内容没有逐文件新增哈希；当前身份边界为已有训练清单、
  验证 panel/split/list、模型/配置和源码哈希。下一步为任务 4 独立评审，再进入任务 5/6。
  Handoff：集成本任务提交后消费新控制器和 sample 遥测接口；不请求切换 g5000、不请求
  修改公共契约，也不修改其他 owner 的任务文件。

## 2026-10-25 任务 3：九格精确样本配置生成

- 时间：`2026-10-25T15:32:12+08:00`；分支 `codex/nanotrack-factorial-v3`，工作树
  `/home/u20/single-object-tracking/.worktrees/nanotrack-factorial-v3`，开始基线
  `b831713865d118b4e17856465da1f9251447233a`，开始时 `git status --short` 无输出。
  结果提交可在提交后用 `git log -1 --format=%H` 查询，本正文不预写提交哈希。
- 已核对公共 `STATUS.md`、集成契约和本任务进度；输入资产根固定为
  `/home/u20/single-object-tracking`。冻结初始化 checkpoint SHA-256
  `40ebbe4296938bf5d53c8605ab05e729b306fc0bce65921a57ab6613030410d1`，
  来源 Stage 1 配置 SHA-256 `cb3065982818a701a7ebb469c41b894b0a6c11e7aac6cb91dc42e5ed50547f2e`，
  配对策略 `/home/u20/single-object-tracking/artifacts/nanotrack_factorial_v3/pair-audit/pair-policy.json`
  SHA-256 `e33eb9423b08879b6b6f9068711e1617298a6fceb4a5ca4241f6a5bd45338024`，
  审计 `audit.json` SHA-256 `69b4d5d253ef40894f34a3648af31872e5676a40ce8d8a2f98a44cf04adb1ca2`。
- 新增 `scripts/generate_nanotrack_factorial_configs.py`（SHA-256
  `bf5484eb387059c7ba60de869d0adc20dd393be657110164d0061012eab91b44`）和
  `tests/test_nanotrack_factorial_configs.py`（SHA-256
  `6983e589bd735ea146e92bd17324743c6a4a21185d1bb4ba037cf1ef4252fe95`）。
  先写九格手算字面配额测试，实测 RED 为 `ModuleNotFoundError` / exit 1；实现后
  `2 tests / OK`。最大余数使用精确分数，并列按数据项名称升序。每格共 40960 样本，
  初筛 10240，每 2560 样本评测；batch 8/32/64 对应完整更新 5120/1280/640，
  评测间隔 320/80/40，LR 均为 `2e-5 -> 8e-6`。GOT 0% 配置无 GOT 条目；
  10% 为 OTHER/PERSON/VEHICLE `1366/1365/1365`，30% 各 4096。
- 输出目录 `/home/u20/single-object-tracking/.worktrees/nanotrack-factorial-v3/configs/nanotrack_factorial_v3/`：
  `b8_g0.yaml` `493c4e8896b6c4113cf9a5637ea793c999019206f66e614d38935193642be128`；
  `b8_g10.yaml` `f26e4491f18444246cca95d9a8cf33146c4c099048564f880da2f38b873de189`；
  `b8_g30.yaml` `9fecd21c0a0879307c02dbb44679ad06dd134bb9d865b1e3864a3d8aab60dd5c`；
  `b32_g0.yaml` `8787819602a6938f33225b0aae4e3da7eae563e8189a6b2d76b10ab1d8f85b8a`；
  `b32_g10.yaml` `c0bcf47f87e94c3e73656f378c8bb4d47f697655dec2fe7c959308fbff0bf797`；
  `b32_g30.yaml` `d2059cdb1d434328c9507d275ec6fbb3bdd617131b91c197f49fab8fcd62e9f3`；
  `b64_g0.yaml` `3ddae5568c65e4a5b727dcb74cc735c95c05d8edd680b0cf1b92980f00fa5926`；
  `b64_g10.yaml` `a305402d974c8e09199bb1732bb446f235c96afa678691c2cc1c3d864f610a8e`；
  `b64_g30.yaml` `b61666e811640347ec89c0294144601c710747862abf9368f947ee0ec24ab2ba`；
  `experiment-manifest.json` `d8385c25de4fce516545ee7e7b45c30f9aa7d76af8256a0afa965d93abee99cd`。
  清单记录逐格整数配额、样本/更新、LR、种子、配置哈希及输入路径/哈希，并绑定来源配置；
  产物运行根是 `/home/u20/single-object-tracking/artifacts/nanotrack_factorial_v3`。
- 实际命令：`/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B -m unittest tests.test_nanotrack_factorial_configs -v`
  （RED 后 GREEN）；`/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B scripts/generate_nanotrack_factorial_configs.py`
  （生成九格）；以同一解释器 `-B -c` 逐份加载上述 YAML，安装任务 2 策略后真实构造
  `BANDataset`，每格取 `dataset[0]`。九格均 `len=40960`，模板 `[3,127,127]`、
  搜索图 `[3,255,255]`、分类 `[15,15]`、回归 `[4,15,15]`、框 `[4]`，所有数组有限；
  退出时恢复配对补丁。外部 `dataset.py` 前后 SHA-256 均为
  `cd6b925b12f2879d5979c2c1ee8229317ea4bcdc7ce6e8fface4f9d0f2d39373`。
- 相关 data-loader、历史 GOT 配置与训练测试命令：
  `/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B -m unittest tests.test_nanotrack_factorial_configs tests.test_nanotrack_three_stage_data tests.test_nanotrack_got_replay_data tests.test_nanotrack_got_replay_configs tests.test_nanotrack_training -v`，
  实测 `71 tests / OK`。历史三阶段配置单测单独运行 `6 tests / failures=4`，失败为工作树
  绝对路径比较。提交前完整 discover **仅运行一次**：
  `/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B -m unittest discover -s tests -v > /tmp/nanotrack-task3-discover.log 2>&1`，
  实测 `211 tests / failures=4 / exit 1`，无错误；四条均是前述历史三阶段
  `stage1/stage2/stage3` 及 smoke 路径断言，与任务 2 历史失败相同；完整套件并非全绿。
- 边界与下一步：本轮只完成配置生成和真实数据加载单样本烟测，没有优化/反传、桌面评测、
  GOT train-hidden、`sot-v1.mp4`、ONNX/RKNN、板端或吊舱验证。任务 4 控制器需消费并锁定
  本清单及配对审计身份，在同种子/GOT 比例下保证跨 batch 样本顺序和逐样本增强种子一致。
  Handoff：协调者只需集成本任务提交；不请求修改公共契约或其他任务进度文件。

## 2026-10-25 任务 2 评审修复回合 1：采样补丁生命周期

- 时间：`2026-10-25T00:51:02+08:00`。
- 分支/工作树：`codex/nanotrack-factorial-v3` /
  `/home/u20/single-object-tracking/.worktrees/nanotrack-factorial-v3`。
- 评审基线：`bcb9e1cc22d4209c19952a11210d8f7ea3959358`；开始时工作树干净，
  结果提交在提交后回报协调者，可用 `git log -1 --format=%H` 查询。
- 评审发现已复现：带策略调用 `main()` 后，共享 `SubDataset` 类的补丁没有恢复，
  同进程第二次无策略运行仍按原始帧号采样，但 checkpoint 策略身份为空。
- 最小修正：新增幂等 `restore_pair_policy(subdataset_class)`，恢复保存的上游方法并
  删除补丁备份；训练入口用 `ExitStack` 管理每次 `main()` 的生命周期，安装成功后
  立即注册恢复回调，正常返回（含内部提前返回）及异常传播均执行恢复。
  原训练正文移到内部 `_main`，逻辑与采样随机流未变。
- RED：先新增两项集成回归，真实连续调用两次 `main()`（首次带策略，第二次无策略），
  分别覆盖第一次正常返回与构造数据集时报错；均实测第二次仍返回补丁配对结果，
  `Ran 2 tests in 0.021s` / `FAILED (failures=2)` / exit 1。
  再新增恢复 API 测试，观察 `Ran 1 test` / `FAILED (failures=1)` / exit 1，
  原因为缺少明确恢复接口。测试没有以模拟的恢复结果代替真实行为。
- 精确测试命令（均在上述工作树执行）：

```bash
/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B -m unittest tests.test_nanotrack_pair_sampling.TrainerPairPolicyTest.test_normal_main_exit_restores_policy_before_same_process_legacy_run tests.test_nanotrack_pair_sampling.TrainerPairPolicyTest.test_exceptional_main_exit_restores_policy_before_same_process_legacy_run -v
/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B -m unittest tests.test_nanotrack_pair_sampling.PairSamplingTest.test_explicit_restore_is_idempotent_and_allows_fresh_reinstallation -v
/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B -m unittest tests.test_nanotrack_pair_sampling tests.test_nanotrack_training tests.test_nanotrack_got_replay_training -v
```

- GREEN：第三条实测 `Ran 54 tests in 2.175s` / `OK` / exit 0。
  覆盖正常/异常退出后的恢复、同进程下一次无策略调用、航拍路径、策略重装、重复恢复、
  恢复后重新安装，以及历史训练/resume。构造测试使用空训练阶段正常退出，未运行 GPU 训练。
- 本修复没有重复完整 discover；上一轮 `206 tests / failures=4` 的历史路径失败记录保留，
  本轮聚焦通过不表示完整套件全绿。`git diff --check` 通过。
- 本工作树的最新输出 SHA-256：
  `scripts/nanotrack_pair_sampling.py`：`a1cd1711321261a8b89becbf9c7dbd145a653450f6f2ab1706c2b52191a05dd5`；
  `scripts/train_nanotrack_uav123.py`：`6a011f13860a3572f8b873f8c4cff1972f7443d016c249b5302e55d0919eef57`；
  `tests/test_nanotrack_pair_sampling.py`：`c223c86513c0217d8bcedd26542b0bf3ed9e54cfa1742325c1eb6ba553674a5b`。
- 外部输入 `/home/u20/single-object-tracking/repos/siamtrackers/NanoTrack/nanotrack/datasets/dataset.py`
  在实现前后均为 `cd6b925b12f2879d5979c2c1ee8229317ea4bcdc7ce6e8fface4f9d0f2d39373`，未修改。
  既有 `/home/u20/single-object-tracking/artifacts/nanotrack_factorial_v3/pair-audit/audit.json`
  仍为 `69b4d5d253ef40894f34a3648af31872e5676a40ce8d8a2f98a44cf04adb1ca2`，同目录
  `pair-policy.json` 仍为 `e33eb9423b08879b6b6f9068711e1617298a6fceb4a5ca4241f6a5bd45338024`。
- 边界：只保证训练入口本轮调用的补丁退出恢复；未开展 GPU 训练、隐藏集、板端或吊舱验证，
  未改审计口径、模型、策略文件身份、公共契约或其他任务文件。
- 下一步/Handoff：协调者复核此修复提交后再继续任务 3/4；直接调用底层安装 API 的调用者
  也须在退出时调用恢复 API，训练入口已统一处理，不再依赖进程退出。

## 2026-10-25 任务 2：GOT 原始帧号配对策略与只读审计

- 时间：`2026-10-25T00:44:15+08:00`。
- 分支/工作树：`codex/nanotrack-factorial-v3` /
  `/home/u20/single-object-tracking/.worktrees/nanotrack-factorial-v3`。
- 开始基线：`5713e3eb9875f78a0410012d3ed322f7bf9e7c4f`，初始工作树干净。
  结果提交在提交后回报协调者，可用
  `git log -1 --format=%H -- scripts/nanotrack_pair_sampling.py` 查询；本正文不虚构自身提交。
- 已读 STATUS、任务进度、集成契约及任务 2 简报；不消费其他任务的新模型或板端结果。
  公共契约中 g5000 身份不变，本任务没有读取或生成模型/二进制。
- 开始及实现后均核对外部
  `/home/u20/single-object-tracking/repos/siamtrackers/NanoTrack/nanotrack/datasets/dataset.py`，
  SHA-256 均为 `cd6b925b12f2879d5979c2c1ee8229317ea4bcdc7ce6e8fface4f9d0f2d39373`。
  没有编辑外部 checkout 的任何文件；运行 Python 时使用 `-B`。

### 接口、身份及历史兼容

- 新增 `scripts/nanotrack_pair_sampling.py`：`eligible_search_frames` 支持
  `original_frame_id` 和 `list_position`，前者按原始帧差（含边界）选择，range 0
  仍保留模板本帧；无序、重复、空帧列表及非法范围/模式直接失败。
- 策略文件是非空 JSON 前缀映射，只允许 `GOT_` 开头的前缀和
  `original_frame_id` 模式，重复键、未知模式及航拍前缀直接失败。
  `install_pair_policy` 只替换当前进程的方法；匹配 GOT 时复用 NumPy 全局随机流，
  保留 track/template/search 的选择顺序。航拍与未匹配 GOT 直接调用原方法。
- `scripts/train_nanotrack_uav123.py --pair-policy <JSON>` 在数据集构造前安装策略。
  `pair-policy-provenance.json` 保存策略版本、绝对路径、映射和精确文件字节 SHA-256；
  periodic/candidate/epoch/last/resume checkpoint 均传递 `pair_policy_sha256`。
  CLI 在外部导入前检查 resume 身份，restore 在加载 model/optimizer/RNG 或截断指标前
  再检查；不同哈希、丢弃策略、由无策略旧 checkpoint 直接 resume 到新策略均失败。
  新 stage 权重初始化仍可沿用旧权重；无 `--pair-policy` 时保持旧采样和元数据形状。
- 策略版本：`nanotrack-got-original-frame-id-v1`。
  策略路径 `/home/u20/single-object-tracking/artifacts/nanotrack_factorial_v3/pair-audit/pair-policy.json`，
  内容为 `{"GOT_": "original_frame_id"}`（格式化文件含换行），SHA-256
  `e33eb9423b08879b6b6f9068711e1617298a6fceb4a5ca4241f6a5bd45338024`。

### 审计输入、命令及输出

审计只读现有 GOT train normal 清单，不读取图片、GOT train-hidden 或验证集。
隐藏路径在打开前拒绝；数字帧键按上游非正 bbox 过滤规则处理。三个输入的绝对根为
`/home/u20/single-object-tracking/data/nanotrack_got_replay_v1/got/normal/`：

| 文件 | SHA-256 | 序列/轨迹数 | 有效帧引用数 |
| --- | --- | ---: | ---: |
| `other.json` | `975f0b915347503d48d5e524a238e53eec8a5ec07e09feb73f33715be6e03a9d` | 4579 | 219783 |
| `person.json` | `022f8e05c1dcffa59f8202efeba8f36c33d6a9bc8c3efd2e6a57650eb56ce54c` | 2156 | 103488 |
| `vehicle.json` | `3c53c70e05d22bffe76b2df11a77767a44d9ae63fcb5ee4b6ea8f4d89c5df7a3` | 2117 | 101519 |

在本任务工作树实际运行：

```bash
/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B scripts/audit_nanotrack_frame_pairs.py --manifest GOT_OTHER_NORMAL=/home/u20/single-object-tracking/data/nanotrack_got_replay_v1/got/normal/other.json --manifest GOT_PERSON_NORMAL=/home/u20/single-object-tracking/data/nanotrack_got_replay_v1/got/normal/person.json --manifest GOT_VEHICLE_NORMAL=/home/u20/single-object-tracking/data/nanotrack_got_replay_v1/got/normal/vehicle.json --frame-range 100 --pair-policy /home/u20/single-object-tracking/artifacts/nanotrack_factorial_v3/pair-audit/pair-policy.json --output-dir /home/u20/single-object-tracking/artifacts/nanotrack_factorial_v3/pair-audit
```

- 输出 `/home/u20/single-object-tracking/artifacts/nanotrack_factorial_v3/pair-audit/audit.json`，
  SHA-256 `69b4d5d253ef40894f34a3648af31872e5676a40ce8d8a2f98a44cf04adb1ca2`，exit 0。
  策略副本、审计 JSON 均在 Git 忽略的 artifacts 根，不提交。
- 统计口径为穷举有序候选对，每个候选对等权；P50/P90/P95 使用线性分位数。
  `same_frame_proportion` 是同帧候选对占比；额外给出均匀选择模板、再均匀选择候选时的
  `uniform_template_same_frame_probability`，不混称为真实 quota 加权训练频率。

| 子池 | 旧→新候选对 | 旧 gap P50/P90/P95/max | 新 gap P50/P90/P95/max | 排除旧候选比例 |
| --- | ---: | --- | --- | ---: |
| OTHER | 10549233→9961595 | 30/80/107/920 | 28/68/79/100 | 0.0557043341 |
| PERSON | 4967424→4042234 | 43/147/214/1200 | 34/79/88/100 | 0.1862514655 |
| VEHICLE | 4869755→4217105 | 34/120/170/1480 | 29/72/84/100 | 0.1340211161 |

JSON 同时保存候选数 min/max/mean、同帧比例、清单路径/哈希、序列/轨迹/帧数和策略身份。
新策略同帧候选占比分别为 0.0220630331/0.0256016846/0.0240731497，均匀模板同帧概率
分别为 0.0233608871/0.0301259462/0.0271098800。

### 测试与源码身份

所有命令均在本任务工作树执行，解释器为 `/home/u20/single-object-tracking/.venvs/nanotrack/bin/python`：

```bash
/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B -m unittest tests.test_nanotrack_pair_sampling -v
/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B -m unittest tests.test_nanotrack_pair_sampling.TrainerPairPolicyTest.test_main_installs_policy_and_records_provenance_before_dataset_construction -v
/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B -m unittest tests.test_nanotrack_training tests.test_nanotrack_got_replay_training -v
/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B -m unittest tests.test_nanotrack_pair_sampling.TrainerPairPolicyTest.test_no_policy_preserves_historical_metadata_shape -v
/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B -m unittest tests.test_nanotrack_pair_sampling tests.test_nanotrack_training tests.test_nanotrack_got_replay_training -v
/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B -m unittest discover -s tests -v > /tmp/nanotrack-task2-discover.log 2>&1
```

- RED：最初 14 tests / 14 failures / exit 1，缺少两个新模块及命令行参数；随后新增
  构造顺序/溯源测试，单独 1 test / 1 failure / exit 1，原因同为缺失模块。
- 首次 GREEN：15 tests / OK / exit 0。
- 历史训练回归首轮 35 tests / 1 failure，具体为
  `test_nanotrack_training.NanoTrackTrainingPolicyTest.test_resume_checkpoint_metadata_and_python_numpy_torch_rng_restore`：
  无策略返回多出空哈希字段。本任务新增历史形状测试，亲见 1 test / 1 failure / exit 1，
  然后最小修正为只在有显式策略时新增身份字段；没有改历史测试期望。
- 最终聚焦配对与历史训练/resume 回归：51 tests in 2.114s / OK / exit 0。
  新增测试共 16 项，手算 `[1,50,200]`、range 60 的旧候选对 9、新候选对 5、排除 4/9。
- 实际导入外部 `SubDataset`，以 `__new__` 和内存 bbox 清单构造最小实例，seed 23 后调用
  100 次真实 `get_positive_pair`，逐对断言原始帧差不超过 60；观察
  `REAL_EXTERNAL_SUBDATASET_ADAPTER: 100 pairs passed; no images loaded`。
- 完整 discover 只运行一次：206 tests in 158.673s / FAILED (failures=4) / exit 1，
  没有 error。按顶层方法计 204 项通过、2 项失败；四条失败记录中含同一方法的三次
  stage 子测试，名称为：
  - `test_nanotrack_three_stage_configs.FormalThreeStageConfigTest.test_formal_configs_match_quota_paths_and_training_contract (stage='stage1')`
  - `test_nanotrack_three_stage_configs.FormalThreeStageConfigTest.test_formal_configs_match_quota_paths_and_training_contract (stage='stage2')`
  - `test_nanotrack_three_stage_configs.FormalThreeStageConfigTest.test_formal_configs_match_quota_paths_and_training_contract (stage='stage3')`
  - `test_nanotrack_three_stage_configs.FormalThreeStageConfigTest.test_smoke_configs_are_bounded_and_keep_policy_representative_pools`
  四条均为历史 `/home/u20/single-object-tracking/artifacts/...` 与当前工作树
  `.worktrees/nanotrack-factorial-v3/artifacts/...` 的路径比较，与任务 1 完整套件的
  四条历史失败相同，本任务未改这些配置或测试。**完整套件不是全绿**。
  完整日志在 `/tmp/nanotrack-task2-discover.log`；提交前 `git diff --check` 通过。
- 本任务输出源码绝对根为本工作树，SHA-256：
  `scripts/nanotrack_pair_sampling.py`：`8473a4c3add5877167388919ab90efdc88c8068c9e2e575db5ca941302c5d053`；
  `scripts/audit_nanotrack_frame_pairs.py`：`f73bdc52402a4484a15b983bf79f6c4b2ab23e16d3873aec0de96c2b8134d42e`；
  `scripts/train_nanotrack_uav123.py`：`e42750b6047c9a15e93ca12710f49c74ba15cab9e31dabbd8613ffeabee841ee`；
  `tests/test_nanotrack_pair_sampling.py`：`99289ceb55450c8948974be6aaee062960c88d6e452da7b3baba2c6513b5b331`。

### 边界、下一步和 Handoff

- 证明的是受控配对/审计、checkpoint 身份与历史 resume 兼容；没有新 GPU 训练、桌面推理、
  GOT train-hidden 评测、`sot-v1.mp4`、ONNX/RKNN、开发板或吊舱验证，不修改 g5000 身份。
- 下一步：任务 3 生成九单元格精确样本配置，显式传入本策略绝对路径和文件哈希；任务 4
  控制器在状态中锁定 `audit.json` 与策略身份，恢复时同样校验。旧策略 checkpoint 若需
  用作新实验起点，应走权重初始化而非 exact resume。
- Handoff：仅请求协调者集成本任务提交并消费上述受控接口/策略身份，不请求修改公共
  STATUS、集成契约或其他任务文件；规格与计划保持英文且本任务未改动。

## 2026-10-25 文档语言格式修正

- 时间：`2026-10-25T00:32:16+08:00`。
- 分支/工作树：`codex/nanotrack-factorial-v3` /
  `/home/u20/single-object-tracking/.worktrees/nanotrack-factorial-v3`。
- 开始基线：`916cb101c48ff50678bd8c42be04cbb92da50b95`（任务 1 已完成的评分策略提交）；
  初始 `git status --short` 无输出。结果提交在提交后回报协调者，可用
  `git log -1 --format=%H -- docs/superpowers/specs/2026-10-24-nanotrack-factorial-v3.md` 查询。
- 用户格式修正：本任务后续新增或修订的 specs/plans 正文使用英文；最终实验报告只生成
  中文 `REPORT.zh-CN.md`。历史英文/中文报告均保留且不覆盖，本轮不新建或覆盖 `REPORT.md`。
- 实际修改：完整翻译
  `docs/superpowers/specs/2026-10-24-nanotrack-factorial-v3.md` 和
  `docs/superpowers/plans/2026-10-24-nanotrack-factorial-v3.md` 的自然语言正文，包括全部任务、
  文件操作、接口说明、步骤和预期结果；同步修正规格、计划及本任务记录中的旧语言要求。
- 技术核对：相对开始基线，规格的 50 个行内代码项和 147 个数值项、计划的 108 个行内
  代码项和 173 个数值项的多重集合分别完全一致；因此命令、路径、接口、哈希、种子、
  公式及数值均保留。任务仍为 1–7，步骤数依次为 5/6/6/6/6/7/7，共 43 步。
- 已实际运行 `git diff --check`；两份规格/计划未残留中文自然语言。通过只读 Node.js
  文本比较核对上述条目，并搜索排除会误导后续执行的中文 specs/plans 要求。
- 代码身份复核：`sha256sum` 实测评分策略、策略测试、旧 replay 源码仍分别为
  `eb1360c4a6bca3d9ec0e9aec96fd5c26a16224da9db01774445527459e7acb16`、
  `6b0b9d14ea1d3ba2f3676b81ab279d4b780735ce5d337f7de70f0f60d5303c14`、
  `d91c681a0af0fa6f2a19d78581f0b0a99da09127208aa52abe7de76f242ceb4b`。
- 证据边界：只修订上述两份文档和本任务进度文件，没有消费或生成模型/二进制，没有运行
  Python 测试、训练、桌面推理、隐藏集、视频回归、转换、板端或吊舱验证。既有测试失败
  记录与 g5000 身份保持不变；文档语言核对不构成新增技术验收证据。
- 下一步：从任务 2 实施 GOT 原始帧号配对与审计，后续 specs/plans 持续使用英文。
- Handoff：无需修改公共 STATUS、集成契约或其他任务文件；协调者集成本提交时采纳
  英文 specs/plans、仅中文最终实验报告的格式约定，技术方案与任务顺序不变。

## 2026-10-25 任务 1：版本化类别优先评分策略

- 时间：`2026-10-25T00:23:04+08:00`。
- 分支/工作树：`codex/nanotrack-factorial-v3` /
  `/home/u20/single-object-tracking/.worktrees/nanotrack-factorial-v3`。
- 开始基线：`40c6b110d80a2aa5a1e2eb9e4e0191012773d2d3`；结果提交由
  `git log -1 --format=%H -- scripts/nanotrack_factorial_policy.py` 查询，提交后回报协调者。
- 输入契约：`scripts.run_nanotrack_got_replay.build_metrics` 的指标映射。本任务没有消费
  或生成模型/二进制；旧 replay 源码 SHA-256 仍为
  `d91c681a0af0fa6f2a19d78581f0b0a99da09127208aa52abe7de76f242ceb4b`。
- 新增输出均位于上述工作树：`scripts/nanotrack_factorial_policy.py`，SHA-256
  `eb1360c4a6bca3d9ec0e9aec96fd5c26a16224da9db01774445527459e7acb16`；
  `tests/test_nanotrack_factorial_policy.py`，SHA-256
  `6b0b9d14ea1d3ba2f3676b81ab279d4b780735ce5d337f7de70f0f60d5303c14`。
- 保留协调者预先完成的 `.gitignore` 四行修正，使 `.venvs`、`data`、`artifacts`、`repos`
  符号链接被忽略；没有提交符号链接或改写其指向的历史工件。

### 接口及行为

- 评分版本：`SCORING_VERSION = "nanotrack-category-first-v3"`。
- `target_score_v3(metrics)` 先分别跨有指标域计算 person/vehicle 宏平均，再计算四个
  必需域 UAV123/UAVDT/VisDrone-MOT/VisDrone-SOT 的 overall 宏平均；使用
  `0.4*P + 0.4*V + 0.2*D`，不读取缓存旧评分或帧数/轨迹数/训练频次。
- `guardrail_decision(candidate, baseline)` 返回 `qualified`、`reasons`、`checks`、
  `scoring_version` 和双方 `T_v3`；七个门分别约束 GOT/UAV123 下降不超过 0.01、
  person/vehicle 宏平均与其余三域 overall 下降不超过 0.015。边界允许 `1e-12` 浮点误差。
- `rank_key(metrics, baseline)` 供 `max` 或降序排序使用，合格返回 `(True, T_v3)`，
  不合格返回 `(False, -inf)`；调用者仍必须过滤不合格项，全部不合格时不得取其 max
  作为候选。GOT/UAV123 不再次进入排序分数。
- `final_promotion(candidate_by_seed, baseline)` 要求恰好三个种子的映射，三者逐一过门，
  并且三种子 `T_v3` 算术平均相对固定基线至少提高 0.005；结果包含
  `promoted`/`qualified`、原因、平均分、提升量及 `seed_decisions`。
- 必需域缺失、person/vehicle 任一宏平均完全缺失、非法/非有限评分输入、非法种子数量
  抛出 `ValueError`。旧域优先 `target_score`、`category_macro` 与
  `composite_score=0.70*T+0.15*GOT+0.15*UAV123` 未改动。

### 实际测试记录

以下命令均在任务工作树运行，解释器统一为
`/home/u20/single-object-tracking/.venvs/nanotrack/bin/python`：

```bash
/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B -m unittest tests.test_nanotrack_factorial_policy -v
/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B -m unittest tests.test_nanotrack_got_replay_controller tests.test_nanotrack_got_replay_reporting -v
/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B -m unittest discover -s tests -v
```

- RED：先新增测试、尚未编写策略模块时，第一条命令实测 `Ran 24 tests`、
  `FAILED (failures=22)`、exit 1。22 项均明确失败于
  `No module named 'scripts.nanotrack_factorial_policy'`；2 项旧评分兼容测试通过。
- GREEN：实现最小独立策略后重跑第一条，`Ran 24 tests in 0.015s`、`OK`、exit 0。
  覆盖类别权重、四域平均、缺失类别、频次无关、GOT/UAV 方向冲突、七门边界、
  三种子聚合门、非法/非有限输入与历史兼容。
- 历史控制器/报告：第二条实测 `Ran 11 tests in 0.072s`、`OK`、exit 0。
- 完整套件仅运行一次：第三条实测 `Ran 190 tests in 159.280s`、
  `FAILED (failures=4)`、exit 1、无 error。按顶层测试方法计，188 项通过、2 项失败；
  4 个失败记录含同一方法内的 3 个 stage 子测试，完整名称如下：
  - `test_nanotrack_three_stage_configs.FormalThreeStageConfigTest.test_formal_configs_match_quota_paths_and_training_contract (stage='stage1')`
  - `test_nanotrack_three_stage_configs.FormalThreeStageConfigTest.test_formal_configs_match_quota_paths_and_training_contract (stage='stage2')`
  - `test_nanotrack_three_stage_configs.FormalThreeStageConfigTest.test_formal_configs_match_quota_paths_and_training_contract (stage='stage3')`
  - `test_nanotrack_three_stage_configs.FormalThreeStageConfigTest.test_smoke_configs_are_bounded_and_keep_policy_representative_pools`
- 四项均比较历史 `/home/u20/single-object-tracking/artifacts/...` 与当前
  `.worktrees/nanotrack-factorial-v3/artifacts/...` 的配置路径，属于已报告的基线问题。
  已有符号链接使历史 smoke CLI/report 测试本轮通过，故没有照抄起始 5 失败/1 错误。
  **完整套件不是全绿**；本任务没有修改这些历史配置或测试。
- `git diff --check` 通过；旧 replay 文件无 Git diff，SHA-256 与基线一致。

### 边界、下一步和 Handoff

- 仅证明评分/排名/晋级策略及历史兼容的单元行为；未运行真实 GPU 训练、桌面数据集
  评测、隐藏集、`sot-v1.mp4`、ONNX/RKNN、开发板或吊舱验证，没有改变 g5000 身份。
- 下一步：任务 2 按原始帧号实施 GOT 配对策略；后续控制器消费本评分版本、逐种子
  `guardrail_decision` 和最终三种子聚合结果。
- Handoff：协调者集成本提交后可锁定新实验评分接口；历史 v1/v2 保持原语义，公共模型
  身份和板端 promotion gate 无需变更。不得把本单元里程碑视为交叉实验完成。

## 2026-10-25 航拍类别数量与评分方案复核

### 实测数据分布

在工件根目录 `/home/u20/single-object-tracking` 使用
`./.venvs/nanotrack/bin/python -B -` 只读解析四域 `normal/*.json`，只计数字帧键；
`hard/*.json` 与 normal 高度重叠，未相加形成虚假总量。观察结果如下：

| 类别 | 训练轨迹数 | normal 有效帧引用数 |
| --- | ---: | ---: |
| person | 1,718 | 52,198 |
| vehicle | 3,959 | 102,046 |
| bike | 823 | 20,878 |
| other | 25 | 5,141 |
| unknown | 70 | 60,359 |

- 四域 normal 总计 240,622 个帧引用，与历史准备摘要一致。
- person 由 UAV123 的 35 条/11,806 帧和 VisDrone-MOT 的 1,683 条/40,392 帧组成；
  UAVDT 的 1,037 条/24,888 帧均为 vehicle，VisDrone-SOT 的 70 条/60,359 帧均标为
  unknown。因此可确认“已识别类别中的行人较少”，但不能把 VisDrone-SOT 的 unknown
  推断成没有行人。
- 固定验证证据合并后，person 为 35 条序列、6,045 个计分帧、约 5 个独立源视频组；
  vehicle 为 57 条序列、9,086 个计分帧、约 7 个独立源视频组。连续帧不视为独立样本。
- 验证输入/结果身份：`data/nanotrack_conservative_three_stage/val-aerial.json`
  SHA-256 `8af84a37cc29bf1efc72a37adeae30b3ebf364c9c928e51b120077414f12b77f`；
  `data/uav123_nanotrack/annotations/val.json` SHA-256
  `116ea69d78c36f4a2422ea1ccbb034c81fba280522094e5e4c461bac0576f443`；
  基线 aerial 结果 SHA-256
  `d045f2c507a2c2f29411fc314dda9cc812d5c5c9899520ce5ca8feca0560d6c5`；
  基线 UAV123 summary SHA-256
  `6a36faec2bcca6e15655e2fcea0c242efbdb459123afa7ec43c1d03238fc8e15`。

### 评分结论与规格修订

- 数据频次不等于业务重要性。车辆和行人均为正式目标时，不应因 person 样本较少而按
  训练数量降低其评分权重；否则会进一步掩盖少数类别退化。
- 旧域优先 T 在每个域内使用 person 0.4、vehicle 0.4、overall 0.2，但缺失项重归一化后，
  四域平均中的直接名义权重约为 person 0.20、vehicle 0.3667、overall 0.4333；所以它并非
  全局 person/vehicle 等权。
- 新实验改用版本化的类别优先 `T_v3 = 0.4*P + 0.4*V + 0.2*D`：`P`、`V` 分别为
  可用域的 person/vehicle 宏平均，`D` 为四域 overall 宏平均。旧 `target_score` 和历史
  `composite_score` 保持不变，不重算、不改写 v1/v2 结论。
- 为控制较少独立源视频造成的方差，新实验同时报告 `P`、`V`、`D`、`T_v3` 的按源视频
  分组 bootstrap 95% 区间，并增加 UAVDT、VisDrone-MOT、VisDrone-SOT 逐域退化门；
  不能仅凭单个点估计宣布 person 改善。
- 已同步修订规格与实施计划。新实验只生成中文报告 `REPORT.zh-CN.md`，不新建/覆盖本轮
  `REPORT.md`；历史英文/中文报告均保留且不覆盖。本任务后续新增或修订的规格和计划正文统一使用英文。

### 证据边界和下一步

- 本次只读统计和文档修订没有启动训练、桌面推理、隐藏集、ONNX/RKNN、开发板或吊舱
  验证，也没有改变 g5000 模型身份。
- 下一步仍从实施计划任务 1 开始，先用测试锁定 `T_v3`、旧评分兼容性和逐域门槛；随后
  才实施 GOT 原始帧号配对与九单元格实验。
- 公共契约当前无需切换模型或状态；协调对话只需在集成本提交时知悉新实验评分版本和
  中文报告约束，待真实实验结果形成后再决定是否更新公共候选身份。

## 2026-10-24 交叉对照规格与计划

- 初始检查：主工作树为干净的 `main@bb297504070b3dfcf2fd4b4da05ebb1503e309db`，
  与 `origin/main` 一致；此前 NanoTrack 进度提交 `81b95123dc5678b924802482c88fb1ae3e9d5980`
  已通过 `707113b` 集成。没有在 `main` 上实施代码修改。
- 新工作树创建命令：

```bash
git worktree add .worktrees/nanotrack-factorial-v3 \
  -b codex/nanotrack-factorial-v3 bb297504070b3dfcf2fd4b4da05ebb1503e309db
```

- 已形成并自检的新规格与计划：
  `docs/superpowers/specs/2026-10-24-nanotrack-factorial-v3.md`、
  `docs/superpowers/plans/2026-10-24-nanotrack-factorial-v3.md`。
- 计划阶段没有修改评分/训练代码，没有启动 GPU 训练、评测、隐藏集或视频回归。
- 工作树基线全套测试命令：

```bash
/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B \
  -m unittest discover -s tests -v
```

  实测 `Ran 166 tests in 176.221s`，结果为 `FAILED (failures=5, errors=1)`。
  其余 160 项通过。五项失败来自 `test_nanotrack_three_stage_configs` 将已提交配置中
  `/home/u20/single-object-tracking/...` 的历史绝对路径与当前 worktree 根路径比较；一项报错
  来自同一测试直接调用 worktree 下不存在的 `.venvs/nanotrack/bin/python`。另一项同时要求
  worktree 中存在 Git 忽略的历史 smoke report。该组失败发生在任何 v3 实现之前，属于已知
  工作树基线兼容问题；不能把后续局部测试通过描述成“全套测试通过”。依照工作树流程，
  需要用户决定继续实施并保留该基线边界，或先单独修复工作树兼容性。

## Handoff

建议的契约变更必须由协调对话审核后写入
`docs/progress/integration-contract.yaml`。本任务对话不得直接把候选状态改成已部署。

- **无需切换公共模型身份或放开 promotion gate**：checkpoint 与 selection 哈希已重新匹配当前契约，`onnx/rknn/board_validation` 应继续保持未验证状态。
- 建议协调对话在公共状态说明中区分“g5000 已冻结可供下游评估”和“下一轮优化仍待实施”。
- 可补充公共契约中的候选配置身份：`configs/nanotrack_v3_three_stage_stage1.yaml`，SHA-256 `cb3065982818a701a7ebb469c41b894b0a6c11e7aac6cb91dc42e5ed50547f2e`；它与已保存 UAV123 评测 provenance 一致，不是 RKNN 导出或板端配置验证。
- 本次只提交此任务文件；结果 commit 在提交后回报协调对话，也可用 `git log -1 --format=%H -- docs/progress/nanotrack-finetune.md` 查询。不要在本文件内虚构自身尚未产生的 commit ID。
- 开发板原生交互式 Debug 编译、调试、测试先于生产交叉编译；交叉编译仅消费协调者锁定的板端 handoff。真实吊舱部署和验收只接受人工反馈，本任务不声明通过。

## 2026-10-22 首次同步与重新核对

### Git 与只读检查

- 初始 cwd：`/home/u20/single-object-tracking`，分支 `main`，HEAD 为协调基线 `29b083e4e6ced2fd46eb7f9ecf821287b4cc401a`。
- `git status --short --branch` 输出仅 `## main`；无未提交修改。`git merge-base --is-ancestor 29b083e4e6ced2fd46eb7f9ecf821287b4cc401a HEAD` 返回 0，无需 fetch/merge/reset。
- 已依次完整阅读 `AGENTS.md`、`docs/progress/STATUS.md`、`docs/progress/integration-contract.yaml`、本任务文件。任务文件没有要求消费其他任务的新模型或板端结果。
- `.worktrees/` 已由 `.gitignore:15` 忽略；没有可用原生工作树工具。首次 `git worktree add` 因 Git 元数据只读失败，获授权后以同一命令成功创建独立分支/工作树，未修改主工作树文件：

```bash
git worktree add .worktrees/nanotrack-progress-sync -b codex/nanotrack-progress-sync 29b083e4e6ced2fd46eb7f9ecf821287b4cc401a
```

### 当前实测工件身份

以下路径相对于 **工件根目录 `/home/u20/single-object-tracking`**，不是新工作树。大文件没有复制进工作树或提交 Git。

| 工件 | 相对路径 | 本次 SHA-256 |
| --- | --- | --- |
| 冻结 checkpoint，3,468,939 bytes | `artifacts/nanotrack_conservative_three_stage/run/stage1/candidate_stage1_g00005000_e001_s02500.pth` | `40ebbe4296938bf5d53c8605ab05e729b306fc0bce65921a57ab6613030410d1` |
| 冻结选择 | `artifacts/nanotrack_conservative_three_stage/run/selection.json` | `a4b7a0940911cf1e064d3bdf5a312f0c35d893c71ef63374fa8b41fb6e97cc6c` |
| 原 Stage 1 配置 | `configs/nanotrack_v3_three_stage_stage1.yaml` | `cb3065982818a701a7ebb469c41b894b0a6c11e7aac6cb91dc42e5ed50547f2e` |
| 训练解析配置 | `artifacts/nanotrack_conservative_three_stage/run/stage1/resolved_config.yaml` | `e720aa6dc7685731c8ddee55c1d07f1f4c601e271219fdcc69e00f86ef49ddfb` |
| 三阶段中文报告 | `artifacts/nanotrack_conservative_three_stage/REPORT.zh-CN.md` | `4f74bfa993f6b7f7fd8c34ff7405d6345fc34fff0abbe1038ceef689475cfa7c` |
| GOT v2 选择状态 | `artifacts/nanotrack_got_replay_v2/run/selection.json` | `3c9c37ced8841b83f2ab4a6e3e29b52fbec5f4fd094945108d19c89bf3844c67` |
| GOT v2 候选表 | `artifacts/nanotrack_got_replay_v2/run/candidate_table.json` | `a61c7815c9f0a002069eea886a5c250ca5922b0f242a75b1f9874e1cd0ec7636` |
| GOT v2 历史验证记录 | `artifacts/nanotrack_got_replay_v2/verification.json` | `b9fae4a9e7f118717a2937480dcf0d691228577981069cb614b3b565afec7b63` |
| GOT v2 中文报告 | `artifacts/nanotrack_got_replay_v2/REPORT.zh-CN.md` | `51c9fc1dad19a1dfecc1168f587f80d4b10de500d0db0b99d7c67d3820bc5956` |
| GOT v2 Stage 1 配置 | `configs/nanotrack_got_replay_v2/stage1.yaml` | `3e209b1b46481df742b9d5349d0f9a1c76050e15bcc80f9608b857aa40238a4b` |

已运行 `sha256sum <上述路径>` 和 `stat -c '%s %n' <checkpoint路径>`。两种 Stage 1 配置用途不同，不以文件名相似推定内容相同。

### 实际重算与测试

在工件根目录使用 `./.venvs/nanotrack/bin/python -B -` 运行只读 JSON/哈希断言；核心可复现检查如下（没有调用会重写 verification.json 的验证器 main）：

```python
import json, hashlib
from pathlib import Path
from scripts.verify_nanotrack_got_replay import classify_selection_outcome
from scripts.run_nanotrack_got_replay import final_gate, composite_score, target_score
root = Path('/home/u20/single-object-tracking')
h = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
s = json.loads((root / 'artifacts/nanotrack_conservative_three_stage/run/selection.json').read_text())
assert s['selection_final'] is True
assert h(Path(s['final_checkpoint_path'])) == s['final_checkpoint_sha256']
r = root / 'artifacts/nanotrack_got_replay_v2'
sel = json.loads((r / 'run/selection.json').read_text())
t = json.loads((r / 'run/candidate_table.json').read_text())
print(classify_selection_outcome(sel, t, (r / 'final/summary.json').exists()))
for item in t['candidates']:
    assert h(Path(item['checkpoint'])) == item['checkpoint_sha256']
    assert not final_gate(item['metrics'], sel['baseline'])['qualified']
    assert abs(composite_score(item['metrics']) - item['metrics']['composite_score']) < 1e-12
best = max(t['candidates'], key=lambda x: composite_score(x['metrics']))
print(len(t['candidates']), t['seeds'])
print(best['key'], target_score(best['metrics']), composite_score(best['metrics']))
```

实测结果：三阶段 checkpoint 身份匹配；GOT v2 共 8 个候选哈希匹配且均未过 final gate；`outcome=no_candidate_passed`，`selected_key=None`，`hidden_evaluation_completed=False`。两个种子均为 `stage1_best=None`、`stage2_skipped=no qualified stage1 candidate`；`final/` 未发现结果条目。最佳但未晋级候选为 `seed20261011_stage1_c1`，T=`0.7432561249618961`、C=`0.7341685649057923`。这次重算利用已有指标，不是重新推理。

在任务工作树实际执行：

```bash
/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B -m unittest discover -s tests -p 'test_nanotrack_got*.py' -v
/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B -m unittest discover -s tests -p 'test_nanotrack_batch_calibration.py' -v
```

分别为 **33 tests / OK / exit 0** 和 **4 tests / OK / exit 0**。这里只重跑了 37 项相关单元测试，没有宣称整个项目测试通过，没有运行 GPU 训练或重新评测隐藏集。

### 新方案实施状态与证据边界

- 本次读取 `scripts/run_nanotrack_got_replay.py:80`，旧 `0.70/0.15/0.15` 评分仍存在，文件 SHA-256 为 `d91c681a0af0fa6f2a19d78581f0b0a99da09127208aa52abe7de76f242ceb4b`。相关单元测试仍明确检查旧权重；测试通过不代表新设计已实现。
- 本次读取 `/home/u20/single-object-tracking/repos/siamtrackers/NanoTrack/nanotrack/datasets/dataset.py:110`，仍按 `template_frame` 列表下标加减 `frame_range`。该外部源码不由本仓库 `git ls-files` 跟踪，当前文件 SHA-256 为 `cd6b925b12f2879d5979c2c1ee8229317ea4bcdc7ce6e8fface4f9d0f2d39373`；后续修改必须保存可复现补丁和源身份，不能只改外部目录。
- 在当前 `artifacts/nanotrack*`、`configs/nanotrack*` 清单中，GOT 回放仅查见 v1/v2；未发现新版交叉对照的完成工件。这是本地检查范围内的结论，不声称检查了其他对话所有工作树或外部机器。
- 历史报告、verification 中的性能/环境数据只作为历史记录引用；本次没有重跑完整数据审计、CUDA 推理、ONNX/RKNN 转换、开发板测试或吊舱验收，也没有证明未触碰隐藏数据的全系统访问历史。
- 研发待办：以 T 排序及固定基线退化约束；原始帧号采样审计/修正及新旧对照；batch/GOT 交叉对照；从验证失败提取问题类别但只从训练集补困难样本；独立目标域最终验证。上述均为待办，不作为完成证据。

## 后续更新模板

```text
时间：<ISO 8601 + timezone>
分支/工作树：<branch> / <absolute path>
基线/结果提交：<base commit> / <result commit>
输入：<path, SHA-256>
输出：<path, SHA-256>
验证：<command -> observed result>
边界/阻塞：<facts>
下一步：<one concrete action>
Handoff：<requested contract change or none>
```
