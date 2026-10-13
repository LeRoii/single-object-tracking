# 单目标跟踪算法本机部署

本目录已在 RTX 2060 / Ubuntu 22.04 上完成六个算法的官方源码部署和真实视频推理：FEAR-XS、LightTrack-Mobile、NanoTrack V3、SiamABC S-Tiny、SiamMask 和 STMTrack。

## 快速验收

```bash
cd /home/u20/single-object-tracking
bash scripts/verify_all.sh
```

验收脚本会检查 NVIDIA 内核驱动、磁盘模块与 NVML 版本一致性，逐个环境的依赖和 CUDA 可用性，六份官方权重，以及视频帧数或逐帧预测结果行数。

## 目录

- `repos/`：固定到具体 commit 的官方源码。
- `.venvs/`：六套隔离 Python 环境。
- `artifacts/`：运行日志、结果视频和边界框。
- `scripts/`：非交互视频入口和统一验收脚本。
- `DEPLOYMENT_STATUS.md`：版本、命令、结果和兼容处理明细。

所有推理入口都使用算法仓库原有模型、权重加载器和跟踪管线；新增脚本只处理无显示器运行、视频读写与结果保存。
