# RK3566 YOLOv8 Detector 与 NanoTrack 长时跟踪集成设计规格

日期：2026-10-13

## 1. 背景与目标

现有 `rk3566/nanotrack_v3` 已在 RK3566 上完成 NanoTrack V3 双模板长时控制器、
异步 `DetectorAdapter` 契约、真实视频推理和稳定性验证。当前默认
`NullDetectorAdapter` 只能在运动预测中心附近做区域搜索；目标离开该搜索范围后无法提供
全图候选，因此不能完成全局重捕获。

本阶段部署一套基于 YOLOv8 的 detector，为 `ASSISTED_RECOVERY` 和 `LOST` 状态提供
最多三个全图候选。目标是先实现正确、可审计、可替换的板端功能链路；本阶段不以 detector
精度、量化效果、吞吐率或功耗作为通过条件。

本设计属于新增推理子系统和现有恢复数据流的集成，不照搬
`ancient-tracker/rknn_yolov8_detector-640384`。该旧工程只用于确认 YOLOv8 DFL 多分支输出
形态。其硬编码 RK3588 路径、裸指针、detached thread、无界结果队列、缺失请求身份和旧
二进制均不进入新实现。

## 2. 范围与非目标

### 2.1 本阶段包含

- 获取 Rockchip 官方优化版 YOLOv8n ONNX，并记录来源、版本和 SHA-256。
- 使用 RKNN-Toolkit2 2.3.2 转换出 `target_platform=rk3566` 的 FP16 RKNN 模型。
- 新增可独立测试的 YOLOv8 后处理和 RKNN 推理封装。
- 新增单工作线程、有界请求/结果槽的 `YoloDetectorAdapter`。
- 将 detector 作为可选组件接入现有视频 CLI 和长时控制器。
- 在 RK3566 上完成独立 detector smoke、视频检测以及 detector 辅助重捕获验证。
- 输出模型指纹、张量信息、请求身份、候选类别/置信度、延迟和丢弃原因。

### 2.2 本阶段不包含

- detector 模型微调、训练集建设、INT8 标定和精度优化；
- detector 或整条跟踪链路的性能达标承诺；
- 红外专用 YOLO 模型；
- 多目标跟踪、ReID、类别跟踪 ID 或云台协议；
- 让 detector 检测框直接驱动控制输出；
- 复用旧工程的动态库 ABI、线程池或预编译二进制。

## 3. 模型基线和兼容策略

### 3.1 首版模型

- 来源：Rockchip `rknn_model_zoo/examples/yolov8` 的优化版 `yolov8n.onnx`。
- 输入：固定 640x640 RGB888，单 batch。
- 类别：COCO 80 类。
- RKNN：FP16，目标平台严格为 `rk3566`。
- 工具链：RKNN-Toolkit2 2.3.2，与开发板 RKNN Runtime 2.3.2 对齐。
- 转换配置：`mean_values=[[0,0,0]]`、`std_values=[[255,255,255]]`、
  `do_quantization=false`。

旧目录内现存 detector 模型的内部目标平台均为 `rk3588`，不得复制到 RK3566 工程或作为
加载失败后的回退模型。

### 3.2 模型清单

detector 使用独立清单 `models/detector-manifest.json`，至少记录：

- ONNX/RKNN 文件名和 SHA-256；
- 来源 URL 或仓库提交；
- RKNN-Toolkit2 版本、目标平台和量化类型；
- 输入尺寸、色彩顺序和 letterbox 填充值；
- 输出数量、每个输出的逻辑角色和类别数量；
- 类别表文件及其 SHA-256；
- `calibrated=false` 和“仅功能部署”的状态。

启动时，程序必须验证模型文件存在且清单声明为 `rk3566`。运行时还必须查询实际输入输出
张量；张量形状与清单不一致时立即失败，不得尝试猜测布局继续运行。

### 3.3 后续模型替换边界

后续定制可见光或红外模型只要保持受支持的 YOLOv8 DFL 输出契约，即可通过模型路径、
类别表、类别过滤配置和 manifest 替换，不改变长时控制器。若输出契约改变，应新增明确的
decoder 类型而不是在当前 decoder 中静默猜测。

## 4. 模块设计

### 4.0 `DetectorEngine`

在异步 adapter 与 RKNN 实现之间增加最小引擎接口：

```text
infer(owned BGR frame, DetectorRequest) -> EngineResult
```

`EngineResult` 包含候选、预处理/推理/后处理耗时和原始/过滤后数量。生产实现为
`YoloRknnEngine`；主机测试注入确定性的 fake engine。这样线程、请求身份和异常降级可以在
没有 RKNN/OpenCV 的主机测试，而不会用假 RKNN 头文件证明真实推理。

### 4.1 `YoloV8Postprocessor`

纯 C++17 模块，不依赖 RKNN、OpenCV 或线程。职责：

- 接收三尺度 DFL box、class score 和可选 score-sum tensor view；
- 支持官方优化模型的 9 输出，以及同一语义下无 score-sum 的 6 输出；
- 从 tensor shape 推导网格、DFL 长度和类别数，但只接受 NCHW、三个尺度和一致类别数；
- 支持 FP32 view 以及带 zero-point/scale 的 INT8 view，便于后续量化；
- 执行置信度过滤、DFL softmax 解码、逐类别 NMS 和最多 N 个候选截断；
- 将模型坐标反 letterbox 映射回 detector ROI，再映射到原始视频坐标；
- 裁剪边界并拒绝 NaN/Inf、非正面积或完全位于 ROI 外的框。

排序必须使用候选自身的置信度，禁止出现旧实现中排序索引与置信度数组错位的问题。

### 4.2 `YoloRknnEngine`

板端模块，依赖 RKNN Runtime 和 OpenCV；使用 RAII 管理：

- 模型字节和 `rknn_context`；
- 输入/输出 tensor attributes；
- 每次推理取得的 RKNN 输出；
- 预处理缓冲区。

构造时完成模型加载、版本查询和 tensor contract 验证。推理流程为：

1. 校验 BGR8 图像和 ROI；
2. 截取 ROI；
3. BGR 转 RGB；
4. 等比例缩放到 640x640，以 RGB `(114,114,114)` letterbox；
5. `rknn_inputs_set`、`rknn_run`、`rknn_outputs_get`；
6. 调用 `YoloV8Postprocessor`；
7. 无论成功或异常都释放本次输出。

RK3566 不设置 RK3588 的三核 core mask。任何 RKNN 调用失败都转成带阶段、返回码和模型路径
的异常，由 adapter 捕获并形成可审计错误；不得 `printf` 后继续使用半初始化状态。

### 4.3 `YoloDetectorAdapter`

实现现有 `DetectorAdapter` 语义，但补充提交就绪状态，避免慢 detector 导致请求身份不断
被新请求覆盖：

- `available()`：模型已配置且 worker 正常；
- `can_submit()`：当前没有在途请求、待处理请求或未消费结果；
- `submit()`：只在 `can_submit()` 为真时接受请求，复制 ROI 像素和完整请求身份；
- `poll()`：非阻塞返回最多一个完成批次；
- 析构：通知 worker、等待当前推理结束、join，并释放 engine。

内部只有：

- 一个工作线程；
- 一个可选 pending request；
- 一个可选 completed result；
- 一个 mutex 和一个 condition variable。

不使用 detached thread、无界 queue、每帧新建 RKNN context 或多 context 并行。请求图像必须
由 adapter 自有，不能保存调用方 `ImageView` 指针。

若推理失败，worker 保存结构化错误。`poll()` 将错误交给运行时记录，adapter 随后标记为
不可用；具体契约为下一次 `poll()` 抛出一次包含阶段和错误码的异常，runtime 捕获并写入
当前帧诊断，之后 `available()` 返回 false。NanoTrack 路径继续工作。若模型未配置，仍使用
`NullDetectorAdapter`。

### 4.4 detector 数据契约补充

`DetectorAdapter` 增加 `can_submit()`，控制器的 `begin_frame` 只在 detector 已配置且就绪时
生成请求。这样 controller 只有在请求实际可以接受时才更新 `last_detector_request_frame`，
避免慢 detector 的旧结果被无意义地判成 frame mismatch。

`DetectorBatch` 继续携带：

- `source_frame_id`；
- `source_timestamp_ms`；
- `bbox`、`confidence`、`class_id`。

controller 保留当前 150 ms 视频时间新鲜度、请求 frame identity、最多三个候选、下一帧
NanoTrack 再验证和 2/3 确认规则。detector 候选绝不能直接设置 `control_valid=true`。

runtime 增加 `DetectorDiagnostics`，承载 submit/poll 状态、请求/批次 frame id、候选计数、
分段延迟和错误文本；该对象只用于日志，不参与 controller 决策。

## 5. 数据流

1. `TRACKING/UNCERTAIN/LOCAL_RECOVERY`：不调用 detector。
2. `ASSISTED_RECOVERY/LOST` 且满足 500 ms 周期：
   - adapter 已配置且 `can_submit=true` 时，controller 创建全图 `DetectorRequest`；
   - runtime 将当前帧复制给 adapter，随后继续本帧 NanoTrack 区域搜索；
   - detector worker 独立完成 YOLO 推理。
3. 后续帧调用 `poll()`：
   - frame identity、视频时间新鲜度、框合法性不通过则记录并丢弃；
   - 合法候选按 detector confidence 排序并截断为三个；
   - 下一帧每次只取一个候选作为 NanoTrack 搜索假设。
4. NanoTrack 对该区域运行 anchor/recent 验证：
   - 可靠候选进入已有确认窗口；
   - 最近三个时刻至少两个空间/形状一致才重捕获；
   - 重捕获后重新初始化运动模型，immutable anchor 不变。

## 6. 类别策略

新增 `--detector-class-id N`：

- `N >= 0`：只保留该类别；
- `N = -1`：允许所有类别，默认值；
- 超出模型类别范围立即报错。

首版板端验证中，person 视频使用 COCO class 0，car 视频使用 class 2。类别只减少候选范围，
不代表目标身份；同类别多目标仍必须由 NanoTrack anchor/recent 模板确认。

如果初始目标不属于 detector 类别，系统仍能做区域模板恢复，但不能宣称具备全图找回能力。
本阶段不自动从初始框推断类别，避免一次错误检测永久锁错类别。

## 7. CLI 和配置

新增可选参数：

```text
--detector-model FILE
--detector-manifest FILE
--detector-labels FILE
--detector-class-id N
--detector-conf FLOAT
--detector-nms FLOAT
```

规则：

- 三个 detector 文件参数要么全部省略，要么全部提供；
- 未提供时使用 `NullDetectorAdapter`；
- confidence 和 NMS 必须有限且位于 `(0,1)`；
- class id 默认 `-1`；
- 参数写入 session metadata，模型和类别文件写入 SHA-256；
- 不改变现有必填参数和无 detector 命令行为。

首版默认 detector conf 为 0.25、NMS 为 0.45。它们是功能 bring-up 值，必须记录为
`calibrated=false`，不得作为产品阈值。

## 8. 构建和文件组织

建议新增：

```text
include/yolov8_postprocess.hpp
include/yolo_rknn_engine.hpp
include/yolo_detector_adapter.hpp
src/yolov8_postprocess.cpp
src/yolo_rknn_engine.cpp
src/yolo_detector_adapter.cpp
tests/test_yolov8_postprocess.cpp
tests/test_yolo_detector_adapter.cpp
scripts/convert_yolov8_rk3566.py
models/yolov8n-rk3566-fp16.rknn
models/coco80.txt
models/detector-manifest.json
```

CMake 组织：

- `nanotrack_yolov8_postprocess` 始终可在主机编译测试；
- `nanotrack_yolo_detector` 只在 `ENABLE_RKNN_RUNTIME=ON` 时编译；
- 主程序在板端链接 detector 模块；主机逻辑测试不需要 RKNN 头文件；
- 继续使用现有 OpenCV 和 `librknnrt.so` 路径，不引入旧 detector 动态库或 RGA 依赖。

首版模型文件可以随隔离板端工程部署，但不得复制进旧
`/home/cat/Project/SOT/nanotrack_v3`。

## 9. 错误处理和安全约束

- 模型缺失、哈希不匹配、目标平台错误或 tensor contract 不匹配：启动失败。
- detector 推理异常：记录一次结构化错误并禁用 detector，NanoTrack 继续读视频。
- 请求在途时：不创建新 detector request，不覆盖 controller 请求身份。
- 结果过期、frame mismatch、非法框或空结果：记录状态，不改变运动模型和模板。
- detector 结果只能成为搜索假设，不能直接提交 bbox。
- 任何未完成重捕获确认的输出保持 `control_valid=false`。
- 队列和帧副本数量有固定上限，长时间 LOST 不得产生内存累积。

## 10. 日志和元数据

逐帧 CSV 在现有字段基础上增加或明确记录：

- detector 是否 configured/ready/requested；
- request frame id；
- worker 状态；
- batch source frame id 和 age；
- raw candidate count、filtered count；
- 被验证候选的 class id/confidence；
- preprocess/inference/postprocess/total detector latency；
- detector disable/error reason。

session metadata 增加：

- detector enabled、model/manifest/labels 路径和 SHA-256；
- RKNN API/driver、输入输出 tensor 描述；
- conf/NMS/class filter；
- detector request/complete/reject/error 计数及延迟摘要。

日志不得把 detector 成功等同于重捕获成功；两者必须分开统计。
新增字段使 CSV/metadata `schema_version` 从 2 升为 3，板端 verifier 必须按版本检查；不得在
不改版本号的情况下改变列语义。

## 11. 测试和验收

### 11.1 主机测试

- DFL 固定张量的字面值 box 解码；
- FP32/INT8 等价解码；
- score-sum 有/无两种输出；
- 每类 NMS、置信度排序、top-N、class filter；
- letterbox 正反映射和边界裁剪；
- 拒绝错误输出数量、shape、类别不一致、非有限值；
- adapter fake-engine 的异步请求、`can_submit`、poll、异常禁用和析构 join；
- Null adapter 与旧 CLI 回归；
- detector 候选必须再经 NanoTrack 2/3 确认，首个候选不得有效控制。

### 11.2 模型转换验证

- 保存 ONNX/RKNN 哈希和转换日志；
- `strings` 或板端 RKNN 日志确认 target platform 为 RK3566；
- 官方测试图像的 ONNX/RKNN 候选类别和坐标在允许差异内一致；
- 不把“成功导出文件”视为板端推理通过。

### 11.3 RK3566 功能验收

1. 板端原生干净构建和完整 CTest 通过。
2. detector 模型加载，输出 tensor contract 与 manifest 一致。
3. 官方测试图像产生可解码标注图和候选 CSV。
4. person 视频以 class 0 运行：日志中出现 detector request、batch、NanoTrack 验证和重捕获
   状态；若未找回，保留工件并如实报告，不以程序无崩溃代替算法成功。
5. car 循环视频以 class 2 运行：目标消失时 `control_valid=false`，重新出现后只能在 2/3
   确认完成时恢复为 true。
6. detector 关闭后旧命令和 `NullDetectorAdapter` 路径仍通过。
7. 至少 10 分钟 detector-enabled 运行无崩溃、无队列增长和无持续内存增长。已有
   detector-free 36 分钟稳定性证据保留，但不能替代本项。

本阶段记录 detector 和整条 pipeline 的实际 latency、FPS、RSS、温度与 NPU 信息，但不设
性能通过线。模型性能和阈值优化作为后续独立工作。

## 12. 完成定义

只有同时具备以下证据才可称为“YOLO detector 已部署并可供长时跟踪使用”：

- 官方来源 ONNX、RK3566 FP16 RKNN、类别表和 manifest 均有哈希；
- 新实现不依赖旧 detector 二进制或硬编码路径；
- 主机单元/集成测试通过；
- RK3566 原生构建、模型加载和真实 detector 推理通过；
- detector batch 带正确请求身份进入控制器；
- 检测框不绕过 NanoTrack 验证和 2/3 确认；
- 至少一段真实视频产生完整 detector 辅助恢复证据；
- detector-enabled 稳定性测试通过；
- 所有未标定阈值继续标记 `calibrated=false`。

