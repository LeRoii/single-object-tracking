# NanoTrack 微调任务进度

- 任务状态：`ready_for_downstream_evaluation`
- 最近核对：`2026-10-22T21:05:39+08:00`
- 核对基线：`main@352018bd588a999d03f2685dc7b71266f396b39d`
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

## Next action

由协调对话确认是否启动 g5000 的独立 ONNX/RKNN/板端候选链路；未确认前保持冻结工件只读。

## Handoff

建议的契约变更必须由协调对话审核后写入
`docs/progress/integration-contract.yaml`。本任务对话不得直接把候选状态改成已部署。

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
