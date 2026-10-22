# RK3566 开发板验证任务进度

- 任务状态：`functional_pass_promotion_held`
- 最近核对：`2026-10-22T21:18:30+08:00`
- 核对基线：`main@352018bd588a999d03f2685dc7b71266f396b39d`
- 责任边界：在交叉编译之前，使用具备完整编译环境的真实 RK3566 开发板，对交互式 Debug 版本执行原生编译、调试、加载/推理、集成视频、控制安全、时延、资源和稳定性测试。

本任务是开发阶段的板端验证，不消费交叉编译生产产物。交叉编译只在本任务冻结并移交最新可用结果后进行；真实吊舱测试由人完成。

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

- 隔离或修复视频 writer/buffer/allocator 内存路径，并证明 RSS 与 VmHWM 平台化。
- 用相同输入、时长、采样协议完成 COCO/VisDrone9 资源对比。
- 使用标注的遮挡/可靠重现数据证明 detector-confirmed recovery，而不是只统计候选或 motion reacquisition。
- 完成可见光/红外阈值标定和同类目标身份风险评估。

## Next action

只在 `integration-contract.yaml` 的候选组合被协调对话锁定后开始下一轮开发板验收。下一轮至少保留源 commit、板端原生编译二进制、模型/manifest/labels/输入哈希、完整命令、资源采样和 verifier 结果。

## Handoff

任何 `PASS` 都必须注明层级：`model_load`、`detector_function`、`integrated_function`、`stability`、`recovery_quality` 或 `development_board_acceptance`。低层级通过不得升级表述为高层级通过。

当某一组合达到预定开发板门槛后，本任务应向协调对话移交精确 source commit、模型/manifest/labels 哈希、配置、板端验证证据和裁决。协调对话将其标记为最新 handoff，供交叉编译任务获取。此 handoff 不代表真实吊舱已通过测试。

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
