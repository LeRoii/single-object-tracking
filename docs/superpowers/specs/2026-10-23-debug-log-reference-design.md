# RK3566 Debug 日志说明设计

## 目的

在 `RK3566_NANOTRACK_LONGTERM_CODE_FLOW.zh-CN.md` 中增加可直接用于板端排障的
Debug 日志参考，说明每种 `DEBUG_EVENT` 的触发条件、字段意义、实际源码来源和
判读边界。

## 范围与约束

- 只说明 `rk3566/nanotrack_v3` 独立交互式 Debug 程序的标准输出，不把这些日志
  描述成 ViewLink `ai_alg` 生产接口已经具备的能力。
- 以当前 `main.cpp`、`debug_telemetry.cpp`、`long_term_runtime.hpp`、
  `long_term_controller.hpp` 和 `long_term_types.hpp` 为唯一字段依据。
- 按事件分组列出字段，补充公共格式、枚举值、打印频率及典型诊断顺序。
- 明确 detector 只提供候选；`accepted=1`、检测框存在或收到 batch 都不能单独使
  `control_valid=1`。
- 不修改程序行为、阈值、状态机或公共协调契约。

## 采用方案

采用“按事件分组的字段字典”。相比单一总表，它能保留同名字段在生命周期事件和
帧结果事件中的不同来源；相比把说明分散到现有流程章节，它更适合根据一条实际日志
快速反查。

## 验证

- 从两处日志生成源码提取全部 `event=` 名称，与文档事件列表双向核对。
- 检查事件表中每个字段都能追溯到明确的命令行选项、OpenCV 视频属性、
  `SearchPlan`、`ControllerOutput`、`DetectorDiagnostics` 或本地循环计数器。
- 运行 Markdown 基础检查和 `git diff --check`。
