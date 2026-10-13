# RK3566 NanoTrack Long-term Controller Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在现有 RK3566 NanoTrack V3 视频程序上实现双模板、时间驱动状态机、受限重捕获、可观测日志和预留 detector 接口的长时跟踪控制器。

**Architecture:** 将状态机、运动预测、质量融合、配置和 detector 数据契约实现为不依赖 RKNN/OpenCV 的 C++17 核心库；NanoTrack 运行时只负责单次 search feature 与双模板 head 推理，并在策略确认后提交状态。视频入口负责单调时间、适配器调度、模板动作和完整日志，默认使用 `NullDetectorAdapter`。

**Tech Stack:** C++17、CMake/CTest、OpenCV、RKNN Runtime、现有 NanoTrack V3 RKNN 三模型。

**Spec:** `docs/superpowers/specs/2026-10-13-rk3566-nanotrack-long-term-controller.md`

## Global Constraints

- 目标平台为 RK3566；交叉编译不在本计划范围内，但源码必须支持开发板原生构建。
- 首版沿用现有 `template_backbone.rknn`、`search_backbone.rknn`、`head.rknn` 和 `manifest.json`。
- 正常阶段每帧最多一次 search backbone；anchor/recent 可复用 search feature 分别运行 head。
- detector 只预留接口，默认 `NullDetectorAdapter`；不可用时长时控制器必须继续运行。
- 视频为首版唯一输入；现有必填 CLI 参数保持兼容，新增参数可选。
- 可见光/红外配置独立；内置阈值只作为未标定默认值并在日志标记。
- `control_valid` 只允许可靠跟踪或完成 2/3 确认后的重捕获结果为 true。
- 没有板端证据不得宣称 RK3566 性能或部署验收完成。
- 当前目录不是有效 Git 仓库：不创建 worktree、不提交；每项 RED→GREEN 命令和裁决写入同名 progress ledger。

## Review Focus

- 视频 PTS 缺失、重复或回退时，程序应生成严格单调时间或明确失败，不得破坏状态时序；Task 5 覆盖。
- NanoTrack 任一 head 输出 NaN/Inf 或非正框时，应拒绝该帧且不得提交运动/模板状态；Task 1、4 覆盖。
- anchor/recent 给出两个高分但空间冲突的候选时，应优先 anchor 且进入确认，不得直接有效控制；Task 3 覆盖。
- detector 批次晚到、frame id 不匹配或候选超过三个时，应可审计地丢弃/截断；Task 3 覆盖。
- LOST 状态长时间无 detector 时，应持续有界搜索且内存保持有界、`control_valid=false`；Task 3、6 覆盖。

---

### Task 1: NanoTrack 响应统计

**Files:**
- Modify: `rk3566/nanotrack_v3/include/tracker_math.hpp`
- Modify: `rk3566/nanotrack_v3/src/tracker_math.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_tracker_math.cpp`

**Interfaces:**
- Consumes: 现有 `decode_head(cls, loc, state, config, scale_z, frame_width, frame_height)`。
- Produces: `ResponseMetrics {raw_score, penalized_peak, psr, apce, peak_margin}`；`DecodeResult::metrics`；保持 `DecodeResult::score` 兼容。

- [x] **Step 1: 写失败测试**：手工构造 3x3 分类/回归张量，断言 raw score、第二峰 margin、PSR、APCE 为独立计算的字面值；NaN 和零方差旁瓣分别拒绝或给出有限约定值。
- [x] **Step 2: 运行** `cmake -S rk3566/nanotrack_v3 -B /tmp/nanotrack-longterm-build -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF && cmake --build /tmp/nanotrack-longterm-build -j2`；**Expected:** 编译失败，缺少 `ResponseMetrics`。
- [x] **Step 3: 实现** 在一次 decode 循环中保存 penalized response map，排除峰值 3x3 邻域计算 PSR，使用全图最小值计算 APCE，并计算非邻域第二峰 margin；所有返回值必须有限。
- [x] **Step 4: 重跑 focused test**；**Expected:** `tracker_math tests passed`。
- [x] **Step 5: 运行** `ctest --test-dir /tmp/nanotrack-longterm-build --output-on-failure`；**Expected:** 现有与新增测试全部通过。

### Task 2: 配置、运动模型与 detector 契约

**Files:**
- Create: `rk3566/nanotrack_v3/include/long_term_types.hpp`
- Create: `rk3566/nanotrack_v3/include/controller_config.hpp`
- Create: `rk3566/nanotrack_v3/src/controller_config.cpp`
- Create: `rk3566/nanotrack_v3/include/motion_estimator.hpp`
- Create: `rk3566/nanotrack_v3/src/motion_estimator.cpp`
- Create: `rk3566/nanotrack_v3/include/detector_adapter.hpp`
- Create: `rk3566/nanotrack_v3/tests/test_controller_support.cpp`
- Modify: `rk3566/nanotrack_v3/CMakeLists.txt`

**Interfaces:**
- Consumes: `BBox` 和 Task 1 `ResponseMetrics`。
- Produces: `ControllerConfig load_controller_config(path)`；`MotionEstimator::{initialize,predict,correct}`；`ImageView`、`DetectorRequest`、`DetectorBatch`、`DetectorAdapter`、`NullDetectorAdapter`；所有状态/质量/模板枚举。

- [x] **Step 1: 写失败测试**：加载完整 key=value 配置、拒绝未知/重复/非有限/越界值；验证可见光和红外文件互不覆盖。
- [x] **Step 2: 写失败测试**：常速度预测字面轨迹、弱帧不校正、重复/回退时间戳拒绝、长间隔 `dt` 上限；null detector 永远不可用且 `poll()` 为空。
- [x] **Step 3: 配置并运行 focused target**；**Expected:** 编译失败，缺少新接口。
- [x] **Step 4: 实现最小支持库**，所有时长使用 `int64_t` 毫秒，运动框最终裁剪到图像边界。
- [x] **Step 5: 新增** `rk3566/nanotrack_v3/config/visible-default.conf` 与 `infrared-default.conf`，均显式标记 `calibrated=false`。
- [x] **Step 6: 重跑 focused 与完整 CTest**；**Expected:** 全部通过。

### Task 3: 长时状态机、质量融合与模板生命周期

**Files:**
- Create: `rk3566/nanotrack_v3/include/long_term_controller.hpp`
- Create: `rk3566/nanotrack_v3/src/long_term_controller.cpp`
- Create: `rk3566/nanotrack_v3/tests/test_long_term_controller.cpp`
- Modify: `rk3566/nanotrack_v3/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 2 `ControllerConfig`、状态/observation/detector 类型和 `MotionEstimator`。
- Produces: `LongTermController::initialize(initial_bbox, frame_id, timestamp_ms, frame_size)`；`SearchPlan begin_frame(frame_id, timestamp_ms, detector_available)`；`ControllerOutput finish_frame(plan, observations, optional<DetectorBatch>)`。

- [x] **Step 1: 写失败测试**：在 199/200/999/1000/2999/3000 ms 边界验证五状态和 1.0/1.5/2.0/3.0/4.0 搜索倍率。
- [x] **Step 2: 写失败测试**：可靠连续跟踪立即有效；弱/拒绝不提交；重捕获只有最近三次中的两个一致可靠候选才恢复有效控制。
- [x] **Step 3: 写失败测试**：anchor/recent 一致融合、冲突优先 anchor、仅 recent 高分需确认、形状/运动硬门拒绝。
- [x] **Step 4: 写失败测试**：detector 500 ms 限频、150 ms 过期、frame id 错配、最多三候选以及下一帧逐一模板验证；null detector 路径持续有界搜索。
- [x] **Step 5: 写失败测试**：15 稳定帧和 1000 ms 后 stage，3 帧 probation 后 commit，期间丢失/冲突 rollback。
- [x] **Step 6: 实现纯策略核心**；任何未接受 observation 都不得调用运动校正或产生 recent commit。
- [x] **Step 7: 重跑 focused 与完整 CTest**；**Expected:** 全部通过，并完成至少 10,000 帧 LOST 合成循环。

### Task 4: NanoTrack 双模板运行时与可提交状态

**Files:**
- Modify: `rk3566/nanotrack_v3/include/nanotrack_v3.hpp`
- Modify: `rk3566/nanotrack_v3/src/nanotrack_v3.cpp`
- Create: `rk3566/nanotrack_v3/include/long_term_runtime.hpp`
- Create: `rk3566/nanotrack_v3/src/long_term_runtime.cpp`
- Modify: `rk3566/nanotrack_v3/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 3 `SearchPlan`/`ControllerOutput`，Task 1 diagnostics，Task 2 `ImageView`。
- Produces: `NanoTrackV3::infer(frame, plan) -> vector<TrackerObservation>`；`commit_bbox`；`stage_recent`；`commit_recent`；`rollback_recent`；`LongTermRuntime::process_frame`。

- [x] **Step 1: 添加 host-buildable 生命周期测试替身**：一次 plan 只能调用一次 search 操作，双模板 observation 共用同一 search token；未接受结果不调用 commit。
- [x] **Step 2: 运行 focused test**；**Expected:** 编译失败，缺少 runtime 接口。
- [x] **Step 3: 重构 NanoTrack**：anchor immutable，recent/staged/backup 三槽；搜索裁剪由 plan 预测框和倍率决定；同一 search feature 运行两次 head；`infer` 不修改 committed state。
- [x] **Step 4: 实现 runtime 调度**：按 controller 输出执行 bbox commit 和 recent stage/commit/rollback；detector 异常降级并记录原因。
- [x] **Step 5: 主机侧构建所有非 RKNN 测试**；**Expected:** 全部通过。RKNN 源码以板端构建作为编译证明，不在主机伪造。

### Task 5: 视频 CLI、完整逐帧日志和可视化

**Files:**
- Modify: `rk3566/nanotrack_v3/include/cli.hpp`
- Modify: `rk3566/nanotrack_v3/src/cli.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_cli.cpp`
- Modify: `rk3566/nanotrack_v3/src/main.cpp`
- Create: `rk3566/nanotrack_v3/include/tracking_log.hpp`
- Create: `rk3566/nanotrack_v3/src/tracking_log.cpp`
- Create: `rk3566/nanotrack_v3/tests/test_tracking_log.cpp`
- Modify: `rk3566/nanotrack_v3/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 4 `LongTermRuntime` 输出。
- Produces: 可选 `--controller-config FILE`、`--sensor visible|infrared`、`--session-metadata FILE`；扩展 CSV 和状态着色视频；严格单调 `timestamp_ms`。

- [x] **Step 1: 写失败 CLI 测试**：新增参数解析、sensor 枚举、配置存在性、旧命令仍可解析、未知参数仍拒绝。
- [x] **Step 2: 写失败日志测试**：初始化和跟踪行的列数一致；所有原始指标、状态、模板动作、detector 决策和分段耗时存在；CSV 转义正确。
- [x] **Step 3: 写失败时间测试**：合法 PTS 保留，重复/回退 PTS 用 FPS 步长修复，FPS 无效时使用 30 FPS 并记录 fallback。
- [x] **Step 4: 实现 CLI、日志和 main 集成**；LOST 仍画预测框但使用无效控制颜色，不能复用绿色可靠框。
- [x] **Step 5: 运行完整 host CTest**；**Expected:** 全部通过。
- [x] **Step 6: 更新** `rk3566/nanotrack_v3/README.md`，给出旧命令兼容示例、长时命令、CSV 字段和阈值未标定警告。

### Task 6: 主机验收与 RK3566 板端验证

**Files:**
- Create: `rk3566/nanotrack_v3/tests/test_long_term_replay.cpp`
- Create: `artifacts/rk3566_nanotrack_long_term/VERIFICATION.md`
- Create: `artifacts/rk3566_nanotrack_long_term/verification.json`

**Interfaces:**
- Consumes: Task 1–5 完整实现和现有已部署模型/板端视频。
- Produces: 主机逻辑证明、板端原生构建与视频输出、性能/恢复/稳定性边界证据。

- [x] **Step 1: 写并运行确定性 replay 测试**：正常、短遮挡、1–3 s 恢复、永久丢失、形变、干扰物和无 detector 路径，逐帧断言状态与 `control_valid`。
- [x] **Step 2: 干净配置并运行** `cmake -S rk3566/nanotrack_v3 -B /tmp/nanotrack-longterm-final -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF && cmake --build /tmp/nanotrack-longterm-final -j2 && ctest --test-dir /tmp/nanotrack-longterm-final --output-on-failure`；**Expected:** 100% 通过。
- [x] **Step 3: 静态检查** `rg` 无 TODO/TBD、无未处理默认 detector、无旧 `track()` 路径绕过控制器；**Expected:** 只有文档中明确的未来工作描述。
- [x] **Step 4: 向用户申请开发板连接**；在未获连接前保持目标 active，不声称部署完成。
- [x] **Step 5: 板端原生干净构建** `ENABLE_RKNN_RUNTIME=ON`，运行 `--help` 和短视频 smoke；**Result:** RK3566 原生构建、板端 CTest 7/7、三模型加载、schema-v2 CSV/视频校验均通过。
- [ ] **Step 6: 用现有模型执行可见光/红外视频**，记录正常段 mean/P50/P95、端到端 FPS、内存、温度、NPU/driver 版本和输出哈希。
- [ ] **Step 7: 执行带标注恢复回放和各 30 分钟稳定性**；按规格记录恢复率、时间、误捕获率、形变 AUC 和资源趋势，任何未达标项如实标为未通过。

Board status: Step 6 visible-light evidence is complete; infrared input is
missing. Step 7 visible process stability passed for 2,207 s and 7,000 frames,
but mostly exercised bounded `LOST` behavior rather than tracking continuity;
labeled recovery/deformation data and infrared stability remain unavailable. The detector-free girl sequence
demonstrates that global reacquisition does not pass without an external
candidate source.

## Completion Contract

Task 1–5 的 RED→GREEN 与完整主机 CTest 必须有当前会话证据；Task 6 必须在真实 RK3566
上完成构建、视频运行和验收记录。仅主机测试通过时实现可以交给板端验证，但整个目标仍不
能标记 complete。
