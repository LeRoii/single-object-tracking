# occ_video_1 第二目标 271 帧草案真值设计

## 意图

为 `occ_video_1/1.mp4` 中用户在 RK3588 交互运行时选取的第二辆车建立一个独立
episode 草案，完整覆盖 frame 0--270，用于复现“稳定尺寸遮挡、NanoTrack 候选恢复、
同类车辆误重捕获”问题。草案不能把 NanoTrack、detector 或运动预测输出直接称为真值；
用户确认前不得进入正式 IoU、阈值标定或公共集成契约。

## 已确认输入

- 源视频固定为
  `/home/u20/single-object-tracking/data/occ_video_1/1.mp4`，SHA-256
  `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`，
  1920x1080、30 FPS、271 帧、frame id 从 0 开始。
- 第二目标初始化来自用户本次交互选择：frame 0 像素框
  `(x=967, y=513, width=42, height=42)`；暂定 episode/target id 为
  `occ_video_1_car_002_full_object` / `car_002`。
- 诊断输入绑定本次运行日志、CSV、metadata 和 271 帧输出视频的已记录 SHA-256；
  它们只能用于候选生成、身份追踪和问题定位，不能成为真值来源。
- 用户选择完整覆盖 frame 0--270，并选择半自动双向标注；不确定部分留给用户确认。

## 方案比较与选择

1. **半自动双向候选 + 人工边界复核（已选）**：人工设置可靠关键帧，在原始视频上
   前向和反向跟踪；只把前后结果一致且视觉身份连续的框列为候选，不一致区间进入人工
   清单。工作量可控，并能显式隔离遮挡估计。
2. **逐帧纯人工框选（未选）**：可见帧精度上限较高，但 271 帧成本较大；完全遮挡帧
   仍然只能估计，不能消除真值边界问题。
3. **直接采用本次运行 CSV（排除）**：搜索期 CSV 主框是运动预测，frame 262 后已经
   提交错误同类目标，不满足算法输出与真值分层原则。

## 生成方法

1. 严格解码并核对视频身份、分辨率、帧率和帧数。
2. 以用户 frame 0 初始化框为首个候选锚点；在可见区间增加稀疏人工关键帧，关键帧
   选择以稳定身份和完整目标边界为准，不从 detector 类别推断身份。
3. 在相邻关键帧间分别执行前向和反向跟踪。两方向框达到约定的一致性门时形成
   `bidirectional_candidate`；不一致、边界不清或身份竞争帧形成
   `needs_human_review`，不自动晋升。
4. 遮挡边界以原始无叠加视频逐帧判断。完全遮挡或身份不可判定帧的
   `target_bbox_xywh=null`、`bbox_valid=false`；如轨迹可合理估计，只在
   `estimated_bbox_xywh` 中提供 `bidirectional_interpolation`，并记录依据帧、中心
   不确定半径和尺度不确定度。
5. 对问题窗口至少标记会进入搜索区域的同类干扰车辆，特别是本次约 frame 259--264
   造成错误恢复的候选。干扰物 ID 使用 episode 内稳定的
   `car_002_distractor_NNN`，逐帧记录其 `bbox_xywh`、`same_class`、
   `appearance_similarity`、`overlaps_target`、`identity_ambiguous_with_target` 和复核状态；
   干扰物只用于身份复核，不进入主目标 YOLO 标签。
6. 生成全 271 帧复核拼图和待确认清单。用户确认的修订独立记录，不能静默覆盖候选
   来源或原始框。

## 状态与评测边界

逐帧主记录使用独立 schema。`target_state` 与 `review_status` 是正交字段，不得用一个
字段同时表达可见性和复核结论。`target_state` 至少支持 `visible`、
`partial_occlusion`、`full_occlusion`、`ambiguous`、`out_of_view`；
`review_status` 至少区分：

- `manual_confirmed_visible`：用户确认的可观测完整目标框，可进入普通 IoU；
- `assistant_visual_candidate`：经双向与视觉连续性检查的候选，用户确认前不可进入正式
  指标；
- `needs_human_review`：身份、边界或双向结果存在争议，可带候选框但不得计分；
- `estimated_non_observable`：完全遮挡/不可判定，仅有辅助估计，永不进入普通 IoU。

逐帧 JSONL 中，正式 `target_bbox_xywh` 只允许存放人工确认的可观测真值；半自动结果
写入独立 `candidate_bbox_xywh`，完全遮挡辅助估计写入独立
`estimated_bbox_xywh`。三者不可互相回填。

YOLO 兼容层同样严格分层，并各自保留 frame 0--270 共 271 个文件：

- `candidate_labels/frame_NNNNNN.txt` 保存半自动候选，供人工复核和草案调试；
- `labels/frame_NNNNNN.txt` 只保存用户已确认的可观测真值，未确认/不可观测帧为空；
- `estimated_labels/frame_NNNNNN.txt` 只保存明确标记的遮挡辅助估计。

非空文件使用 `class_id=0 normalized_cx normalized_cy normalized_w normalized_h`。正式
evaluator 只允许读取 `labels/`，且必须拒绝 metadata 中仍有未处理
`assistant_visual_candidate` 或 `needs_human_review` 的草案版本。

## 草案交付结构

生成物先放在本工作树的非公共草案目录：

```text
artifacts/ground_truth/occ_video_1_car_002_draft/
├── README.md
├── annotation_metadata.json
├── annotation_status.csv
├── frames.jsonl
├── events.json
├── candidate_labels/
├── labels/
├── estimated_labels/
├── candidate_labels.sha256
├── labels.sha256
├── checksums.sha256
├── review_manifest.csv
└── review_contact_sheets/
```

视频不复制，metadata 通过绝对读取来源和 SHA-256 绑定。该目录不是现有 `car_001`
数据的替换，也不修改 `docs/progress/integration-contract.yaml`。

## 自动检查与人工交接

- frame 0--270 连续、无重复缺失；视频严格解码身份匹配；所有数值有限且框在图内；
- `full_occlusion`/`ambiguous` 帧的可观测框必须为空；估计字段及依据完整；
- 三个 YOLO 层各有 271 个连续命名文件；非空文件为单行五字段、归一化范围合法，
  各自 manifest 逐项校验通过；`target_bbox_xywh`、`candidate_bbox_xywh` 和
  `estimated_bbox_xywh` 与各自目录逐帧一致且不跨层；
- 每个干扰物 ID 稳定，引用字段完整；`target_state` 与 `review_status` 均取合法枚举且
  与可见比例、真值框有效性一致；
- 输出各状态数量、待确认连续区间和关键帧拼图；不计算或宣称算法准确率；
- 用户确认后才产生新版本和正式哈希。是否将其纳入 canonical dataset 由协调者另行
  决定，本任务不修改公共契约或现有标签。

## 后续算法设计关联

用户已选择：进入未来的 `SUSPECTED` 状态后先继续使用 NanoTrack 观测框，不立即切换
运动预测框。该控制器变更是独立后续任务，本标注草案只提供可复现评测输入，不同时
修改控制器。
