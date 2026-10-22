# 多对话项目状态总表

本文件由协调对话维护，是四个任务的简明索引。详细事实、命令和证据分别记录在任务文件中；
这里不复制完整日志，也不把计划当作已完成证据。

- 初始化时间：`2026-10-22T21:05:39+08:00`
- 初始化基线：`main@352018bd588a999d03f2685dc7b71266f396b39d`
- 最近汇总：`2026-10-22T22:11:26+08:00`，四个任务回传已集成至
  `main@062dd5b1c8adfe7e79ca4fe4100f299bf5e8cc9f`
- 当前集成契约：[`integration-contract.yaml`](integration-contract.yaml)

## 当前状态

| 任务 | 当前状态 | 当前可交付物 | 主要边界或阻塞 | 详情 |
| --- | --- | --- | --- | --- |
| NanoTrack 微调 | `ready_for_downstream_evaluation` | g5000 候选身份已复核，37 项相关单测通过 | 尚无绑定该权重的 ONNX/RKNN/板端验收；下一轮评分、采样和交叉对照仍待实施 | [`nanotrack-finetune.md`](nanotrack-finetune.md) |
| Detector 训练 | `candidate_ready_promotion_held` | VisDrone9 PT、ONNX、RKNN 和 manifest/labels 身份已复核，76 项回归通过 | 正式切换被集成内存门和重捕效果门阻塞；canonical manifest 仍是 COCO | [`detector-training.md`](detector-training.md) |
| 开发板验证 | `functional_pass_promotion_held` | 当前 Debug 工件、13 项板端测试、detector 单图和短程集成功能已复核 | 源码候选尚未集成锁定；VmHWM、detector-confirmed 重捕和阈值门未闭环 | [`board-validation.md`](board-validation.md) |
| 生产交叉编译 | `blocked_waiting_for_approved_board_handoff` | 已同步协调基线并确认当前没有可消费 handoff | 输入身份尚未批准，因此未执行交叉编译；这不是编译失败 | [`cross-compile.md`](cross-compile.md) |

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

## 当前协调事项

- 交叉编译任务只需确认能够取得协调对话锁定的最新板端验证结果，并用现有流程编译成功；不要求承担完整工具链/sysroot provenance、开发板复测或吊舱验收。
- 由协调对话决定是否将 NanoTrack g5000 候选送入 ONNX/RKNN/板端链路；旧官方 NanoTrack V3 板端结果不能替代微调候选的板端证据。
- VisDrone9 detector 保持候选状态，直到同协议内存复测和正式 promotion gate 通过。
- 开发板验证继续保持“功能可运行”与“长时重捕/开发板交付门”两条结论分离，并在交叉编译之前完成。
- 下一轮开发板候选尚未锁定：交互式 Debug 源码候选 `b98b1335c3d263f542fd52656c7d7c4d58b2bb77`
  仍需集成审查，并需同时决定 NanoTrack 模型身份和 COCO/VisDrone9 detector 选择。
- `development_board_validation.latest_handoff.available` 继续保持 `false`；在门槛通过前，交叉编译任务只等待，不消费历史产物。
- 真实吊舱测试为人工门禁，不属于任何 Codex 自动验证结论。

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
