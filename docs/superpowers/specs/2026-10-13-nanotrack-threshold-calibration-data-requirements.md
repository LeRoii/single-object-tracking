# RK3566 NanoTrack 长时跟踪阈值标定数据需求规范

**文档版本：** 1.0  
**日期：** 2026-10-13  
**适用平台：** RK3566  
**适用模型：** 当前已在RK3566开发板验证的NanoTrack V3三模型部署  
**输入形式：** 离线视频文件  
**输出目标：** 可见光、红外两套独立的跟踪质量、模板更新、运动门控和重捕获阈值配置

## 1. 目的

本规范定义为RK3566 NanoTrack分层长时跟踪控制器采集、标注、划分、校验和交付阈值标定数据的要求。

数据用于标定以下运行时决策：

1. NanoTrack当前定位结果是否可靠。
2. 是否应从`TRACKING`进入`UNCERTAIN`。
3. 是否应从`UNCERTAIN`恢复或进入`LOCAL_RECOVERY`。
4. 局部搜索候选是否可作为原目标重新接管。
5. 当前帧是否允许更新`recent`模板。
6. 目标尺度、长宽比、位置变化是否仍属于合理形变和运动。
7. 云台遥测存在时，其运动补偿是否可用。

本数据不用于训练或微调NanoTrack权重。NanoTrack在本任务中视为固定模型；模型更换后必须重新回放并重新标定阈值。

## 2. 范围与边界

### 2.1 纳入范围

- 可见光和红外同模态单目标跟踪。
- 用户通过初始水平矩形框指定目标。
- 任意目标和已知类别目标。
- 正常连续跟踪、目标形变、局部遮挡、出画边缘、快速运动、云台运动和相似干扰物。
- 最长3秒的局部丢失与重新出现。
- 水平矩形框输出。
- 视频解码后的逐帧离线回放。

### 2.2 不纳入范围

- NanoTrack训练、微调和模型选择。
- 交叉编译工具链和产品部署包。
- 可见光与红外之间的跨模态身份匹配。
- 超过3秒后的无限期全画面找回。
- 旋转框和像素级分割标注。
- detector模型训练、转换和候选阈值标定。

detector相关字段在数据结构中预留。首版阈值标定不得依赖detector输出，且detector接口缺失不得阻塞NanoTrack局部恢复标定。

## 3. 标定对象

阈值配置不得只包含一个NanoTrack分类分数。每路传感器应独立输出下列阈值组。

### 3.1 响应质量阈值

- 前景最大响应值。
- 经尺度/窗口惩罚后的最大响应值。
- PSR（Peak-to-Sidelobe Ratio）。
- Top1与Top2响应间隔。
- 响应图归一化熵。
- 峰值邻域能量占比。
- `anchor`与`recent`模板响应差值。

### 3.2 运动门控阈值

- 中心点位移相对目标对角线的比例。
- Kalman创新量和马氏距离。
- 速度和加速度突变量。
- 遥测补偿前后的预测残差。
- 不同丢失时长下允许的搜索区域扩张范围。

### 3.3 形状门控阈值

- 相邻可信帧面积比。
- 宽度比和高度比。
- 长宽比变化率。
- 多帧持续尺度变化与单帧异常跳变的区分阈值。
- 框与图像边界的截断比例。

### 3.4 模板更新阈值

- recent模板更新所需的连续高可信帧数。
- 最小模板更新时间间隔。
- 候选模板与旧recent模板的响应差。
- 候选模板3帧观察期的提交/回滚条件。
- 禁止更新条件，包括遮挡、边界截断、响应多峰、异常运动和刚完成重捕获。

### 3.5 重捕获阈值

- recent候选进入anchor复核的最低条件。
- anchor/recent联合接受条件。
- 运动预测门控范围。
- 形状一致性范围。
- 2/3帧连续确认所需的最低质量。
- 多候选接近时保持`REACQUIRING`而非强制选择的二义性条件。

## 4. 数据总体规模

### 4.1 最低交付规模

| 项目 | 可见光 | 红外 | 合计 |
| --- | ---: | ---: | ---: |
| 物理源视频 | ≥60 | ≥60 | ≥120 |
| 独立跟踪episode | ≥100 | ≥100 | ≥200 |
| 有效标注时长 | ≥45分钟 | ≥45分钟 | ≥90分钟 |
| 有效标注帧 | ≥67,500帧 | ≥67,500帧 | ≥135,000帧 |
| 真实完全遮挡事件 | ≥60 | ≥60 | ≥120 |
| 真实部分遮挡事件 | ≥30 | ≥30 | ≥60 |
| 明显形变事件 | ≥40 | ≥40 | ≥80 |
| 同类或相似干扰事件 | ≥20 | ≥20 | ≥40 |

有效帧按25 FPS折算。原视频为30 FPS时保留原始帧率，不得为了满足数量重复帧或复制episode。

### 4.2 Episode定义

一个episode表示一次独立的单目标跟踪任务，应满足：

- 有且只有一个指定目标ID。
- 包含明确初始化帧和初始框。
- 持续时间原则上为10～60秒。
- 初始化时目标至少80%可见，框不应被画面边界严重截断。
- 同一物理视频可以产生多个episode，但所有episode必须进入同一个数据划分。
- 同一目标在同一连续拍摄中的相邻片段不得跨数据划分。

## 5. 场景覆盖要求

以下事件数量按每种传感器分别统计。一个事件可以同时属于多个标签，但必须有独立的事件区间和属性。

| 场景 | 最低数量 | 定义与要求 |
| --- | ---: | --- |
| 干净稳定跟踪 | 20个episode | 无明显遮挡、无同类交叉，用于标定误报恢复率和模板稳定性 |
| 0.2～0.5秒完全遮挡 | 15次 | 目标完全不可见，随后在局部邻域重新出现 |
| 0.5～1秒完全遮挡 | 15次 | 同上 |
| 1～2秒完全遮挡 | 15次 | 同上 |
| 2～3秒完全遮挡 | 15次 | 同上 |
| 部分遮挡 | 30次 | 可见比例在10%～80%之间，至少持续5帧 |
| 尺度变化 | 20次 | episode内目标面积最大值/最小值≥2 |
| 长宽比变化 | 20次 | episode内长宽比最大值/最小值≥1.5 |
| 姿态或非刚性形变 | 20次 | 人体姿态、车辆视角、机翼/结构形态等发生明显变化 |
| 快速目标运动 | 20次 | 连续帧中心位移显著高于该传感器常规分布 |
| 快速云台运动 | 20次 | 云台转动导致全局背景明显运动；至少一半应有同步遥测 |
| 画面边界截断 | 15次 | 目标框与图像边界相交，截断比例覆盖轻度和重度 |
| 相似干扰物进入搜索区 | 20次 | 存在外观相似或同类别非目标对象 |
| 同类交叉或重叠 | 10次 | 目标与干扰物发生交叉、靠近或短时重叠 |
| 低对比度/照度变化 | 20次 | 可见光逆光、阴影、昼夜变化；红外热对比度下降或极性变化 |
| 小目标 | 20个episode | 初始或过程中的目标面积低于画面面积0.1% |

真实事件可以重叠，例如“云台运动+小目标+部分遮挡”。统计时应保留所有标签，评测报告必须能够按单标签和组合标签过滤。

## 6. 目标尺寸与运动分布

每种传感器的episode应覆盖下列目标面积比例：

| 目标面积/画面面积 | 名称 | 最低占比 |
| --- | --- | ---: |
| <0.1% | 微小目标 | 20% |
| 0.1%～1% | 小目标 | 30% |
| 1%～5% | 中目标 | 30% |
| >5% | 大目标 | 10% |

剩余10%可以按真实业务分布分配。若真实业务中微小目标比例更高，应按业务比例增加，不得为了均衡而删除困难样本。

运动分布至少覆盖：

- 静止目标、静止相机。
- 运动目标、静止相机。
- 静止目标、运动相机/云台。
- 运动目标、运动相机/云台。
- 速度突变和方向反转。
- 变焦或FOV变化。

## 7. 视频文件要求

### 7.1 基本要求

- 每个物理源视频必须能够被`ffprobe`和OpenCV严格解码。
- 帧数、分辨率、帧率和时间基必须可确定。
- 视频内不得存在未记录的丢帧、重复帧或时间戳倒退。
- 优先保留采集原始文件；如需转码，必须保留原文件哈希和转码命令。
- 校准和验收不得使用带跟踪框、OSD遮挡目标或经过二次裁剪的可视化输出作为唯一来源。

### 7.2 推荐格式

- 容器：MP4或MKV。
- 编码：H.264/H.265；如设备输出原始AVI，可保留原格式。
- 帧率：固定25或30 FPS。
- 可见光：保留实际采集分辨率，验收至少覆盖1920×1080。
- 红外：保留实际采集分辨率，验收至少覆盖640×512。
- 可见光颜色：RGB/BGR三通道，具体顺序写入manifest。
- 红外：允许单通道8/16位或三通道复制形式，位深与归一化方式必须记录。

### 7.3 文件完整性

每个视频必须记录：

- 相对路径。
- SHA-256。
- 字节数。
- 宽、高、帧率、总帧数和时长。
- 编码格式、像素格式和位深。
- 传感器类型。
- 采集设备/镜头标识。
- 拍摄会话ID。
- 是否存在云台遥测。

## 8. 标注语义

### 8.1 每帧目标状态

`target_state`只允许下列枚举：

- `visible`：目标清晰可见。
- `partial_occlusion`：目标仍可定位，但部分被遮挡。
- `full_occlusion`：目标仍推断在视场内，但完全不可见。
- `out_of_view`：目标完全离开当前视场。
- `ambiguous`：人工无法可靠判断目标身份或位置。

### 8.2 边界框规则

- 坐标统一为左上角原点的`[x, y, width, height]`浮点数。
- `visible`和可可靠推断完整范围的`partial_occlusion`标注目标完整水平外接框。
- 完全不可见、出画或人工无法可靠判断时，`bbox`必须为`null`且`bbox_valid=false`。
- 不得用最后可见框填充不可见帧。
- 框必须限制在图像坐标范围内；被边界截断时另行记录`truncation_ratio`。

### 8.3 可见度和截断度

- `visibility_ratio`：目标实际可见面积/估计完整目标面积，范围0～1。
- `occlusion_ratio = 1 - visibility_ratio`，不包含画面边界截断。
- `truncation_ratio`：目标超出图像边界的估计比例，范围0～1。
- 完全遮挡时`visibility_ratio=0`。
- 完全出画时`truncation_ratio=1`，并使用`target_state=out_of_view`。

### 8.4 模板和控制真值

每帧必须给出：

- `measurement_reliable_gt`：该帧是否可作为可靠位置测量。
- `template_update_allowed_gt`：该帧是否允许生成recent模板。
- `control_valid_gt`：该帧的真实目标位置是否足以用于闭环控制。
- `identity_ambiguous`：当前目标与干扰物身份是否无法可靠区分。

默认规则：仅当`visibility_ratio≥0.8`、`truncation_ratio≤0.1`、身份明确且框标注可靠时，`template_update_allowed_gt`才可为true。人工复核可以将其改为false，不得放宽为true。

### 8.5 形变标签

`deformation_tags`为数组，可取：

- `scale`
- `aspect_ratio`
- `viewpoint`
- `rotation`
- `articulation`
- `illumination`
- `thermal_contrast`
- `blur`
- `none`

形变标签描述该帧相对最近可靠模板的变化。连续形变应同时建立事件区间。

### 8.6 干扰物标注

在相似干扰或交叉事件区间，至少标注进入预测/搜索区域的干扰物：

- 稳定的`distractor_id`。
- 水平框。
- 类别。
- `same_class`。
- 人工外观相似等级：`low/medium/high`。
- 是否与目标发生重叠。

普通无干扰帧无需标注全画面所有对象。

## 9. 事件标注

每个遮挡、形变、快速运动、边界或干扰事件使用独立区间记录：

```json
{
  "event_id": "vis_0007_occ_02",
  "episode_id": "vis_0007_target_01",
  "type": "full_occlusion",
  "start_frame": 415,
  "end_frame": 452,
  "first_reliable_reappearance_frame": 453,
  "attributes": {
    "duration_ms": 1520,
    "occluder": "vehicle",
    "target_moved_during_occlusion": true
  }
}
```

事件边界定义：

- 完全遮挡从首个`visibility_ratio=0`帧开始。
- 结束帧为最后一个完全不可见帧。
- `first_reliable_reappearance_frame`必须满足目标重新可定位且身份明确。
- 部分遮挡事件从`visibility_ratio<0.8`开始，恢复到≥0.8并持续至少3帧后结束。

## 10. 云台遥测要求

云台遥测不是所有视频的强制项，但每种传感器至少20段云台运动episode必须具有可用遥测。

遥测文件字段：

```text
timestamp_us,yaw_deg,pitch_deg,roll_deg,yaw_rate_dps,pitch_rate_dps,roll_rate_dps,hfov_deg
```

要求：

- 使用单调时间戳或提供与视频PTS的映射。
- 记录单位、坐标方向和角度零点。
- 遥测采样率不得低于视频帧率的一半。
- 每帧应能找到前后各不超过80 ms的遥测样本，否则该帧标记`telemetry_valid=false`。
- 丢包、复位、异常跳变必须保留并标记，不得静默插值成正常数据。

## 11. 数据目录和文件格式

建议交付结构：

```text
threshold-calibration-v1/
├── README.md
├── dataset_manifest.json
├── checksums.sha256
├── videos/
│   ├── visible/
│   └── infrared/
├── annotations/
│   ├── frames/
│   │   └── <video_id>.jsonl
│   ├── episodes.json
│   └── events.json
├── telemetry/
│   └── <video_id>.csv
└── splits/
    ├── calibration.json
    ├── development.json
    └── locked-test.json
```

### 11.1 dataset_manifest.json

```json
{
  "schema_version": "1.0",
  "dataset_id": "rk3566-nanotrack-threshold-v1",
  "created_at": "2026-10-13T00:00:00+08:00",
  "videos": [
    {
      "video_id": "vis_0001",
      "path": "videos/visible/vis_0001.mp4",
      "sha256": "...",
      "sensor": "visible",
      "width": 1920,
      "height": 1080,
      "fps_num": 25,
      "fps_den": 1,
      "frame_count": 1500,
      "pixel_format": "yuv420p",
      "decoded_color_order": "BGR",
      "capture_session_id": "session_20261013_01",
      "telemetry_path": "telemetry/vis_0001.csv"
    }
  ]
}
```

### 11.2 episodes.json

```json
{
  "schema_version": "1.0",
  "episodes": [
    {
      "episode_id": "vis_0001_target_01",
      "video_id": "vis_0001",
      "target_id": "target_01",
      "start_frame": 100,
      "end_frame": 900,
      "init_frame": 100,
      "init_bbox_xywh": [812.0, 430.0, 76.0, 54.0],
      "category": "vehicle",
      "known_detector_class": true,
      "scenario_tags": ["scale", "partial_occlusion", "gimbal_motion"]
    }
  ]
}
```

### 11.3 每帧JSONL

每行对应一个解码帧，`frame_id`从0开始且连续：

```json
{"frame_id":100,"timestamp_us":4000000,"target_state":"visible","bbox_xywh":[812.0,430.0,76.0,54.0],"bbox_valid":true,"visibility_ratio":1.0,"truncation_ratio":0.0,"measurement_reliable_gt":true,"template_update_allowed_gt":true,"control_valid_gt":true,"identity_ambiguous":false,"deformation_tags":["none"],"camera_motion":"gimbal","telemetry_valid":true,"distractors":[]}
```

对于完全遮挡：

```json
{"frame_id":415,"timestamp_us":16600000,"target_state":"full_occlusion","bbox_xywh":null,"bbox_valid":false,"visibility_ratio":0.0,"truncation_ratio":0.0,"measurement_reliable_gt":false,"template_update_allowed_gt":false,"control_valid_gt":false,"identity_ambiguous":false,"deformation_tags":["none"],"camera_motion":"gimbal","telemetry_valid":true,"distractors":[]}
```

## 12. 数据划分

### 12.1 比例

- `calibration`：60%，用于选择所有高低阈值和规则参数。
- `development`：20%，用于检查过拟合和最多一次规则调整。
- `locked-test`：20%，只允许在规则和阈值冻结后运行最终验收。

### 12.2 防泄漏规则

- 以物理源视频和采集会话为最小分组单位划分。
- 同一物理目标、同一连续拍摄、相邻裁剪片段不得跨划分。
- 同一视频产生的所有episode必须在同一划分。
- 合成遮挡版本与其原始视频必须在同一划分。
- 按传感器、目标尺寸、类别、遮挡时长和场景标签分层，使三个划分均有覆盖。
- `locked-test.json`生成后记录SHA-256，不得根据测试结果替换样本。

## 13. 合成数据使用规则

合成遮挡、亮度变化和模糊可用于扩大标定覆盖，但只能作为补充：

- 合成事件不得超过calibration遮挡事件的30%。
- development中合成事件不得超过20%。
- locked-test必须全部使用真实采集事件。
- 合成过程必须保存原始视频ID、随机种子、遮挡轨迹和生成参数。
- 不能通过复制静止框或简单黑块覆盖来替代全部真实遮挡。

## 14. 标注质量控制

### 14.1 自动检查

交付前必须通过：

- 所有视频严格解码成功，解码帧数与manifest一致。
- `frame_id`连续且无重复。
- 时间戳严格单调。
- 所有有效框有限、宽高为正且位于图像范围内。
- 不可见帧的`bbox`必须为null。
- episode和事件区间不得超出视频范围。
- 初始化帧必须存在有效框。
- 所有相对路径存在且SHA-256匹配。
- 三个split互斥，且并集等于全部物理源视频。

### 14.2 人工复核

- 所有完全遮挡、重捕获和同类交叉事件100%复核。
- 每个普通episode至少抽查首帧、中间帧、末帧及所有事件边界。
- 至少20%的episode由第二名标注者独立复核。
- 双标可见帧框IoU中位数应≥0.85。
- 遮挡开始/结束帧的双标差异应≤2帧。
- 不满足要求的数据必须修订，不得仅在报告中声明误差。

## 15. 阈值标定流程要求

### 15.1 原始信号回放

首先以“只记录、不改变状态”的方式运行固定NanoTrack模型，逐帧保存：

- 原始和惩罚后响应图统计。
- NanoTrack输出框。
- anchor/recent模板响应。
- Kalman预测、创新量和协方差。
- 尺度、面积、长宽比和边界特征。
- 传感器和遥测有效状态。
- 单阶段推理耗时。

每次回放必须绑定：

- 三个RKNN模型SHA-256。
- 可执行程序SHA-256。
- RKNN Runtime和NPU驱动版本。
- 数据集manifest与split SHA-256。
- 完整配置文件SHA-256。

### 15.2 监督标签生成

运行时阈值不可访问真值，但标定阶段用真值将帧划分为：

- `reliable`：目标可见、身份明确，预测框与真值框IoU≥0.5，且中心误差≤真值框对角线的0.25。
- `degraded`：目标仍可见但定位质量低于reliable，或存在明显二义性。
- `failed`：目标不可见、预测框IoU<0.1、跟到干扰物，或人工确认身份已经切换。

IoU在0.1～0.5之间的帧不得自动并入reliable，应保留为degraded用于迟滞阈值设计。

### 15.3 参数选择

- calibration集执行网格搜索或确定性参数搜索。
- 高质量规则要求响应、运动、形状三组条件均通过。
- 低质量规则采用至少两组失败；非法框或运动硬门限失败立即判低。
- 优化目标优先最大化`reliable`与`failed`分类宏F1。
- 同时约束错误重捕获事件率≤3%。
- 可见光和红外分别搜索参数，不允许先混合再只输出一套阈值。
- development集只允许一次预先记录的规则调整；调整后必须重新冻结配置。
- locked-test只运行一次正式验收，不得用于回调阈值。

## 16. 验收指标

所有指标分别报告可见光、红外和总体宏平均。

| 指标 | 要求 |
| --- | ---: |
| 真实失效后0.2秒内退出TRACKING的事件比例 | ≥90% |
| 正常序列误进入恢复状态的时间占比 | ≤5% |
| 0.2～3秒遮挡后的找回成功率 | ≥85% |
| 重新可见至恢复的时间中位数 | ≤0.5秒 |
| 重新可见至恢复的P95时间 | ≤1秒 |
| 错误重捕获事件率 | ≤3% |
| 干净序列AUC相对固定NanoTrack基线下降 | ≤0.02 |
| 形变子集AUC相对固定模板基线提升 | ≥0.03 |
| 不允许模板更新帧上的错误模板提交 | 0次 |
| locked-test视频、标注和结果完整率 | 100% |

找回成功定义为：恢复后连续5帧中至少4帧IoU≥0.5，且没有切换到干扰物。

错误重捕获定义为：系统重新置为`TRACKING/control_valid=true`，但目标身份错误，或恢复后连续3帧IoU<0.1。

## 17. detector预留要求

为避免未来重新标注，现阶段必须保留：

- `category`
- `known_detector_class`
- 同类干扰物框和ID
- 遮挡结束后的首个可靠重现帧
- 目标身份是否有二义性

未来detector接入后，可以追加每帧检测候选日志：

```json
{
  "source_frame_id": 453,
  "completed_timestamp_us": 18275000,
  "candidates": [
    {"bbox_xywh": [820, 438, 80, 58], "class_id": 2, "score": 0.84}
  ]
}
```

首版数据验收不要求生成该文件，也不得因为没有detector而删除类别和干扰物标注。

## 18. 模型替换规则

模型替换后，数据集可以复用，但阈值配置不得直接继承。至少执行：

1. 重新生成全部原始响应和运动特征日志。
2. 对比新旧模型响应分布。
3. 重新运行calibration和development参数选择。
4. 冻结新配置后重新运行locked-test。
5. 保存新模型、阈值配置和验收报告之间的哈希映射。

如果只替换模板或搜索backbone中的任意一个RKNN文件，也视为完整模型替换。

## 19. 最终交付物

完整数据交付必须包含：

1. 原始或可追溯转码的视频文件。
2. `dataset_manifest.json`。
3. 每帧JSONL标注。
4. `episodes.json`和`events.json`。
5. 可用时的云台遥测CSV。
6. calibration/development/locked-test划分文件。
7. 全部文件SHA-256清单。
8. 自动质量检查程序及PASS报告。
9. 标注规范、人工复核记录和已知限制。
10. 固定NanoTrack基线回放结果。
11. 可见光和红外的最终阈值配置。
12. development和locked-test验收报告。

只有当数据、模型、阈值配置和评测结果均能通过SHA-256相互追溯时，阈值标定任务才视为完成。
