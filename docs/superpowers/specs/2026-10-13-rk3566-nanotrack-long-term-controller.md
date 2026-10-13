# RK3566 NanoTrack 长时跟踪控制器设计规格

## 1. 目标与范围

在现有 `rk3566/nanotrack_v3` 三模型 RKNN 推理链路上增加可部署的长时跟踪控制器。
首版输入为视频文件，继续使用已经在 RK3566 上验证过的 NanoTrack V3 模型；detector
只定义异步适配接口，不绑定检测模型。系统需要在短时遮挡、目标形变、快速运动和暂时
出视野后避免盲目更新，并在目标重新出现后通过模板与时空约束重新确认。

本规格不包含交叉编译、检测模型实现、云台协议接入和模型再训练。交叉编译由另一项工作
完成；本项目必须保持在开发板原生构建和运行的能力。

## 2. 平台和兼容约束

- 目标平台为 RK3566，C++17、OpenCV 和 RKNN Runtime。
- 沿用 `template_backbone.rknn`、`search_backbone.rknn`、`head.rknn` 文件名和现有
  `manifest.json`，首版不修改模型。
- 正常跟踪阶段每帧最多运行一次 search backbone；同一 search feature 可以分别运行
  anchor/recent 两次轻量 head。
- 保持现有视频、初始框、输出视频和输出 CSV 命令兼容；新增选项必须是可选项。
- 可在不安装 RKNN/OpenCV 的 Linux 主机上编译并测试策略、运动模型、配置和 CLI。
- 可见光与红外使用独立配置文件，禁止假定一套阈值可直接复用。

## 3. 总体数据流

每帧按以下顺序处理：

1. 控制器根据上一帧状态和单调时间戳预测目标框，生成一个 `SearchPlan`。
2. NanoTrack 以计划中的中心和搜索倍率裁剪一次 search patch，运行一次 search backbone。
3. 同一 search feature 分别与不可变 anchor 模板、可更新 recent 模板运行 head，输出两份
   候选及原始响应统计。
4. 控制器融合两份候选，执行质量、运动、形状、模板一致性和候选连续性门控。
5. 只有被接受的结果才能提交为跟踪状态；不可靠结果不得污染运动模型或 recent 模板。
6. 控制器输出水平框、状态、质量、`control_valid`、模板动作和下一帧恢复计划。
7. 若 detector 可用且到达调度周期，提交最多三个候选；候选只能成为下一帧的搜索假设，
   必须再次通过 NanoTrack 模板验证，不能直接作为跟踪结果。

## 4. 核心接口

### 4.1 帧和检测接口

公共接口使用不拥有内存的 `ImageView`，字段为数据指针、宽、高、行跨度和通道数。

`DetectorAdapter` 是异步接口：

- `available()` 报告是否配置了 detector；
- `submit(ImageView, DetectorRequest)` 接收帧、ROI、`frame_id`、单调时间戳和最大候选数；
- `poll()` 返回可选 `DetectorBatch`；
- `NullDetectorAdapter` 永远不可用且不产生候选，作为首版默认实现。

检测结果必须携带来源 `frame_id`、来源时间戳、类别、置信度和水平框。来源帧不匹配或在
消费时已超过 150 ms 的结果直接丢弃。每批最多保留三个候选。

### 4.2 NanoTrack 接口

`NanoTrackV3` 提供：

- 初始化 anchor/recent 模板和当前框；
- 按 `SearchPlan` 运行一次 search backbone，并返回 anchor/recent 两份 observation；
- 只在控制器接受后提交框；
- stage/commit/rollback recent 模板；
- anchor 模板初始化后不可修改。

每份 observation 至少包含 raw score、penalized peak、PSR、APCE、peak margin、水平框、
模板类型、`frame_id` 和时间戳。所有统计输出原值，不在推理器中做最终可靠性判断。

## 5. 状态机与恢复范围

状态时间由首次不可靠帧的单调时间戳计算，不按固定帧数近似：

| 状态 | 丢失时长 | 搜索策略 | detector 策略 |
|---|---:|---|---|
| `TRACKING` | 0 | 预测中心，1.0 倍搜索 | 不调用 |
| `UNCERTAIN` | 0–0.2 s | 预测中心，1.5 倍搜索 | 不调用 |
| `LOCAL_RECOVERY` | 0.2–1.0 s | 2.0/3.0 倍区域搜索，每帧一次 backbone | 不调用 |
| `ASSISTED_RECOVERY` | 1.0–3.0 s | 3.0/4.0 倍区域或 detector 假设 | 可用时每 500 ms 最多一次 |
| `LOST` | 超过 3.0 s | 4.0 倍区域持续搜索，不输出有效控制 | 可用时继续限频调用 |

只要恢复候选未完成确认，`control_valid` 必须为 false。恢复需要最近三个时刻内至少两个
互相一致的可靠候选；一致性由候选 IoU、中心距离和尺度变化共同判断。确认后回到
`TRACKING`，重置丢失计时，但不得重置 immutable anchor。

无 detector 时，`ASSISTED_RECOVERY` 和 `LOST` 继续以模板驱动的区域搜索工作；系统不得
因为 detector 不可用而报错或停止读视频。

## 6. 质量融合与门控

控制器使用以下独立证据，不以 NanoTrack 单一 score 判断目标是否存在：

- raw score、PSR、APCE 和 peak margin；
- anchor/recent 候选的 IoU 和中心差；
- 预测框与候选框的归一化中心距离；
- 宽高比和面积的对数变化；
- detector 候选来源新鲜度（若启用）；
- 可选云台角速度或平台运动元数据（首版字段预留，未提供时为无效）。

输出分为 `RELIABLE`、`WEAK`、`REJECTED`。暂定默认值只用于打通工程，不构成产品阈值：

- reliable：score 0.80、PSR 8.0、peak margin 0.10；
- weak：score 0.55、PSR 5.0、peak margin 0.04；
- 模板冲突：anchor/recent IoU 低于 0.30；
- 最大归一化中心残差 1.5，最大面积对数变化 1.0，最大宽高比对数变化 0.7。

阈值全部从配置加载并写入日志；正式值必须按
`docs/superpowers/specs/2026-10-13-nanotrack-threshold-calibration-data-requirements.md`
标定。anchor 与 recent 冲突时优先 anchor；仅 recent 可靠而 anchor 明确拒绝时，候选不得
单帧恢复，仍需两帧确认。

## 7. 运动模型

首版使用常速度 alpha-beta 模型预测中心与框尺寸，输入为单调毫秒时间戳。只用
`RELIABLE` 或完成重捕获确认的框校正；`WEAK`/`REJECTED` 只做预测。时间戳回退、重复或
非有限输入必须被拒绝。恢复搜索范围要包含运动预测，但不得无限扩大。

## 8. 双模板生命周期

- anchor：初始化帧提取，整个跟踪会话不可变。
- recent：初始化时等于 anchor；连续至少 15 个可靠稳定帧后才允许 stage 新模板。
- 两次 stage 间隔至少 1000 ms。
- staged recent 进入 3 个可靠帧 probation；期间发生拒绝、模板强冲突或状态离开
  `TRACKING` 时 rollback，否则 commit。
- 模板更新动作由策略产生，特征提取和交换由 NanoTrack 运行时执行。

## 9. 输出和可观测性

逐帧 CSV 至少记录：frame id、单调/视频时间戳、状态、quality、`control_valid`、输出框、
anchor/recent 原始指标、模板一致性、运动残差、搜索倍率、假设来源、模板动作、detector
请求/丢弃原因、NanoTrack 与整帧耗时。初始化帧也必须有一行明确记录。

视频叠加框按状态区分颜色，并显示状态、质量和 `control_valid`。模型 manifest 哈希、配置
文件哈希、RKNN API/driver 版本和启动参数写入会话头或旁路元数据。

## 10. 异常处理

- 视频时间戳缺失时按源 FPS 生成单调时间；源 FPS 无效时使用 30 FPS，并记录 fallback。
- 输入帧尺寸变化、非 BGR8、无效初始框、非单调时间、模型输出非有限值均立即失败并给出
  可定位错误。
- detector 超时或异常不得阻断 NanoTrack 路径；适配器错误要记录并降级为不可用。
- LOST 状态继续输出最后预测框供观测，但 `control_valid=false`，调用方不得据此驱动云台。

## 11. 验收边界

主机测试必须覆盖状态边界、2/3 确认、过期 detector 丢弃、模板 probation/rollback、无
detector 降级、时间戳异常、配置解析和响应统计。板端才可证明 RKNN/OpenCV 集成与性能：

- 正常阶段平均不低于 25 FPS，P95 单帧不高于 50 ms；
- 恢复成功率不低于 85%，恢复中位数不高于 0.5 s、P95 不高于 1 s；
- 错误重捕获不高于 3%；
- 目标形变子集 AUC 相对当前单模板基线提高至少 0.03；
- 可见光、红外分别连续运行 30 分钟，无崩溃、无持续内存增长和无 NPU 错误。

没有开发板实测数据时，只能声称主机逻辑与构建接口通过，不能声称 RK3566 部署完成。
