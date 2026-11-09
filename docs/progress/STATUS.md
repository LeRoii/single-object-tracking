# 多对话项目状态总表

本文件由协调对话维护，是四个任务的简明索引。详细事实、命令和证据分别记录在任务文件中；
这里不复制完整日志，也不把计划当作已完成证据。

- 初始化时间：`2026-10-22T21:05:39+08:00`
- 初始化基线：`main@352018bd588a999d03f2685dc7b71266f396b39d`
- 最近汇总：`2026-11-09T21:23:05+08:00`，四个任务的最新已提交回传已集成至
  `main@e0b59f4936ce074c1d263624745d975df52bdef3`
- 最近协调契约更新：`2026-11-07T22:55:06+08:00`，基于
  `main@40002ba22bda62cffa8519f323abab5f970e0836` 冻结统一非训练算法测试数据
  `data/occ_video_1`
- 当前集成契约：[`integration-contract.yaml`](integration-contract.yaml)

## 当前状态

| 任务 | 当前状态 | 当前可交付物 | 主要边界或阻塞 | 详情 |
| --- | --- | --- | --- | --- |
| NanoTrack 微调 | `ready_for_downstream_evaluation` | g5000 身份保持冻结；九格矩阵 72/72 完成且 verifier 结论为 `no_safe_cell` | 没有新 cell 同时通过双 seed 安全门；g5000 仍无绑定的 ONNX/RKNN/板端验收 | [`nanotrack-finetune.md`](nanotrack-finetune.md) |
| Detector 训练 | `candidate_ready_promotion_held` | VisDrone9 PT、ONNX、RKNN 和 manifest/labels 身份已复核，76 项回归通过 | 正式切换被集成内存门和重捕效果门阻塞；canonical manifest 仍是 COCO | [`detector-training.md`](detector-training.md) |
| 开发板验证 | `functional_pass_promotion_held` | 统一数据集已在真实 RK3566 路径完成文件与 271 项标签哈希核验；既有 Debug 功能证据保留 | 尚未用统一数据集形成新的算法运行证据；正式源码/模型 handoff 仍未批准 | [`board-validation.md`](board-validation.md) |
| 生产交叉编译 | `candidate_cross_compile_succeeded_handoff_unapproved` | 已有候选 A6 编译、隔离板端启动和诊断记录 | 候选记录不是协调者批准的生产 handoff；正式编译裁决和真实吊舱验收仍未完成 | [`cross-compile.md`](cross-compile.md) |

## 依赖顺序

1. NanoTrack 微调和 detector 训练分别冻结模型身份、配置及评测边界。
2. 协调对话锁定一组源码和模型结果，交给交互式 Debug 版本进行开发板原生编译、调试和测试。
3. 开发板验证通过相应门槛后，冻结精确源码 commit 和模型/manifest/labels 哈希并形成最新 handoff。
4. 交叉编译任务确认能够取得这份最新 handoff，并成功编译生产产物。
5. 生产产物部署到真实吊舱后，只能由人完成测试；协调对话只记录人工反馈，不自行声明吊舱验收通过。

## 同步规则

- 每个任务对话只更新自己的任务文件，并在 `Handoff` 中声明需要协调对话采纳的契约变更。
- 状态变化必须附证据引用；`planned`、`reported`、`observed` 和 `verified` 不得混用。
- 工作树之间不共享未提交文件。同步点是已提交并被集成的 Git commit。
- 模型、视频、日志和二进制不因绝对路径相同就视为同一工件，必须核对 SHA-256。
- 协调对话更新本表时，应先重新读取四个任务文件和当前 Git 状态。

## 统一非训练算法测试数据

- 后续所有非训练算法测试统一使用
  `/home/u20/single-object-tracking/data/occ_video_1`；训练数据和训练流程不受此约束。
- 主视频为 `occ_video_1/1.mp4`，SHA-256 为
  `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`，
  1920×1080、30 FPS、271 帧。
- 数据包含完整白色轿车 `class_id=0` 的 271 帧 YOLO 框，以及人行天桥遮挡事件：
  169～171 帧部分遮挡、172～180 帧完全遮挡、181 帧部分重现、182 帧首次可靠重现、
  184 帧稳定重现。
- 169～181 帧为人工确认的双向插值估计框，不属于可观测真值，不得混入普通 IoU；
  其余框的来源和复核边界见数据目录内 `annotation_status.csv` 与
  `annotation_metadata.json`。
- `sot-v1.mp4` 与 `girl_dance.mp4` 从此同步点起不得再用于新增的非训练算法测试证据；
  历史结果保留原始输入身份，不删除、不改名，也不得冒充新基准结果。
- 开发板原生测试和交叉编译产物的运行回归在使用前必须复制这组数据并核对契约中的
  视频、元数据、事件记录和标签清单哈希。纯编译/链接检查本身不需要视频输入。

## 当前协调事项

- 交叉编译任务只需确认能够取得协调对话锁定的最新板端验证结果，并用现有流程编译成功；不要求承担完整工具链/sysroot provenance、开发板复测或吊舱验收。
- 由协调对话决定是否将 NanoTrack g5000 候选送入 ONNX/RKNN/板端链路；旧官方 NanoTrack V3 板端结果不能替代微调候选的板端证据。
- VisDrone9 detector 保持候选状态，直到同协议内存复测和正式 promotion gate 通过。
- 开发板验证继续保持“功能可运行”与“长时重捕/开发板交付门”两条结论分离，并在交叉编译之前完成。
- 下一轮开发板候选尚未锁定：交互式 Debug 源码候选 `b98b1335c3d263f542fd52656c7d7c4d58b2bb77`
  仍需集成审查，并需同时决定 NanoTrack 模型身份和 COCO/VisDrone9 detector 选择。
- `development_board_validation.latest_handoff.available` 继续保持 `false`；在门槛通过前，交叉编译任务只等待，不消费历史产物。
- 真实吊舱测试为人工门禁，不属于任何 Codex 自动验证结论。
- 开发板验证、生产交叉编译及其他非训练算法任务下一次运行前，必须切换到
  `data/occ_video_1` 并记录板端/目标环境实际路径和 SHA-256；不得继续新增
  `sot-v1.mp4` 或 `girl_dance.mp4` 证据。

## 更新记录

### 2026-10-22T21:05:39+08:00

- 基于 `main@352018b` 初始化四任务索引。
- 保留现有历史报告作为引用，不移动、不重写原证据。
- 尚未把任何未提交文件或另一个工作树的状态计入集成基线。

### 2026-10-22T21:18:30+08:00

- 修正流程顺序为“开发板原生编译/交互调试/测试 → 交叉编译生产产物 → 人工吊舱测试”。
- 将交叉编译任务缩减为确认取得最新板端验证结果并编译成功。
- 明确真实吊舱测试只能由人执行和确认。

### 2026-10-22T22:11:26+08:00

- 集成 detector `700da83459b742e0d83ea28493feef6cd3103cca`、NanoTrack 微调
  `81b95123dc5678b924802482c88fb1ae3e9d5980`、开发板验证
  `acc595d19b3e270f8adde7cebca5deeb7e8f9328` 和交叉编译
  `2795a3303921e750a73ef7d1d38482c2b3012a67` 四份任务回传。
- 采纳交叉编译 owner refresh，将其状态更新为等待批准的开发板 handoff；尚未执行编译。
- 保持开发板 promotion hold 和 `latest_handoff.available=false`，未把短程烟测升级为开发板交付通过。
- 合并结果复验：detector/Python `76 passed`，NanoTrack 相关单测 `33 + 4` 项通过，
  RKNN-independent 主机构建完成且 CTest `12/12` 通过；这些主机结果不替代板端门禁。

### 2026-11-07T22:55:06+08:00

- 根据用户决定，将 `data/occ_video_1` 冻结为所有非训练算法测试的统一输入；训练任务
  继续使用各自训练数据。
- 记录主视频、标注元数据、遮挡事件、逐帧状态和 271 个正式标签的内容哈希；详细身份
  写入 `integration-contract.yaml` schema v2。
- `sot-v1.mp4` 和 `girl_dance.mp4` 仅保留为历史证据输入，从本同步点起不再用于新增测试。
- 本次更新只建立数据消费契约，不声称该视频已传到 RK3566、已由板端运行，也不替代
  交叉编译、板端性能或真实吊舱验收。

### 2026-11-09T21:23:05+08:00

- 集成 NanoTrack 微调 `b43aa257e99b7ffaf8e0845c4fdfaabef1d45821`、detector
  `1a851916b3f64fbf65cdf6e9fa4394800a2c795e`、开发板验证
  `29cd9948196330ed45fac18d7963b9cd385beb58` 和生产交叉编译
  `74f2bc8485b2054341979e53562a71d4cbc71d4e` 的最新已提交回传。
- 同时集成 `999f346c6df19be2e96abe9a32f413f0b93cee7c` 的长时跟踪真值数据要求文档。
- NanoTrack 完整单测在其链接工作树运行 `277` 项，只有 4 个历史绝对路径比较失败；
  相同路径测试在主工作树为 `6/6` 通过。合并结果仍需执行完整主工作树回归后才能作为最终验证结论。
- 保持 `development_board_validation.latest_handoff.available=false`；候选交叉编译和板端诊断
  不升级为批准 handoff、生产部署或真实吊舱验收。
