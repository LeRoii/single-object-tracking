# 单目标跟踪算法本机部署范围

## 目标

在 `/home/u20/single-object-tracking` 中按各官方仓库说明部署并实际运行以下六个跟踪器：

1. STMTrack
2. FEAR-XS
3. SiamMask
4. LightTrack-Mobile
5. NanoTrack
6. Siam-ABC Tiny/Small

## 完成标准

每个算法必须同时具备以下证据才算跑通：

- 官方源码固定在可追溯的 Git commit；
- 依赖环境可重复创建；
- 官方预训练权重已下载并能被程序加载；
- 使用官方示例素材或仓库指定测试入口完成至少一次真实推理；
- 保存终端日志和可检查的输出文件；
- `DEPLOYMENT_STATUS.md` 记录环境、commit、命令、输出和已知限制。

仅成功导入 Python 包、构造模型或完成 ONNX 导出，不视为“跑通”。

## 本机约束

- 操作系统：Ubuntu 22.04，x86_64。
- CPU：Intel Core i5-9400F，6 核。
- 内存：16 GiB。
- GPU：GeForce RTX 2060 6 GiB；NVIDIA 驱动与 NVML 已统一为 580.178.04，优先按官方 CUDA 路径运行，并保留 CPU 可用性记录。
- 系统 Python：3.10.12；旧项目使用独立环境，禁止修改系统 Python 包。

## 目录约定

- `repos/<name>`：官方或论文作者指定源码仓库。
- `.venvs/<name>`：算法隔离环境。
- `artifacts/<name>`：模型权重、输出视频、图像和运行日志。
- `scripts/`：可重复执行的安装与验证入口。
- `DEPLOYMENT_STATUS.md`：部署证据总表。
