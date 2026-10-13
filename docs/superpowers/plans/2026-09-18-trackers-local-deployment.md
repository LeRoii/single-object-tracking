# Trackers Local Deployment Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 按官方说明在当前 x86_64 Ubuntu 主机上部署并实际运行六种单目标跟踪算法，保存可复现命令和逐项推理证据。

**Architecture:** 每个算法使用独立 Python 环境，官方源码放在 `repos/`，运行证据放在 `artifacts/`。统一脚本只负责编排官方安装和 demo 命令，不重写算法实现；遇到旧依赖与当前系统不兼容时，使用不改变模型语义的兼容补丁并逐项记录。

**Tech Stack:** Git, Python virtual environments, PyTorch, OpenCV, FFmpeg, official pretrained checkpoints

**Spec:** `docs/deployment-scope.md`

## Global Constraints

- 六个算法均需完成真实推理，不能以 import 或模型构造代替。
- GPU 为 RTX 2060 6 GiB，驱动/NVML 580.178.04 已通过 `nvidia-smi` 验证；优先采用官方 CUDA 路径，显存不足时再按官方支持方式切换 CPU。
- 每个源码仓库必须记录 Git commit。
- 不修改系统 Python 包，算法依赖必须隔离。
- 所有输出和日志写入 `artifacts/<name>/`。

---

### Task 1: 建立部署骨架与环境管理

**Files:**
- Create: `scripts/common.sh`
- Create: `scripts/bootstrap_envs.sh`
- Create: `DEPLOYMENT_STATUS.md`

**Interfaces:**
- Produces: `ensure_repo(name, url)`、`run_logged(name, command...)` shell 函数及六个隔离环境目录。

- [ ] **Step 1:** 创建 `repos/`、`.venvs/`、`artifacts/` 目录并写入公共日志函数。
- [ ] **Step 2:** 安装支持项目所需 Python 版本的用户态环境管理器。
- [ ] **Step 3:** 运行环境自检，确认 Python、pip、Git、FFmpeg 可执行，并将输出保存到 `artifacts/system-info.log`。
- [ ] **Step 4:** 创建状态表，记录各项目源码、commit、环境、权重、运行命令和输出。

### Task 2: 部署并验证 FEAR-XS

**Files:**
- Create: `scripts/setup_fear.sh`
- Create: `scripts/run_fear.sh`
- Modify: `DEPLOYMENT_STATUS.md`

**Interfaces:**
- Consumes: `scripts/common.sh`。
- Produces: `artifacts/fear/output.mp4` 和 `artifacts/fear/run.log`。

- [ ] **Step 1:** 克隆 `https://github.com/PinataFarms/FEARTracker.git` 并记录 commit。
- [ ] **Step 2:** 根据官方 `requirements.txt` 创建隔离环境并安装依赖。
- [ ] **Step 3:** 确认仓库自带 FEAR-XS checkpoint 与 `assets/test.mp4` 可读。
- [ ] **Step 4:** 执行官方 `demo_video.py`，使用官方初始框 `[163,53,45,174]` 生成输出视频。
- [ ] **Step 5:** 用 `ffprobe` 验证输出视频含有效视频流和非零帧数，记录结果。

### Task 3: 部署并验证 SiamMask

**Files:**
- Create: `scripts/setup_siammask.sh`
- Create: `scripts/run_siammask.sh`
- Modify: `DEPLOYMENT_STATUS.md`

**Interfaces:**
- Consumes: `scripts/common.sh`。
- Produces: `artifacts/siammask/` 下的官方 demo 可视化结果和运行日志。

- [ ] **Step 1:** 克隆 `https://github.com/foolwood/SiamMask.git` 并记录 commit。
- [ ] **Step 2:** 创建兼容旧版源码的隔离环境，安装官方 requirements 并执行 `bash make.sh`。
- [ ] **Step 3:** 从官方 README 指定地址下载 `SiamMask_DAVIS.pth`。
- [ ] **Step 4:** 执行官方 `tools/demo.py --resume SiamMask_DAVIS.pth --config config_davis.json`。
- [ ] **Step 5:** 验证 demo 结果文件非空并记录实际运行耗时。

### Task 4: 部署并验证 LightTrack-Mobile

**Files:**
- Create: `scripts/setup_lighttrack.sh`
- Create: `scripts/run_lighttrack.sh`
- Modify: `DEPLOYMENT_STATUS.md`

**Interfaces:**
- Consumes: `scripts/common.sh`。
- Produces: `artifacts/lighttrack/run.log` 与至少一段序列的预测框结果。

- [ ] **Step 1:** 克隆 `https://github.com/researchmm/LightTrack.git` 并记录 commit。
- [ ] **Step 2:** 按官方 `install.sh` 对应依赖创建隔离环境，并编译仓库扩展。
- [ ] **Step 3:** 下载官方 LightTrack-Mobile checkpoint，校验文件可由 PyTorch 加载。
- [ ] **Step 4:** 使用官方 tracking 入口在一段官方格式测试序列上执行推理。
- [ ] **Step 5:** 验证预测结果包含逐帧有效边界框并记录帧数。

### Task 5: 部署并验证 NanoTrack

**Files:**
- Create: `scripts/setup_nanotrack.sh`
- Create: `scripts/run_nanotrack.sh`
- Modify: `DEPLOYMENT_STATUS.md`

**Interfaces:**
- Consumes: `scripts/common.sh`。
- Produces: `artifacts/nanotrack/output.mp4` 与 `artifacts/nanotrack/run.log`。

- [ ] **Step 1:** 克隆 `https://github.com/HonglinChu/NanoTrack.git` 并记录 commit。
- [ ] **Step 2:** 按仓库说明安装 PyTorch 或 NCNN demo 所需依赖。
- [ ] **Step 3:** 使用仓库提供的模型文件和示例媒体运行官方 demo。
- [ ] **Step 4:** 用 `ffprobe` 或逐帧结果文件确认发生了非零帧推理。

### Task 6: 部署并验证 STMTrack

**Files:**
- Create: `scripts/setup_stmtrack.sh`
- Create: `scripts/run_stmtrack.sh`
- Modify: `DEPLOYMENT_STATUS.md`

**Interfaces:**
- Consumes: `scripts/common.sh`。
- Produces: `artifacts/stmtrack/run.log` 与逐帧预测框。

- [ ] **Step 1:** 克隆 `https://github.com/fzh0917/STMTrack.git` 并记录 commit。
- [ ] **Step 2:** 根据官方 setup 和 requirements 创建隔离环境并编译扩展。
- [ ] **Step 3:** 下载官方 GoogLeNet STMTrack checkpoint。
- [ ] **Step 4:** 通过官方 `main/test.py --config ...` 入口运行最小测试序列。
- [ ] **Step 5:** 检查结果帧数与输入帧数一致并记录输出路径。

### Task 7: 部署并验证 Siam-ABC Tiny/Small

**Files:**
- Create: `scripts/setup_siamabc.sh`
- Create: `scripts/run_siamabc.sh`
- Modify: `DEPLOYMENT_STATUS.md`

**Interfaces:**
- Consumes: `scripts/common.sh`。
- Produces: `artifacts/siamabc/run.log` 与逐帧预测框或输出视频。

- [ ] **Step 1:** 从论文项目页解析并克隆作者发布的官方代码仓库，记录 URL 与 commit。
- [ ] **Step 2:** 按官方环境文件创建隔离环境并安装依赖。
- [ ] **Step 3:** 下载 Tiny 或 Small 官方权重并校验模型加载。
- [ ] **Step 4:** 使用官方测试或 demo 入口完成一段视频推理。
- [ ] **Step 5:** 验证输出包含非零帧有效框并记录结果。

### Task 8: 完成验收和复现入口

**Files:**
- Create: `scripts/verify_all.sh`
- Create: `README.md`
- Modify: `DEPLOYMENT_STATUS.md`

**Interfaces:**
- Consumes: 六个算法的运行脚本和 artifacts。
- Produces: 一条命令可执行的验收汇总及最终部署说明。

- [ ] **Step 1:** 编写逐项检查源码 commit、环境、权重、日志和输出的验收脚本。
- [ ] **Step 2:** 运行六个 demo 并保存最新日志。
- [ ] **Step 3:** 执行 `scripts/verify_all.sh`，要求六项全部通过。
- [ ] **Step 4:** 在 README 中记录从零复现命令、CPU/GPU说明和每个算法入口。
- [ ] **Step 5:** 对照 `docs/deployment-scope.md` 逐条审计，确认没有以弱验证代替真实推理。
