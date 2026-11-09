# 长时单目标跟踪数据集真值需求规范

**文档版本：** 1.0  
**日期：** 2026-11-07  
**适用范围：** 可见光/红外视频中的 NanoTrack 长时单目标跟踪、遮挡恢复、出画重入、形变适应及同类别干扰评测  
**数据用途：** 阈值标定、算法开发、离线评测和回归测试  

## 1. 文档目的

本文档只规定长时单目标跟踪数据集需要提供什么样的人工真值、辅助估计和交付证据，不规定：

- 使用哪一种标注工具或界面；
- 标注任务采用单人还是多人协作；
- NanoTrack、detector 或控制器的内部日志格式；
- 运行日志采用 schema-v5、schema-v9 或其他版本；
- 具体算法应使用哪些阈值。

符合本文档的数据应能够支持：

1. 普通可见阶段的跟踪精度与稳定性评测；
2. 部分遮挡、完全遮挡和出画后的目标找回评测；
3. 同类别目标交叉、靠近和重叠时的身份保持评测；
4. 尺度、姿态、视角和非刚性形变条件下的鲁棒性评测；
5. 完整目标与任意局部子目标初始化的独立评测；
6. 遮挡期间运动预测能力的开发与单独评价；
7. 后续可靠阈值标定，且不会把算法输出反向当成人工真值。

## 2. 核心原则

### 2.1 真值、估计和算法输出必须分层

数据中必须区分以下三类信息：

| 层级 | 含义 | 是否属于数据真值 |
| --- | --- | --- |
| 可观测真值 | 人工根据当前帧真实可见像素和明确身份标出的目标状态与位置 | 是 |
| 遮挡期辅助估计 | 人工根据前后帧、运动趋势或场景结构推测的合理位置 | 否 |
| 算法输出 | NanoTrack、detector、运动模型或其他程序产生的结果 | 否 |

完全遮挡、完全出画或身份无法确定时，不存在可直接观测的位置真值。此时允许填写合理的`estimated_bbox_xywh`，用于运动预测开发和界面连续显示，但必须同时满足：

- 可观测真值框为`null`；
- 估计框存放在独立字段中；
- 明确记录估计来源、置信等级和不确定范围；
- 不得将估计框命名为 ground truth；
- 不得将估计框计入普通 IoU、AUC、精确率或成功率统计；
- 不得以估计框证明算法在遮挡期间“准确看见”了目标。

### 2.2 目标身份优先于类别

类别相同不代表身份相同。遮挡前后的同一物理目标必须保持稳定`target_id`；其他同类别对象必须使用独立且稳定的`distractor_id`。不得用 detector 类别或置信度替代人工身份判断。

### 2.3 标注环境与推理环境解耦

数据集真值不得依赖某个算法版本才能解释。算法结果可以作为独立预标注来源导入，但未经人工确认不得写入可观测真值层；detector 候选不得直接变成真值框。

### 2.4 数据可追溯

每一项正式评测结果必须能够追溯到确定的视频文件、标注版本、数据划分和人工修订记录。文件名相同不能代替内容校验。

## 3. 强制程度

本文档使用以下强制等级：

- **必填**：缺失时，对应 episode 不得进入正式评测或阈值标定；
- **条件必填**：出现相应场景时必须填写；
- **推荐**：不会阻止基础验收，但缺失会限制专项分析能力；
- **可选**：用于扩展研究或问题定位。

## 4. 数据组织单位

### 4.1 视频

视频是未经改变时间顺序的源媒体文件。每个视频必须具有唯一`video_id`。

### 4.2 Episode

episode 表示一次独立的单目标跟踪任务。同一个视频可以建立多个 episode，例如：

- 分别跟踪两辆不同的车；
- 分别以完整人体和头部局部区域初始化；
- 对同一目标采用不同的合法初始帧。

每个 episode 只能有一个主跟踪目标和一种固定的`target_scope`。不得在运行中途把局部目标改成完整目标。

### 4.3 事件

事件是具有明确开始、结束和语义的连续帧区间，例如完全遮挡、出画重入、同类交叉和明显形变。

## 5. 视频级元数据

每个视频必须提供以下字段：

| 字段 | 强制性 | 要求 |
| --- | --- | --- |
| `video_id` | 必填 | 数据集内唯一且稳定 |
| `path` | 必填 | 相对数据集根目录的路径 |
| `sha256` | 必填 | 原始文件 SHA-256 |
| `byte_size` | 必填 | 文件字节数 |
| `width`、`height` | 必填 | 严格解码后的源分辨率 |
| `frame_count` | 必填 | 按实际解码帧计数 |
| `fps_num`、`fps_den` | 必填 | 精确帧率分数 |
| `time_base` | 必填 | 视频时间基 |
| `codec`、`pixel_format` | 必填 | 实际编码与像素格式 |
| `bit_depth` | 必填 | 实际位深 |
| `sensor` | 必填 | `visible`或`infrared` |
| `capture_session_id` | 必填 | 标识同一次连续采集，防止数据泄漏 |
| `edited` | 必填 | 是否剪辑、拼接、变速、插帧或循环 |
| `source_video_id` | 条件必填 | 转码、裁剪或派生视频指向原始视频 |
| `edit_description` | 条件必填 | 说明所有时间轴变化及对应帧位置 |

存在剪辑点、跳帧、循环拼接或时间轴重排的视频，只有在相关位置被显式标注并排除后，才能用于遮挡时长和重捕获延迟评测。

## 6. Episode 级真值

每个 episode 至少提供：

| 字段 | 强制性 | 说明 |
| --- | --- | --- |
| `episode_id` | 必填 | 数据集内唯一 |
| `video_id` | 必填 | 所属视频 |
| `target_id` | 必填 | 主物理目标的稳定身份 |
| `category` | 必填 | 如`person`、`car` |
| `target_scope` | 必填 | `full_object`或`subtarget` |
| `tracking_target_id` | 必填 | 实际被跟踪对象的稳定 ID |
| `carrier_target_id` | 条件必填 | 局部目标所属的完整物理目标 |
| `subtarget_type` | 条件必填 | 如`head`、`face`、`vehicle_part`、`arbitrary_region` |
| `start_frame`、`end_frame` | 必填 | episode 的闭区间 |
| `init_frame` | 必填 | 初始化帧 |
| `init_bbox_xywh` | 必填 | 初始化目标框 |
| `scenario_tags` | 必填 | 本 episode 包含的主要场景 |
| `evaluation_eligibility` | 必填 | 可用于哪些评测以及排除原因 |

初始化帧必须满足：

- 目标身份明确；
- 初始化框宽高均大于 0；
- 框位于源图坐标系；
- 目标范围定义明确，是完整目标还是某个局部子目标；
- 不得把 detector 输出直接作为未复核的初始化真值。

推荐在每个有效遮挡事件前保留不少于 1 秒可靠可见画面，在可靠重现后保留不少于 2 秒连续画面。上下文不足的事件可以用于诊断，但不得作为完整重捕获验收样本。

## 7. 坐标、帧号和时间

### 7.1 帧号

- `frame_id`按严格解码顺序从 0 连续编号；
- 不得使用容器包序号、程序日志行号或显示播放器编号代替；
- episode 范围内每一帧都必须有一条主目标状态记录。

### 7.2 时间

- 每帧记录`timestamp_us`；
- 优先使用可靠的解码显示时间戳；
- 若时间戳不可用，可根据精确帧率生成，但必须在 manifest 中声明生成方式。

### 7.3 边界框

- 格式统一为左上角原点的`[x, y, width, height]`；
- 可观测真值框使用源图坐标，宽高必须大于 0；
- 可观测真值框裁剪到图像范围内；
- 遮挡期估计框允许部分或全部位于图像外，以表达目标出画后的合理运动；
- 所有数值使用有限浮点数，不得出现 NaN 或无穷值。

## 8. 逐帧目标状态

`target_state`为逐帧必填枚举：

| 状态 | 定义 | 可观测真值框 |
| --- | --- | --- |
| `visible` | 目标身份明确，主体清晰可定位 | 必填 |
| `partial_occlusion` | 目标部分被遮挡，但身份和位置仍可可靠判断 | 条件必填 |
| `full_occlusion` | 人工判断目标仍在视场内，但没有可辨认目标像素 | 必须为`null` |
| `out_of_view` | 目标完全离开画面 | 必须为`null` |
| `ambiguous` | 身份、位置或是否仍在视场内无法可靠判断 | 必须为`null` |

`full_occlusion`和`out_of_view`必须区分。无法可靠区分时使用`ambiguous`，不得强行选择。

## 9. 可观测真值框

每帧使用以下字段描述可观测真值：

| 字段 | 强制性 | 说明 |
| --- | --- | --- |
| `target_bbox_xywh` | 可定位时必填 | 跟踪目标的完整范围；完全遮挡、出画或身份不确定时为`null` |
| `visible_bbox_xywh` | 部分遮挡时必填 | 仅包围实际可见像素 |
| `carrier_bbox_xywh` | 局部模式条件必填 | 完整物理目标可可靠定位时填写 |
| `bbox_valid` | 必填 | `target_bbox_xywh`是否可作为可观测真值 |
| `bbox_source` | 必填 | `manual`、`interpolated_reviewed`或`not_available` |
| `visibility_ratio` | 必填 | 目标可见比例，范围 0～1 |
| `truncation_ratio` | 必填 | 因画面边界不可见的比例，范围 0～1 |
| `identity_status` | 必填 | `certain`或`ambiguous` |
| `annotation_quality` | 必填 | `reliable`、`usable_with_caution`或`unusable` |

规则：

1. `target_bbox_xywh`表示当前跟踪目标范围，`visible_bbox_xywh`只表示可见像素范围，两者不得混用。
2. 短、连续、身份明确的可见区间允许插值，但必须逐帧人工复核。
3. 不得跨越`full_occlusion`、`out_of_view`或`ambiguous`区间插值可观测真值框。
4. 完全遮挡时`visibility_ratio=0`；完全出画时`truncation_ratio=1`。
5. 画面中出现同类别对象但主目标身份无法判断时，必须使用`ambiguous`，不能把该对象标成主目标。

## 10. 遮挡期与出画期辅助估计

### 10.1 适用目的

辅助估计用于：

- 训练或调试运动预测模型；
- 研究遮挡期间的轨迹延续；
- 为人机界面提供带明确提示的连续预测框；
- 在目标重现时计算预测终点偏差和轨迹连续性。

辅助估计不是可观测真值。

### 10.2 字段要求

在`full_occlusion`、`out_of_view`或部分`ambiguous`帧，可以提供：

| 字段 | 强制性 | 说明 |
| --- | --- | --- |
| `estimated_bbox_xywh` | 推荐 | 推测的完整目标位置，可在画面外 |
| `estimate_available` | 必填 | 当前帧是否提供估计框 |
| `estimate_method` | 有估计时必填 | 估计方法枚举 |
| `estimate_confidence` | 有估计时必填 | `high`、`medium`或`low` |
| `estimate_uncertainty_px` | 有估计时必填 | 对目标中心的像素不确定半径 |
| `estimate_scale_uncertainty_ratio` | 有估计时必填 | 宽高相对不确定比例 |
| `estimate_basis_frames` | 有估计时必填 | 估计所依据的前后关键帧 |
| `estimate_reviewer_note` | 推荐 | 运动假设、场景约束或争议说明 |

`estimate_method`至少支持：

- `forward_motion`：仅依据遮挡前运动前向外推；
- `bidirectional_interpolation`：依据遮挡前后同一目标位置双向插值；
- `scene_constrained`：结合道路、通道、遮挡物等场景结构估计；
- `manual_judgement`：标注人员综合判断；
- `not_estimated`：无法给出有意义估计。

### 10.3 使用限制

- 使用遮挡后画面进行的`bidirectional_interpolation`属于离线辅助估计，不能用来声称在线算法具备同等预测能力；
- 用于训练运动模型时，必须保留`estimate_method`，支持按来源过滤；
- 用于算法评测时，普通跟踪指标只使用可观测真值；
- 若研究估计轨迹的一致性，必须单独报告并明确其参照也是人工估计而非观测事实；
- 目标重现后的首个可靠可观测框可作为预测终点误差的客观评价依据。

## 11. 遮挡、出画与重现事件

每次完全遮挡或出画重入必须建立事件记录，至少包含：

| 字段 | 强制性 | 说明 |
| --- | --- | --- |
| `event_id` | 必填 | 数据集内唯一 |
| `episode_id` | 必填 | 所属 episode |
| `type` | 必填 | `full_occlusion`或`out_of_view_reentry`等 |
| `first_partial_frame` | 条件必填 | 首次进入部分遮挡 |
| `last_reliable_pre_event_frame` | 必填 | 事件前最后一个可靠可观测帧 |
| `first_unobservable_frame` | 必填 | 首个无可观测位置真值的帧 |
| `last_unobservable_frame` | 必填 | 最后一个无可观测位置真值的帧 |
| `first_visible_frame` | 必填 | 重现后首次出现可辨认像素 |
| `first_reliable_reappearance_frame` | 必填 | 首个身份与框均可靠的重现帧 |
| `stable_reappearance_frame` | 必填 | 从该帧起至少连续 3 帧可靠 |
| `identity_ambiguous_after_reappearance` | 必填 | 重现后是否存在身份争议 |
| `valid_for_recovery_evaluation` | 必填 | 是否可用于正式恢复评测 |
| `invalid_reason` | 条件必填 | 不可用于正式评测时说明原因 |

`first_visible_frame`不等于可靠重现帧。重捕获延迟应从`first_reliable_reappearance_frame`开始计算，不能从 detector 首次检出或单帧闪现开始计算。

以下情况必须设置`valid_for_recovery_evaluation=false`：

- 事件期间存在未说明的剪辑、拼接、循环或跳帧；
- 遮挡前后不能确认是同一物理目标；
- 重现后帧数不足，无法判断是否稳定；
- 人工无法给出可靠重现边界；
- 视频带有遮挡目标的外部跟踪框或其他无法去除的提示，足以影响标注或算法。

## 12. 局部子目标

当初始化框是脸、头部、车体局部或任意子区域时，必须额外记录：

- `subtarget_type`；
- `carrier_target_id`；
- 初始化局部框相对完整目标的归一化中心和宽高比例；
- `subtarget_observable`：当前姿态下该局部区域是否仍有明确物理含义；
- `subtarget_identity_stable`：局部区域在形变后是否仍能视为同一跟踪对象；
- 完整目标可可靠定位时的`carrier_bbox_xywh`。

局部区域完全不可辨认但完整物理目标仍可见时：

- 局部目标的`target_bbox_xywh`必须为`null`；
- 可以保留完整目标的`carrier_bbox_xywh`；
- 不得用完整目标框替换局部目标真值框；
- 如需估计局部区域位置，只能写入`estimated_bbox_xywh`。

## 13. 形变标注

逐帧`deformation_tags`至少支持：

- `scale`
- `aspect_ratio`
- `viewpoint`
- `rotation`
- `articulation`
- `illumination`
- `thermal_contrast`
- `motion_blur`
- `none`

每个非`none`标签记录`severity`：`low`、`medium`或`high`。持续明显形变必须建立事件区间，并记录：

- 开始帧和结束帧；
- 形变前最后可靠帧；
- 形变后首个稳定帧；
- 目标身份是否始终明确；
- 局部子目标是否仍可观测。

## 14. 同类别和高相似干扰物

在事件前至少 1 秒至可靠重现后至少 2 秒的关键窗口内，所有可能进入主目标搜索区域并造成身份切换的同类别或高相似对象必须标注：

| 字段 | 强制性 | 说明 |
| --- | --- | --- |
| `distractor_id` | 条件必填 | 全 episode 稳定身份 |
| `category` | 条件必填 | 干扰物类别 |
| `bbox_xywh` | 可观测时条件必填 | 干扰物可观测框 |
| `same_class` | 条件必填 | 是否与主目标同类别 |
| `appearance_similarity` | 推荐 | `low`、`medium`或`high` |
| `overlaps_target` | 条件必填 | 是否与主目标可见区域重叠 |
| `identity_ambiguous_with_target` | 条件必填 | 是否造成人工身份争议 |

同时建立出现、离开、靠近、交叉、重叠和分离事件。无需标注全画面所有无关对象，但不得遗漏可能成为 detector 重捕候选或造成 NanoTrack 身份切换的对象。

## 15. 逐帧记录示例

### 15.1 正常可见

```json
{"frame_id":120,"timestamp_us":4800000,"target_state":"visible","target_bbox_xywh":[238.0,154.0,78.0,74.0],"visible_bbox_xywh":[238.0,154.0,78.0,74.0],"bbox_valid":true,"bbox_source":"manual","visibility_ratio":1.0,"truncation_ratio":0.0,"identity_status":"certain","annotation_quality":"reliable","estimate_available":false,"estimated_bbox_xywh":null,"deformation_tags":[{"type":"none","severity":"low"}],"distractors":[]}
```

### 15.2 部分遮挡

```json
{"frame_id":245,"timestamp_us":9800000,"target_state":"partial_occlusion","target_bbox_xywh":[250.0,158.0,80.0,76.0],"visible_bbox_xywh":[274.0,158.0,56.0,70.0],"bbox_valid":true,"bbox_source":"manual","visibility_ratio":0.58,"truncation_ratio":0.0,"identity_status":"certain","annotation_quality":"usable_with_caution","estimate_available":false,"estimated_bbox_xywh":null,"deformation_tags":[{"type":"articulation","severity":"medium"}],"distractors":[{"distractor_id":"person_right","category":"person","bbox_xywh":[330.0,128.0,142.0,318.0],"same_class":true,"appearance_similarity":"high","overlaps_target":true,"identity_ambiguous_with_target":false}]}
```

### 15.3 完全遮挡并提供辅助估计

```json
{"frame_id":260,"timestamp_us":10400000,"target_state":"full_occlusion","target_bbox_xywh":null,"visible_bbox_xywh":null,"bbox_valid":false,"bbox_source":"not_available","visibility_ratio":0.0,"truncation_ratio":0.0,"identity_status":"certain","annotation_quality":"unusable","estimate_available":true,"estimated_bbox_xywh":[268.0,161.0,81.0,77.0],"estimate_method":"bidirectional_interpolation","estimate_confidence":"medium","estimate_uncertainty_px":18.0,"estimate_scale_uncertainty_ratio":0.15,"estimate_basis_frames":[243,286],"estimate_reviewer_note":"依据遮挡前后同一目标位置和近似匀速运动估计；不属于可观测真值","deformation_tags":[{"type":"none","severity":"low"}],"distractors":[{"distractor_id":"person_right","category":"person","bbox_xywh":[326.0,126.0,145.0,320.0],"same_class":true,"appearance_similarity":"high","overlaps_target":false,"identity_ambiguous_with_target":false}]}
```

### 15.4 完全出画并估计到图像外

```json
{"frame_id":410,"timestamp_us":16400000,"target_state":"out_of_view","target_bbox_xywh":null,"visible_bbox_xywh":null,"bbox_valid":false,"bbox_source":"not_available","visibility_ratio":0.0,"truncation_ratio":1.0,"identity_status":"certain","annotation_quality":"unusable","estimate_available":true,"estimated_bbox_xywh":[1938.0,366.0,128.0,92.0],"estimate_method":"forward_motion","estimate_confidence":"low","estimate_uncertainty_px":65.0,"estimate_scale_uncertainty_ratio":0.30,"estimate_basis_frames":[397,398,399],"estimate_reviewer_note":"仅依据出画前速度外推，框中心已位于画面外","deformation_tags":[{"type":"none","severity":"low"}],"distractors":[]}
```

## 16. 推荐交付结构

```text
long-term-tracking-ground-truth-v1/
├── README.md
├── dataset-manifest.json
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

所有 JSON/JSONL 顶层记录必须包含独立的`ground_truth_schema_version`。该版本只描述数据集真值合同，不得复用算法运行日志的`schema_version`。

## 17. 数据划分要求

- `calibration`：用于阈值搜索和参数拟合；
- `development`：用于算法迭代和误差分析；
- `locked-test`：冻结后只用于最终评测，不得参与阈值选择。

同一原始视频、同一连续采集、同一物理目标及其裁剪/转码版本不得跨数据划分。完整目标与从其派生的局部子目标 episode 必须位于同一划分。可见光与红外分别分层统计，不得用一种模态的结果替代另一种模态。

## 18. 自动验收规则

交付前必须自动检查：

1. 视频可严格解码，文件大小和 SHA-256 匹配；
2. 解码分辨率、帧数、帧率和 manifest 一致；
3. episode 范围内`frame_id`从 0 基准连续映射，无重复或缺失；
4. 所有可观测真值框数值有限、宽高为正且裁剪在源图内；
5. `full_occlusion`、`out_of_view`和`ambiguous`帧的`target_bbox_xywh`为`null`；
6. `estimated_bbox_xywh`只出现在辅助估计字段，不覆盖可观测真值字段；
7. 有估计框时，估计方法、置信等级、不确定范围和依据帧完整；
8. 状态、可见比例、出画比例和框有效性相互一致；
9. 所有事件边界顺序合法且位于 episode 内；
10. 所有 target、carrier 和 distractor 引用的 ID 存在且稳定；
11. 所有数据划分互斥，且不存在同源视频或同一物理目标泄漏；
12. 所有标注、划分和清单文件均进入`checksums.sha256`。

## 19. 人工质量复核

以下内容必须逐帧复核：

- 完全遮挡开始和结束边界；
- 出画和重新入画边界；
- 首次可靠重现和稳定重现边界；
- 同类目标交叉、重叠和分离区间；
- 身份存在争议的帧；
- 所有遮挡期辅助估计的起止点和依据帧；
- 所有从预标注复制到真值层的框。

每次修改必须保留标注版本、标注人、复核人、时间和修改原因。无法达成可靠判断的帧必须保持`ambiguous`，不得为追求数据完整性而强行给出可观测真值框。

## 20. 支持的后续算法评价

符合本文档的数据可支持以下评价，但具体指标阈值由后续算法评测与阈值标定方案另行规定：

### 20.1 可观测阶段

- 成功率、精确率、AUC；
- 中心误差和尺度误差；
- 形变前后稳定性；
- 完整目标与局部子目标分层结果。

### 20.2 遮挡恢复

- 重捕获成功率；
- 从`first_reliable_reappearance_frame`开始计算的重捕获延迟；
- 错误身份重捕率；
- 身份切换次数；
- 恢复后持续稳定帧数。

### 20.3 遮挡期预测

- 遮挡结束或目标重现时的预测中心误差；
- 重现时预测尺度误差；
- 预测不确定范围对真实重现位置的覆盖率；
- 遮挡时长与预测误差的关系；
- 前向在线预测与离线双向人工估计的差异。

遮挡期间的估计框不得混入普通可观测跟踪指标。若产品界面选择显示连续预测框，应明确标识为预测状态；是否允许预测结果参与设备控制属于算法和系统安全设计，不属于数据集真值。

## 21. 数据集验收结论

数据交付后应形成一份验收报告，至少给出：

- 视频与标注文件数量、大小和哈希检查结果；
- 各状态帧数和有效事件数量；
- 完整目标、局部目标、传感器类型和目标类别分布；
- 遮挡时长、出画时长、形变类型和同类干扰数量分布；
- 提供辅助估计的帧数、估计方法和置信等级分布；
- 自动规则失败项及处理结果；
- 人工争议帧、排除 episode 和排除原因；
- calibration、development 和 locked-test 的同源泄漏检查结果。

只有通过自动检查、关键事件人工复核且数据划分无泄漏的 episode，才能进入正式阈值标定和算法评测。
