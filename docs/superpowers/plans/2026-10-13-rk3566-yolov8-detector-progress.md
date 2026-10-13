# RK3566 YOLOv8 Detector 实施证据

对应计划：`docs/superpowers/plans/2026-10-13-rk3566-yolov8-detector.md`

## 环境

- 基线提交：`d8e7ae3`
- 功能分支：`feature/rk3566-yolov8-detector`
- 工作区：`.worktrees/rk3566-yolov8-detector`
- 说明：计划编写时目录尚未初始化 Git；用户随后要求初始化，并批准使用基线提交和隔离 worktree。

## RED 证据

- Task 1 / `can_submit`：`test_controller_support` 编译失败，明确报错
  `NullDetectorAdapter has no member named can_submit`。
- Task 1 / runtime 诊断：`test_long_term_runtime` 编译失败，明确缺失
  `DetectorDiagnostics`、`DetectorTiming` 和 `DetectorBatch` 诊断字段。

## GREEN 证据

- 基线：主机 CTest 7/7 通过。
- Task 1 定向测试：`controller_support`、`long_term_runtime` 2/2 通过。
- Task 1 完整主机回归：CTest 7/7 通过。

## 开发板证据

待执行。

## 工件

待执行。

## 偏差与裁决

- 使用实际 Git 提交和 SDD ledger 取代计划中“非 Git 工作区”的假设；测试、模型和板端证据要求不变。
- `DetectorBatch` 保留原有 `frame/timestamp/candidates` 三字段顺序，新增计数和耗时字段追加在后，避免破坏旧聚合初始化调用方。
- `tracking_log.cpp` 在 Task 1 先做最小兼容映射；完整 schema v3 仍在 Task 4 实施。

## 未解决风险

- RKNN-Toolkit2 2.3.2 的导入和模型转换尚未在本次执行中验证。
- detector 模型尚未在 RK3566 上执行真实推理。
