# RK3566 NanoTrack V3 长时目标跟踪工程代码流程

本文用于快速理解 `rk3566/nanotrack_v3` 原始算法工程，以及该算法移植到 ViewLink 工程 `ai_alg` 后的实际运行流程。文档基于 2026-10-16 的源码状态编写。

## 1. 先看结论：整个系统做了什么

系统不是“每帧只跑一次 NanoTrack”这么简单，而是由四层组成：

1. **宿主接口适配层**接收 ViewLink 主程序给出的 RGB 帧和控制命令，负责参数校验、RGB 转 BGR、任务启动/取消、结果回填。
2. **NanoTrack V3 短时跟踪器**使用三个 RKNN 模型，根据模板和当前搜索区域产生候选框及响应质量指标。
3. **长时控制器**根据响应质量、运动预测、模板一致性和时间，决定当前处于正常跟踪、不确定、局部恢复、检测辅助恢复还是丢失状态。
4. **YOLOv8 异步检测器**只在长时间跟踪不可靠时提供候选区域；候选框不能直接成为最终跟踪结果，必须再经过 NanoTrack 模板验证和多帧确认。

核心原则是：

> NanoTrack 决定“这个候选是否仍像最初目标”，YOLO 只负责在大范围内提出“目标可能在哪里”。

最终部署版没有鼠标框选、键盘逐帧、OpenCV HighGUI、`DISPLAY` 或视频文件播放逻辑。初始框由宿主通过 `STATE_INIT_TRACK` 传入。

## 2. 两套目录的关系

### 2.1 Linux 原始算法工程

当前文档所在工程中的算法目录：

```text
/home/u20/single-object-tracking/rk3566/nanotrack_v3
```

该目录包含完整的独立工程能力：

- NanoTrack、长时控制器、YOLO 检测器源码；
- 独立程序的 `main.cpp` 和命令行解析；
- 模型转换、训练、部署和验收脚本；
- 单元测试及板端验证文档；
- 独立程序的视频输入、CSV、视频和元数据输出逻辑。

### 2.2 WSL 中的 ViewLink 部署工程

移植目标目录：

```text
/home/wjm/viewpro_gimbal_zp/rk3566/gimbal_tracker
```

实际集成位置：

```text
ai_alg/
├── CMakeLists.txt
├── include/native-lib.h
├── source/
│   ├── native-lib_impl.cpp       # 新的真实接口实现
│   └── native-lib_stub.cpp       # 原空实现，保留但不再参与编译
└── nanotrack/
    ├── include/                  # 算法头文件
    ├── source/                   # NanoTrack、控制器和 YOLO 源码
    └── config/visible-default.conf
```

移植版复用了原始算法核心，但没有移入下列独立调试组件：

- `main.cpp`；
- `cli.cpp`；
- 鼠标和键盘交互代码；
- `VideoCapture`、`VideoWriter`；
- `imshow`、`waitKey`、`namedWindow`；
- CSV、视频和会话元数据输出。

因此，独立工程负责算法研发和验证，`ai_alg` 负责嵌入 ViewLink 的实时视频链路。

## 3. 总体调用关系

```text
ViewLink 预编译主程序
        │
        │ tracker_detector_init()，程序启动时一次
        │ do_tracker_detector()，每帧一次
        ▼
ai_alg/source/native-lib_impl.cpp
        │
        ├── RGB888 1920x1080 → BGR888
        ├── 解释 STATE_INIT_TRACK / STATE_DO_TRACK / CANCEL
        └── 管理全局 AlgorithmContext
                │
                ▼
        LongTermRuntime
          │           │
          │           ├──────────────► YoloDetectorAdapter
          │           │                 └─ 后台线程运行 YoloRknnEngine
          │           │
          ├──► LongTermController
          │      ├─ 运动预测
          │      ├─ 状态机和搜索范围
          │      ├─ 质量判定
          │      └─ 重捕获确认
          │
          └──► NanoTrackV3
                 ├─ template_backbone.rknn
                 ├─ search_backbone.rknn
                 └─ head.rknn
                        │
                        ▼
              ControllerOutput / RuntimeFrameResult
                        │
                        ▼
              rect_show + track_box_result.rect[0..4]
```

外部不会并发调用接口。算法内部只有 YOLO 检测器使用一个工作线程，主跟踪状态始终由调用 `do_tracker_detector()` 的线程串行维护。

## 4. 关键类及职责

| 模块 | 主要文件 | 作用 |
|---|---|---|
| C ABI 适配 | `ai_alg/source/native-lib_impl.cpp` | 把 ViewLink 的数据结构和命令转换成算法输入输出 |
| 数据结构 | `long_term_types.hpp` | 定义图像视图、跟踪观测、检测候选、控制器状态等 |
| 运行编排 | `long_term_runtime.cpp` | 按固定顺序执行控制器、检测器和 NanoTrack |
| 长时决策 | `long_term_controller.cpp` | 状态机、质量门控、运动预测、模板更新、重捕获 |
| 短时跟踪 | `nanotrack_v3.cpp` | 模板/搜索特征 RKNN 推理和框解码 |
| 运动模型 | `motion_estimator.cpp` | 预测目标下一位置，在失去可靠观测时维持预测框 |
| 异步检测 | `yolo_detector_adapter.cpp` | 拷贝当前帧并在后台线程执行检测，非阻塞地返回批次 |
| YOLO RKNN | `yolo_rknn_engine.cpp` | 模型/清单校验、letterbox、RKNN 推理和后处理 |
| YOLO 解码 | `yolov8_postprocess.cpp` | DFL 解码、置信度筛选、NMS、映射回原图坐标 |
| 配置读取 | `controller_config.cpp` | 运行时读取并校验长时控制器参数 |
| RKNN 封装 | `rknn_model.cpp` | NanoTrack 三个 RKNN 模型的加载、输入输出和资源释放 |

## 5. 程序启动：`tracker_detector_init()`

`tracker_detector_init(AI_TRACK_INFO *info)` 只在程序启动时调用一次。它初始化跟踪器和检测器资源，但**不代表一个具体跟踪任务已经开始**。

执行顺序如下：

1. 清空输出计数和跟踪结果。
2. 校验：
   - `info` 非空；
   - 平台必须为 `RK_A6`；
   - 当前必须为可见光；
   - 图像尺寸必须是 `1920×1080`。
3. 从 `/app/model/visible-default.conf` 读取长时控制器配置。
4. 创建 `NanoTrackV3`，从 `/app/model` 加载：
   - `template_backbone.rknn`；
   - `search_backbone.rknn`；
   - `head.rknn`。
5. 创建 `YoloRknnEngine`，加载并校验：
   - `/app/model/yolov8n-rk3566-fp16.rknn`；
   - `/app/model/detector-manifest.json`；
   - `/app/model/coco80.txt`。
6. `YoloRknnEngine` 校验模型文件大小、SHA-256、类别数量、输入输出 tensor 数量、形状、数据类型和布局。
7. 创建 `YoloDetectorAdapter`，同时启动后台检测线程。
8. 创建 `LongTermRuntime`，把短时跟踪器、检测器和控制器连接起来。
9. 预分配一张 `1920×1080×3` BGR 图像。
10. 填写版本号 `nanotrack-v3-longterm-rk3566-v1`，返回 `true`。

任何模型、配置、清单或 RKNN 初始化异常都会被捕获，销毁上下文，并返回 `false`。这里没有备用模型回退机制。

## 6. 每帧入口：`do_tracker_detector()`

函数签名：

```cpp
bool do_tracker_detector(unsigned char *img_in,
                         AI_TRACK_INFO *info,
                         RECT_SIZE *rect_show);
```

每次调用首先清空旧输出，避免调用失败时宿主误用上一帧数据。返回值的契约是：

- 返回 `true`：宿主可以读取本帧输出；
- 返回 `false`：宿主不应读取本帧输出。

### 6.1 命令处理

| `info->cmd` | 行为 | 返回值 |
|---|---|---|
| `STATE_NO_ACTION` | 不运行算法 | `false` |
| `STATE_AI_DETECT` | 当前版本不提供独立 AI 检测输出 | `false` |
| `STATE_INIT_TRACK` | 用 `init_track_box` 启动/重启具体跟踪任务 | 成功为 `true` |
| `STATE_DO_TRACK` | 处理一帧长时跟踪 | 成功为 `true` |
| `STATE_TRACK_CANCEL` | 取消当前任务，将 `task_active=false` | `false` |

若切换到红外或出现 `is_ir_change_mode`，当前任务立即取消并返回 `false`。当前移植版本不处理红外，也不处理 PIP。

### 6.2 图像格式转换

宿主输入约定固定为：

- RGB888；
- 三通道；
- `1920×1080`；
- 每行步长 `5760` 字节。

适配层先用输入内存构造带固定步长的 `cv::Mat`，再执行：

```cpp
cv::cvtColor(rgb, bgr_frame, cv::COLOR_RGB2BGR);
```

算法内部的 `ImageView` 一律表示 BGR8 图像。YOLO 引擎在送入 YOLO 模型前还会把 BGR 转回模型要求的 RGB。

## 7. 启动具体任务：`STATE_INIT_TRACK`

`init_track_box` 使用原始 `1920×1080` 图像坐标，格式是左上角 `x/y + width/height`。

适配层会把初始框限制在图像范围内：

- `x` 限制到 `[0, 1919]`；
- `y` 限制到 `[0, 1079]`；
- 右边界不超过 `1920`；
- 下边界不超过 `1080`；
- 宽和高最少为 1 像素。

随后调用：

```text
LongTermRuntime::initialize
  ├─ 校验 BGR 图像和初始框
  ├─ NanoTrackV3::initialize
  │    ├─ 保存初始目标位置和尺寸
  │    ├─ 计算图像通道均值
  │    ├─ 从初始框裁剪模板区域
  │    ├─ template_backbone.rknn 提取模板特征
  │    └─ anchor/recent 模板均初始化为该特征
  └─ LongTermController::initialize
       ├─ 当前框 = 初始框
       ├─ 初始化运动模型
       ├─ 状态 = TRACKING
       └─ 清空丢失时间、检测候选和确认历史
```

初始化成功的**当帧立即输出初始框**：

- `rect[0]`、`rect[1]`、`rect[2]` 都等于初始框；
- `rect_show` 等于初始框；
- 返回 `true`。

## 8. 正常跟踪帧的完整时序

当命令为 `STATE_DO_TRACK` 且任务有效时，适配层调用：

```cpp
LongTermRuntime::process_frame(image, frame_id, timestamp_ms);
```

`frame_id` 严格递增；时间戳来自 `steady_clock`，并强制至少比上一帧大 1 ms，避免状态机遇到相同或倒退时间。

### 8.1 第一步：控制器制定搜索计划

`LongTermController::begin_frame()`：

1. 根据上次可靠观测和速度预测本帧目标框。
2. 根据从首次不可靠帧开始经过的时间确定状态。
3. 为当前状态选择搜索区域放大倍数。
4. 若进入检测辅助阶段并存在 YOLO 候选，改用检测候选作为搜索中心。
5. 若检测器空闲且达到请求间隔，生成一次全图检测请求。

生成的 `SearchPlan` 包括：预测框、搜索倍率、假设来源、检测候选信息，以及是否提交检测请求。

### 8.2 第二步：异步提交/获取 YOLO 结果

如果 `request_detector=true`：

1. 主线程把当前 BGR 帧逐行复制到检测任务自己的连续内存；
2. 把任务放入 `YoloDetectorAdapter`；
3. 唤醒后台线程；
4. 主线程继续执行 NanoTrack，不等待 YOLO 完成。

同一帧还会调用 `poll()` 获取以前提交且已经完成的检测批次。检测结果必须满足：

- `source_frame_id` 和 `source_timestamp_ms` 对应最后一次请求；
- 结果年龄不超过 `detector_stale_ms`；
- 候选框和置信度有效；
- 候选数量不超过配置上限。

帧不匹配、过期或非法结果不会用于恢复。

### 8.3 第三步：NanoTrack 推理

`NanoTrackV3::infer(image, plan)` 根据计划中的预测框和搜索倍率工作：

1. 以预测框中心裁剪搜索区域，越界部分使用初始化时的通道均值填充。
2. 把搜索图送入 `search_backbone.rknn` 得到搜索特征。
3. 将搜索特征分别与模板特征送入 `head.rknn`：
   - 永久锚点模板 `anchor`；
   - 若存在，则还有近期模板 `recent`。
4. 解码分类/回归响应，并应用窗口惩罚、尺度和宽高比惩罚。
5. 把结果从搜索图坐标还原到原始 `1920×1080` 坐标。
6. 为每个模板生成一个 `TrackerObservation`，包含框和质量指标：
   - 原始分数 `raw_score`；
   - 惩罚后峰值 `penalized_peak`；
   - PSR；
   - APCE；
   - 峰值差 `peak_margin`。

注意：`infer()` 只产生候选观测，不直接永久更新跟踪状态。是否接受观测由长时控制器决定。

### 8.4 第四步：控制器评估与决策

`LongTermController::finish_frame()` 先执行硬门控：

- 候选中心与预测中心的归一化距离；
- 面积变化对数；
- 宽高比变化对数。

通过硬门控后，再根据 score、PSR、APCE 和 peak margin 把观测分为：

- `Reliable`：可靠；
- `Weak`：弱；
- `Rejected`：拒绝。

若 anchor 和 recent 同时存在，控制器还检查两个模板给出的框是否一致：

- 二者可靠且一致时，选择分数更高者；
- 二者可靠但不一致时，偏向不可变的 anchor，并要求额外多帧确认；
- 只有 recent 可靠时，同样需要额外确认；
- 其余情况选择质量级别更高者。

最终只有满足接受条件的可靠观测才会：

- 设置 `accepted=true`；
- 设置 `control_valid=true`；
- 修正运动模型；
- 更新 `current_bbox`；
- 清除丢失计时和检测候选；
- 回到 `TRACKING`。

未接受时，输出框保持为运动预测框或当前检测假设框，并进入/维持恢复状态。也就是说，丢失期间仍有用于显示的预测框，但它不等于已确认重新捕获。

### 8.5 第五步：提交状态和模板动作

回到 `LongTermRuntime` 后：

- 若观测被接受，调用 `NanoTrackV3::commit_bbox()` 提交新位置；
- 根据控制器输出执行近期模板操作：
  - `StageRecent`：提取一个候选近期模板；
  - `CommitRecent`：试用期通过，正式采用；
  - `RollbackRecent`：试用失败，恢复原近期模板；
  - `None`：不变。

这样做是为了防止一次错误观测直接污染模板并造成不可逆漂移。

## 9. 长时状态机

状态由“从第一次没有可靠接受结果开始经过的单调时间”驱动，而不是简单按帧数驱动。

默认可见光配置：

| 状态 | 时间范围 | 搜索倍率 | 主要行为 |
|---|---:|---:|---|
| `TRACKING` | 有可靠跟踪 | 1.0 | 正常局部搜索 |
| `UNCERTAIN` | 丢失后 `<200 ms` | 1.5 | 短暂扩大搜索，等待恢复 |
| `LOCAL_RECOVERY` | `200–1000 ms` | 2.0，后半段 3.0 | 依靠运动预测和更大局部区域搜索 |
| `ASSISTED_RECOVERY` | `1000–3000 ms` | 4.0 | 启用 YOLO 辅助候选并由 NanoTrack 验证 |
| `LOST` | `≥3000 ms` | 4.0 | 继续输出预测框并持续尝试检测辅助恢复 |

重要区别：

- `mode` 描述算法当前状态；
- `accepted` 表示本帧观测是否被内部状态接受；
- `control_valid` 表示该框是否足够可靠，可用于控制；
- ViewLink 适配层在正常、弱、不确定和丢失阶段都返回 `true`，因为宿主仍需读取并显示预测框。

适配层当前没有把 `control_valid` 独立暴露给宿主，所以宿主看到 `true` 仅表示“输出字段有效”，不能等同于“控制器认为目标可靠”。

## 10. YOLO 辅助重检测流程

### 10.1 为什么异步

YOLO 全图检测比 NanoTrack 局部匹配更重。后台线程让主跟踪帧不必同步等待检测完成。适配器最多维护：

- 一个待处理任务；
- 一个正在执行任务；
- 一个已完成结果。

只有队列、执行和完成槽都空闲时才允许提交新请求。

### 10.2 YOLO 单次推理

`YoloRknnEngine::infer()`：

1. 从请求 ROI 裁图；当前控制器请求的是整张图。
2. 等比例缩放到 `640×640`，空白区域填充 114。
3. BGR 转 RGB。
4. 以 `UINT8/NHWC` 输入 RKNN。
5. 获取 6 或 9 个浮点输出 tensor。
6. 对 stride 8、16、32 的输出执行 DFL 解码。
7. 按置信度和类别筛选。
8. 执行 NMS。
9. 去除 letterbox 缩放与填充，把框映射回原始图像坐标。
10. 按置信度返回有限数量的候选。

当前集成配置：

- 置信度阈值：0.25；
- NMS 阈值：0.45；
- `class_id=-1`，保留所有类别；
- 单批次最多 3 个候选；
- 检测请求间隔至少 500 ms；
- 检测结果超过 150 ms 视为过期。

### 10.3 候选为什么不能直接输出

YOLO 只检测类别目标，不能证明检测到的是用户最初框选的同一实例。因此候选进入控制器后需要继续验证：

1. 按置信度排序候选并分配内部 candidate id。
2. 以候选区域作为 NanoTrack 搜索假设。
3. anchor 和 recent 都必须给出可靠且相互一致的结果。
4. NanoTrack 结果还必须与检测候选在位置和形状上相符。
5. 候选被锁定后，在 3 帧窗口中至少命中 2 次才算重新捕获。
6. 如果同一检测批次存在多个相同类别候选，当前保守策略把它视为身份歧义，不允许候选进入锁定确认。

通过确认后才设置 `reacquired=true`，重新初始化运动模型并回到 `TRACKING`。

## 11. 双模板机制

系统维护两个模板：

### Anchor 模板

- 来自用户最初给定的框；
- 整个任务期间不更新；
- 用于长期身份约束，防止模板随错误目标持续漂移。

### Recent 模板

- 用于适应目标外观缓慢变化；
- 只有连续稳定跟踪达到阈值，并满足更新间隔时才暂存；
- 暂存后还要经过连续可靠帧的试用期；
- 试用失败立即回滚。

默认配置要求：

- 至少 15 个稳定帧才考虑更新；
- 两次更新间隔至少 1000 ms；
- 新模板试用期 3 个可靠帧。

## 12. 运动预测

`MotionEstimator` 保存目标中心、尺寸和速度。可靠跟踪时用新框纠正状态；没有可靠观测时，根据时间差预测下一框。

默认参数：

- `motion_alpha=0.85`：位置修正权重；
- `motion_beta=0.05`：速度修正权重；
- `motion_max_dt_ms=200`：单次预测采用的最大时间间隔。

所有预测框最终都会限制在图像边缘内。丢失期间 `rect_show` 显示的主要就是此预测链路产生的框，或正在验证的检测候选框。

## 13. 输出字段含义

移植版的输出映射如下：

| 字段 | 当前含义 |
|---|---|
| `rect_show` | 最终显示框，即 `ControllerOutput::bbox` |
| `track_box_result.rect[0]` | NanoTrack 本帧第一个原始观测框；无观测时为预测框 |
| `track_box_result.rect[1]` | 最终融合/显示框，与 `rect_show` 相同 |
| `track_box_result.rect[2]` | 运动模型预测框 |
| `track_box_result.rect[3]` | 以预测框为中心、按当前搜索倍率扩大的恢复搜索区域 |
| `track_box_result.rect[4]` | 当前正在验证的 YOLO 检测候选区域；没有候选时为零框 |
| `is_track_base_ai` | 本帧接受结果来自已确认的检测假设时为 `true` |
| `is_reinit_track` | 当前实现始终为 `false` |
| `total_num` | 当前仅做跟踪，固定为 0 |
| `car_cnt` | 固定为 0 |
| `person_cnt` | 固定为 0 |

所有输出框最终都会裁剪到 `1920×1080`，并保证有效输出的宽高至少为 1。

## 14. 配置文件

部署版在运行时读取：

```text
/app/model/visible-default.conf
```

配置主要分为五组：

1. **质量阈值**：reliable/weak 的 score、PSR、APCE、peak margin。
2. **几何门控**：中心距离、面积变化、宽高比变化。
3. **状态时间**：uncertain、local recovery、assisted recovery 的边界。
4. **检测策略**：请求间隔、过期时间、候选数量、确认约束。
5. **模板和运动策略**：recent 更新、确认窗口、搜索倍率、运动滤波参数。

当前配置明确标记 `calibrated=false`，它是打通链路的初始参数，不应视为已经针对最终场景标定的产品阈值。

修改配置时，应优先观察以下关系：

- reliable 阈值过高会更容易进入恢复；过低会增加漂移风险；
- `template_agreement_iou` 过高会让双模板更难同时通过；
- `detector_stale_ms` 必须大于实际 YOLO 端到端耗时，否则完成结果会持续被丢弃；
- `confirmation_required/window` 决定重捕获速度与误捕获风险；
- 搜索倍率越大，恢复覆盖范围越大，但 NanoTrack 判别难度和误匹配概率也会上升。

## 15. 模型文件与启动前提

目标设备 `/app/model` 至少需要：

```text
/app/model/
├── template_backbone.rknn
├── search_backbone.rknn
├── head.rknn
├── yolov8n-rk3566-fp16.rknn
├── detector-manifest.json
├── coco80.txt
└── visible-default.conf
```

任何必要文件缺失或校验失败，`tracker_detector_init()` 都返回 `false`。

当前交叉编译环境的 `librknnrt.so` 版本为 1.5.2，而检测模型原开发/验证环境使用 RKNN Toolkit/Runtime 2.3.2。A6 编译成功只证明代码和链接链路成立，不证明目标板上的旧运行时一定能加载该模型。最终运行前必须在板端实际验证 RKNN 模型初始化和推理。

## 16. A6 构建链路

`ai_alg/CMakeLists.txt` 显式把适配层和算法源文件编译进静态库 `libAI.a`，并使用 C++17。主要链接依赖为：

- `rknnrt`；
- `pthread`；
- `opencv_core`；
- `opencv_imgproc`；
- `yaml-cpp`；
- `spdlog`。

原 `native-lib_stub.cpp` 仍留在目录中作为历史文件，但不在 `add_library(AI ...)` 的源文件列表内，因此不会进入最终静态库。

构建命令：

```bash
cd /home/wjm/viewpro_gimbal_zp/rk3566/gimbal_tracker
BUILD_JOBS=4 ./run.sh --platform A6
```

构建流程可概括为：

```text
A6 交叉工具链
  └─ 编译 ai_alg → libAI.a
       └─ 与预编译 libviewlink 等组件链接
            └─ build_A6/viewlink_app
```

当前已验证生成的程序是 AArch64 ELF。板端部署和运行验收不属于这次编译结果本身。

## 17. 异常与边界行为

### 初始化失败

- 配置、模型、清单或标签不存在；
- SHA-256、文件大小或 tensor 契约不匹配；
- RKNN 初始化失败；
- 输入平台、模式或尺寸不符合约定。

以上情况都会返回 `false`，并把可见光模型坏标志保持为失败状态。

### 跟踪帧失败

- 输入指针为空；
- `rect_show` 为空；
- 未执行成功的初始化；
- 未启动具体跟踪任务；
- 命令不支持；
- 算法内部抛出异常。

函数会清空本帧输出并返回 `false`。若 `STATE_INIT_TRACK` 执行异常，任务会被置为未激活。

### YOLO 后台线程失败

检测异常会在下一次 `poll()` 时重新抛给主流程，并将检测适配器永久禁用。NanoTrack 主链本身仍有机会继续运行，但当前 `LongTermRuntime::process_frame()` 会把错误记录到诊断字段；移植适配层没有对外暴露该诊断。

## 18. 建议的源码阅读顺序

如果希望在半小时内建立整体认识，按下面顺序阅读：

1. `ai_alg/include/native-lib.h`：先理解宿主传入和读取什么。
2. `ai_alg/source/native-lib_impl.cpp`：看命令和输入输出如何接入。
3. `long_term_types.hpp`：熟悉内部数据结构和枚举。
4. `long_term_runtime.cpp`：掌握一帧的固定执行顺序。
5. `long_term_controller.cpp` 的 `begin_frame()` 和 `finish_frame()`：理解真正的长时决策。
6. `nanotrack_v3.cpp`：理解模板、搜索和 head 三模型如何组成短时跟踪。
7. `motion_estimator.cpp`：理解预测框来源。
8. `yolo_detector_adapter.cpp`：理解异步任务和线程安全边界。
9. `yolo_rknn_engine.cpp` 和 `yolov8_postprocess.cpp`：理解检测模型输入输出。
10. `visible-default.conf`：最后把代码中的判断与实际阈值对应起来。

## 19. 调试时最值得先看的位置

| 现象 | 优先检查 |
|---|---|
| 程序启动即初始化失败 | `/app/model` 文件、manifest/hash、RKNN Runtime 版本和错误日志 |
| `STATE_INIT_TRACK` 没框 | 输入尺寸、平台类型、RGB 指针、初始框和命令值 |
| 很快进入丢失 | reliable/weak 阈值、输入 RGB/BGR 是否正确、NanoTrack响应指标 |
| 丢失后从不请求 YOLO | 是否已进入 `ASSISTED_RECOVERY/LOST`，检测器是否 ready，500 ms 间隔 |
| YOLO 有结果但从不恢复 | 150 ms 过期限制、同类多目标歧义、anchor/recent 一致性、2/3 确认 |
| 恢复到错误目标 | 检查同类候选、模板阈值、检测候选与 NanoTrack 几何一致性 |
| 框在边缘异常 | 输入初始框格式、裁剪规则、坐标是否是原始 1920×1080 |
| 跟踪能显示但云台不应动作 | 区分“接口返回 true”和内部 `control_valid`；当前 ABI 未单独输出后者 |

## 20. 当前实现的已知限制

1. 只支持 A6、可见光、固定 `1920×1080 RGB888` 和步长 5760。
2. 不支持红外和 PIP，模式切换会取消当前任务。
3. 不实现独立 `STATE_AI_DETECT` 输出，检测仅用于内部辅助恢复。
4. `sr_model_init()` 和 `sr_infer_frame()` 仍返回 `false`。
5. YOLO 使用 COCO80 且 `class_id=-1`，并非针对具体跟踪目标训练的 ReID 模型。
6. 同类多目标场景采用保守拒绝策略，可能长时间无法重捕获。
7. 配置尚未基于最终数据集标定。
8. 适配层没有把 `control_valid`、状态、质量指标和检测错误完整暴露给宿主。
9. A6 交叉编译成功不等于板端运行时兼容已通过，尤其需要验证 RKNN 1.5.2 与现有模型的兼容性。

## 21. 一帧处理的伪代码

```cpp
bool do_tracker_detector(rgb, info, rect_show) {
    clear_outputs();
    reject_ir_cancel_or_unsupported_commands();
    bgr = RGB_to_BGR(rgb);

    if (info->cmd == STATE_INIT_TRACK) {
        bbox = clamp(info->init_track_box);
        runtime.initialize(bgr, bbox, next_frame_id(), monotonic_time());
        output_initial_bbox_immediately();
        return true;
    }

    if (info->cmd != STATE_DO_TRACK || !task_active)
        return false;

    // LongTermRuntime::process_frame
    plan = controller.begin_frame();
    if (plan.requests_detector)
        detector.submit_copy_async(bgr, plan.request);
    completed_detection = detector.poll_without_waiting();
    observations = nanotrack.infer(bgr, plan);
    result = controller.finish_frame(plan, observations, completed_detection);

    if (result.accepted)
        nanotrack.commit_bbox(result.bbox);
    apply_recent_template_action(result.template_action);

    fill_rect_0_to_4_and_rect_show(result);
    return true;
}
```

理解这段伪代码后，再去阅读具体类，整个工程的主线就不会丢失。
