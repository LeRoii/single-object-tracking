# RK3566 Target Class Establishment Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在每次人工初始化或重新框选时自动建立目标类别；无法唯一识别类别时关闭 detector 辅助恢复，同时保留 NanoTrack 跟踪、全类别检测诊断和既有安全链。

**Architecture:** 新增纯 C++ 目标类别匹配单元，LongTermRuntime 通过扩展后的单 worker detector adapter 完成一次阻塞式全类别推理，再把不可变的 `TargetClassState` 交给 controller。Controller 在既有时效、generation、HOLD 检查之后执行类别门控，只有同类别候选能进入位置门控和 NanoTrack 2/3 验证；显示、日志和 metadata 消费同一个状态对象。

**Tech Stack:** C++17、CMake/CTest、OpenCV highgui（板端交互程序）、RKNN Runtime（RK3566 板端）、现有 key=value runtime config、JSON/CSV 字符串序列化。

**Spec:** `docs/superpowers/specs/2026-10-29-target-class-establishment-design.md`

## Global Constraints

- 自动模式 `detector_class_id=-1`；固定模式 `detector_class_id>=0`，越界是启动错误。
- 默认匹配参数：`min_roi_coverage=0.50`、`max_center_ratio=0.60`、`ambiguity_margin=0.10`，均标记为未标定。
- 初始化 detector 候选只建立类别，不替换人工 ROI、不进入恢复队列、不调用 tracker commit。
- 类别未知只关闭 detector 恢复；Debug detector 仍按既有节流运行并显示 NMS 后的全部类别框。
- detector 框不能直接产生 `control_valid=true`；只允许 NanoTrack anchor/recent 验证和既有 2/3 确认链恢复控制。
- adapter `request_id` 与 controller `detector_eligibility_generation` 独立且均不得复用。
- 不提交模型、视频、日志和构建产物；只记录路径、SHA-256、命令和观察结果。
- 只修改任务拥有的 `docs/progress/board-validation.md`，不修改 `STATUS.md` 或 `integration-contract.yaml`。
- Host 构建或 CTest 通过不是板端验收；没有人工反馈不能声称真实吊舱验收。

## Review Focus

- 同一帧快速连续重新框选：旧 pending/in-flight/completed 结果不能污染新初始化；Task 2 和 Task 4 必须覆盖。
- ROI 是车辆/人体任意局部区域且存在重叠异类框：唯一类别正确建立，否则安全地返回 AMBIGUOUS；Task 1 必须覆盖。
- detector 推理异常或未配置：NanoTrack 仍以人工 ROI 初始化，类别 UNKNOWN 或 FIXED，且不会产生 detector probe；Task 4 必须覆盖。
- UNKNOWN、CLASS_MISMATCH 与 invalid/stale/HOLD 同时出现：结构性拒绝原因不能被类别原因覆盖；Task 3 必须覆盖。
- 重新框选发生在相同 `frame_id`：CSV 不重复写帧，metadata history 和结构化事件仍保留每次初始化；Task 7 和 Task 8 必须覆盖。

---

## File Structure

- `include/long_term_types.hpp`, `src/long_term_types.cpp`: 目标类别状态、候选匹配诊断、候选类别 disposition、最终拒绝原因及字符串转换。
- `include/target_class.hpp`, `src/target_class.cpp`: 纯几何匹配、固定类别和 UNKNOWN 状态构造函数；该头文件消费而不反向定义 long-term 公共类型，避免循环包含。
- `include/detector_adapter.hpp`, `include/yolo_detector_adapter.hpp`, `src/yolo_detector_adapter.cpp`: 阻塞初始化检测、adapter request ID、worker quiesce 和异常禁用。
- `include/long_term_controller.hpp`, `src/long_term_controller.cpp`: 保存目标类别状态并在恢复候选入队前执行类别门控。
- `include/long_term_runtime.hpp`, `src/long_term_runtime.cpp`: 初始化编排、固定/自动/UNKNOWN 路径及初始化历史结果。
- `include/cli.hpp`, `src/cli.cpp`, `src/runtime_config.cpp`, `config/interactive-debug.conf`: 新参数解析、范围校验和 Debug 默认自动类别。
- `include/interactive_debug_overlay.hpp`, `src/interactive_debug_overlay.cpp`, `src/debug_telemetry.cpp`: 类别状态、候选框颜色和初始化事件。
- `include/tracking_log.hpp`, `src/tracking_log.cpp`: CSV schema 8 和初始化历史 metadata。
- `src/main.cpp`: labels/class_count 校验、全类别 engine、交互初始化呈现和计数。
- `RK3566_NANOTRACK_LONGTERM_CODE_FLOW.zh-CN.md`: 运行流程和 Debug 字段说明。
- `docs/progress/board-validation.md`: 仅记录实际板端证据、边界和 handoff 请求。

### Task 1: 纯目标类别匹配器

**Files:**
- Create: `rk3566/nanotrack_v3/include/target_class.hpp`
- Create: `rk3566/nanotrack_v3/src/target_class.cpp`
- Create: `rk3566/nanotrack_v3/tests/test_target_class.cpp`
- Modify: `rk3566/nanotrack_v3/include/long_term_types.hpp`
- Modify: `rk3566/nanotrack_v3/src/long_term_types.cpp`
- Modify: `rk3566/nanotrack_v3/CMakeLists.txt`

**Interfaces:**
- Consumes: `BBox`, `FrameSize`, `DetectorCandidate` from `long_term_types.hpp`.
- Produces: `TargetClassStatus`, `TargetClassReason`, `TargetClassMatchConfig`, `TargetClassCandidateMatch`, and `TargetClassState` in `long_term_types.hpp`; `establish_target_class(...)`, `make_fixed_target_class(...)`, and `make_unknown_target_class(...)` in `target_class.hpp`.

- [ ] **Step 1: Write matcher tests that fail before the API exists**

Add named cases for full-object ROI, arbitrary local ROI, no match, same-class duplicates, clear cross-class winner, cross-class ambiguity, clipped boxes, invalid/NaN candidates, deterministic ties, and equality at all three thresholds. Assert exact status, reason, selected class/index, candidate eligibility and scores within epsilon.

- [ ] **Step 2: Run the focused test and verify RED**

Run: `cmake -S rk3566/nanotrack_v3 -B /tmp/nanotrack-target-class-build -DENABLE_RKNN_RUNTIME=OFF && cmake --build /tmp/nanotrack-target-class-build --target test_target_class -j2`

Expected: configure/build fails because `target_class.hpp` or the target does not exist.

- [ ] **Step 3: Add the exact public data model and matcher signatures**

Use:

```cpp
struct TargetClassMatchConfig {
  float min_roi_coverage = 0.50F;
  float max_center_ratio = 0.60F;
  float ambiguity_margin = 0.10F;
};

TargetClassState establish_target_class(
    const BBox& roi, FrameSize frame_size,
    const std::vector<DetectorCandidate>& candidates,
    std::size_t class_count, const TargetClassMatchConfig& config);
TargetClassState make_fixed_target_class(int class_id, bool detector_available);
TargetClassState make_unknown_target_class(TargetClassReason reason);
```

`TargetClassCandidateMatch` carries original index/candidate, clipped bbox, coverage, IoU, center ratio, score, eligibility and rejection reason. `TargetClassState` carries status/reason/optional class ID, `target_class_allows_recovery`, best/runner-up class and score, optional selected candidate, and every candidate diagnostic.

- [ ] **Step 4: Implement the approved formula and deterministic tie rules**

Clip before area calculation; compute `0.55 coverage + 0.20 center_term + 0.10 IoU + 0.15 confidence`; reduce to one winner per class; use confidence, area and input index for same-class ties; use class ID for cross-class ordering; apply one shared epsilon.

- [ ] **Step 5: Run the focused test and verify GREEN**

Run: `cmake --build /tmp/nanotrack-target-class-build --target test_target_class -j2 && ctest --test-dir /tmp/nanotrack-target-class-build -R '^target_class$' --output-on-failure`

Expected: `100% tests passed, 0 tests failed`.

- [ ] **Step 6: Commit the matcher**

```bash
git add rk3566/nanotrack_v3/CMakeLists.txt rk3566/nanotrack_v3/include/long_term_types.hpp rk3566/nanotrack_v3/src/long_term_types.cpp rk3566/nanotrack_v3/include/target_class.hpp rk3566/nanotrack_v3/src/target_class.cpp rk3566/nanotrack_v3/tests/test_target_class.cpp
git commit -m "feat(rk3566): add target class matcher"
```

### Task 2: 单 worker 阻塞初始化检测协议

**Files:**
- Modify: `rk3566/nanotrack_v3/include/detector_adapter.hpp`
- Modify: `rk3566/nanotrack_v3/include/yolo_detector_adapter.hpp`
- Modify: `rk3566/nanotrack_v3/src/yolo_detector_adapter.cpp`
- Modify: `rk3566/nanotrack_v3/include/long_term_types.hpp`
- Modify: `rk3566/nanotrack_v3/tests/test_yolo_detector_adapter.cpp`

**Interfaces:**
- Consumes: existing `DetectorRequest` and `DetectorBatch`.
- Produces: `DetectorBlockingResult { std::uint64_t request_id; DetectorBatch batch; }`, `DetectorAdapter::detect_blocking(const ImageView&, const DetectorRequest&)`, and adapter-assigned `DetectorBatch::request_id`.

- [ ] **Step 1: Add failing worker-protocol tests**

Test exact blocking request/result identity, monotonically increasing IDs across async and blocking work, pending cancellation, waiting for an in-flight request without deadlock, completed-result discard, `poll()` not consuming initialization results, reinitialization on the same frame ID, destructor wakeup, and engine exception disabling subsequent submit/blocking calls.

- [ ] **Step 2: Run the focused test and verify RED**

Run: `cmake --build /tmp/nanotrack-target-class-build --target test_yolo_detector_adapter -j2 && ctest --test-dir /tmp/nanotrack-target-class-build -R '^yolo_detector_adapter$' --output-on-failure`

Expected: compile failures for missing blocking API/request ID.

- [ ] **Step 3: Extend the adapter state machine**

Add internal `RequestKind { kAsync, kInitialization }`, an adapter-owned `next_request_id_`, request identity on pending/completed/error state, and an `initialization_exclusive_` flag. `NullDetectorAdapter::detect_blocking()` returns `std::nullopt`.

- [ ] **Step 4: Implement quiesce and exact-result consumption**

Cancel unclaimed pending work, wait and discard old in-flight/completed work, submit the initialization request, and wait only for its ID/kind. Every pending→in-flight, success, error, stop and consume transition calls `notify_all()`; all waits release the mutex and `engine_->infer()` never holds it.

- [ ] **Step 5: Verify focused and repeated concurrency tests**

Run: `cmake --build /tmp/nanotrack-target-class-build --target test_yolo_detector_adapter -j2 && for i in 1 2 3 4 5; do ctest --test-dir /tmp/nanotrack-target-class-build -R '^yolo_detector_adapter$' --output-on-failure || exit 1; done`

Expected: five consecutive passes with no hang.

- [ ] **Step 6: Commit the adapter protocol**

```bash
git add rk3566/nanotrack_v3/include/detector_adapter.hpp rk3566/nanotrack_v3/include/yolo_detector_adapter.hpp rk3566/nanotrack_v3/include/long_term_types.hpp rk3566/nanotrack_v3/src/yolo_detector_adapter.cpp rk3566/nanotrack_v3/tests/test_yolo_detector_adapter.cpp
git commit -m "feat(rk3566): add blocking detector initialization"
```

### Task 3: Controller 类别门控和正交诊断

**Files:**
- Modify: `rk3566/nanotrack_v3/include/long_term_types.hpp`
- Modify: `rk3566/nanotrack_v3/src/long_term_types.cpp`
- Modify: `rk3566/nanotrack_v3/include/long_term_controller.hpp`
- Modify: `rk3566/nanotrack_v3/src/long_term_controller.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_long_term_controller.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_long_term_replay.cpp`

**Interfaces:**
- Consumes: `TargetClassState` from Task 1.
- Produces: `DetectorClassDisposition`, expanded `DetectorCandidateRejection`, `LongTermController::initialize(..., const TargetClassState&)`, and `ControllerOutput::target_class`.

- [ ] **Step 1: Add failing class-gate and safety tests**

Cover matching class queue, mismatch no queue, UNKNOWN no queue, fixed class, invalid class, batch accepted with zero queue, target class reset on reinitialize, eligibility generation advance on same-frame reinitialize, and no detector probe/control transition from merely receiving candidates.

- [ ] **Step 2: Add rejection-precedence tests**

Assert priority `INVALID → GENERATION_INVALIDATED → FRAME_MISMATCH → STALE → HOLD_INVALIDATED → TARGET_CLASS_UNKNOWN/CLASS_MISMATCH → POSITION_REJECTED → CAPACITY_REJECTED`, while `class_disposition` remains independently inspectable.

- [ ] **Step 3: Run controller tests and verify RED**

Run: `cmake --build /tmp/nanotrack-target-class-build --target test_long_term_controller test_long_term_replay -j2 && ctest --test-dir /tmp/nanotrack-target-class-build -R '^(long_term_controller|long_term_replay)$' --output-on-failure`

Expected: compile/assertion failures for missing target-class state and dispositions.

- [ ] **Step 4: Implement controller state and gate**

Change initialize to accept and copy `const TargetClassState&`; reset candidates/confirmations and increment eligibility generation. Update every existing controller/replay initialization fixture explicitly: detector-recovery fixtures pass their intended fixed class, while tests unrelated to detector recovery pass UNKNOWN. Populate candidate diagnostics first, preserve structural rejection priority, then apply target unknown/mismatch before position/capacity gates.

- [ ] **Step 5: Verify controller and replay suites GREEN**

Run the Step 3 command again.

Expected: both tests pass, including existing HOLD and 2/3 recovery cases.

- [ ] **Step 6: Commit controller gating**

```bash
git add rk3566/nanotrack_v3/include/long_term_types.hpp rk3566/nanotrack_v3/src/long_term_types.cpp rk3566/nanotrack_v3/include/long_term_controller.hpp rk3566/nanotrack_v3/src/long_term_controller.cpp rk3566/nanotrack_v3/tests/test_long_term_controller.cpp rk3566/nanotrack_v3/tests/test_long_term_replay.cpp
git commit -m "feat(rk3566): gate recovery by target class"
```

### Task 4: Runtime 初始化编排和重新框选隔离

**Files:**
- Modify: `rk3566/nanotrack_v3/include/long_term_runtime.hpp`
- Modify: `rk3566/nanotrack_v3/src/long_term_runtime.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_long_term_runtime.cpp`

**Interfaces:**
- Consumes: blocking adapter API, matcher API, controller target-class initialize.
- Produces: `TargetClassRuntimeConfig`, `TargetClassInitializationResult`, constructor parameter `const TargetClassRuntimeConfig&`, `LongTermRuntime::initialize(...) -> TargetClassInitializationResult`, and `RuntimeFrameResult::target_class` plus optional initialization diagnostics for the paused initialization frame.

- [ ] **Step 1: Add failing initialization-path tests**

Use fake tracker/detector to cover automatic unique match, ambiguous/no-match UNKNOWN, detector unavailable, detector exception, fixed override without blocking call, same-frame reselect, old async cleanup, and initialize while a frame is open. Assert tracker/controller always receive the original manual ROI and no initialization candidate gets a recovery ID or tracker commit.

- [ ] **Step 2: Run runtime tests and verify RED**

Run: `cmake --build /tmp/nanotrack-target-class-build --target test_long_term_runtime -j2 && ctest --test-dir /tmp/nanotrack-target-class-build -R '^long_term_runtime$' --output-on-failure`

Expected: compile failures for new constructor/return type.

- [ ] **Step 3: Implement runtime configuration and result types**

`TargetClassRuntimeConfig` contains requested class ID, class count and `TargetClassMatchConfig`. `TargetClassInitializationResult` contains initialization index, optional request ID/batch, `TargetClassState`, detector timing/error and the original ROI. Runtime exposes `const TargetClassInitializationResult& last_initialization() const`; every `RuntimeFrameResult` copies the current `TargetClassState`.

- [ ] **Step 4: Implement initialization ordering**

Validate no open frame and validate ROI; fixed mode builds fixed state without detector inference; auto mode calls `detect_blocking`, catches engine failure as UNKNOWN/DETECTOR_ERROR, matches candidates, initializes controller with the state, then initializes tracker/motion using the unchanged ROI. Never pass initialization batch to `finish_frame()`.

- [ ] **Step 5: Verify runtime tests GREEN**

Run the Step 2 command again.

Expected: all runtime tests pass, including existing HOLD reinitialization.

- [ ] **Step 6: Commit runtime orchestration**

```bash
git add rk3566/nanotrack_v3/include/long_term_runtime.hpp rk3566/nanotrack_v3/src/long_term_runtime.cpp rk3566/nanotrack_v3/tests/test_long_term_runtime.cpp
git commit -m "feat(rk3566): establish class during initialization"
```

### Task 5: CLI、配置与全类别引擎兼容

**Files:**
- Modify: `rk3566/nanotrack_v3/include/cli.hpp`
- Modify: `rk3566/nanotrack_v3/src/cli.cpp`
- Modify: `rk3566/nanotrack_v3/src/runtime_config.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_cli.cpp`
- Modify: `rk3566/nanotrack_v3/config/interactive-debug.conf`

**Interfaces:**
- Consumes: `TargetClassMatchConfig` defaults from Task 1.
- Produces: `Options::target_class_min_roi_coverage`, `target_class_max_center_ratio`, `target_class_ambiguity_margin`, corresponding CLI/config keys, and Debug default `detector_class_id=-1`.

- [ ] **Step 1: Add failing parser/config tests**

Assert defaults, command-line override, config-file mapping, CLI-over-config precedence, accepted boundary values and rejected NaN/out-of-range values. Update shipped-config expectation from class 0 to -1.

- [ ] **Step 2: Run CLI tests and verify RED**

Run: `cmake --build /tmp/nanotrack-target-class-build --target test_cli -j2 && ctest --test-dir /tmp/nanotrack-target-class-build -R '^cli$' --output-on-failure`

Expected: failures for unknown options/config keys and old shipped value.

- [ ] **Step 3: Implement exact options and validators**

Add `--target-class-min-roi-coverage`, `--target-class-max-center-ratio`, and `--target-class-ambiguity-margin`; validate `(0,1]`, `(0,2]`, `[0,1]`; update usage and key=value mappings.

- [ ] **Step 4: Change the shipped Debug profile to auto mode**

Set `detector_class_id=-1` and write all three uncalibrated defaults explicitly in `interactive-debug.conf`.

- [ ] **Step 5: Verify CLI tests GREEN**

Run the Step 2 command again.

- [ ] **Step 6: Commit configuration support**

```bash
git add rk3566/nanotrack_v3/include/cli.hpp rk3566/nanotrack_v3/src/cli.cpp rk3566/nanotrack_v3/src/runtime_config.cpp rk3566/nanotrack_v3/tests/test_cli.cpp rk3566/nanotrack_v3/config/interactive-debug.conf
git commit -m "feat(rk3566): configure automatic target class"
```

### Task 6: Debug overlay 和结构化类别事件

**Files:**
- Modify: `rk3566/nanotrack_v3/include/interactive_debug_overlay.hpp`
- Modify: `rk3566/nanotrack_v3/src/interactive_debug_overlay.cpp`
- Modify: `rk3566/nanotrack_v3/include/debug_telemetry.hpp`
- Modify: `rk3566/nanotrack_v3/src/debug_telemetry.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_interactive_debug_overlay.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_debug_telemetry.cpp`

**Interfaces:**
- Consumes: runtime/controller `TargetClassState`, initialization result and candidate class dispositions.
- Produces: initialization-selected box kind, gray mismatch/unknown boxes, target-class status text, and `target_class_init_start/candidate/result` events through `DebugTelemetry::initialization_events(const TargetClassInitializationResult&, const std::vector<std::string>& labels)`.

- [ ] **Step 1: Add failing overlay tests**

Assert `TARGET CLS car(2) AUTO`, `TARGET CLS UNKNOWN - DET RECOVERY OFF`, labels fallback to `cls=<id>`, selected initialization candidate color/kind, gray mismatch/unknown candidates, and unchanged accepted-control rendering.

- [ ] **Step 2: Add failing telemetry tests**

Assert exact fields for request ID, initialization index, candidate count, best/runner-up class and score, class disposition and final rejection. Ensure initialization events do not appear as frame detector recovery events.

- [ ] **Step 3: Run focused tests and verify RED**

Run: `cmake --build /tmp/nanotrack-target-class-build --target test_interactive_debug_overlay test_debug_telemetry -j2 && ctest --test-dir /tmp/nanotrack-target-class-build -R '^(interactive_debug_overlay|debug_telemetry)$' --output-on-failure`

Expected: missing enum/text/event assertions fail.

- [ ] **Step 4: Implement presentation from shared state**

Do not infer class from current detector candidates. Change `make_interactive_debug_overlay` to accept `const std::vector<std::string>& labels`; render only values carried by runtime/controller, with `cls=<id>` fallback. The initialization frame is a normal `RuntimeFrameResult` carrying optional initialization diagnostics, so it uses the same overlay entry point.

- [ ] **Step 5: Verify overlay and telemetry GREEN**

Run the Step 3 command again.

- [ ] **Step 6: Commit Debug presentation**

```bash
git add rk3566/nanotrack_v3/include/interactive_debug_overlay.hpp rk3566/nanotrack_v3/src/interactive_debug_overlay.cpp rk3566/nanotrack_v3/include/debug_telemetry.hpp rk3566/nanotrack_v3/src/debug_telemetry.cpp rk3566/nanotrack_v3/tests/test_interactive_debug_overlay.cpp rk3566/nanotrack_v3/tests/test_debug_telemetry.cpp
git commit -m "feat(rk3566): show target class diagnostics"
```

### Task 7: CSV schema 8 和初始化历史 metadata

**Files:**
- Modify: `rk3566/nanotrack_v3/include/tracking_log.hpp`
- Modify: `rk3566/nanotrack_v3/src/tracking_log.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_tracking_log.cpp`
- Modify: `rk3566/nanotrack_v3/tests/test_verify_board_schema.sh`

**Interfaces:**
- Consumes: `TargetClassState`, match thresholds and initialization results.
- Produces: `kTrackingSchemaVersion=8`, four per-frame target-class fields, requested mode/ID and engine filter metadata, separate initialization detector counters, and ordered `target_class_initializations` history.

- [ ] **Step 1: Add failing schema and metadata tests**

Require `target_class_status`, `target_class_id`, `target_class_reason`, `detector_recovery_enabled` in initial/tracking CSV rows; require schema 8. Metadata must preserve two same-frame initialization records with distinct index/request ID and must keep initialization detector latency/counters separate from frame latency/counters.

- [ ] **Step 2: Run logging/schema tests and verify RED**

Run: `cmake --build /tmp/nanotrack-target-class-build --target test_tracking_log -j2 && ctest --test-dir /tmp/nanotrack-target-class-build -R '^(tracking_log|verify_board_schema)$' --output-on-failure`

Expected: schema 7 and missing target fields fail.

- [ ] **Step 3: Implement schema 8 serialization**

Keep one decoded frame per CSV row; do not emit a duplicate frame row on reselection. Store all reselections in metadata history and structured events. Serialize optional class IDs/scores as `null` in JSON and empty fields in CSV.

- [ ] **Step 4: Verify logging/schema tests GREEN**

Run the Step 2 command again.

- [ ] **Step 5: Commit schema 8**

```bash
git add rk3566/nanotrack_v3/include/tracking_log.hpp rk3566/nanotrack_v3/src/tracking_log.cpp rk3566/nanotrack_v3/tests/test_tracking_log.cpp rk3566/nanotrack_v3/tests/test_verify_board_schema.sh
git commit -m "feat(rk3566): record target class schema 8"
```

### Task 8: 主程序集成、文档与 Host 全回归

**Files:**
- Modify: `rk3566/nanotrack_v3/src/main.cpp`
- Modify: `rk3566/nanotrack_v3/include/yolo_rknn_engine.hpp` only if a class-count accessor is needed
- Modify: `rk3566/nanotrack_v3/src/yolo_rknn_engine.cpp` only if a class-count accessor is needed
- Modify: `RK3566_NANOTRACK_LONGTERM_CODE_FLOW.zh-CN.md`
- Test: all files under `rk3566/nanotrack_v3/tests/`

**Interfaces:**
- Consumes: all Tasks 1–7 interfaces.
- Produces: interactive initialization behavior, validated fixed class, always-all-class RKNN engine, initialization event output/history, and updated operator documentation.

- [ ] **Step 1: Add or extend host-testable integration assertions**

Where logic can be separated from RKNN/OpenCV main, add tests that fixed class ID must be `< labels.size()`, engine filter is always `-1`, UNKNOWN continues diagnostic detector scheduling but cannot create a probe, and same-frame reselection produces two history records without duplicate CSV frames.

- [ ] **Step 2: Run affected tests and verify RED**

Run: `ctest --test-dir /tmp/nanotrack-target-class-build --output-on-failure`

Expected: newly added integration assertions fail before main wiring/helpers exist.

- [ ] **Step 3: Wire main initialization and detector identity**

Create `YoloRknnEngineConfig.class_id=-1`; obtain verified labels/class count; reject fixed ID outside the labels; construct runtime target-class config; display `ESTABLISHING TARGET CLASS...` before blocking inference; keep playback state unchanged; pass the returned initialization result to overlay, events and metadata. Count initialization detector requests/completions/errors separately.

- [ ] **Step 4: Preserve the control and ROI safety boundary**

Initial `RuntimeFrameResult` uses the returned target state but its bbox remains the manual ROI. Do not assign candidate ID, call `commit_bbox()`, send the initialization batch to `finish_frame()`, or set post-loss control validity from detector output.

- [ ] **Step 5: Update the Chinese flow/debug documentation**

Document initialization sequence, matching formula, uncalibrated thresholds, FIXED/ESTABLISHED/UNKNOWN, request ID versus eligibility generation, class disposition/final rejection, schema 8 fields, initialization events and the detector-control safety boundary.

- [ ] **Step 6: Run clean Host build and full CTest**

Run: `rm -rf /tmp/nanotrack-target-class-final-build`

Run: `cmake -S rk3566/nanotrack_v3 -B /tmp/nanotrack-target-class-final-build -DENABLE_RKNN_RUNTIME=OFF`

Run: `cmake --build /tmp/nanotrack-target-class-final-build -j2`

Run: `ctest --test-dir /tmp/nanotrack-target-class-final-build --output-on-failure`

Expected: all configured tests pass; record exact count and elapsed time, without claiming board acceptance.

- [ ] **Step 7: Inspect safety-sensitive diff and commit**

Run: `git diff --check && git status --short`

Inspect: `git diff 93be0fb1a8af151b6c20ca9a318c3bee40a9ad99 -- rk3566/nanotrack_v3/src/main.cpp rk3566/nanotrack_v3/src/long_term_controller.cpp rk3566/nanotrack_v3/src/long_term_runtime.cpp`

```bash
git add rk3566/nanotrack_v3/src/main.cpp rk3566/nanotrack_v3/include/yolo_rknn_engine.hpp rk3566/nanotrack_v3/src/yolo_rknn_engine.cpp RK3566_NANOTRACK_LONGTERM_CODE_FLOW.zh-CN.md
git commit -m "feat(rk3566): integrate target class initialization"
```

Stage only files actually changed; do not add optional engine files if no accessor change was needed.

### Task 9: RK3566 原生构建与板端验证

**Files:**
- Modify: `docs/progress/board-validation.md`
- Do not commit: board build directory, binary, models, videos, logs.

**Interfaces:**
- Consumes: committed source HEAD from Task 8 and existing board-native RKNN/OpenCV environment.
- Produces: reproducible board evidence and a coordinator handoff request; does not alter the public integration contract.

- [ ] **Step 1: Record the exact pre-test identities**

On the board record source commit, executable SHA-256, NanoTrack model/manifest SHA-256, detector model/manifest/labels SHA-256, OS/kernel, RKNN API/driver, OpenCV version, input video path/hash and exact config path/hash.

- [ ] **Step 2: Perform a clean native board build and CTest**

Use a new board build directory with `ENABLE_RKNN_RUNTIME=ON` and the board's actual RKNN/OpenCV paths. Run all configured CTest tests and retain the command/output path.

Expected: build succeeds and all board CTest tests pass. If not, stop scenario claims and diagnose before continuing.

- [ ] **Step 3: Run interactive initialization scenarios**

Validate on `girl_dance`: person full/local ROI establishes person; background/no-match ROI becomes UNKNOWN with `DET RECOVERY OFF`; reselect in one process does not inherit class/batches. If a lawful vehicle sample is available, validate full-car and local-car ROI; otherwise record vehicle validation as unobserved, not passed.

- [ ] **Step 4: Check safety and diagnostic evidence**

Retain logs/screenshots showing all detector boxes, mismatch/unknown labels, distinct request IDs, new eligibility generation, no old-result contamination, and no detector-only transition to `control_valid=true`.

- [ ] **Step 5: Update only the owned progress file**

Record ISO 8601 timestamp with timezone, branch/worktree/base/result commit, all identities/hashes, exact commands, observed results, evidence paths, blockers, evidence boundary, next action and coordinator handoff request. Keep `latest_handoff.available=false` until the coordinator explicitly adopts the commit.

- [ ] **Step 6: Commit board evidence with no large files**

```bash
git add docs/progress/board-validation.md
git commit -m "docs(rk3566): record target class board validation"
```

- [ ] **Step 7: Final branch review**

Run: `git status --short && git log --oneline 93be0fb1a8af151b6c20ca9a318c3bee40a9ad99..HEAD`

Run: `git diff --check 93be0fb1a8af151b6c20ca9a318c3bee40a9ad99..HEAD`

Expected: clean worktree, no large/generated artifacts, and a commit chain containing design, code, tests, docs and board evidence only.
