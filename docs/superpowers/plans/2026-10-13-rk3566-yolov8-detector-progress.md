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
- Task 2 / adapter API：`test_yolo_detector_adapter` 编译失败，明确缺失
  `yolo_detector_adapter.hpp`。
- Task 2 / 有界副本：active 状态下用非法图像二次 submit 时错误地先返回 BGR 校验错误，证明旧顺序在拒绝 busy 前仍会创建/校验第二份工作项。
- Task 3 / API：`test_yolov8_postprocess` 编译失败，明确缺失纯 C++ 后处理接口。
- Task 3 / 行为：加入可编译接口和占位实现后，定向测试以
  `YOLOv8 postprocess is not implemented` 失败。

## GREEN 证据

- 基线：主机 CTest 7/7 通过。
- Task 1 定向测试：`controller_support`、`long_term_runtime` 2/2 通过。
- Task 1 完整主机回归：CTest 7/7 通过。
- Task 2 adapter 定向测试 1/1 通过，覆盖 stride 深拷贝、有界槽、poll、异常禁用和析构 join。
- Task 2 ASAN+UBSAN 1/1 通过；受 ptrace 环境限制，以 `ASAN_OPTIONS=detect_leaks=0` 关闭 LSan。
- Task 2 完整主机回归：CTest 8/8 通过。
- Task 3 后处理定向测试 1/1 通过，覆盖 6/9 输出、DFL、FP32/INT8、NMS、类别过滤、letterbox 和非法输入。
- Task 3 ASAN+UBSAN 1/1 通过；完整主机回归 CTest 9/9 通过。

## 开发板证据

待执行。

## 工件

待执行。

## 偏差与裁决

- 使用实际 Git 提交和 SDD ledger 取代计划中“非 Git 工作区”的假设；测试、模型和板端证据要求不变。
- `DetectorBatch` 保留原有 `frame/timestamp/candidates` 三字段顺序，新增计数和耗时字段追加在后，避免破坏旧聚合初始化调用方。
- `tracking_log.cpp` 在 Task 1 先做最小兼容映射；完整 schema v3 仍在 Task 4 实施。
- 当前托管环境不支持 ptrace 下的 LeakSanitizer；本阶段保留 ASAN/UBSAN，并由析构 join 测试覆盖线程生命周期，LSan 证据缺失明确保留。

## 未解决风险

- RKNN-Toolkit2 2.3.2 的导入和模型转换尚未在本次执行中验证。
- detector 模型尚未在 RK3566 上执行真实推理。
