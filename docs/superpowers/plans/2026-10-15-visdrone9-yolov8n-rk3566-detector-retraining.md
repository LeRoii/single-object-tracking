# VisDrone9 YOLOv8n RK3566 Detector Retraining Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 RTX 2060 主机上用 VisDrone2019-DET 训练 9 类 YOLOv8n，导出与现有 C++ 九输出契约兼容的 RK3566 FP16 RKNN，并在开发板完成 detector 与长时跟踪集成验收后安全切换。

**Architecture:** 原始 VisDrone 数据保持只读，由独立转换器生成被 Git 忽略的 YOLO 数据视图和可审计统计；训练与 RKOPT ONNX 导出固定使用 Rockchip YOLOv8 commit `4674fe6e003dfbc5f2250d3b39dd31faaf7a9877`。现有 RKNN converter 生成 9 类 manifest 和 FP16 模型，新模型部署到独立板端 release，候选仍经过 NanoTrack 模板验证与 2/3 时序确认。

**Tech Stack:** Python 3.10、pytest、PyTorch/CUDA、Rockchip `ultralytics_yolov8`、ONNX/ONNX Runtime、RKNN-Toolkit2 2.3.2、C++17、OpenCV 4.10、RK3566 RKNPU2。

**Spec:** `docs/superpowers/specs/2026-10-15-visdrone9-yolov8n-rk3566-detector-retraining.md`

## Global Constraints

- 原始数据只读：`/home/u20/dataset/VisDrone2019-DET`；train=6471，val=548，禁止验证集回流训练集。
- 类别固定为 `person,bicycle,car,van,truck,tricycle,awning-tricycle,bus,motor`；原始 1/2 合并为 0，原始 0/11 排除。
- 模型固定 YOLOv8n、640x640、batch 8、seed 42、AMP、正式训练最多 100 epoch、patience 20。
- Rockchip YOLOv8 固定 commit `4674fe6e003dfbc5f2250d3b39dd31faaf7a9877`；RKNN-Toolkit2 固定 2.3.2。
- RKNN 固定 `target_platform=rk3566`、FP16、RGB/NHWC uint8 输入、pad 114、三尺度九输出、`class_count=9`、`dfl_bins=16`。
- 数据集、虚拟环境、第三方 checkout、缓存、日志、媒体、`*.pt`、`*.onnx`、`*.rknn` 不得加入 Git。
- 不修改或复制 `ancient-tracker`；不覆盖现有板端 release `/home/cat/Project/SOT/nanotrack_v3_longterm/releases/rk3566-yolov8-detector`。
- detector 框只能作为 NanoTrack 搜索假设；未经模板验证和 2/3 确认不得设置 `control_valid=true`。
- 目前另一个会话修改的 `rk3566/nanotrack_v3/docs/detector-recapture-todo.md` 和 `rk3566/nanotrack_v3/docs/occlusion-video-annotation-requirements.zh-CN.md` 不纳入本计划提交。

## Review Focus

- 越界、负宽高、非有限和 `score<=0` 的 VisDrone 框必须过滤或裁剪并计入审计；Task 1 的参数化测试覆盖。
- 原始 1/2 合并和 0/11 排除不能造成类别偏移；Task 1 用包含全部 0..11 类别的夹具锁定映射。
- 源图片缺失、断开的图片符号链接或标签缺失必须使独立审计失败；Task 1 的只读审计测试覆盖。
- CUDA 对 `nvidia-smi` 可见但 PyTorch 不可用，或恢复 checkpoint 与数据/commit 不一致时必须拒绝训练；Task 3 测试覆盖。
- ONNX 输出顺序/形状、9 类 manifest、模型/标签哈希任一不一致必须在上板前失败；Task 6 和 Task 7 测试覆盖。

---

## File Structure

### 新增文件

- `rk3566/nanotrack_v3/tools/visdrone_dataset.py`：纯函数形式的 VisDrone 解析、转换和只读审计。
- `rk3566/nanotrack_v3/tools/convert_visdrone9.py`：数据转换 CLI，生成标签、图片符号链接和 JSON 报告。
- `rk3566/nanotrack_v3/tools/audit_visdrone9.py`：独立重新扫描生成数据的 CLI。
- `rk3566/nanotrack_v3/tests/test_visdrone_dataset.py`：数据边界、映射和审计测试。
- `rk3566/nanotrack_v3/models/visdrone9.txt`：运行时九类标签表。
- `rk3566/nanotrack_v3/training/visdrone9-data.yaml`：固定 YOLO 数据视图路径和类别名。
- `rk3566/nanotrack_v3/tools/train_visdrone9.py`：CUDA 探测、smoke/full 参数生成、元数据和恢复约束。
- `rk3566/nanotrack_v3/tests/test_train_visdrone9.py`：训练参数、CUDA 失败和恢复身份测试。
- `rk3566/nanotrack_v3/training/requirements-yolov8-visdrone.lock`：成功环境的精确依赖锁。
- `rk3566/nanotrack_v3/tools/rkopt_validation.py`：RKOPT ONNX 契约、解码和 PT/ONNX 匹配纯函数。
- `rk3566/nanotrack_v3/tools/verify_rkopt_onnx.py`：九输出 ONNX 静态验证 CLI。
- `rk3566/nanotrack_v3/tools/compare_pt_onnx.py`：固定样本 PT/ONNX 一致性报告 CLI。
- `rk3566/nanotrack_v3/tests/test_rkopt_validation.py`：输出顺序、形状、DFL 解码和匹配测试。
- `rk3566/nanotrack_v3/docs/visdrone9-detector-training.md`：环境、数据审计、训练和导出证据。
- `rk3566/nanotrack_v3/docs/visdrone9-detector-board-verification.md`：板端 detector/长时跟踪验收证据。

### 修改文件

- `.gitignore`：仅在忽略测试发现缺口时补充新的训练缓存或报告目录规则。
- `rk3566/nanotrack_v3/tools/convert_yolov8_rk3566.py`：为新 manifest 写入训练 checkpoint/config 身份。
- `rk3566/nanotrack_v3/tools/verify_detector_manifest.py`：验证可选但成组出现的训练 provenance。
- `rk3566/nanotrack_v3/tests/test_detector_manifest.py`：锁定九类输出和训练 provenance。
- `rk3566/nanotrack_v3/models/detector-manifest.json`：仅在板端验收通过后切换为新模型身份。
- `rk3566/nanotrack_v3/README.md`：仅在正式切换后更新模型名、标签和类别示例。

### Git 忽略产物

- `artifacts/yolov8-visdrone9/dataset/`：YOLO 标签和原图符号链接。
- `artifacts/yolov8-visdrone9/runs/`：训练输出、checkpoint 和图表。
- `artifacts/yolov8-visdrone9/export/`：PT、ONNX、RKNN 和一致性报告原始 JSON。
- `.tools/ultralytics_yolov8/`：固定 commit 的 Rockchip checkout。
- `.venvs/yolov8-visdrone/`：训练环境。

### Task 1: VisDrone9 数据转换与独立审计

**Files:**
- Create: `rk3566/nanotrack_v3/tools/visdrone_dataset.py`
- Create: `rk3566/nanotrack_v3/tools/convert_visdrone9.py`
- Create: `rk3566/nanotrack_v3/tools/audit_visdrone9.py`
- Create: `rk3566/nanotrack_v3/tests/test_visdrone_dataset.py`
- Create: `rk3566/nanotrack_v3/models/visdrone9.txt`
- Create: `rk3566/nanotrack_v3/training/visdrone9-data.yaml`
- Modify: `.gitignore` only if an ignore assertion fails

**Interfaces:**
- Consumes: VisDrone split directories `VisDrone2019-DET-{train,val}/{images,annotations}`.
- Produces: `parse_annotation(line: str) -> RawAnnotation`, `convert_box(annotation: RawAnnotation, image_width: int, image_height: int) -> ConvertedBox | SkipReason`, `convert_dataset(source_root: Path, output_root: Path, image_mode: str = "symlink") -> dict`, `audit_dataset(dataset_root: Path, expected_names: Sequence[str]) -> dict`.

- [ ] **Step 1: 写失败测试锁定全部类别映射和过滤规则**

  在 `test_visdrone_dataset.py` 构造包含原始类别 0..11、`score=0`、负宽高、越界框、遮挡和截断字段的临时数据。断言 1/2 都输出 class 0，3..10 输出 1..8，0/11/无效框被跳过，遮挡和截断合法框被保留，审计计数与原因精确匹配。

- [ ] **Step 2: 运行测试并确认失败**

  Run: `python3 -m pytest -q rk3566/nanotrack_v3/tests/test_visdrone_dataset.py`

  Expected: FAIL，因为 `visdrone_dataset` 尚不存在。

- [ ] **Step 3: 实现最小转换纯函数和 CLI**

  `convert_dataset` 必须为每张图片生成同名标签文件和新数据视图中的相对符号链接；不得修改源目录。CLI 参数固定为 `--source-root`、`--output-root`、`--report`、`--image-mode {symlink}`，输出原子写入的 JSON 报告。

- [ ] **Step 4: 增加断链、缺图、缺标签和非法归一化坐标审计测试**

  `audit_dataset` 必须重新读取输出，不复用转换时内存统计；遇到断开的图片链接、缺失 label、class 超过 8、坐标非有限或不在 `[0,1]` 时返回失败状态并列出路径。

- [ ] **Step 5: 运行单测、语法检查和 Git 忽略检查**

  Run:

  ```bash
  python3 -m pytest -q rk3566/nanotrack_v3/tests/test_visdrone_dataset.py
  python3 -m py_compile rk3566/nanotrack_v3/tools/visdrone_dataset.py rk3566/nanotrack_v3/tools/convert_visdrone9.py rk3566/nanotrack_v3/tools/audit_visdrone9.py
  git check-ignore -v artifacts/yolov8-visdrone9/dataset/example.jpg artifacts/yolov8-visdrone9/runs/example.pt .venvs/yolov8-visdrone/bin/python
  ```

  Expected: tests PASS；三个生成路径均被忽略。

- [ ] **Step 6: 提交数据工具**

  ```bash
  git add .gitignore rk3566/nanotrack_v3/tools/visdrone_dataset.py rk3566/nanotrack_v3/tools/convert_visdrone9.py rk3566/nanotrack_v3/tools/audit_visdrone9.py rk3566/nanotrack_v3/tests/test_visdrone_dataset.py rk3566/nanotrack_v3/models/visdrone9.txt rk3566/nanotrack_v3/training/visdrone9-data.yaml
  git commit -m "feat: add audited VisDrone9 dataset conversion"
  ```

### Task 2: 转换真实数据并冻结审计证据

**Files:**
- Create: `rk3566/nanotrack_v3/docs/visdrone9-detector-training.md`
- Runtime-only: `artifacts/yolov8-visdrone9/dataset/`
- Runtime-only: `artifacts/yolov8-visdrone9/dataset-conversion.json`
- Runtime-only: `artifacts/yolov8-visdrone9/dataset-audit.json`

**Interfaces:**
- Consumes: Task 1 的转换与审计 CLI。
- Produces: 可供 YOLO 训练读取的数据视图，以及转换报告/独立审计报告的 SHA-256 和文档摘要。

- [ ] **Step 1: 记录源数据身份并确认只读边界**

  Run: 统计 train/val 图片与标注数量、目录大小，并对各 split 的排序文件名清单计算 SHA-256。Expected: train 6471/6471、val 548/548；不写入 `/home/u20/dataset/VisDrone2019-DET`。

- [ ] **Step 2: 执行真实数据转换**

  ```bash
  python3 rk3566/nanotrack_v3/tools/convert_visdrone9.py \
    --source-root /home/u20/dataset/VisDrone2019-DET \
    --output-root artifacts/yolov8-visdrone9/dataset \
    --report artifacts/yolov8-visdrone9/dataset-conversion.json \
    --image-mode symlink
  ```

  Expected: 7019 个图片入口和 7019 个 label；报告无致命错误。

- [ ] **Step 3: 用独立入口重新审计**

  ```bash
  python3 rk3566/nanotrack_v3/tools/audit_visdrone9.py \
    --dataset-root artifacts/yolov8-visdrone9/dataset \
    --report artifacts/yolov8-visdrone9/dataset-audit.json
  ```

  Expected: `status=pass`，类别仅 0..8，所有图片链接可读，转换和审计的有效实例总数一致。

- [ ] **Step 4: 抽样可视化而不提交图片**

  从 train/val 各确定性抽取 20 张，绘制转换框到 `artifacts/yolov8-visdrone9/dataset-preview/`，人工检查合并类别、边界裁剪和密集目标；将样本清单及结论写入训练文档。

- [ ] **Step 5: 提交只含文档的审计摘要**

  文档记录源清单 hash、两个 JSON hash、各类实例数、过滤原因和预览结论，不加入 JSON、大图或数据目录。

  ```bash
  git add rk3566/nanotrack_v3/docs/visdrone9-detector-training.md
  git commit -m "docs: record VisDrone9 conversion audit"
  ```

### Task 3: 可复现训练环境与训练入口

**Files:**
- Create: `rk3566/nanotrack_v3/tools/train_visdrone9.py`
- Create: `rk3566/nanotrack_v3/tests/test_train_visdrone9.py`
- Create: `rk3566/nanotrack_v3/training/requirements-yolov8-visdrone.lock`
- Modify: `rk3566/nanotrack_v3/docs/visdrone9-detector-training.md`
- Runtime-only: `.tools/ultralytics_yolov8/`
- Runtime-only: `.venvs/yolov8-visdrone/`

**Interfaces:**
- Consumes: Task 2 的 dataset audit SHA-256、Rockchip checkout、预训练权重。
- Produces: `build_train_kwargs(mode: Literal["smoke", "full"], data: Path, project: Path, seed: int = 42) -> dict[str, object]`, `probe_cuda(torch_module) -> dict`, `validate_resume_metadata(checkpoint: Path, expected: RunIdentity) -> None`，以及 smoke/full 训练 CLI。

- [ ] **Step 1: 写失败测试锁定训练参数与拒绝条件**

  断言 smoke 为 5 epoch、full 为 100 epoch 且 batch=8/imgsz=640/patience=20/seed=42/amp=true；fake torch 报告 CUDA false 或 tensor 运算失败时必须退出；resume 元数据的 repo commit、数据审计 hash、类别或模式不一致时必须拒绝。

- [ ] **Step 2: 运行测试并确认失败**

  Run: `python3 -m pytest -q rk3566/nanotrack_v3/tests/test_train_visdrone9.py`

  Expected: FAIL，因为训练入口尚不存在。

- [ ] **Step 3: 实现训练入口，延迟导入第三方依赖**

  CLI 参数：`--mode {smoke,full}`、`--repo`、`--weights`、`--data`、`--dataset-audit`、`--project`、`--resume`。`--help` 不得导入 torch/ultralytics；真实执行先验证 checkout commit、数据 hash、CUDA tensor 运算，再调用该 checkout 的 `YOLO(...).train(**kwargs)` 并写 `run-identity.json`。

- [ ] **Step 4: 克隆固定 Rockchip commit 并建立隔离环境**

  Clone 到 `.tools/ultralytics_yolov8`，checkout `4674fe6e003dfbc5f2250d3b39dd31faaf7a9877`；在 `.venvs/yolov8-visdrone` 安装该仓库依赖及 CUDA 版 PyTorch。保存 `pip freeze` 为 lock 文件，并在文档记录 Python、torch、CUDA、cuDNN、GPU 和 driver 实际值。

- [ ] **Step 5: 下载预训练权重并验证 GPU**

  把 `yolov8n.pt` 放入忽略目录，记录 URL、大小、SHA-256。同时执行 `nvidia-smi`、`torch.cuda.is_available()`、设备名和一次 CUDA tensor 运算；四项都成功才继续。

- [ ] **Step 6: 运行测试和 CLI 静态检查**

  ```bash
  python3 -m pytest -q rk3566/nanotrack_v3/tests/test_train_visdrone9.py
  python3 -S rk3566/nanotrack_v3/tools/train_visdrone9.py --help
  git check-ignore -v .tools/ultralytics_yolov8/.git .venvs/yolov8-visdrone/bin/python artifacts/yolov8-visdrone9/runs/a/weights/best.pt
  ```

  Expected: PASS；外部 checkout、环境和权重均被忽略。

- [ ] **Step 7: 提交训练入口与环境锁**

  ```bash
  git add rk3566/nanotrack_v3/tools/train_visdrone9.py rk3566/nanotrack_v3/tests/test_train_visdrone9.py rk3566/nanotrack_v3/training/requirements-yolov8-visdrone.lock rk3566/nanotrack_v3/docs/visdrone9-detector-training.md
  git commit -m "feat: add reproducible VisDrone9 training entrypoint"
  ```

### Task 4: 5 epoch 冒烟训练

**Files:**
- Modify: `rk3566/nanotrack_v3/docs/visdrone9-detector-training.md`
- Runtime-only: `artifacts/yolov8-visdrone9/runs/smoke/`

**Interfaces:**
- Consumes: Task 3 训练入口和身份文件。
- Produces: 完整 5 epoch checkpoint、验证指标、资源记录和可恢复性证据；不产生候选正式模型。

- [ ] **Step 1: 运行完整数据 smoke**

  ```bash
  .venvs/yolov8-visdrone/bin/python rk3566/nanotrack_v3/tools/train_visdrone9.py \
    --mode smoke \
    --repo .tools/ultralytics_yolov8 \
    --weights artifacts/yolov8-visdrone9/source/yolov8n.pt \
    --data rk3566/nanotrack_v3/training/visdrone9-data.yaml \
    --dataset-audit artifacts/yolov8-visdrone9/dataset-audit.json \
    --project artifacts/yolov8-visdrone9/runs
  ```

  Expected: 5 epochs 完成，loss/指标有限，生成 `best.pt`、`last.pt` 和逐类验证结果。

- [ ] **Step 2: 验证 checkpoint 可加载和恢复**

  使用相同入口对 `last.pt` 做只读身份检查和模型加载；用固定 val 样本执行预测。Expected: 类别表恰为 9 类、预测无 NaN/Inf、恢复身份全部匹配。

- [ ] **Step 3: 记录 smoke 边界**

  在文档记录命令、开始/结束时间、实际 GPU、峰值显存、最后一轮 loss/指标、checkpoint hash 和异常；明确写明 smoke 模型不是发布候选。

- [ ] **Step 4: 提交 smoke 证据摘要**

  ```bash
  git add rk3566/nanotrack_v3/docs/visdrone9-detector-training.md
  git commit -m "docs: record VisDrone9 smoke training"
  ```

### Task 5: 100 epoch 正式训练与模型选择

**Files:**
- Modify: `rk3566/nanotrack_v3/docs/visdrone9-detector-training.md`
- Runtime-only: `artifacts/yolov8-visdrone9/runs/full/`
- Runtime-only: `artifacts/yolov8-visdrone9/export/yolov8n-visdrone9-best.pt`

**Interfaces:**
- Consumes: Task 4 已通过的环境与数据身份，但从原始 `yolov8n.pt` 重新开始。
- Produces: 由 val mAP50-95 选择的 `best.pt`、完整逐类指标和训练报告。

- [ ] **Step 1: 从预训练权重启动 full run**

  使用 Task 4 相同命令，只把 `--mode` 改为 `full`。不得从 smoke checkpoint 接续。Expected: 最多 100 epoch，early stopping 仅由 patience 20 触发。

- [ ] **Step 2: 对中断使用受约束 resume**

  仅当 repo commit、数据审计 hash、类别、seed、训练模式和 run identity 全部一致时使用 `--resume <last.pt>`；记录中断和恢复时间。身份不一致则新建 run，不覆盖旧结果。

- [ ] **Step 3: 固化 best checkpoint 和逐类指标**

  按 val mAP50-95 选择 `best.pt`，复制到 export 忽略目录，记录 PT SHA-256。导出总体和九类 precision/recall/AP50/AP50-95、confusion matrix、PR/loss 图身份、最佳 epoch、总时长和峰值显存。

- [ ] **Step 4: 抽查失败样本**

  固定抽查小目标、密集同类、遮挡和截断场景；文档仅记录样本 ID、观察和误差类型，不提交预测图。

- [ ] **Step 5: 提交训练报告**

  ```bash
  git add rk3566/nanotrack_v3/docs/visdrone9-detector-training.md
  git commit -m "docs: record VisDrone9 full training results"
  ```

### Task 6: RKOPT ONNX 导出和 PT/ONNX 一致性

**Files:**
- Create: `rk3566/nanotrack_v3/tools/rkopt_validation.py`
- Create: `rk3566/nanotrack_v3/tools/verify_rkopt_onnx.py`
- Create: `rk3566/nanotrack_v3/tools/compare_pt_onnx.py`
- Create: `rk3566/nanotrack_v3/tests/test_rkopt_validation.py`
- Modify: `rk3566/nanotrack_v3/docs/visdrone9-detector-training.md`
- Runtime-only: `artifacts/yolov8-visdrone9/export/yolov8n-visdrone9-rkopt.onnx`
- Runtime-only: `artifacts/yolov8-visdrone9/export/validation-samples.txt`
- Runtime-only: `artifacts/yolov8-visdrone9/export/pt-onnx-parity.json`

**Interfaces:**
- Consumes: Task 5 `best.pt` 和固定 Rockchip exporter。
- Produces: `validate_output_contract(model) -> ContractReport`、`decode_rkopt_outputs(outputs, conf: float, nms: float) -> list[Detection]`、`compare_detections(reference, candidate, min_iou: float) -> ParityReport`，以及已验证的九输出 ONNX。

- [ ] **Step 1: 写失败测试锁定九输出契约和顺序**

  用合成 ONNX/张量断言顺序严格为每个 stride 的 box/class/score-sum，class channel=9、box=64、score-sum=1、网格 80/40/20；缺输出、交换输出、10 类、动态 batch、NaN/Inf 必须失败。

- [ ] **Step 2: 写 DFL 解码和检测匹配测试**

  合成一个确定性目标，断言解码 bbox/class/confidence；匹配仅允许同类并以 IoU 贪心配对，报告 matched recall、median IoU、p95 confidence delta。

- [ ] **Step 3: 运行测试并确认失败**

  Run: `python3 -m pytest -q rk3566/nanotrack_v3/tests/test_rkopt_validation.py`

  Expected: FAIL，因为验证模块尚不存在。

- [ ] **Step 4: 实现纯函数和两个 CLI**

  `verify_rkopt_onnx.py` 只做静态/有限值检查；`compare_pt_onnx.py` 对原图使用同一 640 letterbox、conf 0.25、NMS 0.45。生成样本清单后先计算 hash，再运行比较。

- [ ] **Step 5: 用固定 exporter 导出 ONNX**

  在 `.tools/ultralytics_yolov8` 中把 exporter 模型路径指向 Task 5 checkpoint，固定 640、batch 1、非 dynamic，运行官方 `ultralytics/engine/exporter.py`。记录命令、repo commit、PT/ONNX hash 和 ONNX opset。

- [ ] **Step 6: 执行契约和数值一致性门槛**

  固定从 val 确定性抽取 50 张；对 PT 置信度不低于 0.35 的检测，要求同类 IoU>=0.90 的 matched recall>=0.98、matched median IoU>=0.99、置信度绝对差 p95<=0.02。任一门槛失败先诊断预处理/输出顺序，不进入 RKNN 转换。

- [ ] **Step 7: 运行单测并提交工具与报告摘要**

  ```bash
  python3 -m pytest -q rk3566/nanotrack_v3/tests/test_rkopt_validation.py
  git add rk3566/nanotrack_v3/tools/rkopt_validation.py rk3566/nanotrack_v3/tools/verify_rkopt_onnx.py rk3566/nanotrack_v3/tools/compare_pt_onnx.py rk3566/nanotrack_v3/tests/test_rkopt_validation.py rk3566/nanotrack_v3/docs/visdrone9-detector-training.md
  git commit -m "feat: validate Rockchip YOLOv8 export parity"
  ```

### Task 7: 9 类 RKNN 转换与 provenance

**Files:**
- Modify: `rk3566/nanotrack_v3/tools/convert_yolov8_rk3566.py`
- Modify: `rk3566/nanotrack_v3/tools/verify_detector_manifest.py`
- Modify: `rk3566/nanotrack_v3/tests/test_detector_manifest.py`
- Modify: `rk3566/nanotrack_v3/docs/visdrone9-detector-training.md`
- Runtime-only: `artifacts/yolov8-visdrone9/export/yolov8n-visdrone9-rk3566-fp16.rknn`
- Runtime-only: `artifacts/yolov8-visdrone9/export/detector-manifest.json`
- Runtime-only: `artifacts/yolov8-visdrone9/export/visdrone9.txt`

**Interfaces:**
- Consumes: Task 6 ONNX、Task 5 PT、`models/visdrone9.txt`。
- Produces: 向后兼容 schema 1 的 manifest；新增成组的 `training` 对象，含 checkpoint/config/repository/commit/hash，C++ 对未知字段保持兼容。

- [ ] **Step 1: 写失败测试锁定九类和训练 provenance**

  调用 `build_detector_manifest` 生成 9 类、9 输出 manifest，断言三个 class tensor channel=9；训练字段必须包含 PT hash、data audit hash、Rockchip commit 和展开训练参数 hash。训练字段只提供一部分时 verifier 必须拒绝；旧 COCO manifest 仍须通过。

- [ ] **Step 2: 运行测试并确认失败**

  Run: `python3 -m pytest -q rk3566/nanotrack_v3/tests/test_detector_manifest.py`

  Expected: 新 provenance 测试 FAIL，现有测试保持通过。

- [ ] **Step 3: 最小扩展 converter/verifier**

  CLI 新增 `--training-checkpoint`、`--training-repository`、`--training-commit`、`--training-config`、`--dataset-audit`；五者要么全部给出，要么全部省略。manifest schema 仍为 1，避免改动板端 C++ 解析器。

- [ ] **Step 4: 运行 Python 和主机 C++ 回归**

  ```bash
  python3 -m pytest -q rk3566/nanotrack_v3/tests/test_detector_manifest.py rk3566/nanotrack_v3/tests/test_visdrone_dataset.py rk3566/nanotrack_v3/tests/test_train_visdrone9.py rk3566/nanotrack_v3/tests/test_rkopt_validation.py
  cmake -S rk3566/nanotrack_v3 -B /tmp/nanotrack-visdrone9-host -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF
  cmake --build /tmp/nanotrack-visdrone9-host -- -j2
  ctest --test-dir /tmp/nanotrack-visdrone9-host --output-on-failure
  ```

  Expected: 全部 PASS；不得因 manifest 扩展改变现有 C++ 行为。

- [ ] **Step 5: 用固定转换环境生成 FP16 RKNN**

  先把已跟踪的 `models/visdrone9.txt` 原样复制到 export 目录，以满足 converter 对 RKNN、manifest、labels 同目录的约束；再使用 `.venvs/rknn-convert` 和 Toolkit2 2.3.2 调用扩展 converter。Expected: 转换成功；manifest 为 9 类九输出，`quantized=false`、`calibrated=false`、`deployment_status=function_only_board_unverified`，PT/ONNX/RKNN/labels hashes 均匹配。

- [ ] **Step 6: 提交 converter 改动和转换摘要**

  ```bash
  git add rk3566/nanotrack_v3/tools/convert_yolov8_rk3566.py rk3566/nanotrack_v3/tools/verify_detector_manifest.py rk3566/nanotrack_v3/tests/test_detector_manifest.py rk3566/nanotrack_v3/docs/visdrone9-detector-training.md
  git commit -m "feat: record trained detector provenance"
  ```

### Task 8: 开发板独立 detector 验收

**Files:**
- Create: `rk3566/nanotrack_v3/docs/visdrone9-detector-board-verification.md`
- Board-only: `/home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/`

**Interfaces:**
- Consumes: Task 7 RKNN/manifest/labels 和现有 `yolo_detector_smoke` AArch64 程序。
- Produces: 真实 RK3566 张量、静态图检测、耗时和内存证据；不切换正式模型。

- [ ] **Step 1: 只读确认板端身份和目标目录不存在**

  通过反向隧道 `cat@127.0.0.1:10022` 检查 `uname -a`、RKNN API/driver、剩余空间、当前 release 和新 release 是否存在。若新目录已存在，使用新的版本名，不覆盖。

- [ ] **Step 2: 创建隔离 release 并传输最小产物**

  复制现有可执行文件/依赖和新的 RKNN、manifest、`visdrone9.txt`；传输后在板端重新计算 SHA-256，与主机 manifest 对比。不得修改旧 release。

- [ ] **Step 3: 运行 manifest 和模型加载 smoke**

  在板端运行 verifier 和 `yolo_detector_smoke`，先使用固定 20 张 VisDrone val 样本，再分别用 `--class-id 0` 和 `--class-id 2` 检查 person/car 过滤。Expected: 1 个 NHWC 输入、9 个 NCHW 输出，class channel=9，无 RKNN 错误；每张都生成非空输出文件，允许个别图片零候选。

- [ ] **Step 4: 记录实际功能资源边界**

  对 20 张图片记录 preprocess/inference/postprocess/total latency、候选数量、进程 VmHWM；明确这些是功能测量，不是性能通过线。

- [ ] **Step 5: 提交板端独立验收报告**

  文档记录板端路径、命令、所有身份 hash、张量信息、通过/失败矩阵、延迟和内存，不拉取或提交图片。

  ```bash
  git add rk3566/nanotrack_v3/docs/visdrone9-detector-board-verification.md
  git commit -m "docs: verify VisDrone9 detector on RK3566"
  ```

### Task 9: 长时跟踪集成验收和正式切换

**Files:**
- Modify: `rk3566/nanotrack_v3/models/detector-manifest.json`
- Modify: `rk3566/nanotrack_v3/README.md`
- Modify: `rk3566/nanotrack_v3/docs/visdrone9-detector-training.md`
- Modify: `rk3566/nanotrack_v3/docs/visdrone9-detector-board-verification.md`
- Board-only: versioned acceptance artifacts under the new release

**Interfaces:**
- Consumes: Task 8 已通过的隔离 release 和现有 NanoTrack/LongTermController。
- Produces: detector 请求进入 NanoTrack 验证的真实视频证据；只有全部硬门槛通过才更新 canonical manifest/README。

- [ ] **Step 1: 运行 person、car 和 detector-disabled 三组视频**

  复用上一版验收的同一视频、起始框和控制器配置；新模型分别使用 class 0 和 class 2，另运行 detector-disabled 回归。每组保留 output、CSV、metadata、run log 和输入/可执行文件/model hashes。

- [ ] **Step 2: 验证安全和队列不变量**

  运行 `scripts/verify_board.sh` 并断言：帧数一致、request/complete 无错误、候选能进入 NanoTrack hypothesis、`LOST && control_valid=1` 为零、请求完成差不超过 1、detector-disabled 计数全为零。

- [ ] **Step 3: 运行持续性样本并检查无界增长**

  使用不少于 1000 帧或 10 分钟的视频循环，采样 RSS/VmHWM、线程数、请求/完成数。若内存持续单调增长、线程数增长或队列 gap>1，保持新模型为未发布状态并记录失败。

- [ ] **Step 4: 对照上一版 COCO detector**

  在相同输入上列出两模型的 detector batch、候选验证、reacquired、错误数和资源指标。不得把“候选更多”表述为“找回成功”；找回结论只来自控制器的 confirmed reacquisition。

- [ ] **Step 5: 仅在硬门槛通过后切换 canonical manifest**

  将 `models/detector-manifest.json` 更新为新模型文件名、9 类标签和真实 hashes；README 示例改用 `models/visdrone9.txt`，说明 person=0/car=2。板端保留旧 release，不删除、不覆盖。

- [ ] **Step 6: 全量回归和大文件检查**

  ```bash
  python3 -m pytest -q rk3566/nanotrack_v3/tests
  cmake -S rk3566/nanotrack_v3 -B /tmp/nanotrack-visdrone9-final -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF
  cmake --build /tmp/nanotrack-visdrone9-final -- -j2
  ctest --test-dir /tmp/nanotrack-visdrone9-final --output-on-failure
  git diff --check
  git status --short
  git ls-files | rg '\.(pt|pth|onnx|rknn|avi|mp4|jpg|png)$' && exit 1 || true
  ```

  Expected: Python/C++ 测试全部 PASS；无新模型、媒体、虚拟环境、数据或另一个会话的文档进入本任务提交。

- [ ] **Step 7: 提交正式切换和最终报告**

  ```bash
  git add rk3566/nanotrack_v3/models/detector-manifest.json rk3566/nanotrack_v3/README.md rk3566/nanotrack_v3/docs/visdrone9-detector-training.md rk3566/nanotrack_v3/docs/visdrone9-detector-board-verification.md
  git commit -m "feat: promote VisDrone9 detector for RK3566 tracking"
  ```

  若任一硬门槛失败，只提交报告中的失败证据，不修改 canonical manifest 和 README，不宣称替换完成。
