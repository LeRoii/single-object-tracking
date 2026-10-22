# 遮挡与重捕获视频数据标注要求

**文档版本：** 1.0

**日期：** 2026-10-14

**适用对象：** RK3566 NanoTrack 长时跟踪、detector 辅助重捕获及同类别干扰评测

**坐标格式：** 左上角原点的 `[x, y, width, height]`，解码帧从 0 开始编号

## 1. 目的与适用边界

本文档定义单目标跟踪视频中遮挡、重新出现、形变和同类别干扰的标注要求。标注结果应能够支持：

- 判断目标何时真正丢失、何时重新出现以及何时重新具备可靠控制条件；
- 计算遮挡后的重捕获成功率、重捕获延迟、错误重捕获和身份切换；
- 分析 NanoTrack 的 `anchor/recent` 模板验证、候选筛选和 2/3 确认规则；
- 分别评估完整目标初始化和局部子目标初始化；
- 标定可见光、红外以及不同目标模式下的独立阈值。

本文档规定逐个视频的标注合同。数据集规模、场景数量、数据划分和阈值验收线继续采用
[`2026-10-13-nanotrack-threshold-calibration-data-requirements.md`](../../../docs/superpowers/specs/2026-10-13-nanotrack-threshold-calibration-data-requirements.md)。

检测器输出不是人工真值。标注中可以记录检测类别，但不得把 detector 候选框直接当作目标真值，也不得用 detector 是否检出代替人工身份判断。

## 2. 标注对象与基本约定

### 2.1 物理目标、跟踪目标和干扰物

- `target_id`：物理目标的稳定身份，同一 episode 内不得改变。
- `target_scope`：`full_object` 或 `subtarget`。
- `tracking_target_id`：实际交给跟踪器的对象。完整目标模式下与 `target_id` 相同；局部模式下表示依附于物理目标的局部区域。
- `carrier_target_id`：局部目标所属的完整物理目标 ID。仅 `subtarget` 模式必填。
- `distractor_id`：干扰物的稳定身份。遮挡前后、交叉前后不得重新编号。

即使目标完全遮挡，也必须保留相同的 `target_id`，但不可凭预测填写不可见目标框。

### 2.2 坐标和帧号

- `frame_id`按严格解码顺序从 0 连续编号，不使用容器包序号或程序 CSV 行号代替。
- 所有框均使用源视频分辨率坐标，不使用缩放后的显示画面坐标。
- 框使用浮点数 `[x, y, width, height]`，宽高必须大于 0，且裁剪到图像范围内。
- 时间使用 `timestamp_us`；若原视频时间戳不可靠，应按固定帧率生成并在 manifest 中说明。
- 完全遮挡、完全出画或身份无法判断时，目标框必须为 `null`，不得沿用最后可见框或插值框。

### 2.3 强制程度

本文档中的字段分为：

- **必填**：缺失时该 episode 不得进入正式评测；
- **条件必填**：对应场景出现时必须标注；
- **可选**：可提高分析能力，但不影响基础验收。

## 3. 视频与 episode 元数据

### 3.1 视频级必填字段

每个源视频必须记录：

| 字段 | 要求 |
| --- | --- |
| `video_id` | 数据集内唯一 |
| `path`、`sha256`、`byte_size` | 能唯一定位并校验原始视频 |
| `width`、`height`、`frame_count` | 以严格解码结果为准 |
| `fps_num`、`fps_den`、`time_base` | 不得只记录四舍五入后的 FPS |
| `codec`、`pixel_format`、`bit_depth` | 记录实际编码和位深 |
| `sensor` | `visible` 或 `infrared` |
| `capture_session_id` | 用于防止同源片段跨数据划分 |
| `edited` | 是否剪辑、拼接、变速或重复帧 |
| `source_video_id` | 转码或裁剪时指向原始文件 |
| `transcode_command` | 发生转码时条件必填 |

存在剪切、跳帧、循环拼接或时间轴重排的视频，不得作为真实连续遮挡时长的依据，除非所有编辑点均被显式标注并从相关指标中排除。

### 3.2 Episode 级必填字段

一个 episode 表示一次独立的单目标跟踪任务，至少包含：

```json
{
  "episode_id": "vis_0001_target_01_subtarget",
  "video_id": "vis_0001",
  "target_id": "person_left",
  "target_scope": "subtarget",
  "tracking_target_id": "person_left_head_01",
  "carrier_target_id": "person_left",
  "category": "person",
  "detector_class_id": 0,
  "start_frame": 0,
  "end_frame": 986,
  "init_frame": 0,
  "init_bbox_xywh": [244.0, 161.0, 74.0, 70.0],
  "init_visibility_ratio": 1.0,
  "scenario_tags": ["full_occlusion", "same_class_distractor", "deformation"]
}
```

初始化帧要求：

- 目标身份明确，且 `init_bbox_xywh` 有效；
- 建议可见比例不低于 0.8，边界截断比例不高于 0.1；
- 明确记录是完整目标还是局部子目标，不能只从框大小推断；
- 同一视频若要比较两种模式，应建立两个 episode，不得在运行中途改变 `target_scope`；
- 一个有效遮挡事件应尽量保留至少 1 秒可靠遮挡前画面和 2 秒可靠重现后画面。缺少该上下文时，该事件只能用于诊断，不能用于完整的重捕获验收。

## 4. 逐帧标注

### 4.1 目标状态

`target_state`为必填枚举：

| 状态 | 定义 | 目标框 |
| --- | --- | --- |
| `visible` | 目标清晰可定位，`visibility_ratio >= 0.8` | 必填 |
| `partial_occlusion` | 目标仍可定位，但可见比例低于 0.8 且大于 0 | 条件必填 |
| `full_occlusion` | 人工判断目标仍在视场内，但完全不可见 | 必须为 `null` |
| `out_of_view` | 目标完全离开视场 | 必须为 `null` |
| `ambiguous` | 人工无法可靠判断目标身份或位置 | 必须为 `null` |

`full_occlusion`和`out_of_view`必须区分。无法确定是遮挡还是出画时使用`ambiguous`，不得强行选择。

### 4.2 边界框和可见度

每帧至少记录：

| 字段 | 强制性 | 说明 |
| --- | --- | --- |
| `target_bbox_xywh` | 可定位时必填 | 跟踪器应输出的目标完整范围；局部模式下为局部目标范围 |
| `bbox_valid` | 必填 | `target_bbox_xywh`是否可作为真值 |
| `visible_bbox_xywh` | 部分遮挡时必填 | 仅包围实际可见区域；不得代替`target_bbox_xywh` |
| `carrier_bbox_xywh` | 局部模式条件必填 | 完整物理目标可可靠定位时填写 |
| `visibility_ratio` | 必填 | 实际可见面积/估计完整面积，范围 0～1 |
| `truncation_ratio` | 必填 | 因画面边界而不可见的比例，范围 0～1 |
| `bbox_source` | 必填 | `manual`、`interpolated_reviewed`或`not_available` |

规则：

- `visible_bbox_xywh`描述可见像素范围，`target_bbox_xywh`描述估计完整范围，两者不得混用。
- 允许在短、连续且身份明确的可见区间插值，但所有插值框必须人工复核。
- 不允许跨越完全遮挡区间插值目标框。
- `visibility_ratio`不包含出画导致的截断；出画部分由`truncation_ratio`单独表达。
- 完全遮挡时`visibility_ratio=0`；完全出画时`truncation_ratio=1`。
- 身份不确定时，即使画面中存在同类别对象，也必须使用`ambiguous`，不能把该对象标成目标。

### 4.3 跟踪与控制真值

以下字段为必填：

- `measurement_reliable_gt`：当前目标框是否可以作为可靠位置测量；
- `template_update_allowed_gt`：是否允许使用当前帧更新 recent 模板；
- `control_valid_gt`：是否存在身份明确、位置可靠且足以驱动控制的真值；
- `identity_ambiguous`：目标与干扰物身份是否无法可靠区分。

默认只有在`visibility_ratio >= 0.8`、`truncation_ratio <= 0.1`、身份明确且框可靠时，`template_update_allowed_gt`才允许为`true`。存在模糊、严重运动模糊、重捕获刚完成或标注人员不能确定时，应保守标为`false`。

`control_valid_gt`表示真值是否允许控制，不代表算法的`control_valid`输出。detector 框或 detector 置信度不能直接使任何一个字段变为`true`。

### 4.4 逐帧示例

目标正常可见：

```json
{"frame_id":120,"timestamp_us":4800000,"target_state":"visible","target_bbox_xywh":[238.0,154.0,78.0,74.0],"visible_bbox_xywh":[238.0,154.0,78.0,74.0],"carrier_bbox_xywh":[204.0,130.0,146.0,310.0],"bbox_valid":true,"bbox_source":"manual","visibility_ratio":1.0,"truncation_ratio":0.0,"measurement_reliable_gt":true,"template_update_allowed_gt":true,"control_valid_gt":true,"identity_ambiguous":false,"deformation_tags":["none"],"distractors":[]}
```

部分遮挡并存在同类干扰：

```json
{"frame_id":245,"timestamp_us":9800000,"target_state":"partial_occlusion","target_bbox_xywh":[250.0,158.0,80.0,76.0],"visible_bbox_xywh":[274.0,158.0,56.0,70.0],"carrier_bbox_xywh":[218.0,134.0,150.0,312.0],"bbox_valid":true,"bbox_source":"manual","visibility_ratio":0.58,"truncation_ratio":0.0,"measurement_reliable_gt":false,"template_update_allowed_gt":false,"control_valid_gt":false,"identity_ambiguous":false,"deformation_tags":["articulation"],"distractors":[{"distractor_id":"person_right","category":"person","bbox_xywh":[330.0,128.0,142.0,318.0],"same_class":true,"appearance_similarity":"high","overlaps_target":true}]}
```

完全遮挡：

```json
{"frame_id":260,"timestamp_us":10400000,"target_state":"full_occlusion","target_bbox_xywh":null,"visible_bbox_xywh":null,"carrier_bbox_xywh":null,"bbox_valid":false,"bbox_source":"not_available","visibility_ratio":0.0,"truncation_ratio":0.0,"measurement_reliable_gt":false,"template_update_allowed_gt":false,"control_valid_gt":false,"identity_ambiguous":false,"deformation_tags":["none"],"distractors":[{"distractor_id":"person_right","category":"person","bbox_xywh":[326.0,126.0,145.0,320.0],"same_class":true,"appearance_similarity":"high","overlaps_target":false}]}
```

## 5. 遮挡与重现事件标注

逐帧状态之外，每次遮挡必须建立事件记录。完全遮挡事件至少包含：

```json
{
  "event_id": "vis_0001_occ_01",
  "episode_id": "vis_0001_target_01_subtarget",
  "type": "full_occlusion",
  "first_partial_frame": 244,
  "first_full_frame": 258,
  "last_full_frame": 281,
  "first_visible_frame": 282,
  "first_reliable_reappearance_frame": 286,
  "stable_reappearance_frame": 288,
  "last_reliable_pre_occlusion_frame": 243,
  "occluder_type": "person",
  "occluder_id": "person_right",
  "target_moved_during_occlusion": true,
  "identity_ambiguous_after_reappearance": false,
  "valid_for_recovery_evaluation": true
}
```

事件边界定义：

- `first_partial_frame`：可见比例首次低于 0.8 的帧；没有部分遮挡阶段时可为`null`。
- `first_full_frame`：首个`visibility_ratio=0`且判断仍在视场内的帧。
- `last_full_frame`：最后一个完全不可见帧。
- `first_visible_frame`：遮挡后首次出现任何可辨认目标像素的帧，不要求可可靠定位。
- `first_reliable_reappearance_frame`：目标身份明确、框可可靠标注且`measurement_reliable_gt=true`的首帧。
- `stable_reappearance_frame`：从该帧开始至少连续 3 帧可靠；用于排除单帧闪现。
- `last_reliable_pre_occlusion_frame`：进入遮挡前最后一个可靠测量帧。

重捕获延迟统一从`first_reliable_reappearance_frame`开始计算，而不是从首个可见像素、最后完全遮挡帧或 detector 首次检出帧开始计算。

下列情况必须设置`valid_for_recovery_evaluation=false`并写明`invalid_reason`：

- 遮挡期间存在剪辑、拼接、循环边界或无法解释的跳帧；
- 遮挡前后不能确认是同一物理目标；
- 重现后没有足够帧数判断是否稳定恢复；
- 人工无法给出可靠重现帧；
- 原始视频已有覆盖目标的 OSD 或跟踪框，且无法恢复无叠加源视频。

## 6. 局部子目标、形变和同类干扰

### 6.1 局部子目标

当初始框是脸、头部、车体局部或任意子区域时，必须额外记录：

- `subtarget_type`：`face`、`head`、`vehicle_part`、`arbitrary_region`或自定义语义；
- `carrier_target_id`及可定位帧上的`carrier_bbox_xywh`；
- 初始局部框相对于 carrier 的归一化中心、宽高比例；
- `subtarget_observable`：该局部区域在当前姿态下是否仍具有明确物理含义；
- `subtarget_changed_by_deformation`：形变是否使原局部区域的外观或几何关系发生明显变化。

局部区域完全不可辨认但完整物理目标仍可见时，不得把完整目标框填入`target_bbox_xywh`。应将局部目标标为不可可靠测量，同时保留 carrier 框供身份和 detector 区域评测使用。

### 6.2 形变标签

`deformation_tags`可以包含：

- `scale`
- `aspect_ratio`
- `viewpoint`
- `rotation`
- `articulation`
- `illumination`
- `thermal_contrast`
- `blur`
- `none`

每个标签同时记录`severity`：`low`、`medium`或`high`。连续明显形变必须在`events.json`中建立区间，不能只在单帧上零散打标签。

### 6.3 同类或相似干扰物

在遮挡前 1 秒至可靠重现后至少 2 秒的评测窗口内，所有进入目标搜索区域的同类别或高相似干扰物均为条件必填：

- 稳定`distractor_id`；
- 每帧`bbox_xywh`和类别；
- `same_class`；
- `appearance_similarity`：`low`、`medium`或`high`；
- `overlaps_target`；
- `identity_ambiguous_with_target`；
- 出现、离开、交叉和重叠事件区间。

不要求标注全画面所有无关对象，但不能遗漏 detector 可能作为重捕获候选且可能造成身份切换的对象。检测置信度只能作为算法日志，不能用于确定`distractor_id`。

## 7. 数据组织和交付格式

推荐目录：

```text
occlusion-tracking-v1/
├── README.md
├── dataset_manifest.json
├── checksums.sha256
├── videos/
│   ├── visible/
│   └── infrared/
├── annotations/
│   ├── episodes.json
│   ├── events.json
│   └── frames/
│       └── <episode_id>.jsonl
├── splits/
│   ├── calibration.json
│   ├── development.json
│   └── locked-test.json
└── quality/
    ├── validation-report.json
    └── review-records.jsonl
```

交付要求：

- `schema_version`写入所有 JSON/JSONL 顶层记录；
- 每个视频、标注文件和划分文件均写入`checksums.sha256`；
- 同一物理视频产生的完整目标和局部目标 episode 必须进入同一数据划分；
- 同一连续拍摄、同一物理目标或同一视频的裁剪版本不得跨 calibration、development 和 locked-test；
- 可见光与红外分别标注、分别划分、分别报告，不得用一套阈值或标注结论替代另一模态；
- 原始视频、标注规范版本、审核记录、程序输出和模型/配置哈希必须可追溯。

## 8. 质量检查与可计算指标

### 8.1 自动检查

交付前必须自动验证：

- 视频可以严格解码，帧数、分辨率、时间戳和 manifest 一致；
- `frame_id`从 0 连续且无重复，逐帧标注数量与 episode 范围一致；
- 有效框数值有限、宽高为正且位于源图范围内；
- `full_occlusion`、`out_of_view`和`ambiguous`帧的目标框均为`null`；
- `visibility_ratio`、`truncation_ratio`与`target_state`满足本文约束；
- 所有事件边界位于 episode 内且时间顺序合法；
- 每个 target、subtarget、carrier 和 distractor 引用的 ID 均存在且稳定；
- 完全遮挡区间不存在插值目标框；
- 三个数据划分互斥，且按物理源视频分组；
- 所有路径存在且 SHA-256 匹配。

### 8.2 人工复核

- 完全遮挡、可靠重现、同类交叉和算法`reacquired=1`事件必须 100%复核；
- 所有事件边界至少逐帧检查前后各 5 帧；
- 至少 20%的 episode 由第二名标注者独立复核；
- 双标普通可见帧框 IoU 中位数应不低于 0.85；
- 遮挡开始、结束和可靠重现边界的双标差异应不超过 2 帧；
- 身份存在争议时必须升级人工审核，不能以 detector 类别或置信度裁决；
- 修订必须保留标注版本、审核人、时间和变更原因。

### 8.3 正式评测指标

完成上述标注后，至少报告：

| 指标 | 定义 |
| --- | --- |
| 遮挡事件重捕获成功率 | 每个有效事件是否在规定窗口内恢复原目标 |
| 重捕获延迟 | `first_reliable_reappearance_frame`到首次有效恢复帧的时间 |
| 错误重捕获率 | 恢复到错误身份，或恢复后连续 3 帧 IoU `< 0.1` |
| 身份切换次数 | 输出目标从`target_id`切换到任一`distractor_id`的事件数 |
| 漏恢复率 | 目标已可靠重现但算法在窗口内未恢复 |
| 遮挡期非法控制率 | 不具备控制真值时算法输出`control_valid=true`的帧比例 |
| 恢复后稳定性 | 恢复后连续 5 帧中至少 4 帧 IoU `>= 0.5`的事件比例 |
| 形变分层指标 | 按形变类型和严重度报告 AUC、精确率与恢复率 |
| 干扰分层指标 | 按同类数量、相似度和交叉/重叠状态报告错误恢复和延迟 |
| 模式分层指标 | `full_object`与`subtarget`分别报告，不混合掩盖差异 |

算法恢复帧必须同时满足算法自身的身份确认和控制安全条件。对当前控制器而言，detector 候选只提供搜索假设；只有 NanoTrack 模板验证、同一候选的确认规则及其他门控完成后，才允许产生`control_valid=true`。评测脚本必须单独检查该安全约束，不能仅检查最终 IoU。
