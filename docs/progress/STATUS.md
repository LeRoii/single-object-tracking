# 多对话项目状态总表

本文件由协调对话维护，是四个任务的简明索引。详细事实、命令和证据分别记录在任务文件中；
这里不复制完整日志，也不把计划当作已完成证据。

- 初始化时间：`2026-10-22T21:05:39+08:00`
- 初始化基线：`main@352018bd588a999d03f2685dc7b71266f396b39d`
- 当前集成契约：[`integration-contract.yaml`](integration-contract.yaml)

## 当前状态

| 任务 | 当前状态 | 当前可交付物 | 主要边界或阻塞 | 详情 |
| --- | --- | --- | --- | --- |
| NanoTrack 微调 | `ready_for_downstream_evaluation` | 已冻结 Stage 1 / g5000 候选及桌面评测证据 | 微调候选尚未形成绑定该权重的 RKNN/板端验收证据；V1 窗口协议偏差保留 | [`nanotrack-finetune.md`](nanotrack-finetune.md) |
| Detector 训练 | `candidate_ready_promotion_held` | VisDrone9 PT、RKOPT ONNX、RK3566 FP16 RKNN 候选及 provenance | 正式切换被集成内存门和重捕效果门阻塞；canonical manifest 仍是 COCO | [`detector-training.md`](detector-training.md) |
| 开发板验证 | `functional_pass_promotion_held` | 交互式 Debug 版本的板端原生编译、功能与安全证据 | VmHWM 未平台化；没有 detector-confirmed 重捕；阈值/可见光/红外验收未闭环 | [`board-validation.md`](board-validation.md) |
| 生产交叉编译 | `needs_owner_refresh` | 应消费最新的已通过开发板验证的结果并生成生产产物 | 尚未记录能否取得协调对话锁定的最新结果并编译成功 | [`cross-compile.md`](cross-compile.md) |

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
