# VisDrone9 YOLOv8n RK3566 Detector 重训练与替换设计规格

日期：2026-10-15

## 1. 背景

`rk3566/nanotrack_v3` 当前使用 Rockchip 官方优化结构的 COCO 80 类
YOLOv8n FP16 RKNN 模型。模型通过 `YoloDetectorAdapter` 为长时控制器提供全图候选，候选仍需
经过 NanoTrack anchor/recent 模板验证和时序确认，不能直接产生
`control_valid=true`。

当前任务使用 `/home/u20/dataset/VisDrone2019-DET` 重训练一个面向航拍目标的
YOLOv8n，将 detector 从 COCO 80 类替换为 VisDrone 9 类。首要目标是建立可复现的训练、
导出、RKNN 转换和板端替换链路；本阶段不以最高检测精度、INT8 量化或板端 FPS 达标作为
阻塞条件。

## 2. 目标与非目标

### 2.1 目标

- 将 VisDrone 原始标注确定性转换为 9 类 YOLO 检测数据。
- 以预训练 `yolov8n.pt` 为起点，在 640x640 输入下完成 5 epoch 冒烟训练和
  100 epoch 正式训练。
- 使用 Rockchip 官方 YOLOv8 分支导出优化 ONNX，保持现有三尺度九输出契约。
- 使用 RKNN-Toolkit2 2.3.2 生成 `target_platform=rk3566` 的 FP16 RKNN。
- 生成标签、manifest、哈希、训练配置、评估报告和板端验证证据。
- 新模型先并行部署，验收通过后再切换正式启动配置；旧模型可立即回滚。

### 2.2 非目标

- 本阶段不做 INT8 量化、量化校准或 NPU 性能优化。
- 不承诺一次训练达到产品精度，不进行 YOLOv8n/s/m 或多分辨率大规模搜索。
- 不改变长时控制器状态机、候选身份验证和 `control_valid` 安全边界。
- 不把数据集、虚拟环境、训练缓存、checkpoint、ONNX、RKNN 或运行视频加入 Git。
- 不使用 `ancient-tracker` 中的工程、二进制或实现代码。

## 3. 技术路线选择

采用 Rockchip 官方 `airockchip/ultralytics_yolov8` 仓库，同时完成训练和优化 ONNX
导出。设计锁定的仓库 HEAD 为：

```text
repository: https://github.com/airockchip/ultralytics_yolov8
commit: 4674fe6e003dfbc5f2250d3b39dd31faaf7a9877
```

Rockchip 说明其优化只作用于导出：移除不适合 NPU 的模型内后处理和 DFL，并新增
score-sum 分支；训练仍按原 YOLOv8 流程执行。统一使用同一固定版本可避免新版 Ultralytics
checkpoint 与旧版 Rockchip exporter 之间的隐式结构不兼容。

不采用以下方案：

- 最新 Ultralytics 训练后直接交给旧版 Rockchip exporter：需要额外证明 checkpoint
  兼容性。
- 导出标准单输出 ONNX 后修改 C++：会扩大运行时代码和测试范围，并破坏当前已验证的
  detector 张量契约。
- ONNX 图后处理脚本临时拆图：对节点名和 exporter 版本敏感，可维护性差。

相关官方资料：

- <https://github.com/airockchip/ultralytics_yolov8/blob/main/RKOPT_README.zh-CN.md>
- <https://github.com/airockchip/rknn_model_zoo/blob/main/examples/yolov8/README.md>

## 4. 数据集和类别契约

### 4.1 输入数据

原始数据位置：

```text
/home/u20/dataset/VisDrone2019-DET
```

已观察到的固定划分为：

- train：6471 张图片和 6471 个标注文件；
- val：548 张图片和 548 个标注文件。

原始目录只读使用。转换数据输出到仓库外或 Git 忽略目录，不修改原始图片或标注。训练集和
验证集严格沿用原始划分，不从 val 抽样回 train，不使用测试集调参。

### 4.2 类别映射

| VisDrone 原始 ID | 原始类别 | 新 ID | 新类别 |
|---:|---|---:|---|
| 1 | pedestrian | 0 | person |
| 2 | people | 0 | person |
| 3 | bicycle | 1 | bicycle |
| 4 | car | 2 | car |
| 5 | van | 3 | van |
| 6 | truck | 4 | truck |
| 7 | tricycle | 5 | tricycle |
| 8 | awning-tricycle | 6 | awning-tricycle |
| 9 | bus | 7 | bus |
| 10 | motor | 8 | motor |

原始 ID 0 `ignored regions` 和 ID 11 `others` 不参与训练。该映射保留当前调用约定中的
`person=0` 和 `car=2`。

标签文件顺序固定为：

```text
person
bicycle
car
van
truck
tricycle
awning-tricycle
bus
motor
```

### 4.3 转换规则

每行 VisDrone 标注按照 `x,y,width,height,score,category,truncation,occlusion` 解析：

- 跳过 category 0、category 11、`score <= 0`、非有限数值、非正宽高记录；
- 将框裁剪到图像有效范围，裁剪后无面积则跳过；
- 不按目标尺寸、遮挡程度或截断程度过滤；
- 转换为 YOLO 归一化 `class cx cy width height`；
- 归一化坐标必须有限，中心和宽高必须处于合法范围；
- 每张原图必须有对应标签文件，允许合法的空标签文件。

转换过程必须输出 JSON 审计报告，至少包含：split 图片/标签数量、每类转换前后实例数、
过滤原因计数、裁剪框数量、空标签数量、缺失文件、重复文件名和异常样本路径。转换完成后
运行第二个只读校验步骤重新扫描全部生成标签，不能只相信转换脚本退出码。

## 5. 环境与可复现性

建立独立训练环境，例如 `.venvs/yolov8-visdrone`。虚拟环境不加入 Git。安装版本必须从
Rockchip 仓库的 requirements 起步，并额外记录以下实际值：

- Python、PyTorch、CUDA runtime、cuDNN；
- GPU 型号、驱动版本和训练开始时显存状态；
- Rockchip YOLOv8 完整 commit；
- 初始 `yolov8n.pt` URL、大小和 SHA-256；
- 完整训练参数、随机种子、命令和数据 YAML SHA-256。

当前宿主机已确认可运行 `nvidia-smi`，训练命令必须在能够看到 NVIDIA 设备的宿主机环境中
执行。开始训练前还要以 PyTorch 同时验证 `torch.cuda.is_available()`、设备名和一次 CUDA
tensor 运算；仅有 `nvidia-smi` 成功不算训练框架可用。

## 6. 训练设计

### 6.1 固定基线

```text
model: yolov8n.pt
imgsz: 640
epochs: 100
batch: 8
patience: 20
seed: 固定并记录
amp: true
task: detect
```

其余增强和优化器参数首轮使用锁定 Rockchip 分支的默认值，完整展开保存到训练结果中，避免
只记录命令行覆盖项。不得根据 val 单类表现反复修改超参数后仍将同一 val 结果描述为无偏
最终评估。

### 6.2 两阶段执行

第一阶段在完整 train/val 链路运行 5 epoch 冒烟训练，检查：

- 数据可加载且不存在持续损坏样本；
- CUDA、AMP、反向传播和 checkpoint 保存正常；
- train loss 有限，验证流程能产生逐类指标；
- 中断后 checkpoint 可由同版本代码重新加载。

冒烟训练结果仅证明管线可工作，不作为最终模型。正式训练从相同预训练权重重新开始，执行
最多 100 epoch；early stopping 由 `patience=20` 控制。按照验证集 mAP50-95 选择
`best.pt`，同时保留 `last.pt` 供故障诊断。

### 6.3 训练报告

报告至少包含：

- 总体 precision、recall、mAP50、mAP50-95；
- 九类各自 precision、recall、AP50、AP50-95；
- confusion matrix、PR 曲线和 loss 曲线；
- 最佳 epoch、实际训练 epoch、总时长、峰值 GPU 显存；
- 失败样本抽查，特别是小目标、密集同类、遮挡和截断场景。

## 7. 优化 ONNX 导出

使用同一固定 Rockchip 仓库加载 `best.pt`，按照其 RKOPT exporter 导出固定 batch、固定
640x640 的 ONNX。导出后首先运行 ONNX checker，并验证：

- 输入语义为 RGB、NCHW、FP32，形状 `[1,3,640,640]`；
- 输出严格为 9 个；
- stride 8/16/32 各有 box、class、score-sum；
- box channel 为 64，class channel 为 9，score-sum channel 为 1；
- 网格分别为 80x80、40x40、20x20；
- 所有抽样输出有限。

使用固定 VisDrone val 样本清单比较 PT 与优化 ONNX：执行相同 letterbox、置信度过滤、DFL
解码和 NMS，记录类别一致率、框 IoU、置信度差和候选数量差。容差和样本清单必须先固定再
运行，不能只凭输出图目视判断导出成功。

## 8. RKNN 转换与模型清单

使用仓库现有 `.venvs/rknn-convert` 和 RKNN-Toolkit2 2.3.2，将优化 ONNX 转换为：

```text
target_platform: rk3566
do_quantization: false
mean_values: [[0, 0, 0]]
std_values: [[255, 255, 255]]
```

继续使用 `rk3566/nanotrack_v3/tools/convert_yolov8_rk3566.py` 的 manifest 和哈希校验能力，
但在实现阶段要补充测试，证明 9 类输出形状由标签数量正确生成。新 manifest 必须记录：

- PT、ONNX、RKNN 文件名、大小和 SHA-256；
- 训练仓库和 commit；
- 输入尺寸、色彩顺序、letterbox 填充值；
- 九个输出的角色和形状；
- `class_count=9`、`dfl_bins=16`；
- `target_platform=rk3566`、`quantized=false`；
- `calibrated=false` 和板端验证状态。

## 9. 产物和 Git 边界

Git 可跟踪：

- 数据转换、审计、导出和验收脚本；
- 数据 YAML、九类标签、manifest 或 manifest 模板；
- 固定依赖和训练配置；
- 设计、执行计划、训练报告和板端验收报告。

Git 不跟踪：

- `/home/u20/dataset` 下原始数据；
- 转换后的图片/标签副本和缓存；
- `.venvs`、第三方仓库 checkout；
- `*.pt`、`*.onnx`、`*.rknn`、TensorBoard/W&B 目录、视频和批量预测图。

模型产物保存在明确的本地 artifacts 目录并通过 SHA-256 与 manifest 绑定。不得为了方便提交
而把大文件强制加入 Git。

## 10. 部署、验收和回滚

### 10.1 分阶段替换

新模型使用独立版本名，例如：

```text
yolov8n-visdrone9-rk3566-fp16-v1
```

在开发板建立新发布目录并保留当前 COCO 模型目录。先用显式 CLI 参数指向新模型，不覆盖
旧文件、不更改默认启动入口。全部验收通过后才切换正式配置。回滚只需恢复旧模型目录和
参数，不依赖重新编译。

### 10.2 四层验收

1. **PyTorch 层**：正式训练报告和九类逐类指标完整，`best.pt` 可加载并完成 val 推理。
2. **ONNX 层**：九输出契约通过，固定样本与 PT 的结果差异在预先规定容差内。
3. **RKNN detector 层**：开发板加载、张量检查和多张静态图检测通过；记录逐张预处理、
   推理、后处理和总延迟以及进程峰值内存。
4. **长时跟踪层**：视频运行中 detector 请求、结果、新鲜度检查和候选队列均正常；至少能
   观察到候选进入 NanoTrack 验证。未通过模板和时序确认的候选始终保持
   `control_valid=false`。

本阶段不设最低 FPS 或最低 mAP 硬门槛，但出现以下任一情况不得替换正式模型：模型/标签/
manifest 哈希不一致、输出契约错误、板端推理失败、持续非有限输出、长时控制器安全边界
被绕过，或运行过程出现无界内存增长。

### 10.3 结果表述边界

- 主机 PT/ONNX 成功不能表述为 RK3566 部署成功。
- 单张板端检测成功不能表述为长时找回成功。
- detector 给出正确框不能表述为目标身份已确认。
- 当前验证集指标只能说明 VisDrone val 表现，不能直接代表用户真实光电视频效果。

## 11. 实现前待办

设计批准后编写执行计划，计划必须把工作拆分为：

1. Git 忽略边界与测试；
2. 数据转换器和审计器；
3. 独立训练环境与依赖锁定；
4. 5 epoch 冒烟训练；
5. 100 epoch 正式训练；
6. 优化 ONNX 导出与 PT/ONNX 一致性验证；
7. RKNN 转换、manifest 和主机静态校验；
8. 开发板 detector 验证；
9. 长时跟踪集成验收、报告和正式切换。

正式训练属于长时间 GPU 任务。任何中断都应从受验证的 checkpoint 恢复，不能把不完整
run 的局部指标当作最终结果。
