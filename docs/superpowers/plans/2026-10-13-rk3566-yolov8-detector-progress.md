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
- Task 7 detector 身份测试 RED：同一源帧号但错误源时间戳的 batch 被旧逻辑接收。controller 改为同时核对 request 的 frame ID 和视频时间戳后 GREEN。
- Task 7 恢复回放覆盖：单一高置信候选不能直接恢复、两个一致的 NanoTrack 验证才满足 2/3、空 batch（含类别过滤后为空）、非法框、同类干扰目标、慢 detector 原始请求身份及 170 ms 过期结果丢弃。
- Task 7 主机 CTest 10/10、Python manifest/conversion 测试 25/25 通过；ASAN+UBSAN CTest 10/10 通过，延续 `detect_leaks=0` 的已记录环境限制。
- Task 8 第一次 4200 帧稳定性运行在 543 秒/2380 输出帧处 RED：LOST 大范围搜索中的一个有限数值 NanoTrack proposal 整体落在画面外，旧代码以 `decoded box is outside the frame` 终止进程。
- Task 8 修复将且仅将“完整落在画面外”的 head proposal 转换为本帧无 observation；非有限数值、模型接口及 RKNN 错误仍为致命错误。新增纯函数回归后，主机及 ASAN+UBSAN CTest 均 10/10 通过。

## 开发板证据

- 隔离发布目录：`/home/cat/Project/SOT/nanotrack_v3_longterm/releases/rk3566-yolov8-detector`；未修改 `/home/cat/Project/SOT/nanotrack_v3`，也未覆盖已有长期跟踪目录。
- 板卡：`lubancat`，AArch64，Linux `4.19.232`，内存 3,990,208 kB，验证时可用 3,628,296 kB；根分区 29 GiB，验证时剩余 20 GiB。
- 编译器 GCC 8.3.0，OpenCV 4.10.0，RKNN API `2.3.2 (429f97ae6b@2025-04-09T09:09:27)`，driver `0.8.2`。
- 板端干净配置并原生构建成功；进入构建目录执行完整 CTest 10/10 通过。板端旧 CTest 不支持 `--test-dir`，因此最终证据采用 `cd build-board-yolo && ctest --output-on-failure`。
- `ldd` 确认两个 AArch64 ELF 均解析到 `/home/cat/ThirdParty/OpenCV4100/lib/libopencv_world.so.410` 与 `/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/aarch64/librknnrt.so`，没有 `not found`。
- 真实视频帧 smoke 推理成功：2 个 `person` 候选，置信度 0.948730/0.937012；raw=20、filtered=2；单次 preprocess=19.475 ms、inference=190.856 ms、postprocess=44.834 ms、total=255.165 ms。该值只是一帧功能证据，不设性能门槛。
- Task 8 使用独立 `build-board-acceptance` 做干净原生构建；修复后的最终板端 CTest 10/10 通过。
- 官方 Ultralytics `bus.jpg`（810×1080，SHA-256 `c02019c4979c191eb739ddd944445ef408dad5679acab6fd520ef9d434bfbc63`）真实推理得到 5 个候选：4 个 `person`、1 个 `bus`。人工检查标注框和类别合理；total=257.344 ms，仅作单帧功能证据。
- 人员视频 detector-enabled：987 输出帧校验通过，58/58 detector request/complete、0 reject、0 error；58 个 batch 与 113 个 detector hypothesis 可在 schema-v3 中分别审计。目标在第 99 帧后丢失且未找回，`reacquired=0`，因此视频推理通过但恢复效果失败。
- 车辆两轮循环 detector-enabled：701 输出帧校验通过，32/32 request/complete、0 error；LOST 429 行且全部 `control_valid=false`。第二轮目标重现后未找回；4 次 `reacquired` 均发生在第一轮且来源为 motion，不作为 detector 长时找回证据。
- 同一人员视频 detector-disabled：987 输出帧校验通过，metadata `enabled=false`，CSV 全部 `detector_configured=0`，request/complete/error 均为 0；旧命令行为和 schema-v3 保持可用。
- 修复后的稳定性运行：连续 1267 秒、4201 输出帧/4200 跟踪帧，272/272 request/complete、0 reject、0 error，线程数恒为 9，最大在途差为 1，温度 28.888–31.875°C，瞬时 NPU load 2%–18%。所有 4012 个 LOST 行均 `control_valid=false`。
- 稳定性时延边界：mean runtime=214.973 ms、`runtime_fps=4.652`（跟踪 runtime，不含视频 I/O/叠加/编码）；mean pipeline=295.998 ms、`end_to_end_fps=3.378`（完整管线）。不设置性能通过线。
- 稳定性内存：60 秒后 RSS 首/尾为 147,152/149,328 kB，中位数 171,772 kB，线程恒定且多次回落到约 149 MB；但周期峰值随运行抬升，最大 RSS 446,320 kB、VmHWM 476,684 kB，线性拟合约 +102.116 kB/s。因此功能稳定性通过，但内存峰值风险未关闭。
- Task 8 分项：板端构建 PASS；单元测试 PASS；模型加载 PASS；官方图推理 PASS；人员/车辆视频推理 PASS；detector-disabled 回归 PASS；控制安全门 PASS；21 分钟进程与队列稳定 PASS；目标消失后找回 FAIL；内存峰值稳定性 RISK。
- Task 9 清理审计发现旧 NanoTrack RKNN wrapper 的异常/析构清理返回码未记录；独立代码审查又发现 `outputs_release` 失败时可能二次释放，以及 YOLO 清理失败会遮蔽主异常。最终版本使用可单测的一次性清理守卫：调用前先撤销 armed 状态，任何结果都只尝试一次；异常路径保留主异常并单独记录清理错误。
- 审查前的 `final-smoke-120` 完成 120 跟踪帧和工件校验，但 request/complete 为 0/0，只证明 detector 配置/模型启动和 NanoTrack 正常路径，不再称为 detector 推理证据。
- 修复后二进制完成板端 CTest 11/11，并在 `final-detector-250` 完成 250 跟踪帧：request/complete/reject/error=9/8/0/0，结束时仅保留一个允许的在途请求，`verify_board.sh` 通过。长稳证据仍使用修复前、正常推理逻辑相同的 SHA-256 `e95ef3d1...f5c` 二进制；清理错误路径修复后的最终交付二进制 SHA-256 见下。

## 工件

- 21 分钟稳定性二进制 `build-board-acceptance/nanotrack_v3`：SHA-256 `e95ef3d17b2673ec4fb5b26f9d2c2875806ffa101ef4f5deb9f69bcbacc36f5c`。
- 最终清理审计后二进制：SHA-256 `a1bf9773ed2c042f542ffdd28b78ef75a69b3d9a051f3b564aa7a1c8c2c108c4`。
- 板端 `yolo_detector_smoke`：SHA-256 `c83f9c4dc3a64cc6af9f569e0a57c7499d6b1b68a6bae2401cae9f1b9e59407f`。
- smoke 输入：`artifacts/task6-smoke/input.jpg`，SHA-256 `cdda98096936407912313796db1aa44801a58c224c547e6238fe66c65e350a42`。
- smoke 标注图：`artifacts/task6-smoke/output.jpg`，SHA-256 `957aa3f6a79b9501865dd4205968db78ad76f3259ce6bef2241c50b7bb791eb6`。
- smoke 候选：`artifacts/task6-smoke/candidates.csv`，SHA-256 `53fc2666a47a479283b067a30702ca480adb662f2c856930b67898e19203e305`。
- Task 8 根目录：`artifacts/20261014-acceptance`。
- 官方图标注输出：`official-smoke/bus-annotated.jpg`，SHA-256 `b710b489f7f097975b6b2b6453e6b9706f344f357201c3d1faaac6289cc87e08`；候选 CSV SHA-256 `34d3c06ab1ca0b56540f8443c8a2e7aae6b31cb2b7c11f71a91fce9373e2a235`。
- 人员 detector-enabled：`person-video/{output.avi,bboxes.csv,session-metadata.json,run.log}`；对应 SHA-256 `2ec5e34265ca85563849a380bcd99e0008f37d8229c1c6a7e888c58e7fbb60ce`、`f735bac077079dae1560b34df81b2e59b4cffd0bc86e82e192404e1da41039b0`、`abbc509a3a9ad53788a82e698f14c925e5b58ccc171e58176526dea8146639af`、`57ab757ebc7cca5654339349e928eda47d5260b367aa894d26f4d7f70d24ebe9`。
- 车辆循环：`car-loop-700/{output.avi,bboxes.csv,session-metadata.json,run.log}`；对应 SHA-256 `f049a94651f7f4fff666fbca181d41f31a7cc5dea1e204e9fb84b072db1dcb56`、`4fdb9b616f725a96a7eb389ab87fc46e06fd6b08a1e492d19f348ef2c5929522`、`74f464cfd08b33a712b3529e1ed80729ee7f7c043dfd9e0dbb164fbcbf2e58c6`、`7a6f46ddc5f4294713ef173d26d66cd5445283af064ee4156b250f67cb3934ab`。
- detector-disabled：`person-no-detector/{output.avi,bboxes.csv,session-metadata.json,run.log}`；对应 SHA-256 `0786b850b6691da2ce8837634d7c2ce3e9075fb7d9bd57ceb965405c403827f1`、`9f18a2389af9f296ae8f3d446c5d805460f5da9d4b0cb3bfb677466f7fe021ec`、`4e044dfa98ea7e331194cf4c8c4f28c8483d45c30b17fbeb27ef8b843ad1b9d8`、`5782f052ebea60070601f9fcf7cb99c635f8234c8fe43c9bc23330a9ec5572a2`。
- 21 分钟稳定性：`stability-4200-v2/{output.avi,bboxes.csv,session-metadata.json,run.log,resources.csv,wall-seconds.txt}`；完整 SHA-256 依次为 `7c9d71fe612959fa167f13339c1ee71cbed1a6f73144062e1132d77128d8dc86`、`e69af12359720a4eb2f94122599bb35c802f02c2ec43429a07afe69401330b17`、`0bb09e5d2e512754591a797fee9f525e5a27d054052844b1e9cc1c680fd7d0e1`、`f7ea149fac9b05e59d945e31bb1986797f443ef2f99297c78ca976098b26f981`、`9364ff75bbb48649119aab5dc1599cde5761ee7a18ba749b7d9e6714717be79f`、`2faac09313662ca9ae94dde4aaba6fd7c86d6d58e9b84ae749604c9e9db30646`。
- 审查前 120 帧配置/启动 smoke：`final-smoke-120/{output.avi,bboxes.csv,session-metadata.json,run.log}`；request/complete=0/0，对应 SHA-256 `8859ae3d14653315cf76419c069d0f198950d7751026a1ec5392fb6e7075cb21`、`6a27e842abde999b7c49549c9869bbd1652582c5decf69f98a0a140ce7d4a29a`、`1ee32c0b628837888ca9bad3df29adcdb5dec349c014485cc90d76d7cbd6fc9f`、`61727d35fadc0fc30325ea44decedc08c39f44f00a576052d77f79b5f462a603`。
- 最终 detector 推理短测：`final-detector-250/{output.avi,bboxes.csv,session-metadata.json,run.log}`；对应 SHA-256 `204c0170c7bbec5329cabfcf38e16a1b81e263429a1bf229a6c59e3bd4095fec`、`fe59aa495ba5005125ce4d90d11e8656df0921fe2be3c17402d9d7e2a58ac032`、`15a0fce8943d4fd7b71dd7d43998a7e82629f674402578c3cd07ab013bc21c01`、`93b0aa0d9bb1940d0d2e74021d9846ce53d17800d74b4ee59c78cb5e24063d7f`。
- 最终板端 CTest 日志：`ctest-final-cleanup.log`，11/11，通过；SHA-256 `666d76800eda9c0b3e7d00495bbadcf71d2e71ab6e46881bcf135f2418308c7d`。

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
- 官方图来源采用 Ultralytics 官方 GitHub release asset，而非 Rockchip 示例中未随仓库保存的本地图片；图像只用于通用 COCO 输出合理性检查。
- 首次稳定性失败工件 `stability-4200` 被保留，修复后使用新目录 `stability-4200-v2` 从零重新运行，没有覆盖或拼接时长。

## 未解决风险

- generic COCO detector 尚未做业务域阈值标定，`calibrated=false` 保持不变。
- 人员和车辆实测均未证明“丢失后可找回”；detector 候选进入了 NanoTrack 验证链，但现有模板验证/确认配置没有重新建立有效控制。恢复正确性当前为 FAIL，不能作为长时稳定跟踪交付。
- 通用 COCO 类别不能解决同类多目标身份歧义；当前没有 ReID 或目标特征检索，错误同类目标只能依赖 NanoTrack 模板门抑制。
- 21 分钟运行的首尾 RSS 接近，但周期峰值和 VmHWM 随进度抬升；需进一步隔离 OpenCV AVI writer、帧缓冲及 allocator 行为，并在修复后重复稳定性验收。
- Toolkit 转换日志包含部分常量空 tensor 与默认 float16 qtype 警告；真实板端图像/视频推理已成功，但这些警告仍应随模型替换重新审查。
