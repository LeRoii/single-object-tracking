# NanoTrack 微调任务进度

- 任务状态：`ready_for_downstream_evaluation`
- 最近核对：`2026-10-26T09:21:27+08:00`。
- 核对基线：`bb297504070b3dfcf2fd4b4da05ebb1503e309db`（四任务进度集成提交）。
- 当前任务分支：`codex/nanotrack-factorial-v3`。
- 当前任务工作树：`/home/u20/single-object-tracking/.worktrees/nanotrack-factorial-v3`。
- 状态说明：既有 g5000 可交付下游评估；新实验控制器在两轮修复后通过独立评审。25 项控制器测试、87 项相关历史回归、真实权重 CPU 初始化和实际训练器预检均通过；batch/GOT 交叉对照及困难样本强化尚无真实 GPU 训练证据。
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

- 尚未在集成契约中批准 g5000 取代当前板端 NanoTrack 模型。
- 尚无绑定 g5000 的 RKNN 模型与真实 RK3566 验收记录。
- 历史控制器评分仍为 `0.70*T + 0.15*GOT + 0.15*UAV123`；新控制器已接入 `T_v3`，目前只有伪端到端结果，不能宣称新实验已实际训练或产生性能提升。
- 外部 NanoTrack 数据加载器保留历史列表索引语义；受控训练入口已可用显式 `--pair-policy` 在构造数据集前仅为匹配的 GOT 子池安装原始帧号策略，尚未用于真实新训练。
- batch `{8,32,64}` × GOT `{0%,10%,30%}` 的新实验、困难样本归因与强化对照没有可交付的运行记录；不能声明 batch 64 或任何配比最优。
- 后续独立目标域留出集的可用性与隔离性尚需核查；已查看的历史隐藏集不能重新称为未触碰测试集。

## Next action

1. 已在 `codex/nanotrack-factorial-v3` 基于 `main@bb297504070b3dfcf2fd4b4da05ebb1503e309db`
   编写下一轮规格与实施计划：
   `docs/superpowers/specs/2026-10-24-nanotrack-factorial-v3.md`、
   `docs/superpowers/plans/2026-10-24-nanotrack-factorial-v3.md`；已获准开始实施，任务 1 评分策略和任务 2 配对策略及审计已完成局部验证。
2. 九单元格精确样本配置和可恢复控制器已实现；下一步为
   验证失败诊断、等样本交叉实验、训练集困难样本对照和最终报告。当前没有启动真实训练、
   隐藏集评测或 `sot-v1.mp4` 回归。
3. g5000 保持只读候选；下游 ONNX/RKNN/开发板链路不因新实验计划自动切换。

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
