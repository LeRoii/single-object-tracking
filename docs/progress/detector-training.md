# Detector 训练任务进度

- 任务状态：`candidate_ready_promotion_held`
- 最近核对：`2026-10-22T21:34:11+08:00`
- 核对分支/工作树：`codex/detector-progress-sync` / `/home/u20/single-object-tracking/.worktrees/detector-progress-sync`
- 核对基线：`main@29b083e4e6ced2fd46eb7f9ecf821287b4cc401a`
- 责任边界：VisDrone9 数据转换/审计、YOLOv8n 训练、PT/ONNX 一致性、RKNN 转换和候选模型 provenance。

## 2026-10-22 协调基线同步核对

- 主工作树 `/home/u20/single-object-tracking` 在同步检查时已经位于
  `main@29b083e4e6ced2fd46eb7f9ecf821287b4cc401a`，工作区干净；目标 commit
  与 HEAD 相同，因此未执行 merge、reset、restore 或文件覆盖。
- 本次进度更新从该 commit 创建独立分支和工作树，不消费其他工作树的
  未提交文件。
- 在本地现存候选工件上实际执行 `sha256sum` 和 `stat`，结果如下；这些
  大文件继续保持在 Git 之外：

| 层级 | 实际路径 | 字节数 | SHA-256 |
| --- | --- | ---: | --- |
| PT checkpoint | `/home/u20/single-object-tracking/artifacts/yolov8-visdrone9/export/yolov8n-visdrone9-best.pt` | 6235363 | `6744672ac5e35debba2a647ad570e9fe84a1cabd1c45c9840080d0a4127fe36d` |
| RKOPT ONNX | `/home/u20/single-object-tracking/artifacts/yolov8-visdrone9/export/yolov8n-visdrone9-rkopt.onnx` | 12072229 | `b2948072f01da7fe3b686392a47fe742380c98775c4149111527b7ddf26a5cc8` |
| RK3566 FP16 RKNN | `/home/u20/single-object-tracking/artifacts/yolov8-visdrone9/export/yolov8n-visdrone9-rk3566-fp16.rknn` | 7797510 | `3698bc9bea1f43a4bac2826a09e03be0439fddfbc5c5604d8ed0f92b36881df0` |
| Candidate manifest | `/home/u20/single-object-tracking/artifacts/yolov8-visdrone9/export/detector-manifest.json` | 3890 | `05e6d8c7c210b7f9d85b8fb712aa00b014f310e46e85f171ce1158afc90ba63e` |
| VisDrone9 labels | `/home/u20/single-object-tracking/artifacts/yolov8-visdrone9/export/visdrone9.txt` | 64 | `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992` |

- 当前 tracked canonical 仍是 COCO：
  `rk3566/nanotrack_v3/models/detector-manifest.json` SHA-256
  `8adcb1026803d688a7b85c55977d533eac6c15070ac4260bedb09d1da49d6061`，
  `rk3566/nanotrack_v3/models/coco80.txt` SHA-256
  `bd17f1ee35d5f3c862a4894605855abbb9dda4b0621fdb0ac4c2c8c7bb7e730a`。
- 基线回归命令
  `/home/u20/single-object-tracking/.venvs/yolov8-visdrone/bin/python -m pytest -q rk3566/nanotrack_v3/tests`
  实际结果为 `76 passed in 2.77s`。
- 本次没有重新训练、重新导出 ONNX、重新转换 RKNN，也没有通过 SSH
  重跑开发板或真实吊舱测试；板端结论仍只引用已提交证据，并由
  `board-validation.md` 的所有者维护。

## 已观察证据

- 完整训练：100/100 epochs；冻结 `best.pt` SHA-256：
  `6744672ac5e35debba2a647ad570e9fe84a1cabd1c45c9840080d0a4127fe36d`。
- 独立验证：precision `0.45418`、recall `0.34786`、mAP50 `0.34907`、mAP50-95 `0.20716`。
- RKOPT ONNX SHA-256：
  `b2948072f01da7fe3b686392a47fe742380c98775c4149111527b7ddf26a5cc8`。
- RK3566 FP16 RKNN SHA-256：
  `3698bc9bea1f43a4bac2826a09e03be0439fddfbc5c5604d8ed0f92b36881df0`。
- 候选 manifest SHA-256：
  `05e6d8c7c210b7f9d85b8fb712aa00b014f310e46e85f171ce1158afc90ba63e`。
- labels SHA-256：
  `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`。
- 训练/导出/转换证据：
  `rk3566/nanotrack_v3/docs/visdrone9-detector-training.md`。

## 当前裁决

- VisDrone9 模型是成功完成训练、转换和真实板端功能验证的候选模型。
- 仓库 canonical `rk3566/nanotrack_v3/models/detector-manifest.json` 仍保留原 COCO detector；不得把候选表述为已正式切换。
- 板端 promotion hold 属于集成验收结论，详细状态由
  `board-validation.md` 管理。

## Blockers

- 相同输入和相同采样协议下的旧/新模型资源对比尚未闭环。
- 集成长稳测试中 VmHWM 持续增长，未通过 promotion gate。
- 现有人员样例没有 detector-confirmed reacquisition；固定阈值下 usable hypotheses 少于 COCO 基线。

## Next action

保持候选工件及 provenance 不变，等待板端任务完成内存路径隔离/修复后的同协议复测；如果重新训练或重新转换，必须生成新的工件身份，不能覆盖本次证据。

## Handoff

候选板端发布目录：
`/home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1`。
协调对话在 promotion gate 全部通过前不得把它写成 canonical detector。

本次核对没有产生新的模型身份、状态变化或公共契约字段，因而不请求
修改 `integration-contract.yaml`；协调对话只需集成本任务进度提交。

## 后续更新模板

```text
时间：<ISO 8601 + timezone>
分支/工作树：<branch> / <absolute path>
基线/结果提交：<base commit> / <result commit>
数据/训练身份：<audit/config/checkpoint SHA-256>
导出/转换身份：<ONNX/RKNN/manifest/labels SHA-256>
验证：<command -> observed result>
边界/阻塞：<facts>
下一步：<one concrete action>
Handoff：<requested contract change or none>
```
