# 部署状态与复现记录

验收日期：2026-09-18。主机为 Ubuntu 22.04、Intel i5-9400F、16 GiB 内存、RTX 2060 6 GiB。NVIDIA 已加载模块、磁盘模块及 NVML 均为 `580.178.04`。

## 结果总表

| 算法 | 官方源码 commit | 环境 | 官方权重 | 真实推理结果 |
|---|---|---|---|---|
| FEAR-XS | `0a3bd039918909c79c1b7e55a4bfb7807520abde` | Python 3.7.12 / PyTorch 1.7.1 / CUDA 10.2 | `FEAR-XS-NoEmbs.ckpt` | 661 帧，`artifacts/fear/output.mp4`，总耗时 19.38 s |
| LightTrack-Mobile | `39c426f48ee674795cdf0e00301a0b4ad0785d2a` | Python 3.6.15 / PyTorch 1.1.0 / CUDA 10.0 | `LightTrackM.pth` | 120 帧 VOT 格式结果，32.4 FPS |
| NanoTrack V3 | PyTorch 仓库 `248663fde6bf7c40190cf10ee396d5662919ecd3` | Python 3.7.12 / PyTorch 1.7.1 / CUDA 10.2 | `nanotrackv3.pth` | 987 帧，115.72 FPS |
| SiamABC S-Tiny | `b1c94e06fdf2dd3cb14ed07b05e38aa4601ece03` | Python 3.7.12 / PyTorch 1.7.1 / CUDA 10.2 | `model_S_Tiny_v1.pt` | 256 帧，进度统计约 73.7 FPS |
| SiamMask | `a2b3ce3dca23b3a911d96b791e007eec6d39a625` | Python 3.6.15 / PyTorch 1.1.0 / CUDA 10.0 | `SiamMask_DAVIS.pth` | 120 帧分割跟踪，57.60 FPS |
| STMTrack | `61730c19ec0eaea393fa3119b8d71023e1173d5b` | Python 3.7.12 / PyTorch 1.7.1 / CUDA 10.2 | fulldata `epoch-19.pkl` | 120 帧，28.50 FPS |

六个环境均已通过 `pip check`，并分别执行了 CUDA 张量分配。统一验收命令：

```bash
bash scripts/verify_all.sh
```

## 推理命令

### FEAR-XS

```bash
cd repos/fear
PYTHONPATH=. ../../.venvs/fear/bin/python demo_video.py \
  '--initial_bbox=[163,53,45,174]' \
  --video_path=assets/test.mp4 \
  --output_path=../../artifacts/fear/output.mp4
```

日志：`artifacts/fear/run.log`。权重 SHA-256：`8efe7dfd3498e385f332fd655f360a848d723f5fd77c2d433b2c084029616be5`。

### LightTrack-Mobile

先用 `scripts/prepare_vot_demo.py` 将官方示例视频转换为仓库测试入口要求的 VOT 目录，再运行官方测试脚本：

```bash
cd repos/lighttrack
../../.venvs/lighttrack/bin/python tracking/test_lighttrack.py \
  --arch LightTrackM_Subnet --dataset VOT2019 --video fear-demo \
  --resume snapshot/LightTrackM/LightTrackM.pth --stride 16 --even 0 \
  --path_name back_04502514044521042540+cls_211000022+reg_100000111_ops_32
```

结果：`repos/lighttrack/result/VOT2019/LightTrackM/baseline/fear-demo/fear-demo_001.txt`；权重 SHA-256：`b3ee9358386c0cb97948b1f68c8499f38929e9914663a6ef039c6ffa9117bef3`。

### NanoTrack V3

NanoTrack 主仓库 commit 为 `76b1c6711ae11958ec592e525163826d357d5db5`，其 README 指向作者的 `SiamTrackers/NanoTrack` 作为官方 PyTorch 实现，本机实际推理采用后者：

```bash
.venvs/nanotrack/bin/python scripts/run_nanotrack_video.py \
  --repo repos/siamtrackers/NanoTrack \
  --config repos/siamtrackers/NanoTrack/models/config/configv3.yaml \
  --snapshot repos/siamtrackers/NanoTrack/models/pretrained/nanotrackv3.pth \
  --video repos/siamtrackers/NanoTrack/bin/girl_dance.mp4 \
  --output artifacts/nanotrack/output.mp4 \
  --bbox-output artifacts/nanotrack/bboxes.txt --bbox 244 161 74 70
```

权重 SHA-256：`4f0d25108385a67bdec5fa01ca37c74acb72343b53996dd645fe18b86e512e69`。

### SiamABC S-Tiny

```bash
cd repos/siamabc
PYTHONPATH=. ../../.venvs/siamabc/bin/python realtime_test.py \
  '--initial_bbox=[416,414,61,97]' \
  --video_path=assets/penguin_in_fog.mp4 \
  --output_path=../../artifacts/siamabc/output.mp4
```

结果框：`artifacts/siamabc/bboxes/output.txt`；权重 SHA-256：`556f790e13969c009d657eddbea3fdcea9ccf3c7fdf399fd56dc6f2e8f91c058`。

官方 `requirements.txt` 固定 PyTorch 1.7.1，但代码使用后续版本才加入的 `torch.concat` 别名；已等价替换为 `torch.cat`。同时移除了 `core/models/blocks.py` 中从未使用、却会拉入未声明依赖或不存在 torchvision API 的导入，不改变前向计算。

### SiamMask

```bash
.venvs/siammask/bin/python scripts/run_siammask_video.py \
  --repo repos/siammask \
  --config repos/siammask/experiments/siammask_sharp/config_davis.json \
  --snapshot repos/siammask/experiments/siammask_sharp/SiamMask_DAVIS.pth \
  --video repos/fear/assets/test.mp4 \
  --output artifacts/siammask/output.mp4 \
  --bbox-output artifacts/siammask/bboxes.txt --bbox 163 53 45 174 --max-frames 120
```

已按官方 `make.sh` 编译两套 region 扩展。原 README 的 PyTorch 0.4.1/CUDA 9.2 归档栈不适合当前驱动，采用同代、仍可获取的 PyTorch 1.1.0/CUDA 10.0 官方 wheel；算法源码未改。权重 SHA-256：`d5a0e7cb13e2799bcf93545691261e7a48413ffb54bce9cb3b7fa3c560da4204`。

### STMTrack

```bash
.venvs/stmtrack/bin/python scripts/run_stmtrack_video.py \
  --repo repos/stmtrack \
  --config repos/stmtrack/experiments/stmtrack/test/vot/stmtrack-googlenet-vot.yaml \
  --snapshot repos/stmtrack/snapshots/stmtrack-googlenet-fulldata-train/epoch-19.pkl \
  --video repos/fear/assets/test.mp4 \
  --output artifacts/stmtrack/output.mp4 \
  --bbox-output artifacts/stmtrack/bboxes.txt --bbox 163 53 45 174 --max-frames 120
```

README 明确说明 PyTorch 1.5、1.6 或更高版本也可用，因此采用 PyTorch 1.7.1；其余包直接来自官方 `requirements.txt`。权重 SHA-256：`247c3d778063e068339daaa2c18311ce5b8cb2821bce2ebd7f7ff8bbe46387cb`。

## 边界说明

- NanoTrack V3 已额外在 RK3566 上完成全新 RKNN 部署与完整视频验收；其余算法的 x86_64/CUDA 结果仍不等同于已经完成板端部署。
- LightTrack、SiamMask、STMTrack 使用了最小真实视频序列进行功能验收，没有下载完整基准数据集，因此这里不声明论文精度复现。
- `artifacts/` 中保留了原始日志；SiamABC 的 `run-attempt*.log` 记录了官方依赖/API 不一致的排查过程。

## RK3566 NanoTrack V3 全新部署（2026-09-29）

部署目标为 LubanCat-1（RK3566，aarch64），新工程目录为
`/home/cat/Project/SOT/nanotrack_v3`。没有复用或改动板端已有的
`/home/cat/Project/SOT/nanotrack` 工程。模型从上述官方 PyTorch commit 和权重
重新导出 ONNX，再使用 RKNN-Toolkit2 2.3.2 为 `rk3566` 转换为三个非量化 RKNN：

- `template_backbone.rknn`：`38b60334e8e0f0999bfb7ff9e943090546461f9307682653685237e24b2a7709`
- `search_backbone.rknn`：`92125f1ef6254dade5786bb999faa5b9d9412ceb2b7987f6dbba82f959b7ef6a`
- `head.rknn`：`2065bfca503cca9197e67de1a754efad930dfed4d2fc73881c77d9e850dcb600`

板端使用 RKNN Runtime 2.3.2、NPU 驱动 0.8.2 和 OpenCV 4.10.0，以无 GUI
命令行方式完成 `girl_dance.mp4` 全视频推理。源视频为 856x480、30 FPS、987 帧；
首帧初始化后实际跟踪 986 帧。板端验收结果：

```text
PASS board artifacts frames=986
SUMMARY tracked_frames=986 mean_ms=38.089 p50_ms=37.777 p95_ms=47.016 tracking_fps=26.254
```

验收同时检查 RKNN 日志无 `error/unsupported`、模型哈希、CSV 行数和框边界、
以及输出视频解码帧数。最终工件位于 `artifacts/rk3566_nanotrack_v3/final/`。

与官方 PyTorch 轨迹逐帧比较时，中位 IoU 为 0.960409、平均 IoU 为 0.796395；
RKNN 内部使用 FP16 计算，长序列中出现一次累积漂移，最低 IoU 为 0。因此这里证明
部署可运行且接近实时，不声明与 PyTorch 全序列数值等价，也不把该视频对比当作标准数据集精度评测。
