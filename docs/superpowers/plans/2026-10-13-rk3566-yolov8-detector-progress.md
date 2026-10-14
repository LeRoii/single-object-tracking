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
- Task 4 / CLI：`test_cli` 编译失败，明确缺少 detector 文件、类别、置信度和 NMS 配置字段。
- Task 4 / 日志与元数据：`test_tracking_log` 编译失败，明确缺少 schema-v3 detector 字段、候选类别/置信度审计信息和会话元数据序列化接口。
- Task 4 / 初始化行：新增配置/就绪状态测试后，编译失败，证明旧接口无法记录 detector 初始化状态。
- Task 5 / manifest：pytest 收集失败，明确缺少 `verify_detector_manifest.py`。
- Task 5 / 转换 CLI：pytest 收集失败，明确缺少 `convert_yolov8_rk3566.py`。

## GREEN 证据

- 基线：主机 CTest 7/7 通过。
- Task 1 定向测试：`controller_support`、`long_term_runtime` 2/2 通过。
- Task 1 完整主机回归：CTest 7/7 通过。
- Task 2 adapter 定向测试 1/1 通过，覆盖 stride 深拷贝、有界槽、poll、异常禁用和析构 join。
- Task 2 ASAN+UBSAN 1/1 通过；受 ptrace 环境限制，以 `ASAN_OPTIONS=detect_leaks=0` 关闭 LSan。
- Task 2 完整主机回归：CTest 8/8 通过。
- Task 3 后处理定向测试 1/1 通过，覆盖 6/9 输出、DFL、FP32/INT8、NMS、类别过滤、letterbox 和非法输入。
- Task 3 ASAN+UBSAN 1/1 通过；完整主机回归 CTest 9/9 通过。
- Task 4 定向测试 3/3 通过，覆盖 detector CLI 完整性与阈值边界、schema-v3 精确列、CSV 转义、候选审计字段、初始化行和 detector 元数据。
- Task 4 `main.cpp` 使用系统 OpenCV 头和最小临时 RKNN 类型桩完成 `-fsyntax-only` 检查；这不是板端链接或运行证据。
- Task 4 完整主机回归：CTest 9/9 通过。
- Task 5 manifest/转换工具测试 19/19 通过；`python -S` 环境中的 `--help` 和仅校验 manifest 路径通过，证明不会提前导入 RKNN。
- 固定环境 `/home/u20/single-object-tracking/.venvs/rknn-convert/bin/python` 为 Python 3.10.12；`rknn-toolkit2==2.3.2` 实际导入成功，未替换版本。
- 官方来源：`airockchip/rknn_model_zoo@837db8e992815ff8647eadb709f07067f4bc3a71`。优化版 ONNX 为 12,650,184 字节，SHA-256 `0c8716701f471067932b797eeb67c8e5db47c693c2557c881d7679ec12e21bc5`；ONNX checker 通过并确认 9 输出契约。
- 官方 COCO 标签为 621 字节、80 行，SHA-256 `bd17f1ee35d5f3c862a4894605855abbb9dda4b0621fdb0ac4c2c8c7bb7e730a`。
- RK3566 FP16 转换成功：最终保留 RKNN 8,097,993 字节，SHA-256 `4d53a9b1e12996556294be8e099fe2dc0815d5c9cfdc327edfe350832baf8cf6`；完整日志 84,868 字节，SHA-256 `ab45cbe6eb167d626bf9cefc182b2c3972cbd3c8b129ade24b9d1535032be3cb`。
- 生成 manifest 校验通过；RKNN 内嵌元数据可见 `target_platform:["rk3566"]`、版本 2.3.2、一个 640×640 输入和 9 个预期输出。这里只证明转换产物，不等同于板端推理。
- Task 6 主机逻辑构建及 CTest 10/10 通过；使用板端 RKNN Runtime 2.3.2 真实头文件对 engine、smoke 和 main 完成语法检查。
- Task 6 第一次板端构建在 GCC 8.3 的 `std::filesystem` 链接处 RED（缺失 `stdc++fs`）；添加旧 GCC 条件链接后原生构建 GREEN。
- Task 6 第一次模型启动在错误的 NCHW 输入假设处被严格契约检查拒绝；真实模型属性为 `1×640×640×3 FP16 NHWC`。修正为实际 NHWC 内部张量验证后，保留外部 `UINT8 NHWC RGB` 提交契约，真实推理 GREEN。

## 开发板证据

- 隔离发布目录：`/home/cat/Project/SOT/nanotrack_v3_longterm/releases/rk3566-yolov8-detector`；未修改 `/home/cat/Project/SOT/nanotrack_v3`，也未覆盖已有长期跟踪目录。
- 板卡：`lubancat`，AArch64，Linux `4.19.232`，内存 3,990,208 kB，验证时可用 3,628,296 kB；根分区 29 GiB，验证时剩余 20 GiB。
- 编译器 GCC 8.3.0，OpenCV 4.10.0，RKNN API `2.3.2 (429f97ae6b@2025-04-09T09:09:27)`，driver `0.8.2`。
- 板端干净配置并原生构建成功；进入构建目录执行完整 CTest 10/10 通过。板端旧 CTest 不支持 `--test-dir`，因此最终证据采用 `cd build-board-yolo && ctest --output-on-failure`。
- `ldd` 确认两个 AArch64 ELF 均解析到 `/home/cat/ThirdParty/OpenCV4100/lib/libopencv_world.so.410` 与 `/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/aarch64/librknnrt.so`，没有 `not found`。
- 真实视频帧 smoke 推理成功：2 个 `person` 候选，置信度 0.948730/0.937012；raw=20、filtered=2；单次 preprocess=19.475 ms、inference=190.856 ms、postprocess=44.834 ms、total=255.165 ms。该值只是一帧功能证据，不设性能门槛。

## 工件

- 板端 `nanotrack_v3`：SHA-256 `9a16f4a8a0eaa3dbaea9d2c7150f9e1585743def9be1e47a64aa702089e1480e`。
- 板端 `yolo_detector_smoke`：SHA-256 `47a54eb7713f1fb54b44a2ae892bb43b6a0a7ce75a8071a0cdc6f7f3482eaba1`。
- smoke 输入：`artifacts/task6-smoke/input.jpg`，SHA-256 `cdda98096936407912313796db1aa44801a58c224c547e6238fe66c65e350a42`。
- smoke 标注图：`artifacts/task6-smoke/output.jpg`，SHA-256 `957aa3f6a79b9501865dd4205968db78ad76f3259ce6bef2241c50b7bb791eb6`。
- smoke 候选：`artifacts/task6-smoke/candidates.csv`，SHA-256 `53fc2666a47a479283b067a30702ca480adb662f2c856930b67898e19203e305`。

## 偏差与裁决

- 使用实际 Git 提交和 SDD ledger 取代计划中“非 Git 工作区”的假设；测试、模型和板端证据要求不变。
- `DetectorBatch` 保留原有 `frame/timestamp/candidates` 三字段顺序，新增计数和耗时字段追加在后，避免破坏旧聚合初始化调用方。
- `tracking_log.cpp` 在 Task 1 先做最小兼容映射；完整 schema v3 仍在 Task 4 实施。
- schema-v3 序列化放在可主机测试的 `tracking_log` 模块；Task 6 接入实际 detector engine 后再填入 tensor 描述、SHA-256、detector RKNN 版本及运行计数。
- `SearchPlan` 仅为审计保留被取出的 detector 候选类别和置信度；候选仍只能作为 NanoTrack 搜索假设，不能直接产生有效控制框。
- 本独立实现保留已批准的 letterbox pad=114；官方当前 Python 示例因 RGA 初始化路径使用黑色 padding。两者可能令边缘输出略有差异，必须以板端 smoke 的候选合理性作为后续验证门槛。
- 相同声明输入的第二次 Toolkit2 转换得到相同文件大小但不同 RKNN SHA-256，因此不宣称序列化字节级确定性；manifest 始终绑定最终保留产物，任何重建都必须重新生成并复核哈希。
- 当前托管环境不支持 ptrace 下的 LeakSanitizer；本阶段保留 ASAN/UBSAN，并由析构 join 测试覆盖线程生命周期，LSan 证据缺失明确保留。
- 板端系统时钟停留在 2019，部署文件时间来自 2026，GNU Make 会报告 clock skew 并保守重建；本次使用干净构建且产物/测试/推理均已核验，但量产环境应同步系统时钟。
- 原计划指定固定长期目录；该目录已有本工程之前的部署。为避免破坏或覆盖，当前版本放入其 `releases/rk3566-yolov8-detector` 子目录。

## 未解决风险

- generic COCO detector 尚未做业务域阈值标定，`calibrated=false` 保持不变。
- 单帧 smoke 已证明模型加载和真实推理；视频集成、恢复正确性、detector-disabled 回归和 10 分钟稳定性仍需 Task 8。
- Toolkit 转换日志包含部分常量空 tensor 与默认 float16 qtype 警告；转换成功且产物契约通过，但其板端影响仍需 Task 6/8 实测。
