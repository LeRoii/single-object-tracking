# RK3566 生产模型路径配置设计

## 目的

让生产 `viewlink_app` 不再把 `/app/model` 写死在 C ABI adapter 中，而是从独立部署配置读取 NanoTrack、YOLO、manifest、labels 和控制参数文件路径。候选版本完整放在 `/app/nanotrack_b827_candidate`，不覆盖现有 `/app/bin` 和 `/app/model`。

## 已确认输入

- u20 中三个 NanoTrack RKNN 与板端验证记录的 SHA-256 完全一致，可以用于本次候选部署：
  - `template_backbone.rknn`：`38b60334e8e0f0999bfb7ff9e943090546461f9307682653685237e24b2a7709`；
  - `search_backbone.rknn`：`92125f1ef6254dade5786bb999faa5b9d9412ceb2b7987f6dbba82f959b7ef6a`；
  - `head.rknn`：`2065bfca503cca9197e67de1a754efad930dfed4d2fc73881c77d9e850dcb600`。
- 当前 detector 本来就是 COCO80 YOLOv8；`detector-manifest.json` 和 `coco80.txt` 已匹配该接口。
- u20 当前没有 `yolov8n-rk3566-fp16.rknn`。不能把任意 COCO 模型改名替换；候选文件必须符合现有九输出 YOLOv8 后处理契约，并与 manifest 身份一致。期望 SHA-256 为 `4d53a9b1e12996556294be8e099fe2dc0815d5c9cfdc327edfe350832baf8cf6`。
- 为先打通功能链路，用户批准临时使用 u20 中已通过既有板端 detector 功能检查的 VisDrone9 工件：
  - RKNN：`/home/u20/single-object-tracking/artifacts/yolov8-visdrone9/export/yolov8n-visdrone9-rk3566-fp16.rknn`，SHA-256 `3698bc9bea1f43a4bac2826a09e03be0439fddfbc5c5604d8ed0f92b36881df0`；
  - manifest：SHA-256 `05e6d8c7c210b7f9d85b8fb712aa00b014f310e46e85f171ce1158afc90ba63e`；
  - labels：SHA-256 `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`。
- VisDrone9 仅用于模型加载、detector 和集成启动验证，不代表 COCO80 效果或最终模型验收。以后通过配置切换回 canonical COCO，不再修改 adapter。

## 采用方案

新增独立的 `/app/nanotrack_b827_candidate/config/production-deployment.conf`，而不是向 `visible-default.conf` 增加部署字段。这样控制器参数和文件部署职责保持分离，也避免修改现有严格的 controller 配置解析器。

配置采用严格的 `key=value` 格式，包含以下必填键：

```text
tracker_model_dir=/app/nanotrack_b827_candidate/model
controller_config_path=/app/nanotrack_b827_candidate/config/visible-default.conf
detector_model_path=/app/nanotrack_b827_candidate/model/yolov8n-visdrone9-rk3566-fp16.rknn
detector_manifest_path=/app/nanotrack_b827_candidate/config/detector-manifest.json
detector_labels_path=/app/nanotrack_b827_candidate/config/visdrone9.txt
```

C ABI adapter 只保留部署配置文件自身的固定位置；`tracker_detector_init()` 启动时读取并校验全部键，然后用这些路径初始化 tracker、controller 和 detector。未知键、重复键、空值、缺失键或文件不存在均使初始化返回 `false`，同时保持 `is_bad_vis_model=true`，不使用静默回退模型。

## 目录结构

```text
/app/nanotrack_b827_candidate/
├── bin/viewlink_app
├── config/
│   ├── production-deployment.conf
│   ├── visible-default.conf
│   ├── detector-manifest.json
│   └── visdrone9.txt
└── model/
    ├── template_backbone.rknn
    ├── search_backbone.rknn
    ├── head.rknn
    └── yolov8n-visdrone9-rk3566-fp16.rknn
```

## 验证与边界

- 先为部署配置解析器覆盖正常配置、缺键、重复键、未知键和空值测试，再修改 adapter。
- A6 交叉编译必须成功；重新记录 adapter、配置、`viewlink_app` 和 `libAI.a` SHA-256。
- 部署前逐个核对四个 RKNN、manifest、labels 和 controller 配置哈希。
- 板端启动采用有时限的前台 smoke，并在启动前确认没有既有 `viewlink_app` 进程；只记录进程启动、模型加载和错误日志，不声明跟踪、长期恢复或吊舱验收通过。

## 未采用方案

- 不把模型路径塞进 `visible-default.conf`：会混合部署与算法参数，并破坏当前严格 controller parser 的职责边界。
- 不继续写死 `/app/model`：无法形成隔离候选目录。
- 不直接复用板端 `VIS.rknn` 或任意其他 COCO 模型：其输出契约和 manifest 身份未经确认。
