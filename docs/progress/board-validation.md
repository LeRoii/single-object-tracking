# 开发板验证任务进度（当前 RK3588）

## 2026-11-19T10:07:20+08:00 当前十目标实验版集成至 main 与 Debug

- 用户明确要求将当前版合并到 `main` 和 `Debug`。集成源为干净的 `codex/ten-target-smoothness@ebe92c6d002f2ae5d8082b771f0d7b900ab56cfa`；板端实际程序仍绑定算法源码 `33f34bafe75dab4da376ba2f9bb88867d421b5e6`、可执行文件 SHA-256 `01ce6658dcd57c519897c827e022736ceb4a04bbfd1f5d4c4588afbace7f84e2`，从 `33f34ba` 到 `ebe92c6` 的 `rk3566/nanotrack_v3` diff 为空。集成前分支基线分别为 `main@d4f14028768484027d37d9174a02eaedc23b80ab`、`Debug@3f6d9e36933a1802da83fd6ce6c24cee77492c34`；两工作树均干净。本条位于两个合并提交中。
- `main` 取实验分支的通用控制器/运行时/恢复/配置、对应单测及评测脚本，保留原有非 Debug 交互文件；`Debug` 保留 detector 点击初始化交互。逐文件对照两个待提交工作树的 `rk3566/nanotrack_v3` 与评测代码，非界面差异为零；界面差异仅限 `include/{cli,interactive_control}.hpp`、`src/{cli,interactive_control,interactive_debug_overlay,main,runtime_config}.cpp`、`tests/test_{cli,interactive_control,interactive_debug_overlay}.cpp` 十个文件。两边 `git diff --cached --check` 均通过，无未解决冲突。
- 主机实际运行 `cmake -S rk3566/nanotrack_v3 -B /tmp/nt-merge-{main,debug}-build -DENABLE_RKNN_RUNTIME=OFF`、`cmake --build ... --parallel 2`；两边构建成功，构建日志 `/tmp/nt-merge-main-build.log` / `/tmp/nt-merge-debug-build.log` SHA-256 `aa8b40d08bc46f65cd728fe94ad22e398c0ba6455a0fd6c43a1ba2216bd14479` / `ad5b3841c9a97e0cc5511773c47f0c164de899e1e6f93785609f289c824cd5db`。首次两工作树并行 `ctest` 时，`main` 的 CLI 测试因共用 `/tmp/nanotrack_v3_runtime_config_test/` 与 Debug 测试冲突而未找到配置；待 Debug 完成后单独重跑 `ctest --test-dir /tmp/nt-merge-main-build --output-on-failure -j2`，两边均 **23/23 PASS**。最终 CTest 日志 `/tmp/nt-merge-main-ctest-serial.log` / `/tmp/nt-merge-debug-ctest.log` SHA-256 `4f4a7eff16e0b9aa232e8c737b3d29371ae564f6d7b30e2aae4836f41615a6a5` / `0e6a0618992054a59a5bad759ffab0a99d096e30f09bd1e4dfbfd4470505c687`。两边 `python3 -m unittest tests.test_evaluate_occ_video_1 scripts.tests.test_evaluate_human_occlusion_constraints -q` 各 **32/32 PASS**。
- **证据边界 / Handoff**：本次是源码分支集成和主机 RKNN-off 测试，未重新部署或执行新的板端 GUI/271 帧回放。此前 `33f34ba` 的 RK3588 二十路与外部车结果仍按原记录解释。`STATUS.md` 和 `integration-contract.yaml` 由协调对话维护；其旧 `development_algorithm_baseline` 所指 `occlusion-triad-3c848ad` 板端路径已按用户清理要求删除，需由协调对话更新可用性及新集成提交身份。本次不将此实验版自动提升为生产 handoff 或吊舱验收。

## 2026-11-18T23:08:49+08:00 Project/SOT 仅保留最新实验程序及其运行依赖

- 用户明确要求保留最新代码、删除历次独立实验版本。任务分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `2998ed55067a001c54a68cb35785f42bb0733741`；本记录与清理结果同提交。此次只改板端文件和本任务进度文件，不改算法、`main`/`Debug` 或公共契约。板端 `LKD3588`，登录方式为用户已提供的交互式认证，未在脚本、Git 或记录中保存密码。
- 删除前从 Git 提交 `33f34bafe75dab4da376ba2f9bb88867d421b5e6` 生成精确源码归档，主机 `/tmp/nt-ten-target-33f34ba-source.tar` 与板端 `/home/neardi/Project/SOT/nt-ten-target-33f34ba/source-33f34ba.tar` SHA-256 均 `37649615c423be012b1da64cdba51b2aef9392639acdc8fc414f91d244b994f0`，大小 `5,724,160` 字节；`git diff --name-only 33f34ba HEAD -- rk3566/nanotrack_v3` 无输出。将旧共用目录的 `data/`、`models/`、`releases/` 移入保留目录 `assets/`，另保留原生重编译所需 `rknn_api.h`；交互配置只更新上述输入/模型绝对路径，SHA-256 由 `71d7e1fb610b1c3f75272845e8346db2d0f45b741ad13f552c29d07bef68e3a2` 变为 `c6b15b9cf660efae3e2945a47327e7c756ad943ac33ae3dd04379054c33b9889`。程序 `/home/neardi/Project/SOT/nt-ten-target-33f34ba/nanotrack_v3` SHA-256 仍为 `01ce6658dcd57c519897c827e022736ceb4a04bbfd1f5d4c4588afbace7f84e2`；搬迁前后视频 `assets/data/occ_video_1/1.mp4` / tracker manifest `assets/models/manifest.json` / detector RKNN `assets/releases/yolov8n-visdrone9-rk3588-fp16-v1/models/yolov8n-visdrone9-rk3588-fp16.rknn` SHA-256 分别仍为 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d` / `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261` / `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c`。`VERSION.txt` 更新现行配置与源码归档身份，SHA-256 `3efd8c5b9f069a345923b8a2ef10557dc22bd525430fdcc0f1bfb50579945cf3`。
- 清理前在保留目录记录顶层文件名/类型、大小和根文件 SHA-256：`cleanup-history-top-level.tsv` / `cleanup-history-size-kib.tsv` / `cleanup-history-root-files.sha256`，SHA-256 分别为 `931ad91b05bc3e1319bb7d626794ddbf1b81ddccaadf02acc572d2b79b4d03b1` / `83fc1451300623513d0712fa514a64c04279642c62ee88edd7483e0a383ded4d` / `e42da0897f2d98219b5e92a766ad3a44eae43b935ff420dbbe0e3ee0ad69fc2f`。板端清理脚本 `cleanup-history.py` SHA-256 `79c17f40ba891c7299851d392ed2f038aafcf66d05b8535a34adb78311ab6a9f`，固定 SOT/保留目录，并在删除前逐项核对程序、源码、视频、模型、配置哈希和两帧烟测输出；预演显示将删除 175 个目录、320 个文件。实际执行 `python3 /home/neardi/Project/SOT/nt-ten-target-33f34ba/cleanup-history.py --delete`，结果日志 `cleanup-history-run.log` SHA-256 `bc10c4c130a8fb83f49ef6403693af23a65836d31c0fa443ed3273e1f30198ad`，逐项日志 `cleanup-history-result.tsv` SHA-256 `a91faab2c0f79db3240f67ca584f92a634a1f3eaacd38bf095d307a63683d324`，496 行含表头、495 项均记录 `deleted`。板端复核 SOT 顶层仅余 `nt-ten-target-33f34ba`，目录约 `923M`；`/dev/root` 57G 中已用约 13G、可用约 43G、使用率 23%（清理前可用约 6G）。
- 搬迁前后均实际用当前程序执行 `./nanotrack_v3 --config /tmp/nt-cleanup-smoke.conf --bbox 994.000916,581.063293,35.289490,39.486084`，配置由现行交互配置只改为非交互、`max_frames=2` 和 `/tmp/nt-cleanup-smoke.{avi,csv,json}` 输出；两次 exit 0，最终 `SUMMARY tracked_frames=2 initializations=1`，模型加载报告 RKNN API `2.1.0`、driver `0.9.8`。这只证明搬迁与清理后程序可加载并处理两帧，不是重新进行 271 帧或 GUI 点击验收。板端桌面运行命令仍为 `cd /home/neardi/Project/SOT/nt-ten-target-33f34ba && ./nanotrack_v3 --config config/interactive.conf`。
- **证据边界 / Handoff**：用户要求清除历史版本，因此旧板端独立实验目录、源包、脚本、运行 CSV/视频均已删除，板端原件不可从清单恢复；Git 提交与部分主机归档/报告仍可能在各自路径。协调契约的 `development_algorithm_baseline` 目前仍指向已删除的 `/home/neardi/Project/SOT/occlusion-triad-3c848ad/`，协调对话应更新该路径的可用性说明；此次清理不改变其历史测试裁决，也不把 `33f34ba` 候选升级为正式基线或生产 handoff。后续若需旧实验，应从精确 Git 提交与已核验工件重新运行。

## 2026-11-18T22:46:22+08:00 Project/SOT 内重复归档清理

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `0d251c489410e0fdf78ae4c9905da468fd1ec39b`。用户进一步授权清理 `Project/SOT` 内确认无用的文件。本次再次以已提供账号交互式登录 `LKD3588`，只清理可核验的重复**归档**，不删除未确认备份的历史运行目录、人工记录、输入视频、模型、源码或当前 `nt-ten-target-33f34ba` 程序。先用 `tar -d` 实际逐文件比较 `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/inputs/occ_video_1.tar` 与同根 `data/occ_video_1`，以及 `/home/neardi/Project/SOT/dual-gt-safe-head-c3ce34a/runs/safe-probe-text.tar.gz` 与已解包的 `runs/safe-probe`；两次比较均 exit 0、差异输出 0 行。另两份板端包 `/home/neardi/Project/SOT/nt-occlusion-detector-probe.tar.gz`、`/home/neardi/Project/SOT/nt-detector-occlusion-audit.tar.gz` 的 SHA-256 分别为 `053320aff02672cbd953c3e10746cf48b658906c6c227f13944a96fd580139c3` / `38d9f48224f8aaa8ba83814057cf3f15493a47df4029e4dbbce8d9fa604671d9`，与主机保留的 `/tmp/` 同名包逐字节哈希相同；板端已解包的对应诊断目录也保留。
- 删除前板端将四份完整路径、SHA-256、字节数分别写入持久目录 `/home/neardi/Project/SOT/nt-ten-target-33f34ba/cleanup-duplicate-archives.sha256` / `cleanup-duplicate-archives.sizes`，清单本身 SHA-256 `5ffd02e298d1be8a9da2ec18c5877424a3d93e3a38d1f5f6cc22677611dfd62f` / `513774a84d1994d1d0bde2a7c9c24defd198c965b9a5e456efa0bbb5c7223ce4`，`sha256sum -c` 四项全 OK。四包依次为 `946616320`、`259612070`、`96051859`、`44499241` 字节，合计 `1,346,779,490` 字节（约 `1.25 GiB`）。实际 `rm` 仅删除清单中的四个归档，之后逐一验证文件不存在；板端 `/dev/root` 从约 `4.7G` 可用增至 `6.0G` 可用、使用率 90%。`nt-ten-target-33f34ba/nanotrack_v3` 仍为 SHA-256 `01ce6658dcd57c519897c827e022736ceb4a04bbfd1f5d4c4588afbace7f84e2`，输入视频仍为 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`，交互配置仍为 `71d7e1fb610b1c3f75272845e8346db2d0f45b741ad13f552c29d07bef68e3a2`。
- 其他较大旧归档仍保留：例如 car010 onset detector 包在主机仅有 CSV 子集；`models-rk3588.tar.gz` 与当前模型目录并非同一路径布局；历史运动冻结失败帧包保留原交付身份。本次没有新的板端推理或 GUI 验收，不改算法、`main`/`Debug` 或协调 handoff。

## 2026-11-18T22:35:44+08:00 RK3588 最新实验程序持久化及旧构建中间文件清理

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `e63b4d5772cffa869e3ee37725d2deab8b00c626`。用户指出旧回答给出 `/run/user/1000` 易失程序而非 `Project/SOT`，并要求清理板上无用文件。此前免密 SSH 到 `192.168.1.106` 确实连通但认证失败；这只证明免密方式不可用，不能说开发板无法登录。本次使用用户已提供的账号密码交互式认证成功，密码未保存到命令、脚本、Git 或进度文件。板端 `hostname=LKD3588`，初始 `/dev/root` 57G、已用 55G、可用约 100M、使用率 100%；`/run/user/1000` 为 tmpfs，原程序重启后会消失。
- 板端原程序 `/run/user/1000/nt-size-retain-v2-build/nanotrack_v3` 实际 SHA-256 `01ce6658dcd57c519897c827e022736ceb4a04bbfd1f5d4c4588afbace7f84e2`，与原 `33f34bafe75dab4da376ba2f9bb88867d421b5e6` 板测记录一致。实际 `install -m 755` 到持久目录 `/home/neardi/Project/SOT/nt-ten-target-33f34ba/nanotrack_v3`，复制同版 controller 配置到 `config/controller.conf` SHA-256 `bb19d2e8e085dd57084653b6ca37bb981b10ea2a0d00bc05d166f8a508c6e26f`；从已记录的相同参数交互配置重定向 controller/output 路径，生成 `config/interactive.conf` SHA-256 `71d7e1fb610b1c3f75272845e8346db2d0f45b741ad13f552c29d07bef68e3a2`，输出固定在该目录 `runs/manual/{output.avi,frames.csv,session.json}`。`VERSION.txt` SHA-256 `2193ef4908a621b93f26ecae36d40d71ba5f66d762764ecfa5503bc4fd72a07b` 记录源码和二进制身份。复制后程序 SHA-256 再核仍为 `01ce6658...`；`ldd` 未报告缺失动态库。输入视频、tracker manifest、detector RKNN 仍位于 `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/`，本次板端复核 SHA-256 依次为 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d` / `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261` / `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c`。
- 清理前用板端 `find` 只选 `/home/neardi/Project/SOT/*/build/CMakeFiles`，排除原始 `nanotrack_v3_debug_main-1bf657f` 与人工测试目录 `occlusion-triad-3c848ad`，且确认没有正在运行的 cmake/make/ninja/编译器进程。164 个旧实验目录的清单及 `du -sk` 尺寸分别留在持久目录 `cleanup-cmakefiles.paths` / `cleanup-cmakefiles.tsv`，SHA-256 `e8d272d821b3e7aafece640f10212db8a5612f2314b37c02a75c79d76e333f64` / `1a40612d0d473ca2db7e8c6a71c6842815d06292950b627fb82818ccc1a2022d`，合计 `4,786,964 KiB`（约 `4.565 GiB`）。实际仅对清单中符合上述固定路径形状的 CMake 中间目录执行 `rm -rf`；清理后清单中残存目录数为 0，`/dev/root` 可用约 `4.7G`、使用率 92%。未删除源码、程序、模型、视频、运行 CSV、会话元数据或日志；旧候选若需增量重编译，必须重新生成 CMake 中间文件。
- 板端桌面会话运行命令：`cd /home/neardi/Project/SOT/nt-ten-target-33f34ba && ./nanotrack_v3 --config config/interactive.conf`。此配置未做新的 GUI 点击实测；已有二十路 271 帧 headless 板测仍绑定 `33f34ba` 与原哈希。手动运行前若 `runs/manual/` 已有记录，应另存以免固定输出路径覆盖。此部署及清理不改算法、`main`/`Debug`、协调契约或生产 handoff。

## 2026-11-18T22:13:36+08:00 部分遮挡采用估计完整车框和宽松距离门

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `92d0d45927e3341b1478ca19a1f148856ce46210`；本条所在提交为结果提交。用户先要求部分遮挡统一用完整目标框，随后明确隐藏边界只需估计、评测距离不能离谱、不硬性要求高 IoU。保留所有原 canonical 和版本化真值包，不覆盖既有标签；新增 Git 内 `scripts/truth_overlays/full_object_estimates_v1.json` SHA-256 `f6532aecdbcf15ade8b0c3a7fb3a3f8f27b4c77802f0d35830b02bfa8b0b3211`，生成脚本 SHA-256 `5b8bb5fa680971ed16ca27d719cebe92979b49fd0f15e07070ac9fec3f7269ca`。每目标绑定原包哈希和视频 SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`，逐帧保留可见部分框、估计完整车框和估计方法。
- 新估计范围：car001 169–171/181（沿用已有双向插值完整框）；car002 211–212/217–229；car003 124–130；car004 126–132（其中 127–129 原 v4 完整估计不变）；car009 115–120；car010 149–157；car005–008 没有新增估计帧。car004 第 42 帧仍是可见但无精确框。`scripts/evaluate_occ_video_1.py` 新增 `--truth-version full_object_estimates_v1`，估计帧的普通 IoU 为空，仅检查算法框中心距估计完整框中心 `<=1` 个估计目标对角线；可见完整框继续用原普通 IoU 门。估计完整框必须包含原可见部分框；全遮挡和出画状态不被改为部分可见。输入数据来自 `/home/u20/single-object-tracking/data/occ_video_1`；源码目录不是数据根。
- 实际 `python3 scripts/prepare_full_object_estimates_v1.py --dataset-root /home/u20/single-object-tracking/data/occ_video_1 --output /tmp/nt-full-object-estimates-v1-regenerated.json`、`cmp` 对已提交 overlay，逐字节一致。实际 `python3 -m unittest tests.test_evaluate_occ_video_1 -q` 为 28/28 PASS，`python3 -m py_compile scripts/evaluate_occ_video_1.py scripts/prepare_full_object_estimates_v1.py`、`git diff --check` 通过；评测器/测试 SHA-256 `43993192bd8dd681c543d22bd8aa06d3ee558240f097dd50a2dddfea7fa57114` / `215dc2e2a0e88d3e29e44aa1a6143f7d02058f7fa70124b9fe7975663ab776c8`。逐帧视频抽查图 `/tmp/nt-full-estimate-contact.jpg` SHA-256 `dc82e2ce8d78f9822e546c964e5e5d03684669d77ed35c334f2b64e81be4b8cb`，红框为估计完整框、绿框为旧可见部分框；隐藏部分无法逐像素确认。
- **重评边界**：只对已保存的 RK3588 `33f34bafe75dab4da376ba2f9bb88867d421b5e6` 原始 CSV 重评，板端程序仍是 SHA-256 `01ce6658dcd57c519897c827e022736ceb4a04bbfd1f5d4c4588afbace7f84e2`，没有新编译或新板端运行。输入 `/tmp/nt-size-retain-v2-full/full-{gt,det}/car_###/frames.csv`，每路新报告同目录 `score-full-object-estimates-v1.json`，汇总 `/tmp/nt-full-object-estimates-v1-summary.json` SHA-256 `e89b67e76d60458014256c7032ad11e61ee8bf236a25533b6d248e6cedda8647`。十目标两种初始化 20/20 在新真值版本下 PASS，估计帧最大归一化中心误差最高为 car010 GT `0.575`、DET `0.569`；这只说明旧运行在宽松距离口径下不过分偏离。用旧 `car_004_visible_unboxed_v3` 对同一 GT CSV 复评仍是 `visible_boxes` FAIL、mean IoU `0.7777791550` 不变；不能把重新评分说成算法变好。CLI 实测 car004 DET 新版本输出 `/tmp/nt-car004-full-object-cli-score.json` 为 PASS。
- **Handoff**：协调对话若采用新口径，应登记 overlay 文件/哈希、原包版本与 1 对角线距离门，并把旧版报告并列保留；`main`/`Debug` 算法源码、正式基线和 `latest_handoff.available=false` 均未由本提交改动。当前只有同一视频的十目标估计，尚无跨场景验证。

## 2026-11-18T21:55:07+08:00 SAM2.1 tiny 可见掩码离线筛查未胜 NanoTrack

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `1e2f39e3567dfde6810e21430ee27b080ede1ebe`，运行时与正式真值均未改。主机从 [Meta 官方 SAM2 仓库](https://github.com/facebookresearch/sam2) 克隆推理源码 `/tmp/nt-sam2@2b90b9f5ceec907a1c18123530e92e794ad901a4`，下载 [官方 SAM2.1 Hiera Tiny 权重](https://dl.fbaipublicfiles.com/segment_anything_2/092824/sam2.1_hiera_tiny.pt) 至 `/tmp/nt-sam2.1_hiera_tiny.pt` SHA-256 `7402e0d864fa82708a20fbd15bc84245c2f26dff0eb43a4b5b93452deb34be69`；`hydra-core`/`iopath` 等轻量依赖只装在 `/tmp/nt-sam2deps`。使用既有 `.venvs/yolov8-visdrone` Python 3.10 / torch 2.4.0，CPU、无可用 CUDA。所有输入仍来自 SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d` 的 canonical 视频、`33f34ba` RK3588 car004 GT 初始化 CSV 和 car004 v3 可见部分真值。
- 实际 `PYTHONPATH=/tmp/nt-sam2:/tmp/nt-sam2deps .venvs/yolov8-visdrone/bin/python /tmp/nt-sam2-visible-probe.py --target car_004 --frames 126 127 128 129 --output /tmp/nt-sam2-car004-probe.json`（Python 路径相对 `/home/u20/single-object-tracking`）。在 160×160 局部图上以当前跟踪框做 box prompt，记录三张候选掩码、置信度与外接框；脚本/JSON/日志 SHA-256 `27bb60f1f89241acc34e4d0e147fc7b7dc71743e3d9879d0b1ed90704aaa676f` / `13e26376aa525388e244fdf1fb14082ddff258bd0be7d04feff99de5384a6ff3` / `6eaa8217725478effcfcedd4070c24c141ac2b92f52dd8f1fa2cf367488a32a0`。最高分掩码在第 126–129 帧的外接框高度仍为 `49/49/44/44px`，真值仅约 `32.9/30.0/26.1/26.1px`，未估出可见边界。
- 实际 `.venvs/yolov8-visdrone/bin/python /tmp/nt-sam2-video-crops.py` 从第 120–135 帧各生成一张以当前跟踪框为中心的 160×160 JPEG；生成脚本/裁剪位置 JSON/16 张图归档 SHA-256 `b5a6beeba0f18030088dc7385cb49bd33f38c480ae92f9c47111b5a41146b3dc` / `9686daaeea0721c5d0686c25423791bffb645b62e13b26e5d194b7fb657b0719` / `63abb310fc84b7c31fc5cf9d00555891f220427bc1d6d6b8af295dae49e12122`。再实际 `PYTHONPATH=/tmp/nt-sam2:/tmp/nt-sam2deps .venvs/yolov8-visdrone/bin/python /tmp/nt-sam2-video-probe.py`，在第 120 帧以当前 NanoTrack 框初始化 SAM2 video predictor 并传播 16 帧；脚本/逐帧 JSON/日志 SHA-256 `59526753558ca218ff61e945c068cf846bb6b99fd17603f54db003b84fed9bd1` / `1dd1451ac29aa5f05ee8447b22439e5d089cea12aea6426ce46620f329ad6d44` / `a60c835eb94cbcc70e8a0e47a36b7c66c286f691d154303ec6a1fbdf42d9defd`，CPU 传播 16 帧约 52 秒。实际 `python3 /tmp/nt-sam2-visible-score.py`，脚本/同真值比较 JSON SHA-256 `a956300e6a00ac7f96975befe68c55b309c9c659842235e8bf660acb2145a707` / `1ff753fae22c5ce56f5a392a7dea14a7ea56c707c10fd97b157f328bf373d7b8`。第 125–129 帧 SAM2 视频 mask 外接框 IoU `0.766/0.467/0.402/0.343/0.349`，当前 NanoTrack `0.849/0.508/0.429/0.372/0.378`；每帧均较低。
- **裁决**：这份官方冻结权重的 box prompt 和短片传播都没有解决 car004 的可见部分框，且当前 CPU 时延远超现有实时链路；不将 SAM2 接入 tracking/control/display，不以单帧模型实验宣称所有提示方式或所有分割模型都不可行。`33f34ba` 仍为最新完整板测的实验算法，二十路 18/20 PASS、car004 GT/DET 第 127–129 帧 FAIL；下一步需真正独立的可见像素监督与跨场景验证。

## 2026-11-18T21:47:41+08:00 car004 可见范围离线筛查：光流裁框和现成 SiamMask 均不可靠

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `cf231d80907097838549740095d7c1b6aefa323b`；本节只读取 `33f34ba` RK3588 完整二十路结果 `/tmp/nt-size-retain-v2-full/`、正式版本化真值和 canonical 视频；未改运行时、模型、真值、main/Debug 或协调基线。用户确认 car001 第 166–168 帧保留现行绿框，car004 第 127–129 帧按仅可见部分框计 IoU。car004 第 126 帧真值高度已约 `32.9px`，跟踪框约 `52.6px`，第 127–129 帧真值高度约 `30.0/26.1/26.1px`、跟踪框约 `52.6/50.0/49.6px`。
- 实际 `/home/u20/single-object-tracking/.venvs/yolov8-visdrone/bin/python /tmp/nt-aligned-flow-current.py > /tmp/nt-aligned-flow-current.log`，脚本/逐帧 CSV/日志 SHA-256 `979e6a692d392dda6ce27bbef1ebe3dfe05fc2c5131898380b7170536a0e57fb` / `debed603f91908d50a2222c51277429c782029aabaf5892e022eb2e8e42e6ed6` / `1bb2d5989731f136948c1f8301b66f79d58a7bc38bb249d91874cf4f4f1bfdb0`；当前二十路每帧框上计算四帧全局运动补偿后的上下半部光流。实际 `python3 /tmp/nt-visible-flow-rule-screen.py > /tmp/nt-visible-flow-rule-screen.json`，脚本/报告 SHA-256 `726a8787a10104acabc6d09b66fa4ee54848f776ec27c2f8e2f275d933aaca7f` / `7a18f52d3b242444f49dc65818b5581aa389ac5823ff24a6852c14332fae84c6`。仅作反事实筛查：在已接受且有正式 IoU 的可见帧，若上半运动像素比例减下半 ≥0.15，则将发布框高度裁到原来 60%、上边不变。触发 27 个“路线×帧”，其中 car004 GT/DET 第 127–129 帧共 6 个改善 >0.1 IoU，但其余 21 个降低 >0.1 IoU；car003 GT 第 122/123 帧 `0.765→0.595/0.590`，第 18/28 帧也大幅退化。car004 第 126 帧未触发。这不是已实现运行时规则，也不是正式 CTest/板端 gate；**否决该光流裁框规则**。
- 实际 `/home/u20/single-object-tracking/.venvs/yolov8-visdrone/bin/python /tmp/nt-row-appearance-probe.py > /tmp/nt-row-appearance-probe.log`，脚本/日志 SHA-256 `21a5945f3c1ad33f859c08dc64005d927669179b5b3aafd982490feb71530c2c` / `4261df3467140f842fae53553d84a5ed445d9fa584ccd9987af25c195fc5c092`。在当前 GT 初始化跟踪框上比较当前与四帧前的上下半部 Lab 像素差：car004 第 123–127 帧下/上差异比均已约 `2.25–3.06`，遮挡前也高；car003 第 122/123 帧达 `4.08/4.79`，正常框仍应保留。简单行外观阈值同样不能泛化。
- 复用本机官方 SiamMask `repos/siammask@a2b3ce3` 的冻结 `SiamMask_DAVIS.pth`（SHA-256 `d5a0e7cb13e2799bcf93545691261e7a48413ffb54bce9cb3b7fa3c560da4204`），只在第 120 帧以当前 car004 跟踪框重新初始化，运行 16 帧短片；实际命令为 `.venvs/siammask/bin/python /tmp/nt-siammask-short-probe.py --repo repos/siammask --config repos/siammask/experiments/siammask_sharp/config_davis.json --snapshot repos/siammask/experiments/siammask_sharp/SiamMask_DAVIS.pth --video data/occ_video_1/1.mp4 --bbox 726.894958 876.827515 45.487244 51.628174 --start-frame 120 --max-frames 16 --output /tmp/nt-siammask-car004-f120-135.mp4 --bbox-output /tmp/nt-siammask-car004-f120-135.txt`（路径以 `/home/u20/single-object-tracking` 为工作目录）。临时 runner/输出框/MP4/日志 SHA-256 `9188dc53d43fb425a322bf0e524453067b3f7c908f7f1f35427495ad3dbdbcd6` / `15716f4143dd3a8f24050112f93569d2dbdd6698db002f9fc4119d52b3476c21` / `7f4180025175a4eaf1bde418d9fca8c8e8b27f6b95e5a9ac46f970823258d3d3` / `f15ded991b17782ada06e8889263a65a70615db4428c4bc2a15763c2d2b747e1`；CPU 约 `3.44 FPS`。SiamMask 第 126/127 帧框 `53×51/60×69px`，同帧真值约 `47.5×32.9/44.9×30.0px`，第 127 帧 IoU `0.326` 低于当前 NanoTrack `0.429`，第 128/129 帧仍高 `58/55px`。该短片只能否决这份冻结权重在此目标的直接可见范围输出，不能外推所有分割模型。
- 下一步需要能在遮挡下预测实例**可见像素**的独立监督与跨场景验证。公开 [OVIS 数据论文](https://link.springer.com/article/10.1007/s11263-022-01629-1) 报告 25 类、约 296k 个遮挡场景实例掩码，[作者模型仓库](https://github.com/qjy981010/CMaskTrack-RCNN) 提供预训练权重入口；**尚未下载、运行、转换 RKNN 或评测其车类小目标**，只能作为候选数据/模型来源。现有 `33f34ba` 尺寸候选继续保留为实验版，car004 两路仍 FAIL，整体验证仍是 18/20 PASS。

## 2026-11-18T21:37:07+08:00 组合尺寸候选完成二十路与外部三车：18/20 不退化，ID14 进一步贴近真值

- 精确实验源码 `codex/ten-target-smoothness@33f34bafe75dab4da376ba2f9bb88867d421b5e6`、基线 `676d8d4dc2ee82a36081e5d9599d93f14e68bc9e`，工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`；板端程序 `/run/user/1000/nt-size-retain-v2-build/nanotrack_v3` SHA-256 `01ce6658dcd57c519897c827e022736ceb4a04bbfd1f5d4c4588afbace7f84e2`。板端实际 `bash /run/user/1000/nt-size-retain-v2-full20.sh`，二十路各 271 帧；板端 `/dev/shm/nt-size-retain-v2-full/`、主机 `/tmp/nt-size-retain-v2-full/`，原始归档双方 `nt-size-retain-v2-full20.tar.gz` SHA-256 `be2e646c2d24940fb779206e185abd6cec2711b3d81f8d19addb579c3457d9ef`，执行日志 `/dev/shm/nt-size-retain-v2-full20.log` SHA-256 `5be3adee472fd0de5cad0e28a61c68fb7e94ac131012271fa26c81e949f39411`。输入视频、tracker/detector 模型、manifest/labels 与配置 SHA-256 沿用本文件 `2026-11-18T18:04:40+08:00` 和 `973eb78` 记录；板端 Linux `6.1.84`、RKNN API `2.1.0`、driver `0.9.8`。
- 实际 `NT_SOURCE_COMMIT=33f34bafe75dab4da376ba2f9bb88867d421b5e6 python3 /tmp/nt-size-retain-v2-score-full.py`；脚本 SHA-256 `1dbb350625f392cdef307f46705c09a5610621bf8bbe3c642e8b26ea1eddd0fa`，运行日志 `/tmp/nt-size-retain-v2-score-full.log` SHA-256 `ae0bf302c602c62566472af273b4d0d33ec016e28f0a699a4afdc4b6ec24e66f`，各路 `score.json/.md` 在上述主机目录，摘要 `/tmp/nt-size-retain-v2-full-summary.json` SHA-256 `f8f2fc79e328529e6b95da0b9c817b5f7f0e88278ea2b421b14a0e2cc65c0436`。car003/004/010 用 v3 真值，car009 用 v2，其余 canonical，car001 第 166–168 帧保留现行框。相较 `8ce4257` 同口径结果，**18/20 PASS 不变**；二十路所有正式 gate 的通过状态与失败帧、重捕帧、可见 SEARCHING 帧数均逐路不变。car004 GT/DET 仍仅 `visible_boxes` 第 `[127,128,129]` 帧 FAIL；其余各路 mean IoU 变化极小，未构成整体可见区域收益。
- 实际 `bash /run/user/1000/nt-size-retain-v2-external.sh`，脚本 SHA-256 `ba92db822a295a1b731c0771f495bf086024842e6e6b5132302c31545e649719`；外部另外三车板端 `/dev/shm/nt-size-retain-v2-external/`、主机 `/tmp/nt-size-retain-v2-external/`，原始归档双方 `nt-size-retain-v2-external.tar.gz` SHA-256 `157934750bfff2196ede59ab45fd164138c574a8e453067951e7db01145cb00a`，执行日志板端 `/dev/shm/nt-size-retain-v2-external.log` SHA-256 `cc229cd5c46b68fa49e7c8c2f5cff8c49c152a0b44142e3c5c7cb2314b212b14`。实际 `python3 /tmp/nt-size-retain-v2-score-external.py`，脚本 SHA-256 `b88f5bf369c0ccd323978146a7e699734c22f0820ff5eb38c17c693b6bcb4425`，摘要 `/tmp/nt-size-retain-v2-external-summary.json` SHA-256 `c064977a58d54353b68925cf508b80564689c7ef87532157da9245ddb91027b6`。ID14 清晰无截断帧 mean IoU `0.6481→0.6679`、中心误差仍 `12.08px`、低 IoU 已接受 0、第 169 帧宽 `127.83→87.26px`；另三车 ID12/ID9/ID8 同版评价不变。只验证了两段 VisDrone 视频，不能推断跨场景泛化。
- **Handoff/下一步**：保留 `33f34ba` 为实验分支上的较优尺寸候选，未合 main/Debug、未升级协调基线。剩余 car004 第 127–129 帧要求可见部分框，现有 full-object detector 与 NanoTrack 都继续覆盖遮挡车身；以前的 GrabCut、MOG2、光流和现有分割权重探针没有提供可靠通用裁剪依据。需要独立可见区域证据及不同场景真值后再考虑发布；不按这三帧硬编码桥栏裁剪。板端 `/run` 和 `/dev/shm` 重启即失效，主机归档已保留并核对 SHA-256。

## 2026-11-18T21:25:28+08:00 双批次强小框后的尺寸目标短时保持：聚焦 RK3588 有益，全量进行中

- 实验分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，基线 `676d8d4dc2ee82a36081e5d9599d93f14e68bc9e`，代码、单测与本记录同提交。前版 `8ce4257` 放宽强中心小框的尺寸确认后，第 154/157/160 帧产生确认；此前独立试验 `125d9f9` 没有效果，因为在原门下这些候选未获确认。本候选只有在双批次确认且 detector 面积不超过 NanoTrack 框一半时，短时记住**原始 detector 目标尺寸**；掉检后仍按既有 1000ms 上限时效衰减及 5%/帧尺寸步长逼近，内部 NanoTrack 裁剪/运动中心不读取发布尺寸。源码/单测 SHA-256 `7a3c4ac36e4670ee6e440a8cbb3da81f2611a862f39a9baea85f2795e2c0367f` / `d12adefb456c40efdb27b0ff2e3adbea917b5e330055ff8c04624c4c22e1d711`；主机 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure -j4` **23/23 PASS**，日志 `/tmp/nt-size-retain-v2-host-{build,ctest}.log` SHA-256 `de5b6263cf1dfa55e3ab883284f54989b3b6cc6a10f6e5854ef1cd0901db5c37` / `bf92afdfe6c588a74167fa55b25c123f11125b119a42da6b4e6e35314b3010f0`。
- 隔离板端源码 `/run/user/1000/nt-size-retain-v2-source/`、构建 `/run/user/1000/nt-size-retain-v2-build/`，沿用前版相同的 RK3588 原生 CMake 参数与视频/模型/manifest/标签及配置。实际 `TMPDIR=/dev/shm cmake --build /run/user/1000/nt-size-retain-v2-build --parallel 2` 和 `ctest --test-dir /run/user/1000/nt-size-retain-v2-build --output-on-failure -j4`，**23/23 PASS**；程序 SHA-256 `01ce6658dcd57c519897c827e022736ceb4a04bbfd1f5d4c4588afbace7f84e2`，cmake/build/CTest 日志板端 `/dev/shm/nt-size-retain-v2-{cmake,build,ctest}.log` SHA-256 `9aebc4ed55d7d98cadc5eb8c74f71c3ab6c1855311f2002a317061946eecfd37` / `ffc6f184c25095a791681171c1a3633206c3bc49f82b8366dad0cf25f85f702e` / `60448a1bb9b075197e7edac126522d633401bb06b25d7f7fdcc094724f7b1337`。
- 实际 `bash /run/user/1000/nt-size-retain-v2-focused.sh`，脚本 SHA-256 `84f79cd5fe12d9d5fe6146c3a66ec6966dfbf3a4d4c22d8d019821a08cc6035a`；car002 GT/DET 各 271 帧和外部 VisDrone 0137/ID14 233 帧，原始板端 `/dev/shm/nt-size-retain-v2-focused/`、主机 `/tmp/nt-size-retain-v2-focused/`，归档双方 `nt-size-retain-v2-focused.tar.gz` SHA-256 `b05c7ccbed9313c75e4992c36877d38c0cdf3900d10a2584e6b34c22d0aef027`。car002 两路跟踪/控制/状态字段逐帧等于 `8ce4257`；外部 ID14 清晰帧 mean IoU `0.64813→0.66793`、中心误差仍 `12.08px`、低 IoU 已接受仍 0，第 169 帧发布宽 `127.83→87.26px`，真值约 `68px`。比较脚本/JSON `/tmp/nt-size-retain-v2-{compare.py,external-compare.json}` SHA-256 `dc14f7ad43ae144bfcebe2438725d1a88696152532c9d6866f9256d2c84e168c` / `0edcbdf995baa549ebf2ffff102e833fdccd8c6fdee20dba56827ea5d967cb7d`。
- `bash /run/user/1000/nt-size-retain-v2-full20.sh` 正在运行，脚本 SHA-256 `9bf0364d8317142e8872134a2cf7aaa0f1b3d1a4fde49d6ebe75ced85ab256a6`；完整二十路、外部其余三车及正式评分尚未完成。本候选**仅是实验**，不合 main/Debug、不升级协调基线。下一步先查 gate/恢复帧与误缩框，再决定保留或撤回。板端 `/run` 和 `/dev/shm` 重启即失效。

## 2026-11-18T21:17:53+08:00 强中心小框尺寸门完成全量评分：18/20 不退化，外部一车缩框改善

- 已固定实验源码 `codex/ten-target-smoothness@8ce42573760d30642f46fccbe3353995f60684da`，工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，基线 `7ed12f8fe829573a59ed9950b3bb97b081ac8b77`；`git show` 的运行时和单测 SHA-256 与前节板端传输源码吻合。板端程序 SHA-256 `2c85e0ce2f3c284212c52917a53db706e69012e6a02a23c073e4c6533befd740`，输入视频、模型、manifest、标签的逐文件 SHA-256 沿用下文 `2026-11-18T18:04:40+08:00` 记录，运行配置沿用 `973eb78`；板端 Linux `6.1.84`、RKNN API `2.1.0`、driver `0.9.8`。前节归档的 20 路原始 CSV 在主机 `/tmp/nt-size-iou-full/full-{gt,det}/car_###/`、板端 `/dev/shm/nt-size-iou-full/`。实际 `NT_SOURCE_COMMIT=8ce42573760d30642f46fccbe3353995f60684da python3 /tmp/nt-size-iou-score-full.py`，评分脚本 `/tmp/nt-size-iou-score-full.py` SHA-256 `ab987d4ffa825df9d4c016fe86c59a8e0d84af07483cd0e696fbbe7b4095855f`，运行日志 `/tmp/nt-size-iou-score-full.log` SHA-256 `1a19136ea171a8d403b05d23b644e2091512de0794297134d080f13d4ca7b392`，逐路 `score.json/.md` 留在上述主机目录；摘要 `/tmp/nt-size-iou-full-summary.json` SHA-256 `d6fd7515bee7e1015009cd9980aca1182ebe1bf2b14638c9ad8abb14b019b0a3`。真值选择为 car003/004/010 v3、car009 v2，其余当前 canonical，car001 第 166–168 帧保留原框。
- 正式全量结果 **18/20 PASS**，相对 `973eb78` 同口径评分，二十路所有 gate 的通过状态和失败帧均未变化，恢复帧及可见 SEARCHING 帧数逐路不变。car004 GT/DET 两路仍在 `visible_boxes` 第 `[127,128,129]` 帧 FAIL，算法框包含桥栏下被遮部分，与用户确认的仅可见部分真值口径不符；本尺寸门不会修复。其余 18 路无新增误接受或漏恢复。car002 GT/DET 算法字段逐帧相同；car006/007/010 等少数路线 mean IoU 变化约 `+0.0001` 至 `+0.0005`，不构成显著整体收益。
- 实际 `bash /run/user/1000/nt-size-iou-external.sh`，脚本 SHA-256 `68d59b5dc6f8727ca8f269635bda2fcf63ecc66e68e7be49072898fb41a61968`；外部 VisDrone 另外三车逐帧板端 `/dev/shm/nt-size-iou-external/`、主机 `/tmp/nt-size-iou-external/`，原始归档双方 `/dev/shm/nt-size-iou-external.tar.gz` / `/tmp/nt-size-iou-external.tar.gz` SHA-256 `ed34f73901f6b3621c23a9ba7adaeb7dbe793b162e737f699fd8b343478b5eff`，执行日志板端 `/dev/shm/nt-size-iou-external.log` SHA-256 `cc229cd5c46b68fa49e7c8c2f5cff8c49c152a0b44142e3c5c7cb2314b212b14`。实际 `python3 /tmp/nt-size-iou-score-external.py`，脚本 SHA-256 `2fe44fdb82a8a85e95f659c06ea67a5409ffcb966cf8780a2bf9c135facbac51`，摘要 `/tmp/nt-size-iou-external-summary.json` SHA-256 `2f4cf16dc53e809ca5129edb77d9b9c2dd4c4c1d8c2de3c5e8120608a8db48fb`。外部 ID14 清晰无截断帧 mean IoU `0.6342→0.6481`，中心误差 `12.08px` 不变，已接受 IoU<0.2 保持 0；其余 ID12/ID9/ID8 对同版逐帧评价均不变。ID12 的中心误差仍约 `184.72px`，该候选没有解决其身份/位置问题。
- 本提交是**实验分支的较优候选**，没有证据将 0.4 倍 IoU 系数推广到其他场景，也未解决 car004 的可见部分框；不合 main/Debug、不升级协调基线。后续应优先寻找可迁移的可见区域定位证据或不同场景逐帧真值，再决定是否采纳；仅有同一视频十目标不能证明泛化。板端 `/run`、`/dev/shm` 的证据会随重启消失，主机归档与 SHA-256 已保留。

## 2026-11-18T21:15:17+08:00 强中心小框尺寸门候选：RK3588 回放已完成，正式全量评分进行中

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，基线 `7ed12f8fe829573a59ed9950b3bb97b081ac8b77`。仅将 `select_tracking_size_candidate` 的强中心、更小 detector 框 IoU 门从 `0.5×detector_confirmation_iou` 调至 `0.4×`；保留中心包含、置信度、候选唯一性、两批次连续确认、发布尺寸每帧步长及 NanoTrack 内部裁剪/控制路径。增加 0.20 IoU、双批次确认且内部裁剪不变的单测。源码/单测 SHA-256 `c1465d13b9836327c6e7550a1c7e2a5d7c454a8d3e3790fdbbe6c2db60d3a3bf` / `eaefecc7c24e60cb881ac12377297139c12203919304d06a2925aeb5ec4e3f80`。主机 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure -j4` **23/23 PASS**，日志 `/tmp/nt-size-iou-host-{build,ctest}.log` SHA-256 `55a91f132a573a101c420a541472d84b0ab6d01b652856cac94a3545076b9052` / `5627f790a888a2b89e532665da05727fc1ee117288ce1d7633d19237a507c7b4`。
- 原因核查：从已验证 `973eb78` 源码在板端易失目录复制隔离诊断版，只增加候选几何打印；诊断源 `/tmp/nt-size-audit-long_term_runtime.cpp` SHA-256 `844e5198043d8b26ffeb093f483c74dda60ececeb9bf40f1a7c208eb7897aa4c`，板端程序 `/run/user/1000/nt-size-audit-build/nanotrack_v3` SHA-256 `43232f2d545dee0327a6744fc3faaa52ebf6418a4df73349159dcd602216ecfc`。实际回放外部 VisDrone `uav0000137_00458_v` ID14 233 帧，原始 `/dev/shm/nt-size-audit-run/{frames.csv,run.log}` SHA-256 `e40d8400fdae06c37390a8ed867f480c94aed67921be3863f449ebee46f0ac36` / `bdb42d6eccc98d22ab24df1b274fa6acfa16df29df963213b1aa71f95fbf0456`。第 153/156/159 帧同类 detector 目标框置信度约 0.80、中心含 tracker 中心，IoU 约 0.202/0.213/0.201，低于原强中心尺寸门 `0.25`；旁车不含中心。该探针仅解释尺寸确认中断，不是新算法验收。
- 候选在板端 `/run/user/1000/nt-size-iou-source/` 原生构建，实际 `cmake -S ... -B /run/user/1000/nt-size-iou-build -DENABLE_RKNN_RUNTIME=ON -DRKNN_INCLUDE_DIR=/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/include -DRKNN_LIBRARY=/usr/lib/librknnrt.so`、`TMPDIR=/dev/shm cmake --build /run/user/1000/nt-size-iou-build --parallel 2`、`ctest --test-dir /run/user/1000/nt-size-iou-build --output-on-failure -j4`，**23/23 PASS**。二进制 `/run/user/1000/nt-size-iou-build/nanotrack_v3` SHA-256 `2c85e0ce2f3c284212c52917a53db706e69012e6a02a23c073e4c6533befd740`；cmake/build/CTest 日志 `/dev/shm/nt-size-iou-{cmake,build,ctest}.log` SHA-256 `215ef5a53778564a195d80ac1a7f435e384c13692a05a207c308253395b3958d` / `567ca14feafc005598aed17fe777806e1fdfb87b9fcd8c270fba5091c4f539f5` / `159bbabc3ba89ca8047f6bea03ae932811cab8155998688a4f7a991ee9cdb6bb`。
- 实际 `bash /run/user/1000/nt-size-iou-focused.sh`（主机/板端脚本 `/tmp/nt-size-iou-focused.sh`），car002 真值/detector 各 271 帧及外部 ID14 233 帧，原始板端 `/dev/shm/nt-size-iou-focused/`、主机 `/tmp/nt-size-iou-focused/`，归档双方 `/dev/shm/nt-size-iou-focused.tar.gz` / `/tmp/nt-size-iou-focused.tar.gz` SHA-256 `ebf670ae09c1958a7cd02c2c38dcfa80f51ac350dbac24c5c74f558eec06934e`。car002 两路算法字段逐帧与 `973eb78` 一致；外部 ID14 清晰帧 mean IoU `0.63420→0.64813`、中心误差均 `12.08px`、已接受 IoU<0.2 均 0，第 165 帧发布宽 `157.43→121.62px`，仍比真值约 `68px` 宽。比较脚本/JSON `/tmp/nt-size-iou-{compare.py,external-compare.json}`，JSON SHA-256 `9a4959465e9ca92eb2fe57669844a36834dc7a1bee63fda946488ba5a318e6d6`。
- 实际 `bash /run/user/1000/nt-size-iou-full20.sh`，二十路各 271 帧，脚本 SHA-256 `a7e682aa2fca955928b78db5b4df43eab865132796a873e7eab9f81287a912b2`，板端 `/dev/shm/nt-size-iou-full/`、主机 `/tmp/nt-size-iou-full/`，原始归档双方 `nt-size-iou-full20.tar.gz` SHA-256 `2a134821144c363f7841354f8b466b7903d3a0b4cb2ab8fc28a75fb3816a72de`，板端执行日志 SHA-256 `5be3adee472fd0de5cad0e28a61c68fb7e94ac131012271fa26c81e949f39411`。沿用原视频、RKNN 模型、manifest/labels 与 `973eb78` 配置；新增真值边界按 car003/004/010 v3、car009 v2，car001 第 166–168 帧保留现行框。**尚待正式评分和额外外部三车回放；不晋级 main/Debug 或协调基线。** 板端 `/run`、`/dev/shm` 重启即失效。下一步评分二十路的所有正式 gate、中心误差、恢复帧及外部误缩框。

## 2026-11-18T20:45:00+08:00 未完成目标尺寸保持试验无实质收益，撤回源码

- 试验源码 `codex/ten-target-smoothness@125d9f9186510f9740e664829b5d2bc304480834`，工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，输入和配置沿前节 `973eb78`，仅变更已确认缩框目标尺寸的短时保存。主机/板端源码包 `/tmp/nt-centered-size-125d9f9-source.tar.gz` / `/dev/shm/` 同名 SHA-256 `b7f459e8798fa3a4c6a780571b7a66a0271fe03091d99001d88b76f4292efd07`；实际板端 `bash /run/user/1000/nt-centered-size-125d9f9-setup-run.sh` 和 `bash /run/user/1000/nt-centered-size-125d9f9-focused.sh`，脚本 SHA-256 `70d63af362fc0bccaf45c1f42aa3db5db074e423f0f0d305724c733929d0274f` / `e7a0c84b262e17048d4fdb232451ce7be3e4301c8366ab772733875b9469d6e2`。RK3588 原生 CTest **23/23 PASS**，二进制 `/run/user/1000/nt-centered-size-125d9f9-build/nanotrack_v3` SHA-256 `d41e51d7b9184f6d7762b171a01b198fadf167bb1fee2f927c732ec8d91fa8fc`，运行配置 SHA-256 `7cb20f97639db4a32324b067db26a6c36577a5bc23bc16c430c3ce5dc344c8fd`，cmake/build/CTest 日志板端 `/dev/shm/nt-centered-size-125d9f9-{cmake,build,ctest}.log` SHA-256 `22e27bb17ca793db4ab1725db3c49ac196cd47e671f410b0565f8e258e25c0c4` / `221cf9b98115b6491ea817645e4567dcddbcc80acb71375fa50bd3b26f54d664` / `59422b2f2c4c85462600ae5d9ac1fa8b9c7c235bf98ca0a6b16fdec5657b673f`。
- 实际聚焦 car002 GT/DET 各 271 帧、外部 0137/ID14 233 帧，板端 `/dev/shm/nt-centered-size-125d9f9-focused.tar.gz` 与主机 `/tmp/` 同名 SHA-256 均 `1458a4528d93b287790308b99f44641bdbcf3f8dfd69dfc88e477ad320c15483`，主机解包 `/tmp/nt-centered-size-125d9f9-focused/`；实际评测 car002 两路 `score.json/.md` 与外部 `python3 /tmp/nt-centered-size-125d9f9-compare.py`，脚本/JSON SHA-256 `ce23bfdfc47e0e4a6819c4069a9ed97c783ef37d68535bda4ebd956a1cbe9498` / `3f421af1e94dc11c2ba433e20979130f902f692ee0656e21c0b88219dd0f73a3`。与 `973eb78` 相比，car002 两路核心输出逐帧相同；ID14 仅 19 帧有不足 `0.001px` 的浮点差，mean IoU、中心误差、低 IoU 接受数及第 165–170 帧宽度无实质变化。因预期收益未出现，**不扩跑 20 路**，不作为新基线；`125d9f9` 的板端原始证据和提交保留。
- 本次把运行时与单测的该试验改动精确恢复到 `973eb78` 的算法内容；实际 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure -j4`、`git diff --check`，主机 **23/23 PASS**，日志 `/tmp/nt-size-target-revert-host-{build,ctest}.log` SHA-256 `d7203c20bedb4a12d28eae162a78c64afec357e3b821362e3fbcd23f25a28e23` / `73b880ffabf9d2bc5bb256a7f9215073bee0d1628b549feb35f9689380c852a2`。保留前节 `973eb78` 为最新已完整板测的实验候选；旧真值 14/20、v3 真值 18/20，car004 第 127–129 帧可见部分框仍失败。未改 main/Debug、协调基线或公共数据契约。

## 2026-11-18T20:42:00+08:00 保留已确认缩框目标尺寸：主机候选，待 RK3588

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，起点 `280c654eaa6461e20832b06d93180bbba2ddc333`；代码、单测和本记录同提交。前版 `973eb78` 对外部 0137/ID14 的框尺寸改善仍不足：最后确认的明显小 detector 框尚未达到中间目标尺寸时，代码只保存“当帧已发布尺寸”，短时掉检便停止继续缩框并逐渐回到 raw NanoTrack 尺寸。该目标第 165–170 帧已接受输出宽仍约 `157–160px`，真值约 `68–69px`，相同运行的显示 detector 框约 `89–90px`，但不能直接把显示框作为控制框。原始证据 `/tmp/nt-centered-size-973eb78-external/uav0000137_00458_v/target_14/frames.csv`，归档 SHA-256 `49b47f032da85010b589ab85d62f5ba6bde0c7d7f01b9b961a2418f17df898d4`。
- 本候选仅在前版已经通过同类、唯一、中心、强置信度、双批次及 detector 面积≤tracker 面积一半等条件时，暂存**已确认但受 5%/帧步长限制而未到达的尺寸目标**；短时掉检/进入 SUSPECTED 后沿原有时效衰减和步长继续接近该目标。没有新 detector 候选时不赋予身份或重捕资格，内部 NanoTrack 搜索裁剪和运动中心不读取发布尺寸；car002 的面积比约 0.61 不触发。单测增加掉检后下一帧仍向已确认较小尺寸前进的断言。实际 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure -j4`、`git diff --check`，主机 **23/23 PASS**，构建/CTest 日志 `/tmp/nt-size-target-retain-host-{build,ctest}.log` SHA-256 `2300550266cf06d713cc20052c9ba83fd1fbcfaa92db612e90ac34d58044b2c6` / `01a48ce988b12f6f5472652390e21352e6d5561906cfffe6255325dbd3126d7e`。**仅主机候选**；先板端 car002 双路与外部 ID14，再决定完整二十路及外部四车；旧版 14/20、v3 18/20 和协调基线不因主机测试改变。

## 2026-11-18T20:36:00+08:00 二倍面积门 RK3588 完整回放：十目标不退化，外部 ID14 改善

- 精确算法源码 `codex/ten-target-smoothness@973eb7876c30ba44e6ba61205fdae30ecd577734`，工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`；后续真值注册提交 `2c7b2f6a6d160d416b58a7f3525a644ee0568f01` **不改算法**。主机源码包 `/tmp/nt-centered-size-973eb78-source.tar.gz` 与板端 `/dev/shm/` 同名 SHA-256 `b97232c6dc5d0efd0806bd0105e3b0a8d0d87cd36f52867ac7a0b114058aa133`；原生源码 `/run/user/1000/nt-centered-size-973eb78/`、构建 `/run/user/1000/nt-centered-size-973eb78-build/`，这两处及 `/dev/shm` **重启即失效**。实际 `bash /run/user/1000/nt-centered-size-973eb78-setup-run.sh`，脚本 SHA-256 `d378328c2a7a72dad2e69bf63050b75f997fa713f0afb26a63d8bc63c07a7d14`；RK3588 原生 CTest **23/23 PASS**，二进制板端 `/run/user/1000/nt-centered-size-973eb78-build/nanotrack_v3` 和主机 `/tmp/nt-centered-size-973eb78-nanotrack_v3` SHA-256 均 `959f814de2d808524d507b59d70ce45efc27bd63d474c6d99b24cbf04053f807`。headless 配置 SHA-256 `f3a8cf017bd50f6f1c4d710a80314be9d827c94331e0f271c1872179c5c39f8f`，cmake/build/CTest 日志板端 `/dev/shm/nt-centered-size-973eb78-{cmake,build,ctest}.log` SHA-256 `72c836a5e495b1a649a05c84d55a3a2d17e84d5323c1b4133646c3980465132f` / `50c9e7ffa8d52e3e6a520555df82236937969ab9db69dabd4681cf397d3d53b7` / `84ad1314ec636fbd7d07a3dc95ed2e42b943ac2352d7fbfbaa5f74b7e1675d61`。RKNN API `2.1.0`、driver `0.9.8` 见运行日志；输入视频 SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`，tracker manifest/三模型 `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261` / `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55` / `d3cb0e3879df6a1272adb59bf501324f1f8f55b58309d3c4da4a731b600973ee` / `4da379f54093d79f02775e90bfea33ebee0837c6fc73a324499279b4c57807c5`，detector RKNN/manifest/labels `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c` / `ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9` / `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`，controller config `bb19d2e8e085dd57084653b6ca37bb981b10ea2a0d00bc05d166f8a508c6e26f`；本轮板端重新核哈希，均与既有基线输入相同。
- 实际 `bash /run/user/1000/nt-centered-size-973eb78-focused.sh`、`bash /run/user/1000/nt-centered-size-973eb78-full20.sh`、`bash /run/user/1000/nt-centered-size-973eb78-external.sh`，脚本 SHA-256 `2c22bd1eed0e18cd73cb0e38381b031b1dc1f1270d80cd367f97bbacbe56a1a3` / `66f3493c9300024412ff17e399fa4c4b69d946a9bc4e967bee2ee12531191d96` / `abf4c165d75a739a1f17993dd3e56ed3fe95abaef7ecc333c54d5980e52af109`；聚焦 car002 GT/DET 各 271 帧、外部 0137/ID14 233 帧；全十目标 GT/DET 各十路每路 271 帧；外部 0137 ID12/14 各 233 帧、0305 ID9/8 各 141 帧，所有运行 exit 0。板端 `/dev/shm/nt-centered-size-973eb78-{focused,full20,external}.tar.gz` 与主机 `/tmp/` 同名 SHA-256 依次 `a640508a30d63cc3d90dcb6b1273ddc7e4761c5b2bcbfa702fe727bc3361a1cb` / `ae73bb3b2b6ab8d4f4f3b5dbbe736023016055b05ceb3aa9cc39e4b13818409d` / `49b47f032da85010b589ab85d62f5ba6bde0c7d7f01b9b961a2418f17df898d4`；解包主机 `/tmp/nt-centered-size-973eb78-{focused,full,external}/`，各路原始 CSV/session/log 保留。实际 `python3 /tmp/nt-score-centered-size-973eb78-full.py`，脚本/日志/摘要 SHA-256 `4c53c424dc191cfcdd101ec6c1934af414ad641d7fc5ef54f21e91998ca2443a` / `613862ac5113e3b36ca9a2e8d3eff38edceeb39eb4d559b3c80f2f1715c0c18c` / `06fe5e76cd6c8c03394a8228efac6e1211d8fde58f7f629dcd291f2cbb275c79`；相对 `f422bea` 同旧真值口径 **14/20 PASS、门禁及重捕帧全部不变**，已接受可计可见帧 4699 不变，整体加权 mean IoU `0.835883→0.835961`，可见 SEARCHING 134 不变；car002 GT mean IoU `0.8723→0.8723`，第 20–25 帧发布框不再错误缩小。实际按用户确认的 v3 真值独立 `python3 /tmp/nt-score-centered-size-973eb78-boundary-v3.py`，脚本/日志/摘要 SHA-256 `15398afdd58d46b253a4e5200b57dc374cb5d50559a921d046ad94a403e20393` / `1cc621f5edc121635d3b8076c6cf2138e3e96740a7029e0f0275e0e1fd2abe83` / `5e3ffe6c4c66cc443d03d59fd55960d99e25467711f124d013a02040893857b6`；**18/20 PASS**，仍失败 car004 双初始化第 127–129 帧可见部分框。此变化仅由真值版本引起。
- 外部实际 `python3 /tmp/nt-score-centered-size-973eb78-external.py`，脚本/日志/摘要 SHA-256 `fe91f6c984c3ba5f7db32a9b094e6baa3334bda56eab64402eadda33857183db` / `de37f3cfc1acaed4925b5ede12507e5a515c546cd1b44871a81bdd0f28f24229` / `9877d39d4ceb55d124a7c35d7e8a7a90a278effcc3a5631a926bb65102147f83`。与 `f422bea` 同目标成对比：0137/ID14 清晰未截断 mean IoU `0.5928→0.6342`、平均中心误差 `12.08px` 不变、SEARCH 0 不变、已接受低 IoU<0.2 `5→0`；0137/ID12 和 0305/ID9/8 的核心指标逐项相同。0137/ID14 第 113 帧 `75a22fa` 的单帧发布宽度回弹 `17.7%` 被限制至约 `0.6%`，但后段第 165–170 帧发布宽度仍约 `157–160px`，真值约 `68–69px`，尚不够紧。
- **裁决/Handoff**：保留 `973eb78` 为有外部收益且十目标无已知新回归的**实验候选**；car004 可见部分框及外部 ID14 后段尺寸仍未解决，不把它升级为协调基线，不合 main/Debug。已准备但**未 GUI 人工测试**的板端 `/run/user/1000/nt-centered-size-973eb78/runs/interactive-on.conf` 与主机 `/tmp/nt-centered-size-973eb78-interactive-on.conf` SHA-256 均 `9bc8811c5623e51a14d3032c207597a48a99cecd6965220466bc54005815e3e7`；板端运行前先 `mkdir -p /dev/shm/nt-centered-size-973eb78-manual`，在板端桌面会话执行 `/run/user/1000/nt-centered-size-973eb78-build/nanotrack_v3 --config /run/user/1000/nt-centered-size-973eb78/runs/interactive-on.conf`。板端二进制/配置在易失目录，主机源码包/二进制及原始证据哈希如上可复建；吊舱验收仍需人工。

## 2026-11-18T20:19:00+08:00 三处用户确认的部分可见边界：独立真值版本与重评

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，起点 `973eb7876c30ba44e6ba61205fdae30ecd577734`；本节仅增加版本化真值制备脚本、评测器版本注册和本任务记录，**不修改任何算法或旧真值包**。用户确认 car003 第 130 帧、car004 第 131–132 帧、car010 第 156–157 帧是部分可见且暂无可计 IoU 的可见部分框；car010 第 149–155 帧原 v2 语义不变。实际 `python3 scripts/prepare_occlusion_boundary_visible_unboxed_v3.py --source-root /home/u20/single-object-tracking/data/occ_video_1 --output-root /tmp/nt-occlusion-boundary-v3-final`，从 car003 原包、car004/010 v2 包分别复制生成 `car_003_visible_unboxed_v3`、`car_004_visible_unboxed_v3`、`car_010_visible_unboxed_v3`，只改确认帧为 `visible/user_confirmed_visible_unboxed`，同步事件起点、状态计数、历史、README 与逐文件哈希。三份包并列安装到 `/home/u20/single-object-tracking/data/occ_video_1/ground_truth_versions/`；`checksums.sha256` SHA-256 依次 `282d0dcb4b018610deae9912e95415e264f59c2490bd08db1f7f4733171f0337` / `caf198ced5205640e0899913da16a21947be951991b5e0c37e4019ee6dbe3957` / `e5ea0ef17756cb8c4eb765ec558efb4bcab84060efe4ae667eabb4a074910510`。备份 `/tmp/nt-occlusion-boundary-v3-packages.tar.gz` SHA-256 `eb2b2d128b1981463ecae5c77663fcf1fbc609b4b79839d21aeaaeec0ba29c5a`；制备脚本/评测器 SHA-256 `befdf3d8b95350bdc09eaca658788eab7c5ab9da67e8060f1131e351af13ccc8` / `3b4183688e695bab329968219234ce57760dbf22ba1a46d4e420f52fa0e26b08`。
- 实际对安装后的三版运行 `load_truth(load_contract(...))`，271 帧且上述修订帧均 `visible/observable=false/bbox=None`；`python3 -m unittest tests.test_evaluate_occ_video_1 -q` **26/26 PASS**。同一份已保存的 `f422bea` RK3588 20 路 CSV，只对受影响 car003/004/010 双初始化实际 `python3 /tmp/nt-score-occlusion-boundary-v3.py` 另存 `score-boundary-v3.json/.md`；脚本/摘要 `/tmp/nt-occlusion-boundary-v3-rescore.json` SHA-256 `cd299337a2113af8ead93fe5107e3192e92e268ed2930e2b41779ec2d9f48ba1` / `a33e7dcd8353d9ed7db4fad8fbe435bfc0975e72f4b924135121b847c2913ad6`。car003 GT/DET 130 帧全遮挡 gate 消失，两路 FAIL→PASS；car010 GT/DET 156–157 帧全遮挡 gate 消失，两路 FAIL→PASS；car004 GT 131–132 帧全遮挡 gate 消失，但 GT/DET 仍因 127–129 可见部分框 FAIL。全二十路按 v3 口径由旧 **14/20→18/20 PASS**，**这是同一程序输出的真值语义修订，非算法改进**。旧 score.json、原包、v2 包均保留。
- **Handoff**：请协调对话核验三版 package manifest 与本提交，再决定是否将 v3 真值登记进 `docs/progress/integration-contract.yaml`；本任务不改公共契约。后续算法回放同时报告旧口径和 v3 口径，car004 第 127–129 帧仍需可泛化的可见部分框定位证据，不通过擦除可见帧取得 PASS。

## 2026-11-18T19:08:00+08:00 二倍面积差方可缩框：主机候选，待 RK3588

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，起点 `2c64d78ec5f2e5e241ac1a3666d6acaee8194475`；本候选代码/测试/本记录同提交。前版 `2c64d78` 板端隔离源码 `/run/user/1000/nt-centered-size-2c64d78/`、原生二进制 `/run/user/1000/nt-centered-size-2c64d78-build/nanotrack_v3` SHA-256 `5f66dc7a1491915012ed1804bf6ba38419d0bb3d4790ac2f29e1a641a4de1ae4`，原生 CTest **23/23 PASS**；配置 SHA-256 `0ab6536789fcf3b319a74aeb73beb9475c907cb464eecd3ceff696e91c5643b4`；源码包 `/tmp/nt-centered-size-2c64d78-source.tar.gz` 与板端 `/dev/shm/` 同名 SHA-256 `bbf3d6b2c9de8ea672cee5abad5019f3530cf4d71907a4d5672fbd602f5fb481`。setup/focused 脚本 SHA-256 `590e5ef1900423010db6580b25e8e70808f813b9bca8b35190fbc28aaff87edf` / `c9151ad0efed3f766583925e48c0e12132f1ef40968bc24ac62472b710237e94`。实际 `bash /run/user/1000/nt-centered-size-2c64d78-setup-run.sh`、`bash /run/user/1000/nt-centered-size-2c64d78-focused.sh`，GT/detector car002 各 271 帧、0137/ID14 233 帧；原始包板端 `/dev/shm/nt-centered-size-2c64d78-focused.tar.gz` / 主机 `/tmp/` 同名 SHA-256 `e5fa7ff424b211d783317a58485bbaac5afc24d6a693d2ef215f4546fc03a2f8`，主机解包 `/tmp/nt-centered-size-2c64d78-focused/`。
- `0.65` 面积门仍会纳入 car002 的 detector 框约 `34×31px` 与 Recent 原始框约 `41×42px`（面积比约 `0.61`），第 20–25 帧发布宽度 `42→38px`；GT car002 mean IoU `0.8654`，相对 `f422bea` 的 `0.8723` 仍退化；detector 初始化 car002 `0.8068`，两路正式 gate PASS。0137/ID14 相对 `f422bea` mean IoU `0.59277→0.64447`、低 IoU 误接受 `5→0`；第 113 帧输出尺寸平滑维持。实际外部成对比较脚本/JSON SHA-256 `b82d7285776a2d84775af1f5b0d5e5e850f103083274a7fd0d4e5daf18c6ae0e` / `7cfce9381e3489cddd9cf58e0253f12166f28cf9b2a860a6768b1714f00f9924`，car002 两份评分在上述主机运行目录。
- 因此把收缩门改为 detector 面积≤原 tracker 面积 `0.50`，要求至少二倍面积差；保留既有双批次、同类、置信度、中心等条件及 detector 较大时的原路径。合成单测把 16×16 对 20×20（面积比 0.64）作为拒缩反例，12×12 正例保持。实际主机 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure -j4`、`git diff --check`，**23/23 PASS**；日志 `/tmp/nt-half-area-size-host-{build,ctest}.log` SHA-256 `04934eb69089e679edc2aab98ab915ead9471687a5b0cdec87b86f30079bbd15` / `a5a0dc12db92bcad6f8789c35de397b41feaf60622cdde33b1db6bb53bc924b0`。下一步板端聚焦 car002 双路和 0137/ID14，成功后再补齐 20 路与其他外部车；尚非新正式基线。

## 2026-11-18T19:03:00+08:00 缩框要求明显尺寸分歧：主机候选，待 RK3588 反例回放

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，起点 `346cbac19352baff77848bba5bc965c3e96c7cec`；候选源码、单测和本记录同提交。上版 `346cbac` 板端 `/dev/root` 满导致首次解包失败，已仅删除该次不完整抽取目录，随后把相同源码置于 `/run/user/1000/nt-centered-size-346cbac/`、构建置于 `/run/user/1000/nt-centered-size-346cbac-build/`（可执行、重启后失效），`TMPDIR=/dev/shm/nt-centered-size-346cbac-tmp`。源码包主机及板端 SHA-256 `ae9ed2ff97f80556537c95a63130cd27f9b8f5269bb96847b0f2955fb16f637b`；实际板端原生 CMake/build/CTest **23/23 PASS**，程序 SHA-256 `d53c01d7454502c410e777de7441933905f4ea970d7ddd872ac87db7b73f653f`，配置 SHA-256 `f5845a627b15d9be2df9ec732501843fa48f0f7c95a5efaf40e60cd7e9b99102`，cmake/build/CTest 日志 `/dev/shm/nt-centered-size-346cbac-{cmake,build,ctest}.log` SHA-256 `93c0c935c14db33bf906a0ad64b7956aad928ce76efcdbb68665365ebf9be626` / `3c40fbdceaab52e2fd8372b87d0625c8ab8b704f74dcabdafd729e892883f516` / `ec025787eb91b3ea528449f489d9cee23e85238a81bc32ed8522fa471d13c21d`。
- 实际 `bash /run/user/1000/nt-centered-size-346cbac-focused-run.sh` 与 `bash /run/user/1000/nt-centered-size-346cbac-full20.sh`，脚本 SHA-256 `0f17ace246a9b11b9a0ef69e838e5f0616f6dcb5ddeff0c86f74c32e4aefeceb` / `a48ee8616cbf7c74fbe777e45c496a073aefd24c009a129b379cf9a03c464d0d`；前者八路 canonical 加 0137/ID14，后者复用八路原 CSV 后补足十目标×真值/detector 初始化 20 路，每路 271 帧、exit 0。聚焦与完整原始归档板端 `/dev/shm/nt-centered-size-346cbac-{focused,full20}.tar.gz`、主机 `/tmp/` 同名，SHA-256 `6171ac30ae23e27c8ba3e90193750194060fd83ec489921c4d523bba24a232b1` / `d5af87ed9b8814be496e9de5cf6f8868f744e1c11e8a0a3879f7eb9a784a3e69`；主机解包 `/tmp/nt-centered-size-346cbac-{focused,full}/`。使用各目标对应的现行真值版本，实际 `python3 /tmp/nt-score-centered-size-346cbac-full.py`，脚本/输出日志/摘要 SHA-256 `61677618263303ed13308f9b1e4cd1f5ee79ebe27c1d2957a28c8d7f503a5f96` / `b8fc5365ea326a14e2a2a95b3922d2cea9f9ae373d9829782a4fead232931f13` / `8d574fb4d201ba54666dbc2c3fd13391d0765804d0f59ce734cc41c7d421c79f`。相对 `f422bea`，正式 **14/20 PASS、所有 gate 与重捕帧不变**；0137/ID14 清晰帧 mean IoU `0.59277→0.66419`、中心误差 `12.08px` 不变、已接受 IoU<0.2 从 5 帧降为 0，第 113 帧发布宽度突变由上一缩框候选 `17.7%` 降至本版约 `0.6%`（成对比较脚本/报告 SHA-256 `97194b2d3d85b9843cf279572802dca2e9ea936fbe86071f791c8a48813ca46b` / `7724953793c5c7bef964e3a13c13aad71602502c75307566a93a5461187700f8`）。
- **反例**：car002 GT 初始化可计已接受平均 IoU `0.8723→0.8534`；第 20–48 帧原约 42px 的真值宽被 detector 支持压到约 38px，第 22–23 帧 IoU 各下降约 0.20。detector 初始化 car002 也从 `0.8065→0.8014`。这类小尺度差异未证明 tracker 膨胀，不能接受为普遍缩框。新候选仅当被连续确认的 detector 框面积≤原 NanoTrack 框面积 `0.65` 时允许缩小发布尺寸；面积比更接近时保持 tracker 尺寸，较大 detector 的原有纠偏不变。单测增加居中 18×18 检测对 20×20 tracker 不缩框反例，原 12×12 明显缩框及掉检/SUSPECTED 平滑正例保留。主机实际 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure -j4`、`git diff --check`，**23/23 PASS**，构建/CTest 日志 SHA-256 `8e7cce08901dc5fbda740b93d1774a65df3b9535ef6d456c77d9148a3a4a47c8` / `6f91a0bc85c4694c04978ce94e908938c84d39eb979f6b84f4f023d79b7198df`。仅主机候选；下一步板端先回放 car002 双路与 0137/ID14，若修复反例且保留收益再跑 20 路和其他外部目标。未升级正式基线、不合 main/Debug、不交生产。

## 2026-11-18T18:44:00+08:00 检测缩框的短时保持：主机候选，待 RK3588 聚焦回放

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，起点 `75a22faada46f57c79253c9143dc7d5274bee723`；源码、单测和本记录同提交。前版 `75a22fa` 已在 RK3588 原生编译、CTest **23/23 PASS**，程序 `/run/user/1000/nt-centered-size-75a22fa-build/nanotrack_v3` SHA-256 `bfadabfe147f01ad97d9914217fd3263870d0133cbd9967673e477e6b11b6e1e`。实际板端聚焦脚本 `/home/neardi/Project/SOT/nt-centered-size-75a22fa-focused.sh` SHA-256 `fbd791137e13fbbeec4e436a33dfe3bbaad2e120d9c6e627426c589d46f02e3a`；八路 canonical 与外部 0137/ID14 原始文本归档板端/主机 `/tmp/nt-centered-size-75a22fa-focused.tar.gz` SHA-256 `ab5ed8872b409aa5c2b45c79ed39734cde282d31f4690c370b2a7e44ca21f7fe`，解包 `/tmp/nt-centered-size-75a22fa-focused/`；评分摘要 `/tmp/nt-centered-size-75a22fa-focused-summary.json` SHA-256 `658a63a16e34a925109dd1578a9b2532d651b1fb4046881a2242e6bad4a23609`。八路正式 gate 均不变；外部 0137/ID14 清晰帧 mean IoU `0.59277→0.64576`、中心误差不变，但第 165–170 帧的五个低 IoU 仍在。该候选第 113 帧从 TRACKING 转 SUSPECTED 时，发布宽度 `168→197.7px` 突变，且 detector 缺席后缩框很快回弹；不能仅凭 mean IoU 升基线。
- 本候选保留最近一次可靠 detector 双批佐证的较小发布尺寸，在 detector 短暂缺席时按现有 stale/refresh 上限退化；TRACKING、SUSPECTED、SEARCHING 状态间的尺寸过渡按配置步长限幅。只作用 `tracking_bbox`/`control_bbox` 的发布尺寸，保留中心；内部 NanoTrack 搜索框和身份判定不读此值，逐帧 note 增 `DETECTOR_SIZE_RETAINED`。合成单测覆盖检测缺席及随后 SUSPECTED，不允许尺寸跳变或控制中心漂移。主机实际 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure -j4`、`git diff --check`：**23/23 PASS**；日志 `/tmp/nt-centered-size-retention-host-build.log` / `/tmp/nt-centered-size-retention-host-ctest.log` SHA-256 `a5d90d2d19a08e3ff45ba7f6c3bb877d3f4ccb647b649406c88d0b9abef2200e` / `2b80f6a73fef13ac217c5548297f9ccb9976e2297d785949db5359acd29bcc83`。
- **证据边界/下一步**：尚无此新源码的板端结果，不改变正式基线或 main/Debug。先 RK3588 原生构建和 0137/ID14、car001/004/007 聚焦对照，检查低 IoU 接受、尺寸 P95/最大步长、控制中心及各 gate；聚焦无回归后再跑完整 20 路与四条外部轨迹。用户再次确认 car010 第 149–155 帧为部分可见、无框、暂不计 IoU；现有 `car_010_visible_unboxed_v2` 已精确满足此语义，原包不覆盖。car003 第 130 帧、car004 第 131–132 帧、car010 第 156–157 帧仍待独立核对，不把真值变更当算法改善。

## 2026-11-18T18:23:22+08:00 居中双次检测允许受限尺寸收缩：主机候选

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `a3ab73f374b3bf75747437e2d24fa6419b6d27c2`，源码与本段在同一提交。为防只按十目标视频调参，实际在 RK3588 现有 VisDrone `uav0000137_00458_v`、`uav0000305_00000_v` 取另外四辆车做基线 `8fb939d` 与候选 `f422bea` 成对回放：选择条件为首帧无遮挡、轨迹连续、首框宽高均不少于 25px、至少 60 帧，按前 250 帧遮挡次数取每段前两辆此前未测的车，得到 0137 的 ID 12/14 与 0305 的 ID 9/8。板端实际 `bash /home/neardi/Project/SOT/nt-visual-reanchor-f422bea-extra-paired.sh`，脚本 SHA-256 `5c4fc39dff03366245f24099618d5fa288a4a7339196504f6f44c7b2b97ce2b5`；同一控制器配置内容 SHA-256 `bb19d2e8e085dd57084653b6ca37bb981b10ea2a0d00bc05d166f8a508c6e26f`。板端/主机文字证据归档 `/dev/shm/nt-visual-reanchor-f422bea-extra-paired-text.tar.gz` / `/tmp/nt-visual-reanchor-f422bea-extra-paired-text.tar.gz` SHA-256 均 `2b68e5ddd799e1e1e2808b47b22e4e720e5a8a191c331bbd07b17fbfef046796`，主机解包 `/tmp/nt-visual-reanchor-f422bea-extra-paired/{baseline,candidate}/`；每路原始 `frames.csv/session.json/run.log/config.conf`。实际 `python3 /tmp/nt-score-visual-reanchor-f422bea-extra-paired.py`，脚本/摘要 SHA-256 `129d57aae8c7ec74a14dd5470df9a6a9b6f1d8976ca2c17adeaf206254f21d6c` / `d4f7dbfb87b5bedd9bfb8c1cccc9c0bad74847b214f15baa53a44261e8db163c`。
- 0137/ID14 第 16 帧候选同帧 detector+可靠 Recent 视觉佐证命中原车、置信度 `0.863281`，第 41 帧再次命中 `0.874023`；基线第 20 帧已 SEARCHING、第 59 帧误重捕至另一辆车。原标注清晰未截断帧 mean IoU `0.0460→0.5928`，中心误差 `1103.29→12.08px`，SEARCHING `20→0`，已接受且 IoU<0.2 的帧 `112→5`。逐帧对照图 `/tmp/nt-extra-target14-visual.png` SHA-256 `d20fccdae79187c220627037bf314b0e2c3106da1d6c004990366c6ff1ad0b1b` 显示基线从黑车漂到近处银车，候选维持黑车；另三辆目标的清晰帧核心指标与基线相同。**这一收益属于候选 `f422bea`，不能归因给下面的新尺寸改动。**
- 同一正确轨迹后段仍有泛化问题：0137/ID14 第 165–170 帧真值车宽约 `68–69px`、高 `50px`，已接受 `tracking_bbox` 宽约 `165–168px`、高 `104px`，五个清晰帧 IoU<0.2；Recent 原始框也约 `159×104`，而 detector 支持的 display 框约 `90×69`。现有 `select_tracking_size_candidate` 要求 IoU≥`0.8×detector_confirmation_iou`，较小而居中的检测框因面积比被拒；现有发布尺寸 `max(raw, detector)` 只能放大，不能以已确认检测收缩。
- 本候选仅在全目标模式、同类唯一检测、Recent 可靠、检测批次新鲜、两批相互一致的既有条件下，允许**更强置信度≥0.5、较小框同时包含 tracker 中心、相对中心距离≤既有门槛一半、IoU≥既有门槛一半**作为收缩证据；发布框按原有每帧最多 `5%` 的尺寸步长靠近检测与 tracker 尺寸的中间值，并在短期 detector 支持窗口内保持收缩结果。内部 NanoTrack 搜索裁剪、身份恢复和运动速度均不读该发布尺寸；控制框仍与已接受发布跟踪框一致。合成测试增加小而居中的双批次检测成功，以及低置信度、偏心、极小框不生效反例。实际主机 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure -j4`、`git diff --check`，23/23 PASS；构建/CTest 日志 SHA-256 `d28b105869a08c14edf75d2338858286cd63e258a5800382d03a87657c0cd05a` / `9e7c4409429963f594d0b5aba2dff8be15112a0f4860187dca9fce7802edcc72`。**仅主机候选**；必须先板端检查 0137/ID14 与 car004、car001/007，然后完整十目标双初始化及外部成对回归，不因单目标缩框就升级基线。

## 2026-11-18T18:04:40+08:00 强 detector 视觉佐证：二十路与外部两段 RK3588 验证

- 为人工复核另备同控制器参数的板端 `/home/neardi/Project/SOT/nt-visual-reanchor-f422bea/runs/interactive-on.conf`，主机 `/tmp/nt-visual-reanchor-f422bea-interactive-on.conf` 与板端 SHA-256 均 `f892f516cca119376a2c316dc381ac1e92ab3258d39ce16f85497df2d01a5a19`。它仅从已实测 headless 配置打开 `interactive=true`，输出改到 `/dev/shm/nt-visual-reanchor-f422bea-manual/`；尚未做 GUI 人工点击验收。板端可先 `mkdir -p /dev/shm/nt-visual-reanchor-f422bea-manual`，再在候选根运行 `./nanotrack_v3 --config runs/interactive-on.conf`。`/dev/shm` 输出重启后消失，需人工另存。
- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，候选源码 `f422bea2ed555d927cbc6cf6a42fb6d868240344`。主机/板端源码包 `/tmp/nt-visual-reanchor-f422bea-source.tar.gz` / `/home/neardi/Project/SOT/nt-visual-reanchor-f422bea-source.tar.gz` SHA-256 均 `745725099cd349dadf08e52606a3cda07314c352b6897f3e2839042d60465f99`；板端隔离根 `/home/neardi/Project/SOT/nt-visual-reanchor-f422bea`。实际 `bash .../nt-visual-reanchor-f422bea-setup.sh` 初次 `cmake --build` 因 `/dev/root` 下的编译器 `/tmp` 满而中止（不是测试失败）；随后 `TMPDIR=/dev/shm/nt-visual-reanchor-f422bea-tmp cmake --build /run/user/1000/nt-visual-reanchor-f422bea-build --parallel 3` 及 `ctest --test-dir /run/user/1000/nt-visual-reanchor-f422bea-build --output-on-failure` 成功，**23/23 PASS**。主机 `ctest --test-dir /tmp/nt-ten-target-build --output-on-failure -j4` 同为 23/23 PASS，已见上一节主机日志哈希。板端程序已从临时构建目录复制到持久路径 `/home/neardi/Project/SOT/nt-visual-reanchor-f422bea/nanotrack_v3`，两处 SHA-256 均 `605f86699cd4a05229021484a7046d8e919f1f6b70564e4e5002d45244d6b0e7`；运行配置 `runs/headless-on.conf` SHA-256 `bae7f93365f193c6674afcd78558c81583a53cc1ce2f69b6d767b9518e897377`，板端 `build-retry.log` / `ctest.log` SHA-256 `386185be13436431e73288d46b77c3143af08e7f3191e1c1e811bf171036149d` / `ae826eaca322f7e4c666646a032402b03d17ea9aa83e6ea3e9b3666a36c03285`。板端 Linux `6.1.84`，RKNN API `2.1.0`、driver `0.9.8`，可从 `full-gt/car_004/run.log` 复核。
- 输入视频 `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；tracker 模型 `template_backbone.rknn` / `search_backbone.rknn` / `head.rknn` SHA-256 `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55` / `d3cb0e3879df6a1272adb59bf501324f1f8f55b58309d3c4da4a731b600973ee` / `4da379f54093d79f02775e90bfea33ebee0837c6fc73a324499279b4c57807c5`，`models/manifest.json` SHA-256 `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`。detector `yolov8n-visdrone9-rk3588-fp16.rknn` / `detector-manifest.json` / `visdrone9.txt` SHA-256 `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c` / `ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9` / `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`。
- 实际 `bash /home/neardi/Project/SOT/nt-visual-reanchor-f422bea-focused.sh` 后 `bash /home/neardi/Project/SOT/nt-visual-reanchor-f422bea-full20.sh`，脚本 SHA-256 `bd8a15ffc64c3ff28fa80763ac6fbe3c1752bfa5e0be0dcc337085212a9ebc8e` / `8e4efbd06c73c30687bd5a0567dbb797e16b8b74148b916c05a57592e074a65f`；十目标分别首帧真值与 detector 初始化，20 路每路 271 帧，exit 0，八路原始数据直接复用。板端 `/dev/shm/nt-visual-reanchor-f422bea-full20.tar.gz` 与主机 `/tmp/nt-visual-reanchor-f422bea-full20.tar.gz` SHA-256 均 `40e71f1a2cd4f1cb58be218ad2a3c6d93344bc75a4244e5433901e0a3bd86060`；解包 `/tmp/nt-visual-reanchor-f422bea-full/full-{gt,det}/car_###/`，每路 `frames.csv/session.json/run.log/score.json/score.md`。实际 `python3 /tmp/nt-score-visual-reanchor-f422bea-full.py`（脚本 SHA-256 `9ae2b3224cfd1e49f0f2bc085c2213582c101936030da390a9c5b51e70e1d345`），摘要 `/tmp/nt-visual-reanchor-f422bea-full-summary.json` SHA-256 `f3da5a94b00d44592657a3c55a49aeb8a4623537e8ed8468cd69111a1b3a1d71`，car004/009/010 使用用户确认的可见无框 v2。相较 `8fb939d` 正式基线，**14/20 PASS 不变，全部正式 gate 与重捕帧不变**；car003 双初始化的 206 帧维持 SUSPECTED、242 帧 SEARCHING 且无错重捕，之前候选新增的 209–211 搜索帧消失。可见帧 SEARCHING 总数 `177→134`，可计 IoU 且已接受的可见帧 `4683→4699`；car004 GT/DET `39→23` / `37→18`、car005 双路 `15→11`。总体已接受帧 mean IoU `0.83659→0.83588`，主要由新增困难帧造成：原先共同接受的 car004 GT、car005 双路 IoU 逐帧不变；新增 car004 六帧每路 IoU `0.61–0.72`、中心误差 `3.6–10px`，car005 两帧每路 IoU `0.51–0.62`、中心误差 `6.9–12.1px`。car004 可见部分框 127–129 仍 FAIL，遮挡边界 car003 130、car010 156–157 仍待核实。
- 实际 `bash /home/neardi/Project/SOT/nt-visual-reanchor-f422bea-external.sh`（脚本 SHA-256 `5d3a4dddec31770f94439e0db7d044c2ec5b86ad109effc1230f864f096cde75`）额外回放 VisDrone `uav0000305_00000_v` 141 帧、`uav0000137_00458_v` 233 帧，配置 SHA-256 `cefa03ed065e8932ed63c65df869529454acb9ce8fd4feb43267bc0e05a13965` / `419ae6378d3bfd4aaac7ead1bba55f9d8e2b560e6e62258b9f520e1c0b23b06b`。板端 `/dev/shm/nt-visual-reanchor-f422bea-external.tar.gz` 与主机 `/tmp/nt-visual-reanchor-f422bea-external.tar.gz` SHA-256 均 `9fb0f3d19b5cb0bd4f0031d577332247484c7844a4ca6088fc6d01c6bfdf1252`；实际 `python3 /tmp/nt-score-visual-reanchor-f422bea-external.py`（SHA-256 `5b08371247b6c01877d9086d9cd087b38749a3360bf86296139844f574f4b956`），摘要 `/tmp/nt-visual-reanchor-f422bea-external-summary.json` SHA-256 `d518a6646b4d75a0506fa8214877e242f9b9b821af44bc99e37f97624dd37b9b`；两段与 `8fb939d` 基线的清晰帧 IoU、中心误差、SEARCHING、低 IoU 接受数和重捕帧完全一致。两段同属 VisDrone 场景，只作为独立回归；不能证明跨域泛化。
- **Handoff/下一步**：此候选减少了可见目标不必要的搜索时间，未产生已知新门槛失败，可供继续人工验证；但六路正式失败仍在，且没有用户提供的不同场景逐帧真值，**不升级正式基线、不合 main/Debug、不交付生产/吊舱验收**。优先完成 car003 130、car004 131–132、car010 156–157 的遮挡状态核对，并寻找跨目标可靠的“可见部分框”定位依据，避免按单场景特征裁框。

## 2026-11-18T17:46:20+08:00 视觉佐证保速板端完整回放与较强置信度候选

- `0b054fe66a25f42cd2f09ee34cf980e08a8791cf` 源码包 `/tmp/nt-visual-reanchor-0b054fe-source.tar.gz` SHA-256 `490e529261e9c91e30aea32dec1129ce9eb31807fb9c2d1f57c896089a81a624`；板端 `/home/neardi/Project/SOT/nt-visual-reanchor-0b054fe` 源码、`/run/user/1000/nt-visual-reanchor-0b054fe-build/nanotrack_v3` SHA-256 `8ae0da4b42df93b5338508d78a931d0b82187d4e4541c96c5144c82ebf5cd461`、配置 `runs/headless-on.conf` SHA-256 `a836715db6b55156ce1f1d3c2451b3ce6bdc383f3a747be33e36424b79d140d8`。输入板端 `nanotrack_v3_debug_main-1bf657f/data/occ_video_1/1.mp4`，模型 SHA-256 `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`。原计划在 `/dev/shm` 构建，但其挂载 `noexec` 导致首次 CTest 22 个二进制无法启动；改在可执行的 `/run/user/1000` 重建，实际板端 CTest **23/23 PASS**，记录 `cmake-run.log` / `build-run.log` / `ctest-run.log` SHA-256 `da29f2c69df60a39c5f8d2a77511875e4cfc4e9747cb733397e0225e6aea7432` / `00f172303ea5c8b24e361954b9cfe608185fea352a965c30f40dbab4bf69b559` / `e5f3eb1256a7cd8836c84e7141669c6575b40cd11fbf4b1ac12a62427986ab3d`。
- 实际 `bash /home/neardi/Project/SOT/nt-visual-reanchor-0b054fe-focused.sh` 跑八路，脚本 SHA-256 `786cf74499edbd7a1acf4ecdde44dae9cabccb984f0f11033509350f64f5e7f0`；再 `bash /home/neardi/Project/SOT/nt-visual-reanchor-0b054fe-full20.sh` 补齐十目标×首帧真值/detector 初始化，脚本 SHA-256 `5e6e26762a278e97fd2814c6f7f3e5897ac7ed2057112678a60f939147e65`，八路原始记录逐字节复用，每路 271 帧、exit 0。板端及主机原始归档 `/dev/shm/nt-visual-reanchor-0b054fe-full20.tar.gz` / `/tmp/nt-visual-reanchor-0b054fe-full20.tar.gz` SHA-256 均 `9706ea27a64fb3b8d7c2c23ea6678ba27a3db8bf83cad8b14a4160d365f07153`；解包 `/tmp/nt-visual-reanchor-0b054fe-full/full-{gt,det}/car_###/`。实际 `python3 /tmp/nt-score-visual-reanchor-full.py`，脚本/日志/摘要 SHA-256 `80b3110c19350c3dd128e72b75986640b70b4290d1c7b8ed645ba` / `19c3353953805ce2af155ac9d2f36e70263b66792826934eb9c24b0a9b6a1fe4` / `7a7bf56539a744d80276de28b8e30024618976868b08bc670885f7e173e5a307`；使用各目标规范真值，car004/009/010 用可见无框 v2。
- 相对 `8fb939d` 基线，正式门仍 **14/20 PASS**，没有门槛变化；car003 双路第 242 帧保持 SEARCHING、未发生上一候选的离画错重捕。可见帧 SEARCHING：car004 GT `39→23`、DET `37→18`，car005 双路 `15→11`；但 car003 双路 `10→13`，且 car004/005 已接受帧 mean IoU 约降 `0.0025–0.0051`。car004 第 127–129 帧可见部分尺寸仍不贴真值；car003 第 130 帧、car010 第 156–157 帧遮挡语义仍待真值核对。此版有局部收益但未升级基线，也未合并 main/Debug。
- 逐帧差异表明 car003 第 206 帧唯一视觉佐证置信度仅 `0.403564`，随后新增 SEARCHING 第 209–211 帧；car004 的关键佐证置信度 `0.507–0.842`，car005 `0.793–0.868`。本次进一步试验将视觉**解除怀疑**所需的 detector 置信度定为 `max(visual_suspect_min_confidence, 0.5)`；普通遮挡探针/检测仍用原配置阈值，且保留 Recent 可靠性、响应独特性和可信速度条件。合成测试新增 `0.45` 置信度只触发遮挡搜索的反例。实际主机 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure -j4`、`git diff --check`，23/23 PASS，构建/CTest 日志 SHA-256 `52e200ab09dd2bfe78235c8bf22bb26a898777250ce7285affc12bd089045043` / `6c18f9bda12ee962401976364e28b4bcdf6bef4efef90d758fe37da6ebeb34ea`。本段后者仍是**仅主机候选**，需同样完整板端回放及外部场景检查。

## 2026-11-18T17:24:52+08:00 视觉佐证只重锚位置并保留可信速度：主机候选

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `d74b051cbc8dda8b5898dcc067367fa477c78a63`，本节与源码/测试同提交，提交号以 Git 为准。对上轮完整二十路原始 CSV 精确核查：被否决的 `adb7914` 在 car003 第 206 帧视觉佐证后直接接受并向运动模型写入观察；随后搜索预测由左移转为右移，第 242 帧把离画后的另一辆车误重捕。该路第 242 帧旧预测与上帧输出均贴画面下缘，而候选回到图内，现有半对角线位移保护恰未触发。`8fb939d` 基线两路 car003 在第 242 帧仍保持 `IDENTITY_CONFLICT`，没有错重捕。输入 `/tmp/nt-joint-recovery-full/runs/full-{gt,det}/car_003/frames.csv` 与 `/tmp/nt-visual-corrob-adb7914-full/full-{gt,det}/car_003/frames.csv`，原始归档 SHA-256 `a884f8f750a3c2a35dd8c4a04ec0a2d99004797853d86d8a0ec3fed8a3bcd53c` / `f7a8846ba1c2b92c2f9df74f1ddcf3cabeaa906a5257684c309b9dbb1c0b5b25`。
- 对十目标双初始化的既有基线与被否决候选，离线扫描所有已接受重捕：只有错误的 car003 GT/DET 第 242 帧满足“上帧输出和本帧运动预测同时贴底边、重捕框完全回到内部”；既有外部 VisDrone 0305 的第 110 帧正确重捕不贴边。此观察提示离画保护的改进方向，但单靠本视频的边缘阈值会有过拟合风险，本候选**不改边缘阈值**。
- 通过 `/tmp/nt-visual-corrob-reapply.patch`（SHA-256 `24458af59c3a800202a9db42fe070b2a4c991fd4e4244cd96b3b7257c87d4e1c`）重用已留档的同帧 detector/可靠 Recent/响应峰独特性视觉佐证路径；唯一新原则是：detector 证明当前位置，不证明被遮挡前后速度。仅这一路接受时，控制器用 `motion_last_clean_trend_` 的速度执行 `reanchor_preserving_velocity`，把搜索裁剪位置贴到刚确认的 Recent 框而不从缩水观测估计新速度；普通异步 detector 守护和常规接受路径不变。没有最后可信趋势就不启动该特殊佐证。合成测试增加 `VISUAL_TARGET_REANCHOR` 的执行断言，现有双响应下跌和低峰值裕度反例继续搜索。
- 实际 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure -j4`、`git diff --check`，RKNN-off **23/23 PASS**；构建/CTest 日志 `/tmp/nt-visual-reanchor-host-full-build.log` / `/tmp/nt-visual-reanchor-host-full-ctest.log` SHA-256 `332347a64f38cc821f13bc35a8a5a6ccea33342f35323677ebd00e702d203222` / `e50e38ed3e4e15de2f95cf33ba2073f1bfba6e256dc8393f5f97458f40d6cf00`。**仅主机候选**，尚无板端效果；下一步先在 RK3588 相同初始化下检查 car003 第 206/242 帧、car004 第 210–240 帧及 car007 遮挡语义。通过后才扩完整 20 路及外部 0305/0137；不得用主机 CTest 或单路改善升级基线。

## 2026-11-18T17:18:15+08:00 视觉探针确认第三版完整二十路出现 car003 离画错重捕，算法撤回

- 第三版精确源码 `codex/ten-target-smoothness@adb7914a225f7deb66433149c3d6843353e1b257`，源码包 `/tmp/nt-visual-corrob-adb7914-source.tar.gz` / 板端同名 SHA-256 `39b36167ab3f0c4c593e828b76f2c6723c34d8c063744ed210a354e1702ef74e`；setup/focused/full20 脚本 SHA-256 `477e223e412d801f2ad567244968474fb2c3558193472a44c35dd73bbc767225` / `da6fe47dec61d85cf0b4effdd1c8155297037313aef78756009a32a728046d37` / `1c915119d45ceb081768df959b3be6188ec02986cef9398acfb7b0ddddc761b4`。板端隔离根 `/home/neardi/Project/SOT/nt-visual-corrob-adb7914`；首次 setup 构建因 `/dev/root` 空间耗尽失败（原生编译器报 `No space left on device`），只清理了本实验三个旧隔离构建的可再生 `.o`，其源码包、二进制、日志、CSV/归档保留并复核二进制 SHA。实际重试 `cmake --build build --parallel 2`、`ctest --test-dir build --output-on-failure`，RK3588 原生 **23/23 PASS**；程序 SHA-256 `2b4f4541e84f4edfb3086969458f77da18beff735740c379d6231bc58be83914`，配置 SHA-256 `85039665d2873b8843c877f5f1148177bed64399c70a7b4dc053efe330ff0713`，cmake/build-retry/CTest 日志 SHA-256 `ccef36f7accbaf6e69b15bb90b776b2c3569f46d93464c8ee65aaa3e5a0735b8` / `f8743a5ec02263c69de9d65bfb7bc2f702a1614534bcdedc95a7984e21299f0f` / `39f344480146a27fd92d443e95a7d4962a6fc23def67efb14baeeec7d5f6068a`。输入视频、模型/manifest/labels、RKNN 版本与 `8fb939d` 一致。
- 实际 `bash /home/neardi/Project/SOT/nt-visual-corrob-adb7914-focused.sh`，七路各 271 帧 exit 0；板端原件 `runs/focused-{gt,det}/car_###/`，板端/主机归档 `/home/neardi/Project/SOT/nt-visual-corrob-adb7914-focused.tar.gz` / `/tmp/nt-visual-corrob-adb7914-focused.tar.gz` SHA-256 `9e8b687adcb26ac8f8db3dcc64b5cbe3969d8c7b24f0d50fe03d19e87a98f7ce`。实际 `python3 /tmp/nt-visual-corrob-adb7914-score.py`，脚本/摘要 SHA-256 `3fb738bc827dc478da933e1c95bc9fcce31835211c07cae287855812e066925c` / `3a2f5ce85693988323a35da218ccec9003667543f76bc9b48178f29efd52fa90`；七路正式 verdict 与 gate 帧集合相对 `8fb939d` 未变。car004 GT/DET 可见 SEARCHING `39→23` / `37→18`，已接受帧 `193→199` / `195→201`，但已接受 mean IoU `0.8456→0.8405` / `0.8302→0.8254`。car004 第 211–240 帧**全部输出框** mean IoU `0.4215→0.4755` / `0.4007→0.5013`，后段仍漂移；局部收益不足以证明全量安全。
- 板端根分区仅余约 9MB，完整二十路实际 `bash /home/neardi/Project/SOT/nt-visual-corrob-adb7914-full20.sh` 将原始输出写到独立 `/dev/shm/nt-visual-corrob-adb7914/full-{gt,det}/car_###/{frames.csv,session.json,run.log}`；十目标 GT 与 detector 首帧初始化各十路、每路 271 帧、CSV 272 行、exit 0。内存盘原件打包并立即核哈希传回主机 `/tmp/nt-visual-corrob-adb7914-full20.tar.gz`，板端/主机 SHA-256 均 `f7a8846ba1c2b92c2f9df74f1ddcf3cabeaa906a5257684c309b9dbb1c0b5b25`，主机解包 `/tmp/nt-visual-corrob-adb7914-full/`。实际 `python3 /tmp/nt-score-visual-corrob-full.py`，脚本 SHA-256 `1d193487376b4ec704cdfe59768918e578198bf1b1e19df08db91dd7c466ff7b`；各路 `--scope full` 正式报告在解包目录，摘要 `/tmp/nt-visual-corrob-adb7914-full-summary.json` SHA-256 `26932cace4497c8505eef7a0f9c95d38420722992f554b4f83dbc30393ba2b2c`，含 CSV/评分 SHA-256。现行 car004/009/010 用户确认的版本化真值均沿用，未重标。
- 全量仍为 **14/20 PASS**，但 car003 GT/DET 新增第 242 帧目标离画后错重捕，`out_of_view_semantics` 失败帧 `[242–251,256]`、`reacquisition_claims` 失败帧 `[242]`；car003 可见 SEARCHING `10→13`，已接受 mean IoU `0.8198→0.8192`。其余路正式 gate 无新增失败；局部车004/005可见收益不能抵消新的身份错误。car003 第 206 帧一次 visual probe 直接接受改变后续搜索轨迹，第 242 帧是控制器普通恢复误选；同视频上的峰值裕度阈值无法证明跨时间身份安全。**否决整条 visual probe 同帧直接接受候选，不合 main/Debug，不升级基线。**
- 本次将六份算法/单测文件精确恢复到起点 `35e21d42fdf2e65448103a6b7af4776205c914ca`，`git diff --exit-code 35e21d4 -- <六文件>` 为零；保留 `9f1659c` 可观测性和上述历史提交。实际主机 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure -j4`、`git diff --check`，RKNN-off **23/23 PASS**；构建/CTest 日志 `/tmp/nt-visual-corrob-revert-build.log` / `/tmp/nt-visual-corrob-revert-ctest.log` SHA-256 `1c7a4132c9b213fc7321bc0a3985b645ff6a5dcd5bfb551063cf9df67e9b99eb` / `0270e40ed4d97f49f8c6ae43d00c7cd9f301e185ae0f01e559dd5ae0b15c953f`。板端只再清理第三版可重建 `.o`，二进制 SHA 复核不变，`/dev/root` 可用空间约 27MB。下一步待人工核对残余车身与遮挡边界，并设计不会使已确认目标离画后误重捕的独立验证；目前仍未达到全目标贴真值的目标。

## 2026-11-18T16:55:37+08:00 同帧视觉确认修订版出现 car007 误接受，增加响应峰独特性门

- 第二版精确源码 `codex/ten-target-smoothness@5fecc46fa01ef1aa7d07ca4011b2525473de6955`，源码包 `/tmp/nt-visual-corrob-5fecc46-source.tar.gz` / 板端同名 SHA-256 `e83bf6eed2a2bcaba891337d9db42ea312cf62455d9614471f0f0eb1c64d890f`；setup/focused 脚本 SHA-256 `7c19e41d67ad4959c72f01deb2c5c94922f424c5d7d65467834b93cab6c4bd91` / `e97ae11bcac8dc9b5cc284c307ffffa737c7dd2bcaf50fc5be9b17eb6c018f7d`。RK3588 隔离根 `/home/neardi/Project/SOT/nt-visual-corrob-5fecc46`；实际 `bash /home/neardi/Project/SOT/nt-visual-corrob-5fecc46-setup.sh` 原生 build/CTest **23/23 PASS**，程序 SHA-256 `ebb71c46a68e6d9339e4a9c33e6153353670c55645ceac315bce7125f032439e`，配置 SHA-256 `d4528f23b32fa947a2c8c8614ccd3a0d32b19245c421e05811ab0244c5c0f50d`；cmake/build/CTest 日志 SHA-256 `e2852b05fd1da452e85fcd5031048554247785f482984b281d11873d4456e6f7` / `9bf147c1528bc009ab8820b37ff2ab21a6253cde4c4586990c9e9b50c2e105dc` / `2cf143de201c64593f51396c80b5b6bc11aad214bd3de6da04367c9320834805`。同一 canonical 视频、模型、manifest/labels、RKNN 版本和用户确认的真值版本，均未改。
- 实际 `bash /home/neardi/Project/SOT/nt-visual-corrob-5fecc46-focused.sh`，与上节相同七路各 271 帧、CSV 272 行、程序 exit 0；板端原件 `runs/focused-{gt,det}/car_###/{frames.csv,session.json,run.log}`，板端/主机归档 `/home/neardi/Project/SOT/nt-visual-corrob-5fecc46-focused.tar.gz` / `/tmp/nt-visual-corrob-5fecc46-focused.tar.gz` SHA-256 `f43814cecf624abc4130280fd47301320e355748721c7f05d206cf79ed7811c6`。实际 `python3 /tmp/nt-visual-corrob-5fecc46-score.py`，脚本 SHA-256 `dc4317ebaac5a2eeef4bbe7d8f68a45bcbde712bdd7f339c9f3101897e1f1dea`；逐路正式报告在主机解包目录，摘要 `/tmp/nt-visual-corrob-5fecc46-summary.json` SHA-256 `5061bb1fb0ac563128c4f7f738d22fff38f1386409c4e0a2f2fc765dce2c9830`。car004 GT/DET 可见已接受 `193→199` / `195→201`，SEARCHING `39→23` / `37→18`，但可见已接受 mean IoU `0.8456→0.8310` / `0.8302→0.8254`；car005 双路各多 2 帧接受，IoU 微降。car008 GT、car001 detector 正式 PASS 保持；**car007 detector 正式 PASS→FAIL**，第 217 帧全遮挡仍 accepted，第 218–219 帧仅 SUSPECTED、未 SEARCHING。故第二版也不能晋级，不扩 20 路。
- car007 detector 第 212/214 帧 visual probe 允许清除怀疑后，第 217 帧仍沿错误观测；这两次原始 Recent 峰值裕度只剩可信参考约 `17%/7%`，而同路线第 216 帧约 `45%`。该低峰值裕度反映响应不独特，原可靠绝对门并未识别。下一候选在同帧视觉确认前要求 `peak_margin >= 0.25 × trusted_peak_margin`，仍保留唯一 detector、原质量 RELIABLE、PSR/APCE 半幅跌落等约束；本四分之一只是待跨场景检验的**实验阈值**，不能仅凭此视频宣称普适。合成测试新增 PSR/APCE 尚可但峰值裕度偏低的反例，要求继续 SEARCHING。
- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，本候选从 `5fecc46` 出发，与本节同提交。实际 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure -j4`、`git diff --check`，RKNN-off **23/23 PASS**；主机构建/CTest 日志 `/tmp/nt-visual-corrob-v3-host-build.log` / `/tmp/nt-visual-corrob-v3-host-ctest.log` SHA-256 `e24f2193f5ddb90e8ab8b220bdf01a582853cc0efcc68f932068cb501aa514b8` / `e554b540bf88badbc960d06ccade52f4d6be42de1d4bad1492781a9897946973`。下一步同七路原生复测；若 car007 全遮挡语义仍坏或没有稳定贴真值收益，撤回视觉确认算法。未合 main/Debug 或升级基线。

## 2026-11-18T16:46:50+08:00 视觉探针确认候选首版板测无效，修订部分可见门

- 首版精确源码 `codex/ten-target-smoothness@3956add2b9238dd94239f967662deef8e6dfb640`，源码包 `/tmp/nt-visual-corrob-3956add-source.tar.gz` / 板端同名文件 SHA-256 `c3328a9e93d2d4d1d81bab733539c0bd79a9e71e0d6820c366d7bc13219fffd3`；setup/focused 脚本 SHA-256 `b91bc5c89d74d51199be62642aeb1c6324895efb0b8b43c8a6e5dfdf43dc8458` / `1408e6cfe425a03d5886b17497a19553e4e97a0a413c7f4254d5394bf676145e`。板端隔离根 `/home/neardi/Project/SOT/nt-visual-corrob-3956add`；实际 `bash /home/neardi/Project/SOT/nt-visual-corrob-3956add-setup.sh` 原生 build/CTest **23/23 PASS**，程序 SHA-256 `ad09e9afc8df7c2cae8c4c562541d5a45722946f14370039f14205ebd6ba5875`、配置 SHA-256 `cb5ed8e01054dd097397a521ab5c00c381a2f8acf0e51c58c9c575b5b59c2999`，cmake/build/CTest 日志 SHA-256 `e8c921f7ab30e9fddafe4e8bfa9a3031e512207cbd28cdfd5277bf6937ed53a9` / `34cb7020492262f04514c0042d82ad3fafaecfae91957c10e93a17617ddbaefb` / `6f83ecd1e78c0b0bec42992508cab04a619135b6d62664fa46a2adc8203ba72f`。视频、RKNN 模型、manifest/labels、运行时版本与 `8fb939d` 相同，未重新训练。
- 实际 `bash /home/neardi/Project/SOT/nt-visual-corrob-3956add-focused.sh`，car004/005 GT+detector、car008 GT、car001/007 detector 共七路各 271 帧、CSV 272 行、程序 exit 0。板端原件 `runs/focused-{gt,det}/car_###/{frames.csv,session.json,run.log}`，板端/主机归档 `/home/neardi/Project/SOT/nt-visual-corrob-3956add-focused.tar.gz` / `/tmp/nt-visual-corrob-3956add-focused.tar.gz` SHA-256 `4d0710401f62d6b0ced9ca92d01df786bb47e4fc48632be748221469ce2d7588`。实际 `python3 /tmp/nt-visual-corrob-3956add-score.py`，脚本 SHA-256 `37880e87e75ef9575d49786df9f09a80941ca0b73d060fa07255e856243a0d9a`，沿用各目标现行真值版本及 `--scope full`；逐路正式 score 在主机解包目录，摘要 `/tmp/nt-visual-corrob-3956add-summary.json` SHA-256 `e2f4324291782327eab8bbaee222f4ce5665c78ff36dbf7bf9dd747b0c6e849e`。七路全部正式 verdict/gate 帧集合与 `8fb939d` 相同，但 car004 GT/DET 可见 SEARCHING `39→32` / `37→30`、car005 GT/DET `15→12` 时可见**已接受帧和平均 IoU 均未改善**；car007 detector 已接受可见帧 `251→248`，搜索移后。故首版**不得晋级**。
- 逐帧证据：首版 `VISUAL_PROBE(target_corrob=1)` 在 car004 第 210–218 帧、car005 第 256–259 帧却仍 `SUSPECTED/accepted=0`；控制器 `assess_recent` 的 `partial_visibility` 由原始高度小于当前框 `0.98` 且中心偏移触发，阻断普通 detector 守护的直接接受。下一候选仅让**同帧、唯一、重合的 visual probe 框 + 完整质量 RELIABLE 的 Recent + PSR/APCE 未共同半幅下跌**绕过这一条尺寸疑虑，仍发布 NanoTrack Recent 框；普通异步守护、弱响应、双响应下跌维持原门。合成测试现用有真实 `decoded_bbox` 缩高及中心偏移的输入，验证该路径和双响应下跌负例。
- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，本修订从 `3956add` 出发、与本节同提交。实际 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure -j4`、`git diff --check`，RKNN-off **23/23 PASS**；主机构建/CTest 日志 `/tmp/nt-visual-corrob-v2-host-full-build.log` / `/tmp/nt-visual-corrob-v2-host-full-ctest.log` SHA-256 `9b2cbac9ca8312f44cf4e3f9577b8c9ded34edb8b3d922052302ee8130fd1a17` / `07d4d694b718b74026cda3fff7e8b09a5427f784c937c0371e0051a6569125ae`。**修订尚未板测，不是效果结论**；下一步七路同框复测，car007/008/001 遮挡门退化即撤回，再视结果扩完整二十路和外部场景。不合 main/Debug、不升级基线或生产 handoff。

## 2026-11-18T16:37:53+08:00 同帧 detector 确认可靠 Recent 的视觉探针候选：主机验证

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `35e21d42fdf2e65448103a6b7af4776205c914ca`；本节与源码及测试同提交，结果提交号以 Git 为准。旧 visual probe 在已 SUSPECTED 的 car004/005/007 可见帧把**检测到同一个目标**也当作遮挡邻车，从而提前 SEARCHING；先前单纯延迟 SEARCH 的 `a6c2bb7` 候选已证明会损及 car001/007/008 遮挡语义，不能复用。
- 实际执行 `python3 /tmp/nt-visual-hit-response-audit.py`，脚本/输出 `/tmp/nt-visual-hit-response-audit.py` / `/tmp/nt-visual-hit-response-audit.json` SHA-256 `2215a5f5e120a214d9a57149e9e9a676748ef590c646f66dcc5687bb05cfd8ba` / `2cefc7ec923940a164ce7a3d540bbdbce31ece2043ba6edabe72bd3fd4e76836`。输入为上节 `9f1659c` 的七路 RK3588 逐帧诊断，原始归档 SHA-256 `9f99be4d691ebc0f8e09641ed70173a6f3e33e51b128dbb3d7ff27df8ebbfdff`。14 个 visual hit 中 car004 GT/DET 210–211、car005 GT/DET 256–257、car007 DET 212–213 与 car001 DET 167–168 现行可见；car008 GT 241 可见、242 现行完全遮挡。car008 241/242 的 PSR/APCE 相对可信参考均越过既有半幅跌落线；car004/005/007 可见段不同时越过。car001 的峰值裕度不足使 Recent 为 WEAK，不能用本候选强行接受。
- 候选只在视觉探针同帧唯一同类 detector 框与 Recent IoU 达到现有 `detector_confirmation_iou`、Recent 经控制器原有完整评估为 RELIABLE、且 PSR/APCE 未共同低于现有半幅滞回线时，借用 `suspected_detector_guard` 清除怀疑并接受**原 Recent 框**。弱/拒绝观测、双响应同时下跌、无参考、多个近候选保持原遮挡搜索；detector 框不直接发布 `tracking_bbox` 或控制。`VISUAL_PROBE` note 加 `target_corrob` 以区分该路径。合成运行时测试分别覆盖可靠同目标恢复和双响应下跌继续搜索。
- 实际 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure -j4`、`git diff --check`，RKNN-off **23/23 PASS**；构建/CTest 日志 `/tmp/nt-visual-corrob-host-full-build.log` / `/tmp/nt-visual-corrob-host-full-ctest.log` SHA-256 `e42bc03e5966b76e15e49e62d6c9839ddc3462f9db65d6192b9fb0a4496d4543` / `f6a4ffdc68dd7a4f6e7b112dcd80a69ecf2ca0bc0866552554bfbbb5a5c4d32d`。**仅主机候选**，尚未证明十目标质量改善；下一步在 RK3588 隔离部署，先测 car004/005、car001/007/008 定向反例，再决定是否扩全量 20 路。现行真值疑似全遮挡边界仍待用户核对，不因候选改标注。暂不合 main/Debug、不批准生产 handoff 或吊舱验收。

## 2026-11-18T16:25:37+08:00 detector/Recent/Safe 离线三方对照与遮挡标注待核

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，起点 `c2eecf187e5d8e106bba22ce9682e23e8f1788f5`，起始工作区干净。本节仅分析既有 RK3588 原始记录，不改算法、配置或真值。输入为 `8fb939d` 板端运行的 `/tmp/nt-joint-recovery-full/runs/full-{gt,det}/car_###/frames.csv` 共 20 路，程序 SHA-256 `d3ed7247a27a65ab8bfd100719093b5e18e2df7bd2773fda232610a139876d11`，原始归档 SHA-256 `a884f8f750a3c2a35dd8c4a04ec0a2d99004797853d86d8a0ec3fed8a3bcd53c`；视频 `/home/u20/single-object-tracking/data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`。输入模型/manifest/labels 与 RKNN 版本沿用 `8fb939d` 板端记录；上节 `9f1659c` 诊断运行仅供交叉核查，未混入本次离线筛选。
- 实际执行 `python3 /tmp/nt-provisional-triad-audit.py`，脚本 SHA-256 `29739ef76761b28c34912cbe9101b9fb5c4390fc390e415bbc0c20231ad00906`，输出 `/tmp/nt-provisional-triad-audit.json` SHA-256 `1e07522d92963675bae6f5d4c5f97b714898b63b511d1e05217ac90bf2d26daa`。这是**离线反事实筛选**：对 SEARCHING/未接受帧检查 detector 候选与 Recent、Safe 原始框重合、各自响应和运动距离；它不是程序新输出，也不改变控制有效性。car004 GT/检测初始化分别筛出 15/13 个现行可见帧，候选框对真值平均 IoU 约 `0.810/0.822`；car004 第 214–230 帧候选常比现行运动输出更贴近真值。car005 也出现可见帧机会。
- 同一筛选还命中现行标为完全遮挡的 car002 GT 第 215 帧、car005 第 172 和 263–264 帧、car007 第 217 帧、car010 第 164–168 帧等；故三方几何一致**不能单独授权发布框或控制**。实际执行 `python3 /tmp/nt-provisional-triad-false-montage.py`，脚本 SHA-256 `51f079d549ef191907db41d79fb7457ab87c0a199269157106216ec74e7c8971`，对照图 `/tmp/nt-provisional-triad-false-montage.png` SHA-256 `efcfc1a2adbbfa033ce29f29598265e8bac4085a43141bd1ba80cc5d87825344`。图中有疑似残余车身，但相似车辆的身份和遮挡边界未获人工确认；已请求用户逐段核对，暂不重标。
- **证据边界 / Handoff**：这是对既有 20 路板端运行的主机离线分析，不是新板端算法验证或正式评分。现行正式基线仍为 14/20 PASS；不合 main/Debug、不批准生产 handoff 或吊舱验收。下一步先确认疑似残余车体的目标身份及部分可见状态，再设计以连续时序和 NanoTrack 独立质量约束的恢复候选，并用完整 20 路及外部场景复测；不能用单视频的 IoU/置信度界限调全局阈值。

## 2026-11-18T16:17:43+08:00 视觉探针候选框板端审计：几何重合可用但不能单独授权

- 诊断源码 `codex/ten-target-smoothness@9f1659c36ddbb3941b8e75ada5ec9e83aabb76c1`，主机/板端源码包 `/tmp/nt-visual-probe-geometry-9f1659c-source.tar.gz` / `/home/neardi/Project/SOT/nt-visual-probe-geometry-9f1659c-source.tar.gz` SHA-256 `b7c83ff66c12f8859d502a198b2396fa54c1f98c86025679a091ada0196248d2`。隔离板端根 `/home/neardi/Project/SOT/nt-visual-probe-geometry-9f1659c`；实际 `bash /home/neardi/Project/SOT/nt-visual-probe-geometry-9f1659c-setup.sh` 原生构建/CTest 23/23 PASS，程序 SHA-256 `06fc9e8792a7332ac08e556b3b2f63548f73e450db283044f9e8ee5d59e14d30`，运行配置 SHA-256 `ed4f63dc902f8ab5d77695c87b1c7e2630596c0c73f3361b3e6d4c42fb76cc9e`；cmake/build/CTest 日志 SHA-256 `6a237d2d59f1f6aef7f1b80f6161dedd5c64a65897c1019eb5bfad542bf83af3` / `184f98b3383c1b77bd7235f0b3a0a825414207834951d11496ff4f074c82dd1b` / `e58e9c34ad43cac11215bb5a363bbefbc178ea383730954ca59fdfab5130b0e9`。输入视频、模型/manifest/labels、RKNN 运行版本仍按 `8fb939d` 同一身份；无新模型或真值修改。
- 实际 `bash /home/neardi/Project/SOT/nt-visual-probe-geometry-9f1659c-focused.sh`，脚本 SHA-256 `a6d3ffdce9e457e75e9cfd2b07c30e4ef45a72a2ec4ed7cf98977b3a43b1a454`；car004/005 GT+detector、car008 GT、car001/007 detector 共七路均 271 帧、CSV 272 行。板端原始 `runs/focused-{gt,det}/car_###/{frames.csv,session.json,run.log}`，板端/主机归档 `/home/neardi/Project/SOT/nt-visual-probe-geometry-9f1659c-focused.tar.gz` / `/tmp/nt-visual-probe-geometry-9f1659c-focused.tar.gz` SHA-256 `9f99be4d691ebc0f8e09641ed70173a6f3e33e51b128dbb3d7ff27df8ebbfdff`。主机实际 `python3 /tmp/nt-visual-probe-geometry-9f1659c-score.py`，脚本 SHA-256 `7a061866dbfce93b13f5ce27f20c0d83ddde971a3fb3d922797f8097428acdd4`，逐路 `--scope full` 评分；摘要 `/tmp/nt-visual-probe-geometry-9f1659c-summary.json` SHA-256 `10247bfccd02adabaa0580f3d3c05d8639d2b418d21e916ce8b8b471e53823e5` 含各 CSV/评分 SHA-256。相对 `8fb939d`，七路正式 gate 和 PASS/FAIL 均未变；少量 detector 异步完成帧差引起至多约 1.18px 的显示/跟踪值差，不构成算法效果改善。
- car004 GT 第 210/211 帧近候选置信度 `0.842/0.801`、对 Recent IoU `0.850/0.811`，detector 初始为 `0.685/0.795`、IoU `0.822/0.808`；第 131–133 帧现行完全遮挡期无近框。car005 GT 第 256/257 帧可见时为置信度 `0.868/0.826`、IoU `0.782/0.726`。car008 GT 第 241/242 帧现行状态从可见到完全遮挡，但近候选仍有置信度 `0.433/0.436`、IoU `0.683/0.666`。car001 detector 初始可见第 167/168 帧置信度仅 `0.535/0.462`、IoU `0.662/0.642`；因此单一高置信度或高 IoU 阈值会漏掉可见目标，也不能凭 detector 与 Recent 高重合直接授权身份/控制。对应可视对照 `/tmp/nt-visual-probe-montage.png` SHA-256 `8bfba1dfba2dfc33348d17fb5cee241a094932c2e14f77b873195d302112ff8c`；car008 第 242 帧桥栏上方似有车顶，但是否为跟踪目标及 242–244 帧是否应改为部分可见，已请求人工确认，**不擅自改真值或正式评分**。
- 本诊断只增加 note 可观测性，未增加接受帧，也未解决 car004 后段轨迹；不推广算法、不合 main/Debug、不批准生产 handoff。下一步在人工核对遮挡边界后，评估带时间连续性和独立 NanoTrack 质量约束的 detector 候选验证；不得用本视频上的 IoU/置信度分界直接定全局阈值。

## 2026-11-18T16:08:07+08:00 视觉探针候选几何可观测性：主机候选

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，起点 `8840a406bbc0f62f741daf2fb6a6b6a37c7cf2b3`，工作区起始干净。视觉探针原逐帧 note 只记附近候选中心距离、数量和亮度比，无法核查候选是否真正包住 Recent；上一板测证明单纯推迟 SEARCHING 无法改善已接受轨迹，且会破坏遮挡语义。为评估能否安全复用同一帧的阻塞 detector 结果，本候选只把最近同类候选从 640×640 探针画布映回原图、裁到图内，并记录其置信度、原图框和对 Recent 框 IoU；搜索、接受、控制、模型调用时序均未改变。无有效框时不写几何字段。
- 主机实际执行 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure -j4`，23/23 PASS；日志 `/tmp/nt-visual-probe-geometry-build.log` / `/tmp/nt-visual-probe-geometry-ctest.log` SHA-256 `f92f5de81f93b3f4f456ba42a661027ec592bda6eb81e15e0241c5513cb92f33` / `9b5f2bed918f3f9273ee529b18ae561f5e173a2f0b409a53ff8e845311d3927e`，`git diff --check` PASS。这是诊断日志扩展，尚未有板端逐帧数据；下一步隔离原生构建并在 car004/005 可见误触发及 car008 完全遮挡负例上读取同一管线的候选几何，再决定是否实现 detector 与 NanoTrack 的联合验证。未合入 main/Debug、未批准生产 handoff。

## 2026-11-18T16:04:56+08:00 视觉探针仅延后搜索的板测失败，撤回算法

- 候选源码 `codex/ten-target-smoothness@a6c2bb796a239c8842e5bf39df6b8c4cebceed4b`，主机/板端源码包 `/tmp/nt-visual-response-a6c2bb7-source.tar.gz` / `/home/neardi/Project/SOT/nt-visual-response-a6c2bb7-source.tar.gz` SHA-256 `a1e819c2bf4d83439414a184dedd7bfa81ca88aeaa28adc7b20b3406784c25ab`。板端隔离根 `/home/neardi/Project/SOT/nt-visual-response-a6c2bb7`；实际运行 `bash /home/neardi/Project/SOT/nt-visual-response-a6c2bb7-setup.sh`，RK3588 原生 CMake/build/CTest 23/23 PASS，程序 SHA-256 `cd8b72a68506db9002dbe1bdaf5a3486f0bf036a8da65ba1d9dd6a4850bbc86f`，运行配置 SHA-256 `559867ac14e3320c33035dfc750aa3419ee01b76d8e76a825290d0ce63a88b63`，cmake/build/CTest 日志 SHA-256 `9cd84f49602490b68cc70c03a2eac147bb337d4d11e62e618f423f6b7b37dda9` / `3e0d91583d3ca3c86956a82221e29a7d1a3c19988d8b784a17dfae9b3cebf3a1` / `02869b64d60cbe248bf0a14e1812b40fa7bb20cf99f929aa6da9a7bce80b08df`。视频、模型与 RKNN 版本未变。
- 实际 `bash /home/neardi/Project/SOT/nt-visual-response-a6c2bb7-focused.sh`，脚本 SHA-256 `4441a5ebf7b78f37fee68b4405920342b8b1ed03c034a387824905cde83ee561`，car001/002/004/005/007/008 GT 与 detector 初始共十二路均 271 帧、CSV 272 行。原始结果板端 `runs/focused-{gt,det}/car_###/`，板端/主机 `/home/neardi/Project/SOT/nt-visual-response-a6c2bb7-focused.tar.gz` / `/tmp/nt-visual-response-a6c2bb7-focused.tar.gz` SHA-256 `e149e40658399c2ca69d080a3ca7786ad39614dc19d48d194a791644e8d2715d`。主机实际 `python3 /tmp/nt-visual-response-a6c2bb7-score.py --config-sha 559867ac14e3320c33035dfc750aa3419ee01b76d8e76a825290d0ce63a88b63`，脚本 SHA-256 `1c39c72a688742e5986232f7d7fc979f64e054b35da3204854856b02800122aa`；正式逐目标 `score.json` 在主机解包目录，摘要 `/tmp/nt-visual-response-a6c2bb7-summary.json` SHA-256 `c5be975a156e69ebabba2ab0943fd90b9f24cb0bacaf119c29519976edc7f7aa`，含每路 CSV/评分 SHA-256。
- car004 GT/det 可见 SEARCHING 分别 `39→32`、`37→30`，car005 GT/det `15→9`，但这些路**已接受的可见框数量和平均 IoU 未增加**；主要是 SUSPECTED 延长。car008 GT 第 242–244 帧完全遮挡状态失败，car001 detector 初始第 172 帧失败，car007 detector 初始第 217 帧失败，三路原 PASS 变 FAIL；car007 detector 重捕 `226→231`。严格“双项都达到完整跌落门才准搜索”会错误保护仅一项已恶化的遮挡目标。**否决此候选，不跑二十路、不合 main/Debug。**本次将四份运行时代码和单测源码恢复到起点 `564aeca` 的算法内容；主机实际重新执行 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure -j4`，23/23 PASS，日志 SHA-256 `faff648ab69471187fde1f962c45f45a1a09e4e974771fda9fce5aa54835a6bc` / `057bca1bc47df755148b90538677a3157b8667e28c7034a39e660de957419988`。后续若使用视觉探针结果，应把候选几何和 NanoTrack 新鲜观测共同验证，不能仅靠延后状态转换取得表面搜索帧改善。

## 2026-11-18T15:54:36+08:00 视觉探针双响应确认：主机候选，待 RK3588

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，起点 `564aeca50d35dd1082eed5c660ce84d7b60af107`，工作区起始干净。二十路现行真值回放显示，car004 GT/det 第 210–211 帧 detector 近邻命中、PSR/APCE 未同时跌落，却由视觉探针强制 SEARCHING；car005 第 256–257 帧、car001 detector 初始第 167–168 帧也有相同模式。相反 car004 第 133 帧、car008 第 242 帧的完全遮挡视觉探针帧，两项响应均明显下降。当前视觉探针把“附近有同类车”直接累积为遮挡证据，无法区分仍可见目标与遮挡车。原始输入与正式评分为 `/tmp/nt-joint-recovery-full/runs/full-{gt,det}/car_###/{frames.csv,score.json}`，归档 SHA-256 `a884f8f750a3c2a35dd8c4a04ec0a2d99004797853d86d8a0ec3fed8a3bcd53c`。
- 本候选仅在视觉探针命中且准备以附近候选**提前强制搜索**时，要求相对已可信响应参考的 PSR 和 APCE 都达到原有跌落门；任一未跌落则不使用这条强制搜索快捷路径，普通控制器的怀疑时长、失踪判定和 detector 缺席保护均不改。无可信参考时保留原行为。新增 `visual_probe_response_guarded` 诊断并写入逐帧 note。主机实际运行 `cmake --build /tmp/nt-ten-target-build --target test_long_term_runtime --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build -R '^long_term_runtime$' --output-on-failure`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure -j4`，全套 23/23 PASS；日志 `/tmp/nt-response-visual-build.log`、`/tmp/nt-response-visual-ctest.log` SHA-256 `33672724893f8d4503539cf64a9511ac6896f5b9f1b18d101b438c08987e70c5` / `17fc0d038cfb445d88f7d7a0920ba75acb436186034402c6ba333ea8610ba11f`，`git diff --check` PASS。此处只证明主机逻辑与编译，尚无板端有效性结论；下一步原生 RK3588 定向回放可见误触发和完全遮挡负例，之后按是否损及身份与真值框决定完整二十路。未合入 main/Debug、未批准生产 handoff。

## 2026-11-18T15:47:10+08:00 共同贴边恢复板测失败并撤回算法

- 候选源码 `codex/ten-target-smoothness@71061e848adb9d7c4acacc742c0399cfcb8e6538`，主机/板端源包 `/tmp/nt-edge-71061e8-source.tar.gz` / `/home/neardi/Project/SOT/nt-edge-71061e8-source.tar.gz` SHA-256 `c7d3ca34964b723834e0d6bf20e4ab2e7c8a4ac3839dffd9b3f3c403e47d2736`。RK3588 隔离目录 `/home/neardi/Project/SOT/nt-edge-71061e8`；实际原生 `cmake -S rk3566/nanotrack_v3 -B build -DENABLE_RKNN_RUNTIME=ON -DBUILD_TESTING=ON -DRKNN_INCLUDE_DIR=/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/include -DRKNN_LIBRARY=/usr/lib/librknnrt.so`、`cmake --build build --parallel 4`、`ctest --test-dir build --output-on-failure`，23/23 PASS；程序 SHA-256 `b3195347e66b9d90194c27f8edf8341bdced0d9dc45373ce742d51cf537ae248`，运行配置 SHA-256 `e3d32059e23255efdbf6501ebc5701984d57356638b4ea9f11a6fbc58b2d40e0`，cmake/build/CTest 日志 SHA-256 `29423bd0555d23f1be733b5868be84808abaa7a8ae5f115f58677e7e81287dbd` / `acac9a4366a8e814bd8603230ca035a065b6a17640119c516f7fe4c27157b86c` / `e98a2b2ea657d4397bd48c40141a6f887ba2f7c2a1f854ee852bf09de730e9bb`。同一 canonical 视频、tracker/detector 模型、manifest、labels 和板端 RKNN 版本沿用上节 `8fb939d`；本轮未改模型或真值。
- 实际运行 `bash /home/neardi/Project/SOT/nt-edge-71061e8-focused.sh`，脚本主机/板端 SHA-256 `aadc107f61044c69b046ccb14b16670a1ebbb795bfd1d788099cad7899efc728`，car002/003/004/010 各 GT/detector 初始化共八路，全部 271 帧、CSV 272 行。板端原始 `runs/focused-{gt,det}/car_###/{frames.csv,session.json,run.log}`；板端/主机归档 `/home/neardi/Project/SOT/nt-edge-71061e8-focused.tar.gz` / `/tmp/nt-edge-71061e8-focused.tar.gz` SHA-256 `f53de893d0131e27d2973aec448e321832fdbd7e742d17ede1b79513fcd8463a`。主机实际 `python3 /tmp/nt-edge-71061e8-score.py`，脚本 SHA-256 `c5e2c757c3ae04af2e69f94343727199f7b805670a786422470d4c6698a83981`，使用 `scripts/evaluate_occ_video_1.py --scope full --source-commit 71061e8... --config-sha256 e3d3205... --model-sha256 3599d73...` 对各自真值正式评分；八份 `score.json` 在主机同名运行目录。摘要 `/tmp/nt-edge-71061e8-summary.json` SHA-256 `22e966dd6e62fcd287fdfa60e020606315eb853e43e256dba83438cd48d5a81c`，含各 CSV/评分 SHA-256 与旧版对照。
- car004 GT 第 224 帧提前重捕，但第 224–231 帧已接受框约 92×57 且过大，逐帧可见 IoU 失败；目标 241 帧离画后，第 243–270 帧仍错误接受，新增 `out_of_view_semantics` 和 `reacquisition_claims` 失败。car004 detector 初始化无改善；car002/003/010 双初始化评分未变化。**否决共同贴边放宽**。本次将控制器和单测源码精确恢复到起点 `34f1f3a` 的算法内容；主机重新运行 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure -j4`，23/23 PASS，日志 SHA-256 `54b470e0aaccb2555782dcebe4a24bc2364b066748e143ea2bb29d6fb9b0309c` / `f3fb151bb39cc74e3fd8ea06c3739e130b7cf18d72ea9a734d433c89403a090c`。当前可信候选仍为前述 `8fb939d` 二十路 14/20 PASS，未合入 main/Debug、未批准生产 handoff。后续若研究边缘可见框，必须同时解决框尺寸、离画身份与初始框一致性，不能只放宽恢复门。

## 2026-11-18T15:38:22+08:00 共同图像边缘可见框恢复：主机候选，待 RK3588

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，起点 `34f1f3a4f95274d98cb65c512a57baa65f952d1c`。现行 car004 真值初始化第 214–221 帧 detector 与两路 NanoTrack 框接近目标，却被 `search_reacquisition_min_age_ms=400` 拦下；第 223–230 帧三框共同贴图像下边缘，旧的“detector 必须完整在图内”条件使恢复证据仍为 `REJECTED`，一直无确认命中。先前 100 ms 全局缩短探针让 car002/003 错认，因此本候选不改搜索最短时间，只允许 detector/Recent/Safe 三框共同贴同一图像边缘、都在图内、靠近运动预测且唯一三路几何一致时，重评估原始可见框；仍需连续 NanoTrack 验证才能恢复，detector 单独不能授权控制。原始证据 `/tmp/nt-joint-recovery-full/runs/full-gt/car_004/frames.csv`，正式评分 `/tmp/nt-joint-recovery-full/runs/full-gt/car_004/score.json`，对应包 SHA-256 `a884f8f750a3c2a35dd8c4a04ec0a2d99004797853d86d8a0ec3fed8a3bcd53c`。
- 增补共同贴边允许恢复、其中一路未贴边仍拒绝的控制器单测。主机实际执行 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure -j4`，23/23 PASS；`git diff --check` PASS。此处仅为主机验证，未主张板端改善。下一步提交候选并在 RK3588 隔离目录原生构建，优先回放 car004 和早重捕负例 car002/003/010，再决定是否跑二十路。未合入 main/Debug，未批准生产 handoff。

## 2026-11-18T15:29:03+08:00 双响应滞回二十路板测：遮挡误接受少一帧，正式通过数不变

- 精确候选源码 `codex/ten-target-smoothness@8fb939d146ef654853a216ebe822c04626ca4338`、工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`；主机/板端源码 tar `/tmp/nt-joint-recovery-8fb939d.tar` / `/home/neardi/Project/SOT/nt-joint-recovery-8fb939d.tar` SHA-256 `50587cae9f590de62c02a77f87b73052732987e616013b04e0c2be32629f748e`。隔离板端根 `/home/neardi/Project/SOT/nt-joint-recovery-8fb939d`；实际 `bash /home/neardi/Project/SOT/nt-joint-recovery-setup.sh`，脚本 SHA-256 `8cda57ecb35458c0bf349289108e2746a7f1c16a99349ca0cd7ee8e7c2bc53d3`；RK3588 原生 CMake/build/CTest 23/23 PASS，程序 SHA-256 `d3ed7247a27a65ab8bfd100719093b5e18e2df7bd2773fda232610a139876d11`；cmake/build/CTest 日志 SHA-256 `c73401964dac228966bc0a5a78a00239695d7c7fc7d5d6a78a6fb6244a0e2f15` / `261ede7e6ce0a7433e1b8fd494ef1a1a6a1dcdd1bae77ae4925c39dd993b520f` / `d81eaf9d68b338a348b24ab03bbc91ea552d7f61b075fc460c4dea88015415ae`。controller/headless SHA-256 `bb19d2e8e085dd57084653b6ca37bb981b10ea2a0d00bc05d166f8a508c6e26f` / `f0d54a8d748f9393536a39de03ac05be46265152de1c20e16809fb1476e2bf38`；视频、tracker manifest、detector RKNN/manifest/labels 精确哈希及 RKNN API 2.1.0/driver 0.9.8 沿用上节已核验身份，未更换模型。
- 实际先运行板端 `bash /home/neardi/Project/SOT/nt-joint-recovery-focused.sh`（脚本 SHA-256 `4389c548643506b579b9911ffaf641a0cef9010afac1cdf24e6ef6f50de92172`），外部 0305/0137 两段 141/233 帧及 car003/004/010 双初始化、car005 GT 共七 canonical 路各 271 帧，全部 exit 0。原始 `runs/focused/<name>/{frames.csv,session.json,run.log}`，板端/主机 `/tmp/nt-joint-recovery-focused.tar.gz` SHA-256 均 `c35a4fc0b1667ae45f8894281f133d0c05ebeb153d85d10087cb524e94290a42`；主机解包 `/tmp/nt-joint-recovery-focused/`。实际 `python3 /tmp/nt-score-joint-recovery-focused.py`，脚本/摘要 SHA-256 `4aa01f2b1f380e0a18f8accbec966fe121c79e7c605e0d0aeb5a717808bd9010` / `9002235969c1dd84a54ad7ea9e1fc9a1ff8b5c8ef3c992fa22177bb1dc64f05e`，每路正式 score.json 在对应主机目录；两段外部原标注实际 `python3 /tmp/nt-score-joint-recovery-external.py`，脚本/摘要 SHA-256 `f2ba7fabcc38525f127e15d2ec956a14e27d6d84cf786df5e44d4b4df13c5088` / `e99d154926991e3cb95eacf85cb0994224ed74b398249d6a996b65561a2a6fb9`，与冻结 `526d491` 清晰 IoU/搜索/重捕完全相同。
- 定向无新增正式门失败后，实际运行 `bash /home/neardi/Project/SOT/nt-joint-recovery-full20.sh`，脚本主机 SHA-256 `650536e4b157354c73aab579b6c8c61fdb917d036e0452bf20b1ea5a50564917`；十目标首帧 GT/detector 初始化各十路、每路 271 帧、exit 0。原始 CSV/session/log 板端 `runs/full-{gt,det}/car_###/`；板端与主机 `/tmp/nt-joint-recovery-full20.tar.gz` SHA-256 均 `a884f8f750a3c2a35dd8c4a04ec0a2d99004797853d86d8a0ec3fed8a3bcd53c`；主机解包 `/tmp/nt-joint-recovery-full/runs/`。实际 `python3 /tmp/nt-score-joint-recovery-full.py`（脚本 SHA-256 `e0e67c69cbe3908c203fdc6534bc481bb0848e481a01b14ec28568aa5cdfeb30`）逐路用 `scripts/evaluate_occ_video_1.py --scope full --source-commit 8fb939d146ef654853a216ebe822c04626ca4338 --config-sha256 f0d54a8d748f9393536a39de03ac05be46265152de1c20e16809fb1476e2bf38 --model-sha256 3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`，car004/009/010 分别使用用户确认的可见无框真值版本。逐路报告在各目标目录；摘要 `/tmp/nt-joint-recovery-full-summary.json` SHA-256 `9614b23378383f38b983cf2e15979ba3ea945695ff350d6d9ba82dc0f17ba7f6`。相同真值下冻结 `526d491` 与本候选均 **14/20 PASS**，全部正式 gate、重捕帧、可见 SEARCHING 帧数一致；可见已接受 mean IoU 仅 car010 GT `-0.000336`、car007 DET `+0.000063` 的微小差异。
- car004 GT 完全遮挡期间 `accepted=1` 从旧版第 131 帧的 `1` 次变成 `0` 次；131 帧由 TRACKING/accepted 改为 SUSPECTED/不接受，132 帧仍 SUSPECTED，133 帧 SEARCHING，146 帧正确重捕保持不变。正式 `full_occlusion_semantics` 仍要求完全遮挡时 SEARCHING/MOTION_SEARCH，因此 131–132 两帧仍 FAIL；car004 127–129 的可见部分框尺寸仍 FAIL，car003 双路第 130 帧、car010 双路第 156–157 帧的搜索状态亦仍 FAIL。**本候选保留为实验增量，不合 main/Debug、不升级正式基线或生产 handoff。**
- 可见部分定位只读探针：主机同源 VisDrone9 PT 在 640 输入、conf 0.25 下对 car004 第 127 帧给约 `38×51` 的近车框，仍高于真值可见部分；128–129 无近车框，脚本/JSON SHA-256 `7edc54689eba7a5401eb6914d881474a1d2f01d2643390c7ce2246b3d2b751ce` / `6beac832b227e824230dbd1e22af9f31f9d30630f24a968185f28faf37376410`。同源 PT 不等于 RKNN 板端逐候选输出；板端日志只证明无有效同类近框。已保存配准光流对 car004 128–129 上半框强、下半框零，但 car001 166–167 也出现上强下弱，而其正式可见框仍高约 47px；复算脚本/日志 SHA-256 `9d4eed58f4dcf0e6b87974370132320cde2f3bca57255a8a9b9f617871f1f454` / `e41237de21232ff3c89c824a8eda1f2f82dc9ff1b4cfdb56098f3eb10e94149d`。两种信号均不能直接生成跨目标可靠的可见部分框，未进入源码。
- **Handoff/下一步**：该滞回只减少一帧被完全遮挡目标的可信误观测，十目标整体轨迹和六路失败仍未解决。继续保留独立实验分支及板端原件；下一步优先寻找可跨目标验证的可见部分定位证据与遮挡后恢复身份信号，所有新候选仍须完整二十路对照。请协调对话在使用 car010 可见无框真值包时更新契约，本任务不直接改公共 `STATUS.md`/`integration-contract.yaml`。

## 2026-11-18T15:08:19+08:00 双响应恢复滞回：主机候选，待 RK3588 回放

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `476156f112c5ae0a6cab6ae89e78d1c6646caa25`；本节与代码、单测同提交，结果提交号以 Git 为准。冻结 `526d491` car004 GT 第 130 帧暂态，131 帧 PSR/APCE 相对可信基准仅 `0.765/0.768`，却接受并清除暂态；该帧真值为完全遮挡。检测框在 127–129 帧没有匹配的目标，不能用 detector 解决那三帧可见部分尺寸。
- 本候选只在**已有连续响应退化**时增加双指标清除滞回：PSR 与 APCE 均未恢复至各自既有下降阈值的一半以内时，保持暂态；任一指标充分恢复则沿用原清除流程。不改变初次进入暂态、搜索最短龄、detector 请求、重捕确认或控制框语义。它不把 car010 第 156 帧提前切到搜索，避免使第 168 帧仍完全遮挡的 detector 候选提前满足 400ms 恢复门；car010 156–157 仍为独立未解问题。
- 对冻结 `526d491` 十目标 GT/DET 20 路及两段外部 VisDrone 已保存 CSV 实际执行 `python3 /tmp/nt-joint-recovery-screen.py`，脚本/输出 SHA-256 `565c044930bf4af61102c32bc9e27bb49865f00081224b72b4ec3e492c62938f` / `a27bee2f00854b0f7710b2b039c370b9eef92d7e9bc54b466b525c19a6ab2588`。静态命中仅 car004 GT 第 131 帧；这是原轨迹筛查，不能证明改后轨迹或跨场景收益。主机实际 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure --parallel 4`，RKNN-off 23/23 PASS；日志 SHA-256 `eb7f7ef89675cf54de33a268673936192b02b42f5a3a0c20e268fbe12c593047` / `65551b2cb19b1a1e3d49a0c128978c0158410eafebc2e38d708baa8e9db8fa41`。新增合成测试覆盖双指标未恢复继续暂态、完全恢复清除与单指标恢复不延长暂态。
- **Handoff/下一步**：先在 RK3588 上原生构建并定向复测 car004/010/003 双初始化和外部 0305/0137；若 car004 第 131–132 帧改善且其他路无回归，再完成十目标双初始化 20 路。同一视频内筛查未替代独立场景验证；未获板端证据前不合 main/Debug、不升级基线。

## 2026-11-18T14:57:01+08:00 二十路回放否决高度守门组合；car010 可见状态独立版本化

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`。板端候选精确源码 `34db59b483cf1efadbcc4533f1a83d5ca505f172`，源码 tar SHA-256 `8cd0da510401b6db8b7013cbb69978c3b71311504cd42ff9558c779b5a490bac`，隔离根 `/home/neardi/Project/SOT/nt-edge-extent-34db59b`，原生 CMake/build/CTest 23/23 PASS，程序 SHA-256 `bf240e267b4a2c59f7365fa24d2f9f2b85a3ea0651efa24b74f6fcdca2a651dd`；controller/headless SHA-256 `33ab0765722868329fa07c79bf9bc02c0a7109c0f173b86857e187667fbdd350` / `6eac5fe47bbe82c1e570bfacae45454123c061cd94fd358a8d9693bf4c73a301`。输入视频、tracker manifest、detector RKNN/manifest/labels SHA-256 沿用本文件 13:47 节核验身份：`eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d` / `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261` / `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c` / `ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9` / `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`；日志 RKNN API 2.1.0、driver 0.9.8。
- 实际板端 `bash /home/neardi/Project/SOT/nt-edge-extent-full20.sh`，脚本主机 SHA-256 `d9970001a6ca03fd26eb729db7a60b4320b13afc91bc35300b5d7d751847d85c`，十目标真值和 detector 初始化各十路、每路 271 帧、exit 0。原始 CSV/session/log 在板端 `runs/full-{gt,det}/car_###/`，板端与主机 `/tmp/nt-edge-extent-full20.tar.gz` SHA-256 均 `90e12fc4f990d6648caa570d2ba3a65a734b54c9201af6f414ba40bb8e583830`；主机解包 `/tmp/nt-edge-extent-full/runs/`。实际 `python3 /tmp/nt-score-edge-extent-full.py`（脚本 SHA-256 `e1a5b751a963d0d7023751462094ee28a694d42cfb037d6b1ed0d35bfd4d1f5e`），逐路 `scripts/evaluate_occ_video_1.py --scope full --source-commit 34db59b483cf1efadbcc4533f1a83d5ca505f172 --config-sha256 6eac5fe47bbe82c1e570bfacae45454123c061cd94fd358a8d9693bf4c73a301 --model-sha256 3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`；配对摘要 `/tmp/nt-edge-extent-full-summary.json` SHA-256 `6093053a467d364e4373570df710f535ad4d6deb34509ed68c0973a36df1858d`，逐路正式报告在各目标目录。对同一修订真值的冻结 `526d491`，PASS 14/20；组合候选仅 PASS 7/20。car004 228–232 帧的下缘可见框改善，但 127–129 仍框高过大；car003 新增 239 帧错误恢复及出画接受，car005 GT 在完全遮挡 172 帧仍接受，car006/008/009 等新增遮挡门失败，car007 detector 路晚期框变小偏移。**组合候选否决，不合 main/Debug、不升级基线。**
- 因果对照在**同一程序**仅将 `recent_visibility_frame_drop_ratio=0.95` 改为 `0`，隔离 controller/headless SHA-256 `df2bcbaedacedb6a14d9e3acf5f3c240af680081b296ad9497a262be68a6c0bb` / `c3c11a2d76b51e1d4838edf5273e6f6e92f9fd4d1e9748c3aa53cd5b66608cdb`。实际板端 `bash /home/neardi/Project/SOT/nt-edge-extent-noguard-probe.sh`（SHA-256 `9e412138ecf26c677d7e0be45517cb5faf0daaa8dfdb74e3fb1f15efb59bc4cd`），11 路各 271 帧 exit 0；原件 `runs/noguard-{gt,det}/`，归档板端/主机 `/tmp/nt-edge-extent-noguard.tar.gz` SHA-256 `54fc4078390ff9ee5dacc5c3e6fd4c9205ff2276b9a0e916ce5c29e52cb46972`。实际 `python3 /tmp/nt-score-edge-extent-noguard.py`，脚本/摘要 SHA-256 `95de03765d763f284990f4aa7c51183c50665a3181feb713e9628bbcf62dac8c` / `161bbfc89cfe5839646b6f9f145ec2fb82f082b0fecb44b16011be5341c945dd`。11 路全部回到冻结基线的 PASS/FAIL 与恢复帧；car004 GT/DET 可见 IoU 精确回到 `0.8456/0.8302`，贴边尺寸发布单独无实质收益。car005 168 帧原始高度仅较前一帧降约 4.8%，被 5% 门槛过滤，至 172 帧仍接受完全遮挡框。上述证据指向新增高度门导致广泛退步，源码/配置/单测已恢复到 `44facb4` 的算法内容，`git diff 526d491 -- rk3566/nanotrack_v3/src rk3566/nanotrack_v3/include ...` 无差异；主机重新 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure --parallel 4` 23/23 PASS，日志 SHA-256 `8f8eaf7443af8a62017750b7f49e7f4c6bb4ac1e74e6d1a9452d5cb75b475ec4` / `4de17636b2abbcc210ba2430e447f978cc9c24ce33a2b00ff4e28f379e22972f`。恢复后的提交号以本节 Git 提交为准；此恢复仅主机验证，未声称已在板端重新部署。
- 用户确认 car010 第 149–155 帧**部分可见、无精确框、暂不计 IoU**。原 `/home/u20/single-object-tracking/data/occ_video_1/ground_truth/car_010/checksums.sha256` SHA-256 `5c56d98b583120cb1fef755768e70c3b0e8b5d93e4d3460847497fa463c9f2da` 不变。实际 `python3 scripts/prepare_car010_visible_unboxed_v2.py --source-root /home/u20/single-object-tracking/data/occ_video_1 --output-root /tmp/nt-car010-truth-stage`，脚本 SHA-256 `9d693c397989a26f2e8cdff316477fa3c17b0d931b4e10ecbacb9494ed51f1c2`；新包无覆盖地安装到 `ground_truth_versions/car_010_visible_unboxed_v2`，`checksums.sha256` SHA-256 `364746fb0c93f7bdc264487f40adf2a83c545b9e472881976919f75b9fa0e1bc`、逐文件 `sha256sum -c` PASS，备份 `/tmp/nt-car010-visible-unboxed-v2-package.tar.gz` SHA-256 `ecb9ef7df2f38d5971f1a09b91d04ce6c7c16fee90aebd237fd13ed5c89fe8eb`。新版本仅改 149–155 为 `visible/user_confirmed_visible_unboxed`；156–168 保留全遮挡；评测器 `load_truth` 验证 149–155 `observable=false`、无框。用同一版真值重评冻结及当前 car010 双路，四份报告分别在 `/tmp/nt-edge-guard-results/runs/edge-guard-{gt,det}/car_010/score-visible-unboxed.json` 和 `/tmp/nt-edge-extent-full/runs/full-{gt,det}/car_010/score.json`；四路都仅剩 156–157 帧完全遮挡时尚处 SUSPECTED，未达到正式 SEARCHING 门，仍 FAIL；不把 149–155 的无框帧当作 IoU 成功。实际 `python3 -m unittest tests.test_evaluate_occ_video_1 -q` 26/26 PASS。
- **Handoff/下一步**：请协调对话在采用 `car_010_visible_unboxed_v2` 时更新集成契约；原包保留。算法仍以冻结 `526d491` 内容作为本实验分支对照，先查 car004 127–129 可见尺寸、car010 156–157 遮挡状态和 car003 的独立失败，再设计跨场景约束；外部 0305 的单场景收益不允许覆盖十目标回归。现有真值与吊舱人工验收范围不同，本轮无吊舱验收。

## 2026-11-18T14:29:17+08:00 贴边可见范围输出候选：主机通过，待 RK3588 定向回放

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`；起点 `d454f3a953b2c023562ec0004d7d2880cc4356ec`。上一候选源码 tar 主机/板端 SHA-256 `42012c07489a7239f61b1d21863dd0ddd323a1c241fb994e843abf7aab107cc6`，板端隔离根 `/home/neardi/Project/SOT/nt-frame-drop-d454f3a`，实际 `bash /home/neardi/Project/SOT/nt-frame-drop-setup.sh` SHA-256 `ef44dc8a20ca3a0a83579e0807c3ffedc0c8eb212612c79eb0b7a50c5207ec6e`；RK3588 原生 build/CTest **23/23 PASS**，程序 SHA-256 `9ee9753d4ecf9e547a9c39cfe3ac93715b565c926fabc60520a27933da95fba2`，controller/headless SHA-256 `33ab0765722868329fa07c79bf9bc02c0a7109c0f173b86857e187667fbdd350` / `fb96f6d9031caa11a99dcaf61a2a348b7fd036a84eecb057f552c5db7601b5aa`，cmake/build/CTest 日志 SHA-256 `614b6666936af74cf9e8096652ef9ece23a16a2be7c6dfafafc71b89daa25b04` / `a925e6a91ceafdab70eca5dc56a2834f2d9dc6b250f8207c5c110162e9858533` / `9e376e0075647e8ce90cbd1e38b552de32ed75ee041ccda50ed1bf0acd78c57d`。视频、NanoTrack/VisDrone9 模型、manifest/labels 与本文件 13:47 节已核验身份相同；外部两段原标注身份与同节相同。
- 实际运行 `/home/neardi/Project/SOT/nt-frame-drop-focused.sh` SHA-256 `7856a9c56a6bb59055ac29759cc041a43e1b360a97c5a1273b8e47595f84c8a5`；外部 0305/0137 两段 141/233 帧，canonical car004/010 GT 与 detector 初始化四路各 271 帧，全部 exit 0。原始输出板端 `runs/focused/<name>/{frames.csv,session.json,run.log}`，板端 `/home/neardi/Project/SOT/nt-frame-drop-focused.tar.gz` 与主机 `/tmp/nt-frame-drop-focused.tar.gz` SHA-256 均 `5a969fc0fcd161618fd79e0e94295b87a455d43af98f44d39532dabadf247df0`。主机四路正式 `scripts/evaluate_occ_video_1.py --scope full --source-commit d454f3a953b2c023562ec0004d7d2880cc4356ec --config-sha256 fb96f6d9031caa11a99dcaf61a2a348b7fd036a84eecb057f552c5db7601b5aa --model-sha256 3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261` 报告位于主机 `/tmp/nt-frame-drop-focused/runs/focused/car_###_{gt,det}/score.json`；外部对原标注评分与 canonical 配对摘要 `/tmp/nt-frame-drop-focused-summary.json` SHA-256 `df960abb7beff038c5862d37c305e1d671cf5d48cdf945cd7c9c6a247569d25e`。
- 外部 0305 清晰帧平均中心误差 `21.23→4.08px`，搜索帧 `24→0`，mean IoU `0.657→0.709` 且无已接受低 IoU；0137 与基线完全一致。car004 第 130 帧双路仍 SUSPECTED，第 131 帧 detector 初始化路仍 SEARCHING，保住原遮挡响应；但 car004 GT/DET accepted-visible mean IoU `0.846/0.830→0.821/0.809`，第 228–232 帧新增可见框失败（发布框高度约 55px，真值可见部分约 23–28px）。car010 双路基本不变，其 149–157 帧现行真值遮挡语义失败没有解决。用户已确认可见部分框是 car004 的目标输出，因此不能把第 228–232 帧只因跟踪中心对准就判为通过。**d454f3a 单独版不晋级，不跑完整二十路。**
- 本提交在原有“两次独立、同类唯一 detector 与已接受 Recent 几何一致才改发布尺寸”的通道增加通用贴边可见范围：当 detector 与 NanoTrack 的框同触同一图像边缘且 detector 该方向尺寸显著更小时，发布用 detector 的可见尺寸，并在既有中心位移上限内向 detector 中心修正；内部 NanoTrack crop、模板和运动状态不变，证据过期后不持续外推。错误类别、远处、歧义或陈旧 detector 仍不可触发。新运行时合成测试检查双检测确认后发布框缩小而内部框不变、错误类别不可缩框；实际主机 `cmake --build /tmp/nt-ten-target-build --parallel 4` / `ctest --test-dir /tmp/nt-ten-target-build --output-on-failure --parallel 4`，RKNN-off **23/23 PASS**，日志 `/tmp/nt-edge-extent-host-{build,ctest}.log` SHA-256 `a48cd664b31882cfb258f35a3ab7bb18b4f7849b5dbe43de3c6f0848f453e9b2` / `bb43b456df7bde8e3e00b9e754771369a2ad3f3960827801acc86535ab83c776`，源码/单测 SHA-256 `493538467fa7fe9e1d68b91e2ecd0f17046e9baca92193d9112937d233ae280a` / `5a024eb143690bd1f2f5153fca0028b50fd5f3e35a6b2a7f74aa80b31fb688e8`，`git diff --check` 通过。
- **Handoff**：本提交仅为主机候选；先在 RK3588 原生编译并复测上述六路，重点看 car004 第 127–129、211–240 帧可见框与出画后误接受，以及外部 0305 收益是否保留。若仍有显著退步，撤回候选；定向通过后再对十目标双初始化二十路完整回放，不以界面烟测代替真值评分。

## 2026-11-18T14:17:11+08:00 连续原始高度守门候选：主机通过，待板端筛查

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `44facb4edbd1367219dab79c6f7083f6b8819970`；本节与候选源码、配置、单测同提交，结果提交号以 Git 为准。用户已确认 car004 第 127–129 帧按桥栏上方**可见部分框**考核，现有真值保留；该三帧仍是未解决的算法尺寸失败，不把全车框判成正确，也不按桥栏位置裁剪。
- 只读重放已保存 CSV 发现：外部 VisDrone `uav0000305_00000_v` ID18 第 88 帧开始错误 SUSPECTED；该帧 NanoTrack 原始高度对上一帧仅 `0.985`，但对受缩放速率限制的已发布高度为 `0.911`，中心残差 `0.04025` 刚越过旧阈值。car004 双初始化第 130 帧原始高度相对上一帧约 `0.928/0.931`，确有突降；第 131 帧还有独立视觉遮挡搜索证据。旧 `recent_visibility_height_ratio=0.98` 只比较已发布高度，可能把平滑透视缩小误当遮挡。
- 先仅在原 `526d491c8c2cf072e27cccf0a7ea80bb29819c7e` 板端程序上关闭这条高度判据（ratio 与 center 两项必须同时为 0；第一份只关 ratio 的隔离配置因解析校验失败，运行日志保留在板端 `runs/no-height-probe/`），正确脚本 `/home/neardi/Project/SOT/nt-no-height-probe-v2.sh` 主机 SHA-256 `b3b084b90e764ac054f257b91964647d6937437702f9d2e36375e7c8d5de0f34`。controller/headless SHA-256 `2e681e7bd5ee92ab698bbda0541f037fc1b4767f228ae4f5984e835016c510b5` / `179c9866e4ab655ee3bb0940cc0b712ff8e273962796a1739c58c69e7b540f89`；程序 SHA-256 `21b9aecacb16b06b6c9232e9f27277bec4575db882052f09d7e6479356c506d3`，视频/模型/manifest/labels/RKNN 环境沿用上节身份。实际六路板端运行 exit 0，外部两段分别 141/233 帧，car004/010 GT 与 detector 初始化四路各 271 帧；原件 `/home/neardi/Project/SOT/nt-edge-guard-526d491/runs/no-height-probe-v2/`，板端/主机 `/tmp/nt-no-height-probe-v2.tar.gz` SHA-256 均 `6792e510babd2ea75cacdf732136a50bea889bbdbb3dbb910bbd9e1b5b5825a4`。主机正式 `scripts/evaluate_occ_video_1.py --scope full` 四路报告各在 `/tmp/nt-no-height-probe-v2/runs/no-height-probe-v2/car_###_{gt,det}/score.json`；外部原标注评分与四路摘要 `/tmp/nt-no-height-probe-v2-summary.json` SHA-256 `dc42285649162b099266e955dc2c7cadfe28e075e9b4e8b24af100b39b7d5dbb`。
- 完全关闭该判据时，外部 0305 清晰帧 mean IoU `0.657→0.709`、平均中心误差 `21.23→4.08px`、搜索帧 `24→0`，0137 完全不变；但 canonical car004 GT/DET 的 accepted-visible mean IoU `0.846/0.830→0.811/0.808`，229–233 帧新增部分可见框失败，因此**不能全局删除**。car010 双路输出基本不变，149–157 帧遮挡语义失败未被解决。报告使用相同板端源码/模型/视频，唯一实验差异是高度判据开关。
- 本候选保留旧的已发布高度比与中心残差门，新增默认 0（关闭）、可选 `recent_visibility_frame_drop_ratio=0.95`：仅当当前 decoded 高度还比上一**可信** NanoTrack 原始高度突降至少 5% 时才把汽车高度变化作为遮挡证据；在正式可见配置显式启用。可信原始高度在接受普通跟踪观测时更新，进入 SEARCHING 时清除；暂态帧不覆盖参照。配置解析/范围/身份与控制器单测覆盖首帧、缓慢变化、突降及暂态保持。实际主机 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure --parallel 4`，RKNN-off **23/23 PASS**；日志 `/tmp/nt-frame-drop-final-{build,ctest}.log` SHA-256 `93321ca39f9b06f7b3513e05577856536b26f6c0a3f3cff4358bd02bf6d206c4` / `e863bc6d26a093cef8fd618ee22c4b613639d0f38dcb19566f111d1f12ed98e3`，算法源码/单测 SHA-256 `6d8e16ee85b8affb641e2a136daa389bc06b55d20a0aed96e20b2362f947e197` / `c7be2e18375d392ec458e06bebae1ad84b9b9c22d7a85909f17c50d8ee661a90`，`git diff --check` 通过。
- **Handoff**：这只是主机候选。先在 RK3588 原生编译和外部 0305/0137、canonical car004/010 双路定向重放；若保持身份/遮挡门且改善外部，再跑全部十目标双初始化。不可因主机单测或单一外部收益合并 main/Debug；该门仍不能单独解决 car004 第 127–129 帧的可见部分框尺寸。

## 2026-11-18T14:01:05+08:00 100 ms 全局搜索等待板端反例：撤回配置试验

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `bcd3f1d9969ddb236500a8f9c5388a0ef9ba32ef`；算法源码及板端程序仍是 `526d491c8c2cf072e27cccf0a7ea80bb29819c7e` / SHA-256 `21b9aecacb16b06b6c9232e9f27277bec4575db882052f09d7e6479356c506d3`。只在板端隔离实验配置把 `search_reacquisition_min_age_ms=400` 改为 `100`，未改仓库算法、正式配置或原有运行记录；实验 controller/headless SHA-256 `d051ce12a192d4a41dd111b4e65f7a82ae112ad2089691445da4f19af95b9626` / `311fdd102bec34c626727caf69d1f4ed321c0b91e93ae1c02ccaba603b17d99d`。视频、tracker 与 detector 模型、manifest/labels 和 RKNN 运行环境仍沿用本文件 13:47 节已核验身份。
- 实际复制并运行 `/home/neardi/Project/SOT/nt-age100-probe.sh`，脚本主机 SHA-256 `e24fa08e3fe529d516315b7d50d08e49d7b63de895f9e30c9ca4b08d18f840bb`；四路 detector 首帧初始化框详见该脚本，car004/002/003/010 各 271 帧、程序 exit 0、CSV 各 272 行。原件板端 `/home/neardi/Project/SOT/nt-edge-guard-526d491/runs/age100-probe/car_###/{frames.csv,session.json,run.log}`；板端 `/home/neardi/Project/SOT/nt-age100-probe.tar.gz` 与主机 `/tmp/nt-age100-probe.tar.gz` SHA-256 均 `3939b84f8d48b67098c395dc4c82f49fdf9a1b15e11198629c31c81e66540d26`，主机解包 `/tmp/nt-age100-probe/runs/age100-probe/`。
- 主机逐路实际执行 `python3 scripts/evaluate_occ_video_1.py --dataset-root /home/u20/single-object-tracking/data/occ_video_1 --target-id <target> --boxes <frames.csv> --session-metadata <session.json> --scope full --source-commit 526d491c8c2cf072e27cccf0a7ea80bb29819c7e --config-sha256 311fdd102bec34c626727caf69d1f4ed321c0b91e93ae1c02ccaba603b17d99d --model-sha256 3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261 --output-json <score.json> --output-markdown <score.md>`；四份报告在各主机目标目录，002/003/004/010 的 SHA-256 分别 `15d19890d702a6709cc3e0ccd6dc792233443cfe2326dd220282c201eb664705` / `4c204a39d867f6b983f6cf6bfffd7557eff98f43ba2e629585b74c3c4d01141a` / `b4e6f7481c0b6915fd488668ccfcfc9fdd05ee8c5098e55ad30fc0a6964f6513` / `293e1803686f8ca3fb27ca637b1047e80397c11daccc801602c4405f9c93c102`。基线对照摘要 `/tmp/nt-age100-probe-summary.json` SHA-256 `f4db1deab8a22d48ace4a47e7204818efce443eff0ef2dd805c1ba9150af070e`。
- car004 搜索期 214–215 帧 detector 对真值 IoU 约 `0.99/0.95`，双头原始框也对准目标；100 ms 版在 217 帧恢复，但输出约 `69×67` 而 detector 约 `68×48`，223 帧又进入 SEARCHING，可见 SEARCHING 73→67 帧，仍 FAIL。car002 从基线 PASS 退为 FAIL：220 帧在远离运动预测约 1.5 个目标对角线的另一辆车上提前恢复，230–270 帧身份与可见框失败，mean accepted visible IoU `0.807→0.666`。car003 在 136/187 帧错误恢复，出现 241–270 帧出画后误接受，IoU `0.820→0.576`。car010 在 165 帧遮挡期间提前恢复且不符真值，IoU `0.841→0.751`。car004 第 42 帧本来被旧真值误标为完全遮挡，100 ms 版此帧正式失败不能当算法反例；其余三路恶化不依赖该标注。
- **裁决/Handoff**：全局缩短等待否决，保持正式 400 ms。car004 217 帧短暂收益说明等待门造成一部分漏跟，但宽高/试用期仍有独立问题；早恢复若再试，须同时阻断 car002 远运动错认、car010 近运动遮挡错认和出画后持续误接受，并保留 detector 只能经新鲜 NanoTrack 验证后用于控制的约束。car004 第 127–129 帧真值只框可见部分，已向用户核对该段目标框定义，在得到答复前不编造完整目标框或加入桥栏特化规则。此试验不合 main/Debug，也不升级基线。

## 2026-11-18T13:47:14+08:00 边缘守门二十路完成；car009 用户确认状态版本化

- 实验源码 `codex/ten-target-smoothness@526d491c8c2cf072e27cccf0a7ea80bb29819c7e`，tar 主机/板端 SHA-256 `252230f08a9db89c0fa0e65cd4a57ef4d5db672b64a3683132140fd6341b0853`，板端隔离根 `/home/neardi/Project/SOT/nt-edge-guard-526d491`。实际 `bash /home/neardi/Project/SOT/nt-edge-guard-setup.sh` SHA-256 `5803acb48311391f89f8bcf855ff9d53d51be7b5a19d89d8a883f1b9e36d2516`，RK3588 原生 build/CTest **23/23 PASS**；程序 SHA-256 `21b9aecacb16b06b6c9232e9f27277bec4575db882052f09d7e6479356c506d3`，controller/headless 配置 SHA-256 `bb19d2e8e085dd57084653b6ca37bb981b10ea2a0d00bc05d166f8a508c6e26f` / `f03a5efaac12d851fe695952cd20533c9660a36f0b6c3e6211b89ac34ae5d6b2`，cmake/build/CTest 日志 SHA-256 `fd6d77c6cc8bca6d13aded7f6e486888dbace1723b39da523bc1828702ddc3f1` / `40a5721322b4c4d2dbcd01b14666718aa997cda15af0e0a7df66f3e2afe3cfd9` / `eb4c44366574996bfe0066ca9028529e558ab39ffa0f8f5847da8dbf3e3d3b88`。输入仍是冻结 canonical 视频、NanoTrack/VisDrone9 模型/manifest/labels，哈希见 2026-11-18T10:55:56 节和 `/tmp/nt-center-gain-board-inputs.sha256`；外部两段视频与上节身份相同。
- 实际运行 `/home/neardi/Project/SOT/nt-edge-guard-external.sh`（SHA-256 `c73e58aaf4e7e470c48a599a8cfb77c62b78631702564216ab43a50a8128e2fe`）及四个 `nt-edge-guard-{focused,rest}-{det,gt}.sh`（focused-det/gt SHA-256 `a94a7104fcacd2aed8f43db4988c9780d7b6e354a8edf0acae0b27ef52aca905` / `d9e1c8499e512bfca5cb70c6f66054f29095172d141b15096653218a31f8db12`；rest-det/gt `dd26b7628817362c438df96143059ee5e1dbc46c29486604909c4b14bcdbc51e` / `ba6d5f3dec40b6d2bd5d6649d0a7beaa814061d8dcfbcda05c29e636df31202b`），外部 141/233 帧、canonical 十目标 GT/detector 初始化 20 路各 271 帧，程序均 exit 0。板端原始 CSV/session/log `runs/external/<sequence>/` 与 `runs/edge-guard-{det,gt}/car_###/`；主机归档 `/tmp/nt-edge-guard-{external,det10,gt10}.tar.gz` SHA-256 `bddba0afc306617a72fab2e6b9e9bd626332c1aec91fb4ff334867fd09046a6b` / `011e9fac977c0711a2b17f458160c93cfef96792c943ede67f1e600f9f3fd09d` / `942a4d5c54d2d68165aa33656c7edbe1bab6b8d4dc57b0a6c40d63629f7712e9`。
- 实际 `python3 /tmp/nt-score-edge-guard-external.py`、`python3 /tmp/nt-score-edge-guard-full.py`，外部报告 `/tmp/nt-score-edge-guard-external.json` SHA-256 `25b1a15b0c2dda1a9e188e8c1e0791530d3487b5daca95ddb207a3fa669ff8fb`；20 路摘要 `/tmp/nt-edge-guard-full-summary.json`，脚本/摘要 SHA-256 `9bb28f591a88715ebfa8720efbec9938f9cf902314036033c07165d0aa156aa5` / `5e6729d2a3b39d5befc6459dea253fbc67a8293dcd829609a5aa2116b41c1ab8`。外部 0305 ID18 仍第 111 帧正确重捕，但 115–123 帧 SUSPECTED、124 帧又 SEARCHING；0137 ID22 与 `3e7d5de` 完全一致。十目标 car001/002 双初始化均 PASS，car004 第 224 帧错大重捕已消除，GT 路可见接受框 mean IoU `0.8456`；detector 路可见 SEARCHING `57→37`，但第 127–129 帧尺寸过大，冻结正式 `visible_boxes` PASS→FAIL，mean IoU `0.8413→0.8302`。图 `/tmp/nt-car004-f127-crop.png` SHA-256 `1ca067c2ed091b100045c6d6eadc1c8e60b2c5aa005e238024f2fc4b823edc56` 明示真值只包桥栏上方可见车身（约 `45×30`），算法框约 `45×53` 包含桥栏下方；当帧无可用 detector 框。其余正式 verdict 与冻结基线相同；未以针对桥栏的阈值或单场景光流掩盖这三帧。
- 用户明确确认 car009 第 115–120 帧为**部分可见**，原 `ground_truth/car_009` 包 SHA-256 `2c85fb097a845730bd9ed86402ae829000d63ca93e31cd9296dcee7cab882117` 保持原样。实际 `python3 scripts/prepare_car009_visible_unboxed_v2.py --source-root /home/u20/single-object-tracking/data/occ_video_1 --output-root /tmp/nt-car009-visible-unboxed-v2-dataset`，脚本 SHA-256 `3fbf3309bb25dffb286a11dad58732c3668856ad70b8e82066b370e7eda13150`；将六帧记为 `visible/user_confirmed_visible_unboxed`、正式框仍空、`ordinary_iou_allowed=false`，删除旧 115–120 full 事件并更新元数据/校验。新包已无覆盖地安装到 `/home/u20/single-object-tracking/data/occ_video_1/ground_truth_versions/car_009_visible_unboxed_v2`，其 `checksums.sha256` SHA-256 `5abb03ff2e995b9c90d964c6e870e51d7e784b348be5ad8261ee80c27e5ba2a7`，`sha256sum -c` 全项 PASS；备份 `/tmp/nt-car009-visible-unboxed-v2-package.tar.gz` SHA-256 `aff99c99e542221228d6e043669523d7e0ed904727c201eac5ad61f1a8d1881f`。本提交在评测脚本中增加可选版本身份，未改默认真值。
- 使用 `--truth-version car_009_visible_unboxed_v2` 对相同 `526d491` car009 GT/det CSV 重评，两个完整报告 `/tmp/nt-edge-guard-results/runs/edge-guard-{gt,det}/car_009/score-visible-unboxed.json` SHA-256 `c8ea33af79ced4acd5d3a0ebee076fe2dd6e928b25703fb93c16ea7aa3cfed00` / `1649bb01b27a5b5b21b2edcdd91d925f266f273032237b06b8724b0b2790d6a9`，均 PASS；六帧无精确框，不计 IoU，不能据此证明框紧贴目标。实际 `python3 -m unittest tests.test_evaluate_occ_video_1 -q` **26/26 PASS**（系统/venv 无 pytest，故使用仓库原有 unittest）。**裁决/Handoff**：`526d491` 继续为实验候选，car004 detector 路真实尺寸门仍 FAIL，不合 main/Debug、不升级基线。请协调对话在采纳新 car009 真值版本时更新契约；仅该版本的状态修正被用户确认，未替 car010 149–155 或 car003 130 做未经确认的真值修订。

## 2026-11-18T13:29:53+08:00 画面边缘禁止不完整 detector 框重设全目标尺寸候选

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，起点 `c69c57068ee1951f71a1524fb62c3eafb3e13cbb`；本节与回退代码、新单测同提交，提交号以 Git 为准。撤回上一版全局原始高度参照和 detector 尺寸发布，恢复 `ff0dd2f` 的对应源码/配置。唯一保留的新算法改动：只有 detector 完整位于图像内且各边离边界超过 1px，才允许它与两路 NanoTrack 原始框建立新尺寸/新视角的 re-center 假设；贴边候选仍可走原有严格恢复路径。理由是画面边界裁掉的可见部分无法证明全目标尺寸，car004 第 224 帧恰是这种情况。合成测试增加下缘贴边且双模板同意的反例，要求不得绕过旧框尺寸保护。
- 实际主机 `cmake --build /tmp/nt-ten-target-build --parallel 4` 与 `ctest --test-dir /tmp/nt-ten-target-build --output-on-failure --parallel 4`，RKNN-off **23/23 PASS**；日志 `/tmp/nt-edge-guard-full-{build,ctest}.log` SHA-256 `99b70251a78c8b8345229bab9b6a7a599dbce8028b127f5661ab3815e0492e1e` / `98260c588ceec64ae373c380929f4e815b85f40d21a04cb05248dfe7f4c05808`，源码/单测 SHA-256 `6f8ca519a79d7b63057b98d9362d2b829b720343305c7ebfb158bd82b661c36f` / `cfd7d508dbc1abe9f1f264f72f4a0e4e77730a1cba3e6fb5e6af06bed85acb59`，`git diff --check` 通过。**Handoff**：仍仅主机候选，先上板复测外部 0305/0137 与 car004 双初始化，核对第 224 帧是否不再误重捕及外部第 111 帧是否保留正确重捕；通过后才扩 20 路，不合 main/Debug。

## 2026-11-18T13:27:33+08:00 原始高度与 detector 尺寸组合候选板测否决

- 精确源码 `codex/ten-target-smoothness@c69c57068ee1951f71a1524fb62c3eafb3e13cbb`，主机/板端源码 tar SHA-256 `dd8d05652b6b9c3ee93b0c846f053db9d6e3b412ca7dbe63094aec3d2ef8722c`；板端隔离根 `/home/neardi/Project/SOT/nt-raw-size-c69c570`。实际运行 `/home/neardi/Project/SOT/nt-raw-size-setup.sh` SHA-256 `790f82efa34e96cabec0861b2acb7c1735745e1cd579fe9783d1e296821a2ca0`，RK3588 原生 build/CTest **23/23 PASS**，程序 SHA-256 `2670f2ed7f489346d89a204df3032f9a6b174ae15a75a38089817fbdf40d09bd`，controller/headless 配置 SHA-256 `4ae8558a043756775c85eb699bdd409033ef1079b62012f87ab80c2fe3f9b13a` / `6a7ec84ac40bb7b37783aa4dfc1c8f6d4602d07769125a16bf7f35d607c8def3`，cmake/build/CTest 日志 SHA-256 `b4512f578d04a2dd0eb159daf27db9838710620f1abf06c4429d7beccbebd9bb` / `2ba9cac0bc1b2ec182a6eb773cb99f8e907799e5f4df1b5bd3f2cf5d361c97f3` / `698a56f67584f787d5669aed8e923be5eaff3196b4ddf215fbcd899360f81c40`；输入视频、模型、manifest/labels 身份沿用上一节。
- 实际板端 `nt-raw-size-external.sh`（SHA-256 `d3a9221def3b78402d4ce2cd51b0fdeab69052a448556dc2280fece9f5d021d2`）及 `nt-raw-size-focused-{det,gt}.sh`（SHA-256 `b6e18d0e41386ccf9948c077af2418f105de7b1eb05faae756d1c895991d290b` / `132554ec6195bd819f8d7e6f1f01be9b6a68d224b990e4b7b9ea8c3b38fde1a9`），外部两段 141/233 帧及 car001/004 两种初始化四路各 271 帧，程序 exit 0。板端原件 `runs/external/<sequence>/` 与 `runs/raw-size-{det,gt}/car_{001,004}/`；主机原件包 `/tmp/nt-raw-size-{external,focused}.tar.gz` SHA-256 `54e2a1cb4a178ab6332d803188a24a7b33dfec34352af40afae197b8a951a8ce` / `29648ad2328dffc551754e5c509922f4850736af608b6e7213bdde7dd2e4a4b4`。
- 实际 `python3 /tmp/nt-score-raw-size-external.py` 与 `python3 /tmp/nt-score-raw-size-focused.py`，报告 `/tmp/nt-score-raw-size-external.json` / `/tmp/nt-raw-size-focused-summary.json` SHA-256 `2056c69cbd848a17067a65352f21943826445e09017d639e3ca60a55fe5d19e2` / `81a531b56d4d1610d9b864bcb578b24c849e6461447c98a6e26e93c552df3752`。外部 0305 ID18 全段中心误差 `22.98→4.14px`、IoU `0.6341→0.6854`，但框偏小且 0137 ID22 IoU `0.7615→0.7438`、中心误差 `109.21→123.77px`。car004 detector 初始化相对冻结基线可见框平均 IoU `0.8413→0.7945`，重捕 `[144]→[146,181,218,238]`，有新增全遮挡/出画仍接受；car001 两路仍 PASS 但 IoU 下滑。**裁决**：组合候选明确否决，不扩 20 路，不合 main/Debug，不升级基线。后续撤回全局原始高度参照和 detector 尺寸发布；仅独立评估有物理意义的画面边缘恢复限制。

## 2026-11-18T13:20:21+08:00 原始高度参照与双模板确认后的 detector 可见尺寸候选

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，起点 `ff0dd2febf6564cbdb868794bc43730d36197a4e`，本节与源码/配置/测试同提交，提交号以 Git 为准。第 115 帧外部轨迹中高度判断把约 74px 原始观测同约 78px 平滑输出直接比较，现记录上一次可信原始高度作同类参照；初始化和重捕时重置。真实原始高度收缩且位置异常仍保留原判据。
- 仅在 full-object detector 与两路原始 NanoTrack 结果唯一匹配、两帧 NanoTrack 确认并完成 re-center 时，新增可配置的 `recovery_recenter_use_detector_size`：默认 `false` 保持通用旧行为；本次实验配置设 `true`，发布 detector 可见尺寸，中心仍按既有变化死区融合。detector 本身不授予控制。配置身份哈希随该字段变化，session metadata 记录该值；板端实验配置 SHA-256 `4ae8558a043756775c85eb699bdd409033ef1079b62012f87ab80c2fe3f9b13a`。合成控制器测试验证尺寸采用 detector 框、原始高度与平滑高度分离、真实缩高仍进入暂态；旧的部分车身恢复全框回放使用默认 `false`，保持原合同。
- 实际主机 `cmake --build /tmp/nt-ten-target-build --parallel 4` 和 `ctest --test-dir /tmp/nt-ten-target-build --output-on-failure --parallel 4`，RKNN-off **23/23 PASS**；日志 `/tmp/nt-raw-size-full-{build,ctest}.log` SHA-256 `c7bc3ed5c6f3f542a2147252b42fd019a05b65c22a2f01ccdee34cf701104ce3` / `4a43974503d566e1b80d6f2aa1078f93f465a1e1f3805976942fc54c184c9e73`，`git diff --check` 通过。**Handoff**：此为未上板候选；先在 RK3588 精确提交上测外部 0305/0137 与 car004 GT/DET，再核查完整十目标双初始化轨迹，不以主机测试宣称改善或合 main/Debug。

## 2026-11-18T13:12:00+08:00 恢复试用期候选板测改善外部轨迹，但十目标 car004 退化

- 分支 `codex/ten-target-smoothness@3e7d5dea1f764496d267d826a9b798026ccae067`，源码 tar 主机/板端 SHA-256 `fbb1f9091a5f3e40e06fcc2d7bb0b22e1c2c06ea93754f96e582e056d76be478`；板端根 `/home/neardi/Project/SOT/nt-recovery-probation-3e7d5de`。实际运行 `/home/neardi/Project/SOT/nt-recovery-probation-setup.sh`（SHA-256 `fd735f7f853d87a0e6d5aee7cec4501de6183454c081f08293c1f92440394957`），RK3588 原生 CMake/build/CTest 23/23 PASS；程序 SHA-256 `e2c4cd28ef75df731db9308c7d325a6c55b0325f157ef5e679d1c943eef68859`，controller/headless 配置 SHA-256 `bb19d2e8e085dd57084653b6ca37bb981b10ea2a0d00bc05d166f8a508c6e26f` / `0bf37ea3448b7c76ac4d73c0061a6b28560339cb1e7a35ae68d72d8a2d664c44`；板端 cmake/build/CTest 日志 SHA-256 `5febc57e589d3528bbe8d22c709c36d9a572cd2509c2847f301de4f147bc3018` / `1e5d42d31c9723c57c88d6953975f1854936f6d7c70df98edfc953142457996e` / `bdee93ebdfca3f2222312078b0ece3a4cfa36f5bf57fb21a4a11242fb1975365`。模型、manifest、labels、视频身份沿用上一节，未更换。
- 实际运行 `/home/neardi/Project/SOT/nt-recovery-probation-external.sh`（SHA-256 `eacf5e9c933385eacb14d7db8534eddc485eedf98c6c5f6c2bf033fed92344cb`），两段外部 VisDrone 视频分别 141/233 帧、程序 exit 0；板端原件 `runs/external/<sequence>/{frames.csv,session.json,run.log}`，主机包 `/tmp/nt-recovery-probation-external.tar.gz` SHA-256 `5e5957fbec7ca3d0944bc3bd4fbd277b13c8c83e8e576471b08ce672105689cd`。实际 `python3 /tmp/nt-score-recovery-probation-external.py > /tmp/nt-score-recovery-probation-external.json`，脚本/报告 SHA-256 `40994b425ff5a6a69d894ef85e786e9f38614881355395a6ef4bb4af48b1680c` / `b14e4f3b815e9d04430680574bc9cb7dbb0a736715ba984c291ed004320bdbfc`：0305 ID18 第 111 帧正确重捕、112–114 帧持续 accepted，相比前候选全段平均 IoU `0.5925→0.6341`、中心误差 `51.50→22.98px`，但 115–123 帧又 SUSPECTED、124 帧 SEARCHING；0137 ID22 与前候选逐帧相同。原因是试用期提交后 `partial_visibility` 将约 74px NanoTrack 原始高度与约 78px 平滑输出高度直接比较，误认缩小；两者处于不同处理阶段，不应直接作比例。
- 按与 `8ac400b` 完全相同的首帧真值/detector 初始化框，实际依次运行板端 `nt-recovery-probation-{focused,rest}-{gt,det}.sh`，四脚本 SHA-256（focused-det/rest-det/focused-gt/rest-gt）分别 `38422dd2afd14b155cb3b58af258ed8d369fd517e85190eadfaf74d01e9792cf` / `95197463f6bfff1ec0c3b637b2f4e515afd4fd6b68ab79281264dc29ee9d6495` / `9e0be00ec1a93a9fd480e56d2b235a9523c62e592958fa2583c6c078fccd31dd` / `2a1070713f77f60f3da45a2db6f789b34bc36c09c2123e204405700e19307cab`，20 路每路 271 帧、程序 exit 0，板端原件 `runs/probation-{gt,det}/car_###/{frames.csv,session.json,run.log}`；主机归档 `/tmp/nt-recovery-probation-{det10,gt10}.tar.gz` SHA-256 `8fb8d16796126f88bae8e10b46035f0ec872bee96c2b66c6c0cb4115db68d4c8` / `4e1940495ee23cd7f177ee228250a01c6dc6a1c8ac0754a1e17d5ba6142894f0`。实际 `python3 /tmp/nt-score-recovery-probation-canonical.py`，脚本/报告 SHA-256 `5282e0e13e6fec0a4aad528ed9f71c65b1526d49b924bac35f61b3172ccd45e0` / `9d9fa679119fc5b80c3587aeed3e4c148146d55ae997365318ffc8eef5760cd3`。car001/002 两路保持 PASS；car004 detector 由 PASS→FAIL，GT 路继续 FAIL，原因均有新增第 224 帧重捕及 224–229 帧过大框。第 224 帧 GT 可见约 `56×34`，detector 约 `65×42`，发布框约 `89×60`、IoU `0.355`；图 `/tmp/nt-car004-f224-crop.png`，源图 SHA-256 `02e40453f8c8c0fdddc146ba3defd0bc2b45d4de7303a98eeaecfd0629c98951`。其余路 verdict 与冻结基线相同。car009 第 115–120 帧正式 full 标记与用户确认的部分可见语义冲突仍未修订，不据此宣布正式门通过。
- **裁决/Handoff**：`3e7d5de` 不作为新基线，不合 main/Debug。下一步对比同类型原始观测尺寸，并在 NanoTrack 双模板确认的 detector 恢复中避免用旧框尺寸把下缘可见目标扩成大框；先聚焦 0305 和 car004，再扩 20 路，保留冻结 `8314980` 及实验 `8ac400b` 对照。

## 2026-11-18T12:52:55+08:00 外部正确重捕后的恢复试用期误判修正

- 上轮源码 `codex/ten-target-smoothness@d405d4e114e1a8898d662a2672d9ec20311e09ea` 精确归档主机/板端 `/tmp/nt-detector-recenter-d405d4e.tar` / `/home/neardi/Project/SOT/nt-detector-recenter-d405d4e.tar` SHA-256 均 `215f5a6c9a50a5f7d8da3d5d911c15f8400b02890f2c0a1233cc3507d8463a6e`；板端隔离根 `/home/neardi/Project/SOT/nt-detector-recenter-d405d4e`。实际 `bash /home/neardi/Project/SOT/nt-detector-recenter-setup.sh`，脚本 SHA-256 `61100c16f7028b0cb1173a192afa09a57b9a03a09e124d90cb8e4de14425bb14`，RK3588 原生 CMake/build/CTest **23/23 PASS**，二进制 `build/nanotrack_v3` SHA-256 `42f79b7a7a51963b10df737c44869fb2b60a57cf7d3cde4c172f3fa9f2a5fdff`。controller/headless 配置 SHA-256 `bb19d2e8e085dd57084653b6ca37bb981b10ea2a0d00bc05d166f8a508c6e26f` / `6ff6d3e96c8b57e96e5a78ee7fafac765851a266648f9a0a3db4caa8a0cd3c18`；cmake/build/CTest 日志 SHA-256 `b87c8a222259382aa0965377d6dceae0d921735a64d4669e3f06333f1be3f85b` / `ce797eb4f885b9ac18011cb7c8816c2610f2afa919b1ea2586231559be21361b` / `22b1a45335c317c762de316b8c402867ac9193e0857559e415584dc4c38d4500`。复用上一节同身份 NanoTrack/VisDrone9 RKNN 模型、manifest/labels 和 RKNN API/driver，未切换生产模型。
- 实际 `bash /home/neardi/Project/SOT/nt-detector-recenter-external.sh`，脚本 SHA-256 `076848846bc3e44b5ec8e94865d3fa46e112b087f5f047baaf4726ceaaa92dae`；两段外部视频 SHA-256 `f00b419afd23b679a42ed2b87c556eda9acf95a9f0a6d7ab1c2752d29215f96c` / `c71eda8bcd92239d065322cd78ce59c032e2bd4d344690d76b551824a4531622` 再核对相同，0305/0137 视频分别 141/233 帧，首帧真值框 `(1024,897,55,121)` / `(702,383,112,83)`，输出 CSV 142/234 行含表头且程序 exit 0。运行配置 SHA-256 `27159f108f37dafcf25a9990148a0031950a40cc75a3a8536dbe9cec19adab9f` / `2885df68b4be6938038831dce8593edd75492fd30da91b26627b197b9a1ab5c1`；板端原件 `runs/external/<sequence>/{frames.csv,session.json,run.log}`，板端/主机 `/tmp/nt-detector-recenter-external.tar.gz` SHA-256 `8e8c9b3247bb5637d5a2c9a24d3e1aeda13800e013f15530264b01f22991aa3c`，主机解包 `/tmp/nt-detector-recenter-external-results/`。
- 实际 `python3 /tmp/nt-score-detector-recenter-external.py > /tmp/nt-score-detector-recenter-external.json`，脚本/报告 SHA-256 `c9b2cf289c8d1e36b192000d714175602bc7506ea1deb934e39f4d8588153455` / `f0e815bbf99f385775de646277c54ab95b1956e535147cd38f9bae4dc19054cb`。0305 ID18 **第 111 帧正确重捕**：IoU `0.693`、中心误差 `7.2px`，旧版仍搜索 IoU `0.066`；112–115 帧预测框仍短暂贴近真值，但 `accepted=0`，116 帧回滚，之后未重捕。整段平均 IoU `0.5646→0.5925`，平均中心误差反而 `43.64→51.50px`，P95 误差步进 `6.17→14.53px`；不能以单次正确重捕宣称稳定。0137 ID22 轨迹/状态几乎不变，平均 IoU `0.7610→0.7615`，仍无后段重捕。
- 0305 第 112–115 帧 staged/Recent 双模板框和真值相近，响应 PSR 正常、`template_iou=0.92–0.95`，但恢复试用期 `assess_recent` 仍将新视角下相对已接受框的约 4% 高度收敛与中心移动视作 `partial_visibility`，触发 SUSPECTED 并回滚。当前分支从 `d405d4e` 继续做**试用期限定**修正：仅在 `recovery_probation_` 时不应用旧框高度启发式；普通跟踪的可见性判据、staged/Recent 一致性与可靠响应门不动。新测试在竖长→横向正确重捕后构造高度缩 4% 且双模板一致的后续帧，要求接受而不误判；既有普通跟踪可见性反例仍在。源码/测试 SHA-256 `5165acfdaf947fb7dd3a44e790f3b46e9206412a3e63cea2e3fd242285a011ed` / `792b490c06fc9e640b14516265e8cb4823588ae2e3c00cdd2e5bcc2694017530`。实际主机 `/tmp/nt-ten-target-build/test_long_term_controller` 通过，`cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure --parallel 4` **23/23 PASS**；日志 `/tmp/nt-recovery-probation-host-{build,ctest}.log` SHA-256 `472dfc8a714fd7368b4bf1802b32bf03cfc6d6a1c8002cf94a23dcc713e0330b` / `98ccef687345ca1c1aaf5157e0733bc1f1fbabe1d2baf6d704f4d49d2717b4f7`。本节与试用期修正同提交，提交号以 Git 为准。**Handoff**：先对精确新提交在板端重跑外部两段；若正确恢复持续且无新错捕，再跑 canonical 十目标 GT/DET 20 路。暂不合 main/Debug 或升级基线。

## 2026-11-18T12:43:56+08:00 唯一近场 detector 双模板恢复候选主机验证

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `b171cb910f3e6aed61c5754e42bd2e91d69287f5`；本节与源码/单测同提交，提交号以 Git 为准。用户确认 car009 第 115–120 帧**部分可见**，现行真值却标为 `full_occlusion`；请协调对话版本化修订状态并保留原标注，逐帧可见框仍未提供，本任务不自行写可计 IoU 框。car010 第 149–155 帧的视觉歧义见下节，同样不私自改正式真值。
- 用已保存的外部同场景以外 VisDrone val 两段板端 CSV 与原始标注核对：`uav0000305_00000_v` ID18 第 101–134 帧 detector 多次跟着真值车，110 帧 detector `(1362.41,472.72,118.31,51.33)` 对真值 `(1379,470,124,60)`，Recent/Safe 原始框与 detector IoU `0.664/0.675`，PSR `6.34/6.87`、APCE `19.19/20.55`；但已提交的旧竖长跟踪框约 `64.69×112`，原始框约 `125×58` 被旧框的 0.75 尺寸限制判为收缩，恢复质量仍是 `REJECTED`。三路无法进入已有 `focus_triad`，多个远处同类框引发 `UNRESOLVED_OVERFLOW`，110–141 帧 `IDENTITY_CONFLICT` 多次阻止真正候选。该外部目标本身在后段失败，适合筛查恢复规则；不把它当用户十目标验收。
- 候选仅在 full-object 恢复、当前探针 detector 与 Recent/Safe **原始 decoded 框唯一几何一致**、两个原始框在图像内、detector 中心距运动预测 `≤0.5` 预测框对角线时，临时按 detector 框重评两模板的 NanoTrack 响应与几何。仍须两路均达到原有 `RELIABLE` 阈值、候选表排除重叠的 rival/overflow，并由后续新鲜 NanoTrack 帧完成原有恢复确认；其他普通跟踪、遮挡尺寸限制与控制授权路径不变。修正后的源码/单测 SHA-256 `92a2c99f06f0db1c59c1b007f1b8b481fe664c3d441262b49f97626c217aab46` / `fe49d057b04e0bed8f82d37cf7c478ea4b4896aa2a41f0334e88f10a09c3f062`。
- 新控制器单测覆盖：竖长旧框到横向目标的 detector + Recent/Safe 唯一匹配、存在远处 detector overflow 时须两帧 NanoTrack 确认并发布 raw 新尺寸，以及远处相似目标不得借此旁路旧框收缩保护。实际 `cmake --build /tmp/nt-ten-target-build --target test_long_term_controller --parallel 4`、`/tmp/nt-ten-target-build/test_long_term_controller`，均通过；随后实际 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure --parallel 4`，主机 RKNN-off **23/23 PASS**。全量 build/CTest 日志 `/tmp/nt-detector-recenter-host-{build,ctest}.log` SHA-256 `71779a55d2335a75637c9ce592ccbd4df9dde50edf06bc4fa23831ec00593e2e` / `d71f132f17ec87fc94b7133d2c9b288044fc85b03b97225db1cc37b6305737d4`，`git diff --check` 通过。**Handoff**：只证明控制器合成条件与主机构建；须精确源码提交后 RK3588 原生构建、外部 0305/0137 配对和 canonical 十目标双初始化整段复测，检查早错捕、框尺寸及身份冲突。主机测试不升级 main/Debug 或正式基线。

## 2026-11-18T12:30:43+08:00 全局对齐光流与遮挡真值边界复核

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `e08846857c224341536950886b5291b17d173b38`；此轮只读取已保存的 `8ac400b` 板端二十路 CSV、canonical 视频和版本化真值，没有改运行时、模型或正式标注。实际 `/home/u20/single-object-tracking/.venvs/nanotrack/bin/python /tmp/nt-visible-extent-mog2-probe.py > /tmp/nt-visible-extent-mog2-probe.log`，脚本/日志 SHA-256 `1bdea3b091966a4350d752f1f5b681ec47b5888c93d9a2232651869789acc557` / `aa082f0e97ef6a53577f780f39c09d431e955e67e6a055403b7389ccd77cdbd3`：car004 第 128 帧和 car010 第 149 帧的 MOG2 前景像素在跟踪框的整个高度都有，不能据此裁出可见框。
- 为扣除相机移动，实际用 `/home/u20/single-object-tracking/.venvs/nanotrack/bin/python` 分别运行 `/tmp/nt-global-align-diff-probe.py`、`/tmp/nt-aligned-flow-probe.py`，脚本 SHA-256 `b37035abc963eff3d26bfb865335ee74379245b7fb6acf2e252bfd84be417be9` / `f796f43d22ce9d973a2cc397b1db5150afe51fb56edf7b2b23938269c4ce3668`。car004 第 124→128 帧半尺寸图像的全局 ECC 位移约 `(-5.55,+6.03)px`，说明未配准背景差存在明显相机运动；配准后框内大于 3px 的残余光流面积占比从第 125/126 帧 `0.951/0.977` 降为第 128/129 帧 `0.084/0.162`。car010 第 145/146 帧 `1.000/0.978` 到第 149/150 帧 `0.062/0.127`，也是下降；但仅两处正例不足以定阈值。
- 实际 `/home/u20/single-object-tracking/.venvs/nanotrack/bin/python /tmp/nt-aligned-flow-canonical-screen.py > /tmp/nt-aligned-flow-canonical.log`，脚本、CSV、日志 SHA-256 `e746bd2a60b587653a81d75c5c5badf689373ba4371fb0fc7e93b5988a8944c7` / `8298b702b2e407180a9cc1dac3fc7cb9925a5dd2492a2acbbef489984a08c5eb` / `551b43fe12ec3e0417d68e483a18bf82abda8d7f4738b0e277bf8f81b9eae2c9`，20 路共 5340 个 f4–270 路帧，没有 ECC 失败。实际 `/home/u20/single-object-tracking/.venvs/nanotrack/bin/python /tmp/nt-aligned-flow-audit.py > /tmp/nt-aligned-flow-audit.json`，脚本/摘要 SHA-256 `f75c4f54890eab85806f6d8a0445d33d60f576219743d28d5c8d677609b89664` / `4305ffb483607b49c3c37a2ed87272475728f7e59c48bb25b1827691a0162dfd`。用已接受帧前 8 帧占比中位数作因果基准，筛查 `flow_frac3<0.2` 且当前/基准 `<0.25` 时，正式 visible 接受帧触发 `190/4125`、full_occlusion 接受帧触发 `13/26`；正常可见的多目标在第 9–12、16–18、26–30 帧同时触发，car010 自身在 52–54、76–78 帧也连续触发。该光流下降无法单独作为可靠遮挡门或框裁剪依据；上述阈值仅事后筛查，未部署。
- 对照图 `/tmp/nt-car_010-occlusion-frames.png` SHA-256 `2e87526be9b257ce628f251f6ed302255679b377ec33ce78fa2179162672f9e2` 显示第 149–155 帧黄色目标仍有部分车身可辨、第 156 帧则在框内不可辨；`8ac400b` GT/DET 两路正式 `full_occlusion` 误接受恰好**只**是 `149–155`，第 156 帧已 `accepted=0`，但第 156–157 帧仍未切到 SEARCHING，也是正式遮挡门失败的一部分。这与用户“149–169 大部分被遮挡、149 判定/169 重捕为建议”一致，但与当前正式包将 149–155 记为 `full_occlusion` 存在语义冲突；没有逐帧可见框，不把它们私自改为可计 IoU 真值。car009 的正式误接受为 GT `116–120`、DET `115–120`；逐帧图 `/tmp/nt-car_009-occlusion-frames.png` SHA-256 `0f9bf317c157a6c242f9d01369371de8cf33723ca5b36a6f442eafe9e2e74c6f` 对部分可见与完全不可见存在歧义，已向用户请求核对。**裁决/Handoff**：不加未经独立验证的光流阈值，不改真值和 main/Debug；当前 `8ac400b` 仍为实验候选。下一步以确认后的可见性状态区分真实隐身误接受与标注语义冲突，再针对仍失败的可见框尺寸和重捕轨迹寻找跨场景证据。

## 2026-11-18T12:19:25+08:00 候选轨迹的可见性模型离线筛查

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `51a3a498691a29cb7250b1ba6d705da7f2920cc1`；算法源码仍是 `8ac400b9e4513ab6cbcef65d3db2b06a24a5b462`，未改运行时。用户确认当前没有另一段带逐帧可见框和遮挡状态的独立场景；继续只用现有十目标真值作此轮诊断，不据此重新调阈值。
- 实际运行 `OMP_NUM_THREADS=6 MKL_NUM_THREADS=6 /home/u20/single-object-tracking/.venvs/nanotrack/bin/python /tmp/nt-score-candidate-visibility.py`，脚本 SHA-256 `708aff7fc663ba7f6e62e6920f198565f1f6541d2c0a7a7479ceb3030e9df998`；冻结双帧 VisDrone train/val 可见性模型 `/tmp/nt-paired-visibility-screen/best.pt` SHA-256 `c6506b81f558190c10927d68158a426e8b0b3efaca9e739ad3ece46450cfe40d`，验证集固定阈值 `p_full<0.39032320827245715`，从候选二十路板端 CSV 的 `tracking_*` 框裁剪 canonical `1.mp4` 原视频前后相隔 16 帧的图块，得到 `/tmp/nt-candidate-visibility-scores.csv`，SHA-256 `b2499d1dfd290d39d82fc90ec781ebab0e9de6415236b7811b748bba1a22500a`，5100 路帧。真值只参与事后分组；car004 使用确认过的 `car_004_visible_unboxed_v2`。
- 实际运行 `/home/u20/single-object-tracking/.venvs/nanotrack/bin/python /tmp/nt-candidate-visibility-audit.py > /tmp/nt-candidate-visibility-audit.json`，脚本/摘要 SHA-256 `383de174ca29cce18394eaab18be3ba406c5ac54e379fd76f4f9eadab8bb1f6a` / `422d4907d78f3efa94f8d928b4b0accabd4745aa1c85088c5f4efa0bd0e4a641`。GT/DET 初始化真值标为 visible 且接受的帧分别 `2170/2194`，模型触发 `254/172`；只看运动输入冻结帧则分别 `153/85` 中触发 `3/7`。GT 触发全在 car004 第 127–129 帧；DET 触发在 car004 第 127–129 与 car001 第 162–165 帧。car004 第 127–129 帧模型有遮挡迹象，但它输出的是状态概率，不给可见部分坐标；现有接受框高约 50px、真值可见框高约 26–30px，单纯阻止接受并不能生成贴真值的框。car001 在真值仍可见的 162–165 帧也触发，直接接入会有额外状态变化风险。**裁决/Handoff**：不将此离线模型或专门针对 car004 的尺寸裁剪并入算法；保留 `8ac400b` 实验候选，仍不合 main/Debug、不升级正式基线。下一步需要可在不同画面验证的可见部分定位方法；VisDrone 两段成对检查见下节，不能充当完全遮挡真值验收。

## 2026-11-18T12:10:15+08:00 两段 VisDrone MOT 验证视频的板端成对检查

- 为检验 `8ac400b9e4513ab6cbcef65d3db2b06a24a5b462` 方向约束是否只适合 canonical 同一场景，选本机原始 `/home/u20/dataset/VisDrone2019-MOT/VisDrone2019-MOT-val` 两段**不同画面**汽车 `category=4`、第 1 帧干净标注的 track：`uav0000137_00458_v` ID 22，初始 `(702,383,112,83)`，原标注 233 帧、143 帧起出现部分遮挡标签，annotation SHA-256 `f055c20516df44a7092500e868830ac3a07f24e02f3241992ff211e3438887cc`；`uav0000305_00000_v` ID 18，初始 `(1024,897,55,121)`，原标注 141 帧、104 帧起出现部分遮挡标签，annotation SHA-256 `6414b5f6ab2e35cb20d04a226438b58f69bb89ece8561fb471a8fa1b687a5280`。此 val 数据未进入当前阈值选择，但 VisDrone 模型训练可能使用同系列 train split；这里只作为外部配对行为检查，不能证明普遍泛化。标注序号 1 对应运行 CSV `frame_id=0`，首帧两版对真值均 IoU 1.0；真值不进入板端算法。
- 主机实际用 `ffmpeg -framerate 30 -start_number 1 -i <sequence>/%07d.jpg -frames:v <233|141> -c:v libx264 -preset veryfast -crf 18 -pix_fmt yuv420p` 编码；第二段原高 1071 为奇数，加 `pad=ceil(iw/2)*2:ceil(ih/2)*2` 仅在底部加 1px。主机 `/tmp/nt-external-visdrone-validation/uav0000137_00458_v.mp4` 为 2688×1512、233 帧，SHA-256 `c71eda8bcd92239d065322cd78ce59c032e2bd4d344690d76b551824a4531622`；`uav0000305_00000_v.mp4` 为 1904×1072、141 帧，SHA-256 `f00b419afd23b679a42ed2b87c556eda9acf95a9f0a6d7ab1c2752d29215f96c`。板端同哈希、输入路径 `/home/neardi/Project/SOT/nt-external-visdrone-validation/`；编码日志均为空且 ffprobe 帧数核对通过。视频重编码不等同原 JPG 推理，但两版共享同一字节输入。
- 实际运行板端 `bash /home/neardi/Project/SOT/nt-external-visdrone-paired.sh`，脚本 SHA-256 `e830e1f601873801648f0aeec926e6a7377bde55f93137887d7228505250bda1`；每段同首帧 GT 框先后用冻结基线 `8314980` 可执行 SHA-256 `2b61916d1e8763aac09c1af08000f31564cc1d5ca3f4063f674ac47a45fefd6f` 与候选 `8ac400b` 可执行 SHA-256 `f11423a949afa22370348f03e45da2a749f68bdb8a6d4c83bde5ebdf6767cfb3`，同一 NanoTrack/VisDrone9 RKNN 模型，四路全部 exit 0、帧数 `233/141`。各运行配置只从同版本 `headless-on.conf` 改视频路径，四份配置 SHA-256 按 0137 基线/候选、0305 基线/候选为 `f619826b77617d94917bd4324689b7c79c86240f595d25392f8ef381b19066bf` / `fb31dba74afc80b59309d560bb1e16a22fdb8bfa75bca73de11b37ad59a17df2` / `c874de7968ef4627f1e2d7741bf25cb0cb75c6cfb1e40ab4718f05494ec15f5a` / `248b3babc99cb363ad9456549c5c6d65f39af215d9926ae84d36f3a473e04034`。板端原件分别在两程序根 `runs/external/<sequence>/{frames.csv,session.json,run.log}`；板端/主机 `/tmp/nt-external-visdrone-results.tar.gz` SHA-256 `8f3a0eeae56e71c4b2b128bc3dfab0e2fa4c81ab25f590cca4c65c44fce11ea7`，主机解包 `/tmp/nt-external-visdrone-validation/`。
- 实际 `python3 /tmp/nt-score-external-visdrone.py > /tmp/nt-score-external-visdrone.json`，脚本/逐帧报告 SHA-256 `13912802076512beacce26e54e64f74f0971fe776757aeacb93ee7a9a415504c` / `d3634b8f0eb5f799a36e28e880bfb1b59ba1e9d4c85d97310f45d196ced61e8d`。0137 ID22 全标注帧平均 IoU `0.7610→0.7615`、清晰未截断帧 `0.7529→0.7535`，两版清晰 SEARCHING 均 28 帧、部分遮挡标签帧均接受 24 帧，状态/接受无差异；只有 24 帧几何不同，单帧最大 IoU 差约 `0.031`。0305 ID18 全标注帧 `0.5646→0.5646`、清晰未截断 `0.6071→0.6071`，逐帧几何/状态相同。两版在各序列后段均已搜索且远离真值，未正确重捕；本候选没有修复这些跨场景失败。外部 MOT 的 `occlusion=1` 是部分遮挡标注，不能直接套 canonical 的“完全遮挡时绝不接受”门。**裁决/Handoff**：外部两段未见新增明显退化，但也无跨场景实质收益；结合上节 20 路，本候选仍为有局部收益且有 car004 部分可见框缺陷的实验分支版本，不合 main/Debug、不升级正式基线或生产 handoff。

## 2026-11-18T12:03:42+08:00 像素分辨率候选二十路板端闭环整段对照

- 继续验证已提交源码 `codex/ten-target-smoothness@8ac400b9e4513ab6cbcef65d3db2b06a24a5b462`，同一 RK3588 原生可执行文件 `/home/neardi/Project/SOT/nt-direction-resolution-8ac400b/build/nanotrack_v3` SHA-256 `f11423a949afa22370348f03e45da2a749f68bdb8a6d4c83bde5ebdf6767cfb3`、controller/headless 配置 SHA-256 `bb19d2e8e085dd57084653b6ca37bb981b10ea2a0d00bc05d166f8a508c6e26f` / `0510225eb16e914ed5985256835e30d293ab17b49600500d9f6a6ef9ead08ed7`，canonical 视频和 RKNN 模型/manifest/labels 哈希见上节及 `/tmp/nt-center-gain-board-inputs.sha256`。此前 car001/car004 四路原件保留；实际补跑 `bash /home/neardi/Project/SOT/nt-direction-resolution-rest-gt.sh` 和 `bash /home/neardi/Project/SOT/nt-direction-resolution-rest-det.sh`，脚本 SHA-256 `79a6d9d666156e83155d91e2cca6227cfbbba8a88cae65eece6470771e72d24f` / `1206d51194cfac4cd9b76252c806a1ab88e9e8ad278c640be2fe26b4bed2b69a`，其余 car002/003/005–010 双初始化各 271 帧、全部 exit 0，总计该候选 20 路。原始板端 `runs/resolution-{gt,det}/car_###/{frames.csv,session.json,run.log}`；板端/主机文本包 `/tmp/nt-direction-resolution-{gt10,det10}.tar.gz` SHA-256 `692d7f9d45ccb65242f506a490cea86b4100d99696bbea011048d6aca8d81299` / `bb8211c289f247c1fc1d7b77e084a7cf313c8765633169629006a34d72c0ae82`，主机解包 `/tmp/nt-direction-resolution-8ac400b/runs/`。没有把大视频写入 Git。
- 实际 `python3 /tmp/nt-score-direction-resolution-full.py`，脚本/正式二十路摘要 `/tmp/nt-direction-resolution-8ac400b-full-summary.json` SHA-256 `45f156e3895072871ff2c9dcb2e844bd6803c273a5d6ab5635c5dec05b26e555` / `24a9dcf3117b8e06c3555c8b8c3afef9bec2bbbe1652fc0d72716ef251b43312`；实际 `python3 /tmp/nt-whole-trajectory-resolution-full.py > /tmp/nt-whole-trajectory-resolution-full.json`，脚本/逐路全可见帧报告 SHA-256 `6422491a1ea921d69dce52775331182daf9881f8101781fb6abcdec4b3c82828` / `e1142f7322f8150c871806f99ebdff18bb1e43434d303fa60e84e955a876289d`。评分 car004 使用已确认 `car_004_visible_unboxed_v2`，真值只用于事后评分。十路 GT 初始化的状态/恢复/正式门禁全部与冻结 `8314980` 相同，绝大多数逐帧框相同，car009 全可见 mean IoU 仅 `-0.00006`。detector 初始化 car001 的原 PASS、188 帧重捕、7 帧可见 SEARCHING 均保持，全可见 mean IoU `0.7577→0.7577`；car004 全可见 mean IoU `0.7214→0.7555`、中心误差 `7.48→5.11px`、P95 误差步进 `4.92→3.21px`、可见 SEARCHING `57→37`，重捕保持 144；car008 全可见 mean IoU `0.8161→0.8248`、中心误差 `2.11→1.68px`。其余 detector 路变化约零；car009/010 遮挡期误接受分别仍为 `6/7` 帧，主要失败未修复。
- **裁决边界**：本候选改善 car004/008 整段轨迹且未出现上轮 car001 式跨目标退步；但 car004 detector 第 127–129 帧接受的高框对部分可见真值 IoU 低于正式阈值，故该路正式 `visible_boxes` PASS→FAIL，十路 detector 正式 PASS 数 `7→6`，并未达到“每目标都贴真值”。不以全段均值覆盖局部失败，不合 main/Debug、不宣布新正式基线或生产 handoff。下一步用非 canonical、不同画面的标注目标检验趋势门泛化；若能保住，再需独立可见范围证据处理局部框高，不继续用 car004 帧号定阈值。

## 2026-11-18T11:53:03+08:00 像素分辨率方向候选四路聚焦保住 car001 并改善 car004 整段

- 候选源码 `codex/ten-target-smoothness@8ac400b9e4513ab6cbcef65d3db2b06a24a5b462`，主机 `/tmp/nt-direction-resolution-8ac400b.tar` 与板端 `/home/neardi/Project/SOT/nt-direction-resolution-8ac400b.tar` SHA-256 均 `3e527a8f7de2dbee8432ce0155463c0838ae378695cf2c999f4c24757eead81c`；板端隔离根 `/home/neardi/Project/SOT/nt-direction-resolution-8ac400b`。实际 `bash /home/neardi/Project/SOT/nt-direction-resolution-setup.sh`，脚本 SHA-256 `afc655250786fc1ea632d5fa01b8e23bb973a46f0556b088d0b8cf78324b783a`，RK3588 原生 CMake/build/CTest **23/23 PASS**；程序 SHA-256 `f11423a949afa22370348f03e45da2a749f68bdb8a6d4c83bde5ebdf6767cfb3`，controller/headless 配置 SHA-256 `bb19d2e8e085dd57084653b6ca37bb981b10ea2a0d00bc05d166f8a508c6e26f` / `0510225eb16e914ed5985256835e30d293ab17b49600500d9f6a6ef9ead08ed7`；板端 cmake/build/CTest 日志 SHA-256 `f73d8dd2f6f151e74b3ae0de16d51d38093d49123958e89e2fa14a9bbe33ae4f` / `b5aed7089fe78e2ca843bc679dd3ffe2a5ec66637e4bcffd6bc54ae03f28b166` / `19391acaa4dd4c8e840a486038200095ed574665451c57ea57c3aaedfb39af5f`。视频、NanoTrack RKNN 三件套及 VisDrone9 detector/manifest/labels 均为已核验冻结 `8314980` 同一板端绝对路径与 SHA-256，见 `/tmp/nt-center-gain-board-inputs.sha256`；实际设备仍 RK3588，RKNN API 2.1.0 / driver 0.9.8。
- 实际 `bash /home/neardi/Project/SOT/nt-direction-resolution-focused-gt.sh` 和 `bash /home/neardi/Project/SOT/nt-direction-resolution-focused-det.sh`，脚本 SHA-256 `ba698a438a002ef842bc3e2bb1ec3cfa61dba2df17a4090e0aa0bbf0c8c98ba7` / `32b6aca2c0179c95923664adce3bdbee53f207583d949a66d30771472e009b81`，car001/car004 两种首帧框四路各 271 帧、全部 exit 0，原件板端 `runs/resolution-{gt,det}/car_{001,004}/{frames.csv,session.json,run.log}`；主机及板端 `/tmp/nt-direction-resolution-focused4.tar.gz` SHA-256 `c4b746dfd7e4ba74ee33e238c7235bd4ac0cd63360f1ab8d30716ad6108a0093`，主机解包 `/tmp/nt-direction-resolution-8ac400b/runs/`。实际 `python3 /tmp/nt-score-direction-resolution.py`，脚本/摘要 `/tmp/nt-direction-resolution-8ac400b-focused-summary.json` SHA-256 `28450096f4d4b837bad86ed6a137c5a421ee6f6b42e3a86943d1446cb80151e7` / `105f82841b40e06f1cf08b7960e4e646072f51e871b2a5b4d292aef02553536f`；实际 `python3 /tmp/nt-whole-trajectory-resolution.py > /tmp/nt-whole-trajectory-resolution.json`，脚本/报告 SHA-256 `34702ae0e74a17a6803539bc01eeb4669d15f77b236cb99797fe321563fe407c` / `a335990dafae53d5abc420e9f5700bc0a3d4eaee5c8b26bf6eb5a3e9ff33d3c0`。car004 用用户确认的 `car_004_visible_unboxed_v2`。
- 对冻结 `8314980`：car001 detector 初始化 **PASS 保持**，重捕 `188→188`、可见 SEARCHING `7→7`，全可见 mean IoU `0.7577→0.7577`、中心误差 `4.57→4.57px`；car004 detector 初始化全可见 mean IoU `0.7214→0.7555`、中心误差 `7.48→5.11px`、可见 SEARCHING `57→37`，仍 144 帧正确重捕。两路 GT 基本不变。car004 第 127–129 帧仍接受约 50px 高整车框，而部分可见真值高约 26–30px，故正式 `visible_boxes` 从 PASS→FAIL；当前结果是整段改进但仍有局部尺寸缺陷。**Handoff**：不据四路宣称十目标泛化或合 main/Debug；下一步同程序补其余八目标双初始化 16 路，若任一路出现 car001 式退化则否决，再考虑局部缺陷处理。

## 2026-11-18T11:48:38+08:00 冻结速度方向的像素分辨率约束主机候选

- 分支 `codex/ten-target-smoothness`、工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `85fd3010642a91f3dd8b895debb9d7c0e4c99168`；本节与源码同提交，提交号以 Git 为准。恢复已板端完整验证过但因 car001 大退步否决的 `ae9d5ec` 组合源码，在冻结趋势方向门增加唯一通用条件：若旧可信速度在两次 detector 确认间的预期位移 `<1px`，视为低于图像分辨率，允许新实测位移更新；旧趋势足够长时仍拒绝反向单次替换。PSR 视觉搜索约束及其他运行时逻辑沿用 `ae9d5ec`，不使用目标 ID、帧号或真值作运行时输入。源码/测试 SHA-256 `389861d6556bc5862491517be4330273a03654afd782333f094875b6ac4b3163` / `c28a08a9eb70de329a3a4f6dfc1988c567672cbe8620e60c3446a6079673f2d0`。
- 实际 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure --parallel 4`，主机 RKNN-off **23/23 PASS**；日志 `/tmp/nt-direction-resolution-host-{build,ctest}.log` SHA-256 `153fd5bb2f9a3dba2bc5e73793afcc9b807a64e337d3e5448ce9ad70a1c36371` / `bbf88e1e5c5449a5056f32ac6e68111a7f784b17e72307d7ac2ddd7b3268ecf7`，`git diff --check` 通过。主机 CTest 不是板端轨迹效果证据。**Handoff**：先隔离 RK3588 原生构建并成对回放 car001/car004 detector 及 GT 初始化；若 car001 保住且 car004 整段收益保留，再补完整 20 路，逐路同时检查全可见 IoU、中心误差、误差步进、遮挡误接受及正式门禁。此前 `ae9d5ec` 不获晋级，main/Debug 和生产 handoff 仍不变。

## 2026-11-18T11:47:03+08:00 按整段轨迹目标补齐组合候选十目标双初始化

- 分支 `codex/ten-target-smoothness`、干净起点 `fed95499be819daababdbc80b6987271c8c63d95`。用户优先要求十目标逐帧真值贴合、可见期稳定、遮挡后快速正确恢复，149/169 只是参考。复查上轮组合候选 `ae9d5ec608d71c3fba1de38cd630067b5fab0108` 聚焦记录后，不再用单一 PASS/FAIL 否决其 car004 detector 路：相对冻结 `8314980`，该路全部有可观测真值的可见帧平均 IoU `0.7214→0.7555`、中心误差 `7.48→5.11px`、误差步进 P95 `4.92→3.21px`、可见 SEARCHING `57→37`；但 127–129 帧接受的整车高框未缩到部分可见真值，正式 `visible_boxes` 门 FAIL。这个局部收益需要扩到全部目标核查，不以单路代替总体结论。
- 实际复用板端已原生编译并 CTest 23/23 PASS 的隔离程序 `/home/neardi/Project/SOT/nt-motion-direction-ae9d5ec/build/nanotrack_v3` SHA-256 `bb703918f7a81f02f64a63ad0601a8271fad671059d99d7280267c81b91b987a`；controller/headless 配置 SHA-256 `bb19d2e8e085dd57084653b6ca37bb981b10ea2a0d00bc05d166f8a508c6e26f` / `772ff9e5c254b37ea41e1d1729b4c81494a423a5af77472ad34e594c5ac96496`，视频和 NanoTrack/VisDrone9 模型哈希同上一轮 `/tmp/nt-center-gain-board-inputs.sha256`。聚焦六路已留原件；实际补跑 `bash /home/neardi/Project/SOT/nt-motion-direction-rest-gt.sh` 与 `bash /home/neardi/Project/SOT/nt-motion-direction-rest-det.sh`，脚本 SHA-256 `fc232d7998621864f5d86dd0cdb64f9c655777d4533c4c1e8ac6e89d0f10be46` / `72568c1bd03ff571a0b2fcc8cb931d17a04e2ab6271f897300bcde65087d8c56`，其余七目标每路 271 帧、全部 exit 0，遂得到完整 20 路。板端 CSV/session/log 原件 `runs/motion-{gt,det}/car_###/`；板端/主机文本归档 `/tmp/nt-motion-direction-{gt10,det10}.tar.gz` SHA-256 `b0ec1ed02042fdb8f9079e79cffe55775796a054dabc791651982f8e096ff27d` / `27b4f6e0e51a727b4b7978a2edc42143f46e8e1930e1a35a87479c3461769995`，主机解包 `/tmp/nt-motion-direction-ae9d5ec/runs/`。大视频未生成或回传。
- 实际 `python3 /tmp/nt-score-motion-direction-full.py`，脚本/正式 20 路摘要 `/tmp/nt-motion-direction-ae9d5ec-full-summary.json` SHA-256 `935b6028ecb465a68f1a0caff2025d1b25e73a0056a219c84f0b14bd1d7b81b8` / `2aace360c4c4bbd7327db5448c4456c4d3be6b1ff6d3ec5417ac0483a061c8e9`；car004 使用用户确认的 `car_004_visible_unboxed_v2`。实际 `python3 /tmp/nt-whole-trajectory-full.py > /tmp/nt-whole-trajectory-full.json`，脚本/报告 SHA-256 `7ad5025cb84072f24a15bbc9d7673a9d3ece38d15de10a9a6cc9c3d677ebd369` / `8dbcb8ba3e94765c4f5c0ace6459d987ca3d9ca969c7f729a83243e550874e69`。该报告把每个可见且可计 IoU 的帧纳入均值，含 SEARCHING 预测框；也记录全部可见帧中心误差、误差步进、遮挡期误接受，而非只统计接受帧。GT 十路的状态、重捕和门禁基本不变；detector car004 如上改善、detector car008 全可见 mean IoU `0.8161→0.8248`；但 **detector car001 严重回退**：可见 SEARCHING `7→80`、重捕 `188→262`、全可见 mean IoU `0.7577→0.5118`、中心误差 `4.57→31.42px`，正式 PASS→FAIL。car009/010 遮挡期误接受没有改善。故原组合候选仍否决，不能以 car004 局部收益覆盖 car001 退化，当前分支算法保持 `fed9549` 字节基线。
- 首次因果分歧在 car001 detector 路第 161 帧：基线一次 `DETECTOR_CORROBORATED_TREND` 把冻结速度由 `(-1.30,-1.51)` 改成随后 `(6.02,29.48)px/s`，组合候选的方向约束拒绝更新；第 168 帧虽 Recent PSR 相对可信基线下降约 `25.4%`，组合候选同帧视觉探针 miss，至第 170 帧才 SEARCHING，后续搜索轨迹远离真值。car004 detector 路第 125 帧旧可信运动约左下数十 px/s，反向新位移若直接写入则下一帧速度 `(35.66,-17.05)px/s`，拒绝该反向替换可维持 144 帧重捕。**待验证的新通用解释**：近静止时旧速度方向低于像素分辨率，不应被点积门视为可靠方向；用旧速度乘两次确认间隔的预计位移是否至少 1px 作置信条件，比按目标/帧号加例外更合理。此节只记录假设，未改源码或宣称候选通过。

## 2026-11-18T11:33:58+08:00 单独冻结方向约束六路无实质收益，撤回实验源码

- 实验源码 `codex/ten-target-smoothness@f98871d36f62c788f88554b103769c7b45c78547`，仅含冻结期间两次 detector 显示佐证的 NanoTrack 位移不能与可信速度反向的约束，PSR 守门已撤销。`git archive` 主机 `/tmp/nt-direction-only-f98871d.tar` 与 RK3588 `/home/neardi/Project/SOT/nt-direction-only-f98871d.tar` SHA-256 均 `780fed29f22a4ba1c7a574a3339f78df99d08025232ff343ee09e2281b53f98f`；板端隔离根 `/home/neardi/Project/SOT/nt-direction-only-f98871d`。实际 `bash /home/neardi/Project/SOT/nt-direction-only-setup.sh`，脚本 SHA-256 `0a8b1f9b3212dd0811b185879c7f42349eea1d553325e316d1b8c8ac6a899002`，原生 CMake/build/CTest **23/23 PASS**；程序 SHA-256 `0be270d4722a38793f6bb3b7c3281ac7bb6ea9b6b275fd9f33727d37bb9c9df6`，controller/headless 配置 SHA-256 `bb19d2e8e085dd57084653b6ca37bb981b10ea2a0d00bc05d166f8a508c6e26f` / `b6514da6bc2b4a1e0349ff513eafdd781020af8c9494ae35829ad35fe2927596`，板端 cmake/build/CTest 日志 SHA-256 `3d52c8d478d0bafe16eb02e0167218b6b3eec312441e32` / `7100b553a3dac84500b6a4da6ac13dd90e3993507459e920a61d002d5280b1ea` / `639fe6710f2b35839c87bcb7ada9f17adebe728cfe7f2de0874027d727472326`。视频、模型及标签哈希按冻结基线板端绝对路径、`/tmp/nt-center-gain-board-inputs.sha256` 核对，RKNN API 2.1.0 / driver 0.9.8 同先前环境；此次不主张新平台验收。
- 实际 `bash /home/neardi/Project/SOT/nt-direction-only-focused-gt.sh` 和 `bash /home/neardi/Project/SOT/nt-direction-only-focused-det.sh`，脚本 SHA-256 `5e8fa511291564d86df2c7f47cef9a324e9a8334b0d9c6f5f13c19dbf1288e41` / `dde372a5ce88a8708a485a5334687ca57b30b08f9ca6ef08812a2365355ecceb`，car004/009/010 GT 与 detector 首帧框各 271 帧，全部 exit 0；板端原件 `runs/direction-{gt,det}/car_###/{frames.csv,session.json,run.log}`，主机文本包 `/tmp/nt-direction-only-{gt3,det3}.tar.gz` SHA-256 `9e612a95bfad33421613d11ad8e829a63a9acf3a9e20ea28964871eae6f73058` / `8d82e8b0e63476a21897d741519cbbe6b44233764dd5d537f25e2270aa734bed`。实际 `python3 /tmp/nt-score-direction-only.py`，脚本/摘要 `/tmp/nt-direction-only-f98871d-focused-summary.json` SHA-256 `69d1cbb9220fbba34e4b63f3f4f1ae3134fc81ed97b695cf6d779a194c2d829d` / `f2045bfe91f48090cab55d49c34c5fb699ea3878529c19e8ad72b50fb1dd1548`；car004 使用用户确认的 `car_004_visible_unboxed_v2`。六路正式 PASS/FAIL、可见 SEARCHING 数、重捕事件、失败门均不变；car004 detector 可见接受框 mean IoU `0.8413→0.8415`，car009 双路 `0.8100→0.8099` / `0.8202→0.8203`，car010 双路无实质变化。方向约束未修复已知 car004 GT、car009、car010 失败，对十目标轨迹贴真值没有足够收益，因此不扩完整 20 路，不合 main/Debug。
- 已将 `rk3566/nanotrack_v3/src/long_term_runtime.cpp`、`tests/test_long_term_runtime.cpp` 恢复为本轮干净起点 `244ace7941deffa52eb26410541523f4a1ff6515` 的字节内容，`git diff 244ace7 -- rk3566/nanotrack_v3` 为空；主机实际 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure --parallel 4` **23/23 PASS**，日志 `/tmp/nt-final-baseline-host-{build,ctest}.log` SHA-256 `c02723aa8c1feb112efe0d5665d50c921ef8b1b46654a26b3349b8cbb7df0b30` / `e05dcecf5d93d4f3a5ad616c34616744e4e5e4999bcc237db8d4dd9bc06ade67`；`git diff --check` 通过。**Handoff**：所有本轮规则候选均否决，冻结 `8314980` 十目标双初始化板测仍是效果基线；实验分支源码恢复该算法，未改正式真值、模型、main/Debug 或生产 handoff。下一步若要继续改善部分遮挡期真值贴合，需要独立场景的实例可见范围/身份监督或可靠感知证据，不以此单视频追加阈值。

## 2026-11-18T11:27:55+08:00 组合候选因部分遮挡框退化否决；单独验证冻结方向

- 起点 `codex/ten-target-smoothness@ae9d5ec608d71c3fba1de38cd630067b5fab0108`，该提交同时含 PSR 守门与冻结速度方向约束。源码包主机 `/tmp/nt-motion-direction-ae9d5ec.tar`、板端 `/home/neardi/Project/SOT/nt-motion-direction-ae9d5ec.tar` SHA-256 均 `c365ac8f34c7b812413b49846aaae2d18b8412ad74052017972840ce13bbb2e8`；板端隔离根 `/home/neardi/Project/SOT/nt-motion-direction-ae9d5ec`。实际 `bash /home/neardi/Project/SOT/nt-motion-direction-setup.sh`，脚本 SHA-256 `c400c484edada1a93d19c9778d4e5213ed77a2a4d1378e0eaaff099356c32e41`，原生 CMake/build/CTest **23/23 PASS**；程序 SHA-256 `bb703918f7a81f02f64a63ad0601a8271fad671059d99d7280267c81b91b987a`，controller/headless 配置 SHA-256 `bb19d2e8e085dd57084653b6ca37bb981b10ea2a0d00bc05d166f8a508c6e26f` / `772ff9e5c254b37ea41e1d1729b4c81494a423a5af77472ad34e594c5ac96496`，板端 cmake/build/CTest 日志 SHA-256 `96c396a5f6d99d1ad13dccf214bd09b0d1fff1e1d7ebe137dfb114a823aac9c5` / `2f925bfacaadc788f452c50ba695c865f65ae6a732d634875f3d3377dd3630ae` / `e6e3ed4e7069539f9377fa763e1e442ec503bbeecfdf840a69e538e425e63ead`。板端 canonical 视频/模型/manifest/labels 身份同冻结 `8314980`，见 `/tmp/nt-center-gain-board-inputs.sha256`。
- 实际运行 `bash /home/neardi/Project/SOT/nt-motion-direction-focused-gt.sh` 和 `bash /home/neardi/Project/SOT/nt-motion-direction-focused-det.sh`，脚本 SHA-256 `9c2ce4161a5f6387f9950843a84cff93bc3bb784a25d66b58e02a8c3852d8958` / `dccbc21af04c07ffed614532e1a1979c58faa420e6a519ca9044869120bc5504`，六路各 271 帧、全部 exit 0；板端原件 `runs/motion-{gt,det}/car_###/{frames.csv,session.json,run.log}`，主机 `/tmp/nt-motion-direction-{gt3,det3}.tar.gz` SHA-256 `4e332186c52f808b2d914538ab89d1f092e84663ce94df8cca875a19bd1ca208` / `53e9a396acdd8358ca2b81ea8a381f7a5b39f658bbd83f804e4e586247cf84cf`。实际 `python3 /tmp/nt-score-motion-direction.py`，脚本/六路摘要 `/tmp/nt-motion-direction-ae9d5ec-focused-summary.json` SHA-256 `bc1ac277404aae00f5a1db6228a7382737466a42f93b428426c6cd5f30932534` / `b44285c8d48d50f47ec88ce9fb0f876a70f1971c96d29430371355992e14e4b1`。car004 GT、car010 双路、car002 双路与冻结基线实质相同；car004 detector 初始化恢复到原 144 帧，SEARCHING 的正式可见帧 `57→37`，但第 127–129 帧部分可见真值要求较小框，组合候选接受了过大的整车框，`visible_boxes` 门从 PASS→FAIL，可见接受框 mean IoU `0.8413→0.8302`，中心误差 `2.24→2.33px`。故此组合候选**否决**，不扩 20 路、不合 main/Debug；不能用可见覆盖增加掩盖三帧框不贴真值。
- 为辨别冻结速度方向约束单独作用，已恢复 `244ace7` 内容的 PSR 守门逻辑及相应单测，只保留方向约束。新运行时 SHA-256 `d316af80a6e02b927d4426caf36a0f29fdd1a4d8948b97f982409b3114635580`；实际 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure --parallel 4`，主机 RKNN-off **23/23 PASS**；日志 `/tmp/nt-direction-only-host-{build,ctest}.log` SHA-256 `58b085d5ba7bfb155df3db7a78c8cfc010ed04c4068971bb53b85b40d16b07cb` / `e667c85b997ec01a61e79086d18106c3e46b05c3ecda7941bd124dcc50895de8`。本节方向约束单独候选提交号以 Git 为准。**Handoff**：先按相同输入原生构建及聚焦 car004/009/010 双初始化；若无稳定收益或新退化则撤销，不升级十目标基线与生产 handoff。

## 2026-11-18T11:20:34+08:00 PSR 佐证候选否决；冻结运动方向约束主机候选

- 起点 `codex/ten-target-smoothness@a4a60a356b7e6e5619cf13ad951562ccacb44a74`，提交源码包主机 `/tmp/nt-psr-guard-a4a60a3.tar` 和板端 `/home/neardi/Project/SOT/nt-psr-guard-a4a60a3.tar` SHA-256 均 `8a809334e517f23d21c5d3ab61af9b3e70592833a4c60a415b8c2758d0446ab3`。板端隔离根 `/home/neardi/Project/SOT/nt-psr-guard-a4a60a3`，实际 `bash /home/neardi/Project/SOT/nt-psr-guard-setup.sh`，脚本 SHA-256 `8ccd40e904abed1523c03f49b150dd7bd87f1cdfb7780ee132ae3212b3c6a89e`；RK3588 原生 CMake/build/CTest **23/23 PASS**，程序 SHA-256 `903b13b19decea5b47487a95bdc57f51df9c41383e4cdc464dada7837dcba187`，controller/headless 配置 SHA-256 `bb19d2e8e085dd57084653b6ca37bb981b10ea2a0d00bc05d166f8a508c6e26f` / `31f7b4463551103f81de3e885f8142371955b835dbbb754d02562c54dff42de2`；板端 `cmake.log`/`build.log`/`ctest.log` SHA-256 `9177f23d4430f032b8bbc36d9421d8d8d1269c5fd182dadc02a1de92c7034997` / `6d301ac147b4dd2697505b7c2c28f37f96cc8341dfacf5634284c33b0b4ac1dd` / `0786c28790fb351360cd0989dd2b31ddc65811f9e312698759785bb9237fb5f9`。视频、模型、manifest、labels 均为冻结 `8314980` 同一板端绝对路径与哈希，见上一节和 `/tmp/nt-center-gain-board-inputs.sha256`。
- 实际运行 `bash /home/neardi/Project/SOT/nt-psr-guard-focused-gt.sh` 与 `bash /home/neardi/Project/SOT/nt-psr-guard-focused-det.sh`，脚本 SHA-256 `47dabec912a89566ae551a1b6a1656e6b49439812e36f3506c065c4ce6790254` / `c76221aa297b6eaaa34283306e65a5efff9547cdfd2c24b096efa0fcfebc3e53`；car004、car010、car002 两种首帧框各 271 帧、全部 exit 0。原件板端 `runs/psr-{gt,det}/car_###/{frames.csv,session.json,run.log}`；主机 `/tmp/nt-psr-guard-{gt3,det3}.tar.gz` SHA-256 `69a9f8792b21e31262fce01895bbd265f9404c827d2fa01712dc89912ae8a954` / `636efa96a7b2a17344e12a1688307c5ca07a1c11b6100c13d3923af86d3c1aae`。实际 `python3 /tmp/nt-score-psr-guard.py`，脚本/摘要 `/tmp/nt-psr-guard-a4a60a3-focused-summary.json` SHA-256 `80edb41dc536c3148792f245f6239d08d90529401b65c388e99e1d64da818446` / `d8adafdc630d09af3c04c1cf5de03ddf53206a5e2f8d65e747f506571b1a4630`。GT car004/010/002 三路与冻结结果相同；detector car010/002 基本相同；**detector car004 PASS→FAIL**，可见接受框 mean IoU `0.8413→0.6233`、可见 SEARCHING `57→63`、重捕 `144→200`。所以 PSR 增强不能单独替代第 111 帧早搜索，本候选不扩完整 20 路、不合 main/Debug。
- 逐帧看清原因：car004 detector 初始框下，第 111 帧仍贴近真值，但未早搜索时，第 125 帧冻结运动输入后，一次 `DETECTOR_CORROBORATED_TREND` 把冻结速度从约 `(-1.16,+15.67)` 更新成下一帧 `(+35.66,-17.05)px/s`。这条所谓 detector 佐证只以“显示框来源为双确认检测”为触发，速度实际由两次 **NanoTrack 框中心差**得出；第 126–131 帧持续冻结并沿反向速度搜索，第 137 帧目标重现时预测位置已往右上漂。此因果归因基于同一 run CSV，仍需新闭环验证。原始 `PSR` 守门源码尚在实验分支，本节进一步增加通用方向一致性：仅在冻结期间两次受 detector 显示佐证的 NanoTrack 位移与上次可信速度点积非负时，允许用它更新冻结运动趋势。只拒绝单次反向速度替换，不让 detector 框直接授权控制。新源码 SHA-256 `54339750de9863171d0c0fb61a363a1b1d35d19d1424e7667fd21dee578a2c96`。
- 实际 `cmake --build /tmp/nt-ten-target-build --parallel 4` 与 `ctest --test-dir /tmp/nt-ten-target-build --output-on-failure --parallel 4`，主机 RKNN-off **23/23 PASS**，日志 `/tmp/nt-motion-direction-host-{build,ctest}.log` SHA-256 `191ebe4c179d0829c7016fda26c19961f89f42f6e569356928c165d09e76c330` / `666a7a0ae60ae46155b4aead9705bcca824368d2335aa7477bf5fb4d98bbda89`；`git diff --check` 通过。**Handoff**：方向一致性候选未上板。先 RK3588 独立构建六路；若 car004 detector 初始化仍退步则撤回 PSR 与方向候选，不据主机单测宣称效果提升。

## 2026-11-18T11:13:06+08:00 下缘遮挡规则消融否决；PSR 佐证候选主机通过

- 分支 `codex/ten-target-smoothness`、工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `244ace7941deffa52eb26410541523f4a1ff6515`；本节候选提交号以 Git 提交为准。用户明确 car010 第 149 帧判遮挡及第 169 帧重捕只是参考，不作为硬门；优先优化十目标的逐帧真值贴合、可见时稳定和遮挡后正确重捕。冻结 `8314980` 的 car004 detector 初始化第 111 帧仍有 Recent PSR `8.218363`、相对可信基线 `+0.018322`，却被 `VISUAL_OCCLUSION_SEARCH` 强制搜索；这提示下缘长直线加亮度规则有误报，但单帧不能证明应全局删除。
- 只改板端实验配置、不改冻结程序，实际运行 RK3588 `/home/neardi/Project/SOT/nt-search-output-8314980/build/nanotrack_v3` SHA-256 `2b61916d1e8763aac09c1af08000f31564cc1d5ca3f4063f674ac47a45fefd6f`；将 controller `lower_band_visibility_veto=true` 改 `false`，`runs/no-lower-band.conf` SHA-256 `d615423aa76084419068744bd06d9565b4c2e19a672edb86338cea5cd00c807e`，头文件 `runs/headless-no-lower.conf` SHA-256 `aff80d7e9ebe461a3a9b97ddf2fdb4ae35bd93b6b2dde1c03928443b84ed01c4`。实际 `bash /tmp/nt-no-lower-focused.sh`，脚本 SHA-256 `52984e614fbe04e24fe404ad2423448448291eb22aa874b2dc80aed66629e2c8`，以原十目标基线相同 GT/detector 首帧框、视频、NanoTrack/VisDrone9 模型身份聚焦跑 car004、car010、car002 共六路，每路 271 帧，全部 exit 0。原始 CSV/session/log 留板端 `runs/no-lower-{gt,det}/car_###/`，主机包 `/tmp/nt-no-lower-{gt3,det3}.tar.gz` SHA-256 `85213738f774cf7b8684038ea170ab6f917c38a961887cb200dddf61f3293c1e` / `dd73422558e526c2e9d005e88ff8420babafdc9ce5023629b1762ed5f0eb6151`。板端视频/模型哈希与上节 `8314980` 十目标对照相同，见 `/tmp/nt-center-gain-board-inputs.sha256`；本次没有重新传模型。
- 实际 `python3 /tmp/nt-score-no-lower-focused.py`，脚本/报告 `/tmp/nt-no-lower-focused-summary.json` SHA-256 `f21ea186e85c99d0b3a716320da3a949332b7da276bcb9e4d751f905b2b71f34` / `6c9de72524e9c43b9ae4920738d8bb39dde82f4216d31a39646b07a8c7cdce3f`；正式全段评分对照版本化 car004 真值：car004 detector 初始化 **PASS→FAIL**，可见接受框平均 IoU `0.8413→0.6232`、中心误差 `2.24→64.83px`、恢复 `144→200`；GT 初始化仍 FAIL、可见 SEARCHING `39→45`、恢复 `146→152`。car010 两路状态/恢复基本不变，car002 两路仍 PASS 但恢复分别 `228→229/231`。故否决全局关闭下缘规则，不扩跑 20 路，不合 main/Debug。
- 新的**未上板候选**保留下缘规则，但仅在 Recent PSR 不高于已有可信响应基线时，才允许这项场景线索单独强制搜索；无基线时沿用旧路径。`rk3566/nanotrack_v3/src/long_term_runtime.cpp` / `tests/test_long_term_runtime.cpp` SHA-256 `50949fbe6fc7406a076539c437f4f9a3d31ed9f6a1b72d9fc4ecf664a36031ec` / `c28a08a9eb70de329a3a4f6dfc1988c567672cbe8620e60c3446a6079673f2d0`。新增单测明确验证基线相等会阻止观察、比基线强则不因场景线索强制搜索；实际 `cmake --build /tmp/nt-ten-target-build --parallel 4` 和 `ctest --test-dir /tmp/nt-ten-target-build --output-on-failure --parallel 4`，RKNN-off **23/23 PASS**，日志 `/tmp/nt-psr-guard-host-{build,ctest}.log` SHA-256 `429c637a9f3bb83b814cea98b04cee468ef275eaf44f44211646024dd0432faa` / `c4d8934f44c60a14fc1dc6ec2ccf0c317576b74a10137154191703eefc5bb5f4`；`git diff --check` 通过。**Handoff**：先板端独立原生构建和六路闭环；仅若不损既有目标并有收益，再扩完整十目标双初始化。生产 handoff 仍未批准。

## 2026-11-18T10:55:56+08:00 detector 发布中心增益十目标板测否决并撤回

- 实验源码 `codex/ten-target-smoothness@f9f37f7cbc33c7b82c73963747e047c0fd60937d`。`git archive` 源码 `/tmp/nt-center-gain-f9f37f7.tar` 与 RK3588 `/home/neardi/Project/SOT/nt-center-gain-f9f37f7.tar` SHA-256 均 `56f238f952f52f99f5f33059c39ec95b75af764651a9c91ef285773f6d886d9c`；隔离板端根 `/home/neardi/Project/SOT/nt-center-gain-f9f37f7`。部署脚本 `/tmp/nt-center-gain-setup.sh` SHA-256 `5cf68aa8c65f48ea6e16f37d0f8b48a91800554fc0f40801fd66baba0159217a`；实际板端原生 CMake、build、CTest **23/23 PASS**，程序 `build/nanotrack_v3` SHA-256 `9395ebf52ad0cebc1a095448488676cbefe37b668a993a71002975dc163780ac`，controller/运行配置 SHA-256 `bb19d2e8e085dd57084653b6ca37bb981b10ea2a0d00bc05d166f8a508c6e26f` / `394880c64088a4f72e6f0dcb3489f08b9e6e58148484802dca66983098ca4450`；板端 cmake/build/CTest 日志 SHA-256 `42cbd99c56b43fce6db4fb85587a07155bcceac114344fb7f9d472dc53673039` / `4a27d16a5b7bc50ed66a71a58e2ecc5fd89dd040967775583379a79114d9dab3` / `4b2e3ad14a91b28ad92629277046e0b828c8732d2439887e960c1ff188b03ebd`。`/tmp/nt-center-gain-board-setup.log` SHA-256 `fadc175c09e9278e2c52fb1ba66ba8b23cff95fb864b8dbf8e0e7fdade645981`。
- 输入仍是板端 `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；同根 `models/manifest.json` 与 template/search/head RKNN SHA-256 分别 `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261` / `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55` / `d3cb0e3879df6a1272adb59bf501324f1f8f55b58309d3c4da4a731b600973ee` / `4da379f54093d79f02775e90bfea33ebee0837c6fc73a324499279b4c57807c5`；VisDrone9 detector RKNN/manifest/labels SHA-256 `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c` / `ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9` / `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`。完整相对路径清单 `/tmp/nt-center-gain-board-inputs.sha256` SHA-256 `8492cada2fd610b78bfc901fee3e390a35385acaa572a95299e4bb1421f65e2d`；板端日志 RKNN API `2.1.0`、driver `0.9.8`。
- 实际执行板端 `bash /home/neardi/Project/SOT/nt-center-gain-full-gt.sh`（SHA-256 `1b0048305003868971e8b80b93433b0a7957518c1cc2096589e474d4bcaf6bfe`），十个目标均以与冻结 `8314980` 完全相同的首帧正式真值框、同配置、同 271 帧运行，全部 exit 0；原始 `frames.csv`、`session.json`、`run.log` 保留板端 `runs/center-gt-full/car_###/`。板端及主机 `/tmp/nt-center-gain-f9f37f7-gt.tar.gz` SHA-256 `ab5fba3500653477e9b8d89e3287b8f6afd24526791d07807dca800163ba2cd3`，主机解包 `/tmp/nt-center-gain-f9f37f7/runs/center-gt-full/`。实际 `python3 /tmp/nt-score-center-gain-gt.py > /tmp/nt-score-center-gain-gt.log 2>&1`，按 `car004_visible_unboxed_v2` 及其余正式真值评分；脚本/摘要 `/tmp/nt-center-gain-f9f37f7-gt-summary.json` SHA-256 `ca78cc3c760f5d3e35bb534819b3d6f608b4db5c08b8f88b0c66b6d3159e565a` / `5bea5407eefaebd6d24b9af1126ae1463ac4ba5162aa5783c85899e23005872b`。十路 PASS/FAIL、恢复帧及正式门禁均未变；可见接受帧平均 IoU **9/10 下降**：001 `0.7767→0.7677`、005 `0.8389→0.8264`，其余下降约 `0.0002–0.0069`，008 仅 `0.8212→0.8213`。九路中的八路平均中心误差增加，唯一减少的 009 仅约 `0.006px`。001 原始 Recent 坐标/PSR 与状态逐帧相同，明确是发布层中心加权造成的退化。
- 为确认第二种初始化，实际启动 `bash /home/neardi/Project/SOT/nt-center-gain-full-det.sh`（SHA-256 `a27c1d366ec3c86682aa8ec551b578c93c114c725e1f05f4b051008e114a5f8e`），在真值初始化 9/10 退步后停止剩余实验；001–003 三路完整 271 帧，板端保留 `runs/center-det-full/car_001` 至 `car_003`。板端/主机 `/tmp/nt-center-gain-f9f37f7-det3.tar.gz` SHA-256 `797daad1b74c965af458296b199a5605a542ee99ec9283cb2d5f3ac784af3c08`；主机复评分摘要 `/tmp/nt-center-gain-f9f37f7-det3-summary.json` SHA-256 `bad89ce587b991c095cba0078345bcae7ae1deda3b6618f93af1ecfb12ca72b4`，三路可见平均 IoU 也均下降：001 `0.7655→0.7557`、002 `0.7987→0.7954`、003 `0.8198→0.8163`。其余 detector 初始化未运行，不声称有对应板端结果。
- **裁决/Handoff**：否决增强 detector 发布中心增益。已将唯一改动的运行时文件恢复到 `35d0885` 的字节内容，`git diff 35d0885 -- rk3566/nanotrack_v3` 为空；主机重构建与 CTest **23/23 PASS**，日志 `/tmp/nt-center-gain-rollback-{build,ctest}.log` SHA-256 `fb22f82bf030a565d0c1fd8c1c623bbce3250dd231785c186a7ba38941a0c198` / `612fbb81c484d68c264f4e66a90a0f7ec33ad0ab75e8ed2b0a9bea442ea68ae4`。冻结 `8314980` 的 20 路仍是当前十目标效果对照；本候选不合 main/Debug、不作为新基线或生产 handoff。离线 display 框几何代理在已确认帧显示局部潜力，但不等于实际发布层闭环收益；下一步应针对跟踪器与 detector 的系统性偏差作独立来源/时序分解，不能继续按此代理加权。
- 为判断是否可改用经过显示平滑的 detector 中心，实际 `python3 /tmp/nt-display-center-causal-screen.py > /tmp/nt-display-center-causal-screen.json`；脚本/逐路报告 SHA-256 `eac27716d427d1418f1950dd67956e076661b7c26867541ff38fa145a3a75211` / `b65479871d6d33891b8da43e057093efc502f55de159cd2a54457ce6f1ebe77c`。该只读反事实在已接受且显示源为双确认 detector、显示框面积不小于跟踪框时，给当前发布中心 25% 的目标偏移并以 `alpha=0.35` 因果衰减；十目标双初始化全可见已接受帧平均 IoU 每路变化只在 `−0.00007` 至 `+0.00105`，最大中心偏移不足 `0.7px`。这种稀疏显示证据不能带来足够的全轨迹收益，也不把 cosmetic display 路径接入跟踪/控制。

## 2026-11-18T10:41:28+08:00 改以十目标轨迹贴合为主的 detector 中心权重候选

- 分支 `codex/ten-target-smoothness`、工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `35d08857853481c5bd39b58877a188d9af136990`。用户明确第 149 帧判遮挡、第 169 帧重捕只是建议；本轮以每目标全段逐帧轨迹、可见框尺寸和中心贴合为主，仍记录遮挡错误观测与身份风险，不把两帧时点作为单独晋级门。实际 `python3 /tmp/nt-ten-failure-root-audit.py > /tmp/nt-ten-failure-root-audit.json`，脚本/报告 SHA-256 `2e77c1eaed3737f0e45ccd727cf5876f92fd489a753899748c2bdf3673344722` / `7a5925e327abf89b249f15afe214ad0783da9c3331c10dcecfe04b734449716e`；从已冻结 20 路版本真值评分确认：`car003/009/010` 双初始化的失败门均为遮挡期误留观测，`car004` 真值初始化另有 127–129 帧可见部分框不合格；用户确认的 `car004` 第 42 帧可见由版本化真值保留。
- 实际 `python3 /tmp/nt-output-detector-weight-screen.py > /tmp/nt-output-detector-weight-screen.json`，只读输入为冻结 `8314980d4ce293ee49ec132465677895acc219aa` 的 RK3588 20 路 CSV 和现有真值；脚本/逐帧结果 SHA-256 `9cc28426dbfcd985efa82d71c211fdaefa4781703e6e75db78fdf8344c459d09` / `2e9f2435a223279f9b81cb99ccfdcf5e8166ec6469bbc7cb2be55e3155d1d9b1`。仅在已接受 TRACKING 且 display 来源为双确认 detector 的帧，取 detector 显示框作**离线几何代理**：其面积不小于跟踪发布框时，把中心向该代理移动 25%，尺寸只允许扩大。两个初始化共 10 个目标，18/20 条目标路的这类帧平均 IoU 提高或持平；car002 两路轻微下降，不能把显示框直接当运行时控制框。代理只说明值得测试更强的独立 detector 中心佐证，不是闭环结果。
- 候选仅把已有“连续两次同类、唯一、近期 detector 框 + 独立可靠 Recent”之后的**发布层**中心权重由 `clamp(area_ratio−1,0,0.5)` 改为 `clamp(3×(area_ratio−1),0,0.5)`。面积不大于 NanoTrack 时仍不按 detector 修正中心；尺寸规则、内部 tracker 框、下一帧裁剪、运动状态、模板提交和 detector 请求均不改。运行时源码 SHA-256 `5bc87f38927118f8cf40eed844dd765ed0fed5e737ff57ba2e3b5986d3e8e4bb`。这是通用面积比例规则，没有目标 ID、帧号或车体纹理特例。
- 实际 `cmake --build /tmp/nt-ten-target-build --parallel 4 > /tmp/nt-center-gain-host-build.log 2>&1`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure --parallel 4 > /tmp/nt-center-gain-host-ctest.log 2>&1`，RKNN-off 主机构建成功、CTest **23/23 PASS**；日志 SHA-256 `5a480f0c6238407855f573009660bdd3b19cee8dab35820140eb4f68c2384675` / `5bf1891b86f33455968cec38f279607ddb8bbfa408c4dfeb070920869fdeb988`，`git diff --check` PASS。**尚未板端编译或闭环回放，不能据此宣布轨迹改善。** 下一步同模型/视频/两种首帧框在 RK3588 跑完整十目标，按每目标可见平均 IoU、中心误差、连续性和全部正式失败门与 `8314980` 对比；若 car002 或其他目标明显退化则撤回，不合 main/Debug。

## 2026-11-18T10:32:10+08:00 相邻外观、四帧外观及现成实例分割筛查

- 分支 `codex/ten-target-smoothness`，工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `6ffb005b79448ab79c502869ad292455f19832c9`。本节只读主机筛查，输入仍为冻结 `8314980d4ce293ee49ec132465677895acc219aa` 的 RK3588 十目标 GT/detector 双初始化 CSV、正式真值和 SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d` 的视频；未改算法、配置、模型、真值或板端结果。用户确认的 car010 149/169 人工时限保持。
- 实际 `/home/u20/single-object-tracking/.venvs/rknn-convert/bin/python /tmp/nt-appearance-transition-screen.py > /tmp/nt-appearance-transition-screen.json` 和同解释器运行 `/tmp/nt-appearance-lag4-screen.py > /tmp/nt-appearance-lag4-screen.json`。脚本/逐帧结果 SHA-256 分别为 `03482d695874d798b8e069aa09d9a15d5f7b5a4a367857a0af4c04d828a01c76` / `9d3243e01ad0ea6270c82b22dc357928a0346036bc9d38f91e55343543f93d3d`、`0612f544cb891b0c29f0ba0ee1eb412de59c54c63a5dcb374e880f740bd78291` / `9861738f29595b28e24ea54dbac943446b56dd5c8e3b774b6a62bfd0eb070adb`。在运行时 Recent 原始框裁剪 64×64 像素，比较相邻/相隔四帧的归一化像素相关性和 HSV 直方图距离；正式可见且已接受 TRACKING 帧 GT/DET 分别约 1800 路帧。car010 第 149 帧相邻相关性 GT/DET `0.896/0.980`，在可见分布低相关尾部只占 `13.4%/74.1%`；四帧相关性 `0.511/0.654`，分别位于低相关尾部 `3.8%/7.3%`。同一 5% 低相关门不能兼顾两种初始化；直方图变化在可见波动内。car010 第 149 帧的四帧稀疏 LK 光流仍主要跟随运动目标（29 个有效角点中约 93% 的垂直位移超过 2px），并未出现可稳定触发的运动消失。这里的可见分位数从 canonical 视频本身计算，仅作否决筛查，不能当独立校准阈值。
- 对照放大图 `/tmp/nt-car010-145-156-enlarged.jpg` SHA-256 `4acc99e8b7f68427ed907b6d8c938279e8f33e11842526fec116300a1123bc25` 显示目标在桥下逐步被遮挡，无第 149 帧单独的强外观断点。car004 GT 第 126–129 帧真值高度从约 33px 缩至 26px，现有跟踪发布框仍约 50px 高；同帧全画面 PT detector 第 126–127 帧仍给出约 51px 高的框，第 128–129 帧无近框。因此跟踪器/检测器的整车框都不能直接代表可见部分真值。
- 用本机现有 COCO `yolov8n-seg.pt`（SHA-256 `a7cd8f929e1903d78a12a48efecab430209f18dc46cb96c3599a5980c63c423c`）对 128×128 目标附近裁剪、`imgsz=640`、`conf=0.05` 做主机推理。实际 `OMP_NUM_THREADS=6 MKL_NUM_THREADS=6 MPLCONFIGDIR=/tmp/nt-matplotlib YOLO_CONFIG_DIR=/tmp/nt-yolo-config /home/u20/single-object-tracking/.venvs/yolov8-visdrone/bin/python /tmp/nt-segmentation-probe.py > /tmp/nt-segmentation-probe.json`；脚本/结果 SHA-256 `95051a0747c48f892c5d30b5f25c913f2536b85d5f361778e7b84a59e3b76306` / `4c817b891c0f0ab68088a131010e13dbdc4e52c0b88602b7b13b2c33fe34e828`。car004 第 126–129 帧全无近目标实例；car010 第 149 帧仅有置信度 `0.064` 的近车分割，其 mask 外接框 `32.6×36.6px`，没有缩至可见部分。此模型无法补足两个关键症状；仅主机 PT 筛查，未转换 RKNN 或上板。
- **裁决/Handoff**：不接入相邻外观阈值、四帧相关阈值、稀疏光流或这份现成分割模型。它们没有同时识别 car010 第 149 帧两种初始化，也未给出 car004 第 126–129 帧可靠可见范围。十目标正式 6/10 双初始化、car004 修订真值 GT 6/10/DET 7/10、main/Debug 与生产 handoff 均不变。下一步须获得能标注/预测部分遮挡下同一实例可见范围的独立监督或其他稳定证据，再做完整 20 路闭环；不能把 canonical 本段的阈值当通用模型。

## 2026-11-18T10:22:33+08:00 car010 人工遮挡边界与恢复时序复核

- 分支 `codex/ten-target-smoothness`、工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `8167d1c59eee2b60bbc92acf99bd0682d6086cbe`。用户进一步确认今天新增第 2 个目标 `car_004` 第 42 帧可见，且 `car_010` 第 149–169 帧大部分被遮挡，要求不晚于第 149 帧识别遮挡、不晚于第 169 帧正确重捕。`car_004_visible_unboxed_v2` 已单独保留该可见语义；本轮未凭肉眼补造 `car_010` 的逐帧真值框，也未改 canonical 包、运行时算法或 main/Debug。
- 实际 `python3 /tmp/nt-car010-deadline-audit.py > /tmp/nt-car010-deadline-audit.json`，只读输入为已归档冻结源码 `8314980d4ce293ee49ec132465677895acc219aa` 的 RK3588 十目标双初始化 CSV 与现有真值。脚本/报告 SHA-256 `3fc39fc45ec804fbd0ce3dd1c70fffc14ff43ff4a14b1552cdd740fc691ababb` / `d8159ea2ef6884eb9524665e1fe30e5d57d03499b083d95e049ecf16cf0ef5c8`；`car_010` GT/DET 原始 CSV SHA-256 `cbe415cb3098bcbdd1416d9db245405692af6f6f959294f6da9382b31b85b1f1` / `0e4ea7a19e99ebb3fc3b60158b0f7028beeb7f00fde3a55fc66bfa2fce3f0f87`。原始运行命令、模型/视频身份及板端输出路径见冻结 `8314980` 的十目标章节。
- 两路第 149 帧仍 TRACKING 且 `accepted=1`，Recent PSR `8.175/8.182`，与第 148 帧 `8.029/7.996` 接近；第 156 帧才 SUSPECTED，第 158 帧 SEARCHING。第 169 帧按现有正式真值首次重新可见，搜索年龄 `366ms`，被 `SEARCH_MIN_AGE` 拒绝；GT/DET Recent IoU `0.358/0.392`，Safe IoU `0.363/0.384`，没有可直接接受的准确跟踪框。第 170 帧年龄 `400ms`，三路证据首次进入 `AWAITING_CONFIRMATION`，detector 框对真值 IoU `0.844/0.857`；第 171 帧第二次确认后 `RECOVERY_STAGED`。因此缩短等待时间本身不能证明第 169 帧正确重捕。
- 对同一 20 路的第 `300–399ms` 搜索年龄窗口只读筛查：GT/DET 各有 8 个“因最小年龄拒绝且缓存 detector 置信度非零”的正式全遮挡帧，分布在 car003/005/006/008/010（GT）及 car003/005/006/007/010（DET）。缓存置信度并不等于当帧已通过身份验证，这些是放松年龄门必须核查的反例，不是直接证明会误重捕。此前单独设置 `333ms` 以及与同帧二次搜索组合的候选均已在板端回归中被否决；不据此数据调整阈值。
- **裁决/Handoff**：人工两项期限仍未达到，当前正式十目标 GT/DET 均 6/10 PASS、`car004` 修订真值 GT 6/10/DET 7/10 的既有结果不变。本轮只形成定位证据，不声称新算法或板端通过。下一候选必须有独立实例可见性信号使第 149 帧提前退出，并在第 169 帧有足够准确的候选与身份确认；需在同一板端视频的全部十目标双初始化回放及外部反例中验证，才可考虑更新基线。

## 2026-11-18T10:14:55+08:00 搜索发布速度反事实与外部四帧可见性模型否决

- 分支 `codex/ten-target-smoothness`、工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，起点 `c5259f0b87d4e327cfa4f69bf087d2ba24904e3e`，开始时干净。本节仅主机离线实验，未改运行时、模型配置、正式真值、main/Debug 或板端程序。对冻结 `8314980d4ce293ee49ec132465677895acc219aa` 的 RK3588 十目标 GT/detector 双初始化完整 20 路 CSV，实际 `PYTHONPATH=. python3 /tmp/nt-search-output-velocity-screen.py > /tmp/nt-search-output-velocity-screen.log`。脚本/日志/逐帧 JSON SHA-256 `98682535cce6811962b9a7b99a0772338bb3d763f1d3ca6545f104f940b09b69` / `a814cf4b7fa5fdc784e5cadfc1e495ad7289c175ed116302bffba055e0b49feb` / `f5c0617f1b03e9fc1031a8fe2652b4d4c102a006c6eea2585c2f486672358247`。用最近八次已接受的 TRACKING 框的相邻速度中值，从最后接受位置外推 SEARCHING 发布中心；只读反事实不改变搜索 ROI/控制。197 个有可观测真值的搜索帧路，中心均误差 `19.85→20.09px`，car004 GT `22.85→31.43px` 而 DET `23.81→18.70px`，对两种初始化方向不一致，故不接入此发布规则。
- 为检验连续图像是否比已否决的单帧/双帧遮挡头提供独立信号，实际 `OMP_NUM_THREADS=6 MKL_NUM_THREADS=6 /home/u20/single-object-tracking/.venvs/nanotrack/bin/python /tmp/nt-temporal-visibility-screen.py > /tmp/nt-temporal-visibility-screen.log 2>&1`。输入为本机 `/home/u20/dataset/VisDrone2019-MOT/VisDrone2019-MOT-{train,val}` 汽车 `category=4`、`truncation=0`、25–90px 的标注，取同一 track 的 t−9/t−6/t−3/t 四帧、每帧 1.4 倍框上下文 64×64 RGB；train/val 采用原序列分组，原始序列名交集 0，选中 train 45 序列 `3000/1800/500` 清晰/部分/重遮挡，val 6 序列 `1200/400/100`。该实验裁剪使用外部数据集**每帧真值框**，只是一种上界，实际运行时没有这些框；canonical 视频没有进入训练、选 epoch 或阈值。脚本/训练日志 SHA-256 `7490070b2d6d0d9906c8625b825295f4332f6679060ea1bc87789f7a19f0c2d0` / `a2f356eaaa37a22a5901a170ee5d22092fad4ee034d955f03e0530520d9e7794`；缓存 `/tmp/nt-temporal-visibility-screen/{train,val}.npz` SHA-256 `bd6da816502cd2b3147c0baca5c4ca50fd8fc060b390ba70d71a754f3cd759f0` / `650a74575b2dd351c455e946d3e346fcc2d5b7f70296a1d35754a880ae869f3f`，采样索引 SHA-256 `7be053d62afcce122ea573da2098b6ba50e9fab43aa61f79414e034e0f43bd76` / `408a1182db82d982fa0f3e61b48bfccf0a3144882a7bd5cc48b66459d9b7134d`。固定种子 20261118，12 通道四层小 CNN 训练 5 epoch，按独立 val AUC 选第 5 epoch；最佳完整可见对任意遮挡 AUC `0.7943`，低于此前双帧模型 `0.8845`，验证清晰 5% 误报阈值 `p_clear<0.095264` 下部分/重遮挡召回 `26.5%/32%`。模型 `best.pt` 与报告 `report.json` SHA-256 `02b801a2e900550781319c665a469a554eb5821b67e5690a338b8b7958e2db5e` / `da154c3dd0cc3203179ab8eee24541ecbec230b652dc891830ce9c04bc09e342`。
- 冻结模型和阈值后实际 `OMP_NUM_THREADS=6 MKL_NUM_THREADS=6 PYTHONPATH=. /home/u20/single-object-tracking/.venvs/nanotrack/bin/python /tmp/nt-temporal-visibility-canonical.py > /tmp/nt-temporal-visibility-canonical.log 2>&1`，用同一 20 路板端输出的 tracker 框裁剪 `1.mp4` 四帧，不用真值框选运行时位置。脚本/日志/逐帧分数 `/tmp/nt-temporal-visibility-screen/canonical-scores.csv` / 报告 `canonical-report.json` SHA-256 `1d1c93f6ce5839b269a48561088e6a12294a07106133a75f6fc3d9a93a5d91f5` / `b0073788a4cc69581f7198e0ee5201b24ef4e4d247ec77814f45ffed07806f44` / `c393e2ad3f8cd7897d68f5c5d3ab38e2f885f3d2279a6b96bea9bcdd444aaea5` / `fdaf3581c5cae3745efe6c5d844d7d433707ff8122979387a2115004f5413b0a`。用户指定的 car010 第 149 帧 GT/DET `p_clear=0.432/0.366`，均高于独立固定阈值、漏报遮挡；正式可见且接受帧误报 GT `29/2238`、DET `34/2243`。实际 `/tmp/nt-temporal-threshold-audit.py` 复算若把阈值抬到覆盖两路第 149 帧，独立 val 清晰误报升到 `34.25%`，canonical GT/DET 清晰误报为 `103/2238`、`99/2243`；脚本/报告 SHA-256 `0d14521c068fcfb28f53378e18f08b9a8fec43b9ed041010a46b29464ee19b98` / `c93601591fd676559a30e0fcc6f6c00a6d9ca133c25c33c5c1bc81bb7f3354d3`。
- **裁决/Handoff**：速度外推和四帧可见性头都没有兼顾两种初始化、独立误报和 car010 第 149 帧时限；不接入算法、不导出 ONNX/RKNN、不上板。已有十目标修订真值复评 GT 6/10、DET 7/10 与人工 car010 149/169 时限失败保持不变。后续需要可在小目标部分遮挡时稳定识别实例可见范围、且有独立身份验证的监督数据/模型，再做 RK3588 双初始化完整闭环；不能用本视频提高分类阈值凑一帧。

## 2026-11-18T09:52:20+08:00 正式评测器支持已确认 car004 修订真值

- 分支 `codex/ten-target-smoothness`、工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `e369f482d171869efcde6200077064c3d0fd5e8d`；本次只改评测器、测试和本任务进度，**不改运行时算法、模型、正式 canonical 真值或协调契约**。`scripts/evaluate_occ_video_1.py` 增加可选 `--truth-version car_004_visible_unboxed_v2`；版本与目标 ID 绑定，固定 `ground_truth_versions/car_004_visible_unboxed_v2/checksums.sha256` SHA-256 `c8c394b41c2cb489f33a1b7e631cd1e9776cd9d75c15a5860ec221e2ddda1126`，逐文件校验整个包。只允许第 42 帧 `visible + user_confirmed_visible_unboxed + 空标签`，列为可见但无普通 IoU；未指定版本的原评测行为不变。报告 `dataset.truth_version`、`dataset.annotation_root` 和包哈希显式记录版本，错误目标与篡改包被拒绝。评测器/测试 SHA-256 `efe16221f9ae0fdd6b64527f342fa931407f79ac79bcd2acc9b8a678eeacc2cf` / `7a93394fa44779cd3b8930cdda3c1b953b485274a9e45340f17f49eb7c66c330`。
- 实际 `python3 -m unittest tests.test_evaluate_occ_video_1 scripts.tests.test_evaluate_human_occlusion_constraints -q` **30/30 PASS**，`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure -j4` **23/23 PASS**，CTest 日志 `/tmp/nt-versioned-evaluator-ctest.log` SHA-256 `c2f5e5e1b25df6a60e2b4a0051d1795766aaf93cddf5413b9fd930c9bf07cd3a`，`git diff --check` PASS。新增合成包测试覆盖唯一合法无框可见帧、版本目标不匹配、标签篡改和包哈希锁。用旧临时复评报告核对 `verdict/gates/strata/reacquisition`，GT/DET 四项逐字等同。
- 实际 `python3 /tmp/nt-evaluate-versioned-ten.py > /tmp/nt-evaluate-versioned-ten.log 2>&1`，脚本/日志 SHA-256 `313ccf5d2ed3b8fbc277ca9b815acb1accfd0be53d884fab3551444a519fc714` / `831b9eaa8987ee2d16f3125ded654a56bf5e6f9f1b85df2242ade9986158c266`。只读复评冻结的 RK3588 源码 `8314980d4ce293ee49ec132465677895acc219aa` 的 20 路 GT/detector 首帧初始化 CSV（原始路径 `/tmp/nt-search-output-8314980-full/runs/output-{gt,det}-full/car_###/`）；car004 使用上述修订包，其余九目标仍使用固定 canonical 真值。每路完整 `score.json`/`score.md` 在 `/tmp/nt-versioned-8314980-eval/{gt,det}/car_###/`，汇总 `/tmp/nt-versioned-8314980-eval/summary.json` SHA-256 `094e819b5a4b26f0d449d143a9e6945ac63bfb8869516b29897904f99ea8ca73`。GT 初始 **6/10 PASS**，detector 初始 **7/10 PASS**；car004 GT 仍因 127–129 帧框及 131–132 帧全遮挡失败，detector 初始在修订真值下 PASS。car003/009/010 仍双路失败。它是**相同板端输出按另一套已确认真值复评**，不是新板端程序或算法提升；原 canonical 20 路正式结果仍为双路 6/10。
- **Handoff**：请协调对话在采用这个版本化真值前核对数据契约与包哈希，并保留旧版评分；在此之前本任务继续同时报告 canonical 与修订版，不把 7/10 称作现行协调基线。用户的 car010 第 149/169 帧时限仍独立失败；下一算法候选必须改善可见性/身份的闭环证据并通过完整 20 路，不能仅靠改真值取得 PASS。

## 2026-11-18T09:41:36+08:00 有界等待板端聚焦回放否决；人工期限窗口修正

- 分支 `codex/ten-target-smoothness`、工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，试验源码 `577b3ecc5de44d6159a04d82389c3a2ed98f6a95`。实际将主机 `/tmp/nt-poll-wait-577b3ec.tar.gz` 传至 RK3588 `/home/neardi/Project/SOT/nt-poll-wait-577b3ec.tar.gz`，双方 SHA-256 `baea0e36d227f3271f6ad8e1f5780497daac10c56d31d8d25ee953f38a7b5cb9`。隔离原生目录 `/home/neardi/Project/SOT/nt-poll-wait-577b3ec`，实际 `cmake -S rk3566/nanotrack_v3 -B build -DENABLE_RKNN_RUNTIME=ON -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug -DRKNN_INCLUDE_DIR=/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/include -DRKNN_LIBRARY=/usr/lib/librknnrt.so`、`cmake --build build -j4`、`ctest --test-dir build --output-on-failure -j4`，**23/23 PASS**。程序 `build/nanotrack_v3` SHA-256 `918feb1cbfc74038c3cd8e97e1444805d2f585d0dd7b7d046c15cc0ed2a5b1d3`；`cmake.log`、`build.log`、`ctest.log` SHA-256 `c57440625e64db523d972ef2be60bc9d15ca4ae1813d2904d5b5e46b8ddcef6e` / `d693d9f8537de844c2a91434af4452a66926029f4f8b10bc5598dfac2bcc3dc8` / `a7d98b0631b6cf49369bd292490e0e33b2c464bcd493406b12f8547ac747edd6`。运行配置 `runs/headless-on.conf` SHA-256 `cae77ef8e894248250a324ce42d5a001d2ad71e9f430f6ff7b90ff8e922e9bac`，controller 配置 SHA-256 `0c290748192ae5f5eeac4a7b93991d07e79ad6e6770bdbe85801fb0cdbd3551f`。
- 运行脚本 `runs/focused.sh` SHA-256 `af4d9cad34aa94c4381a70a288881ed27f435159736ad1d1741dcfb28c7a0105`，实际 `bash runs/focused.sh`：用冻结 `8314980` 的相同首帧框跑 car006 真值框三次，car002/004/010 真值与 detector 框各一次，共 9 路，每路 271 帧。准确 `--bbox` 与命令由该脚本及板端 `/home/neardi/Project/SOT/nt-search-output-full-{gt,det}.sh` 固定；原始 CSV、session、日志保留板端 `runs/poll-gt-006-{a,b,c}/car_006/`、`runs/poll-{gt,det}-focused/car_###/`。板端与主机 `/tmp/nt-poll-wait-577b3ec-focused.tar.gz` SHA-256 同为 `9b77f2939a3f26beb3c61359169128f4ee83f7e04747b7660e7c2bb0635a9b5a`；主机解包 `/tmp/nt-poll-wait-577b3ec-focused/`。本轮重新核验输入视频 SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`，NanoTrack manifest/三个 RKNN SHA-256 `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261` / `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55` / `d3cb0e3879df6a1272adb59bf501324f1f8f55b58309d3c4da4a731b600973ee` / `4da379f54093d79f02775e90bfea33ebee0837c6fc73a324499279b4c57807c5`，detector RKNN/manifest/labels SHA-256 `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c` / `ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9` / `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`，日志 RKNN API `2.1.0`、driver `0.9.8`。
- 实际以 `scripts/evaluate_occ_video_1.py --scope full` 对九路逐路评分，使用源码/配置/manifest 哈希如上；每路 `score.json`、`score.md` 在主机相应运行目录，摘要 `/tmp/nt-poll-wait-577b3ec-focused-summary.json` SHA-256 `71740bec67cb0cde81436cd8ed6eb89492be20ef99b0950181031a87e34f7df2`。相对 `8314980`，九路的正式门禁和 PASS/FAIL 全部相同：car006 三次均 PASS、215 帧重捕，且排除耗时字段后逐帧输出一致；car002 双路 PASS、228 帧重捕；car004 双路 FAIL，GT/DET 分别 146/144 帧重捕；car010 双路 FAIL，171 帧重捕。car010 双路第 149 帧仍 TRACKING 且接受，第 156 帧 SUSPECTED、第 158 帧 SEARCHING、第 169 帧 `SEARCH_MIN_AGE`、第 171 帧才 `RECOVERY_STAGED`。七个可与基线配对的回放中，pipeline P95 差约 -1.2 至 +1.1ms；未见可靠延迟收益。该聚焦结果已否定候选的预期，不再花费板端时间跑 20 路。
- 将 `scripts/occ_video_1_human_constraints_v1.json` 的 `correct_reacquisition_by_frame_169` 窗口由恰好 169 改为 149–169 **含端点**，避免更早正确重捕被误判；新增测试证明第 168 帧正确重捕可通过，`python3 -m unittest scripts.tests.test_evaluate_human_occlusion_constraints -v` **4/4 PASS**，约束文件/测试 SHA-256 `645afa468f9db7b51b01cb9a9a9f610d8b007dc4b3d91f835c71f1fae6b85c7b` / `afae2f5703b4cc4183f24354a39f679be9f6c6d208efd412d2581692495bc5ee`。实际 `python3 scripts/evaluate_human_occlusion_constraints.py --constraints scripts/occ_video_1_human_constraints_v1.json --dataset-root /home/u20/single-object-tracking/data/occ_video_1 --runs-root /tmp/nt-poll-wait-577b3ec-focused/runs --run-template 'poll-{mode}-focused/{target}/frames.csv' --output-json /tmp/nt-poll-wait-577b3ec-human.json`，报告 SHA-256 `24570bdbc590b24e39a5c3a4258fb6837370aeb4d73bd34ea4917ec5d0e5332b`：car004 第 42 帧两路可见检查通过，car010 第 149 帧遮挡和截至 169 帧重捕两路均失败。
- **裁决/Handoff**：本提交把 10 个有界等待算法、配置及测试路径恢复到 `9fe480d901db7cb135ab1e3edf2142f03d8b5f5b` 的内容；试验提交和板端原始数据保留。该候选仅改善本次 car006 重复的时序一致性，没有提高正式精度或满足 car010 人工时限，**不作为新基线、不合 main/Debug、不进入生产 handoff**。后续继续寻找可泛化的实例可见性或身份信息，不能把单视频 detector 漏检当遮挡真值。

## 2026-11-18T09:27:02+08:00 异步检测下一帧有限等待候选（主机通过，待板端）

- 分支 `codex/ten-target-smoothness`、工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `9fe480d901db7cb135ab1e3edf2142f03d8b5f5b`；本次提交号以 Git 为准。检查冻结 `8314980` 的 RK3588 十目标双初始化 20 路 CSV：1734 次检测批次中，异步 `DISPLAY_REFRESH`、`SEARCH_RECOVERY`、`SUSPECTED_VALIDATION` 共有 1670 次源帧后第 1 帧返回、3 次第 2 帧返回；另 61 次 `VISUAL_OCCLUSION` 为同帧阻塞结果。全批 detector 耗时中位数 `81.60ms`、P95 `106.58ms`、最大 `126.07ms`。上一版 car006 相同源码重复运行曾因 128 帧验证批次到达 129 或 130 帧导致不同状态路径，说明异步时序值得单独检验；本次统计不证明新候选会提高精度。
- 增加默认 `0` 关闭的 `detector_poll_wait_ms`（合法范围 0–200ms），实验配置新文件 `rk3566/nanotrack_v3/config/visible-dual-gt-poll-wait.conf` 设置 `50ms`，SHA-256 `0c290748192ae5f5eeac4a7b93991d07e79ad6e6770bdbe85801fb0cdbd3551f`；原 `visible-dual-gt-visual-gate.conf` 保持原样。若仍有上帧异步请求，在本帧 NanoTrack 推理后对 detector 结果做有界等待；本帧新提交的请求不等待，同帧视觉阻塞路径不变。只改变检测返回参与控制器的时序，不允许 detector 直接授权控制；启用时配置身份附带参数，默认 `0` 的身份不变。
- 实际 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure --parallel 4`，主机 RKNN-off **23/23 PASS**；构建/CTest 日志 SHA-256 `7f3bd755b5ac1278132d6515a3ede86b134f5855f7b6f9579288b095e86179b0` / `2f5d489c49fabdfda77c0b874dc13b8f9acfa0b522f747a2a09fb597cc183e3d`，`git diff --check` PASS。新增真实 Yolo adapter 测试验证有界超时、完成唤醒、结果只消费一次；运行时 fake 测试验证只在前帧请求等待、默认关闭和配置身份/边界。**边界/Handoff**：这是主机候选，不是新的 RK3588 回归或十目标改进；下一步隔离原生编译、同配置重复 car006 和失败目标，再以正式 20 路及延迟对照决定保留或撤回，不合 main/Debug、不变更生产 handoff。

## 2026-11-18T09:16:03+08:00 检测历史遮挡信号跨序列验证否决

- 分支 `codex/ten-target-smoothness`、工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `987db3d59566f880e61b053a424bc40a4b4d690f`；仅主机离线研究，没有改运行时、配置、正式真值，也没有新的板端闭环。实际运行 `OMP_NUM_THREADS=4 MKL_NUM_THREADS=4 YOLO_CONFIG_DIR=/tmp/nt-yolo-config /home/u20/single-object-tracking/.venvs/yolov8-visdrone/bin/python /tmp/nt-visdrone-detector-temporal-val.py > /tmp/nt-visdrone-detector-temporal-val.log 2>&1`。输入为本机 VisDrone2019-MOT val 七个序列的 `category=4` 汽车标注和现有 VisDrone9 PT 模型 SHA-256 `6744672ac5e35debba2a647ad570e9fe84a1cabd1c45c9840080d0a4127fe36d`，每三帧采样一次，检测阈值 0.05；仅保留可见范围未截断、宽高至少 20px、track id>0 的汽车，按真值中心半个对角线内最近框配对，导出 9529 条帧记录。真值只用于离线配对/评分，不进入算法特征。脚本、逐帧 JSONL、运行日志 `/tmp/nt-visdrone-detector-temporal-val.{py,jsonl,log}` SHA-256 依次为 `be92c28fe6ab0c354d47d486ad8eaf155caf032f55c85afe85ad673916c5c576`、`3726a3d06deffb0ad61230f8aed880c517100a1fc4372e6819973193549815cd`、`0e8a38fb7bf08aba231e921bfdfe73836e60b1e23dc64a7d954815724cc07df6`。这是不同视频序列的 PT 筛查；未核查其与原 detector 训练图像是否重叠，不声称严格训练集独立，也不等同板端 RKNN 的置信度分布。
- 实际 `python3 /tmp/nt-visdrone-batch-trend-eval.py > /tmp/nt-visdrone-batch-trend-eval.log`、`python3 /tmp/nt-visdrone-batch-trend-onset.py > /tmp/nt-visdrone-batch-trend-onset.log`。脚本/日志 SHA-256 分别 `efee9b6cec0e839ea57b1c78481c9e01130ed24701b1ba812c404c2078efaf27` / `802a34805e8002e932972fc4ce67973120e383a96e8cb1e93c2b47fb86701660`、`097c545fc6be2a39afc88e6a6dfcef8cacc6a1d64eac248cacbce143cfc917c0` / `3b4045041255587c9b88183f045ed25957a40496fccfe73ae3ab91afcb7ccecb`。把上一节板端候选“两批漏检前置信度与框宽相对历史同时下降”用于该集合：宽度按当帧真值宽归一化时，产生 10 次部分遮挡事件和 2 次清晰误报；但 40 次清晰→遮挡起点中，37 次有足够历史，起点后 6 帧内仅检出 **1/37**。阈值先从 canonical 症状探索，MOT 只是反证，不是独立定阈验证；真值框归一化对运行时也是上界近似。
- 为检查固定阈值之外是否有可学习的通用历史信号，实际 `OMP_NUM_THREADS=4 MKL_NUM_THREADS=4 /home/u20/single-object-tracking/.venvs/yolov8-visdrone/bin/python /tmp/nt-detector-history-learn.py > /tmp/nt-detector-history-learn.log 2>&1`。只用最近 8 次检测的相对置信度、相对宽度和命中位共 24 个特征，训练 30 epoch 小型 MLP；按序列分为 3 train、1 tune、2 完全保留 test。tune 清晰样本第 99 百分位固定阈值 `0.581609`；test 有 1107 个清晰样本，误报 **771/1107 = 69.65%**，AUC `0.558`，即使遮挡召回 `92.3%` 也不可用。脚本 `/tmp/nt-detector-history-learn.py`、模型 `/tmp/nt-detector-history-model.pt`、报告 `/tmp/nt-detector-history-learn-report.json`、日志 `/tmp/nt-detector-history-learn.log` SHA-256 依次为 `cb7c6f82783729da2b06747c957c0a80b12157280796e3d950fcd07fda5c9b24`、`49d3ac721ff629917c43cfdb997019e317e0907ab70f9059c0923405645fd6f1`、`cae8004d303853654feb84a359c015b270fe176097d2245ec8659c94386bb41b`、`cae8004d303853654feb84a359c015b270fe176097d2245ec8659c94386bb41b`。
- **裁决/Handoff**：检测历史固定规则对一般遮挡起点召回过低，学习版跨序列清晰误报过高；都不接入算法，不上板，不将 `car_010` 上的离线触发视为达到 149/169 时限。后续应转向实例可见范围或独立身份信息，不用这两个反例调出单视频专用阈值。正式十目标双初始化 6/10、人工时限失败、main/Debug、生产 handoff 均不变。

## 2026-11-18T09:03:58+08:00 car010 人工遮挡语义与板端检测漏检反例

- 分支 `codex/ten-target-smoothness`、工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `642ebc95021a0f77eb3bfdcc958318268014b25f`；本节只读分析，未改算法、配置或正式真值。用户再次确认新增第 2 目标 `car_004` 第 42 帧可见；`car_010` 第 149–169 帧大部分被遮挡，要求最迟 149 帧判遮挡、最迟 169 帧正确重捕。这是定性可见性和时限裁决，不提供 149–168 帧逐帧可观测精确框；沿用独立人工时限检查，不把原包的 `full_occlusion` 标签或新插值框当作用户确认的逐帧几何真值。
- 实际运行 `python3 /tmp/nt-rknn-batch-gap.py > /tmp/nt-rknn-batch-gap.log`。脚本/日志 SHA-256 `9dfbcecd7718f89795d6434006a99aef9b9e29b1a10f80a2d3f4277e50cbed95` / `c3fa8b39fae60746b6318084ec3cd88bd143b8e4570a0366214590ac07b017d8`；输入 RK3588 ROI 检测全帧回放包 `/tmp/nt-detector-presence-26785db-results.tar.gz` SHA-256 `d2297da269df15520558bb3ec06cfff60c38957b894272848e95b6762ab2e3a2`，以及原始 20 路算法 CSV 包 `/tmp/nt-search-output-8314980-full.tar.gz` SHA-256 `bf9cb97661d1a1cbc18dea566a69debb22b75f2474a24df68da4f6d9e240e3b4`。取运行时实际到达的检测批次帧号，在该批次源帧跟踪中心半个对角线内寻找 `confidence>=0.25` 的车框；前六批至少四批命中、接着连续两批无命中时记为候选遮挡信号。ROI 回放沿用较早 `26785db` 的预测位置，此项只是板端 RKNN 检测结果与 `8314980` 算法记录的离线对照，不是同程序闭环。
- `car_010` GT/DET 双初始化在源帧 142 的近框置信度约 `0.38`，145/147 帧降至 `0.16/0.11`；按当前算法实际批次到达时刻，该候选信号最早在第 148 帧出现，早于人工第 149 帧期限。不过相同规则在正式可见且被接受的帧上，GT/DET 路分别误触发 `8/9` 个帧路，涉及 `car_002` 第 74/77 帧、`car_003` 第 200 帧、`car_009` 第 44/47 和 122/125 等；`car_002` 的漏检前近框仍有约 `0.65` 置信度，第 74–77 帧不能仅据漏检否决跟踪。重复计数也可能属于同一事件，不能把 `8/9` 当独立事件率。
- **裁决/Handoff**：单独的两批 detector 漏检虽能给 `car_010` 149 帧时限提供早期线索，却会拒绝其他正确可见帧，不并入运行时。`car_010` 第 169 帧重捕还需 NanoTrack 当前帧独立验证；这项漏检线索不解决重捕门。正式 6/10 双初始化、辅助人工时限失败、`main`/`Debug` 和生产 handoff 均不变；继续寻找能区分遮挡与检测漏检的独立身份/可见性信息。

## 2026-11-18T08:55:53+08:00 人工遮挡时限的独立回归检查

- 分支 `codex/ten-target-smoothness`、工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `6426524824e7c191e9fb29cf836ebd0b00a467ae`，本次提交号以 Git 提交为准。新增 `scripts/evaluate_human_occlusion_constraints.py` 与 `scripts/occ_video_1_human_constraints_v1.json`，SHA-256 `37c550f580825ae8511dc8474e09917c76c995062ccef909a5aa2f1d431cdf1d` / `5e97f905940b20e312fc97cfc0c61124a44f488da2d58c3009393f05a6c0b616`。这些是**辅助用户约束检查**，不改正式十目标真值、原 evaluator 或运行时算法；继续保留完整 20 路正式评分。
- 检查项直接对应已确认人工信息：car004 第 42 帧应发布可见且有效观测（无精确框，不打 IoU）；car010 第 149 帧应已进入 SUSPECTED/SEARCHING 且不接受观测；car010 第 169 帧应有经 `RECOVERY_STAGED` 的正确重捕，须与该帧已有真值框 IoU≥0.5。第 169 帧窗口取现有正式包首次可见帧，避免把遮挡期间的错捕当通过；若人工修订该帧状态，约束版本也必须随之更新。每路输出输入 CSV/真值/约束哈希，确保不混用运行记录。
- 实际 `python3 scripts/evaluate_human_occlusion_constraints.py --constraints scripts/occ_video_1_human_constraints_v1.json --dataset-root /home/u20/single-object-tracking/data/occ_video_1 --runs-root /tmp/nt-search-output-8314980-full/runs --output-json /tmp/nt-human-constraints-8314980.json`，报告 SHA-256 `4a6d0e2566b3f67a8e217f22b85fd18600a60d9cd93e6823e50e12c2746eecdf`：car004 GT/DET 的第 42 帧均 PASS；car010 GT/DET 的 149 帧遮挡状态和 169 帧正确重捕均 FAIL，与原始 CSV 逐帧诊断一致。实际 `python3 -m unittest discover -s scripts/tests -p 'test_evaluate_human_occlusion_constraints.py' -v` 3/3 PASS，覆盖可见/遮挡状态分离、重捕身份和截止帧；`python3 -m py_compile ...` 与 `git diff --check` PASS。
- **边界/Handoff**：该辅助检查只固化用户已确认的三项局部约束，不能替代十目标的中心精度、连续性和正式门禁，也不是新的板端运行。正式 car010 149–155 部分可见帧是否需要精确框仍待用户回复；算法、正式双初始化 6/10 基线、main/Debug、生产 handoff 均不变。后续任何候选须同时通过正式全量评测和此人工时限检查。

## 2026-11-18T08:50:27+08:00 独立训练集合成遮挡增强反例

- 分支 `codex/ten-target-smoothness`、工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `bedfbdd9b91d4eb84b6afa113604d5b472a890d2`；本节仅主机模型训练与只读冻结回放，未修改算法、配置或真值。输入继续复用 VisDrone2019-MOT 按序列严格 train/val 划分的配对图块 `/tmp/nt-paired-visibility-screen/{train,val}.npz`，SHA-256 `1ad9c6b77ae26ca6eb3a62ad2530d6b7e3b123f1e71908fa836fdf54fe517e97` / `81a44caade11b045e193a885d8d02654dfbd3c66e04172f5b239dce2051dff08`；canonical 视频不用于训练、选 epoch 或定阈值。
- 实际 `/home/u20/single-object-tracking/.venvs/nanotrack/bin/python /tmp/nt-paired-visibility-synthetic.py > /tmp/nt-paired-visibility-synthetic/train.log 2>&1`。在训练集 3000 个无遮挡样本的当前图块上合成随机角度 `0–180°`、长 `20–62px`、厚 `5–19px`、透明系数 `0.8–1.0` 的背景纹理遮挡条，标签为部分遮挡；原训练样本保留。脚本/日志 SHA-256 `4c155eaaf26e3fc6e8762598441d5bd70abe21a5e962ff0e0e911f1db9827df7` / `78aed1b85ecc18ea5c63f022129de92988be5e62686e29e6c4bc67771c3bf355`。仍用同一 6 通道 PairCNN，独立 val AUC 选第 5 epoch，完整可见 vs 任意遮挡 AUC `0.8460`，低于未增强模型 `0.8845`。冻结模型 `/tmp/nt-paired-visibility-synthetic/best.pt` 与报告 `report.json` SHA-256 `17c723383ea019e34a4596626bf6d3f03a257b604f59a50bc5a25caa291bfa64` / `80b2504e3ae554eade8aeed8c424cf0e191051539648fd2d0d1282ec90b0f164`。独立 val 清晰样本第 5 百分位阈值 `p_full<0.0087743` 时，部分/重遮挡召回 `46%/42%`，原模型为 `57%/42%`。
- 冻结模型后实际 `/home/u20/single-object-tracking/.venvs/nanotrack/bin/python /tmp/nt-score-synthetic-canonical-lag3.py > /tmp/nt-paired-visibility-synthetic/canonical-lag3.log 2>&1`；脚本/20 路逐帧 `/tmp/nt-paired-visibility-synthetic/canonical-scores-lag3.csv` / 摘要 `canonical-report-lag3.json` / 日志 SHA-256 `0f7a761e2556c1101eb3ff9835a23af1636e1d673bd5288b74ed9cf0af97ce22` / `6135f62bffaf3ee53479f75799cf25b0db6e78045f075aa663deffb0c64b95f4` / `d484a955a2ed2beb491890c23db3d1472583323a9444e28bcddfe6c8c50f13ea` / `456a32cddb124172ca438ee8c70395265c7c9e303a4d48677f16ada3efeba24e`。清晰参考只读使用 3 帧前真值可见框，不可冒充运行时可获得的参考；固定外部阈值在 4582 个当前正式可见且已接受帧路中触发 56 个，但 car010 第 149 帧 GT/DET `p_full=0.376/0.579`，均未报遮挡；car003/009 起点也未命中。人工确认的 car010 149/169 时限没有改善。
- **裁决/Handoff**：随机合成遮挡没有提升独立验证泛化，也未识别 car010 关键起点；不导出 ONNX/RKNN、不接入运行时、不以 canonical 视频重定阈值。正式十目标 6/10 双初始化基线、当前算法、main/Debug、生产 handoff 均不变。下一步需更贴近真实遮挡可见范围的独立监督；car010 部分可见帧是否应有精确框仍待用户回复。

## 2026-11-18T08:44:55+08:00 前景可见比例与局部检测联合筛查否决

- 分支 `codex/ten-target-smoothness`、工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `b0a703203a012647ccc5e2d9258734b04b0de417`；只读复用同一视频 SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`、`8314980d4ce293ee49ec132465677895acc219aa` 的 20 路原始 RK3588 CSV、PT detector SHA-256 `6744672ac5e35debba2a647ad570e9fe84a1cabd1c45c9840080d0a4127fe36d`。未改源码、运行配置或正式真值。
- 先实际执行 `/home/u20/single-object-tracking/.venvs/rknn-convert/bin/python /tmp/nt-grabcut-visibility-probe.py` 对 car002/004/009/010 局部窗口检查；脚本、输出 JSON、日志 SHA-256 `821b241b9e05aa3e2a49a831ecc8da3dc32d18a70f3d2cdebb1320aa0f73986c` / `3cb8e67fd61e2e5364acc0e6de227e46d7891689bcb1228b28e5263ac7abc963` / `709c412b0b2d2881ed5f7de6de81183baa84484f19b6589e83ea05357650d1e2`。以 tracker 框内中心区域为必前景、框外为背景的 GrabCut 分割，car010 GT 第 148→149 帧前景占比 `0.630→0.513`，但其他目标绝对占比差别很大。
- 随后实际 `/home/u20/single-object-tracking/.venvs/rknn-convert/bin/python /tmp/nt-grabcut-full-screen.py > /tmp/nt-grabcut-full-screen.log 2>&1`，对十目标 GT/DET 双初始化的 TRACKING/SUSPECTED 帧同法计算前景占比与最近 8 个接受帧中值之比；脚本/逐帧 JSON/日志 SHA-256 `06831163ecdbdbbdbe0e3a61baca9c420937cd39a24b53485ca6a723bc30ca2` / `bb36b9fb42c4672acce3324650fa16ec3be488aa031616b90b02c73681fb4125` / `73e8853bc3faeab60376611620743e8e6ce6bc21b213a8eecac71c0146914b31`。4600 个有足够历史的正式可见且接受帧路中，比例 `<0.8` 误触发 175 个、`<0.85` 误触发 317 个。car010 第 149 帧 GT/DET 比例 `0.746/0.815`，要求同时覆盖两路至少需阈值高于 `0.815`；该阈值在本视频并不低误报。
- 实际 `MPLCONFIGDIR=/tmp/nt-matplotlib YOLO_CONFIG_DIR=/tmp/nt-yolo-config /home/u20/single-object-tracking/.venvs/yolov8-visdrone/bin/python /tmp/nt-grabcut-roi-conjunction.py > /tmp/nt-grabcut-roi-conjunction.log 2>&1`，仅对比例 `<0.85` 的 329 个已接受帧路，用当前跟踪框中心的 3 倍局部裁剪运行 PT detector；脚本/逐帧 JSON/日志 SHA-256 `9a8316ae0f1f5c9c71f64d168d519de789b636d247bbe15efe16f61b4cfd460a` / `497e4cf2d1a3c08055c4872c6a7f05d3914a3e77dbd5259e561791b0e832d7c1` / `878a5b0eb5e50da63d1a6bf7e02cfd0bff6f66d32a37d7f2da28d9b69be82123`。`比例<0.82 且局部无近车框` 虽覆盖 car010 第 149 帧 GT/DET，但在正式可见接受帧仍触发 59 个，集中 car003/009；这是本视频 PT 上界，未在 RK3588 接入或验证。
- 独立外部验证实际 `/home/u20/single-object-tracking/.venvs/rknn-convert/bin/python /tmp/nt-grabcut-visdrone-val.py > /tmp/nt-grabcut-visdrone-val.log 2>&1`；输入为既有 VisDrone2019-MOT val 的 2450 对 64×64 图块 `/tmp/nt-paired-visibility-screen/val.npz` SHA-256 `81a44caade11b045e193a885d8d02654dfbd3c66e04172f5b239dce2051dff08`，其中无遮挡 2000、部分遮挡 400、重遮挡 50。脚本/逐样本 JSON/日志 SHA-256 `b6eb205d724434a7e793ca507580b9cc8e9531017d29d54f95a7e5cc5e1087d1` / `07bf93308332f964668a747e7298386b17d52ae93c95acee9d3cdadd9e72240d` / `1213bb8093b75d1f4c16e3b624a88de8a6023d0944b395fb35e394175aabd442`。用 val 清晰样本 1% 误报分位的配对前景比例阈值 `0.662`，部分/重遮挡召回仅 `2.75%/0%`；5% 误报阈值 `0.797` 时也仅 `14.25%/10%`。外部图块尺寸与运行时滚动参考不同，只能作为分割信号的泛化筛查，不能将阈值直接移植。
- **裁决/Handoff**：不接入 GrabCut 占比、局部检测的联合阈值或目标颜色特例；它们既不能在独立数据保持低误报和足够召回，也不足以使十目标板端输出贴真值。当前实验算法、正式 6/10 双初始化基线、main/Debug 及生产 handoff 均不变。后续需有目标实例可见范围/身份的独立监督与成套板端回归；正式 car010 149–155 标签的部分可见语义仍待用户对有无精确真值框的回复。

## 2026-11-18T08:36:56+08:00 十目标检测缺席与局部运动信号只读筛查

- 分支 `codex/ten-target-smoothness`、工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `64aed41b5b22d111b7f20608d578becafbe22c02`；本节无算法/配置/真值修改。输入为冻结的 RK3588 20 路源码 `8314980d4ce293ee49ec132465677895acc219aa`，原始包 `/tmp/nt-search-output-8314980-full.tar.gz` SHA-256 `bf9cb97661d1a1cbc18dea566a69debb22b75f2474a24df68da4f6d9e240e3b4`，视频 `/home/u20/single-object-tracking/data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`。全帧 PT 检测复用 `/tmp/nt-detector-temporal-census.jsonl` SHA-256 `0cc2cebef983093bf8ed8743493ba8fb756270c67d940cf90301f96ddb803ecc`，模型 `/home/u20/single-object-tracking/artifacts/yolov8-visdrone9/export/yolov8n-visdrone9-best.pt` SHA-256 `6744672ac5e35debba2a647ad570e9fe84a1cabd1c45c9840080d0a4127fe36d`；这是主机同帧上界，不是 RK3588 异步候选。
- 实际 `python3 /tmp/nt-det-miss-response-screen.py`，脚本/逐帧 `/tmp/nt-det-miss-response-screen.py` / `.csv` SHA-256 `b66224306deadd5481e443bbed1a3f358b8b793edda8a9dbab483ca05345be66` / `a86eef24424fa721fe0a114f2102e86f39f680aa7cf89cc8e67697ccb63ec220`。以跟踪框与同类 PT 候选 IoU≥0.3 或中心距≤半个框对角线为“附近支持”：car004 GT 第 131 帧连续缺席 4 帧且 PSR 相对下降 `-0.235`，car009 GT 第 116 帧缺席 2 帧且下降 `-0.119`；但 car010 GT/DET 第 149 帧仍有附近候选，缺席连续数为 0，PSR 下降仅 `-0.019/-0.028`。在 4660 个正式标可见且被接受的帧路中，连续缺席至少 2 帧为 19 个、至少 3 帧为 14 个；把 2 帧缺席与 PSR 下降至少 0.1 相交为 0 个可见误报、3 个现标遮挡命中，但**仍漏掉 car010 第 149 帧**。这只是本视频离线筛查，不以零误报调正式阈值。
- 实际 `MPLCONFIGDIR=/tmp/nt-matplotlib YOLO_CONFIG_DIR=/tmp/nt-yolo-config /home/u20/single-object-tracking/.venvs/yolov8-visdrone/bin/python /tmp/nt-roi-detector-onset-screen.py > /tmp/nt-roi-detector-onset-screen.log 2>&1`，模拟现有 `make_visual_crop` 的 3 倍目标裁剪、中心 320×320 与 160px 灰边，GT/DET 两路 car003/004/009/010 的指定窗口共 98 帧路。脚本/逐帧 JSON/日志 SHA-256 `2b4541b5f3584f8e4619533b3f14ad74240e7f8805532074092693108400da8a` / `bd10bb71d8a4f1b54ecef01266c9063833bc34801741268ddaa9fdf2ef4a7d16` / `f1ce42fafc43cb995136ad22915e1476189a08a61976a41169aab30856e9ebee`。car010 两路 149 帧局部没有 30px 内 car 候选，但 145–148 帧**可见时也连续无近框**；car009 GT 第 115 帧已有 `0.28` 的近框而正式标遮挡。局部 detector 无候选/有候选均不能独立定遮挡边界。该实验用 PT 近似，不声明板端 RKNN 同值。
- 实际 `/home/u20/single-object-tracking/.venvs/rknn-convert/bin/python /tmp/nt-flow-band-probe.py`，脚本 SHA-256 `35aa7ea9cfe94ebe4fbbf4c3a8975a0a24f006bbde313fcd60f9ec7fcab2264f`；局部 Farneback 的中心静止像素比例在 car010 第 149 帧为 0，而第 147 帧可见时为 1，信号受视频重复帧和小目标裁剪影响，不能作为单帧遮挡门。**裁决/Handoff**：不接入“检测缺席+PSR”、局部检测有无或静止像素单阈值；它们没有解决用户指定 car010 149/169 时限，且仅本视频支持。正式十目标 6/10 双初始化、算法源码、main/Debug 及生产 handoff 不变。car010 第 149–155 帧部分车身可见、正式包却标完全遮挡；已请求用户明确这几帧是否应有精确可见部分真值框，答复前不替换正式标注。

## 2026-11-18T08:28:28+08:00 条件空探针板端回放无收益，撤销算法候选

- 分支 `codex/ten-target-smoothness`，工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`。候选源码提交 `a37d83bee6f1280401a44284b2735bc7baec299c`，源码包 `/tmp/nt-empty-probe-conditional-a37d83b.tar` SHA-256 `f35863b43c4619ae5e9decc26ce6dabeec2bf98338d87fdbfa1b81ccb7b5c027`，RK3588 板端根目录 `/home/neardi/Project/SOT/nt-empty-probe-conditional-a37d83b`。实际 `bash /home/neardi/Project/SOT/nt-empty-probe-conditional-setup.sh` 原生编译及板端 CTest 成功；脚本 SHA-256 `9c1d484ce4e1da32503af6effdb63c1f2db4e71b0c6ec30b1d24ae3a3f840602`，板端程序 SHA-256 `612f4f0f62d050637f8839f6c044442761402443f78dee621c56528eac8704fc`，编译/CTest 日志 SHA-256 `7d53f2f1570953c59f70df17e27855dd6628ecade24721bbcfe98a480b9521e9` / `680e8a9c668235ab1da62ddfdc0d68bf9dc10b4a2eeeb01842daaf693e60727b`。RKNN API `2.1.0 (967d001cc8@2024-08-07T19:28:19)`，驱动 `0.9.8`。
- 视频 `/home/u20/single-object-tracking/data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；板端 detector 模型/manifest/labels SHA-256 `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c` / `ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9` / `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`，实验配置 SHA-256 `752e36e05f6972a1ec5a0344d11076a4d685ef86d19a550191a5168aa329cd8b`，板端 headless 配置 `f4bb40bc783a9ddf79e8d44e6d1728bdbec17eba205645fda222505072c923ab`。
- 实际 `bash /home/neardi/Project/SOT/nt-empty-probe-conditional-focused.sh` 跑 car002/003/004/006/009/010 的 GT 与 detector 初始化共 12 路、每路 271 帧；脚本 SHA-256 `83fd71b0d59984b9ac776dcdc4556fd3bff511115d7101ad1922842c8e0dc64c`。原始记录板端路径 `.../nt-empty-probe-conditional-a37d83b/runs/hold-{gt,det}-focused/car_###/{frames.csv,frames.csv.jsonl,session.json,run.log}`，主机副本 `/tmp/nt-empty-probe-conditional-a37d83b-focused/runs/`；实际 `python3 /tmp/nt-score-empty-probe-conditional.py`，评分脚本/摘要 `/tmp/nt-score-empty-probe-conditional.py` / `/tmp/nt-empty-probe-conditional-a37d83b-focused-summary.json` SHA-256 `c73ad676ef08b9d3160ab8b2e1f679e118f32a4b756ad33414add23d70131f28` / `1a0be25ee23839d3acef13c8792516e35fdf5b4f7da7f97c800bdfad474a1629`；每路正式评分在相应记录目录 `score.json`/`score.md`。12 路相对冻结基线的正式门禁失败帧集合、PASS/FAIL、重捕帧均无变化：GT/DET 聚焦六目标各 2 PASS、4 FAIL。car004 GT 仅第 131 帧触发保护，car009 GT 第 116–119 帧触发；car003 可见第 51 帧未触发。用户修订的 car004 第 42 帧按可见对照，car004 修正版真值下 GT 仍 FAIL、DET 仍 PASS。car010 两路均未触发，第 149–155 帧仍接受跟踪结果，第 156 帧 SUSPECTED、第 158 帧 SEARCHING、第 171 帧才确认重捕，均未满足用户的 149/169 时限；GT/DET 第 149 帧的 CSV SHA-256 分别由全路文件 `8f332c8d148570cce64f35880a87c02a390e5acabdda8256cd21008bcd9fda17` / `3c29a2a59cd0532e1e25df91608cfd1ed34809122b5760e043fee35fc9919448` 固定。
- **裁决/Handoff**：条件空探针没有正式收益，本提交将上述 11 个算法、配置、测试文件恢复到 `9824f226d13bb9280f1e26add0d6ee5aeac7df57` 的内容；实验提交和板端输出留存，main/Debug 及生产 handoff 不变。car010 第 149 帧 Recent 的 PSR/APCE 为 GT `8.18/17.91`、DET `8.18/17.65`，与第 148 帧接近；现有响应质量无法提前发现该处大部分遮挡。第 169 帧仍被 `SEARCH_MIN_AGE`（400 ms）阻止，f168 detector 请求结果在 f169 返回但未提供目标候选，f170 才有首个三方一致候选、f171 二次确认。下一轮应测试具备独立可见性/同帧 detector 证据的通用方案，不能仅调本视频帧数或放松重捕确认来凑时限。

## 2026-11-18T08:15:52+08:00 有条件的空视觉探针清除保护，主机阶段

- 分支 `codex/ten-target-smoothness`，工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `9824f226d13bb9280f1e26add0d6ee5aeac7df57`；本次提交号以 Git 提交为准。仅在有效同帧视觉探针结果没有任何候选、Recent 框亮度相对可信历史低于 `0.65`、状态已为 SUSPECTED 时延迟清除；不直接宣称遮挡或授权重捕。新参数 `visual_probe_empty_hold_bright_ratio` 默认 `0` 关闭，实验配置 `rk3566/nanotrack_v3/config/visible-dual-gt-visual-gate.conf` 设为 `0.65`，配置 SHA-256 `752e36e05f6972a1ec5a0344d11076a4d685ef86d19a550191a5168aa329cd8b`。输出日志新增 `VISUAL_PROBE_EMPTY_HOLD`。
- 冻结的板端基线 `8314980d4ce293ee49ec132465677895acc219aa` 共 7 个有效空探针事件：阈值 `<0.65` 离线命中 car004 GT 第 131 帧和 car009 GT 第 116 帧，未命中可见的 car003 第 51 帧；canonical 离线审计脚本/JSON `/tmp/nt-empty-probe-bright-audit.py` / `/tmp/nt-empty-probe-bright-audit.json` SHA-256 `0d8b1b795823c3a6e50251f909791dd8ce385a004da1475ab3bc6ed27bb08ecd` / `bfabf03f032e1a4f43d11d15486d7da50bd786704eefdcecbacf5d0ead2d819a`。仅 7 个事件，不能据此断言泛化。
- 独立 VisDrone2019-MOT train/val 近似图块亮度筛查脚本 `/tmp/nt-visdrone-bright-ratio.py`、报告 `/tmp/nt-visdrone-bright-ratio.json` SHA-256 `c7d947deaa5aa2fd596cea1addd7bafe3a74de60d625ae78e745d370db9d948a` / `282bdd9c09e567725847446dfc79b453fd849c9af7b74c34447c3317bcc00a42`。独立 val 可见样本 1780 个中 `<0.65` 触发 22 个（1.24%）；部分/重遮挡分别 18/355、19/50。该离线裁剪近似运行时取样，且空探针本身是额外条件。
- 实际执行 `cmake --build /tmp/nt-ten-target-build --parallel 4` 成功、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure` 23/23 PASS，新增配置上下界、运行时有效空探针和控制器延迟清除回归。**边界/Handoff**：尚无本候选 RK3588 运行与正式十目标评分；主机测试不是板端证据。下一步在板端原生编译，先对 car002/003/004/006/009/010 两种初始化做聚焦回放；若可见误报或正式门禁退化，撤销候选。car004 第 42 帧按用户修订视为可见；car010 第 149–169 帧大部分遮挡，须在第 149 帧前进入遮挡判定并在第 169 帧前正确重捕，本亮度门未声称能满足该时限。

## 2026-11-18T08:09:57+08:00 3 倍上下文可见性模型对照否决

- 分支 `codex/ten-target-smoothness`，工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `70d96d40bd08882342fd9ef6cdd21bf813a80081`。本次保持上节独立 VisDrone2019-DET 训练/验证划分、原始样本顺序、ImageNet MobileNetV3 Small 骨干权重和 576→64→1 头结构，只将图块视野由标注框 1.5 倍扩至 3 倍；不使用 canonical 视频训练、选 epoch 或设阈值。实际 `/home/u20/single-object-tracking/.venvs/yolov8-visdrone/bin/python /tmp/nt-wide-context-data.py > /tmp/nt-wide-context-data.log 2>&1`，脚本/日志 SHA-256 `4a443d690682c8de12904e395a0363e707e3daaad97c07b2f3c00f88e741dcd8` / `9f77b0eb4006651439c39a32fab4406e311a7c3fa6006a39d190d51c170ec507`，脚本逐样本断言标签顺序与上节缓存一致。`/tmp/nt-mobilenet-visibility-wide/{train,val}.npz` SHA-256 `a3cda10ec885fc97973d6bda97557cb8a1d94e151c4b101fb3b8621fe25bcba3` / `61fd00ebe55af3876bd152dfb2689beca4e7c550ce79c9c91ffb60c8daba9702`。
- 实际 `MPLCONFIGDIR=/tmp/nt-matplotlib YOLO_CONFIG_DIR=/tmp/nt-yolo-config /home/u20/single-object-tracking/.venvs/yolov8-visdrone/bin/python /tmp/nt-mobilenet-visibility-wide.py > /tmp/nt-mobilenet-visibility-wide/train.log 2>&1`；脚本/日志 SHA-256 `0f6eed85b28cffe4b2c623e42f460b3a52160d94351a4855efcca21591ec28e8` / `dc4fdcd94c5ac3a7866588b6742fbc04d3f08a8b97ed5de69d99576327e2e4bc`。按独立 val AUC 选 epoch 22，AUC `0.88709`（上节 1.5 倍 `0.92914`）；验证集可见帧 5% 分位固定 `p_clear<0.07198425`，部分/重遮挡召回 `0.4391/0.7614`。特征缓存 `train-features.npy`/`val-features.npy` SHA-256 `ced7f378d721441453512091bb5a7f15a409b728e35f1118c2f8e0dd9be6d6ae` / `87a4c76c8dcaaf0041b5abd63f9c6373bdd5a068deaaa1b2d2b267541771c50e`，头 `/tmp/nt-mobilenet-visibility-wide/best-head.pt`、报告 `report.json` SHA-256 `3f7e76ada1a6383ab958216f881db7dab64fcee9b6cbd558ffa70cdba74bc281` / `0db1f285cfc667ef3a7132590c617f739b18675cffe3d3bf2ab4b4960e9e6770`。
- 冻结头/阈值后实际两次运行 `MPLCONFIGDIR=/tmp/nt-matplotlib YOLO_CONFIG_DIR=/tmp/nt-yolo-config /home/u20/single-object-tracking/.venvs/yolov8-visdrone/bin/python /tmp/nt-score-mobilenet-visibility-wide-canonical.py --crop-source {tracker,detector}`，脚本 SHA-256 `e8269da824ba591b29213b2e91b26be0c1ede54623df960a9288cd73f572b77b`。仍复用 `8314980d4ce293ee49ec132465677895acc219aa` RK3588 20 路 CSV、视频 SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；detector 裁剪是主机同帧 PT 上界，不能冒充板端异步 RKNN。逐帧 `/tmp/nt-mobilenet-visibility-wide/canonical-{tracker,detector}.csv` SHA-256 `418bc9065bd029a6b126be6310b1fcc935439cef9dd5ee1a7bc7805758a09141` / `ae36b78ddb3a95f544f3a395ca6ae574b3ada0ff5a3b8546937ee2294d99cdf8`，摘要 JSON SHA-256 `09a672369b0f3c4bac8b5749e7d126751bc835a2b1cb24f0f87d86a01e4f2108` / `807c1ed543b8367843313ec2e7cec4f28c4c106499413241b23076438ad211a2`。
- **结果/裁决**：正式可见且已接受的 4660 帧路里，tracker/detector 裁剪误报 `20/29`；但 car010 第 149 帧 GT/DET 的 `p_clear` 仍为 tracker 裁剪 `0.813/0.831`、同帧 detector 裁剪 `0.855/0.855`，远高于独立 `0.072` 阈值；car003 第 130 帧也未触发。扩大上下文降低了该帧清晰概率，却损害外部泛化且未满足遮挡时限。**结束此可见性分类头路线**：不接入运行时、不导出 ONNX/RKNN、不在 canonical 视频上调参。正式十目标 6/10 双初始化、通用算法、main/Debug 和生产 handoff 不变；后续需不同的实例可见范围/身份信息，而非继续按本视频调图块视野。

## 2026-11-18T08:03:59+08:00 预训练 MobileNet 可见性分类头跨数据集筛查否决

- 分支 `codex/ten-target-smoothness`，工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `6db2d09f4906c2966e303b2bf839b03d74190fcb`。现有四处失败区间逐帧审计已确认 car003 第 130 帧无完整车身、car009 第 115–120 帧桥区响应不是可靠目标，故没有以改标注替代算法改进。本节仅桌面训练与只读复用 `8314980d4ce293ee49ec132465677895acc219aa` 的 20 路 RK3588 CSV 和 canonical 视频 `1.mp4`（SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`）；没有改变正式真值、运行代码、main/Debug 或生产 handoff。
- 从 PyTorch 官方 `https://download.pytorch.org/models/mobilenet_v3_small-047dcff4.pth` 下载 ImageNet 预训练 MobileNetV3 Small，保留 `/tmp/mobilenet_v3_small-047dcff4.pth`，SHA-256 `047dcff4addef86ea5bc2eff13c9614dc11f47ab1160d0a71a25e7db994f4e1f`。沿用先前冻结 VisDrone2019-DET 独立 train/val 裁剪缓存 `/tmp/nt-det-visibility-cnn/{train,val}.npz`，SHA-256 `fd6d8daf37dc414fb76d5728d9d172c1d398799a578c5a217666508f3fdbf37b` / `314c3999ae19f92fcfc318bd512e60f35e22e49158f7fa47360f501ff673bcff`；64×64 RGB 上采样至 128×128，冻结骨干，仅训练 576→64→1 分类头，按独立 val AUC 选 epoch，不用 canonical 视频定阈值。实际 `MPLCONFIGDIR=/tmp/nt-matplotlib YOLO_CONFIG_DIR=/tmp/nt-yolo-config /home/u20/single-object-tracking/.venvs/yolov8-visdrone/bin/python /tmp/nt-mobilenet-visibility.py > /tmp/nt-mobilenet-visibility.log 2>&1`；脚本/日志 SHA-256 `dfec6a3562b4578774af7175da346e469c96613ded1235c79a4737fd752c2ff1` / `5b401ea9b5ae97b436ac779832287649d267eeab863f9bd8f193586b5fdf8dad`。特征缓存 `/tmp/nt-mobilenet-visibility/{train,val}-features.npy` SHA-256 `b6c643933142cb333ae7b0b1da6d556f5abe9c9370a50b3098a0120aef18d40d` / `83e8fe251f333a90806a676b626f14ba94645940d1f1ba7761fdb6796102fbd8`；best epoch 25，独立验证 AUC `0.92914`，固定可见误报约 5% 的 `p_clear<0.13812885` 下部分/重遮挡召回 `0.5974/0.9318`。分类头 `/tmp/nt-mobilenet-visibility/best-head.pt`、报告 `report.json` SHA-256 `025c352308497f41042f3a1905941ea8879c52ef3e48eaa0f12049a5fa43676e` / `69ba5b083609f8bfbbe8dc03ea5fa7880ed6ac82e3267d25399a3fb37ae670b9`。
- 冻结模型和阈值后实际两次运行 `MPLCONFIGDIR=/tmp/nt-matplotlib YOLO_CONFIG_DIR=/tmp/nt-yolo-config /home/u20/single-object-tracking/.venvs/yolov8-visdrone/bin/python /tmp/nt-score-mobilenet-visibility-canonical.py --crop-source {tracker,detector}`，其中 detector 裁剪仅用同帧冻结 PT census 的近框作**上界筛查**，实际 RK3588 异步 RKNN 不保证可用。脚本 SHA-256 `300377a107fc79bffbb6fff42749b8a08ee46b167d793ad6e03770cb21cef38c`；逐帧 `/tmp/nt-mobilenet-visibility/canonical-{tracker,detector}.csv` SHA-256 `4122e753d63c947d56213ec420da04fde2623c3f7a898c4bc67b237b2c842b51` / `ebf186700722ac344f91a3e021603f79cfbbdfb3ffdec8279714025e4e52556f`，摘要 `canonical-{tracker,detector}.json` SHA-256 `a76b553e128d68b5625f83947b308521353e52d9679ffcfeefd1e913314e2ef6` / `905afe60fccf713344400f033ecf09fdc0c945c3b0ad84c2de4d933f2f6b3de8`，运行日志 SHA-256 `c8dbe456ddee59718ac4518ea5e9dde215b7bbeadb2d1783a43ca44ce4fc4f20` / `ca06fe7916aad2e92c6649262651ffc603a27059962b1fcc88a0866dfae23818`。
- **结果/裁决**：正式可见且已接受的 4660 帧路中，tracker/detector 裁剪分别误报 `114/122`；car009 第 115 帧 GT/DET 得 `p_clear=0.028/0.037`，可检测。但 car003 第 130 帧两路 `0.310`，car010 第 149 帧 tracker 裁剪 GT/DET `0.977/0.978`、同帧 detector 裁剪 `0.955/0.955`，均远高于独立阈值。外部验证性能不能替代用户要求的 car010 149/169 门；**否决接入**，不导出 ONNX/RKNN，不以 canonical 帧调高阈值。下一候选需可在实例级识别小车被桥部分遮挡，同时保持清晰帧低误报；正式十目标 6/10 双初始化成绩和当前算法基线不变。

## 2026-11-18T07:56:25+08:00 car004 可见框误差归因与角点可见性筛查

- 分支 `codex/ten-target-smoothness`，工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `6996ac19b64a30a6d9f47aa240d4f0d0dc37f2cc`。只读复用 `8314980d4ce293ee49ec132465677895acc219aa` 的 20 路完整板端 CSV、冻结 PT detector census、正式真值与视频；没有修改算法、真值或配置。car004 GT 路第 22–145 帧连续 `motion_input_frozen=1`，但在该路正式可见且被接受的冻结帧中，tracking 中心平均误差 `2.332px`，非冻结帧 `2.896px`；运动预测中心误差对应 `17.716px` / `4.212px`。说明冻结预测确实滞后，不能仅由这一点推断跟踪输出整体变差。实际 `python3 /tmp/nt-motion-freeze-census.py`，脚本/16 路普查 `/tmp/nt-motion-freeze-census.json` SHA-256 `86d15ecaa39c7a3f3c37753b52ef7bb8463b853cd5781e875867d9ed0daa2fd4` / `b261ddc78dc7a3f536a62ef8dbaf8d198701cea4454b98a4d6c50816786e90bd`；统计仅覆盖新增八目标 GT/DET 路，car001/002 不在此张表。
- car004 GT 路第 125→126 帧正式真值框高度 `53.68→32.88px`，第 127–129 帧进一步为 `30.02/26.06/26.09px`，图中目标下半被天桥遮住；NanoTrack tracking 框仍约 `45×50px`，第 127–129 帧 tracking 中心偏真值 `14.0/15.2/14.9px`。板端输出框 IoU 为 `0.408/0.342/0.337`；同帧主机 PT detector 第 127 帧近框对真值 IoU `0.52`，第 128–129 帧没有目标近框。逐帧图 `/tmp/nt-car004-f122-130.jpg` SHA-256 `977e60d68fef4cc8f27a06e86333c4803acd70406dababcdf8267a9d389c8d9c`，绿色正式真值、红色输出、黄色 PT detector；图为诊断，不是评测输入。该段主要需要分辨被遮挡后的**可见车身范围**；仅重设冻结速度或提高 detector 权重不能覆盖缺框的 128–129 帧。
- 为寻找不依赖单帧 detector 的通用信息，实际运行 `/home/u20/single-object-tracking/.venvs/rknn-convert/bin/python /tmp/nt-lk-visibility-screen.py`：对每路 8 帧前已接受的 tracking 框内角点做 Lucas–Kanade 往返光流，并比较角点位移与跟踪中心位移；脚本/逐帧 `/tmp/nt-lk-visibility-screen.csv` SHA-256 `c6233dcbd43519e7acf3c07bd49032cf5e77f13dd728380c2fbd71cd19805ab4` / `0a9911356ede8cf7c8cbbcddac62360226e7219480453c1ea247d4ce69be9f38`。car003 第 130 帧 GT/DET 一致角点比均 `0`，car004 GT 第 127/128 帧 `0.344/0.214`；但 car010 第 149 帧 GT/DET 仍为 `0.429/0.591`。若以 `<0.6` 判遮挡，正式可见且已接受的样本会误触发 `943/3348`，`<0.2` 仍误触发 `648/3348`。这些是固定回放的离线信号筛查，参考框取自既有输出；未在 RK3588 增加运行时光流。
- **裁决/Handoff**：不放松运动冻结，也不接入角点数量阈值；它们不能同时修好 car004 的可见范围和 car010 的第 149 帧时限，且会损害清晰帧。需要独立可见范围或身份信息，并须在外部分布及 20 路 RK3588 双初始化中证明低误报与框中心连续性。算法仍与 `040a3f1e98eff843424db9e4770a3e8dc88c2cd1` 同内容，正式十目标 6/10 双初始化成绩和 main/Debug、生产 handoff 不变。

## 2026-11-18T07:48:48+08:00 亮度变化与通用实例分割的遮挡信号筛查

- 分支 `codex/ten-target-smoothness`，工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，起点 `984c1a543e44a4cae3a6c4a20e7a01f94071181b`，工作树干净。本节只读复用 `8314980d4ce293ee49ec132465677895acc219aa` 板端 20 路全长 CSV 与 canonical 视频 `1.mp4`（SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`），没有改运行算法、标注或正式配置。
- 按 `long_term_runtime.cpp` 的灰度阈值、8 次接受帧中值及 Recent 原始框，在主机实际执行 `/home/u20/single-object-tracking/.venvs/rknn-convert/bin/python /tmp/nt-brightness-gate-screen.py`；脚本 SHA-256 `b700c9e06c90b8419b19414d2d42b23c944a9d7b1c9fd2315906320ef4174887`，逐帧 `/tmp/nt-brightness-gate-screen.csv` SHA-256 `1428b897a18fc605e63c6c1234bfb1fc691a32874773405c558d134ca563ffbc`。car010 第 149 帧相对最近可信亮度中值，GT/DET 路为 `0.973/0.735`；car003 第 130 帧为 `1.274/1.274`。若固定阈值为 0.9，已接受可见帧中会触发 `959/4634`；0.7 仍触发 `94/4634`，又不能覆盖 car010 GT 第 149 帧。此计算用解码视频重建的亮度历史，近似运行时信号，不是新的板端轨迹；同一源码运行日志 car010 第 157 帧亮度比与重建值略异，结论仅限信号可分性筛查。
- 从 Ultralytics 官方 `v8.2.0` 发布工件下载 COCO `yolov8n-seg.pt` 到 `/tmp/yolov8n-seg.pt`，SHA-256 `a7cd8f929e1903d78a12a48efecab430209f18dc46cb96c3599a5980c63c423c`，未纳入 Git。用冻结跟踪框中心裁 3 倍目标边长 ROI、缩放 640，主机 CPU 推理汽车类；实际 `MPLCONFIGDIR=/tmp/nt-matplotlib YOLO_CONFIG_DIR=/tmp/nt-yolo-config /home/u20/single-object-tracking/.venvs/yolov8-visdrone/bin/python /tmp/nt-coco-seg-feasibility.py`，脚本/诊断 JSON SHA-256 `beafe5747d4fbdf3c2c69a46355d319eeba40c0b9377278e0e548d33f053b2d9` / `1e61dbcd54117d19f8cb56d28fadbd456572e27a41a1e41386078693c37488ce`。car010 第 149 帧没有近框，但已确认可见的 car004 第 42 帧及 car002 第 70–71 帧同样没有近框；car003 第 51 帧则有近框，说明它与现用 VisDrone9 detector 有差异，但缺席不能直接证明遮挡。
- 固定从每个目标已接受、正式可见的帧均匀取 10 帧（car001 缺独立 `frames.jsonl`，仅从前 100 帧已接受帧取样），实际执行相同 Python 环境的 `/tmp/nt-coco-seg-clear-sample.py`；脚本 SHA-256 `9b111f27900e1fb8d59a7095fb7c0e460618f65740bf6359f5a3bed296be1810`，100 份可复核结果 `/tmp/nt-coco-seg-clear-sample.json` SHA-256 `266f832685bcf37d476f70313cf937dc042d6b3956095368020c1e938aef14cb`。仅 `31/100` 帧在跟踪中心半个框对角线内找到 COCO 汽车分割框；car006/009/008 为 `0/10`，car010 `1/10`。这是小样本主机能力筛查，并未转换 ONNX/RKNN 或在 RK3588 运行。
- **裁决/Handoff**：亮度比和现成 COCO 分割模型均不能同时满足 car010 第 149 帧及时遮挡判定与清晰帧低误报。不得将分割“无框”直接设为 SEARCH 或无效控制，不用本视频调阈值。当前实验分支算法仍与 `040a3f1e98eff843424db9e4770a3e8dc88c2cd1` 相同，正式十目标 6/10 双初始化成绩、main/Debug 和生产 handoff 不变。后续如开发实例可见性模型，需要在独立数据集验证部分遮挡与清晰小目标，再按 20 路板端协议复验。

## 2026-11-18T07:47:00+08:00 空视觉探针暂缓规则板端否决与算法撤回

- 两版实验源 `76e03d2a710b225d329075ac77b945382f706eb9`、`b49aebc254caaeca91bfad032ea90017e0f9c9b8`，分支 `codex/ten-target-smoothness`，工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`。第一版源包主机/板端 SHA-256 `8088c3b72e349b5d48eea037e0038a711bc8eabb852bc557544f8b73a6fb12f2`，隔离构建 `/home/neardi/Project/SOT/nt-visual-empty-hold-76e03d2`，RK3588 原生 CTest 23/23 PASS，程序 SHA-256 `b9cc03d5ebbe7e52d907c8ba83978b38bbe98749458210b9f45ebbe4210e1d7a`。实际运行 `bash /home/neardi/Project/SOT/nt-visual-empty-hold-focused.sh`，脚本 SHA-256 `fb29086ae39782e3165b58397c454a10770df39b00c562be222bc602087ca872`; car002/003/004/006/009/010 各 GT/detector 双初始化、全 271 帧的 CSV/会话/日志留在 `runs/hold-{gt,det}-focused/car_###/`。板端与主机归档 `/home/neardi/Project/SOT/nt-visual-empty-hold-76e03d2-focused.tar.gz`、`/tmp/nt-visual-empty-hold-76e03d2-focused.tar.gz` SHA-256 均 `19279fe50a65eaf202a54e8ba61e84f5e484a6de2f42ae602d4ba73f60d0fb83`。实际运行 `python3 /tmp/nt-score-visual-empty-hold-focused.py`，12 路正式评分与基线逐门完全相同；摘要 `/tmp/nt-visual-empty-hold-76e03d2-focused-summary.json` SHA-256 `44a3b384b4851fbb2a7cdd1407e86f80e2561e855e20dc21dfe313e48236c887`。原因是输出可先显示 SUSPECTED，而内部 `suspected_since_ms_` 尚未启动，旧规则的额外门使空探针从不生效。
- 修正版源包主机/板端 SHA-256 `8953baa3749be220384e954a3d19f2033268bb51d2ab3ac778912f214785e8c9`，隔离目录 `/home/neardi/Project/SOT/nt-visual-empty-hold-b49aebc`；实际运行 `bash /home/neardi/Project/SOT/nt-visual-empty-hold-b49aebc-setup.sh`，脚本 SHA-256 `1c99e9afee349b892022a36ca3349b8f10aabcce203da24516521e6c76332661`，RK3588 原生 CTest 23/23 PASS。程序、controller 配置、运行配置 SHA-256 分别 `2ef1f08fd1fe804241023d2a16651d0f5bc015848715077404230b46b103fe91` / `bb19d2e8e085dd57084653b6ca37bb981b10ea2a0d00bc05d166f8a508c6e26f` / `163b3d0df44ece16f87ceeb8d0aec1f7502510bc60923026accdbd794aeb0834`；`cmake.log`、`build.log`、`ctest.log` SHA-256 分别 `6f79408e49ea56b21d1c37f7c83fe59a4615becc43a752f00e3048eb495bbc39` / `343819bb01faa539fc5196857ab7da1b7d60c5cc417ba3e0f69592e41918e287` / `e27e27c8e314b84513af651bda85fc7fea51121b7e1a59c0ed040b8cc8a551d5`。实际 `bash /home/neardi/Project/SOT/nt-visual-empty-hold-b49aebc-focused.sh`（脚本 SHA-256 `2cfb10e0f59c3802d7341477237580d1a32b49063d1a2e022d2b33a470217df1`），相同 12 路完整回放；板端/主机 `nt-visual-empty-hold-b49aebc-focused.tar.gz` SHA-256 均 `97a073882028b5528bbdeb555b5db398a30369f7ae31adeba0ca067657538934`，运行总日志 SHA-256 `4365c8f734572d935ba3d4a842c93e6b797b800dd826f15e4631c2a8abdb2f9d`。正式评分脚本 `/tmp/nt-score-visual-empty-hold-b49aebc.py` SHA-256 `eff9393438c9f0c23c5ae68aa3dfab24b4f5ea97268b63fa52416f8206a25cd2`，摘要 `/tmp/nt-visual-empty-hold-b49aebc-focused-summary.json` SHA-256 `c860d6714a796b00474ba7d21b1684b8a48bd98df323736a7a89981371bec125`。
- **结果/裁决**：car004 GT 第 131–132 帧与 car009 GT 第 116–119 帧不再误接受，但 car004 校正“42 帧可见”后的 GT 路仍 FAIL，car009 GT 无正确重捕；car003 GT/DET 第 51 帧实际可见，却因一次空探针直到第 70 帧才重捕，接受帧 `179→135`，并新增第 131 帧全遮挡误声明及第 139 帧重现失败。car010 GT/DET 仍第 149 帧接受、第 171 帧重捕，错过用户要求的 149/169 时限；car002/006 保持 PASS。**否决**仅凭 detector 空批次阻止清除的规则。已把六个实验源码文件精确恢复到 `040a3f1e98eff843424db9e4770a3e8dc88c2cd1`；`git diff 040a3f1 -- rk3566/nanotrack_v3` 为空。实际重跑主机 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure --parallel 4`，23/23 PASS，日志 SHA-256 `a2f99393f911b344459d711e5f03766d19fda7042896b8cac79e1338bca7995b` / `ca48b4ee1cc2bd1704d4097e18f452454f7daa1a55e83b3c925b9b579819d37c`。本回合没有获得新的算法基线；十目标正式 6/10 双初始化、main/Debug 和生产 handoff 不变。下一步寻找与单帧 detector 漏检独立的遮挡/身份信息；不能把 car004 第 42 帧人工修正视为已经纳入协调真值。

## 2026-11-18T07:28:37+08:00 空探针暂缓规则计时门修正候选

- 分支 `codex/ten-target-smoothness`，工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，起点 `76e03d2a710b225d329075ac77b945382f706eb9`。板端第一版在 car003 第 51 帧和 car004 GT 第 131 帧虽有有效空视觉探针，仍直接从显示的 SUSPECTED 回到 TRACKING；定位为 `assess_recent` 额外要求内部 `suspected_since_ms_` 已启动，而输出 SUSPECTED 可先于该计时器。修正为有效空探针直接暂缓当前帧清除，仍沿用原有疑似进入与搜索时长门；新增计时器尚未启动的测试。
- 实际执行 `cmake --build /tmp/nt-ten-target-build --parallel 4` 和 `ctest --test-dir /tmp/nt-ten-target-build --output-on-failure --parallel 4`，主机 RKNN-off CTest **23/23 PASS**，`git diff --check` PASS。板端第一版 12 路回放正在隔离目录中运行，修正版尚未上板；因此本条仅证明修正后的单元路径，不宣称十目标精度改善。下一步冻结源码，在 RK3588 另建目录运行相同双初始化回放，核查遮挡时序、误拒与真值评分。

## 2026-11-18T07:16:39+08:00 空视觉探针暂缓清除 SUSPECTED 候选待板端

- 分支 `codex/ten-target-smoothness`，工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `040a3f1e98eff843424db9e4770a3e8dc88c2cd1`。实际运行 `python3 /tmp/nt-suspected-clear-transition-audit.py`，脚本 `/tmp/nt-suspected-clear-transition-audit.py` SHA-256 `9998788276d4a3f4c800f69f4291e12fc576f3d1929b06774959690d5d41ba0e`，20 路 271 帧逐帧转移结果 `/tmp/nt-suspected-clear-transition-audit.json` SHA-256 `580b0bf85cc70ec9e209b6844656537913af462c91d703ac9c51d176158394ac`。35 次 SUSPECTED→TRACKING 中，33 次回到正式可见帧，2 次回到正式全遮挡帧，后者为 car004 GT 第 131 帧及 car009 GT 第 116 帧，两次板端同帧 VISUAL_OCCLUSION 探针均返回 0 候选。可见 car003 GT/DET 第 51 帧也各有一次空探针后正确返回，故不能把探针空当作立即 SEARCH 的充分条件；这份转移筛查没有使用真值驱动运行时规则。
- 新候选只在上一帧 SUSPECTED、当前帧视觉探针成功返回**有效空候选批次**时，暂缓用当前 Recent 响应清除疑似状态；探针错误、过期、未执行或非空不触发，单次空结果不直接进入 SEARCH。保留现有不确定期、控制上界和后续清除路径；`note` 写 `VISUAL_PROBE_EMPTY_HOLD` 以便核查。增加控制器测试覆盖暂缓后仍可清除，以及运行时测试覆盖空探针不立刻宣称丢失。实际 `cmake --build /tmp/nt-ten-target-build --parallel 4 > /tmp/nt-visual-empty-hold-build.log 2>&1` 和 `ctest --test-dir /tmp/nt-ten-target-build --output-on-failure > /tmp/nt-visual-empty-hold-ctest.log 2>&1`，主机 RKNN-off CTest **23/23 PASS**；日志 SHA-256 `da18bc45fd95147f42f6b31d8fbc2d838383be0d3dd5fb164cea3df9fe3f3617` / `3c09a16bfee8a40c9f0951f68bf296f6cd1911ecc1489b7de6f85c8d54c21`。`git diff --check` PASS。
- **证据边界 / Handoff**：当前仅是主机候选，未在 RK3588 上运行，不能宣称 car004/009 改善或十目标得分变化；car010 第 149 帧仍无触发该规则的前置 SUSPECTED，不能指望此候选满足该时限。下一步以精确提交归档在板端隔离构建，先测 car003/004/009 及已通过的 car002/006，再按结果决定是否完成 20 路回归或撤回。

## 2026-11-18T07:07:45+08:00 初始化偏差校准与外部遮挡裁剪模型筛查

- 分支 `codex/ten-target-smoothness`，工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `1286df963fee8535339e5a65f1b8f65aa3009355`。在上一节冻结的 20 路 RK3588 CSV、主机 PT detector 全帧候选上实际运行 `python3 /tmp/nt-detector-calibrated-offset-screen.py`；脚本 SHA-256 `a7535decfe1240de08089f62ab41ae6f7b66a858b68275e1a732d7273d70e416`、结果 `/tmp/nt-detector-calibrated-offset-screen.json` SHA-256 `35367da8c0b33182d7d929fb973c6ba744ef59362099debac48fc8bcb5d4c47e`。用初始化后前 5/10/20 次与 tracker 重合的 detector batch 作每目标偏差基线，只修正后续偏差变化，并按真实板端 batch 到达时刻与非 TRACKING 状态重置；未用真值校准。10 次基线、EMA β=0.1 时，GT/DET 两路可见接受帧中心 MAE `2.101→1.991px` / `1.959→1.796px`，残差逐帧速度 RMS `1.042→1.053px` / `0.999→1.010px`；car010 两路中心误差均变大。此为无反馈的离线 PT 候选替代模拟，不是改后 RKNN 程序。
- 为测试更直接的遮挡信息，独立使用 `/home/u20/dataset/VisDrone2019-DET/VisDrone2019-DET-{train,val}` 的官方分组；仅取 `category=4` 汽车、`truncation=0`、宽高 20–120px 的标注框，按标签 0/1/2 从 train 取 `12000/12000/6000`、val 取 `3903/3286/616` 裁为带 1.5 倍上下文的 64×64 RGB。实际 `/home/u20/single-object-tracking/.venvs/nanotrack/bin/python /tmp/nt-det-visibility-cnn.py --epochs 5 > /tmp/nt-det-visibility-cnn.log 2>&1`；脚本/日志 SHA-256 `222f2775fa0ba36d7793749efaf74fdf840afce352170652f743716a5b715214` / `4bff7d80a00f1399651f972e416ce1f3609a42915ab27be81d26b53b5b5faaeb`，缓存 `/tmp/nt-det-visibility-cnn/{train,val}.npz` SHA-256 `fd6d8daf37dc414fb76d5728d9d172c1d398799a578c5a217666508f3fdbf37b` / `314c3999ae19f92fcfc318bd512e60f35e22e49158f7fa47360f501ff673bcff`，采样索引 SHA-256 `c008974cedc18854e5a4a37bba05d4fbf495334bdbeb9dd1ff0ce8ecfcfea7bf` / `04b8b5013285c7288f28174e7d8faf98aa0596bdb9143c56203266e6df7cc3f5`。小 CNN 按独立 val 的无遮挡与任意遮挡 AUC 选第 4 epoch，AUC `0.8769`（部分 `0.8598`、重度 `0.9678`）；模型 `/tmp/nt-det-visibility-cnn/best.pt` SHA-256 `23ace75f925799fe406fb0f228a8bc2ac09eff34325bcb77a77b955aed1c989f`，报告 `/tmp/nt-det-visibility-cnn/report.json` SHA-256 `0b12794629ac89c8738a07dd1ae47d29aa9a3149e8d93b4e9ded5b2eb9fade4e`。按 val 可见误报 5% 预先固定 `p_clear<0.379247`；val 部分/重度遮挡召回 `44.6%/84.9%`。canonical 视频未参与训练、选 epoch 或定阈值。
- 模型冻结后实际分别运行 `/home/u20/single-object-tracking/.venvs/nanotrack/bin/python /tmp/nt-score-det-visibility-canonical.py --crop-source tracker` 和 `--crop-source detector`；脚本 SHA-256 `096b954a119ab400df9745d025188ddd3dbbd35c9c17b1aa810632b7f57452d5`。两种裁剪的 20 路逐帧分数 `/tmp/nt-det-visibility-cnn/canonical-scores-{tracker,detector}.csv` SHA-256 `d132713195cb9e621eccc9aa50a8997635948757c4918fd706c2643d5f513a8a` / `742a15bccb03c9e172c21ffc0540aa08ba50997480ce9a7f3e0cbb5517a07827`；摘要 `canonical-report-{tracker,detector}.json` SHA-256 `5b022f14486786d852505efb68c4f11f7a8afe0419faaf0156dd84d5803278c7` / `cdba35ce2b28e91b109ef115ca3e0f22bcaf60f077fd78395c84f3646bac6cfd`。每种覆盖 `5420` 帧路；可见且已接受的 `4660` 帧路均无阈值误报，但 car003 第 130、car009 第 115、car010 第 149 帧 GT/DET 两路均未识别遮挡。car010 第 149 帧 `p_clear≈0.79`，远高于 `0.379247`；用同帧 PT detector 框替换 tracker 裁剪也未改善。car004 第 42 帧两路均判可见，与用户修订一致。此结果是主机图块推理，未做 ONNX/RKNN 转换或板端运行。
- **裁决/Handoff**：不接入本次偏差校正或外部裁剪 CNN；前者没有兼顾中心精度和抖动且损害 car010，后者虽有独立数据 AUC，却不能识别规定的 149 帧部分遮挡起点。当前算法源码、正式十目标 6/10 双初始化板端基线、main/Debug 与生产 handoff 不变。继续需要能在清晰帧低误报下识别部分遮挡和身份的独立证据，并复核正确重捕不晚于 169 帧；不能把外部验证集指标当作本视频通过。

## 2026-11-18T06:53:01+08:00 十目标检测时序与因果融合筛查

- 分支 `codex/ten-target-smoothness`，工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `8510bcb68efd7b6abcc957d8031c9c7930c25113`。只读复用 `8314980` RK3588 20 路各 271 帧 CSV 和 canonical 视频 `1.mp4`（SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`）。另以已冻结 VisDrone9 PT `/home/u20/single-object-tracking/artifacts/yolov8-visdrone9/export/yolov8n-visdrone9-best.pt`（SHA-256 `6744672ac5e35debba2a647ad570e9fe84a1cabd1c45c9840080d0a4127fe36d`）运行 `MPLCONFIGDIR=/tmp/nt-matplotlib YOLO_CONFIG_DIR=/tmp/nt-yolo-config /home/u20/single-object-tracking/.venvs/yolov8-visdrone/bin/python /tmp/nt-detector-temporal-census.py > /tmp/nt-detector-temporal-census.log 2>&1`；脚本/271 帧 car 检测 JSONL `/tmp/nt-detector-temporal-census.jsonl`/日志 SHA-256 依次 `c92f1c13385c83c68964da473aa39b86f8d5b9afbef4490e7aae960c151693cc` / `0cc2cebef983093bf8ed8743493ba8fb756270c67d940cf90301f96ddb803ecc` / `f7a91fc2989bea88a7f2f36073013a43044d18ce2e5e13a6a3b596a61a6727fb`。这是主机 PT、同帧完整检测上界，不能冒充板端 RKNN 实际候选或时序。
- 实际运行 `/home/u20/single-object-tracking/.venvs/rknn-convert/bin/python /tmp/nt-background-foreground-screen.py`、`python3 /tmp/nt-detector-background-audit.py`；脚本 SHA-256 `e099dda89f1c2eea91efb2d37ce4cf9bee94b03b2d80df510070554c2b73f58e` / `62b67c7580a4ec4bcad02c08e8e0fad85e9b61a3ff0ec2eb13f9be084b75cb0f`，逐帧 `/tmp/nt-background-foreground-screen.csv` 与摘要 `/tmp/nt-detector-background-audit.json` SHA-256 `83f5933dee8830bcd278af71e569f8a25c07acf07272978d28c56bf1a9e72aa9` / `5d9725b728749dc631bc420ef00224918955f6b17f263ed14ceea0ef01dcce4f`。十目标可见且有真值框的 2502 帧中，PT car 框与真值 IoU≥0.3 为 2461 帧、缺席 41 帧（car004 16、car010 14，其余 8 目标共 11）；这证明检测在该视频上有高覆盖率，但不证明缺席即遮挡。car010 GT 路第 148/149 帧 MOG2 框内前景比例同为 `0.3268`，DET 路 `0.3399→0.3086`；第 149 帧 detector 近框面积与跟踪框比例也仅由第 148 帧约 `0.835→0.820`（GT 路）。所以这两类简单信号不能稳健地完成第 149 帧边界。
- 实际运行 `python3 /tmp/nt-detector-blend-upper-bound.py`、`python3 /tmp/nt-detector-async-blend-screen.py`、`python3 /tmp/nt-detector-offset-ema-screen.py`；脚本 SHA-256 依次 `2cc2dd56f427fae51e3badbd256bd9d6b8a5eb1acfd0953522c390d70d84eedb` / `bf146393f238ee991e05581fa54f17e9e47f50c49db000a6752a22c060e23eb1` / `1203e124cc950bde51132b907f682d50dae17ae5449f0881fe90c8858c5a18fb`；摘要 `/tmp/nt-detector-blend-upper-bound.json`、`/tmp/nt-detector-async-blend-screen.json`、`/tmp/nt-detector-offset-ema-screen.json` SHA-256 `e84a9cc2732e97472914ca4cc81045ab05d1762476deb2f7c8b2f96a574998fd` / `cb719832116b1c45751ed87eeaf447c4d9229b9e2802925f992511c7dea4823d` / `50a978b05710e440af05006631fdbee1b39c9bfd8e207494cfcc10763423b30d`。同帧 PT 框与跟踪框重合时 α=0.5 融合，可见接受帧中心 MAE 在 GT/DET 两路分别 `2.101→1.633px`、`1.959→1.518px`；但 car001/002 两路均变差。按实际板端 detector batch 到达时刻，以源帧 PT 框与源帧跟踪框之差作因果修正，α=0.5 后 MAE `2.101→1.806px`、`1.959→1.668px`，逐帧中心残差速度 RMS `1.042→1.203px`、`0.999→1.181px`；β=0.15 的偏差 EMA 虽把 RMS 增量缩至约 `0.03px`，MAE 改善也仅约 `0.15–0.16px`，仍使 car001/002 变差。模拟没有把校正反馈进跟踪器或控制器，PT 与 RKNN 框也不完全相同。
- **裁决/Handoff**：不把简单 detector 融合或 MOG2 前景门并入当前算法。此类改动虽提高部分可见帧中心精度，却增加帧间抖动或损害既有目标，且不能满足 car010 第 149/169 帧时限。十目标正式 6/10 双初始化板端成绩、`car_004_visible_unboxed_v2` 数据修订请求、main/Debug 和生产 handoff 均不变；后续应寻找可泛化的部分遮挡身份/可见性信息，并用真实 RKNN 异步输出验证中心与连续性。

## 2026-11-18T06:41:07+08:00 冻结可见性模型的 LaSOT 跨数据集筛查

- 分支 `codex/ten-target-smoothness`，工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `6adf4ba3d842871ae00d454a4dca95bc55cfcb19`。实际运行 `/home/u20/single-object-tracking/.venvs/nanotrack/bin/python /tmp/nt-paired-visibility-lasot.py`；脚本 SHA-256 `ac59b4dedd869e814b4481a5eda2047ddcdfa3bd33715887a976fc4854322172`，输出 `/tmp/nt-paired-visibility-screen/lasot-car.json` SHA-256 `8d2df4c7f4460ad86702a75e3cbb201891b0b5926a1d5451ab7e38b2563f6ee1`。冻结 VisDrone 模型 `/tmp/nt-paired-visibility-screen/best.pt` SHA-256 `c6506b81f558190c10927d68158a426e8b0b3efaca9e739ad3ece46450cfe40d`，沿用 VisDrone 验证集阈值 `p_full<0.390323`，未在 LaSOT 或 canonical 数据上调参。
- 输入为独立 `/home/u20/dataset/LaSOT/LaSOTBenchmark/car/car-*` 共 20 个汽车序列；从每段确定性抽最多 150 个无遮挡帧及全部可用的完全遮挡帧，以 3–30 帧前最近清晰真值框为参考，当前裁剪也使用真值框，框宽高至少 20px。共 3000 个无遮挡、196 个完全遮挡样本；可见误报 `164/3000`，完全遮挡检出 `139/196`，AUC `0.9527`。序列差异明显：`car-20` 仅检出 `9/64`，`car-16` 可见误报 `45/150`。LaSOT 部分完全遮挡框是最后位置或无形目标框，裁剪使用真值位置；结果只是离线跨数据集筛查，不等同运行时定位或板端结果。
- **裁决/Handoff**：独立数据说明模型有识别某些完全遮挡的能力，但用户确认的新增 `car_010` 第 149–169 帧为大部分遮挡，第 149 帧必须开始识别且第 169 帧前正确重捕；现有模型在 canonical 第 149 帧失败，不能以 LaSOT 的完全遮挡结果替代该要求。新增 `car_004` 第 42 帧确认可见，继续使用已有 `car_004_visible_unboxed_v2` 反事实版本检查，不把该帧算作有 IoU 真值。暂不接入模型；算法、main/Debug 与生产 handoff 均未改变。后续需要能识别部分遮挡起点且不损失清晰帧的独立证据，再在 RK3588 十目标 GT/DET 双初始化验证。

## 2026-11-18T06:35:19+08:00 疑似期抵扣搜索等待的离线反例

- 分支 `codex/ten-target-smoothness`，工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `c81630bd65d23eeaa808c108ac34d2e0ccc7c047`；只读复用 `8314980` RK3588 十目标 GT/DET 初始全量 CSV 及对应正式真值。实际 `python3 /tmp/nt-recovery-age-credit-screen.py`；脚本 `/tmp/nt-recovery-age-credit-screen.py`、结果 `/tmp/nt-recovery-age-credit-screen.json` SHA-256 `2433b4c25f90043ec51d55ceb58ffaa68ee7c00c14c4584e9c9bcda991725935` / `2621671a5d1098b32207d86e7c1ae274273b7db97a734f782abe5b45aab62326`。假设仅把连续 SUSPECTED 的前两帧抵扣进 400ms 恢复等待，不提前改变 SEARCH 裁剪/运动；在当前搜索龄 333–399ms 的 48 个帧路样本中，正式真值仍有 `18` 个 `full_occlusion`、`4` 个 `partial_occlusion`、`26` 个 `visible`。此为静态风险筛查，没有冒充改后运行结果。
- car010 GT/DET 第 168 帧搜索龄 333ms，正式真值仍标 `full_occlusion`，却有置信度 `0.708496` 的 detector 近框及 Recent/Safe PSR 约 `5.8–6.2`；若抵扣 67ms，当帧可能开始确认。第 169 帧搜索龄 366ms，原始 Recent/Safe 中心相对该帧真值 GT 路分别偏 `15.60/15.10px`、DET 路 `14.06/14.15px`，未贴紧目标。第 170 帧才出现原版首次候选确认，171 帧正确重捕。单改计时既不能保证 169 帧 NanoTrack 的框正确，又会放行正式遮挡区中的高响应近框；本文件前述 333ms 板测已在 car002/003 有早错捕反例。另用 `/tmp/nt-probe-car010-candidate-visibility.py`（SHA-256 `c1deff9c8599db85a419fc8583fd9c21e31cc67fd3d8339ba7e8487f9c32d93f`）检查冻结配对可见性模型：第 168 帧 detector 候选 `p_full≈0.66`，169 帧正确 detector 框 `p_full≈0.60–0.65`，也不能安全地区分两帧。
- **裁决/Handoff**：不单独实施疑似期抵扣搜索等待；要达成用户 car010 第 169 帧既早又准，仍需要当前帧可见性和身份的独立证据，以及在 detector 候选位置由 NanoTrack 给出贴真值的确认。没有新程序、板端结果或正式分数变化；当前算法、main/Debug 和生产 handoff 不变。

## 2026-11-18T06:30:42+08:00 跨序列双帧可见性模型筛查，car010 起点反例否决接入

- 分支 `codex/ten-target-smoothness`，工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `2bf5a65108a70bde6cfe8b67f2d0ac35c39450e1`；本节仅桌面模型训练和只读回放，没有改算法、正式配置、真值或运行板端程序。原始训练源 `/home/u20/dataset/VisDrone2019-MOT/VisDrone2019-MOT-{train,val}/`，汽车 `category=4`、`truncation=0`、有效框宽高 25–90px，按相同目标 3–30 帧前最近的 `occlusion=0` 框与当前框配对；两个图块各带 1.4 倍上下文裁为 `64×64`，训练/验证严格使用数据集原 train/val 序列（采样后 44/6 个序列，序列名交集 0），`occ_video_1` 不参与训练、选 epoch 或定阈值。实际 `/home/u20/single-object-tracking/.venvs/nanotrack/bin/python /tmp/nt-paired-visibility-screen.py --epochs 5 > /tmp/nt-paired-visibility-screen.log 2>&1`；脚本 SHA-256 `9f73ae729c9383055210c23385b0df2ad569e74cd907d7ad5556cf768e66bc9d`，训练/验证裁剪缓存 `/tmp/nt-paired-visibility-screen/{train,val}.npz` SHA-256 `1ad9c6b77ae26ca6eb3a62ad2530d6b7e3b123f1e71908fa836fdf54fe517e97` / `81a44caade11b045e193a885d8d02654dfbd3c66e04172f5b239dce2051dff08`，采样清单 `/tmp/nt-paired-visibility-screen/{train,val}-index.json` SHA-256 `7f49c55ac1be7c294de86b5e866f12fc3692a2c342fd8e87de91a0e6a51c2a64` / `9a1f855da196931f7502fa7f488425d19c3694f41aaf8572c47defa524776f5d`，训练日志 SHA-256 `fbe9ca4307fb0658c10fe6584f88a10e1aba2c020902e2edfe770f3f8e2a38d5`。
- 训练实际选 3000 无遮挡、2000 部分遮挡、500 重遮挡配对，验证 2000/400/50；四层小 CNN 输入两帧 RGB，固定种子 `20261118`，CPU PyTorch `1.7.1`。按独立验证集的完整可见 vs 任意遮挡 AUC 选第 3 epoch，`0.8845`（部分 `0.8932`，重遮挡 `0.8149`），三类准确率 `0.8514`；模型 `/tmp/nt-paired-visibility-screen/best.pt` SHA-256 `c6506b81f558190c10927d68158a426e8b0b3efaca9e739ad3ece46450cfe40d`，报告 `/tmp/nt-paired-visibility-screen/report.json` SHA-256 `750b1c8faefaa2ed8351f977fca7c5b47e82287118b45c7449966d466d38ac23`。这是桌面外部分布指标，不证明 RK3588 可运行或当前遮挡起点有效。
- 模型冻结后，对同一 `8314980` 板端 20 路 CSV 和 canonical `1.mp4`（SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`）分别用 3、8、16 帧前**已知真值可见**的跟踪框作参考，只用于离线验证；运行时没有真值，此结果不能证明实际参考帧一定清晰。验证集完整可见样本第 5 百分位固定 `p_full<0.390323` 为遮挡（验证误报 5%，部分遮挡检出 57%，重遮挡 42%），不按 canonical 调阈值。分别实际运行 `/home/u20/single-object-tracking/.venvs/nanotrack/bin/python` 加 `/tmp/nt-score-paired-visibility-canonical.py`、`/tmp/nt-score-paired-visibility-canonical-lag3.py`、`/tmp/nt-score-paired-visibility-canonical-lag8.py`，脚本 SHA-256 依次 `f766878b3ad5e8b91beed0e15544d751ebe11f508caa3fb542519bb748cd6ad5` / `31e723e162dc86cee9d3fbb539f78451de99f80259bf2769898c1f8a085623bc` / `0344ad30e433e98186006ee07230b2f7e2e3588461e1fcf42afbb47e52da8fd3`；报告 `/tmp/nt-paired-visibility-screen/canonical-report.json`、`canonical-report-lag3.json`、`canonical-report-lag8.json` SHA-256 `518678c4f89ae1672459f9fec19bd8e6a33c94672ce87ba65d59592976f324fc` / `5460b27fe1437b9c43e19537fb3a31497a860840b205b1b7bd7cbd313e99fc5f` / `61ee0515034f885c7a1efd304132023a9cb1ee4f03ddbecb53ffbab68b7c9394`；逐帧 `canonical-scores.csv`、`canonical-scores-lag3.csv`、`canonical-scores-lag8.csv` SHA-256 `33f2fca3b4774995aa20489824318fd3d24f3b56a87188785c3acc0b706395c6` / `799066fbb02704633c2fc2bc8e4c2f49ca04800c99dd869da21dedf5b163388d` / `99f8cba939d98fb1873f90bfbf5f6fc265e7467e69bc2b533eff810924a6893a`，均在同目录。固定阈值在 3/8/16 帧参考下分别误报当前正式可见且已接受帧 `57/4582`、`193/4421`、`264/4152`；**car010 第 149 帧 GT/DET 两路三个参考间隔均未报遮挡**，`p_full` 依次为 3 帧 `0.926/0.943`、8 帧 `0.695/0.763`、16 帧 `0.757/0.794`。若在 8/16 帧参考下硬把阈值放宽到覆盖双路该帧，会分别误报 `987/4421`、`1327/4152` 个可见已接受帧。car004/003/009 的部分起点有检出，但无法补足用户要求的 car010 第 149 帧。
- **裁决/Handoff**：该外部跨序列模型比先前单帧 VisDrone CNN 有更高验证 AUC，但 canonical 关键反例不通过，且参考清晰帧在真实运行时不可由真值保证；不导出 ONNX/RKNN，不接入 tracker、display、重捕或控制，不用 canonical 调参。保留模型和逐帧离线报告供模型/外观 owner 研究更独立的实例可见性与身份信息；正式十目标双初始化 6/10、main/Debug 和生产 handoff 不变。

## 2026-11-18T06:16:36+08:00 十目标 SUSPECTED 起点普查否决一帧即搜索

- 分支 `codex/ten-target-smoothness`，工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `5d691926c3c06e4e2faa73e65b98ae6e0b2fd180`。只读复用精确 `8314980` RK3588 十目标 GT/detector 双初始化完整 20 路 `/tmp/nt-search-output-8314980-full/runs/output-{gt,det}-full/car_###/frames.csv` 与每个目标现行正式真值，逐路列举从非 SUSPECTED 首次进入 SUSPECTED 的帧。实际 `python3 /tmp/nt-suspected-onset-census.py`；脚本及结果 `/tmp/nt-suspected-onset-census.json` SHA-256 `7761539f23acde9fd7ad358e78430aefd2044dbe043f3b3df1f8b1a3ee500a1d5ce69329882dae1bbd62b` / `fa42f6724aeb92ad123e58caa341952f1a3ee500a1d5ce69329882dae1bbd62b`。未运行新板端程序、未修改算法或正式数据。
- 共 57 次起点：按现行真值，53 次在 `visible`，4 次在 `full_occlusion`；53 次可见起点中 34 次恢复 `TRACKING`、19 次随后进入 `SEARCHING`。已通过目标也有长的可见可疑段，例如 car005 GT/DET 第 120 帧起 5 帧、car006 GT 第 120 帧起 9 帧、car007 GT/DET 第 120 帧起 5 帧、car010 GT/DET 第 120 帧起 8 帧。因此“首次 SUSPECTED 即直接 SEARCHING”将把大量真值可见帧送入搜索，不能当作跨目标通用的遮挡提前策略。此为离线状态普查，没有假定改策略后的后续轨迹相同。
- 真值标为全遮挡的四次起点分别为 car004 GT 第 132 帧、car009 GT 第 115 帧、car010 GT/DET 第 156 帧；其中 car009 GT 下一帧又接受桥区响应并回到 TRACKING。car003 第 130 帧此前已从 124 帧开始 SUSPECTED，不属于新起点；car010 用户要求的第 149 帧当前仍为 TRACKING，也不可能靠“首次进入 SUSPECTED 后缩短等待”来满足。**Handoff**：继续保持 car004 第 42 帧可见修订与 car010 149/169 人工时限；后续候选需有独立可见性或身份信息，不能全局压缩疑似时长。当前正式双初始化 6/10、main/Debug 与生产 handoff 不变。

## 2026-11-18T06:13:11+08:00 Safe 模板常态探针板测否决，实验源码撤回

- 实验源 `codex/ten-target-smoothness@97e7a424a2eb34698a54d559f68db99bdb46055e`；主机 `/tmp/nt-safe-probe-97e7a42.tar` 与板端同名源包 SHA-256 均 `15e03b0f0c5db58ff2c6611b4b5cde1e5d5d5488713ea4b7a92395b27ffcb521`，板端隔离目录 `/home/neardi/Project/SOT/nt-safe-probe-97e7a42`。实际运行 `bash /home/neardi/Project/SOT/nt-safe-probe-97e7a42-setup.sh`，脚本 SHA-256 `619534bba5d0ab804dc0a80508088f3698a32f375ede4381948565366187d871`；RK3588 原生 CMake/build/CTest **23/23 PASS**，程序 SHA-256 `4b1880762f4901a7f99acea0079499499b5c1452ce16a35fb9f62211ca03efa6`，运行配置 `runs/headless-on.conf` SHA-256 `16c9fd59ab38900ed2c16205ce4fef681743962d6f85c5561e963233f75335a5`，controller 配置 SHA-256 `bb19d2e8e085dd57084653b6ca37bb981b10ea2a0d00bc05d166f8a508c6e26f`，`cmake.log`/`build.log`/`ctest.log` SHA-256 `f0f974ffb6d5611b30a8037761d2d3f32a1837e034d390c5d7446556224262ad` / `e1aa1699935db8505a200f449956c93c67d787e246a735c9842cc5b3ba87da07` / `7dfe5972567120c8c8e460371122d7735edded2626766321a9b12babf283643d`。实际输入视频 SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；NanoTrack manifest/三个 RKNN SHA-256 `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261` / `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55b` / `d3cb0e3879df6a1272adb59bf501324f1f8f55b58309d3c4da4a731b600973ee` / `4da379f54093d79f02775e90bfea33ebee0837c6fc73a324499279b4c57807c5`；VisDrone9 detector RKNN/manifest/labels SHA-256 `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c` / `ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9` / `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`；日志 RKNN API `2.1.0`、driver `0.9.8`。
- 实际运行 `bash /home/neardi/Project/SOT/nt-safe-probe-97e7a42-focused.sh`（脚本 SHA-256 `bc5bd76df1f4ac66857bd8ebd42cf002b50d3959520438e551442f4aaa844ce2`），用原 GT/detector 首帧框分别跑 car010、car009，两者 GT/DET 双初始化，再跑已通过 car002 GT，共五路，每路 271 帧。准确 `--bbox`、命令和 `NT_TRACKING_SAFE_PROBE=1` 在脚本；原始 `frames.csv`、`session.json`、`run.log` 保留板端 `runs/safe-{gt,det}-car_###/`。板端 `/home/neardi/Project/SOT/nt-safe-probe-97e7a42-focused.tar.gz` 与主机 `/tmp/nt-safe-probe-97e7a42-focused.tar.gz` SHA-256 均 `c0eb2e51f42d34a3e5fc9a7c619ab960887a0e09787f013ddcaeecd51c47e54f`；主机解包 `/tmp/nt-safe-probe-97e7a42-focused/`。正式 `evaluate_occ_video_1.py --scope full` 逐路评分脚本 `/tmp/nt-score-safe-probe.py` SHA-256 `01eb318163a2ae1249889ced286702a5bca602a5527305b33efbb5a42d404be9`，摘要 `/tmp/nt-safe-probe-97e7a42-focused-summary.json` SHA-256 `c72fe2ec3d6b2d11b867c860880e4189bfe022ed8cd5ec9da2dd0b9d441aa491`；五路相对 `8314980` 的全部评分门相同：car010 GT/DET **FAIL**、均 171 帧重捕；car009 GT/DET **FAIL**、仍无重捕；car002 GT **PASS**、228 帧重捕。此诊断没有扩大到 20 路，因为起点识别信号已被否决。
- 分析脚本 `/tmp/nt-analyze-safe-probe.py`、结果 `/tmp/nt-safe-probe-97e7a42-analysis.json` SHA-256 `9b47b5a4bfd02d5ebe4ec32a98834da08ee4827417557ce9176563db02a462f8` / `14e305d9eb956a461cbf8203b230050dd49dd09c95d8864bdda028642ed66985`。car010 第 149 帧 Recent/Safe PSR：GT `8.175/8.168`、DET `8.182/8.033`，中心距仅 `0.019/0.009` 个 Recent 框对角线；car009 第 115 帧 GT `5.903/5.943`、DET `7.879/7.756`，中心距 `0.016/0.025`。Safe 与 Recent 是不同模板（`safe_reused_recent=0`），仍一致指向桥区，故双模板一致**不能证明目标可见**。附加 Safe head 使五路平均 tracker 耗时增加约 `0.69–1.04ms`；五路 tracking phase、accepted、reacquired、模板动作逐帧不变，晚期三处发布中心有 `>0.05px` 时序差异，不影响门判定。
- **裁决/撤回**：不将常态 Safe 探针或基于其一致性的遮挡门加入正式算法。撤回后 `git diff 9a62d0246486b356ef5ef3f5058f6b3193544ac3 -- rk3566/nanotrack_v3` 输出为空，恢复到上轮可信算法；主机重跑 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure`，**23/23 PASS**，日志 `/tmp/nt-safe-probe-revert-build.log` / `/tmp/nt-safe-probe-revert-ctest.log` SHA-256 `d021a7a1e517a30d7741e1390359a20ddbdc3a10c0b50a2723c9771d61804f6c` / `eed8e399b80605537e5e87aa9d92c22ab407053edb4d2025302a2569f17d3b23`。Handoff：需要真正独立的可见性或目标身份信息；car010 149/169 时限仍未达。十目标正式 6/10 双初始化基线、main/Debug 与生产 handoff 不变。

## 2026-11-18T05:35:29+08:00 运动对齐外观筛查否决与 Safe 模板独立探针候选

- 分支 `codex/ten-target-smoothness`，工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `9a62d0246486b356ef5ef3f5058f6b3193544ac3`。只读复用 `8314980` RK3588 20 路全长 CSV、十目标正式状态和同一 SHA-256 为 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d` 的视频。实际执行 `/home/u20/single-object-tracking/.venvs/rknn-convert/bin/python /tmp/nt-motion-compensated-appearance-screen.py`；脚本、逐帧结果 `/tmp/nt-motion-compensated-appearance-screen.csv` SHA-256 `642ae97abea9280b70903b284c8abc02de50c3d42d33d45d04fcf5e0e4132c` / `c2b0112e81414dcc7812656aa12583a01e51c94143cd73a7c56e647f4c22124f`。用每路跟踪框裁剪并统一到 48×48，分别与 3/8/16 帧前同一路框做灰度 NCC、颜色 MAD、边缘余弦相似度，覆盖全部 271 帧和 GT/detector 双初始化。
- 要让 car003/004/009/010 四个遮挡起点都触发，16 帧前 NCC 阈值需 `<=0.522764`，在 `4342` 个当前正式标为可见且被接受的帧中误触发 `609`；同时要求颜色 MAD `>=0.089928` 与边缘余弦 `<=0.267964`，仍误触发 `559/4342`，遍布十目标。3/8 帧前门的误触发更多。**否决此固定外观阈值**，不把同视频拟合门加入运行时；裁剪对齐不等于实例分割，不能从高相似度推断目标完整可见。
- 为测试已有 Safe 模板是否提供独立证据，实验源码在设置 `NT_TRACKING_SAFE_PROBE=1` 时，仅对普通 TRACKING 的 Recent-only 帧多运行 Safe head，把 Safe 观测从运行时/控制器输入中移除，结束后仅放回诊断日志；默认无环境变量时行为不变。此探针不修改搜索、重捕、控制判定；附加计算可能改变异步 detector 时序，须用逐帧输出核对边界。实际主机 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure`，**23/23 PASS**；日志 `/tmp/nt-safe-probe-host-build.log`、`/tmp/nt-safe-probe-host-ctest.log` SHA-256 `e47329f5dda6c15c9feadd76935e32b35b9eec08bc803859d3bfc41c71208ec5` / `8dbc4139d848bbde9fb4ec6ec4145f35fc7114c02393a276b92fd69efe10e1fd`，`git diff --check` PASS。这只是可观测性候选，**无板端或改善结论**。下一步冻结本提交并在 RK3588 隔离原生构建，先测 car010/009 和已通过目标的 Safe/Recent 差异及运行时间；若信号不能区分遮挡起点与可见帧，撤回探针。正式 6/10、main/Debug 和生产 handoff 均不变。

## 2026-11-18T05:28:29+08:00 四处失败区间逐帧图像核对与用户遮挡语义边界

- 分支 `codex/ten-target-smoothness`，工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `a097d158321780437ca40f441790b4ac673a6ed5`；本节仅核对既有 `8314980d4ce293ee49ec132465677895acc219aa` RK3588 20 路板测原始 CSV 和同一视频，没有改算法、配置、正式标注或重新声称板测。输入视频 `/home/u20/single-object-tracking/data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；板测原始包 `/tmp/nt-search-output-8314980-full.tar.gz` SHA-256 `bf9cb97661d1a1cbc18dea566a69debb22b75f2474a24df68da4f6d9e240e3b4`。实际运行 `/home/u20/single-object-tracking/.venvs/rknn-convert/bin/python /tmp/nt-failure-contact-sheet.py`，脚本 SHA-256 `41e8c6e2b34cd92869c4d88975a70e1f9ab20356618be258528055e894a041ac`。
- 对照图 `/tmp/nt-failure-audit-{car_003,car_004,car_009,car_010}.jpg` SHA-256 依次为 `6d79dc0fddc223cf5b5779590e7633ac9ac4fb1457fcf4cf49756b27993b2cf9`、`d63e1e0afcc62f5d87c9bfd732773ed8d8819bbb6228b048313be1184e01323b`、`7f4b8941cd361d7fe3a461290eaca442276c274c128620154ba0aad6c4e8d7c6`、`a13eab504ffb330863eba4dd26b2d7fbd9022c0e3ed551dc00bd10210b2f83a8`。红框为 GT 初始化路算法发布框；绿框为正式标注的可见框；图像及 CSV 帧号直接对齐。car003 第 130 帧已无完整车身，算法仍 `SUSPECTED/accepted=0`，第 131 帧才 `SEARCHING`，属于状态晚一帧。car004 第 127–129 帧绿框逐步只包住桥上可见部分，红框仍近似整车；第 131 帧重新 `TRACKING/accepted=1`，第 133 帧才搜索。car009 第 115 帧目标被桥遮挡，GT 初始进入 `SUSPECTED/accepted=0`，第 116–120 帧又接受桥区响应；detector 初始在 115–120 帧始终 `TRACKING/accepted=1`。这两路均非正确的遮挡保持。
- 用户明确确认**今天新增第 2 目标 car_004 第 42 帧可见**，以及 **car_010 第 149–169 帧大部分被遮挡，遮挡判定不晚于 149、正确重捕不晚于 169**。car004 第 42 帧的版本化可见修订见下文 `car_004_visible_unboxed_v2`，不能把旧正式标注的该帧 FAIL 当算法失败。car010 对照图中第 149–155 帧仍有部分黄色车身，第 156–168 帧车身从该视角不可辨，第 169 帧又出现；正式包把 149–168 全写成 `full_occlusion`，其中至少 149–155 与“完全不可见”不符。此视觉核对不生成未经人工逐帧确认的精确框或替换正式评分。现有 GT/DET 两路第 149 帧均 `TRACKING/accepted=1`，到 156 帧才 `SUSPECTED/accepted=0`，158 帧 `SEARCHING`，169 帧 `SEARCH_MIN_AGE`（366ms），170 帧 `AWAITING_CONFIRMATION`，171 帧正确重捕。因而用户两项时限均未满足；第 149 帧高 PSR（GT `8.175270`、DET `8.181628`）和低归一化中心残差（GT `0.042224`）解释现有响应门为何未提前报遮挡，不能据此证明真实可见。
- **Handoff**：协调对话需把 car004 第 42 帧修订和 car010 149/169 两个时限纳入版本化评测契约；car010 的可见部分与完全不可见子区间需逐帧审定状态，原版保留。下一算法候选需要独立的遮挡/身份信息，并同时复核 car003/004/009 的桥区反例与 20 路 GT/DET 初始化；当前正式成绩仍为两路各 6/10，不提升新基线、main/Debug 或生产 handoff。

## 2026-11-18T05:24:15+08:00 短时帧差与历史背景差筛查否决

- 起点 `codex/ten-target-smoothness@a5b225a5b4ce2afab9646bf87a194414d304d255`，工作树干净；只读复用 `8314980` 板端 20 路 CSV、对应目标现行真值状态和同一 SHA-256 为 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d` 的视频。脚本 `/tmp/nt-temporal-foreground-screen.py`、逐路逐帧结果 `/tmp/nt-temporal-foreground-screen.csv` SHA-256 `28c56534919d753bdf11d4bcd00f9079b2264080dd10360d17a66fb119f4f0b2` / `231feafb618c780c26bc39ae1278323116999a9a8688b551bb05586de82e3acd`；每个当前跟踪框按相同像素位置比较 3/8/16 帧前以及 12/16/20 帧历史中位背景，共 5420 个帧路样本。
- 以“框内灰度/颜色差异较少”为遮挡信号，3 帧前 `>8` 色差像素占比的阈值若覆盖 car009 第 115 帧、car010 第 149 帧双初始化，需放宽到 `<=0.658`，会在 `1965/4601` 个现标可见接受帧触发；8 帧前阈值 `<=0.825` 则为 `2762/4501`；历史背景的 `>16` 色差占比阈值 `<=0.667` 为 `1687/4262`。car010 GT 第 148/149 帧 8 帧差占比 `0.844/0.825`，detector 初始 `0.791/0.778`，起点变化很小。**否决未经补偿的帧差/背景差单阈值**，不加入运行时；此结果不排除更完整的相机运动补偿或实例分割模型，且当前目标在第 149 帧仍部分可见。car010 149/169 时限和正式 6/10 双初始化结果未改善，保持实验算法与生产 handoff 不变。

## 2026-11-18T05:22:08+08:00 可见帧中心平滑筛查，收益不足以加入运行时

- 起点 `codex/ten-target-smoothness@5ddf4d989287e3ab639d317cdf926a6afe72721c`，工作树干净；本节只读，对已保存的 `8314980` RK3588 20 路全长 CSV 与各自正式可观测真值比较。脚本 `/tmp/nt-visible-center-filter-screen.py`、结果 `/tmp/nt-visible-center-filter-screen.json` SHA-256 `696b75092086aab42169b89f717068bfde5b6c1dc89f680d6c16b295a520865b` / `04d134f12e525d3d4062c559c23f8ed9a8b305e38dfc7b39248ca2a005ab0288`；只对 accepted/Tracking 帧做因果中心 EMA，非接受或重捕时复位。原始每路平均中心误差的跨 20 路均值 `2.0591px`、平均 IoU `0.8327`。`alpha=0.85` 后误差 `2.1250px`、17/20 路变差；`alpha=0.70` 后 `2.3297px`、20/20 路变差。可见中心不能靠固定低通变得更贴真值。
- 进一步筛查因果 alpha-beta 速度补偿，脚本 `/tmp/nt-visible-alpha-beta-screen.py`、结果 `/tmp/nt-visible-alpha-beta-screen.json` SHA-256 `44def7ef5c65e93521b60f803f0030ebe8da38e785e6a5a657feb6e3f9ec1f90` / `fc5667ccd34e24270c425c30b286b8d46622bc258111d596067d2759028bc973`。30 组参数中同数据最优 `alpha=0.95,beta=0.4` 的跨路平均误差 `2.0556px`、IoU 仍约 `0.8327`、P95 逐帧误差增量 `1.838→1.786px`；中心误差改善仅 `0.0035px`，且参数也在这同一段视频上筛选，不能当泛化收益。**否决**新增可见帧滤波状态；保留当前可见中心发布路径和已板测的无观测 SEARCH 发布平滑。下一步继续研究有独立身份/可见性证据的遮挡与重捕，不能为了轨迹外观牺牲真值中心。

## 2026-11-18T05:19:10+08:00 十目标局部光流反例与 car004 修订真值复评

- 起点 `codex/ten-target-smoothness@1cd521a01f105569ca4fba0546038ef466eb361e`，工作树干净；本节只读分析、未改算法或正式数据。对同一板端 20 路 `/tmp/nt-search-output-8314980-full/runs/output-{gt,det}-full/car_###/frames.csv` 与同一视频 `1.mp4` 逐帧做局部 LK 特征光流：脚本 `/tmp/nt-local-flow-all-screen.py`、结果 `/tmp/nt-local-flow-all-screen.csv` SHA-256 `bc1dbdb7b9930d5b9630ff507bfca3971df3e3c812d04241e5f8d5faa6f60ed0` / `5c2b218a55d42b8620d5f291c5ee1e0e7eaf415f9d7d79a649d5bb792047b16e`，共 5396 个相邻帧路样本，其中 4640 个现标可见且被接受。要让两路 car010 第 149 帧都触发，“框内位移 <1px 特征比例”门槛须不高于 `0.486`，在可见接受帧会误触发 `1927/4640`；按光流与输出框位移的中位残差，门槛须不高于 `0.262px`，会误触发 `4269/4640`。car009 第 115 帧有较明显残差，但 detector 初始仅 `1.181px`，也不足以与可见帧稳定分开。光流受框内桥体/车身混合、可见帧静止纹理及框自身抖动影响，**否决单阈值加入运行时**；此为离线启发式筛查，不冒充板端算法候选。
- 用已版本化的 `car_004_visible_unboxed_v2`（第 42 帧用户确认可见、无精确框；`checksums.sha256` SHA-256 `c8c394b41c2cb489f33a1b7e631cd1e9776cd9d75c15a5860ec221e2ddda1126`）和只读适配评测器 `/tmp/nt-eval-car004-visible-unboxed-v2.py` SHA-256 `6984f068350ffbb1398cb5e869a7ec679ced5fe008c11d58e3ba709e1cef9b16`，对 **同一** `8314980` 板端全量 car004 GT/DET CSV 反事实复评：GT 仍 FAIL（131–132 完全遮挡、127–129 可见框）；DET PASS。报告 `/tmp/nt-search-output-car004-{gt,det}-corrected.json` SHA-256 `47643808bff0a1174f9e3f828ef4966b173e2c4f45fc72e22fa2ff7b1b2848c7` / `68d2e37ed25db8aa65351a585299d5531fdf7acbf8e78c79e2e7ee8ed4a3a430`。正式 canonical 的 6/10 双初始化成绩不变；若协调契约将该修订真值纳入，detector 初始化可按新版本重新统计为 7/10，GT 仍 6/10。**Handoff**：请协调对话版本化采纳第 42 帧修订，保留旧标注可追溯；car004 126–129 框几何语义尚未获用户确认。car010 149/169 时限仍需新的独立可见性/身份证据，不能用已否决的单阈值替代。

## 2026-11-18T05:13:28+08:00 发布层平滑完成十目标双初始化板测，改善连续性但未解决遮挡

- 候选 `codex/ten-target-smoothness@8314980d4ce293ee49ec132465677895acc219aa`；源码归档主机 `/tmp/nt-search-output-8314980.tar` 与板端同名包 SHA-256 均 `e420da1aabe20bdd66eb01682d2e4c2b460f27451adcd7d0f658958987918c3d`，隔离板端根 `/home/neardi/Project/SOT/nt-search-output-8314980`。实际 `bash /home/neardi/Project/SOT/nt-search-output-setup.sh`，脚本 SHA-256 `940a86a419cb8ec98d8bbfbfe934487017eb40f36f41b62611591ab4d344647f`，RK3588 原生 CMake/build/CTest **23/23 PASS**；程序 SHA-256 `2b61916d1e8763aac09c1af08000f31564cc1d5ca3f4063f674ac47a45fefd6f`，controller/运行配置 SHA-256 `bb19d2e8e085dd57084653b6ca37bb981b10ea2a0d00bc05d166f8a508c6e26f` / `92e0ccc338367e58d2ce326bf784aa5e7b8a049d1dc98648daf0860b217e26bc`；`cmake.log`/`build.log`/`ctest.log` SHA-256 `25f3c76a723dad728a9b58a999dd32c00a9518d51c75bff2db19b8804d78b332` / `7bb043ca9eac705c4188f628904af673a9190251e86f28b7d943fe1c151b7eae` / `8b2c7bdf6f21a4f09b2499a0f1cf3ed982b6e64a08f27e3cdf4f38b4dda2a411`。
- 同板输入复核：`1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；NanoTrack manifest/三个 RKNN SHA-256 `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261` / `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55b` / `d3cb0e3879df6a1272adb59bf501324f1f8f55b58309d3c4da4a731b600973ee` / `4da379f54093d79f02775e90bfea33ebee0837c6fc73a324499279b4c57807c5`；VisDrone9 detector RKNN/manifest/labels SHA-256 `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c` / `ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9` / `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`；run.log 记录 RKNN API `2.1.0`、driver `0.9.8`。视频、模型和配置的准确绝对路径见板端 `runs/headless-on.conf` 和各路 `session.json`。
- 实际先运行 `bash /home/neardi/Project/SOT/nt-search-output-focused.sh`（SHA-256 `d6c1f3262be054c967fd40f2749f23b831a1cb15600dc260ab879063a796142f`）五路，后运行 `bash /home/neardi/Project/SOT/nt-search-output-full-gt.sh` 与 `bash /home/neardi/Project/SOT/nt-search-output-full-det.sh`（SHA-256 `30685b164dedb06022eb0d8600a45dc05f5e5c13a6162bbee21d056f663bdf19` / `7ff5f0770d044fd331cdf23579ded555a177a910fcd68b58b56959d93178b8fe`）十目标真值/检测初始框共 20 路，每路 271 帧；准确 CLI/初框见脚本。原始 CSV、session、run.log 板端 `runs/output-{gt,det}-full/car_###/`。板端 `/home/neardi/Project/SOT/nt-search-output-8314980-full.tar.gz` 与主机 `/tmp/nt-search-output-8314980-full.tar.gz` SHA-256 均 `bf9cb97661d1a1cbc18dea566a69debb22b75f2474a24df68da4f6d9e240e3b4`；五路定向包 SHA-256 `1b6d025d474b18170cdece749efbce693219ceb5d5f430928dd0f27e7967a0e1`。正式 `evaluate_occ_video_1.py --scope full` 评分脚本 `/tmp/nt-score-search-output-full.py` SHA-256 `1a862da197fa0649099060d10e1a487ac305c7e06e302c00dd6167abaf3daeb0`，20 份 score/CSV 哈希及逐路结果摘要 `/tmp/nt-search-output-8314980-summary.json` SHA-256 `6f90696ecfc8856b7cd3c68ad7e4e91c8665c416db96f405770345a174775822`。
- 正式结果 GT **6/10 PASS**、detector **6/10 PASS**，通过目标均为 001/002/005/006/007/008。与收窄锚点同范围对照，只在 car004 GT 133–145 帧有 `SEARCH_OUTPUT_SMOOTHED`，预测连续性 FAIL→PASS，133 帧发布 `(720.37,891.22)`、内部运动预测仍 `(710.37,899.12)`；130–147 帧运动预测、候选和恢复字段逐帧与收窄锚点相同，第 146 帧仍正确重捕。car004 GT 仍因 127–129 可见框和 131–132 完全遮挡门 FAIL，detector 初始仍因第 42 帧正式真值错误 FAIL。car006 GT 这次 199 帧遮挡门 PASS，而收窄锚点单次 FAIL、其自身重复 PASS，属于已观察到的异步完成时机波动，不能归因于发布层平滑。car009、car010 仍 FAIL；car010 149 帧接受、171 帧才正确重捕，用户 149/169 时限未达。**Handoff**：此候选仅改善一处无观测预测连续性且未造成 20 路新回退，可保留为实验分支；尚不足以作为新十目标基线，不合 main/Debug、不批准生产 handoff。下一步须找跨目标可靠的遮挡起点证据与不早错捕的恢复证据，继续 20 路双初始化板端闭环。

## 2026-11-18T04:54:44+08:00 搜索输出与内部裁剪分离候选，主机通过待板端

- 分支 `codex/ten-target-smoothness`，工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `25492427b9121304d3760d262670f41711a0f355`。针对上版“移动内部搜索位置会错捕”的反例，只在冻结运动首次进入 SEARCH 且对外框与独立运动预测相差超过 0.25 个预测框对角线时，对发布的 tracking/control 中心以 `search_output_smoothing_alpha=0.50` 向真实运动预测收敛；内部运动状态、NanoTrack 裁剪、detector 仲裁及恢复证据均不改。默认 `0` 关闭，实验可见光配置开启，发布帧 `note=SEARCH_OUTPUT_SMOOTHED`，配置身份在启用时改变。
- 先用现有收窄锚点 20 路原始 CSV 做只读反事实，脚本 `/tmp/nt-search-output-decouple-screen.py` SHA-256 `45e9bbf6498f5d667d39727e903b767a6b24f1913d01b22ad074161d33422f2a`，摘要 `/tmp/nt-search-output-decouple-screen/summary.json` SHA-256 `6c5efa999bb1fa60918e26751542cbfb30c0be5510734dbb794db905f8e40e09`。只修改 car004 GT 133–145 帧发布中心，正式 evaluator 的预测连续性从 FAIL 转 PASS，最大搜索速度 `758.178→379.089` px/s、加速度 `21984.260→10573.454` px/s²；其余 19 路与其他评分门不变。反事实不证明真实运行时或板端效果。
- 实际主机 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure`，**23/23 PASS**；日志 `/tmp/nt-search-output-host-build.log` / `/tmp/nt-search-output-host-ctest.log` SHA-256 `418f1334c8225b3cf995720d04e37737a96d527bd5c96989db205371b315f19d` / `36408e2b533d426823c5c29f5884975de62caef6361a287c5471077bfe0fbaf8`，`git diff --check` PASS。此为未晋级候选；下一步在 RK3588 隔离原生构建，先测 car004 GT/DET 与已通过目标并核对 car004 146 帧身份；若无回退再跑 20 路。car010 149/169 时限仍未解决，不升级 main/Debug 或生产 handoff。

## 2026-11-18T04:45:36+08:00 搜索起点重置造成错捕，板端候选否决并恢复源码

- 候选 `codex/ten-target-smoothness@179179bdc1c23617f2f7b50d8e4888e4063088d9`，源码包主机 `/tmp/nt-search-rebase-179179b.tar`、板端同名包 SHA-256 均 `dc29ab020fd624e38f9a3e0f55041ef11caef6f11a58d764609136fbce999baf`。隔离板端根 `/home/neardi/Project/SOT/nt-search-rebase-179179b`，实际 `bash /home/neardi/Project/SOT/nt-search-rebase-setup.sh`（脚本 SHA-256 `447e0d703594bdd46544069e496ee62945d469e94d9fc8f259d51c14877e0cdd`）原生 CMake/build/CTest **23/23 PASS**。程序 SHA-256 `2ea44cb13948dfba8e50b4cd611522dd43bd5c823b18621ed25fd5f80376380c`，controller/运行配置 SHA-256 `a94d1097b23e569ce71a7047590a7d3b67c57cce35689b83bb66bac4a5e0f9ac` / `6fc675b2f09953bf308fe44dc809bd4a8e874b5bdea611badd6a2c4bde3d2120`；`cmake.log`/`build.log`/`ctest.log` SHA-256 `c07d864c2474ead9f994270e34b1fb1a88afdd575f45fbd91757265b598db7e2` / `ab11b25a5283b755c0d5a3c9054ce2696e2913a83643383897109ea146aa735a` / `1b21f0720a35fbefc8729480c64f35c4a84cbb700ad75254b7a3f256bcc46db0`。视频、tracker/detector 模型及 manifest/labels、RKNN API `2.1.0`/driver `0.9.8` 沿用前次已核验身份。
- 实际运行 `bash /home/neardi/Project/SOT/nt-search-rebase-focused.sh`（主机脚本 SHA-256 `e0527eacc01ffc8b7b6f46f3e66aa313c1b2fdbfdfcf6cc017d6725f5a43ee45`），car004 GT/DET、car002 GT、car009 GT、car010 GT 各 271 帧，准确初始框和 CLI 写在脚本。板端原始目录 `runs/focused-{gt,det}-car_###/`；板端 `/home/neardi/Project/SOT/nt-search-rebase-179179b-focused.tar.gz`、主机 `/tmp/nt-search-rebase-179179b-focused.tar.gz` SHA-256 均 `b2b53b956dad11d6b4af77f84c615d9ea17ca97e453e49ee0f72571288859af6`。用正式 evaluator `--scope full` 逐路评分，脚本 `/tmp/nt-score-search-rebase-focused.py` SHA-256 `2295a76ff159f41d4b06229b3c7ab9cb1ed5a5626b70a91fb242ee6ec9022e4a`，完整报告和逐帧 CSV 在 `/tmp/nt-search-rebase-179179b-focused/runs/focused-{gt,det}-car_###/`。五份 `score.json` SHA-256 按前述顺序为 `fb6928eed1ec4ee98cfb2576d934b96cbf9622fe9760f638f206e979f9b2f1d8` / `93c8c222aa0426c7f1bda5c2c7539656f42eae0c54bb94c8de6358b366d22739` / `0d339878ddf9d6dec59908de0d3a4fbf7b27948942207ad803467de6639eb7d3` / `770dcd4c8894f79f2c696f9d23be04f97b8ce99383bf0f40188affc2be654d06` / `bef85ab41f1002c3e87bc3df64ed075cae81fd4ac176a1be86b07ad70286d97a`。
- car004 GT 133 帧确实触发 `SEARCH_REANCHORED_LAST_TRUSTED`，搜索输出速度尖峰从 `758.18` 降到约 `61.03` px/s，预测连续性门转 PASS；但搜索裁剪也随重置移向受遮挡观测，旧收窄锚点 146 帧正确重捕（IoU `0.8205`）变为 158 帧错误重捕（IoU `0.3062`），新增重捕声明、重现与大量可见框失败，有效观测平均 IoU `0.8262→0.7124`。car004 DET 仍仅有正式真值第 42 帧误标造成的遮挡门失败；car002 GT 保持 PASS、228 帧重捕；car009 GT 与 car010 GT 仍 FAIL，car010 149 帧仍接受、171 帧才重捕。**候选否决**：搜索位置不能仅因最近观测更新就覆盖长期冻结的独立运动假设；在完全遮挡期间最近接受框可能已属于遮挡物。
- 已仅把本候选六个算法、配置和测试文件恢复至 `c4461fae3fa8c11fe59ad1e2070b295b22edb69d` 内容，`git diff c4461fa -- rk3566/nanotrack_v3` 为空；原始板端包、候选源码提交和本节证据保留。恢复后主机 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure` **23/23 PASS**，日志 `/tmp/nt-search-rebase-rollback-build.log` / `/tmp/nt-search-rebase-rollback-ctest.log` SHA-256 `6466e8d10c54a45d9e180f374055cb3135aafdbe924b3c751788cd79be97341f` / `e1196855ee1d4d19883af71bca3a7d803fb937cdfd0e9fd0819e6f1f18b3e030`。不扩跑剩余 15 路、不合 main/Debug、不改变开发或生产基线。**Handoff**：需要把搜索时对控制输出的平滑与用于身份重捕的搜索裁剪位置分开评估，不能牺牲正确身份以消除单帧速度尖峰。

## 2026-11-18T04:37:43+08:00 冻结运动搜索起点连续性候选，主机通过待板端

- 分支 `codex/ten-target-smoothness`，工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，干净起点 `c4461fae3fa8c11fe59ad1e2070b295b22edb69d`。先对最近 20 路板端 CSV 复核：car004 GT 初始化在 131 帧接受观测、132 帧可疑、133 帧进入搜索；运动输入已冻结约 3.7s，运动预测与最近观测中心分离约 0.41 个框对角线，133 帧输出从 132 帧观测中心跳到陈旧预测位置，造成搜索速度 `758.178` 超过真值限 `689.190` px/s、加速度 `21984.260` 超过 `20761.594` px/s²。20 路中仅该搜索入口同时满足“运动冻结且分离超过 0.25 对角线”。该问题是冻住的速度仍合理、位置却长期未跟随已接受观测，不需要按车号或帧号修正。
- 用该 20 路 GT 数据离线筛查 2 帧运动对齐残差：可捕捉 car010 149 帧与 car009 115 帧的单阈值在 2249 个已接受可见帧中仍误触发 90 帧左右；再与 10 帧外观相关性下降相交，在 1738 个新增目标已接受可见帧中仅剩 3 个误触发，却漏掉 car010 149 帧。只读脚本与结果 `/tmp/nt-motion-consistency-screen.{py,json}` SHA-256 `cb4f9c861ce3ee6e6de1bc4d771c44f397e62acb514dc5cc91c17392a9cce9bf` / `3569f188e91acec9e36fb94af2cdd2b87afc9ea717c0655077c12bf45d8b8261`，交集 `/tmp/nt-cue-intersection.{py,json}` SHA-256 `36dd0f015bcbf064e814b8dfc00832e3139330a5712cf275bca35dce1c88ad3b` / `51994219d13186fd3ce80b3be6bcd85c78148d4609738242bc8e13b53fd34791`。因此不把这两个图像启发式加入运行时，继续保持 car010 149/169 未解决的状态。
- 新 opt-in `motion_search_rebase_min_center_ratio=0` 默认关闭，仅 `visible-dual-gt-visual-gate.conf` 设为 `0.25`。只有首次进入 SEARCH、运动输入已冻结、最近可信观测不超过既有 `uncertain_ms`、运动预测与按可信速度外推的最后接受框中心相距超过 0.25 个框对角线时，才把搜索运动位置重置到该最后接受框的速度外推位置；不使用本帧失败的 NanoTrack 结果，不改变可信速度、不把 detector 候选直接授权给控制。默认关闭时保留旧配置身份。单测覆盖启用/关闭时搜索起点、运动速度与无观测控制语义，并覆盖配置解析、非法阈值和身份。
- 实际主机命令 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure`，RKNN-off CTest **23/23 PASS**；日志 `/tmp/nt-search-rebase-host-build.log` / `/tmp/nt-search-rebase-host-ctest.log` SHA-256 `ad21b121ba6e453c0d76534148070bac28dd4ea08e5d9f19688f84fdb8c5720a` / `3ee0be1eee4bf54d73ace2367128db6ac7d1b31d1823ed586c5697d6fe68f86b`，`git diff --check` PASS。尚无板端新程序、效果或成本证据；下一步隔离 RK3588 原生构建/CTest，先跑 car004 GT/DET 和已通过目标，再决定是否全 20 路。此候选不解决 car010 的可见性判定或 169 帧重捕，不升级新基线、main/Debug 或生产 handoff。

## 2026-11-18T04:25:23+08:00 333ms 与同帧二次搜索组合定向板测否决

- 起点 `codex/ten-target-smoothness@c1a64244cd6415759a1a68f8f197e27b1f8dacb1`、干净工作树。只复用此前已否决的隔离板端候选源码 `6b262a770503ed44492b8cac381f7db647c5033d`，程序 `/home/neardi/Project/SOT/nt-fresh-reprobe-6b262a7/build/nanotrack_v3` SHA-256 `a96f2f8fc452c1b2b7edd31e8db984d4c7411b53992a8b5e70aac04944ef2bf3`；不改源码，仅复制 controller 配置并把 `search_reacquisition_min_age_ms=400` 改为 `333`。新 controller 与运行配置 SHA-256 `3ff4ba6814ceff77b0ecbf0ab3b0fd6098d242083d27d032ec5c6028ba0688f3` / `27965370184ffbc170b231be9df3e2f89f40d091e8a3d1e93ae1021eb8cfcca2`。模型、视频、manifest、labels、RKNN API/driver 均沿用上节已核验板端身份。
- 实际执行 `bash /home/neardi/Project/SOT/nt-reprobe-age333-probe.sh`，脚本 SHA-256 `796ae10e64359d2b2dc4291549d6aaf37d31eaefd29c841d6f74af83e564d02f`，用首帧真值或 detector 框独立跑 `car_010` GT/DET、`car_002` GT/DET、`car_003` GT，每路 271 帧；每路准确 `--bbox` 与完整运行命令在该脚本。原始 CSV、session、日志和配置保留板端 `runs/age333-{gt,det}-car_###/`；板端及主机 `/tmp/nt-reprobe-age333-probe.tar.gz` SHA-256 均 `92323aeab18363be1e456d15f0707836ab3c82a6778fd9173f066bea3e96cbdc`，主机解包 `/tmp/nt-reprobe-age333-probe/`。正式 evaluator 实际以 `--scope full --dataset-root /home/u20/single-object-tracking/data/occ_video_1 --source-commit 6b262a770503ed44492b8cac381f7db647c5033d --config-sha256 27965370184ffbc170b231be9df3e2f89f40d091e8a3d1e93ae1021eb8cfcca2 --model-sha256 3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261` 逐路评分，脚本 `/tmp/nt-score-reprobe-age333.py` SHA-256 `16243fa9cb3649af9de607b2e736f8a596e9e5f0f1bcd235ece5b0f37abf621c`。
- 五路全部 **FAIL**。`car_010` 两路 149 帧仍 `TRACKING/accepted=1`；GT 初始化 169 帧 `IDENTITY_CONFLICT`、176 帧重捕，DET 初始化 169 帧 `AWAITING_CONFIRMATION`、170 帧重捕，仍均晚于人工 169 时限。`car_002` GT/DET 都在 223 帧错捕，后续可见目标框和身份门失败；`car_003` GT 在 143/192 帧错捕，新增可见框、身份、出画和重现失败。五份 `score.json` SHA-256 按上述顺序分别为 `02b5128537161adf1f9b8ff3ed9ab21efd6017342600468d43aed23710200fc3` / `8e357e8653fde717793a16842749f838f05f854a969c8cc33b4df05d50492105` / `ac00a93ec24ceaa9d06ac10d614f6078e474c278ce439b62b819cbddf98008f6` / `9e8cf5bd1a0c16bab1b6e3cec9cb2987961b39c678acdea3e23efebf9afe1a33` / `bfe805659a22ec5bf8b4ce20c9028575916247525a2f59def55bea6bd03ad0c0`。
- **Handoff**：333ms 与同帧二次搜索组合否决，不扩跑其余 15 路、不进入当前源码或 main/Debug。现有 400ms 最短等待并非唯一障碍：即使提前放行，169 帧的身份/连续确认仍挡住 car010，同时错捕其他目标。保持上节用户 149/169 时限和 20 路回归门；生产 handoff 不变。

## 2026-11-18T04:19:15+08:00 十目标人工边界纠正与 car010 检测信号核查

- 分支 `codex/ten-target-smoothness`，工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，本次起点 `963427edb4be72a0555a5de9008e065ad02b52cb`，起点干净；`rk3566/nanotrack_v3` 源码与收窄锚点 `052b6de115aa3c59dadee30bf009529ea39f32ac` 的 diff 为零。本节只记录数据边界与只读诊断，没有算法或正式配置变更。
- 用户确认新增十目标中的 `car_004` 第 42 帧目标可见；`car_010` 第 149–169 帧大部分被遮挡，要求算法不晚于 149 帧判为遮挡、不晚于 169 帧正确重捕。正式 `car_004` 标注仍把 42 帧列为 `full_occlusion` 且无框，故该帧现有评分失败不能当算法错误；现有独立修订包 `/home/u20/single-object-tracking/data/occ_video_1/ground_truth_versions/car_004_visible_unboxed_v2/` 将其列为可见无精确框，不参与普通 IoU，原标注未改。`car_004` 131–132 帧及 GT 初始的预测连续性仍为独立失败，126–129 帧框几何语义尚待确认。正式 `car_010` 包把 149–168 列为完全遮挡、169 列为首次可见；用户描述为“大部分被遮挡”，不能擅自把部分可见改成精确真值框。
- 最新 20 路板端回放记录 `/tmp/nt-fresh-reprobe-6b262a7-full/runs/reprobe-gt-full/car_010/frames.csv`（板端候选源码 `6b262a770503ed44492b8cac381f7db647c5033d`，该候选已撤回）在 149 帧仍 `TRACKING/accepted=1`，Recent PSR `8.175270`；158 帧才 `SEARCHING`。169 帧搜索时长 `366ms`，被 `SEARCH_MIN_AGE` 拦住；170 帧 `AWAITING_CONFIRMATION`，171 帧才正确 `RECOVERY_STAGED` 重捕。故两个人工时限均未满足。car004 第 42 帧 `TRACKING/accepted=1`，与用户可见判定一致，但正式错误标注给出假失败。
- 在同一 `LKD3588` 板上用既有 `8e596d5cf8e5e704f13cd573153333920b3ac0f0` detector 烟测程序、模型与视频诊断 145/147/148/149/150/151/153/155/156/158/165/168/169/170 帧。实际脚本 `bash /home/neardi/Project/SOT/nt-car010-detector-audit.sh`，脚本 SHA-256 `be5eb0f0b7681d8468c2b57634f1bad7b45a4159439009229e580bb6733fceac`；原始板端包 `/home/neardi/Project/SOT/nt-car010-onset-detector-audit.tar.gz` SHA-256 `ca7e5310bc367154fdb425b89fe6c997686ec43c11fa8969b247e06072754085`，主机仅 CSV 包 `/tmp/nt-car010-onset-detector-csv.tar.gz` SHA-256 `bb48806b15cf7abf9e737c5ca5f592f9f7f4eb741e1ca5c1a85fd9a9c116093e`。烟测默认只输出前 100 候选，不能用缺席判目标未检出。
- 为解除前 100 截断，在板端独立目录 `/home/neardi/Project/SOT/nt-detector-full-list-audit` 复制原 `rk3566/nanotrack_v3` 源，仅把 `yolo_detector_smoke.cpp` 的请求上限 `100` 改为 `1000`，保持原证据目录不动；诊断源码 SHA-256 `3b0dab5a8ca07b314b108a5310d94114391aaeb6be6fe00362f5d056e39870e5`。原生 CMake/build 的日志 SHA-256 `debdca7bc33815c10566882d4a63fc9c59433de2a2f62dc5e43940d49e84cbb9` / `630e0eb01cea840cf2fa790bfef047118fc4e60fc5657cd41a3941628564e731`，诊断程序 SHA-256 `fbdae3495c3660a9a26f3687ee2341c5c07b24de5178e4ff490ab401e3064fa3`。对保存的 145/147/148/149/165/169 帧实际运行 `build/yolo_detector_smoke --model <RK3588-VisDrone9.rknn> --manifest <detector-manifest.json> --labels <visdrone9.txt> --image <frame-N.png> --conf 0.25 --nms 0.45 --class-id -1 --output-image <detected-N.jpg> --output-csv <detections-N.csv>`；板端及主机 CSV/日志包 SHA-256 均 `5b2992ce2e756b4ad5b2e7146df14c8ba4b7edfc229e6389cb3120597d502c2a`，主机解包 `/tmp/nt-detector-full-list-csv/`。149 帧输出 153 候选，目标附近车框 `(984.0,439.4,28.2,34.1)`、置信度 `0.427`，在排序第 118 位；可见 147 帧同目标附近置信度 `0.420`，148 帧 `0.518`，可见 145 帧没有近框。因此单帧 detector 缺席或置信度门不能稳定地区分遮挡起始；165/169 帧目标附近候选置信度 `0.708/0.736`。149 帧推理计时 `59.075ms`。此为单帧全图 detector 诊断，不能冒充运行时连续跟踪结果。
- 输入与环境：视频 SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；RK3588 detector 模型、manifest、labels SHA-256 依次 `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c` / `ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9` / `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`；原烟测程序 SHA-256 `3ba28a7afa1e5aa9c09a551f1fbb2b0e137e2b0006ecb1941fdd7fe8d9712f72`；RKNN API `2.1.0`、driver `0.9.8`。板端原始视频、模型与程序 SHA-256 在本次复核一致。
- **Handoff**：请协调对话把 `car_004` 42 帧可见及 `car_010` 149/169 两个时限纳入下一版数据/评测契约，保留原标注版本以便回溯。当前算法仍未达到 car010 时限；没有通过 10/10、没有新开发基线或生产 handoff。下一算法候选须拿出跨目标可区分的遮挡证据，并在 GT/detector 双初始化 20 路板端复核，不能只靠这段视频上的 detector 单帧阈值。

## 2026-11-18T04:05:08+08:00 同帧 NanoTrack 二次搜索完成 20 路板测，无关键收益而撤回

- 实验源码 `codex/ten-target-smoothness@6b262a770503ed44492b8cac381f7db647c5033d`；主机 `/tmp/nt-fresh-reprobe-6b262a7.tar`、板端同名源包 SHA-256 `073a114acc4ad1d7d5c5d12db2806af6482755b32ddc238e287e0e9c7ac25536`，隔离板端根 `/home/neardi/Project/SOT/nt-fresh-reprobe-6b262a7`。实际 `bash /home/neardi/Project/SOT/nt-fresh-reprobe-setup.sh`（主机 SHA-256 `5b9ad9cb9ef1bb6a88f46ee9a958a51fa3c312632ddea3fe5caf7af2b5f60cb5`）RK3588 原生 CMake/build/CTest 23/23 PASS，程序 SHA-256 `a96f2f8fc452c1b2b7edd31e8db984d4c7411b53992a8b5e70aac04944ef2bf3`；运行配置 `runs/headless-on.conf` SHA-256 `073d94ab9e00edb95a6ee5df333b9756c460de705ea2561a622d58b488048e32`；`cmake.log`、`build.log`、`ctest.log` SHA-256 依次 `450a62c37044acd1928f22f2a1861702f83b7ccb34d07ec0e49f7e1a7c0bd7c5`、`12882f658253243b10cc9c873a82ffccb21d41d0a2692ef2542273805908781d`、`342d19ace646271c421c93838e20baefa50c8cea6ca35299313fcbfc1974c5c6`。视频、模型/manifest/labels、RKNN API/driver 身份同上次板测。
- 定向先实际运行 `bash /home/neardi/Project/SOT/nt-fresh-reprobe-focused.sh`（主机 SHA-256 `efca27a27d8a68e8b688aa38920d8eed91239c97e4e0fcd4dc6c5b77561d9c07`）七路，每路 271 帧；原件板端 `/home/neardi/Project/SOT/nt-fresh-reprobe-6b262a7-focused.tar.gz`、主机 `/tmp/nt-fresh-reprobe-6b262a7-focused.tar.gz` SHA-256 均 `f51e1b0fabf87ba939c3f66ee9b8f91bbf21d95da307399cd6a8bca2fa46f79c`，摘要 `/tmp/nt-fresh-reprobe-6b262a7-focused-summary.json` SHA-256 `d969dd4f7d8cbe983ad56a32322a34aa1b057c483162f1d37f441a1cd0b89221`。car010 GT/DET 均在 169 帧触发 `DETECTOR_SEARCH_REPROBE`，两路仍 171 帧才正确重捕；car002 GT/DET、car006 GT、car007 DET 均 PASS，car003 GT 仍有完全遮挡失败，没有新增早期错捕。二次裁剪后 car010 169 帧 Recent/Safe 与该帧真值 IoU 在 GT 初始化约 `0.45/0.51`、detector 初始化约 `0.50/0.53`，但 400ms 最短搜索及连续身份门仍不允许当帧接受。
- 为检验全部目标，实际 `bash /home/neardi/Project/SOT/nt-fresh-reprobe-full.sh`（主机 SHA-256 `2645503379b4e1f77ba8185a8d988f55d0d3ae66fbbe1cb1877b2f3b14d3d5a7`）执行 `bash runs/nt-reprobe-full-gt.sh`、`bash runs/nt-reprobe-full-det.sh`，两脚本 SHA-256 `6339c2fdcde88745292022295d49b72ceaef8a002ff15ae4868520545ddda728` / `cec754f8b85b9dc3d7401ae8f9a93aeffb5571e25055f0bfc0da41759bc3a2b9`。20 份原始 CSV 均 271 帧；板端 `/home/neardi/Project/SOT/nt-fresh-reprobe-6b262a7-full.tar.gz`、主机 `/tmp/nt-fresh-reprobe-6b262a7-full.tar.gz` SHA-256 均 `0668c36ccef8f9997b5c272760bd364109e904f24d30b431ba81822d1b06fd1f`；解包 `/tmp/nt-fresh-reprobe-6b262a7-full/runs/reprobe-{gt,det}-full/car_###/`。正式评分脚本 `/tmp/nt-score-fresh-reprobe-full.py` SHA-256 `76b9b3abae4831d1a155ac42f7d38efe584d3de0b0252be33a0f71978f68be09`，对每份执行 `python3 scripts/evaluate_occ_video_1.py --dataset-root /home/u20/single-object-tracking/data/occ_video_1 --target-id <target> --boxes <frames.csv> --session-metadata <session.json> --scope full --source-commit 6b262a770503ed44492b8cac381f7db647c5033d --config-sha256 073d94ab9e00edb95a6ee5df333b9756c460de705ea2561a622d58b488048e32 --model-sha256 3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261 --output-json <score.json> --output-markdown <score.md>`；摘要 `/tmp/nt-fresh-reprobe-6b262a7-full-summary.json` SHA-256 `08ab06997a740cfd34fb5fef835fbc37eaff8a3343c64b7d01091394e3ea5ac6`。GT 与 detector 初始化均 **6/10 PASS**，通过集合皆 001/002/005/006/007/008，car003/004/009/010 仍 FAIL；旧版 `car_004_visible_unboxed_v2` 修订真值下 detector 初始化 car004 另得 PASS，GT 仍 FAIL。car010 两路仍 171 帧重捕、149 帧仍 `TRACKING/accepted=1`。与收窄锚点单次全量对照，car010 两路状态、接受和重捕逐帧零差异。二次搜索只增加计算、没有达成提前遮挡或 169 帧重捕，**实验否决并撤回源码**，保留源提交与原始板端记录，不合入 main/Debug。下一步要么取得能在 169 帧被 NanoTrack 独立验证的当前帧证据，要么承认现有安全门不能仅凭 detector 置信度满足这个时限；不能用 detector 框直接授权控制。

## 2026-11-18T03:45:42+08:00 当前帧新鲜检测触发二次 NanoTrack 搜索候选待板端

- 起点 `codex/ten-target-smoothness@375b171024d4cb5fe4a0f17c03e0788a7137cc44`，算法源码恢复到 `d068ca2` 等同内容、工作树干净。为判断 car010 的 169 帧能否安全提前，实际在 RK3588 `bash /home/neardi/Project/SOT/nt-car010-f169-detector.sh`（主机脚本 SHA-256 `5a3e864c7c1c9f2094c5702e1aa4d4dc33ec27175b0f3ac69967244d8489c792`）抽取同一视频第 169 帧，用 `8e596d5` 既有 `build/yolo_detector_smoke` SHA-256 `3ba28a7afa1e5aa9c09a551f1fbb2b0e137e2b0006ecb1941fdd7fe8d9712f72`、同一 VisDrone9 模型/manifest/labels 执行单帧全图推理。板端 `/home/neardi/Project/SOT/nt-car010-f169-detector.tar.gz`、主机 `/tmp/nt-car010-f169-detector.tar.gz` SHA-256 均 `a76a1c4b3ca280f2656d2314e9300baf39e870fe88796db7b2f6219817bc1f10`；解包 `/tmp/nt-car010-f169-detector/runs/car010-f169-detector/`，图像、CSV、日志 SHA-256 `3768a0f28b0f685f678a40a3b77fda7cb628548f44d34d92551f4202e54fb6cf` / `945b6f94f61770c63fd2a6bacecf3633d62108ceadb62a6e8d340fec1e39bbb9` / `0bebae2394e9a4428631ae2f714073045a5d146df42bdecdf5f5924bcb3edf29`。detector 在 `(983.25,438.74,35.70,33.29)`、置信度 `0.735840` 的框与该帧现有真值 IoU `0.875`、中心误差约 `0.98px`；但此前 GT 初始化回放的同帧 Recent/Safe 与此框 IoU 仅约 `0.389/0.594`（detector 初始化约 `0.404/0.402`），**单帧 detector 对了不等于当前 NanoTrack 已确认**，不能直接把它授权给控制。
- 新实验参数 `search_fresh_detector_reprobe=true` 默认关闭，仅 `visible-dual-gt-visual-gate.conf` 开启。SEARCHING 时若上一帧异步 detector batch 刚在本帧 NanoTrack 首次推理后到达，且只有一个同类有效候选在运动预测半个对角线内、首次 Recent/Safe 与其不一致，则用该候选中心、原搜索宽高在**同一帧追加一次 NanoTrack 推理**，以第二次推理的 Recent/Safe 交现有 detector、身份、连续确认门处理；几何一致的既有候选保留其确认历史。detector 仍不直接接受跟踪或授权控制；日志标记 `DETECTOR_SEARCH_REPROBE`，额外推理时间计入 tracker/runtime 耗时。FakeTracker/FakeDetector 单测验证旧裁剪与新裁剪各一次、无直接重捕，配置解析检查通过。主机实际 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure`，23/23 PASS；最终日志 SHA-256 `8e8dae9ea1ec95de0fe6822ca048b4ad6946a9ab12fa2a258a2e05cc5d224076` / `44543744e3af87b5d0c814e065374942d18d9804856206b6f53fc015e56b12b7`。尚无板端新程序效果证据；下一步隔离原生构建、定向跑 car010/002/003，再决定是否全量 20 路。该机制也不能自行解决 car010 第 149 帧过晚遮挡判定，需单独验证。

## 2026-11-18T03:37:29+08:00 运动邻近早期重捕六路板测无目标收益，候选撤回

- 实验源码 `codex/ten-target-smoothness@bf69b0002e94be1238ea50c255fe9092fccacd67`，主机 `/tmp/nt-early-near-bf69b00.tar`、板端同名源包 SHA-256 `c7c0fe37d5d0fef7e81fa3ec20bdd1027f781ed6103f3bd509ba4a6d53026fcb`。隔离板端根 `/home/neardi/Project/SOT/nt-early-near-bf69b00`，实际 `bash /home/neardi/Project/SOT/nt-early-near-setup.sh`（主机 SHA-256 `3ce444fa66554e9ac9fe4ca2f664f5b17de815252aae750b566e303dfd5fb3f0`）执行 RK3588 原生 CMake/build/CTest，23/23 PASS。程序 SHA-256 `88b7514a161eed419e2ed5e78b713ccff2ab0f0181ad9f5c4d8798a9780066de`，运行配置 `runs/headless-on.conf` SHA-256 `ca6b7bcb93c9380acd1b5dfff36ea869eb85d7d967ec47cab65b787cdecadbed`；`cmake.log`/`build.log`/`ctest.log` SHA-256 `f5e4c6cc28eb0023a7a9c5f779f54982d72ab9e3ee9b4d9f6c2ec70d53293574` / `7681f1e7aab67f66b117c58e64120f804a0e6d50f425620482338372048adb6c` / `7553c70810298f1d10dfbf8e9b8ed144fc469a3f24ecbf94dba6c244214f7a69`。视频、模型/manifest/labels、API/driver 同上述已核验身份。
- 实际板端 `bash /home/neardi/Project/SOT/nt-early-near-focused.sh`（主机 SHA-256 `1f634637aa89d4df6d98fbd419490816a0549fe73b3409c00928ea36ce3db845`）完成 car010 GT/DET、car002 GT/DET、car003 GT、car007 DET 六路，各 271 帧。板端 `/home/neardi/Project/SOT/nt-early-near-bf69b00-focused.tar.gz`、主机 `/tmp/nt-early-near-bf69b00-focused.tar.gz` SHA-256 均 `11b4b6fb7759f605e106f156c48bf4d59d700b8ad7c91d6a03fa0ad07863de0d`；解包 `/tmp/nt-early-near-bf69b00-focused/runs/focused-<mode>-<target>/`。逐份运行 `python3 scripts/evaluate_occ_video_1.py --dataset-root /home/u20/single-object-tracking/data/occ_video_1 --target-id <target> --boxes <frames.csv> --session-metadata <session.json> --scope full --source-commit bf69b0002e94be1238ea50c255fe9092fccacd67 --config-sha256 ca6b7bcb93c9380acd1b5dfff36ea869eb85d7d967ec47cab65b787cdecadbed --model-sha256 3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261 --output-json <score.json> --output-markdown <score.md>`；摘要 `/tmp/nt-early-near-bf69b00-focused-summary.json` SHA-256 `794ca40f101dc9a67a13359c5f2cc39d834d4df716e385e8b4d12b5a7c89a861`。
- car002 GT/DET 均 PASS、228 帧正确重捕，car007 DET PASS；car003 GT 仍 FAIL（完全遮挡语义），但没有 333ms 全局配置在 143 帧造成的早错捕。car010 DET 仍 FAIL、171 帧重捕，GT 仍 FAIL、176 帧重捕；两路 149 帧仍 `TRACKING/accepted=1`。169 帧候选虽近运动预测，但 detector 与当前 Recent/Safe 的几何一致门不成立，返回 `SEARCH_MIN_AGE`；GT 路后续 170–174 帧发生身份冲突。这组真实时序表明较早门没有带来目标收益；**候选撤回**，不扩跑剩余 14 路、不合入 main/Debug。撤回源码后保留实验提交及板端原始结果；下一步研究当前帧 detector 与双模板能否形成及时且有身份保障的证据，同时单独解决 149 帧早期遮挡，不能放松几何门来迎合一帧。

## 2026-11-18T03:31:15+08:00 运动邻近的较早重捕候选主机通过，待板端

- 起点 `codex/ten-target-smoothness@d068ca2077d7416edf7ab29bcea4be8755de9e40`，工作树无预存改动。对前次隔离 `333ms` 板端回放定量比较：car010 detector 初始的 168/169 帧正确恢复候选与运动预测中心相距 `0.197/0.129` 个预测框对角线，Recent/Safe 与候选 IoU 约 `0.65–0.70`；car002 错捕 221–223 帧为 `1.239–1.271`，car003 错捕 141–143 帧为 `1.907–2.035`。只读审计脚本 `/tmp/nt-recovery-motion-audit.py` SHA-256 `8f5846ed05b6818cbc490ebe00709b40ef5c41808c6ed93b5a6599960282e79b`。另以视频半分辨率、10 帧间隔归一化 RGB 裁剪相关性筛查八个新增目标，能命中 car010 149 帧的相关性下降规则也命中 71 个已标可见帧，约占 1897 个可见检验帧的 `3.74%`，且多集中在遮挡或重捕附近；脚本 `/tmp/nt-appearance-audit.py` SHA-256 `855dfee6606e46997188d46e568c19075248543221f2b009e8d271f51b25c2cd`。离线近似只用于拒绝简单外观门，不能当作板端算法验证。
- 新候选增加可选配置 `search_early_near_motion_age_ms=333`，默认 0（关闭），仅实验 profile 开启。400ms 原门保持；在较早时刻，必须有 full-object detector 候选、可靠 Recent/Safe、三框几何一致、运动预测仍可信，且三框中心都在预测中心半个对角线内，才允许开始候选确认；两次独立 tracker 帧且 detector 支持才能提前接受。远运动的相似车辆仍等原 400ms 门；不改变 detector 直接控制的禁令。新增近运动两帧可接受、远运动不可提前接受单测及配置解析检查。主机实际 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure`，23/23 PASS；日志 `/tmp/nt-early-near-host-build.log` / `/tmp/nt-early-near-host-ctest.log` SHA-256 `430da93f2c3b60f3cf34b595b561249f2e06a99edf9bb573adb9d31eb7fbd026` / `b55c2b96ff675893ab7db8e2e6fc9d4d816bd2c925f4ac54fc7a483426c56549`。这是由本组数据提出的实验假设，不因主机测试通过就晋级；下一步 RK3588 原生编译，先跑 car010、car002、car003，再跑完整双初始化十目标并检查 149/169 帧、其他目标门禁和耗时。

## 2026-11-18T03:22:03+08:00 333ms 搜索等待配置探针否决

- 使用板端隔离源码 `codex/ten-target-smoothness@052b6de115aa3c59dadee30bf009529ea39f32ac`，程序 `/home/neardi/Project/SOT/nt-detector-anchor-narrow-052b6de/build/nanotrack_v3` SHA-256 `296909f438193fe447930ef733965cdc731dfaa9f5b8057ff712e0b690dbed55`；仅把配置 `search_reacquisition_min_age_ms=400` 改为 `333`，配置 `runs/age333-controller.conf`、`runs/headless-age333.conf` SHA-256 `9be167f0ab5128e9a5aa7c113fe3601994477b0c4acc6837afe0e222b78735fd` / `e26d860f37f7ee97ac2249c237a42d0143f4060daea78d2eabbf82f4b212fc9a`，未改程序。视频、模型及运行版本同上一原生测试。实际板端 `bash /home/neardi/Project/SOT/nt-narrow-age333-focused.sh`（主机 SHA-256 `0e85a473bac69a6fa498da2e0d1f934e5c261a10e40ea4187dd941b447dca658`）跑 car010 GT/DET、car002 GT/DET、car007 DET、car003 GT 六路，均 271 帧。板端 `/home/neardi/Project/SOT/nt-narrow-age333-focused.tar.gz`、主机 `/tmp/nt-narrow-age333-focused.tar.gz` SHA-256 均 `0351cca88d404fbf6821a77c6d1de2303c50cca84c907859d3839e3682c1c5f8`；解包 `/tmp/nt-narrow-age333-focused/runs/age333-<mode>-<target>/`。逐份以 `python3 scripts/evaluate_occ_video_1.py --dataset-root /home/u20/single-object-tracking/data/occ_video_1 --target-id <target> --boxes <frames.csv> --session-metadata <session.json> --scope full --source-commit 052b6de115aa3c59dadee30bf009529ea39f32ac --config-sha256 e26d860f37f7ee97ac2249c237a42d0143f4060daea78d2eabbf82f4b212fc9a --model-sha256 3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261 --output-json <score.json> --output-markdown <score.md>` 正式评分，摘要 `/tmp/nt-narrow-age333-focused-summary.json` SHA-256 `7ac45b2f30c61db88dc2106afa0d9f051bd61c892484d34af21760a449d70098`。
- car010 detector 初始正确重捕只提前 `171→170`，GT 此次异步时序下仍 `176`；两路均未满足 169。car002 GT/DET 均于 223 帧错误重捕，`PASS→FAIL`，identity、reacquisition、visible_boxes 三门失败；car003 GT 又在 143/211 帧错误重捕并新增身份、出画等失败，car007 DET 仍 PASS。**333ms 配置否决**，不改正式配置。`0ms` 此前也在 car002/010 错捕；安全重捕需区分目标身份与可见证据，不能统一缩短搜索等待。当前代码分支 HEAD `4d7b7a7b44a1992dfe1190eeffde93fb2ad18158` 的算法内容已恢复到 `052b6de`，仍在实验分支，不合入 main/Debug。下一步比较正确 car010 与错误 car002/003 的同帧候选、双模板和时序信号，再决定是否有通用门可验证。

## 2026-11-18T03:13:54+08:00 同步 SUSPECTED 检测定向板测回退

- 实验源码 `codex/ten-target-smoothness@723c7f85402c5880abe67c5959b3a0362f591b5f`，主机 `/tmp/nt-guard-sync-723c7f8.tar` 与板端同名源包 SHA-256 `da0bda93247e60034e19e497936940919b21ff0e349cfcbc5811f4aa96bb0f27`；隔离板端根 `/home/neardi/Project/SOT/nt-guard-sync-723c7f8`。实际 `bash /home/neardi/Project/SOT/nt-guard-sync-setup.sh`（主机 SHA-256 `f3829911e89a033f9b3aeaad533858b23441bfd1033140c1c1a5155b36c00278`）执行 RK3588 原生 CMake、build、CTest，23/23 PASS。程序 SHA-256 `636faaecbf9e7b9c0021ce5879a6a49c9459c41c55be73bcf0c8fd7e3c47e7d1`；配置 `runs/headless-on.conf` SHA-256 `2e72622ba169e39d58ac7b7c175bd5c66e7b47eddcbb838d6def5a8cf6452c5e`；`cmake.log`、`build.log`、`ctest.log` SHA-256 依次 `4e18fce0e82abe61669824133f3609c7de4736ec3762bfa6a43a589dd0ea25c6`、`66c39e38dc85a9fc720d08beadbab2080582b203a8a62f3912f099255082df67`、`b12b3e927dc034b55402f8aa95b51d95c7e765e5fd4498bdc1d62797cb28285b`。模型、视频及 RKNN 运行版本与上节相同；这次只验证已保存视频回放，不构成吊舱验收。
- 实际 `bash /home/neardi/Project/SOT/nt-guard-sync-focused.sh`（主机 SHA-256 `333363a6959e9465fdd985da781af2ec649c2b9930150cbe17bf8358718d8856`）完成 car006 GT 三次、car010 GT 两次及 detector 一次、car002 GT 一次、car007 detector 一次，八份 CSV 均 271 帧。板端 `/home/neardi/Project/SOT/nt-guard-sync-723c7f8-focused.tar.gz`、主机 `/tmp/nt-guard-sync-723c7f8-focused.tar.gz` SHA-256 均 `0190aa74a251ea92df8b7a55540cca1c5845384c7b396b97bce9fed7efd473ae`；解包 `/tmp/nt-guard-sync-723c7f8-focused/runs/focused-<mode>-<target>-<repeat>/`。逐份运行 `python3 scripts/evaluate_occ_video_1.py --dataset-root /home/u20/single-object-tracking/data/occ_video_1 --target-id <target> --boxes <frames.csv> --session-metadata <session.json> --scope full --source-commit 723c7f85402c5880abe67c5959b3a0362f591b5f --config-sha256 2e72622ba169e39d58ac7b7c175bd5c66e7b47eddcbb838d6def5a8cf6452c5e --model-sha256 3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261 --output-json <score.json> --output-markdown <score.md>`，摘要 `/tmp/nt-guard-sync-723c7f8-focused-summary.json` SHA-256 `b74973e493a21fc61626736b3871afe8cb7ad1d768b426e48f62916e1bf7485f`。
- car006 三次 GT 均 PASS、215 帧重捕，但可见均值 IoU 从收窄版这次 `0.858` 降至 `0.830`；pipeline p95 从 `119.9` 升至 `213.5–221.9 ms`，同步可疑验证请求 25 次。car010 两次 GT 均 FAIL，重捕从收窄版 `171` 延至 `176`；detector 初始一次 FAIL，171 帧重捕，149 帧仍接受旧跟踪。car002 GT 和 car007 detector 各一次 PASS。由于 car010 明显退步且 car006 延迟大幅上升，**同步候选否决**，不再跑其余十二路，也不合入 main/Debug。已把实验配置、运行时及测试源码恢复到 `052b6de` 对应内容，保留实验提交与原始板端记录；下一提交记录回退。用户最新真值边界为新 car004 第 42 帧可见、car010 第 149–169 帧大部分遮挡，要求遮挡判定不晚于 149 且正确重捕不晚于 169；现有候选均未满足 car010 两个时点。car004 第 126–129 帧真值框几何语义的补充确认仍待用户回复，不据此调参。

## 2026-11-18T03:02:38+08:00 收窄检测锚点板测暴露异步时序差异；同步可疑验证候选待板端

- 收窄锚点源码 `codex/ten-target-smoothness@052b6de115aa3c59dadee30bf009529ea39f32ac`；主机 `/tmp/nt-detector-anchor-narrow-052b6de.tar`、板端 `/home/neardi/Project/SOT/nt-detector-anchor-narrow-052b6de.tar` SHA-256 均 `371fbedaab3abd79bfd97d75b023c9035cf7acef9329ee6c7d4356e6c25aaa11`，隔离板端根 `/home/neardi/Project/SOT/nt-detector-anchor-narrow-052b6de`。RK3588 原生 CMake/build/CTest 23/23 PASS，日志 SHA-256 `9aa39bbf71877b75bce8f4c5c7a2589247baeb6ebc7a177738eda28e622057da` / `a5b6ec71b426bb8a2d4a17c37a13950d7bf8e8f3146628db8890e25ce0228539` / `8bf0de80e248d0626db68afba380ab03a108c403601f474250a92641c041c05e`；程序 SHA-256 `296909f438193fe447930ef733965cdc731dfaa9f5b8057ff712e0b690dbed55`。运行配置 `runs/headless-on.conf` SHA-256 `cea35c3aca2a0c5e71ff115f0e4d3f674b696a82b7877acfffe8d6c2bfbf5125`；同一视频、tracker manifest、VisDrone9 detector RKNN/manifest/labels 和 API/driver 身份见上节。实际 `bash runs/nt-narrow-focused.sh`（SHA-256 `89c8e0fc29f6c3aba8f6b89c7258b0554937934c1f3113997da037c860c78517`）12 路与 `bash runs/nt-narrow-remaining.sh`（SHA-256 `c3c16cb610e0d6940ff09bdbeddb3b769293ffd9577ae09cb12c7ae3be033b7f`）8 路，20 份 CSV 均 271 帧。板端 `/home/neardi/Project/SOT/nt-detector-anchor-narrow-052b6de-full.tar.gz`、主机 `/tmp/nt-detector-anchor-narrow-052b6de-full.tar.gz` SHA-256 均 `d9429d0f96c6b15bb0567c709ba18710babaee78f2466b588ef43dc4b8104cb1`；解包 `/tmp/nt-detector-anchor-narrow-052b6de-full/runs/narrow-{gt,det}-full/car_###/`。正式评分脚本 `/tmp/nt-score-narrow-full.py` SHA-256 `e7eac76ea161367c68fa47cb5f0e14039b7f67c9b9cc43acae7d638182b9beef`，摘要 `/tmp/nt-detector-anchor-narrow-052b6de-full-summary.json` SHA-256 `73b1462a93972a45517b99525a42e35d44b92a37ee729eee407b2b35de4129a6`。这一次 GT **5/10 PASS**（car006 新增 199 帧完全遮挡语义失败），detector 初始化 **6/10 PASS**；car003/004/007 宽规则新增错误已消失，car010 两路正确重捕仍为 171 帧。
- 逐帧核对表明 car006 的分叉从 129 帧开始，**早于任何恢复锚点**：同一 128 帧 `SUSPECTED_VALIDATION` 异步请求在基线于 129 帧完成，收窄版这次于 130 帧完成；于是基线 129 帧清除可疑，收窄版升级搜索，后者再于 199 帧比基线晚一帧进入 SEARCHING。为查一致性，板端实际运行 `/home/neardi/Project/SOT/nt-car006-repeat.sh`（主机脚本 SHA-256 `cd767f6dd87a30524637ad3e8e78ef5cd6ffbabb84cadfe96d4796dae05aa992`），收窄版和原 `f4044e9` 程序各重复两次相同 car006 GT 首框与 271 帧。四份均正式 PASS，129 帧均收到 128 帧 batch 并清除可疑、199 帧均 SEARCHING；收窄版初次 FAIL 与同程序重复 PASS 证明这一门受异步完成时机影响，不能把单次 5/10 归因于锚点，也不能声称它稳定 6/10。重复原件板端 `/home/neardi/Project/SOT/nt-car006-repeat.tar.gz`、主机 `/tmp/nt-car006-repeat.tar.gz` SHA-256 均 `ff277fe9765371a3b1d2bff869dfe66e10566523ba87c40d974d5365b686ad28`；主机评分脚本 `/tmp/nt-score-car006-repeat.py` SHA-256 `61e0f51ba6eb56b6e2564d7297d7d71c5d44e3ba092ccd29cd156d00ad5a9c27`，摘要 `/tmp/nt-car006-repeat-summary.json` SHA-256 `6199665a3de16aa42aae695cac5f51096452dfa9fac7c1bb450ecdb6b2d615b9`。
- 新隔离候选增加 `suspected_detector_guard_blocking`，默认关闭，仅 `visible-dual-gt-visual-gate.conf` 打开。在 SUSPECTED 验证触发时同步等待**当前帧** detector 结果，先停掉旧异步请求，再按原有唯一同类框、双次命中、Recent 可靠性和身份门处理；detector 仍不直接授权控制。其余 DISPLAY_REFRESH 和 SEARCH_RECOVERY 保持异步；源码变更为运行时、配置、测试，测试明确让 fake detector 有未完成异步请求而验证阻塞路径仍取当帧 batch。主机实际 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure`，23/23 PASS；日志 `/tmp/nt-guard-sync-build-final.log` / `/tmp/nt-guard-sync-ctest-final.log` SHA-256 `e38f195f70db59d16df98b2e544c505aef0672107b76a5f8a998f2f1343ab7de` / `40e1c8db53d9b5cf508b66384ccad93749b1668ec43f4934119e1cad49151e7f`。尚无板端延迟或效果证据，不合入 main/Debug；下一步原生构建、定向重复 car006 与 car010，若无明显回退再跑全量双初始化。

## 2026-11-18T02:37:16+08:00 全量固定检测裁剪回退，收窄到近运动且明显漂移候选

- 宽规则源码 `codex/ten-target-smoothness@d4a59c255e83b67bd0c49061c15522aa36a7d7eb`；源包主机 `/tmp/nt-detector-anchor-d4a59c2.tar`、板端 `/home/neardi/Project/SOT/nt-detector-anchor-d4a59c2.tar` SHA-256 `8302edac39a603200829fa9ca24fbc0d908251beaba8d69de3a3261c3d5d73ec`，隔离板端根 `/home/neardi/Project/SOT/nt-detector-anchor-d4a59c2`。RK3588 原生 CMake/build/CTest `23/23 PASS`，日志 `cmake.log` / `build.log` / `ctest.log` SHA-256 `d4c121a3b3a496c9f9f281119be87b037d1204e2502565924ebd327579252443` / `003e162576e05be936439c74a7ae704e3774d0acfe9835b617ba1e54d137d7be` / `d5376c8fa7987ca8d6000603afc1e9c4d19e90da119d737219e3ab5183a8d76c`；程序 `build/nanotrack_v3` SHA-256 `d0257a8562556e8f8e716c359d7a61dc4501faa416c13fa14c83028dcca0cf26`。配置 `runs/headless-on.conf` SHA-256 `9973856fe7e0776b82faf593bc3e5c12351fd0ea27d9f7a9959a343006b5779b`；GT/DET 全量脚本 SHA-256 `ef98e10c6936082d10ebd2b62874a3af0960ddbc2df79a3121a88357a44bb508` / `ed462fca40272b45a525448fedf30606a17d5df3a712e0e9c642ff9ef335a926`。视频、tracker manifest、VisDrone9 RKNN/manifest/labels 仍为上文已核验身份，RKNN API `2.1.0`、driver `0.9.8`。
- 实际先运行 `bash runs/nt-anchor-focused.sh`（SHA-256 `59c08b1d735a34201dc12e91a8b70af85b8c4896252ea839049fc240ded66ed8`）七路，再运行 `bash runs/nt-anchor-remaining.sh`（SHA-256 `f2ec5783f31bfbab0c2ab2f8b996d242a4df69cc91fa2202d2fe81ff8e9978d8`）十三路；20 份 CSV 均 271 帧。板端 `/home/neardi/Project/SOT/nt-detector-anchor-d4a59c2-full.tar.gz`、主机 `/tmp/nt-detector-anchor-d4a59c2-full.tar.gz` SHA-256 均 `a44935a90e97fecec8355e93ecd5a9a52c53d442e5f5953db558115006e59def`；解包 `/tmp/nt-detector-anchor-d4a59c2-full/runs/anchor-{gt,det}-full/car_###/`。主机正式评分脚本 `/tmp/nt-score-anchor-full.py` SHA-256 `e717cc4a9f875ee3edef85d0488100f319d88edc3f4c4139b5cb155e8b90c44d1`，20 份 score 与摘要 `/tmp/nt-detector-anchor-d4a59c2-summary.json` SHA-256 `d8e26ebeef3e59a25da2a870f2f659bba81d1458b1622863983fbe186410d1ec`。正式 GT **6/10 PASS**、detector 初始化 **5/10 PASS**：car010 GT 正确重捕 `176→171`、detector 初始化仍 `171`，当帧真值 IoU `0.741/0.668`；但 car007 detector 初始化从 PASS 退为 FAIL（264–270 中六个可见框失败），car003 多次错误重捕及出画后接受，car004 detector 初始化在 234/252 帧错捕，故**宽规则否决**，不能作为新基线。
- 单独把搜索最短等待 `400→0ms` 写入 `runs/anchor-age0/controller.conf`，SHA-256 `c4670ce80984f86c3cdbbd75baa7ba9756b7b5523c44854d401f7e914545fe4a`，运行配置 SHA-256 `0005308d75abc844959e7188ed7fd132e0b598bdc2781731d8f811090a31bb69`；`bash runs/nt-anchor-age0-focused.sh` SHA-256 `76dbfbd9c745773fac305ae328178b6cd70a5a05df096fd700a1e4eed2690f1c` 完成 car002/009/010 GT 与 car002/010 detector 共五路，各 271 帧。板端 `/home/neardi/Project/SOT/nt-detector-anchor-d4a59c2-age0.tar.gz`、主机 `/tmp/nt-detector-anchor-d4a59c2-age0.tar.gz` SHA-256 均 `79bfd9e0f422528860005854426fc3d2c31b25f95ad51e8d9fb70622018489cf`；正式评分脚本 `/tmp/nt-score-anchor-age0.py` SHA-256 `f11487fa9e1c77d6937897579a4c8edc49d6796f46c88a496df855a9a30abccb`，摘要 SHA-256 `11ca1a8d9ad13b44d0ea9b3d4919a62d348868728f0b367dff2e28698a2da2cd`。car002 两路都在 220 帧错捕并 FAIL，car010 GT 在遮挡中的 165 帧过早重捕、detector 初始化在 162 帧错认，全部 FAIL；保留 400ms 安全等待。
- 对错误候选计算 detector 中心距运动预测相对预测框对角线：car010 正确 170/171 帧约 `0.30–0.48`；car003 假重捕 220/243 帧约 `0.80/1.15`，car004 假重捕 234/252 帧约 `1.15/0.55`。car007 正确 225/226 帧 Recent 与 detector 仅约 `0.02–0.04` 检测框对角线偏移，car010 正确 170/171 帧约 `0.12–0.17`。新收窄候选只在 **detector 与未确认 Recent 相差至少 0.1 检测框对角线、detector 与运动预测不超过半个预测框对角线**时将下一帧裁剪固定在 detector；小偏移和远运动候选保留原先 tracker 裁剪。仍不允许 detector 直接授权控制。新增小偏移不改裁剪单测，主机 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure`，23/23 PASS；日志 `/tmp/nt-detector-anchor-narrow-build.log` / `/tmp/nt-detector-anchor-narrow-ctest.log` SHA-256 `ff90c1ef36ef8268e99a4018b622e082943be43d619bf742efd2199ba1ba9257` / `6a645cbd3ab125bf7720ebe2698c57e6cb157271bed93cd71197bd64f0218dcc`。这组条件来自本组诊断，必须用完整板端闭环排除过拟合；目前只是实验，不合入 main/Debug 或生产 handoff。

## 2026-11-18T02:13:57+08:00 检测候选确认期间固定搜索中心（主机通过，待板端）

- 起点 `codex/ten-target-smoothness@7f175041dd4110ed6b42b9795a40ced991a5a3ed`，工作树干净。只读对照 car010 `age0` 板端回放：164 帧 detector 候选在 `(986.4,431.7)`，后续探针中心逐步被未经确认的 NanoTrack 结果带到 `(978.7,424.8)`，169 帧 Recent/Safe 与真值 IoU 低且仍 `IDENTITY_CONFLICT`。候选把 **full-object 模式下尚有效的 detector 证据**作为下一帧 NanoTrack 搜索裁剪及提示框中心，不让一次未确认的 tracker 偏移拖动裁剪；detector 仍不能直接授权观察或控制，恢复必须经过原有双模板、身份冲突和确认门。`subtarget` 与无 detector 候选路径沿用原行为。主机新增单测证明初次 detector 裁剪、偏移 tracker 仅获 1 次命中而不接受、下一帧裁剪仍指向 detector；实际 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure`，`23/23 PASS`，日志 `/tmp/nt-detector-anchor-build.log` / `/tmp/nt-detector-anchor-ctest.log` SHA-256 `588d88514ad31797eedc5faafaa656fadd4dce31abe56f792908f77e6a1a9ef8` / `bd6f8bbd6d9c2f28b55f2d700c9c2a9232e659c128be7913e71718317e07c69f`。尚无 RK3588 效果证据，下一步先测 car002/009/010 及 car001 detector 初始化的误重捕，再决定是否跑十目标。
- 另对用户确认的 car004 逐帧几何作只读核对：126 帧真值高度从 125 帧 `53.7px` 降至 `32.9px`，tracker/同帧独立 detector 仍约 `52.6/50.4px`；127 帧真值约 `30px`，detector 仍约 `51px`。对照图 `/tmp/nt-car004-125-129-gt-trk-det.png` SHA-256 `11e9423b4a4643186a70df30d427b197687c127241b62491b0e0ac8313cf8d82`，生成脚本 `/tmp/nt-car004-overlay.py` SHA-256 `8483c66b94ab24b8328cc90e513fa60f921b1234bc68cc3b8a1acd5aa1804c08`；绿真值从 126 帧起只框桥上方可见车身，红跟踪和青 detector 仍框整车。已经向用户核对这几帧应按完整车、可见部分还是无框部分遮挡评测；在回复前不改真值，也不按此目标定制算法。离线 10 目标颜色直方图筛查 `/tmp/nt-color-audit.py`、`/tmp/nt-color-spatial-audit.py` 显示，以能在 car010 149 帧触发的阈值，分别约 `957/2471`、`580/2471` 个现标可见帧也会触发；否决把简单颜色阈值用于该帧提前遮挡判定。这是近似离线诊断，不是新板端闭环评分。

## 2026-11-18T02:04:02+08:00 car010 边界信号审计

- 只读核对 `69fae4bbec8fe05634ff8a167dd5c3ef60e25b77` 板端正式回放 `/tmp/nt-tracking-geometry-69fae4b-full/runs/size-gt-full/car_010/frames.csv` 与对应 `score.json`、`rk3566/nanotrack_v3/src/long_term_runtime.cpp`：149 帧仍 `TRACKING/accepted=1`，Recent PSR `8.175270`；detector worker 当帧 `ready`，没有 149 帧新 batch。当前视觉遮挡探针要求**前一帧已是 SUSPECTED**，故不会在 149 帧触发；第一次同步视觉探针在 157 帧，158 帧才进入 SEARCHING。169 帧 `SEARCHING`，Recent/Safe PSR `5.539/5.544`，无选中 detector 近邻框；170 帧首次 `AWAITING_CONFIRMATION`，171–174 帧有 `UNRESOLVED_OVERFLOW`，175 帧重新开始确认，176 帧实际重捕。用户要求遮挡判定不晚于 149、重捕不晚于 169；当前证据表明仅降低现有 PSR 或搜索最短时长不能直接满足此边界。
- 主机离线脚本 `/tmp/nt-brightness-audit.py` SHA-256 `9e8900440e26ea00ed680bec6c858503e1e8265f266433c418d80eab97f30646` 对原视频 BGR、Recent 原始框及近期已接受框计算亮像素占比。car010 的 149 帧相对近期参考约 `1.05`，亮度并未骤降；154/155 帧才约 `0.64/0.55`。作为反例，仍可见的 car002 在 71–73 帧约 `0.81/0.75/0.64`，故简单亮度门会误伤可见目标。该脚本是诊断近似复算，不是板端算法输出或新的正式评分。用户确认 car010 149–169 帧大部分被遮挡；现存标注包却将 149–168 标为 `full_occlusion`、169 标为 `visible`，不能将“部分可见”自动解释为可靠完整框。保留原标注和评分不改写，后续需版本化可见性语义并校准早期遮挡识别与身份安全的重捕证据。

## 2026-11-18T01:58:44+08:00 视觉负证据候选完成十目标双初始化板测，因回退撤回

- 起点 `codex/ten-target-smoothness@c16e4e1e13774f18f342ad12d5d92f51ab9f39bc`；主机源包 `/tmp/nt-visual-negative-c16e4e1.tar` 和板端 `/home/neardi/Project/SOT/nt-visual-negative-c16e4e1.tar` SHA-256 均 `d1640d952368cdeaaf0b393e83378c42400183472b43ad8d391fdbd09ed65301`，隔离板端根 `/home/neardi/Project/SOT/nt-visual-negative-c16e4e1`。RK3588 原生 CMake/build/CTest `23/23 PASS`，日志 `cmake.log`、`build.log`、`ctest.log` SHA-256 分别为 `623799a768b76a81e340094f0c83c3fd144784c6ce48eccdc6d683d6e3fba88e`、`7f22670f678936354515b2f1a1de83014de04cbb4c43857455e380dd935d85f2`、`7ba306043d1031958335aeb63393cc9babc9818699725ab4e3196e40ef7736f5`。程序 `build/nanotrack_v3` SHA-256 `8574a924f7c339d83c1b9f028283e7613424eb5f910fe2e6f29fcb64a58c836d`；运行配置 `runs/headless-on.conf` SHA-256 `47335fd5f3e8a398e6cd9682f6aa65fb3ae40bc6ac072d48ccb0d85a0e80168b`，controller 配置 SHA-256 `185885a4b7e8090754a91cbd45557823bda19da38cda03ed92b6d2ee6298d350`。视频 SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`，tracker manifest SHA-256 `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`；detector 与 RKNN API/driver 身份沿用上节同板隔离验证，实际运行环境 API `2.1.0`、driver `0.9.8`。
- 实际执行 `bash runs/nt-visual-full-gt.sh && bash runs/nt-visual-full-det.sh`，脚本 SHA-256 分别 `8ad3051a066d8d0477d00ebec6a549b58660899f8c9dd80520a63b97d044575b`、`9989d4b9c6acef94229da483222fe90e6af920c4b88f2a1e9493b20c7f2a7f7d`；十目标真值首帧和 detector 首帧共 20 路，每路 271 帧。板端原始输出 `runs/visual-{gt,det}-full/car_###/`；板端 `/home/neardi/Project/SOT/nt-visual-negative-c16e4e1-full.tar.gz`、主机 `/tmp/nt-visual-negative-c16e4e1-full.tar.gz` SHA-256 均 `bfb7a9d92ced9d04c17b921c64c545251d309f133e7488ba369f27249d5aac7f`。主机正式评分 20 份在 `/tmp/nt-visual-negative-c16e4e1-full/runs/visual-{gt,det}-full/car_###/score.json`，摘要 `/tmp/nt-visual-negative-c16e4e1-summary.json` SHA-256 `b0cdda45671768db1c63789c9d405cd01de586623b693d4b67db5da73a51831c`；评分脚本 `/tmp/nt-score-visual-negative.py` SHA-256 `b9a6b5e6e6037c733c70173a4939ba18d8b168cfed78836716159755762caa3f`。
- 正式真值/检测初始化均 **6/10 PASS**，通过 001/002/005/006/007/008，没有提升总通过数。真值 car004 的 131 帧开始搜索，`prediction_continuity` 从 FAIL 到 PASS；采用用户确认的 car004 第 42 帧“可见但无精确框”版本真值时，检测初始化 car004 PASS、真值初始化仍因可见 127–129 帧框偏差 FAIL。真值 car009 虽在 116 帧提前搜索，129 帧重捕 IoU 从基线 `0.635` 降为 `0.529`，134/136 帧 IoU 从 `0.680/0.663` 降为 `0.415/0.431`，新增 `visible_boxes` 失败；265–270 帧未及时进入搜索，新增完全遮挡语义失败。car010 的 149 帧仍接受了观察，176 帧才重捕；与用户要求“149 前识别遮挡、169 前重捕”不符。因此此强负证据门整体**否决**，不合入 main/Debug，不作为新基线或生产 handoff。源代码和相关单测已从当前分支撤回到 `c7c4de24cc5870d91b0e9fba96e74e89373db55d` 等同状态；以上历史板测证据保留。
- 撤回后主机实际 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure`，`23/23 PASS`；日志 `/tmp/nt-visual-negative-revert-build.log` / `/tmp/nt-visual-negative-revert-ctest.log` SHA-256 `039bfdfd9243b3f90c5b63239ea0dbbc77cf2b77f3099a817306c659d4551e75` / `105288edf910501314037617e146748aca748ef4681211f67026cbc4ec3e5027`。`git diff --quiet c7c4de24cc5870d91b0e9fba96e74e89373db55d -- rk3566/nanotrack_v3` exit 0，算法源码等同撤回前可信候选。Handoff：`car_004_visible_unboxed_v2` 仍待协调契约采纳；该可见性修正不提供第 42 帧 IoU 真值。十目标应继续采用对应目标真值，`car_010` 的 149–169 帧按用户说明属大部分遮挡，遮挡状态需不晚于 149 帧识别、重捕需不晚于 169 帧。本次未改动其标注源包。下一步分析这两处失败的通用运动/视觉信号，不按目标号定阈值。

## 2026-11-18T01:42:11+08:00 强视觉负证据阻止可疑清除候选（主机通过，待板端）

- 起点 `codex/ten-target-smoothness@c7c4de24cc5870d91b0e9fba96e74e89373db55d`，工作树干净，仍以 `69fae4b` 正式 20 路和本分支 `f4044e9` 20 路作为基线。逐帧显示 car004 GT 路 131 帧、car009 GT 路 116 帧均在 `SUSPECTED` 之后运行了当帧视觉探针，亮度相对可信参考分别为 `0.621/0.518`，探针没有近邻同类框，控制器却 `SUSPICION_CLEARED` 并接受观察；car002 GT/DET 的可见 71 帧也有探针 miss 并清除，但亮度比 `0.814/0.893`。其他可见帧也有低亮度探针 miss，故不能将 miss 本身变成搜索。候选只在**已有可疑、有效当帧视觉探针 miss、亮度比≤0.65，且控制器本来会直接清除可疑**时否决清除并转 SEARCHING；原来仍属降质/暂态的路径不强制转搜索。`visual_negative_clear_veto_bright_ratio=0` 默认关闭，仅在 `visible-dual-gt-visual-gate.conf` 明确设 `0.65`；必须小于既有视觉探针门 `0.90`。新增 `VISUAL_NEGATIVE_CLEAR_EVIDENCE` 日志说明证据，配置身份仅在启用时附加新参数。
- 主机修改通用运行时、控制器、配置和相关单测；测试覆盖可疑清除与负证据冲突、原有降质仍为暂态、detector 无结果不能算负证据、配置非法值。实际 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure` 23/23 PASS，日志 `/tmp/nt-visual-negative-host-build-final.log` / `/tmp/nt-visual-negative-host-ctest-final.log` SHA-256 `00cc36617e11f57a915eec10f3e45885061a2905c91b65de48c2b35c8ff60e68` / `e0af24f43ecf81969316bd72d3e21d2dd85139ce9f9723eff4158693cfd427d2`。曾有两项测试因新 fixture 的计时设置不合法/清除观察窗不足而失败，修正 fixture 后重跑全绿；没有把初次失败当作代码效果。下一步板端原生构建，先用 car002/004/008/009 检查可见误搜索和遮挡语义，若有益且无明显退步再跑十目标双初始化；未据主机单测晋级。

## 2026-11-18T01:32:46+08:00 car004 第 42 帧可见性版本化修正（待协调契约采纳）

- 起点 `codex/ten-target-smoothness@6a6c61804795f0cdbc193eadf8c6d5c74ef05401`，工作树干净；用户明确确认今天新增第 2 目标 `car_004` 第 42 帧可见，但未提供精确框坐标。用 `python3 /tmp/nt_prepare_car004_visible_unboxed_v2.py`（脚本 SHA-256 `29941eea8de95993a291d32843ea33206d45c3b26016244470e3769de140d665`）从原包生成新版本：42 帧 `target_state=visible`、`review_status=user_confirmed_visible_unboxed`，正式 label 仍空，`ordinary_iou_allowed=false`；删除仅该帧的错误 `full_occlusion` 事件，并更新计数、来源及 checksums。没有将相邻帧插值框充当正式真值。版本包 `/home/u20/single-object-tracking/data/occ_video_1/ground_truth_versions/car_004_visible_unboxed_v2/`，其 `checksums.sha256` SHA-256 `c8c394b41c2cb489f33a1b7e631cd1e9776cd9d75c15a5860ec221e2ddda1126`，`sha256sum -c` 全项通过；备份归档 `/tmp/nt-truth-car004-visible-unboxed-v2.tar.gz` SHA-256 `42fe28818f4a4d85031b62e9b8f8864f408e4b82e9a2449daff245b0222ee98d`。旧 canonical `ground_truth/car_004/checksums.sha256` 仍为 `c04048811d0e91c28d835035d1bc2bd6d66d511672bfc9b8c09625b1e6cc1379`，未修改。
- 为试算，复制当前评测器到 `/tmp/nt-eval-car004-visible-unboxed-v2.py`（SHA-256 `6984f068350ffbb1398cb5e869a7ec679ced5fe008c11d58e3ba709e1cef9b16`），仅切换 car004 包哈希并允许显式 `user_confirmed_visible_unboxed` 跳过该帧普通 IoU；正式仓库评测器和其他目标身份均未改。对同一 271 帧板端输出，用 `--dataset-root /tmp/nt-truth-car004-visible-unboxed-v2 --target-id car_004 --scope full` 分别重算 `69fae4b` 基线和 `f4044e9` 中心候选的 GT/detector 初始化：基线 GT 仍 FAIL（131/132 遮挡、127--129 可见框、预测连续性），基线 detector 改为 PASS；中心候选 GT 仍 FAIL，中心候选 detector 改为 PASS。报告 `/tmp/nt-car004-visible-unboxed-v2-scores/{base-gt,base-det,center-gt,center-det}.json` SHA-256 依次 `9ed365b651f9c897a0acb660569ebf19114026720f8dd6be13da095d78029ca3` / `15531460e80bd06d52778892adc203c6c0e3d821be07eecddb58f0278d9c52ba` / `5f0059b94b0587f90339d2c7ec5b47c832dbfea3fbea8e26fb29f5f56d2e1d22` / `6dcf51b9872c923d95fc57e917828dffc77760d64d036172eb063960931d7adf`。仅是**修正包反事实评分**，不能与当前 canonical 正式 6/10 PASS 混称。
- Handoff：请协调任务审查新真值版本及 `user_confirmed_visible_unboxed` 语义，决定 canonical 路径、评测器身份和 `integration-contract.yaml` 更新；在采纳前正式基线仍使用旧包。若后续补得第 42 帧精确人工框，应再增新版本并纳入该帧 IoU，而非覆写当前或历史版本。本任务继续处理 car003/004/009/010 的真实算法失败。

## 2026-11-18T01:29:25+08:00 遮挡外观与重捕三路证据反例（仅离线诊断）

- 起点 `codex/ten-target-smoothness@e9efc71fb914ca559a95cf1b4c9b9dd3c632ff0f`，工作树干净；没有改动算法、真值、评测器或板端配置。复用上文已核验的 20 路 `69fae4b` 逐帧记录、271 帧独立 detector 烟测及 `age0` 三路隔离回放。主机实际运行 `python3 /tmp/nt-hog-detector-intersection.py > /tmp/nt-hog-detector-intersection.txt`，脚本/结果 SHA-256 `92f84fe91b224be06ce68211f7fd46317f57e0f8b073bfe522a06b929a218504` / `f81400ab5e0756863b98a6f7ed82c6e5cb155cf0714fcda01bdabb3bb49cbd43`；该脚本将已有 HOG 趋势异常与**同帧独立全帧 detector** 对 Recent 原始框 IoU<0.4 相交，非运行时闭环判定。
- 相交条件在 GT/detector 初始化路仍分别误触发 **18/12 个可见帧**，同时命中 **21/17 个遮挡帧**。GT 路 car010 的 149 帧命中，但 detector 初始化路的 149 帧没有命中；GT 路 car003 可见 127--129、car004 可见 128--130、car009 可见 110--112 均会被误判。且运行时 detector 在 car010 的 149 帧没有新批次，无法把离线同帧命中直接迁入在线策略。否决该组合的直接判遮挡规则；不能用较少误报掩盖可见目标的连续误冻结。
- 主机实际运行 `python3 /tmp/nt-recovery-triad-compare.py > /tmp/nt-recovery-triad-compare.txt`，脚本/结果 SHA-256 `7052d9d855d6894cbb758244371b1a234fe91bb04656d212d80df0bed2173800` / `19a9979c22cb203cc17155f1e5a3b40778d6fee3c50e9f9d9a9a34c5eb255f8e`。`age0` 中 car010 首次重新可见的 169 帧仍 `IDENTITY_CONFLICT`，Recent/Safe 相互 IoU `0.952`，但 Recent 与真值仅 `0.319`；本帧尚未收到 169 帧 detector 批次。170/171 帧 detector 与真值 IoU `0.844/0.842`，Recent 与 detector 仅 `0.363/0.339`，说明双模板高度一致也可能共同偏移，当前冲突并非只因距离权重不足。反例 car002 错误候选在 218/219 帧与 Recent IoU `0.729/0.586`、两模板 IoU `0.871/0.852`，但 detector 与运动预测不重叠，且候选中心离预测大；仅放宽三路重叠或减少确认帧会重演 220 帧错捕。下一步需能区分遮挡中的同类近邻与目标的独立身份/可见性证据，再调整冲突裁决；不为 car010 帧号写特例。

## 2026-11-18T01:21:29+08:00 重捕最短等待归零反例（否决）

- 源码仍为 `codex/ten-target-smoothness@f4044e9c3e5afeae8c52605499c025a0c7cdeb24`，板端隔离目录 `/home/neardi/Project/SOT/nt-center-area-f4044e9`、原生程序 SHA-256 `c6102537dcb0b74eadb393ad3be4fa5137c0ce1e025ae601b45c83a7fd19d455`，视频/模型/环境与下节相同。只把 controller 的 `search_reacquisition_min_age_ms=400` 在新文件 `runs/age0/controller.conf` 改为 `0`，SHA-256 `c4670ce80984f86c3cdbbd75baa7ba9756b7b5523c44854d401f7e914545fe4a`；运行配置 `runs/age0/headless.conf` SHA-256 `b04d1ba534344b7b7165f72985d064430edf5350995512f627de5b97af13c7b5`。原配置未覆盖。
- 板端实际用 `./build/nanotrack_v3 --config runs/age0/headless.conf --bbox <各目标首帧真值框> --output-boxes runs/age0/<target>/frames.csv --session-metadata runs/age0/<target>/session.json` 回放 car002/009/010 各 271 帧。原始板端 `runs/age0/`，板端 `/home/neardi/Project/SOT/nt-center-age0.tar.gz`、主机 `/tmp/nt-center-age0.tar.gz` SHA-256 同为 `ae496c94a48c69ac02ef6d88976bbd8391c264009a4e88352a055917f99419fd`。主机正式评测器 `scripts/evaluate_occ_video_1.py --scope full` 分别评分；`/tmp/nt-center-age0/runs/age0/car_{002,009,010}/score.json` SHA-256 `c3e1f1deef81b231056db56eddc990623a0e208e3096c3e6638c449191086559` / `37fb1f54e828af849ff0023ee1418db89fd974f27b6b7487864d197c0362f47b` / `c0d5a7d81d4c0fc5592f3c5c29d10e917cc63c0b4e2dcaa19d9f5b0eea49c422`。
- car010 149--155 帧仍错误接受、176 帧才重捕，时序完全未改善；160--167 帧虽出现 detector 恢复候选，却持续 `AWAITING_CONFIRMATION` 或 `IDENTITY_CONFLICT`。car009 仍在完全遮挡时接受。car002 从原 `400 ms` 配置的 228 帧正确重捕、PASS，变成 220 帧对错误目标 `RECOVERY_STAGED`（框从约 `x=848,y=691` 跳到 `x=905,y=739`），正式评分 `identity_proxy/reacquisition/visible_boxes` 三门 FAIL。**否决单独缩短等待**，不合入默认/调试配置，也不把此试验视为新基线。下一步需解决 car010 的 149 帧可观测遮挡证据与 169 帧候选身份确认，不能以等待时间代替身份门。

## 2026-11-18T01:14:04+08:00 输出中心候选 RK3588 全量回放与 car010 时序核查（未晋级）

- 工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，分支 `codex/ten-target-smoothness`，源码 `f4044e9c3e5afeae8c52605499c025a0c7cdeb24`，相对基线 `69fae4bbec8fe05634ff8a167dd5c3ef60e25b77`。源码包主机 `/tmp/nt-center-area-f4044e9.tar`、板端 `/home/neardi/Project/SOT/nt-center-area-f4044e9.tar` SHA-256 `37003fbdeec19dd76177f41a7036b8cf01448904ef42648dfe66f83a70573b7f`；板端隔离目录 `/home/neardi/Project/SOT/nt-center-area-f4044e9`。板端原生 CMake/build/CTest 23/23 PASS，日志 SHA-256 `e4a540e6bd944284098a8a6b81a6ce866205e7f4a2d40bf5dbd26150f691a79e` / `e8c809f12054e2d364096f77344c4e21829dad5746045cc64f4ccfbe8edf8c70` / `24f4975a6ea19fdc4df54abba64d9c0335abfc9160cf91b28b97fe1363ee9215`。程序 `build/nanotrack_v3` SHA-256 `c6102537dcb0b74eadb393ad3be4fa5137c0ce1e025ae601b45c83a7fd19d455`，配置 `runs/headless-on.conf` SHA-256 `807a5cf7b0d75a3f103958d663eee9259e0833196b09d4c608471b06f7a9d309`，模型 manifest SHA-256 `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`；视频、detector 和 RKNN 环境与本文件上一节相同且板端已核验。
- 板端实际运行 `bash runs/nt-center-full-gt.sh`、`bash runs/nt-center-full-det.sh`，两脚本 SHA-256 `e46ed909faf2fd84eb1f2bd91190b817a2bfc449d8c3c2a12ecee2ef236dad52` / `19c149c703d8fbd8f73a1b7a5589e4bf6d9f15467a02a29ed656a47f076d2a3e`。两种初始化各十目标、每路 271 帧完整；原始输出板端 `runs/center-{gt,det}-full/<target>/`，归档板端 `/home/neardi/Project/SOT/nt-center-area-f4044e9-full.tar.gz`、主机 `/tmp/nt-center-area-f4044e9-full.tar.gz` SHA-256 同为 `c3b6a1233d4cfcefdbe9587c7cbbb5f07986278295223478ce9c4380113ced38`。主机实际运行 `python3 /tmp/nt-score-center-area.py`（SHA-256 `5d0e945a583d04f1eb4b2e8d7719feb6e11fced698f892744e4c9dfd9aff1b0e`）按原始正式十目标包评分；摘要 `/tmp/nt-center-area-f4044e9-summary.json` SHA-256 `125bafe71679e6d0922f7ddcaa381430f9131d23f916d7830513736f48921bbd`，20 份正式 `score.json` 与逐帧 CSV 在主机同名目录。
- 正式评分 GT/detector 初始化各 **6/10 PASS**，通过集仍为 001/002/005/006/007/008，003/004/009/010 仍 FAIL。输出中心候选的可见观测平均 IoU 相对 `69fae4b`：GT 路 car007 `0.8097→0.8383`、car009 `0.7979→0.8099`，但 car001 `0.7837→0.7767`、car005 `0.8456→0.8389`；detector 路 car009 `0.8078→0.8202`，car001 `0.7720→0.7655`。新八目标 GT 路可见中心平均误差 car009 `2.476→1.442 px`、car008 `2.484→1.407 px`，但 car005 `1.844→2.158 px`；若以连续可见帧的真值相对误差变化衡量抖动，car003 `0.978→1.042 px`、car005 `0.813→0.863 px`。20 路的 `state/accepted/reacquired/recovery_reason` 等状态字段与基线逐帧零差异，故该候选没有解决遮挡时序，也不是新的十目标基线；下一步需对中心修正做独立证据门，避免给已准确的目标施加偏移，或撤回该候选。
- 用户确认 `car_004` 第 42 帧可见；现行真值仍错误标为 `full_occlusion/no_bbox`。仅在 `/tmp/nt-truth-car004-f42-visible` 做了**临时反事实**：框 `[821.25,753.94,43.42,46.66]` 是 41/43 帧框的插值，不是用户确认的逐像素框，不能标成正式手工真值。临时包 `/tmp/nt-truth-car004-f42-visible.tar.gz` SHA-256 `d2073b06168099df821b1cf98a47969075374bb66eb9620d9bf5483b181fbbe7`，临时评分 `/tmp/nt-car004-f42-corrected-score.json` SHA-256 `e61562e28328266a5ad6a638b8001e37ca4e8a13d37b57e4f0fc11dd22972116`，**不计正式成绩**。源包未更改；需在确认第 42 帧精确框及标注来源后，将修正做成可追溯的新版本并更新协调契约，不能以这份插值包覆盖旧历史。
- `car_010` 正式真值 149--168 标完全遮挡、169 为重新可见；用户补充 149--169 大部分被遮挡，要求不晚于 149 判遮挡、不晚于 169 重捕。基线和本候选均在 149--155 继续接受，156 首次拒绝、158 进入搜索、159--169 因 `search_reacquisition_min_age_ms=400` 返回 `SEARCH_MIN_AGE`、170 才开始候选确认、176 才 `RECOVERY_STAGED`。149 帧 Recent PSR `8.175`、相对可信基线 `-0.019`，APCE `17.914`、相对基线 `-0.014`，框未显著缩小；此前已否决的 Safe 双模板诊断回放 `/tmp/nt-dualprobe-bcbce9b-ten/runs/dual-probe-ten/car_010/frames.csv` 中同帧 Safe PSR `8.168`、双模板 IoU `0.931`，亦未给出早期降质证据。独立全帧 detector 在 148 帧命中、149 帧目标近邻无检测，但现行异步 detector 约每三帧返回一次、149 帧没有新结果。全帧烟测 car010 的其他**可见**帧有 12 次“前帧命中、本帧漏检”，单次漏检不能独立冻结目标。单帧 detector f149 总耗时约 64 ms，强制每个 30 FPS 帧阻塞检测会越过帧预算。仅缩短 400 ms 等待可能在 162--168 的遮挡期错误重捕，须先获得更可靠的遮挡证据，再闭环测试恢复门；现有算法未达到用户时序要求。
- Handoff：本候选不合入 `main`/`Debug`，不申请生产 handoff。协调任务需处理 car004 真值版本和契约；本任务继续研究通用遮挡证据并保持 car010 时序门与十目标双初始化回归，不按目标编号写判定规则。

## 2026-11-18T00:55:07+08:00 全帧 detector 支持审计与输出中心面积权重候选（主机通过，待板端）

- 起点 `codex/ten-target-smoothness@706bb2771b4370478464455ebbe787b0e9337724`，沿用 `69fae4bbec8fe05634ff8a167dd5c3ef60e25b77` 的 20 路板端运行与同一 canonical 视频/十目标真值。RK3588 实际运行 `bash /home/neardi/Project/SOT/nt-occlusion-detector-probe.sh` 和 `bash /home/neardi/Project/SOT/nt-full-detector-audit.sh`，脚本主机 `/tmp/nt-occlusion-detector-probe.sh` / `/tmp/nt-full-detector-audit.sh` SHA-256 `a391e7e0277177f8d27a9d1b61ef3a6c17f776a8d1d26053fd59788831969c1c` / `cb0609c00730522cc1a9c14a269f2aa0d859435f164d215204c4f28458cb5447`。板端程序 `/home/neardi/Project/SOT/nt-tracking-half-8e596d5/build/yolo_detector_smoke` SHA-256 `3ba28a7afa1e5aa9c09a551f1fbb2b0e137e2b0006ecb1941fdd7fe8d9712f72`，视频 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`，VisDrone9 RKNN/manifest/labels `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c` / `ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9` / `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`；烟测日志为 RKNN API 2.1.0、driver 0.9.8。参数 `--conf 0.25 --nms 0.45 --class-id -1`，每帧最多输出 100 候选；全段 271 个 CSV/日志齐全。板端 `/home/neardi/Project/SOT/nt-occlusion-detector-probe.tar.gz`、主机 `/tmp/nt-occlusion-detector-probe.tar.gz` SHA-256 均 `053320aff02672cbd953c3e10746cf48b658906c6c227f13944a96fd580139c3`；全段板端 `/home/neardi/Project/SOT/nt-full-detector-audit.tar.gz`、主机 `/tmp/nt-full-detector-audit.tar.gz` SHA-256 均 `930ef0ea6b6d490dcbd4603c83d454aeeffc7715bd3754225d350ebc525eb124`，原始 CSV 留在板端 `runs/full-detector-audit/`。主机 `python3 /tmp/nt_detector_support_audit.py > /tmp/nt-detector-support-report.json`、`python3 /tmp/nt_full_detector_analysis.py > /tmp/nt-full-detector-analysis.json`，报告 SHA-256 `3f80db9628efa10da2f482f2fd9c3c2b730dcc7354f6004444960d0194d2a26a` / `36562db7e2226934ee2964b362803fd25fdb65a6edff9eb8f2ceb9f55c101390`；后者脚本 SHA-256 `974fd16180b22a479dbb0a31b24d717cd139846d9cbb244141db9e9f697f142b`。
- 以真值有效可见帧、同类 detector IoU≥0.5 计算的**独立单帧烟测**命中：car001--010 分别为 `258/258, 251/252, 201/208, 213/234, 253/259, 256/256, 263/265, 258/261, 254/259, 190/251`；car004 可见段最长连续漏 14 帧，car010 最长 13 帧，car009 遮挡前 110--112 连续漏 3 帧且 120 帧在标注遮挡区出现 `0.542` 的同类候选。该结果受 100 候选上限、IoU 门、离线全帧运行与现有异步调度差异限制，不能等同运行时召回；进一步否决“detector 未检出即遮挡”或“检出即接受”。car004 第 42 帧单帧检测框与 tracker IoU `0.892`、置信度 `0.810`，并与两侧可见真值框连续。
- 按完整检测结果做静态、逐帧**非闭环**中心反事实：已确认唯一同类检测候选、IoU≥0.4、归一化中心近邻时，以 `weight=clamp(detector_area/tracker_area-1,0,0.5)` 调整输出中心；真值初始 car003--009 平均中心误差均下降，例如 car009 `2.476→1.622 px`，旧 car001 `4.015→4.055 px` 略退步；detector 初始 car009 `1.719→1.461 px`、car001 `4.110→4.127 px`。主机代理 `/tmp/nt-center-area-weight.txt` SHA-256 `d2bac5c30fe2b820ce6cd12a058f7f00abd1e5ec3d7c0abe62e805dc0f0783c0`；不把反事实当板端闭环收益。基于既有两次同类几何确认与新鲜期，候选只给外发 `tracking_bbox/control_bbox` 中心以面积差连续权重及现有中心平滑/最大修正限制；内部 NanoTrack 框/下一帧裁剪不变，新增 `DETECTOR_VERIFIED_OUTPUT_CENTER` 事件。单测覆盖确认前无偏移、确认后渐变、内部框不动；主机实际 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure` 最终 23/23 PASS，日志 `/tmp/nt-center-area-host-build-final.log` / `/tmp/nt-center-area-host-ctest-final2.log` SHA-256 `0f42fe5d181144919f8de05de0edb67580d56b1fedc238d76169a744d76aed2d` / `728fd66d44f326b3053b2efee7862d6b41e78b1ca1a0a85e74fccc8b31c898c5`。下一步 RK3588 双初始化完整十目标闭环与平滑性回归，若旧目标中心/通过门明显退步则撤回。
- 用户最新裁决：`car_004`（新增第 2 目标）42 帧**可见**，需修正该帧真值并保留旧版可追溯；`car_010` 149--169 帧大部分被遮挡，要求算法**最迟 149 帧判遮挡、最迟 169 帧重捕**。当前 car010 真值 `events.json` 将 149--168 标 `full_occlusion`、169 标 `first_visible_after`，暂不改其标签/验收边界。中心候选不改变状态机，不能解决此时序门；后续单独迭代遮挡判定。

## 2026-11-18T00:29:43+08:00 十目标遮挡误判反例审计：否决单一风险/外观门

- 工作树 `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，分支 `codex/ten-target-smoothness`，分析基线 `750241e97d4f24500ca25e4d5a00bfd14ea0420b`；沿用 RK3588 `69fae4bbec8fe05634ff8a167dd5c3ef60e25b77` 的 20 路板端原始 `frames.csv`，归档和程序/配置/模型/视频身份见下节，未重跑推理或修改算法、真值。主机实际运行 `python3 /tmp/nt_risk_audit.py > /tmp/nt-risk-audit.txt`、`python3 /tmp/nt_appearance_audit.py > /tmp/nt-appearance-audit.txt`、`python3 /tmp/nt_texture_audit.py > /tmp/nt-texture-audit.txt`；脚本 SHA-256 依次为 `549762150a69e60062b26ed4b49e24713a91cf77ad51d2b53409b45aa52d38b0` / `94d7f1d088c9804df8d24950c51fbb8b62514e3fd80b2846cb26db0a87d2c86b` / `ad5839c658d5ec7b15bd281b93f672ed6e2816a9a2365e6e84c892244c4d12a6`；输出 SHA-256 依次为 `674a1c3623f87ce5c10cbcea87a84c8b532c2b097f11e1c2e472ae7dbddf31a5` / `1dd292e7c3389637ab14cb1f86a5e5531ed4f6942e5d272475521fa7bff1c4f1` / `03968c49eea61da3dcf67c137bc03d97baa827010ab4573d3b24bd79f4bd28ce`。外观审计使用同一视频经 ffmpeg 导出的 271 张 JPEG、PIL/NumPy 对当前 Recent 原始框计算简易 RGB 直方图和 HOG 相似度；只作离线反例筛查，非线上分类器或独立验证集。
- 将连续 3 帧内 2 次 `RESPONSE_AND_INNOVATION_RISK` 直接解释成遮挡，在真值初始和 detector 初始路分别误触发 **190** / **160** 个可见帧；连续 5 帧内 3 次也分别误触发 **165** / **138** 个可见帧。该状态本身不能作为进入 SEARCHING 的通用条件。`car_009` 真值初始在 115 帧短暂 SUSPECTED，116 帧 `SUSPICION_CLEARED` 后于 116--120 帧接受路牌；detector 初始连 115 帧也未进入 SUSPECTED。`car_003` 130 帧已经 SUSPECTED 且未接受，131 帧才 SEARCHING。这三种误差不能用一个风险计数阈值修复。
- 简易外观相似度也无可靠统一阈值：`car_009` 真值初始接受的完全遮挡帧 RGB 直方图相似度 `0.929--0.986`，与可见帧第 5 百分位 `0.919` 重叠；HOG 遮挡帧 `0.555--0.780`，可见帧第 5 百分位 `0.631`。`car_010` 虽逐渐下降，但 149--153 帧有部分黄色车身，且相似度变化从标注的完全遮挡起点之前已开始。否决把这些静态阈值直接加入算法；继续研究独立目标证据、可靠响应基线及恢复状态机，并用完整十目标双初始化闭环检查。
- 编号复核：新增第 2 个目标 `point_02` 为合并目录 `car_004`，真值第 240 帧可见、第 241 帧起 `out_of_view`；新增第 4 个目标 `point_04` 为 `car_006`。`car_004` 真值第 42 帧标 `full_occlusion/no_bbox`，但逐帧图 `/tmp/car_004-occlusion.jpg` SHA-256 `6853bb97d174ee4fe17d846b20499de53f8beac702cf6b2f2a1cc36e068d4798` 显示银灰车与 39/41/43/44 帧连续可见；`car_010` 真值第 149--153 帧标 `full_occlusion/no_bbox`，图 `/tmp/nt-occlusion-audit/car_010.png` SHA-256 `11deb07c51209c0ec37d0a4b7ef076898de0d199237ca8ccc10567ef5525cd91` 显示黄色车仍有部分车身。对应真值 `frames.jsonl` SHA-256 为 `8c65462f1ef97acd0a20bade2615d57d97727296eebee6bfffbb29721fc20cff` / `68729304b6356219dcd4fbd118b5040d71ebd4acc1410cea3be285e50261badb`。已向用户请求标注语义裁决；在收到前不修改真值或评测器，不用单帧号规则迎合现有标注。当前 6/10 PASS 结论及未批准合并/生产 handoff 状态不变。

## 2026-11-18T00:18:41+08:00 尺度一致性门板端全量：car009 可见框门修复，总体仍 6/10

- 实验源码 `codex/ten-target-smoothness@69fae4bbec8fe05634ff8a167dd5c3ef60e25b77`；源码包主机 `/tmp/nt-tracking-geometry-69fae4b.tar`、板端 `/home/neardi/Project/SOT/nt-tracking-geometry-69fae4b.tar` SHA-256 均 `f4be1fbd3245a15a7acbf031750d9b39b647b3e9fb496b07ccb96a41f9cd809e`，准备脚本 `/tmp/nt-tracking-geometry-setup.sh` SHA-256 `a5b07e04c4155546622d6d96f3f10092beacd5841643ebf9397968a1496cc599`，隔离目录 `/home/neardi/Project/SOT/nt-tracking-geometry-69fae4b`。板端原生 CMake/build/CTest 与前版同选项，23/23 PASS；`cmake.log` / `build.log` / `ctest.log` SHA-256 `4978681493fa1a16e7a8c7ceea5bc0a4c09a46755b0258e592b308310cd3494d` / `7e0d287732556db639eb73635bf9eb9bf0c33e7cf5df376dd5ae652348b21c6e` / `1fe4b20cf7f945b6a7d50e7f2ee40b577829a85460b78fc8f56f9c09a2a72d00`，程序 `build/nanotrack_v3` SHA-256 `81a1a7a99ad942c352afc244d5097841f5972531050b77d04597cda0394a39c2`，运行配置 `runs/headless-on.conf` SHA-256 `250c0e649cf4c8cbf238132f201807eafeaee105c4e4b5bea2dc03031f8f1930`。板端核验 controller 配置、视频、tracker manifest、VisDrone9 RKNN/manifest/labels SHA-256 依次为 `62607f7eb3798737fc927a1f997e0d52cc5f78a7250d8df8b23aebfb320d5f44` / `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d` / `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261` / `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c` / `ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9` / `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`；run.log 为 RKNN API 2.1.0、driver 0.9.8。
- 实际 `bash runs/nt-size-full-gt.sh` / `bash runs/nt-size-full-det.sh`（SHA-256 `9c3b81b80e9134f61f648f534133fe5b1a18e3799c4613dd750a6db9240fbe5a` / `49530b8642251f2561f5e0c3dd737d1a5f1fca4009fcfed7194255540bc962d0`），同前版十目标首帧真值框/detector 框，20 路每路 271 帧；板端输出 `runs/size-{gt,det}-full/<target>/`，板端 `/home/neardi/Project/SOT/nt-tracking-geometry-69fae4b-full.tar.gz`、主机 `/tmp/nt-tracking-geometry-69fae4b-full.tar.gz` SHA-256 均 `925d46727a818188522c237b11fb48f041d89c8e849f3d9d17d84e2fc3d9afa1`。主机 `python3 /tmp/nt-score-tracking-geometry.py`（脚本 SHA-256 `a61b8fe57c99e4cab80022132a7df4fccce62fca9bde101c5201cef9fb5ad77b`），正式评测 `--scope full --source-commit 69fae4bbec8fe05634ff8a167dd5c3ef60e25b77 --config-sha256 250c0e649cf4c8cbf238132f201807eafeaee105c4e4b5bea2dc03031f8f1930 --model-sha256 3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`；20 份 `/tmp/nt-tracking-geometry-69fae4b-full/runs/size-{gt,det}-full/<target>/score.json`。摘要 `/tmp/nt-tracking-geometry-69fae4b-summary.json` SHA-256 `54ce11e922a5b6b5557978e83e02e15642751602d58c97bd706fe2460e3fb5ce`；相对 8e596d5 的逐目标对照 `/tmp/nt-tracking-geometry-69fae4b-compare.json` SHA-256 `2afd3f24678d9ecc1364a20e6c900a4ede3c29fdf5f9fdb58d0a9bf108eb188c`。
- 两种初始化仍 6/10 PASS，集合均为 001/002/005/006/007/008，**无通过集退步**。真值初始 car009 可见观测 IoU `0.766489→0.797886`，247/253/257/258 帧输出宽度约 `28→36 px`，可见框门从 FAIL 变 PASS；仅剩 115--120 帧完全遮挡仍接受的问题。真值初始 car008 `0.778886→0.810109`，car007 `0.835456→0.809724`（仍 PASS），car002 保持 `0.873143`。其余门基本不变；20 路内部状态、接受/重捕、模板与运动预测字段相对 8e596d5 **零差异**，说明改善来自输出尺寸而非重捕时序。该通用尺度门有可复核收益但尚不能使十目标全部通过，未合入 main/Debug；下一步重点是 car003/004/009/010 的遮挡接受错误及 car004 真值初始的可见/预测连续性，不把 detector 缺检直接当成遮挡证据。

## 2026-11-18T00:04:39+08:00 可见帧中心诊断与尺度一致性门候选（主机通过，待板端）

- 起点 `codex/ten-target-smoothness@330e037e6b2c7f415a22e04b3e718e38af2e3fd8`；同一 RK3588 程序 `/home/neardi/Project/SOT/nt-tracking-half-8e596d5/build/yolo_detector_smoke` SHA-256 `3ba28a7afa1e5aa9c09a551f1fbb2b0e137e2b0006ecb1941fdd7fe8d9712f72`。实际 `bash /home/neardi/Project/SOT/nt-center-probe.sh`（脚本主机 `/tmp/nt-center-probe.sh` SHA-256 `60b5fa57c832a07aeef04a254d4fd86354b956acd991ec6f64ef0a66b3266e7f`）：用同一 271 帧视频抽取 126/127/128/129/200/247/253/257/258 帧，逐张运行 `./build/yolo_detector_smoke --model <上述 VisDrone9 RKNN> --manifest <上述 manifest> --labels <上述 labels> --image <帧 PNG> --conf 0.25 --nms 0.45 --class-id -1 --output-image <jpg> --output-csv <csv>`。板端输出 `runs/center-probe/`，板端 `/home/neardi/Project/SOT/nt-center-probe-8e596d5.tar.gz`、主机 `/tmp/nt-center-probe-8e596d5.tar.gz` SHA-256 均 `7dfc9db0ecf8b2d74c457a8fd704c1fb3de1f40dc89a56acc9f662b4ee4f8b69`；对照真值、tracker 与 detector 的报告 `/tmp/nt-center-probe-8e596d5-report.json` SHA-256 `a924e01fbf2681b53ffcc1322019c0ed538ea58dd9fd6cce7f8d8b808c744f81`。
- 真值初始 car009 的 247/253/257/258 帧：NanoTrack 中心误差约 4.16/2.91/2.31/3.04 px，单帧 detector 为 1.28/1.57/0.67/0.64 px；detector 与 tracker 框 IoU 仅 0.460/0.443/0.417/0.424，因此当前尺寸证据的 0.50 同帧 IoU 门把这组有用的大框排除。car002 第 200 帧相反，tracker 中心 0.61 px 比 detector 1.63 px 更准，说明不能直接总是采用 detector 中心。car004 126/127 帧 detector 中心也只比 tracker 略好（9.78/11.77 对 12.23/14.72 px），128/129 未找到覆盖 tracker 中心的候选；该段真值框高度突然变小，中心偏差不能仅靠 detector 解决。这些是独立单帧烟测，不能证明运行时异步结果会被接受。
- 候选将跟踪输出尺寸证据的同帧 IoU 下限改为现有 detector 确认阈值的 0.8 倍（当前 `0.50→0.40`），并要求 detector/tracker 中心距不超过现有尺度归一化确认容差；仍需同类、唯一、置信度足够、新鲜且连续两次几何确认，仅调整对外宽高，NanoTrack 中心和内部裁剪保持原路径。20×20 tracker 对居中 30×30 detector 的 IoU 约 0.44，属于此类尺度差；34×34 大框或错位/重复候选仍不通过。主机 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure` 23/23 PASS；日志 `/tmp/nt-size-geometry-host-build.log` / `/tmp/nt-size-geometry-host-ctest.log` SHA-256 `763a4d72e851ed500e033f18c8f090e42cf2479efbdeb0e894b67c5a81a0487b` / `436cfc91b5dda41fdf5d2385459f634de0e2b09103b70ab02ee1f6d05688a61d`。下一步提交并复跑 RK3588 十目标双初始化，确认是否修复 car009 可见帧门且无其他目标退步；不以单帧反事实晋级。

## 2026-11-17T23:59:48+08:00 等权 detector 尺寸候选：双初始化十目标无退门，仍 6/10

- 板端源码 `codex/ten-target-smoothness@8e596d5cf8e5e704f13cd573153333920b3ac0f0`，主机 `/tmp/nt-tracking-half-8e596d5.tar`、RK3588 `/home/neardi/Project/SOT/nt-tracking-half-8e596d5.tar` SHA-256 均 `e92388a8939022512ca053d724149408dbf94029f5c827286e50461aab4dfe5c`；隔离目录 `/home/neardi/Project/SOT/nt-tracking-half-8e596d5`，准备脚本 `/tmp/nt-tracking-half-setup.sh` SHA-256 `24de4af2b61edc656b8fb1f2e03dbb24f6f16586083ba73370438c0a7e6cd0b2`。原生 CMake/build/CTest 与上一尺寸版同选项，23/23 PASS；`cmake.log` / `build.log` / `ctest.log` SHA-256 `e8af996f3a7b766a477f91e10f327815caf83c867aa890adbd55162048214de1` / `86b8cc3a0f6500d2eae5cb7397f7e2ea6cf182e8ffd9450d3dfbcb8cd6ca2457` / `b1303dfafe2f2f59029974afd8754f60ed4792e61a0a5ccba19cd74a9ff3fbdb`；程序 `build/nanotrack_v3` SHA-256 `0ae160a18a8f79ce14647cf0cc4c99ac039df43e1c593c0b68dbff1c8c1193f5`，运行配置 `runs/headless-on.conf` SHA-256 `58ee6769d224431d8e9023ca8428eb0ecea7f5bc2b2fba4634446da4d3df449f`。板端实测 controller 配置、视频、tracker manifest、VisDrone9 RKNN/manifest/labels SHA-256 依次仍为 `62607f7eb3798737fc927a1f997e0d52cc5f78a7250d8df8b23aebfb320d5f44` / `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d` / `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261` / `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c` / `ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9` / `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`；运行日志仍为 RKNN API 2.1.0、driver 0.9.8。
- 实际板端 `bash runs/nt-size-full-gt.sh`、`bash runs/nt-size-full-det.sh`（脚本 SHA-256 `360613d4b5b08c493ca2f5ae14ead5789f58acbf08a7385e6b9c802c7a19de5a` / `b3f06e61965b42a99eb53909be9e8f30c71fe6646b850b02390b1291e997470a`），同一首帧真值框或 detector 框运行 20 路、每路 271 帧；板端原始路径 `runs/size-{gt,det}-full/<target>/`。板端 `/home/neardi/Project/SOT/nt-tracking-half-8e596d5-full.tar.gz`、主机 `/tmp/nt-tracking-half-8e596d5-full.tar.gz` SHA-256 均 `d85f513892f7c585ea8081be6a105c4cbe4237f5bc8faeabad0714949668bacb`。主机执行 `python3 /tmp/nt-score-tracking-half.py`（SHA-256 `3f083aabfe67264fdcaa4a9e7aef6a5a0b3759eb70584ea7af009bf713baf1b0`），使用正式评测器 `--scope full --source-commit 8e596d5cf8e5e704f13cd573153333920b3ac0f0 --config-sha256 58ee6769d224431d8e9023ca8428eb0ecea7f5bc2b2fba4634446da4d3df449f --model-sha256 3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`；20 份评分 `/tmp/nt-tracking-half-8e596d5-full/runs/size-{gt,det}-full/<target>/score.json`。摘要 `/tmp/nt-tracking-half-8e596d5-summary.json` SHA-256 `50461e0300182b726f2a0750e39e961e6c20b76e43b90eb0ad83e93a45294c4d`，与 d0dc0e7 的对照 `/tmp/nt-tracking-half-8e596d5-compare.json` SHA-256 `3b7618c8a98cfabec0ef472f6e3b214759746acb0b303427e1b05720953dfc89`。
- 真值初始及 detector 初始均仍 6/10 PASS，集合 001/002/005/006/007/008，无通过门退步。真值初始 car002 可见观测 IoU `0.894625→0.873143`，较完整尺寸权重版 `0.847421` 收窄退步；新增八目标均高于 d0dc0e7，例如 car003 `0.752566→0.813997`、car009 `0.698474→0.766489`，但 car004/009 可见单帧框门仍 FAIL，完全遮挡语义仍使 003/004/009/010 FAIL。detector 初始 car010 `0.785621→0.785533` 微降，其余目标可见观测 IoU 提高。20 路的状态、接受/重捕、模板和运动预测字段与 d0dc0e7 **零差异**，说明这次只改对外框尺寸；图像边缘裁剪使 23 个已发布中心较原版移动，最大约 2.126 px，不能声称所有发布中心完全不变。候选未合入 main/Debug，仍需处理真实遮挡与 car004/009 可见帧的中心/身份问题，不把 6/10 当成目标完成。

## 2026-11-17T23:45:39+08:00 跟踪输出尺寸改为 NanoTrack/detector 等权候选（主机通过）

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，起点 `175c2163292d8c035c093c0c7dddbf4f36d0f143`。上一完整 detector 尺寸版使真值初始化 car002 可见 IoU `0.895→0.847`，虽仍 PASS，却不符合逐目标尽量贴真值的目标。静态几何反事实以原框与完整尺寸版框线性插值，等权时 car002 约 `0.873`，新增 car003/004/005/006/007/008/009/010 均改善；这些数值未包含重新运行的时序与 detector 调度，不能当正式结果。
- 新候选保留唯一同类且两次一致的 detector 证据、NanoTrack 中心、原有每帧尺寸变化上限和内部裁剪隔离，仅把 detector 尺寸对输出尺寸的目标修正取一半；未确认或过期路径沿用原代码。单测新增多帧收敛上界，验证与 24 px detector 一致时 20 px tracker 输出最终不超过 22 px，内部 tracker 框仍为 20 px。主机实际 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure`，23/23 PASS；日志 `/tmp/nt-size-half-host-build.log`、`/tmp/nt-size-half-host-ctest.log` SHA-256 `e42f99cac8b1aceb0bfc19774a2487890197b4ede18ad714e3e082d473058763` / `bf6cb6cd8387c94ea88d44b01ff15dbbeaca15916584bd0cd8f3b0a426bf1e95`。下一步提交后用 RK3588 隔离目录双初始化全量回放，尤其检验 car002 退步是否收窄且十目标通过集不降。

## 2026-11-17T23:43:23+08:00 完整 detector 尺寸跟踪输出：十目标无门禁晋级，car002 尺寸退步

- 实验源码 `codex/ten-target-smoothness@850a518892a0f40972a937a55ee02f3a612ecabe`，起点 `dc42c24189ae126113d7a52da0325bcd429542f1`。源码包主机 `/tmp/nt-tracking-size-850a518.tar`、板端 `/home/neardi/Project/SOT/nt-tracking-size-850a518.tar` SHA-256 均 `43bbc74bb3c5320f928d232db47de528e073c887b99b06ce1622d268ed9d5167`。隔离板端目录 `/home/neardi/Project/SOT/nt-tracking-size-850a518`，准备脚本 `/tmp/nt-tracking-size-setup.sh` SHA-256 `1ef300f2f9dae568127b99d1fa83de44dd2f80426ed8537e864f4051d3960a58`。板端原生 CMake/build/CTest 与上节同选项，23/23 PASS；`cmake.log`、`build.log`、`ctest.log` SHA-256 `63458e19376305452b6c420120d1bf756104df8dcfaaebcb94d14c3d9aeb82b7` / `ddde17141aed8de1ac53b2f1031aa8e3fdaa16b30cd6311aa07b567e84e147b6` / `7bbccb982600207bdcd0e1744acb7542f234e7f148cd18369d9117f3a9d09ee3`；程序 `build/nanotrack_v3` SHA-256 `80e47e0cd5f7cd29ba6c67e839bdcf4e4c9221275a95e8089659b1cf7dc2a2b7`。配置 `runs/headless-on.conf` SHA-256 `f38adbcde1386e885ace9d0d2f11dc315730ebb566eb0045a54310a6b78a8de3`；controller 配置 `62607f7eb3798737fc927a1f997e0d52cc5f78a7250d8df8b23aebfb320d5f44`；视频 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`、tracker manifest `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`、VisDrone9 RKNN / manifest / labels `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c` / `ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9` / `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`，运行日志报 RKNN API 2.1.0、driver 0.9.8。
- 板端实际 `bash runs/nt-size-full-gt.sh`、`bash runs/nt-size-full-det.sh`，脚本 SHA-256 `c6d301d0f323b5aa67d3a27592f9a589af41a7ca99e322eea5facabd55f54692` / `b8d0b62912893f8c947b9df136820ada62fd795cf78ff5f4b6e32c2115abda59`，使用与上一版完全相同的十目标首帧真值框和 detector 框。20 路各 271 帧，原始输出板端 `runs/size-{gt,det}-full/<target>/`；板端 `/home/neardi/Project/SOT/nt-tracking-size-850a518-full.tar.gz`、主机 `/tmp/nt-tracking-size-850a518-full.tar.gz` SHA-256 均 `aff5f12d02e88f1801381efe6fd59e9e1da8bc4cf3589f5220e24c113320ef9c`。主机实际 `python3 /tmp/nt-score-tracking-size.py`（脚本最终版 SHA-256 `35efabc9ca8d2344d195dc50d3ccf05e53008c436c1514cb8e41ce3cb386f3eb`），使用正式 `scripts/evaluate_occ_video_1.py --scope full --source-commit 850a518892a0f40972a937a55ee02f3a612ecabe --config-sha256 f38adbcde1386e885ace9d0d2f11dc315730ebb566eb0045a54310a6b78a8de3 --model-sha256 3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`；20 份评分在 `/tmp/nt-tracking-size-850a518-full/runs/size-{gt,det}-full/<target>/score.json`，摘要 `/tmp/nt-tracking-size-850a518-summary.json` SHA-256 `83041304a592d11f1be0eb1bf09aaa70cb5f9dd11330dee7328a14d0858dc13d`。与 d0dc0e7 逐目标对照及九个内部状态/运动字段逐帧一致性文件 `/tmp/nt-tracking-size-850a518-compare.json` SHA-256 `3a825808fbeb1d11df0df5c94671b60682b48ec3a4d282b51fcda457c2eea2ae`。
- 真值初始与 detector 初始都仍为 6/10 PASS，集合均 001/002/005/006/007/008；car003--010 八个新增目标的真值初始可见观测均 IoU 都提高，例如 003 `0.753→0.844`、009 `0.698→0.793`，但 **旧目标 car002 `0.895→0.847` 明显退步**；detector 初始 car010 `0.785621→0.785446` 极小退步。car004/009 真值初始仍有单帧可见 IoU 低于门，三种其余失败集中在完全遮挡语义与 car004 搜索预测连续性。所有 20 路 `state`、`tracking_phase`、`accepted`、`reacquired`、模板与运动预测逐帧一致，证实此候选只改变输出尺寸，并不能修复遮挡。不能因通过集不变而把完整权重版晋级；下一步改为跨目标更保守的等权尺寸融合并全量复测。

## 2026-11-17T23:25:20+08:00 双检测确认后的跟踪输出尺寸候选（主机通过，待板端）

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，起点 `dc42c24189ae126113d7a52da0325bcd429542f1`。在上一版 RK3588 十目标双初始化结果中，新增目标 car003--010 的可见且接受帧做静态反事实：保持 NanoTrack 中心、只用当帧显示通道已平滑的 detector 尺寸并按既有每帧 5% 上限调节输出宽高，八目标平均 IoU 均提高（例如合并目录 car_009 `0.6985→0.8242`，car_010 `0.8870→0.9081`）。这只是同一结果上的离线尺寸代理，不证明闭环收益，且不能直接让视觉 `display_bbox` 成为算法框。
- 候选在运行时对已有周期性 detector batch 独立筛选：已知人/车类别、完整目标检测、唯一同类候选、足够置信度、候选覆盖 NanoTrack 中心且 IoU 达到既有 detector 确认门；连续两次同一候选几何确认、新鲜期内才把 detector 宽高缓慢用于对外 `tracking_bbox/control_bbox`，中心仍用 NanoTrack，内部 tracker 提交框/下帧裁剪不变。无检测、错误类别、低置信度、重复/远处/过期候选保持旧路径；状态在初始化和离开正常跟踪时清理。新增 `DETECTOR_TRACKING_SIZE_CONFIRMED` 事件。实际 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure`，23/23 PASS；日志 `/tmp/nt-size-evidence-host-build.log` / `/tmp/nt-size-evidence-host-ctest.log` SHA-256 `6dc0e2a1fdbdd57b3b657887b400cfdb221460c20132332e823251bba9de4040` / `51da9fabb1c7509c2dde27fa2ee8514b5b4edf4664f546a7944be97f34e4466d`。下一步提交源码并在 RK3588 隔离目录原生构建、双初始化复跑十目标，检查新尺寸是否真让平均框更贴真值且无通过集退步。
- 用户澄清新增八目标的 `car004` 是 `point_04`，正式合并十目标目录名为 `car_006`：其第 42 帧现已是 `manual_confirmed_visible`，199--213 帧现已是 `manual_confirmed_no_bbox/full_occlusion`，无需改真值。先前审计的合并目录 `car_004` 实为新增 2 号目标（`point_02`），241 帧起出画面；此前报告的该目标第 42 帧疑点仍单独保留，不能把两者混同。核对路径 `/home/u20/single-object-tracking/data/occ_video_1/ground_truth/car_006/{annotation_metadata.json,events.json,frames.jsonl}` 与合并目录 `car_004` 同名文件；本轮未更改任何真值文件。

## 2026-11-17T23:05:26+08:00 恢复中心融合死区：RK3588 双初始化十目标全量复测

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，算法源码 `d0dc0e799597fdb3b7ce5575d1ee17d1d71eb4ba`（起点/对照 `93eb13ef778581a6d4fa5a0887e8fb98be474d26`）。源码包主机 `/tmp/nt-recovery-deadband-d0dc0e7.tar`、板端 `/home/neardi/Project/SOT/nt-recovery-deadband-d0dc0e7.tar` SHA-256 `68916b301b057deec11187043eb87af6f37fcad46a743ef41cd29282de337c6d`；板端隔离目录 `/home/neardi/Project/SOT/nt-recovery-deadband-d0dc0e7`。主机 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure` 23/23 PASS，日志 SHA-256 `f9a9a258444638e2d2437e550c08e890c92d8bdd764298ae39be6b20c79929c2` / `c3a411d897ab51c49615857abc2b04cf1d0f3bcceafd3fe6514cee5920ac5c98`。板端原生 `cmake -S rk3566/nanotrack_v3 -B build -DENABLE_RKNN_RUNTIME=ON -DBUILD_TESTING=ON -DRKNN_INCLUDE_DIR=/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/include -DRKNN_LIBRARY=/usr/lib/librknnrt.so`、`cmake --build build --parallel 4`、`ctest --test-dir build --output-on-failure` 23/23 PASS；`cmake.log` / `build.log` / `ctest.log` SHA-256 `6660c4686ebefe649453d5237d0d8f69ed416402680a696bc708e4d53cb5ad48` / `3d3b6e246fa41d7e1e1a5448e55e79c38174b7c4f26b2af413a21569b4c5c13e` / `53226f200ac3c0ec0fda5b715bd8f6201811981c60695b901dcfa536cbc5fedc`。
- 板端程序 `build/nanotrack_v3` SHA-256 `2f9ea58cd87b6c8ec9b941c90a0f530f8c823cc018767ad749a35d3181b1f9d0`；运行配置 `runs/headless-on.conf` SHA-256 `72f3790a8a35232f3922ebc66f41b8fd65f6067541322cdd4c35c1e9f23d0344`，controller 配置 SHA-256 `62607f7eb3798737fc927a1f997e0d52cc5f78a7250d8df8b23aebfb320d5f44`。输入 `occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；tracker manifest SHA-256 `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`；VisDrone9 detector RKNN / manifest / labels SHA-256 `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c` / `ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9` / `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`；RKNN API 2.1.0、driver 0.9.8。目标级真值身份、首帧真值框及 detector 框沿用上节 `93eb13e` 的已核验版本；detector 框按已有最小 36×36 初始化规则。
- 板端实际运行 `bash runs/nt-deadband-focus-gt.sh`、`bash runs/nt-deadband-focus-det.sh`，脚本 SHA-256 `10cf5d07a4b4afb12ef82409cce1be0967fadbf88c329ebbfa396ee5bf877c9e` / `e0a602fedfa814686f0e43e0607462f010fcf67d281827fdaa86d7f6159a92c0`，正反例目标 003/005/008/010 各 271 帧；之后 `bash runs/nt-deadband-full-gt.sh`、`bash runs/nt-deadband-full-det.sh`，脚本 SHA-256 `56c50f71bb1bbb5a8f5803f3e6375c0087f3eeb9dc79459d6d4f71fe84f107de` / `e8855cbd2657424c5c501c510181a7f31edb93b0c2e74671bdf5f99b6e7646f5`，十目标双初始化共 20 路、每路 271 帧。全量板端原始输出 `runs/deadband-gt-full/<target>/`、`runs/deadband-det-full/<target>/`；真值归档板端 `/home/neardi/Project/SOT/nt-recovery-deadband-d0dc0e7-full-gt.tar.gz`、主机 `/tmp/nt-recovery-deadband-d0dc0e7-full-gt.tar.gz` SHA-256 `4c0ece4eec9ce6f1d541fe7691b063cb06380bc2ac982e907a4481b956e59690`；包含 detector 初始的全量归档板端 `/home/neardi/Project/SOT/nt-recovery-deadband-d0dc0e7-full.tar.gz`、主机 `/tmp/nt-recovery-deadband-d0dc0e7-full.tar.gz` SHA-256 `6f1e5604702c06d001cf02dc3bc266221db14e50f5791ca7b1fec70bb67effdc`。聚焦包 `/tmp/nt-recovery-deadband-d0dc0e7-focused.tar.gz` SHA-256 `906d2d68934b0bd167f1a268006a5b077330212704a2bc1143c242f9ff3cdc3c`。
- 用 `scripts/evaluate_occ_video_1.py --scope full --source-commit d0dc0e799597fdb3b7ce5575d1ee17d1d71eb4ba --config-sha256 72f3790a8a35232f3922ebc66f41b8fd65f6067541322cdd4c35c1e9f23d0344 --model-sha256 3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261` 逐目标正式评分，附完整路径参数见归档 `score.json` provenance。真值初始得 6/10 PASS（001/002/005/006/007/008），detector 初始也为 6/10 PASS（相同集合），均无通过集退步；20 份原始 CSV 均为 271 帧。逐目标 `score.json` / `frames.csv` SHA-256、通过门、实际中心融合帧见主机 `/tmp/nt-recovery-deadband-d0dc0e7-summary.json`，SHA-256 `fa6e7023bdb396ebd209ce21c35214202afc09d9af9c8763283377193c059878`。真值 003 第 144 帧与 010 第 176 帧融合，观测可见均 IoU 分别 `0.731464→0.752566`、`0.867262→0.886993`；005/008 无融合，保持 PASS；detector 初始 003/004/010 分别在 144/144/171 帧融合。真值 004/009 仍有额外可见框门失败，且 003/004/009/010 均有完全遮挡语义失败；detector 初始四个失败目标仅余完全遮挡语义门。car004 第 42 帧真值与可视内容的矛盾仍待人工复核，不改真值或评测器。此版只是一项未晋级的候选，未合入 main/Debug，尚不构成十目标通过或吊舱验收。下一步先逐帧复核完全遮挡期间的误接受与真值边界，再决定通用算法修正。

## 2026-11-17T22:43:28+08:00 等权恢复中心融合：car003 改善、car008 微退步；收窄有意义差值

- 实验源码 `codex/ten-target-smoothness@d9e7a35c62ce423a1523b5f48a94867daf1e51a1`，主机 `/tmp/nt-recovery-center-d9e7a35.tar` 与板端 `/home/neardi/Project/SOT/nt-recovery-center-d9e7a35.tar` SHA-256 均 `24f478f544cc713ba4a7737854660c6693c93654290c9601f6f9444f9d73b1b8`；隔离根 `/home/neardi/Project/SOT/nt-recovery-center-d9e7a35`。RK3588 原生 `cmake -S rk3566/nanotrack_v3 -B build -DENABLE_RKNN_RUNTIME=ON -DBUILD_TESTING=ON -DRKNN_INCLUDE_DIR=/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/include -DRKNN_LIBRARY=/usr/lib/librknnrt.so`、`cmake --build build --parallel 4`、`ctest --test-dir build --output-on-failure`，23/23 PASS；`cmake.log` / `build.log` / `ctest.log` SHA-256 `fc91fbd166ac67016175568d01cfb92fcabee8631caf9a796683d4197f36635b` / `3d0a79980cb868c769c3a84b39c07642a6f6908ed43b223a8355087c8fa0cab6` / `9d9078f1ee3e91a39c5ebf7de44d0fced543e241feacdc130c91e6c539c2b58b`。程序 `build/nanotrack_v3` SHA-256 `aa818e750256770d0f0169a6111f35fb3189933a79d09c674b2188a43ed2108a`，运行配置 `runs/headless-on.conf` SHA-256 `18e122568411bf309310816f0d22c0e0252e140b0ff8743975b27a91302ef65b`；controller 配置、视频、模型/manifest/labels 与上节 `93eb13e` 同一核验身份，RKNN API 2.1.0、driver 0.9.8。
- 板端实际 `bash runs/nt-center-focus-gt.sh` / `bash runs/nt-center-focus-det.sh`，脚本 SHA-256 `65937471a586c0449c7a57d2bd690ae759a4691a8a52bc3ef62e2fadf14ec945` / `e4dc3b0a063cabe44ed0d7b5b0fc6585bbbba2e60d26c5bea6107a61b9a71a36`；各使用上版完全相同的首帧真值框或 detector 框，分别跑 003/005/008/010 各 271 帧。板端原始目录 `runs/center-gt-focus/<target>/`、`runs/center-det-focus/<target>/`；板端 `/home/neardi/Project/SOT/nt-recovery-center-d9e7a35-focused.tar.gz`、主机 `/tmp/nt-recovery-center-d9e7a35-focused.tar.gz` SHA-256 均 `e30048e6d1999c9bfed2a552ffe6b92f9ad5136dc2b12bb09951ceaea810b184`，真值独立归档 `/home/neardi/Project/SOT/nt-recovery-center-d9e7a35-focus-gt.tar.gz`、主机 `/tmp/nt-recovery-center-d9e7a35-focus-gt.tar.gz` SHA-256 均 `b76f48c99ad8efbb3d1911a318ff07143475baf4e9a5e35053eb4c273b235e7e`。正式 `scripts/evaluate_occ_video_1.py --scope full --source-commit d9e7a35c62ce423a1523b5f48a94867daf1e51a1 --config-sha256 18e122568411bf309310816f0d22c0e0252e140b0ff8743975b27a91302ef65b --model-sha256 3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261` 评分在主机 `/tmp/nt-recovery-center-d9e7a35-focus-gt/runs/center-gt-focus/<target>/score.json` 和 `/tmp/nt-recovery-center-d9e7a35-focused/runs/center-det-focus/<target>/score.json`；八份摘要 `/tmp/nt-recovery-center-d9e7a35-focused-summary.json` SHA-256 `62ca172f20c7e3cdf52eaf3b925a784605daaafc5cf8d6215d9166c39ea7b090`。
- 真值初始：003 有效观测 IoU `0.731→0.753`，第 144 帧重捕声明及随后可见框门由 FAIL 变 PASS，只余 130 帧遮挡语义；005 仍 PASS（`0.767→0.763`）；010 仍 FAIL 但有效观测 IoU `0.867→0.887`；**008 从 PASS 变 FAIL**，有效观测均 IoU `0.700325→0.699662`，258 帧的 4.8 px detector/跟踪中心差使下一帧搜索与状态时序分叉。detector 初始：005/008 仍 PASS、003/010 仍 FAIL；003 可见框和重捕声明门也修复。故无条件等权融合不能推广十目标，不合入 main/Debug。
- 下一候选保留同一 NanoTrack 身份门、detector 新鲜期与恢复几何门，仅在中心差达到既有 `detector_confirmation_center_distance` 的一半时才融合；小差值由 NanoTrack 原框保持，避免为了几像素差改变下一帧裁剪。此死区取自现有尺度归一化确认容差，不使用目标/帧号。主机 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure` 23/23 PASS，日志 `/tmp/nt-recovery-center-deadband-host-build.log` / `/tmp/nt-recovery-center-deadband-host-ctest.log` SHA-256 `f9a9a258444638e2d2437e550c08e890c92d8bdd764298ae39be6b20c79929c2` / `c3a411d897ab51c49615857abc2b04cf1d0f3bcceafd3fe6514cee5920ac5c98`；单测覆盖有意义差值融合和小差值不融合。待新隔离板端复跑正反例及完整十目标，不凭静态反事实晋级。

## 2026-11-17T22:32:09+08:00 NanoTrack 确认后融合新鲜 detector 恢复中心（待板端）

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，起点 `1579c6a72f8248eba33d71353657206dd47063df`；主机历史记录仅包含进度更新，算法起点仍为板端 `93eb13ef778581a6d4fa5a0887e8fb98be474d26`。对七个有可见真值的 detector 支持重捕帧离线比较中心误差：car003 第144帧 NanoTrack 21.0 px、detector 6.9 px；car010 第176帧为 9.6/0.7 px；但 car005 第182帧为 2.3/8.5 px、car008 第258帧为 2.6/6.5 px。因此直接采用 detector 中心并不稳妥，等权融合保留了两方信息。单帧几何反事实的 IoU：003 为 0.379→0.534，004 0.778→0.893，005 0.891→0.752，006 0.725→0.769，007 0.850→0.870，008 0.787→0.730，010 0.552→0.732；这些只是固定输出框静态计算，后续裁剪/模板闭环可能完全不同。
- 新候选只在统一恢复已经由 NanoTrack 验证、候选带有 detector 支持、无身份冲突、detector 为完整目标且仍在既有新鲜期、两框满足现有恢复几何一致性时，把恢复控制框中心取 NanoTrack 与 detector 中心的中点，尺寸仍来自 NanoTrack；探测、身份确认和控制授权门不变。原始 NanoTrack 观测保留在 `selected_observation_bbox`，日志 note 记 `RECOVERY_DETECTOR_CENTER_FUSED`。无 detector、过期、非完整目标或不一致时保持原路径；不是按目标/帧号定制。
- 新增控制器单测验证两次 NanoTrack 确认前不可接受、确认后中点与原始观测/控制框的关系；主机实际 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure`，23/23 PASS，日志 `/tmp/nt-recovery-center-host-build.log` / `/tmp/nt-recovery-center-host-ctest.log` SHA-256 `e0599784cca586b1c9ea80e6b5cadf1d2a30e3fa0d1bece3792769ba41bd9a34` / `55fa7b1b579b6bac4506afd58744fd99719b943c8c3d787c6b529dbdb5dd5d3b`。下一步提交源码后，在 RK3588 隔离目录原生构建、先跑 car003/005/008/010 的正反例，再跑完整十目标双初始化；有害退步则撤回，不合入 main/Debug。

## 2026-11-17T22:18:53+08:00 十目标失败帧视觉审计：car004 第 42 帧待真值复核

- 只读分析上一节板端算法源码 `93eb13ef778581a6d4fa5a0887e8fb98be474d26` 的十目标真值初始化输出 `/tmp/nt-frozen-focus-93eb13e-full-gt/runs/ten-target-full/<target>/frames.csv` 与正式多事件真值 `data/occ_video_1/ground_truth/<target>/frames.jsonl`，视频仍为 SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`。失败集合仍是 003/004/009/010；完全遮挡门失败帧分别为 003:130、004:42/131/132、009:115--120、010:149--157。没有修改原真值、评测器或算法。
- 用 `ffmpeg` 从同一视频抽取原始帧 39--44，并以 Python/Pillow 在 41/42/43 帧绘制同一目标的输出框（红）及可用真值框（绿）；对照图 `/tmp/nt-car004-frame42-audit.png` SHA-256 `2645059790f051e7a40969b40daef3f0e715313bab5e24d21aa3fff7adaee1ae`。car004 的第 42 帧正式真值为 `manual_confirmed_no_bbox/full_occlusion`，前后 41/43 帧均有框；但第 42 帧同一银色车辆在视频中持续可见，跟踪框落在车上，且输出为 `TRACKING/accepted=1`。因此该单帧失败存在标注与画面不一致的强迹象；已请求人工复核，在确认前仍保留正式 FAIL，不把算法调到拒绝可见车辆。
- 另以 `ffmpeg -f rawvideo -pix_fmt rgb24` 解码同一 271 帧，Python/Numpy 逐帧计算 car002--010 输出框中灰度大于 150 的像素比例，并与近 8 个已接受 TRACKING 框的中位数比较；表 `/tmp/nt-brightness-audit-93eb13e.csv` SHA-256 `a8e8b461585ce8817b18f7e44f90143ebc8d1e5787e0347d93cbdd9b15399a8c`。car009 完全遮挡仍接受的 116--120 帧比值约 0.59/0.63/0.44/0.44/0.42，car010 的 149--155 帧约 0.83/0.76/0.75/0.67/0.69/0.56/0.52；但可见且被接受帧比值低于 0.90 的数量在 car002/003/004/005/006/007/008/009/010 分别为 23/49/16/29/15/15/3/17/7。car004 第 42 帧比值约 0.99。故单独抬高相对亮度门槛会误拒大量可见目标，也不能解决 car004 的待复核单帧；不据此调参。接下来分别研究真实遮挡下的身份/可见性证据，以及待复核真值对评分的影响。
- 同一视频和框以每通道 16-bin RGB 直方图对首十帧中位分布计算卡方距离，离线表 `/tmp/nt-color-hist-audit-93eb13e.csv` SHA-256 `8a8bc5474a017bb0e3ea4e626f2b4099b84cd3ad887f1caf390b0c090a971851`。car010 遮挡 149--155 帧从 0.289 升至 0.530，有一定信号；但 car009 遮挡 116--120 帧仅 0.155--0.293，落在其可见帧常见范围，car004 第 42 帧为 0.096。颜色直方图单门不能一致覆盖这三种情况，因此暂不加此类外观阈值。
- 板端在同一隔离根用 `ffmpeg -vf select=...` 抽取 41/42/43、113/115/117/120、147/149/153/157 帧，逐张执行 `./build/yolo_detector_smoke --model <同一 VisDrone9 RKNN> --manifest <同一 detector manifest> --labels <同一 labels> --image <帧图> --conf 0.25 --nms 0.45 --class-id -1 --output-image <jpg> --output-csv <csv>`；程序 SHA-256 `3ba28a7afa1e5aa9c09a551f1fbb2b0e137e2b0006ecb1941fdd7fe8d9712f72`，日志 `runs/detector-occlusion-audit/smoke.log` SHA-256 `6993ddf7d2280e28ba09943f6ca4349fe652bc04962f71b61787b6ec3d5f3c74`。板端包 `/home/neardi/Project/SOT/nt-detector-occlusion-audit.tar.gz` 与主机 `/tmp/nt-detector-occlusion-audit.tar.gz` SHA-256 均 `38d9f48224f8aaa8ba83814057cf3f15493a47df4029e4dbbce8d9fa604671d9`，解包 `/tmp/nt-detector-occlusion-audit/detector-occlusion-audit/`。car004 41/42/43 的近距 car 检测中心距输出框分别 0.8/0.8/1.3 px，置信度 0.784/0.810/0.760；第 42 帧视觉与 detector 两条证据均支持人工复核。car009 第 115 帧无近距 car 检测，但正式完全遮挡的第 120 帧却有中心距 6.4 px、置信度 0.542 的近距 car 候选；car010 第 147 帧可见却无 60 px 内的 car 候选。此程序是全帧独立检测，和运行时 ROI/异步检测口径不同，仅为反例，不把其候选表当作运行时接受证据。detector 有/无均不能独立判定目标可见性。
- 直接枚举当前十路运行时 CSV 的 `VISUAL_OCCLUSION` 请求：car009 第 116 帧完全遮挡，检测返回 0 个过滤候选但高 PSR 仍使跟踪恢复；car004 第 131 帧完全遮挡也返回 0 候选却仍接受。若把“疑似时一次视觉 detector 未命中”直接改为 SEARCH，car003 第 51 帧可见且检测返回 0 候选、当前正确接受的例子会被误拒；car002 第 71 帧可见也有视觉请求。因此现有记录已反证单次未命中即丢目标的通用规则，不直接放宽搜索触发门。后续应寻找与外观、检测和 NanoTrack 独立的可见性证据，或验证更可靠的检测模型，而非按这些帧号补丁。

## 2026-11-17T22:12:00+08:00 最新程序复核手点目标 120--141 帧

- 在板端隔离根 `/home/neardi/Project/SOT/nt-frozen-focus-93eb13e` 使用上一节算法源码 `93eb13ef778581a6d4fa5a0887e8fb98be474d26`、程序 SHA-256 `5407c0f7a05a78bfc9aab88a8b273514485dfde506a06a57291c5bfc93581e98`、运行配置 SHA-256 `58a363afa735d5f1b76d4f3cdd47f66619998749ad6ee9a122694878ce5915ec`；该配置引用的 controller 配置确实有 `suspected_detector_guard_enabled=true`（SHA-256 `62607f7eb3798737fc927a1f997e0d52cc5f78a7250d8df8b23aebfb320d5f44`）。视频、模型、manifest、labels 和 RKNN 版本与下一节同一核验身份。
- 实际命令 `./build/nanotrack_v3 --config runs/headless-on.conf --bbox 1032.833740,405.310425,36,36 --output-boxes runs/manual-yellow-latest/frames.csv --session-metadata runs/manual-yellow-latest/session.json > runs/manual-yellow-latest/run.log 2>&1`，exit 0、271 帧、CSV 272 行。板端原始目录 `runs/manual-yellow-latest/`，板端 `/home/neardi/Project/SOT/nt-frozen-focus-93eb13e-manual-yellow.tar.gz` 与主机 `/tmp/nt-frozen-focus-93eb13e-manual-yellow.tar.gz` SHA-256 均 `2f07c0755cd6b5929d08c3a39fc5e253f60f8510dab60bb20e23ebbb38e72b00`；CSV/session SHA-256 `19f32421e25ae4c3780c027eb5611913df4a01f19635b3a7515d79252c236cf4` / `cd1b2e27be218649989dfc1b47532e35ab2ee0f344d790c8b8377cf49c48d91a`。主机解包 `/tmp/nt-frozen-focus-93eb13e-manual-yellow/runs/manual-yellow-latest/`。
- 逐帧记录：120--127 为 `SUSPECTED`，121/124/127 提交 `SUSPECTED_VALIDATION`，128 收到源帧 127 的检测批次后 `SUSPICION_CLEARED`、重新接受 NanoTrack，141 仍 `TRACKING` 且接受；真遮挡时 163 为 `UNCERTAIN`，176 恢复 `TRACKING`。这与原实验版避免 141 帧误入 `SEARCHING` 的结论一致。手点黄车没有逐帧正式真值，不能据此声明框贴真值；新十目标真值初始化 6/10、detector 初始化 6/10 见下一节，003/004/009/010 仍失败。当前版本留在任务分支，不合入 main/Debug 或生产 handoff。

## 2026-11-17T22:06:18+08:00 冻结运动唯一近距 detector 优先探测：完整十目标回放

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`；起点 `eee6592eb20a792c19694cc44bc870831d8d8f75`，板端算法源码 `93eb13ef778581a6d4fa5a0887e8fb98be474d26`。源码包主机 `/tmp/nt-frozen-focus-93eb13e.tar`、板端 `/home/neardi/Project/SOT/nt-frozen-focus-93eb13e.tar` SHA-256 均 `16d8890ad065c8de4e6becaa6cb8739a1664257d8738a08342e5af2594beadb9`，隔离运行根 `/home/neardi/Project/SOT/nt-frozen-focus-93eb13e`。新代码只在运动输入冻结、预测仍可信且近距门只剩一个 detector 候选时优先安排下一帧 NanoTrack 探测；检测框本身不能授权控制。
- 板端实际 `cmake -S rk3566/nanotrack_v3 -B build -DENABLE_RKNN_RUNTIME=ON -DBUILD_TESTING=ON -DRKNN_INCLUDE_DIR=/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/include -DRKNN_LIBRARY=/usr/lib/librknnrt.so`、`cmake --build build --parallel 4`、`ctest --test-dir build --output-on-failure`，23/23 PASS；`cmake.log` / `build.log` / `ctest.log` SHA-256 为 `c8419e25d967c137c1bb512e75dd23770890162348e852abac6f0c81596709f7` / `a42bbba46cb455eb4184254b3214fb158bbdce4346d8028ef14ad43ec090c13a` / `fe316905074bc5e7f83439da31ca16cd8528e01433151467ff1941d217aa1389`。程序 `build/nanotrack_v3` SHA-256 `5407c0f7a05a78bfc9aab88a8b273514485dfde506a06a57291c5bfc93581e98`；运行配置 `runs/headless-on.conf` SHA-256 `58a363afa735d5f1b76d4f3cdd47f66619998749ad6ee9a122694878ce5915ec`；controller 配置 SHA-256 `62607f7eb3798737fc927a1f997e0d52cc5f78a7250d8df8b23aebfb320d5f44`。输入 `/home/neardi/Project/SOT/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；tracker manifest / detector RKNN / detector manifest / labels SHA-256 为 `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261` / `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c` / `ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9` / `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`；RKNN API 2.1.0、driver 0.9.8。
- 先 `bash runs/focused.sh`、`bash runs/focused-det.sh`，脚本 SHA-256 `341ccd406e10ffd6729efd5ae5d7a381dbaa3f5b104414efa1642f18028133b4` / `c2fbb8a7a0a75c8aacd306c23db86fbeb6acf5ec7d18c18967ec430edffec606`；真值初始化 001/003/004/007 及 detector 初始化 001 均 271 帧。原始目录 `runs/ten-target-motion/<target>/`、`runs/det-init-motion/car_001/`；板端 `/home/neardi/Project/SOT/nt-frozen-focus-93eb13e-focused.tar.gz`、主机 `/tmp/nt-frozen-focus-93eb13e-focused.tar.gz` SHA-256 均 `059a80b426b19fcb8283a528abd6c5d7002e1404f7a7380fcb1ffbf34a774d92`。真值 001/007 PASS、003/004 FAIL；detector 初始化 001 PASS，188 帧正确重捕。
- 再 `bash runs/full-gt.sh`、`bash runs/full-det.sh`，各十目标使用各自首帧真值框或 detector 框（不足 36×36 的框保中心最小修正，003 用真值框回退），脚本 SHA-256 `bb80975ba9ed17e46239078bd7b23db88419ca80ecdcbbf6b858c3b2cf03b6ed` / `d86372d534e3c2a23ee57d71d52fb8d431d5810ffee24cb206166f59752b04f8`，每次 271 帧、CSV 272 行。板端原始目录 `runs/ten-target-full/<target>/`、`runs/det-init-full/<target>/`；完整包板端 `/home/neardi/Project/SOT/nt-frozen-focus-93eb13e-full.tar.gz`、主机 `/tmp/nt-frozen-focus-93eb13e-full.tar.gz` SHA-256 均 `7073ca6b983e30206c2c269761f5ee371e2cbc97aea2a1ea1fe76a5c67fea2e3`；真值独立归档 `/home/neardi/Project/SOT/nt-frozen-focus-93eb13e-full-gt.tar.gz`、主机 `/tmp/nt-frozen-focus-93eb13e-full-gt.tar.gz` SHA-256 均 `aa1e20b3dc09d53c1f368566a2a9f8928a2f5a1b40b6f1c72aa3bc3f54b7f8b7`。
- 主机用 `scripts/evaluate_occ_video_1.py --scope full --source-commit 93eb13ef778581a6d4fa5a0887e8fb98be474d26 --config-sha256 58a363afa735d5f1b76d4f3cdd47f66619998749ad6ee9a122694878ce5915ec --model-sha256 3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261` 逐目标评分，结果在主机 `/tmp/nt-frozen-focus-93eb13e-full-gt/runs/ten-target-full/<target>/score.json` 与 `/tmp/nt-frozen-focus-93eb13e-full/runs/det-init-full/<target>/score.json`，二十份摘要 `/tmp/nt-frozen-focus-93eb13e-summary.json` SHA-256 `8a011b231dd1c49c5ea038f8869b8c69b5e8669933bf3a9989bd6bb14a926314`。真值初始化仍 **6/10 PASS**（001/002/005/006/007/008），003/004/009/010 FAIL，无原有 PASS 退步。detector 初始化从基线 **5/10 提到 6/10 PASS**（同六个目标）：car001 基线 184 帧错认邻车、有效观测 IoU 0.520、再现后 IoU 0.005；本版 188 帧正确重捕、有效观测 IoU 0.747、再现后 IoU 0.742，其他原有 PASS 保持。003/004/009/010 的完全遮挡语义与可见框问题仍在；这是局部改进，不足以替代十目标基线或证明吊舱验收，不合入 main/Debug。
- 定向单测新增冻结唯一近距候选的下一帧探测、非冻结保持原选择、detector 不能直接接受；主机改动后 build / CTest 23/23 PASS，日志 `/tmp/nt-frozen-focus-test-build.log` / `/tmp/nt-frozen-focus-host-ctest2.log` SHA-256 `ffc69c9040a9d9ea50b4dd691e286bfc58fb9e49361ca8f8530bd19650a0e432` / `9e084f23d1221c3669a2547b24018ab5b434efbb6448f1f9aba9d61b9e793543`。测试和本进度写入本节后续提交；板端运行的算法源码仍是 `93eb13e`，后续提交只含测试和进度。下一步研究四个失败目标的遮挡/身份共性，避免按目标或帧号加门限；本版先作为隔离实验保留。

## 2026-11-17T21:48:39+08:00 唯一近距 detector 候选先获得双模板探测（待 RK3588）

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，起点 `eee6592eb20a792c19694cc44bc870831d8d8f75`。上节 `b168b3a` 的 car001 detector 初始化在 182--197 帧每批只有一个近距 detector 候选，但统一候选表仅在“至少两个候选且第一项优势明显”时调用 `focus_detector`；未聚焦的唯一检测框不能优先决定下一帧 NanoTrack 搜索裁剪，Recent/Safe 旧候选持续竞争并报 `IDENTITY_CONFLICT`，正确恢复延至 202 帧。新候选保留该版 detector 确认趋势及冻结时的半对角线近距门；只在冻结、预测仍可信、近距门仅剩唯一 detector 时把该候选设为下一帧探测焦点。焦点只改变搜索裁剪，仍必须由新鲜 Recent/Safe 逐帧验证和原身份冲突门决定是否接受；不增加 detector 推理频率，不按目标/帧号定制。
- 主机实际 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure` 23/23 PASS；日志 `/tmp/nt-frozen-focus-build.log` / `/tmp/nt-frozen-focus-ctest.log` SHA-256 `b3a2bcdbd85dab20f7863dd04299017d3b7c01090ef4ff116b6d3c2c4a06bba2` / `e13dcfb15f5e0db601c0c4dc673771264dc5b8676e4fc0e41fef3c845525b117`。下一步隔离 RK3588 原生五路定向回放；若 car001 达到身份及重捕门且 003/004/007 不退步，再跑十目标。不可用 host CTest 替代板端结论，不合入 main/Debug。

## 2026-11-17T21:44:34+08:00 冻结趋势加近距候选阻断错认但超时，撤回

- 候选源码 `codex/ten-target-smoothness@b168b3a276502e3d25cd8bd5ee523c2b2dfd4610`，源码包主机 `/tmp/nt-near-trend-b168b3a.tar`、板端 `/home/neardi/Project/SOT/nt-near-trend-b168b3a.tar` SHA-256 均 `6112341cf0b9720e8e9888a563315f77613205495a0eb6c8be04063845703155`；板端隔离根 `/home/neardi/Project/SOT/nt-near-trend-b168b3a`。原生 CMake/build/CTest 23/23 PASS，日志 `cmake.log` / `build.log` / `ctest.log` SHA-256 `500682f46c26bf305f314a8e57b7aff469e7f91af31adf5b92e853457f737319` / `12d94a97905f2069deef2279c97fb6daf70bb3648565f3e15347164a3cd8505e` / `87e68dd53f924670da201474761b390959d7c81b575d0176cf6d96de22344bdf`；程序 `build/nanotrack_v3` SHA-256 `f58a72730ca4c3a3e744f7e4d4c2f2512e22bab7a720fb4a55e89e0613488a4c`，运行配置 `runs/headless-on.conf` SHA-256 `efa23f317ddd0ac61e6fd4efb3cfd503d56747b156587c58c1401e8984807c0a`。controller 配置、视频、模型/manifest/labels 与 `7212f6f` 同一核验身份，RKNN API `2.1.0`、driver `0.9.8`。
- 实际 `bash runs/focused.sh` / `bash runs/focused-det.sh`，脚本 SHA-256 `e82cbb0ee676c58b93ebeead8c5e2fc548e5d431c61ea2642a58cbff61bf90da` / `48e1f6aa475e9f4afec6993162c718573541cf74a8314f04a179b5dbe121bcf8`；car001/003/004/007 真值初始化和 car001 detector 初始化，各 271 帧、CSV 272 行。板端原始 `runs/ten-target-motion/<target>/`、`runs/det-init-motion/car_001/`，板端 `/home/neardi/Project/SOT/nt-near-trend-b168b3a-focused.tar.gz` 与主机 `/tmp/nt-near-trend-b168b3a-focused.tar.gz` SHA-256 均 `caa301ac7be4390bac5582906ae2c4e29139923d6b0fdc91881dc1a51605f391`。正式 `scripts/evaluate_occ_video_1.py --scope full --source-commit b168b3a276502e3d25cd8bd5ee523c2b2dfd4610 --config-sha256 efa23f317ddd0ac61e6fd4efb3cfd503d56747b156587c58c1401e8984807c0a --model-sha256 3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261` 评分 `/tmp/nt-near-trend-b168b3a-focused/runs/<run>/<target>/score.json`，五份哈希清单 `/tmp/nt-near-trend-b168b3a-scores.sha256` SHA-256 `68336c6c4a0bdc3f24f3f8ddf4fadcaf65089fd80a1d982306801238a4cef0d5`。
- 真值路 001/007 PASS、003/004 FAIL；detector 初始 car001 可见均 IoU `0.732`、身份门通过，不再错认邻车，但重捕第 **202 帧**，超过 `[182,190)` 期限，正式 FAIL。182--197 帧每批仅约一项 detector 候选通过近距门，Recent/Safe 与候选持续 `IDENTITY_CONFLICT`；200--202 帧才形成两次确认。该门解决远处假候选，却没有让正确目标及时获得 NanoTrack 共同验证。第一版把门套到所有可信运动时，已有远距搜索及 subtarget 单测失败；收窄到冻结时主机 23/23 PASS，另用未提交的定向单测验证冻结远候选被拒、非冻结仍保留，但板端门槛仍失败。**否决本组合，不跑十目标推广，不合入 main/Debug，不作为生产 handoff。**本次恢复全部算法和单测源码到 `64c0a9b941d6e8c4589b97d56df8b1b1b0435e95`，主机 build/CTest 23/23 PASS，日志 `/tmp/nt-near-trend-revert-{build,ctest}.log` SHA-256 `0ea11c7d3abe32cdb261dce9f5808687b68dd72786345b0d1ef9fa4136432d0c` / `3bca65e252b797e177ca101f57bc10f1af41786fb56bd0264a08d39e080ff629`；算法源码与该起点 diff 为空。下一步研究如何在仍然保持多候选身份安全时让正确 detector/双模板证据及时汇合，避免仅调运动或距离。

## 2026-11-17T21:38:16+08:00 冻结运动证据与近距候选联合验证（待 RK3588）

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，起点 `64c0a9b941d6e8c4589b97d56df8b1b1b0435e95`。只读对比上两版发现：car001 detector 初始化的错误 detector 恢复探针距运动预测约 `1.2` 个预测框对角线，而首帧真值初始化的正确 car001 确认探针约 `0.21--0.23`；已通过的 car002/005/006/007/008 正确确认探针约 `0.08--0.47`。单独修运动趋势把错误邻车验证提前，说明必须同时约束候选身份。联合候选复用上节“两次显示 detector 确认后的运动趋势”实现，仅当运动输入冻结且搜索运动预测仍被标为可信时，让统一恢复路径只验证中心距不超过半个预测框对角线的 detector 候选；运动预测不可信时恢复原来的宽搜索。普通（非 unified）路径仍使用原配置，不增加 detector 请求，不改 tracker 控制授权。此半对角线是相对目标尺度的通用几何约束，尚需板端多目标正反验证，不能把上述观察当作独立验证集。
- 第一版把近距门应用于所有“运动预测可信”的搜索，主机 `long_term_controller` 和 `long_term_replay` 两组测试失败，包含已有的远距 subtarget 探测和跨车交错恢复契约；随即收窄到“运动输入被冻结”的状态，原有主机回归 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure` 23/23 PASS，最终日志 `/tmp/nt-near-trend-build.log` / `/tmp/nt-near-trend-ctest.log` SHA-256 `be459f933cf95e6e4269c05bd1bd8a4453257eded95050f9bd97e05a26df3791` / `7a3a4a52c54a7a5786bcc8b11a25d4c955c7ff615144da2b191053d04c3b817b`。下一步 RK3588 原生定向（car001 两种初始化及 003/004/007），若稳定再完整十目标；这仍是实验，不合入 main/Debug。

## 2026-11-17T21:31:36+08:00 detector 确认速度候选误重捕，恢复可信算法

- 候选 `codex/ten-target-smoothness@7212f6fe4536e29f55e92cc06550a2c06e8c6711`，源码包主机 `/tmp/nt-frozen-trend-7212f6f.tar`、板端 `/home/neardi/Project/SOT/nt-frozen-trend-7212f6f.tar` SHA-256 均 `a518db5171334051c7770028c74af6f9e163d6da8937abf95832492b5e72e975`；板端隔离根 `/home/neardi/Project/SOT/nt-frozen-trend-7212f6f`。原生 CMake/build/CTest 23/23 PASS，`cmake.log` / `build.log` / `ctest.log` SHA-256 `a45e50e901bf31c56165cff62415643ec6c773cbd27448f8f111ab664ba7d879` / `a32c9ec945fc9765a4141e08a1627352b956b5dc62952b3f601548a3e08e131b` / `7d42e84c92148d3612480e739660d8ec18c3589f3131a1e639652b86da6e2487`。程序 `build/nanotrack_v3` SHA-256 `7b19b63c4be1b40f15a7ce550aa2a8adaf080251dd78a939d306a8f2bf41b719`，运行配置 `runs/headless-on.conf` SHA-256 `3e6a62a837aeccc8fa309037b99746969769eea5b0d639bc127c8640b6b9bb3d`；controller 配置、视频、tracker manifest、detector RKNN/manifest/labels、RKNN API/driver 与上节 `03b954e` 相同。
- 实际 `bash runs/focused.sh` / `bash runs/focused-det.sh`，脚本 SHA-256 `9a201e77175f2e103ac3dfc3256ac2207c83e01e1d50c1de5c0ee56e35de3432` / `c61c5e4ff38045273a23a751a13466555769a0883838c55617a7039e6729a880`；car001/003/004/007 真值初始化及 car001 detector 初始化，各 271 帧、CSV 272 行。板端原始 `runs/ten-target-motion/<target>/`、`runs/det-init-motion/car_001/`，板端 `/home/neardi/Project/SOT/nt-frozen-trend-7212f6f-focused.tar.gz` 与主机 `/tmp/nt-frozen-trend-7212f6f-focused.tar.gz` SHA-256 均 `a59ccabdae3af63279560b36688887ace8f853eb15a6602b86e35d4844e938fd`。正式 `scripts/evaluate_occ_video_1.py --scope full --source-commit 7212f6fe4536e29f55e92cc06550a2c06e8c6711 --config-sha256 3e6a62a837aeccc8fa309037b99746969769eea5b0d639bc127c8640b6b9bb3d --model-sha256 3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261` 评分 `/tmp/nt-frozen-trend-7212f6f-focused/runs/<run>/<target>/score.json`，五份哈希清单 `/tmp/nt-frozen-trend-7212f6f-scores.sha256` SHA-256 `ee960b73463e84679da9df0863a968981a50e66a06544d5c88f346b7ad7e2d00`。
- 真值路 001/007 PASS、003/004 FAIL；car004 新增预测连续性失败。detector 初始 car001 仍 FAIL，有效可见 IoU `0.515`，**181 帧错误重捕且早于正式首次可靠重现**。161 帧 detector 确认把冻结速度 y `-1.5→+29.5 px/s`；170 帧运动预测 y `792→807`、181 帧 `792→818`，但更接近真值的运动预测反而引导验证到了错误车辆。因此运动位置和速度失配是问题之一，单独修正不能保证目标身份；这版不能推广十目标。**否决，不合入 main/Debug，不作为生产 handoff。**本次将运行时源码恢复到 `f37b8b895faed8cfcb7f41b94fe41f608e74a724`，主机重新 build/CTest 23/23 PASS，日志 `/tmp/nt-frozen-trend-revert-{build,ctest}.log` SHA-256 `d9a65555a0cc5e89708e60ad142a2807eb033e4eb3dfc01e84a0327046c30eed` / `b8998bb1a29ce5ed508ce021981cf4076735a82587ecbe4fcb3e8134bcb26023`，算法源码与该可信起点 diff 为空。下一步需研究错误重捕候选的目标身份独立证据，不能只靠预测接近或 detector 同类框。

## 2026-11-17T21:26:14+08:00 单次位置对齐未修复 car001，改测双次确认的运动趋势

- 前一位置对齐候选 `codex/ten-target-smoothness@03b954ea5dbdc00b21170904164c7441295dfed2`，主机 `/tmp/nt-motion-position-03b954e.tar`、板端 `/home/neardi/Project/SOT/nt-motion-position-03b954e.tar` SHA-256 均 `518b8d597c8a768b62557204975f97b32de291bc9a7e465124694784e38f8b8b`。板端隔离根 `/home/neardi/Project/SOT/nt-motion-position-03b954e`，原生 CMake/build/CTest 23/23 PASS；程序 `build/nanotrack_v3` SHA-256 `58c41db3a75409689383b03f0dee0fb77da244f12eb3424fce8ab8a6c4d3ee80`，运行配置 `runs/headless-on.conf` SHA-256 `a333bf51cedc9e337376e30555be284f0fd3483f4a68332a9c481bcdd8f4ca82`，controller 配置、视频、tracker manifest、detector RKNN/manifest/labels 与上节 `1bca41f` 核验身份一致。`cmake.log` / `build.log` / `ctest.log` SHA-256 `5072e5486341e625379a07ef95d3bcf1b1ceba5c45aecf6524ad73a753181148` / `0086fb5c119f97d71235484479dad07167d481b1824099cdadb34c213b40e52d` / `fd7a93b971bfc5fc897333e1a955d73f6bd4cf06799d7c680cd066683777e361`。
- 实际 `bash runs/focused.sh` 跑 car001/003/004/007 首帧真值框（脚本 SHA-256 `d38556d306d4ca7390231b0c4fa7c49356b5d8342add9b4fe8c1160b369a2eae`），`bash runs/focused-det.sh` 跑 car001 detector 首帧框（脚本 SHA-256 `5149fedfd240d2333aab8bc59d062128d774aba7d75bbdfb8fdc9324d203e80b`）；各 271 帧、CSV 272 行。板端原始 `runs/ten-target-motion/<target>/`、`runs/det-init-motion/car_001/`，板端 `/home/neardi/Project/SOT/nt-motion-position-03b954e-focused.tar.gz`、主机 `/tmp/nt-motion-position-03b954e-focused.tar.gz` SHA-256 均 `9f6ea6c554feb0e8a4d39da8808288d80251e59b9d05943b344a0e658bb15b19`。正式 full 评分 `/tmp/nt-motion-position-03b954e-focused/runs/<run>/<target>/score.json` 五份哈希清单 `/tmp/nt-motion-position-03b954e-scores.sha256` SHA-256 `34b189316a1d174884c126735975caf6b913ad81d43655474e1f494967638321`。真值路 001/007 PASS、003/004 FAIL；detector 初始 car001 仍 FAIL，可见均 IoU `0.516`，182 帧错误重捕，比基线 184 帧更早。161 帧 `DETECTOR_CORROBORATED_POSITION` 将预测 y≈792 对齐到 y≈798，但冻结的 y 速度仍约 `-1.5 px/s`，重现时预测仍偏上。**否决位置单点对齐，不推广十目标、不合入 main/Debug。**
- 新候选不增加 detector 请求。保留最近一次已由显示刷新独立确认的、已接受 Recent 框；下一次确认时，只有两个框 IoU 达到既有显示一致性阈值、间隔不超过两倍既有 detector 新鲜期、位移速度不超过既有运动速度限制，才在冻结状态下按既有 `motion_alpha` 融合两次框中心的速度，并对齐运动位置。随后搜索仍使用该有 detector 支持的趋势；失去跟踪或重捕即清空历史。主机 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure` 23/23 PASS，日志 `/tmp/nt-frozen-trend-{build,ctest}.log` SHA-256 `7f2c31926d0cf9d5c17ad0fb161e654464008d9526b9ad2d514c9158d40655c1` / `fbe6074bc013007610d6793b6138b0e1bdd5b41f145a166e02bdf4275b5aa07f`。待提交后 RK3588 先跑同五路定向，再决定是否全十目标；不能凭主机测试或单目标改善晋级。

## 2026-11-17T21:19:00+08:00 冻结运动位置的 detector 确认对齐候选（待 RK3588）

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，起点 `f37b8b895faed8cfcb7f41b94fe41f608e74a724`。上一版提前 detector 流已否决，本候选不增加 detector 请求、不改变恢复门；仅在显示刷新已经由两次独立 detector 结果确认、且当前 Recent 被接受而运动输入仍冻结时，把运动估计的位置对齐当前跟踪框，仍沿用冻结前可信速度。动机是基线 car001 detector 初始化的 157--165 帧：已接受框 y≈795→800，而冻结运动预测 y≈792；161 帧已有 `DETECTOR_CONFIRMED`。不设车号、帧号或图像位置阈值。
- 主机实际命令 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure`，23/23 PASS；日志 `/tmp/nt-frozen-display-build.log` / `/tmp/nt-frozen-display-ctest.log` SHA-256 `d07e71da86a9a90a3437c1ed49c50a57bd4e491d2245cbb7e2db31c279f3db84` / `d1c72808056e4aec2606ee9fe441e41bf604dc5889e6ed76e38c33f13c769c72`。这只是主机候选；下一步隔离 RK3588 原生构建，先跑 car001 真值和 detector 两路，再按结果决定是否跑十目标。此前 `a1aa449` 仍为 6/10 已知可信回放基线；不合入 main/Debug，不作为生产 handoff。

## 2026-11-17T21:14:06+08:00 提前 detector 验证板端回归，否决并恢复源码

- 候选源码 `codex/ten-target-smoothness@1bca41f5dfe33721b586c01114ed826cdd5bd23f`，主机 `/tmp/nt-early-1bca41f.tar` 与板端 `/home/neardi/Project/SOT/nt-early-1bca41f.tar` SHA-256 `63a0ca81d97ef8c7d04c6fac70250e0b882c6744bc4b8871066aa8ef0eb5fce1`。板端隔离目录 `/home/neardi/Project/SOT/nt-early-1bca41f`，原生 `cmake -S rk3566/nanotrack_v3 -B build -DENABLE_RKNN_RUNTIME=ON -DBUILD_TESTING=ON -DRKNN_INCLUDE_DIR=/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/include -DRKNN_LIBRARY=/usr/lib/librknnrt.so`、`cmake --build build --parallel 4`、`ctest --test-dir build --output-on-failure`，23/23 PASS。`cmake.log` / `build.log` / `ctest.log` SHA-256 `532fb9f54a04aca126f6d80243c3de9c2010b05f4b176cada41212f35d23e32c` / `28199d480ee71ff572f6d421c8ad1ef0788ee6c132cab8f5a011eaa2dd9dd4c9` / `b9236fd21c7f0837692f36c365a00e4550b64f38028d2b4f131f736dd240f827`；程序 `build/nanotrack_v3` SHA-256 `83567a9a14f920618fdb7fbee4b27c39a38c4373354d78630d19fc4b7e3b5042`，运行配置 `runs/headless-on.conf` SHA-256 `998dabda70ccba90fc93bd356aad4de2b8564fd0408225584a50c2c23310556d`，controller 配置 SHA-256 `62607f7eb3798737fc927a1f997e0d52cc5f78a7250d8df8b23aebfb320d5f44`。输入 `data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；tracker manifest SHA-256 `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`（上节简写的“模型”即此清单），VisDrone9 RKNN 文件 SHA-256 `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c`，detector manifest / labels SHA-256 `ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9` / `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`；运行日志 RKNN API `2.1.0`、driver `0.9.8`。
- 实际命令 `bash runs/ten.sh`（脚本 SHA-256 `80a4c2ffc2a34a53ec8e18b3322e890534be7b8458dacbb1acbaff7a6cb5cf07`）使用各目标首帧真值框；`bash runs/det-init-ten.sh`（脚本 SHA-256 `874c320d3b535a106d44fe61c8b5b5179fbce4cb11a047a1718e520d78d33cf4`）使用上节记录的 detector 框或 car003 真值回退。每路 271 帧、CSV 272 行。板端原始 `runs/ten-target-early/<target>/`、`runs/det-init-early/<target>/`，板端 `/home/neardi/Project/SOT/nt-early-1bca41f-runs.tar.gz` 与主机 `/tmp/nt-early-1bca41f-runs.tar.gz` SHA-256 均 `5397049a3023d99c2a8118234faea2f414b56f0aca3237262c6f8952c3773951`；真值路提前归档 `/tmp/nt-early-1bca41f-truth.tar.gz` SHA-256 `a605e79ccce084310ba6e9751a0ab68f184fc2c615a33a658889082f7a9a75c5`。正式评测器 `scripts/evaluate_occ_video_1.py --scope full --source-commit 1bca41f5dfe33721b586c01114ed826cdd5bd23f --config-sha256 998dabda70ccba90fc93bd356aad4de2b8564fd0408225584a50c2c23310556d --model-sha256 3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261` 的报告分别在 `/tmp/nt-early-1bca41f-truth/runs/ten-target-early/<target>/score.json`、`/tmp/nt-early-1bca41f-runs/runs/det-init-early/<target>/score.json`；20 份评分哈希清单 `/tmp/nt-early-1bca41f-scores.sha256` SHA-256 `d74e53809043fb1ad61475fa7c4c8e4491a0edef626ae7b078e84a130d812cb1`，四路对比 `/tmp/nt-early-1bca41f-comparison.json` SHA-256 `7001a99b8f5cb0c2c02717169da47c43da90908aae68d9586757388a5299d850`。
- 真值初始化 **5/10 PASS**（001/002/005/006/008），较原基线 6/10 少 007；003 在 242 帧离画后错误重捕，004 出现 153/184/239 多次错误重捕，007 的 198/204/206--208 可见框低于逐帧门。detector 初始化 **4/10 PASS**（002/006/007/008），较原基线 5/10 少 005，001 仍在 185 帧错认、有效观测均 IoU `0.523`。额外 SUSPECTED_VALIDATION 请求增多，例如 003 `4→20`、004 `0→22`、005 `15→47`，并改变显示刷新与恢复时序。提前介入不能以这一组测试证明“可见时更稳”，且引入身份风险；**否决候选，不合入 main/Debug，不作为生产 handoff**。本次把运行时和单测源码恢复到 `1188982697df09c7f76cac7d30af9eef7545fb7a`，主机 build/CTest 23/23 PASS，日志 `/tmp/nt-early-guard-revert-{build,ctest}.log` SHA-256 `f2828b4bdee53c64e9ffc636e91173515252bb61f38431792ae94a5dd54ff4f2` / `90c7366ea49a6e34c919cf47111673c8ade1e30522c889cc36930b53d50b5732`，源码与起点 diff 为空。后续应研究冻结速度和 detector 验证对推理调度的干扰，再构造不增加假恢复的通用门。

## 2026-11-17T20:58:31+08:00 detector 首帧框敏感性与提前验证候选

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，起点 `1188982697df09c7f76cac7d30af9eef7545fb7a`。先在已核验的 RK3588 基线程序 `a1aa4493649db7f196ba7fb83bbe914951a692bc`（程序 SHA-256 `234730adcba1d90c4fd56551c8c68d9b1c8b9e3b5913405aaba8bd2778e67982`，运行配置 `7668c319313fdf1c21f583f9b22c8866eb0d06b8c3562c9c670a2a490e042c21`，controller 配置 `62607f7eb3798737fc927a1f997e0d52cc5f78a7250d8df8b23aebfb320d5f44`，模型 `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`）跑十目标 detector 首帧初始化 A/B。`/tmp/nt-det-init-selection.csv` SHA-256 `aae78a0277e8f84841ea2fc32bf89f311e219e3f1b795e9d3024eb0cc283ba40` 记录 ROI detector 原框、同中心最小 36×36 修正框及 car003 无合格检测时的真值回退；板端 `runs/nt-det-init-ten.sh` SHA-256 `24c39328b720741c6d716379e9925e3c3a96f2edf284864f31fb35e714b5f128` 用 `--bbox` 逐目标运行，不等同 Debug 实际点击及类别覆盖。板端原始 `/home/neardi/Project/SOT/nt-triad-safe-a1aa449/runs/det-init-ten/<target>/`，板端及主机 `/tmp/nt-triad-safe-a1aa449-det-init-ten.tar.gz` 归档 SHA-256 `c3add9c2ddd47800255fbe22d8d197298f371d83b6463ae636a60abe82c8ac6e`；主机正式 full 评分 `/tmp/nt-triad-safe-a1aa449-det-init-ten/runs/det-init-ten/<target>/score.json`。detector 初始化 5/10 PASS（002/005/006/007/008），相对真值初始化 6/10（001/002/005/006/007/008）；001 由 PASS 变 FAIL、可见均 IoU `0.761→0.520`，错误重捕 184 后持续错认。其初始框只差亚像素，但在可见期运动输入长时间冻结并保留向上的旧速度，搜索轨迹由此分叉。其余 004/009 的可见框改善仍有完全遮挡语义失败；不可根据目标编号挑较好初始框。
- 新候选在 Recent 响应首次明显下降（相对可信响应两项下降，或 PSR/APCE/峰差低于可靠阈值，且原始分数至少弱阈值）时提前提交同类别 detector 验证；可复用同帧显示刷新批次作为一条证据。仍要求两次独立新批次、同类、唯一且与源帧/当前 Recent 几何一致，detector 本身不授权控制；目标稳定后停止额外流。主机实际命令 `cmake --build /tmp/nt-ten-target-build --parallel 4` 与 `ctest --test-dir /tmp/nt-ten-target-build --output-on-failure`，23/23 PASS，日志 `/tmp/nt-early-guard-{build,ctest}.log` SHA-256 `1335e63ba689bcd093af1c13267abc0316a77dccb44b639f8bf82287f2951438` / `f313192b8b70e51c1cbf90566432a48ac3502d36a0609032872b701a8f990eea`。这是主机候选；下一步提交源码后 RK3588 原生构建，先定向再全十目标回放。生产 handoff 与 main/Debug 集成均未批准。

## 2026-11-17T20:38:29+08:00 暂态清除候选板端失败，撤回 100/200 ms 两版

- 200 ms 候选源码 `codex/ten-target-smoothness@5f5d15a78dc85e5c08958950511b445c9c33e9ad`，源码包主机 `/tmp/nt-budget-5f5d15a.tar`、板端 `/home/neardi/Project/SOT/nt-budget-5f5d15a.tar` SHA-256 均 `60133f1565521fd27ab30b61fe70603f760ee6272913a2808c759747ac0918dc`。板端隔离目录 `/home/neardi/Project/SOT/nt-budget-5f5d15a` 原生 CMake/build/CTest 23/23 PASS，日志 SHA-256 `6a5137d5cf290c06c77c3b8c97bfd85f1932a7ab65064f4bfdc215f65d71d7bd` / `46d676a1eab2a37811141566dd594ed233b4fcea71eadf402be3a4df9e869228` / `8daf11aa04cf418f99b3a224dfa57969d870e88b087a2a88d90769fc83bd4620`；程序 `build/nanotrack_v3` SHA-256 `c28c47f846b0b8e072e419da8de942a7fe8cdf95085d30b0f2655b03ad703b09`，运行配置 `runs/headless-on.conf` SHA-256 `d27764e90574f77f0faf9ac041ff2f77223d455af5edfede599089156c19d613`，controller 配置 SHA-256 `ae2146729690c324028386c7eb779fe25a3ec5344fd8a23e45d1045520136efd`。视频、模型/manifest/labels 与 RKNN 环境同上节 `311089f`；模型 SHA-256 `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`。
- 命令 `bash runs/focused.sh`，脚本 SHA-256 `b84a0e8ddcd6ac3ef28edf05ffd38270d13f7d1fa2beb0efba7f458090d2f3e5`，car002/008/009 各首帧真值框、各 271 帧、CSV 272 行；板端原始 `runs/focused-on/<target>/`，板端 `/home/neardi/Project/SOT/nt-budget-5f5d15a-focused.tar.gz`、主机 `/tmp/nt-budget-5f5d15a-focused.tar.gz` SHA-256 均 `a8f63b6d7574f8a97316ab1c2b1510456c49f2452860363531dd8e2040811684`。主机同一正式评测器 `--scope full --source-commit 5f5d15a... --config-sha256 d27764e... --model-sha256 3599d73...` 报告 `/tmp/nt-budget-5f5d15a-focused/runs/focused-on/<target>/score.json`，002/008/009 SHA-256 `150788f772487f1687085114f61b4b04cef9274208d6d92def85f4dfbe77b035` / `669560fff04aa96cdac8ba2ffd2cb2a58bfe032e2bc66692cbe4e92484040b94` / `3c2e918229fffde92a9905b8dfe5350d344d07d8dd12d87d7a1b7464eb131b52`。
- 002 仍 PASS，但有效可见 IoU `0.895→0.848`；008 从 PASS 退为 FAIL，有效可见 IoU `0.7003→0.6948`；009 仍 FAIL。009 的 115--120 完全遮挡帧全部不再接受观测，但状态保持 SUSPECTED，正式评测仍要求 SEARCHING 并报全段遮挡语义失败；因此延长暂态清除既损可见框又不满足遮挡语义。**否决 100/200 ms 两版，不跑十目标推广、不合入 main/Debug。**本提交把六份算法、配置及单测文件精确恢复到 `831b583876222c58fc8d6b8411e2eed357939d21`，主机重新 build/CTest 23/23 PASS，日志 `/tmp/nt-tentative-revert-build.log` / `/tmp/nt-tentative-revert-ctest.log` SHA-256 `baedece830e0e9b89f8f4ca117945ea89026b55103bd66c929bdebd07b085f9c` / `30e4ff5ea20c9a505c1492d040cae99f5d886851676ba212520012c5c3e8bec1`。当前可信十目标仍为 `a1aa449` 6/10；下一步应寻找能真正区分不可见与可见的独立证据，而非只延长 SUSPECTED。

## 2026-11-17T20:34:10+08:00 暂态清除使用既有 uncertainty 观察窗候选（待 RK3588）

- 起点 `codex/ten-target-smoothness@311089ff0b6ac3c6ae3007b8810be321c4637afd` 的 100 ms 板端定向结果见下节。此候选仅将 opt-in 模式的连续干净清除时长改为 `max(suspicion_clear_ms, uncertain_ms)`；当前配置两者为 100/200 ms。使用已有控制器时间预算，不新增针对车号、帧号或桥架位置的阈值。单测验证五个连续干净帧仍暂态、第六个清除；主机 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure` 23/23 PASS，日志 `/tmp/nt-tentative-budget-build.log` / `/tmp/nt-tentative-budget-ctest.log` SHA-256 `c58b75c5c2573c75f3d6052e0cb15106d603e1a7915fb34864c3760341bc8a16` / `9c6cdcc8e5ff5516d19f82290192cd3b4f5d77c35f88520150fdad4b0297d518`。下一步原生板端 car002/008/009 定向回放；若通过再十目标 full 复测。

## 2026-11-17T20:32:54+08:00 tentative 连续干净 100 ms：板端定向结果仍未过关

- 候选源码 `codex/ten-target-smoothness@311089ff0b6ac3c6ae3007b8810be321c4637afd`；源码包主机 `/tmp/nt-clear-311089f.tar`、板端 `/home/neardi/Project/SOT/nt-clear-311089f.tar` SHA-256 均 `f650850053189cbee25f85465d3a37030af65a1897c4df158425763599b540ce`。板端隔离目录 `/home/neardi/Project/SOT/nt-clear-311089f` 原生 CMake/build/CTest 23/23 PASS，日志 SHA-256 `3f24647912d0b8195acc91104c288fbd465c86bc9d6f4119b150ded0f13c86a4` / `0523f8adeea59f6ad98f8bd1b2e5a866fce81c443969ba17d70beba75eb9d279` / `adfba2764a96b9db5c87eefdfec621f65034ab979f7287a7f664eb7e469f2caf`；程序 `build/nanotrack_v3` SHA-256 `d65e9b2dfe8bcf7eb2974469d8a11cddac888e2b435e87e872df3f0f958f691d`，运行配置 `runs/headless-on.conf` SHA-256 `a7b40315f412ce57a14fc17d85cce401e88624fd603da73fe236b3fcff781db5`，controller 配置 SHA-256 `ae2146729690c324028386c7eb779fe25a3ec5344fd8a23e45d1045520136efd`（确认为 `tentative_requires_clear_interval=true`）。视频、模型、manifest、labels 及 RKNN 环境沿用并已在 `a1aa449` 板端基线核验；模型 SHA-256 `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`。
- `bash runs/focused.sh` 用 car002/008/009 各自首帧真值框初始化，各 271 帧、CSV 272 行；脚本 SHA-256 `2a4ebe250ce7dc8d78f3008adb942103ed8d0914eb34891685d61305962704d6`。板端原始 `runs/focused-on/<target>/`，包板端 `/home/neardi/Project/SOT/nt-clear-311089f-focused.tar.gz`、主机 `/tmp/nt-clear-311089f-focused.tar.gz` SHA-256 均 `7d0c060e2ef9cad82761b042e1b178693015415bb98719ea7a98f89228fa317b`。同一正式评测器 `--scope full --source-commit 311089f... --config-sha256 a7b4031... --model-sha256 3599d73...` 报告 `/tmp/nt-clear-311089f-focused/runs/focused-on/<target>/score.json`，002/008/009 SHA-256 `64d1170ad9a3b08e2c30d1058a9d478d51b92b5dc9df04a4d9b2077893824f28` / `1e2709126e82a2f40954bf255b4395ccd26679297828259cc6b04f867c59c48c` / `75e8fe2afd98c418a91b0550387e29000564b6c4406cb675b57d70466b6e4854`。
- 002、008 仍 PASS；008 有效可见观测均 IoU `0.7003→0.7073`。009 仍 FAIL，但均 IoU `0.6985→0.7881`，旧的 247 帧逐帧可见框失败消失；115 帧可疑后，116--118 帧不再接受，119--120 帧仍在完全遮挡期被接受，故遮挡语义门未通过。**100 ms 还不能替换基线**；下一步用控制器已有的 200 ms `uncertain_ms` 作为暂态清除观察窗进行定向对照，并检查可见目标的损失，不按目标编号设置运行时规则。

## 2026-11-17T20:26:18+08:00 单次响应回升不立即清除暂态可疑的候选（待 RK3588）

- 起点 `codex/ten-target-smoothness@831b583876222c58fc8d6b8411e2eed357939d21`，工作树原先干净，十目标可信基线 `a1aa449` 仍 6/10。读取同一基线 CSV 和真值：car004 在 130 帧 SUSPECTED、131 帧完全遮挡却因一次回升重新接受、132 帧又可疑；car009 在 115 帧完全遮挡进入 SUSPECTED，116 帧一次回升后重新接受并持续到 120。car010 的 149--155 帧 PSR 基本稳定，单纯加强响应阈值或这个候选都无法解释该段。基于 canonical 视频的只读外观相似度与 LK 光流探针 `/tmp/nt-appearance-probe.{py,txt}`、`/tmp/nt-flow-probe.{py,txt}`：car004 的 42 帧仍呈连续运动，car010 的 149 帧图像块与三帧前直方图交集约 0.956，外观/光流没有跨目标一致的简单遮挡阈值；探针不进入正式判定。
- 新 opt-in 配置 `tentative_requires_clear_interval=false` 默认保持旧行为；在 `visible-dual-gt-visual-gate.conf` 显式设 `true`。首次弱响应造成 tentative 后，连续干净响应达到既有 `suspicion_clear_ms` 才恢复直接接受；期间再次弱化会重新计时。已有 detector 正向 guard 仍可立即清除可疑。单测覆盖新模式的持续干净计时、配置解析及配置身份；主机 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure` 23/23 PASS，日志 `/tmp/nt-tentative-clear-build.log` / `/tmp/nt-tentative-clear-ctest.log` SHA-256 `97f2b220725f6eb0e78b541d4b58c72d3d73000a57d123942b2992e230847f78` / `c02caff1eaf5cb50b7d4535ffde7df4cd10a0200b808e6ded508b523c7ab8a58`。这仅是主机候选，下一步在 RK3588 原生构建并对 car009、car008、car002 定向回放；若有益且不损通过目标，再跑十目标。不可用主机单测声称板端效果。

## 2026-11-17T20:09:57+08:00 detector 佐证旧 Recent 的局部收益与身份退步：候选撤回

- 候选源码 `codex/ten-target-smoothness@1cf489f887927c1fced722b7d841d3c2fc3e3e2a`，提交源码包主机 `/tmp/nt-probation-1cf489f.tar`、板端 `/home/neardi/Project/SOT/nt-probation-1cf489f.tar` SHA-256 均 `00e831d15a0958cfbedc85f03d39f58fb9566c2b6fb1815976a5cc3d3b542d1f`；板端独立目录 `/home/neardi/Project/SOT/nt-probation-1cf489f` 原生 CMake/build/CTest 23/23 PASS，日志 SHA-256 `5874a5f6a99d031657620576f23891ebb5e8c5ccabd1688f6b426db11ddd06eb` / `588b66c73f7644a54e11584f9e1d760aaa0d5f7267deff7eb374a523c276b778` / `416081b5c0c7a3f3115d0aac1095e1e69b9fe472b410104a1b394e0e89c8f604`。程序 `build/nanotrack_v3` SHA-256 `7eea996402f7808ac6093e2a5ceb61d0ca14d853dc598f3ded8cb8087d60307e`；`runs/headless-on.conf` SHA-256 `29a4e1070203dbd64aa83a0e0689a924729fa52869ab3e1b3b2a5960882e8c15`，controller 配置为上节同一提交中的 `visible-dual-gt-visual-gate.conf`。canonical 视频、VisDrone9 RKNN 模型/manifest/labels、驱动版本与已核验 `a1aa449` 板端环境一致，模型 SHA-256 `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`。
- 命令 `bash runs/focused.sh`，脚本 SHA-256 `f72a1607a3b38e4fa43ff5adb86f3b473c4a1cb1ce2649ed7b689f46c71db950`，各目标用对应首帧真值框初始化，car001/003/004 各 271 帧、CSV 272 行；板端原始输出 `runs/focused-on/<target>/`，板端 `/home/neardi/Project/SOT/nt-probation-1cf489f-focused.tar.gz`、主机 `/tmp/nt-probation-1cf489f-focused.tar.gz` SHA-256 均 `9cf13c6492a4a6dd4c2b5591a34a600a033441d39f352fcfb63db7e0423cceb9`。主机同一正式评测器 `--scope full --source-commit 1cf489f... --config-sha256 29a4e10... --model-sha256 3599d73...` 的 `score.json` 位于 `/tmp/nt-probation-1cf489f-focused/runs/focused-on/<target>/`，001/003/004 SHA-256 `ce4f8dcb9fcf35d1020478b7384c5b5b04aced0f99c9096135a94d31c63979c7` / `cc7ba16f637ef44c9e0afe8dde7f8412c9dec965eaf348ede709e216b33ab6d7` / `e845d3db860f85ca65969a2b0b673560a1b9d7d3bd762a1300b8eb7988055485`。
- 001 仍 PASS，004 仍 FAIL，二者逐帧结果与 `a1aa449` 对照相同。003 在 145 帧选择旧 Recent 并撤销 staged，不重入搜索，145--151 框明显贴近真值，有效可见观测均 IoU `0.731→0.778`，旧基线 144--151 的可见框失败只剩 144。然而新轨迹在 240 帧错误重捕另一辆车，241--251 离画后仍接受；新增完全遮挡、离画与预测连续性失败。基线 240 帧有 `IDENTITY_CONFLICT` 并拒绝该候选；局部定位改善改变了后续运动/候选状态，不能推断长期身份安全。**否决并撤回该候选，不跑十目标推广、不合入 main/Debug。**本提交将三份算法/测试文件精确恢复到 `158da03f691938410ed63213b06353238f0abdd1`，主机重新 build/CTest 23/23 PASS，日志 `/tmp/nt-probation-rollback-build.log` / `/tmp/nt-probation-rollback-ctest.log` SHA-256 `aa155ed52ec65ddbc207b139e0ea2b9a85e81cbea46eaede2e503421342ba748` / `9fe8fba56b59dcaaa597cb12934eb693544a8b064e577cfdb6a8165d4c4fa716`。当前可信十目标结果仍为 `a1aa449` 的 6/10；下一步身份安全约束需要与输出几何调整同时验证。

## 2026-11-17T20:04:28+08:00 detector 佐证恢复试用期旧 Recent 模板候选（待 RK3588）

- 起点 `codex/ten-target-smoothness@158da03f691938410ed63213b06353238f0abdd1`，逆向运动门已撤回。旧基线 car003 在 144 帧恢复时 detector 框 IoU 0.622；145--147 帧旧 Recent 原始框与真值 IoU 为 0.643/0.622/0.788，但程序选择并提交的 staged 框 IoU 仅 0.38/0.35/0.34。两头 IoU 0.605/0.523/0.387 低于其他大多数正确重捕试用期的双头一致度；旧 Recent 距已确认 detector 中心仅 4--5 px，staged 距 16--19 px。证明恢复后模板选择有可检验的几何问题，单纯加 PSR 权重会选错 staged。
- 本候选只在恢复试用期保留“已经过 NanoTrack 确认的 detector 区域”作短时参考；当两头都可靠、IoU <0.70、旧 Recent 中心距该区域不超过半个 detector 框对角线、且比 staged 至少近 0.10 个对角线，并且 Recent 自身通过普通响应判定时，选择旧 Recent 作为跟踪/控制观测，撤销 staged 模板但留在 TRACKING。其余普通跟踪和无 detector 参考的恢复流程不变。单测覆盖仲裁、撤销、无需重入搜索及清理参考；主机 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure` 23/23 PASS，日志 `/tmp/nt-probation-detector-build.log` / `/tmp/nt-probation-detector-ctest.log` SHA-256 `3644b29a9939c67c40ec149d43e657000f314999f542080d9013f90e5e6c11d6` / `af0658203abd61b1c212ad50c6093a498e66f48ee2c8934a3087c7000cc20d6e`。下一步 RK3588 原生构建、car001/003/004 重点 A/B，再跑十目标回归；主机单测不证明实际框改善。

## 2026-11-17T19:58:32+08:00 逆向运动门板端定向回放失败并撤回

- 候选源码 `codex/ten-target-smoothness@607ec6ab904c800c8d592fbb5a088c70ca5bbe83`，源码包主机/板端 SHA-256 `7990126887841c5725e6c5ca06cb8ca4838f493f5409d75780a73a7b297787fc`，板端隔离目录 `/home/neardi/Project/SOT/nt-direction-607ec6a` 原生 CMake/build/CTest 23/23 PASS，三份日志 SHA-256 `b78cb72ae37fa8e20c117cc7cb85127a7ee9bad141aa6f0bf24f0ca81021d8ce` / `7f2659d1fa3d735111e0ab64e98c1b4983572641056d30d555b4918b13fd76ef` / `18fa90356f52a9f1a1aeea1c85ef0011e5fce2ae22e49e06698a4f156152e716`；程序 `build/nanotrack_v3` SHA-256 `b67869934ab7771f2a938acfa1d4b5597f3af4620b1dabae7596869d4486e70c`。运行配置 `runs/headless-on.conf` SHA-256 `85357ccf7dcb46f9858b1615e8c3891dea814fe3b4acf87e87c0952683952976`，controller 配置 `49355ef4e5357c438a1091e98b1ddeb8895d7a44248029cb35d7bc98481962ad`；视频、模型及 RKNN 身份沿用 `a1aa449` 上节已核验的输入。`bash runs/focused.sh` 用各自首帧真值框运行 car001/car003/car004 各 271 帧，脚本 SHA-256 `55bbf0a6830a39f6d697b6245c1ae651cdf56a31835325258daa18beaceb67a2`；板端原始目录 `runs/focused-on/<target>/`，板端包 `/home/neardi/Project/SOT/nt-direction-607ec6a-focused.tar.gz`、主机包 `/tmp/nt-direction-607ec6a-focused.tar.gz` SHA-256 均 `de029da3affc7c14fa41044af1ee255e73b3ce75d14b38923f581f92527e04f7`。主机解包及同一正式评测器 `--scope full` 报告 `/tmp/nt-direction-607ec6a-focused/runs/focused-on/<target>/score.json`，001/003/004 SHA-256 依次 `4ab16c97fae6cf57c3a5bf95a01ee96a3436fc83c8915fb0d925ad5b1a8629c0` / `ab633fbce3255bdbbecf64dd3559d610794d8a722f2a2ec34ecf392cc5011704` / `0d5b5d3880b77ed70cb499169ae2862d16de79132b90e0aa83031a482e344416`。
- car001 仍 PASS、189 帧重捕；car004 仍 FAIL、156 帧重捕；car003 从基线 144 帧重捕变为 186 帧错误目标重捕，首个有效重现观测从 152 变为无，整体更差。第 144 帧 detector 框与真值 IoU 0.622、Recent 原始框 0.418，检测候选并非相反运动所证明的异物；旧基线第 145--147 帧 Recent 原始框 IoU 0.643/0.622/0.788，但新 staged 模板框偏上，跟踪输出仅 0.361/0.328/0.324。这说明主问题是恢复后模板/框几何，逆向运动门误拒绝了可用目标。**否决 `MOTION_DIRECTION_CONFLICT` 规则，不跑十目标推广，不合入 main/Debug。**本提交把该规则及测试精确恢复到 `e455cd95765b4fcf3e0c164c8cf3db7026b60412`，`git diff e455cd9 -- <两个算法/测试文件>` 为空；主机重新 build/CTest 23/23 PASS，日志 `/tmp/nt-direction-rollback-build.log` / `/tmp/nt-direction-rollback-ctest.log` SHA-256 `034f9ca4df3ef8c973fe2f855a281ce2a08e1707b4a05e6727556d766029d718` / `4dc1af37baff6df07d8df4bbd13c0689795ab5d28187c22fb992001dfb697d4b`。下一步验证恢复 probation 时两模板几何与 detector 证据，避免再用陈旧速度作身份否决。

## 2026-11-17T19:50:17+08:00 逆向重捕单测修正：比较字符串内容而非指针

- `52bd88a` 在 RK3588 原生 CMake/build 成功，但 CTest 22/23；仅 `long_term_controller` 新断言失败。隔离板端诊断执行实际得到 `accepted=0, reacquired=0, recovery_reason=MOTION_DIRECTION_CONFLICT, hits=3`，算法行为符合预期；测试错误地对 `const char* recovery_reason` 使用 `==`，主机 Release 构建恰好合并了字面量指针而误通过。改为 `std::string(...)` 内容比较；板端该次测试源码临时插入状态打印，仅用于定位，不把修改后的临时构建当作提交源码验证。
- 修正后主机 Release `cmake --build /tmp/nt-ten-target-build --parallel 4`/CTest 23/23，日志 SHA-256 `e7e796a9948d4545d324346db6ae8a860ba9a328c087e8b1710d5688c2f16011` / `b8904bcbb96d02abbfd7e2a66f679001d9f961798526d1861740380c665cd42a`；独立 Debug `cmake -S rk3566/nanotrack_v3 -B /tmp/nt-direction-debug-build -DENABLE_RKNN_RUNTIME=OFF -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug`、build、CTest 23/23，日志 SHA-256 `01d07d127588ed5c185e538f64ccda881cb19c07fd8c13041a37df8a00a612f6` / `4972261dff9b29ef1af9225ac2b46c7e74902ff933a6ffa15576082144618624` / `e3418bc83d26fcef0cef7cf21ebcc65b5c5277504eb72781032bfa06912cf1f2`。下一步从本提交的干净源码重新板端构建/CTest，再开始回放。

## 2026-11-17T19:46:41+08:00 远距离逆向重捕身份门候选（待 RK3588）

- 起点 `codex/ten-target-smoothness@e455cd95765b4fcf3e0c164c8cf3db7026b60412`，双模板诊断源码已精确撤回，十目标可信 A/B 基线仍是 `a1aa449` 的 6/10。对该基线重捕帧离线计算“Recent 原始中心相对运动预测中心”距离及与可信速度的夹角：错误 car003 144 帧为 0.65 个预测框对角线、方向余弦 -0.98；正确 car004 156 帧虽然距离 1.15 个对角线，但余弦 +1.00；其他正确重捕 001/002/005/006/007/008/010 的距离均不超过 0.40 个对角线。此区分来自运动几何，而非目标编号、场景位置或车顶外观。
- 当前候选在重捕确认前，仅当预测仍可信、候选距预测超过半个框对角线、速度超过每秒 0.25 个对角线，且候选位移与速度方向余弦小于 -0.5 时，拒绝该远距离逆向身份；近距离、速度不可靠和顺向远距离候选仍走原有 detector + NanoTrack 验证。拒绝理由记 `MOTION_DIRECTION_CONFLICT`。单测覆盖远距离顺向三次验证可通过及逆向三次验证仍不可通过；主机 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure` 23/23 PASS，日志 `/tmp/nt-reverse-motion-build.log` / `/tmp/nt-reverse-motion-ctest.log` SHA-256 `3a7b6ea58524f2b89a1bb6d6293448c9f70906f7bc0c393533d805b67bdad7f4` / `9e607d53661a4c8826495200da708335c2f79940736b1ffcf9d3d1f767162c80`。下一步 RK3588 原生构建、car003/car004 定向回放与十目标 full 回归；单测与离线几何尚不能证明板端改善。

## 2026-11-17T19:41:56+08:00 双模板普通跟踪探针无身份区分力，撤回诊断开关

- 诊断源码 `codex/ten-target-smoothness@bcbce9b3e95cc4dfba4b5947bc2a700381b634ff`，源码包主机 `/tmp/nt-dualprobe-bcbce9b.tar`、板端 `/home/neardi/Project/SOT/nt-dualprobe-bcbce9b.tar` SHA-256 均 `74a1103fc3e798df8ae5f30925fb094b2a0c6b39c621430d1b5a14696e757c5c`；板端独立目录 `/home/neardi/Project/SOT/nt-dualprobe-bcbce9b` 原生 CMake/build/CTest 23/23 PASS，日志 SHA-256 `727fa07337d83c2e5eaa91444658413c74b762308be279c39e547cb2569c06d1` / `55fd69c8209d2b1b6e31519e193bbbb230f5840b0fde7bf0f1520d92ef31d55c` / `d0793f21742735efaad04cd70d5f0d5217c063d09ec4570ebcad460284ef7cf4`；程序 `build/nanotrack_v3` SHA-256 `29340a162fb5ecacd17fa24a72b3252f589f70c1d2a0112a8a03e6bf45a90270`。显式开启开关的配置 `runs/dual-probe.conf` SHA-256 `774832ed07a8d6934357579703f16ae33dacd09beb26514e1438f533e8bf84a1`、`runs/dual-probe-controller.conf` SHA-256 `49355ef4e5357c438a1091e98b1ddeb8895d7a44248029cb35d7bc98481962ad`；视频、模型、manifest、labels、RKNN 版本沿用上节同一身份。
- 脚本 `runs/ten.sh` SHA-256 `b5dcb3aa0f257ff049304cf2fe8b153de65a99f0a75b3082d99aa96a98bffd49`，命令 `bash runs/ten.sh` 用十目标各自首帧正式真值框初始化；每个 CSV 272 行。板端原始 `runs/dual-probe-ten/<target>/`，板端 `/home/neardi/Project/SOT/nt-dualprobe-bcbce9b-ten.tar.gz`、主机 `/tmp/nt-dualprobe-bcbce9b-ten.tar.gz` SHA-256 均 `d824d2d6f339af22d8d22b212f41391fa33a69af7b3a0a7bf466debc45ac7fb3`；主机解包与本评测器 full 评分 `/tmp/nt-dualprobe-bcbce9b-ten/runs/dual-probe-ten/<target>/score.{json,md}`，评测绑定源码 `bcbce9b3e95cc4dfba4b5947bc2a700381b634ff`、配置 SHA-256 `774832ed07a8d6934357579703f16ae33dacd09beb26514e1438f533e8bf84a1`、模型 SHA-256 `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`。001/002/005/006/008 PASS，003/004/007/009/010 FAIL，共 5/10；007 的可见框门退步（有效观测均 IoU 0.759，但 236 帧以后多帧低于逐帧门）。各目标普通 TRACKING 帧记录到约 139--213 个 Safe head，跟踪器耗时中位数约增加 0.4--1.3 ms，且改变了 006/007 的状态时序。
- 关键误接受帧 Safe/Recent 仍一致：003 的 144 帧误重捕双模板 IoU 0.856；004 的 42/131 帧完全遮挡仍接受时 IoU 0.967/0.964；009 的 116--120 帧约 0.715--0.887；010 的 149--155 帧约 0.892--0.986，两路 PSR 也共同保持较高。因而“Safe 与 Recent 同框”无法区分这些错误身份，不能作为直接放行证据。当前提交撤回 `tracking_safe_probe_diagnostic` 配置、计划和单测，四个算法/配置文件及对应单测与 `cfd18f0` 精确一致；主机重新 build/CTest 23/23 PASS，日志 `/tmp/nt-dualprobe-revert-build.log`、`/tmp/nt-dualprobe-revert-ctest.log` SHA-256 `4632cefddcae73b993d04b03c9461727e9ba837c9b718ee9fed82447c963fac7` / `03a0960ab9ab72995630e177e6d9ec2c450f43ba8bffc26353c7a5f2db587b77`。保留原始诊断包；继续从运动方向与候选身份约束探索，而不是收紧 PSR 或按目标编号裁剪。

## 2026-11-17T19:32:09+08:00 可选双模板跟踪探针（仅诊断，待板端）

- 起点 `codex/ten-target-smoothness@cfd18f0772535f1431bff41cb89cf6c5f15c5d9c`，工作树干净，上一十目标回放 6/10。剩余四目标失败中 003/004/009/010 均有完全遮挡仍接受观测的帧；其中 009/010 的 Recent PSR 仍较高，单调提高 PSR 门限不具通用性。先收集固定 Safe 模板与 Recent 模板在普通 TRACKING 的同帧响应及 IoU，核查是否有跨目标区分信号。
- 新配置 `tracking_safe_probe_diagnostic=false` 默认不改变现有运行；显式设为 `true` 时，普通跟踪帧额外运行 Safe head，现有控制器仍按 Recent 决策，输出记录 Safe 原始响应和双模板 IoU。恢复期、probation 及未开启配置的路径不变。配置身份只在开关为真时追加新字段，避免默认配置无行为变化却变更原身份。单测验证配置身份和普通 TRACKING 双模板计划；主机 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure` 23/23 PASS，日志 `/tmp/nt-dual-probe-build.log`、`/tmp/nt-dual-probe-ctest.log` SHA-256 `8526b9857d9292ecb328f280917b991f885e4181a4e0ff130fe7e1cd86b1a3cc` / `b499b96f3a29177e7a6f62dcb642ecbeffdccc1b3094af4fcdea81f50dc1fcf9`。下一步在 RK3588 隔离目录开关开启跑失败目标与已通过对照，检查原始 Safe 证据、运行耗时和状态影响；本候选不是判定策略或新基线。

## 2026-11-17T19:25:05+08:00 保留普通冲突检查的三方候选：RK3588 十目标 6/10

- 源码 `codex/ten-target-smoothness@a1aa4493649db7f196ba7fb83bbe914951a692bc`，包主机 `/tmp/nt-triad-safe-a1aa449.tar`、板端 `/home/neardi/Project/SOT/nt-triad-safe-a1aa449.tar` SHA-256 均 `91de16bbedc27e9af70e3139ded167344b1fc15ff0c1a33ecebc5af311fc81f7`；板端独立根 `/home/neardi/Project/SOT/nt-triad-safe-a1aa449`。原生 CMake 命令及 RKNN include/library 与上节 0be8f30 一致，build/CTest 23/23 PASS；`cmake.log` / `build.log` / `ctest.log` SHA-256 `325ad98ce8f3ad3e54628b9f6cb55b9a569795836524852d33beb536851d6db4` / `925ee2b87977993ad1f2618da2c9b781a9cb1bd2823b794ddf847341629b7863` / `5f45dd7cb2d11464de7f6cb0c617acf89a43eb43d419e85aebcd3d6f2bc41cef`。程序 `build/nanotrack_v3` SHA-256 `234730adcba1d90c4fd56551c8c68d9b1c8b9e3b5913405aaba8bd2778e67982`，运行配置 `runs/headless-on.conf` SHA-256 `7668c319313fdf1c21f583f9b22c8866eb0d06b8c3562c9c670a2a490e042c21`；controller 配置 SHA-256 `62607f7eb3798737fc927a1f997e0d52cc5f78a7250d8df8b23aebfb320d5f44`，视频、模型、manifest、labels 和 RKNN 版本沿用上节同一路径及已核验身份。
- 先用 `bash runs/two.sh` 跑 car001/car003（脚本 SHA-256 `e67003559e6ed187f7922107bcdb95cb6c1d43efafbfff8225599725f303e5f5`）：每目标 271 帧，car001 在 189 帧重捕，car003 仅 144 帧重捕，不再出现 226/243 帧假重捕。板端包 `/home/neardi/Project/SOT/nt-triad-safe-a1aa449-two.tar.gz`、主机 `/tmp/nt-triad-safe-a1aa449-two.tar.gz` SHA-256 均 `0368fb707653d9829a2f2811d7627dfc88fcaf1a22d996c8e4c32f800ca8e0ae`；原始目录 `runs/two-target/<target>/`。
- 再用 `bash runs/ten.sh` 跑十目标（脚本 SHA-256 `c13a1ba6c5582427925e713a2ca82c2ae2d188e34401f4da569a076aabe8c164`），各目标首帧正式真值初始化、exit 0、CSV 272 行。板端原始目录 `runs/ten-target-on/<target>/`；板端 `/home/neardi/Project/SOT/nt-triad-safe-a1aa449-ten.tar.gz`、主机 `/tmp/nt-triad-safe-a1aa449-ten.tar.gz` SHA-256 均 `5114cba2cfe57618342e023a1474834172bbcef4beddb661509563413f9e4dfb`。主机解包及当前 `scripts/evaluate_occ_video_1.py --dataset-root /home/u20/single-object-tracking/data/occ_video_1 --target-id <target> --boxes .../frames.csv --session-metadata .../session.json --scope full --source-commit a1aa4493649db7f196ba7fb83bbe914951a692bc --config-sha256 7668c319313fdf1c21f583f9b22c8866eb0d06b8c3562c9c670a2a490e042c21 --model-sha256 3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261` 评分在 `/tmp/nt-triad-safe-a1aa449-ten/runs/ten-target-on/<target>/score.{json,md}`；汇总 `/tmp/nt-triad-safe-a1aa449-summary.json` SHA-256 `dd6e3fdcc958204f057b89e2bc795caf4f19df2e095c0be117748ca900500170`。
- PASS：001（有效观测 IoU 0.761、189 帧重捕）、002（0.895、228）、005（0.767、182）、006（0.804、215）、007（0.783、227）、008（0.700、258），共 **6/10**。FAIL：003（130 帧遮挡语义、144 帧错误身份、144--151 可见框）、004（42/131--132 遮挡语义、预测连续性、127--129 可见框；156 帧重捕本身通过）、009（115--120 遮挡语义、有效观测均 IoU 0.698）、010（149--157 遮挡语义；可见框均 IoU 0.867）。相对上一候选 0be8f30，car003 新增的 226/243 假重捕和离画后接受已消失；相对诊断基准 1bede96，car001 的超窗恢复 192 帧提前至 189 帧，其余已通过目标保持 PASS。**仍有四目标失败，不设为新的集成/生产基线，不合入 main/Debug。**下一步分析共同的完全遮挡漏判与相邻车身份切换，避免通过真值帧号定制规则。

## 2026-11-17T19:14:03+08:00 修复三方候选实验中的普通冲突漏检（待板端）

- 基线 `codex/ten-target-smoothness@3f6ffd40113580b3b323666841bdeee8b04b79ff` 工作树干净，实验分支源码仍与只读诊断版 `1bede963aaab74262798dc260a8ddb3d03d1ff78` 相同。复查前一被拒候选发现具体回归：`RecoveryCandidateTable::observe()` 新增的 `if (matched) break` 在**普通匹配**第一次命中后也停止遍历，漏检第二个匹配身份；car003 第 222 帧冲突由 `UNREPRESENTED_MATCH` 变为 `UNRESOLVED_OVERFLOW`，223 帧起搜索响应和裁剪方向分叉，226/243 帧假重捕。
- 当前修复候选只在当前帧已由唯一三方 detector 明确指定候选时跳过普通匹配遍历；否则遍历全部候选并保留原有双重匹配冲突。仅在 detector 中心距运动预测中心不超过 0.5 个预测框对角线时允许该特殊指定。单测新增普通双重匹配必报 `UNREPRESENTED_MATCH`，保留此前同目标重复候选可合并、重叠 detector 对手仍不可合并的测试。主机 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure`，23/23 PASS；日志 `/tmp/nt-triad-preserve-ambiguity-build.log`、`/tmp/nt-triad-preserve-ambiguity-ctest.log` SHA-256 `1aac44d08967b0612a30ad2cd620cb7900d84aaba39f93aeaf35a12ace965a23` / `33a9d690bbfc26b459c7e5497147469448b0a1ad3d9d76758eb4d`。下一步在 RK3588 原生构建、先跑 car001/car003 再根据结果跑全十目标；尚未认定安全或设为基线。

## 2026-11-17T19:09:09+08:00 运动邻近候选完成复测，拒绝推广并恢复算法基准

- 候选源码 `codex/ten-target-smoothness@0be8f308b15657fbe0a7ba0b8e7cfb79f849ee74`，主机/板端包 `/tmp/nt-motion-0be8f30.tar`、`/home/neardi/Project/SOT/nt-motion-0be8f30.tar` SHA-256 均 `f0e4116e26aa8b839b5376b7574f5b3e1d55ace6096b1a6c09305224aae84ad1`；板端独立目录 `/home/neardi/Project/SOT/nt-motion-0be8f30` 原生配置命令 `cmake -S rk3566/nanotrack_v3 -B build -DENABLE_RKNN_RUNTIME=ON -DBUILD_TESTING=ON -DRKNN_INCLUDE_DIR=/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/include -DRKNN_LIBRARY=/usr/lib/librknnrt.so`，build/CTest 23/23 PASS。`cmake.log` / `build.log` / `ctest.log` SHA-256 `dc9e3f6e20f9ed20e3275a924c0752a72ab9ec23248813d059e9ffefdf5d73b5` / `96a6a37a6d323c30d4f65bdf9e71349f28288652656b91f4c0095726e1255c55` / `be6424d813fd128ce857b647b2efd6a878e99b0dcadc5f77bf89afc0c4e39b3c`；程序 `build/nanotrack_v3` SHA-256 `bbadb6fec971d57083c55710c054783712dc0fd81551a8d7b5e6fc1d20f7ccd9`，配置 `runs/headless-on.conf` SHA-256 `1b82a25d64ae8d70b27a791f66bb5fc25c7dbb32a668aa6651ab3ece0d174455`，controller 配置和模型等输入与上节相同。脚本 `runs/ten.sh` SHA-256 `5dcfe2b3757d1799eefbbed4f05b8d06dd0c0dbf24cced442503a3cad79134d4`，命令 `bash runs/ten.sh` 逐目标以首帧真值框运行，十个 CSV 各 272 行。
- 板端原始输出 `runs/ten-target-on/<target>/`，板端 `/home/neardi/Project/SOT/nt-motion-0be8f30-ten.tar.gz` 与主机 `/tmp/nt-motion-0be8f30-ten.tar.gz` SHA-256 均 `bedbd4f6da283bd77cc5e5543fed0a2044cec1a427be9d0a634f3a5267aeec7d`；解包和 `--scope full` 评分在 `/tmp/nt-motion-0be8f30-ten/runs/ten-target-on/<target>/`。`001/002/005/006/007/008` PASS，共 6/10；car001 重捕 189 帧达标，car003 仍错误重捕 144/226/243 帧，car004/009/010 仍 FAIL。car006 此次 PASS，但前一候选曾 FAIL，不能认定此改动稳定改善了 car006。
- 再以相同视频、配置及 car003 首帧真值框，在诊断基准 `1bede963aaab74262798dc260a8ddb3d03d1ff78` 和候选 `0be8f308b15657fbe0a7ba0b8e7cfb79f849ee74` 各独立重跑一次：命令 `./build/nanotrack_v3 --config runs/headless-on.conf --bbox 986.06016,669.34998,39.88992,37.99980 --output-boxes runs/repeat-car003/frames.csv --session-metadata runs/repeat-car003/session.json`。基准仅 144 帧重捕，候选稳定复现 144/226/243。板端 `/home/neardi/Project/SOT/nt-car003-repeat-pair.tar.gz`、主机 `/tmp/nt-car003-repeat-pair.tar.gz` SHA-256 均 `30d5c8b7668a832582247058c0f05d1d50ed68e6c122cb90a82d82f02b90a899`；主机解包 `/tmp/nt-car003-repeat-pair/`，full 评分 JSON SHA-256 基准 `ae506c67242c180031a6ed64b68d18118218a1c45cd0152e25a69e794c37b291`、候选 `4e3cce27776240744b69f653c8e0b894018267c2e020163825d1c68fbb40eb78`。候选在 223 帧 Recent/Safe 原始响应开始与基准分叉，随后连续搜索到错误目标；近运动门没有阻止此状态链。因此 0be8f30 **拒绝推广**。
- 本提交把 `recovery_candidates.hpp/.cpp`、`long_term_controller.cpp` 和对应单测精确恢复到诊断基准 `1bede963aaab74262798dc260a8ddb3d03d1ff78`（`git diff --name-only 1bede963 -- <四个文件>` 为空），保留只读诊断字段与上述证据。主机重新 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure` 为 23/23 PASS；日志 `/tmp/nt-revert-duplicate-build.log` / `/tmp/nt-revert-duplicate-ctest.log` SHA-256 `7f63b35e6c98c8f5ad6e07a695e1b058def75981b0724a21973632e443fb9c34` / `8cc64e2e5b11c5b58248ea5e0d580d1be8b93f715d0549275804639b0aace539`。main/Debug、正式生产 handoff 与新基线均未变。下一步针对 223 帧开始的搜索裁剪/候选表状态链设计更强的身份约束后再完整复测。

## 2026-11-17T18:55:26+08:00 重复候选覆盖的十目标回放与运动邻近限定

- 板端源码 `codex/ten-target-smoothness@12defc572e575a7bfe9197684ad7c1705451ad50`，主机/板端源码包 `/tmp/nt-triad-12defc5.tar`、`/home/neardi/Project/SOT/nt-triad-12defc5.tar` SHA-256 `2653bd23b3351b30a3cba730c9916e018cd3f47440aa34886f06f2b91e6114e3`。板端原生构建目录 `/home/neardi/Project/SOT/nt-triad-12defc5`，CMake/build/CTest 23/23 PASS，日志 SHA-256 分别为 `6b5c412742987555f39c20c6d42b61c4144188d04bac84383d102b796566c2f9` / `d2c8d53cd0571dfc8824022207c972025335df902e3fa139e7b6d54adb1c2853` / `fae604098d72a78cd24586a38daf47ea773469e20236fa288e827e2332c0064f`；程序 `build/nanotrack_v3` SHA-256 `950121d427ab0cfd7ca74211f081cbb120cd2f46f8fb90f26a6d7eded77e5a4a`。运行配置 `runs/headless-on.conf` SHA-256 `c7c50140b7ea082648b5e1809d35ae066eaedf4a8a9362cd5f37643459990d76`，controller 配置 SHA-256 `62607f7eb3798737fc927a1f997e0d52cc5f78a7250d8df8b23aebfb320d5f44`；视频/模型身份沿用下节 26785db 回放。脚本 `runs/ten.sh` SHA-256 `a8f7bda65495142b02d31115c86b8112d680f0084c0ca4866ed2be9c2dc68a5b`，各目标用首帧正式真值框运行 271 帧，十个 `frames.csv` 均 272 行。
- 板端原始输出 `runs/ten-target-on/<target>/`，板端 `/home/neardi/Project/SOT/nt-triad-12defc5-ten.tar.gz` 与主机 `/tmp/nt-triad-12defc5-ten.tar.gz` SHA-256 均 `9bcfd464732c9affcf622aa38cbc1d6c20e360abfc93e361e24bd6913093e5e5`；主机解包及 full 评分 `/tmp/nt-triad-12defc5-ten/runs/ten-target-on/<target>/`。按当前 `scripts/evaluate_occ_video_1.py --dataset-root /home/u20/single-object-tracking/data/occ_video_1 --target-id <target> --boxes <target>/frames.csv --session-metadata <target>/session.json --scope full --source-commit 12defc572e575a7bfe9197684ad7c1705451ad50 --config-sha256 c7c50140b7ea082648b5e1809d35ae066eaedf4a8a9362cd5f37643459990d76 --model-sha256 3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261` 评分，`001/002/005/007/008` PASS，其余 FAIL，仍 5/10。car001 重捕由 192 帧提前至 189 帧，满足 `[182,190)`；car003 新增 226/243 帧错误重捕和离画后接受，car006 第 199 帧遮挡语义退步。**12defc5 不可作为新基线。**
- 逐帧诊断 car003 新误重捕在 225/242 帧 detector 中心距运动预测中心约 0.74/0.98 个预测框对角线；car001 正确重捕在 188 帧仅 0.23。当前候选仅在三方唯一 detector 与运动预测中心相距不超过半个预测框对角线时，允许覆盖重复 tracker-only 候选冲突；其他三方证据仍走既有候选表判定。这是对“冲突覆盖”范围的限制，不能用目标编号或真值做运行时门限。主机 `cmake --build /tmp/nt-ten-target-build --parallel 4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure`，23/23 PASS，日志 `/tmp/nt-triad-motion-near-build.log` / `/tmp/nt-triad-motion-near-ctest.log` SHA-256 `95fae9f498f3413b16ad5f73456eb4bcd68f752532124103e1df2a2c15208782` / `f11c6a9c3b15db6ea77604b170629e43e6b1490761ce492cbc151041fabea19b`。下一步板端原生构建和十目标完整复测，核查 car001 保留提前重捕、car003 不再新增假重捕、car006 不因候选修正退步。正式生产 handoff 未批准。

## 2026-11-17T18:44:04+08:00 三方唯一候选被重复 tracker 候选误拦截：诊断与修复候选

- 诊断源码 `codex/ten-target-smoothness@1bede963aaab74262798dc260a8ddb3d03d1ff78`，源码包主机 `/tmp/nt-recovery-evidence-1bede96.tar`、RK3588 `/home/neardi/Project/SOT/nt-recovery-evidence-1bede96.tar` SHA-256 均 `ec917dc1473f3c1c14f0776b5cd0935c4bff2047aa1d4d51176fc6976d19a012`。板端独立目录 `/home/neardi/Project/SOT/nt-recovery-evidence-1bede96`，原生 CMake/build/CTest 与上节同选项且 23/23 PASS，`cmake.log` / `build.log` / `ctest.log` SHA-256 `5f172cd012d0dca21a56d01b8e4427451c2a53a622fdaa62d8da72de05deb4a7` / `4e73fcaa05979da48ddbd810827c45a7c043b46055cfcefa1a603e313a20ef0e` / `3dc940710c8782817710881f76f1ab098b808412bf92871b14d0d4fa3c7e28d6`；`build/nanotrack_v3` SHA-256 `4fa929248ab8e55442887cfa96e38fc44b69880e7cd1d023615a836161474905`，运行配置 `runs/headless-on.conf` SHA-256 `a53de3be5a2a0e43495b2c118a7c7953ef7ac5daba4b43d96ce96791f7bce879`，controller 配置 SHA-256 `62607f7eb3798737fc927a1f997e0d52cc5f78a7250d8df8b23aebfb320d5f44`。canonical 视频/模型与 26785db 相同。
- 用首帧正式真值 `--bbox` 跑 car001/car003 各 271 帧、exit 0；脚本主机 `/tmp/nt-recovery-evidence-1bede96-two.sh`、板端 `runs/two.sh` SHA-256 均 `94d6d30bc25e4dce49edda299325b4567915d5f7cf0f3a27bd98a3c50be418aa`。板端原始 `runs/two-target/<target>/`，包板端 `/home/neardi/Project/SOT/nt-recovery-evidence-1bede96-two.tar.gz`、主机 `/tmp/nt-recovery-evidence-1bede96-two.tar.gz` SHA-256 均 `e11e0e44861fb6cee5f54d275c4af0f04b6ab9c0e95a5a5ad857f79288ff05cf`。用 `scripts/evaluate_occ_video_1.py --dataset-root /home/u20/single-object-tracking/data/occ_video_1 --target-id <target> --boxes /tmp/nt-recovery-evidence-1bede96-two/runs/two-target/<target>/frames.csv --session-metadata .../session.json --scope full --source-commit 1bede963aaab74262798dc260a8ddb3d03d1ff78 --config-sha256 a53de3be5a2a0e43495b2c118a7c7953ef7ac5daba4b43d96ce96791f7bce879 --model-sha256 3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261` 评分，报告 `score.json` SHA-256 car001 `2f316c169b9c88149337d535e7687aabd15c1ccc3248a0e7f2b157d8378f64a5`、car003 `ae506c67242c180031a6ed64b68d18118218a1c45cd0152e25a69e794c37b291`，仍分别 FAIL、192/144 帧重捕。与 26785db 同目标记录逐帧比较状态、接受、重捕、关键框和候选字段 **0 差异**，证明诊断版未改算法路径。
- 新字段揭示：car001 188 帧 Recent/Safe 均 RELIABLE，当前帧三方唯一 detector 候选已聚焦，却在将 tracker 证据写入候选表时因另一个匹配同一框的旧 tracker-only 候选记 `UNREPRESENTED_MATCH` 并清空确认；189--190 持续冲突，191--192 才恢复。car003 143--144 帧三方检验也成功，但没有这种冲突。基于当前帧**唯一 detector 与双 tracker**一致时，修复候选只将这两个 tracker 证据绑定到已聚焦 detector key，仍校验 detector 几何和同来源时间连续性；第二个 detector 若也匹配，三方聚焦本身拒绝，保持身份歧义。主机 `cmake --build /tmp/nt-ten-target-build -j4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure -j4` 23/23 PASS，日志 `/tmp/nt-triad-duplicate-build.log` / `/tmp/nt-triad-duplicate-ctest.log` SHA-256 `8f195154f4946d3b373a24894d3bfdb296e016c642add47ae7971b962fb1462e` / `2ff48b235e8d9be33ccd3249e336373754d64398af9b6cfa92007e40373bc95a`；新增正反例覆盖重复 tracker-only key 不否决唯一三方和两个 detector 身份仍不放行。**修复候选尚无板端结果，不合入 main/Debug**；下一步完整十目标回放，检查 car001 提前恢复及 car003/004/009 身份风险。

## 2026-11-17T18:37:43+08:00 恢复冲突证据诊断候选（主机阶段）

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，起点 `85c35cb448bfb03a837a80039c57b54de22b5bee`。当前十目标严格结果仍 5/10。car001 的 188--190 帧已知 Recent/Safe 原始框高度重合、响应分数较高、ROI detector 目标候选靠近预测中心，但运行时均记 `IDENTITY_CONFLICT`，191--192 才确认；car003 在 144 帧重捕后可见框偏上，且上节融合实验证明任意改变内部恢复中心会在该序列引入后续错误身份。不能只凭视觉上最近或 detector 框直接放宽冲突门。
- 本次只增加 `RECOVERY_EVIDENCE` 日志：记录当前 Recent/Safe 质量级、是否满足三方检验前置条件、是否成功聚焦唯一 detector 候选，以及候选表冲突原因（未表示的多重匹配、未解决溢出、已聚焦候选的对手、多个候选或探针不一致）。不修改候选选择、确认计数、控制授权、跟踪框、配置或模型。只在恢复等待/冲突/提交事件写入 `frames.csv` 的 `note`；现有 schema 不变。实际执行 `cmake --build /tmp/nt-ten-target-build -j4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure -j4`，主机 23/23 PASS；日志 `/tmp/nt-recovery-evidence-build.log` / `/tmp/nt-recovery-evidence-ctest.log` SHA-256 `39aebb9c7bde762ff7424db03feed22374fbc15c8431bb94d24d6e889150c3bd` / `07ce170d62e5bbe4022187e2cffdae0cf21691d840c1ecfff28cc63cf5e4a702`。下一步提交精确源码，在 RK3588 原生编译并以同首帧真值框回放 car001/car003，核对状态与上一版一致后再根据诊断选择安全改法；不合入 main/Debug。

## 2026-11-17T18:32:52+08:00 恢复中心融合十目标闭环不增通过且触发错误重捕，撤回算法

- 板端测试源码 `codex/ten-target-smoothness@998569a43ed8e5f1f1e8c2d655d405a35395eb80`，源码包主机 `/tmp/nt-blend-998569a.tar`、RK3588 `/home/neardi/Project/SOT/nt-blend-998569a.tar` SHA-256 均 `60472eb992066b5634743fd85598b5ba5d19de68075eb1051e4168e53ddd24ea`；原生目录 `/home/neardi/Project/SOT/nt-blend-998569a`。原生 CMake/build/CTest 使用上节 RKNN 路径及同命令，23/23 PASS；`cmake.log` / `build.log` / `ctest.log` SHA-256 依次 `ab1b1eee27b6b7eea88f7212458792dae60852d82326f4d5f06811c2981684d1` / `98d91c2c2afee48ecbc25d5b6eafc177187e519eccf9850278c70ea7dbaad38b` / `1b287f8da7f5fcc8de2525745fbb04e691b138c2a182de57c7437853f1e99dab`；`build/nanotrack_v3` SHA-256 `d004ea6e8b59bfbfcf0608a364a32739924ba5ef73de5c62acaaf56a2c6ffa86`。运行配置 `runs/headless-on.conf` SHA-256 `728c1b94522b7f442bb8dd7e9452709a4d273b672d723ee9f4ed61c0b5e78029`，仅将上版配置的 controller 路径改为本源码目录；controller 配置为上节 `0.25` 候选。canonical 视频及 tracker/detector 模型身份与 26785db 对照相同。
- 主机脚本 `/tmp/nt-blend-998569a-ten.sh`、板端 `runs/ten.sh` SHA-256 均 `a0e57b20cff218106fb5745276eff4594ec412636e6424ccd1061a1ce7b12ad0`，按十目标各首帧正式真值使用 `./build/nanotrack_v3 --config runs/headless-on.conf --bbox <首帧框> --output-boxes runs/ten-target-on/<target>/frames.csv --session-metadata runs/ten-target-on/<target>/session.json`，各 exit 0、271 帧。板端 `/home/neardi/Project/SOT/nt-blend-998569a/runs/ten-target-on/<target>/` 保留原始 CSV/session/log；结果包板端 `/home/neardi/Project/SOT/nt-blend-998569a-ten.tar.gz`、主机 `/tmp/nt-blend-998569a-ten.tar.gz` SHA-256 均 `50adc4ed257d1694fd9dc9e846b29d3da7667a1a2c50fab0612ba85bdd6a5fdc`。主机同版评测器 `python3 scripts/evaluate_occ_video_1.py --dataset-root /home/u20/single-object-tracking/data/occ_video_1 --target-id <target> --boxes /tmp/nt-blend-998569a-ten/runs/ten-target-on/<target>/frames.csv --session-metadata .../session.json --scope full --source-commit 998569a43ed8e5f1f1e8c2d655d405a35395eb80 --config-sha256 728c1b94522b7f442bb8dd7e9452709a4d273b672d723ee9f4ed61c0b5e78029 --model-sha256 3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`，各报告 `score.{json,md}`；汇总 `/tmp/nt-blend-998569a-summary.json` SHA-256 `b196f2b00bf7afc2fd60526dbc346b5efc4c8fab7719d64d44438ba9d4833467`。
- 严格结论仍 **5/10**：002/005/006/007/008 PASS；001/003/004/009/010 FAIL。car003 恢复帧 144 IoU 由 0.380 提升至 0.455，145--151 也局部改善，但整体仍错位，并在 225 完全遮挡及 244 离开视野时**新增两次错误重捕**，新增 `out_of_view_semantics` 与 `prediction_continuity` 失败；car010 176 帧 IoU 0.554→0.633，149--157 完全遮挡漏判不变。收益不能抵消身份回退，否决该内部 rebase。已将 `998569a` 所改八个算法/配置/单测文件恢复到 `bb6b23c7b343891b27f98d425b45b205b13b004d` 的字节内容，`git diff bb6b23c -- <八文件>` 无差异；只读 ROI 探针及历史提交保留。恢复后主机 build/CTest 23/23 PASS，日志 `/tmp/nt-recovery-blend-rollback-build.log` / `/tmp/nt-recovery-blend-rollback-ctest.log` SHA-256 `81ca769688f7b961fa0f11a4b0f563a7c767a098a65045e5f7a115a1f20672cd` / `92e978e35ac19b26788bc15ab1877f6627e2a060f0214139c17b13ed9a9077d6`。**不合入 main/Debug、不更新基线或生产 handoff**；下一步需要不改变身份路径的可见框几何改进，且优先解决 003/004/009 的错误或缺失身份证据。

## 2026-11-17T18:21:33+08:00 已确认恢复的 detector 中心小比例融合候选（主机阶段）

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，起点 `bb6b23c7b343891b27f98d425b45b205b13b004d`，本节源码与任务进度随同一提交。十目标原始 26785db 输出的恢复帧离线对照：car003 NanoTrack IoU 0.380，保持 NanoTrack 宽高、将中心向已锁定 detector 区域移动 25% 后约 0.455；car010 0.554→0.633，car005 0.891→0.821，car008 0.720→0.700。此为单帧几何反事实，不是重跑结果，后续裁剪与身份仍须板端闭环验证。
- 新增默认 `0`、可配置 `[0,1]` 的 `recovery_detector_center_blend`，仅在已由 NanoTrack 确认重捕、完整目标 detector 区域已锁定时，对**恢复中心**做有界比例融合；恢复宽高仍来自 NanoTrack，调整后的框一致用于控制输出、内部 motion rebase 和模板暂存。detector 不能单独授权控制或恢复。实验配置 `rk3566/nanotrack_v3/config/visible-dual-gt-visual-gate.conf` 设为 `0.25`，SHA-256 `ef8191ab1ea3b03c6e02369d257185d6097ffec47e5b509251c3c4d5cad8ddce`；`frames.csv` 的 `note=RECOVERY_DETECTOR_CENTER_BLEND` 标识应用帧。配置身份包含新字段；默认路径仍为 NanoTrack 原几何。
- 实际执行 `cmake --build /tmp/nt-ten-target-build -j4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure -j4`，主机 RKNN-off CTest 23/23 PASS，日志 `/tmp/nt-recovery-blend-build.log` / `/tmp/nt-recovery-blend-ctest.log` SHA-256 `b4041b64303248fa99f2d848b6f1dc3ddc5bc49a33e54b26ec414fc80605553f` / `cd2af40957d711140465bec620b3798506ba01028b4d173caba62fbe824e6a5f`；`git diff --check` PASS。单测覆盖解析非法值、只有锁定的完整目标候选参与融合、无 detector 的恢复不融合、宽高不随 detector 变动、内部下一帧裁剪中心与恢复输出一致。**尚无该算法板端结果**；下一步 RK3588 原生编译及同视频/模型/真值首帧框完整十目标回放，对照 26785db 原始结果，不合入 main/Debug。

## 2026-11-17T18:14:16+08:00 640 填满 ROI 尺度在关键失败目标上被否决

- 独立诊断源码 `codex/ten-target-smoothness@779a07fd9ddbea9765a47eda2f5c37e2511b383c`（正式算法仍为 `26785dbcff4ac83f0045e1cd7868fbbc0e5b634d`），源码包主机 `/tmp/nt-fill-779a07f.tar`、RK3588 `/home/neardi/Project/SOT/nt-fill-779a07f.tar` SHA-256 均 `07d5a0a947c58347f25e03326add4508cf2f009ce1a24c7727f8d8a370aa4197`。板端独立目录 `/home/neardi/Project/SOT/nt-fill-779a07f`；首次 CMake 因未传 RKNN 路径失败，随后使用 `cmake -S rk3566/nanotrack_v3 -B build -DENABLE_RKNN_RUNTIME=ON -DBUILD_TESTING=ON -DRKNN_INCLUDE_DIR=/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/include -DRKNN_LIBRARY=/usr/lib/librknnrt.so`、`cmake --build build --parallel 4`、`ctest --test-dir build --output-on-failure -j4` 成功，CTest 23/23 PASS。构建日志 `cmake.log` / `build.log` / `ctest.log` SHA-256 依次 `c9bdfb6d6805d98db39dfd1db2570bd38cf4f12fae339c56852b67c29c8abed6` / `211ebd61f0ebb6f9489f8b155d173dddaf4591c9cefbaf2fa6c79cfcd16a7395` / `f108f63dbdc61e8822ae3289973dbdf5ebbe893163e55b9e8e5941cec4998d56`；`build/yolo_roi_replay` SHA-256 `ffd597bf5cf66eeb79d98d29e4ba2354c7a94314362d82617965382a765b795e`。
- 使用上节同一十目标 ROI trace、canonical 视频和 VisDrone9 模型，实际运行 `./build/yolo_roi_replay <detector_model> <detector_manifest> <detector_labels> <video> /home/neardi/Project/SOT/nt-detector-presence-26785db/<target>.roi /home/neardi/Project/SOT/nt-detector-presence-26785db/<target>.fill.csv fill`，选择决定性的 car010/car003，各 exit 0、271 帧。CSV SHA-256 分别 `4e3d131a67158d16ce4469c263539bb01f9a4e579a73c2ac695a14ae91349fb7` / `304734e537b97eacdba5a7c0e97964aff24e1c157b684e884845ab2559e4d3c3`；日志随结果包板端 `/home/neardi/Project/SOT/nt-fill-779a07f-probes.tar.gz`、主机 `/tmp/nt-fill-779a07f-probes.tar.gz` SHA-256 均 `2ad16440bee61f1c1482649ea5ee0829431563788dd534b5d163378cca36e4ec`。
- 近邻定义为候选中心距预测中心 `<=30px`。car010 遮挡前 15 个可见帧，标准 320 内容模式的 `confidence>=0.40` 命中 `6/15`，填满 640 模式降到 `0/15`；car003 同窗口从 `0/15` 到 `3/15`，但临近遮挡的最后五帧依然全部未检出。放大尺度未形成可靠可见性证据，故**不修改正式 detector 权重或遮挡判定**；诊断探针保留用于复现，不推广到 main/Debug。十目标正式结果仍 5/10，下一步转向恢复候选的身份连续性分析和独立的无遮挡/遮挡证据。

## 2026-11-17T18:09:50+08:00 十目标 ROI detector 可见性探针与尺度实验准备

- 算法仍是 `codex/ten-target-smoothness@26785dbcff4ac83f0045e1cd7868fbbc0e5b634d`，评测提交 `846b066b407c4b96a066dbf8823245d2998a5dee`；无正式 runtime、模型或控制输出改动。以该算法十目标板端 CSV 的每帧 `motion_predicted_bbox` 生成 `/tmp/nt-detector-presence-26785db/car_001.roi` 至 `car_010.roi`，每条 271 ROI。输入打包 `/tmp/nt-detector-presence-26785db.tar` 与 RK3588 `/home/neardi/Project/SOT/nt-detector-presence-26785db.tar` SHA-256 均 `39361cdada748d140eefa8077519a853ff4d25c2db032e7386062b845b39214c`。板端现有 `build/yolo_roi_replay` SHA-256 `5d8949d9907ae4b18b35205f4183c7811d6bbb19e66807a9f3126b17f51f2fd9`；同一已核验 canonical 视频和 VisDrone9 RKNN/manifest/labels，执行 `./build/yolo_roi_replay <detector_model> <detector_manifest> <detector_labels> <video> /home/neardi/Project/SOT/nt-detector-presence-26785db/<target>.roi /home/neardi/Project/SOT/nt-detector-presence-26785db/<target>.csv`，十条均 exit 0、271 帧。结果包板端 `/home/neardi/Project/SOT/nt-detector-presence-26785db-results.tar.gz`、主机 `/tmp/nt-detector-presence-26785db-results.tar.gz` SHA-256 均 `d2297da269df15520558bb3ec06cfff60c38957b894272848e95b6762ab2e3a2`。
- 以候选中心距预测中心 `<=30px` 统计：完全遮挡前 15 个可见帧，car003 仅 `2/15` 帧有置信度 `>=0.10` 的近邻候选；car010 为 `14/15`，但 `>=0.40` 仅 `6/15`。car010 完全遮挡首帧 149 仍有近邻候选（置信度 0.12），150--152 才连续未命中；car004/005/007/008 首几个完全遮挡帧也仍有候选。因此原 320×320 内容的 detector 有效未命中不能直接作为通用遮挡判定，单设 0.40 置信度门会误伤可见目标。对全部十目标做的主机离线框中心 EMA 0.85/0.70/0.55 对有效可见 IoU 没有一致提升，且会增加多数目标的跟随滞后；不采用简单平滑。
- 本次仅给只读 `yolo_roi_replay` 增加可选 `fill` 模式：保持原 3 倍目标裁剪位置，改为将 ROI 填满 640×640 输入，按实际缩放映射检测框，默认 320 内容模式字节行为不变。下一步提交精确探针源码、板端原生编译并比较十目标可见/遮挡分离度；若尺度变化仍不能提供可靠证据，不向正式 runtime 添加 detector 未命中硬门。当前候选仍为 5/10，main/Debug 与生产 handoff 未动。

## 2026-11-17T17:56:41+08:00 十目标板端复测与提前重捕评分校正

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，本轮算法起点及板端源码 `26785dbcff4ac83f0045e1cd7868fbbc0e5b634d`（本节评测器改动另随当前提交）。源码包主机 `/tmp/nt-revoke-26785db.tar`、板端 `/home/neardi/Project/SOT/nt-revoke-26785db.tar` SHA-256 均 `6a720cd6c6f46875cb5aa32e940536229cbead138a49c265fd1d5af20ef60f46`；RK3588 原生程序 `/home/neardi/Project/SOT/nt-revoke-26785db/build/nanotrack_v3` SHA-256 `d21d710fb8bf2361a03ccb97695f1b330fe95c57e1e23db328311b79a1d7f6c5`；运行配置 `runs/headless-on.conf` SHA-256 `c91bb8cf73cc5394d34cefd4f22cc66f568dc4ce5b4a98a103231a425ccc18f9`。视频、标注、tracker/detector 模型身份沿用本文件前述已核验基线；板端 Linux 6.1.84、RKNN API 2.1.0、driver 0.9.8。原生 CMake/build/CTest 23/23 PASS，三份日志 SHA-256 依次 `7d22377ffb352d1e93e50051b6d10093d4a6eef1248b4a86d220bb3c566cf019` / `c50a1950afadb27551c5f82d0ce371b713dd4a3614fab8464eb71eee2d649d98` / `ef2be349eb01801141d414a7caf9b06d207b0a9850d4e3828d872ae381`。
- 板端运行 `./build/nanotrack_v3 --config runs/headless-on.conf --bbox <各目标首帧正式真值> --output-boxes runs/ten-target-on/<target>/frames.csv --session-metadata runs/ten-target-on/<target>/session.json`，十目标各 exit 0、271 帧。脚本 `runs/focus-on.sh`（005/006/007/010）SHA-256 `2755d573e781a7744dbdaad0b91681f84d566682e0044d46c478136faa96ff09`，`runs/rest-on.sh`（其余六目标）SHA-256 `909e121cc88f8f6055dc794dae408dc7619b3e5d0c58e9f1f2a1a6679eb608b1`。原始输出在板端 `/home/neardi/Project/SOT/nt-revoke-26785db/runs/ten-target-on/<target>/`；原始包板端 `/home/neardi/Project/SOT/nt-revoke-26785db-ten.tar.gz`、主机 `/tmp/nt-revoke-26785db-ten.tar.gz` SHA-256 均 `daa8297a40c8c66382be65a2fa0152456503417a566bb38dad4ea80b30674250`。
- 使用本次评测器运行 `python3 scripts/evaluate_occ_video_1.py --dataset-root /home/u20/single-object-tracking/data/occ_video_1 --target-id <target> --boxes /tmp/nt-revoke-26785db-ten/runs/ten-target-on/<target>/frames.csv --session-metadata .../session.json --scope full --output-json .../score-v2.json --output-markdown .../score-v2.md --source-commit 26785dbcff4ac83f0045e1cd7868fbbc0e5b634d --config-sha256 c91bb8cf73cc5394d34cefd4f22cc66f568dc4ce5b4a98a103231a425ccc18f9 --model-sha256 3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`。报告在同名主机目录，汇总 `/tmp/nt-revoke-26785db-summary-v2.json` SHA-256 `68d6f21966463f27f81185f2be4e21ff2a8ebe2b574867ebf358bf6f4cbf7860`。通过：002（有效观测 mean IoU 0.895、228 帧重捕）、005（0.767、182）、006（0.804、215）、007（0.783、227）、008（0.700、258），共 **5/10**。失败：001（192 帧恢复超窗）、003（完全遮挡、144 帧错误身份及可见框）、004（完全遮挡、预测连续性及可见框）、009（完全遮挡及可见框）、010（149--157 帧完全遮挡仍发布观测）。
- 评测器仅对标注 `re_emergence_frames` 的目标开放提前重捕：重捕帧至首次完整可见帧必须全部为无框的部分遮挡；首次完整可见起连续三帧必须有合法观测/控制且 IoU 均不低于 0.70。002 标注重新露出 217--229、首次完整可见 230；228 帧重捕，230--232 帧 IoU 0.791/0.832/0.809，满足用户已确认的“越早且正确越好”。单测覆盖错误重捕框、缺少接受观测、在完全遮挡重捕均不放行；`python3 -m unittest tests.test_evaluate_occ_video_1 -q` 25/25 PASS，`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure -j4` 23/23 PASS，`git diff --check` PASS。此变更只改评分，不改板端程序。此前固定 230 起点口径的同一原始结果为 4/10，保留历史记录。
- 该候选仍有五目标失败，**不作为新基线，不合入 main/Debug，不发布 RK3566/生产 handoff**。下一步优先查 010 的高 PSR 完全遮挡漏判和 001 的 `IDENTITY_CONFLICT` 延迟，同时对 003/004/009 保持身份约束，避免为提早搜索误捕相邻车。

## 2026-11-17T17:42:01+08:00 守护后视觉探针连败撤销候选（待板端验证）

- 上一仅诊断源码 `codex/ten-target-smoothness@0638c966b28e9d1d685f9b51985bb90245cbd492`，源码包 `/tmp/nt-probe-0638c96.tar` 与 RK3588 `/home/neardi/Project/SOT/nt-probe-0638c96.tar` SHA-256 均 `82ef6801525441ba02157b8e93b7b67241fcdd79594cab5293edba37b677abf8`。RK3588 原生程序 `/home/neardi/Project/SOT/nt-probe-0638c96/build/nanotrack_v3` SHA-256 `d40398425a4a891f85a2059818ad03a5ba871c8d0d781ac591682aa80f97131a`，配置 `runs/headless-on.conf` SHA-256 `1ae6042ece06d9a057ed446611a218411e5905d8721de237552d7d5fbb7b0194`，CMake/build/CTest 与前节同命令且 23/23 PASS；日志 SHA-256 `662f16ccd04850ae8dd81b883ffe814fbf382c566d7726ad693bc205519fc196` / `3c3737006c7547760e64b03f7bee429ccba12b348187d75ba915070af058ebfa` / `6b39dc612c8742be583dfc13e8001c3c00f87525d5d47761b82ec5c0ccb4d282`。同视频/模型身份见下节。脚本 `runs/focus-on.sh` SHA-256 `aada1b76c81a19b7d4f67974b358e4abc3b00952c0d5973a1e9c55a0f12c0f90` 按首帧正式真值跑 005/006/007/010，各 exit 0、271 帧；原始日志板端 `runs/focus-on/<target>/`，主机包 `/tmp/nt-probe-0638c96-focus.tar.gz` SHA-256 `17e097b45701b610a5fdc861390b2c14fc574030c898e82d083762e692f3f500`，报告 `/tmp/nt-probe-0638c96-focus/runs/focus-on/<target>/score.{json,md}`，汇总 `/tmp/nt-probe-0638c96-summary.json` SHA-256 `445548e5101757b3ef744c57dc82537036d77eda7e59b9f6ac2e1dfe78c286f9`。005 PASS 0.742/182 帧恢复，006 FAIL 0.794/215，007 PASS 0.769/227，010 FAIL 0.866/176。与 8b3dd0d 同目标对齐时，271 帧状态、接受、重捕、Recent 原始框和 PSR 全相同；005/010 发布框全同，006/007 各只有一个发布框因异步 detector 候选细微不同。该诊断版没有改变状态结论。
- 新日志显示：006 在 197 帧视觉探针命中（距预测中心 10.95 px），198--201 连续未命中（最近距离约 27--61 px）仍 SUSPECTED，202 才 SEARCH；010 在 157--162 连续未命中仍 SUSPECTED，163 才 SEARCH。005 已于 169 SEARCH，007 于 214 SEARCH。基于此，实验仅在**本跟踪阶段曾被双 detector 命中守护成功**、随后进入 SUSPECTED 且当前帧视觉探针连续两次有效未命中时，撤销此前的保护并进入 SEARCH；没有保护历史的 004/009 沿用原不提前搜索逻辑。检测报错/缺结果不计为未命中，搜索或重捕后清除历史。主机 `cmake --build /tmp/nt-ten-target-build -j4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure -j4` 23/23 PASS；集成单测覆盖“先成功保护、两次视觉未命中才搜索”和“无保护历史不提前搜索”；日志 `/tmp/nt-guard-revoke-full-build.log` / `/tmp/nt-guard-revoke-ctest.log` SHA-256 `5d762dcff558b0cec9e576f30b72ef7ff38fa8b90d5e4a2a32790f40eb2df4f4` / `f07a1f363dddd10f684e8d3a7bb989edb606ff44523272ccc26a1e89cff13b0e`。**尚需 RK3588 005/006/007/010 与全十目标 A/B，未合入 main/Debug。**

## 2026-11-17T17:34:20+08:00 十目标尺寸保持结果与视觉探针诊断候选

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`；源码 `8b3dd0dbbee6b01bddd01d707e7b18863ece07a3` 包 `/tmp/nt-output-8b3dd0d.tar` 与 RK3588 `/home/neardi/Project/SOT/nt-output-8b3dd0d.tar` SHA-256 同为 `c84ed3a3a25fdb64b29d8e2e993fa28de9f468122f9143007587190f2967dcfd`。RK3588 原生程序 `/home/neardi/Project/SOT/nt-output-8b3dd0d/build/nanotrack_v3` SHA-256 `b1be4872999f1ac4b30e1a34b12c38109715cb5089d4b86c3a91533c07584c21`，CMake/build/CTest 与前节相同且 23/23 PASS；日志 SHA-256 `130a8c8c2c360706fd426b97aba9332d1a17d12a02189e82ac8ca4e52e553089` / `72b80582b95f8c7921ef653939429176b43d19016f9800f0bc5b3807f1e3c479` / `c57342fcde00f46f77dd5f147f40b83ff9b5b769779b3c362bff5f0cabfebd0e`。同视频/模型身份沿用下节。guard-on 运行配置 `runs/headless-on.conf` SHA-256 `012c3f2f46991784711b02d1efa69d6842e3cca16ba370fefc05f95b73407f92`；先用 `runs/focus-on.sh` 跑 004/007/009，后用 `runs/rest-on.sh` 跑其余七目标，脚本 SHA-256 `c4c28be06e28b74e47a077bf79e3c5df795658bfd2c1984b2aec548d397e325c` / `07eb322a81a9d75d6341f53fd1d0ea6da602a39a807aee61ed45c96fa317a5a7`。两组均执行相同 `./build/nanotrack_v3 --config runs/headless-on.conf --bbox <目标首帧正式真值> --output-boxes .../frames.csv --session-metadata .../session.json`，十条均 exit 0、271 帧。原始结果板端 `runs/ten-target-on/<target>/`，主机 `/tmp/nt-output-8b3dd0d-ten.tar.gz` SHA-256 `d3122cdebf9730cb4e25c73c3ba5646d8af779f475b387e2d8d4e16a0feb8be0`，报告 `/tmp/nt-output-8b3dd0d-ten/runs/ten-target-on/<target>/score.{json,md}`、汇总 `/tmp/nt-output-8b3dd0d-summary.json` SHA-256 `fb67388c8a54c4e247b8f21270f1b628fe5869d270dfdbfb726c8f13435e00ef`。
- full 严格结果（可见平均 IoU）：001 0.742 FAIL、002 0.895 FAIL、003 0.702 FAIL、004 0.649 FAIL、005 0.742 PASS、006 0.794 FAIL、007 0.769 PASS、008 0.695 PASS、009 0.697 FAIL、010 0.866 FAIL，仍仅 **3/10**；输出尺寸保持使 007 的可见框门通过，005 仍正确在 182 帧恢复，004/009 撤回空探针规则后恢复原身份路径。006 199--201 帧 SUSPECTED 未搜索，010 149--162 帧同类迟判仍在；尚不能作为新基线或合入 main/Debug。
- 为判别 guard 活跃时视觉探针究竟看到目标还是旁车，增加仅诊断字段到运行结果，并在 CSV `note` 中记录视觉探针是否命中、附近候选数量、最近距离和亮度比；不改状态逻辑、追踪框、控制或 detector 请求。主机 `cmake --build /tmp/nt-ten-target-build -j4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure -j4` 23/23 PASS，日志 `/tmp/nt-probe-telemetry-build.log` / `/tmp/nt-probe-telemetry-ctest.log` SHA-256 `61613e3706f5fbfdac85549838600f1376099d24c6ecd0b05b380f7584850f14` / `7413d1c8eaf57678b3a50c14bf84072a9b9cd3c376afa03f9c7fd3f46541d7af`。下一步 RK3588 复测 005/006/007/010，并与 8b3dd0d 逐字段比较内部结果，诊断后再决定是否有非过拟合的遮挡改法。

## 2026-11-17T17:23:15+08:00 空视觉探针提前搜索否决，保留输出尺寸保持候选

- `codex/ten-target-smoothness@eed7147714d2508d513ba530c0cab0dfd9d527a6` 源码包 `/tmp/nt-output-eed7147.tar` 与 RK3588 `/home/neardi/Project/SOT/nt-output-eed7147.tar` SHA-256 均为 `30aa1196a16a98f2a8b05c919f4010b242d098710ccad18d0c2ce70e1300d467`。RK3588 原生程序 `/home/neardi/Project/SOT/nt-output-eed7147/build/nanotrack_v3` SHA-256 `6166292c0d262ef610cd658a36f02abf48f29a02df5b1d2f39f8c6ab443fbbab`，CMake/build/CTest 同上一节且 23/23 PASS；日志 SHA-256 `b2c5ca46c5f34cf9148540f5ca3683485302a47b838a42bd925a6d4cd20df013` / `3f28e2a66ab9efa74fc52f7f3987dd4030e3c696a6e2ff940a734c88f01cfb1f` / `d70e4d555783624c3244c47e9ba0544c30f4b937dbeb23338f6ab283febab80b`。同视频/模型身份沿用下节。guard-on 运行配置 `runs/headless-on.conf` SHA-256 `3e06888b69843b6ffd74c2fa062a986643287192fcb0bc625029b952d0f53cea`，十目标脚本 `runs/ten-target-on.sh` SHA-256 `1c205c2652bd458bef352c9e70d33255dcda272541f891c99d5432adcebcab19`；按各自首帧正式真值运行 `./build/nanotrack_v3 --config runs/headless-on.conf --bbox <truth0> --output-boxes runs/ten-target-on/<target>/frames.csv --session-metadata runs/ten-target-on/<target>/session.json`，十条均 exit 0、271 帧。原始输出板端 `runs/ten-target-on/<target>/`；主机包 `/tmp/nt-output-eed7147-evidence.tar.gz` SHA-256 `b936c9693de30774f5f4c464742be0888599d5a5eb73cde20fbb4b2a19855638`、报告 `/tmp/nt-output-eed7147-evidence/runs/ten-target-on/<target>/score.{json,md}`，汇总 `/tmp/nt-output-eed7147-summary.json` SHA-256 `8c70b006aa1235694be60587e0290ba6f9f8c69c2fb67cefa32fcda0b7ab7632`。
- full 结果（可见平均 IoU）：001 0.742 FAIL、002 0.895 FAIL、003 0.702 FAIL、004 0.420 FAIL、005 0.742 PASS、006 0.794 FAIL、007 0.769 PASS、008 0.695 PASS、009 0.431 FAIL、010 0.866 FAIL，严格仅 3/10。空视觉探针规则使 004 和 009 过早进入搜索，后续分别在 150/171/236 和 129/146 帧错误重捕，身份与恢复门明显退化；**否决该规则**，不能只看 007 通过。当前修订仅移除这一条规则和对应单测，保留不改变 NanoTrack 裁剪的短期输出尺寸保持供隔离验证。主机 `cmake --build /tmp/nt-ten-target-build -j4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure -j4` 23/23 PASS，日志 `/tmp/nt-output-hold-build.log` / `/tmp/nt-output-hold-ctest.log` SHA-256 `4e83a6998e290aae42d7187a6699acf282915c15985fc4b70afbf1fc9221e128` / `c6763a30c36bcc5b73a93415ef27fe942af344482cffd0670e6fc68f6356d2c9`。**该修订尚未板端运行**；先复测 004/007/009 的身份、尺寸和门槛，再决定是否完整十目标。main/Debug 不动。

## 2026-11-17T17:12:57+08:00 输出尺寸隔离验证与负视觉证据候选（待板端验证）

- 实验源码 `codex/ten-target-smoothness@04b97870a32f62e3a1abdab1eaf85233c87aeb2a`，源码包 `/tmp/nt-output-04b9787.tar` 与 RK3588 `/home/neardi/Project/SOT/nt-output-04b9787.tar` SHA-256 均 `35b6b43220c05694ac45df4d471b9f22edc054324f6196915762e7084e8e1a87`。RK3588 原生程序 `/home/neardi/Project/SOT/nt-output-04b9787/build/nanotrack_v3` SHA-256 `a9c71fc2c032d12fa96d0ed3d3d2d39e469ba0111e63441415eec1e40074bc8e`，CMake/build/CTest 仍为下节同命令且 23/23 PASS；日志 SHA-256 `5ba5fdb8440300e3f13446d166bd9e55a0418ed8eef20f2337d0b166131f6558` / `d80dd5e0d3993f826e3a7f48a5ff8ec2a9d112f10524ae40637c10eaf134a294` / `fcc1c9548df3127b5c236e8f61c5e882cf963d72971a94bb945abac3e743f58e`。同一视频和 NanoTrack/VisDrone9 模型哈希沿用下节身份。guard-on 运行配置 `runs/headless-on.conf` SHA-256 `71ac3b33a2ac06d438f56f79479c73957bdf63b8d1ee2c146c54053e08a9b28c`，十目标脚本 `runs/ten-target-on.sh` SHA-256 `5cfab5e3901d668d3df90f547d92c8631e618903cbcc6ec3e239d6ea5a27272a`；逐目标执行 `./build/nanotrack_v3 --config runs/headless-on.conf --bbox <各自首帧正式真值> --output-boxes runs/ten-target-on/<target>/frames.csv --session-metadata runs/ten-target-on/<target>/session.json`，十条均 exit 0 且 271 帧。原始输出留在板端 `runs/ten-target-on/<target>/`，主机包 `/tmp/nt-output-04b9787-evidence.tar.gz` SHA-256 `313da6396135e794c3622b380f7e8289f44bf771cc5f00b4a47badb457cdc3f3`；报告 `/tmp/nt-output-04b9787-evidence/runs/ten-target-on/<target>/score.{json,md}`，汇总 `/tmp/nt-output-04b9787-summary.json` SHA-256 `c807321714eee20cfeaf6d31234b47c92c8cb958fba0fbb7a9bc92337e80ee4d`。
- full 严格结果：001 0.742 FAIL、002 0.895 FAIL、003 0.702 FAIL、004 0.649 FAIL、005 0.739 PASS、006 0.791 FAIL、007 0.763 FAIL、008 0.695 PASS、009 0.697 FAIL、010 0.866 FAIL（数值是可见帧平均 IoU）。005 172--175 遮挡与 182 帧恢复通过；006 在 199--201 帧仍 SUSPECTED，007 的 206--208 帧可见框 IoU 不合格，010 在 149--162 帧仍未正确搜索。**仅 2/10 严格通过，不能推广。**另以 e18067e 原生程序、相同 guard-on 配置及精确相同首帧框重跑 car005：对照 CSV `/tmp/nt-e180-car005-on-compare.csv` SHA-256 `409ad3f65cbf9742d3d7079bb1befdf02597f3a8def560e8086aa863d87075ee`，对照配置 `runs/headless-on-compare.conf` SHA-256 `bc0ebeeaf0caf809756be47d1991d35f131bd7d793209e80ba68884397ba53f9`。`/tmp/nt-output-04b9787-parity.json` SHA-256 `773a7c7b2953438f39d2b677930c8f8d87e1feec2c31fbc068c99a00b630f47d` 记录 271 帧 `recent_raw`、PSR、运动预测、search_scale、状态/接受/重捕、display 来源均零差异，而对外 tracking 宽度有 230 帧变化；证实该实验未改变 car005 的 NanoTrack 裁剪。
- 下一实验使用现有 `detector_stale_ms` 在近期确认后保留**对外发布**的尺寸，避免 detector 一次短暂缺席时向偏小 NanoTrack 框迅速回缩；不改变内部裁剪。视觉探针原先只把附近 detector **命中**解释为遮挡证据，但 car004 帧131、car009 帧116 在目标全遮挡时探针结果为空，仍错误留在跟踪；新条件要求当前帧视觉探针零原始候选且 Recent 区域亮度低于触发阈值的 75%，才立即进入 SEARCHING，避免仅凭一次 detector 漏检丢目标。car003 可见帧51 同样零候选但亮度比 0.814，不满足强变化；这只是同视频筛查，不能证明跨场景泛化。新增空检测/轻微变化单测，主机 build/CTest 23/23 PASS，日志 `/tmp/nt-output-v2-build.log` / `/tmp/nt-output-v2-ctest.log` SHA-256 `8ab85cefc461e7c2b137aa1dae9d6d33a618232df4e87566fe645f67e990adb0` / `dbbd4a5d6e96ce87dc4c0a4a0674b6acc9ae72fe964f5d4beb7fbdaaf884f7c0`。**尚需 RK3588 全十目标 A/B；未合并 main/Debug。**

## 2026-11-17T16:56:38+08:00 detector 佐证仅修正输出尺寸实验（待板端验证）

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，起点 `0a53c77562e1ad83f0e1df17c3e71edc45e5f24c`。离线使用已归档十目标 guard-off 原始 CSV 与正式真值计算：以 NanoTrack 中心和 detector 显示包络尺寸组成的框，已接受可见帧平均 IoU 在 car001/003/004/005/006/007/008/009/010 均上升，但 car002 由 0.894 降至 0.763；显示包络不能直接当算法框或控制框。这是诊断反事实，非运行时效果。
- 本实验只在原有 SUSPECTED guard 连续两次匹配同类 detector 候选且控制器独立接受 NanoTrack Recent 时，更新**对外发布**的 tracking/control 尺寸；中心取 NanoTrack，每维单帧变化上限为 `min(10%, max_accepted_scale_step_ratio)`，detector 不缩小当前 NanoTrack 尺寸。控制器内部状态、下一帧 NanoTrack 裁剪、模板提交和视觉遮挡亮度历史都先使用原始框，发布尺寸在这些步骤完成后才修改；日志 `note=DETECTOR_VERIFIED_OUTPUT_SIZE` 可追踪应用帧。默认 guard 仍关闭，未接受或非 Recent 的输出不由 detector 授权。主机 `cmake --build /tmp/nt-ten-target-build -j4` 与 `ctest --test-dir /tmp/nt-ten-target-build --output-on-failure -j4` 23/23 PASS，日志 `/tmp/nt-output-fusion-build.log` / `/tmp/nt-output-fusion-ctest.log` SHA-256 `4308d7cf5d8983c20fcd206187516e875f6495e91fe550e8c118567a8ff322b9` / `b79117fe9bcef19080081fe06df89ab3b0bdec85156bd743922a8da2be787cc6`。单测核对输出中心和内部/已提交 NanoTrack 框分离、误类不触发。**尚无板端证据**；下一步同视频/模型/初始框完整十目标回放，并核查遮挡迟判与正确重捕是否保持。不得据主机单测推广或合入 main/Debug。

## 2026-11-17T16:49:05+08:00 亮度口径复测仍退化；撤回 detector 尺寸修正

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`。亮度口径修正源码 `451906ec5363a834c325541eec889dae9212d97c`；源码包 `/tmp/nt-size-451906e.tar` 与 RK3588 `/home/neardi/Project/SOT/nt-size-451906e.tar` SHA-256 均 `8b9aad60fd7507b6c3e79f35d976dafbada6810a51d630358ea933fb86b8f16c`。板端程序 `/home/neardi/Project/SOT/nt-size-451906e/build/nanotrack_v3` SHA-256 `e4e64c9e4c557463fa0f70ea19fba1b4a3a1694472fb0463db2e770a1e6d3d96`；运行配置 `runs/headless-on.conf` SHA-256 `e29a40c83601af12306311be3ba3e17184a939c91eba7d8449433e9697a58108`。原生 CMake/build/CTest 采用上一节相同命令且 23/23 PASS；日志 SHA-256 `358245ab57155affc128da238778b0c85dad838b0788c980439cd7093481d982` / `0baaa083744bdd0513fcb50d58ba1b800d80b5665ea480b0cb2df2eda7db346f` / `a3d575d17a6d0a105af853b073ce1e9acd784a43342386cebefa1b19617829a3`。同一视频与模型身份见下节。
- 实际执行板端脚本 `runs/focus-on.sh` SHA-256 `15a6f639baab3c70953b4693240fb47c75971dff3d4d3ab42fb960943ff454e4`，对 car005/007 用同一首帧真值初始化，各 271 帧 exit 0；原始结果在 `runs/focus-on/<target>/`，主机包 `/tmp/nt-size-451906e-focus.tar.gz` SHA-256 `ec2bc03f37e1c864eb7cbdf01cb0c11db2a1638e20f256dbab97a3b2fe10e62b`，报告 `/tmp/nt-size-451906e-focus/runs/focus-on/<target>/score.{json,md}`。full 评分 car005 可见 IoU 0.794 FAIL（172--173 遮挡未及时搜索、187 恢复），car007 0.817 FAIL（217--221 遮挡误留跟踪、235 恢复），与首版尺寸修正相同。**否决 detector 尺寸修正路线**：修正虽然改善可见框，但改变后续 NanoTrack 裁剪与遮挡状态，不能仅凭可见 IoU 推广。本次将这两版实验的四个算法/单测文件恢复到已跑十目标安全候选 `a1d056d69cfec3380c5819cef620bf295ad00631` 的字节内容，保留研究提交和证据；仍不得合入 main/Debug。下一步研究不改变跟踪搜索裁剪的 detector 可见性佐证，且需对十目标同时验证无遮挡保留与遮挡及时退出。

## 2026-11-17T16:45:07+08:00 尺寸佐证首轮板端 A/B 与亮度取样口径修正

- `codex/ten-target-smoothness@bb5d86215b768213d28744d83989dfe51f2aac7e` 源码包 `/tmp/nt-size-bb5d862.tar` 与 RK3588 `/home/neardi/Project/SOT/nt-size-bb5d862.tar` SHA-256 同为 `b9de885111daaae7415399a017be51f813789fb2d0be122be25b5a504e884b3b`。板端原生构建程序 `/home/neardi/Project/SOT/nt-size-bb5d862/build/nanotrack_v3` SHA-256 `dbcc431fc1edfa38ab47b839135ba7216bab36c5dab0a9e183354fe947e83493`；`cmake -S rk3566/nanotrack_v3 -B build -DENABLE_RKNN_RUNTIME=ON -DBUILD_TESTING=ON -DRKNN_INCLUDE_DIR=/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/include -DRKNN_LIBRARY=/usr/lib/librknnrt.so`、`cmake --build build -j4`、`ctest --test-dir build --output-on-failure -j4`，23/23 PASS；日志 SHA-256 `2dd6424b79e16d2ca4fb059e970d59da6f53a2ec5aa3f1e0ac7f46a6b8b9f71a` / `ef6d1abcf544655b0e0873a31f5dbaa9161d18e8bb5e091d760d3deb08dd1b76` / `67362c4c2c00cfc4cf09f4e1f7e6d6d030c2f144622356b9cb0608fd29910737`。同一视频、NanoTrack/VisDrone9 模型哈希沿用下节完整身份；运行配置 `runs/headless-on.conf` SHA-256 `6a9abd6ea919aa9e8ef179324e821b05a8253bfa3eb6b3ffb395b00cddd96262`，回放脚本 `runs/ten-target-on.sh` SHA-256 `ce78bc05a0612ce27ce5a65b48fe1bfe7b655bb8e16057c492c270602f574396`。逐目标执行 `./build/nanotrack_v3 --config runs/headless-on.conf --bbox <首帧正式真值> --output-boxes runs/ten-target-on/<target>/frames.csv --session-metadata runs/ten-target-on/<target>/session.json`，十目标均 exit 0、271 帧。原始日志留板端，主机包 `/tmp/nt-size-bb5d862-evidence.tar.gz` SHA-256 `da18d4f73724e8ca6f9c72b4125192ba45e19d948876d858a4433f0137062c6f`，报告 `/tmp/nt-size-bb5d862-evidence/runs/ten-target-on/<target>/score.{json,md}`，汇总 `/tmp/nt-size-bb5d862-summary.json` SHA-256 `952a3cceed5539c693d7631b3178a317e9de4b97c74ac4a132185f50bb1aa5c5`。
- full 评测结果：001 0.742 FAIL，002 0.895 FAIL，003 0.702 FAIL，004 0.649 FAIL，005 0.794 FAIL，006 0.849 PASS，007 0.817 FAIL，008 0.695 PASS，009 0.697 FAIL，010 0.862 FAIL（数字为可见帧平均 IoU）。对照 guard-off，尺寸提示显著改善 005/006/007/010 的可见框；但 005/007 在完全遮挡阶段迟判并延后恢复，严格通过仅 2/10，不能推广。逐帧查到尺寸提示扩大了被接受跟踪框，而视觉遮挡亮度历史取自该扩大框、当前比较取自 NanoTrack 原始 Recent 框，面积口径不一致。后续修订改为亮度历史也取原始 Recent 框；主机重构建和 CTest 23/23 PASS，尚须在板端复测 005/007 并回归十目标。本轮的尺寸提示仅限实验 guard-on，既有 guard-off 算法路径未改变，未合并 main/Debug。

## 2026-11-17T16:33:08+08:00 detector 佐证后的有限尺寸修正候选（待板端验证）

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，基线 `a1d056d69cfec3380c5819cef620bf295ad00631`。研究旧 detector guard 在 120--141 帧可阻止可见目标进入 SEARCH，但十目标 A/B 中 car006/007 由 PASS 变 FAIL；逐帧查到保护后跟踪中心大致正确，跟踪框尺寸约 30--35 px，而 guard-off 后正确重捕的尺寸约 40--46 px。先前对单纯缩短 SUSPECTED 时序的十目标 A/B 已否决，详见下节。
- 本候选保留 guard 默认关闭。仅在同类别、唯一、近期 detector 两次确认且 NanoTrack Recent 被控制器独立接受时，将该 detector 框作为**尺寸**提示：每维向 detector 尺寸移动一半，单帧最多变化 10%；中心仍取 NanoTrack，未接受观测、误类、歧义和过期框不触发。修改 `long_term_controller.hpp/.cpp`、`long_term_runtime.cpp` 与运行时单测；`cmake --build /tmp/nt-ten-target-build -j4` exit 0，`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure -j4` 23/23 PASS。此处仅是主机行为验证，尚未证明板端真值收益；下一步在 RK3588 隔离目录原生构建，对十目标首帧真值初始化做 guard-on/guard-off 对比。不要合入 main/Debug 或作为新基线，直至 A/B 无有害退化。

## 2026-11-17T16:18:14+08:00 十目标安全候选完成板端回归；快速时序 A/B 否决

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，算法源码提交 `e18067ea641bb0699c945a2ec2e74b7ec1dae6a1`；源码包 `/tmp/nt-ten-target-e18067e.tar` 与 RK3588 `/home/neardi/Project/SOT/ten-target-e18067e/source.tar` SHA-256 均为 `17ac2337009220d68b96620c6789a807371754dbf8a1b547bb624fd16db420db`。板端 Linux 6.1.84、RKNN API 2.1.0、driver 0.9.8，原生二进制 `/home/neardi/Project/SOT/ten-target-e18067e/build/nanotrack_v3` SHA-256 `913fd2e899b60f6d01b12b7bdf6d35e5a8bb2385ec2126c229420927272e40f9`。实际执行 `cmake -S rk3566/nanotrack_v3 -B build -DENABLE_RKNN_RUNTIME=ON -DBUILD_TESTING=ON -DRKNN_INCLUDE_DIR=/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/include -DRKNN_LIBRARY=/usr/lib/librknnrt.so`、`cmake --build build -j4`、`ctest --test-dir build --output-on-failure -j4`，23/23 PASS；板端 cmake/build/CTest 日志 SHA-256 `6951b4323ca49759827927ecd368097196ac80f4e2ec0be8c88f602fb0b5ef32` / `7c919f3f6551cb3afcbb8ec4cb8c86bee6ac2b82bd2323c1351cc91d0bec14b0` / `82d559fcc112786db2c1b54644967aa4a98ebbe5db2512e91b91cf1420141b3f`。
- 板端视频 `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；NanoTrack manifest SHA-256 `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`，template/search/head RKNN 为 `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55` / `d3cb0e3879df6a1272adb59bf501324f1f8f55b58309d3c4da4a731b600973ee` / `4da379f54093d79f02775e90bfea33ebee0837c6fc73a324499279b4c57807c5`；实际加载的 VisDrone9 detector RKNN/manifest/labels 为 `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c` / `ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9` / `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`。主运行配置 `runs/headless-guard-off.conf` SHA-256 `9df02c498a22827dcf08224f568b7108da080fc5211e9ab52eaafcb50582217c`，控制参数 `runs/guard-off.conf` SHA-256 `2113697ec9c91a23bbd6726438523c1a45d6b15e0dde3e27ea2429f4d540f9c9`；仅关闭尚未泛化通过的 SUSPECTED detector guard，其余沿用上节视觉双真值配置。
- 对 `car_001` 至 `car_010` 逐一执行 `./build/nanotrack_v3 --config runs/headless-guard-off.conf --bbox <该目标首帧正式真值框> --output-boxes runs/ten-target-off-20261117/<target>/frames.csv --session-metadata runs/ten-target-off-20261117/<target>/session.json`，全部 exit 0、271 帧。原始 CSV/session/log 留在板端上述目录；主机归档 `/tmp/nt-e180-ten-off.tar.gz` SHA-256 `52a0f0d50bcaaa3b93d9b8efa8db8cab3382c442d020cf00bc932ea23a115796`。主机各目标 full 评测报告在 `/tmp/nt-e180-evidence/runs/ten-target-off-20261117/<target>/score.{json,md}`；小结 `/tmp/nt-e180-ten-off-summary.json` SHA-256 `2bc8ef39419db34aa6249367a41d5c5396282ee0d712406ded519061576c436f`、四类文件逐目标哈希清单 `/tmp/nt-e180-ten-off-manifest.sha256` SHA-256 `c51ad0b8979cf7b3a940e35980590b7991ce1d8340cc1455c1ffdb425339a546`，`sha256sum -c` 40/40 OK。
- full 结果（可见帧平均 IoU）：`001` 0.742 FAIL（192 帧重捕晚于固定 190 截止）；`002` 0.894 FAIL（228 帧早于固定 230 起点，此提前重捕已由用户单独认可）；`003` 0.702 FAIL（144--151 帧恢复框偏上、130 帧未进入搜索）；`004` 0.649 FAIL（42、131--132 帧未进入搜索、搜索转场速度门、127--129 帧框偏差），但 156 帧正确恢复且 241 帧出画后不再误重捕；`005` 0.743 FAIL（172--173 帧未进入搜索）；`006` 0.806 PASS；`007` 0.745 PASS；`008` 0.695 PASS；`009` 0.697 FAIL（115--120 帧未进入搜索、末段四个框偏差）；`010` 0.818 FAIL（149--162 帧仍未搜索）。八条新目标已接受可见帧的中心误差均值按目标等权平均 2.14 px，不能以此掩盖遮挡或身份错误。相较上一候选，`003/004` 出画后错误重捕均消失；`004` 正确恢复的提升保留。
- 对同一二进制另做通用时序 A/B：从 `runs/guard-off.conf` 仅将 `suspicion_enter_ms 100→50`、`uncertain_ms 200→100`，得到 `runs/occlusion-fast.conf` SHA-256 `1962f25613152cedefd18358c9c17057f15024af7fb148216ce6715306697a6d`，运行配置 `runs/headless-occlusion-fast.conf` SHA-256 `2ee838f8bb80cafae00df0f987f023cc8b51cb38554bd770eadd8ecf037ed029`。十目标原始输出板端 `runs/ten-target-fast-20261117/<target>/`，主机归档 `/tmp/nt-e180-ten-fast.tar.gz` SHA-256 `5749d73bccbb7f1ce912b91e6b8730df9171e83535397184bdf886b38ecfd843`，报告在 `/tmp/nt-fast-evidence/runs/ten-target-fast-20261117/<target>/score.{json,md}`。`005` 变 PASS、`006/007/008` 仍 PASS，但 `003` IoU 0.702→0.587、`004` 0.649→0.424 且错认目标、恢复失败；**明确否决快速时序配置**，保留原 100/200 ms。
- 多事件评测器补充当前日志 schema 14 兼容，并把合法恢复确认命中数接受范围由固定 2 修为 2--5，保留模板事务、控制来源、身份与真值 IoU 的其余校验；正反例 `python3 -m unittest tests.test_evaluate_occ_video_1 -q` 24/24 PASS。这只是修正日志协议误判，不修改真值或放宽可见框门槛。**Handoff**：此分支仍为研究候选，未合并 main/Debug；下一轮需用多序列证据研究无目标时高 PSR 导致的迟判，以及验证 detector 尺寸信息能否只在 NanoTrack 确认后安全修正跟踪尺寸。正式 RK3566/生产 handoff 仍未批准，也无真实吊舱验收。


## 2026-11-17T15:57:57+08:00 边缘离画风险修订候选

- 上一候选 `codex/ten-target-smoothness@e6660ea2903e2f988364cde031b181e5cab3e84c` 源码包 `/tmp/nt-ten-target-e6660ea.tar` 与 RK3588 `/home/neardi/Project/SOT/ten-target-e6660ea/source.tar` SHA-256 同为 `6ff4ac0cef3f9c8b10ec4125e44730f9c481bae6df6c0b983bbdee850fec2419`。该隔离目录原生构建程序 SHA-256 `6194a99a813f890e776f4e706c8c227e0994f0d4c5675bb3939b4a6d44f48dda`，CTest 23/23 PASS；板端 cmake/build/CTest 日志 SHA-256 分别为 `5fb09a57de7a2cb1b5cfd3e0145127698c66eff362afd4ba1d7e37e4b4cf8b29` / `e75d8e32cdb066c263846d1c5f11b4a798d900042577b683512d649aefa0a7cc` / `d921bf2a5dc79185471f5cfed93db1c21d2fc74f3142a44221f2e545b7c622b8`。模型、视频和运行时身份见下节，同机不变。
- 对 `car_004` 使用首帧真值框 `865.609,680.680,39.341,45.069` 分别以 guard 开关配置运行，输出在该板端目录 `runs/car004-on/` 和 `runs/car004-off/`，主机包 `/tmp/nt-e666-car004.tar.gz` SHA-256 `3613ac0b931733d620f6098eba01ba23bf32438fa1caf56ef8f306960097873e`。两者结果相同：第 156 帧恢复正确目标，可见平均 IoU 由上节旧程序 0.420 提高到 0.649；但目标贴下边缘于 241 帧离画后，在 262 帧错误重捕画面内另一辆车，评测 `out_of_view_semantics` 和 `reacquisition_claims` 均 FAIL。此版本不得推广。
- 本修订在通用恢复确认中增加边缘离画方向门：最后可信框及当前运动预测都已贴同一图像边缘时，若新候选中心大幅反向跳回画面内，则不能把它当作同一目标的恢复；不影响非边缘处的三路恢复，也不把 detector 直接用于控制。加入远离画边界的负例到控制器单测；执行 `cmake --build /tmp/nt-ten-target-build -j4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure -j4`，主机 23/23 PASS。**尚未板端运行本修订**；板端上一候选的十目标 guard-off 全量回放仍在执行，需先完成并归档，再部署本修订进行同协议复测。正式生产 handoff 继续为未批准。


## 2026-11-17T15:48:51+08:00 十目标真值首轮与远距离三路恢复候选

- 分支/工作树 `codex/ten-target-smoothness` / `/home/u20/.codex/worktrees/suspected-detector-guard/single-object-tracking`，起点为已提交实验 `codex/suspected-detector-guard@57ee1acb843196d2368ccec2f2ea7d35c14578c5`；八条新增正式真值来自 `codex/occ-eight-target-ground-truth@acc855d0b45c9d1449e9e25b9e08573be1698c5c`，数据在 `/home/u20/single-object-tracking/data/occ_video_1/ground_truth/car_003` 至 `car_010`，各自 `labels.sha256` 和 `checksums.sha256` 校验均退出 0。视频仍为 SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`。本分支接入八目标多事件评测，并允许当前日志 schema 14 与已有 schema 12 同协议评测；`python3 -m unittest tests.test_evaluate_occ_video_1 -q` 为 24/24 PASS。
- 用 RK3588 `/home/neardi/Project/SOT/suspected-detector-guard-39b0d83/build/nanotrack_v3`（源码 `39b0d835e189c2a36b2b9068f4066173a024c47d`，程序 SHA-256 `336a7a46c480bad63ca484a7830f25d592e2422271542704c93cef586cd56464`）对十目标以各自首帧正式真值框初始化，命令为 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox <first_truth_bbox> --output-boxes runs/ten-target-gt-init-20261117/<target>/frames.csv --session-metadata runs/ten-target-gt-init-20261117/<target>/session.json`。均 exit 0 且含 271 帧；输出板端目录如上，主机归档 `/tmp/nt-ten-target-all.tar.gz` SHA-256 `519965389c0e6e976fa10ead042b360883e455a16a4cec25490784c081ff9f03`，主机解包及 full 评分在 `/tmp/nt-ten-target-evidence/runs/ten-target-gt-init-20261117/<target>/`。配置 SHA-256 `62607f7eb3798737fc927a1f997e0d52cc5f78a7250d8df8b23aebfb320d5f44`；模型/标签及 Linux、RKNN API/driver 身份见下节同一可执行文件记录。
- 开 guard 的八目标 full 评分：`car_003` 0.702 FAIL、`004` 0.420 FAIL、`005` 0.704 PASS、`006` 0.723 FAIL、`007` 0.665 FAIL、`008` 0.695 PASS、`009` 0.697 FAIL、`010` 0.854 FAIL（数字是全部可见帧平均 IoU）。两个旧目标以首帧真值初始化为 `001` 0.742 FAIL、`002` 0.894 FAIL；其中 `001` 192 帧重捕晚于固定 190 帧期限，`002` 228 帧重捕早于固定 230 帧下限，后者此前已获人工认可，但全量评分原样保留。
- 相同程序、首帧框和视频，改用关闭 guard 的 `runs/headless-guard-off.conf` 对新八目标 A/B：`003` 0.702 FAIL、`004` 0.420 FAIL、`005` 0.743 FAIL、`006` 0.806 PASS、`007` 0.745 PASS、`008` 0.695 PASS、`009` 0.697 FAIL、`010` 0.818 FAIL。配置 `runs/guard-off.conf` SHA-256 `2113697ec9c91a23bbd6726438523c1a45d6b15e0dde3e27ea2429f4d540f9c9`；板端输出 `runs/eight-target-guard-off-20261117/<target>/`，主机归档 `/tmp/nt-eight-guard-off-all.tar.gz` SHA-256 `732d2b54be1bb9bb1dec73f32a22e6f62babd0057f795fe3ec32bb68923b965c`。guard 在 `006/007` 有明显负效应，本候选不得作为新基线直接推广。
- `car_004` 于 154 帧后有与真值接近的 detector 提议及 Recent/Safe 两路高质量同框，三次验证满额仍卡在 `AWAITING_CONFIRMATION`；遮挡期间旧运动中心已偏离，双模板运动中心门拒绝恢复。本候选仅在**唯一 detector 候选、三次 NanoTrack 验证、连续 Recent/Safe 同意**时允许越过该旧中心门；detector 不能直接授权控制。新增远距离恢复单测；主机执行 `cmake -S rk3566/nanotrack_v3 -B /tmp/nt-ten-target-build -DENABLE_RKNN_RUNTIME=OFF -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release`、`cmake --build /tmp/nt-ten-target-build -j4`、`ctest --test-dir /tmp/nt-ten-target-build --output-on-failure -j4`，23/23 PASS，构建/CTest 日志 SHA-256 `77e62278ad13a9e7372b336eb5140615e5d9011abd9c4dd45206b8c878558890` / `7e9b0ba0254029b580736bcb2c22ef5d56738aece7bac1af854e2643029ac93c`。**尚未板端运行本候选**；下一步在隔离板端目录原生构建并重跑十目标，核查新条件是否实际触发及是否导致错误目标。正式生产 handoff 未批准。


## 2026-11-17T13:24:45+08:00 SUSPECTED detector 佐证：RK3588 原生 A/B

- 实验分支/工作树 `codex/suspected-detector-guard` / `/home/u20/single-object-tracking/.worktrees/suspected-detector-guard`；板端使用已提交源码 `39b0d835e189c2a36b2b9068f4066173a024c47d`，从 `Debug@3f6d9e3` 起步。源码包 `/tmp/nt-guard-source-39b0d83.tar` 与板端 `/home/neardi/Project/SOT/nt-guard-source-39b0d83.tar` SHA-256 同为 `5bae8bbd894c75d4f7f1c807cb6c6546f6339291f49675439db744f3d326c89c`；独立板端根 `/home/neardi/Project/SOT/suspected-detector-guard-39b0d83`，原生构建 `build/nanotrack_v3` SHA-256 `336a7a46c480bad63ca484a7830f25d592e2422271542704c93cef586cd56464`。命令：`cmake -S rk3566/nanotrack_v3 -B build -DENABLE_RKNN_RUNTIME=ON -DBUILD_TESTING=ON -DRKNN_INCLUDE_DIR=/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/include -DRKNN_LIBRARY=/usr/lib/librknnrt.so`、`cmake --build build -j4`、`ctest --test-dir build --output-on-failure -j4`；23/23 PASS。板端 Linux `6.1.84`、RKNN API `2.1.0`、driver `0.9.8`。
- 输入原视频 `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；NanoTrack manifest `models/manifest.json` SHA-256 `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`，head/search/template RKNN 依次为 `4da379f54093d79f02775e90bfea33ebee0837c6fc73a324499279b4c57807c5` / `d3cb0e3879df6a1272adb59bf501324f1f8f55b58309d3c4da4a731b600973ee` / `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55`；VisDrone9 detector RKNN/manifest/labels 为 `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c` / `ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9` / `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`。
- A/B 使用同一可执行文件；开配置 `rk3566/nanotrack_v3/config/visible-dual-gt-visual-gate.conf` SHA-256 `62607f7eb3798737fc927a1f997e0d52cc5f78a7250d8df8b23aebfb320d5f44`，关配置 `runs/guard-off.conf` SHA-256 `2113697ec9c91a23bbd6726438523c1a45d6b15e0dde3e27ea2429f4d540f9c9`。实际对每组运行 `./build/nanotrack_v3 --config runs/headless-{guard-off|eval}.conf --bbox <初始框> --output-video runs/<目标>-{off|on}/output.avi --output-boxes runs/<目标>-{off|on}/frames.csv --session-metadata runs/<目标>-{off|on}/session.json`，标准输出/错误写同目录 `run.log`；手点黄车框 `1032.833740,405.310425,36,36`、car001 `993.645630,581.063293,36,39.486084`、car002 `972.080505,518.201843,36,36`。每组 271 帧，视频 1920×1080、30 FPS；大视频只留板端。
- 手点黄车：关闭时 120～140 帧 SUSPECTED、141 帧 SEARCHING、166 帧重捕后又丢；开启时 121/124/127 帧提交 SUSPECTED_VALIDATION、128 帧在两次匹配检测后恢复 TRACKING，141 帧仍 TRACKING。原画面与输出框抽帧确认 155/176/185/200/228/260 仍指向原黄车；真实桥遮挡时 156～162 帧 SUSPECTED、163～175 帧 SEARCHING、176 帧重捕。主机 PT 对应 120～141 帧 detector 候选的中心代理误差均值，关闭/开启为 3.04/1.02 px；黄车无逐帧真值，此值只检验相互一致性。板端 `manual-off/on` CSV SHA-256 `6146425c676607c4ae88d57e7a2c63c649c00aea65ca4849aea8384db1354342` / `9baacaf87dae3dd17283f42ada317c92c4e5baa4c1f9780130061c20d8e5dec4`，AVI SHA-256 `5db627657ac06ca1340e81645dbe1fb0069b3c2d8fe8be09cd56eb48bd988154` / `a264a54b71c2d9e864e81419f0e946431924f2909c97c1aeca8afb90301ed302`，板端端到端 8.80/8.56 FPS，后者 detector 验证请求/结果各 9 次、受保护接受 18 帧。
- 有真值两车：`python3 scripts/evaluate_occ_video_1.py --dataset-root data/occ_video_1 --target-id car_001|car_002 --boxes <对应 frames.csv> --session-metadata <对应 session.json> --scope full --output-json <对应 report.json> --output-markdown <对应 report.md> --source-commit 39b0d835e189c2a36b2b9068f4066173a024c47d --config-sha256 <开/关配置 SHA-256> --model-sha256 3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`。car001 开/关均 PASS、186 帧重捕、可观测帧均 IoU `0.760854`；car002 开/关均在现有 228 帧早捕门 FAIL（用户已接受早捕），其余门通过、可观测帧均 IoU `0.752668`。两目标各自开/关 AVI SHA-256 分别完全相同：car001 `3160e4f2563c8d9e4bec0f35270240108d923cf742340dd53414fd0f732d1dcf`，car002 `3ffc05744176239285f7b8591e401111e6650df7eea800d20cf6ed84e5a21d62`。CSV SHA-256：car001 关/开 `9fdf4e42f572bf40ecb639072e536728c941d001d39fcaac1100f7dc7dcecf0a` / `d46399e24a82a6b6cfe09c60235ca5d37501a92f3865fd87934a1eb71642cd41`；car002 关/开 `bed1c32e8d4141e03ea377e0f703e46e23b36e1fd45f6248ad116e74eced4377` / `169ad297345590b955b005c81dd7bb1400fd55522f8565c705c64904269f0b8e`。CSV/session/log 回传包 `/tmp/nt-suspected-guard-evidence.tgz` SHA-256 `ee16ee3ca8171be2b5f6eac22bb53d4e1144e7de3baf2bc2c2bc6e46ffc95a7b`，解压至 `/tmp/nt-suspected-guard-evidence/runs/`；板端原包留 `runs/evidence-csv.tgz`。
- **Handoff / 边界**：这版在原手点可见目标上避免 141 帧误入搜索，两辆有真值车的输出未变，故作为独立实验版保留；不合入 main/Debug，也不更新生产 handoff。黄车没有人工真值，单次板端回放不能证明跨目标、人类目标或吊舱控制质量。下一步增加不同车/人的可见目标及遮挡场景复测，检查检测缺失时的退化行为，再决定是否集成。

## 2026-11-17T13:09:51+08:00 SUSPECTED detector 佐证实验版：主机阶段

- 分支/工作树 `codex/suspected-detector-guard` / `/home/u20/single-object-tracking/.worktrees/suspected-detector-guard`，起点 `Debug@3f6d9e36933a1802da83fd6ce6c24cee77492c34`，工作区初始干净；本条与实验代码同提交，板端结果另记。只在该分支的 `visible-dual-gt-visual-gate.conf` 启用新门，未改 main/Debug 基线。
- 输入手测记录 `/tmp/nt-3c8-manual-frames.csv` SHA-256 `cf3dfa3e2bebd4ed62fd0b2d8f9d896fc65bf101c0a89a1b1a47010eb69c8d23`；同视频主机 PT detector 探针 `/tmp/nt-3c8-detector-probe-118-141.json` SHA-256 `09e9460bc62ce15d04fa0df3a7b19c64653380ba6d23191d3ee4ad2ad49d7df9`。原手测 120～141 帧无 detector 请求，主机 PT 在 120～140 帧全部找到与 Recent 原始框中心相距最多 3.2 px 的目标候选。该手点目标没有逐帧真值，主机 PT 不能代替板端 RKNN。
- 实验实现：SUSPECTED 时异步检测；连续两次新鲜、同类别、唯一且与 Recent 重合的检测仅佐证可靠 NanoTrack 观测，允许其越过相对响应下降门，并维持运动更新；候选框不直接作为控制框。类别不符、位置不符、重复候选、过期结果或无跟踪观测均不得触发。保留现有搜索、遮挡和恢复保护。请求/返回用途记为 `SUSPECTED_VALIDATION`，使用现有运动门原因记 `DETECTOR_SUPPORTED_RECENT` 或 `DETECTOR_SUPPORTED_REANCHOR`。
- 实际执行 `cmake -S rk3566/nanotrack_v3 -B /tmp/nt-suspected-guard-build -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF -DCMAKE_BUILD_TYPE=Release`、`cmake --build /tmp/nt-suspected-guard-build -j4`、`ctest --test-dir /tmp/nt-suspected-guard-build --output-on-failure -j4`：23/23 PASS；`test_long_term_runtime` SHA-256 `7f9c4899fc7e765c3e29d5201682858840d3768c541743d3e26027d7b844e7e0`。测试包含连续匹配、错误类别/位置、歧义、过期及无跟踪观测。下一步在 RK3588 隔离目录原生编译并对原手点目标和两真值目标闭环回归，核对源码、二进制、模型及输出哈希；此阶段不是板端或吊舱验收。

## 2026-11-16T00:20:17+08:00 去顶部纹理门集成至 main/Debug

- 根据用户对 car002 第 228 帧提前重捕的明确认可，将任务分支 `codex/remove-recovery-texture-veto@a084819789b2cabb8ba461dc2add2693491a09ee` 的通用算法与本任务进度提交集成：`main` 工作树 `/home/u20/single-object-tracking/.worktrees/occlusion-main-integration` 从 `17ef93287232656667d6d1099ca5cadb3a1b3d21` 依次 cherry-pick 至 `1774bcd`；`Debug` 工作树 `/home/u20/.codex/worktrees/integrate-adaptive-debug/single-object-tracking` 从 `3d7f579d3ec7bc0be830865615dc0132ca1a673b` fast-forward 至 `a084819`。两边通用 controller/config/runtime/tracking log、对应单测及 `scripts tests` 的 `git diff --quiet` exit 0；`git diff --name-only main Debug -- rk3566/nanotrack_v3 scripts tests` 仅有 CLI、交互控制/覆盖层、主入口、运行配置与三份交互测试共 10 个 Debug 专属文件。
- 两分支分别执行 `cmake -S rk3566/nanotrack_v3 -B /tmp/nt-no-texture-<main|debug>-merge-build -DENABLE_RKNN_RUNTIME=OFF -DBUILD_TESTING=ON`、`cmake --build ... --parallel 4`、`ctest --test-dir ... --output-on-failure`，各 `23/23 PASS`。main cmake/build/CTest 日志 SHA-256 `9b45bd059accc02f7eeea8cc5712f39c9d50e9527ff3e3f1ce3a44d7d616bdbe` / `02254eca100a90ac8f2e1fe9ca59e946eed49fdb04172ea1f1df097eab6bcac0` / `6f6411960b66936093edbaf838a6afb5e6a161fcf45d9f6d1e852b3fecd12024`；Debug 对应 `80b8eb12811082da1f7f3c161aef6f02980a914b165a96e996f7b61f6e6fabd6` / `dfe57195daf13b8f64b55315be3ea807d2101fe031ced1d2947a01f3fbeee452` / `bbaf263529357313a9e3a1df77717443dcbe541ffa46d911d819aced6f76b1bc`。RK3588 实测仍绑定本文件下文源码 `fd6c29df`、程序 SHA-256 `fde50985d78e8be9fc062641de386a0c68d0a122f4b268bb2ea3fe496a0e4fe2`；后续 Git 提交只增进度文档，算法源码一致。固定 full evaluator 对 car002 的时间窗仍报告 FAIL，用户人工认可覆盖此单点时间门；不能据此声称真值被修改、第三辆车稳定恢复或正式生产 handoff。
- **Handoff**：请协调方依据本次人工接受记录决定是否把“部分遮挡且目标身份连续时可在首个可靠真值框之前重捕”写入下一版评测规则；保留原真值与报告哈希，不能单靠移动 `first_reliable_reappearance_frame` 来制造通过。正式 `latest_handoff.available` 仍为 `false`。

## 2026-11-16T00:18:35+08:00 car002 第 228 帧提前重捕获获人工认可

- 用户明确确认“car002 在 228 帧重捕没问题，重捕获发生得早也好，越早越好”，覆盖本文件上节因评测脚本固定 `first_reliable_reappearance_frame=230` 而暂缓合并的判断。冻结真值仍保留原样：car002 第 228/229 帧为 `manual_confirmed_no_bbox`、可见比例 `0.86/0.93`，不能把这两帧算作可观测框 IoU；现行 full evaluator 仍会显示 `reacquisition` 时间窗 FAIL，该结果与用户本次人工接受结论并列保存，不修改历史报告或真值。
- 对板端 `fd6c29df`、CSV SHA-256 `130222ddb5a551861da34c3a9451d2303b2114c2bc163f05d3607e21702b078e` 的第 228 帧再次核对：`TRACKING`、`accepted=1`、`reacquired=1`，框 `(843.082,700.505,47.830,47.830)`；第 230～233 帧对可靠真值的 IoU `0.7545/0.7791/0.8025/0.8223`，中心误差 `5.097/4.043/3.693/1.860px`，未见接到邻车框。手点第三辆车无真值且后续仍丢失，此项不变。用户认可的是第 228 帧提前重捕，不等于全部场景已验收或正式生产 handoff。下一步将去除顶部纹理门的通用算法同步合入 main/Debug，并核查两分支算法一致；评测时间窗与人工接受的差异留待协调方更新规则。

## 2026-11-16T00:11:45+08:00 移除顶部纹理门：板端回放及回归裁决

- 任务分支/工作树 `codex/remove-recovery-texture-veto` / `/home/u20/single-object-tracking/.worktrees/remove-recovery-texture-veto`，源码提交 `fd6c29df0655ef221ced0e623326043dbe37e02b`，基线 `Debug@3d7f579d3ec7bc0be830865615dc0132ca1a673b`。源码包 `/tmp/nt-no-texture-fd6c29d-source.tar` 与板端 `/home/neardi/Project/SOT/occlusion-no-texture-fd6c29d/source.tar` SHA-256 `f589216c881b8e23db309516a246473b49488c841543944dd77c4c807ef5bc08`；板端原生构建目录及全部输出均在上述独立目录。实际执行 `cmake`、`cmake --build`、`ctest`，RK3588 CTest `23/23 PASS`；`cmake.log`、`build.log`、`ctest.log` SHA-256 分别为 `83a3de2ff34591389062c90799d21bfbfb0182f366794ddc4ec1cdb90dfbef95`、`94759e1a816c1c14099767322733cee3cfacb9e25563144ba7e3d03efba279d6`、`422b5e616dff678ac4507cfdfefa0fee9bf707e96cfbbd58da5df7575917f336`；程序 `build/nanotrack_v3` SHA-256 `fde50985d78e8be9fc062641de386a0c68d0a122f4b268bb2ea3fe496a0e4fe2`。RKNN API/driver `2.1.0/0.9.8`。
- 板端配置 `runs/headless-eval.conf` SHA-256 `1000ecaf50ccd140748a4ec8ced35949c9d18911e8043ecd5794235a3cdd6fce`，controller 配置 SHA-256 `2f9980bbc27d89a0a6ff8aab44af77b85e7c3a7c5333b1f48534ec999b23feb3`。输入视频 `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；template/search/head RKNN SHA-256 `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55` / `d3cb0e3879df6a1272adb59bf501324f1f8f55b58309d3c4da4a731b600973ee` / `4da379f54093d79f02775e90bfea33ebee0837c6fc73a324499279b4c57807c5`，tracker manifest `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`；detector RKNN/manifest/labels SHA-256 `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c` / `ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9` / `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`。各 session 记录的目标类别均为 2。
- 在板端执行 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox <x,y,w,h> --output-boxes runs/<case>/frames.csv --session-metadata runs/<case>/session.json > runs/<case>/run.log 2>&1`，`manual-init` 使用用户点击框 `(1032.83374,405.310425,36,36)`，`car001-detector` 使用 detector 点击修正框 `(993.645660,581.063293,36,39.486084)`，`car002-detector` 使用 `(972.080475,518.201843,36,36)`，每组 271 数据帧、exit 0；这属于同框命令行回放，不是新 GUI 单击烟测。三组 CSV/session/log SHA-256 依次为：manual `467458f73b58accd9d7af95231d2bc3f9e3eab60e1955dd4f58d12f3651202b8` / `aff12ac784b48a64b75a21bea6c3f98d2e6855d66fe2aa64e54364169fe3d1c6` / `ebe3b3d1a4dcc35ffb64be66c268ef37099b8d46d69ae2b2df04aa50e6d6f4fd`；car001 `840881598961579867aa122cccbe167a2e4897fb5d6630b4b523713125118d52` / `1ff93bd70a71dad49248d0ae32e95ea74bae1629bfffc266d39820983ed657aa` / `eadebf33dccb307a306fa70743a84b46de0f454be9f95e87acba602b719e824f`；car002 `130222ddb5a551861da34c3a9451d2303b2114c2bc163f05d3607e21702b078e` / `3b1a0d544c4302fa37fd977312f7bf28556ff2ee5afd3cc769b1e440da7d6696` / `214e34873ba7be7dbaef6cf1c2e9a35cd299b30c434c124c7f27a167c127beb6`。
- 在主机执行 `python3 scripts/evaluate_occ_video_1.py --dataset-root /home/u20/single-object-tracking/data/occ_video_1 --target-id <car_001|car_002> --boxes /tmp/nt-no-texture-eval/<case>/frames.csv --scope full --output-json ... --output-markdown ...`：car001 `/tmp/nt-no-texture-eval/car001/report.json` SHA-256 `f81874e21a563a145f9e6f69a0120532ad63c0fed9a9dcd44541baa2aa9651a5` **PASS**、186 帧重捕、有效观测 mean IoU `0.760854`；car002 `/tmp/nt-no-texture-eval/car002/report.json` SHA-256 `c342e214cb136602f299c227db8ccc9dfe00bf04df919130a98eb07c41e651fe` **FAIL**，第 228 帧过早发布 `RECOVERY_STAGED`（可靠重现窗口从 230 开始），其余六项门通过。手点第三辆车无对应真值，166 帧曾重捕，随后再次丢失，不据此宣称成功。
- 为隔离本次变更，用旧程序 `occlusion-triad-3c848ad/build/nanotrack_v3`（SHA-256 `e4cc5d94ce09b8ab41f4ae923cd0e731f052398df9175725c5a10059276c70f8`）及其原配置、同一 car002 `--bbox` 再跑到新目录 `runs/car002-old-baseline/`；CSV/session/log SHA-256 `91d361ab9bc2362f2d394d92ebc999cf289e873829c6f18c616f81b798f2006b` / `ed62ab6f01e7bb4354025d4d6cb60e3fc91edf8b98aeb481f1bc526c8a8fc777` / `1261118af5c1fda319edacc62b19340f811c1005a7f3ab2f317e35ed62bbc72c`。同一 evaluator `/tmp/nt-no-texture-eval/car002-old-baseline/report.json` SHA-256 `a70aa2531e63b179aabfabdf2e8b5e167eb1c6457f12f77c94fbd63c4cc68cf` **PASS**，233 帧重捕。因此新版本的 228 帧早捕是本次移除后出现的回归。**裁决：代码保留在任务分支，不合入 main/Debug，也不替换已确认基线或生产 handoff**；后续需找到不依赖车顶纹理且能避开 car002 早捕的通用身份/可见性证据，并复验两真值及手点目标。

## 2026-11-16T00:01:07+08:00 移除恢复顶部纹理硬门：主机阶段

- 分支/工作树 `codex/remove-recovery-texture-veto` / `/home/u20/single-object-tracking/.worktrees/remove-recovery-texture-veto`，起点为干净的 `Debug@3d7f579d3ec7bc0be830865615dc0132ca1a673b`。本次依据板端 `/home/neardi/Project/SOT/occlusion-triad-3c848ad/runs/manual/frames.csv`（SHA-256 `cf3dfa3e2bebd4ed62fd0b2d8f9d896fc65bf101c0a89a1b1a47010eb69c8d23`）诊断：该次首帧目标位于约 `(1033,405)`，并非 car001/car002 真值目标；第 165 帧以后多次 Recent/Safe 与可见车同框，却有 76 帧被 `RECOVERY_TOP_TEXTURE_VETO` 拦截。用户指出车顶并不总可见，要求移除这个过拟合门。
- 删除顶部四分之一亮度标准差计算、恢复 veto 判定、配置字段、日志字段及相关 veto 单测；现行 `visible-dual-gt-visual-gate.conf` 删除 `recovery_min_top_luma_stddev=32.0`，新文件 SHA-256 `2f9980bbc27d89a0a6ff8aab44af77b85e7c3a7c5333b1f48534ec999b23feb3`。旧运行配置里的同名数值键仅作兼容解析并忽略，不再进入算法或配置身份；增加兼容性测试。detector 候选驱动下一帧 NanoTrack 搜索裁剪的机制本次未改。
- 实际执行 `cmake -S rk3566/nanotrack_v3 -B /tmp/nt-remove-texture-host-build -DENABLE_RKNN_RUNTIME=OFF -DBUILD_TESTING=ON`、`cmake --build /tmp/nt-remove-texture-host-build --parallel 4`、`ctest --test-dir /tmp/nt-remove-texture-host-build --output-on-failure`，主机 RKNN-off CTest `23/23 PASS`，`git diff --check` 通过。下一步提交精确源码，在 RK3588 独立目录原生构建并以手点目标及 car001/car002 detector 来源框回放；现阶段没有新板端效果证据，不替代已登记基线或生产 handoff。

## 2026-11-15T15:29:59+08:00 双分支集成与通用算法一致性

- 从新任务分支 `codex/occlusion-recovery-triad@2bef2cc18c21c5c3f226cc9bb4f08d1bccb16680` 集成。`main` 从 `661b60351e8050607843908c4d24f3094d391692` 在 `/home/u20/single-object-tracking/.worktrees/occlusion-main-integration` 仅引入通用配置/运行时/恢复算法/单测及本进度文档，源码提交 `876a92257251ba0fd81850dea016e303bf435630`；`Debug` 从 `5f76a5acfc8386c8c8fe6a9d5b7a6f0bd2a6175e` 在 `/home/u20/.codex/worktrees/integrate-adaptive-debug/single-object-tracking` 快进至 `2bef2cc18c21c5c3f226cc9bb4f08d1bccb16680`，包含仅 Debug 的 detector 单击最小尺寸、原/修正框事件记录。
- 两分支均实际执行 `cmake -S rk3566/nanotrack_v3 -B /tmp/nt-triad-<branch>-integration-build -DENABLE_RKNN_RUNTIME=OFF -DBUILD_TESTING=ON`、`cmake --build ... --parallel 4`、`ctest --test-dir ... --output-on-failure`，main 与 Debug 各 23/23 PASS。main CMake/build/CTest 日志 `/tmp/nt-triad-main-integration-*.log` SHA-256 `1d0ef00804854ed2d5dfaf0db53991f77ab02ffabe08618c0039654beea86ca2` / `a7911789f0e34833837f334261385bf6364bd31ea9a2e2919a2bdaa611f8a3b0` / `a6e7a53a7395c1c3625286209258decad02f46b274194057fc370abe88d0eea2`；Debug 对应 `/tmp/nt-triad-debug-integration-*.log` SHA-256 `37d3a4c9fa7cbc6af1de96eec85f3765fd9c90f5dd7ddb7eb7c48bcca4e5e523` / `01cfadaa2948904d041d7b5eeabbfcefedc52f59625d7bbfce2b8430e2cfcc81` / `2b36c45cd6d496126b55342e65f2484a598a972455f3dac73e23af1d79724cb1`。
- `git diff --quiet main Debug --` 通用配置、恢复候选、长时控制器、运行时算法、对应单测及 `scripts tests` exit 0；`git diff --name-only main Debug -- rk3566/nanotrack_v3 scripts tests` 仅列出 CLI、交互控制、覆盖层、`main.cpp`、运行配置 Debug 键及对应交互单测共 10 文件。两分支未重新在 RK3588 生成程序；板端证明绑定的仍是源码 `3c848ad98f465ea24f710feb41a17567b6dd7bf0`、程序 SHA-256 `e4cc5d94ce09b8ab41f4ae923cd0e731f052398df9175725c5a10059276c70f8` 及上方原件路径，该源码与 `Debug@2bef2cc` 的程序源码相同（后者仅增加进度文档）。RK3588 双目标视频通过不等于真实吊舱验收，正式生产 handoff 未批准。

## 2026-11-15T15:26:46+08:00 detector 单击双目标两次复测均 PASS

- 分支 `codex/occlusion-recovery-triad`，工作树 `/home/u20/single-object-tracking/.worktrees/occlusion-recovery-triad`，起始基线 `Debug@5f76a5acfc8386c8c8fe6a9d5b7a6f0bd2a6175e`，GUI 源码提交 `3c848ad98f465ea24f710feb41a17567b6dd7bf0`。主机 `/tmp/nt-triad-clickmin-source.tar` 与 RK3588 `/home/neardi/Project/SOT/occlusion-triad-3c848ad/source.tar` SHA-256 `acdc2e0fd26fa67a504b1609745ad052b9703b245196d07d4a0cc1cc9b8e8d34`。板端 `aarch64`、RKNN API 2.1.0/driver 0.9.8，输入 `data/occ_video_1/1.mp4`、template/search/head RKNN、detector RKNN/manifest/labels 的精确哈希见上方 `debug-click-0060b7b` 清单，路径由本次 `runs/headless-eval.conf` 指向同一板端冻结输入；配置 SHA-256 `cda6b05b8677da03fb2947ff8624ee9d02bb73faa98462a68c3f7ab4aa1069eb`，controller SHA-256 `52a3722eec8ed32c2f3ba44be50617ff6c16ad12254f04320b9802738f406bd6`。实际执行板端原生 CMake/构建/CTest，23/23 PASS，`build/nanotrack_v3` SHA-256 `e4cc5d94ce09b8ab41f4ae923cd0e731f052398df9175725c5a10059276c70f8`，cmake/build/ctest 日志 SHA-256 `47b5f4fd4bcffb87eedd81733eeb33003617c2ce896e084a85275cdcfdd13016` / `24f9e4ed7adc9cee065c34bda38eb72e03e266872ab164f03353150176a2a79e` / `ceb19e453fdb5061a1714af65d4ca3ac3b4f6aecd97cd782da6f9aeffdac42bd`。
- 实际运行 `xvfb-run -a -s '-screen 0 2600x1200x24' python3 detector-click-replay.py` 点击 car001 `(1009,600)`；原 detector 框 `(994.001,581.063,35.289,39.486)`，初始化框 `(993.646,581.063,36,39.486)`，类别 2，均明确写入 `click_selection_resolved`。脚本 SHA-256 `43ac8b21b93226f6b0278e5daa6b40025bd13389a5bf0d70a798c0a20b43e7ad`；板端 `runs/detector-click/frames.csv` / `session.json` / `run.log` / `output.avi` SHA-256 `e51f90cb2be0991bc73d12c89303e0c69e2ca28bd04bbdf7439a32cb1b0df1b4` / `3e46aa6903c67baec4e0a57828b446cb03e0d78e53a992521f76864803377478` / `8cabed92da7acaebce8060811923ea2d606dc54994b1ca43fa9e8874b20f9c3d` / `9d4d862e43f208ff209a8d3f51dae85c43fbc4caff486841954ab5dff89cb16d`；CSV 271 数据帧，full evaluator `/tmp/nt-triad-3c8-car001-score.json` SHA-256 `f6b8b148595a382aff75b9f3b2349c7bdbe4e6cec3eb9d51602a8cfb4ca71551` **PASS**，172 帧 SEARCHING、187 帧重捕、可见有效观测 mean IoU `0.757506`。
- 同法运行 `detector-click-car002.py` 点击 car002 `(988,535)`；原 detector 框 `(973.035,520.490,34.091,31.424)`，初始化框 `(972.080,518.202,36,36)`，类别 2。脚本 SHA-256 `93bfa4b48ca93f1aa89614eb6a6812be1a40ebc6c5a4c296bda34cbd2c11f09d`；板端 `runs/detector-click-car002/frames.csv` / `session.json` / `run.log` / `output.avi` SHA-256 `359c5dad0293b3271fe437a2943fc47528fbb7327c8ab8d4730bdcc176a9e1a3` / `4553c998f06769c42cd512eeec377cd037abe1a7ba562290220428317bf92f9d` / `86ef0779a5411bef524a0946ca702ef3e19e686f0debe78ab48a07e40ea6c874` / `502438c5015c88e76246eb6ba062764681b744d5dc929a3ee37fd2098ae1891e`；CSV 271 数据帧，full evaluator `/tmp/nt-triad-3c8-car002-score.json` SHA-256 `e7004e25aa0729d2fe9e013dab13fa91a587ccb7df4a3b97e2300bb7e9ed0b2c` **PASS**，233 帧重捕、可见有效观测 mean IoU `0.756859`。
- 各以同一点击再运行一遍 `detector-click-repeat.py` / `detector-click-car002-repeat.py`，脚本 SHA-256 `0ee19bf8e1f3be44c6b4b6c307b00677de33a27a533508327c25904cdab52e79` / `263c27b6f142d9ac6d34c3dc15b43c2dbeed8941e69239b91f51640c46f090a6`；板端重复 CSV/session 位于 `runs/detector-click-repeat/`、`runs/detector-click-car002-repeat/`，SHA-256 分别为 `518bb782c7772ce95458b4d9e1adc60169626a5a0a1040c6483c00e34815e6fc` / `1a6281c2b9f1e4cc131be3c25b7f7463ec4fb056b543d2cce7b381fafb593338`、`0ec848f81810c503b739b397ab3abdd1db3ea9e6770c341bec26c79b0b1e76d6` / `02c12ea6757fb7b5aaa90463cd67172193b8f17fe96680fb4ebbdbf81babf205`。两份 full evaluator `/tmp/nt-triad-3c8-car001-repeat-score.json` / `/tmp/nt-triad-3c8-car002-repeat-score.json` 各 **PASS**，报告 SHA-256 与首轮对应报告完全相同，重捕帧 187/233、有效观测 mean IoU 0.757506/0.756859。以上是 RK3588 视频/GUI 证据，不等同真人吊舱验收；正式生产 handoff 未批准。下一步合并 main/Debug 并核查通用算法源码哈希相同。

## 2026-11-15T15:20:26+08:00 获准记录原 detector 框并限制点击跟踪框最小尺寸

- 用户确认 car002 单击可保留 detector 原框，同时用以原框中心为准的最小 `36×36` 修正框初始化跟踪器。分支 `codex/occlusion-recovery-triad`，工作树 `/home/u20/single-object-tracking/.worktrees/occlusion-recovery-triad`，任务基线 `Debug@5f76a5acfc8386c8c8fe6a9d5b7a6f0bd2a6175e`，本阶段前置提交 `6987903cf5430dcaea3d78cbebaf059fa5bf87dd`。Debug 单击 detector 时，新增配置 `click_detector_min_bbox_size=36x36`，宽高须为正整数，分别只补足不足 36 的维度并按图像裁剪；`click_selection_resolved` 同时输出原 detector `bbox`、实际 `tracking_bbox` 和 `bbox_adjusted`。空白点击回退框及命令行 `--bbox` 语义不变。改动范围仅 Debug CLI/交互文件，通用算法部分继续与 main 保持同源。
- 主机实际执行 `cmake --build /tmp/nt-occlusion-triad-build --parallel 4`、`ctest --test-dir /tmp/nt-occlusion-triad-build --output-on-failure`，RKNN-off CTest 23/23 PASS；日志 `/tmp/nt-triad-host-build-12.log` / `/tmp/nt-triad-host-ctest-12.log` SHA-256 `c1754349c1a8a3be457104a98f38016b09fc534ca275cda7c92077a817647efa` / `8878771031cfdbb1e560b6ec0155c6dac84fd2a96c4bd4c76c864591768e30e5`。下一步板端原生构建并分别实际单击 car001/car002，核对原框与修正框、完整真值评分及程序/工件哈希；正式 handoff 仍未批准。

## 2026-11-15T15:16:31+08:00 改按 detector 单击初始化双目标验证

- 用户把本轮验收范围改为 detector 实际结果初始化，不再要求八个手设框；板端 `1927bda7432cb4a07abbacf1e79fdd4e74e24460` 的八框批次在 car002 nominal 运行中按要求停止，已完成的零散 CSV 仅保留原件，不作本轮验收。分支/工作树 `codex/occlusion-recovery-triad` / `/home/u20/single-object-tracking/.worktrees/occlusion-recovery-triad`，起始基线 `Debug@5f76a5acfc8386c8c8fe6a9d5b7a6f0bd2a6175e`。源码包 `/tmp/nt-triad-motion-gate-source.tar` 与板端 `/home/neardi/Project/SOT/occlusion-triad-1927bda/source.tar` SHA-256 `e80af940114d69bd419b5bec5567fa93e0be2036e1a35fb127a2fc88d14e58df`；板端配置 `runs/headless-eval.conf` SHA-256 `cda6b05b8677da03fb2947ff8624ee9d02bb73faa98462a68c3f7ab4aa1069eb`，输入视频/模型/标签与上方已登记清单一致。原生 CTest 23/23 PASS，程序 `build/nanotrack_v3` SHA-256 `ced5c458190ac0ba81e72b9b646dcce87c66a83be4f5a25f5171e6680ce868ee`，cmake/build/ctest 日志 SHA-256 `d4a3fbcb12f44cfcc13a5d86796d8d2c18564bac621cb895b939774` / `292cd5aeaba6208b99d425c4b47a80e13462d0a376e3233c6f742de6b7153728` / `60bdd3241420a0b0e09609827c2dc93c41f3264e6ba53b3bbde308016bd26d4d`，RKNN API/driver 2.1.0/0.9.8。
- 实际 GUI `xvfb-run -a -s '-screen 0 2600x1200x24' python3 detector-click-replay.py`，第 0 帧单击 car001 `(1009,600)`，detector 框 `994.001,581.063,35.289,39.486`、类别 2。运行退出 0、CSV 271 数据帧；脚本/CSV/session/log/AVI SHA-256 `c6e5e289d0993ecfb3eb6905f64a7ae3db958880bb071ba3701a83a8d3d84854` / `cd848e270242beee44ca3e081be4beef0eec6f93e60930fd83598a1ecdf9e48d` / `ad6970e909804e6bda8becd172a0ffebc35e8db76bb057c48b4d0882aaa1a720` / `d9781a700575989d610ae4423b44323329ad0064bd3b508fe64e44828275b572` / `64fdb24c5bfa1f4b17b8e2d95690d9960663d0fb207e5cff62acc0b5c195ca7b`，原件板端 `runs/detector-click/`。`scripts/evaluate_occ_video_1.py --target-id car_001 --scope full` 报告 `/tmp/nt-triad-1927-detector-click-score.json` SHA-256 `2670266571b7d3b6433bc255563c803e27c5469a2c4ba1cd7d1d0400a2dfcabe` **PASS**，172 帧 SEARCHING、189 帧重捕，可见有效观测 mean IoU 0.747518。
- 实际 GUI car002 `xvfb-run ... python3 detector-click-car002.py`，第 0 帧单击 `(988,535)`，detector 仅选框 `973.035,520.490,34.091,31.424`、类别 2；脚本/CSV/session/log/AVI SHA-256 `03cb66cc27fb7a599e1d4675545400cf575b6d6241f58134e6c9f56da0301fe9` / `f6b3b4dcee7849b2fb5689e351eb69c27947c09ca957eedf94a8b598d229c86b` / `fc1fb835e72b5308840ec5b63eea0ee0fb8f73e4025702540f230898e8ee43af` / `ab41df05a6c746ee2209aadb1c962d278e14b6d63b410e73ebbeedd253d0dc1e` / `f6ea1aee5ece1abc1b2cba0a92b337921dd10ef742e1f619403b727b9074770f`，原件板端 `runs/detector-click-car002/`。full evaluator `/tmp/nt-triad-1927-detector-click-car002-score.json` SHA-256 `d3e9815bbfed440e7cc4f6a04cdc6c89ed3b9315addf41c763f0b4dc1858842a` **FAIL**：73、192 帧错误重捕，有效观测 mean IoU 0.134866。板端隔离点击扫描 `car002-detector-scan.py` SHA-256 `272e92552b7b8323d5fd95cc992b9f869852bc74f3bac31b4e780b88763a6351`：真值框左上边缘多点没有其他 detector 候选，只有上述较小框覆盖中心/右下。
- 仅为定位，用同一板端程序/配置、以该 detector 框中心为准扩到 `36×36`，运行 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox 972.080475,518.201843,36,36 --output-boxes runs/car002_detector_min36_probe/frames.csv --session-metadata runs/car002_detector_min36_probe/session.json`，CSV/session SHA-256 `c25acdbed02fb0b0679e331c565e14152a504033b0a8085f1200c1fc64208fb6` / `5675bd1c192541e1ca2cdcf4d4c861e39a7ce5a4c0b109bfebcc535491e4a2c9`，full evaluator `/tmp/nt-triad-car002-min36-score.json` SHA-256 `a70aa2531e63b179aabfabdf2e8b5e167eb1c6457f12f77c94fbdc63c4cc68cf` **PASS**：233 帧重捕、有效观测 mean IoU 0.756859。这是尺寸修正探针，**尚非 detector 原框实际 GUI 验收**。已向用户澄清是否允许记录原框并以最小 `36×36` 修正框初始化；得到答复前不将该探针计为最终通过，也不合并 main/Debug。

## 2026-11-15T15:02:06+08:00 精确框 PASS、八框 2/8 后增加重捕运动门

- 分支 `codex/occlusion-recovery-triad`，工作树 `/home/u20/single-object-tracking/.worktrees/occlusion-recovery-triad`，起始基线 `Debug@5f76a5acfc8386c8c8fe6a9d5b7a6f0bd2a6175e`，板端源码提交 `4ef8159b68c80f0bc7fda3ba4c87495884f73453`。源码归档 `/tmp/nt-triad-overflow-source.tar` SHA-256 `30d094bf7292757b406399c5ef33d7884e8a89cefcde1c39ca7f7bd5b64b08e3`，板端独立根 `/home/neardi/Project/SOT/occlusion-triad-4ef8159`；配置 `runs/headless-eval.conf` SHA-256 `cda6b05b8677da03fb2947ff8624ee9d02bb73faa98462a68c3f7ab4aa1069eb`，视频/模型/标签为上方已登记的精确哈希，RKNN API/driver 2.1.0/0.9.8。RK3588 native CTest 23/23 PASS，程序 SHA-256 `b544a98c64c7256183e0e600a2a0a07dbc1d1182dec3f6a744d1ce93176e36b7`，cmake/build/ctest 日志 SHA-256 `96e58882ea8394aa78dc1e5919e3a14635a06514e6f00e8fc2ab5d5646e69076` / `fc568b8116f4c2a1bfd68d54026a91c49171700a02d9716427da90a1e20f298c` / `5c332c99b242f4c5f7fb616bee480731715c621182e652d005bdccb1f8d9ac61`。
- 精确点击框同命令回放 exit 0，板端 `runs/car001_exact/frames.csv` / `session.json` / `run.log` SHA-256 `caa28322163c88b2373d2ae9f08c8e70906f4b0a09122f018f5e9c8c690176d7` / `4f4d7b6d0ce97d88e4e8abf6a00ff26bafb6e13cc6cd2a70639cf9a942ce4cdf` / `47ccd8dad6ba6354b13a3572b33c794bd32f4808ef4d9dc4264ac6c82143f8af`。full evaluator `/tmp/nt-triad-4ef-car001-exact-score.json` SHA-256 `c64c2fcfe6a5861d536635d819423d4726e34d8a348fdd510fbf44e2c4ae2ef0` **PASS**：172 帧 SEARCHING，189 帧重捕，可见观测 mean IoU 0.747518。
- 用板端脚本 `run_dual_window_four.sh` / `run_dual_window_extra_four.sh` 分别执行 car001 四框 `992,582,37,39` / `994,584,35,37` / `993,585,36,36` / `990,580,39,41`，car002 四框 `970,517,38,38` / `967,513,42,42` / `968,515,40,40` / `972,519,36,36`，每路 271 帧、exit 0。脚本 SHA-256 `94eabea46d7c08b53fb4fe20b424e17bb87a8373f5042ccfd55907937febc1e9` / `83f0f442d58d1b3dd8967b6472de608ea0a24a9fe1e239d3b3767d54e54dce73`，输出在板端 `runs/<name>/`；主机 CSV 打包 `/tmp/nt-triad-4ef-eight-frames.tar.gz` SHA-256 `fd8b3768285e11f49cd3741563509e934a5b23aefcb52f96b756aa78109b41bd`。实际执行 `scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ... --output-json /tmp/nt-triad-4ef-eight-score.json`，JSON SHA-256 `d84b1a1acb5362918d0e97e6bed59f6c248ae46780e2ee6af4d3667e05375e29`：仅 car002 truth/right PASS，**2/8，否决本候选**。car001 nominal/user/left 在 177 帧错误重捕，car002 nominal/left 在 224–225 帧错误重捕。
- 失败帧的目标候选距运动预测分别约 `1.33–1.39` 个预测框对角线；精确 car001 真重捕约 `0.74`，car002 truth 真重捕约 `0.22`。本修订只对 full-object 且 Recent/Safe 同候选的恢复设可配置的运动中心门 `recovery_dual_template_max_motion_diagonals=1.0`；远处的单模板 detector 验证与 subtarget offset 路径保留既有行为。主机 RKNN-off CTest 23/23 PASS，日志 `/tmp/nt-triad-host-build-10.log` / `/tmp/nt-triad-host-ctest-10.log` SHA-256 `17c74f8f01565cc486f80a90cc16e4e44fd2b961cbdff793623ac9b2a30bad7c` / `9446ce7a9e2b6ee5337e8ce819dfd38f03c9a1c745cc6f2157cdd00bb1eb26fc`。下一步重新板端精确框和八框回归；尚未集成 main/Debug、正式 handoff 未批准。此处把前几个候选条目的时间标题按对应 Git 提交时间纠正，内容与结论不变。

## 2026-11-15T14:50:50+08:00 全遮挡及时进入搜索后重捕延迟定位

- 前次源码 `933a6df061903f274089853b3046b4a27666b835`，分支/工作树 `codex/occlusion-recovery-triad` / `/home/u20/single-object-tracking/.worktrees/occlusion-recovery-triad`，起始基线 `Debug@5f76a5acfc8386c8c8fe6a9d5b7a6f0bd2a6175e`。源码包 `/tmp/nt-triad-final-candidate-source.tar` SHA-256 `66a38b7ea3eed839224431ab902cad5f8c8679fda620c5e1fe98fc12e567d522`，板端根 `/home/neardi/Project/SOT/occlusion-triad-933a6df`，配置 SHA-256 `cda6b05b8677da03fb2947ff8624ee9d02bb73faa98462a68c3f7ab4aa1069eb`，视频/模型/标签与上述清单一致；RK3588 native CTest 23/23 PASS，程序 SHA-256 `daef757d676dcc74e22575e45e4f0070ada14e44d177ec74ad10a0c9021417c9`，cmake/build/ctest 日志 SHA-256 `f9e4f7b01f925070d93dbc7f7aa1b8da8615eaf8367258809354fbded3da6c87` / `f64dc260dd0d705c43165e4a15d15574c1e864ad64799d170c97cc37f70c98c6` / `4c36d4d585af9e88c484f7625fa6c0eb7c6ab992be95cb17021f95089ee6736f`，RKNN API/driver 2.1.0/0.9.8。
- 同一精确框命令独立回放两次，首轮 CSV/session/log 位于板端 `runs/car001_exact/`，SHA-256 `60c9c94ed59687ecc00dc9f52fdc1bd6355bc497ce3f56b85a0acabccf73075d` / `7079e3209e895bded437438c8e29e841aadbfe27586228581eae916606f3e237` / `2b1f508a51e234d748b0683d6b51a4c0cecad97d487745b29f624211f9e19164`；重复运行 `runs/car001_exact_repeat/` CSV/session SHA-256 `3d5aef06c20133316ea746a045dcc8895e6ccf79b1caa3029cf834d3e4f4ef36` / `98fe8bb5c9d8381b6b89612ac3818bf05c4cb6de854edcdfb938a34ef80ccac4`。两次均第 172 帧进入 SEARCHING，但第 190 帧才重捕。full evaluator `/tmp/nt-triad-933-car001-exact-score.json` SHA-256 `1e9e86447b9238573e0bb5aeee5a679fb90b24dffab339c777307b8875145010`：全遮挡、可见框、身份代理 PASS，重捕时窗 FAIL（必须早于190）。
- 逐帧发现第 188 帧已有 Recent/Safe 与目标 detector 连续同框；第 189 帧新 detector 批次包含大量远处溢出框，候选表原逻辑把所有已验证命中清空，导致第 190 帧才有第二次确认。本修订保留远处溢出时的命中，仍要求当帧重新检查所有溢出框是否与任一模板重合；真正竞争框依然否决。主机 CTest 23/23 PASS，日志 `/tmp/nt-triad-host-build-7.log` / `/tmp/nt-triad-host-ctest-7.log` SHA-256 `8003a5763b45fa2211d75c3b716b6457842ac58fc33d3f5f7e4f70633a8c727d` / `a67f716293c85b285047f9e37d3d5be7e3484692e9be0ed759cd213187f8ce0e`。下一步板端精确框及八框回归；正式 handoff 未批准。

## 2026-11-15T14:45:56+08:00 视觉门诊断：首个有效命中在全遮挡帧

- 前次源码 `d6236935d88aae126111d51d0d0465edad914976`，分支/工作树 `codex/occlusion-recovery-triad` / `/home/u20/single-object-tracking/.worktrees/occlusion-recovery-triad`，起始基线 `Debug@5f76a5acfc8386c8c8fe6a9d5b7a6f0bd2a6175e`。源码包 `/tmp/nt-triad-safe-source.tar` SHA-256 `ffffb9ffc27e76e27410fd30262d7fe44cc720a2f7255b5a3678edbd9391c9ec`，板端独立根 `/home/neardi/Project/SOT/occlusion-triad-d623693`，配置 SHA-256 `cda6b05b8677da03fb2947ff8624ee9d02bb73faa98462a68c3f7ab4aa1069eb`，视频/模型/标签沿用已登记哈希；RK3588 native CTest 23/23 PASS，程序 SHA-256 `2f58a3ec64e7ae3e5931a3871575e29c4ae24820976fbda7ca82821d7eb9c401`，cmake/build/ctest 日志 SHA-256 `970de27f1192706b54608dd267d8b1b53b8cdd2e08190b274659084d4d59d14c` / `ec144b991fd6a63fcc5963542869e728bb19d61c793db9412d077144067e51a0` / `d0706d3b93a46c7e868de65f337e89938bc476a29661c19ffdceb0b2c17a6510`，RKNN API/driver 2.1.0/0.9.8。
- 仍用同一精确框命令回放 271 帧，板端 `runs/car001_exact/frames.csv` / `session.json` / `run.log` SHA-256 `a79baa8c1ff5c4d7e8c23697345c573c2dbbe5af779707bb265df12e6cd8b81d` / `4267845876f3af2c4dda7f4232473e4589379db0577d5f5e1479702a184d9262` / `6aafb01284e75ba5223a721fb336899d29f6181129d2eb1b78cd7d8f860f77c3`。full evaluator `/tmp/nt-triad-d623-car001-exact-score.json` SHA-256 `2ccc6c94cc43ca9936dcb7e756c75adec3ce7d51bf138392c19424985b7138ae`，仍仅第 172 帧 full-occlusion FAIL，188 帧重捕 PASS。
- 在隔离目录 `/home/neardi/Project/SOT/occlusion-triad-diag-d623693` 临时编译的诊断版只用于定位：169–171 帧视觉 detector 已运行但无有效视觉命中，172 帧才首次命中，所以两帧视觉命中门槛无法在 172 帧满足。当前修订改为视觉命中时若本帧 Recent 与 Safe 均有解码观测、IoU>=0.5，则即时停止发布观测；无 Safe 观测时维持原两帧/近处歧义三帧规则。主机 RKNN-off CTest 23/23 PASS，日志 `/tmp/nt-triad-host-build-6.log` / `/tmp/nt-triad-host-ctest-6.log` SHA-256 `9d07bfa47ebe3c4403203ff469c27193136e8da0edda5d70120bbe2fb30b96d2` / `ebfa22b1cdd27f661714e52dbfaa9ea8bbb3cc491e9bc8793d4baa8b0fe6bd0b`。下一步板端精确框与八框回归；正式 handoff 未批准。

## 2026-11-15T14:41:11+08:00 精确框再次反馈：Safe 交叉验证缩短视觉等待

- 前次候选源码 `aef24347d386286d22964189e8097810a044e063`，分支/工作树 `codex/occlusion-recovery-triad` / `/home/u20/single-object-tracking/.worktrees/occlusion-recovery-triad`，基线仍为 `Debug@5f76a5acfc8386c8c8fe6a9d5b7a6f0bd2a6175e`。源码归档 `/tmp/nt-triad-aef2434-source.tar` SHA-256 `31bee38feb9a9067f0316d6af927a539d616b225baa638206633705dfb97b579`，板端独立根 `/home/neardi/Project/SOT/occlusion-triad-aef2434`；配置 `runs/headless-eval.conf` SHA-256 `cda6b05b8677da03fb2947ff8624ee9d02bb73faa98462a68c3f7ab4aa1069eb`，视频/模型/标签与上方已登记的精确哈希一致。RK3588 native CMake、编译和 CTest 23/23 PASS，程序 SHA-256 `705f45cd36d0f3a03dfaca3021c548a21cbf7eb04e7417ed7bdb8a24edcb0f28`，cmake/build/ctest 日志 SHA-256 `a596bf4b15d1435bc8b9d1368cf7b65315240245de3bacac4762dc4dd24c4dc8` / `83d1daf9f0d11d7ba8dda76479c2a52838e44817c0377e24cd3d54123599c341` / `dc0a20bea3af98d57ebda11d6b37d1f30d68586add1572c7f3b3c9e941298d17`，板端 RKNN API/driver 2.1.0/0.9.8。
- 同一精确框命令 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox 994.000916,581.063293,35.289490,39.486084 --output-boxes runs/car001_exact/frames.csv --session-metadata runs/car001_exact/session.json` exit 0，271 帧；CSV/session/log SHA-256 `d60a4dfed86a9dcbe3121ff570870260de8ef900b698248c3e5dbf5cb03a35ee` / `fbe19ddb7ba4533f022f5a20396e2a46ca0cb5f09bfc76dc846b8db51481ef95` / `67fe3fdea01ed2b8dd0f07c6b4f6ac95e31c6dea0c9fb036a69c10c21a4de02b`。full evaluator 报告 `/tmp/nt-triad-aef-car001-exact-score.json` SHA-256 `092b68b3822316112f6a8ed7f6c8f063eb782d692026e6224b1462c7b9234e35`，重捕仍在 188 帧，唯一 FAIL 是 172 帧全遮挡仍 SUSPECTED。第 172 帧 Safe 实际执行且与 Recent 同框，附近视觉候选仍被标记歧义，因而多等一帧。
- 本修订仅在连续两帧视觉遮挡信号且当前帧 Recent/Safe 均有同帧观测、IoU>=0.5 时取消歧义额外等待；只有 Safe 计划而没有观测不能触发。近处竞争框无 Safe 交叉验证仍需三帧，远处框不算歧义；主机 CTest 23/23 PASS，日志 `/tmp/nt-triad-host-build-5.log` / `/tmp/nt-triad-host-ctest-5.log` SHA-256 `07bc30f083e86830d9902a82e865512f8df4fd6ff76c0758b0616e58f080a566` / `ef8e4ad9ec45018d1a85b850a29b0bce6155b18bdf13488276710637f98a0fdf`。下一步再次板端精确框并执行八框回归；本候选尚未正式 handoff。

## 2026-11-15T14:35:31+08:00 三路重捕板端有效，视觉搜索时机再修订

- 分支 `codex/occlusion-recovery-triad`，工作树 `/home/u20/single-object-tracking/.worktrees/occlusion-recovery-triad`，基线 `Debug@5f76a5acfc8386c8c8fe6a9d5b7a6f0bd2a6175e`，前次源码提交 `9bb7737afe8a2991da02ae3790220e46e7b787b1`。源码归档 `/tmp/nt-triad-9bb7737-source.tar` SHA-256 `de9579ef6908bbf23f729309afc5b22e307b239558ab0042fcd0bdffe86b4210`；板端独立根 `/home/neardi/Project/SOT/occlusion-triad-9bb7737`，原生 CTest 23/23 PASS，程序 `build/nanotrack_v3` SHA-256 `463fa8d0092305291de05bc54ad3615c8749a0721a38d857a93beeb549436b14`。输入视频、模型与标签沿用已登记的 `debug-click-0060b7b` 精确哈希；运行配置 `runs/headless-eval.conf` SHA-256 `cda6b05b8677da03fb2947ff8624ee9d02bb73faa98462a68c3f7ab4aa1069eb`，controller SHA-256 `52a3722eec8ed32c2f3ba44be50617ff6c16ad12254f04320b9802738f406bd6`，RKNN API/driver 2.1.0/0.9.8。
- 实际运行 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox 994.000916,581.063293,35.289490,39.486084 --output-boxes runs/car001_exact/frames.csv --session-metadata runs/car001_exact/session.json`，输出 271 帧；CSV/session SHA-256 `92bc6e160f873d2120d2b0c0f1fe98dd675938521c551822ef9340e2fa2786dd` / `fe5ae6ec46ee09263da0baf1df71af5562268719c050ae1dcc29915ed739e3ed`，原件在板端该独立根的 `runs/car001_exact/`。`scripts/evaluate_occ_video_1.py --dataset-root data/occ_video_1 --target-id car_001 --scope full` 的报告 `/tmp/nt-triad-9bb-car001-exact-score.json` SHA-256 `8728883fdc57dabb8a4db0f318e14f80770c88b96da2abf07fea2331c1383242`：重捕、可见框、身份代理 PASS，第 188 帧完成重捕；全遮挡语义 FAIL，因第 172 帧仍为 SUSPECTED、第 173 帧才 SEARCHING。
- 新修订只将预测框附近且达到置信度门槛的同类检测框计入视觉身份歧义；远处第二框不再强制增加一帧确认，近处竞争框仍需额外一帧。主机重新构建及 CTest 23/23 PASS，日志 `/tmp/nt-triad-host-build-3.log` / `/tmp/nt-triad-host-ctest-3.log` SHA-256 `8bc9c723643b0946969d2d89bec08e5b1f82692de7` / `8df1ed4efd779b97a721381fdd6c8f9183ad1d5f1f178318b5c1837d61276c45`。下一步板端精确点击框与八个固定框回放；正式 handoff 未批准。

## 2026-11-15T14:29:41+08:00 精确 car001 框板端否决后修订候选

- 前一候选 `codex/occlusion-recovery-triad@e42f2d04d646addb0d4e96eddc032c2c98827a6d`，板端隔离根 `/home/neardi/Project/SOT/occlusion-triad-e42f2d0`，源码包主机 `/tmp/nt-triad-e42f2d0-source.tar` 与板端 `source.tar` SHA-256 同为 `14fbcde1294a61a1a1e5cdeb56807474e67e7d16225b9e4dfd90ee0a214b4c0a`；RK3588 原生编译和 CTest `23/23 PASS`，`build/nanotrack_v3` SHA-256 `c894e0a165527d3ec89e7f5ea5994be035d04443322b09a7bf21a874f730b84b`，运行配置 / controller SHA-256 `e58dd857e9aa15278b086dcedce3381067e7738c09cd3dca8eab5505479adb13` / `52a3722eec8ed32c2f3ba44be50617ff6c16ad12254f04320b9802738f406bd6`；CMake/构建/CTest 日志 SHA-256 `f5db95026a28e843c10a13821b9ab73651868861b9eae3e40f65f60788a99b65` / `56c78f5a329b7428b3897fa05346abb3b737b13b4f1f6ee1410a192bac78c0bf` / `e520d97c6e0bb2ae5839a14032b9a58dc8e510e6c2c3bf3484e71ada0f1acd39`。
- 实际执行 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox 994.000916,581.063293,35.289490,39.486084 --output-boxes runs/car001_exact/frames.csv --session-metadata runs/car001_exact/session.json`，exit 0，271 数据帧；CSV/session/log 板端原件 SHA-256 `f99ee98912fb8a9907e4ca1e6e75f47feb2013a8566c2ead8a14880460257cbd` / `70fc289a96b476b54276a3ae91c7d295fe9505ac9a32be5a8b9e2b64e42ba81a` / `ebc77b6088677fd73761423e008bfe0b7d55ab2ca8e93085e7d2da785975dd1d`。主机回传同哈希，`scripts/evaluate_occ_video_1.py --dataset-root data/occ_video_1 --target-id car_001 --boxes /tmp/nt-triad-car001-exact.csv --session-metadata /tmp/nt-triad-car001-exact-session.json --scope full ...` 报告 `/tmp/nt-triad-car001-exact-score.json` SHA-256 `273cee892e4d040efe1b0b1f1908e094010b3944e9284175cf86bf539107ede8`，**FAIL**：无重捕、全遮挡语义失败。第 184 帧有 121 个位置有效检测候选，仅保留 3 个，未表示的远处候选被整体标记为冲突，三路确认无法生效。
- 修订候选在候选表短期保存溢出检测框；唯一三路匹配须检查全部保留框与溢出框，任何溢出框匹配 Recent 或 Safe 仍否决，只有远处溢出不再压掉两帧验证。主机 RKNN-off 实际重新构建并运行 CTest `23/23 PASS`，构建/CTest 日志 `/tmp/nt-triad-host-build-2.log` / `/tmp/nt-triad-host-ctest-2.log` SHA-256 `60bb8712c2329c5ba286fa6bd106b1244c7b3fdc7fc7d1333d614a07c2608de7` / `df4d052a14b9b32f0216cb49c1885c30062f4d5cfd007839caca5ff9090eaa7f`；本修订尚未板端运行，下一步部署新隔离目录。正式 handoff 未批准。

## 2026-11-15T14:23:07+08:00 car001 三路重捕候选主机验证（板端待验）

- 分支 `codex/occlusion-recovery-triad`，工作树 `/home/u20/single-object-tracking/.worktrees/occlusion-recovery-triad`，基线 `Debug@5f76a5acfc8386c8c8fe6a9d5b7a6f0bd2a6175e`；来源为用户单击手测 `0060b7b` 程序在 car001 第 185 帧的 `IDENTITY_CONFLICT`，不是旧算法与新算法版本差。原始板端记录 `/home/neardi/Project/SOT/debug-click-0060b7b/runs/manual-click/frames.csv`，主机只读副本 `/tmp/nt-car001-manual-frames.csv` SHA-256 `6925b6c033e06d92efd39232dd57cf65cc795ce4047499b4259841a2b5581f96`，初始 detector 框 `(994.000916,581.063293,35.289490,39.486084)`、类别 2。
- 本候选仅为唯一 detector 与可靠 Recent/Safe 同框时建立候选专属的两帧确认；其他探测框若也匹配任一模板则保留冲突，detector 单独不能授权控制。主机实际执行 `cmake -S rk3566/nanotrack_v3 -B /tmp/nt-occlusion-triad-build -DENABLE_RKNN_RUNTIME=OFF -DBUILD_TESTING=ON`、`cmake --build /tmp/nt-occlusion-triad-build --parallel 4`、`ctest --test-dir /tmp/nt-occlusion-triad-build --output-on-failure`，结果 `23/23 PASS`；最终构建/CTest 日志 `/tmp/nt-triad-host-build.log` / `/tmp/nt-triad-host-ctest.log` SHA-256 `2b2d7bea36ceb923042695d41c3a49312cf74b79997fb8ca45da860513cc0ff7` / `b2f1fcf80f98b1f3261f16ff9082104780859ae716425a60cc60b11a74e7018e`。
- **边界 / 下一步**：目前只有 RKNN-off 主机单测，没有本候选 RK3588 原生构建、八框回放、精确点击框回放或真值评分；不得视为板端通过。板端免密 SSH `neardi@192.168.1.106` 本轮认证失败；待建立授权连接后在隔离目录构建并执行上述回放。遮挡进入 SEARCHING 时机尚未修改，须以板端闭环证据决定安全触发规则。正式 handoff 继续保持未批准。

## 2026-11-14T22:59:13+08:00 Debug 单击交互集成到 main / Debug

- 用户要求两分支算法一致、Debug 仅有交互差异。`main` 从 `84b63aeaf575a717652d757b5cd92c993959b835` cherry-pick 通用运行时提交 `78665da675a8ca05e46a96dcfa49dd697299fe26`，得到 `main@1f5b10a0b56da60fa76a34c145bd92e1e314cec0`；`Debug` 从 `1573b20c9aef2b8747752bf5467b5ac4a0261136` 快进功能分支至 `Debug@1791a85c42c7ec4a7fc8f1ae19b5e3ccf469a724`。`git diff --name-only main Debug -- rk3566/nanotrack_v3 scripts tests` 只有 10 个 Debug 交互、配置解析及相应单测文件；`LongTermRuntime` 头文件、实现和单测完全一致，评测脚本与 Python 测试完全一致；逐段核对 `main.cpp` 的 `lower_occluding_band` 与 `runtime.process_frame` 调用一致。Debug 板端实测源码为上方 `0060b7b`，后续 `1791a85` 仅增加配置解析单测和进度文档，不改变板端程序源码。
- 分支集成工作树 `main` `/home/u20/single-object-tracking/.worktrees/click-main-integration`、`Debug` `/home/u20/.codex/worktrees/integrate-adaptive-debug/single-object-tracking` 均实际运行 `cmake -S rk3566/nanotrack_v3 -B /tmp/nt-click-<branch>-integration-build -DENABLE_RKNN_RUNTIME=OFF -DBUILD_TESTING=ON`、`cmake --build ... --parallel 4`、`ctest --test-dir ... --output-on-failure`，各 23/23 PASS。CTest 日志 `/tmp/nt-click-main-integration-ctest.log` / `/tmp/nt-click-debug-integration-ctest.log` SHA-256 `bee6024b2c1c69e3163d1c948ac0a33a8488597c2896227d7a4d55ffb09cf259` / `2696b238db9c18f204a6c13d460f59f8b8047fea46437987dbbe805db0919ac5`；主机 main `main.cpp` 使用板端 RKNN 头文件完成 `g++ -fsyntax-only`。下一步双分支原子推送并核对远端提交；正式生产 handoff 仍未批准，真实吊舱验收仍需人工完成。

## 2026-11-14T22:55:11+08:00 Debug 单击选目标 RK3588 原生验证

- 功能分支/工作树 `codex/debug-click-select` / `/home/u20/single-object-tracking/.worktrees/debug-click-select`，起点 `Debug@1573b20c9aef2b8747752bf5467b5ac4a0261136`，通用源码 `78665da675a8ca05e46a96dcfa49dd697299fe26`，本次板端精确 GUI 源码 `0060b7b9d1af8d195c1dd9cb37696267ed562f78`。`git archive` 主机 `/tmp/nt-click-0060b7b-source.tar` 与 RK3588 `/home/neardi/Project/SOT/debug-click-0060b7b/source.tar` SHA-256 同为 `ffa1e18a66d07582d4c1703fd174c43e4bf3e320db3d21818af45072e1fb7da5`，板端 `aarch64` / Linux `6.1.84`；原生 `cmake -S rk3566/nanotrack_v3 -B build -DENABLE_RKNN_RUNTIME=ON -DBUILD_TESTING=ON -DRKNN_INCLUDE_DIR=/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/include -DRKNN_LIBRARY=/usr/lib/librknnrt.so`、`cmake --build build --parallel 4`、`ctest --test-dir build --output-on-failure` 均成功，CTest 23/23 PASS。`build/nanotrack_v3` SHA-256 `71417d15b192e161e2d92b408810afc93c75c35475bc82d8dd7f0e8db6f82562`；`cmake.log` / `build.log` / `ctest.log` SHA-256 `5c8b7ea9e9cf42ef19735952c44d4821191f0117fb67ab55992ac349f760291e` / `d4b795720de867ddf8943f2593caf14ceeb0e42a4ee52e1b2b8327beac847450` / `bb6c7cd265c680785556bdaa8849a40a01e12cc09f786a901dca224325405bc7`。模型加载日志 RKNN API `2.1.0`、驱动 `0.9.8`。
- 冻结输入 `data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`，板端运行配置 `/home/neardi/Project/SOT/dual-gt-ui-13bd45d/runs/headless-eval.conf` SHA-256 `55f4ffb4f196adddb4505f878874d97348aec75613a5a8c0b03fc04701e326f0`，controller 配置 SHA-256 `52a3722eec8ed32c2f3ba44be50617ff6c16ad12254f04320b9802738f406bd6`。template/search/head RKNN SHA-256 `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55` / `d3cb0e3879df6a1272adb59bf501324f1f8f55b58309d3c4da4a731b600973ee` / `4da379f54093d79f02775e90bfea33ebee0837c6fc73a324499279b4c57807c5`，tracker manifest SHA-256 `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`；detector RKNN/manifest/labels SHA-256 `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c` / `ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9` / `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`。完整板端路径、字节数及哈希清单 `/home/neardi/Project/SOT/debug-click-0060b7b/evidence.json` SHA-256 `78fb10d4380c06c10e431a1858cf945045124a510b3b84427d6bae64f7c63d8a`。
- 板端实际 `xvfb-run -a -s '-screen 0 2600x1200x24' python3 click-gui-smoke.py` 在第 0 帧按 `R`、点击 car001 映射源坐标 `(1009,600)`，单次 detector 返回框 `(994.001,581.063,35.289,39.486)`、类别 `2`、初始化 `ESTABLISHED`；再按 `R` 点击空白 `(30,60)`，回退框 `(0,28,62,64)`、类别 `UNKNOWN`，再按 `R` 后 Esc 取消且初始化次数仍为 2。输出根 `/home/neardi/Project/SOT/debug-click-0060b7b/runs/click-gui-v2/`，`run.log` SHA-256 `1692c1bcdbdcc78121047f9166e99a7ea08c4479e69a67ee78988fdf6d18eb0c`，`selected.png` / `fallback.png` SHA-256 `dbd28419af0328ab536ddc4b9387d48ea88b66980616265ac403a302a4b04380` / `6af05baecfaecfaa0aa4dc479a5a3e46f13ace87776bedc2c26fe70c98340f48`。独立自定义配置 `runs/click-gui-custom/custom.conf` SHA-256 `82b3dc8ecd08191cf6c226b635e9b205189357673c9df8e8e481693e8a553206` 增加 `click_fallback_bbox_size=80x96`，同法点击空白 `(180,150)` 得到 `(140,102,80,96)`、类别 UNKNOWN，`click-gui-custom.log` SHA-256 `81d3b65d16853ad85cf8a6f3219f8347925f9333e3739b812b8da1c390a4d63a`。首次误用历史 `rk3588-interactive-debug.conf` 因已弃用 controller 键启动失败，失败日志保留于 `runs/click-gui/`，后续全部改用上述已验证配置。
- 仍在同板端执行 `./build/nanotrack_v3 --config /home/neardi/Project/SOT/dual-gt-ui-13bd45d/runs/headless-eval.conf --bbox <各目标初始框> --output-size 640x360 --output-video ... --output-boxes ... --session-metadata ...`：car001 `(992,582,37,39)` / car002 `(967,513,42,42)` 各 271 帧，CSV 板端 `runs/regression-car001/frames.csv` / `runs/regression-car002/frames.csv` SHA-256 `f0c6da8b6c2d497de23b33faa73b76fe7750647218b079babb5e3ce34787ec09` / `4ad251cc6b7b602c590a7a79fc91adae3dfbe0d0b8bd3a8a80624eb003ca97af`；主机副本同哈希。主机 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root /home/u20/single-object-tracking/data/occ_video_1 --run car_001 nominal 992,582,37,39 /tmp/nt-click-car001.csv --run car_002 truth 967,513,42,42 /tmp/nt-click-car002.csv --output-json /tmp/nt-click-two-target-score.json`，两目标均 PASS，重捕帧 `188` / `233`，tracking 平均中心误差 `4.15` / `1.63px`，报告 SHA-256 `af822aecf7df46484dcb22c3b7633520eeaa99dcae396e524a2a0f486237dd40`。与旧异步运行逐字段有差异，不声称逐帧字节一致；本次两目标真值门均通过。主机最终 CTest 23/23，日志 `/tmp/nt-click-final-ctest.log` SHA-256 `4bcf36e7dc4b3a51f896c2a53c1392a6964f29c429f2a7e5ba270b78959de230`。
- **边界 / Handoff**：以上仅覆盖单段视频、Xvfb 模拟输入和两组原有初始化框；实际人手操作和吊舱验收未完成。通用运行时变更须在 main 与 Debug 同步且核对源码一致，Debug 独有点击交互；正式 `latest_handoff.available=false` 仍由协调者裁决。

## 2026-11-14T22:41:59+08:00 Debug 单击选目标源码及主机测试

- 分支/工作树 `codex/debug-click-select` / `/home/u20/single-object-tracking/.worktrees/debug-click-select`，基线 `Debug@1573b20c9aef2b8747752bf5467b5ac4a0261136`，共用运行时提交 `78665da675a8ca05e46a96dcfa49dd697299fe26`。Debug 按 `R` 暂停后在 Display 单击，检测当前帧一次；命中任何有效类别时取裁剪后的 detector bbox 与类别，重叠按置信度、面积、原顺序决胜。未命中、detector 不可用或报错时，用点击位置为中心的固定框并强制目标类别未知；错误写 Debug 事件。单击显示图先缩放到 1280×720 并居中，点击坐标映射回源帧，图像留白点击忽略。`click_fallback_bbox_size=WxH` 配置默认 64x64，CLI `--bbox` 维持原语义。原 RK3588 固定配置文件有哈希保护，未改其字节。
- 实际主机 `cmake --build /tmp/nt-click-common-build --parallel 4` 和 `ctest --test-dir /tmp/nt-click-common-build --output-on-failure` 23/23 PASS，覆盖 bbox 命中、重叠选择、空白/边缘框、配置值和类别模式；CTest 日志 `/tmp/nt-click-full-ctest.log` SHA-256 `ffcf9566b18d1aeaf3d25fc00b03d0dfbcee12a6c078945c5cb0e706ef28cec3`。从 RK3588 已有开发目录读取 `rknn_api.h` 至主机 `/tmp/nt-click-sdk/include/` 后，`g++ -std=c++17 -fsyntax-only -Wall -Wextra -Wpedantic -I/tmp/nt-click-sdk/include -Irk3566/nanotrack_v3/include $(pkg-config --cflags opencv4) rk3566/nanotrack_v3/src/main.cpp` exit 0。该验证只覆盖主机语法与 RKNN-off 单测；下一步提交后到 RK3588 原生构建、GUI 单击与回归实测。

## 2026-11-14T22:40:22+08:00 Debug 单击选目标：共用初始化契约

- 新功能分支/工作树 `codex/debug-click-select` / `/home/u20/single-object-tracking/.worktrees/debug-click-select`，基线 `Debug@1573b20c9aef2b8747752bf5467b5ac4a0261136`。通用 `LongTermRuntime::initialize` 增加默认兼容的类别选择参数：现有配置、复用已选 detector 候选、强制未知三种模式。点击候选复用单次检测 batch、覆盖既有固定类别，空白/错误强制未知，不再二次提交 detector；CLI `--bbox` 保持原语义。此通用源码将在 main 和 Debug 保持相同。
- 主机实际运行 `cmake -S rk3566/nanotrack_v3 -B /tmp/nt-click-common-build -DENABLE_RKNN_RUNTIME=OFF -DBUILD_TESTING=ON`、`cmake --build /tmp/nt-click-common-build --parallel 4`、`ctest --test-dir /tmp/nt-click-common-build --output-on-failure`，23/23 PASS；配置日志 `/tmp/nt-click-common-cmake.log` SHA-256 `d3ff64b86e348b57df7563847ba4882672998fcc0f5b7ffa1e703ed21aafe066`，构建日志 `/tmp/nt-click-full-build.log` SHA-256 `200c73c0d3cea6f513c7de4a8bb7e0816c7ac642d74b3dc3e09794fd21ef62b2`。此处仅是主机 RKNN-off 单测；下一步完成 Debug 图形交互与真实 RK3588 原生构建、点击实测。

## 2026-11-14T22:19:00+08:00 main / Debug 算法一致性核对与双分支发布

- 算法候选已经合入 `main@05556e660f22eabc112088a27ca3c05e8958d3ab`、`Debug@0f6b56f849324a64991a7f63a6d12d9975d5c923`（后续仅可能追加文档同步提交）。`git diff --name-only main Debug -- rk3566/nanotrack_v3 scripts tests` 只列出 `src/main.cpp`、`src/interactive_debug_overlay.cpp`、`tests/test_interactive_debug_overlay.cpp`；排除这三个交互文件后，NanoTrack 所有其他源码、配置、脚本和测试完全相同。逐段比较两边 `main.cpp` 的 `lower_occluding_band` 与 `runtime.process_frame` 调用也完全相同；其余差异是双窗口、显示框、关键 detector 调试框和交互视频输出。
- Debug 工作树 `/home/u20/.codex/worktrees/integrate-adaptive-debug/single-object-tracking` 实际执行 `cmake -S rk3566/nanotrack_v3 -B /tmp/nt-dual-gt-debug-integration-build -DENABLE_RKNN_RUNTIME=OFF -DBUILD_TESTING=ON`、`cmake --build /tmp/nt-dual-gt-debug-integration-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-debug-integration-build --output-on-failure`，CTest `23/23 PASS`；日志 `/tmp/nt-dual-gt-evidence/debug-integration-{cmake,build,ctest}.log` SHA-256 分别为 `357c88221eae7ac90b23e3fed92af3568d02f51f5ffda4ce90922a0a5fd266e2`、`bb435edecc07774a3a82c36c8880df250b18eee312b0f7e9dfcd41923639c5ee`、`14db662e6f8d19cd527321d466eafba8f1acbb114eb0b6c25a6acfa2210f05f9`。系统 Python 3.10 对两个分支运行 `python3 -m unittest discover -s tests -p 'test_evaluate_occ_initialization_consistency.py'`，各 `2/2 PASS`；旧 NanoTrack venv Python 不支持评测脚本所用的 `math.dist`，故此项用系统 Python 验证。
- 远端起点 `main@b802265`、`Debug@7e45f9d` 经 `git ls-remote` 核对后，已执行 `git push --atomic origin main Debug`，首次双分支推送成功。本次不包含新板端源码编译或真实吊舱测试；继续沿用上方绑定的 RK3588 证据边界。后续每次合并应再次检查 NanoTrack 非交互源码零差异，并人工审查 `main.cpp` 的交互差异。

## 2026-11-14T22:14:24+08:00 双真值候选向 main / Debug 集成

- 本次集成从 `main@b80226577954febe22c5eff168125f77a6cdc724` 和 `Debug@7e45f9db9493c8411139dba76294fdd179eb2759` 出发，接收候选 `codex/dual-gt-init-consistency@291e62d1cb4211cf32b2dfc160ade78305774044`；main 集成工作树 `/home/u20/single-object-tracking/.worktrees/dual-gt-main-integration`。main 保留原交互界面，Debug 保留双窗口及只显示关键 detector 框的交互界面；两个目标分支的跟踪、重捕、控制、评测源码必须相同，`main.cpp` 的算法路径也须逐项核对。
- main 实际执行 `cmake -S rk3566/nanotrack_v3 -B /tmp/nt-dual-gt-main-integration-build -DENABLE_RKNN_RUNTIME=OFF -DBUILD_TESTING=ON`、`cmake --build /tmp/nt-dual-gt-main-integration-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-main-integration-build --output-on-failure`，CTest `23/23 PASS`。构建与测试日志位于 `/tmp/nt-dual-gt-evidence/main-integration-{cmake,build,ctest}.log`，SHA-256 分别为 `3a5526a494fd963a7168f3f738c747d491745860332847d7a7c46cc06dcb575c`、`9fbc5d1363f1fcf30689d9a18ebc9220c550ffd596b4cd61bc667b8fdf30c45a`、`f5fc0363db4452de74559e7327ae55c7f38c87f2aa926267e2480a5e3fdd8046`。
- 此处是主机侧集成证据，RK3588 八框 `8/8` 与 GUI 证据仍绑定上方精确板端源码与二进制；新分支提交未经重新板端运行，不升级为生产或吊舱验收。下一步合并 Debug 并核查两个分支算法源码差异和测试结果，向协调者回传最终 commit；`latest_handoff` 仍由协调者裁决。

## 2026-11-14T22:00:30+08:00 双窗口 Debug 补齐后的 RK3588 验证

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，源码基线 `3a87a4264134172f55670fa6ecd64f0759050928`，双窗口与关键 detector 框移植提交 `13bd45dad7589a039b50fa54b49aa3ca30c1628e`；`git diff --name-only 4b4e47a..13bd45d -- rk3566/nanotrack_v3` 仅列 UI 源文件 `src/main.cpp`、`src/interactive_debug_overlay.cpp` 与其测试，追踪核心/配置未变。主机 `/tmp/nt-dual-gt-evidence/dual-window-13bd45d-source.tar` 与板端隔离根 `/home/neardi/Project/SOT/dual-gt-ui-13bd45d/source.tar` SHA-256 同为 `813a311a3ed5a4b14c014991c235605aecb69b422b29bf7fe7f49572306df62f`；构建脚本 `/tmp/nt-dual-gt-evidence/build_dual_window_board.sh` SHA-256 `284e6bb40e915a94b37f9230fd31c8179b271ca04b4c62d9a03048da82327075`，传输后板端核对一致。
- 实际 RK3588 原生 `bash build_dual_window_board.sh`、CTest `23/23 PASS`，`build/nanotrack_v3` SHA-256 `32c9a88c1319ffb74e96f9978f140d68a02e6cfe407b579d3db5ef38b1ce1a2a`，`runs/headless-eval.conf` / controller 配置 SHA-256 `55f4ffb4f196adddb4505f878874d97348aec75613a5a8c0b03fc04701e326f0` / `52a3722eec8ed32c2f3ba44be50617ff6c16ad12254f04320b9802738f406bd6`，`runs/cmake.log` / `build.log` / `ctest.log` SHA-256 `90d23309e97f029a7e2e7447774b2239fdeaa605d2592b1dd37bfac28feddbad` / `40b52584d7b5d2f8273a8a8f4e187819b42c1be655ec0025179fe464b91d06c2` / `d1d8383770c80da69ebd1d840289383cbffb0a563123e8598bb731118f0e1a84`。LKD3588 Linux `6.1.84`、session 实报 RKNN API `2.1.0` / driver `0.9.8`，视频、tracker、detector、manifest、labels 与下方 `4b4e47a` 已核对身份和绝对路径一致。主机 RKNN-off CTest 也 `23/23 PASS`，日志哈希见紧随的候选记录。
- GUI 验证先以 `bash smoke_dual_window_board.sh` 在 `xvfb-run` 中实际发现 `NanoTrack Display` 和 `NanoTrack Debug` 两个 `1280×720` 窗口，位置分别 `(0,0)` / `(1296,0)`；窗口清单板端 `runs/gui-smoke/windows.txt` 与主机打包副本 SHA-256 `3d47c75f2218958c84b5274af51d13b0dc99b9d81541524562fbef99fd78bba4`。首次内容抓取因板端无 `xwd` 失败，保留该失败日志；改用板端现有 `scrot`，实际 `bash capture_dual_window_scrot_board.sh` 等待 `INITIALIZED frame=0` 后抓屏成功，脚本 SHA-256 `8355a4306837de9d28be01988c7e22fbee642f48fda32860fd89b9a179f5f28a`。板端 `runs/gui-content-scrot/desktop.png` 与主机 `/tmp/nt-dual-gt-evidence/dual-window-13bd45d/desktop.png` SHA-256 同为 `73cae1942edb0e6bb52ee6f7ffb7a8eb4312e1d6982af0ef2bd4ec6ea0646c3d`：首帧左侧 Display 只有白色 `display_bbox`，右侧 Debug 的目标相关框/状态单独呈现；逐帧未入队或被拒绝的 detector 候选不绘制由板端 `test_interactive_debug_overlay` 覆盖。该截图是虚拟桌面首帧，不冒充人工真实显示器验收。
- 回放脚本 `run_dual_window_four.sh` / `run_dual_window_extra_four.sh` SHA-256 `9a4972850781db9badd3df768c169f8c7e4322cf7c8419c61bcc97d489492344` / `dfe5c0f69cf36467a5921bd08c632190e90579306c9f2adac12d5fdee8932635`，实际在新目录顺序运行两目标各四初始框，每组 exit 0、CSV 含 `0～270` 共 271 帧。板端 `runs/evidence-text.tgz` 与主机 `/tmp/nt-dual-gt-evidence/dual-window-13bd45d-evidence-text.tgz` SHA-256 同为 `534820a750d4792511f95a8381a39e71eb78b77e884273869335012acea95b81`，原件板端 `runs/<label>/`。用两份确认真值执行 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ... --output-json /tmp/nt-dual-gt-evidence/dual-window-13bd45d/eight_score.json`，报告 SHA-256 `3cc2c1da5c1ca41dd1f0b98a9e67f692f06b7996f41531285d15dbca64a7e7f4`：**8/8 PASS**，car001 重捕 `188/188/189/186`，car002 `232/232/236/232`；tracking/display 中心均误差与前项相同。逐帧比较本轮与前项八组 CSV 的 state、有效性、reacquired、tracking/display/control 四元框，八组各 `271` 帧均 `0` 个不一致。CSV 文件整体哈希受时延字段影响并不相同，不能直接用文件哈希表示几何一致。
- **Handoff / 边界**：用户指出的两项 Debug UI 已在最新板端实验二进制生效，正确图形运行根为 `/home/neardi/Project/SOT/dual-gt-ui-13bd45d`，旧 `/home/neardi/Project/SOT/dual-gt-wide-band-4b4e47a` 二进制不包含它们。建议在板端图形终端运行 `cd /home/neardi/Project/SOT/dual-gt-ui-13bd45d && mkdir -p runs/manual_car001_ui && DISPLAY=:0 ./build/nanotrack_v3 --config runs/headless-eval.conf --interactive --bbox 992,582,37,39 --output-video runs/manual_car001_ui/output.avi --output-boxes runs/manual_car001_ui/frames.csv --session-metadata runs/manual_car001_ui/session.json` 核对 car001，car002 可改 `--bbox 970,517,38,38`。GUI 烟测与 canonical 单段回放不能证明真实吊舱或跨场景接受；协调者正式 `latest_handoff.available=false` 未改。

## 2026-11-14T21:44:28+08:00 双真值候选补齐双窗口与关键检测框 Debug 界面

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，起始干净，基线 `3a87a4264134172f55670fa6ecd64f0759050928`。用户指出当前板端实验二进制漏掉前一天已实现的两项 Debug UI。核对发现本分支从 `main@b80226577954febe22c5eff168125f77a6cdc724` 延伸，源分支 `Debug@7e45f9db9493c8411139dba76294fdd179eb2759` 的双窗口/候选筛选未进入该 `main` 基线。精确移植原 UI 源码提交 `ef7dba9e9fe0028850979edcdd5fa06acf06cdb8` 的三文件差异，未改本分支 tracker、controller、恢复配置及两套 GT。
- 新交互界面用 `NanoTrack Display` 单独显示仅一只白色 `display_bbox`，`NanoTrack Debug` 显示运动、模板、控制和关键恢复证据；初始化只画被选 detector 框，逐帧原始 detector 候选只保留类别匹配、位置通过、无拒绝且已入队者，不再绘制所有拒绝/未入队框。ROI 选择仍在 Display 窗口，关闭任一窗口退出。实际 `git show --format= ef7dba9 -- <三文件> | git apply --check`、`git apply`、`git diff --check` 通过；主机 RKNN-off `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4` 与 `ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure` 为 `23/23 PASS`，日志 `/tmp/nt-dual-gt-evidence/dual-window-host-build.log` / `dual-window-host-ctest.log` SHA-256 `c1d8661f43ae4b1c4a7218e48b6bc193c4903ef96851b6ddb6ebc65dd01d2af7` / `ae81844ebf70c7cb84a6693fd2f704b152fb67625e1369d2d583b0437c86061d`。
- **证据边界 / 下一步**：上述只是源码与主机单测，当前已验证 `8/8` 的板端二进制 SHA-256 `f61cdf3518e11c59620ff268a8ee518837fc072c00322d0b9b16abc70d215613` 尚不包含双窗口。须提交精确源码后在新的 RK3588 隔离目录原生构建/CTest、图形会话核对两个窗口与精简 detector overlay，并确认最少两目标 headless 轨迹与前项相同；完成前不得称新 UI 已在板端生效。`latest_handoff.available=false` 不变。

## 2026-11-14T15:02:35+08:00 双目标八初始框板端通过与同框复跑

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，候选源码提交 `4b4e47ae1bf4dd5245d9e251ddc693e9057b4d89`，前项基线 `359e8c4a1b2ead6896fec77fdd337e824e7bce1b`。主机源码包 `/tmp/nt-dual-gt-evidence/wide-band-4b4e47a-source.tar` 与板端隔离根 `/home/neardi/Project/SOT/dual-gt-wide-band-4b4e47a/source.tar` SHA-256 同为 `1f897f85dbb979ffb9b72563f4ed9b910a6c3ffdbe5eec068ae89276e5102a29`；构建/前四框/后四框脚本 SHA-256 分别 `d792f9c889a5af49b1c4a4662092a467ea365d3252e8b0b71bf73e6d6024f38b` / `c1b3aa073b5931b1dabb9238deab629b42a2d5335e5102e820a4c21b11aecb40` / `c745bf819380e94caeb1fcda892f16274c9d2e5673f5bbe70aa075f7da8739b1`，板端与主机传输核对一致。
- RK3588 `LKD3588` Linux `6.1.84`，session 实报 RKNN API `2.1.0 (967d001cc8@2024-08-07T19:28:19)`、driver `0.9.8`。本轮板端复核视频 `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`，官方 NanoTrack template/search/head RKNN 与 manifest SHA-256 `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55` / `d3cb0e3879df6a1272adb59bf501324f1f8f55b58309d3c4da4a731b600973ee` / `4da379f54093d79f02775e90bfea33ebee0837c6fc73a324499279b4c57807c5` / `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`，VisDrone9 detector/manifest/labels SHA-256 `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c` / `ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9` / `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`。主机双目标 GT `data/occ_video_1/ground_truth/car_001/labels.sha256` / `car_002/labels.sha256` SHA-256 `9ec01854884126350f93e1af3a4aee002d38ce7c2d50c71c78de0f1a8dc0a648` / `2f0e78ab1123cb035c87efa06171f1057c108f38b06fcc5c1ed3a3ec63fdbe98`；GT 只在追踪后评分，不参与逐帧推理。
- 板端实际执行 `bash build_wide_band_board.sh`，原生 CMake/构建/CTest `23/23 PASS`，`build/nanotrack_v3` SHA-256 `f61cdf3518e11c59620ff268a8ee518837fc072c00322d0b9b16abc70d215613`，controller/headless 配置 SHA-256 `52a3722eec8ed32c2f3ba44be50617ff6c16ad12254f04320b9802738f406bd6` / `e3d9b1307bd2cdf0ea18e49e73798075ab6337473ef841647888cca6351b035a`；`runs/cmake.log` / `build.log` / `ctest.log` SHA-256 `7c5e596b6f8598f7a7fb2b25afb773175b4e592abbf624d1c05cd1f78af47142` / `3abdf3450dfdfd8ecd559e3ecacbbe3d8d92c9f5cd806498fa7d3c9cd763d1a6` / `24bfab022514f809d42ff5ee78f58f85cc9abaca6d776e81d26369413e632320`。实际顺序执行 `bash run_wide_band_four.sh`、`bash run_wide_band_extra_four.sh`，八组均 exit 0、每组 CSV 为帧 `0～270` 共 271 行，原件板端 `runs/<label>/`。八组文本包板端 `runs/eight-text.tgz` 与主机 `/tmp/nt-dual-gt-evidence/wide-band-eight-text.tgz` SHA-256 同为 `13d1068b963c6e88b58961cbfd5c5d9d0c8247b14f8aae1c45977af39ed6882f`；八 CSV SHA-256 按 car001 标准/人工/左/右、car002 标准/真值/左/右依次为 `9190cc9e848077ccbed36def30ac968b7571d5ae502808137753f3b350e33c62` / `a644841daa77240ea37028d4857bf7c4890c8bdbe43f18c1bb7c92955f3e8670` / `998fd4c36ba389d52b8fbc1706f9eb1c3cd3755104edf09f0ddc020a9dbe54b5` / `5bd98cc692a288b71c058c045578d14e5ca1f711c2700409b7126b55ccde5ed3` / `132165fede21500a318f4f9db8114bc08f5d696b1ed754954b5e61988b015eca` / `200c13e335879d36d2a926c3f2c2005aaada22630dae2b5e21db9afced3dbe64` / `812a5f01bb73290d98ca3231c486e1c222d08632cb3f6ed0ce251c1f594ff0c4` / `8fbf6dfd6ecc9b2ad1e3fd9c10be41fd70bc20e5e05fe2a27b22f7dd009b3111`。
- 实际 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ... --output-json /tmp/nt-dual-gt-evidence/wide-band-4b4e47a/eight_score.json`，JSON SHA-256 `853cf8429c31776acf8cc169202daaa4d7624cb2f571c3b5bc91182ad7ffd0d3`：**八框 8/8 PASS**。car001 四框重捕 `188/188/189/186`，tracking/display 可见中心平均误差均约 `4.2～4.6px`，tracking 平均 IoU `0.718～0.777`，初始化间中心分散 P95 tracking/display `3.28/2.86px`；car002 四框重捕 `232/232/236/232`，tracking/display 可见中心平均误差约 `1.6～2.4px`，tracking 平均 IoU `0.740～0.893`，初始化间中心分散 P95 两路均 `4.78px`。car002 四框第 `211～212` 帧均 `SEARCHING`、`observation_valid=0`、`control_source=MOTION_SEARCH`；第 `229` 帧均未错误重捕，人工确认无框段假观测/观测控制均为零。`display_bbox` 的 detector 仅用于显示，不替代 tracking/control GT 评分。
- 同一板端二进制/配置独立复跑 car001 右偏、car002 真值与左偏初始框，三组均 exit 0 且各 271 行，原件 `runs/repeat_<label>/`；重复文本包板端 `runs/repeat-text.tgz` 与主机 `/tmp/nt-dual-gt-evidence/wide-band-repeat-text.tgz` SHA-256 `2c90fffb1f0876ef2350ead189e2a4cdfb518789f60ad7033b2d1d8f98c3c9fa`。重复 CSV SHA-256 分别 `8f443385ded134558e71bef24fa7d948c119ffc576cf0b77603381c4d647c684` / `5f83d4b881eda77fd09fdc90b0d66ae64a3caebfe03a290c68f6619bee8ecdf8` / `5b51b5586cdac3ea6a8a276b6a8620191f6dddab5c928bbb63ea08b41a446ba9`；重新评分 `/tmp/nt-dual-gt-evidence/wide-band-4b4e47a/repeat_score.json` SHA-256 `8f3dd68c87575f49eb5faa7042878a4521678999692684b943b0e0036b7fb610`，三组均 PASS、重捕帧不变，逐帧 state/tracking/display/control 坐标及有效性字段 `0` 处不一致。CSV 整体哈希不同来自计时等非几何字段。
- **边界 / Handoff**：此结论覆盖 canonical 单段可见光视频、两份确认真值和 8 个轻微变化初始框；car001 右偏恢复过渡帧 tracking 最大中心误差 `15.84px`，car002 右偏第 `231` 帧最大 `23.50px`，不宣称逐帧零误差或跨场景/真实吊舱验收。请协调者审查源码 `4b4e47a` 与上述哈希及 `8/8` 结果后决定是否更新正式集成契约；本任务不改 `STATUS.md` 或 `integration-contract.yaml`，其中 `latest_handoff.available=false` 在协调者批准前保持不变。

## 2026-11-14T14:50:26+08:00 桥线几何和恢复纹理门二次候选

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，基线 `359e8c4a1b2ead6896fec77fdd337e824e7bce1b`；代码将桥下缘检测范围改为相对框高 `1.5～1.85`，并要求同帧相对高度 `0.3～0.8` 没有另一条跨目标上缘长线，避免 car002 第 209～210 帧仍可见时过早拒绝；恢复候选顶部亮度标准差下限 `26→32`，用于推迟 car002 真值框第 229 帧不完整目标的确认。仍只在桥线与近期框亮度下降同时成立时拒绝观测；`display_bbox` 的 detector 用途不变。当前仅为候选，尚无本次代码板端闭环证据。
- 实际 `.venvs/nanotrack/bin/python /tmp/nt-dual-gt-evidence/screen_wide_lower_band.py` 在上轮冻结轨迹和 canonical 视频上离线筛查，脚本 SHA-256 `085fe943ad06d20b214c0362cb1338b3eb2bc8b30045fbe2316dbc87ab8caaba`：car002 四框第 211 帧均命中，标准/左/右第 212 帧也命中；car002 第 209～210 帧均未命中；car001 命中帧集中于自身遮挡开始后。此筛查不能证明闭环。主机 RKNN-off `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4` 与 `ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure` 为 `23/23 PASS`，日志 SHA-256 `0d657575e05e39ed9471e0eda822a34122bf6e5970df5ec204c1ff0fe6ec3760` / `5e39ade88fb5cf00f2e8adabea410b47ca94d43fbda1d8ed121e2ca5342d5d02`；`python3 -m unittest discover -s tests -p test_evaluate_occ_initialization_consistency.py` 为 `2/2 PASS`，日志 SHA-256 `a6707d4eff38012da33ddc34bc6dc076a3b17151cfc9aef223c8790cbe64c3da`，`git diff --check` 通过。**下一步 / Handoff**：提交后在 RK3588 新隔离目录原生构建/CTest 并重跑两目标八框 271 帧，任一遮挡或中心/IoU 门失败则否决；`latest_handoff.available=false`。

## 2026-11-14T14:47:57+08:00 桥下缘候选八框 6/8 否决

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，基线 `65ffde946cc7bcafeb1deb97a08f85ad590853f3`，源码 `0a6b19d66181d5a9dd5ac98a946725853dcd6a7c`；源码包主机 `/tmp/nt-dual-gt-evidence/lower-band-0a6b19d-source.tar` 与板端 `/home/neardi/Project/SOT/dual-gt-lower-band-0a6b19d/source.tar` SHA-256 `75c0c84a349f45a2e4c38aaeada55a86d22454670d5a17acdeae41f5e909fd33`。板端原生构建 `bash build_lower_band_board.sh` 和 CTest `23/23 PASS`，二进制 `build/nanotrack_v3` SHA-256 `09cd98dd74b88cfef06e10582287ae071d1c4e4c6eff43d86e7e1d0b391ecd4b`；controller/headless 配置 SHA-256 `4ca3d4b3bc4282fc7a5b2b642b2a19a24887b425ba06adfc1c2b5b93fe818db3` / `5404a6ff66aaa5871c13d2da011e9fa6c2ef118da1132a7037fde01e8afa57c2`。构建/前四框/后四框脚本 SHA-256 `3ea8a84245f8086b30c3e4c80f10e4adc1778f2077f4ef65981d52ef979afb91` / `316260a54361f03ee7cef12f6623e758898629fa444d3642145edffc1db291c4` / `a4975d1dd1ad447ffc3452c73baf1f59a7b2e64ceb16771df902f5e032e689ae`，`runs/cmake.log` / `build.log` / `ctest.log` SHA-256 `3762296afd7bf73611304630661939f74cab62d9b43c26fd3163a5e17a5c5867` / `a10b076f27075cf9a6e3dc1579ede8324ec74e0849136d39e8e2066b6a2e1589` / `8e97da94f5578706dbfe8875122bcf9e8f96d1f5429b6120e298d92c004bef31`。LKD3588 Linux/RKNN 与 canonical 视频、模型、清单、标签哈希沿用下方 `452c48a` 已核对身份。
- 实际运行 `bash run_lower_band_four.sh` 与 `bash run_lower_band_extra_four.sh`，每组 271 帧 exit 0；原件板端 `runs/<label>/`，八组文本包板端 `runs/eight-text.tgz` 与主机 `/tmp/nt-dual-gt-evidence/lower-band-eight-text.tgz` SHA-256 同为 `b955be527829f8b27eb4859fd3b59ea8812b1106c59bdd08f231e88a6cb86afd`，八 CSV SHA-256 按 car001 标准/人工/左/右、car002 标准/真值/左/右依次为 `a524b86801f98c20a9636b0c9d23dc5a146188325b2c621373177c313762d0d9` / `753499e5ca72a2b029e377d93180b40d57db0f2af90543ab19e458d1fc84a72e` / `b938f63994deecce7b825a62dc67107d1c24b4266a05f638210f8df3e3a5ee25` / `691834845762c2e04e1444ebbe447f3848bd96cf2e72913cecd4ac7f1720c27f` / `68663a3c45bd47e081a3c02e56a4928c7801eb5c17e30ab63a598d6dbf519c06` / `8fbc140167c6097e412a682f05759c33ce11b5f86d5475469c590acdaa4f85fa` / `355f60f00ce869edb54dc9f437e5ec7d3147bd05e193e90fed60ac596ce90937` / `9fe76b742c40906d355550fb37ce7b38092b42444057f0f4baa9f45929ddcddc`。
- 实际 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ... --output-json /tmp/nt-dual-gt-evidence/lower-band-0a6b19d/eight_score.json`，JSON SHA-256 `3df1d7c0fae0c414ab48df686ea2ee1cb48ac81febfe07a5ec8ebd7b740fccbe`。car001 四框 PASS，重捕 `187/187/189/186`，可观测 tracking 中心平均误差 `4.09/4.14/4.24/4.51px`，初始化间中心分散 P95 `3.13px`；car002 标准/右偏 PASS，真值框第 229 帧在人工无框期提前重捕，左偏第 211～212 帧疑似观测送控制，故总体 `6/8 PASS`。car002 初始化间中心分散 P95 `5.12px`，通过的轨迹中心误差约 `1.6～2.5px`；这不能抵消两个遮挡门失败。
- 离线帧探针 `/tmp/nt-dual-gt-evidence/probe_current_lower_band.py` SHA-256 `21095f9a825792ea93ee8bc16700edf3987798e6f65f7cb1e37f75ec6d978d66` 指向第 211 帧桥下缘纵向位置 `1.57～1.82` 倍框高，当前窄范围在不同初始框/板端轨迹上漏检；`/tmp/nt-dual-gt-evidence/probe_current_recovery_texture.py` SHA-256 `28629e05b107291b9cc477ba60e5a5490c9ae92b4188be8145290869ab1a969b` 指向真值框第 229 帧候选顶部亮度标准差 `31.28`，当前阈值 `26` 放行。探针只用于定位，不能充当闭环通过证据。**Handoff**：候选否决，收紧几何/恢复门后重新板端八框；正式 handoff 仍 `latest_handoff.available=false`。

## 2026-11-14T14:35:45+08:00 八框 7/8、car002 左偏无框段与桥下缘候选

- 前项精确源码 `codex/dual-gt-init-consistency@65ffde946cc7bcafeb1deb97a08f85ad590853f3`，源码包 `/tmp/nt-dual-gt-evidence/focus-texture-65ffde9-source.tar` 与板端隔离根 `/home/neardi/Project/SOT/dual-gt-focus-texture-65ffde9/source.tar` SHA-256 同为 `8f412cda4573f2c0fc16e68ebadd6446209fc3a753d21e29863728658c039e29`；构建/前四框/后四框脚本 SHA-256 `088bcc3a403f0559b984a29b2d42dd9d8c679a0f92826a13df5c3447274cb25b` / `29e9c6387532e9e0de32b42e02376651619b39c5b1f35347ea198b4843f77f77` / `5353547faff99dfd71abc4e53efe642abc95b9609b223b4f6fb8a741ad51256b`，板端传输核对一致。板 `LKD3588` Linux `6.1.84`、RKNN API/driver `2.1.0` / `0.9.8`，canonical 视频及 tracker/detector/manifest/labels 精确哈希沿用下方 `452c48a` 已复核值。板端 controller SHA-256 `274bc592b92db7480fcdb8e9a056a397f6c7b44f8c585dfb4cec0a1dc7d69b93`，`runs/headless-eval.conf` SHA-256 `415f0b8354fbfaa501049b4e9243ffffd7cac1f82cda4af3d1535d9d2c818a1c`。实际 `bash build_focus_texture_board.sh` 原生构建/CTest `23/23 PASS`，二进制 `build/nanotrack_v3` SHA-256 `5fe98db67ed7bcb2d76d31f1a6431cc7beb85aa7cf46a060a96a977892f3c5`；`runs/cmake.log` / `build.log` / `ctest.log` SHA-256 `152be2ab8c466a9fbdcd02276062567ae6ed7bb73b903da7ad43e885b3b3a091` / `c167b7fc7c9f9106e5254e6b674c0587ca365c3c4bd3fdb819c988a5cd485a6d` / `a5b6dd013e94b105de7b03e6e221d8c52798feaabf1dbf2ad3653978acbfe4dc`。
- 实际顺序运行 `bash run_focus_texture_four.sh` 与 `bash run_focus_texture_extra_four.sh`，两目标各四个前项相同 ROI 均 exit 0、各 271 帧，CSV/session/log 原件板端 `runs/<label>/`。八 CSV SHA-256 按 car001 标准/人工/左/右、car002 标准/真值/左/右为 `6276e3925ba9e11399e8139d375015bd3acfe0a739ac5a8d8346b690b87fc1db` / `60d57b21362a9958197e3a9920a3eef31924f563866bcaa1d2ec403a8d58e11b` / `0c4e77c606fb60e1d4dcf7bb24ecffe13413504ed933e9a029523bbd3d825a5f` / `c44bb0eb598b8be45ca7f41c9f24604eab6013c48a80db1103da56afb1635574` / `a8e990f43561135b23970c484a421a06070a87bbbb26911a7220ef84f6bb6c32` / `ae6c66f17e1a91d0ca96c96ac423080ae604b78786b8bc281d07d4288163ecb3` / `46af27b3d28363f2f72375705c5275f1aab9a8ee0289b044ebf5e8205a820a00` / `827d38be6890b7951d90b9384d4e96bf09c952356d9193a877d30b861a616ddc`。板端 `runs/eight-text.tgz` 与主机 `/tmp/nt-dual-gt-evidence/focus-texture-eight-text.tgz` SHA-256 同为 `683bf1de9b6ab629f22dc4833cd6fb8bdf23edbe610154aca6eb630fd2fbd695`，主机解包后八 CSV 哈希一致。
- 实际 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ... --output-json /tmp/nt-dual-gt-evidence/focus-texture-65ffde9/eight_score.json`，报告 SHA-256 `01575674ae599aaca34b5d656f7ceca040c898a74858abf205e2f018a71d2483`：car001 四框均 PASS，重捕 `187/187/189/186`，tracking 可见中心约 `4.09/4.14/4.24/4.51px`，四框中心分散 P95 `3.13px`；car002 标准/真值/右偏 PASS，重捕 `231/230/231`，tracking 可见中心约 `2.12/1.56/2.50px`；car002 左偏 FAIL，第 `229` 帧在确认无框期错误重捕 1 帧，另第 `211～212` 帧将疑似观测送控制。故总体 `7/8 PASS`，不能晋级。
- 用同一板端二进制在隔离 `runs/threshold26/` 只把 `recovery_min_top_luma_stddev=23→26`，controller/headless SHA-256 `51d45e5a42a18d23701f04ea404d68336201fde3b47c8b822cade3f048273eb9` / `a20626f1562f196367401a3997c9417941d154fa83856df43dcd806eae4effc7`，实际完整运行 car002 左偏 271 帧 exit 0，CSV/session/log SHA-256 `49b695b8001c7f9bb7fbc3d22591f21e08751506c88d1a7e92116fb97feb3a82` / `7e6c0712b229039351717d2c68889a55efe8acba80eec83e21d533324c5505f0` / `5e53c369692e3a4b2f535bbbe7210f6e4e6dd0047ed8a9a4f8e477d1287a0d1408`。主机 CSV `/tmp/nt-dual-gt-evidence/car002-left-threshold26.csv` SHA-256 一致；同一评分脚本报告 `/tmp/nt-dual-gt-evidence/car002-left-threshold26-score.json` SHA-256 `af79bc6a5ba2c43972f68d9951b15c892fd2a335fb12a4828ca98562f4f86ff2`：第 `236` 帧重捕、无确认无框期恢复观测，但 `211～212` 疑似观测控制仍 FAIL。此为配置筛查，未宣称完整八框改善。
- 当前候选新增默认关闭的 `lower_band_visibility_veto`：主视频帧上以当前跟踪框附近 OpenCV Canny/Hough 识别跨过目标水平位置、长度至少四倍框宽、距框顶 `1.3～1.6` 倍框高的桥下缘；仅在同帧 NanoTrack 近期框亮像素占比低于最近可信框中位数 `0.9` 倍时立即拒绝当前帧观测并进入 SEARCHING。对上一轮八框冻结轨迹离线核对，car002 第 `211` 帧四框均满足长线与亮度下降，正常可见 car001 第 `153/155` 帧相似长线亮度约为历史 `0.97～0.99` 而不命中；car002 第 `69～70` 帧护栏线落在框内而非下缘。离线筛查脚本 `/tmp/nt-dual-gt-evidence/probe_crossing_line_focus.py`/TSV 及单独亮度计算未绑定独立场景，**不能证明当前闭环轨迹**。实验 controller 配置 SHA-256 `4ca3d4b3bc4282fc7a5b2b642b2a19a24887b425ba06adfc1c2b5b93fe818db3`，同时采用阈值 `26`；主机 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure` 为 RKNN-off `23/23 PASS`，构建/CTest 日志 SHA-256 `318b217a351bcce9715f775f282cb53f828512bf529fa5db43979635faca52c3` / `1a1882a313062e4b473e7fc6828c9021ed7f650f4d9ff73f929c805b1fb7358e`，Python 评分器回归 `24/24 PASS`，`git diff --check` 通过。**下一步 / Handoff**：同提交后 RK3588 新隔离目录原生构建/CTest、八框完整 271 帧闭环评分；若任一无框、重捕、两路中心/IoU 或跨初始化一致性门失败即否决。正式 `9f84efe` 八框仍 `2/8 PASS`，`latest_handoff.available=false`。

## 2026-11-14T14:14:21+08:00 车体形状修正八框 6/8 与恢复纹理门候选

- 上一实验精确源码 `codex/dual-gt-init-consistency@3437f78afb66dc3667f428d3fc071468b1e007e1`，主机 `/tmp/nt-dual-gt-evidence/shape-prior-3437f78-source.tar` 与 RK3588 隔离根 `/home/neardi/Project/SOT/dual-gt-shape-prior-3437f78/source.tar` SHA-256 同为 `b49e7c46a16d8f52711dc144fcc24381f773fb2f55f7b5d0ecae43679af070fe`。构建/首四框/后四框脚本 SHA-256 `1fc4659b8793b7cc5458ab1aebe6fedb818cbb08d3521843923ff99eb0582f62` / `c79fdd46da488ed9d75cf15dbea59021651766d50c71e139c4ecdf1a59b97781` / `554a3bf44df7c30dc901309c07750ba4d0515bda684bc46fda0263464ad2552c`，板端传输一致。板 `LKD3588` Linux `6.1.84`、RKNN API/driver `2.1.0` / `0.9.8`；canonical 视频和 tracker/detector/manifest/labels 哈希沿用下方 `452c48a` 已复核值。实验 controller SHA-256 `94da012f13b141184f54262f6b359704100051af9064a8cceb35321cf42b38dd`，`runs/headless-eval.conf` SHA-256 `e6e0d5fda5b7057c3f9638e0d13ac3c5ff0a1f94794bd066d6f3c539cf82e358`。原生 `bash build_shape_prior_board.sh`/CTest `23/23 PASS`，`build/nanotrack_v3` SHA-256 `c3ede4554e02ae9a08fbb12a91f85f587cbc188bb5abc6070654fe398654bb7f`；`runs/cmake.log` / `build.log` / `ctest.log` SHA-256 `72b41edcbbad40df6f569a91886dcb570c235d401292cf3c2da0ff8c60d50120` / `f0203ddc9b9f14adf960b7326788e042ecf72e35a73eaf3b0eb5af86a37275e3` / `72579111aa1a209ac8a2aaa34b5b268f554aca742f1d9696dc146a84a853f67a`。
- 实际 `bash run_shape_prior_four.sh` 和 `bash run_shape_prior_extra_four.sh` 跑 car001 标准 `(992,582,37,39)`、人工 `(993,585,36,36)`、左偏 `(990,580,39,41)`、右偏 `(994,584,35,37)`，car002 标准 `(970,517,38,38)`、真值 `(967,513,42,42)`、左偏 `(968,515,40,40)`、右偏 `(972,519,36,36)`；八组各 271 帧、exit 0，CSV/session/log 原件板端 `runs/<label>/`。八 CSV SHA-256 顺序为 `0a3ebc0a58386d7b2400246edaace0506cb770185b38c7e692d796cef05c0faf` / `41578f17ae5498cd7037a7529075b100e4bced87bdcc93f7f5f373574bb89dbf` / `7d0d9112bce5f5e25e6ab668f4caa04598e7ff1617be138dfd6eaebcf81892b7` / `3b0f350ca579f199738e8ebe6d8e53f4a4b73ac5afd6591c1a74b207d655def1` / `9d27539618a4c143c03be714385c6dabad572d20e8306cfc3da7997cfa07ec8e` / `2454249bada432f02d1d06a9968a469ae432d29c43826aab6f9268d697945e54` / `e921e4342be913c26166b3253e0c7dc4e8ccdbe5b1974a3d25c643ca3e3ac955` / `c071c078d94a40c8bbd7bb457f0aba5cd96b7a87e1ee95e34a2cbdb581fd58e1`。首/后四组文本包板端 `runs/four-text.tgz` / `extra-four-text.tgz` 与主机 `/tmp/nt-dual-gt-evidence/shape-prior-four-text.tgz` / `shape-prior-extra-four-text.tgz` SHA-256 分别同为 `c792b5f25cb8489d0aced8e840e367be3ec25386d17ba1b7da8df24aa21adc2f` / `994b2fd22d945b3c291f5fbe34383dc99a4ad80b0f2038d3645afb236256f73a`；主机解包八 CSV 哈希一致。
- 实际 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ... --output-json /tmp/nt-dual-gt-evidence/shape-prior-3437f78/eight_score.json` 对各自确认 GT 评分，报告 SHA-256 `31452879f9f3177c507bc1e8a9118976f2d0485a993f64db16a53bced29d0d30`。car001 标准/人工/右偏 PASS，重捕 `187/189/186`，tracking 中心 `4.09/4.39/4.51px`，三框相互 P95 中心分散约 `0.92～1.23px`；左偏 FAIL，邻车身份冲突持续、无重捕，tracking 中心 `27.01px`，四框总分散 P95 `144.27px`。car002 标准/真值/右偏 PASS，重捕均 `230`，tracking 中心 `1.90/1.56/2.27px`；左偏 FAIL，第 `228` 帧过早重捕且第 `228～229` 帧在确认无框期输出观测，尽管可见段中心 `1.62px`。总体 `6/8 PASS`；形状先验确实修复 car001 右偏框的完整尺寸与跨框一致性，但未达八框目标。
- 新实验针对两处剩余失败：当最邻 detector 残差不超过位置门 `0.35` 倍且次邻超过该位置门时也允许先 NanoTrack 验证，以覆盖 car001 左偏预测中心距目标约 `12px`、邻车约 `47px` 的竞争，但邻车若也由 NanoTrack 三帧确认仍保留冲突否决；另在 SEARCHING 且已有恢复 probe 时，若最近模板原始框顶部四分之一亮度标准差 `<23.0`，跳过本帧恢复观测，保留追踪原始结果和 `RECOVERY_TOP_TEXTURE_VETO` 诊断。离线从上述八组固定轨迹查得 car002 左偏第 `226/228` 帧顶部标准差约 `17.4/22.8`，其他已通过组的实际三帧确认段多为 `24～53`；此阈值来自同一视频、未经独立校准，必须板端闭环检验。开关默认关闭，实验配置 SHA-256 `274bc592b92db7480fcdb8e9a056a397f6c7b44f8c585dfb4cec0a1dc7d69b93`；主机 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure` 为 RKNN-off `23/23 PASS`，日志 SHA-256 `f56a6b3712a16c888ccb1bdd7e3ea1c9857e24f6095ff2e7f0978b53d8d2dd03` / `551ca5a1ebb75d665f257fafcd7a5ba4c3bee9f5b6f42f67d02216a4989203cb`；`git diff --check` 通过。**下一步 / Handoff**：精确提交后 RK3588 新隔离目录原生构建/CTest、八组 271 帧闭环评分；未过八组则不晋级。正式 `9f84efe` 八框仍 `2/8 PASS`，`latest_handoff.available=false`。

## 2026-11-14T13:58:36+08:00 恢复框车体纵横比先验实验（待板端闭环）

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，起始工作树干净，基线 `ecdeeaede64b7ec2aeebde61e9a3be2907b490cb`；本条与源码同提交。三帧版本已在两套 GT 四组回放中 `3/4 PASS`，唯一 car001 右偏框恢复时第 186 帧跟踪框约 `46.6×37.4px`，相对真值约 `44×48px` 明显扁宽，后段第 250 帧跟踪高度约 `39.3px` 对真值 `55px`；当前检测器第 185 帧目标候选约 `47.4×32.8px`，第 187 帧才约 `48.5×42.2px`，说明恢复时先选中局部车体。上述图像/CSV 原件见上方 `0a47e91` 与 `/tmp/nt-dual-gt-evidence/fullframe-smoke-db58683/`，独立诊断不等于在线批次。
- 实验新增默认关闭的 `recovery_restore_initial_aspect`：只在车类且已通过原三帧 detector 支持的恢复确认后，若所选 NanoTrack 框宽高比超过初始 ROI 宽高比 `1.15` 倍，围绕所选框中心补高至初始纵横比，单次最多增加 `35%`；同一修正框用于 `tracking_bbox`、控制框、运动初始化和近期模板准备，`display_bbox` 的 detector 仍只负责外观。没有从真值读入运行时。实验配置 `rk3566/nanotrack_v3/config/visible-dual-gt-visual-gate.conf` SHA-256 `94da012f13b141184f54262f6b359704100051af9064a8cceb35321cf42b38dd`，session metadata 显式记录开关；默认关闭以免影响其他配置。此同集启发式未证明泛化或真实效果。
- 新增 replay 回归验证三帧检测支持门、围绕 NanoTrack 中心补高，以及补正框进入模板事务。实际 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`；日志 `/tmp/nt-dual-gt-evidence/shape-prior-host-build.log` / `shape-prior-host-ctest.log` SHA-256 `ace693747121e772826fc6d260e3bcd7951aa63497d5a359760ece084d983742` / `9853daddc33b26498532af3eb732ae0666ac3a0fc8e0e27394b77d272fc7cbbb`，`git diff --check` 通过。**下一步 / Handoff**：精确提交后在 RK3588 新隔离目录原生构建/CTest，重跑四组各 271 帧，用两套已确认 GT 检查两路中心、IoU、全遮挡、恢复期限和跨初始框一致性；不达四组门则否决并撤回。正式八框仍 `2/8 PASS`，`latest_handoff.available=false`。

## 2026-11-14T13:54:36+08:00 四帧候选板端 2/4 否决并撤回

- 实验精确源码 `codex/dual-gt-init-consistency@b39c12b0d19019bd769168c71f6e80b9178d8449`，主机 `/tmp/nt-dual-gt-evidence/four-hit-b39c12b-source.tar` 与 RK3588 隔离根 `/home/neardi/Project/SOT/dual-gt-four-hit-b39c12b/source.tar` SHA-256 同为 `7def1a9059113f2ea6466af925f3e6bda2dbc56493c4628ff90230740f850879`；构建/四框脚本 SHA-256 `7a2d43eed125c53483e7b933d77f55488db667ecb9651bd368cfd23451b5508e` / `5de7126a7270a63ee43c782d4630c92f9a02f183c01f8cca9178661a1e03d947`，板端传输校验一致。板 `LKD3588` Linux `6.1.84`、RKNN API/driver `2.1.0` / `0.9.8`；canonical 视频及 tracker/detector/manifest/labels 沿用下方 `452c48a` 已复核哈希。实验 controller SHA-256 `513557b0667fe95db8be10d02929cd3d68e0972765b458d8196813c4b2d0e4c7`，`runs/headless-eval.conf` SHA-256 `f922643a960736cfebce3a3285ed2c00abf7c3003d46473b4648b1c368dfacc5`。
- 实际 `bash build_four_hit_board.sh` 原生构建/CTest，`23/23 PASS`，二进制 `build/nanotrack_v3` SHA-256 `067fabb832b8d4a7933bbc4de602435991f59d0a39b492f7b669985f1c5c0dad`，`runs/cmake.log` / `build.log` / `ctest.log` SHA-256 `5d45bfb1135af0267a31031f998dba10228ff5e33a48a56fc005c4b682331156` / `3e7ec886ea71278c98864c44915c6d19be97e3fab47933d903a182d649de94b5` / `fe90720a9bd01fe354adb91885f205967c181d7c6a10a9a826dc036e88f9e5a1`。实际 `bash run_four_hit_four.sh`，car001 标准/右偏、car002 标准/真值四组均 exit 0、各 271 帧，CSV/session/log 原件板端 `runs/<label>/`；四 CSV SHA-256 顺序为 `83c2a693f7b05cd0dc633552e8d3eaacc4b8a7ba3a7a33103b88334ce09ac9d9` / `6e238c2785e74129410058f409e8f068f270d323f9773bfe7d28d90b117235b8` / `c4f3f11b66707aaf17c2e766b767abc60a1a9f2db4dd1fc0f8a0a9c1b9f0d82b` / `c228121e5430f4a52a58921a7d8d5ce80cc1a2cbb4c1f9a4b7c94835cefdb2ce`。板端 `runs/four-text.tgz` 与本机 `/tmp/nt-dual-gt-evidence/four-hit-four-text.tgz` SHA-256 同为 `aa648da5e3a2f032e3d54fad6830116a1492bde42cd264276003ae6ce3f5953a`，本机解包四 CSV 哈希一致。
- 实际 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ... --output-json /tmp/nt-dual-gt-evidence/four-hit-b39c12b/four_score.json`，报告 SHA-256 `a0d8696d2a190ddf26d097b6ad6cfa13c096eabaae82f8df995ff5c258afa7ab`：car001 标准/右偏均 FAIL，重捕第 `196/195` 帧，超过 `<190` 期限；tracking/display 中心 `4.43/4.47px`、`5.34/5.39px`，右偏全可见 IoU `0.693`、后段高度偏小尚未根治。car002 标准/真值均 PASS，第 `234` 帧重捕，中心 `1.74/1.75px`、`1.69/1.69px`。总体 `2/4 PASS`，四帧等待对 car001 的时间门造成退化，不能扩八框或晋级。
- **裁决 / Handoff**：把四帧配置、候选数组/窗口和测试恢复到已板测三帧 `0a47e91` 内容，保留只接受源码允许 `2/3` 确认帧数的评分器修正及正/负回归；原实验提交、板端隔离证据和报告保留。撤回后实际 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure` 为主机 RKNN-off `23/23 PASS`，日志 SHA-256 `2bdcc1fffdf44b9d82b45d9152e8773355b5a9517c890fc0f18a58251b0ff16e` / `a38655d8c79f6dc67f8d85c53a287c2dbd25d1a90407400818b7a5d72ddfca15`；Python 评分器 `24/24 PASS`，`git diff --check` 通过。当前最佳实验为三帧 `3/4 PASS`，car001 右偏后段全车框尺寸与中心一致性仍未达门；正式八框 `2/8 PASS`，`latest_handoff.available=false`。下一步需独立车体完整性证据或可信的恢复后全车几何校正，再重测四框及八框。

## 2026-11-14T13:48:41+08:00 三帧版本板端 3/4 与四帧恢复时机候选

- 前项精确源码 `codex/dual-gt-init-consistency@0a47e919cfc6ced0364ce52df9a98830fb06f989`，主机与 RK3588 隔离根 `/home/neardi/Project/SOT/dual-gt-three-hit-0a47e91` 的 `source.tar` SHA-256 同为 `0ea28dcaa98cf6a7ce04f7edf6182853ef7dff0538a10adc1f59709554afc0be`；构建/回放脚本 SHA-256 `dbbb80e29172f450f64ddb65c4c4458fb63a187354291ea9689024cfae8434e4` / `8ca530fa3d7ce553938a5423d29279fc949bd796a5e195a7b1fbb1ece2dfa4cf`，传输复核一致。板 `LKD3588` Linux `6.1.84`、RKNN API/driver `2.1.0` / `0.9.8`；canonical 视频及 tracker/detector/manifest/labels 身份沿用下方 `452c48a` 已核对输入。执行板端 `bash build_three_hit_board.sh`：原生构建及 CTest `23/23 PASS`，二进制 SHA-256 `2c314d8df7fd99b1a824698897393bdc71d4d997998fc90e96bf232223b0471a`；`runs/cmake.log` / `build.log` / `ctest.log` SHA-256 `c0170111ad0b91321f9ac5295610433094d5b0dc0a93b57804fa9bfd0a188837` / `01e8a08ad07ba6a9309de7cb5ccb1ef8bf7176d850373bdcaf815d00b19ab97a` / `80bf6b631103b0d0e588ffc74cd0524829123c11354ff09432ec7a367ab441d0`。实验配置 `69798f4bcd782d5c445172801d0c4119395854a1494e5f68c3eee088f593bc4f`，`runs/headless-eval.conf` SHA-256 `36ee60c44ae3760305e5428b2e817b493e6cd29b4b7a3b8e742e419e205bad0a`。
- 执行 `bash run_three_hit_four.sh`，car001 标准/右偏与 car002 标准/真值四组均 exit 0、各 271 帧；CSV 原件板端 `runs/<label>/frames.csv`，SHA-256 顺序为 `b443b48162c79a9834586b19ebf6243c99d7b7ed10fc8ac7ce767a57ef1b45b1` / `db50d8882222cfb342123aa19665d510a5a632d75fd3c9bc4418c7f0b660e439` / `cc57946a120941c24fc2901a8307de5fb74487f47bc4f74d0a54d426ff6fc1f3` / `d8764438f90ce8c389bcaeff4dcec3919556ea48c6c486dbfe3d2e6a716b68b9`。文本包板端 `runs/four-text.tgz` 与本机 `/tmp/nt-dual-gt-evidence/three-hit-four-text.tgz` SHA-256 同为 `a8ba05cc25ad85533b7f5034fcdf02746a0d63a6ea74938c1b017c57d8ef88d4`；主机解包后四 CSV 哈希一致，session/log 保留在板端及文本包中。
- 首次正式评分器将三帧 `RECOVERY_STAGED` 误报为无有效重捕，原因是 `scripts/evaluate_occ_video_1.py` 只接受 `recovery_confirmation_hits==2`。本提交将其扩为源码允许的 `2/3/4`，并新增正例，原一帧负例保留；`python3 -m unittest tests.test_evaluate_occ_video_1 -q` 为 `24/24 PASS`。用修订版 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ... --output-json /tmp/nt-dual-gt-evidence/three-hit-0a47e91/four_score_corrected.json` 对两套确认真值重评，报告 SHA-256 `705025de6356d75e97e6e6e1a9fad67bb74612d98fe1178d5138e2dc2bbff2a6`：car001 标准 PASS（187 帧重捕，tracking/display 中心 `4.36/4.41px`），右偏 FAIL（186 帧重捕，中心 `5.41/5.43px`，后段框高度约 `39px` 对真值 `53～55px`，visible_boxes 失败）；car002 标准/真值均 PASS（230 帧重捕，中心 `1.90/1.56px` 两路相同）。car001 标准/右偏 tracking 跨框中心 P95 `12.17px`，car002 标准/真值 `2.57px`。因此三帧版总体 `3/4 PASS`，不能扩八框或 handoff。
- 本项实验将恢复确认从三帧增至四帧，试图让 car001 右偏在部分车体 detector 候选后多观察一帧再重建模板；同时把候选验证数组及过期窗口扩至四帧，默认二帧行为保持。实验配置 SHA-256 `513557b0667fe95db8be10d02929cd3d68e0972765b458d8196813c4b2d0e4c7`。实际 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`，日志 `/tmp/nt-dual-gt-evidence/four-hit-host-build.log` / `four-hit-host-ctest.log` SHA-256 `d5e33edf68811e620d259fd1b6cf000b215e2387b00bf2a5fd96c9317e6281f4` / `02ade2fe4ea4795d289c505ff08f7bc3d65e141ae54e2998b03164195eae1d9d`，Python 评分器 `24/24 PASS`，`git diff --check` 通过。**下一步 / Handoff**：同提交后新隔离 RK3588 目录原生构建，先重跑四组；若 car001 右偏未达可见框门或其余组退化，即否决并撤回四帧实验。正式八框仍 `2/8 PASS`，`latest_handoff.available=false`。

## 2026-11-14T13:35:31+08:00 双真值三帧检测支持恢复候选（待板端闭环）

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，改动前干净基线 `bd0ecaa9d8204fadf027ca64ad8ddafc962180ca`，本条与代码同提交。此前 `db58683` 四组仅 `1/4 PASS`，未将其作为正式算法。对 canonical 视频帧 185、188、225、226、229、230 无损抽帧，在 RK3588 用相同 VisDrone9 RKNN 和 `yolo_detector_smoke --conf 0.25 --nms 0.45 --class-id -1` 做全帧候选诊断；视频 SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`，抽帧/CSV 主机原件 `/tmp/nt-dual-gt-evidence/fullframe-smoke-db58683/`，板端原件 `dual-gt-motion-near-db58683/runs/fullframe-smoke/`。smoke 二进制 SHA-256 `3ba28a7afa1e5aa9c09a551f1fbb2b0e137e2b0006ecb1941fdd7fe8d9712f72`；其最多输出 100 个候选，而运行时不受此上限，因此只把它作为局部几何诊断，非运行时批次同一性证明。
- 第 185 帧 car001 右偏框运动预测中心 `(875.63,826.85)`，最邻候选约 `21.97px`、次邻 `59.71px`；第 226 帧 car002 标准框为 `(862.89,718.90)`，最邻 `8.13px`、次邻 `74.37px`；第 229 帧 car002 真值初始框为 `(867.86,710.78)`，最邻 `13.47px`、次邻 `75.50px`。原来的“前两候选距离差大于 44.06px”漏掉 car001 目标。实验改为最邻残差不超过位置门的 `0.6` 且次邻大于 `1.25` 倍位置门时允许先验证最邻；仍需 NanoTrack 验证。实验 controller `rk3566/nanotrack_v3/config/visible-dual-gt-visual-gate.conf` SHA-256 `69798f4bcd782d5c445172801d0c4119395854a1494e5f68c3eee088f593bc4f`，搜索 detector 间隔 `100ms`、恢复至少三帧一致并要求 detector 支持。多候选视觉遮挡门沿用 `db58683` 实验内容；本轮同时记录新增参数到 session metadata。`display_bbox` 的 detector 融合只用于显示，不能替代 `tracking_bbox` 真值中心或授权控制。
- 实际 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4` 和 `ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`；日志 `/tmp/nt-dual-gt-evidence/three-hit-host-build.log` / `three-hit-host-ctest.log` SHA-256 `b77bb395b46d7f243965715f9624c38907ba2bfbd1ca1f1f4f7fbd831b5cae4f` / `d0cd9b5a22c7a195d773aa9e4117090ce070ed307b311634dd495b833728b6eb`，`git diff --check` 通过。新增三帧确认回归，改动了近邻竞争回归的候选位置以覆盖实际门槛。主机结果不能证明重捕时间、全遮挡无框语义或两路中心改善。
- **下一步 / Handoff**：按本提交精确源码在 RK3588 新隔离目录原生构建/CTest，先完整运行 car001 标准/右偏、car002 标准/真值四组各 271 帧并对两套确认真值评分；若四组不通过即否决和撤回，不扩八框。正式 `9f84efe` 八框仍 `2/8 PASS`，`latest_handoff.available=false`；生产编译与真实吊舱验收未获授权。

## 2026-11-14T13:23:00+08:00 多候选视觉门与近运动优先候选板端否决并撤回

- 实验源码 `codex/dual-gt-init-consistency@db5868302899db1955e82e81b24a0f189cacb2b5`，改动前基线 `9e57cf669b68153b10032f8ca8deafe8ce6fc67f`，工作树 `/home/u20/single-object-tracking`；`git archive --format=tar HEAD` 生成 `/tmp/nt-dual-gt-evidence/motion-near-db58683-source.tar` SHA-256 `1bd3e0ed06cfc26c02c25d8b876fc378adef96baba64f6b2df94479f7a14e759`，上传到 RK3588 新隔离根 `/home/neardi/Project/SOT/dual-gt-motion-near-db58683/source.tar` 后 SHA-256 一致。构建/四框运行脚本 `/tmp/nt-dual-gt-evidence/build_motion_near_board.sh` / `run_motion_near_four.sh` SHA-256 `8bb86948526bbb1dd39d8c987684a3d111d4b9eae99987d351c7fd36a8f1807b` / `280c91a1c2f6390203062a9004408811516b9219bcaf60d09f71ce885e3c550a`，板端上传校验一致。板 `LKD3588` Linux `6.1.84`、RKNN API/driver `2.1.0` / `0.9.8`；canonical 视频、tracker RKNN manifest、VisDrone9 RKNN/manifest/labels 精确身份沿用下方 `452c48a` 已复核值。板端 controller 配置 SHA-256 `c5c4e2edec914eb44cd8777377abf3600574cf3b936225bf9dd2e9d382d15fec`，隔离 `runs/headless-eval.conf` SHA-256 `2344c6b370cce49d6b314d758a6d2c0e55bd8a20f77b8bcde4efc4b3cdaa1587`。
- 实际 `bash build_motion_near_board.sh` 执行原生 CMake/build/CTest，`23/23 PASS`，板端 `build/nanotrack_v3` SHA-256 `d38b3b561bbb618644a2774d550dd73b3fa73eb5d14079aeab90bfd0902ad010`，CMake/build/CTest 日志 SHA-256 `cbdec6e7fc0f01cd46bc7453cb1afbf494a550fe73e746047905ba743e079397` / `6b884731c9ac2ec06a366413956aa1376185eced10a079b833cd32ca24434eb7` / `bc166f8bff7ee75c1551b6db8f26999d4fe07c28cf6b4134042a163bd0cfb527`。实际 `bash run_motion_near_four.sh` 运行 car001 标准 `(992,582,37,39)`、右偏 `(994,584,35,37)`、car002 标准 `(970,517,38,38)`、真值 `(967,513,42,42)`，四组各 271 数据帧、exit 0；CSV/session/log 原件在板端 `runs/<label>/`。四 CSV SHA-256 依次 `906b612a2822268db208086710e3c0471fe4454319e89442c48d6df12de5180b`、`f2cfb935c4c80d6b4b57356643b31eefee4ad70c9dfd8fb95576fc1bf8b361a5`、`cc05800e5e6d93d5e0ba04f7790f09be55eea316638bc27e2da51ff62e28a8af`、`6a57fee32e599396d9f42ff2d48a110a8da141e9e2808da51d91f65924a4e640`。板端/本机文本包 `runs/four-text.tgz` SHA-256 同为 `321ee5f38a4640b99b7c59030f60d6e7c7c21b0270cce24ee56be216e6da1a29`；本机解包至 `/tmp/nt-dual-gt-evidence/motion-near-db58683/`，四 CSV SHA-256 与板端一致。
- 实际 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ... --output-json /tmp/nt-dual-gt-evidence/motion-near-db58683/four_score.json` 用两套对应 GT 评分，报告 SHA-256 `8edb934ce2ebc69e1f50fe97f6dc303133d85d5946f4be0adea66fa17c1a0563`：仅 car001 标准 PASS、第 188 帧重捕，tracking/display 可见中心 `4.28/4.32px`；右偏框仍第 190 帧重捕（要求 `<190`），中心 `4.89/4.92px`，FAIL；car002 标准第 226 帧过早重捕、第 234 帧再次进入 SEARCHING，后段难以恢复，tracking/display 中心 `7.56/7.50px`、tracking 可见中心 P95 `45.83px`，`visible_boxes` 与 `reacquisition` FAIL；car002 真值初始框第 229 帧过早重捕，中心 `1.68/1.68px`，`reacquisition` FAIL。car002 标准/真值跨初始框 tracking/display 中心 P95 `49.69/48.69px`，比上一修订版 `4.95/4.95px` 明显退化。多候选视觉门使 car001 右偏框第 172 帧与 car002 两框第 213 帧进入 SEARCHING，但新近运动候选优先仍不足以保护恢复身份与完整可见性，且 car002 标准框后段再次被视觉门推入搜索。总体 `1/4 PASS`，不能扩八框或晋级。
- **裁决 / Handoff**：按失败即撤回规则将实验配置、算法与测试文件恢复到 `9e57cf6` 内容，保留 `db58683` 实验提交、板端原始证据和本条记录；撤回后实际 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4` 与 `ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`，日志 `/tmp/nt-dual-gt-evidence/motion-near-revert-host-build.log` / `motion-near-revert-host-ctest.log` SHA-256 `01ce4a73e6230e8315504b28ec05e396c31b8d2e30ec41f836e94e68b38f1d24` / `12a3ec9e766ca9af517ece6defc85b6440a32e570d82dad637b51d325bfee36d`；`git diff --check` 通过。正式 `9f84efe` 八框仍 `2/8 PASS`，`latest_handoff.available=false`，不启动生产编译或吊舱验收。后续必须提供独立的车体完整性证据及跨遮挡身份连续性，不能再以单纯时间阈值或最近候选排序代替。

## 2026-11-14T13:15:00+08:00 多候选视觉门与近运动候选优先的联合实验（待板端闭环）

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，起始干净，基线 `9e57cf669b68153b10032f8ca8deafe8ce6fc67f`。恢复此前 `3679dca` 的默认关闭视觉遮挡门候选后，依据下方 `gate3` 板端筛查与 car001 右偏框第 185～190 帧候选竞争记录调整两处：局部 VisDrone9 一帧仅一个候选时保持两帧亮度+检测证据，多个候选时必须累计三帧才停用疑似观测；搜索恢复时，若运动预测仍可信、最近 detector 候选在位置门内，并且与第二近候选中心至少相距 `max(40px, 两框最大边长)` 且位置残差不超过后者一半，则优先对较近候选进行既有 NanoTrack 双帧验证。后者只改变候选验证顺序，detector 本身不得直接授权 `tracking_bbox`、`control_bbox` 或重捕。实验 controller `rk3566/nanotrack_v3/config/visible-dual-gt-visual-gate.conf` SHA-256 `c5c4e2edec914eb44cd8777377abf3600574cf3b936225bf9dd2e9d382d15fec`；新分离常数/相对阈值未校准，需在八框和其他场景检验。
- 新增/调整运行时 fake-detector 单测，覆盖多候选前两帧仍 SUSPECTED、第三帧停用观测、单候选第二帧停用观测；replay 单测覆盖“近车与高置信远车分开但归一化残差差值小”的优先顺序和双帧 NanoTrack 验证，既有近邻车辆冲突测试仍通过。实际执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4` 与 `ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`；构建/CTest 日志 `/tmp/nt-dual-gt-evidence/ambiguous-visual-near-focus-host-build.log` / `ambiguous-visual-near-focus-host-ctest.log` SHA-256 `7f0fe9002b825dc39b1be9f291c2108dead7b3fe453bebbb4334183250deee46` / `291a9c31457dcc112334eff266ef13cb37bbef9c4072b5d8ac6138da8b9f1634`，`git diff --check` 通过。主机测试没有运行 RKNN 引擎，不证明正确重捕或中心改善。
- **下一步 / Handoff**：本条与源码同提交后按精确 commit 在 RK3588 新隔离目录原生构建/CTest，先完整跑 car001 标准/右偏与 car002 标准/真值四组各 271 帧，对照两套确认真值、重捕期限、无框段语义、两路中心/IoU、跨初始化一致性及错车；失败即撤回。正式 `9f84efe` 八框 `2/8 PASS`、`latest_handoff.available=false` 不因主机测试改变。

## 2026-11-14T13:06:05+08:00 car001 邻车竞争段的板端参数筛查

- 当前分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，起始干净，基线 `3fcac5fb8b4c1de86bde711ec472afee1ea7ff67`；本轮不改算法源码、真值、模型或正式配置。只复用先前隔离源码 `452c48aa5f6a93abcd6fd888d50801de9894c36b` 的 RK3588 原生二进制 `/home/neardi/Project/SOT/dual-gt-visual-gate-452c48a/build/nanotrack_v3`，重新核对 SHA-256 `1a54d65b23d0746b2dec5bfd49c625233176c3f7e5d69c910fee1884eee129e5`；板 `LKD3588` Linux `6.1.84`、RKNN API/driver `2.1.0` / `0.9.8`，canonical 视频及 tracker/detector 模型、manifest、labels 精确哈希沿用下方 `452c48a` 条目。本轮仅把实验 controller 的连续视觉证据 `2→3`，或另将搜索最短年龄 `400→300 ms`、搜索 detector 间隔 `400→100 ms`，分别复制到独立 `runs/<name>/controller.conf`，再复制 `runs/headless-eval.conf` 改指该配置；未覆盖原实验配置或输出。
- 在板端分别执行 `./build/nanotrack_v3 --config runs/<name>/headless.conf --bbox 994,584,35,37 --output-boxes runs/<name>/frames.csv --session-metadata runs/<name>/session.json`，三组均 exit 0、271 数据帧。`gate3-car001-right` 的 controller/headless/CSV/session/log SHA-256 依次 `1cce98497c2ed10fba702a9cdfc6dda1508671b7d175e9b7da0d1688d5de5644` / `e2282612e3390eae22b243ba39ab66bcc4d1737201ffbf929fb0c7cd662db9e8` / `5e7b68ee8b3dcb5500f643ad0e5d12c8a2997ee2c14dcfe2d3aeadbfed83e3bd` / `0b1448f2acae6875920f50c1150a492c5fb215e4514fcba4961e993cdcff42dd` / `08c938fb9913981a1b419af375443fc2653597b6f339f5e0ead0444df34e6eed`；`gate3-age300-car001-right` 为 `5a920ee311d6b01b1d620c63626af619cdd1511f9e0fee725acd8e0c0173ff8e` / `efbb942d1869ef5f8d257f0caac1d63ac7447251615db13ca0204d82ce215263` / `5a940dcc8d655d8f90f5880ea8b1c9a70e9f26328b4e8a48dde9c22a4a718f01` / `3b827eefd2b989a2b2285533dec4fad729d3c527616feef1a097355aae0a12c1` / `6364c26e58a457cc13e36ea9f0570999f229600945c0f4ad8c50fce4a8310c46`；`gate3-det100-car001-right` 为 `7c78c99f81af3df00b9253a251b9a1a10d385a5c6c72728ef6c4b0a4efc880dd` / `7acfc07b105478266309f5f1a9bc77ed3f9e7983a9a3bb5e99c0a3a58b1c5840` / `0f091a82b71ee2d5d3c5851c3cf2814ccbd8afc6f784a634a877949a8d6df0f5` / `3255a9d371cb9881c0a1fb4650cb1182206e4ba2b8d1d2adeda7391d90986073` / `4c4789af4f7b94b1df5146a15dfeeb3f84fdfdc23170cbc77f72c9a389d7d988`。原件都留板端隔离根相应 `runs/<name>/`；三份 CSV 回传主机 `/tmp/nt-dual-gt-evidence/<name>-frames.csv` 后 SHA-256 与板端一致。
- 用 `python3 scripts/evaluate_occ_video_1.py --dataset-root data/occ_video_1 --target-id car_001 --boxes /tmp/nt-dual-gt-evidence/<name>-frames.csv --scope full --output-json /tmp/nt-dual-gt-evidence/<name>-score.json --output-markdown /tmp/nt-dual-gt-evidence/<name>-score.md` 对同一 car001 确认 GT 评分；三 JSON SHA-256 依次 `b91fa6684c3ec546646c7c41ba50bf57836a73c00dade06291480580b95162b6`、`010fae86f0df7f433708024b6339283d202cb5c95ab386b73ff48357c1f1d71a`、`43bff29a7cfd6618d8d7a16ee62863233fd4d3fd627fe3b895be1795e1bb79b7`。三组均仅因重捕晚于 `[182,190)` 而 FAIL，重捕帧为 `190/196/190`；缩短最短年龄使第 183～190 帧持续 `IDENTITY_CONFLICT`，加快 detector 更新虽在 182/185/188 帧请求并在 183/186/189 帧收到批次，仍到 190 帧才确认。此三次是**单框参数筛查**，不能代替两目标八框闭环。
- 叠加原视频、板端 `tracking_bbox` 与对应真值的诊断图 `/tmp/nt-dual-gt-evidence/diagnostic-occlusion-truth-montage.png` SHA-256 `d0d41c158dfe7c22d29981b1eb6c2ee4f797d372a1d1f78ab8fcfacabd0fc7ad`：car001 右偏框在完全遮挡第 172 帧仍 SUSPECTED，car002 标准框第 226 帧把桥面与部分车体当成可恢复目标，第 230 帧框相对完整车体上移。局部亮像素比无法稳定区分 car002 真值框的 229/230 帧，故本轮不引入单阈值可见性判据。**Handoff**：参数调整不足以同时解决及时重捕、身份与完整车体可见性；正式 `9f84efe` 八框 `2/8 PASS`、`latest_handoff.available=false` 不变，下一步应验证独立候选身份与完整性证据，再做四框板端筛查和八框验收。

## 2026-11-14T12:53:00+08:00 修订视觉遮挡门四框板端否决并撤回

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，板端候选源码 `3679dcaef996e40fd03d639c224c02521aa8eb7c`，改动前基线 `6d8002485d714aacd68f087d988286ec8839c42b`。源码包 `/tmp/nt-dual-gt-evidence/visual-gate-refined-3679dca-source.tar` 与 RK3588 隔离根 `/home/neardi/Project/SOT/dual-gt-visual-gate-refined-3679dca/source.tar` SHA-256 `2d32a7a04ee2e814ce99e85b2918ed0c956835b214c4af480eb1f245f6c95856`；构建/运行脚本 `/tmp/nt-dual-gt-evidence/build_visual_gate_refined_board.sh` / `/tmp/nt-dual-gt-evidence/run_visual_gate_refined_four.sh` SHA-256 `dd404da7eb7eea39ee79aaa3f5f4eea89afa0497e17045bd4ea7154188050525` / `4602d26c6279fbd24d981c4248681fac36fe7e2885be05d321c5d51fa21bc0a8`。同一 `LKD3588` Linux `6.1.84`、RKNN API/driver `2.1.0` / `0.9.8`，canonical 视频、tracker RKNN manifest、VisDrone9 RKNN/manifest/labels 的 SHA-256 沿用上一节已复核值；板端 `runs/headless-eval.conf` SHA-256 `337cd5b586ea55d8abdb2b7824ea8ab69fc52116de24421b7bb22cdfe6a541f8`，实验 controller 配置 SHA-256 `c5c4e2edec914eb44cd8777377abf3600574cf3b936225bf9dd2e9d382d15fec`。原生 CMake 构建和 `ctest --test-dir build --output-on-failure` 为 `23/23 PASS`，二进制 `build/nanotrack_v3` SHA-256 `16c6a05cf45bc7b97bfc222d01053da2025da97409c27e88c288db9856cfcbaf`，CMake/build/CTest 日志 SHA-256 `9530446d6af7e5dde8fa33c8ae84530953c1d416119d98e0e12c0126eaf62570` / `ae5dc2b89577be639719bcd0669f5b1b37c489f6ab1d7d119e4b28f99d7ff702` / `157746f08c386beb08926dd3ea3e50b2b7f8ddc138420e2212aaabf6c5d881a9`。
- 实际板端 `bash run_visual_gate_four.sh` 跑 car001 标准 `(992,582,37,39)`、右下 `(994,584,35,37)`、car002 标准 `(970,517,38,38)`、真值 `(967,513,42,42)`，四组均 271 数据帧、exit 0；原始 CSV/session/log 留隔离根 `runs/<label>/`。四 CSV SHA-256 依次 `d90d38c0d1b69826f430e6d4c1146cf381aaddd75494b860b318091b0b5c34d3`、`074ef9fbcc79dce1557e9cf015122ff44189bf1066eb205a898c14b760521b8a`、`e367f6b8da41ac6747ae7eb3a0b106af8dba22654b1cf52c6a8b23aff1875f87`、`ea73839e6682a3ed4770314322a4761bbfe653026d0e10b93362cee894fa6e1b`。板端/主机文本包 `runs/four-text.tgz` SHA-256 同为 `111af5ac94dd46f2b63542d293408dee6344adac15056826c9b043464829922c`；主机副本解包于 `/tmp/nt-dual-gt-evidence/visual-gate-refined-3679dca/`。以 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ... --output-json /tmp/nt-dual-gt-evidence/visual-gate-refined-3679dca/four_score.json` 对各自确认 GT 评分，报告 SHA-256 `31fd836ba534411c0b2a140c80d2b5c7c12ffacf559751900b7b4d6cfd98538d`。
- 结果仅 `1/4 PASS`：car001 标准保持 PASS、188 帧重捕，tracking/display 可见中心均误差 `4.28/4.32px`；右下初始框在完全遮挡 172～174 帧仍 SUSPECTED、第 191 帧才重捕（要求 `[182,190)`），两路中心 `5.02/5.05px`、IoU `0.701/0.748`，故 FAIL。car002 标准虽第 213 帧进入 SEARCHING，但第 226 帧过早重捕（要求 ≥230），第 230 帧可见框偏离真值，tracking/display 中心 `2.18/2.18px`、IoU `0.815/0.758`，FAIL；真值初始框第 229 帧过早重捕，两路中心 `1.68/1.68px`、IoU `0.887/0.759`，FAIL。标准/右下 car001 tracking/display 跨初始框中心 P95 `2.84/2.16px`；car002 标准/真值 `4.95/4.95px`，但均值掩盖了 car002 第 233 帧最大跨框 `33.29px`。修订改善了 car002 全遮挡输出，却未提供可靠的恢复时机和身份判据；`display_bbox` detector 仍不参与跟踪/控制授权。
- **裁决 / Handoff**：按失败即撤回规则，将视觉遮挡门新增配置、运行时代码与对应测试恢复到 `6d80024` 算法内容，保留候选提交和板端隔离证据。撤回后实际执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4` 与 `ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`；日志 `/tmp/nt-dual-gt-evidence/visual-gate-refined-revert-host-build.log` / `visual-gate-refined-revert-host-ctest.log` SHA-256 `13fe1780d7e3b8fbf78bd7f1c4905788641fbb6489c31fd176af4141a4a1244f` / `ee57870d975c968aa6da40dfd2575f4b6ffe9192d3f66440fc8d675ff5b933c3`，`git diff --check` 通过。正式 `9f84efe` 八框仍 `2/8 PASS`，`latest_handoff.available=false`，不可作为生产编译或吊舱验收交付。下一步需以独立的完整可见性和身份验证限制过早重捕，同时使 car001 右下框在遮挡时及时退出 SUSPECTED；新的候选仍必须先过四框板端筛查，再跑八框。

## 2026-11-14T12:42:56+08:00 视觉遮挡门首轮板端否决与时序/多候选修订

- 首轮候选精确源码 `codex/dual-gt-init-consistency@452c48aa5f6a93abcd6fd888d50801de9894c36b`；本机 `/tmp/nt-dual-gt-evidence/visual-gate-452c48a-source.tar` 与 RK3588 隔离根 `/home/neardi/Project/SOT/dual-gt-visual-gate-452c48a/source.tar` SHA-256 同为 `51d1a5166935a08e7990f1d998fea50bfe3c2de674dd54af99549f2ddaa13d3e`。构建/四组运行脚本 SHA-256 `cbb00369273ac7e2efe137921cb40ff419bafd13eb5e7c0aefcfbf266c455834` / `66e7d3544d2b2a27410f7d8a32b0e23d7b71c973c42eb68d65c80623f2eb1091`。实际板 `LKD3588` Linux `6.1.84`，RKNN API/driver 沿用本文件已验证 `2.1.0` / `0.9.8`；本轮重新核对 canonical 视频、tracker manifest、VisDrone9 RKNN/manifest/labels SHA-256 分别为 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`、`3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`、`8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c`、`ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9`、`41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`。隔离 `runs/headless-eval.conf` SHA-256 `117e73483af49ce9a1e546cbbeca15138f4e9219eacca484874134893ac4bed4`，指向本隔离源码的实验 controller 配置。原生 CMake/build/CTest 23/23 PASS；日志 SHA-256 `2fc34d6506da19241010d5e372f5af3436a62c6bbfd250a1858b8de3aa248009` / `97d62ebbc52d5836e1ea997a22de1ee5de804739a0c311afde109639c21b0190` / `d027655af040861314281f0e5b0fe85f669d29b4a76e8d2afa34293c8e1f9b5a`，二进制 `build/nanotrack_v3` SHA-256 `1a54d65b23d0746b2dec5bfd49c625233176c3f7e5d69c910fee1884eee129e5`。
- 实际 `bash run_visual_gate_four.sh` 完成 car001 标准 `(992,582,37,39)`/右下 `(994,584,35,37)`、car002 标准 `(970,517,38,38)`/真值 `(967,513,42,42)` 各 271 帧、exit 0；CSV/session/log 原件留板端 `runs/<label>/`。四 CSV SHA-256 依次 `cc0e3775931de25fe10857ae65c67095ffbe9a9c59491d9232c3567d3d7c5300`、`78c261dfaa7673615aef2c596d1499a81bd2f5c898c12390fe8e3809828a5e1f`、`29eb86addc845aef2a595075f1003579e843e45df1282e8b83ede4f9094d4a4f`、`1a1731a7865f1375db68e3e3f4c7cb12a85c1456108bc4e5eefd580c0780561b`；session SHA-256 依次 `82d7e339d52e0009fc88fef55555646481c7517b9a23cbcc89f276a023735022`、`db4597e129b7d3e12388529a549ac99ccaee0a7058f71e38f60611575c10fc6d`、`df0baeeafe7796e301cc8b7fe7a9346a3b47d4558cbe5c85c775843234fa6290`、`96217dbb28a4a6b0756235b3a0c8fe44ee15145265194a5d8aaf779733cf3fbc`；日志 SHA-256 依次 `35a9ac5f3598703620785be09550d4ee77d667cabff9161b00d44b08c68bd713`、`b6ebe1a68528c3857f0f4070f98bbf86fefc86397d38d7b2db8198f7af15e1d2`、`70128bd957862af9c9f931a5bbca6d061a1eb2d0ce6b212b6f5f42bf9e92d901`、`c82351d1980b7ae3c61a595ec1d671199a9ff9e13afea69091727b5d970044cd`。文本包板端/本机 SHA-256 一致 `93d493719c07676ec2582187e6a569f0b586e65857bd8d24ea1cf3bb576dd959`，本机副本 `/tmp/nt-dual-gt-evidence/visual-gate-452c48a/`。
- 用 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ... --output-json /tmp/nt-dual-gt-evidence/visual-gate-452c48a/four_score.json` 按目标对应 GT 评分，报告 SHA-256 `d496b7985c2f68bfc364b388903ba908edb44773c70becc99716f463ecd58bf7`：仅 car001 标准 PASS，188 帧重捕、tracking/display 中心 `4.28/4.32px`；右下框第 171 帧视觉门提前 SEARCH，却第 189 帧错误重捕邻车，tracking/display 中心 `33.69/33.51px`、IoU `0.473/0.520`，`visible_boxes` 与 `identity_proxy` FAIL；car002 两框从第 211 帧对外发布 SUSPECTED，但视觉检测直到第 215 帧才开始且未触发，完全遮挡语义仍 FAIL，重捕 `235/233` 帧，中心 `2.59/2.00px`。四组仅 `1/4 PASS`，不能晋级。
- 源码检查确认首轮只看 `SearchPlan.tracking_phase`，该内部模式晚于输出对外 `SUSPECTED`，因此车体亮度虽已下降，第 212～213 帧仍没有新鲜检测；car001 右下局部区域第 170～171 帧同时存在多个候选，单一最近候选允许过早搜索并改变身份竞争。基于**这两处板端反例**修订：下一帧检测条件依据上一帧对外 `tracking_phase`，且同帧局部 detector 批次必须恰有一个候选，才计入连续证据。新增 fake-adapter 回归验证低置信度、多个候选、单次命中均不能触发；实际主机 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4` 与 `ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，RKNN-off `23/23 PASS`，日志 SHA-256 `72ecfa4063f67e9d0bc564e65d5485b63e24e315d8a1ddc538d1a3504c81249d` / `1a6633cad444badd3c19465dae63488e0bd5e67b131fd7622b1da78c50f0f5d6`，`git diff --check` 通过。
- **下一步 / Handoff**：修订版尚无板端结果；本条与源码同提交后需另建 RK3588 隔离目录，先重跑同四框完整闭环并检查上述两处反例及两路中心/IoU、无框和重捕时限；任一目标仍退化即撤回。正式基线 `9f84efe` 八框 `2/8 PASS`、handoff HOLD，`latest_handoff.available=false`。

## 2026-11-14T12:31:10+08:00 现有 VisDrone9 遮挡证据门候选（待板端闭环）

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，起始干净、基线 `6d8002485d714aacd68f087d988286ec8839c42b`。依据上一节同视频/同轨迹的离线反例筛查，在 `ControllerConfig` 增加默认关闭的 `visual_suspect_*` 参数；候选配置 `rk3566/nanotrack_v3/config/visible-dual-gt-visual-gate.conf` SHA-256 `c5c4e2edec914eb44cd8777377abf3600574cf3b936225bf9dd2e9d382d15fec`，`calibrated=false`，亮像素比 `<0.90`、最近 car 候选置信度 `≥0.40`、中心距离 `≤30px`、连续 `2` 帧。可信历史亮度只在 `TRACKING + accepted` 时更新，当前帧检测只在已经 `SUSPECTED` 且亮度下降时发生；局部 ROI 尺度/320px 内容加灰底与此前 `yolo_roi_replay` 一致。使用现有 `YoloDetectorAdapter::detect_blocking` 取得与当前帧绑定的结果，不使用过期异步批次；检测框只做**停止疑似观测**的证据，不替换 `tracking_bbox`、`display_bbox`、控制框，也不能批准恢复。
- 控制器只在连续证据成立时将该帧 recent 处置改为 SEARCHING；实际是否能形成正确重捕、两路中心/IoU、一致性及板端负载尚未验证。新增 runtime 单测覆盖：进入 SUSPECTED 前不调用局部检测、低置信度候选不触发、首次有效命中仍为 SUSPECTED、第二次新鲜有效命中无观测且不以观测控制；配置单测加载候选配置。实际运行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4` 与 `ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`；构建/CTest 日志 `/tmp/nt-dual-gt-evidence/visual-gate-host-build.log` / `visual-gate-host-ctest.log` SHA-256 `3e3fa7fbd61f5be5477413f0318e2b7c362818c2b99eb802279a61af68c2ff7f` / `5de9477b2ebd34030ede33e01ab890bdf57d230f80023466a48939614806dd03`，`git diff --check` 通过。
- **证据边界 / Handoff**：本阶段主机单测未编译或运行 RKNN 引擎，离线阈值来自 canonical 同集筛查且在线框和时序会反馈，不能预判实际结果。待本条与源码同提交后，按精确 commit 在 RK3588 隔离目录原生构建/CTest，先跑 car001 标准/右下、car002 标准/真值四组完整 271 帧，逐帧核对触发时机、误搜索、恢复身份、全遮挡无观测、tracking/display 中心/IoU 和耗时；若不同时改善，则撤回候选并保留反例。正式 `9f84efe` 八框 `2/8 PASS`、handoff HOLD 不变；不能启动生产编译或吊舱验收。

## 2026-11-14T12:17:41+08:00 复用现有 VisDrone9 RKNN 的遮挡候选离线筛查

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，开始干净、基线 `ae9abbc996c608dd25bad61ba9497d636e291522`。本轮仍未改算法、模型、配置或真值；只读复用 `9f84efe` 八组板端完整 CSV、已有 `yolo_roi_replay` 对同一 271 帧视频及各组**冻结运动预测 ROI** 运行后的八份 RK3588 VisDrone9 候选 CSV，以及原视频对应框内亮像素占比 JSON。car002 标准/真值/左/右 detector CSV SHA-256 依次 `46399f6a2705d15200742dc23dd285ab76c1eb449f7b113b261a3945797cfb10`、`89dbe3b47421efe9d419a5472277bb751ccfe5ae082f500cb8020951ec75c9f9`、`8a151d947651d63e0923d87ba9a530b75d9b76761f5b8c78ce35953be0a90330`、`f2d18a3b0db4ddf079ead29978037e00eaff725a44cf8c924535e2abf3e74cc2`，亮度 JSON SHA-256 `41938b45729d023289c6aff2f7032515ad250aba84449e77e8a3e2fcf6a7445e`；原始模型、板、输入哈希及 ROI replay 命令见本文件 `b5ca289` 一节。
- 实际执行 `python3 /tmp/nt-dual-gt-evidence/screen_existing_detector_visibility.py > /tmp/nt-dual-gt-evidence/existing-detector-visibility-screen.log`；脚本 SHA-256 `5b38ca1f6682a1e33d2bde60674264e36fc0d649dcbf13d37dbedfbbfb2aeb90`，报告 JSON/日志 SHA-256 `57f52d00b4beef1ef971e0de6a73d2b20049df4a08995851c2a5c5a8524dee24` / `f3576cc98cb7dcecd585e07d1c4c77be02fc8fdfd46221e511ac6ab86517b455`，均留 `/tmp/nt-dual-gt-evidence/`。离线规则仅在基线 `SUSPECTED` 评估：框内亮像素占比低于最近八个可信帧中位数的 `0.90`、距运动预测中心最近的本帧局部 detector 候选中心不超过 `30px` 且置信度至少 `0.40`，连续两帧才提出“进入搜索”候选；没有在推理中读取 GT。**这是针对本 canonical 轨迹筛出的参数，不是独立泛化验证。**
- 八组冻结轨迹首个双帧命中：car001 标准/人工/左上均无，右下第 `167` 帧；car002 标准/真值/左上/右下均第 `212` 帧。对 car002 左上第 69～70 帧的正常可见 `SUSPECTED`，第 69 帧虽满足亮度和 detector 条件，第 70 帧最近候选置信度仅 `0.23`，因此未连续命中；第 120～142 帧正常可见的长 SUSPECTED 段亦未双帧命中。已有 VisDrone9 候选第 211～212 帧四框面积明显变小且置信度约 `0.41～0.77`；其在 car002 恢复第 229 帧无可靠真值框也能给出目标候选，故这项规则只拟用于**停止可疑观测**，不得作为恢复或控制批准。离线 120px COCO `yolov8n-seg` 补筛在 car002 第 205～212 帧多次漏检；脚本/JSON/日志 SHA-256 `7dc016a57e94dbcccc08b1643615609cabab3e0ab87f3a0c8f1450960a12f5a0` / `3edc2e16526d1007f625fc335420fbc84e34e6fa0ac1173cd00e64c25dba609d` / `36fda8af692d9da8164f23c0a968c338cddb36d95c0e786f934709f23d2b4096`，不晋级轻量分割模型。
- **证据边界 / Handoff**：该信号来自同一基线轨迹的逐帧局部 detector 回放。正式运行中 detector 是异步单工 worker，且当前只在 SEARCH 和显示刷新调度；这份离线结果没有证明第 211、212 帧在真实闭环都能取得新鲜局部候选，也未验证提前 SEARCH 后的恢复身份、car001 右下框重捕期限或两路中心/IoU。下一步若实现候选，必须显式处理请求优先级、源帧对齐、过期、丢帧及 detector 负载，先用四框板端闭环筛查再扩八框；不能把同集离线双帧命中当作完成。正式八框仍为 `9f84efe` 的 `2/8 PASS`，`latest_handoff.available=false`。

## 2026-11-14T12:09:50+08:00 分割模型八组冻结轨迹复核：恢复判据不一致

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，开始干净、基线 `218f6a343ef857b71215335f863891d1351da7c6`。沿用上一节已哈希绑定的 canonical 视频、`9f84efe` 八组板端 CSV 与 `yolov8s-seg-v8.3.0.pt`；从各组 `motion_predicted_*` 中心裁 120px 正方形再放大至 640px，CPU 推理 `classes=[2], conf=0.05, retina_masks=True`，真值只用于推理后裁决。实际运行 `.venvs/yolov8-visdrone/bin/python /tmp/nt-dual-gt-evidence/screen_yolov8s_seg_120_eight.py > /tmp/nt-dual-gt-evidence/yolov8s-seg-120-eight-screen.log 2>&1`，脚本 SHA-256 `e06b8dd791d03984ad158a93991e3a4948f7288a3ffc18e1559b7d3a4a01711b`。car001 四框第 166～195 帧、car002 四框第 205～235 帧，共 244 个冻结 ROI 推理，exit 0；JSON `/tmp/nt-dual-gt-evidence/yolov8s-seg-120-eight-screen.json` SHA-256 `82736f71a97c6c6a2415b944f6c72ea239fe37c08e236404a3a45f61a9807afa`，日志 SHA-256 `3bde1fa3befcec9a501b89a056ac341e7eaaf7b46f7173cbbeae8a0242815c59`。基于同一视频与 car002 真值框位置作只读画面复核，拼图 `/tmp/nt-dual-gt-evidence/car002-occlusion-montage.png` SHA-256 `34e1fcd06e60f6d4f01667dfc7bee20ee174719079c2ceff4bd8695e0011d299`。
- car002 四框在第 210 帧最近候选分割面积与各自第 205～210 帧中位面积之比约 `0.96～1.03`，第 211 帧降至 `0.66～0.78`，有提前拒绝疑似观测的信号。但恢复时标准框第 230～232 帧连续无汽车候选，右扰动框第 229 帧仍无可靠真值框，却已有面积比 `0.98` 的近距离候选；因此不能用面积比、候选存在或置信度构造跨初始框稳定的恢复授权。car001 右扰动框第 187～189 帧最近候选位于约 `24～25px` 距离，候选存在本身也未证明 NanoTrack 会安全选回目标身份。以上只是在**原闭环冻结运动轨迹**上回放；改变搜索时机后轨迹会变，不能由此推断闭环改善。
- **裁决 / Handoff**：保留离线证据，但不晋级该分割模型、阈值或板端源码改动。正式 `9f84efe` 八框仍为 `2/8 PASS`，两路 `tracking_bbox`/`display_bbox` 中心、IoU 和初始框一致性目标未完成；`latest_handoff.available=false`。后续候选必须先在 car002 无可靠框段和 car001 邻车竞争段证明独立身份/完整可见性，再跑 RK3588 八框闭环；不把画面或分割候选当作真值参与在线推理。

## 2026-11-14T12:05:31+08:00 COCO 实例分割恢复可见性筛查（未晋级）

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，开始干净，基线 `aa15a4801024c4fd5aced51d4fcf901884a0c20e`。本轮只在本机 CPU 做离线探针；正式算法、RK3588 二进制/模型/配置与两套真值未改。输入仍是 canonical `data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；每帧裁剪中心来自 `9f84efe` 板端冻结 `motion_predicted_*`，未用真值定位或逐帧修正。官方 COCO 实例分割权重 `/tmp/nt-dual-gt-evidence/yolov8n-seg-v8.3.0.pt` / `yolov8s-seg-v8.3.0.pt` SHA-256 分别为 `a7cd8f929e1903d78a12a48efecab430209f18dc46cb96c3599a5980c63c423c` / `0bac0770b55e5eb5b76a61bc535673288dcec36c2bc0cd25ee0d584c632f3413`；使用 `.venvs/yolov8-visdrone/bin/python`、Ultralytics 8.2.82、torch 2.4.0 CPU，`classes=[2], conf=0.05, imgsz=640, retina_masks=True`。
- 实际执行四份脚本 `/tmp/nt-dual-gt-evidence/screen_yolov8n_seg.py`、`screen_yolov8s_seg.py`、`screen_yolov8s_seg_120.py`、`screen_yolov8s_seg_120_early.py`；SHA-256 依次 `77100b663373a2fff21394e2d039ec72e1f094b163fdef202c7e4a4c0a94bdae`、`a55af61545e55bf1f196137abbd64362f9604855d1d111e927716d16a8ae0f2a`、`67307c208453a1eb6c1019a5c9bcbaa709a05d59f545dfda9281fbc72244550b`、`bbfcab249bf801218c97d40da8b6bd149c15b22c2efb61acc69f50185daca7b0`。前两项以 180px 正方形裁剪，两项 s 模型补筛以 120px 正方形裁剪，均放大到 640px 推理。四份 JSON 输出 SHA-256 依次 `978f025badabed252a061a3e5ad1f4a4b650489af96c4e9107511ddbf7d28f39`、`444ad298fcb9dbff68f126930a7aa53f5945347c18b2da63716aa39b66fc336f`、`4eec59bb90cac39811a043771246c58a1e0c0b79cc139b5cfa56cc2a59eb69c8`、`07ca7bcb657a0e8f43d29b035d7f2dcdde6e1911698fb8b5782236c48694e4b3`；对应 `.log` SHA-256 依次 `e71b2f32bc3ad95451a3d3d7250e82f6fbe0a87660b2db0332d5cac99d369c56`、`0ed498f5ed8a2bc017965bbda76801ae9f990b333b1d21903d053668da2f5c87`、`39f8f50559bdec5d673265ea0abec44c55546e537721ad1d96823fede4877d98`、`f8229ed2641a0eb9b685a71c72a986ef8d16f409d2a2a83b4e4726e874ae67ab`。输出均保留本机 `/tmp/nt-dual-gt-evidence/`。
- 180px 裁剪时 n/s 两模型均漏掉 car002 第 229、230 帧目标，n 模型还漏掉 car001 第 190 帧。120px 裁剪时 s 模型对 car002 第 230 帧有距运动中心约 15px、面积约 1697px² 的候选，但置信度只有 `0.07`；人工确认**无可靠框**的第 228 帧同一位置也有面积约 1556px²、置信度 `0.23` 的候选，第 229 帧又无候选。第 211～215 帧均是无可观测真值框，但最近的汽车分割候选仍连续存在，面积约 1122～1504px²、置信度 `0.28～0.57`。检测到可见车体片段不等于可批准完整目标框。第 205～210 帧可见段最近候选面积约 1678～1979px²，虽能看到遮挡后面积下降，单帧面积或置信度无法同时可靠隔开第 228 与 230 帧。
- **裁决 / Handoff**：不把现成 COCO 分割候选接入 `tracking_bbox`、`display_bbox` 或控制门禁，也不以其推理结果声明恢复成功；本轮未做 RKNN 转换/板端闭环，不构成性能或泛化证据。后续若提出独立可见性判据，先验证它在 car002 第 211～229 帧无框期以及 car001 邻车竞争期的反例，再完整跑本分支八框板端评估。正式基线 `9f84efe` 八框仍 `2/8 PASS`，`latest_handoff.available=false`，两路 tracking/display 中心和框贴近真值的目标未完成。

## 2026-11-14T11:54:13+08:00 SiamABC 小模型双目标 CPU 身份筛查

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，开始干净、基线 `34ce116b835873e211596416651df4b95605804f`；正式算法、板端模型/配置和 GT 未改。独立预训练源 `repos/siamabc@b1c94e06fdf2dd3cb14ed07b05e38aa4601ece03`，固定 `assets/S_Tiny/model_S_Tiny_v5.pt` SHA-256 `eb52c20776bb2d1a20ab7d1d88873944c6b83b56a63f0fe8f2754edeec123576`，只把原 tracker 的设备选为 CPU，未更新权重。输入仍是 canonical `data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；两目标已确认真值仅在跟踪后评分，不参与逐帧推理。
- 实际用 `.venvs/siamabc/bin/python /tmp/nt-dual-gt-evidence/screen_siamabc_cpu.py --bbox <x y w h> --output /tmp/nt-dual-gt-evidence/siamabc-cpu-<label>.csv` 分别跑 car001 标准 `(992,582,37,39)`/右下 `(994,584,35,37)` 与 car002 标准 `(970,517,38,38)`/真值 `(967,513,42,42)`，四组均 exit 0、各 271 行；脚本 SHA-256 `161d08750ebc370b0fe20405d01e49e72b54431f2f2d8a422aba8c123b538fae`。四 CSV SHA-256 依次 `c43b67a7b0bcf6d51e89e827a8d8d3e3d788ba3c2886fb2af74bfc350c5ec413`、`47af52a748a68ae9a9bc2cd32ec7889bbb1509b589be813e518f0bebdb2f8153`、`e12ecff201a900b04163ac7a69a555cebe8232b9df923e2a894fdc84d6f85956`、`ebb651f8f79ea6163162d464ff8d8af680c556fe2811fd4fb224ecd5f88f2d88`；相应日志 SHA-256 `ec5dd12dd4d69207cf9cdab4fe9f53831fabac1116fa8b0fdf5a4eb492df1fae`、`aabdc999a6181546687f1360269290b1373b8462326253560dfec217c8095395`、`20a7395c9bea40e3dc2b88c0e6e8f41f2c47fcfc4332c96d873029240d46d74b`、`3b3d26e020761151c4d349b7cd8080c2a3bab3608f27f09ed0c21706dee107af`，均留本机 `/tmp/nt-dual-gt-evidence/`。评分脚本 `/tmp/nt-dual-gt-evidence/score_siamabc_cpu_four.py` SHA-256 `ea3e926ab657903a9e526e0697ac659725ff532b93096009fb544d78aa769b98`，报告 `/tmp/nt-dual-gt-evidence/siamabc-cpu-four-score.json` SHA-256 `1496b301f23f4c28c0b174c73fbeaf1696d8b2a693537cb90ced3fe55bef8af0`。
- car001 标准/右下可观测 258 帧中心均误差 `5.35/4.58px`、IoU `0.755/0.774`；遮挡前 169 帧中心 `3.18/2.51px`，但重新可观测 182～194 帧平均 `27.52/27.36px`。两框第 182～187 帧均跟到相邻车，中心离对应真值约 `53～56px`、IoU `0`，自身分数仍约 `0.998～1.000`；第 188 帧才同时回到真目标。可见两框跨初始化中心差 P95 仅 `2.12px`，说明“轨迹一致”也可能同时跟错，不能仅靠一致性判身份。car002 标准/真值可观测 252 帧中心 `1.86/2.84px`、IoU `0.765/0.746`；第 229 帧人工确认无完整框，两框仍输出目标附近框，分数约 `0.9999`，第 230 帧分数也约 `0.9999`，没有完整可见性分界。两目标非可观测段各 13/19 帧，直接 tracker 每帧都输出非空框；上述是独立单跟踪器筛查，未接生产控制器的观察/控制有效性门。
- **裁决 / Handoff**：不把 SiamABC 直接替换 NanoTrack，也不以其高分或两框一致作为恢复身份/完整可见性门；该模型在 car001 的错误邻车与 car002 的无框段均给出高分，直接接入可能误授权控制。CPU 桌面筛查不代表 RK3588 负载、RKNN 可转换性或闭环效果；正式板端八框仍为 `9f84efe` 的 `2/8 PASS`、handoff HOLD。其第 188 帧与原 car001 检测目标位置接近的事实可作为后续交叉验证研究输入，但须先证明 car002 无框反例能被独立排除，再经本分支八框板端闭环；两路中心与初始框一致性条件保持不变。

## 2026-11-14T11:43:16+08:00 一对一局部精修四组否决并撤回并行恢复

- 精确候选 `codex/dual-gt-init-consistency@762e5098ae703c15a8bed39a3cdf6083d1fdf3de`；本机源码包 `/tmp/nt-dual-gt-evidence/search-roi-refined-762e509-source.tar` 与板端 `/home/neardi/Project/SOT/dual-gt-search-roi-refined-762e509/source.tar` SHA-256 均为 `8cd0ba977d302549fa77cdc5c97820604241376a65eec7c192e1762534f56d09`。构建/四组运行脚本 SHA-256 `bad8be57c7da62ffbd30825e0dcf7614292eef3af04feba79b42a9041403c953` / `22746d2552edbea368063913fc3683e4889db66bbb7e271abae11bf38d3f97a7`。输入视频、两目标真值、官方 NanoTrack、VisDrone9 detector/manifest/labels 沿用本文件 `9f84efe` 已哈希绑定身份；板 `LKD3588` Linux `6.1.84`、RKNN API `2.1.0`、driver `0.9.8`。板端原生 CMake/build/CTest `23/23 PASS`，日志 SHA-256 `3fda60c3851a3b2a5be2f13ab3a497a5da14f9efc446f8623e50ccb108bd1250` / `33d52e4bc529315843fe4c7ffcf640128e65a1f423cd80f487ec7606939009f4` / `e35c9c4cbd268f827c7cde8e40a439aa0ef5559299395ce37856b6b40d951f04`；二进制 `build/nanotrack_v3` SHA-256 `a1ac90eedc7b1a154967f40841a1e6c330c545b369f857b44af6355a49e16da3`，headless 配置 SHA-256 `698daa1d551326b35ac1c3eeaa7ead5556b8a3a97859e4b029b7b0255d30f9f8`。
- 实际 `bash run_search_roi_four_refined.sh` 完成 car001 标准/右下、car002 标准/真值各完整 271 帧、exit 0。CSV/session/log 留板端 `runs/<label>/`；板端 `runs/four-text.tgz` 与本机 `/tmp/nt-dual-gt-evidence/search-roi-refined-762e509/four-text.tgz` SHA-256 均 `ac1ebb1c11b7e5249c6c1aa2edfc8289b0abf419f9254b7614fa49dbb3ae7166`。四 CSV SHA-256 顺序 `0967c0022d39da20c63c1eb755d432c25696146d56c721fb5f15ac9bf96dff48`、`263a651b5ccec5788e48804f4af813f394732f8e7686ff308f686f4565b7d55c`、`bcd0d222c00f3ea0e139e89742f5a6f653eb69ea40727953c9ea5ed635157af8`、`0db35077e1938ed51c1e7a912b46c5efdbf07b1884051cd4ff37ffb9bc0b6153`。运行 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ... --output-json /tmp/nt-dual-gt-evidence/search-roi-refined-762e509/four_score.json`；报告 SHA-256 `b54394c3901a15dbd0942d0a8fc5216c95b7d637ce232d27dffd7dc2646e278e`。
- car001 标准框 PASS、第 188 帧重捕，tracking/display IoU `0.748/0.767`、中心 `4.36/4.40px`；右下框 FAIL 完全遮挡语义及重捕，第 196 帧才重捕，IoU `0.692/0.740`、中心 `5.39/5.44px`。car002 标准/真值均 FAIL 完全遮挡语义，第 `235/233` 帧重捕，tracking/display 中心分别 `2.59/2.59px` 与 `2.00/2.00px`。car001 两框 tracking/display 中心差 P95 `14.11/12.80px`；car002 `5.09/5.03px`。本轮仍 `1/4 PASS`，比原 `9f84efe` car001 右下第 191 帧重捕晚五帧；并行双路检测的耗时改进不能解决几何进入恢复候选后改变身份仲裁的问题。没有据此扩测八框。
- **裁决 / Handoff**：撤回本候选及 `9c23736` 的八个算法/配置/测试文件，逐文件与 `1355692` 内容一致；候选提交、固定帧并行探针和两个板端隔离根原始输出均保留。撤回后运行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4` 与 `ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`，日志 SHA-256 `883fc4811c5c3e34a834aa19728407a4c46760ead77ec78ccbed4093a47a3cbb` / `b7270ae624549ce787d2ed0a79fadac2f4381620b13b439474b0850b8d2e6682`，`git diff --check` 通过。后续需要独立身份与完整车体可见性证据，不能再仅靠局部框加入现有候选表或加速推理；两目标八框 tracking/display 中心、IoU、无框语义及初始框一致性仍为验收条件。正式基线 `9f84efe` 八框 `2/8 PASS`，handoff HOLD。

## 2026-11-14T11:37:35+08:00 并行四组失败及一对一局部精修候选

- 精确测试候选 `codex/dual-gt-init-consistency@9c23736c35d97f36f1774e0d21451a17347ba089`，源码包本机 `/tmp/nt-dual-gt-evidence/search-roi-parallel-9c23736-source.tar` 与 RK3588 隔离根 `/home/neardi/Project/SOT/dual-gt-search-roi-parallel-9c23736/source.tar` SHA-256 同为 `492338daddcd7e8524b6f565c0ef83bf08a9041e9a09f006dca3c071daaa303a`；构建/四组运行脚本 SHA-256 `bd4609247bf0d678f74dc87d94399144ea6243e773b3eda01564d478fedd882c` / `cb2f93e14a9e1e84924aacdc5383df366eb782500cc31ef19bd3b854329dc650`。原视频、两目标 GT、官方 NanoTrack、VisDrone9 detector/manifest/labels 沿用已核对 `9f84efe` 基线身份；实际 session 与运行日志实报 RKNN API `2.1.0 (967d001cc8@2024-08-07T19:28:19)`、driver `0.9.8`。板端原生 CMake/build/CTest `23/23 PASS`，日志 SHA-256 `d680e9068df0090d4781e3efe8ff40257992361a990a90efd032061d45584a5d` / `591723369a04af3c9940347998a40a8783c8f574f8ba9bfb399d5ab0d851f99b` / `5345983c55d66f568f9fe266856bb86048dd172b0133b4c4dd7fc3525d7cb0da`；二进制 `build/nanotrack_v3` SHA-256 `7c06c981d826fa994754d205933ae4ae55756a98b5b0f6d81ebc338abcc67d40`，headless 配置 SHA-256 `8cd04937f903a24307ffe367a007f35910f3e944b1dc3b48d2ffa3b3c5e8199a`。
- 实际 `bash run_search_roi_four_parallel.sh` 顺序完成 car001 标准/右下、car002 标准/真值四组各 271 帧，exit 0，板端 `runs/<label>/` 留 CSV/session/log；文本包板端 `runs/four-text.tgz` 与本机 `/tmp/nt-dual-gt-evidence/search-roi-parallel-9c23736/four-text.tgz` SHA-256 同为 `fc28d5266face101fffb1e51e31240bad6a044e94944c7f1ce31197d08f083f0`。四组 CSV SHA-256 顺序 `97ba70972dfa9e4f36abd40fa46e50039614eb0c1bebf71dc1f23a6477d66d27`、`48ea08b761e9e54584181f50a1c37d1d2ca655f1637e319854ff90c55b599638`、`525a757e6fc88b2a125d54aba1c282d07bac40c81e9490a3eb32aea255922eb9`、`3b6d893786cd0cb15e043733b2edeb9ef05d79d48e1085fd0955f476ca2889d2`。用 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ... --output-json /tmp/nt-dual-gt-evidence/search-roi-parallel-9c23736/four_score.json` 评分，报告 SHA-256 `a156754e44cb52812518fbffb1aa030954337c9fc8e50985816a80e58fe9bbc3`。
- car001 标准 PASS，第 188 帧重捕，tracking/display IoU `0.748/0.767`、中心 `4.36/4.40px`；右下 FAIL 完全遮挡语义与重捕，第 198 帧才重捕，IoU `0.686/0.735`、中心 `5.61/5.67px`。car002 标准/真值均 FAIL 完全遮挡语义，重捕 `235/233`，两路中心 `2.59/2.00px`（分别）；car001 两框 tracking/display 中心差 P95 `15.47/14.16px`，car002 `5.09/5.03px`。恢复检测墙钟本轮约 `68～101ms`，比顺序双路 `aaf6ab4` 的约 `98～178ms` 明显下降，但只有 `1/4 PASS`，不能扩八框或晋级。对比原基线 car001 右下第 `191` 帧重捕，本轮虽然比顺序双路第 `207` 帧提前，仍晚七帧。
- 帧级对照：car001 右下第 188 帧原基线收到全画面候选后第 189 帧转 detector probe、第 191 帧两击重捕；本轮第 189～191 帧持续 `IDENTITY_CONFLICT`，第 194～196 帧又冲突。已冻结的局部 detector 回放在第 188 帧同时给出邻车 `(896.4,850.7,41.7,38.6)`、目标附近 `(844.9,825.7,50.3,44.2)` 和低置信度宽框 `(844.0,815.0,91.6,54.6)`；原合并条件允许该宽框与约 `(844.5,821.3,48.1,46.3)` 的全画面目标框匹配，并可再次覆盖同一全画面身份及置信度。这是一个由几何和代码直接证实的**潜在**冲突机制，不能仅凭 CSV 断言它就是每次冲突的唯一原因。
- 基于此反例，本项新候选只允许局部框与**原全画面同类框 IoU >0.60** 的一对一精修；每个全画面身份最多取最高 IoU 的局部框，只替换几何、不替换全画面置信度、不添加局部独有候选。双实例并行、局部 320 内容、原 NanoTrack 身份确认与默认关闭语义保留；尝试保住远处竞争车反证并排除宽框重复覆盖。实际 `git diff --check`、`cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`，日志 SHA-256 `4bb74ef6b2564112f8051f9eed2feaa74ce450b6bb181e0b8d40ca0168f06236` / `a921dee3d42caac67cbb1dde7c954f137917d4ace7c6c363ea730c3c75c5c6b7`。**下一步 / Handoff**：新候选尚未在 RK3588 闭环；需以精确提交隔离部署，先跑同四组，若 car001 标准/右下、car002 无框语义或两路中心不能同时改善则撤回。正式基线 `9f84efe` 八框 `2/8 PASS`、handoff HOLD。

## 2026-11-14T11:29:23+08:00 双 RKNN 实例并行恢复候选（待闭环）

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，起始干净、基线 `1355692d8e66fbfd67aedfdaf4578be50a237e1d`。上一双路顺序候选 `aaf6ab4` 的恢复推理接近 `170ms`，car001 右下重捕延至第 207 帧；本项先核对 RK3588 SDK 提供 `RKNN_NPU_CORE_0/1/2` 与 `rknn_set_core_mask`，板端约 `7.2GiB` 可用内存，再实测两个独立 RKNN context 并行的可行性，不假设顺序失败只由算法定位引起。
- 只读延迟探针源码本机 `/tmp/nt-dual-gt-evidence/parallel_yolo_probe.cpp`、板端 `/home/neardi/Project/SOT/dual-gt-handoff-9f84efe/runs/parallel_yolo_probe.cpp` SHA-256 同为 `03e6cfc7fd882568c1ba348491f219e23dcb246e47efd0363a245d575c6b7f3b`；以基线 `9f84efe` 的静态库在板端 `g++ -std=c++17 -O2 -pthread ... -lrknnrt -ldl $(pkg-config --cflags --libs opencv4)` 原生编译，探针可执行文件 SHA-256 `1035caadbf1ae9d35123831fa5511bcb1365ec0eacccc0f9cba5b1d49aa778b3`。输入是已哈希绑定的 `occ_video_1/1.mp4` 第 187 帧、同一冻结 VisDrone9 detector；一实例全画面、另一实例局部 `180×180` 内容缩至 320 后置于 640 画布，预热各 3 次，再各跑顺序/并行 12 次。输出板端 `runs/parallel_yolo_probe.csv` 与本机 `/tmp/nt-dual-gt-evidence/parallel_yolo_probe.csv` SHA-256 同为 `15fa2b8fae53b49b9a8b57e1dc6a89a738e3f7c9f112d749413fe367c84a7c8e`，板端 stderr 空文件 SHA-256 `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`。墙钟顺序均值/中位 `169.06/169.69ms`，并行 `85.70/84.22ms`；两模式候选数全画面/局部均 `100/4`。这是固定帧算力探针，不是完整视频追踪或候选正确性证明。
- 候选恢复 `aaf6ab4` 的默认关闭局部 ROI 与全画面竞争候选合并路径；当前可见配置 `detector_recovery_roi_scale=3.0`、SHA-256 `c6c18974e88376cc7567a518b2fb9a52877a16f62ab4146f08c29a3711b95326`。当该配置开启时，启动时创建两个独立 detector RKNN 实例，恢复局部请求用 `std::async` 并行全画面与局部推理；结果仍合并并经过原 NanoTrack 候选身份确认，detector 不直接授权控制。记录的 `detector_total_ms` 为并行墙钟时间；初始化、显示刷新与配置关闭的普通路径仍只用首实例。实际 `git diff --check`、`cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`，日志 SHA-256 `13685a0bfe2826c3ca70b9e857f10fd5b6bf135a0e0958de3c4e3f91838bb646` / `2d795fcd82259e2a03b292177ef843eceb874b56f417b45186c451286afd9f09`。
- **证据边界 / Handoff**：候选还未在 RK3588 闭环运行，主机 CTest 不编译 RKNN 路径，固定帧并行探针没有 tracker 与 detector 的并发负载。下一步用本项精确提交源码在板端隔离根原生编译、CTest，并先跑 car001 标准/右下及 car002 标准/真值四组 271 帧。只有双路中心、IoU、无框语义、重捕时间及跨初始框差不退化且有整体改善，才扩到八框；否则撤回本候选。正式已验证基线 `9f84efe` 八框 `2/8 PASS`，handoff HOLD。

## 2026-11-14T11:22:40+08:00 MOT 遮挡 CNN 独立筛查否决

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，本轮开始干净，基线 `5b781937a5f82bc10b16bbe818719b12c6f5b546`。正式算法、模型、配置、两目标真值均未改。为寻找跨帧身份之外的完整车体可观测性证据，试用本机外部 `/home/u20/dataset/VisDrone2019-MOT` 汽车 `category=4` 标注，要求有效目标、`truncation=0`、目标宽高均 `25～90px`；按数据集原 train/val 序列划分，分别抽取无遮挡/遮挡一级/二级 `4000/2000/2000` 与 `500/250/250` 图块。两目标 canonical 视频及其真值仅在训练后作反例检查，未用于训练或调参。
- 筛查脚本 `/tmp/nt-dual-gt-evidence/screen_mot_visibility_cnn.py` SHA-256 `177d4cb661b98e1a843cf4445f2a8f25aa6250e727508ed6e271688f431e47bd`；每个标注框连同 1.4 倍上下文裁为 `64×64`，小型四层 CNN 用 `.venvs/nanotrack/bin/python` 中 PyTorch `1.7.1` CPU、固定种子 `20261114` 训练五轮。实际命令 `.venvs/nanotrack/bin/python /tmp/nt-dual-gt-evidence/screen_mot_visibility_cnn.py --epochs 5 > /tmp/nt-dual-gt-evidence/mot-visibility-cnn/train.log 2>&1`，退出 0。训练/验证缓存 `train.npz` / `val.npz` SHA-256 `ca162b72371ef07e0eda3eeaaf6692ab3b2f863377cc4109b5b310c7a1563035` / `b0ab74d77b4c86bd333c6d9e681c18d66c562ab097414b1eb06212286f923ec2`；模型 `model.pt`、报告 `report.json`、日志 `train.log` 留本机 `/tmp/nt-dual-gt-evidence/mot-visibility-cnn/`，SHA-256 `d7adb8bd5d046f75846b66e391f1dbf5bfda476b35fdc5dc93af71be97289371` / `9f46626375e8fbd470276d686b52f79c99d22c65b6a0ceec89e405f56dcc68f0` / `a8a0760c49605da026dfd57a9f3e496120f47cbca77302361cdb55392059056b`。
- 独立验证集 `p_full` 区分无遮挡与遮挡的 AUC 五轮依次为 `0.696/0.637/0.717/0.734/0.731`，末轮三类准确率 `0.580`。在既有 `9f84efe` RK3588 CSV 的恢复原始框上离线打分，car001 右下初始框第 187 帧 `p_full=0.468`（该帧正式车体已可见，但探针仍偏离），第 190 帧可靠候选为 `0.869`；car002 真值框第 229 帧人工确认无完整框的候选为 `0.633`，第 230 帧已完整可见候选反而 `0.608`。统一阈值既不能阻止无框段错误授权又保留正确候选；该模型也没有提供车辆身份判别。
- **裁决 / Handoff**：否决该 CNN 直接接入恢复、tracking/control 或 display。此项只有主机离线筛查，没有 RK3588 推理、闭环轨迹或时延；保留 `9f84efe` 正式八框 `2/8 PASS` 与 handoff HOLD。把训练/验证分割、权重、反例及此前 223 项 RKNN 模板特征证据交给模型/外观 owner 设计身份加完整可见性验证器；必须在未用 canonical 反例调参的条件下取得独立判别力，再回到本分支做两目标八框板端闭环。

## 2026-11-14T11:07:27+08:00 双路检测四组板端否决并撤回

- 精确候选 `codex/dual-gt-init-consistency@aaf6ab4bd4c4cf4fb24fe9bccc75ae76eaf3b5ad`，板端隔离根 `/home/neardi/Project/SOT/dual-gt-search-roi-dual-aaf6ab4`。源码包本机 `/tmp/nt-dual-gt-evidence/search-roi-dual-aaf6ab4-source.tar` 与板端 `source.tar` SHA-256 均为 `c54798762e8fdbda8a90c92886d6df6ee9066e6f30792348917457f71db556b1`。输入视频、GT、模型和板端环境身份沿用本文件所列同板基线，并在本轮与原路径核对。板端原生构建和 CTest `23/23 PASS`；二进制 `build/nanotrack_v3` SHA-256 `55a524cab9a6c4da0aa06d0369c081084e70271366f162205b4c14880cd1f60b`，headless 配置 SHA-256 `0e8125b0d969eb5023746731bd6e0050007af7a0a474ac9d08ed968b9a1a1fdf`。构建和运行脚本本机 `/tmp/nt-dual-gt-evidence/build_search_roi_board_dual.sh`、`run_search_roi_four_dual.sh` SHA-256 分别为 `ee88e8828a36044689968a7bd49c7c6cb12bf5f5b7bb32516f80e52a563b7213`、`ec6d23559bf9be65cf5631e12e6e775060048c6f623cb0b91dff2a25263c722b`。
- 实际执行 `bash runs/run_search_roi_four_dual.sh`，car001 标准/右下及 car002 标准/真值各跑完整 271 帧，exit 0。CSV/session/log 保留板端 `runs/<label>/`；文本包板端 `runs/four-text.tgz` 和本机 `/tmp/nt-dual-gt-evidence/search-roi-dual-aaf6ab4/four-text.tgz` SHA-256 一致为 `ecab297addc749a31786012c6a0f3a63c624dcb53156deea20b5bdc532e39de4`。四份 CSV 顺序 SHA-256 为 `88f3f55d467962ee274d426009674a8a13037c631b0ae4f537c6d380fa13933a`、`6afb2a91e728dce6db06bb9c82ce2f47781949262415374b2713927662b9dea8`、`b82e162b2fc61085630c11898823696801b4bd067e4b4a47db3936e227dded92`、`a2806999370368539365bcfc0ed4f490ee452fea4a476c491e96a296f65d8059`。
- 用 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ... --output-json /tmp/nt-dual-gt-evidence/search-roi-dual-aaf6ab4/four_score.json` 独立评分；报告 SHA-256 `49820e8f5b269cc0e4a0c43051b67a1db3cd9177077c992f27883d4666a2d48d`。car001 标准框 PASS、重捕 189，tracking IoU/中心 `0.748/4.37px`，display `0.767/4.41px`；右下框 FAIL 完全遮挡语义及重捕，第 207 帧才重捕，tracking `0.662/6.53px`，display `0.711/6.61px`。car002 标准/真值框均 FAIL 完全遮挡语义，分别第 235/233 帧重捕；tracking IoU/中心 `0.791/2.59px`、`0.883/2.00px`，display `0.741/2.59px`、`0.750/2.00px`。car001 两初始框 tracking/display 中心差 P95 `22.66/23.39px`，car002 `5.09/5.03px`。本轮仅 `1/4 PASS`，不足以扩测八框；`display_bbox` 的 detector 观感不替代两路中心真值评分。
- 双次恢复 detector 推理在本轮多次达到约 `170ms`，原基线约 `90ms`；时序和重捕结果均退化。**裁决 / Handoff**：撤回该候选六个生产/配置/测试文件至 `0ca0c01` 内容；`git diff --name-only 0ca0c01 -- <六文件>` 输出为空。撤回后实际运行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4` 与 `ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`，日志 SHA-256 `c3f3d7803772ac628759aa21aba8e71ebacba593c89076b8be5c14f91ac3386e` / `48e0d9c17884b413afc22e14d4f8b2559c4a10f77de48bbfee5f6cc1ba59a913`；`git diff --check` 通过。保留候选提交、隔离板端工件和本记录。下一步检查跨帧身份与完整目标可观测性证据，先证明方案有独立判别力，再作 RK3588 八框闭环。正式基线仍 `9f84efe` 八框 `2/8 PASS`，handoff HOLD。

## 2026-11-14T11:00:21+08:00 全画面竞争证据与局部定位联合恢复候选（待板端闭环）

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，开始干净，基线 `0ca0c015760709355f392572aa4b69eddc082bb2`。对照 `9f84efe` 基线、`85fedbd` 局部 ROI 和 `8473b98` 局部加距离门三组已核对板端 CSV：car001 标准框第 179 帧 recent 探针 `(844.67,803.59,49.74,47.99)` 三者相同；基线全画面 detector 批次仍有 `3` 个竞争候选、`IDENTITY_CONFLICT` 阻止错误重捕；仅局部 ROI 是 `2` 个，距离门又过滤到无可竞争候选，于第 179 帧以该尚在遮挡下的探针错误重捕。说明远处竞争车虽不能直接授权控制，却作为**排除错误身份的反证**有实际作用；前两次局部候选失败不是单纯位置门过松。
- 新候选恢复默认关闭的 `detector_recovery_roi_scale` 可选路径，当前可见配置取 `3.0`（SHA-256 `c6c18974e88376cc7567a518b2fb9a52877a16f62ab4146f08c29a3711b95326`）。`SearchRecovery` 局部请求在同一 RKNN 上顺序执行一次原 640 全画面推理和一次 320 局部内容居中 640 画布推理；同类、中心接近且宽高比相近的候选合并，以局部候选更新位置，其余全画面竞争候选保留进入既有 NanoTrack 身份确认。初始化、普通显示刷新、默认未开启局部 ROI 的路径只运行原来一次推理；控制授权仍须 NanoTrack 验证，detector 不直接发布 `control_valid=true`。两次推理可能超过异步队列与 `detector_stale_ms` 的可用时限，合并阈值也可能错误合并邻车，均须板端实测。
- 实际执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`，日志 SHA-256 `355b4e55faef106ba0123a17db64482bbea56feda4a3b4ec614f7976f2599b02` / `611a7be6df510cfe5715ae0b7131b6e0d35c9ae9f52cc3eff2ae02ea480a0f78`；`git diff --check` 通过。**证据边界 / 下一步**：主机单测没有运行 RKNN；需以本条实际提交在 RK3588 隔离目录原生构建，先跑 car001 标准/右下、car002 标准/真值四框完整闭环，核对检测耗时与过期、错误竞争和真值中心/IoU/遮挡/重捕。若仍有 car001 错捕或显著延迟，撤回本候选，不以离线局部预测晋级。正式八框 `2/8 PASS`，handoff HOLD。

## 2026-11-14T10:55:37+08:00 邻车距离门禁四组板端否决并撤回局部恢复代码

- 精确候选 `codex/dual-gt-init-consistency@8473b98275a024970bf855f536cb3207df3fcd6c`；源码包 `/tmp/nt-dual-gt-evidence/search-roi-gated-8473b98-source.tar` / 板端 `/home/neardi/Project/SOT/dual-gt-search-roi-gated-8473b98/source.tar` SHA-256 一致为 `1edc4c655b42eab7b61c550b9be2581d89cc16672fbe892ed9c5cb6ba6fa7189`。板端独立根 `/home/neardi/Project/SOT/dual-gt-search-roi-gated-8473b98`，原视频、模型/manifest/labels 和 RK3588 环境沿用上节已核对身份。原生 CMake/构建/CTest `23/23 PASS`；`build/nanotrack_v3` SHA-256 `b407018f6906345209225686cb64b55295bbbcd6a39ca018387a5a1f882ee82f`，headless 配置 SHA-256 `5a816df3e451f2d44c5b114e20d081251f717cf60d9717573d9179a89cc54690`。实际构建/运行脚本 `/tmp/nt-dual-gt-evidence/build_search_roi_board_gated.sh` / `run_search_roi_four_gated.sh` SHA-256 `a441c33e4290196202c4d148a1e54e3b46f1a8bfb939bfe525c9d6ffadcaeb1f` / `35bb55e6ff377c15efb0b00dd201a520edbe5305019370eaf3ecdf743b293058`。
- 板端执行 `bash runs/run_search_roi_four_gated.sh`，car001 标准/右下、car002 标准/真值各 271 帧、exit 0；CSV/session/log 保留 `runs/<label>/`，文本包板端 `runs/four-text.tgz` 与主机 `/tmp/nt-dual-gt-evidence/search-roi-gated-8473b98/four-text.tgz` SHA-256 一致为 `05363f1f977d3e23d7b6531930a73dce252646cfdfe5d13b16dcb18d6321c05f`。上述四框 CSV SHA-256 顺序为 `2c97a8134e66d3a3e4c5f22910b2f0945c74b8079b261c4816daaf88c5cd8bbc`、`f6063020a3d58442898dd390956893e02b340fcde398d924a92a2bf3e482d907`、`5e6c02c2ba9e6837a961a5e9ff7489972adb5f9e9aec94995f23862859f61e43`、`767c5a0d1388bb5c6444b86d3910496f5d261d973212d5e7e09643e2d11ac55`。用匹配真值执行 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ... --output-json /tmp/nt-dual-gt-evidence/search-roi-gated-8473b98/four_score.json`，报告 SHA-256 `cfe99192d9c334eea1fe6431ecf5fdd554d387949dd2d8c50a4604ccff2b1044`。
- 结果四框 `0/4 PASS`。car001 标准框在 `179/197/220` 帧三次错误重捕，tracking/display 中心均误差 `7.70/7.90 px`、IoU `0.658/0.677`；右下框虽由无门禁第 198 帧提前到 196，仍晚于第 190 帧严格截止且中心 `5.39/5.44 px`。两框中心差 P95 `29.78/30.48 px`，远大于正式基线。car002 标准/真值仍因完全遮挡语义 FAIL，重捕 `234/233`，中心 `2.05/2.00 px`。邻车位置门虽使近旁候选数减少，却在 car001 早期把错误近候选聚焦到恢复链，不能据此授权控制。
- **裁决 / Handoff**：完整闭环已否决局部搜索 ROI 与邻车距离门的组合；将 `visible-adaptive-display.conf`、配置解析与字段、恢复 ROI 生成、RKNN 半幅画布路径及其单测恢复至本候选前 `f69b5f508c98b96cfb0fcbba83235d08ca80ec2a` 的正式算法内容，`git diff f69b5f5 -- <上述六文件>` 无差异。恢复后实际执行主机 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，RKNN-off `23/23 PASS`；日志 SHA-256 `29737c17eeda07f7def734ff98d2f587b482009479653c18b5df4769c3a2b460` / `a8ea2dead926f8211de14f8398f88594effb88bf158aba48033e292ae0569715`。保留候选提交、板端隔离证据和独立只读 `yolo_roi_replay` 探针，不向生产交接该候选。正式八框仍 `2/8 PASS`，handoff HOLD。后续恢复身份须从模板/外观的连续性证据解决，不能只靠最近距离或缩小 detector 搜索域。

## 2026-11-14T10:49:29+08:00 搜索局部 ROI 四组反例与邻车候选门禁修订

- 精确候选 `codex/dual-gt-init-consistency@85fedbd2d045cc760b0bd08e5a95a444856c45d7`，源码包 `/tmp/nt-dual-gt-evidence/search-roi-85fedbd-source.tar` 与板端 `/home/neardi/Project/SOT/dual-gt-search-roi-85fedbd/source.tar` SHA-256 一致为 `567fca9ab75ce9994ef58ee13f06b6f31de31940e4e442333da69a66ecb8150e`。板端 `LKD3588` Linux `6.1.84`、RKNN API `2.1.0`、driver `0.9.8`，原生 CMake/构建/CTest `23/23 PASS`；`build/nanotrack_v3` SHA-256 `abf54e02fbf17368a1c91d16a7e5c00488af2ea88b203957e1589a5f4d4523d3`，`runs/headless-eval.conf` SHA-256 `e9621f9ad27bf6d24d1c911700be3168bb1d0874aca7fb57df22b8628949fc82`，源码配置 SHA-256 `c6c18974e88376cc7567a518b2fb9a52877a16f62ab4146f08c29a3711b95326`；原视频、模型/manifest/labels 与 `9f84efe` 基线相同且此前已哈希绑定。构建脚本 `/tmp/nt-dual-gt-evidence/build_search_roi_board.sh` SHA-256 `d12b1cb814c93ad1e2971be8b18d424ef27265c4afc4309b0c06d3922969a28f`。
- 实际运行板端 `bash runs/run_search_roi_four.sh`，主机脚本 SHA-256 `956d104faf93038a9898b5ce392765aeaa72686b90d24afb7034c930f11e7ee7`；car001 标准/右下和 car002 标准/真值四框各完成 271 帧、exit 0，CSV/session/log 留 `/home/neardi/Project/SOT/dual-gt-search-roi-85fedbd/runs/<label>/`。主机回传 `/tmp/nt-dual-gt-evidence/search-roi-85fedbd/four-text.tgz` 与板端 `runs/four-text.tgz` SHA-256 一致为 `255f659d9f034c30e1a19fceca4a9e7e3fc3a3e15623225dc2e30e49f2de3d85`；按上述四框顺序 CSV SHA-256 为 `b632cdcfc323adf183fa0e9cdd58f297df41a1a0e0acfa546fe1ea13bac9b4df`、`699389268085d59a171c2c2043ccf24de6cd3e60f355900d17d992f8a91fcc16`、`38b5bba597bbbad048f3feae5a90f6e1d042a2c62c2001b332a841c9eb67042d`、`bdeb812e4ef733b3e087e7b5fd30eaafbccb15120d9ca8f8dd73f822b77e71f6`。执行 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ... --output-json /tmp/nt-dual-gt-evidence/search-roi-85fedbd/four_score.json`，报告 SHA-256 `d1b300e8abd4570289d417a52116c1d7d83bb1392f4bbd0f146410ae77dc9c1e`。
- car001 标准仍 PASS、第 188 帧重捕、tracking/display 中心 `4.36/4.40 px`；右下仍 FAIL 且重捕由基线第 191 帧延至 `198`，中心由约 `5.02→5.61 px`，两框 tracking/display 中心差 P95 `15.47/14.16 px`。car002 标准/真值均因 `full_occlusion_semantics` FAIL，重捕 `234/233`，中心 `2.05/2.00 px`。第 189、194 帧 car001 右下局部检测批次各仍保留 `2` 个同类候选，第 188～191 与 194～196 帧连续 `IDENTITY_CONFLICT`；正确目标候选虽在局部区域，近旁车辆仍进入统一恢复候选竞争。因此仅裁 ROI 不够，四组已有决定性退化，未扩八组。
- 在该反例上修订：仅当显式开启 `detector_recovery_roi_scale` 时，对已映射回原视频的候选加目标尺度距离门，中心距运动预测不超过 `0.8×max(预测宽,预测高)`，在统一恢复候选竞争前排除邻车；默认关闭的全帧路径不变。单测包含 ROI 内近车/邻车并检查 `1` 个保留、`1` 个位置拒绝；实际执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`，日志 SHA-256 `fea66b240a24d57054f0a5fdae9484630fe6e8eb2264a6ab08f28ea071697f9d` / `2d741aff3fa378ba926e72b182fe3e116d412fbd0e532c17d7f39b909feb7daa`。**证据边界 / 下一步**：修订版还未上板；须按修订提交重新独立构建并跑四框。若 car001 右下仍晚重捕或任一目标中心/身份变差，应撤回局部检测候选，不能以单测替代闭环。正式基线仍 `2/8 PASS`，handoff HOLD。

## 2026-11-14T10:40:22+08:00 搜索恢复局部检测候选（待 RK3588 闭环）

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，起始干净、基线 `f69b5f508c98b96cfb0fcbba83235d08ca80ec2a`。在 `ControllerConfig` 增加默认 `0` 的 `detector_recovery_roi_scale`；只有配置显式设为 `3.0` 且实际发 `SearchRecovery` 请求时，用运动预测框中心、至少 120 px 的方形 ROI 裁图。RKNN 固定输入仍 640×640，但把该搜索 ROI 缩至 320×320 后居中灰底，postprocess 继续映射到原始视频像素。初始化类别检测、显示刷新、无 ROI 的恢复请求均保留原行为；检测候选仍须经既有 NanoTrack 恢复确认才能令 `control_valid=true`。`visible-adaptive-display.conf` SHA-256 `c6c18974e88376cc7567a518b2fb9a52877a16f62ab4146f08c29a3711b95326`。
- 依据先前完整板端 ROI 探针：car001 第 181 帧错误邻车与正确预测位置相隔约 70 px；3 倍局部 ROI 的半径约 60 px，有望排除该邻车；第 187～190 帧同一 ROI 内的正确车辆候选距真值约 `1～5 px`。这只是反事实动机，局部 ROI 也可能丢失运动预测误差过大的真实目标，或改变异步检测与确认时序；car002 完全遮挡语义尚未解决，不能据此晋级。
- 在 `test_long_term_controller` 增加一个恢复 ROI 包含预测中心、尺寸和帧边界的单测；实际执行 `cmake -S rk3566/nanotrack_v3 -B /tmp/nt-dual-gt-consistency-build -DENABLE_RKNN_RUNTIME=OFF -DBUILD_TESTING=ON`、`cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`，`git diff --check` 通过。**证据边界 / 下一步**：主机未执行 RKNN，候选还须以本条实际源码提交在 RK3588 独立目录原生构建和跑 car001 标准/右下、car002 标准/真值，再按各自确认真值检查中心、IoU、完全遮挡、重捕、身份及延迟；只有无反例才扩八框。正式八框仍 `2/8 PASS`，handoff HOLD。

## 2026-11-14T10:34:34+08:00 稀疏局部检测修正与遮挡外观的双目标再筛查

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，基线 `b97cfc305c90bdb42edba2d34a04e1d27a18e64f`、起始干净。仍只读使用已验证的原始 canonical 271 帧、`9f84efe` RK3588 八组 CSV、上一节八组真实 RKNN ROI 候选 CSV 和各自确认真值；没有更改正式模型、算法、配置或标注。实际执行 `python3 /tmp/nt-dual-gt-evidence/search_sparse_roi_policy.py > /tmp/nt-dual-gt-evidence/search-sparse-roi-policy.txt`，脚本/输出 SHA-256 `590dd59d2408d3ef0d1a01c3671312686ebc93783b1600fa718bb0bf31982c36` / `0c827fb9439c8a7095f7664c240866594db984dcebd78eca45f5f6b8bf96011d`。在固定基线轨迹上扫描 detector 每 `3/4/6` 帧请求、`2/3` 帧回报、中心权重、逐帧衰减、置信门和邻近门的 1728 组反事实；其中 440 组八个平均中心误差均不劣于基线。按最坏单框中心均误差排序的首组为每 6 帧、延迟 2 帧、权重 0.5、每帧保留 0.8、conf≥0.2、候选中心距≤0.3 倍框长，八框中心改善仅 `0.014～0.449 px`，car001 左上仅 `0.014 px`。这是用这八组真值选择参数的同集离线筛查，改动后下一帧 tracker 会反馈且异步时延会变化；不能把 440 组或首组称为泛化、板端闭环或算法完成，暂不接入 tracking/control。
- 另实际运行 `.venvs/nanotrack/bin/python /tmp/nt-dual-gt-evidence/probe_visible_fraction.py > /tmp/nt-dual-gt-evidence/visible-fraction-eight.txt`，脚本/逐帧特征 JSON/文本 SHA-256 `6bb9a2f95cc3ae2a5d3020f55eb923a3aeb280d456c959a1f7af53afbf44ffd3` / `41938b45729d023289c6aff2f7032515ad250aba84449e77e8a3e2fcf6a7445e` / `afb0e38b1247bcd3edb900cee4cb3a727ab8f962d9243062984aee88f014b920`。用每帧基线 tracking 框内灰度 90 分位、亮像素/白像素占比及边缘密度，对首段稳定可见帧的中位数归一化。配对图 `/tmp/nt-dual-gt-evidence/car001-occlusion-contact.png` / `car002-occlusion-contact.png` SHA-256 `0d7e4a89b194f648cdb95e2d88c7eee9be8757a76c713c86120171e65e3ed5d6` / `f13ae07ac8ba5224b31bdd544bd03ef862ed302b3b027528c56bbe78d4fb35ca`，原视频裁片中蓝框为基线 tracking、绿框为已有真值。两车进桥遮挡时框内灰度 P90 下降：car001 四框第 164～165 帧约为稳定段 `0.91～0.92`，car002 四框第 212～213 帧约 `0.94～0.95`，而 car002 第 215～216 帧降至约 `0.68～0.71`；说明该信号的下降时序和深度随目标不同。简单 `P90 < 0.95×参考` 连续两帧的 car002 首触发在 213，但早期仍在可观测第 63～76 帧误触发 `10～15` 帧。独立亮度阈值不能安全地提前转 SEARCHING，也不证明恢复候选身份；需要结合状态与时间证据，并在完整八框闭环检验后才可接入。
- **裁决 / Handoff**：保持 `9f84efe` 正式算法基线 `2/8 PASS` 与 handoff HOLD。下一步只考虑可同时避免 car001 遮挡后错车重捕、car002 第 213～216 帧完全遮挡语义失败、并改善两目标 tracking/display 中心和初始框一致性的因果候选；离线小幅中心修正与单一亮度门均不满足。

## 2026-11-14T10:24:35+08:00 首帧局部 detector 中心归一化八组 RK3588 闭环否决

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，基线 `73d25036f092a244dfbb2b5b6c9f023b8ba803be`，开始时干净。精确使用 `9f84efecd57d611a239cc71c99ab2e4295025dbc` 板端二进制 SHA-256 `41d83ef0d772833b29400a35cacb8825a1a402f24ea7dbfe9279ac09b062343b`、原 `runs/headless-eval.conf`、canonical 视频及两目标确认真值；模型、算法、真值不变。从前一节同一原始首帧 RKNN ROI 探针的八组候选中选择距人工框中心最近的 `car=2`、conf≥`0.1` 且几何匹配者，按 0.75 倍中心差移动初始框、保持人工宽高。八个新初始框由 `/tmp/nt-dual-gt-evidence/run_init_center_board.sh`（SHA-256 `bc3aaf5faa398d055e29e35550beeedb3453151c15e89de08bb9f68c15e4aeb2`）逐项保留；这只修改测试输入，不改正式初始化代码。
- 实际在 `/home/neardi/Project/SOT/dual-gt-handoff-9f84efe` 运行 `bash runs/run_init_center_board.sh`，每组 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox ... --output-boxes runs/init-center-<label>/frames.csv --session-metadata runs/init-center-<label>/session.json`，八组各 271 帧、exit 0，板端 CSV/session/log 全留 `runs/init-center-<label>/`。板端归档 `runs/init-center-eight.tgz`、主机 `/tmp/nt-dual-gt-evidence/init-center-eight.tgz` SHA-256 一致为 `773649f519cf224e7b71280e295bf4922ae2d6a8a1d3b8ce8cc4eb78bb4a796f`。实际执行 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ... --output-json /tmp/nt-dual-gt-evidence/init-center-eight/score.json`，评分报告 SHA-256 `4edd72183868151d9e66ef116c17a09d00eb8a1565708ee4500fca38303855a6`。
- 结果 car001 nominal/user/right PASS，left FAIL；左上框虽仅变为 `(990.6674,580.5800,39,41)`，重捕从基线 188 附近推迟至 201，中心均误差从 `4.34→5.32 px`，导致四框 tracking/display 中心离散度 P95 `23.32/24.13 px`（基线 `3.31` 附近）。car002 四组全部 FAIL `full_occlusion_semantics`，重捕在 `232～233` 帧；其 nominal/truth/left/right tracking 中心均误差 `2.00/1.87/2.15/2.59 px`，跨框中心离散度 P95 `7.28 px`，高于基线 `6.21 px`。虽然 car001 PASS 个数由 1 增到 3，整体仍只有 `3/8 PASS` 且最坏稳定性明显退化。**裁决 / Handoff**：否决首帧中心归一化候选，正式代码不接入；此前每帧局部 detector 的冻结轨迹收益和本次闭环结果不可混同。正式八框仍 `2/8 PASS`，handoff HOLD。后续需针对遮挡判定和重捕身份的因果路径提出新的板端闭环候选，且两目标 tracking/display 中心、IoU 与跨初始框一致性须同时过门。

## 2026-11-14T10:15:37+08:00 RK3588 局部 detector 八组全帧回放与稀疏时延否决

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，探针源码提交 `b5ca2890f61f1cf806c84b8ebe34f450d622129f`，板端隔离根 `/home/neardi/Project/SOT/dual-gt-roi-probe-b5ca289`。输入 canonical 视频 SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`、八组 `9f84efecd57d611a239cc71c99ab2e4295025dbc` 基线 tracking CSV、对应两份确认真值；VisDrone9 RKNN/manifest/labels SHA-256 `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c` / `ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9` / `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`；板端 RKNN API `2.1.0`、driver `0.9.8`、Linux `6.1.84`。源码包 `/tmp/nt-dual-gt-evidence/roi-probe-b5ca289-source.tar` SHA-256 `60db0a8a624008f8e9cde446b0bffa691bc20b4ec365e62c3ada721894536c33` 已上传并核对；原生 CMake 构建和 `ctest` `23/23 PASS`，`build/yolo_roi_replay` SHA-256 `5d8949d9907ae4b18b35205f4183c7811d6bbb19e66807a9f3126b17f51f2fd9`。
- 实际运行板端 `bash runs/run_roi_replay_board.sh`，主机脚本 `/tmp/nt-dual-gt-evidence/run_roi_replay_board.sh` SHA-256 `cfc03dacb7a0271b2fe2bd91fa9e5cb93184fa23a026d91314caf1f5b77d8ff5`。八组各读取原始视频 271 帧和各自冻结 ROI trace，检测输出在板端 `runs/outputs/<label>.csv`、日志在同目录；回传同名 CSV 至 `/tmp/nt-dual-gt-evidence/roi-detector-board-probe/full-eight/`，按 car001 nominal/user/left/right、car002 nominal/truth/left/right 顺序 SHA-256 为 `9985ee407c1d6c9393dfb19f5eec0fe201a63aedf4c137b314a6a30fe3e707a3`、`d3eb3aa3b2adf0a3a3214256ffab9021a7274c612d3349dc6531998a76d17f17`、`c712783da86b7d90209ae4ce409a50792e84c58d6db012a1b4c44f17498d5f2e`、`8e7916951a01bd411787331deb4ee5ef53ca2f34235563f31fd651437072e89f`、`46399f6a2705d15200742dc23dd285ab76c1eb449f7b113b261a3945797cfb10`、`89dbe3b47421efe9d419a5472277bb751ccfe5ae082f500cb8020951ec75c9f9`、`8a151d947651d63e0923d87ba9a530b75d9b76761f5b8c78ce35953be0a90330`、`f2d18a3b0db4ddf079ead29978037e00eaff725a44cf8c924535e2abf3e74cc2`。评分脚本 `/tmp/nt-dual-gt-evidence/score_roi_board_eight.py` SHA-256 `c7ba5d78b0c10231aa0bd36ec91ebf35d8d423754abbb27f978b2182299675dd`，报告 `/tmp/nt-dual-gt-evidence/roi-detector-board-probe/full-eight/score.json` SHA-256 `d10acf36b9a8533e0eaf1260bae46f69e15f4c87c93fe430e4983abbb41da45c`。
- 离线冻结轨迹反事实：`TRACKING` 且有效观测时只融合 detector 中心、保留 tracker 宽高，中心差 `<3 px` 乘 `0.5`，否则乘 `0.75`。car001 四框中心均误差 `4.28/4.63/4.34/5.02→4.14/4.29/4.27/4.78 px`，IoU `0.751/0.714/0.773/0.701→0.757/0.722/0.777/0.705`；car002 中心 `2.59/2.00/1.77/2.92→1.88/1.80/1.63/2.06 px`，IoU `0.791/0.883/0.881/0.728→0.816/0.888/0.882/0.749`。跨四框中心差 P95 car001 `3.31→3.28 px`、car002 `6.21→2.72 px`。但实际 detector 耗时平均 `78.12～79.46 ms/次`，无法按 30 fps 每帧调用。
- 执行 `python3 /tmp/nt-dual-gt-evidence/score_roi_sparse.py`（SHA-256 `2215ea6c8c0cd5820a5e72b41814d7d8e990dbd5592985ccced082ca55876dde`），输出 `/tmp/nt-dual-gt-evidence/roi-detector-board-probe/full-eight/sparse-score.txt` SHA-256 `9b284af2529aae9b1134690de750238108a8be12c7d4268dd3becd9f32e8f64d`；模拟每 3/4/6/10 帧一次且延迟 0/2/3 帧后沿用中心偏移。每 3 帧、延迟 2 帧时 car001 左上 `4.34→4.40 px`、car002 左上 `1.77→1.93 px`；每 4 帧、延迟 3 帧时 car001 右下 `5.02→5.04 px`。所以全帧离线收益不能外推到可运行的异步稀疏闭环，更不能将 detector 候选直接作为控制观测。此路径暂不接入正式 runtime；正式八框仍 `2/8 PASS`，handoff HOLD。下一步针对初始化偏差与局部外观/全车位置的关系提出可板端闭环检验的候选，并同时评估 tracking/display 中心、IoU、遮挡无框和恢复身份。

## 2026-11-14T10:05:41+08:00 局部放大 detector 定位候选与 RK3588 完整帧探针（待闭环）

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，起始干净、基线 `d52169ed870187e92a363ed3c30f242fad287685`。输入只使用 canonical `data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`、`9f84efecd57d611a239cc71c99ab2e4295025dbc` 八组 RK3588 CSV 与各自已确认真值；没有用该视频或真值训练模型。主机 VisDrone9 PT `artifacts/yolov8-visdrone9/export/yolov8n-visdrone9-best.pt` SHA-256 `6744672ac5e35debba2a647ad570e9fe84a1cabd1c45c9840080d0a4127fe36d`。对每帧已有 tracking 框在其中心裁方形 `max(120,3×max(w,h))`，缩放至 `320×320`，置于 `640×640` 的灰色画布中央，用目标类 `car=2`、置信度至少 `0.25`、中心距不超过 `0.6×max(w,h)`、宽高比相对 tracking 各在 `0.5～2` 的最近候选筛查。只在原记录 `TRACKING` 且 `observation_valid=1` 时做离线反事实定位，搜索/疑似、状态、身份、控制及真值均不改。
- 实际运行 `OMP_NUM_THREADS=2 MKL_NUM_THREADS=2 YOLO_CONFIG_DIR=/tmp/nt-yolo-config MPLCONFIGDIR=/tmp/nt-mpl .venvs/yolov8-visdrone/bin/python /tmp/nt-dual-gt-evidence/probe_roi_detector_four.py` 和 `probe_roi_detector_extra_four.py`，两脚本 SHA-256 `57f0d29f8be998995e12163f568106e9fb957b974e0b5b7fcb5a23ed026cfc70` / `9c7ad33d38a2a064c2add204c4b7015828aeb6a57f1d0e34ed147adc115063ed`，八组各 271 帧预测存 `/tmp/nt-dual-gt-evidence/roi-detector-four/<label>.json` 与 `roi-detector-extra-four/<label>.json`。评分脚本 `/tmp/nt-dual-gt-evidence/score_roi_detector_eight.py` SHA-256 `2628879858701f0190e44d023531acc0e42db71f56ca6f826b7e9b07708b88ab`，报告 `/tmp/nt-dual-gt-evidence/roi-detector-eight-score.json` SHA-256 `effe03755e34dfe9c94a8597841a87a8dc73c691e71b70a9eeca35107907d642`。直接替换 detector 的宽高会使 car002 真值初始化平均 IoU `0.883→0.781`，不可用；只用 detector 中心、保留 tracker 宽高则 car001 四框中心均误差 `4.28/4.63/4.34/5.02→4.09/4.09/4.28/4.63 px`，car002 `2.59/2.00/1.77/2.92→2.08/2.11/1.94/2.28 px`，两目标跨四框最大成对中心差 P95 分别 `3.31→3.33`、`6.21→3.68 px`，但 car002 真值/左上单框略退化。再用 `/tmp/nt-dual-gt-evidence/score_roi_blend_eight.py` SHA-256 `4f7529b8e026846e76e8f17493c4b9fd0caa81ec4fc66cfdb9b2f96d184b49ab` 反事实筛查：中心差 `<3 px` 用 0.5 倍、大于等于 3 px 用 0.75 倍修正，八框中心均误差均比基线低，car001 `4.14/4.29/4.28/4.78`、car002 `1.86/1.79/1.57/2.02 px`；各框平均 IoU 也均上升，car002 跨框 P95 `6.21→2.72 px`。这些结果不含对跟踪器的反馈，不能当正式提升。
- 固定 RKNN 输入 `640×640` 时，直接将 ROI 放大到 640 的四框筛查只检出 `26～75` 个可用候选并使 car001 中心退化；将 `320×320` ROI 置于 640 画布的四框脚本 `/tmp/nt-dual-gt-evidence/probe_roi_detector_four_canvas.py` / 报告 `/tmp/nt-dual-gt-evidence/roi-detector-four-canvas/report.json` SHA-256 `613e2362ecc2ac491d006faae80e0eb12e125e20d9ee1e635084ed4e931c19e9` / `0f45ee5d0c6d83b191c73759998d317456ee0b1e1b338456d34bb34a8c82871a`，保留原 320 预测的几何特征。RK3588 实机 `LKD3588` Linux `6.1.84`、RKNN API `2.1.0`、driver `0.9.8`，VisDrone9 RKNN/manifest/labels SHA-256 `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c` / `ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9` / `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`。现有板端 `yolo_detector_smoke` 二进制 SHA-256 `3ba28a7afa1e5aa9c09a551f1fbb2b0e137e2b0006ecb1941fdd7fe8d9712f72` 对 15 个 derived canvas PNG 分别以 `--conf 0.1 --nms 0.45 --class-id 2` 推理，板端输出留 `/home/neardi/Project/SOT/dual-gt-display-trace-d157d0d/runs/roi-canvas-probe/smoke/`；回传 15 CSV 后执行 `/tmp/nt-dual-gt-evidence/score_roi_board_smoke.py`（SHA-256 `52764fca54d9b02d57d81b072b57ff747dcb8fce7783762e0b6478769f59f03e`），报告 `/tmp/nt-dual-gt-evidence/roi-detector-board-probe/smoke-score.json` SHA-256 `b3ab2766e80ba6513ce5c4b2b899d0a2f0d3cb3467910960e959aeeecd221bbf`；car002 标准第 245/260 帧 RKNN 局部候选中心误差 `1.25/2.57 px`，原 tracker `7.12/10.61 px`，与主机 PT 的几何位置一致。另用 640 derived MP4 驱动 Debug 程序时，放大车体使首帧类别判成 UNKNOWN 或 van，不能据此评估原视频的跟踪/控制闭环；其输出只留板端隔离目录，不晋级。
- 新增 `rk3566/nanotrack_v3/src/yolo_roi_replay.cpp` 及 RKNN-on CMake 探针目标 `yolo_roi_replay`：从**原始 canonical 视频**和固定的八组基线 ROI trace 逐帧生成 320 局部画布，使用板端真实 RKNN 一次加载、连续推理并输出所有 car 候选及时延，用以核对完整 271 帧而非稀疏样本。ROI trace 由 `/tmp/nt-dual-gt-evidence/make_roi_traces.py` SHA-256 `c4f7bf15dbcfcb845e318f95d02358943689e91e173cc22bdca0a6eaeca980f1` 从精确基线 CSV 生成，保留 `/tmp/nt-dual-gt-evidence/roi-detector-board-probe/roi-traces/`。该工具不会更改正式 runtime、control、`tracking_bbox` 或 `display_bbox`。实际 `git diff --check` 通过；待精确提交后在板端独立目录原生构建并跑八组，核对 RKNN 候选覆盖、与真值的误差、相邻车辆误选和 271 帧时延，再决定是否将有界中心修正送入闭环。正式基线仍 `2/8 PASS`、handoff HOLD。

## 2026-11-14T09:41:08+08:00 两目标中心误差的因果滤波与图像局部化筛查

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，起始干净、基线 `cf4b8d9071685ba4e020cf5f0f76542a28651a31`。只读输入仍为 `9f84efecd57d611a239cc71c99ab2e4295025dbc` 绑定的 RK3588 八组 271 帧 CSV `/tmp/nt-dual-gt-evidence/handoff-9f84efe/<label>/frames.csv`、canonical 视频 `data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d` 和两目标各自已确认真值；正式算法、模型、板端配置、真值均未改。
- 执行 `python3 /tmp/nt-dual-gt-evidence/probe_causal_filter.py > /tmp/nt-dual-gt-evidence/probe-causal-filter-eight.txt`，脚本/输出 SHA-256 `5e97fc0069e138561e032a94d3b04baac9a54316d2e6df41b2f0cdff538f84f6` / `c88587d2d9a1583501620677a36a8f18ea0ebf11297e85c9bb6a7d859e408cae`。仅对 TRACKING 帧中心作重捕/状态切换即重置的因果 α-β 滤波，不反馈运行时。原八框可见中心均误差 car001 为 `4.28/4.63/4.34/5.02 px`、car002 为 `2.59/2.00/1.77/2.92 px`；`α=0.8, β=0.05` 后分别为 `4.32/4.65/4.40/5.06`、`2.61/2.02/1.77/2.92 px`，两路均无整体收益。`python3 /tmp/nt-dual-gt-evidence/probe_tracking_lag.py > /tmp/nt-dual-gt-evidence/probe-tracking-lag-eight.txt` 脚本/输出 SHA-256 `67b4dc924bc34f1dd5edbda518f0d9cd77c83dffd76d7f7553b99755f5c1576a` / `5e91d226a217b5c837776f3d9b2d96737259ccb09feb99acf766a6b425f441e0`：car001 后段中心偏差约左 `3.75～4.87`、上 `4.62～5.18 px`；car002 后段约左 `0.95～6.06`、下 `1.85～5.94 px`，固定速度超前量无法解释两车偏差方向。
- 执行 `.venvs/nanotrack/bin/python /tmp/nt-dual-gt-evidence/probe_white_component_center.py > /tmp/nt-dual-gt-evidence/probe-white-component-eight.txt`，脚本/输出 SHA-256 `17b8db1d64933e186e949bf6379293a2077963b7303194f88ecc1a1be182aad3` / `030ff53b19d6a33a92456b2317cc9f7a3f920a27f16316439281e74ffc73ee94`。以首 20 帧校准的局部白色连通域中心替换 tracker 中心：car001 后段四框均误差有小幅改善或接近，但前段 P95 变差；car002 误选白色背景/相邻区域，后段标准框 `7.83→30.95 px`，直接否决该颜色中心。执行 `.venvs/nanotrack/bin/python /tmp/nt-dual-gt-evidence/probe_local_template_center.py > /tmp/nt-dual-gt-evidence/probe-template-center-eight.txt`，脚本/输出 SHA-256 `c1b4c8d8a5a9401c641b105b2b1c6dda488d49991a23a0ab03211df3e8dda469` / `21f9d093db00230602bf9df4e44b27268753f08cabd8b60ec46049b122775b8a`。首帧灰度模板在当前 tracking 框 ±12 px 内做 NCC 匹配：car001 遮挡前标准框中心 `3.23→1.47 px`，右下 `3.47→1.71 px`，但 car002 真值框前段 `1.38→3.56 px`、左上 `1.46→3.32 px`，后段四框均退化。再用脚本 `/tmp/nt-dual-gt-evidence/probe_template_gate.py` SHA-256 `de4166b75818d0d5348ba39dee9c0595691a58912a4d1073ea44ba26eba63d93` 扫描位移上限、NCC 增益和半/全校正，输出 `/tmp/nt-dual-gt-evidence/probe-template-gate-eight.txt` SHA-256 `a81dd1acc942e08ec90277f158bd10f15d0c1098c443c06cef5e6db54573057c`；小幅中心均值改善的参数均增加至少一个目标的跨初始框中心分歧，未形成同时满足精度与一致性的候选。
- 另执行 `python3 /tmp/nt-dual-gt-evidence/inspect_recovery_geometry.py > /tmp/nt-dual-gt-evidence/recovery-geometry-four.txt`，脚本/输出 SHA-256 `c4e6f14a51de25037ec2fcec6bfe7509df346975d1be9b265b4078169d42f3f5` / `88681193110c289e4523f1b16305dff9aaff5b006ae4c762ffc939ebe88d5bf1`；car001 右下第 187 帧搜索预测中心 `(875.3,826.0)` 对已确认真值 `(871.0,844.0)` 约偏上 `18 px`，直到第 190 帧才得到近真值 detector 支持的候选。car002 第 229 帧仍人工确认无正式框，第 230 帧首次有真值框。配对源帧图 `/tmp/nt-dual-gt-evidence/bridge-comparison.png` SHA-256 `035ba42ffc8706008ef01c22389be8376c9b6554a5f4469ed1304a0b5a0dc58a`，生成脚本 SHA-256 `245dab8e300900dd371c0c17ed080553a60df46b897a10f007b5ebd09e02b62f`；图像只供定位诊断，不给运行时泄露帧号或真值。
- **裁决 / Handoff**：这些是基于冻结轨迹的离线反事实，不包括修正反馈、模型重跑或板端时延，因此没有接入滤波、颜色中心或 NCC 中心；正式八框 `2/8 PASS`、handoff HOLD。下一步须找能分辨局部外观误匹配与完整车体可观测性的独立证据，并在板端闭环证明两目标、各四个初始框的 tracking/display 中心、IoU、无框语义和恢复身份同时改善。

## 2026-11-14T09:29:00+08:00 横向厚遮挡带候选 RK3588 四组否决并撤回

- 候选源码 `codex/dual-gt-init-consistency@cacb3a896c8a6321e834d8af946f7cc9da18dc82`，基线 `e635a9a69631cd4b930ed5d4e5358a2572f4573c`；主机源码包 `/tmp/nt-dual-gt-evidence/occluding-band-cacb3a8-source.tar` 与板端 `/home/neardi/Project/SOT/dual-gt-occluding-band-cacb3a8/source.tar` SHA-256 `fdc02c2b985c85246b322386a4af2cbbaeeb267a6deb8882d3c27a54bd4b6489`。RK3588 `LKD3588` Linux `6.1.84` 隔离根 `/home/neardi/Project/SOT/dual-gt-occluding-band-cacb3a8`，RKNN API `2.1.0`、driver `0.9.8`。原生执行 `cmake -S rk3566/nanotrack_v3 -B build -DENABLE_RKNN_RUNTIME=ON -DRKNN_INCLUDE_DIR=/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/include -DRKNN_LIBRARY=/usr/lib/librknnrt.so`、`cmake --build build --parallel 4`、`ctest --test-dir build --output-on-failure`，`23/23 PASS`；可执行文件 SHA-256 `48b404ea863356bf23802cb7af439fe9df26f178276a73237b8a3b19726f9c31`。`runs/headless-eval.conf` SHA-256 `2d45ca0f31916e0d05e0e5c2cad189bbf0d42f372aa4a67f3dbb3de939c531d6`；其视频、controller、NanoTrack RKNN、VisDrone9 detector/manifest/labels 路径与下方 `9f84efe` 已哈希绑定基线相同。
- 板端运行 `bash runs/run_screen.sh`，脚本主机 `/tmp/nt-dual-gt-evidence/run_occluding_band_screen.sh` SHA-256 `6c835b57c0c9382b35ff48b398e4d3d810a90671d9b7d13f9fa2d417700858de`，以 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox <ROI> --output-boxes runs/<label>/frames.csv --session-metadata runs/<label>/session.json` 顺序完成 car001 标准 `(992,582,37,39)`、右下 `(994,584,35,37)`、car002 标准 `(970,517,38,38)`、左上 `(968,515,40,40)` 四组，各 271 数据帧、exit 0，未生成 AVI。CSV/session/log 保留于板端 `runs/<label>/`。按此顺序 SHA-256 分别为：car001 标准 `a86d215d9c53b48fdaabb30d24f5a4b214d362bc41f3891ebe8697cad841b460` / `998eb0c94938cec065ebe449dc7325bc9ee9112f1dbf515fc615569cd5f4344f` / `a74b63b15acee88f4791351b02d00fe99e4e101d45ca3b77fca1d1dcf924e486`；car001 右下 `30416112559defa30592650f437a4fe2fa2129e587076bd730e8f700f81d74b9` / `9e5c11289a9616790a7d269f6534ab69954fe5997b88828b3a01c869632461fb` / `3118157fe2c6cc2231eb27a80ddcf4812e23495962e3ad3c023347e654cbce7c`；car002 标准 `7a0162598c14ee82a64bfcd8b1b04f79e3b527a74288a35c79dd1f8ec6e257ed` / `b3a1e4d0d40bd7faa00f51a52dbfc0688c34c79cf31c19ade3b5b2d262e14dd1` / `2e3779d768666d336c179974c22f67461fe1bb7c321fdee7698246a043879e5e`；car002 左上 `5007a36679d4c8a0dcd3221adec46bfe64c3fd1ab53a1445f738781292a0eb4c` / `fccfd924f10c93526047003f1327ec99e5d2f9d824c27827ba572e5b10464f01` / `b0d8aef9807a75aab5e9c00bbc1998b7cafec5bd15bd070e497d0631cd2c77be`。四 CSV 主机副本 `/tmp/nt-dual-gt-evidence/occluding-band-cacb3a8/<label>.csv` 与板端 SHA-256 一致。
- 实际用 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ...` 对各自确认真值评分，报告 `/tmp/nt-dual-gt-evidence/occluding-band-cacb3a8/four_score.json` SHA-256 `dc0bd592ecfeade40c113d0465bbf8dd99e182bd0974e2ccf17f6f68ca97a71b`。car001 标准 `PASS`、188 帧重捕、tracking/display 中心 `4.28/4.32 px`；右下 `FAIL`、193 帧重捕（基线 191）、中心 `5.71/5.65 px`。car002 标准 `FAIL`（可见框、完全遮挡、重捕），227 帧过早重捕且无框段错误观测 1 帧、中心 `2.47/2.47 px`；左上 `FAIL`（完全遮挡），230 帧重捕、中心 `1.80/1.80 px`。car001 标准/右下跨初始化 tracking/display 中心 P95 `3.01/2.18 px`，car002 标准/左上 `8.80/8.80 px`。四组 pipeline mean `113.1～114.0 ms`、P95 `118.8～119.2 ms`。闭环实际 car001 右下 169 帧提前搜索但后段恢复变晚；car002 两框只在 214 帧搜索，211～213 帧仍对已确认无框区间输出疑似观测，故离线历史轨迹的首触发 211 帧不成立。
- **裁决 / Handoff**：该候选不能改善两路中心一致性或完全遮挡语义，撤回六个算法/测试文件改动，保留板端隔离证据和此记录；`display_bbox` detector 仍只用于显示，未把遮挡边缘或 detector 作为控制观测。正式八框仍 `2/8 PASS`，handoff HOLD。后续需基于闭环结果而非固定基线轨迹设计可见性门禁与恢复候选身份验证。

## 2026-11-14T09:21:47+08:00 横向厚遮挡带与疑似跟踪联合触发候选（待 RK3588 闭环）

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，起始工作树干净、基线 `e635a9a69631cd4b930ed5d4e5358a2572f4573c`。`car001_right` 在 166～174 帧进入 SUSPECTED，却到 175 帧才 SEARCHING；car002 四框在 211～219 帧 SUSPECTED，直到 220 帧才 SEARCHING，导致已确认无框区间使用 tentative recent 观测。单独缩短疑似时长会影响 car002 左上框 120～142 帧正常可见的 SUSPECTED 段，不是安全解。
- 主机只读筛查 `/tmp/nt-dual-gt-evidence/probe_crossing_line.py` SHA-256 `0e11d8719057fa38af66633c9ec747fffccb847547db8a6d78681414accf9be2`；输入 canonical `/home/u20/single-object-tracking/data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d` 和 `9f84efe` 八组板端 CSV，输出 `/tmp/nt-dual-gt-evidence/probe-crossing-band-prevbox-eight.tsv` SHA-256 `1dd3fb3afc08f8885bf0d8deac49e93c4cc191e1fda4eb66b317f00caefb6937`。以**前一帧已提交框**圈局部图像，在横向长度至少 2 倍目标宽度、近水平的边缘中寻找相距目标高度 `0.7～1.4` 倍的上下两条线；近六帧至少两次厚带命中且本帧已有 SUSPECTED 时，八框历史轨迹的首个候选触发为 car001 右下第 166 帧及 car002 四框第 211 帧。car001 其他三框已自行进入搜索；car002 左上 69～70、120～142 帧的正常可见 SUSPECTED 段没有触发。单条长横线在 car002 69～79 帧护栏处会误报，故必须是有厚度的两条边缘，且与控制器已有疑似状态同时出现。以上只是固定历史轨迹的离线筛查，不证明修改闭环后的八框结果。
- 候选修改 `main.cpp` 在汽车目标的当前帧用 OpenCV Canny/Hough 检出厚带，缓存六帧布尔值；经 `LongTermRuntime::process_frame` 把“近六帧至少两次命中”送入 `SearchPlan`。`LongTermController::assess_recent` 只有在原有响应退化已进入 SUSPECTED 时提前走现有 SEARCHING 分支，拒绝 tentative 观测并沿用已有 motion-search 控制门禁。没有把边缘、detector 或估计真值写作 accepted 观测，也未改变 `display_bbox`/`tracking_bbox` 的框来源。新增控制器回归覆盖联合条件提前搜索与无厚带时保持 tentative；实际 `git diff --check`、`cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`。该主机构建不编译 `main.cpp` 的 OpenCV 检出函数。
- **证据边界 / Handoff**：本候选尚未在 RK3588 构建或运行，正式 `9f84efe` 八框仍 `2/8 PASS`、handoff HOLD。下一步用精确提交在板端独立目录原生构建，先跑 car001 标准/右下与 car002 标准/左上四组完整 271 帧；必须核对触发帧、错误重捕、无框段观测、两路中心和推理时延，再决定是否扩八框或撤回。厚带启发式目前只在本 canonical 场景筛查，不能据此声称跨场景泛化。

## 2026-11-14T09:12:03+08:00 永久模板裁剪尺度量化 RK3588 四组否决并撤回

- 候选精确源码 `codex/dual-gt-init-consistency@d8178faa283432b4215cdce67a0b25dcbc20b647`，主机源码包 `/tmp/nt-dual-gt-evidence/anchor-scale-d8178fa-source.tar` 与板端 `source.tar` SHA-256 同为 `d21b4f5b25bc0d87c4bff00f67ed8fcd980f8225276266be2506dcde2f9761d1`。RK3588 `LKD3588` Linux `6.1.84` 隔离根 `/home/neardi/Project/SOT/dual-gt-anchor-scale-d8178fa`；原生执行 `cmake -S rk3566/nanotrack_v3 -B build -DENABLE_RKNN_RUNTIME=ON -DRKNN_INCLUDE_DIR=/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/include -DRKNN_LIBRARY=/usr/lib/librknnrt.so`、`cmake --build build --parallel 4`、`ctest --test-dir build --output-on-failure`，`23/23 PASS`，二进制 SHA-256 `d06c7379f5cd7b99784e69e9f71ee347f4095bda7d2dac59fb9311e0848b4a5b`。`runs/headless-eval.conf` SHA-256 `2d45ca0f31916e0d05e0e5c2cad189bbf0d42f372aa4a67f3dbb3de939c531d6`，其绝对视频、controller、官方 NanoTrack RKNN 和 VisDrone9 detector/manifest/labels 路径与下方 `9f84efe` 已哈希绑定板端基线相同；RKNN API `2.1.0`、driver `0.9.8`。构建/CTest 日志留 `runs/`。
- 板端实际运行 `bash runs/run_screen.sh`，脚本主机 `/tmp/nt-dual-gt-evidence/run_anchor_scale_screen.sh` SHA-256 `1292f2528d2c5e4ca60cb6f9874c9849d893c3ce8c9d83b4111abfb036829ad8`；依次以 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox <ROI> --output-boxes runs/<label>/frames.csv --session-metadata runs/<label>/session.json` 跑 car001 标准 `(992,582,37,39)`、右下 `(994,584,35,37)`，car002 标准 `(970,517,38,38)`、真值 `(967,513,42,42)`，每组 exit 0、271 数据帧；未生成 AVI。CSV/session/log 均留在板端对应 `runs/<label>/`：car001 标准 SHA-256 `1c68a4edf1c4f07759d4a0e19585b73cbd469943abbe6ac450200a6aeac4908f` / `5455b4c45c095171cc0384dcda80b13cd3ec15041ab3ab3507d88fb4164f7494` / `b5d9a2575d7c9b92424fd6962371a65b320761c8410f18a421654f7746c47427`；car001 右下 `ce504d76d73e43382a1f4564404957332b7c1a44e70154db8d9749de7e032d19` / `622cc89d3882d08bb9d6a0692b70d31502117d1eaeb1dbe96e3436d5482933a8` / `1566a7e11b2d2e2c3611f152b29e9da94f17c3d8a6ab7f84aa2f5ca883380850`；car002 标准 `235574d08f41731179559ad276eb3b22dfd20b8432b446959a6d0696278fecf7` / `5ed32e27a694fc1544990e0640c733032c3bf037f44961b44fc43e99dc7504a0` / `0611eed8b422087dc972e7f0217d58b409e9b68980401cace149e6c3319748a3`；car002 真值 `99ae8184d86a80835ab4ae26c702276155a0a74938de56a60c099dca1d4611c9` / `7ec1746f2616784f7cae5c2d5d7efb9b6024aa01756d7c805ee574db3a17448c` / `d4a327dc8d5f42ceb4f36b888610d07538cda4ae89798dabad5df99807a5de0b`。四 CSV 主机副本 `/tmp/nt-dual-gt-evidence/anchor-scale-d8178fa/<label>.csv` 哈希与板端逐一一致。
- 实际用 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ...` 针对各自已确认 GT 评分，报告 `/tmp/nt-dual-gt-evidence/anchor-scale-d8178fa/four_score.json` SHA-256 `884b360661e41155f26eba45546d0471908ec11ffc1a4d798ad0bf9fe2546e21`。car001 标准保持 `PASS`、第 189 帧重捕、tracking/display 中心 `4.25/4.31 px`；但右下框 `FAIL`，第 142、186、216 帧重捕，tracking/display 中心 `9.28/8.98 px`、IoU `0.613/0.652`，标准/右下跨框 tracking 中心 P95 `34.11 px`，明显差于正式基线。car002 标准/真值仍因完全遮挡语义 `FAIL`；中心分别 `2.49/2.50` 和 `1.93/1.90 px`，两框跨框 tracking 中心 P95 `8.04 px`。四框可见中心局部改善不能抵消 car001 错误恢复与不一致。
- **裁决 / Handoff**：本项同提交撤回尺度量化代码，保留候选提交和隔离板端证据；`rk3566/nanotrack_v3/src/nanotrack_v3.cpp` 恢复为 `16cbb43` 算法内容。正式 `9f84efe` 八框仍 `2/8 PASS`、handoff HOLD。后续不能仅根据标准框通过或单目标中心均值选择模板尺度，必须保护微扰框恢复身份和两目标遮挡语义。

## 2026-11-14T09:06:35+08:00 小目标首帧永久模板裁剪尺度量化候选（待板端筛查）

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，开始时工作树干净、基线 `16cbb439142c1c500b72a9fb393f0fbdcf452e87`。前一轮直接使用 detector 首框初始化与五路模板特征平均均导致 car001 明显退化，均已否决撤回；本候选不使用 detector 候选更新控制，也不融合多个模板特征。
- 仅修改 `rk3566/nanotrack_v3/src/nanotrack_v3.cpp` 的初始 anchor 特征裁剪框：对宽高均在 30～60 px 且长短比不超过 1.4 的小目标，将模板裁剪框宽高分别四舍五入到 10 px，保持原 ROI 的模板中心；越界则沿用原 ROI。此两目标八个人工初始框的宽高均会变为 `40×40`，但 tracker/控制器的初始框、后续近期模板更新、显示 detector 和所有真值均保持原语义。目标是隔离初始 ROI 尺度微扰对永久模板的影响，判断它是否导致恢复阶段分叉；此启发式还未证明跨目标通用。
- 实际执行 `git diff --check`、`cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`。**Handoff**：待本项与源码同提交后，用精确提交在 RK3588 独立目录原生编译，先跑 car001 标准/右下、car002 标准/真值四组完整 271 帧；比较两路中心、IoU、恢复身份、完全遮挡语义，只有不退化才扩八框。正式已验证 `9f84efe` 八框仍 `2/8 PASS`，handoff HOLD。主机测试不构成板端算法改善证据。

## 2026-11-14T09:02:36+08:00 双目标八框 detector 候选中心按源帧对齐的诊断

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，起始工作树干净、HEAD `80883549b6b0bfcffcfad6f011171ebcb083f874`；正式算法、控制配置、模型和 GT 均未改。沿用既有只读显示候选诊断源码 `d157d0d66a7346dd5460b8a568eea2c0e973cc58` 的 RK3588 隔离根 `/home/neardi/Project/SOT/dual-gt-display-trace-d157d0d`，复核二进制 SHA-256 `ed4f8ad097331988e22ac49dff5fe305ee20163ca2cc425a43b758119bb58e57`、`runs/headless-eval.conf` SHA-256 `c7594ebb9c43789b136e13182fdfd96f634af154f411142f0866b954762cad98`。板 `LKD3588` Linux `6.1.84`，RKNN API `2.1.0`、driver `0.9.8`，视频、两套 GT、tracker RKNN 和 VisDrone9 detector/manifest/labels 身份沿用下方 `d157d0d`/`9f84efe` 已哈希绑定条目。已有 car001/car002 标准框的只读 trace 均与正式基线逐帧轨迹一致。
- 在该板端隔离根执行 `bash runs/run_source_aligned_extra_six.sh`；脚本主机 `/tmp/nt-dual-gt-evidence/run_display_trace_extra_six.sh` 与板端 SHA-256 同为 `ec51bf5acb76aacc9d60dbc80e0cdfd16fad294e01c6c4fd6afa3ee8939564e7`。按 car001 人工 `(993,585,36,36)`、左上 `(990,580,39,41)`、右下 `(994,584,35,37)`、car002 真值 `(967,513,42,42)`、左上 `(968,515,40,40)`、右下 `(972,519,36,36)`，逐组以 `NT_TRACE_DISPLAY_CANDIDATE=1 ./build/nanotrack_v3 --config runs/headless-eval.conf --bbox <ROI> --output-boxes ... --session-metadata ...` 运行完整 271 帧。六组 exit 0、各 271 数据帧；没有生成 AVI。CSV/session/log 留在板端 `runs/source-aligned-extra/<label>/`，18 文件 `runs/source-aligned-extra-checksums.sha256` SHA-256 `23fae55cf3746d5fd5ee8dfe6cc58a3ec47877e9b4872d11aed554588c2975a7`，板端 `sha256sum -c` 18/18 OK。文本包 `runs/source-aligned-extra-six-text.tar.gz` 与主机 `/tmp/nt-dual-gt-evidence/source-aligned-extra-six-text.tar.gz` SHA-256 同为 `ad0b1ca4c357eda06e04b086d0d30b9c8363813acd52ba52b0f2678de7defa46`；主机解包到 `/tmp/nt-dual-gt-evidence/display-trace-source-aligned-extra/` 后按同一清单复核 18/18 OK。
- 主机实际执行 `python3 /tmp/nt-dual-gt-evidence/score_source_aligned_display_eight.py`，脚本 SHA-256 `750b74dad0fe23c1cedf67cb0a1275e4f81ced6681afa28a06735ee04ba6beb9`，报告 `/tmp/nt-dual-gt-evidence/score-source-aligned-display-eight.json` SHA-256 `a9597e9879ceb7dae1022dab2a80edecb2108a881ec7984ed41d9d29ecdbd2a9`。方法先把每个 detector 显示候选与**其源帧** NanoTrack 框中心相减，用最初 12 次匹配的中位偏移校准（无 GT 参与）；只在事后用两目标已确认 GT 计算误差。八组各有 69～77 个匹配候选。六组新增 trace 的 `state`、`tracking_phase`、`reacquired`、`control_valid` 与正式 `9f84efe` CSV 逐帧一致；car001 人工框第 235 帧仅有 1 帧 tracking/display 几何小差异（tracking x 相差约 `0.42 px`），其余新增框几何全相同。
- 源帧对齐后，car001 标准/左上/右下后段 ≥240 帧的跟踪与候选中心平均残差约 `1.34/1.07/1.31 px`；car002 标准/真值/左上/右下分别为 `5.96/3.06/2.85/4.40 px`。若将 detector 候选按 tracker 的源帧到结果帧位移及前 12 次中位偏移作**离线反事实**校正，car002 后段四框的 tracker→候选中心均误差分别 `7.74→2.66`、`4.84→2.76`、`3.41→3.23`、`6.74→3.03 px`；car001 后段四框也略有改善或接近持平。此处只评分候选，程序没有输出或控制这些反事实框。
- **否决单一残差门 / Handoff**：连续三次源帧对齐残差 `>3 px` 虽在 car002 标准与右下框后段持续出现，却在 car002 真值初始化的第 113～128 帧也连续触发；该段真值可见，tracker 中心均误差 `1.51 px`，校正 detector 候选反而 `2.63 px`。因此单独用 detector 残差触发搜索或直接改 `tracking_bbox` 会造成误报；detector 仍只作显示/临时候选，不能直接授权控制。正式八框仍 `2/8 PASS`、handoff HOLD。下一步若利用这个独立定位信号，必须与 NanoTrack 身份/完整可见性验证合并，并在八框闭环证明两路中心与遮挡语义同时改善。

## 2026-11-14T08:50:45+08:00 对称首帧模板平均 RK3588 四组否决并撤回

- 候选精确提交 `codex/dual-gt-init-consistency@4c28b28018f923083926765cca2b8f2596dd9d78`，起始工作树干净。`git archive` 源码包 `/tmp/nt-dual-gt-evidence/anchor-jitter-4c28b28-source.tar` 与板端 `source.tar` SHA-256 同为 `7646d376de97541599868ec06bc288bf13431e4801ba3e35e31cd188bbc1286b`。RK3588 `LKD3588` Linux `6.1.84` 隔离根 `/home/neardi/Project/SOT/dual-gt-anchor-jitter-4c28b28`；执行 `cmake -S rk3566/nanotrack_v3 -B build -DENABLE_RKNN_RUNTIME=ON -DRKNN_INCLUDE_DIR=/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/include -DRKNN_LIBRARY=/usr/lib/librknnrt.so`、`cmake --build build --parallel 4`、`ctest --test-dir build --output-on-failure`，板端 `23/23 PASS`，二进制 SHA-256 `79dc3f32eab9ac97cd60a0045d383e51b7f181e9c89d01b1b8c075db46d5c072`。`runs/headless-eval.conf` 与已验证基线内容相同，SHA-256 `2d45ca0f31916e0d05e0e5c2cad189bbf0d42f372aa4a67f3dbb3de939c531d6`，其绝对 controller、视频、tracker RKNN、VisDrone9 detector/manifest/labels 路径沿用下方 `9f84efe` 已哈希绑定环境；RKNN API `2.1.0`、driver `0.9.8`。构建/CTest 日志在隔离根 `runs/`。
- 板端实际执行 `/home/neardi/Project/SOT/dual-gt-anchor-jitter-4c28b28/runs/run_screen.sh`，脚本主机 `/tmp/nt-dual-gt-evidence/run_anchor_jitter_screen.sh` SHA-256 `4d37ced70cba6e51134a106a9d44e6f8369f3740a80c7307fbfe927a0e845496`；逐组用 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox <ROI> --output-boxes runs/<label>/frames.csv --session-metadata runs/<label>/session.json` 跑 car001 标准 `(992,582,37,39)`、右下 `(994,584,35,37)` 和 car002 标准 `(970,517,38,38)`、真值 `(967,513,42,42)`。四组 exit 0，各 271 数据帧；未生成 AVI。板端 CSV/session/log 按上述顺序 SHA-256：car001 标准 `8e534a6aec594b883b1932b9dea6829fc37e2688208f334fd8e9c8b1291ea638` / `f8770beb768d92a4a78d04d9db2487bc5dd33a0c910f8620d9c9f5669968dd1f` / `f707bacad3b60095071ba4a5cee492e918b2c9640b6acdd07c805c299e0d0e0b`；car001 右下 `e00bd5d54a023f34fc8ab175bb38086eadefd7f8500a9e8251d82e1997b9ff76` / `b0ac0ac4b4007de02503cadc114670ad832a06a87f876f4968443daad3299e11` / `cc04bbcb25afd41fb46ee7c202382bc274017af89057889392bba64734e1baa5`；car002 标准 `5ba1f5fd9f278db438376b5cc374f01e578b4a1d0ed300c2d59a75653e0a8c8b` / `52d79cc0009b38b9f9ad19658dd043fe777636e6b6563fb584c5dd77544da6cb` / `5278d25073d87535055f5e48e368027182a7449179443284b71681e7ddce3a69`；car002 真值 `35692849ad5a23388367cccec6b9098b0f00ca215c8e76b3f0bec807af95ed87` / `52ecc4d05d3bae979c8c810e11e55c9866723645f01347a0fa1c59fc13135356` / `4606312c3227112221400d2eac810a697a7713c5077395c6d5e4cd9036e45e96`。四 CSV 回传到 `/tmp/nt-dual-gt-evidence/anchor-jitter-4c28b28/<label>.csv`，SHA-256 与板端逐一相同；session/log 留板端。
- 主机用 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ...` 给上述四组与各自已确认 GT 评分，报告 `/tmp/nt-dual-gt-evidence/anchor-jitter-4c28b28/four_score.json` SHA-256 `388fa753ce5bc169784f76750e0aa845a220ca0ea58c18453beb5806e1544400`。car001 标准 `FAIL`（完全遮挡语义、恢复），第 153 帧额外重捕且后段 190 帧才重捕，tracking/display 中心 `5.28/5.25 px`、IoU `0.727/0.739`；原基线该框 `PASS`、188 帧恢复、tracking 中心 `4.28 px`。car001 右下 `FAIL`，恢复 192 帧（基线 191），中心 `4.67/4.68 px`、IoU `0.718/0.769`。car002 标准/真值均 `FAIL`（完全遮挡语义），233 帧恢复，中心分别 `2.32/2.34 px`、`2.02/2.02 px`；标准框 IoU 仅 `0.811/0.754`，真值框 `0.879/0.755`。car001 标准与右下 tracking/display 跨框中心 P95 `5.34/5.92 px`，没有足够一致性收益抵消标准框身份和恢复退化。
- **裁决 / Handoff**：本项同提交撤回对称模板平均代码，保留候选提交与板端隔离证据；正式算法文件恢复为已验证 `9f84efe` 内容。`display_bbox` detector 继续只用于显示，未把实验模板/检测框引入控制。正式八框仍 `2/8 PASS`、handoff HOLD，下一步需针对恢复候选身份与完全遮挡可观测性提出并验证新机制，不能从本次 `23/23` 构建测试推断算法改善。

## 2026-11-14T08:45:03+08:00 对称首帧模板扰动平均候选（待板端双目标筛查）

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，开始时工作树干净，基线 `4ce4d4ff1646a3a52cc9b9ac9e85004bc7bbd8b5`。八框原始 CSV 显示 car001 遮挡前中心最大差距一般约 2～3 px，第 176 帧起右下框进入搜索较晚、分歧增大，第 188～190 帧四框中心最大差距约 22 px；单改 detector 周期已有错车和重捕变晚反例。
- 候选只改 `rk3566/nanotrack_v3/src/nanotrack_v3.cpp` 的初始化模板特征：原 ROI 特征加上上下左右各 `min(2 px, 5% 最小边长)` 的对称微扰模板特征，取五路均值作为永久 anchor；边界越界时跳过对应采样。控制器、detector、近期模板、`tracking_bbox`/`display_bbox` 语义及真值均未改。意图是降低微弱初始框差异对永久模板的影响，不用真值调整运行时框。
- 实际执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4` 和 `ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`，`git diff --check` 通过。主机单测不能证明该 RKNN 模板融合的板端轨迹。**Handoff**：待本项与源码同提交后，以精确提交构建 RK3588 隔离目录，先对 car001 标准/右下和 car002 标准/真值四组完整 271 帧筛查；只有两目标中心、恢复身份和无框语义均不退化，才扩八框。正式已验证 `9f84efe` 八框仍 `2/8 PASS`、handoff HOLD。

## 2026-11-14T08:41:14+08:00 首帧 detector 框归一化与显示候选分数反例

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，本轮基线 `8a6c3b68fafadd2865c76952c6e511ccbc384d70`；算法与配置未改。板 `LKD3588` Linux `6.1.84`，RKNN API `2.1.0 (967d001cc8@2024-08-07T19:28:19)`、driver `0.9.8` 沿用已哈希绑定 `9f84efe` 八框环境。板端根 `/home/neardi/Project/SOT/dual-gt-handoff-9f84efe` 的精确源提交 `9f84efecd57d611a239cc71c99ab2e4295025dbc`，本轮复核 `build/nanotrack_v3` SHA-256 `41d83ef0d772833b29400a35cacb8825a1a402f24ea7dbfe9279ac09b062343b`、`runs/headless-eval.conf` SHA-256 `2d45ca0f31916e0d05e0e5c2cad189bbf0d42f372aa4a67f3dbb3de939c531d6`；视频、两套真值和 tracker/detector 模型身份沿用下方 `9f84efe` 条目。未向板端部署新代码。
- 八组原始首帧 detector 均选中同一个对应目标框：car001 `(994.000916,581.063293,35.289490,39.486084)`，car002 `(973.034729,520.489929,34.091492,31.423828)`。为检验“将微扰人工 ROI 统一到 detector 框”是否安全，在独立板端目录 `runs/detector-anchor-screen/` 用现有程序分别执行 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox 994,581,35,39 --output-boxes runs/detector-anchor-screen/car001/frames.csv --session-metadata runs/detector-anchor-screen/car001/session.json` 和相同命令的 car002 `--bbox 973,520,34,31`。两次 exit 0、CSV 各 271 数据帧；未要求 AVI。car001 CSV/session/log SHA-256 `a26997ea0a90ee4d5f901167f34cced29bddbf6068be7f2e1e04a3c09c8b6f5c` / `928bebd73c887f1cd3e2b7fe6f405d3235ac39e83602c81224ced79d7a3fa4b2` / `a568d819229c8a9c6aa0448af7545e8ff79e8b01df6349689ea29f3f6d64f247`；car002 相应 SHA-256 `1f6640df51fb5729a67e5bdc633feedf2490d43d068ceb049848b4217fb8c893` / `9c6a01bf462b30f6b7aacdefc74289b8561957101537da7ac0b0cc2a4758f6f6` / `efb8c481c29b41428a3e6d85e0be7578d1e9e73ef1382e7e4eebaa8af5c69352`。CSV 主机副本 `/tmp/nt-dual-gt-evidence/detector-anchor-car001.csv` / `detector-anchor-car002.csv` 哈希与板端相同；AVI/完整日志留板端。
- 实际用 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run car_001 detector_anchor 994,581,35,39 /tmp/nt-dual-gt-evidence/detector-anchor-car001.csv --run car_002 detector_anchor 973,520,34,31 /tmp/nt-dual-gt-evidence/detector-anchor-car002.csv --output-json /tmp/nt-dual-gt-evidence/detector-anchor-two-score.json` 评分，报告 SHA-256 `6fc6a9e7cefee79f74902266009a2f1099a1d4407c97bb9aab7a15c2c439e834`。car001 `FAIL`（visible_boxes、full_occlusion_semantics、identity_proxy），第 186 帧早恢复到错误车辆，tracking/display 中心均误差 `33.54/33.35 px`、IoU `0.487/0.520`；car002 `FAIL`（visible_boxes、full_occlusion_semantics），第 234 帧恢复，中心 `3.13/3.15 px`、IoU `0.640/0.728`，无框段仍有 1 帧假观测。首帧 detector 归一化会使 car001 相比原四框约 4～5 px 中心误差灾难性恶化，否决该方案。
- 用实验 HOG 遮挡分数对之前 `display-trace-d157d0d` 已匹配的两目标各 76 个 detector 显示候选评分，脚本 `/tmp/nt-dual-gt-evidence/score_hog_display_candidates.py` SHA-256 `275f9a67ca5e466b1190e91a991d567feb589cab0dc4ff3554fd556004765810`，报告 `/tmp/nt-dual-gt-evidence/score-hog-display-candidates.json` SHA-256 `97f5f1d36ebe58c4245eb787c9015c30425fdddce192cf3ce8e373b32fd85d33`。后段 ≥240 帧 car001 detector HOG 平均 `1.75` 高于 tracking `1.30`，但 detector 中心误差 `9.30 px` 差于 tracking `7.33 px`；car002 detector HOG `0.86` 低于 tracking `1.09`，但中心 `2.51 px` 优于 tracking `7.74 px`。该分数反向选择更准的框，不能当两路中心仲裁依据。灰度/白色像素纵向分布探针 `/tmp/nt-dual-gt-evidence/probe_whiteness_occ.py`（SHA-256 `b0df17d979e9085c88da99f0393e95d4336d364e6ad8ba8c06d81bdc0b7ddefb`）及输出 `probe-whiteness-occ.tsv`（SHA-256 `b96662e2c901e2632d7aff86e5bd8425a94840050b0afc77ec54b85244819758`）显示 car002 无框第 229 帧与重新可见第 230 帧纵向白色分布仍相近，简单亮度阈值无法作为完整车体门。
- 证据边界 / Handoff：保留 `9f84efe` 正式基线 `2/8 PASS`，板端隔离输出是筛查反例，不作为新算法或交付。`display_bbox` 里的 detector 框继续只用于显示；任何控制定位变更仍需两目标四种人工微扰框的逐帧真值评分及 RK3588 验证。下一步针对 NanoTrack 模板/恢复假设做不会把相似车辆误认为原目标的可验证改动。

## 2026-11-14T08:34:36+08:00 独立遮挡分类器筛查：car002 无框段误报，暂不接入

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，本轮基线 `709c6db8202f864f199687c5ec027157ce598edc`，开始与结束时工作树均干净（写入本进度项之前）；正式算法、模型、配置和两目标真值未改。延续 RK3588 `9f84efecd57d611a239cc71c99ab2e4295025dbc` 八框基线：`2/8 PASS`，car001 左/右第 191 帧恢复晚于允许界限，右框还有完全遮挡语义失败；car002 四框全因完全遮挡语义失败。用户要求 tracking/display 两路中心均贴真值，并明确 detector 的 display 框只用于视觉效果，故本筛查没有把 detector 或新分类器输出写入控制框。
- 独立训练来源 `/home/u20/dataset/VisDrone2019-DET/VisDrone2019-DET-train`，独立验证 `/home/u20/dataset/VisDrone2019-DET/VisDrone2019-DET-val`；只选 `category=4, score>0, truncation=0`、宽高各 25～90 px 的汽车。`/tmp/nt-dual-gt-evidence/screen_visdrone_occlusion.py` SHA-256 `cd5f2337ac282072e413ddc32e93b0c27577f987533350c3b57c6390d7e13ffb`，实际运行 `.venvs/nanotrack/bin/python /tmp/nt-dual-gt-evidence/screen_visdrone_occlusion.py --output /tmp/nt-dual-gt-evidence/visdrone-occlusion-screen.json`；64×64 灰度 HOG + OpenCV 线性 SVM，训练 2343 样本（无遮挡 1168、遮挡一级 882、二级 293），验证 782 样本（393/292/97）。独立验证 AUC `0.8952`、准确率 `0.8107`，无遮挡/遮挡一级/二级在零阈值的召回 `0.8651/0.6952/0.9381`。报告 `/tmp/nt-dual-gt-evidence/visdrone-occlusion-screen.json` SHA-256 `55bb39f098544660c4d6f05915ad4b0edc8a88ad0221a3c46a5a113ed`，实验模型 `.xml` SHA-256 `7b185f935463f4d2f395189ddc3b229edcd0a84ad9ef601e8dd76c0ac22d1488`（33 MiB，保留在主机 `/tmp`，未入 Git）。两条 canonical 目标均未参与训练或阈值选择。
- 随后实际执行 `.venvs/nanotrack/bin/python /tmp/nt-dual-gt-evidence/eval_visdrone_occlusion_on_occ.py > /tmp/nt-dual-gt-evidence/visdrone-occlusion-canonical.stdout`，脚本 SHA-256 `06a5bacc63b1c881d70265df61c83f52113529b4aea0feb661fef65fdfb80f31`；报告 `/tmp/nt-dual-gt-evidence/visdrone-occlusion-canonical.json` SHA-256 `4c3483ff058419196513a016dc805579b3543540be5f2c9a3399adb8c0b38b9c`。该脚本用 `scripts/evaluate_occ_video_1.py` 严格校验两目标 GT 合约、解码 271 帧，并分别给真值可见框及原 RK3588 八框 CSV 的预测框打分。car001 真值可见 258 帧中 `98.1%` 得分为正，car002 真值可见 252 帧中 `99.6%` 为正；但 car002 已确认无正式框的 211～229 帧，四组预测框仍有 `31.6%～57.9%` 被判为可见，229 帧 car002 真值初始化预测框得分 `+1.04`。这不能提供可靠的完全遮挡门，更不能证明目标身份；只看 VisDrone 验证集 AUC 会误导。car001 181 帧四组预测框均为负，但 `car001_right` 190 帧仍为 `-1.13`，简单分类门还可能延迟正确恢复。
- 证据边界 / Handoff：这是主机离线筛查，未在 RK3588 上运行新模型，不构成板端改进或交付。保持 `9f84efe` 算法基线与 `2/8 PASS`，不向生产交接此分类器。下一步针对两目标的中心误差与初始框扰动继续找可解释、可复现的定位改动；先以两目标全 271 帧和各四初始框评分，再考虑板端验证。

## 2026-11-14T07:05:40+08:00 官方 SiamMask 完整车体掩码的双目标桌面筛查否决

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，分析基线 `cbcfef109eeb6d8a2966ba0b422294a6f8cabaeb`，工作树开始干净；正式算法和配置未改。只用 canonical `/home/u20/single-object-tracking/data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d` 与两目标各自已确认 GT。官方 SiamMask 源码 `repos/siammask@a2b3ce3dca23b3a911d96b791e007eec6d39a625`，`experiments/siammask_sharp/SiamMask_DAVIS.pth` / `config_davis.json` SHA-256 `d5a0e7cb13e2799bcf93545691261e7a48413ffb54bce9cb3b7fa3c560da4204` / `ba5fc84ba741e593ac639e3210c5a58e0e860df8dff3182277a13d3a99e2bf74`。已有 runner `scripts/run_siammask_video.py` SHA-256 `b7e802aaa27b1d1514e025e55b490c747c0acdb8bf333ccb57af8b02c7daed85`，用官方 `siamese_init/siamese_track` 且 `mask_enable=true`、`refine_enable=true`，输出分割 polygon 的轴对齐外接框；实际环境 CPU 约 5 FPS，`nvidia-smi` 本轮无法连接 GPU 驱动。
- 实际用 `.venvs/siammask/bin/python scripts/run_siammask_video.py --repo repos/siammask --config repos/siammask/experiments/siammask_sharp/config_davis.json --snapshot repos/siammask/experiments/siammask_sharp/SiamMask_DAVIS.pth --video data/occ_video_1/1.mp4 --bbox <x y w h> --max-frames 271 --output <MP4> --bbox-output <TXT>` 分别运行 car001 标准初始化 `(992,582,37,39)`、car002 真值初始化 `(967,513,42,42)`；先用 car001 前 20 帧筛查，中心均误差 `1.24 px`、IoU `0.891` 后才扩完整长度。两组输出均为 271 框，MP4 均经 `ffprobe -count_frames` 验证 1920×1080×271。主机 `/tmp/nt-dual-gt-evidence/siammask-car001-nominal-271.txt` / `.mp4` SHA-256 `679ced5f98e690f0fb1cb15c501581bc33fe765a7488f62ef3030c46e0f9df3d` / `e0d919a2b3682d6585e3bb7473be7a58f4c0d064b72f0d0f4f81922f73a9aac8`；car002 真值框相应 `.txt` / `.mp4` SHA-256 `fb45ba8db275fa2d2cf1a6b7cbd41513ecef1e84a3c005de1db4c35eccc8a15f` / `113411dec45a3da0dc8ff00301a11ce894124d77a0955916a53aac97ae769df5`。运行日志 car001/car002 SHA-256 `2daacdb3d35a53165e968fe1ea024f8ebc8e59e0ba2324d2ba07ad136b0ba273` / `73622e09c1afe28ec0fa2933ae28273fefdc2782610a0a70ceac37b73e713c89`。
- 同真值执行 `python3 /tmp/nt-dual-gt-evidence/score-siammask-two-targets.py > /tmp/nt-dual-gt-evidence/score-siammask-two-targets.json`，脚本/报告 SHA-256 `322bd76474f6a0b38a15da29fbd9531d7bcd42a66833ea0bc835c3a06dd10d03` / `f2075c2ebbf820fbd4dd438b4a4d07b3223e8755052066e2ff2629542c1d5314`；比较了 `9f84efe` 精确 RK3588 基线同初始框 CSV 的 tracking 框。car001 前 169 个可观测帧 SiamMask/NanoTrack 中心均误差 `3.41/3.36 px`，IoU `0.802/0.768`；第 182～194 帧 SiamMask 跟到相似车辆，IoU `0`、中心 `74.38 px`，第 240～270 帧仍 IoU `0`、中心 `190.12 px`，全部 258 个可观测帧中心 `57.51 px`，远劣于 NanoTrack `4.28 px`。car002 全部 252 个可观测帧 SiamMask/NanoTrack 中心 `2.93/2.00 px`、IoU `0.632/0.883`；其 15 个人工确认无正式框帧 SiamMask `15/15` 仍给非空掩码外接框，不能作为完整目标可观测性门。
- **裁决 / Handoff**：这两个完整 271 帧代表初始框已分别证明 SiamMask 的 car001 身份漂移与 car002 过大/无框输出；因此不扩大它到其余六框，也不把现成掩码并入 tracking/control 或把 detector 的 `display_bbox` 当作定位修正。桌面独立跟踪器没有生产控制器和 RK3588 时延，此结果只用于否决该直接替换路线，不外推所有分割网络。原八框正式基线仍为 `9f84efe` 的 `2/8 PASS`，handoff HOLD；后续应优先提供能判别完整目标及相似车身份的冻结模型，再用两目标八框板端闭环验证。本条与诊断文档同提交。

## 2026-11-14T07:00:12+08:00 RKNN 模板特征的候选身份与完整可观测性筛查

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，基线 `e7aa84a725b6fb59aa63ea4430079119f54b2f57`，开始时工作树干净；正式算法、配置和 GT 未修改。用 `/tmp/nt-dual-gt-evidence/template-feature-probe.cpp`（SHA-256 `04678226186108396203161bc3503ff7b1457ad51ce853e4e3ad2028910ab253`）复制 `NanoTrackV3::extract_template_feature` 的裁剪、frame0 BGR 全帧均色及 127×127 模板输入语义，调用真实 RK3588 `RknnModel` template backbone。223 项输入请求 `/tmp/nt-dual-gt-evidence/template-feature-requests.tsv` SHA-256 `acb8412c6272a5bdb950d6ea206a944d56bd98d9c17b9a656d3b97215f51cb39`，来自两目标各四初始 ROI、`9f84efe` car001 四组已哈希绑定板端 CSV、`c80c78c` car002 四组被否决候选 CSV，以及各自对应真值图块。真值仅供事后对照，未参与恢复逻辑或训练。
- 板端目录 `/home/neardi/Project/SOT/dual-gt-handoff-9f84efe/runs/template-feature-probe/`；实际用 `g++ -std=c++17 -O2 -I rk3566/nanotrack_v3/include -I /home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/include runs/template-feature-probe/template-feature-probe.cpp rk3566/nanotrack_v3/src/rknn_model.cpp -o runs/template-feature-probe/probe -lrknnrt $(pkg-config --cflags --libs opencv4)` 原生构建，二进制 SHA-256 `92df18a671bb499bf3bf62af770691562ecf42c379883614e2ca10f0f1a05012`。运行 `runs/template-feature-probe/probe <template_backbone.rknn> <1.mp4> runs/template-feature-probe/template-feature-requests.tsv runs/template-feature-probe/features.csv`；探针完成 223/223 项、每项 6144 个 float（模型输出 `1×96×8×8`），运行日志 SHA-256 `dceb8eda9caa71a118ac48107515df43775fa25aeaa0f61b7e8e2fbb0d31c592`，特征 CSV SHA-256 `b2bbfdc612ca5d93d1c32cac32e95184ef21fedef8e6409c20989a68fa83e30d`，主机副本 `/tmp/nt-dual-gt-evidence/template-feature-features.csv` 同哈希。板端模型 `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/models/template_backbone.rknn` 与视频 `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/data/occ_video_1/1.mp4` 本轮实核 SHA-256 `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55` / `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`，RKNN API `2.1.0 (967d001cc8@2024-08-07T19:28:19)`、driver `0.9.8`。
- 主机执行 `/home/u20/single-object-tracking/.venvs/nanotrack/bin/python /tmp/nt-dual-gt-evidence/template-feature-summary.py > /tmp/nt-dual-gt-evidence/template-feature-summary.json`，脚本/结果 SHA-256 `c4af479a5b651959dcc8265241927dd47a3e34f969d7fc1155cc88285cffc2d8` / `128e986ba42c7c900364d4430a70c54b65c41637b8e45e9762f13d2060aae755`。以第 0 帧实际 ROI 模板与同组候选模板的**扁平化 6144 维原始余弦相似度**比较：car001 第 181 帧不可靠 recent 候选四框约 `0.443～0.493`，首次经 GT 证实可靠的 recent 框（标准/人工第 187、左上第 188、右下第 190 帧）约 `0.707～0.734`。但 car002 第 229 帧人工确认无正式框的 safe 候选四框相似度 `0.668～0.713`，均高于各自第 230 帧首次完整可观测**真值框**的 `0.653～0.666`，差额 `+0.014～+0.047`；raw feature norm 同样没有四框一致的部分/完整目标分界。这证明“初始模板余弦达到单一阈值即可发布完整目标”在这组反例中不成立，不能用它替换现有 NanoTrack 两击或无框门禁。
- **证据边界 / Handoff**：这是独立板端特征抽取和离线排序，使用了候选及真值图块，但没有进入 runtime、没有闭环轨迹、没有测额外推理的正式帧时延；仅否决该原始特征与**初始**模板余弦的简单完整性门槛，不推断所有学习型外观/遮挡模型均无效。后续模型/外观 owner 可用冻结的 223 图块、特征 CSV、两目标 GT 和八框板端轨迹设计跨初始框、同类负样本与部分遮挡验证器；任何晋级仍须在 RK3588 用正式八框检查 tracking/display 中心与 IoU、无框段和身份连续性。`display_bbox` detector 仍只作外观用途；正式基线 `9f84efe` 八框 `2/8 PASS`、handoff HOLD。本条与诊断文档同提交。

## 2026-11-14T06:55:03+08:00 搜索状态与 detector 间隔组合的八框板端否决

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，分析与运行基线 `69cbae84c2ecc5a7fd2efc61f555c77e5dd5175b`，开始时工作树干净。沿用精确源码 `9f84efecd57d611a239cc71c99ab2e4295025dbc`、RK3588 隔离根 `/home/neardi/Project/SOT/dual-gt-handoff-9f84efe`、原生二进制 `build/nanotrack_v3` SHA-256 `41d83ef0d772833b29400a35cacb8825a1a402f24ea7dbfe9279ac09b062343b`。板 `LKD3588` Linux `6.1.84`、RKNN API `2.1.0 (967d001cc8@2024-08-07T19:28:19)`、driver `0.9.8`，视频 `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；官方 NanoTrack template/search/head RKNN、tracker manifest、VisDrone9 detector/manifest/labels 与下方 `9f84efe` 已哈希绑定八框矩阵相同。本次只在板端 `runs/` 新建配置与输出，没有覆盖正式源码或模型。
- 逐帧时序显示，car001 右下框原 400 ms 参数在第 188 帧发恢复 detector 请求、189 帧取回上一帧结果、190 帧才首次用 detector 候选作为 NanoTrack 探针，所以正确框在 190/191 帧形成两击。只改 `detector_interval_ms=400→360` 的板端 controller/headless 配置 `runs/detector360-controller.conf`/`runs/headless-detector360.conf` SHA-256 `c5f7599fafe13f609f64b0f90d1d9f69bc8d97049d8bea3f9a157a787a1109c7`/`c98f2eeffab96124627ce7620f5f42718450b1ba84cf6f0ae470eca7a04d3154`；右下框完整 271 帧 CSV/AVI `runs/detector360/car001_right/frames.csv`/`output.avi` SHA-256 `7d258253051b743b7e57e29464282a5c5ced5ae30fe6c714ff8d97c92c810419`/`dff2a0ebf6733b3f00102108bdd16a7bf97132b400b2756732b2a1f477f3a2de`，评分 `/tmp/nt-dual-gt-evidence/detector360/car001-right-score.json` SHA-256 `2aa086654a51be9301ff12d204130c0a2b5deff19d6091ef5846dc15c2f715ce`：第 190 帧重捕、tracking/display 中心均误差 `4.91/4.94 px`，仍因第 172～174 帧完全遮挡语义和第 190 帧晚重捕 FAIL。
- 只改 `uncertain_ms=200→100` 的板端 controller/headless 配置 `runs/uncertain100-controller.conf`/`runs/headless-uncertain100.conf` SHA-256 `a59d9e8ed394b9a1b95074b4217b469254f1fb401adda8af10a4ef16ff8c89c1`/`262fbf0b8664abf9fe16c91e9a9822c783f8254d43566c92efc84cdf7aa2b794`；右下框 CSV/AVI `runs/uncertain100/car001_right/frames.csv`/`output.avi` SHA-256 `de2359cbb18222e90c946e89738b5e3365eb752318d852d995dbff472f6dc553`/`d06493f562a6d94a3f10c78af4c547dadb7821dda6191ce303698844d456ea3e`，评分 `/tmp/nt-dual-gt-evidence/uncertain100/car001-right-score.json` SHA-256 `e0302fe0c295d8e8614335adec3bdf2d7baac6347b9731901936bd3768bf782f`：完全遮挡语义通过，重捕提前至第 190 帧，tracking/display 中心 `4.89/4.92 px`，仍过截止而 FAIL。两单参数结果不能外推组合的八框表现。
- 组合 `uncertain_ms=100`、`detector_interval_ms=360` 的板端 controller/headless 配置 `runs/uncertain100-detector360-controller.conf`/`runs/headless-uncertain100-detector360.conf` SHA-256 `ed586052470ee5f0dae45bcd82231c100c5c0e121912f4f475b47b584c8e3eb1`/`61088c73ba9479ed6e98d846b8bdba1fe46e0ee53d1616acfc764d416484419b`。实际逐组运行 `./build/nanotrack_v3 --config runs/headless-uncertain100-detector360.conf --bbox <ROI> --output-video runs/uncertain100-detector360/<label>/output.avi --output-boxes .../frames.csv --session-metadata .../session.json`，目标与 ROI 为 car001 标准/人工/左上/右下 `(992,582,37,39)`/`(993,585,36,36)`/`(990,580,39,41)`/`(994,584,35,37)` 和 car002 标准/真值/左上/右下 `(970,517,38,38)`/`(967,513,42,42)`/`(968,515,40,40)`/`(972,519,36,36)`。其余七组运行脚本 `runs/uncertain100-detector360/run-seven.sh` 主机 SHA-256 `e6018e9d86464a909f410a49afadd1ccb375a1257b2c911054e1b6f9e40dc334`。八组 exit 0，八个 AVI 经 `ffprobe -count_frames` 均为 1920×1080×271；CSV/JSONL/AVI/session/runtime.log 的 40 文件清单 `runs/uncertain100-detector360/eight-outputs.sha256` SHA-256 `856db8b869d7a192898861dc9f48f5126dcacedd560c31f17032980503ced65d`，板端 `sha256sum -c` 40/40 OK。八组文本包 `runs/uncertain100-detector360/eight-text.tar.gz` SHA-256 `4c61ca94c75c185442e6f89820f537e9484044c18138493c3435c2ce7ed44a12`，主机回传 `/tmp/nt-dual-gt-evidence/uncertain100-detector360/` 哈希相同，解包后 24 个 CSV/session/log 与板端清单一致；AVI 大文件留板端。
- 用 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1` 对匹配真值的八个 `(target, label, ROI, CSV)` 运行完整评分，报告 `/tmp/nt-dual-gt-evidence/uncertain100-detector360/eight-center.json` SHA-256 `5c7d819bdb9c0e661d30e78853a8d4609143227004187d874df55e5bc002ec1e`。car001 右下框单组 PASS，第 189 帧重捕，tracking/display 平均中心误差 `4.82/4.85 px`；但标准/人工/左上三框都在第 181 帧错误重捕，之后第 199/219/219 帧再次重捕，各自 tracking 中心均误差 `28.73/27.39/27.49 px`，正式 FAIL。car002 四框全 FAIL：标准/真值/右下主要因完全遮挡语义，左上还在第 145/245 帧双重捕并丢失可见框；四框 tracking 中心 `1.99/1.82/5.03/3.48 px`，已确认无框段没有假观测并不足以抵消其他失败。跨初始框最大成对中心差 P95：car001 tracking/display `77.00/76.78 px`，car002 `31.83/27.33 px`，远差于 `9f84efe` 基线 `3.31/3.12`、`6.21/6.21 px`。detector `display_bbox` 仍只做外观，两路中心分别评测；单组右下 PASS 不构成双目标一致性改进。
- **裁决 / Handoff**：组合参数明确否决，板端隔离配置和八组输出保留用于复核，正式 `rk3566/nanotrack_v3` 配置、源码、模型均未修改。缩短进入搜索状态和 detector 请求间隔改变多候选调度，导致 car001 三组先捕错车；不能用右下单组通过作为晋级依据。仍以 `9f84efe` 正式八框 `2/8 PASS` 为算法基线，handoff HOLD。后续应先形成抗相似车的候选身份与完整可观测性证据，再重跑同一八框及两路中心；本条与诊断文档同提交。

## 2026-11-14T06:44:05+08:00 两目标重捕窗口的逐帧可用性诊断

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，分析基线 `c371c276910621b079b0588e5c9312f276896f42`，开始时工作树干净；正式算法文件未改。严格加载 canonical `data/occ_video_1/1.mp4` 与 car001/car002 各自已确认真值；视频 SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`。对 `9f84efe` 八框基线的 car001 四组板端 CSV（`/tmp/nt-dual-gt-evidence/handoff-9f84efe/<label>/frames.csv`）和 `c80c78c` 被否决候选的 car002 四组板端 CSV（`/tmp/nt-dual-gt-evidence/strong-score-c80c78c/runs/<label>/frames.csv`）执行 `python3 /tmp/nt-dual-gt-evidence/recovery-window-diagnosis.py > /tmp/nt-dual-gt-evidence/recovery-window-diagnosis.json`。脚本/完整帧级报告 SHA-256 `51eb3fa6efaf82a5b1f5175eeff6514773a62dbb2eb507b385592fe7dc80f70a` / `3816ff6d595ea2d09e13624d001326fd84d988dd83d5e0bf609515af5574f108`，每组报告绑定 CSV 自身 SHA-256、源码 commit、candidate source、冲突、确认击数、原始框/分数及与对应真值的 IoU/中心误差；只作事后诊断，真值没有进入运行时。
- car001 标准/人工框第 187 帧已各有贴近真值的 recent/safe 原始框，随后第 188 帧两击重捕，正式 PASS。左上框第 188/189 帧 recent 原始框 IoU `0.74/0.73`、中心误差 `3.0/5.1 px`，但身份冲突持续至第 189 帧，确认击数为零；第 190/191 帧才两击并重捕，晚于正式第 190 帧的排他截止。右下框直到第 190 帧才首次出现 IoU `0.81`、中心误差 `3.5 px` 的 recent 原始框；第 189 帧 recent/safe IoU 仅 `0.00/0.27`，当前两击语义最早第 191 帧重捕。左上是可用候选被冲突挡住，右下是此前根本没有可靠的目标观测，不能用同一个冲突阈值解释两者。
- car002 被否决候选的真值/右下初始框在人工确认无正式框的第 228/229 帧取得同一 detector 候选的两次 NanoTrack 命中，第 229 帧误重捕；该帧 recent/safe 原始分数分别约 `0.9975/0.9980` 和 `0.9966/0.9968`，detector 框高约 `42.39 px`，recent 原始框高约 `45.71/45.03 px`。第 230 帧首次有可观测真值，正式框高约 `55 px`。高分数只能证明模板响应强，不能证明完整目标已可观测；直接降低 car001 两击要求会增加 car002 第 229 帧同类风险。car002 数据包 `annotation_metadata.json` 的 `formal_evaluation_authorized=true`、`unresolved_review_count=0`，第 229 帧无框状态已人工确认；事件文件保留早期 `needs_human_review` 字样，正式状态以当前元数据、逐帧标注和评估契约为准。
- **裁决 / Handoff**：不以真值帧号做运行时延迟，也不为追单组重捕时间直接降低两击或抬高 raw score 门。两目标实际需求是同时具备完整目标可观测性、候选身份和目标定位证据；现有官方 RKNN 模型及候选表尚未达到八框要求。请协调者将帧级 JSON、八框 CSV 与对应两目标 GT 提供给模型/外观评估 owner，特别审查 car001 第 187～191 帧和 car002 第 228～230 帧的模板响应/遮挡下误确认；新模型或验证器须以冻结哈希在 RK3588 八框闭环复验 tracking/display 中心、IoU、无框段及跨框差。正式八框基线 `2/8 PASS`、handoff HOLD，本条与诊断文档同提交。

## 2026-11-14T06:37:07+08:00 双真值八框重测：高分恢复候选否决并撤回

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，测试源码提交 `c80c78ca0063b6470a5219142c20fd6157ef4dfe`，起始工作树干净。`git archive --format=tar c80c78c` 的板端隔离源码包 SHA-256 `c6edf93fec3def196955f4741324af3afbc535afe54cf20451734430e0b5904d`，板端目录 `/home/neardi/Project/SOT/dual-gt-strong-score-c80c78c`，未覆盖 `9f84efe` 基线。controller/headless 配置 SHA-256 `782a0b2bffcd11ddf67cca4aa40eeae5e0ec8a8d79ff9c3b8d8a547449e685c8` / `209054f9c16515d58d3f78f4bc12d0a245749e7948e21f815966d4e912b89838`。板端原生 CMake、build、CTest 日志 `runs/cmake.log`、`runs/build.log`、`runs/ctest.log` SHA-256 依次为 `b8fa5de7f40fde2fd7b4da7be251777801622f97da57c0fb77cf1268b4e454e9`、`9d95cddf64c737c18f39c4fa6587e552ac758784e87eb51544cfb77a2678c18e`、`b47142f3b598dce36e718e793898110e96cdee811954a55a8ef35a31adf67ce0`；板端 `23/23 PASS`，二进制 `build/nanotrack_v3` SHA-256 `9ddd884a619dcebbee9dc8b5032206fc479b246ced581c88c2076cef97a68710`。
- 板 `LKD3588`、Linux `6.1.84`、RKNN API `2.1.0 (967d001cc8@2024-08-07T19:28:19)`、driver `0.9.8`。输入视频板端路径 `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/data/occ_video_1/1.mp4`，SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；官方 NanoTrack template/search/head RKNN SHA-256 依次为 `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55`、`d3cb0e3879df6a1272adb59bf501324f1f8f55b58309d3c4da4a731b600973ee`、`4da379f54093d79f02775e90bfea33ebee0837c6fc73a324499279b4c57807c5`，tracker manifest `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`；VisDrone9 detector/manifest/labels `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c` / `ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9` / `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`。上述输入哈希在本轮板端重核；GT 沿用 `data/occ_video_1/ground_truth/car_001` 与 `car_002`，目标和初始化框严格配对。
- 板端分别以 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox <ROI> --output-video runs/<label>/output.avi --output-boxes runs/<label>/frames.csv --session-metadata runs/<label>/session.json` 运行八组；`car001_nominal/user/left/right` 的 ROI 为 `(992,582,37,39)` / `(993,585,36,36)` / `(990,580,39,41)` / `(994,584,35,37)`，`car002_nominal/truth/left/right` 为 `(970,517,38,38)` / `(967,513,42,42)` / `(968,515,40,40)` / `(972,519,36,36)`。八组 exit 0，八个 AVI 均经 `ffprobe -count_frames` 验证为 1920×1080、271 帧。输出 CSV/AVI/session/runtime.log 留在板端 `runs/<label>/`；32 文件 `runs/eight-outputs.sha256` SHA-256 `1226a45ff088d64f7b716f7430ced56b375afb0fa7f07f97020ca4687f24d7f7`，板端 `sha256sum -c` 全部通过。八组文本包 `runs/eight-text.tar.gz` SHA-256 `120418dc59b62a8158ff99f5e49d556036194bd9d9a32b9b331af2b8cbb71b55`；主机回传 `/tmp/nt-dual-gt-evidence/strong-score-c80c78c/` 两文件哈希一致，解包后的 24 个 CSV/session/log 与板端 manifest 一致，视频只留板端。
- 主机实际执行 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1`，分别传入上述八个 `(target, label, ROI, CSV)`，输出 `/tmp/nt-dual-gt-evidence/strong-score-c80c78c/eight-center.json` SHA-256 `ae8f49a3425532adaa0c76144d44a4a4ada99fee4110095d101b2f039930dd42`。正式门禁 `3/8 PASS`：car001 标准/用户框 PASS，左上框因重捕第 191 帧 FAIL，右下框因完全遮挡语义和第 191 帧重捕 FAIL；car002 标准/左上框 PASS，真值/右下框都在人工确认无框第 229 帧发生假观测与过早重捕，FAIL。car002 四框重捕帧 `231/229/241/229`。car001 四框可见帧 tracking/display 平均中心误差约 `4.28/4.32`、`4.63/4.69`、`4.34/4.35`、`5.02/5.05 px`，与 `9f84efe` 逐项相同；car002 为 `2.12/2.12`、`1.68/1.68`、`2.47/2.34`、`2.70/2.70 px`。car002 跨框 tracking/display 最大两两中心差 P95 `6.29/6.29 px`，较基线 `6.21/6.21 px` 退化，car001 约 `3.31/3.12 px` 不变。detector 的 `display_bbox` 仅用于视觉效果，两路中心和 tracking IoU 都按各自真值独立评分；不能以显示框外观替代准确性。
- 误重捕第 229 帧的 `candidate_source=DETECTOR`，两框 NanoTrack recent/safe 原始分数约 `0.997`；提高 `0.90` 分数门槛不能可靠排除无框目标。单纯把 detector 两击确认延长一帧，又会使已在第 241 帧才重捕的左上框越过当前截止帧，故不把该调参视为完成。**裁决 / Handoff**：撤回 `c80c78c` 的两项门禁、配置和专属测试；撤回后 `rk3566/nanotrack_v3` 内容与已验证 `8277b33` 逐文件一致。保留候选 commit 与隔离板端证据；正式算法仍以 `9f84efe` 八框 `2/8 PASS` 为验证基线，handoff HOLD。下一步处理候选身份与可观测性，并要求双目标八框两路中心/IoU 和跨框一致性同时改善；不得仅调整评分或 `display_bbox` 掩盖跟踪误差。本条与撤回代码同提交，结果以实际 commit 为准。

## 2026-11-14T06:24:45+08:00 强遮挡提前搜索加 NanoTrack 高分确认候选

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `04539d9`，开始时工作树干净。恢复上一已提交候选 `0c45bca` 的强遮挡连续两帧搜索逻辑，仅在新候选中叠加可关闭的 `recovery_confirm_min_raw_score`，当前 `config/visible-adaptive-display.conf` 取 `0.90`，配置 SHA-256 `782a0b2bffcd11ddf67cca4aa40eeae5e0ec8a8d79ff9c3b8d8a547449e685c8`。恢复候选仍进入身份候选表与冲突判断；达到两次 NanoTrack 命中后，只有**当帧**待提交模板的原始分类分数达到门槛才授权重捕。detector 不直接授权控制，`display_bbox` detector 仅用于外观。
- 反事实依据是同源 `9f84efe` 八框板端日志中八个正确重捕帧的当帧 recent 原始分数最低为 `0.917303`；已否决的 `0c45bca` car002 标准框误重捕第 226/242 帧所选 SAFE/RECENT 原始分数分别 `0.884239/0.813238`。这只说明该阈值值得闭环测试；部分遮挡或相似车辆仍可能得到高分，不能凭冻结 CSV 宣称安全或准确。配置解析/校验/指纹已包含新字段，测试验证低分两击仅保留候选、后来高分时才可由 NanoTrack 提交。实际执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`；`git diff --check` 通过。
- **证据边界 / Handoff**：尚未在 RK3588 运行，正式已验证内容仍为 `9f84efe` 八框 `2/8 PASS`、handoff HOLD。下一步用精确提交部署独立目录，先重跑 car002 标准框；若它避开无框误重捕且中心、重捕时间均不退化，再测 car001 标准/右下、car002 左上并扩八框。不能以只改善完全遮挡语义代替两路中心与初始框一致性。此条与候选代码同提交，结果以实际 commit 为准。

## 2026-11-14T06:20:49+08:00 强遮挡两帧提前搜索的 RK3588 否决与撤回

- 精确候选 `codex/dual-gt-init-consistency@0c45bca`，工作树 `/home/u20/single-object-tracking`。以 `git archive --format=tar 0c45bca -o /tmp/nt-dual-gt-evidence/strong-vis-0c45bca-source.tar rk3566/nanotrack_v3 scripts` 生成并传输源码，主机/板端 `source.tar` SHA-256 均 `b4cc6db339832cbc96f434c5dabc760bddb844f8bef9700945b624e81e9116a7`；板端隔离目录 `/home/neardi/Project/SOT/dual-gt-strong-vis-0c45bca`，未覆盖 `9f84efe` 基线。实际板 `LKD3588` Linux `6.1.84`，session 实报 RKNN API `2.1.0 (967d001cc8@2024-08-07T19:28:19)`、driver `0.9.8`。原生运行 `cmake -S rk3566/nanotrack_v3 -B build -DENABLE_RKNN_RUNTIME=ON -DBUILD_TESTING=ON -DRKNN_INCLUDE_DIR=/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/include -DRKNN_LIBRARY=/usr/lib/librknnrt.so -DCMAKE_CXX_FLAGS=-Werror=switch`、`cmake --build build --parallel 4`、`ctest --test-dir build --output-on-failure`，`23/23 PASS`；日志 `runs/cmake.log`/`runs/build.log`/`runs/ctest.log` SHA-256 `a77115191c3a613aab6664f99963ba9363fa9b2f0d176acdfa2c4eb822a25856`/`0b9f022f802aa538d187f24e67621c87bd20173977acbc646688d218c73a1e02`/`a30ff0a264613531b4aebbb480881db140011b3381814208b56d40c2d67ad2b8`；二进制 `build/nanotrack_v3` SHA-256 `3f4aa6a14fbf8c828266d6e2b935ad7a68db9b60c8a9ecbc03e2e09f86a9330e`。
- 板端视频 `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；官方 NanoTrack template/search/head RKNN SHA-256 `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55`/`d3cb0e3879df6a1272adb59bf501324f1f8f55b58309d3c4da4a731b600973ee`/`4da379f54093d79f02775e90bfea33ebee0837c6fc73a324499279b4c57807c5`，manifest SHA-256 `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`；RK3588 VisDrone9 detector/manifest/labels SHA-256 `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c`/`ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9`/`41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`。候选 controller/headless 配置 SHA-256 `d35d7d608b45a0d1fee1a05d97d53d5946f554009bb0ddb5ff6dd19bba99ce69`/`f44c3b1dbd0dc0109e846af5313e94f58e9b25758832b46073b6b84e9d93268c`。
- 板端实际执行 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox 970,517,38,38 --output-video runs/car002_nominal/output.avi --output-boxes runs/car002_nominal/frames.csv --session-metadata runs/car002_nominal/session.json`，runtime exit 0；AVI 经 `ffprobe -count_frames` 为 `1920×1080×271`。CSV/AVI/session/log SHA-256 `f862b4817073c2695dc6ebd69e0f13a79ebd97fc64d30fe24120b0c7bd197b64`/`157d20dfdb79c9665637362168abebd7cb7556a4694ac2e0cba8cc7fa617e660`/`aef9c64625bd11021947ac118d584bfe06adfee3f9716ba3656775b8880263da`/`c4eab06f4501e4454132e46d9d87d6a1f74ab7b07da18dbdf007b0c52040ceec`。AVI 等大工件留在板端；CSV/session 小工件回传 `/tmp/nt-dual-gt-evidence/strong-vis-0c45bca/car002_nominal/` 且两端哈希相同。执行 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run car_002 car002_nominal 970,517,38,38 <回传 CSV> --output-json /tmp/nt-dual-gt-evidence/strong-vis-0c45bca/car002_nominal/score.json`，报告 SHA-256 `073d9d5308009464143e9ca1e98b885bd7feb92106d5ba07fba7f4a0a077f21d`。
- 第 213 帧如预期转 `SEARCHING`，213～216 帧 `observation_valid=0`、`control_source=MOTION_SEARCH`、`control_valid=1`，因此原先 car002 完全遮挡语义门改善；但第 226 帧在人工确认无框段误重捕，随后第 `242/263` 帧再次重捕。正式评分 `FAIL`，失败门为 `visible_boxes`、`reacquisition`，无框段假观测 `1` 帧；可见 tracking/display 中心均误差 `4.53/4.48 px`，原 `9f84efe` 标准框为约 `2.59/2.59 px`，两路中心均退化。单个代表框已构成决定性反例，按筛查顺序未扩大其余三框或八框，不把未运行的框声称为通过。
- **裁决 / Handoff**：撤回 `0c45bca` 的候选代码、配置和专属单测；撤回后的 `rk3566/nanotrack_v3` 全部文件与已验证 `8277b33` 内容一致，正式配置 SHA-256 恢复 `5d3214646bd3b0e6d38213482178aa44a0deb301d6f303c8b5eaa7f5517d3a3b`。保留候选 commit 和板端隔离根以复核误重捕；不为通过无框门而仅改 CSV 语义或显示框。`9f84efe` 正式八框仍 `2/8 PASS`、handoff HOLD。本条与撤回代码同提交，结果以实际 commit 为准。

## 2026-11-14T06:16:07+08:00 强遮挡证据连续两帧提前搜索候选

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，代码基线 `8277b3380358d75b604e3e7252c066e5afd09cf7`，开始时工作树干净。只用 `9f84efe` 精确 RK3588 八框 CSV（`/tmp/nt-dual-gt-evidence/handoff-9f84efe/<label>/frames.csv`）与 canonical 双目标真值分析，不改视频或标注；视频 `data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`。car002 四框在第 212/213 帧均连续两次满足“当前帧 NanoTrack decoded 高度 ≤ 当前可信框 `0.95`，且相对预测中心残差 `>0.15`”；car001 四框均无连续两帧满足。car002 左上框在可见的 120～142 帧虽有持续中心残差，却因 decoded 高度不缩小而不满足。该回放只验证既有轨迹上的判别，不等于改动后的闭环效果，也不证明跨场景泛化。
- 在 `rk3566/nanotrack_v3` 增加默认关闭的 `recent_visibility_search_height_ratio`、`recent_visibility_search_center_residual`、`recent_visibility_search_consecutive_frames`，当前 `config/visible-adaptive-display.conf` 取 `0.95/0.15/2`，配置 SHA-256 `d35d7d608b45a0d1fee1a05d97d53d5946f554009bb0ddb5ff6dd19bba99ce69`。仅已确认 car 类、decoded 高度与运动残差同时满足且帧号连续时，第二帧从临时观测进入 SEARCHING；单帧仍为 SUSPECTED，干净帧、初始化、异常恢复和结束可疑状态清零。detector 候选与 `display_bbox` 只按现有语义使用，未让 detector 框直接授权 tracking/control。配置解析、校验与指纹包含新参数；负例测试覆盖单帧、干净帧打断和非 car 类。
- 实际执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4` 和 `ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`，`git diff --check` 通过。SSH 实测 `neardi@192.168.1.106` 为 `LKD3588` Linux `6.1.84` 且保留精确基线 `/home/neardi/Project/SOT/dual-gt-handoff-9f84efe`。**证据边界 / Handoff**：本候选尚未上板，主机测试不证明 car002 第 213 帧可合法输出 MOTION_SEARCH，也不证明 car001 三个微扰框的恢复、两目标中心或八框正式门槛会改善。下一步以本条精确提交归档部署到隔离目录，先跑 car002 标准/左上与 car001 标准/右下，若无退化再扩八框；正式 `9f84efe` 八框仍 `2/8 PASS`、handoff HOLD。本条提交后以实际 commit 为结果身份。

## 2026-11-14T06:10:08+08:00 官方原始响应峰值与局部加权中心八框筛查

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `88e2af2`，开始时工作树干净。用 `repos/siamtrackers/NanoTrack/models/pretrained/nanotrackv3.pth` SHA-256 `4f0d25108385a67bdec5fa01ca37c74acb72343b53996dd645fe18b86e512e69` 和配置 `repos/siamtrackers/NanoTrack/models/config/configv3.yaml` SHA-256 `60ea9dc886dcdd0451e60b404490a39d19495480185ca7b6299b72b74d746f87`，输入仍为 canonical `data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`，评价分别使用两目标已确认真值。此处是官方 PyTorch 桌面单跟踪器，不是现有 RK3588 RKNN 加控制器链；不把离线解码称作已验证的板端改动。
- 原型 `/tmp/nt-dual-gt-evidence/soft-center-screen.py` SHA-256 `9180c6a607969882a8c29213465a25fd850a41768d57efe311f4ddb7c0a29f8b`，逐帧执行与官方 `NanoTracker.track` 相同的 `cls/loc` 解码和状态更新，同时只读计算最佳格邻域 3×3 各候选中心按 `pscore` 的 softmax 权重，温度 `0.01/0.03/0.07`；加权中心不反馈到下一帧。先运行原官方 `scripts/run_nanotrack_video.py` 与原型的 car001 标准框各一次，原输出 `/tmp/nt-dual-gt-evidence/official-car001-nominal.csv/.mp4` SHA-256 `6f13e7c03359943630a741bdef269c3fadc05d5b92aa2b244dafebc4081686b5`/`8f89eafa2644f396636c9e9c29cc0acc968efd3735856f67afa878a175e3245a`；271 帧框坐标与原型的基线最大差 `4.997e-7 px`，确认诊断复现原算法。再以 `env OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 .venvs/nanotrack/bin/python /tmp/nt-dual-gt-evidence/soft-center-screen.py --config repos/siamtrackers/NanoTrack/models/config/configv3.yaml --snapshot repos/siamtrackers/NanoTrack/models/pretrained/nanotrackv3.pth --bbox <XYWH> --output /tmp/nt-dual-gt-evidence/soft-official-<目标>-<框>.csv` 对两目标各四初始框跑完整 `271` 帧，八次均 exit 0；ROI 同上一条。八 CSV 清单 `/tmp/nt-dual-gt-evidence/soft-official-eight.sha256` SHA-256 `3399e09c6a678ff7c223e7a3e6d37afe8971190667de1c557d5a1182054e538e`，逐文件核验通过。
- 运行 `python3 /tmp/nt-dual-gt-evidence/score-soft-official.py > /tmp/nt-dual-gt-evidence/score-soft-official.json`；脚本/报告 SHA-256 `7f629b6e8254587893cdfc74ddb17b13be417000a55264f8cc6afc9d0cd76676`/`2a73d74a02ba5c1ed784a9eb32820d0dfa24b1c77e929dfe0c836e08118a59f1`。car001 四框遮挡前 0～159 帧最佳格中心均误差 `2.28/2.18/2.47/2.09 px`，温度 0.03 变为 `2.25/2.14/2.50/2.02 px`；后段 230～270 帧原始四框均约 `126 px`，加权最多降不足 `1 px`，没有纠正错车。car002 遮挡前最佳格四框 `1.52/1.36/1.29/1.70 px`，温度 0.03 为 `1.40/1.43/1.26/1.58 px`；真值框退化。car001 遮挡前跨框中心距 P95 原始 `1.78 px`，温度 0.03/0.07 为 `1.91/2.13 px`；car002 全段温度 0.01 仅 `2.76→2.74 px`。局部加权不足以同时改善两车的精度和一致性。
- **裁决 / Handoff**：不把局部 softmax 中心解码接入 `tracking_bbox`/控制，`display_bbox` detector 仍仅作视觉外观。旧官方 PyTorch 跟踪器的遮挡后错车也说明单帧响应的亚像素微调不是当前身份分叉的主要解法。正式 `9f84efe` RK3588 八框仍 `2/8 PASS`，handoff HOLD；下一步应针对恢复阶段的多候选身份与模板特征，且必须实际复核板端闭环、两目标可见中心与无框段。本条提交后以实际 commit 为结果身份。

## 2026-11-14T06:06:21+08:00 集成契约 g5000 权重的双目标四框桌面筛查

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `56eb83c8e9e25329b26a9ca2ced1fc2ccc455df4`，开始时工作树干净。先按 `docs/progress/integration-contract.yaml` 复核冻结 checkpoint `artifacts/nanotrack_conservative_three_stage/run/stage1/candidate_stage1_g00005000_e001_s02500.pth` SHA-256 `40ebbe4296938bf5d53c8605ab05e729b306fc0bce65921a57ab6613030410d1`、配置 `configs/nanotrack_v3_three_stage_stage1.yaml` SHA-256 `cb3065982818a701a7ebb469c41b894b0a6c11e7aac6cb91dc42e5ed50547f2e`，与契约相符。输入仅用 canonical `data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d` 和匹配的 car001/car002 已确认真值。
- 以 `env OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 .venvs/nanotrack/bin/python scripts/run_nanotrack_video.py --repo repos/siamtrackers/NanoTrack --config configs/nanotrack_v3_three_stage_stage1.yaml --snapshot artifacts/nanotrack_conservative_three_stage/run/stage1/candidate_stage1_g00005000_e001_s02500.pth --video data/occ_video_1/1.mp4 --output /tmp/nt-dual-gt-evidence/g5000-<目标>-<框>.mp4 --bbox-output /tmp/nt-dual-gt-evidence/g5000-<目标>-<框>.csv --bbox <XYWH>`，依次跑 car001 标准 `992,582,37,39`、人工 `993,585,36,36`、car002 标准 `970,517,38,38`、真值 `967,513,42,42`。四次均 exit 0、271 帧、CPU `tracking_fps=68.79/68.85/71.31/70.75`。8 个 CSV/MP4 的逐文件哈希清单 `/tmp/nt-dual-gt-evidence/g5000-four-outputs.sha256` SHA-256 `06425ce8ca53fee27374a39a992d6d2cceca8ea0c53f44a9c693e83baf12283f`，`sha256sum -c` 通过。
- 执行 `python3 /tmp/nt-dual-gt-evidence/score-g5000-four.py > /tmp/nt-dual-gt-evidence/score-g5000-four.json`，脚本/报告 SHA-256 `f1a3555aa84ac3757bb72dc08f935c5f319c65ebab05930dd7922ee619b236ac`/`a447ff9d8479b0fdbdb314fbbbc9d2ede63c27b9e04e7a40ef6c4c0a5f041549`。仅计正式可观测框：car001 两框各 258 帧，中心均误差 `35.03/34.98 px`，后段 230～270 帧 `124.30/124.72 px`，两框相互接近但同时跟错；car002 两框各 252 帧，标准/真值全段 `32.63/2.89 px`，标准后段 `190.58 px`，两框跨 ROI 中心距 P95 `215.09 px`。因此近似一致的两个错误轨迹不能当作准确定位证据。
- **裁决 / Handoff**：g5000 原 checkpoint 不能直接作为双目标一致性替换；不把它转换成 RKNN 或调整当前板端模型。桌面单跟踪器缺少生产控制器、detector、`display_bbox`、无框状态和反馈恢复逻辑，以上不是板端正式门禁，不能代替模型所有者的冻结身份或正式 promotion 决定。`9f84efe` 板端八框仍 `2/8 PASS`、handoff HOLD；下一步若研究中心解码，须从正式 board 原始响应与两目标八框出发，并验证改变中心后的闭环与遮挡语义。本条提交后以实际 commit 为结果身份。

## 2026-11-14T06:02:00+08:00 两套既有微调权重的双目标初始框敏感性筛查

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`；分析基线 `e5c80cd2c314f427b6d7606d423c46a1fed9ac3e`，开始时工作树干净。本次只在主机 `.venvs/nanotrack` 的 CPU 上运行既有单跟踪器，不修改正式 RK3588 算法或板端文件。输入为 canonical `data/occ_video_1/1.mp4`，SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`，评价严格分别使用 car001/car002 的确认真值。运行脚本 `scripts/run_nanotrack_video.py` SHA-256 `302b286429cb57fa827b0f14e4756239f8289f3a5e33bfd284a0d8d4f1ef89d9`。
- UAV123 权重 `artifacts/nanotrack_uav123_finetune/best/best.pth` SHA-256 `761eccd07df08a05932ca0b7c828fdbb85f790744159dd650bc49bb73b88f353`，配置 `configs/nanotrack_v3_uav123_finetune.yaml` SHA-256 `3e1fde4aec5617b9f88427147e9addf2cfd1b3d370d5d3671501d4529e9173f4`；四数据集权重 `artifacts/nanotrack_four_dataset_finetune/best/best.pth` SHA-256 `f19897b6ca2568f420b45e7c718ebc5f6ead353485086056fbcbe67de27c5e0f`，配置 `configs/nanotrack_v3_four_dataset_finetune.yaml` SHA-256 `2e722c135ef863c4d70843e4f1a5f0d0f20d722bb4d447fbefc5b3603955222b`。实际命令形如 `env OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 .venvs/nanotrack/bin/python scripts/run_nanotrack_video.py --repo repos/siamtrackers/NanoTrack --config <上述配置> --snapshot <上述权重> --video data/occ_video_1/1.mp4 --output /tmp/nt-dual-gt-evidence/<模型>-<目标>-<框>.mp4 --bbox-output /tmp/nt-dual-gt-evidence/<模型>-<目标>-<框>.csv --bbox <XYWH>`。car001 标准/人工/左上/右下 ROI 为 `992,582,37,39`/`993,585,36,36`/`990,580,39,41`/`994,584,35,37`；car002 标准/真值/左上/右下为 `970,517,38,38`/`967,513,42,42`/`968,515,40,40`/`972,519,36,36`。UAV123 跑两目标各四框；四数据集先筛 car001 标准/人工、car002 标准/真值四框。并行六组首试资源争用、均被中断且没有产生完整 CSV；后用单线程逐组重跑。12 组均完成 271 帧、exit 0；两组最初不限制线程的 `tracking_fps` 约 43，其余 10 组约 68～73。实际框及 24 个 CSV/MP4 文件的逐文件 SHA-256 见 `/tmp/nt-dual-gt-evidence/old-weights-desktop-outputs.sha256`，清单 SHA-256 `381afb6bc7755d5549ed1b86c234207c2eb37abed61d7ad3b4d75fda69840649`，`sha256sum -c` 全部通过。
- 执行 `python3 /tmp/nt-dual-gt-evidence/score-uav123-eight.py > /tmp/nt-dual-gt-evidence/score-uav123-eight.json` 与 `python3 /tmp/nt-dual-gt-evidence/score-fourds-four.py > /tmp/nt-dual-gt-evidence/score-fourds-four.json`；评分脚本 SHA-256 `17a1d9f5fa8457ef2906e6bec48a78d5e6693a25b3f85e3e18d6cc31c209f1fe`/`26b558da1c917bb07bdedffdb433f2c6f629240f0a07113c0510749dbd48be66`，报告 SHA-256 `3570112aa642086f3ff7cb76a6d8841fa61d7a53b34f791f19112e2a1abd550b`/`e190aa2110f76bcbafb096173a2d8b8456728480fc7ab7b7c6b5be869e6f4fe2`。只计正式可观测框：car001 `258` 帧、car002 `252` 帧。UAV123 car001 标准/人工/左上/右下全段中心均误差 `34.93/3.28/35.00/34.89 px`，标准/左上/右下在后段 `230～270` 平均误差约 `126.6 px`，跨初始框同帧最大成对中心距 P95 `141.37 px`；car002 四框中心均误差 `2.16/2.04/1.95/2.17 px`。四数据集 car001 标准/人工为 `5.16/34.83 px`，人工框后段均误差 `124.13 px`，两框跨框 P95 `141.16 px`；car002 标准/真值为 `2.24/3.24 px`。两套模型都在 car001 遮挡后发生与几像素初始化差别有关的轨迹分叉，且稳定的初始框方向相反。
- **裁决 / Handoff**：两套旧权重均拒绝作为当前双目标一致性候选，不进行 RKNN 转换或板端部署；桌面单跟踪器始终输出框，没有生产控制器、detector、无框状态或显示路径，因此这些数值不是板端正式门禁结果。保留 `9f84efe` 有效算法内容、板端八框 `2/8 PASS` 和 handoff HOLD。模型所有者若继续训练，应以仅作验证的 car001/car002 轻微初始框矩阵检验遮挡后身份分叉，并经 RK3588 实际双目标、两路中心与无框段闭环复测；不可根据单框高 IoU 晋级。本条提交后以实际 commit 为结果身份。

## 2026-11-14T05:51:03+08:00 前后向校验局部光流的八框筛查

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`；分析基线 `d1e9c6a9f2023ea6ba953369594d4b840a5d2556`，开始时工作树干净。主机 OpenCV `4.5.5`，仅用 canonical `data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`、car001/car002 已确认真值与下方 `9f84efe` 精确板端八框 CSV。执行 `g++ -O2 -std=c++17 /tmp/nt-dual-gt-evidence/klt-screen.cpp -o /tmp/nt-dual-gt-evidence/klt-screen $(pkg-config --cflags --libs opencv4)`，源码/二进制 SHA-256 `d832de2b74684ee5b798cbc78b18857e5d80e764a1e815d03ee2911c350cea53`/`25cc77d9223cc36e372803fad87f495c9be46c63ed17ffb50c81d615fabc76be`。算法在初始框内取最多 80 个角点，以 15×15、3 层 LK 光流跟踪，前后向误差阈值 1.5 px、光度误差阈值 25、至少 4 点，逐帧中位平移；没有使用真值调参或把检测器框投入控制。
- 用上条相同的两目标各四初始框完整回放 `271` 帧，输出 `/tmp/nt-dual-gt-evidence/klt-car001-{nominal,user,left,right}.csv` 与 `klt-car002-{nominal,truth,left,right}.csv`；八文件 SHA 清单 `/tmp/nt-dual-gt-evidence/klt-eight-csv.sha256` SHA-256 `59d9c71a94e1a21ea90c1fd2e689a2d11ba7c0e1d9c6f9aaa8a378729ce229b7`。执行 `python3 /tmp/nt-dual-gt-evidence/score-klt-screen.py > /tmp/nt-dual-gt-evidence/score-klt-eight.json`，脚本/报告 SHA-256 `b947ee6945e3815fce48ac36cccff805779a8ffff7ba11c5f4eefd69dae93d9c`/`b00845ce2da8bb1ff336f98b699aed488ceb2fc5f59a4b60d609a889ef090252`。光流有效的可观测帧 car001 每框 `169` 帧，标准/人工/左上/右下中心均误差 `2.29/1.35/3.34/1.83 px`，对应 NanoTrack 同段约 `3.36/3.46/3.13/3.68 px`；car002 每框 `211` 帧，光流 `6.94/9.18/7.38/5.80 px`，NanoTrack 同段约 `1.62/1.39/1.44/1.99 px`。car001 第 174 帧、car002 第 215/216 帧起光流失效；car002 已确认无框的第 211～212 帧仍 `2/15` 错报有效。光流对 car001 遮挡前定位有局部收益，对 car002 明显退化。
- 反事实仅当 NanoTrack `TRACKING`、accepted、control_valid 且光流中心距 ≤5 或 ≤10 px 时两中心各取一半；执行 `python3 /tmp/nt-dual-gt-evidence/score-klt-consensus.py > /tmp/nt-dual-gt-evidence/score-klt-consensus.json`，脚本/报告 SHA-256 `edca97f07b60b63afdd5bff30dec20c3baadf9d35033d7660c48cdaae956a9a2`/`a83af89bfbe667693b93e42e6e358de03749cade823206884f228ce09fb5f742`。5 px 门下 car001 标准/人工/右下全段中心均误差从 `4.28/4.63/5.02` 降到 `3.84/4.06/4.55 px`，左上 `4.34→4.33` 几乎无收益；car002 标准/真值/左上/右下从 `2.59/2.00/1.77/2.92` 变为 `2.44/2.10/1.70/2.85 px`，真值框退化。10 px 门使 car002 四框均比原 NanoTrack 更差。5 px 门下 car001 跨框最大中心距 P95 `3.31→3.51 px`，car002 `6.21→6.21 px`，均未达到更紧且更一致的双目标要求。以上只是冻结轨迹输出反事实，未计入反馈和 RK3588 时延。
- **裁决 / Handoff**：不把局部光流接入跟踪输出或控制，也不以它的有效点数替代遮挡真值；两目标对同一光流规则反应相反。保留源代码和报告在主机 `/tmp/nt-dual-gt-evidence/` 作可复核负例，正式算法文件无改动，八框 `2/8 PASS` 与 handoff HOLD 不变。下步需模型/模板级定位与身份候选，必须同时检验两目标中心、初始框一致性、无框段和板端闭环。本条提交后以实际 commit 为结果身份。

## 2026-11-14T05:46:58+08:00 独立 CSRT 中心观测与跨初始框一致性筛查

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`；分析基线 `d9b7d1a3b26a08878d0c32a4186571dd9c04255b`，工作树开始时干净。只用 canonical `/home/u20/single-object-tracking/data/occ_video_1/1.mp4`（SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`）、两目标已确认 GT 和 `9f84efe` 八组精确 RK3588 CSV 作主机离线比较；没有修改板端或正式运行算法。OpenCV `4.5.5`，执行 `g++ -O2 -std=c++17 /tmp/nt-dual-gt-evidence/csrt-screen.cpp -o /tmp/nt-dual-gt-evidence/csrt-screen $(pkg-config --cflags --libs opencv4)`，源码/二进制 SHA-256 `13258d009d94496c416271a713df644a20a8397bb826dbf0416ee8bd7a6c0b67`/`a7958f8eaaafa33d9d0b99b2dba059927c40e7d2701a7d2e81c87703b3a9aebb`。用 car001 标准/人工/左上/右下 `992,582,37,39`/`993,585,36,36`/`990,580,39,41`/`994,584,35,37`，car002 标准/真值/左上/右下 `970,517,38,38`/`967,513,42,42`/`968,515,40,40`/`972,519,36,36` 运行八次，每次完整 `271` 帧。输出 `/tmp/nt-dual-gt-evidence/csrt-<label>.csv`；16 个原始/重置 CSV 的逐文件 SHA 清单 `/tmp/nt-dual-gt-evidence/csrt-eight-csv.sha256` SHA-256 `c05bbe9f650f7ca9823671af0449539bbcf45331b6d11328e3a1cbc0d0aff2a5`。
- 执行 `python3 /tmp/nt-dual-gt-evidence/score-csrt-screen.py > /tmp/nt-dual-gt-evidence/score-csrt-eight.json`，脚本/报告 SHA-256 `ac9ab67b5d49370233b0f5be5a8da7cd970c45f91808ee5ae54c3a0853da4e97`/`2ad1b2eeb5ea79175e640d0b77ef7eeb164d1b69999798eb78de9311bad6d929`。GT 可观测帧 CSRT 中心均误差 car001 四框标准/人工/左上/右下 `36.82/61.08/90.84/2.77 px`，对应 NanoTrack `4.28/4.63/4.34/5.02 px`；car002 四框标准/真值/左上/右下 CSRT `3.07/2.59/2.22/3.85 px`，NanoTrack `2.59/2.00/1.77/2.92 px`。CSRT 对 car001 三框遮挡后严重漂移，且对 car002 已确认的 15 个无框帧仍 `15/15` 返回成功；其成功标志不能用于目标可观测性或控制授权。
- 反事实只在 NanoTrack `TRACKING`、accepted、control_valid 且 CSRT 中心距不超过 `10 px` 时把两中心各取一半，其余保持 NanoTrack 原框；执行 `python3 /tmp/nt-dual-gt-evidence/score-csrt-consensus.py > /tmp/nt-dual-gt-evidence/score-csrt-consensus.json`，脚本/报告 SHA-256 `8cd8c68028753b22074e03ba2e2ecfa278a8c9eb604cf8abe1990df0422ca359`/`c3dcd753ecd53ab90f36559f2510960dc22b54dfdaa719734543e4bf2e9e9716`。各框可见帧平均中心误差虽都小幅下降，car001 四框最大两两中心距 P95 却从 `3.31→6.77 px`，car002 从 `6.21→6.28 px`；用户要求的跨微扰一致性变差，且此离线冻结轨迹没有计入反馈、板端时延或闭环副作用。
- 再以 `9f84efe` 已确认重捕帧的 NanoTrack 框重置 CSRT，原型 `/tmp/nt-dual-gt-evidence/csrt-reinit-screen.cpp`/二进制 SHA-256 `5a52668d425bf56323e2ac844af5c1f78238a6a5a908e63a54d219590cba6fca`/`a532f0eb6712dd03f35995ff59abeb86d2a6301b2e0999ce71a724bb747aadb2`，八组完整 271 帧；评分脚本/报告 `/tmp/nt-dual-gt-evidence/score-csrt-screen-reinit.py`/`score-csrt-eight-reinit.json` SHA-256 `a6e520abcfc4ecc61abb54a80d48c284c536c5ccde783d1dec124d4cef157141`/`e8bf58a31deefc3afb43f97d4e66e7e7cdf4179f1883b6062643b5399795f0de`。car001 标准框 CSRT 后段均误差从不重置 `141.88→4.56 px`，但 car002 标准/真值框从 `7.55/2.63→12.66/10.53 px`；同一重置策略仍无法同时改善两目标。重置版 10 px 一致性融合报告 `/tmp/nt-dual-gt-evidence/score-csrt-consensus-reinit.json` SHA-256 `df7a95e2b139b14bb6a8efe0a580b0ee017b9ef7106b2410675919e998e4038f`，car002 四框平均中心均高于原 NanoTrack。
- **裁决 / Handoff**：不接入 CSRT 或它的中心融合，也不把其 `ok` 当作无框/重捕证据；虽然部分单框中心更准，但错车漂移、15 个无框误报和跨初始框一致性退化直接违背本目标。当前 `9f84efe` 算法内容、八组 `2/8 PASS` 和正式 handoff HOLD 不变；继续寻找同时改善定位和身份、可经板端闭环与两目标无框帧验证的候选。本条提交后以实际 commit 为结果身份。

## 2026-11-14T05:39:16+08:00 display detector 候选中心与双目标真值对照

- 精确源码 `codex/dual-gt-init-consistency@d157d0d66a7346dd5460b8a568eea2c0e973cc58`，基线 `766f1368ee4a21df0aeb039c6b47d5437080b537`，隔离 RK3588 根 `/home/neardi/Project/SOT/dual-gt-display-trace-d157d0d`。`git archive` 源码包 `/tmp/nt-dual-gt-evidence/display-trace-d157d0d-source.tar` 与板端 `source.tar` SHA-256 均 `b106bc40342b46707318f230a8278b8480e70df6f135b760ec18df27c04306d7`。板端执行 `cmake -S rk3566/nanotrack_v3 -B build -DENABLE_RKNN_RUNTIME=ON -DRKNN_INCLUDE_DIR=/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/include -DRKNN_LIBRARY=/usr/lib/librknnrt.so`、`cmake --build build --parallel 4`、`ctest --test-dir build --output-on-failure`，`23/23 PASS`；二进制 SHA-256 `ed4f8ad097331988e22ac49dff5fe305ee20163ca2cc425a43b758119bb58e57`，板端配置 `runs/headless-eval.conf`/控制器配置 SHA-256 `c7594ebb9c43789b136e13182fdfd96f634af154f411142f0866b954762cad98`/`5d3214646bd3b0e6d38213482178aa44a0deb301d6f303c8b5eaa7f5517d3a3b`，构建/CTest 日志位于 `runs/`。
- 视频板端实际路径 `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/data/occ_video_1/1.mp4`，重新核得 SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；官方 NanoTrack template/search/head RKNN SHA-256 `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55`/`d3cb0e3879df6a1272adb59bf501324f1f8f55b58309d3c4da4a731b600973ee`/`4da379f54093d79f02775e90bfea33ebee0837c6fc73a324499279b4c57807c5`；VisDrone9 RK3588 detector/manifest/labels SHA-256 `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c`/`ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9`/`41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`。板端 Linux `6.1.84`，运行 session 实报 RKNN API `2.1.0`、driver `0.9.8`；主机匹配 GT car001/car002 状态清单 SHA-256 `e44fd23514d6871e72d628ad4843fc90bb4246f306322ee297c8a2aa75818036`/`212525642591c124320e93369f8748d82cc53092628beff051507f930946dec1`。
- 串行运行 `NT_TRACE_DISPLAY_CANDIDATE=1 ./build/nanotrack_v3 --config runs/headless-eval.conf --bbox 992,582,37,39` 与 `--bbox 970,517,38,38`，各自指定 `--output-video/--output-boxes/--session-metadata` 为 `runs/car001_nominal/`、`runs/car002_nominal/`，完整 CSV/JSONL/session/log/AVI 留在板端。CSV SHA-256 `64b33b875a2435f5d8d755c16379336e865207006ed67f47a9796c98fb736a60`/`452ea941b451990b93810c3a9d885c4da291a532b50c63fd6a2dd416248de246`，AVI SHA-256 `2a28d7e23f03d18c3cbcc0dfb464095232d663ee8d3ad6cb77d1b0f4c427ee2a`/`21320c9508abcd54e1b8e28597fa2489dc20a86450bc9b4f97329b0aef0b9138`，`ffprobe` 两段均 1920×1080×271。文本包 `runs/two-text.tar.gz` 板端/主机 SHA-256 `20c96eb00c7f6d0302b1c68ccf1a0c5c8e5531cad6274e8ec5c6a0a1d12f68ec`，主机副本 `/tmp/nt-dual-gt-evidence/display-trace-d157d0d-two-text.tar.gz`。
- 主机执行 `python3 /tmp/nt-dual-gt-evidence/score-display-detector-trace.py > /tmp/nt-dual-gt-evidence/score-display-detector-trace.json`，脚本/报告 SHA-256 `5c0f5cfb693c4f13e69d9c4c637937f49f38dcb5e8af39aa5ae8bd4291e7c2f1`/`4a20ccec3c179a55f80b3385ba8fca10a0be331aef0ba076d3825ad9764d6762`。两路各有 76 个已匹配显示 detector 候选；逐帧比较与 `9f84efe` 基线 271 个 tracking/display 框及 state、phase、reacquired、control_valid 字段差异全为 `0`，说明此次只读诊断未改变轨迹。car001 后段 ≥240 帧 10 个候选的 tracker/detector 中心真值均误差 `7.33/9.30 px`，detector 优胜 `0/10`；car002 同范围 `7.74/2.51 px`，detector 优胜 `10/10`。前段 <160 帧 car001 `3.23/4.66 px`、detector 优胜 `12/48`；car002 `1.49/4.45 px`、优胜 `5/48`。detector 原框经对称 padding 后的中心在该区域保持不变；诊断日志比较的是结果帧真值，候选源帧通常早 1 帧（33 ms），因此包含正常一帧运动差。
- 使用正式评分 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run car_001 car001_nominal 992,582,37,39 <CSV> --run car_002 car002_nominal 970,517,38,38 <CSV> --output-json /tmp/nt-dual-gt-evidence/display-trace-d157d0d-two-center.json`，报告 SHA-256 `db8eada1f9e4d300527e624131cdc01f03983606233fb3423e1d508263ce1acc`：car001 标准框 PASS，重捕 188，tracking/display 中心均误差 `4.28/4.32 px`；car002 标准框 FAIL（完全遮挡语义），重捕 235，两中心均 `2.59 px`。这与原八框代表组完全一致。
- **裁决 / Handoff**：detector 的显示候选中心对两目标呈相反效果，早段两目标多数帧也更差；不可把它作为 tracking/control 的通用中心修正，display detector 仍仅作外观用途。撤下临时诊断打印后执行 `git diff --name-only 9f84efe -- rk3566/nanotrack_v3 scripts`，结果为空；主机重构建及 CTest `23/23 PASS`，日志 `/tmp/nt-dual-gt-evidence/display-detector-trace-revert-host-build.log`/`display-detector-trace-revert-host-ctest.log` SHA-256 `5d8e364186ce39a995aa414cd968aa71fbd73e439471f8e38aad11bb2265bada`/`da80783f68993bf5a98ba0dba26105f2aa54308a43c829f463dd4147b3a44014`。保留精确板端日志/视频和本反例。正式八框 `2/8 PASS` 与 handoff HOLD 不变。后续须针对 NanoTrack 自身定位/模板或经冻结的模型找改善方案，本记录与撤除代码同提交。

## 2026-11-14T05:32:57+08:00 只读 display detector 候选中心诊断源码

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `766f1368ee4a21df0aeb039c6b47d5437080b537`。在 `AdaptiveDisplayBox::ingest` 内仅当显式环境变量 `NT_TRACE_DISPLAY_CANDIDATE=1` 时，把已通过显示候选选择的 detector 框与同帧 tracker 框、源帧及年龄打印到 stderr；未匹配时打印 `none`。代码文件 SHA-256 `aebd3e3debb62c478b41d04eab988b31991248b748f3b70f4ae23f537b938de5`。打印的候选是 padding 后的显示外观框，中心在无图像边缘裁剪时与原 detector 框相同。该诊断不向 tracking/control 写 detector 结果，默认环境不输出。
- 主机执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4` 和 `ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，RKNN-off `23/23 PASS`；构建/测试日志位于 `/tmp/nt-dual-gt-evidence/display-detector-trace-host-build.log`、`/tmp/nt-dual-gt-evidence/display-detector-trace-host-ctest.log`，SHA-256 `67e804ec196d1680ec9412db438c9804d30124c63c2236570f84ebb1126e2fbc`/`047767e41202e4bac7cb0efc89393894c746d15742e3739aac92d7b9135a5dc9`。
- **证据边界 / Handoff**：这是主机可编译的只读诊断，没有板端候选中心或真值结论。下一步须提交精确源码、RK3588 原生构建，以 canonical 输入分别跑 car001/car002 代表初始框，核验 `tracking_bbox`/`display_bbox` 轨迹与 `9f84efe` 不变，再对 trace 候选框和两目标可观测真值计算中心误差与错身份风险；仅在有跨框改善空间且符合 NanoTrack 确认门时才设计定位改动。正式 handoff HOLD，本条与诊断源码同提交。

## 2026-11-14T05:31:06+08:00 完整车体图像分割中心的两帧筛查

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`；筛查基线 `ee450c14e020577b5ba97aee3003722a045c6c1c`。输入沿用下方已核对身份的 canonical 视频/GT 和 `9f84efe` 八框板端 CSV；主机 OpenCV `4.5.5`。执行 `g++ -std=c++17 /tmp/nt-dual-gt-evidence/grabcut-probe.cpp -o /tmp/nt-dual-gt-evidence/grabcut-probe $(pkg-config --cflags --libs opencv4)`，原型源码/二进制 SHA-256 `222596af594cba6f3165af691741667bef34489392e46b72182550dfca8018e2`/`8b1aec3da9c2c5ebaed668595600ac15edbdeef2055d6fbd8ee02246bb32063d`；再运行 `python3 /tmp/nt-dual-gt-evidence/grabcut-screen.py > /tmp/nt-dual-gt-evidence/grabcut-screen.json`，脚本/报告 SHA-256 `d50222bfdc38a3c2b355821ba8041c9df7504153e7b6441f569a73dfc205fbe8`/`0fe73a215a4d2b09edddf61f19d59f1c4ad3f2c61357d0b76ae57160e9e954bd`。
- 仅筛后段第 245、260 帧，两目标各四初始框。GrabCut 使用 tracking 框外扩 7 px 作前景初始矩形，选取邻近连通域的包围框，不使用真值调参。car001 第 260 帧四框原中心误差 `5.57/7.96/5.35/5.27 px`（标准/人工/左上/右下），分割后均 `10.40 px`；第 245 帧分割后均 `6.96 px`，只在标准/右下略好。car002 第 260 帧四框原 `10.61/5.90/5.66/8.44 px`，分割后 `2.06/2.50/3.20/2.06 px`；第 245 帧也均改善。分割轮廓的几何中心与人工完整目标框中心并非总一致，同一规则在 car001 上退化。
- **证据边界 / Handoff**：这只是主机两帧离线筛查，不是 271 帧、遮挡期、RK3588 时延或真实闭环的证明；原型未进入代码分支。不能因为 car002 局部收益把分割结果用于 `tracking_bbox` 或 `display_bbox` 的中心。正式八框结果仍为 `2/8 PASS`、handoff HOLD。下一步需要模型/模板级、同时经两目标八框可见和遮挡段验证的定位方案。本记录提交后以实际 commit 为结果身份。

## 2026-11-14T05:28:32+08:00 初始裁剪聚合与真实模板更新时序核对

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`；分析基线 `ea3ecc3c99ebde6fdd69202993823b715daa52be`，输入仍为下方哈希绑定的 `occ_video_1` 双目标、`9f84efe` 八框板端 CSV 与同源视频解码图像。主机运行 `python3 /tmp/nt-dual-gt-evidence/init-jitter-aggregate.py > /tmp/nt-dual-gt-evidence/init-jitter-aggregate.json`；脚本/结果 SHA-256 `90bd1f09026986310634b311e1f359afe1228e6d54ce52e344a24df301da6fae`/`8ccda2126dc8faf9078867ecb8105710b7ef94ebef782f5e3744f815d17af2c1`。再运行相同脚本的 ±1 px 版本 `/tmp/nt-dual-gt-evidence/init-jitter-radius1.py`，脚本/结果 SHA-256 `c4e44b77d5ec07ebe799e8f1030c2b8cbe47dd59918cc3cdb92427ebb3eb1b98`/`ee41bffe4cad00c0585088a8196eb11d9f64632c4a40a2e9ae10364af2d5fe5a`。两种离线反事实都把第 0 帧实际 ROI 的 3×3 平移图块特征平均，只用第 240～270 帧两车真值图块事后核对相似度。
- RGB 正 margin 帧数在 ±3 px 聚合后，car001 标准/人工/左上/右下从 `13/30/13/29` 变为 `28/30/25/30`，但 car002 标准/真值/左上/右下从 `26/28/26/15` 变为 `8/23/17/3`；±1 px 聚合时 car001 `17/30/18/30`、car002 `23/28/24/11`。没有一个统一小扰动平均方案在两目标八框都改善，这只是像素级代理，不等于 RKNN 特征平均的真实效果，故没有新增初始化推理开销或修改生产跟踪器。
- 主机执行 `python3 /tmp/nt-dual-gt-evidence/template-timeline.py > /tmp/nt-dual-gt-evidence/template-timeline.json`，脚本/报告 SHA-256 `4a760bba9646cb5869782801f7f4ea3c7bfe994ff36e6e8a4bac0da11966e9c1`/`a3d545302128cc7c76a9d359fa9deebefe4700528c135121bac6412611fb33a2`。八框均在第 30/33、63/66、约 96～100、约 129～133 帧阶段/提交 recent 模板，此后还有恢复及稳定段提交；car001 恢复后的提交在 191/194 帧，后段又在 224/227、257/260 等帧阶段/提交。第 240～253 与 258～270 帧 car001 标准框中心均误差 `7.19→7.88 px`、car002 标准框 `6.99→8.74 px`，其他框的变化方向相同。后段误差不能仅归因于初始化时的单张像素模板；实际 recent 模板已多次更新且偏差没有随近期提交消失。
- **裁决 / Handoff**：维持当前算法、模型、400 ms 配置与 `2/8` 正式八框结论；不把初始化裁剪聚合或静态像素相似度升级为板端候选。后续模型/模板研究必须覆盖恢复后 recent 特征、同类负样本和追踪框中心；display detector 框仍只用于视觉外观。正式 handoff HOLD，本条提交后以实际 commit 为结果身份。

## 2026-11-14T05:25:33+08:00 八个初始模板的外观可分性诊断

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`；分析基线 `2a9a8d656ad638dd476086641cf05780c3274c32`。继续使用下方经哈希核验的 `occ_video_1/1.mp4`、两目标真值与精确板端 `9f84efe` 八框 CSV。主机执行 `python3 /tmp/nt-dual-gt-evidence/initial-appearance-separation.py > /tmp/nt-dual-gt-evidence/initial-appearance-separation.json`；脚本 SHA-256 `72c4f14f68cf84c2ed39661c9c6edf6deff3b522715b6bc438e887778fa1b2fe`，报告 SHA-256 `c1dff1276dccfa2892fe929c1544e76ceac7e8ee11cf310efa37b17f6905e41c`。脚本取八组第 0 帧实际初始化框图块，缩放至 32×32，分别与第 240～270 帧两目标的真值图块计算归一化 RGB/灰度/梯度相关系数；“正 margin”仅表示同目标相似度大于另一辆已确认轿车。
- RGB 相关的正 margin 帧数：car001 标准/人工/左上/右下 `13/30/13/29`（每组 31 帧），平均 margin `−0.025/+0.146/−0.003/+0.134`；car002 标准/真值/左上/右下 `26/28/26/15`，平均 `+0.068/+0.080/+0.056/+0.002`。梯度相关在 car001 四框仅 `0/6/4/3` 帧正 margin，但 car002 四框为 `31/28/24/31`。几像素的初始化变化足以改变这项朴素外观量对两辆白车的排序；一种固定像素相关规则不能同时稳定处理两目标八框。
- **证据边界 / Handoff**：这是仅含两辆指定轿车、使用后段真值图块的离线可分性上界，不包含 detector 全部候选、遮挡期虚假观测，也不是 RKNN tracker 的真实特征或板端推理结果；不能把相关系数当作身份验证器直接部署。结合下方原始 NanoTrack 中心偏差，请协调者将八框实际初始化 ROI、CSV 和本诊断交模型/外观评估 owner，用跨框和负样本门槛检验模板鲁棒性。当前代码与正式板端 handoff 均保持 HOLD；下一步需提出有运行时输入可用、且可经八框及无框帧验证的模板或外观候选。本记录提交后以实际 commit 为结果身份。

## 2026-11-14T05:23:11+08:00 原图边界梯度中心修正的离线否决

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`；分析基线 `cd61638cc7474f792be8854377e651f0c03c5e08`，有效板端算法/八框轨迹仍为 `9f84efecd57d611a239cc71c99ab2e4295025dbc`。输入 `data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；八框 CSV 位于 `/tmp/nt-dual-gt-evidence/handoff-9f84efe/<label>/frames.csv`，其中 car001/car002 标准框 SHA-256 分别 `7bcd85d31b9e5840eb4bb56e2d9d7e82eb4b143f9e056c9056fa05e9e9aa3be4`、`23a6e3876ffdc34a863dc5896e2262b851cf7dc84e5d024a7076086c7f82abf4`；其余六框及板端输入、模型、二进制身份见下方 `9f84efe` 八框记录。
- 主机执行 `ffmpeg -v error -i data/occ_video_1/1.mp4 -q:v 2 -start_number 0 /tmp/nt-dual-gt-evidence/rawframes/%03d.jpg -y`，再执行 `python3 /tmp/nt-dual-gt-evidence/edge-center-offline.py > /tmp/nt-dual-gt-evidence/edge-center-offline.json`；脚本 SHA-256 `e948d02680dd34397159cd078956a16ae110b50ecfee30683d88786570c08c25`，报告 SHA-256 `6bc1f0dea1e6a71006078457e8af5ff1bfebbf3902efa8a6e5a7148263e295d6`。脚本在第 240～270 帧沿 tracker 框四边寻找 ±12 px 内图像梯度最强的位置，不使用真值调参；真值只用于事后计算误差。原图目视确认 car001/car002 均为白车但车身明暗分布不同，因此简单亮度质心也没有可靠的共同偏移方向。
- 后段 31 帧每框结果：car001 标准/人工/左上/右下原 tracking 中心均误差 `7.39/8.74/7.18/7.37 px`，梯度修正反事实 `16.31/12.57/17.29/12.85 px`，四框各 `0/31` 帧改善。car002 标准/真值/左上/右下原均误差 `7.85/4.66/3.16/6.65 px`，修正后 `5.07/16.27/8.72/4.27 px`；虽有两框均值下降，另外两框显著退化，且四框的 P95 均差于原轨迹。该修正没有进入 C++、板端或正式八框评分。
- **裁决 / Handoff**：同一图像边缘规则无法稳定消除不同目标/初始框的 NanoTrack 原始中心偏移；不得把 car002 两框的局部收益当成两路达标。继续保持 `9f84efe` 算法内容与 400 ms 配置，正式 handoff HOLD。下一步应研究可跨框验证的外观/模板定位或经协调者冻结的新模型，而非把 display detector 外观框用于 tracking/control 校正。本记录提交后以实际 commit 为结果身份。

## 2026-11-14T05:18:31+08:00 稳定可见后段中心误差归因与模型侧 Handoff

- 在精确板端运行 `9f84efecd57d611a239cc71c99ab2e4295025dbc` 的八个 CSV 上，主机执行 `python3 /tmp/nt-dual-gt-evidence/analyze-late-center-sources.py > /tmp/nt-dual-gt-evidence/late-center-sources.json`；脚本 SHA-256 `80de59ddcdad3c0a2700e23c417e7202642da12f0afb5e7a7796a006039b42cf`，报告 SHA-256 `cd411b68e52eb7a5ec2cbbad7128f27c96e4dff823164d3e24f30b0c1ac878e7`。脚本通过 `scripts/evaluate_occ_video_1.py` 严格加载两目标匹配 GT，取第 240～270 帧的 31 个可观测框，逐帧比较 tracking、display、NanoTrack recent 原始中心与真值。
- 八框 `tracking_bbox` 中心均与当帧 NanoTrack recent 原始中心逐帧重合（每组 `31/31`，误差小于 0.001 px）；display 与 tracking 中心最大差约 0.0001 px。car001 标准/人工/左上/右下后段 tracking 中心距真值均值 `7.39/8.74/7.18/7.37 px`，有方向一致的左上偏移：平均 dx `−4.37/−5.93/−3.90/−4.34 px`、dy `−5.88/−6.37/−5.97/−5.88 px`。car002 四框后段 dx/dy 方向和量值不相同（例如标准 `−5.01,+6.00 px`、真值框 `−3.97,+2.13 px`），不能对所有目标加一组固定中心偏移。此区段的误差已直接来自 tracker 原始输出，不能靠调整 display detector 视觉框或当前控制器平滑解决。
- **证据边界 / Handoff**：上述是同一视频的八框板端回放分析，尚未证明哪种新 tracker 权重、外观验证器或模型侧改动可改善它，也不能单凭本数据训练后再用它宣称泛化。请协调对话将“car001 后段原始 NanoTrack 中心持续偏左上、car002 方向依 ROI 不同”的精确 CSV 和报告交给 NanoTrack 微调/模型评估 owner；若提供新冻结模型，须按契约先核对 checkpoint、ONNX、RKNN、配置和哈希，再由板端任务用两目标八框重跑。当前算法分支与板端正式 handoff 均保持 HOLD。

## 2026-11-14T05:15:50+08:00 car002 恢复前中心分叉与最短等待时间反事实

- 沿用已完成八框验证的精确源码 `codex/dual-gt-init-consistency@9f84efecd57d611a239cc71c99ab2e4295025dbc`、RK3588 隔离根 `/home/neardi/Project/SOT/dual-gt-handoff-9f84efe`、二进制 SHA-256 `41d83ef0d772833b29400a35cacb8825a1a402f24ea7dbfe9279ac09b062343b`、视频/GT/模型/驱动身份，仅修改板端隔离配置，不改仓库 400 ms 正式配置。原八框 CSV 对真值逐帧核对：car002 右下框第 230～232 帧 motion 中心距可观测真值 `22.7/23.5/24.9 px`，recent/safe 原始中心约 `5.6/5.4`、`4.7/4.4`、`5.0/3.9 px`；但原配置第 232 帧才允许首次可信候选命中，第 233 帧两击重捕。四框进入 SUSPECTED 的第 211 帧速度、加速度不同，至第 232 帧开环预测中心最大分叉约 `34.8 px`；用第 211 帧中心和速度在 Python 中离线作零加速度外推，反而使标准框第 232 帧预测中心误差约 `6.4→18.8 px`、真值框约 `10.3→12.9 px`，不能据此改动运动模型。
- 板端 `search_reacquisition_min_age_ms=400→250`：`runs/minage250-controller.conf` SHA-256 `13a2dc71f345a2d71a70ae1c1e1529924f91ec83294712c5b4701e945eda877a`，`runs/headless-minage250.conf` SHA-256 `70264764d17e9088a7657f86d684e9dde0da9419870c7718deef8efbb1ac7eee`；实际命令 `./build/nanotrack_v3 --config runs/headless-minage250.conf --bbox 972,519,36,36 --output-video runs/minage250/car002_right/output.avi --output-boxes ... --session-metadata ...`。AVI 1920×1080×271，CSV/AVI SHA-256 `929248ad347fee27e5559c68fa50d60f3c20edc16efc8546c583f09c2d7adf4a`/`8514c40debf45be15d84b4176a860e90f1f84d976730322285d9120191045f12`，文本包 `runs/minage250-car002-right-text.tar.gz` 主机/板端 SHA-256 `ffc23e24211f52e1b68fdd0d8b3cc35ae65fbeb8417892c39fc274845e286681`。评分 `/tmp/nt-dual-gt-evidence/minage250-car002-right.json` SHA-256 `a9e6b0b784a88f5fc3c351b9616b14e49019858e747c06cd67abc67f14a2d8f2`：第 228/229 帧两击并在第 229 帧错误重捕，此帧属人工确认无框；第 249 帧再次重捕，可见 tracking/display 中心均值 `3.96/3.99 px`，正式 FAIL，否决。
- 板端 `search_reacquisition_min_age_ms=400→300`：控制器/headless 配置 SHA-256 `84a0893fb60ca0b15ffb98700c420fa6f789a854d896872ab7bf3604f3d4ecfb`/`9737eeed8f80120ccc9a3fbc55242db8f05879ba66c1d8217e99af40a75b9ca8`，同一命令改用 `runs/headless-minage300.conf`、`runs/minage300/car002_right/`；AVI 1920×1080×271，CSV/AVI SHA-256 `c0dd3a257fdfcc4762908a7343f3033e465795741ca77b2778ba72494322be55`/`2bd2366dd465c8257fa94cf444d67939ecac3c0133187bcc28995fc29b652b6a`，文本包 `runs/minage300-car002-right-text.tar.gz` 主机/板端 SHA-256 `a49f6c4984bc30de392f979e462961db6e16b3eb78b106420294a7b369b22bc5`。评分 `/tmp/nt-dual-gt-evidence/minage300-car002-right.json` SHA-256 `c1790b90465e987fa05f97b45f0a16d76813f622955d3b327e52b9ec0c488de3`：第 229～231 帧先产生身份冲突，虽第 232～233 帧同一 tracker 候选两击，因冲突后同候选 detector 门禁未重捕，整段无重捕；可见 tracking/display 中心均值 `11.85/10.97 px`，正式 FAIL，否决。
- **裁决 / Handoff**：最短等待是遮挡边界的身份安全门，而非单纯让 tracker 的较好原始框更早输出；250 ms 导致无框帧误重捕，300 ms 使冲突记忆阻止后续重捕，均比原 400 ms 退化。正式源码和配置不变，当前有效八框矩阵仍为 `9f84efe`，handoff HOLD。下一步若处理重现前两路中心，应区分“未经确认的候选框”与可用于控制的可信框，并以 car001 遮挡期相似车辆和 car002 无框帧共同约束，不能把原始响应直接当作真值。

## 2026-11-14T05:10:27+08:00 远距探针门禁的板端筛查否决与撤回

- 精确源码 `codex/dual-gt-init-consistency@4a4032c781428f1f3536c72c656a5966c667d3f8`，源码包 `/home/neardi/Project/SOT/nt-dual-gt-probe-gate-4a4032c.tar` 主机/板端 SHA-256 `bd2411a339a1512e5f89ebe99dc41f796f2c1d10ac11ccbe8c06d20b16a30a15`，隔离根 `/home/neardi/Project/SOT/dual-gt-probe-gate-4a4032c`；RK3588 原生 CTest `23/23 PASS`，`build/nanotrack_v3` SHA-256 `f531e37312f5bbf9a2ea9544ecbda618b6bad9b5ac23b20e4f9aa65cd9720235`。控制器配置 SHA-256 `d0e4f9ad7c46ee2d6e00a30653257c95ee4ab263d51d9863dbb27958a03759d2`，400 ms headless 配置 SHA-256 `910558dc3929fed7a34deb3430509726ad0ac4ec80f0bc42d869248a00476a03`；视频、GT、模型和板端运行版本同上一八框矩阵。
- 板端先把 `detector_interval_ms=400→330`，`runs/detector330-controller.conf` SHA-256 `c46d540ce82b2bdd612582e456f6190494d431c095118bfa92b6f431596df5b7`，headless SHA-256 `d90b49bc98702b4cd45608c9104314a35d1528b7a686d639cf443019934c1be9`。实际运行 car001 标准框 `./build/nanotrack_v3 --config runs/headless-detector330.conf --bbox 992,582,37,39 --output-video runs/detector330/car001_nominal/output.avi --output-boxes ... --session-metadata ...`，CSV/AVI SHA-256 `6af9e30c8ef94b3f342b1b5892b8c746297886f9177b56dd83e0e3fc04625ecb`/`b2c5b22a09cdfb2fedb5a63669810bfadff9a691600cb50faf0d1e17cd7ee60e`，文本包 `runs/detector330-car001-nominal-text.tar.gz` 主机/板端 SHA-256 `a1bad1bc329cfdaa7cf90c6c784ec02508501188f4af3d1132f2c8c2ef11346a`，评分 `/tmp/nt-dual-gt-evidence/probe-gate-4a4032c-detector330-nominal.json` SHA-256 `11e8547ed1f9473be1da7decd21be51ed4a6226be47a795ad4b2ff1bfca03771`。第 179～180 帧错误远距探针被排除，候选未错车重捕；但较早 tracker 原始中心仍距真值约 `12～45 px`，正确贴近真值的两击直到第 192～193 帧，重捕 `193`，tracking/display 中心均值 `4.43/4.47 px`，正式 FAIL。
- 400 ms 正式配置在板端执行 car001 标准/左上/右下三框，AVI 全为 1920×1080×271；标准 CSV SHA-256 `f52e5f9c8293eaee98259ff37283eb1b8f9f969097d827d71717bab93f49366e`，左上/右下 CSV SHA-256 `896dce6c54255ab101d6d1629baca9ac09eed22210ff7d947755897ef4e8f071`/`cf323abff779944f0c344636882752dd63730a52dc65bba44c2ae6dca953a605`，文本包 `runs/standard400-car001-nominal-text.tar.gz`、`runs/standard400-car001-extra-text.tar.gz` 主机/板端 SHA-256 `9a91db2faf91bdbfe6d551a0f648dca08c7e6a046776d94053f10129f4e66e63`、`a515a41ecfd5b5984007bf4823641ea909764c31e43948d219768bdf62cf9af1`。评分 `/tmp/nt-dual-gt-evidence/probe-gate-4a4032c-standard400-car001-three.json` SHA-256 `e31102a8fe3aac627d1c4200310bcebf728e6d6208617eb1533a8df444e65a17`：重捕 `188/191/191`，tracking/display 中心均值与 `9f84efe` 基线逐项一致，标准 PASS、左上/右下仍 FAIL。该门禁对正式参数无观测收益。
- **裁决 / Handoff**：撤回 `4a4032c` 的探针/确认距离门禁、配置和专属单测，恢复 `9f84efe` 算法内容及 400 ms 配置；恢复后主机执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，RKNN-off `23/23 PASS`，日志 `/tmp/nt-dual-gt-evidence/probe-proximity-revert-host-build.log`、`/tmp/nt-dual-gt-evidence/probe-proximity-revert-host-ctest.log`。保留精确源码、板端输出和失败记录。330 ms 错车虽被抑制，但真值重捕时机没有满足目标，不能用它替代八框基线。正式 handoff HOLD。

## 2026-11-14T05:04:21+08:00 保留远距竞争身份并阻止其牵引搜索探针的候选

- 基于 `codex/dual-gt-init-consistency@c6af7ff6bafda5ba075c37501f8526e8382964fa`，工作树 `/home/u20/single-object-tracking`，在 `RecoveryCandidateTable` 新增距可信运动中心的探针选择：detector 起源候选若超出 `detector_candidate_max_center_ratio`，仍保留在身份候选表并参加冲突判定，但不让它决定 NanoTrack 下一帧裁剪；确认阶段亦不得提交该远距 detector 身份。运动预测不可信时保持原选择。默认关闭的 `recovery_confident_detector_proximity_gate` 仅在当前可见配置启用，配置 SHA-256 `d0e4f9ad7c46ee2d6e00a30653257c95ee4ab263d51d9863dbb27958a03759d2`；正式 detector 间隔仍 400 ms，display detector 的视觉修饰不参与 tracking/control 授权。
- 动机来自上一 330 ms 对照：错车 detector 候选距运动中心 `0.037～0.039` 帧对角线，高于既有 `0.02` 限值，但前一候选仍让它连续占用搜索探针，最终将正确重捕延迟到 193 帧。本改动与已否决的“直接过滤 detector 候选”不同，保留远距竞争者作为冲突反证。实际执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`，日志 `/tmp/nt-dual-gt-evidence/probe-proximity-host-build.log`、`/tmp/nt-dual-gt-evidence/probe-proximity-host-ctest.log`；单测核验远距候选不被探测但仍在表中造成身份冲突，近距候选可探测，远距两击不能提交。
- **证据边界 / Handoff**：主机测试不能证明 RK3588 上能改善两个目标；先在 400 ms 上检验 car001 标准/右下、car002 标准/真值不退化，再用 330 ms 失败场景核验错车是否消失且 car001 不晚于重捕时限；满足后扩八框矩阵。正式 handoff HOLD。本条与候选代码同提交。

## 2026-11-14T05:01:11+08:00 远距确认门禁板端否决并撤回

- 精确源码 `codex/dual-gt-init-consistency@71a14162df472da8e9c784f77781a5031e40be07`，源码包 `/home/neardi/Project/SOT/nt-dual-gt-confirm-gate-71a1416.tar` 主机/板端 SHA-256 `3b1dfbb4c2a829858a4daa206eeeb636cc5227b9d6f64749a641c49cd64b98cd`，RK3588 隔离根 `/home/neardi/Project/SOT/dual-gt-confirm-gate-71a1416`。原生 CTest `23/23 PASS`，二进制 `build/nanotrack_v3` SHA-256 `727c498ab2223cab37d65d310c046ed938cdca4ed985a66daa4e758ee52d943a`，400 ms headless 配置 SHA-256 `817bae74744ed3f93f6737d6ec66f8df000281560727157706532698e746d43e`，控制器配置 SHA-256 `af4721bb6dafe4fb63c4f7172a5ba3437b20f82a8dcbc3471125b7edd242336a`；视频、GT、三 NanoTrack RKNN、detector/manifest/labels、Linux/RKNN API/driver 身份同上一八框矩阵。
- 在 400 ms 配置以 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox <ROI> --output-video ... --output-boxes ... --session-metadata ...` 运行 car001 标准/右下、car002 标准/真值四组；四 AVI 均 1920×1080×271。板端 `runs/screen-checksums.sha256` SHA-256 `019952069f21b33f8c5c714fb2b1390b039b37402003317b5850a0985ab96958`，文本包 `runs/screen-text.tar.gz` 主机/板端 SHA-256 `5eb6c56f1521e3a8a921eeea3e55e96a3ddcbcb5b36df4ecad36a337f42489cb`，评分 `/tmp/nt-dual-gt-evidence/confirm-gate-71a1416-screen.json` SHA-256 `c5971f24a9601c144a0ca473b6de442945dbf8c4f5fd716b1f98224ab34f43e0`。四组重捕、中心、正式门禁与 `9f84efe` 代表框逐项一致，门禁未改善现行参数。
- 仅板端 `runs/detector330-controller.conf` 改请求间隔 `400→330 ms`，配置 SHA-256 `05aaa16434373ef3e8b721d6f6dc3b6a70a2f304cd9d374f7bd6887414f60291`，`runs/headless-detector330.conf` SHA-256 `fcd2ffe8005e1cd54df1275de0775312b25278d290733e5d933a1a3adf7b0ad1`；car001 标准框 CSV/AVI SHA-256 `2679b9a89f76143308f3ed169fb9fc2f10c2c71818597dd2fc24494668e0d518`/`b2c5b22a09cdfb2fedb5a63669810bfadff9a691600cb50faf0d1e17cd7ee60e`，AVI 1920×1080×271，文本包 `runs/detector330-car001-nominal-text.tar.gz` 主机/板端 SHA-256 `e2111dcebdce615eac93afe054432504a280313a5cb4bdce2ed89159fb0c54ab`，评分 `/tmp/nt-dual-gt-evidence/confirm-gate-71a1416-detector330-nominal.json` SHA-256 `64bc46a96031938e35da435e91be3cd8d440de7f96d049fa7d5eca51f61ce0fe`。第 179/180 帧远处同类候选获得两次 tracker 命中但没有被提交，证明门禁挡住该错车；随后第 193 帧才重捕，tracking/display 中心均值 `4.43/4.47 px`，仍因重捕晚于时限 FAIL，比 400 ms 标准框 PASS 退化。
- **裁决 / Handoff**：撤回 `71a1416` 新增的确认门禁代码、配置及专属单测；保留该提交和板端证据。源码恢复 `9f84efe` 的算法内容和 400 ms 配置，恢复后实际执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，RKNN-off `23/23 PASS`，日志 `/tmp/nt-dual-gt-evidence/confirm-proximity-revert-host-build.log`、`/tmp/nt-dual-gt-evidence/confirm-proximity-revert-host-ctest.log`。八框评测仍以 `9f84efe` 精确板端结果为最新有效对照。远距候选确认安全性可单独研究，但该门禁未使本任务要求的双目标贴真值、一致性及完整门槛更好。正式 handoff HOLD。

## 2026-11-14T04:55:29+08:00 330 ms 请求反事实否决与远距确认门禁候选

- 沿用精确源码 `codex/dual-gt-init-consistency@9f84efecd57d611a239cc71c99ab2e4295025dbc`，RK3588 隔离根 `/home/neardi/Project/SOT/dual-gt-handoff-9f84efe` 及原生二进制 SHA-256 `41d83ef0d772833b29400a35cacb8825a1a402f24ea7dbfe9279ac09b062343b`；仅板端 `runs/detector330-controller.conf` 把 `detector_interval_ms=400→330`，配置 SHA-256 `95c78de6f39d4a73a459aa2caea2550ddba5374ad5b243d8295f40c681338792`，headless 配置 SHA-256 `68bc79b75c78cbddf0ba4d3552bf1f15060914846c780bd0df992da0d359fe2c`。视频、双目标 GT、模型、manifest/labels 和 RK3588 Linux/RKNN 身份同上一八框矩阵。
- 实际执行五组 `./build/nanotrack_v3 --config runs/headless-detector330.conf --bbox <ROI> --output-video runs/detector330/<label>/output.avi --output-boxes ... --session-metadata ...`，标签为 car001 标准/左上/右下、car002 标准/真值；原始输出在板端 `runs/detector330/`，文件清单 `runs/detector330-checksums.sha256` SHA-256 `1d1c6af3b0fab5cbb0c601b4f7b07ab931b2ca11001d7a3a16fa1dc9c54c763e`，文本包 `runs/detector330-text.tar.gz` 主机/板端 SHA-256 `07da0fa01aa1c4f96238d0353c2391e9639901d7e349d1a8b50990a86c5d8426`。主机 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ...` 五组评分 `/tmp/nt-dual-gt-evidence/detector330-five.json` SHA-256 `1caa497e5b372cd528fce37e6c2a934257201f60128a284f4b9162ce30aac4e6`。
- car001 标准/左上均在第 180 帧遮挡中的错误同类车上重捕，后续分别第 206/216 帧再重捕；右下第 191 帧重捕但也落错目标。三框 tracking/display 中心均值分别 `27.71/27.70`、`23.46/23.45`、`33.44/33.27 px`，跨框 P95 `174.40/174.79 px`；正式五组 `0/5 PASS`。car002 标准/真值中心小幅改善至 tracking `1.73/1.60 px`，但完全遮挡语义仍失败。car001 标准框错误候选在第 177～178 帧距运动中心约 `0.037` 帧对角线，在第 179/180 帧得到两次 NanoTrack 命中，说明两击与同候选 detector 佐证仍不足以区分相似车。**否决 330 ms 全局间隔，正式 400 ms 配置和上次八框证据保持当前对照。**
- 基于该反例新增默认关闭的 `recovery_confident_confirmation_proximity_gate`。当运动预测仍可信时，detector 起源候选在确认帧的 tracker 实际框中心必须距运动预测不超过既有 `detector_candidate_max_center_ratio`；远候选仍保留在候选表中提供身份冲突反证，不删除、不直接授予控制。当前可见配置启用，SHA-256 `af4721bb6dafe4fb63c4f7172a5ba3437b20f82a8dcbc3471125b7edd242336a`，detector 间隔仍为 400 ms。主机执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，RKNN-off `23/23 PASS`，日志 `/tmp/nt-dual-gt-evidence/confirm-proximity-host-build.log`、`/tmp/nt-dual-gt-evidence/confirm-proximity-host-ctest.log`；测试保留远距候选元数据但拒绝两击提交，近距 tracker/detector 恢复路径仍可用。本条在 `/home/u20/single-object-tracking` 与代码同提交，基线 `766b01abb79612f95e4e9ff06477a88efbba03aa`。
- **证据边界 / Handoff**：此确认门禁尚未上板。先用 400 ms 精确候选跑 car001 标准/右下与 car002 标准/真值；若不退化，再以 330 ms 作失败场景反事实并扩八框。不能把主机单测或 car002 的局部收益当作两目标晋级依据，正式 handoff HOLD。

## 2026-11-14T04:43:27+08:00 双目标八初始框的 RK3588 真值矩阵与当前裁决

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`；精确源码 `9f84efecd57d611a239cc71c99ab2e4295025dbc`，基线 `74d02d624de5cd17cfed419dbe337d6b6c9ff06b`。源码包 `/home/neardi/Project/SOT/nt-dual-gt-handoff-9f84efe.tar` 主机/板端 SHA-256 `d437e7cac8a4c184ee15fe198d1e39e3f51944d5cf4f9f93d0f0620584512acf`，隔离根 `/home/neardi/Project/SOT/dual-gt-handoff-9f84efe`，原生 CTest `23/23 PASS`；`build/nanotrack_v3` SHA-256 `41d83ef0d772833b29400a35cacb8825a1a402f24ea7dbfe9279ac09b062343b`，`runs/headless-eval.conf` SHA-256 `2d45ca0f31916e0d05e0e5c2cad189bbf0d42f372aa4a67f3dbb3de939c531d6`，`visible-adaptive-display.conf` SHA-256 `5d3214646bd3b0e6d38213482178aa44a0deb301d6f303c8b5eaa7f5517d3a3b`。RK3588 Linux `6.1.84`、RKNN API `2.1.0`、driver `0.9.8`。
- 板端主视频 `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；官方 NanoTrack 三 RKNN `template_backbone/search_backbone/head` SHA-256 `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55`/`d3cb0e3879df6a1272adb59bf501324f1f8f55b58309d3c4da4a731b600973ee`/`4da379f54093d79f02775e90bfea33ebee0837c6fc73a324499279b4c57807c5`；RK3588 VisDrone9 detector/manifest/labels SHA-256 `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c`/`ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9`/`41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`。主机 `data/occ_video_1` 的 car_001、car_002 已确认 GT 与事件包由评分器逐文件哈希验证，评分不使用遮挡估计框作为普通 IoU；精确 GT 身份见本文件前述双目标条目。
- 板端执行 `bash scripts/run_occ_init_matrix.sh ./build/nanotrack_v3 runs/headless-eval.conf runs/matrix`，含 car001 标准/人工/左上/右下与 car002 标准/真值/左上/右下；八个 AVI 全部实测 1920×1080×271。板端 `runs/matrix-checksums.sha256` SHA-256 `8b04bbed755331a03eadc1697bab35f9dafe8c8847083fb72b281dcfe82628c9`，每路 CSV/AVI/session/run.log/JSONL 与运行命令保留 `runs/matrix/<label>/`；文本包 `runs/matrix-text.tar.gz` 主机/板端 SHA-256 `cea2698426035d55367f2ab8bbe1613b41d66184cbf6abe9c9312470b47a305d`。主机执行 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ...` 对每路指定匹配目标 GT，评分 `/tmp/nt-dual-gt-evidence/handoff-9f84efe-eight-center.json` SHA-256 `7613b4730009660882bf0ce012f569f2aad1a975889355b1930c6f84817cc812`。
- car001 四框重捕帧 `188/188/191/191`；tracking 中心均值 `4.28/4.63/4.34/5.02 px`、display `4.32/4.69/4.35/5.05 px`，跨初始框中心 P95 `3.31/3.12 px`。相对 55efe8d 左上框无重捕和 `27.01/26.24 px`，本次已恢复为同一真实目标且轨迹中心接近其他框；标准、人工框正式 PASS，左上框晚于第 190 帧独占时限、右下框还存在完全遮挡语义失败。car002 四框重捕帧 `235/233/233/233`；tracking 中心均值 `2.59/2.00/1.77/2.92 px`、display `2.59/2.00/1.75/2.89 px`，跨初始框中心 P95 两路均 `6.21 px`，和基线一致；四框都在完全遮挡起始时延迟进入 SEARCHING。两目标均无已确认 no-bbox 帧的误观测，但状态语义门仍未过。
- 对原视频第 `191/232/260` 帧抽图，以对应目标 GT、tracking、display 分别用绿/红/蓝绘制六格局部放大图 `/tmp/nt-dual-gt-evidence/handoff-9f84efe-keyframes.png` SHA-256 `892828228fa7413ac69b8f72e4683a4940d202840eb5df7a9195bc33e09090eb`。目视核对：car001 左上/右下第 191 帧均定位同一白车，第 260 帧两路中心都较真值稍偏左上；car002 左上/右下第 232 帧处于未确认的重捕前搜索，右下框预测中心明显落在车身左下，随后第 233 帧才有正式 tracker 两击重捕。接近的全程均值不能消除这段局部偏差。
- **裁决 / Handoff**：新候选改善了 car001 左上框灾难性漂移，八框中心位置与不同初始框的一致性已定量验证；正式完整评测仍 `2/8 PASS`，不能作为批准的板端 handoff 或真实吊舱验收。下一步先处理 car001 两个微扰框重捕迟于时限与 car002 完全遮挡期间的 SUSPECTED→SEARCHING 延迟，再以同一八框 GT 复测；同时 car001 后段可见帧仍约 `7–9 px` 中心误差，需研究 tracker 本身的漂移，不能靠 display detector 美化掩盖。请协调对话继续保持 `development_board_validation.latest_handoff.available=false`，勿启动生产编译/吊舱验收。

## 2026-11-14T04:36:03+08:00 冲突候选的短时佐证交接，撤回整表延长

- 精确源码 `codex/dual-gt-init-consistency@74d02d624de5cd17cfed419dbe337d6b6c9ff06b`：主机/板端源码包 `/home/neardi/Project/SOT/nt-dual-gt-bound-hit-74d02d6.tar` SHA-256 `910b7a55ff08fcda3658eb36b0d7c4155a69f4de41815563c5d9f63d8648efc1`，隔离根 `/home/neardi/Project/SOT/dual-gt-bound-hit-74d02d6`，板端原生 CTest `23/23 PASS`，二进制 `build/nanotrack_v3` SHA-256 `6c5c0e4a0a9d891842a9ad2b18b56fc35826d1c79c878dc7aa9a884f2f0aeb1d`，headless 配置 SHA-256 `be76106c935b518e3872a86daaec1682634659af107ca41bcb2395cfaebf2231`，控制器配置 SHA-256 `5d3214646bd3b0e6d38213482178aa44a0deb301d6f303c8b5eaa7f5517d3a3b`。同一 RK3588、视频、GT、模型/manifest/labels 和驱动身份沿用 7cbf58d 基线。
- 实际运行 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox 990,580,39,41 --output-video runs/car001_left/output.avi --output-boxes runs/car001_left/frames.csv --session-metadata runs/car001_left/session.json`：AVI 为 1920×1080×271，CSV/AVI/session SHA-256 `8b9363706b56631b3b293f5c0695c1776586adea080ec198f7c64e8bb4835dbb`/`f2fee2ecdf9666ae0bcffacbe73935cb2abf198472ae786516f3bf6a68802799`/`83490d7fa77414124cf60ba29987ace57d569f6d3a7261a9b02c6f5ea3e73d8c`，板端 `runs/car001_left-text.tar.gz` SHA-256 `be64c6e574b9c86f6d5175c35b104748593b10128cbb96036bd2d080e3c99ed1`，评分 `/tmp/nt-dual-gt-evidence/bound-hit-74d02d6-car001-left-center.json` SHA-256 `b8e1d8053386f23620f6405d724ed7634a9efbabf72b2ce1c23b25c1cbfc3bf0`。仍无重捕，tracking/display 中心均值 `27.01/26.24 px`；新保留窗使其他同类 detector 候选也多占一帧，第 190 帧仍有身份冲突，首次 NanoTrack 命中延至 191 帧，而 detector 源时效已过。**否决 74d02d6 的整表延长，正式 handoff HOLD。**
- 基于 `74d02d6` 的新改动恢复原候选帧窗；仅在冲突后 detector 门禁开启、detector 元数据因帧窗到期而源时间尚有效、且该候选已经有匹配 tracker 证据时，保留带源时间的短暂交接标记。其他 detector-only 候选正常清理；只有该候选在源时间仍有效时产生的 NanoTrack 验证命中才绑定佐证，后续仍须同候选两击，冲突/重置清标记。`display_bbox` 的 detector 框依旧只服务显示，不进入 tracking/control 授权。主机 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4` 和 `ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure` RKNN-off `23/23 PASS`，日志 `/tmp/nt-dual-gt-evidence/conflict-handoff-host-build.log`、`/tmp/nt-dual-gt-evidence/conflict-handoff-host-ctest.log`。工作树 `/home/u20/single-object-tracking`，本条与代码同提交。
- **证据边界 / Handoff**：新交接候选尚未上板，先按同一 car001 左上框原生复测；若恢复，再复测标准/右下与 car002 代表框和八框矩阵。未满足两目标、两路中心及一致性目标前不批准 handoff。

## 2026-11-14T04:31:00+08:00 一次提前请求的八框矩阵否决与 detector 命中绑定候选

- `codex/dual-gt-init-consistency@55efe8dd7f4cec6a68f586b27d8d5e788cd7be61` 在 RK3588 隔离根 `/home/neardi/Project/SOT/dual-gt-conflict-refresh-55efe8d` 原生编译，CTest `23/23 PASS`；源码包 SHA-256 `e93c98080c2df7b38dad3f287f2626c1a57361ac10dba0d1d2008de274e932db`，二进制 SHA-256 `0de9914db4a35fb9ab6fec5ef0c7d51a820df48feee6bae150d85f4f490a8b13`，headless 配置 SHA-256 `efa187823c528d251ea3cfd852afe3003da77f9b540fbca37fb7039bc691c813`，控制器配置 SHA-256 `5d3214646bd3b0e6d38213482178aa44a0deb301d6f303c8b5eaa7f5517d3a3b`；视频、两套 GT、模型/manifest/labels 与板端驱动身份沿用 7cbf58d 基线。实际以 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox <ROI> --output-video ... --output-boxes ... --session-metadata ...` 运行八框；八 AVI 均 1920×1080×271，文本在板端 `runs/screen` 和 `runs/matrix-extra`。四组筛查文本包 SHA-256 `76c32e0f6be8140e12969d4c018b4747f0517b00e0faa99063b12139fe3af4f9`，另四组文本包 SHA-256 `f96a11e2329fdddf8426dbf4d87f1f6e8f4334b9183585738bb35890936c6f8a`；两批文件清单 SHA-256 分别 `142d7b26503849c063522d18caf2f19613476a7b3fa75db5090e5d6dcbc3960e`、`83d2b8456f305d96b584f5ce25d38326a1d75bc79c9bbb67b77d23e323163210`。`python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ...` 八框评分 `/tmp/nt-dual-gt-evidence/conflict-refresh-55efe8d-eight-center.json` SHA-256 `b13ab50da5673763c9a9879c1a58139a762b3b08e68f90c2ec50911ca50d8492`。
- car001 标准/人工框在第 188 帧重捕，tracking/display 中心均值 `4.28/4.32`、`4.63/4.69 px`，均 PASS；右下框第 191 帧重捕，中心 `5.02/5.05 px`，仍超时且完全遮挡语义失败；左上框没有重捕，中心 `27.01/26.24 px`，四框中心 P95 `145.34/141.49 px`。car002 四框保持第 `235/233/233/233` 帧重捕，tracking 中心均值 `2.59/2.00/1.77/2.92 px`、display `2.59/2.00/1.75/2.89 px`，跨框 P95 两路均 `6.21 px`；四框都因完全遮挡语义失败。正式门禁 `2/8 PASS`，该提交不能晋级。
- 左上框逐帧证据：第 186 帧请求、第 187 帧收到 detector；第 190、191 帧同一候选出现两次 NanoTrack 命中，却未重捕。原三帧候选保留窗在第 190 帧清掉 detector 元数据，尽管其源帧 186 到当时仅 `133 ms`，尚未超过 `150 ms` 时效。基于 `55efe8d` 新增**仅在启用冲突后 detector 门禁时**多一帧候选元数据保留，同时要求 detector 在当前两次 NanoTrack 命中之一发生时仍新鲜，并将这次佐证绑定到该命中；冲突、跨代、命中重置都会清除绑定。detector 仍不能直接授权重捕或控制，display 仍只作显示。工作树 `/home/u20/single-object-tracking`，改动基线 `55efe8dd7f4cec6a68f586b27d8d5e788cd7be61`，本条随代码提交。
- 主机执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，RKNN-off `23/23 PASS`，日志 `/tmp/nt-dual-gt-evidence/conflict-bound-hit-host-build.log`、`/tmp/nt-dual-gt-evidence/conflict-bound-hit-host-ctest.log`。测试覆盖 detector 源时效内第 4 帧交接、命中绑定、下一帧 detector 过期仍须两次 NanoTrack 命中、冲突清除绑定、过期源不得绑定；默认关闭配置维持原三帧窗。第一次全局延长帧窗导致历史 recovery replay 两项测试失败，缩小至启用配置后 `23/23 PASS`。
- **证据边界 / Handoff**：新候选尚未在 RK3588 运行，须以精确提交原生构建和至少 car001 左上/标准/右下及 car002 代表框复测中心、重捕时机与身份；如过门再跑完整八框。正式 handoff HOLD。

## 2026-11-14T04:19:49+08:00 冲突后首个可信候选触发一次提前 detector 请求

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `4669b7be5bbf89c624362646f8ebac5f9395bc3e`。板端 car001 标准框的冲突到 183 帧，184 帧已有 tracker 候选一击，但 400 ms 定时 detector 下次在 189 帧请求、190 帧到达，造成 192 帧才完成 NanoTrack 两击重捕。新增每次丢失最多一次的定向提前请求：仅当已发生身份冲突、候选有至少一击且该候选尚无 detector 证据时，允许越过常规定时；仍须原有 class/generation/stale 门禁与同候选 NanoTrack 两击确认。未改变 detector 显示用途或直接控制权限，配置 SHA-256 仍为 `5d3214646bd3b0e6d38213482178aa44a0deb301d6f303c8b5eaa7f5517d3a3b`。
- 主机实际执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，RKNN-off `23/23 PASS`；日志 `/tmp/nt-dual-gt-evidence/conflict-refresh-host-build.log`、`/tmp/nt-dual-gt-evidence/conflict-refresh-host-ctest.log`。测试核验冲突后有一击时可提前请求、不可连续越过间隔、无冲突时不提前。首次测试夹具未建立目标类别，按安全设计拒绝 detector 请求；补全固定车类初始化后通过。
- **证据边界 / Handoff**：提前请求可能改变 detector 竞争身份与恢复路径，必须在 RK3588 用精确提交完成 car001 标准/右下、car002 标准/真值四组；只有中心一致性、重捕时机、身份与遮挡语义都不退化，才扩八组。正式 handoff HOLD。

## 2026-11-14T04:18:00+08:00 身份冲突后同候选佐证的四组板端筛查

- 精确源码 `codex/dual-gt-init-consistency@4669b7be5bbf89c624362646f8ebac5f9395bc3e`，源码包 `/home/neardi/Project/SOT/nt-dual-gt-conflict-detector-4669b7b.tar` 主机/板端 SHA-256 `599040638f953d8f562eb20b266ee17b3f8792de26b31b73ea3ae692ee87c437`，隔离根 `/home/neardi/Project/SOT/dual-gt-conflict-detector-4669b7b`。RK3588 原生 CTest `23/23 PASS`，二进制 `build/nanotrack_v3` SHA-256 `800944e5718d12c3507c1431c0a5295fddc2dc8107e6ee26638bdcd7f7fd65d8`，控制器配置 SHA-256 `5d3214646bd3b0e6d38213482178aa44a0deb301d6f303c8b5eaa7f5517d3a3b`，headless SHA-256 `77a6f09404cb76962a77d8e103feb0b3483475e5c2a6169547bebe35c39b4a91`；视频、双目标 GT、官方 tracker 与 detector 身份同 7cbf58d 基线。
- 执行 car001 标准/右下、car002 标准/真值四组 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox <ROI> --output-video ... --output-boxes ... --session-metadata ...`，四 AVI 均 1920×1080×271。16 文件清单 `runs/screen-checksums.sha256` SHA-256 `2306d11277a40b6799fbe4ae4021c7647031688ea9f73a474c4b6ed459276d66`；文本包 `runs/screen-text.tar.gz` 主机/板端 SHA-256 `3badfa3ced7ae547c78913ff99b95c4881f51ffb6031377c5ffb41a9f3b6c3d3`，评分 `/tmp/nt-dual-gt-evidence/conflict-detector-4669b7b-screen-center.json` SHA-256 `1874a177ad362ca4d62ad83d00664a6062aa02525b964098fecea03d98160c2d`。
- car001 标准/右下 tracking 中心均值 `4.39/5.02 px`、display `4.44/5.05 px`，重捕 `192/191`，跨框 tracking/display 中心 P95 `2.88/2.16 px`；标准框 185–190 帧不再输出偏右 staged，而在 192 帧经 detector 同候选佐证重捕，但晚于正式重捕时限。car002 标准/真值 tracking 均值 `2.59/2.00 px`、重捕 `235/233`，基本保持基线路径，完全遮挡语义仍失败。四组正式门禁 `0/4 PASS`；这是中心一致性有改善但时效和遮挡门禁未过的中间候选，不能晋级。
- **裁决 / Handoff**：保留候选源码和板端证据用于定向缩短冲突后 detector 等待；没有批准正式 handoff。后续须证明提前请求不会重现先前 300 ms 全局加速时的同类车误重捕，再做八组矩阵。

## 2026-11-14T04:13:33+08:00 身份冲突后同一候选的 detector 佐证门禁候选

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `5e74e958ebd0b00574776bc4b7c2634c5716aaea`。对已验证八组板端 CSV 和双目标 GT 逐帧核对：car001 标准/人工/左上/右下框在重捕前分别有 `178–183`、`178–182`、`178–180,190–192`、`188` 帧身份冲突；car002 四组在重捕前没有。car001 标准框第 185 帧纯 tracker 候选重捕，中心误差 `9.2 px`，随后 staged 偏框；另三框经 detector 候选在 191–194 帧重捕。car002 四组 tracker-only 重捕中心更贴近真值，不能一律要求 detector。
- 新增默认关闭的 `recovery_conflict_requires_detector`，只在本次丢失已经观察到身份冲突时，要求**同一个** tracker 两击确认候选具有 detector 证据后才准许重捕；无冲突路径仍按既有两击确认。detector 证据自身不授权 tracking/control，仍由 NanoTrack 两击确认。`visible-adaptive-display.conf` 启用，SHA-256 `5d3214646bd3b0e6d38213482178aa44a0deb301d6f303c8b5eaa7f5517d3a3b`；该门禁不消费 display 的 detector 框。
- 实际执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`；日志 `/tmp/nt-dual-gt-evidence/conflict-detector-host-build.log`、`/tmp/nt-dual-gt-evidence/conflict-detector-host-ctest.log`。测试验证冲突被本次丢失记住、冲突后仅 tracker 两击不能授权、同候选 detector 佐证加 NanoTrack 确认可以授权、无冲突两击路径保留。
- **证据边界 / Handoff**：目标是消除 car001 过早且偏框的 tracker-only 重捕，同时保留 car002 的无冲突重捕；必须以精确源码在 RK3588 原生运行四组代表框并比较两路中心、跨框 P95、重捕和遮挡语义，再决定是否扩八组。现阶段正式 handoff HOLD。

## 2026-11-14T04:07:45+08:00 恢复 probation 严格双模板 IoU 门禁的四组板端否决

- 精确源码 `codex/dual-gt-init-consistency@4b46dd3ac230c847d4d0d9d246528b771f8fedb0`，源码包 `/home/neardi/Project/SOT/nt-dual-gt-probation-iou-4b46dd3.tar` 主机/板端 SHA-256 `1d11da318aa4460e8f972236ba9bd9564a8ef922977fe650dbe2629fa9f39591`，隔离根 `/home/neardi/Project/SOT/dual-gt-probation-iou-4b46dd3`。RK3588 原生 CTest `23/23 PASS`；二进制 `build/nanotrack_v3` SHA-256 `c948a95f9e96ce8dac86b96836fa22ce2c2d9041faeb7d6e207a258faa00149d`，控制器配置 SHA-256 `f20007d44e7a06045d1a2a3edbb79cbc8cdf18c8f334907d8589f4e3fa04c4db`，headless 配置 SHA-256 `35cce3fb3da04ee3fbdba14f8efdd48c71b3f722e68c5ce5bf0525f5c51d0cd5`；视频、GT、模型、detector 及环境同基线矩阵。
- 执行 car001 标准/右下、car002 标准/真值四组 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox <ROI> --output-video ... --output-boxes ... --session-metadata ...`。16 文件清单 `runs/screen-checksums.sha256` SHA-256 `879c1e990906eb829e8eda6d403ba42a2d64cdfac26ee9e1c00977fe88338089`，四 AVI 均 1920×1080×271；文本包 `runs/screen-text.tar.gz` 主机/板端 SHA-256 `feb5cd6919326dabfefbcac4107e19c0a124c9f82d5f3cd3783e54837481b770`，评分 `/tmp/nt-dual-gt-evidence/probation-iou-4b46dd3-screen-center.json` SHA-256 `be9c50f37e311c5b764401701c17c6a1e014cdd507870ff00e0692894c71e4f7`。
- car001 标准/右下 tracking 中心均值 `19.04/5.02 px`，重捕 `185,200,223,239`/`191`，跨初始框 tracking/display 中心 P95 `72.58/72.58 px`。car002 标准/真值中心均值 `2.59/2.00 px`，重捕 `235/233`，跨初始框 tracking/display P95 `5.09/5.03 px`，完全遮挡语义仍失败。四组正式门禁 `0/4 PASS`；严格门禁使 car001 标准框反复回滚、错误重捕，未形成稳定身份。
- **裁决 / Handoff**：撤回 `4b46dd3` 代码、配置与单测，恢复基线控制器配置 SHA-256 `63007d32b58c7e4aa920762f4d73f3532ef8ee544e50101c0684914d9b470acc`；保留精确板端输出供后续分析。简单调大 probation 几何阈值与前述中心融合均无法保证两目标/两路中心及初始框一致性。正式 handoff HOLD；下一步应以恢复阶段的多假设身份验证与运动连续性为研究对象，并在相同八组真值矩阵复核，不能依据模型加载、主机测试或单个 ROI 局部收益晋级。

## 2026-11-14T04:03:22+08:00 重捕 probation 双模板冲突门禁候选

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `6363b935fe6dbf1dd0ca1a3b7d60e407d0b89a65`。基线 car001 标准框第 185 帧重捕后，186–189 帧 staged/recent IoU 约 `0.56–0.60`，staged 连续偏离人工真值，recent 则贴近真值；car001 另外三框 probation IoU 至少约 `0.89`，car002 两个代表框至少约 `0.79`。新增默认关闭的 `recovery_probation_min_template_iou`，当前可见配置取 `0.75`；仅当恢复 probation 的 staged/recent 都可靠却低于阈值时，不接受本帧 staged 并回滚模板，避免高分但冲突的 staged 连续污染可信中心。控制器配置 SHA-256 `f20007d44e7a06045d1a2a3edbb79cbc8cdf18c8f334907d8589f4e3fa04c4db`；detector 显示框不参与此门禁。
- 实际执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`；日志 `/tmp/nt-dual-gt-evidence/recovery-probation-iou-host-build.log`、`/tmp/nt-dual-gt-evidence/recovery-probation-iou-host-ctest.log`。新增测试证明可靠但相离的两个模板不能把 staged 作为观测接受。首次测试夹具缺少已暂存模板 slot，导致 `bad_optional_access`；补全真实 probation slot/事务状态后 23/23 通过。
- **证据边界 / Handoff**：这是对基线板端冲突帧的假设，回滚后可能延迟重捕或转向其他目标；必须在 RK3588 上检验 car001 标准/右下与 car002 标准/真值的中心、重捕、身份和遮挡语义，再决定是否扩八组。正式 handoff HOLD。

## 2026-11-14T04:00:13+08:00 双模板中心融合的四组板端否决与撤回

- 精确源码 `codex/dual-gt-init-consistency@9bb08b6c8217d026f8023dc2a63a7ab406888284`，源码包 `/home/neardi/Project/SOT/nt-dual-gt-safe-blend-9bb08b6.tar` 主机/板端 SHA-256 `c541ddad201ddcb0e4424f4c633528bcaf95671538c3b8989a4155d9780c3f4b`，隔离根 `/home/neardi/Project/SOT/dual-gt-safe-blend-9bb08b6`。RK3588 原生 CTest `23/23 PASS`；二进制 `build/nanotrack_v3` SHA-256 `79374a4994cff738e326a6bae29f6fda42da675f6b18c346c769f257287cc1a6`，控制器配置 SHA-256 `b56419b8daffae18afeffb2d40e9957899d82586ee027e74aef5c01b065da621`，headless 配置 SHA-256 `6bb93a9cc4af0e25c547cda80f85c3168e0edaa5620b08ae4ce387267dd7dd9f`；视频、GT、模型、detector 及环境同基线矩阵。
- 执行 car001 标准/右下、car002 标准/真值四组 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox <ROI> --output-video ... --output-boxes ... --session-metadata ...`。16 文件清单 `runs/screen-checksums.sha256` SHA-256 `dbf8f665b27779fc8b740448c3e9a41a61d04485630d438558b4d8713b8cb912`，四 AVI 均 1920×1080×271；文本包 `runs/screen-text.tar.gz` 主机/板端 SHA-256 `5e0acf6bcb81b3771bc3d8be77886a91501a5f8947b7523b5e95903832b2bdbf`，评分 `/tmp/nt-dual-gt-evidence/safe-blend-9bb08b6-screen-center.json` SHA-256 `5a70bb2962eb978eb125a4852f957a899194d5947226d39126f1dbbb30688123`。
- car001 标准/右下 tracking 中心均值 `32.93/5.09 px`，重捕 `186/192`，跨初始框 tracking/display 中心 P95 `135.85/133.08 px`；标准框重捕后偏离累积，真实目标身份代理和可见框门禁失败。car002 标准/真值中心均值 `2.10/1.77 px`，跨初始框 P95 `3.42 px`，但完全遮挡语义仍失败。四组正式门禁 `0/4 PASS`。离线冻结轨迹对融合的微小收益没有覆盖实际反馈：一旦微调中心影响下一帧裁剪，遮挡后的恢复路径可分叉。
- **裁决 / Handoff**：撤回 `9bb08b6` 融合代码、配置、测试以及 `c3ce34a` 的诊断环境开关，保留所有板端输出和上述两条实验记录；恢复 `027e28b` 的当前算法文件与 `63007d32b58c7e4aa920762f4d73f3532ef8ee544e50101c0684914d9b470acc` 控制器配置。两目标/两路中心的一致性目标仍未达到，正式 handoff HOLD。后续研究重捕阶段的身份与模板轨迹分叉，不继续用微小中心修补推断长期效果。

## 2026-11-14T03:54:58+08:00 可信双模板近邻中心融合候选

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `c3ce34ad1f4b8eff368f41fa7fe22e93f1d80101`。诊断显示四组 recent/safe 配对帧 safe 平均中心改善仅 0.22–0.51 px，但偶有远处误响应；只在正常 TRACKING、无模板 probation、两响应均 reliable、中心距不超过 recent 框对角线 15%、框 IoU 至少 0.75 时，按 `recent_safe_center_blend_alpha=0.50` 融合中心，尺寸与 selected_template 仍属 recent。其他配置默认 0 禁用。`visible-adaptive-display.conf` SHA-256 `b56419b8daffae18afeffb2d40e9957899d82586ee027e74aef5c01b065da621`。
- 主机执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，RKNN-off `23/23 PASS`；日志 `/tmp/nt-dual-gt-evidence/safe-center-blend-host-build.log`、`/tmp/nt-dual-gt-evidence/safe-center-blend-host-ctest.log`。新增可靠近邻可融合与远离 safe 不得牵引中心的控制器测试。
- **证据边界 / Handoff**：离线融合只基于冻结轨迹估算，改变中心后会反馈到下一帧裁剪，必须在 RK3588 原生实际运行至少四组筛查，再决定是否扩为八组；正式 handoff HOLD。

## 2026-11-14T03:53:36+08:00 正常跟踪 safe head 诊断结论

- 精确源码 `codex/dual-gt-init-consistency@c3ce34ad1f4b8eff368f41fa7fe22e93f1d80101`，源码包 `/home/neardi/Project/SOT/nt-dual-gt-safe-head-c3ce34a.tar` 主机/板端 SHA-256 `4118ac8a768c3ce49356923fcd09f68c3a99ce325199a7724f419f567f772dd9`，隔离根 `/home/neardi/Project/SOT/dual-gt-safe-head-c3ce34a`。RK3588 原生 CTest `23/23 PASS`；二进制 `build/nanotrack_v3` SHA-256 `88d821e6610d25b88083e8bae6f598a43ff6a4a9db8aa53e8f65e0dc817cb440`，headless 配置 SHA-256 `5fdb1c5eadfec175545c5c36b0af15c66571ab673ead03da7a4f3d455e130219`；模型/视频/GT/detector 及环境同 7cbf58d 基线。
- 执行 car001 标准/右下、car002 标准/真值四组 `NT_DIAGNOSTIC_SAFE_HEAD=1 ./build/nanotrack_v3 --config runs/headless-eval.conf --bbox <ROI> --output-video ... --output-boxes ... --session-metadata ...`。四 AVI 均 1920×1080×271，16 文件清单 `runs/safe-probe-checksums.sha256` SHA-256 `d89d2b47ef9afe97324d207c8bb6919eab6bc86e8269581f89822c47de9886d2`；文本包 `runs/safe-probe-text-v2.tar.gz` 主机/板端 SHA-256 `fd7e596db71a4660af5991a6a7101f9a5585ea65df24c5fa1b6f867a9a9401e1`，评分 `/tmp/nt-dual-gt-evidence/safe-head-c3ce34a-center.json` SHA-256 `98fb7c3bec5e66eb899a44286da43199d83ebf6d805926d814b8727d51bb3168`。
- car001 标准/右下与 car002 标准/真值的可见配对帧数 `233/233/229/227`，safe 相对 recent 平均中心误差 `−0.413/−0.352/−0.509/−0.218 px`；safe 优胜超过 0.5 px 的帧数 `69/125/95/79`，recent 优胜 `25/35/18/52`。car001 标准框第 166 帧 safe 中心误差 53.79 px（recent 7.48 px），不能直接切换 safe。四组 tracking 框逐帧与基线相同；car001 右下第 71 帧额外 head 改变了异步 detector 显示刷新时序，其余 display 相同。**裁决 / Handoff**：撤下诊断环境变量候选，保留原始证据，研究仅在双模板近邻且可信时的中心融合；正式 handoff HOLD。

## 2026-11-14T03:46:30+08:00 安全模板正常跟踪响应的板端诊断候选

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `027e28b8daba1b0ff74e8b3f345f9e43c32118f5`。在 `NanoTrackV3::infer` 增加仅由环境变量 `NT_DIAGNOSTIC_SAFE_HEAD=1` 激活的正常 TRACKING safe head 原始响应采集；控制器仍选择 recent，tracking/control/display 选择策略不变。未设置变量时仍只运行 recent；这只是比较模板几何的诊断候选。
- 实际执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`；日志 `/tmp/nt-dual-gt-evidence/safe-head-diagnostic-host-build.log`、`/tmp/nt-dual-gt-evidence/safe-head-diagnostic-host-ctest.log`。控制器配置 `rk3566/nanotrack_v3/config/visible-adaptive-display.conf` SHA-256 `63007d32b58c7e4aa920762f4d73f3532ef8ee544e50101c0684914d9b470acc`，未改。
- **证据边界 / Handoff**：需以精确提交在 RK3588 原生构建，并以两目标四种代表性初始框逐帧对照 recent/safe 中心与 GT；若 safe 无稳定优势，撤回诊断源码；正式 handoff HOLD。

## 2026-11-14T03:42:44+08:00 冻结 g5000 权重的 RK3588 双目标反事实筛查否决

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，当前源码 `8f102f06fec817130b770faf57e47af99ccc3089`；板端复用 `/home/neardi/Project/SOT/dual-gt-visibility-7cbf58d/build/nanotrack_v3`，二进制 SHA-256 `3f6c3896f777148a0d038d8c02b5d21fefa7461cc649336bb538db3b1cdb15d4`。上游冻结 g5000 checkpoint `artifacts/nanotrack_conservative_three_stage/run/stage1/candidate_stage1_g00005000_e001_s02500.pth` SHA-256 `40ebbe4296938bf5d53c8605ab05e729b306fc0bce65921a57ab6613030410d1`；导出源 `/home/u20/single-object-tracking/repos/siamtrackers/NanoTrack@248663fde6bf7c40190cf10ee396d5662919ecd3`、配置 `models/config/configv3.yaml` SHA-256 `60ea9dc886dcdd0451e60b404490a39d19495480185ca7b6299b72b74d746f87`。
- 实际以 `.venvs/rknn-convert/bin/python rk3566/nanotrack_v3/tools/export_v3.py --repo ... --config ... --weight ... --output-dir artifacts/rk3588-g5000-dual-gt --seed 20260929 --opset 14 --atol 1e-4 --rtol 1e-4` 导出，ONNX/ORT 校验通过，head cls/loc 最大绝对误差约 `6.91e-6/1.11e-4`，记录 `/tmp/nt-dual-gt-evidence/g5000-export.log`、`artifacts/rk3588-g5000-dual-gt/export_metadata.json` SHA-256 `4f499b5f033bb40660338b692124423d8e448f7178ea70a8fd37ad7d9a4bdb84`。再运行 `.venvs/rknn-convert/bin/python rk3566/nanotrack_v3/tools/convert_rknn.py --model-dir artifacts/rk3588-g5000-dual-gt --source-commit 248663f... --weight-sha256 40ebbe... --target rk3588`，Toolkit 2.3.2，未量化，日志 `/tmp/nt-dual-gt-evidence/g5000-rk3588-convert.log`。三 RKNN 文件 SHA-256：template `daa9609ca21c95292757faf023b4486446de821a62c57eb479187ae068a20931`、search `4ec265f417396d2bbafc8a9085b1b07be882b4fbc26f5fcd5b497e8849a7ec49`、head `abbb90b06b370f9fe8e0464751eec6be59da25a1f9ab402532613b1e9725aa3d`；manifest SHA-256 `8bde79120af2be7366b0a829a52c9974a087e9c60d3cf13a97be77d969093206`，内部哈希三项均匹配。
- 模型包 `/home/neardi/Project/SOT/nt-g5000-rk3588-dual-gt.tar.gz` 主机/板端 SHA-256 `44c9cf8d667e87a5c335bb7c4b767b461a72a0137992f7ddac5115a036830745`；板端隔离模型目录 `/home/neardi/Project/SOT/g5000-rk3588-dual-gt` 的三个 RKNN/manifest 哈希同主机。只在基线根派生 `runs/headless-g5000.conf`，SHA-256 `e1d219e065cb7aa97a52b7db431dcdbafddffe7bcb1b5172da18cb3ab76adbee`，视频、真值、detector 及其他控制参数保持原矩阵身份。执行 car001 标准/右下、car002 标准/真值四组 `./build/nanotrack_v3 --config runs/headless-g5000.conf --bbox <ROI> --output-video ... --output-boxes ... --session-metadata ...`。16 文件清单 `runs/g5000-screen-checksums.sha256` SHA-256 `b80e3ec5d764c25a083c015f635e85ef60d3477f6984e26b97179339a0a0fa66`，四 AVI 均 1920×1080、271 帧；文本包 `runs/g5000-screen-text.tar.gz` 主机/板端 SHA-256 `b6b4ba53751c5ae6c38456e3cc9688dec93c227128c3620d297b2dcce58ea8c1`，评分 `/tmp/nt-dual-gt-evidence/g5000-screen-center.json` SHA-256 `babffaf1e4b3d3efc3a103e8dcbda0d786d5b68a10889a8d8d5bfdb3187ff4cb`。
- car001 标准/右下 tracking 中心均值 `7.88/33.15 px`，重捕 `153,193`/`187`，跨框中心 P95 `136.95 px`；car002 标准/真值均值 `3.27/2.97 px`，重捕 `234/235`，跨框 P95 `4.54 px`，标准框无框误观测 1 帧。四组正式门禁全 FAIL。**裁决 / Handoff**：g5000 在这套 RK3588 双目标任务上不能替换官方权重，保留实验包与原始输出，正式模型身份不变，handoff HOLD。此结论不改写上游训练选择或其他数据集性能结论。

## 2026-11-14T03:37:03+08:00 半像素解码中心修正的八组板端否决

- 精确源码 `codex/dual-gt-init-consistency@7d7a40aaf520b208fc3456e31487999f1c135759`，包 `/home/neardi/Project/SOT/nt-dual-gt-pixel-edge-7d7a40a.tar` 主机/板端 SHA-256 `64b907c127b7b1d5cb055e10522e369de2ed720e88e8eacf97cc281e8bee7388`，隔离根 `/home/neardi/Project/SOT/dual-gt-pixel-edge-7d7a40a`。RK3588 原生构建及 CTest `23/23 PASS`；二进制 `build/nanotrack_v3` SHA-256 `243820e7418a353dc8a52360437989add733242842b5e6e58ef9aeb92948a1be`，控制器配置 SHA-256 `63007d32b58c7e4aa920762f4d73f3532ef8ee544e50101c0684914d9b470acc`，headless SHA-256 `58615f65dcce0ddf03295bf1da2e1d9783c1cc0cfd58c3be93afcb7d7b24dd99`；视频、GT、模型和板端环境同前述基线矩阵。
- 执行八组人工初始框变体 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox <ROI> --output-video ... --output-boxes ... --session-metadata ...`。32 文件清单 `runs/screen-checksums.sha256` SHA-256 `664d409ef1052554e9614876b0f1316e02f1fc7676c419d283e2f0d36990327c`，八 AVI 均 1920×1080、271 帧；文本包 `runs/screen-text.tar.gz` 主机/板端 SHA-256 `211335f086aa97d80df1ec1dfee1a8dc3e0ec71193c8794d8fd396b77c9f0564`，评分 `/tmp/nt-dual-gt-evidence/pixel-edge-7d7a40a-center.json` SHA-256 `e06b57f2a79aa8a1ddc4abc10ff0feded884d1928e50e06a591e8409a6bdd704`。
- car001 标准/右下中心均值从基线 4.39/5.02 改为 3.89/4.26 px，重捕 185/186；但人工框第 155/187 帧两次重捕、左上框第 186/204/239 帧三次重捕，二者中心均值 6.22/14.26 px，四框 tracking/display 中心 P95 `45.74/45.85 px`，明显差于基线 `3.51/3.65 px`。car002 四框中心均值 `2.69/2.25/2.35/3.11 px`，跨框 tracking/display P95 `6.34/6.34 px`，完全遮挡语义仍失败，右下另有预测连续性失败；正式八组 `0/8 PASS`。car001 标准/右下可见框失败集中在 185–190 帧：重捕后 staged 模板偏右，recent 模板随后靠近真值，但 probation 输出仍优先 staged。
- **裁决 / Handoff**：半像素解码修正不能在四种初始框上保持身份一致，撤回 `7d7a40a` 的源码变更，保留八组原始板端输出与评分；正式 handoff HOLD。后续应分别检验模型/模板对初始框微扰的稳定性与 probation 选择策略，不能仅凭两组改善认定全目标达标。

## 2026-11-14T03:28:58+08:00 NanoTrack 裁剪像素中心到框边界中心的坐标候选

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `89a9e101f5752c6c5b74744bf5609e1a627adc22`。`NanoTrackV3::infer` 以 `bbox.x+(width−1)/2` 定位像素裁剪中心，而通用 `BBox` 与 `decode_head` 使用 `bbox.x+width/2` 作为框边界中心。仅在 `run_head` 进入解码器前把参考中心加 `0.5 px`，保持裁剪与 RKNN 输入不变；检验零位移时的半像素坐标偏差是否累积成可见中心误差。当前是数学假设，不能从代码差异直接推断板端效果。
- 实际执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`；日志 `/tmp/nt-dual-gt-evidence/pixel-edge-center-host-build.log`、`/tmp/nt-dual-gt-evidence/pixel-edge-center-host-ctest.log`。
- **证据边界 / Handoff**：须在 RK3588 以精确提交测试 car001 标准/右下、car002 标准/真值初始框；同时比较 tracking/display 中心、IoU、重捕、身份与遮挡语义。板端证明前保持 HOLD。

## 2026-11-14T03:25:41+08:00 NanoTrack 窗口权重 0.30 板端否决

- 精确源码 `codex/dual-gt-init-consistency@2e6a0cdca6847c21b1bab979ce66df0cee877d2a`，包 `/home/neardi/Project/SOT/nt-dual-gt-window030-2e6a0cd.tar` 主机/板端 SHA-256 `79a56c94ffa2402d473fe2126cabd9206e23ba9cb852d9143e19e15d70f68282`，隔离根 `/home/neardi/Project/SOT/dual-gt-window030-2e6a0cd`。RK3588 原生构建及 CTest `23/23 PASS`，二进制 `build/nanotrack_v3` SHA-256 `5417ee1826447dd5b592373ac8a29387a0ef2818037260083301c0cf2d788de5`，控制器配置 SHA-256 `63007d32b58c7e4aa920762f4d73f3532ef8ee544e50101c0684914d9b470acc`，headless SHA-256 `0a8ff85069a15eeeb14692a1d8fa74da19fb8e55710a899298c263a229b9e616`；视频、GT、模型和板端环境同前述精确矩阵。
- 执行四组 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox <ROI> --output-video ... --output-boxes ... --session-metadata ...`，car001 标准/右下与 car002 标准/真值。16 文件清单 `runs/screen-checksums.sha256` SHA-256 `c01db02d914937097f8542a2044127a405fbb53b61591bedc4ba38bd296e66f9`，四 AVI 均 1920×1080、271 帧；文本包 `runs/screen-text.tar.gz` 主机/板端 SHA-256 `071ec441fbee1c8c40815feeac0d62897172dcfccf04b564a84736dd87d32b22`，评分 `/tmp/nt-dual-gt-evidence/window030-2e6a0cd-screen-center.json` SHA-256 `1e1677acde63fd74f04097d30b86f3a6353af20850052695bec3d5ee5ab1cf62`。
- car001 标准/右下 tracking 中心均值 `202.96/161.07 px`，分别 3/4 次重捕，跨框中心 P95 `601.66 px`；car002 标准/真值均值 `105.01/25.97 px`，分别 2/3 次重捕，标准框 15 个无框帧误观测 12 帧，跨框中心 P95 `184.90 px`。四组正式门禁全部 FAIL。**裁决 / Handoff**：Hann 权重 0.30 破坏目标身份和中心一致性，撤回该变更，保留板端证据；不据此推定其他权重可用，正式 handoff HOLD。

## 2026-11-14T03:21:28+08:00 NanoTrack 响应窗口 0.30 主机筛查候选

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `ecb997fea5b45a4094d9b56e68cc13c0171bb73b`。保持此前八组最佳的恢复/显示逻辑，只将 NanoTrack 解码的 Hann 窗口权重从 `0.455→0.300`（默认和 RKNN 运行实例同步），检验较弱中心先验能否改善运动段的原始中心偏移和不同初始框的收敛；不使用 detector 中心修正 `tracking_bbox`，`display_bbox` 的 detector 信息仍仅供外观尺寸。
- 实际执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`；日志 `/tmp/nt-dual-gt-evidence/window030-host-build.log`、`/tmp/nt-dual-gt-evidence/window030-host-ctest.log`。
- **证据边界 / Handoff**：尚无 RK3588 推理结果。先筛 car001 标准/右下与 car002 标准/真值框，只有中心、重捕、遮挡语义均不退化才扩展八组；正式 handoff HOLD。

## 2026-11-14T03:19:53+08:00 detector 初始框统一化的板端反事实否决

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，当前源码 `9f64c5df6bb3112fe23836a12a0b5f4818210e7b`，板端复用已测基线根 `/home/neardi/Project/SOT/dual-gt-visibility-7cbf58d`、二进制 SHA-256 `3f6c3896f777148a0d038d8c02b5d21fefa7461cc649336bb538db3b1cdb15d4`，配置/视频/GT/模型/板端环境同基线矩阵。四种人工 ROI 对每个目标都匹配相同的初始化 detector 候选；car001 候选 `994.000916,581.063293,35.289490,39.486084` 接近真值，car002 候选 `973.034729,520.489929,34.091492,31.423828` 比真值 `966.99936,512.99946,42,42.00012` 明显偏小。
- 用 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox <ROI> --output-video ... --output-boxes ... --session-metadata ...` 直接模拟三种确定性初始化：car001 detector 原框；car002 detector 原框；car002 detector 中心加 42×42 框。12 文件清单 `runs/init-detector-screen-checksums.sha256` SHA-256 `058021eaefd6e8b04821a7938c81e4f264d5a2ea5c58c2d16976360191fe7b1e`，三 AVI 均 1920×1080、271 帧；文本包 `runs/init-detector-screen-text.tar.gz` 主机/板端 SHA-256 `0fc44a08a4b16a224e1bcef2b508ac8ef49ee0f8c137f837942636f56fa4a916`，评分 `/tmp/nt-dual-gt-evidence/init-detector-screen-center.json` SHA-256 `282d7043275fa05bb25f338dc6493934ace865b8c0eae324205e0dd79327e9a10`。
- car001 detector 原框第 207 帧才重捕，tracking 中心均值 8.70 px；car002 detector 原框第 82/195 帧两次重捕，均值 103.27 px 且 15 个无框帧有 13 帧误观测。car002 detector 中心加 42×42 框第 233 帧重捕、中心均值 2.17 px，但该 42×42 尺寸来自真值反事实，运行时不能假设。**裁决 / Handoff**：不把 detector 初始候选直接替代人工 ROI；保留三组板端原始输出和评分。正式 handoff 仍 HOLD。

## 2026-11-14T03:16:06+08:00 真正冻结早搜加速度后的板端四组否决

- 修复后精确源码 `codex/dual-gt-init-consistency@a1b368e2966251cf326816780163bdd1916762e9`，包 `/home/neardi/Project/SOT/nt-dual-gt-early-freeze-a1b368e.tar` 主机/板端 SHA-256 `b96eb2ec6557f58a0ee0fd652ad80ac3b4f26c6b86ef3a75aa44000a1bdda089`，隔离根 `/home/neardi/Project/SOT/dual-gt-early-freeze-a1b368e`。RK3588 原生构建及 CTest `23/23 PASS`；二进制 `build/nanotrack_v3` SHA-256 `cc257ac03e078489fca588d95de91bc32c9701c44910a174f8d5b893669eee31`，控制器配置 SHA-256 `0d9d4479ec2e6a273ce79d2b269833aadfab7926fada11c7f02b03dd76785f66`，headless SHA-256 `ca4d32355a163c49cccb0752bf37850429ce361d81a31be63b57ab68cc2d2839`；视频、GT、模型、板端环境同前述精确矩阵。
- 执行四组 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox <ROI> --output-video ... --output-boxes ... --session-metadata ...`，car001 标准/右下与 car002 标准/真值框。16 文件清单 `runs/screen-checksums.sha256` SHA-256 `9368c01ac30de5bc4d081b4897faa8b6310363f26f12a34d16ffcc846f7ebf86`，四 AVI 均 1920×1080、271 帧；文本包 `runs/screen-text.tar.gz` 主机/板端 SHA-256 `acc5afacd0f31fd28388729be19833071518aef76ddff246a16f83a95575553c`，评分 `/tmp/nt-dual-gt-evidence/early-freeze-a1b368e-screen-center.json` SHA-256 `9fd5ca684c60cca453dd9c796fbda7a3a042c7afe343e66b73ebd1027303ac10`。
- car001 重捕 187/198、tracking 中心均值 4.16/5.82 px、跨两框 tracking/display 中心 P95 17.61/15.91 px；car002 重捕 253/253、均值 3.03/2.45 px、跨两框 P95 10.32/9.87 px。冻结加速度较未执行的早搜 600 ms 候选有改善，但仍显著差于 7cbf58d 基线八组的 car001/car002 跨框 P95 3.51/6.21 px，重捕也晚。两组 car002 语义 PASS 不能抵消中心一致性退化。
- **裁决 / Handoff**：四组筛查已经触发否决，无需重复完整八组；撤回 07bdb37/a1b368e 的早搜与冻结源码/配置/单测，保留原始板端输出，正式 handoff HOLD。下一步沿原 400 ms、非提前搜索基线，单独寻找可见段中心偏移与 car002 完全遮挡的更可靠证据。

## 2026-11-14T03:11:25+08:00 冻结加速度触发标志修复及前次四组证据

- `07bdb37d1571eb07b26e2409a68067ae21f4d443` 精确源码包 `/home/neardi/Project/SOT/nt-dual-gt-early-freeze-07bdb37.tar` 主机/板端 SHA-256 `a31a361493e49cf0fc1bea4a47fb3cc9c2310f881ea5aa8fd66145ff784c2bd3`，板端隔离根 `/home/neardi/Project/SOT/dual-gt-early-freeze-07bdb37`。RK3588 原生构建及 CTest `23/23 PASS`；二进制 SHA-256 `37f07eb33d64be6e9b9c1b4ef7f6581c341f08182d3ec890f717d60a966e2816`，控制器配置 SHA-256 `0d9d4479ec2e6a273ce79d2b269833aadfab7926fada11c7f02b03dd76785f66`，headless SHA-256 `37ff6a51c5292c1dc07e229b53fe5820caee9e7524468b53ae2ff377d1f1094c`。视频、GT、模型与板端环境同前述精确矩阵。
- 执行 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox <ROI> --output-video ... --output-boxes ... --session-metadata ...` 四组 car001 标准/右下、car002 标准/真值框。16 文件清单 `runs/screen-checksums.sha256` SHA-256 `d902055fd85f52274edbbaaee129c26aa601597b1050da26dea488fae2a72f90`，四 AVI 均 1920×1080、271 帧；文本包 `runs/screen-text.tar.gz` 主机/板端 SHA-256 `2c6c10af9e6ec5898e2e244de9b1aa9295888e3f84f12d5680eb36d600acdf8f`，评分 `/tmp/nt-dual-gt-evidence/early-freeze-07bdb37-screen-center.json` SHA-256 `bceb5ac6cefd9451871a9ed51aa1ac3f2cfe1d5675aaabeb13a55db25ccbfb9f`。四组与先前 600 ms 提前搜索结果一致：car001 重捕 187/200、跨框 tracking 中心 P95 37.21 px；car002 重捕 244/255、跨框 P95 26.49 px。
- 逐帧核查发现 `motion_acceleration_*` 仍按旧值外推；代码路径先 `clear_suspicion()` 清空 `recent_visibility_streak_`，再检查该计数是否需要冻结，导致 07bdb37 的冻结分支未执行。现将触发标志在清理前保存，并为非零加速度添加单测，实际运行 `cmake --build /tmp/nt-dual-gt-consistency-build --target test_long_term_controller --parallel 4` 和 `/tmp/nt-dual-gt-consistency-build/test_long_term_controller`，结果通过。**证据边界 / Handoff**：07bdb37 板端数据只证明冻结未执行时的退化，不代表修复后的效果；须重新提交精确源码并板端复测，正式 handoff HOLD。

## 2026-11-14T03:06:08+08:00 提前可见性搜索时冻结开环加速度的主机候选

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `9f7dec02227e9a9b5a4b0ddb79839170276bcf67`。恢复已测的连续三帧可见性搜索与 600 ms 最小重捕年龄；仅在该几何搜索触发时调用运动预测器 `freeze_open_loop_dynamics()`，保留当前速度、清除加速度。先前 car001 右下框第 171 帧后预测纵向速度反向，提前搜索到第 190 帧的中心 `y≈809`，真值 `y≈848`；旧正常搜索中心 `y≈827`。该候选检验开环加速度反向是否是早搜后的中心漂移主因；运行时不使用真值帧号或位置。配置 SHA-256 `0d9d4479ec2e6a273ce79d2b269833aadfab7926fada11c7f02b03dd76785f66`。
- 实际执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`；日志 `/tmp/nt-dual-gt-evidence/early-freeze-host-build.log`、`/tmp/nt-dual-gt-evidence/early-freeze-host-ctest.log`。
- **证据边界 / Handoff**：尚无板端推理结论。须 RK3588 精确源码原生构建、双目标八组真值评分；正式 handoff 保持 HOLD。

## 2026-11-14T03:04:05+08:00 提前搜索但延后 detector 探针的板端否决

- 精确源码 `codex/dual-gt-init-consistency@f53a235ccc22751a8a455c437cef5745d17cfd77`，包 `/home/neardi/Project/SOT/nt-dual-gt-probe-wait-f53a235.tar` 主机/板端 SHA-256 `029e2676a48fada4a7e9788681b1c7a437ce5c3e5be432cc09ff82b1f06ff9cc`，隔离根 `/home/neardi/Project/SOT/dual-gt-probe-wait-f53a235`。RK3588 原生构建及 CTest `23/23 PASS`；二进制 `build/nanotrack_v3` SHA-256 `5b035ad3c30338cf7ea8758028305afff76fb99754edff765b97156c1e1542df`，控制器配置 SHA-256 `02c1d19fe98902bb6d63564b276fd23d679960336099abf57cf32733350d882d`，headless 配置 SHA-256 `560ec4cc319bfb3a66938d3015d30c93d3744dd52c3a0b612908aabde1162ad8`。视频、GT、tracker、detector/manifest/labels 和板端环境同前述精确矩阵。
- 实际运行 `bash scripts/run_occ_init_matrix.sh ./build/nanotrack_v3 runs/headless-eval.conf runs/matrix`；32 文件清单 `runs/matrix-checksums.sha256` SHA-256 `de604d878d0b0341f7d6c2d0f619047ea5804c9f85517b8b6a068c0a23e0ffca`，八 AVI 均 1920×1080、271 帧；文本包 `runs/matrix-text.tar.gz` 主机/板端 SHA-256 `1a1cd2aa961d68ccf08b22520710de68332ae7b6cb91e0f653af8230fec27c33`，评分 `/tmp/nt-dual-gt-evidence/probe-wait-f53a235-center.json` SHA-256 `dda9dd5650c07c77bfd980607518c73c3d03d683648dbc89fec900c58d0bd148`。
- car001 标准框第 187 帧重捕但可见框门禁失败，右下框延至第 200 帧且 tracking 中心均值 `6.98 px`；跨初始框 tracking/display 中心 P95 `37.21/35.50 px`。car002 四组完全遮挡语义均 PASS，但重捕 `244/255/244/243`，跨初始框 tracking/display 中心 P95 `37.34/34.52 px`；正式门禁 `4/8 PASS`，与之前仅增加 600 ms 等待的失败候选近似。等待期内不探测 detector 候选未解决后续 recent/safe 命中不连续和晚期 detector 救援的问题。
- **裁决 / Handoff**：撤回 f53a235 的源码/配置/单测，保留上述板端证据；以 `1a7bd72` 的源码和 7cbf58d 的八组矩阵为最佳已测基线，正式 handoff 继续 HOLD。下一步应针对提前搜索造成的运动预测漂移和确认几何设计有独立证据的候选；不能把 4/8 语义 PASS 当成中心一致性达标。

## 2026-11-14T02:57:29+08:00 提前搜索但延后探针的主机候选

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `1a7bd72e80c19cd4084f495b06b08dfe39bd0d4b`。恢复已测的“轿车连续三帧高度缩小且中心残差增大即提前 SEARCHING”候选，并把可见配置的 `search_reacquisition_min_age_ms` 设为 600；新增默认关闭的 `search_probe_requires_reacquisition_age`，只在可见配置启用，使全目标模式在确认等待期间也不把 detector 候选作为 NanoTrack 探针。此前 600 ms 反例在等待期间仍探测遮挡中的同类车；本候选针对该差异。配置 SHA-256 `02c1d19fe98902bb6d63564b276fd23d679960336099abf57cf32733350d882d`。
- 新增单测确保 detector 候选在等待期间可以入表，但不能移动 NanoTrack 探针；已有连续可见性搜索负例随源码恢复。实际执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`；日志 `/tmp/nt-dual-gt-evidence/early-probe-wait-host-build.log`、`/tmp/nt-dual-gt-evidence/early-probe-wait-host-ctest.log`。
- **证据边界 / Handoff**：尚无该源码的 RK3588 推理结果；必须完成精确源码板端构建与双目标八组真值评价，若误重捕或中心退化则撤回。正式 handoff 保持 HOLD。

## 2026-11-14T02:53:58+08:00 保留竞争身份但限制远处探针的板端否决

- 精确源码 `codex/dual-gt-init-consistency@f1e0bffef970f3167da3ba026e876bd6661f5fae`，包 `/home/neardi/Project/SOT/nt-dual-gt-rival-f1e0bff.tar` 主机/板端 SHA-256 `a75b8a48813a38eb6d65daae49f8656b0c81c618848e997cdb0585af3efbc658`，隔离根 `/home/neardi/Project/SOT/dual-gt-rival-f1e0bff`。RK3588 原生构建及 CTest `23/23 PASS`；二进制 `build/nanotrack_v3` SHA-256 `8f8bd403708b8fb4e7ce7675e1a0c81674b053cd4543c9301053d468f24063d7`，控制器配置 SHA-256 `bb8c4758f6a0e6943445130c1aa7241aa5c9f228a881480aa2dabf573a863bfc`，headless 配置 SHA-256 `f5177478d9dca4ea0a3eb4abaabd2ee1ead14c764f4596c40325a09e54773705`。视频、GT、tracker、detector/manifest/labels 和板端环境同前述精确矩阵。
- 300 ms car001 四组：控制器配置 SHA-256 `cf0723e9bcefdba0f0cfc324b503eac80918bf084a6e82889ecbecafc14eda02`、headless SHA-256 `3aa3f9fd8da2e126fb118a95a00966af3894a5eb36fe5e577fe5d6a5d8487952`；执行四个 `./build/nanotrack_v3 --config runs/headless-detector300.conf --bbox <ROI> --output-video ... --output-boxes ... --session-metadata ...`。16 文件清单 `runs/detector300-car001-checksums.sha256` SHA-256 `a82b82141226ffae69a08cc82e21ada8f7cf9f1bf30f1a48891fc20b2036eda0`，四 AVI 均 1920×1080、271 帧；文本包 `runs/detector300-car001-text.tar.gz` 主机/板端 SHA-256 `cd33ac61479d2b60a5037551dcacfa0f9df605d7393a83b56b09d51351b5ac5c`，评分 `/tmp/nt-dual-gt-evidence/rival-f1e0bff-car001-300-center.json` SHA-256 `ff83a524625cbc27b31f4dfd8e89fd0184acc7f5279abd3b39403d18336dff9e`。标准框仍在第 179 帧误重捕并于 197 帧再重捕；跨框 tracking 中心 P95 `15.94 px`，正式门禁仅左上框 PASS。逐帧日志：第 175 帧远处 detector 候选进入，第 176–177 帧形成身份冲突；候选第 178 帧失效后，近期模板在 178–179 帧连续命中导致第 179 帧误确认。
- 400 ms 正式配置：运行 `bash scripts/run_occ_init_matrix.sh ./build/nanotrack_v3 runs/headless-eval.conf runs/matrix`，32 文件清单 `runs/matrix-checksums.sha256` SHA-256 `645829786799804955a3fbe7619acc988d8b7ee1b41a1bb8e784de6cc5b2d78a`，八 AVI 均 1920×1080、271 帧；文本包 `runs/matrix-text.tar.gz` 主机/板端 SHA-256 `7edd857aa8fb350b01d2594358e0fe0a78ab0cc8203a992737fde018dc4c3b83`，评分 `/tmp/nt-dual-gt-evidence/rival-f1e0bff-center.json` SHA-256 `275eb00c2c7d70faebf2a3a43fc6235aba4cfec966978989b7eeab16cef9e5ca`。八组重捕帧、中心均值、跨框 P95 与 7cbf58d 基线逐项一致：car001 `3.51 px`、car002 `6.21 px`，正式门禁 `1/8 PASS`。
- **裁决 / Handoff**：此位置门禁没有改善 400 ms 正式配置，300 ms 又退化，撤回 f1e0bff 的源码/配置/单测，保留板端输出和评价证据；正式 handoff 继续 HOLD。下一候选需针对失效后的近期模板独立确认，不能仅靠延长所有竞争候选的生存期。

## 2026-11-14T02:44:01+08:00 保留 detector 身份竞争证据的探针位置门禁主机候选

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `112ed22c3e059392424d3e9c71b35be972076325`。逐帧对照已保留八组 CSV 与双目标真值：正常可见 TRACKING 帧的 `tracking_bbox` 中心与 NanoTrack 原始中心几乎一致，`display_bbox` 中心亦随 tracking 中心；主要偏差来自遮挡后的错误或延迟重捕，而非显示框尺寸处理。
- 新增默认关闭的 `unified_confident_detector_probe_gate`，仅在 `visible-adaptive-display.conf` 开启。运动预测可信时，距离预测超过现有 `detector_candidate_max_center_ratio=0.02` 的纯 detector 候选仍留在统一候选表中作为竞争身份，暂不引导 NanoTrack 探针；与远处 detector 合并的近期/安全候选也不能确认。近处 detector 候选仍可探测。配置 SHA-256 `bb8c4758f6a0e6943445130c1aa7241aa5c9f228a881480aa2dabf573a863bfc`。
- 主机 RKNN-off 实际运行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，`23/23 PASS`；日志 `/tmp/nt-dual-gt-evidence/confident-rivals-host-ctest.log`。新增单测核对远处同类候选仍在表中否决近期误确认、不能牵引探针，近处候选仍可供验证。
- **证据边界 / Handoff**：这是依据两目标真值和先前板端反例形成的主机候选，尚无此提交的板端推理结果。需精确源码包在 RK3588 原生构建、完成 300/400 ms 初始框矩阵并评分；正式 handoff 保持 HOLD。

## 2026-11-14T02:37:46+08:00 连续可见性搜索候选板端否决并撤回

- `codex/dual-gt-init-consistency@e1830f8aa2ef37478ff9583bba1f16490dafa5a1` 精确源码包 `/home/neardi/Project/SOT/nt-dual-gt-visibility-streak-e1830f8.tar` 主机/板端 SHA-256 `d22a6d53dc7faa4614b8395177fa3f75154b21a1a85de802d5638789ecae7e3c`；隔离根 `/home/neardi/Project/SOT/dual-gt-visibility-streak-e1830f8`。RK3588 原生构建和 CTest `23/23 PASS`，二进制 `build/nanotrack_v3` SHA-256 `7dfe9c810edc89b10cc4579ef9dcc9815ae35fe33f6e274ef9013c57cd7cf89b`，控制器配置 SHA-256 `b490332d497a8d8bc1f7e6f1c088a0187b8e8c95bdd9641959311f7e6b1f23c4`，headless 配置 SHA-256 `bf593394d9eb134976dbfaf8030c3a5ebd18e9fc9ab724ece4c0ca9495c4ae8e`；视频、GT、模型、detector 身份同前述精确矩阵。
- 原始 `search_reacquisition_min_age_ms=400`：实际运行 `bash scripts/run_occ_init_matrix.sh ./build/nanotrack_v3 runs/headless-eval.conf runs/matrix`；八组 32 文件清单 SHA-256 `c779b18a814bfba4b96de810c3b53ed9227b0de312bb77f4e9353b4036658079`，八个 AVI 均 1920×1080、271 帧，文本包 `runs/visibility-streak-text.tar.gz` 主机/板端 SHA-256 `05439b2f2d2d21657ffa467406e13577dec5d94ed0aa1ce43ffea04df3dd1a17`，评分 `/tmp/nt-dual-gt-evidence/visibility-streak-center.json` SHA-256 `06663c83d92e9af1f7459f80f990602974c65da1576870407b5656fe4feb2acb`。car002 完全遮挡语义改善，但标准/右下在第 226 帧过早重捕并随后回滚；car001 右下第 183 帧过早重捕且再次于 201/224 重捕。跨框 tracking 中心 P95 car001 `3.51→40.39 px`、car002 `6.21→30.06 px`；正式门禁 `2/8 PASS`，中心一致性严重恶化。
- 同一二进制筛查 `search_reacquisition_min_age_ms=600`：板端控制器配置 `runs/minage600-controller.conf` SHA-256 `0d9d4479ec2e6a273ce79d2b269833aadfab7926fada11c7f02b03dd76785f66`、headless SHA-256 `f9e1da4092b3d2b46649b06582d5d28a40b6f789fd7663ca839b83f61edfc2c6`；运行 `bash scripts/run_occ_init_matrix.sh ./build/nanotrack_v3 runs/headless-minage600.conf runs/minage600-matrix`，32 文件清单 SHA-256 `e3a2795b843ace03340175a7bd60ef7ac287c8d5aac2ae66852302dd30a37a1d`，八个 AVI 均 1920×1080、271 帧。文本包 `runs/minage600-text.tar.gz` 主机/板端 SHA-256 `5149e3a0658759595ef25bfe3309c77e8079c5810ee0bc089f0e9323ac53eeba`，评分 `/tmp/nt-dual-gt-evidence/minage600-center.json` SHA-256 `b251e5c57182f6700fd7e910427fd284fc1182cfa411e43d96ae4c00f3a82caf`。car002 四组正式语义门禁虽 PASS，但重捕延至 `244/255/244/243`，跨框中心 P95 `37.34 px`；car001 右下重捕第 200 帧，标准框因可见框失败，跨框 P95 `37.21 px`。
- **裁决 / Handoff**：提前 SEARCHING 与单一固定重捕等待期不能同时保持遮挡语义、准确重捕和初始框一致性。撤回 e1830f8 的源码、配置和单测改动，保留两轮失败证据；继续以 `d2c0c22` 的双窗口响应 + 早期部分可见观测隔离为当前候选，正式 handoff 保持 HOLD。

## 2026-11-14T02:25:37+08:00 连续几何可见性下降触发搜索的主机候选

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `d2c0c22`。基于已核对的八组 CSV，car002 四种初始框从第 211 帧连续 10 帧符合“原始 decoded 高度相对可信框 `≤0.98` 且归一化中心残差 `≥0.04`”；car001 可见段没有连续三帧命中，右下框 168～172 部分/完全遮挡段有连续命中。新增可关闭的 `recent_visibility_search_consecutive_frames`，只对已有轿车双条件门禁连续命中计数，当前可见配置取 3；第 3 帧转 SEARCHING/MOTION_SEARCH，干净帧或帧间隔重置，初始化、异常回滚和清除可疑状态同步清理计数。`visible-adaptive-display.conf` SHA-256 `b490332d497a8d8bc1f7e6f1c088a0187b8e8c95bdd9641959311f7e6b1f23c4`，其他配置默认关闭；不以 GT 帧号或 detector 显示框作为运行时判据。
- 新测试覆盖三次连续双条件命中才搜索、干净帧打断计数；既有单条件/其他类别/缺 decoded 框负例保留。实际运行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`；日志 `/tmp/nt-dual-gt-evidence/visibility-streak-host-build.log`、`/tmp/nt-dual-gt-evidence/visibility-streak-host-ctest.log`。
- **证据边界 / Handoff**：只是依据当前双目标真值提出的板端反事实候选。需 RK3588 原生八组复测验证 car002 213～216 完全遮挡是否进入搜索，以及 car001 右下提前搜索、两目标重捕和中心是否退化；未批准 handoff。

## 2026-11-14T02:21:58+08:00 可信运动位置门禁板端否决并撤回

- `codex/dual-gt-init-consistency@f3765aa8c1ce38489f003c928dbe46f6ac548612` 精确源码包 `/home/neardi/Project/SOT/nt-dual-gt-position-gate-f3765aa.tar` 主机/板端 SHA-256 `a7dac31c288405ea997f6efdf8e1d937723b91b00c8596c3ca1a960e4ed45b5c`；隔离根 `/home/neardi/Project/SOT/dual-gt-position-gate-f3765aa`。RK3588 原生编译及 CTest `23/23 PASS`，`build/nanotrack_v3` SHA-256 `2a6d91cb3ff2af86ce84538b11ffeac0a47e8a46d1e69b48ea0cec1cf6bc54d2`。控制器配置 SHA-256 `b6738adf133c258da17aa849b6c04213e3379f6529fd580f24a199c718b66be0`，headless 配置 SHA-256 `e3c221eaa055dbe6d832f151dc33c0e4330c1e7addacfeda2b8089bbb9dae3f6`；视频、GT、tracker、detector 身份同前述精确矩阵。
- 先用板端 300 ms 反事实配置 `runs/detector300-controller.conf` SHA-256 `c730ac8f9eda129abb5e325cd61c0da8704c8449c0b4bd7685194cbb30ed3b9b`、headless SHA-256 `7fa5b2eb89fc8327b47124b2a5c1011bbf2953ea216b713143adc61ccf8074a8` 跑 car001 四组；16 文件清单 SHA-256 `ea159aad22fcf4a663473861882e57da9eadeba34924d1a1bef956d254921b6b`，四个 AVI 均 1920×1080、271 帧，小型文本包 `runs/detector300-car001-text.tar.gz` SHA-256 `96ceddacb457aa03831597552f562a8acf2c0228b42459f0b0c4583562e9741e`，评分 `/tmp/nt-dual-gt-evidence/position-gate300-car001-center.json` SHA-256 `d1039449e7074d89ca7283f19f0b5e68874ba04de2c156f16a01ea1869ca21f0`。car001 左上重捕提前至 186 且 PASS，右下提前至 188 但遮挡语义失败；标准框仍在 179 由 recent 模板误重捕，人工框延至 213，跨框中心 P95 `16.55 px`，故 300 ms 方案否决。
- 用原始 400 ms 配置实际执行 `bash scripts/run_occ_init_matrix.sh ./build/nanotrack_v3 runs/headless-eval.conf runs/matrix`；八组 32 文件清单 SHA-256 `a64e3f29dd8a77d2e029f36df206ee7f074421993808ca30de67fb32d351accd`，八个 AVI 均 1920×1080、271 帧；文本包 `runs/position-gate-text.tar.gz` 主机/板端 SHA-256 `6b4bba010a3abf3ae0b6efb7b2afdbc675958e1411700bb8f0612e4606f5fd2a`，评分 `/tmp/nt-dual-gt-evidence/position-gate-center.json` SHA-256 `fdd2e57ed2ec0b3b7cdbd34b51d2beb25c52f3af90c46e7cbbd4e5841d1bd340`。car001 标准框误重捕 `179/197/220`，人工框第 216 帧才重捕，右下第 204 帧，跨框 tracking 中心 P95 从基线 `3.51→29.54 px`；car002 中心小幅改善但仍遮挡语义失败。正式门禁 `0/8 PASS`。
- **裁决 / Handoff**：位置门禁简单过滤远处 detector 候选会去掉同类身份冲突的反证，反而放行 recent 模板误重捕。撤回 `f3765aa` 的控制器、配置与测试改动，保留失败日志和本记录；不能从该板端结果批准 handoff。后续需分别处理候选可验证性与竞争身份冲突，不能仅用全局位置阈值或 detector 请求频率修复。

## 2026-11-14T02:09:55+08:00 可信运动下的统一恢复 detector 位置门禁候选

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `b598e58`。`consume_detector_batch` 先前在统一恢复模式中无条件绕过 `detector_candidate_max_center_ratio=0.02`；板端 300 ms 筛查中 car001 第 176～179 帧错误候选距离预测位置 `0.036～0.038`，仍通过 detector + NanoTrack 两次命中误重捕。新增默认关闭的 `unified_confident_detector_position_gate`；仅运动预测可信时执行已有位置上限，失去预测置信度后仍允许远处候选经独立验证。`visible-adaptive-display.conf` 启用，配置 SHA-256 `b6738adf133c258da17aa849b6c04213e3379f6529fd580f24a199c718b66be0`。显示 detector 仍不直接授权 tracking/控制。
- 新控制器测试覆盖同类近候选通过、远候选拒绝、远候选不能成为后续探针，以及运动置信度过期时允许远处候选进入验证。实际运行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`；日志 `/tmp/nt-dual-gt-evidence/position-gate-host-build.log`、`/tmp/nt-dual-gt-evidence/position-gate-host-ctest.log`。首次无条件应用位置门禁导致 replay 夹具失败，已改为显式可选配置，默认行为及 replay 回归恢复通过。
- **证据边界 / Handoff**：当前仅主机逻辑证据；需用精确提交在 RK3588 上同时验证 400 ms 基线与 300 ms 参数反事实的双目标八组中心、重捕和遮挡。未经板端证明不批准 handoff。

## 2026-11-14T02:09:55+08:00 detector 300 ms car001 四初始框参数筛查否决

- 沿用 `codex/dual-gt-init-consistency@7cbf58d` RK3588 二进制 `3f6c3896f777148a0d038d8c02b5d21fefa7461cc649336bb538db3b1cdb15d4`；仅板端 `runs/detector300-controller.conf` 把 `detector_interval_ms=400→300`，SHA-256 `4b4f7254aa09e6dfe8f39f0d0e6e7898556a2f066ad5a59d0c9417b1c471013d`，headless 配置 SHA-256 `f16c0f0fcbd441e244bf808dab6fada8b21d6a2819512df65b5d2108b2833f7b`。四个 car001 ROI 的运行输出留在 `runs/detector300-car001/`，16 文件清单 SHA-256 `1c15710a2af1b93cd9b4915a9e046fdb0a49573054d141941b8dbe90099f4903`，四个 AVI 经 ffprobe 均 1920×1080、271 帧。小型文本包 `runs/detector300-car001-text.tar.gz` 主机/板端 SHA-256 `fd26e8d5cd4aea80b44407b881b88b8e8d6b178eefa526e28b3659bd0af09349`；同一初始化一致性脚本评分 `/tmp/nt-dual-gt-evidence/detector300-car001-center.json` SHA-256 `9a8b3c724d374a89d064373a89162d53bc4fcb4ec426a9a1808920b4a761093b`。
- car001 标准/人工/左上在第 179 帧发生遮挡区错误重捕，之后第 199/197/199 帧再次重捕；三组可见帧 tracking 中心均值 `27.94/28.25/28.45 px`，跨初始框 P95 `79.36 px`，均显著恶化。右下组重捕 190，中心均值 4.94 px。**否决**单独把 detector 间隔改为 300 ms；失效揭示已有统一恢复对远处同类 detector 候选的位置上限绕过，需以代码门禁反事实复测。

## 2026-11-14T02:01:22+08:00 car001 重捕最短等待时间筛查无收益

- 沿用 `codex/dual-gt-init-consistency@7cbf58d` RK3588 二进制 `3f6c3896f777148a0d038d8c02b5d21fefa7461cc649336bb538db3b1cdb15d4`，仅在板端 `runs/minage200-controller.conf` 把 `search_reacquisition_min_age_ms=400→200`，SHA-256 `b4427fd16a7ebecc90ed73be4b0276ffe268024cc3cf63639f094abc62ca4191`；headless 配置 SHA-256 `37a9dd8abb30863e9c1c6e29422f7e3bb0c139154d0228d4966dc3a4d3ac6f1e`。源码默认配置保持不变，输入身份与上一八组一致。
- 实际以四个 car001 ROI `992,582,37,39`、`993,585,36,36`、`990,580,39,41`、`994,584,35,37` 分别执行 `./build/nanotrack_v3 --config runs/headless-minage200.conf --bbox ROI --output-video ... --output-boxes ... --session-metadata ...`，输出保留在 `runs/minage200-car001/`，16 文件清单 SHA-256 `d4897a7d23f1a7d8a821b746823f482dc98db8c3d5a79f34bb187f21b4a7789f`；四个 AVI 经 ffprobe 均 1920×1080、271 帧。小型文本包 `runs/minage200-car001-text.tar.gz` 主机/板端 SHA-256 `8164e3937537b944eabbf9ff52c0410fc77fa059a3d156c116a442fd65ef3400`。同一初始化一致性脚本评分 `/tmp/nt-dual-gt-evidence/minage200-car001-center.json` SHA-256 `547da9018fc964fda1f5a310751e4ed0d718b4efa1a3c63077901792fc91751a`。
- 四组重捕仍为 `185/192/194/191`，tracking/display 中心、跨初始框 P95 与原 400 ms 配置相同；尽管标准框 CSV/AVI 哈希改变，评分无收益。**否决**缩短最短等待时间，保持 400 ms；car001 其余三组重捕时序仍需解决，不批准 handoff。

## 2026-11-14T01:57:03+08:00 uncertain_ms=100 参数筛查否决

- 沿用 `codex/dual-gt-init-consistency@7cbf58d` RK3588 原生二进制 `3f6c3896f777148a0d038d8c02b5d21fefa7461cc649336bb538db3b1cdb15d4`，仅板端 `runs/uncertain100-controller.conf` 将 `uncertain_ms=200→100`，配置 SHA-256 `f6c934ee0f20132e384cd4bcdb5c19be5d34b7563fd390a50ba90a7559ba8055`，headless 配置 SHA-256 `a5b3e28effb486177300487bc86b68745e89e764b5f3949513c88d5f866ac5b9`；源码默认配置不变。输入视频、GT、RKNN/detector 身份同上一八组。
- 实际命令 `bash scripts/run_occ_init_matrix.sh ./build/nanotrack_v3 runs/headless-uncertain100.conf runs/uncertain100-matrix`，八组 CSV/AVI/session/log 留在板端，32 文件清单 SHA-256 `cf3148d95b7bd7fb5c9b71240e97ffa7fc73e5a7337f849539911b32ad0af685`；八个 AVI 经 ffprobe 均为 1920×1080、271 帧。小型文本包 `runs/uncertain100-text.tar.gz` 主机/板端 SHA-256 `4a642e173e661cb0656cc7f0f924094a3cceee9fb41f973183da738e277b7245`。用 `scripts/evaluate_occ_initialization_consistency.py` 和同样八个 ROI 评分，`/tmp/nt-dual-gt-evidence/uncertain100-center.json` SHA-256 `3081be617760d7400aa7faee2fb75121721f2dc36676ae3c0296fec0bf0bbc7f`。
- car002 四组第 217～219 帧改为 SEARCHING/MOTION_SEARCH，但第 213～216 帧仍未满足完全遮挡语义。car001 标准框重捕 `185→193`，正式唯一 PASS 丢失；car002 左上出现额外第 144 帧重捕，右下重捕 `233→259`、可见帧 tracking 中心均值 `2.92→7.08 px`，car002 跨框 tracking 中心 P95 `6.21→46.09 px`。**否决**全局 `uncertain_ms=100`，保持源码配置 200 ms；本次不产生代码或 handoff。

## 2026-11-14T01:51:06+08:00 早期部分可见轿车门禁 RK3588 八组复测

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`；源码提交 `7cbf58dca735ad653747b5c7994fad09140987a9`，基线 `b04cdc7`。精确源码包 `/home/neardi/Project/SOT/nt-dual-gt-visibility-7cbf58d.tar` SHA-256 `e5fc4f45ef41befaf165f2c7849f2afc277337aba79f3b6973cf2f3df8c7367b`；隔离根 `/home/neardi/Project/SOT/dual-gt-visibility-7cbf58d`。RK3588 原生构建及 CTest `23/23 PASS`，`build/nanotrack_v3` SHA-256 `3f6c3896f777148a0d038d8c02b5d21fefa7461cc649336bb538db3b1cdb15d4`；控制器配置 SHA-256 `63007d32b58c7e4aa920762f4d73f3532ef8ee544e50101c0684914d9b470acc`，headless 配置 SHA-256 `5fdb1c5eadfec175545c5c36b0af15c66571ab673ead03da7a4f3d455e130219`。视频、GT、tracker、detector 输入身份沿用上一精确矩阵。
- 实际运行 `bash scripts/run_occ_init_matrix.sh ./build/nanotrack_v3 runs/headless-eval.conf runs/matrix`；板端 32 个 CSV/AVI/session/log 文件清单 SHA-256 `49b0c98430f8d145e4d60210d39b8f240ad8361de839917fa4836ea352f7414c`，八个 AVI 均经 ffprobe 确认为 1920×1080、271 帧。小型文本包 `runs/visibility-text.tar.gz` 主机/板端 SHA-256 `11ee0b033d92f7eec58488999d6d41f023dee9f5c915545af22df311a37cf1e4`；主机命令 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ... --output-json /tmp/nt-dual-gt-evidence/visibility-center.json`，评分 SHA-256 `cf516ab3a6a98bd3437a29c8d2ceb98e74a46a6c89c95b5dd5b04d273b9f691e`，八个 ROI 明细可由板端 `scripts/run_occ_init_matrix.sh` 重现。
- car001 四组中心、AVI 和重捕帧 `185/192/194/191` 与双时窗候选一致；car002 四组 15 个无框真值帧的误观测全部归零，误观测控制仅剩 211/212 和 217～219（右下组），右下可见帧 tracking 中心均值 `3.17→2.92 px`、最大 `23.62→10.69 px`，跨框 tracking 中心 P95 `7.67→6.21 px`。car002 真值/左上组第 233 帧重捕，右下第 233 帧重捕，标准框第 235 帧重捕；全部可见帧中心均值 tracking/display 分别为 car001 `4.39～5.02/4.43～5.05 px`、car002 `1.77～2.92/1.75～2.89 px`。
- **证据边界 / Handoff**：正式 full evaluator 仍仅 car001 标准框 `1/8 PASS`；car001 其余组重捕超过 `<190` 门槛，右下还有完全遮挡语义失败；car002 四组在第 217～219 帧仍发布 tentative/OBSERVATION，未满足完全遮挡应为 SEARCHING/MOTION_SEARCH 的门槛。当前视频的中心和一致性有改善，但门禁仍 HOLD，未获得跨场景标定或协调者 handoff；下步筛查短一些的 `uncertain_ms` 能否让遮挡切换提前而不损伤可见帧。

## 2026-11-14T01:43:16+08:00 早期部分可见轿车几何门禁主机候选

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `b04cdc7`。在 recent-safe 普通跟踪中新增可关闭的轿车原始框几何判据：原始 decoded 高度相对当前可信跟踪高度 `≤0.98`，且归一化中心残差 `≥0.04` 时，把该帧作为 provisional/SUSPECTED，禁止更新可信跟踪状态与模板；仅 `target_class_id=2`、具有 decoded 框且配置双参数启用时生效。`visible-adaptive-display.conf` 两参数 `recent_visibility_height_ratio=0.98`、`recent_visibility_center_residual=0.04`，SHA-256 `63007d32b58c7e4aa920762f4d73f3532ef8ee544e50101c0684914d9b470acc`。参数进入解析、校验与配置身份；其他配置默认关闭。该候选不消费 detector 显示框作为 tracking/控制恢复证据。
- 新控制器测试检查单独高度噪声、单独中心运动不触发，双条件使轿车观测 provisional，其他类别或缺少 decoded 框保持原有行为。实际运行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`；日志 `/tmp/nt-dual-gt-evidence/visibility-host-build.log` 和 `/tmp/nt-dual-gt-evidence/visibility-host-ctest.log`。下一步 RK3588 原生编译和双目标八组真值复测，重点观察 car002 第 211/212 帧、可见真值帧及 car001 重捕是否出现副作用。
- **证据边界 / Handoff**：阈值由这组真值视频的离线诊断提出，尚未有板端反事实结果或跨场景标定；不批准 handoff。

## 2026-11-14T01:40:58+08:00 双时窗响应门禁板端复测

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`；源码测试提交 `d34d0b9` 基于 `267e5b8`。源码包 `/home/neardi/Project/SOT/nt-dual-gt-dual-response-d34d0b9.tar` 主机/板端 SHA-256 `2e22e9238c4e7b2ebd8e2024050e9d676feeb802f38c0f20021b7bddb80b50a2`，隔离根 `/home/neardi/Project/SOT/dual-gt-dual-response-d34d0b9`。RK3588 原生编译及 CTest `23/23 PASS`；二进制 `build/nanotrack_v3` SHA-256 `849be5048b8085806d87f1741782a58be2ede88ed625f917357f5d95655ee0ee`，控制器配置 SHA-256 `1378c58fadd4a23b97bcc473ab06f69387d4402b8d3e44bc493f22065aed0b33`，headless 配置 SHA-256 `f5c2e7e63ccf2e0674eeb5d20c98e31f4779eb0ba78cc9257bdc704022240ffb`，同视频/RKNN/detector/manifest/labels 身份。
- 实际运行 `bash scripts/run_occ_init_matrix.sh ./build/nanotrack_v3 runs/headless-eval.conf runs/matrix`，八组 CSV/AVI/session/log 留在板端 `runs/matrix/`，32 文件清单 SHA-256 `b7e29503f8bbac935752b15ba3a04d2730d716bb61751703e638931ccb4859dc`，八个 AVI 经 ffprobe 均 1920×1080、271 帧。小型文本包 `runs/dual-response-text.tar.gz` 主机/板端 SHA-256 `91d24db90431cc3898eee235bd40bc10634e4da736b2f10288bc31c965612875`，主机评分 `/tmp/nt-dual-gt-evidence/dual-response-center.json` SHA-256 `efa2ebf6e5fc620f7c84feb16924114e59cf124827fe75eadd2e64882e61b840`。
- car001 四组 AVI 哈希与 `47ccfc8` 基线相同，重捕 `185/192/194/191`、中心误差与跨框一致性不变，避免了全局长窗口的回归。car002 标准/真值/左上三组 AVI 同基线；右下无框误观测 `14→2`（仅第 211、212 帧）、第 235 帧经两次验证重捕，但可见帧 tracking 中心均值 `3.05→3.17 px`、P95 `9.62→8.25 px`、最大 `10.61→23.62 px`（第 233 帧仍在搜索），car002 跨框 tracking 中心 P95 `7.13→7.67 px`。正式门禁仍仅 car001 标准框 1/8 PASS；car002 四组均有部分遮挡无框误观测。
- 初步独立几何线索：car002 四组第 211～212 帧的原始 tracker 高度相对上一可信高度缩小，同时中心残差上升；在当前八组 CSV 上，原始高度/可信高度 `≤0.98` 且归一化中心残差 `≥0.04` 可标出这两个无框帧，car001 四组可见帧未命中。但这只是当前视频的离线筛查，尚未证明能安全加入控制器；下一候选需以车类、原始框、双条件门禁实现并做负例/板端复测。
- **证据边界 / Handoff**：双时窗改善 car002 遮挡语义，但中心最大误差和正式门禁仍不达标；不批准 handoff。

## 2026-11-14T01:32:37+08:00 双时窗 PSR/APCE 候选主机实现

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `267e5b8`。`ResponseBaseline` 继续用配置的短窗口（板端当前 8 帧）评估原有响应/运动门禁，同时保存 3 倍长度的可信样本窗口；长窗口满样本后，只有 PSR 与 APCE 相对长参考 **同时** 下降才触发额外的 SUSPECTED 观测隔离。冻结、恢复、重捕清理同时作用于两份样本；没有使用 GT 帧号或 detector 输出直接授权跟踪/控制。新增基线单测覆盖短窗口已适应弱响应而长窗口仍可识别双指标下降、单指标下降不触发、冻结与重捕清理；控制器单测覆盖 24 次健康、6 次温和下降、单指标负例和双指标观测隔离。
- 实际运行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`；日志 `/tmp/nt-dual-gt-evidence/dual-response-host-build.log`、`/tmp/nt-dual-gt-evidence/dual-response-host-ctest.log`。下一步打包本提交源码到 RK3588 隔离目录，原生构建并运行同一八组双真值矩阵，重点核对 car001 第 62 帧模板时序和 car002 第 217～229 帧无框误观测。
- **证据边界 / Handoff**：当前只有逻辑/主机测试；真实目标中心、重捕与无框门禁必须以板端复测判定，不批准 handoff。

## 2026-11-14T01:29:22+08:00 24 帧响应参考窗口否决与双时窗判别候选

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，源代码/二进制仍为已测 `47ccfc8`，板端根 `/home/neardi/Project/SOT/dual-gt-stable14-47ccfc8`，二进制 SHA-256 `9d833901e044cd1b06db8d18e691d7aa60bcbd31c68ad46663c6a68502c0b4c2`，同视频与 RKNN/manifest/labels。只在板端派生 `runs/reference-24.conf` 将 `occlusion_reference_window=8→24`，配置 SHA-256 `42a9778a600e349eccb6981267707d8f14e08f116cc73706dac83dd8688c9193`，`runs/headless-reference-24.conf` SHA-256 `51b43a949458ffaaa70ad6bf319498374abc072a55880df0e616dbcbaa4662b0`，未改 Git 源码。
- 实际运行 `bash scripts/run_occ_init_matrix.sh ./build/nanotrack_v3 runs/headless-reference-24.conf runs/reference-24-matrix`；八组 CSV/AVI/session/log 留在板端，32 文件清单 SHA-256 `bd379649548bc3699865974c46efa1334bed74bee0ab11022e0af6244bb44074`。小型文本包 `runs/reference-24-text.tar.gz` 主机/板端 SHA-256 `eb0451911b526741849f46ac81d14ade3fefc36ec49932b7bf6d584e19df07fb`，评分 `/tmp/nt-dual-gt-evidence/reference24-center.json` SHA-256 `54ece8f7efe42c27ccaa28996906cf07411070adf70cb652d3bc2f3bfbbe1f2a`。
- car002 右下无框误观测 `14→2`，在第 234 帧重捕；但 car001 标准框 `185→195`，右下框变为第 `199/231` 帧两次重捕，tracking 中心均值 `5.02→11.98 px`，跨初始框 tracking 中心 P95 `3.51→38.70 px`，正式门禁 0/8。**否决全局 24 帧参考窗口**，保留 8 帧配置。逐帧对照显示，car001 右下在第 62 帧，PSR `6.11` 相对 24 帧参考 `8.28` 下降约 26%，而 APCE `18.54` 相对参考 `18.90` 基本不变；长窗口配合 peak-margin 跌落将这一正常可见帧判为 SUSPECTED，使模板 stage 从第 63 帧偏移。car002 右下第 217 帧的 PSR/APCE `4.76/12.89` 相对长参考 `7.94/18.71` 则同时下降，值得作为更具体的持续遮挡信号。下一候选考虑保留原 8 帧响应/运动门禁，另以长参考的 PSR 与 APCE **双指标同时下降**辅助判断；必须做主机负例和同八组板端测试。
- **证据边界 / Handoff**：双时窗只是依据当前两目标日志形成的假设，不能用真值帧号写入运行时规则。正式门禁未通过，不批准 handoff。

## 2026-11-14T01:22:35+08:00 搜索 detector 间隔 200ms 否决与响应基线定位

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，源代码/二进制仍为已测 `47ccfc8`，板端根 `/home/neardi/Project/SOT/dual-gt-stable14-47ccfc8`，二进制 SHA-256 `9d833901e044cd1b06db8d18e691d7aa60bcbd31c68ad46663c6a68502c0b4c2`；同视频与 RKNN/manifest/labels。只在板端从已提交配置派生 `runs/detector-200.conf`，把 `detector_interval_ms=400→200`，配置 SHA-256 `8608c4270cd5a1b2745a5e249097cdeaca3ba71fa21d3d04c2dce981aadd5204`，`runs/headless-detector-200.conf` SHA-256 `95fab9c7695610a54478f3e31ee24c777dd3bb69dd666ea3d2b1b3af1cd277f7`，没有改动 Git 源码。
- 实际运行 `bash scripts/run_occ_init_matrix.sh ./build/nanotrack_v3 runs/headless-detector-200.conf runs/detector-200-matrix`；八组 CSV/AVI/session/log 留在板端 `runs/detector-200-matrix/`，32 文件清单 SHA-256 `0589ecc59acc67ec00121c0e8d50843526961cf39364b1c8e3e50a38ab203be5`。小型文本包 `runs/detector-200-text.tar.gz` 主机/板端 SHA-256 `f94f8de3139fd74d91e66097ae3b091d00eb8306b688164a68b0dd97702cfcd3`；评分 `/tmp/nt-dual-gt-evidence/detector200-center.json` SHA-256 `f9bd9a9bdcc237312b8160b074e8c08982e5dbe1bcf87108c9f910fb2a569b64`。
- car001 左上框重捕 `194→188` 且正式门禁 PASS，但标准框重捕 `185→186`、tracking 中心均值 `4.39→4.83 px`、平均 IoU `0.751→0.708`；人工框仍在 192，右下仍在 191。car001 跨初始框 tracking 中心 P95 `3.51→13.32 px`，明显违背一致性目标；car002 四组中心及无框误观测无改善。**否决全局 200ms 间隔**，保留 400ms 的已提交配置。日志显示 car001 人工/左上框的正确 detector 候选通常在第 190 帧进入并经两次 NanoTrack 验证，而更早频率也可能让标准框在第 186 帧使用高度偏小的 detector 候选；不能仅用重捕提前判定改善。
- car002 右下框第 211～229 帧主要仍处于 TRACKING，15 个无框帧误报 14 帧。第 216 帧近期响应 PSR/APCE `5.21/13.78`，参考 `7.30/18.32`，两项尚未同时越过相对下降门槛；参考在连续弱响应期间快速下降，到第 222 帧已为 `5.16/13.39`，仅短暂进入 SUSPECTED 并立即清除。与 car002 标准框第 217～221 帧进入 SUSPECTED 的时序相对照，下一步筛查更长的可信响应参考窗口，检查能否减少无框误观测且不损害 car001。
- **证据边界 / Handoff**：200ms 频率仅为板端参数筛查，已否决；响应参考窗口仍是待证假设。正式门禁维持 1/8，不批准 handoff。

## 2026-11-14T01:13:46+08:00 14 帧模板组合板端结果与重复性

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，源码测试提交 `47ccfc8fe0ea2d677154d6483d52573f5555c1b3`，基线 `c16ddf6`；源码包 `/home/neardi/Project/SOT/nt-dual-gt-stable14-47ccfc8.tar` 主机/板端 SHA-256 `d7fcbaf0007f976811efa44b9ba25c627333381c7b8bdf20acaab28ffae7c03d`，板端隔离根 `/home/neardi/Project/SOT/dual-gt-stable14-47ccfc8`。RK3588 原生编译、CTest `23/23 PASS`，`build/nanotrack_v3` SHA-256 `9d833901e044cd1b06db8d18e691d7aa60bcbd31c68ad46663c6a68502c0b4c2`。控制器配置 SHA-256 `1378c58fadd4a23b97bcc473ab06f69387d4402b8d3e44bc493f22065aed0b33`，`runs/headless-eval.conf` SHA-256 `c15132f193e4c541565202f8c5f14c8f4ea265ae2264221519bfd4dcbfcdaca4`；视频、三个 NanoTrack RKNN、detector/manifest/labels 与前述核对身份相同，板端 Linux 6.1.84、RKNN API 2.1.0、driver 0.9.8。
- 实际运行 `bash scripts/run_occ_init_matrix.sh ./build/nanotrack_v3 runs/headless-eval.conf runs/matrix`；八组 CSV/AVI/session/log 留在板端 `runs/matrix/`，32 文件清单 SHA-256 `f0697c09e62ff740d6ecd54bbb3cac9c73613a4181a5e49bd4750bb8d55a28d2`，八个 AVI 经 ffprobe 均 1920×1080、271 帧。小型文本包 `runs/stable14-matrix-text.tar.gz` 主机/板端 SHA-256 `36a3475f6a989758dbb3e900523b3c39781433dcf49ed112b3098472aa7dad74`，主机评分 `/tmp/nt-dual-gt-evidence/stable14-center.json` SHA-256 `d37979b84999808b1e5f35b7354c71394dc54d8f4f720167b2892ce9c79c57fd`。
- 对比 `7f95652` 显示候选，同样配置/模型/初始框下 car001 四组 AVI 哈希完全相同，tracking/display 中心与重捕帧仍为 `185/192/194/191`，正式门禁仅标准框 PASS。car002 标准/真值框 AVI 哈希也相同；左上框 tracking 中心均值/P95 `2.52/8.34→2.17/6.16 px`，无框误观测 `6→2`，但单帧最大中心误差 `9.31→12.37 px`。右下框 tracking 中心均值/P95 `3.61/13.43→3.05/9.62 px`，最大 `15.72→10.61 px`，跨初始框 tracking/display 中心距离 P95 `10.62→7.13 px`；recent stage 从 228 推迟到 236 帧。car002 右下 15 个无框帧仍误报观测 14 帧且未重捕，正式门禁仍 FAIL；标准/真值/左上组也因遮挡语义等未全通过。轿车 display 中心仍基本跟随 tracking，未用外观修正掩盖 tracking 漂移。
- 用相同二进制、配置和 ROI 再运行 car001 右下与 car002 右下，输出位于 `runs/repeat/`，八文件清单 SHA-256 `9a3abc3754ad0a688315cd673a12f77b34312b5e7efe861bc456c6dd2b1168bf`。两组重复 AVI 与各自矩阵 AVI SHA-256 相同；逐帧比较 tracking/display 的 `x/y/width/height`、tracking phase、template action、state reason，在各 271 帧上差异数均为 0；CSV 原始哈希因运行时遥测字段而不同。
- **证据边界 / Handoff**：这是截至此轮中心一致性更好的板端候选，但正式门禁仍仅 1/8 通过，尤其 car002 部分遮挡无框帧误观测及 car001 非标准初始框晚重捕未解决；不批准正式 handoff。下一步需要针对这两个不同的失效原因改进身份/遮挡判别与重捕时机，不能仅凭均值改善宣布达标。

## 2026-11-14T01:05:14+08:00 连续可信帧 14 的模板试验候选

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `c16ddf6`，保留已在板端量测的轿车 display 中心改动。重新应用 `3c5324a` 中“可疑帧清零模板连续可信计数”，同时仅在本轮测试配置 `visible-adaptive-display.conf` 把 `stable_frames_for_update=15→14`，新配置 SHA-256 `1378c58fadd4a23b97bcc473ab06f69387d4402b8d3e44bc493f22065aed0b33`。上一轮 car001 第 49 帧可疑响应后至第 63 帧 stage 有 14 个可信帧；该组合旨在保留该 stage 时序，同时让 car002 第 222 帧可疑响应后的更新等待至目标重新完全可见。它仍只是实验假设，不代表 14 具备跨场景标定。
- 实际运行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`；日志 `/tmp/nt-dual-gt-evidence/stable14-host-build.log` 和 `/tmp/nt-dual-gt-evidence/stable14-host-ctest.log`。下一步在 RK3588 上用同模型、视频、八组初始框原生编译并核对 tracking 与 display 中心、模板 stage 时序、重捕及无框误观测；若 car001 再退化立即撤回。
- **证据边界 / Handoff**：参数与逻辑组合尚未经过板端测试，不批准 handoff。

## 2026-11-14T01:04:21+08:00 显示中心候选板端复测

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`；测试提交 `7f95652` 基于 `ff44961`。源码包 `/home/neardi/Project/SOT/nt-dual-gt-display-center-7f95652.tar` 主机/板端 SHA-256 `96cda00f1533c19accfcecd4425cd26dd6677ef169cac934e9b209b3e7b933fa`，板端隔离根 `/home/neardi/Project/SOT/dual-gt-display-center-7f95652`。原生编译和 CTest `23/23 PASS`，二进制 `build/nanotrack_v3` SHA-256 `57cc0cf06efd9ac0fd306293823a8d46a66a87eb77292594b05bd0c1a267992d`；控制器配置 SHA-256 `d6f9600a0a461c59bb1895711bb91c50c37d73b25eed64204dd1eb4225ff9853`，headless 配置 SHA-256 `69b153d18c4dfd5b0cdf5b741287be3acd69cb6fcf6b8d6b8dbd9f767b4f23f6`。视频、RKNN、detector、manifest、labels 与前述同身份。
- 实际运行 `bash scripts/run_occ_init_matrix.sh ./build/nanotrack_v3 runs/headless-eval.conf runs/matrix`；八组 CSV/AVI/session/log 留在板端 `runs/matrix/`，32 文件清单 SHA-256 `4ede66a040019ffe4c44240386cbb4521029dd4f47501b280da3790eec8a0f2c`；八个 AVI 经 ffprobe 均为 1920×1080、271 帧。小型文本包 `runs/display-matrix-text.tar.gz` 主机/板端 SHA-256 `ebc5d25a15c74901e4ad546879551cf0c9519c01eedfe6863f4ab83c3f282877`，主机评分 `/tmp/nt-dual-gt-evidence/display-center.json` SHA-256 `8f7054ebe80cf600bedfe1537237c29d552fbbac9feab7fe6cdde891cc1db465`。
- 相对 `78baddc` 同协议，car001 四组 display 中心均值 `5.21/5.26/5.42/5.75→4.43/4.65/4.49/5.05 px`（标准/人工/左上/右下），tracking 轨迹、重捕帧、正式门禁不变。car002 四组 display 均值 `3.83/3.80/3.31/3.41→2.47/2.07/2.52/3.61 px`（标准/真值/左上/右下）；右下 P95 `5.35→13.43 px`，car002 跨初始框 display 中心距离 P95 `5.94→10.62 px`，暴露原 tracking 后段漂移。car002 无框帧误观测和正式门禁均未改善。
- **证据边界 / Handoff**：轿车 display 中心改动已在板端证明，但 car002 右下尾部一致性变差；整体目标未达标，暂不批准 handoff。下一轮继续修正 tracking/template 的后段漂移，不使用 display 外观改善冒充 tracking 精度。

## 2026-11-14T00:56:37+08:00 轿车显示中心与跟踪中心一致候选

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `ff44961`。基于已保存的 `78baddc` 八组 CSV 离线构造显示中心与 tracking 中心相同、仍保留原 display 宽高的评分包 `/tmp/nt-dual-gt-evidence/display-center-probe/0.0/report.json` SHA-256 `68a6b0205011bbdf9ab4feb3f0bcb3e0fe8b160865fc8211aaaee33cda2faf7c`。四组 car001 显示中心均值由 `5.21/5.26/5.42/5.75` 降至与 tracking 相同的 `4.39/4.56/4.48/5.02 px`；car002 三组均值改善，右下组由 `3.41` 升至 `3.61 px`，但该组 P95 更差。离线 CSV 重写只是局部几何上限，不等于板端时序运行证据。
- 新候选 `AdaptiveDisplayBox` 对轿车 `class_id=2` 只从 detector 取显示框宽高，中心跟随 tracking；人物等其他类别沿用既有变换。初始化时若 detector 框有偏心，也用 tracking 中心放置其宽高。新增单测用偏心 detector 框核验初始化和两次确认更新均不改变轿车显示中心。实际运行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`。下一步按同一八组板端原生测试核对 display 与 tracking 中心、显示宽高、正式门禁和 detector 刷新时序。
- **证据边界 / Handoff**：该候选目前只有主机单测和离线几何模拟；tracking 重捕及 car002 无框语义不由本改动修复，正式 handoff 仍未批准。

## 2026-11-14T00:53:53+08:00 可疑帧重置模板计数板端否决

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`；测试提交 `3c5324a` 基于 `78baddc`。源码包 `/home/neardi/Project/SOT/nt-dual-gt-stability-3c5324a.tar` 在主机和板端 SHA-256 `a3bbd9db1039a0b6e8bead8e093eecdff4105dbacd2302461ca617a823bb231d`，隔离根 `/home/neardi/Project/SOT/dual-gt-stability-3c5324a`。原生编译、`ctest --test-dir build --output-on-failure` 为 `23/23 PASS`，二进制 `build/nanotrack_v3` SHA-256 `5c46aecca655f3d61dc7a940f30f66d2b12466535ba9d60132461a03691bdfe2`。控制器配置与上条相同 SHA-256 `d6f9600a0a461c59bb1895711bb91c50c37d73b25eed64204dd1eb4225ff9853`，`runs/headless-eval.conf` SHA-256 `f30422ecb2e49d46020bbd6e3c698683f55003619c7220f2be41aba0bf858334`；视频、RKNN 模型、manifest、labels 与上条同身份。
- 实际运行 `bash scripts/run_occ_init_matrix.sh ./build/nanotrack_v3 runs/headless-eval.conf runs/matrix`；八组 CSV/AVI/session/log 留在板端 `runs/matrix/`，32 文件清单 `runs/matrix/checksums.sha256` SHA-256 `fc92c69bdc526574fda9464cfdb19a321c7c5f0466fd47b1958183a9e0fbcb18`；八个 AVI 的 ffprobe 输出均为 1920×1080、271 帧。小型文本证据包 `runs/stability-matrix-text.tar.gz` 主机/板端 SHA-256 `b46a33096805f2cc2b6854ac820b1f74ac66b89d01dc92881297430709d0966d`，主机评分 `/tmp/nt-dual-gt-evidence/stability-center.json` SHA-256 `43e40575b06b02a4b0dd39157e7f2d80173c02b7cd05cc75cdfb41057d4f5ff9`。
- 与 `78baddc` 同协议比较，car001 标准/人工/左上/右下四组重捕帧从 `185/192/194/191` 变为 `197/187/194/195`；原本通过的标准框变为失败，跨初始框 tracking/display 中心距离 P95 由 `3.51/3.85 px` 恶化至 `24.34/22.34 px`。car002 右下框的 15 个无框帧仍误报 14 帧；该变更虽推迟该组 recent 模板的 stage `228→237`，却未改善主要问题。car001 第 49 帧的早期 SUSPECTED 也将遮挡前模板 stage 整体推迟一帧，改变第 162～166 帧的模板状态，说明全局清零有明显副作用。
- **裁决 / Handoff**：撤回 `3c5324a` 的 `publish_tentative` 清零和相应测试期望，保留本条原始板端证据；继续以方向保护的 `78baddc` 行为为基线。两目标两路中心和遮挡语义尚未同时通过正式门禁，不批准 handoff。下一步定位 car001 重捕期与 car002 部分可见期的具体身份/几何判别问题。

## 2026-11-14T00:45:26+08:00 方向保护板端复测与遮挡后模板稳定性候选

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，本轮基线 `78baddcb79d4ee456203c76e249f353a456f0a99`。该提交源码包 `/home/neardi/Project/SOT/nt-dual-gt-direction-78baddc.tar` SHA-256 `81d8ba6078f4e1a5f98af0c23a97443fe1c2a08d8888de334216c88461853141`；板端根 `/home/neardi/Project/SOT/dual-gt-direction-78baddc`。RK3588 原生编译及 CTest `23/23 PASS`，`build/nanotrack_v3` SHA-256 `1ab8a7ed287be7be6930af41dd4674bd6c9851d1a6c03b1c163ba2c708b4ad24`。控制器配置 `d6f9600a0a461c59bb1895711bb91c50c37d73b25eed64204dd1eb4225ff9853`，headless 配置 `2eff9fe55e38f0d46b0b68049153d5ff28c8ee46abf7d9675d43b9d3a161b3a4`。视频、三个 NanoTrack RKNN、detector/manifest/labels 沿用前述已核对身份；板端 Linux 6.1.84、RKNN API 2.1.0、driver 0.9.8。
- 板端运行 `bash scripts/run_occ_init_matrix.sh ./build/nanotrack_v3 runs/headless-eval.conf runs/matrix`，八组原始 CSV/AVI/session/log 位于上述根的 `runs/matrix/car001_nominal/` 等目录；32 文件清单 SHA-256 `047e49a226a599e77b02d7d9618d78b4a52a86d15405b8ccdf041b178b2df5225`，八个 AVI 经 ffprobe 均 1920×1080、271 帧。主机评分 `/tmp/nt-dual-gt-evidence/direction-center.json` SHA-256 `4d1a7a13e46e1509c023627dd29269cf3e9d435cc8caee328f7c`。car001 右下初始化重捕由 193 提前至 191 帧，最大可见 tracking 中心误差 44.16→21.03 px，跨初始框最大中心距离 43.05→19.48 px；car001 其余组与 car002 四组无实质改善。car001 仅标准框通过正式门禁；car002 四组仍失败，右下初始框在 15 个无框标注帧中误报观测 14 帧。car001 右下三次和标准框两次额外重复测试位于 `runs/repeat/`，20 文件清单 SHA-256 `2a6ccbd3335535d6c5a29c62fd7b14d280da62bfe0688989da1135451c7f09cf`；相同初始化的中心轨迹及重捕帧一致。方向保护有局部改善但不足以交付。
- 在相同二进制下筛查 `search_local_late=2.0`：控制器配置 SHA-256 `7c9f173f451ecb57c7141d732c17214027492d8990e7433bcf9a637666dbaab9`，headless SHA-256 `89024fd267ea1742a4d59be6ecfb171fd0f5e833086da4f9548f2141208bf7df`，八组输出 `runs/search-late2/` 的 32 文件清单 SHA-256 `bf214557f3b53d9cfa73183edcc6ffc18617d8590a80dc78515b7e072e84b19a`；评分 `/tmp/nt-dual-gt-evidence/search-late2-center.json` SHA-256 `1cd60e80c04e4c23684c36d5624992c2db4540567c854f7da04023766ca252da`。car001 左上重捕虽提前，但标准框/人工框可见框门禁恶化；拒绝该参数。`search_local_late=2.5` 控制器配置 SHA-256 `e157fa1504f6fb182b89a9a50fa7a5930268f1b9bbae7fc486f5d76f4c8bbcca`，headless SHA-256 `29d5efaa425eeaffe1d1012d2f4844b62a0847651eeb9e52ed6e907180208e06`，car001 四组位于 `runs/search-late25/`；评分 `/tmp/nt-dual-gt-evidence/search-late25-car001.json` SHA-256 `2301ded8e110b32a6d7ce8632074228818afbf53ee00035c18ba5e9b9437ed17`，标准框退化至重捕 188 帧且可见框失败，跨框 tracking 中心 P95 达 41.56 px；同样拒绝。
- 新候选在 `LongTermController::publish_tentative` 清零 `stable_frames_`，使可疑响应打断 recent 模板更新的连续可信帧计数。car002 右下框在遮挡期间出现 SUSPECTED 后仍很快 STAGE/COMMIT recent；该改动旨在避免用短暂恢复的受遮挡外观更新模板，不能把无框帧的部分可见目标直接当成 GT 框。更新 transient/probation 单测，核验必须重新积累四个可信帧才允许 STAGE。实际运行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`。本候选的板端原生构建、八组中心评分尚待完成；生成文件不入 Git。
- **证据边界 / Handoff**：仅方向保护的板端证据已取得，新模板候选仍待实测。两目标中心与无框语义尚不达标，不批准正式 handoff。下一步板端复测新候选并核对 car002 模板时序、中心偏差及 car001 退化风险。

## 2026-11-14T00:23:04+08:00 匀速搜索回退与方向反转保护候选

- `codex/dual-gt-init-consistency@8d43554` 的源码包
  `/home/neardi/Project/SOT/nt-dual-gt-coast-8d43554.tar` 主机/板端
  SHA-256 `e33fbdb858981244ea3f82c009fb6404c5cc298320807295af1a50ffe7b24667`，
  工作树 `/home/u20/single-object-tracking`，板端隔离根
  `/home/neardi/Project/SOT/dual-gt-coast-8d43554`。RK3588 原生编译与
  CTest `23/23 PASS`，`build/nanotrack_v3` SHA-256
  `b4499b5794baf83d88be614e61bfc2628cc6ca5146dcbbb400608db5bdb2a4b6`。
  控制器配置 SHA-256 与上条相同
  `d6f9600a0a461c59bb1895711bb91c50c37d73b25eed64204dd1eb4225ff9853`，
  `runs/headless-eval.conf` SHA-256
  `2db756b777fe4fb916e4f374e29f245eb276fde75f7aaa491f4bc3412c42d199`；
  视频、三个 NanoTrack RKNN、detector/manifest/labels 沿用前述已核对身份。
- 实际运行 `bash scripts/run_occ_init_matrix.sh ./build/nanotrack_v3
  runs/headless-eval.conf runs/matrix`，八组 CSV/AVI/session/log 留在
  `runs/matrix/<场景>/`；32 文件清单 SHA-256
  `0c5d79d43a0f6c81d4e96a04c752bc10c24bc2e3a722e19f424f896899a0ee63`。
  八个 AVI 经 ffprobe 均为 1920×1080/271 帧，主机真值评分
  `/tmp/nt-dual-gt-evidence/coast-center.json`。car001 四组重捕从旧版
  `185/192/194/193` 变为 `192/192/186/191`；右下框跟踪中心均值
  `5.73→5.18 px`，但原本通过的标准框晚重捕而失败，左上框可见框门禁
  新失败，跨初始框 tracking 中心距离 P95 `3.51→6.73 px`。
  car002 四组的跟踪中心均值、遮挡误观测及正式门禁未改善。因此撤回
  `MotionEstimator::begin_search` 的无条件匀速外推改动及相应测试。
- 新候选基于本条源代码继续修正：仅当搜索当前速度与最后接受的趋势
  点积为负时，恢复最后接受的速度，并清除其加速度；不再要求已触发
  单独 motion freeze。car001 右下框在第 165 帧的已接受速度 y 分量
  `+15.58 px/s`，搜索前翻转为负，而标准框没有方向反转；该条件旨在
  只处理这类开环反向。`test_long_term_controller` 新增无 motion freeze 的
  反向回退测试，主机实际运行
  `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、
  `ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，
  RKNN-off `23/23 PASS`。板端原生及两目标八组复测待完成。
- **证据边界 / Handoff**：方向反转保护仍只是主机候选，尚不能声称改善
  RK3588 目标中心或通过正式门禁；不批准正式 handoff。

## 2026-11-14T00:15:30+08:00 轮询时机板端实测与搜索匀速预测候选

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`。
  轮询时机提交 `9e93f9dd9dec35c34ddebf5f88443c7588db7046` 的源码包
  `/home/neardi/Project/SOT/nt-dual-gt-poll-after-9e93f9d.tar` 主机/板端
  SHA-256 都为 `f1fcb0521392c72ea669c2a9ad73ddb1836659e4eca24b84ec9928e45001b44c`。
  RK3588 隔离根 `/home/neardi/Project/SOT/dual-gt-poll-after-9e93f9d`，
  原生 RKNN 编译与 CTest `23/23 PASS`；`build/nanotrack_v3` SHA-256
  `5d21eec75f2b95a6878e1970459b413338491c82c0beebd4668dd46c8b767fc4`。
  显示控制器配置 SHA-256
  `d6f9600a0a461c59bb1895711bb91c50c37d73b25eed64204dd1eb4225ff9853`，
  `runs/headless-eval.conf` SHA-256
  `8487250b6b156922dc47899d1b80cacf6299cd53f6b660623d94724938504259`。
- 实际运行 `bash scripts/run_occ_init_matrix.sh ./build/nanotrack_v3
  runs/headless-eval.conf runs/matrix`，保留 `runs/matrix/<场景>/` 的
  CSV/AVI/session/log；32 文件清单 `runs/matrix/checksums.sha256` SHA-256
  `bb1a84539b89f1e1c58476f7d56a1b890e5c2af6cc6448db6fd30bd3e173f8cd`。
  八个 AVI 经 ffprobe 均为 1920×1080/271 帧，主机真值评分
  `/tmp/nt-dual-gt-evidence/poll-after-center.json`。与上一轮最终 `alpha1`
  对照，八组 tracking 中心均值、重捕帧、正式门禁均未改善；
  car001 重捕仍为 `185/192/194/193`，car002 标准/真值框仍为 `235/235`，
  car001/car002 tracking 跨初始框中心距离 P95 仍为 `3.51/10.62 px`。
  因此同帧消费能力已由单元测试证明，当前板端视频尚无跟踪改善证据。
- 新一轮候选以 `9e93f9d` 为基线：`MotionEstimator::begin_search` 首次进入
  无观测搜索时清除加速度和尺度速度，保持最后已限幅速度做匀速预测，
  避免最后一帧的噪声加速度在开环搜索中持续放大。`test_controller_support`
  增加带非零已学习加速度的测试，核验入搜索后的匀速外推。主机实际运行
  `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4` 与
  `ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，
  RKNN-off `23/23 PASS`；RK3588 原生与双目标八组测试尚待同提交复验。
- **证据边界 / Handoff**：无观测匀速预测仍需确认不会损害快速转弯目标、
  遮挡期身份门禁及 car002 无框帧；不批准正式 handoff。

## 2026-11-14T00:08:27+08:00 Detector 轮询时机主机候选

- `codex/dual-gt-init-consistency`、工作树 `/home/u20/single-object-tracking`，
  基线 `3c6201d3686988d8724d4eb08a7fe3f6a19a0925`。同一帧中，
  `LongTermRuntime::process_frame` 现先做 NanoTrack 推理，再无等待地轮询
  detector，随后将批次交给控制器；这允许推理期间完成的批次参与当前帧决策。
  新测试让 fake detector 在 NanoTrack 推理回调里完成任务，核验同帧消费，
  且 display 专用批次仍不进入跟踪/控制恢复。两次 NanoTrack 身份确认、
  候选时效和其他控制门禁未改。
- 主机实际命令 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、
  `ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，
  RKNN-off `23/23 PASS`。此条只证明主机逻辑和回归；真实 RK3588 原生编译、
  八组双真值中心评分及重复性尚待同一提交的源码包上验证。
- **证据边界 / Handoff**：该候选尚不批准生产或板端 handoff。下一步从本提交
  `git archive` 到隔离板端目录，按相同二进制/模型/视频/初始框的协议
  对照旧 `alpha1` 八组及 car001 右下重复运行。

## 2026-11-14T00:05:00+08:00 搜索间隔归因修正与同参数重复性

- 继续使用 `codex/dual-gt-init-consistency@ecfa29f6810b8a6610bb551ccdd704b531a5e5fc`，
  工作树 `/home/u20/single-object-tracking`，板端隔离根
  `/home/neardi/Project/SOT/dual-gt-23a43db`。沿用上一条已核对 SHA-256 的
  可执行文件、RKNN 模型、detector、输入视频和 `runs/alpha1-controller.conf`。
- **修正上一条参数归因**：源码搜索发现 `search_probe_interval_ms` 只在配置解析、
  指纹/元数据和验证中出现，运行时没有用它调度探测。所以上一条
  `runs/probe33/` 的右下框晚重捕不能归因于 `100→33 ms` 参数变化，
  该参数目前没有运行时效果。重跑同一 `runs/probe33-headless.conf`、
  同一 `994,584,35,37` 初始框两次，分别第 `193/193` 帧重捕；首次
  `runs/probe33/car001_right` 则是第 `205` 帧。两次重复结果留在
  `runs/repeat-probe33/car001_right_<1|2>/`，8 文件清单 SHA-256
  `5de54774a11ef27bcd5256234c7eb5928fe1d6a8a1997b5e64e957f88859140a`，
  主机评分 `/tmp/nt-dual-gt-evidence/repeat-probe33-car001-right.json`。
  首次 detector 第 `188` 帧请求的批次到第 `190` 帧才被消费，
  `detector_total_ms=124.787`；重复一次在第 `189` 帧消费，
  `detector_total_ms=51.056`。这是观测到的时序关联，尚未证明唯一因果。
- 用同一最终 `runs/alpha1-headless.conf` 和相同右下框再连续重复三次，
  `runs/repeat-alpha1/car001_right_<1|2|3>/` 的 12 文件清单 SHA-256
  `04b42866fa509321ff72f5ba144b8d448a612955e6a43fef5b78eb284169eb29`，
  主机评分 `/tmp/nt-dual-gt-evidence/repeat-alpha1-car001-right.json`。
  加上首次，共四次 tracking 中心轨迹逐帧相同、均第 `193` 帧重捕；
  display 受异步检测刷新轻微影响，四次同帧最大成对中心距离 P95
  `1.35 px`。这说明当前基准下本例重现稳定，但不能排除偶发晚重捕。
- 对 car001 四个初始框还筛查了限制运动加速度：
  `motion_max_acceleration_diagonals_per_s2=2.0→0.02`，隔离配置/headless
  SHA-256 分别为
  `448653a89a5bd435640153b8dc136e76f71cc0dbb756ea8d4e3526a281eb3a44`、
  `b34c453a170628f23614b8de460e554c6255349fea7dff7978321ac46d5c2683`；
  结果留在 `runs/accel002/<场景>/`，16 文件清单 SHA-256
  `0c4d89f29668218482960eefdda100e68aa84712b34466e44ab0c6ae452df165`，
  主机评分 `/tmp/nt-dual-gt-evidence/accel002-car001.json`。
  四组重捕变成第 `192/197/196/198` 帧，原标准框 PASS 也失败，
  tracking 最大成对中心距离 P95 `3.51→15.93 px`，拒绝纳入。
  单次确认配置被控制器以 `recovery confirmation policy must be two of three`
  拒绝加载，未运行；保留两次 NanoTrack 确认身份约束。
- **证据边界 / Handoff**：最终分支的显示参数改善两路可见帧中心贴合，
  但 tracking 的晚重捕、car002 遮挡误观测和检测异步时序敏感性未解决。
  下一步需要在保持两次 NanoTrack 确认的前提下，针对候选到达时序做
  可重复的控制器/运行时实验；正式 handoff 仍不批准。

## 2026-11-13T23:56:00+08:00 双路中心真值评分及显示重捕渐变对照

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，
  算法源码基线 `23a43db95c4bb8b3c6ba4090f077fd696d36d951`，新分支从
  `main@b80226577954febe22c5eff168125f77a6cdc724` 开始。源码包
  `/home/neardi/Project/SOT/nt-dual-gt-final-v3-source.tar` 主机/板端 SHA-256
  `b5ae7f07b447fb4465e46120f11db82d34490e493eb15b72b6ed8613ed5b0d99`。
  板端隔离根 `/home/neardi/Project/SOT/dual-gt-23a43db` 原生编译与 CTest
  `23/23 PASS`，`build/nanotrack_v3` SHA-256
  `f2b2da43e7a56b5fbc7c39bdd297b125f184d849ee02ee8ed65d189cba31fa02`；
  本次配置/评分改动在主机 RKNN-off 重构建与 CTest 亦为 `23/23 PASS`，
  中心指标单元测试 `2/2 PASS`。板端 Linux `6.1.84`、RKNN API `2.1.0`、
  driver `0.9.8`。
- 输入仍是上一条逐项记录 SHA-256 的 `data/occ_video_1/1.mp4`、三个 NanoTrack
  RKNN、VisDrone9 detector/manifest/labels 与两目标正式真值。板端原配置
  `rk3566/nanotrack_v3/config/visible-adaptive-display.conf` SHA-256
  `2adf4b35587215c18fcaf86221dd36f4c44fbf25910572a7a53c486235943d5e`，
  `runs/headless-eval.conf` SHA-256
  `dade127e93ab5a6b2f939397209603e663fbcb4df38d62cdcf0051e1de5ff659`。
  实际运行 `bash scripts/run_occ_init_matrix.sh ./build/nanotrack_v3
  runs/headless-eval.conf runs/final`，八场景结果在 `runs/final/<场景>/`，
  `runs/final/checksums.sha256` 覆盖 32 个 CSV/AVI/session/log，清单 SHA-256
  `447b7da068db1cb4e65dfc3c989effa9f352669792f79f9bc6e725ef72ff3752`。
  八个 AVI 经 ffprobe 均为 1920×1080/271 帧；主机 JSON
  `/tmp/nt-dual-gt-evidence/final-23a-center.json`。
- `scripts/evaluate_occ_initialization_consistency.py` 同时记录 tracking/display/control
  可见帧中心误差（像素，平均/P95/最大及最差帧）与多初始框同帧最大成对中心距离；
  对应单元测试在 `tests/test_evaluate_occ_initialization_consistency.py`。
  原配置 car001 四组 tracking 平均中心误差 `4.39/4.56/4.48/5.73 px`，
  display `5.41/5.53/5.71/6.86 px`；四组最大成对中心距离 P95
  tracking `3.51 px`、display `18.53 px`。car002 四组 tracking
  `2.49/2.05/2.52/3.61 px`，display `4.06/3.98/3.43/3.55 px`；
  最大成对中心距离 P95 tracking `10.62 px`、display `6.65 px`。
- 只把 `display_reacquisition_blend_frames=5` 改为 `1`，板端隔离控制器配置
  `runs/blend1-controller.conf` SHA-256
  `42f06ce988467d0aa78f4e3fc5b4491030cbd78c693a1b7e15c25808358e8a99`，
  headless 配置 `runs/blend1-headless.conf` SHA-256
  `341e7dcaddcdc4e7cacf88d7526c2973c6e617b86fd77c42a495062fb3841638`；
  同命令换成 `runs/blend1-headless.conf runs/blend1` 复测八组，
  `runs/blend1/checksums.sha256` SHA-256
  `e1eaf5677e902ca4561f56855e8f18ce3a1a6652ec50f2d75028da69b35ce827`，
  八个 AVI 经 ffprobe 均为 1920×1080/271 帧；主机 JSON
  `/tmp/nt-dual-gt-evidence/blend1-center.json`。car001 display 最大成对中心
  距离 P95 `18.53→3.71 px`，car002 `6.65→5.39 px`；tracking 轨迹、
  重捕帧与失败门禁未变。
- 再以渐变 1 帧配置为基线，把 `display_center_smoothing_alpha=0.80→1.00`；
  板端 `runs/alpha1-controller.conf` SHA-256
  `d6f9600a0a461c59bb1895711bb91c50c37d73b25eed64204dd1eb4225ff9853`，
  `runs/alpha1-headless.conf` SHA-256
  `34892aa163b5101b10d9a797a97c3adeed8aa2c660555c08719a9d84c69dacc5`。
  同命令换成 `runs/alpha1-headless.conf runs/alpha1` 复测八组，
  `runs/alpha1/checksums.sha256` 覆盖 32 个 CSV/AVI/session/log，SHA-256
  `5f511edbc5570f1a6c24eb15c21b8657e7c772575301cb47ef473bdd81a08d20`；
  主机评分 `/tmp/nt-dual-gt-evidence/alpha1-center.json`。八组 display 中心
  平均误差均下降：car001 `5.49/5.50/5.65/6.58→5.21/5.24/5.44/6.31 px`，
  car002 `3.95/3.90/3.38/3.48→3.83/3.78/3.31/3.32 px`。
  display 最大成对中心距离 P95 car001 `3.71→3.85 px`、car002
  `5.39→5.85 px`，属轻微一致性代价；八组 tracking/control 框、状态、重捕、
  observation_valid 逐帧相同。两项显示改动均已写入本分支显示配置，最终主机配置
  SHA-256 与 `runs/alpha1-controller.conf` 相同。
- 另做四组 car001 的未采纳搜索参数筛查：只把
  `search_probe_interval_ms=100→33` 的隔离配置 SHA-256
  `8bfce3c6378f7145c44e5ab9a0ec368c5b61171a44d86e036b9ea396a2f65d4d`，
  headless SHA-256 `aded48547d05128e7f709ad11325344a8bebb28c3984d0cdcf96067da897ed09`；
  输出 `runs/probe33/<场景>/` 的 16 文件清单 SHA-256
  `d61b6201c153a1ab5533a4de2d99de28c6fa0cab6d915800f2a34d2b26af45ea`，
  主机评分 `/tmp/nt-dual-gt-evidence/probe33-car001.json`。右下框重捕
  `193→205` 帧、tracking 均值中心误差 `5.73→8.35 px`、四组最大成对
  中心距离 P95 `3.51→43.05 px`，故拒绝。
  另在 display 渐变 1 帧基础上把 `suspicion_enter_ms=100→33`、
  `uncertain_ms=200→100`，隔离配置/headless SHA-256 分别为
  `a33e6a6dbfd7e4180078acc307a723736984279e1032b86e25bd304b`、
  `76e1c17e1c720a6ad4fad3e6bb86da76f411ffccf349c2ee7d2a4054e1700381`；
  `runs/early-suspicion/<场景>/` 的 16 文件清单 SHA-256
  `c963928c8bed5862ce101e19094c1e3cb13493314de855182bdb69a2c6d1db28`，
  主机评分 `/tmp/nt-dual-gt-evidence/early-suspicion-car001.json`。标准/左上框
  第 `70/76` 帧误重捕且平均中心误差 `52.38/54.72 px`，故拒绝。
- **证据边界 / Handoff**：car001 右下初始框第 192 帧 tracking 中心误差
  `44.16 px`，第 193 帧才重捕；car002 第 233 帧跨初始框 tracking 中心
  最大距离 `26.64 px`，15 个 no-bbox 帧仍有 `3/2/6/14` 次误观测。
  完整评估仅 car001 标准框 PASS；其余七组未通过正式门禁。此分支只保留
  改善显示中心一致性的候选，不批准正式 handoff，不合入 `main`/`Debug`；
  下一步继续验证跟踪中心偏移的搜索探测与遮挡语义。

## 2026-11-13T23:32:00+08:00 搜索期显示隔离板端复测与怀疑期回归

- 本轮被测源码 `codex/dual-gt-init-consistency@2dbffbb6cbe4817a06a2c903b3c34fc9512662c6`，
  基线为前一候选 `e1e1a535c6acfa925ea234e7007f96fac3a5cbd3`，工作树
  `/home/u20/single-object-tracking`。源码包 `/home/neardi/Project/SOT/nt-dual-gt-final-source.tar`
  主机/板端 SHA-256 `7707bb20b6fecef9fd7a3552604751821aaad6e59085eaa14b35c6e25bd88605`。
  RK3588 隔离根 `/home/neardi/Project/SOT/dual-gt-2dbffbb` 原生 RKNN 编译、
  CTest `23/23 PASS`；`build/nanotrack_v3` SHA-256
  `4f72c2e069e118c667236e0d8c4254fc9510c065e51403d2218a2961269cacf8`。
  实测 Linux `6.1.84`、RKNN API `2.1.0`、driver `0.9.8`。
- 输入 `data/occ_video_1/1.mp4` SHA-256 仍为
  `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；
  板端重算 NanoTrack template/search/head RKNN 及 VisDrone9 detector/manifest/labels
  均与下条 `e1e1a53` 记录的六个 SHA-256 完全相同。
  `rk3566/nanotrack_v3/config/visible-adaptive-display.conf` SHA-256
  `2adf4b35587215c18fcaf86221dd36f4c44fbf25910572a7a53c486235943d5e`；
  `runs/headless-eval.conf` SHA-256
  `7190b79337e35fe317606b0489d9295a300aa8d0782240c972c5bbb980cad48a`。
- 实际运行 `bash scripts/run_occ_init_matrix.sh ./build/nanotrack_v3
  runs/headless-eval.conf runs/final`，沿用下条八个 ROI 与两目标真值。八个 CSV 各
  271 数据行，八个 AVI 经 ffprobe 均为 1920×1080/271 帧；全部 32 个 CSV/AVI/
  session/log 留在 `runs/final/<场景>/`，`runs/final/checksums.sha256` 的 SHA-256
  为 `c474042f5704cc06ab81d3543102f094d4fd36efa2813441edc37bf43596b43b`。
  主机逐帧评分 `/tmp/nt-dual-gt-evidence/final-2dbffbb.json`。
- car001 四组显示均值 IoU 为 `0.740/0.737/0.731/0.708`，car002 为
  `0.707/0.714/0.722/0.715`（顺序同矩阵）；跟踪与控制框相对 `e1e1a53`
  逐帧完全相同。完整门禁仍仅 car001 标准框 PASS；其余 car001 第
  `192/194/193` 帧重捕，car002 四组均有遮挡语义失败。car002 在 15 个正式
  no-bbox 帧上，四组分别有 `3/2/6/14` 帧仍报告 `observation_valid=true`；
  该数量已加入 `scripts/evaluate_occ_initialization_consistency.py` 的诊断字段，
  不把这些帧算普通 IoU。
- 可见帧回归：把 SUSPECTED 与 SEARCHING 都保留旧 `.35` 时，car001 标准框和
  用户框最差显示 IoU 从上条 `.80` 的 `0.537/0.602` 降到 `0.400/0.420`，
  主要发生在第 49 帧目标快速运动且 SUSPECTED 的阶段。故本条后的源码改为
  TRACKING/SUSPECTED 用 `.80`，仅 SEARCHING 用旧 `.35`；主机 RKNN-off
  重构建及 CTest `23/23 PASS`，板端复测待完成。
- **证据边界 / Handoff**：显示中心改动不修复 tracker 的初始尺度继承、晚重捕或
  car002 遮挡期误观测。当前仍为新分支候选，正式 handoff 不批准，
  不合入 `main`/`Debug`。

## 2026-11-13T23:23:00+08:00 双目标多初始框 RK3588 对照与搜索行为隔离

- 候选源码 `codex/dual-gt-init-consistency@e1e1a535c6acfa925ea234e7007f96fac3a5cbd3`，
  基线为 `main@b80226577954febe22c5eff168125f77a6cdc724`，工作树
  `/home/u20/single-object-tracking`。`git archive` 源码包
  `/home/neardi/Project/SOT/nt-dual-gt-candidate-source.tar` 的本机与板端 SHA-256
  同为 `01c14bb9a9af59072673eb766360a4c11af6d291634c15472ee63c8925f15066`。
  板端隔离部署根 `/home/neardi/Project/SOT/dual-gt-e1e1a53`，原生编译命令
  `cmake -S rk3566/nanotrack_v3 -B build -DENABLE_RKNN_RUNTIME=ON -DBUILD_TESTING=ON
  -DRKNN_INCLUDE_DIR=/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/include
  -DRKNN_LIBRARY=/usr/lib/librknnrt.so`、`cmake --build build --parallel 4`，
  CTest `23/23 PASS`。可执行文件 `build/nanotrack_v3` SHA-256
  `a1013ddf0c3b198cbafdf2f2321d6153db32d4863b53cec20be675876e1c1bc4`。
  实测板端 Linux `6.1.84`、RKNN API `2.1.0`、driver `0.9.8`。
- 输入视频板端 `data/occ_video_1/1.mp4` SHA-256
  `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；
  template/search/head RKNN SHA-256 为
  `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55`、
  `d3cb0e3879df6a1272adb59bf501324f1f8f55b58309d3c4da4a731b600973ee`、
  `4da379f54093d79f02775e90bfea33ebee0837c6fc73a324499279b4c57807c5`。
  VisDrone9 detector RKNN/manifest/labels SHA-256 分别为
  `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c`、
  `ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9`、
  `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`。
- 同源码配置 `.35/.80` 的控制器文件 SHA-256 分别为
  `85a8f6b8dd2c9215d68c292a388a3011e5a50bf7b854f7e85c567d3378cd6e11`、
  `2adf4b35587215c18fcaf86221dd36f4c44fbf25910572a7a53c486235943d5e`；
  headless 入口 SHA-256 分别为
  `9835e10c89f0487eb6258dcc820de795e7dd04a871284489796aeda2c17c3e15`、
  `e8311ca8b031d2154b8e9b62a04c6098695fd48f334862438e77b7c3d1618e85`。
  每个配置按 `scripts/run_occ_init_matrix.sh` 所列八个 ROI 运行，实际核心命令
  `./build/nanotrack_v3 --config <headless 配置> --bbox <ROI>
  --output-video runs/compare/alpha<035|080>/<场景>/output.avi
  --output-boxes runs/compare/alpha<035|080>/<场景>/frames.csv
  --session-metadata runs/compare/alpha<035|080>/<场景>/session.json`。
  16 个 CSV 均为 271 数据帧，16 个 AVI 经 ffprobe 均为 1920×1080/271 帧；
  `runs/compare/checksums.sha256` 含全部 64 个 CSV/AVI/session/log SHA-256，
  清单 SHA-256 为 `7831279239f9113afa02b6e6da3eb693c2df47b3aa537ed097e668359995e6dc`。
  主机评分 JSON `/tmp/nt-dual-gt-evidence/alpha035-full.json`、
  `alpha080-full.json`，用两目标真值各自的正式可见帧评分，不把不可观测框算普通 IoU。
- car001 四组显示框均值 IoU `.35→.80`：标准 `0.677→0.741`、用户框
  `0.673→0.741`、左上框 `0.665→0.732`、右下框 `0.646→0.708`；
  car002 为标准 `0.676→0.708`、真值框 `0.684→0.714`、左上框
  `0.696→0.731`、右下框 `0.686→0.714`。两配置下跟踪框及控制框保持同一
  初始化对应的逐帧轨迹；四组内显示框两两平均 IoU 最小值 car001
  `0.893→0.888`、car002 `0.895→0.890`，贴合度提升但一致性有小幅下降。
  算法 runtime P95 的八组最大值 `.35=10.569 ms`、`.80=10.335 ms`。
- 完整门禁不随显示参数改变：car001 四组只有标准框 PASS，另三组分别第
  `192/194/193` 帧重捕并有晚重捕或遮挡语义失败；car002 四组均因遮挡语义失败，
  两组无正式重捕。旧 `429ca9b` 二目标八组基线保留在
  `/home/neardi/Project/SOT/display-detector-exact-429ca9b/runs/dual_gt_baseline/`，
  `checksums.sha256` 清单 SHA-256 为
  `10b1f3fa5978124ddfd4b50c2811b01bac720642b2d9fd285c903aac41b03c68`。
- 隔离参数筛查：检测间隔 `400→200 ms` 时 car001 通过数 `1/4→2/4`，但标准框
  跟踪均值 IoU `0.751→0.708` 且跨初始框差异增大；`100 ms` 时左上框无正式重捕，
  跟踪均值仅 `0.567`。把两个目标的唯一 detector 初始化框直接用于 tracker，
  car001 第 192 帧晚重捕，car002 可见框门禁失败、跟踪均值 `0.640`；
  因此没有纳入这些初始化或检测节奏改动。弃用键
  `search_reacquisition_requires_detector` 在该配置中被程序按预期拒绝。
  这些试验在同一板端隔离根的 `runs/detector200/`、`runs/detector100/`、
  `runs/detector_init/`，40 个输出的 SHA-256 清单
  `runs/exploratory-checksums.sha256` 自身 SHA-256 为
  `c1a19a5b25ef663d6888e773e3aaca57fb30310af24b8c65b66c3cc3cceb60d7`。
- 当前分支再将 `.80` 限定于 TRACKING；SEARCH/SUSPECTED 保持原 `.35`，
  避免搜索预测框被额外加速。主机重构建及 CTest 再次 `23/23 PASS`；
  本条后续提交的精确源码与板端复测另记。**Handoff**：显示贴合度改善已观察，
  car001 重捕初值敏感与 car002 完全遮挡语义仍是阻塞；正式 handoff 不批准，
  不合入 `main`/`Debug`。

## 2026-11-13T23:00:00+08:00 双真值初始框一致性候选：主机阶段

- 新分支 `codex/dual-gt-init-consistency`，工作树 `/home/u20/single-object-tracking`，
  修改基线 `main@b80226577954febe22c5eff168125f77a6cdc724`；本条所在提交为代码候选。
  开始时工作区干净。`STATUS.md` 的协调基线 `e0b59f4` 较旧，本轮没有改动它或
  `integration-contract.yaml`。统一视频 `data/occ_video_1/1.mp4` 的本机 SHA-256
  复核为 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；
  两目标真值由 `scripts/evaluate_occ_video_1.py` 严格校验并排除不可观测框。
- 从已留存的 `429ca9b` 板端 CSV 复核：car001 的 `992,582,37,39` 与
  `993,585,36,36` 两次初始化，跟踪框可见帧均值 IoU `0.751/0.719`，显示框
  `0.677/0.672`；完整门禁分别 PASS 与 FAIL（后者第 192 帧重捕，晚于窗口）。
  car002 的 `970,517,38,38` 跟踪框/显示框均值 IoU `0.799/0.676`，
  完整门禁因完全遮挡语义 FAIL。三个 CSV 的新复算工具输出保留在
  `/tmp/nt-dual-gt-baseline-archived.json`；数据源是
  `/tmp/display-detector-exact-evidence/{car_001,user_roi,car_002}/frames.csv`，
  仍属历史 `429ca9b` 运行，不能算本分支改动后的板端表现。
- 诊断显示第 48--58 帧快速运动时，显示框中心落后跟踪框，两个目标均出现低 IoU；
  `AdaptiveDisplayBox` 使用跟踪中心的 `accepted_center_smoothing_alpha=0.35`。
  本候选加入独立的 `display_center_smoothing_alpha`，默认保持已有可见光语义，
  仅 `visible-adaptive-display.conf` 设为 `0.80`，并把有效值记入 session metadata。
  旧 CSV 的离线中心回放估计可提高显示 IoU，但该回放保持旧检测时序和尺寸，
  只是参数筛选，不是运行实测。
- 实际执行 RKNN-off 主机命令：
  `cmake -S rk3566/nanotrack_v3 -B /tmp/nt-dual-gt-consistency-build -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF`、
  `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、
  `ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，
  `23/23 PASS`。`python3 scripts/evaluate_occ_initialization_consistency.py` 已用三份历史 CSV
  完整跑通；新增单测覆盖显示与跟踪中心参数分离。模型未重新训练或转换。
- **证据边界 / Handoff**：RK3588 当前地址 `192.168.1.106` 已连通，真实多初始框基线
  正在运行。候选尚未完成板端原生编译与同源码参数对照；不批准生产 handoff，
  不合入 `main` 或 `Debug`。

## 2026-11-13T22:44:33+08:00 共同算法合入 main 与 Debug

- 用户确认合并本轮功能分支。合并前远端 `main@1fb5a7fe025b38a1d4794ee1a65859a2df0174df`、
  `Debug@59f700d642085ef263a4c34c73caee78cb60bb53`，两工作树均干净。
  `main` 先快进到已验证运动输入门 `89827227d256aaa648d8dba842f2ec0ac0f94895`，
  再将 `5793986` 的共同重捕代码、`c829e8a` 的显示缩放修正及 `429ca9b` 的零边距
  显示配置合入 `811607b2a6ae4adf57ad8dd402724d9c15efc108`。
  `Debug` 将完整功能分支 `b1da4b06f1e2f6ca27fe0858fda2de3a855401cf` 合入
  `df130c27cf3d40db6ac7494ea90b22bd2261fa84`，冲突只在 `src/main.cpp` 的
  Debug 窗口参数传递，采用已在 RK3588 运行的功能分支版本。
- 两分支的 `rk3566/nanotrack_v3` 对比仅三个交互界面文件不同：
  `src/main.cpp`、`src/interactive_debug_overlay.cpp`、
  `tests/test_interactive_debug_overlay.cpp`。共同算法、配置、日志与其余测试文件逐字节一致。
  合并后分别执行主机 RKNN-off CMake 构建和 CTest：`main`、`Debug` 各 `23/23 PASS`；
  `git diff --check` 通过。此前 RK3588 原生与三目标视频证据绑定源提交 `429ca9b`；
  本次没有把 Git 合并本身算作新的板端运行。
- **Handoff**：板端记录已同步到集成分支；协调对话可据此审查新基线，但公共
  `STATUS.md` 和 `integration-contract.yaml` 的正式状态及生产 handoff 仍需其所有者裁定。
  用户要求远端仅保留 `main`、`Debug`；功能分支继续只保留本地。

## 2026-11-13T22:19:33+08:00 detector 原框零边距 RK3588 复测

- 被测源码 `codex/recovery-local-arbitration-debug-video@429ca9bc109d48f03b945bf861f16f0ff7c4cde5`，
  独立板端根 `/home/neardi/Project/SOT/display-detector-exact-429ca9b`；主机与板端
  `source.tar` SHA-256 均为 `097838dd7999b64fba9d77c9470acaedac6b386c3209b2028b0e791e8f4ce72f`。
  板端原生 RKNN、`-Werror=switch` 构建和 CTest `23/23 PASS`；
  `build/nanotrack_v3` SHA-256 为
  `4695f896dca661d53c5b1c468fe38d41e2e0c8e34a47980172bde85ef09d2485`。
  实测 Linux `6.1.84`、RKNN API `2.1.0`、RKNPU driver `0.9.8`。
- 实际输入 `data/occ_video_1/1.mp4` SHA-256
  `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；
  `runs/headless-eval.conf` SHA-256
  `4bdd01e3fcb3761c517cc887d65b46c55e89d0ef453ca792b8a60371d01cb7d9`，
  所引用 `visible-adaptive-display.conf` SHA-256 为
  `faf968acfb86c280b913567eaf74dd40580a7264d752b12546a712e688b335a7`。
  本次板端复算 NanoTrack template/search/head RKNN SHA-256 依次为
  `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55`、
  `d3cb0e3879df6a1272adb59bf501324f1f8f55b58309d3c4da4a731b600973ee`、
  `4da379f54093d79f02775e90bfea33ebee0837c6fc73a324499279b4c57807c5`；
  detector `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/releases/yolov8n-visdrone9-rk3588-fp16-v1/models/yolov8n-visdrone9-rk3588-fp16.rknn`
  维持 `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c`。
- 在板端部署根对 `user_roi=993,585,36,36`、`car_001=992,582,37,39`、
  `car_002=970,517,38,38` 分别执行
  `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox <ROI> --output-video runs/<scenario>/output.avi --output-boxes runs/<scenario>/frames.csv --session-metadata runs/<scenario>/session.json`。
  三次均 exit 0，CSV 各 271 数据行、AVI 各 1920×1080/271 帧；runtime P95 依次为
  `11.016`、`9.672`、`9.823 ms`。三次初始化 `display_bbox` 均与唯一匹配的 detector
  `clipped_bbox` 四坐标完全相等（最大绝对差 `0`）。
- 对上一版 `c829e8a` 的三组相同 ROI 逐帧比较，状态、接受/重捕标志、模板动作、跟踪框、
  控制框均无差异。重捕后 21 帧显示框平均 IoU：用户 ROI `0.661→0.720`，
  car001 `0.621→0.710`，car002 `0.760→0.791`。full evaluator 仍为 car001 **PASS**
  （185 帧重捕）、用户 ROI **FAIL**（192 帧晚于 182--189）、car002 **FAIL**
  （213--216 帧完全遮挡语义），没有借显示改动宣称算法门禁改善。
- `runs/user_roi/`、`runs/car_001/`、`runs/car_002/` 的 CSV SHA-256 依次为
  `f90e6a9aae185dbf0e5c6fbeaf78f1e56a147acbdde268b1bb88c9a503f047eb`、
  `9926c0688495716f3e6aa0ec86cf4b87c1f569000cda4637876c6d752b78ea5f`、
  `f8ea15ff8e4ee52194cb43d241d86a4f88ccde2e6561498b4d5f5a9f1`；AVI 对应为
  `14ed94f1ce0c5799bbd8cc6e8baeef4a319c313310c914d52c9c1583c8ad93e6`、
  `edf20ddcf711e329047ba4d70ce1c18d188a11ba93b57f4deafe091f0062dd03`、
  `4b263ce94b6be8ee8b8f9183c32ff523353ae2d4ab31d06bfa04a2569cae8dc9`。
  主机评价 JSON `/tmp/display-detector-exact-evidence/<scenario>/eval.json` 的 SHA-256
  按上述顺序为 `107642c0726a413b878d6513296e570422c562beb067a7e45c6c6a29babbb7d1`、
  `46cc20aa5435015b715dd94f16e57e6e484f0d6fb2e6941822538e38d9f1be36`、
  `39e30cd2b2d3b05bf6b60954b75db654fe8e6cdfb4eee4c6924acd300f7a824e`。
- 交互模式在 Xvfb 中用同一用户 ROI 跑至第 200 帧后按 Q 正常退出；板端
  `runs/interactive_display/debug.avi` 1920×1080/201 帧，SHA-256
  `7298ae96dd953d26f3644e04fe8ce7f97701d62e8fde14e00a3039559110ec44`。
  `NanoTrack Display` 窗口截图 `/tmp/display-detector-exact-frame200.png` SHA-256
  `7f629ee82d49b13dadf11a8f0be7dec109ae2c16ac7e8d907d5558fa0d4bd0d4`，
  同名副本已保留在主机 `/tmp/`。
- **证据边界 / Handoff**：显示检测为低频、两次确认后渐进纠正，故中间帧仍由 tracker
  传播并平滑；本轮保证不再给 detector 框附加人工边距，不代表逐帧原样显示每次异步
  检测。真实吊舱效果待人工测试。候选保持本地分支，按用户既定规则待确认后才合入
  `main`/`Debug`，不推送远端功能分支。

## 2026-11-13T22:12:14+08:00 显示框使用检测器原框：主机验证

- 分支 `codex/recovery-local-arbitration-debug-video`，工作树
  `/home/u20/.codex/worktrees/recovery-local-arbitration-debug-video/single-object-tracking`，
  修改基线 `7cc966a6d45d592eff5def5d8eba30ddc50a1caf`；源码结果提交以本条
  Git 历史为准。按用户要求，默认配置和 RK3588 可见光配置中的车、人、未知类别
  水平/垂直显示留量均改为 `0`。初始化采用裁剪后的 detector 原框；跟踪期已确认的
  detector 原框作为显示包络目标。帧间传播、两次检测确认、渐进纠正、尺寸步长限制、
  SEARCH 冻结及重捕过渡保持现有语义，因此显示框不是每帧直接复制异步检测结果。
- `rk3566/nanotrack_v3/config/visible-adaptive-display.conf` SHA-256 从
  `3a949556b682011b7ebb40fb0353471b71af0677685e37a938f8af01d66928af`
  变为 `faf968acfb86c280b913567eaf74dd40580a7264d752b12546a712e688b335a7`；
  交互入口配置 `rk3588-adaptive-display-debug.conf` 仍为
  `5d67b007ff13cbd617e6ac54485b45c4a89d52da38d3651f54f5e0d3b92dfaca`，
  其 `controller_config=visible-adaptive-display.conf` 引用新参数。新增单测检查车/人
  初始化和两次确认后的目标框没有人工外扩。
- 实际执行 `cmake --build /tmp/nt-display-rebase-build -- -j4`、
  `ctest --test-dir /tmp/nt-display-rebase-build --output-on-failure`、
  `git diff --check`；主机 `23/23 PASS`。此阶段只有源码和主机结果，RK3588 新部署与
  固定 ROI 视频对照待完成。模型、输入视频与上一条 RK3588 记录相同，未重新转换。

## 2026-11-13T21:44:16+08:00 重捕后显示框缩放 RK3588 三目标复测

- 被测源码 `codex/recovery-local-arbitration-debug-video@c829e8aa9055714f3abc3ed817854e2725002854`，
  独立板端根 `/home/neardi/Project/SOT/display-rebase-c829e8a`；主机与 RK3588 的
  `source.tar` SHA-256 均为 `1a79700ca659d920d03ad91200ddc635c8317c289ad2e3ffef01a74caa1e9911`。
  板端原生 RKNN、`-Werror=switch` 构建及 CTest `23/23 PASS`，
  `build/nanotrack_v3` SHA-256 为
  `35f764601e25a090d921b1e7366ce163e2437094e0ac4c2e50bbd3139a9341ee`。
  本轮日志实测 Linux `6.1.84`、RKNN API `2.1.0`、RKNPU driver `0.9.8`。
- 运行配置 `runs/headless-eval.conf` SHA-256
  `845d99d2f9d470d842057874016bb252402574746c5cb305125e426d9ad7d132`；
  输入 `data/occ_video_1/1.mp4` SHA-256
  `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`。
  `models/template_backbone.rknn`、`search_backbone.rknn`、`head.rknn` 在本次板端
  复算的 SHA-256 依次为 `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55`、
  `d3cb0e3879df6a1272adb59bf501324f1f8f55b58309d3c4da4a731b600973ee`、
  `4da379f54093d79f02775e90bfea33ebee0837c6fc73a324499279b4c57807c5`；
  VisDrone9 RK3588 detector `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/releases/yolov8n-visdrone9-rk3588-fp16-v1/models/yolov8n-visdrone9-rk3588-fp16.rknn`
  为 `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c`。
- 在部署根分别执行
  `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox <ROI> --output-video runs/<scenario>/output.avi --output-boxes runs/<scenario>/frames.csv --session-metadata runs/<scenario>/session.json`，
  其中 `user_roi=993,585,36,36`、`car_001=992,582,37,39`、
  `car_002=970,517,38,38`。三次均 exit 0，CSV 271 数据行、AVI 1920×1080 共 271 帧；
  runtime P95 分别 `9.628`、`10.002`、`9.432 ms`。CSV SHA-256 依次为
  `38e782b3286ef2de6612bc2ba2d8f7ddbdbcaf89c9dfd32b7f17ae61479ea936`、
  `2662ebff0437ffb24a86dccf2dbe06877a1fbfa3920f8fc0ed268537aa736247`、
  `c2f672fa5de7c1480a5d235b4d044874769ad7b67940bedd8fdbdb7e62b83fde`；
  AVI SHA-256 依次为
  `14ed94f1ce0c5799bbd8cc6e8baeef4a319c313310c914d52c9c1583c8ad93e6`、
  `edf20ddcf711e329047ba4d70ce1c18d188a11ba93b57f4deafe091f0062dd03`、
  `4b263ce94b6be8ee8b8f9183c32ff523353ae2d4ab31d06bfa04a2569cae8dc9`。
- 对父提交 `5793986` 的同 ROI CSV：car001、car002 的状态、接受/重捕标志、模板动作、
  跟踪框及控制框每帧完全一致。显示框在 car001 第 190 帧由 `63.6×72.0` 收至
  `55.6×63.9`，car002 第 240 帧由 `70.5×73.5` 收至 `54.8×61.6`。
  用户 ROI 在第 197 帧由 `62.6×72.9` 收至 `54.3×63.5`；重捕帧独立运行中由
  193 变为 192，控制轨迹从该帧开始不逐帧相同，不能将其计为严格隔离性对照。
  因显示模块在控制器输出之后运行，该一帧提前不是显示框重置对同帧控制器的直接输入；
  异步调度差异仅为推断，尚未单独归因。
- 重捕后 21 帧显示框与相应可见真值的平均 IoU：用户 ROI `0.537→0.662`，
  car001 `0.525→0.621`，car002 `0.615→0.760`；跟踪框未因此变为显示框。
  full evaluator：car001 **PASS**（185 帧重捕）；用户 ROI **FAIL**（192 帧仍晚于
  182--189 窗口）；car002 **FAIL**（沿袭 213--216 帧完全遮挡语义），其余门通过。
  evaluator JSON 位于主机 `/tmp/display-rebase-evidence/<scenario>/eval.json`，三份
  SHA-256 依次为 `f531a1609ee6513d1505f3056cc794cb56fe73d19f53e9833ff79f60f7e10f1d`、
  `6861b4d4ae608175798831cb3da9ea440f8f91b4e70b3fc0b42b83376401b2b5`、
  `de69da3d32d834ab266da21e9a64d6889e9960bcb36e3543c4f25d13441ccba3`。
- 交互模式在独立 Xvfb 中执行
  `DISPLAY=:99 ./build/nanotrack_v3 --config rk3566/nanotrack_v3/config/rk3588-adaptive-display-debug.conf --bbox 993,585,36,36 --max-frames 200 --output-video runs/interactive_display/debug.avi --output-boxes runs/interactive_display/frames.csv --session-metadata runs/interactive_display/session.json`；
  实际显示至第 200 帧后按 Q 正常退出。板端 `NanoTrack Display` 窗口截图
  `/tmp/display-rebase-window-frame200.png` SHA-256
  `8c4dda386eafedbdc5e6f4c426d997ab64fb64b2c1c9a63aa992d688fb4fec27`，
  主机副本同名；Debug AVI 1920×1080 共 201 帧，SHA-256
  `7298ae96dd953d26f3644e04fe8ce7f97701d62e8fde14e00a3039559110ec44`，保留在板端
  `runs/interactive_display/`。
- **证据边界 / Handoff**：这是 RK3588 视频与虚拟显示器上的显示框修复验证；真实吊舱
  显示效果待人工观察。候选继续保持本地分支，未获用户确认前不合入 `main` 或 `Debug`；
  请求协调对话消费本源码提交及上述板端证据时保留已知失败项。

## 2026-11-13T21:37:35+08:00 重捕后显示框重复放大修正：主机验证

- 分支 `codex/recovery-local-arbitration-debug-video`，工作树
  `/home/u20/.codex/worktrees/recovery-local-arbitration-debug-video/single-object-tracking`，
  本次修改基线 `1417535d6491e6b88b473e25efaffa548698de6f`；源码提交以本条 Git 历史
  为准。输入为此前 RK3588 用户 ROI `993,585,36,36` 的保留 CSV：
  `/tmp/recovery-5793986-user-frames.csv`，对应源码 `5793986`、板端部署根
  `/home/neardi/Project/SOT/recovery-local-debug-video-5793986/runs/user_roi/`。
- 实际 CSV：192 帧 SEARCH 时跟踪框 `36.0×41.1`、显示框 `49.0×57.2`；193 帧重捕后
  跟踪框变为 `47.9×54.6`，但显示模块继续套用旧宽高比，193--197 帧显示框按每帧
  `5%` 上限增至 `62.6×72.9`，这些帧 `display_correction_applied=0`。固定 car001 ROI
  同样从重捕前 `49.8×57.6` 增至 190 帧 `63.6×72.0`。根因是重捕缩放和旧显示变换
  叠加；首次新的检测显示纠正发生在用户 ROI 的 198 帧，不能解释此前放大。
- 修正仅在接受重捕时用新跟踪框和现有类别边距重建显示变换，并清除重捕前尚未完成的显示
  检测确认；保留五帧中心过渡、每帧 `5%` 尺寸上限及原检测纠正规则。跟踪框、控制框、
  模板、运动模型与重捕状态机代码未变。新增单测覆盖旧包络比例过大时的重捕缩放。
- 实际执行
  `cmake -S rk3566/nanotrack_v3 -B /tmp/nt-display-rebase-build -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF -DCMAKE_CXX_FLAGS=-Werror=switch`、
  `cmake --build /tmp/nt-display-rebase-build -- -j4`、
  `ctest --test-dir /tmp/nt-display-rebase-build --output-on-failure`：主机 `23/23 PASS`；
  `git diff --check` 通过。此阶段仅完成源码和主机验证，RK3588 新构建与视频对照待执行。

## 2026-11-13T21:31:38+08:00 板端进度接管与 RK3588 开发范围

- 用户授权当前对话维护 `docs/progress/board-validation.md`，并明确后续开发只在 RK3588
  上进行。此前 RK3566 记录保留为历史证据；后续原生构建、调试、算法验证与运行证据均绑定
  实际 RK3588 板卡。`STATUS.md` 和 `integration-contract.yaml` 仍由协调对话维护。
- 当前功能分支 `codex/recovery-local-arbitration-debug-video`，工作树
  `/home/u20/.codex/worktrees/recovery-local-arbitration-debug-video/single-object-tracking`，
  基线 `ef7dba9e9fe0028850979edcdd5fa06cdb8`，源码提交
  `5793986141ac560371aab7c6d5e15c44da1999aa`。本进度提交身份以 Git 历史为准。
  该功能分支尚未合入 `main`、`Debug`，也未推送远端。
- RK3588 独立部署根 `/home/neardi/Project/SOT/recovery-local-debug-video-5793986`；
  `source.tar` SHA-256 为
  `a70642fe634edbc2f717fb528834bd1cce98fd1bc19508ddae6e876cf757007c`，
  原生 `build/nanotrack_v3` SHA-256 为
  `785d964aca101085528e7aa274c6990068c53704cea865bfee21619d829bf84c`。
  RKNN 原生构建、`-Werror=switch` 和主机/板端 CTest 均为 `23/23 PASS`。
- 输入视频为板端 `data/occ_video_1/1.mp4`，SHA-256
  `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`。
  模型链接到既有 `adaptive-display-59769e8` 部署：RK3588 NanoTrack
  template/search/head RKNN SHA-256 依次为
  `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55`、
  `d3cb0e3879df6a1272adb59bf501324f1f8f55b58309d3c4da4a731b600973ee`、
  `4da379f54093d79f02775e90bfea33ebee0837c6fc73a324499279b4c57807c5`；
  VisDrone9 RK3588 detector RKNN SHA-256 为
  `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c`。
  这些模型身份沿用既有 RK3588 部署记录；板端历史环境记录为 RKNN API `2.1.0`、
  RKNPU driver `0.9.8`，本轮未单独重测驱动版本。
- 同一固定 ROI 对照 0--270 帧：`car_001=992,582,37,39` 的 full evaluator **PASS**，
  185 帧重捕；`car_002=970,517,38,38` 的跟踪、控制、状态和重捕逐帧与父提交一致，
  保留 213--216 帧已知 `full_occlusion_semantics` FAIL。两目标算法 P95 分别约
  `10.499 ms`、`10.441 ms`。用户 ROI `993,585,36,36` 的重捕由上一版 218 帧提前到
  193 帧，但仍晚于 car001 正式窗口 182--189，故不宣称该 ROI 通过完整门禁。
  三次完整输出分别保留在板端部署根的 `runs/car_001/`、`runs/car_002/`、`runs/user_roi/`。
- 实际交互验证命令：在部署根运行
  `DISPLAY=:99 ./build/nanotrack_v3 --config rk3566/nanotrack_v3/config/rk3588-adaptive-display-debug.conf --bbox 993,585,36,36 --max-frames 195 --output-video runs/debug_video_recovery/output.avi --output-boxes runs/debug_video_recovery/frames.csv --session-metadata runs/debug_video_recovery/session.json`。
  运行退出码 0，AVI 为 `1920x1080`、196 帧，SHA-256
  `0e8218bddd791c8db231c1f5a02c13ade723e8110f5728cf918d171deacaf740`。
  第 190 帧可见运动预测、recent/safe、控制和三个关键 detector 候选框，未绘制全部原始检测框。
- **证据边界 / Handoff**：本轮是 RK3588 上的候选算法与 Debug 视频验证，不等于正式
  开发板源码/模型 handoff 或真实吊舱验收。请求协调对话把后续开发板目标从 RK3566 更新为
  RK3588，并在审查此候选提交及未通过项后再更新公共状态和集成契约。

## 2026-11-12T22:25:39+08:00 双窗口精简交互 Debug RK3588 验证

- 分支 `codex/adaptive-display-box`，运行源码提交
  `23f594c023492034d6d392686b7d4a89b233b9e0`，源码归档 SHA-256
  `88dc420e27752b960fd8b5e8cc3a234545ad148de22800e15028c77e00af3649`；部署到独立目录
  `/home/neardi/Project/SOT/adaptive-display-debug-23f594c`，未覆盖上一版运行或视频证据。
- RK3588 原生 Debug 构建启用 RKNN、`BUILD_TESTING=ON` 和 `-Werror=switch`，包含
  `src/main.cpp` 的完整可执行目标并通过 CTest `23/23 PASS`。新二进制
  `build/nanotrack_v3` SHA-256 为
  `5c0cbc2ccfdf7436a2e46b986ae857e20c869690f1ba8253c630da0ba5492a74`；runtime/controller
  配置 SHA-256 仍分别为
  `5d67b007ff13cbd617e6ac54485b45c4a89d52da38d3651f54f5e0d3b92dfaca`、
  `3a949556b682011b7ebb40fb0353471b71af0677685e37a938f8af01d66928af`。
- 在板端现有 X11 `DISPLAY=:0` 会话短启动交互程序，`xwininfo -root -tree` 实际观察到
  `NanoTrack Display` 与 `NanoTrack Debug` 两个独立窗口，均为 1280x720，位置分别为
  `+2+23`、`+1298+23`；无 HighGUI/X11 创建错误。固定 ROI 启动同时完成 tracker、RK3588
  detector 加载及自动 car 类别建立，随后保持 PAUSE；短测由 timeout 结束，不计作算法视频回归。
- **结论 / 边界**：双窗口显示和关键 detector 过滤已通过主机单测、RK3588 原生编译测试及
  实际窗口创建检查。改动只在交互绘制层，不改变 `tracking_bbox`、`control_bbox`、CSV、检测器
  调度、状态机或控制输出；因此本次未重复运行 271 帧 canonical evaluator，也不改变上一条
  双车算法验收结论、人物板端验收边界或生产/吊舱状态。

## 2026-11-12T22:23:02+08:00 双窗口精简交互 Debug 主机实现

- 分支 `codex/adaptive-display-box`，基线
  `eae32e31e333d3f636294dfaa59ae9ad6c3fba1c`。交互模式拆分为两个同步窗口：
  `NanoTrack Display` 只显示原始画面和白色 `display_bbox`，`NanoTrack Debug` 显示状态、
  NanoTrack/运动诊断与绿色 `control_bbox`；正常、SUSPECTED 和 SEARCH 的有效控制框统一使用
  绿色，具体控制来源仍由标签区分。ROI 重选继续在干净显示窗口完成，任一窗口关闭均安全退出。
- detector 可视化只保留初始化选中框、通过类别/位置门且已进入候选队列的关键框、当前 active
  candidate/probe 及 search hint；不再绘制全部原始拒绝框或对应拒绝文本。运行时 detector
  诊断、CSV、控制器候选队列和日志未改变，本次只修改交互绘制选择。
- 主机 RKNN-off 增量构建和 CTest `23/23 PASS`，其中 overlay 回归验证关键候选保留、类别不符、
  stale、capacity、位置拒绝及无效框均不再进入画面；`git diff --check` PASS。
- **边界 / 下一步**：主机 RKNN-off 不编译 `main.cpp` 的 RKNN/OpenCV 可执行目标，因此当前只构成
  overlay 单测证据；需提交精确源码后在 RK3588 原生重建并启动交互模式，确认双窗口实际显示。

## 2026-11-12T22:02:57+08:00 自适应显示框 RK3588 双车验收

- 分支 `codex/adaptive-display-box`，实施基线
  `main@dccf8e0844b78cf9a1f4ee207352429b6d884979`，运行与验收源码提交为
  `eb66cf6115eff274aa809f5e875a939de632ef99`（显示实现提交
  `59769e89df45e6e39a69306dadae7a6294e7e911`）。精确 HEAD 源码归档在主机与板端的
  SHA-256 均为 `0a7c2f228407304ffe69b989f2c249c39ee26df29b986980d341e394896db042`；
  独立板端根为 `/home/neardi/Project/SOT/adaptive-display-59769e8`，未覆盖现有基线部署。
- 实际 `LKD3588` 原生 Debug 构建启用 RKNN、`BUILD_TESTING=ON` 与 `-Werror=switch`，
  CTest `23/23 PASS`。`build/nanotrack_v3` SHA-256 为
  `ea562308a9970e93ef8839d9fc3d919c99ed48c841f6edc98d7b6bda7e5fd9f6`；自适应 runtime
  配置与 controller 配置 SHA-256 分别为
  `5d67b007ff13cbd617e6ac54485b45c4a89d52da38d3651f54f5e0d3b92dfaca`、
  `3a949556b682011b7ebb40fb0353471b71af0677685e37a938f8af01d66928af`；tracker manifest
  SHA-256 为 `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`。
- 固定 ROI `car_001=992,582,37,39`、`car_002=970,517,38,38` 分别运行 0--270 帧。两次
  runtime exit 0、schema v13 verifier PASS，输出 CSV 与两类 AVI 均为 271 帧；算法运行时间
  P95 分别为 `10.573 ms`、`10.470 ms`，低于 `33.3 ms` 验收线。两次自动类别均唯一建立为
  VisDrone9 car=`2`，显示刷新请求分别 78/80 次、SEARCH 请求均为 2 次、显示纠正应用
  36/37 次、暂停 0 次，最终刷新周期均为 100 ms；single-flight 与 SEARCH 抢占测试通过，
  未见请求积压。
- `output.avi` 保持绘制 `control_bbox`；同一次运行的原视频与 v13 CSV 只读重放生成
  `display-output.avi`，绘制 `display_bbox`。两份显示视频均经 `ffprobe` 确认为
  1920x1080、30 fps、271 帧，car001/car002 SHA-256 分别为
  `cca38a4b6e7f34457e6f9060711a835e589ce5891402cb90a865f3430fb9c7e0`、
  `3caba35936c7c4a4da62d8925ece73d42b0931c6a8fa645c95a739c9ef3b89a6`。控制诊断视频
  SHA-256 分别为 `edf20ddcf711e329047ba4d70ce1c18d188a11ba93b57f4deafe091f0062dd03`、
  `78fe73d1628d2b9b23f91d368175d3e907a02e847c48665a4ffd41186ec04765`。
- 对比既有 `occ-restored-2d31bfe` 基线，两个目标各 271 帧的 53 个跟踪、控制、状态机、
  模板事务和运动模型字段逐项完全一致，差异数均为 0。full evaluator：car001 **PASS**，
  visible mean IoU `0.751326`、185 帧重捕；car002 的可见框、预测连续性、235 帧重捕、连续
  控制和 identity proxy 均 PASS，visible mean IoU `0.798100`，只保留基线已知的 213--216
  帧 `full_occlusion_semantics` FAIL。evaluation JSON SHA-256 分别为
  `dddc669ae1288a6f8c4505095cb710813a699b7e4543fc8596ddae929ef8062e`、
  `131a535178779408ef84a510c6e04dd4536252e6afc298101888dfc9a448f2a9`；完整大工件与日志保留在
  板端 `runs/car_001/`、`runs/car_002/`。
- **证据边界 / Handoff**：本轮证明车目标自适应显示在 RK3588 上满足性能与隔离要求，且不使
  C012/当前控制基线退化。人物类别、边距、姿态变化及接口已有主机单测，但没有新的合规人物
  视频，故不声明人物板端验收；如需纳入正式人物基准，请协调对话更新集成契约。未修改
  `STATUS.md` 或 `integration-contract.yaml`，未执行生产交叉编译或人工吊舱验收。

## 2026-11-12T21:59:27+08:00 evaluator 对 additive schema v13 的兼容修正

- 分支 `codex/adaptive-display-box`，基线为本分支实现提交
  `59769e89df45e6e39a69306dadae7a6294e7e911`，结果为包含本条记录的同一提交。首次 RK3588
  双车运行已证明 schema v13 保留 schema v12 全部语义列，但 canonical evaluator 仍以
  `schema_version == 12` 判断当前重捕，导致实际 185/235 帧声明被列为 invalid schema。
- evaluator 改为接受单一、版本号不低于契约最低版本且语义列完整的 additive schema；session
  metadata 必须与实际行版本一致，旧 schema 9 及缺列版本仍拒绝。新增 v13 继承 v12 重捕语义
  的回归，`python3 -m unittest tests.test_evaluate_occ_video_1` 结果 `24/24 PASS`，
  `git diff --check` PASS。
- **边界 / 下一步**：该修正只改变验收工具的向后兼容版本判断，不改变任何跟踪、显示或控制
  运行时；需把精确新提交同步到 RK3588 后重新执行两车 full evaluator，再记录最终门禁。

## 2026-11-12T21:52:12+08:00 车/人自适应显示框主机实现

- 分支 `codex/adaptive-display-box`，工作树
  `/home/u20/.codex/worktrees/adaptive-display-box/single-object-tracking`，基线
  `main@dccf8e0844b78cf9a1f4ee207352429b6d884979`；结果为包含本条记录的同一提交。实现独立
  `AdaptiveDisplayBox` 后处理层，只覆盖 `ControllerOutput::display_bbox` 及兼容别名 `bbox`；
  `tracking_bbox`、`control_bbox`、模板、运动模型、遮挡判定及重捕控制器均不读取显示状态。
- 检测请求新增 `INITIALIZATION`、`SEARCH_RECOVERY`、`DISPLAY_REFRESH` 用途，运行时按用途
  分流；显示批次不进入恢复候选队列，SEARCH 请求优先提交。自动类别初始化使用
  VisDrone9 `person=0`、`car=2`；唯一匹配时显示框从鼠标 ROI 扩展到带类别边距的全目标框，
  否则保持 ROI。TRACKING 低频修正、SUSPECTED/SEARCH 冻结可信尺寸、SEARCH 跟随预测中心，
  重捕使用五帧渐进过渡。
- 新增 `config/visible-adaptive-display.conf` 与
  `config/rk3588-adaptive-display-debug.conf`；历史配置字节未修改。显示检测初始 5 Hz、范围
  1--10 Hz，并按算法运行时间滚动 P95 在 20/25/30 ms 门槛加速、降频或暂停；schema 升至
  v13，记录请求用途、显示框来源、纠正确认、刷新周期和运行时间 P95。
- 主机命令：`cmake -S rk3566/nanotrack_v3 -B /tmp/nanotrack-adaptive-display-build
  -DCMAKE_BUILD_TYPE=Debug -DENABLE_RKNN_RUNTIME=OFF`，随后 `cmake --build ... -j4` 与
  `ctest --test-dir ... --output-on-failure`，结果 `23/23 PASS`。另以
  `-DCMAKE_CXX_FLAGS=-Werror=switch` 在 `/tmp/nanotrack-adaptive-display-werror` 重建并运行，
  同样 `23/23 PASS`；`git diff --check` PASS。
- **边界 / 下一步**：当前仅为主机实现证据，尚未构成 RK3588 原生构建、性能或视频效果证据；
  下一步将精确提交部署到 RK3588 独立目录，使用 `occ_video_1` 固定 car001/car002 ROI 运行，
  核对 schema v13、算法 P95、无检测积压及既有 C012/当前基线不退化。人物仅完成代码与单测，
  没有合规人物视频前不声明板端人物验收。

## 2026-11-12T16:19:23+08:00 四个已拒绝运动冻结实现的失败帧导出

- 按用户复核请求，从四个已拒绝 RK3588 保留根的原始输出 AVI 导出失败窗口：
  `d7ee7df`、`2b08bc5`、`3f3d7be`、`49027b1`。选择依据为 full evaluator 的首次坏帧、
  错误重捕帧及 car002 的 213--216 帧遮挡语义失败；每个中心帧导出 `center-4` 到
  `center+4` 共 9 张带真实源帧号 JPEG，并生成一张 3x3 contact sheet。
- 最终板端目录为 `/home/neardi/Project/SOT/motion-freeze-failure-frames-v3`，压缩包为
  `/home/neardi/Project/SOT/motion-freeze-failure-frames-v3.tar.gz`；当前工作树的忽略目录副本为
  `results/failure-frame-sequences/motion-freeze-failure-frames-v3/`。共 28 个事件窗口、252 张
  连续帧、28 张 contact sheet；`windows.csv` 记录全部精确帧段，`README.txt` 记录四种实现语义，
  `SHA256SUMS` 覆盖 282 个非清单文件。
- 最终压缩包 SHA-256 为
  `c2bfd257d2817bc35d40dbc4cebbb78dc3403c857ce81899190ded9b3c83514c`。抽查
  `d7ee7df/car_001/f104_false_reacquisition/contact_sheet.jpg` 确认角标为真实源帧
  100--108，框来自各版本保存视频中绘制的 `control_bbox`。首次 v2 导出因旧 FFmpeg 将
  contact-sheet 角标重编号为 0--8 而废弃；v3 才是交付版本，不改变任何源码、运行结果或判定。

## 2026-11-12T16:06:25+08:00 恢复版 2d31bfe RK3588 精确确认

- 分支 `codex/recent-safe-occlusion-recovery-v2`，工作树
  `/home/u20/.codex/worktrees/rk3588-debug-deploy/single-object-tracking`，基线
  `49027b1a7f6ab6da2f81d0693ae4a6b420463cb7`，源码提交
  `2d31bfee11e381a345ffba958beccd7eadd39289`。该提交恢复已验证的 `5fbcf83` search 趋势行为，
  不再采用本轮四个已拒绝的分级运动冻结实现；schema v12 与 `motion_update_valid` 可观测字段
  保留。主机 CTest `22/22`、evaluator 单测 `23/23`、`git diff --check` 均 PASS。
- 源码归档 `/tmp/occ-restored-2d31bfe.tar.gz` 与板端同名文件 SHA-256 均为
  `b070320cfa355dab7fb1eac7692674414361c2acd678450fa1fdef4fd2027aa4`；独立板端根
  `/home/neardi/Project/SOT/occ-restored-2d31bfe`。RK3588 原生 Debug 构建和 CTest `22/22`
  PASS；`build/nanotrack_v3` SHA-256
  `9a30f2305060cd4d1d7aae7e2e41e23b7cd5c3dda011316234d6a43c34e4c240`。配置/模型 manifest
  SHA-256 分别为 `7b038eed1dd052de5879bedbf465a2ded3cc79d5a6f0b662e39d9fd06fc15c34`、
  `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`。
- 固定 ROI `car_001=992,582,37,39`、`car_002=970,517,38,38` 各运行 0--270 帧并 exit 0；
  schema v12 verifier 均 PASS，AVI 均为 `1920x1080`、271 帧。car001 full evaluator **PASS**，
  visible mean IoU `0.751326`、唯一重捕 `[185]`，所有门通过。CSV/AVI/metadata/evaluator JSON
  SHA-256 分别为 `20dabd22713d37e8e38fbecf2bf3bdd4530eab91a0b02164c3d51542e81d0b1e`、
  `edf20ddcf711e329047ba4d70ce1c18d188a11ba93b57f4deafe091f0062dd03`、
  `96a846da68118a5dcc1703dcd3c46766501f7e0cdc2641d456ea4d890634e929`、
  `90d706c80eac5dea5c5b1921eec249049e6d5dfa5f26aba2dd1a5715422a6c41`。
- car002 visible mean IoU `0.798100`，唯一重捕 `[235]`；visible、prediction continuity、
  reacquisition、continuous control、identity proxy 均 PASS，只有已知 213--216 帧
  `full_occlusion_semantics` FAIL，因此 full verdict 仍为 **FAIL**。CSV/AVI/metadata/evaluator JSON
  SHA-256 分别为 `943ca28e792ad2a12a350af0ef5f840c740253ad941cc7086b44350d0b962a41`、
  `78fe73d1628d2b9b23f91d368175d3e907a02e847c48665a4ffd41186ec04765`、
  `2a5ebdf9d6edf7a89609cc10fa0e127e489d53912dd98cb4d1ff3e3d7bdce441`、
  `14187890856aa0909328e9f0450a642278aef09631a536354a0e2dd555dfab3d`。
- **结论 / Handoff**：最新 HEAD 已恢复到最后验证过的不退化行为，四个实验版本继续作为拒绝
  证据保留，不进入 handoff。用户提出的“运动更新停止早于确认遮挡”原则仍合理，但当前 response
  单指标不能可靠区分遮挡与正常强形变；在增加独立遮挡证据并覆盖多场景验证前，不再针对该单一
  视频调冻结阈值。当前候选仍 HOLD，不构成正式生产或真实吊舱验收。

## 2026-11-12T15:59:24+08:00 49027b1 板端拒绝，恢复最后通过候选

- SUSPECTED 分级提交 `49027b1a7f6ab6da2f81d0693ae4a6b420463cb7` 已部署到独立根
  `/home/neardi/Project/SOT/occ-motion-suspected-49027b1`；源码归档 SHA-256
  `1a713edccd21d13ca30cff20b2178dac4fb74cc10b870b8ee86294fe046e16c6`。原生构建、CTest
  `22/22`、两目标运行与 schema v12 verifier 均 PASS，但 full evaluator 仍非双目标通过。
- car002 明显改善：唯一重捕为234帧，visible/control/identity/prediction/reacquisition 均 PASS，
  只保留已知213--216帧 full-occlusion semantics FAIL；但 car001 出现错误重捕
  `[80,101,212]` 并发生大范围身份/可见框失败。car001 的早期强形变/遮挡依赖原运动加速度
  穿越，SUSPECTED 时无差别清零加速度会破坏它，因此 `49027b1` 仍低于 C012/`5fbcf83`
  基线并被拒绝。
- 四次真实板端失败表明，在当前观测量下，仅靠同一组 response 指标无法通用地区分“正常强
  形变/临时遮挡但应继续运动估计”和“稳定尺寸遮挡即将污染运动”；继续调该视频阈值会产生
  过拟合。分支最新源码恢复最后一个双目标已验证不退化的 `5fbcf83` 行为：继续更新当前运动，
  只冻结 search 专用干净趋势；进入 SEARCH 时仅在当前趋势反向时采用干净趋势并清零加速度。
  schema v12 的 `motion_update_valid` 可观测字段保留，但恢复版中所有 accepted TRACKING 观测均
  为 true。失败根目录和报告全部保留。
- **边界 / 下一步**：恢复版本不是对旧策略的最终否定；若继续实现，需增加独立于当前单指标
  波动的遮挡证据或跨场景数据，而不是继续调 `occ_video_1`。恢复提交仍需主机回归与一次精确
  RK3588 确认；在确认前不宣称最新 HEAD 已回到基线。

## 2026-11-12T15:53:46+08:00 3f3d7be 板端拒绝与 SUSPECTED 分级边界校正

- 几何解耦提交 `3f3d7be753096ad3de6e2d27082f73b7054eeb50` 已部署到独立根
  `/home/neardi/Project/SOT/occ-motion-trust-3f3d7be`；原生 RK3588 Debug 构建/CTest
  `22/22` 与两目标程序均成功，但 full evaluator 仍 **FAIL**：car001 错误重捕
  `[84,102,127]`，car002 错误重捕 `[110]`。局部连续性允许 NanoTrack 框继续输出，却也会在
  单指标波动触发的长冻结期间逐渐接受错误目标，因此该提交拒绝，不作为候选。
- 三次失败共同说明：把任一单项 response 相对下降直接解释为“完全正常跟踪结束”过于敏感；
  这不是用户所述旧策略本身，而是本轮对其触发边界的错误映射。现有状态机已经提供所需递进：
  `TRACKING accepted` → `SUSPECTED tentative` → `SEARCHING`。SUSPECTED 框仍对外输出并可控制，
  但已不被接受为可信状态更新，正好是“禁止运动更新早于确认丢失”的阶段。
- 后续实现撤销所有“accepted 单指标下降即冻结”逻辑及冻结几何特例，改为首次发布
  SUSPECTED tentative 时冻结 motion/shadow motion 的开环加速度和尺度速度、保留此前速度；
  SEARCH 继续使用该未被 tentative 框污染的趋势。既有 `suspicion_clear_ms` 清洁迟滞通过后，
  第一帧重新 accepted 时以当前框重新初始化运动模型。该设计不新增阈值，也不根据视频帧号或
  方向做决策。
- 单测覆盖 tentative 期间 observation/control 连续、`motion_update_valid=false`、速度保留、
  加速度清零，以及原有清洁迟滞通过后重建运动状态。当前尚未提交或部署；旧三组失败证据均保留。

## 2026-11-12T15:48:10+08:00 2b08bc5 板端拒绝与冻结几何参考解耦

- 收窄提交 `2b08bc5bd4bd5c69bab6de2888db53972cc88de3` 已部署到独立根
  `/home/neardi/Project/SOT/occ-motion-trust-2b08bc5`，原生 RK3588 Debug 构建和 CTest
  `22/22` PASS，两目标程序 exit 0；但 full evaluator 仍 **FAIL**：car001 错误重捕
  `[84,102,127]`，car002 错误重捕 `[115,175,226]`，故该提交继续拒绝。
- 逐帧证据确认：即使仅冻结运动模型，观测质量的几何门仍以被冻结的运动预测为参考；随着
  NanoTrack 当前框继续运动，两者残差必然增长，最终由“主动冻结”自身制造 observation
  rejection 和 SEARCH。这违反“禁止运动更新先于确认遮挡，但当前 NanoTrack 框仍继续输出”
  的递进语义。
- 后续修正只解开这一处因果耦合：`motion_updates_held_` 且尚未进入 loss/search 时，观测的
  center/area/aspect 连续性改用上一已接受 `current_bbox_`，不再用有意冻结的 motion bbox；
  响应质量门和后续 SUSPECTED/SEARCH 递进保持不变，真正进入搜索后仍使用运动预测几何。
  单测通过人为拉开冻结预测与局部连续观测，确认不会发生自致拒绝。当前尚未提交或部署。

## 2026-11-12T15:43:51+08:00 d7ee7df 板端拒绝与运动信任边界收窄

- 精确提交 `d7ee7df5c6f3ab9805e7f29d08cf73e13e819564` 已部署到 RK3588 独立根
  `/home/neardi/Project/SOT/occ-motion-trust-d7ee7df`；源码归档主机/板端 SHA-256 均为
  `aa9137740e60a9d5ddd3508a401f4864235c9b2f3fc1b1bcdf9b97edace7b8fa`，二进制 SHA-256
  `e62028fa9f60aaf0585ef4710bae33c9556c02382454d060ecb6e2b1fffedde5`。原生 Debug、RKNN、
  `-Werror=switch` 构建与 CTest `22/22` PASS，两目标程序 exit 0，schema v12 板端 verifier
  均 PASS；这些功能结果不覆盖下述算法拒绝。
- 固定 ROI 双目标 full evaluator 均 **FAIL**。car001 出现错误重捕 `[104,158,194,236]`，
  car002 出现错误早期重捕 `[71]` 且未形成目标遮挡后的正式重捕；两个目标可见期质量均大幅
  回退。根因不是初始 ROI：首个单项 peak-margin 相对下降即同时冻结了运动、响应基线、稳定
  计数和模板推进，冻结参考无法适应正常外观变化，运动预测逐渐远离仍正确的 NanoTrack 观测，
  最终由自身制造几何拒绝和错误搜索。
- 本轮拒绝把上述结果升级为候选。修正边界收窄为用户原始要求：`motion_update_valid` 只控制
  运动模型校正；已接受观测原有的响应基线、stable 计数和 recent-safe 模板状态机恢复原行为，
  避免把“运动模型输入更严格”扩大成所有学习状态同时冻结。运动冻结/连续干净后恢复、schema
  v12 和方向启发式移除继续保留。
- 收窄后的主机 RKNN-off 构建、CTest `22/22`、evaluator 单测 `23/23`、`git diff --check`
  均 PASS；尚未提交或部署，下一步必须以新精确提交重新执行 RK3588 双目标回归。旧
  `d7ee7df` 根目录和失败报告保留，不覆盖。

## 2026-11-12T15:35:54+08:00 分级运动更新信任（主机验证）

- 分支 `codex/recent-safe-occlusion-recovery-v2`，基线
  `3f50eb205899e2b8e656d9c194de67f63478fea4`；结果为包含本条记录的当前提交。根据用户确认，
  本提交废止 `5fbcf83` 的“进入 SEARCH 时若方向相反则切换最后干净趋势”启发式，历史 RK3588
  结果继续保留为失败分析证据，不删除、不改写。
- 新策略将“当前 NanoTrack 框可继续输出/控制”和“允许更新运动模型”拆成独立语义：响应相对
  退化但仍被跟踪器接受时，`tracking_bbox/control_bbox` 继续采用当前观测，状态仍可保持
  `TRACKING`，但 `motion_update_valid=false`，运动位置/速度不再被该框校正，开环加速度和尺度
  速度清零；响应基线、稳定计数和 recent 模板推进同步冻结。只有连续干净观测达到既有
  `suspicion_clear_ms` 后，才用该干净时间段的首尾观测重新建立运动状态并恢复更新。
- 日志协议升为 schema v12，新增 `motion_update_valid`；CSV、JSONL、遥测、板端 verifier 与
  `occ_video_1` evaluator 同步校验“只有已接受的 TRACKING 观测才能更新运动模型”。这只是
  可观测性与安全不变量升级，不改变模型、检测器、遮挡阈值、重捕门或云台控制框语义。
- 主机 RKNN-off Debug 目录 `/tmp/nanotrack-motion-trust` 使用
  `-DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS=-Werror=switch` 完成构建；
  CTest `22/22` PASS，`python3 -m unittest tests.test_evaluate_occ_video_1` `23/23` PASS，
  `git diff --check` PASS。新增单测覆盖退化观测继续控制但冻结运动、保留干净速度并清零加速度、
  连续干净时间达标后才恢复更新。
- **边界 / Handoff**：当前仅为主机验证，尚未在 RK3588 上构建或复跑 car001/car002；不能据此
  声称视频效果改善或批准生产 handoff。下一步以精确提交部署到独立板端目录，固定双目标 ROI
  回归并与 C012、`b538667` 及 `5fbcf83` 保留结果比较。

## 2026-11-12T15:06:46+08:00 search 反向趋势修正 RK3588 双目标验证

- 分支 `codex/recent-safe-occlusion-recovery-v2`，源码提交
  `5fbcf8301ce52ad1c779541e30a35ad4153b2753`，基线
  `59c79e74a1dfc150d6257734598459f886d3da49`。源码归档在主机与板端 SHA-256 均为
  `6e73944baa06cf20ef24cca147da11c2a16f03be662c506bad1e7971d36609d5`，独立板端根为
  `/home/neardi/Project/SOT/occ-motion-opposite-5fbcf83`。原生 Debug、RKNN runtime、
  `-Werror=switch` 构建和 CTest `22/22` PASS；二进制 SHA-256
  `8766a6122b2d621bfeb95d4e32db8122cb3566fa9bd57c82aa2f8285de72762e`。
- 初版 `0a8304e9485dcb4b9754e37ca8a05300b03ad819` 对所有退化期都采用冻结趋势，car002
  方向改善但 car001 重捕从185延迟到194，故拒绝；其独立根
  `/home/neardi/Project/SOT/occ-motion-clean-0a8304e` 保留为失败证据。最终版只在进入 SEARCH
  时当前速度与最后干净趋势点积为负才替换速度并清零加速度；方向一致的 car001 保留原运动
  模型。
- 固定 ROI `car_001=992,582,37,39`、`car_002=970,517,38,38` 各运行0--270帧，均
  exit 0，AVI 均为1920x1080、30 FPS、271帧。非交互输出视频主框已改画
  `control_bbox`；`display_bbox`、交互调试叠框及生产显示路径未修改。
- car001 full evaluator **PASS**，重捕帧仍为185，visible/control/identity/prediction/
  full-occlusion 全部门通过。CSV、AVI、metadata、evaluator JSON SHA-256 分别为
  `f9161e18ee0fc6857459246262b59c2079025f51559e97595ede7091e8b08314`、
  `edf20ddcf711e329047ba4d70ce1c18d188a11ba93b57f4deafe091f0062dd03`、
  `575566a3b9e9645942d06342f4a65792c4cd9fc4bba46a6d5249e2c177c80ab6`、
  `dd9859784dfa2e4a00e4bd0e0d841264159aa6e04d5e42ecb4186033967e869f`。
- car002 在210帧仍是左下速度 `(-19.852020,+15.126831)`；遮挡物观测把216帧运动状态污染
  为速度 `(+21.519398,-8.891993)`、加速度 `(+146.120041,-109.989998)`。最终版在222帧
  SEARCH 切换为最后干净趋势 `(-15.818654,+16.426685)` 且加速度归零，并保持到234帧；
  预测中心相对正式重现目标的偏差由旧版约56 px降至约21 px，235帧重捕不变。full evaluator
  仍因既有213--216帧错误声明观测而 FAIL，但 visible/control/identity/prediction/reacquisition
  门均 PASS。CSV、AVI、metadata、evaluator JSON SHA-256 分别为
  `0a1d9288f555adb9561c98560f5de2e2d1f8ef0828b9295e18fbf75eceb80144`、
  `78fe73d1628d2b9b23f91d368175d3e907a02e847c48665a4ffd41186ec04765`、
  `63eb52962671321eb86e16bd4f159a22d2f6fe5ce08ede82a347ae74487b2eb7`、
  `7fd391078f91e8120c7a7f953274341ec5dd8c318547fc2727c61df0dc136637`。
- **边界 / Handoff**：本轮只修正进入 SEARCH 后采用反向污染趋势的问题，不修改阈值、配置、
  模型、模板、遮挡状态机或重捕确认。car001 不低于 `b538667` 基线，car002 已改善但并未解决
  213--216帧遮挡判定过晚；候选继续 HOLD，不构成正式生产 handoff 或真实吊舱验收。

## 2026-11-12T14:55:16+08:00 recent-safe v2 search 运动趋势修正（主机验证）

- 分支 `codex/recent-safe-occlusion-recovery-v2`，基线
  `59c79e74a1dfc150d6257734598459f886d3da49`。复核 RK3588 保留运行
  `/home/neardi/Project/SOT/occ-recent-safe-v2-b538667/runs/car_002/bboxes.csv`：目标在
  195--210 帧总体向左下运动，210 帧运动状态仍为
  `velocity=(-19.852020,+15.126831)`；211--216 帧遮挡物上的观测仍被判为可信并修正运动，
  到 216 帧已反向为 `velocity=(+21.519398,-8.891993)`、
  `acceleration=(+146.120041,-109.989998)`。217--221 帧虽不再更新可信状态，旧加速度仍被
  开环积分，222 帧进入 SEARCH 时进一步变为
  `velocity=(+39.388611,-23.931980)`，解释了预测框向右上偏离。
- 通用修正不改变遮挡判定、跟踪框、控制阈值、模板或模型：控制器保留最近一次“响应未出现
  相对退化”时的运动趋势；单项响应退化只冻结该 search 专用参考，不提前进入 SEARCH，连续
  `suspicion_clear_ms` 干净观测后才更新。真正进入 SEARCH 时，仅当当前速度与保留趋势方向
  相反才采用保留速度并将加速度清零；方向一致时保持原运动模型，避免无差别替换正常趋势。
- 后续非交互保存视频改为绘制 `control_bbox`；`display_bbox` 算法、交互叠框和生产
  `rect_show` 均未修改，等待真实吊舱测试后再决定显示平滑策略。
- 主机 RKNN-off 在独立目录 `/tmp/nanotrack-recent-safe-prediction` 完成 Debug 构建，CTest
  `22/22` PASS；新增测试覆盖保留趋势、反向退化观测和 SEARCH 清零加速度；视频主框切换经
  源码链路复核，`git diff --check` 通过。当前仅为主机验证，尚未部署或运行 RK3588，不能声称 car002 实际
  视频中的预测偏离已消除；下一步是提交源码后在独立板端目录原生构建，并以固定双目标 ROI
  复跑，C012 与 `b538667` 保留结果继续作为不得退化的对照。

## 2026-11-11T14:51:34+08:00 C009 holdout FAIL，停止调参与重复运行

- 冻结命令在 RK3588 上精确执行一次，输出 `1920x1080`/271 帧；首个正式
  `scope=holdout` evaluator 返回 FAIL。按冻结规则未执行 full-scope evaluator、第 2/3
  次重复或任何参数修改。
- 原遮挡后仍在帧 185 重捕，identity proxy、continuous control、prediction continuity
  PASS；但可见期 200--215 帧再次进入搜索，216 帧第二次重捕，不满足全序列
  唯一重捕。holdout 有效观测 mean IoU `0.671046 < 0.70`，all-visible `0.677702`。
- evaluator 还把不含全遮挡帧的 190--270 holdout scope 判为 full-occlusion gate FAIL，这是
  scope 适用性缺陷；排除它也不改变上述两个真实失败。
- RK3588 verifier 另在 CSV 帧 204 失败：`search_hint_present=1` + `CONFLICT`，但
  `search_hint_confirmation_count=0`，违反当前 schema 对 present hint 必须为 `1/2` 的约束。
- 保留根目录：`/home/neardi/Project/SOT/occ-candidate-05cf4d9/runs/C009-full-run1`。
  evaluator JSON/bboxes/video SHA-256 分别为
  `dae7168e3df422b42796ba156e70f7c1bfb67b0fb65596e63a3791a439343ffe`、
  `97f275ae1bf05eb345bb8335757ffbcaac063153d895c1f3ccd0808792f4c645`、
  `ac527664f99b1211d6ffa505d4bb65f9cc69b7795b7aaf870c97d3de30a63f36`。
  C009 结论为 `REJECT for release`；holdout 现已揭盲，后续候选不得声称仍为盲测。

## 2026-11-11T14:44:38+08:00 C009 development 平台 PASS 与冻结

- C009 RK3588 原生 Debug、`-Werror=switch`、CTest `19/19`；可执行/config SHA-256
  `3792ecde2f4b915d38496fa4fdbba6ecbe6854aae50897e9fb5e577d1d2a43a3`、
  `87b4b818e740a5a4137e4ab7339a26b5d70e93e5c7852e468dd26c8a73e06791`。
- 33/33 development 真实矩阵 PASS，重捕 `184--187`，最大 mean IoU 下降 `0.016618`；
  variable-time、detector delay/drop、generation-gap 的哈希 focused CTest 证据 PASS。
- 冻结前独立主机 CTest `19/19` 和 evaluator/robustness Python 单测 `18/18` PASS；
  全仓 Python discovery 另有 16 个模块因当前主机未安装 `cv2`/`torch` 而在导入阶段中止。
- 保留 nominal 在帧 185 重捕，evaluator/board verifier PASS；视频 `1920x1080`、190 帧。
  bboxes/video/evaluator JSON SHA-256 分别为
  `4d78dacdbd984f088aec6dff9139fa8e56de4a6e5687b1e3335273c191317098`、
  `0a5d887e20c3f5b556fc564c17abe90bd8a0a5073b0a795cdb490c34c19c34e3`、
  `8ea2ac37ba2238156f7cbd73f2dc1a4f1b5cb43f16c0d6634ad623d87d3bc504`。
- C009 已写入 `frozen-candidate.json`，状态 `NOT_RUN`；下一步只能执行其中精确 full
  command，不能再改 runtime/config。部分 190--193 status 文本暴露边界继续保留。当前仍无
  post-189 视频/bbox/runtime/evaluator 结果，也没有人眼 identity sign-off。

## 2026-11-11T14:26:43+08:00 C009 候选

- C009 恢复已通过 nominal/width+5 的 C007 runtime（probation=3），保留 400 ms、0.020
  和固定 `2/3` 几何显著规则；仅配套使用已提交的非刀刃 evaluator。
- 下一步优先重签 C007 保留 CSV，补跑缺失项并由 robustness aggregator 实际判定；尚未冻结，
  无 holdout/full 运行。

## 2026-11-11T14:26:10+08:00 C008 拒绝与视觉单帧绝对底线校正

- C008 板端 Debug/CTest `19/19`；probation 4/5 均 nominal PASS，但 width `+5%` 到帧 174
  才进入搜索，无法在 190 前完成两击，因此 C008 拒绝，完整矩阵未启动。
- probation=2 的唯一 C007 失败是帧 166 IoU `0.4916`、中心误差 `0.201`，不是丢失或身份
  错误。evaluator 单帧绝对 IoU 底线改为非贴线的 `0.45`；中心 `<=0.50`、mean `>=0.70`、
  identity、重捕、连续控制和矩阵相对下降 `<=0.03` 不变。合成测试锁定 0.46 PASS、0.43
  FAIL；evaluator/robustness 单测 `17/17` 通过。
- 下一候选恢复 probation=3，并重签/聚合已有真实矩阵报告；无 holdout/full 运行。

## 2026-11-11T14:21:16+08:00 C007 消歧通过与 C008 probation 内移

- C007 板端 Debug/CTest `19/19`；nominal 与关键 width `+5%` 均在帧 185 重捕。所有已执行
  实数 `+/-10%` 邻域 PASS，重捕范围 `184--187`。
- `recent_probation_frames=2` 在遮挡前 recent 模板提交后导致帧 166 视觉框门槛失败；该参数
  确实影响模板历史，不能当作无关抖动。C007 因而不冻结。
- C008 只将 probation 从 3 提到更保守的 4，并要求 `3/4/5` 平台；两击、消歧、运动控制和
  detector 隔离不变。尚无 C008 板端结果，无 holdout/full 运行。

## 2026-11-11T14:09:35+08:00 C006 平台结果与 C007 几何显著消歧

- C006 nominal 在帧 185 重捕；cadence `360/400/440 ms` 分别在 `184/185/187`，走廊
  `0.018/0.020/0.022` 均在 185 重捕。中心、width `-5%`、height `+/-5%` 通过。
- width `+5%` 的真实目标/邻车中心距离比为 `0.009615/0.017311`，但 count-only policy
  仍全部拒绝；同时 detector/template 分数更偏邻车，不能用分数消歧。
- C007 增加固定 `nearest <= 2/3 * runner-up` 几何显著规则。只解除最近候选的 ambiguity
  标志，仍需 anchor、recent veto、最小搜索龄与连续两击；相近候选继续拒绝。focused CTest
  `3/3` 通过，尚待全量测试、提交和 RK3588 验证。无 holdout/full 运行。

## 2026-11-11T13:50:36+08:00 C005 nominal 拒绝与 C006 最小候选

- C005 精确提交 `89215ab` 在 RK3588 原生 Debug 构建、`-Werror=switch`、CTest `19/19`
  通过，但 nominal 在 detector frame 182 的批次中 `0.015` 走廊把 121 个位置候选全部拒绝，
  因而无重捕；发现 nominal 失败后已立即停止其余 32 组矩阵。
- C006 保留已证明需要的 400 ms cadence，只把走廊恢复为 C004 的 `0.020`。这样以更早的新鲜
  可见帧解决 deadline/ambiguity，而不靠过紧位置阈值。尚无 C006 板端结果，无 holdout/full
  运行。

## 2026-11-11T13:47:27+08:00 C005 通用内点候选

- C005 仅把 detector cadence 从 `500` 内移到 `400 ms`，并把全帧对角线归一化走廊从
  `0.020` 收紧为 `0.015`；运行时 recent-veto-only、两击原子提交、同类歧义拒绝、
  generation/stale 隔离均不变。
- 选择依据是 C004 的真实邻域失败类别，不使用帧号或真值坐标：未来必须同时通过 cadence
  `360/440 ms` 和 corridor `0.0135/0.0165`，以及中心保持的初始化扰动，才允许冻结。
- 当前仅写入候选与台账，尚无 C005 板端结果；无 holdout/full 运行。

## 2026-11-11T13:46:23+08:00 C004 鲁棒性矩阵拒绝冻结

- RK3588 已执行 37 组 development-only 真实运行；临时视频每组评分后删除，保留 CSV、
  metadata、日志和报告。多数阈值 `+/-10%`、中心 `+/-2 px`、中心保持的高度 `+/-5%`
  均 PASS。
- 两个有效失败阻止冻结：`detector_interval_ms +10%` 无法在 190 前重捕，说明 nominal
  500 ms 卡在截止边缘；中心保持的 width `+5%` 在搜索后得到两个走廊内同类候选，安全地拒绝
  授权。width `-5%` 没有单帧质量失败且相对 mean IoU 只下降 `0.019565`，但被 evaluator
  与聚合器不一致的固定 `0.75` mean 门槛提前判死。
- evaluator 已以合成测试统一合同：单报告仍要求每有效观测 IoU `>=0.50`、中心误差
  `<=0.50`，绝对 mean floor 为 `0.70`；Task 8 聚合器继续要求所有变体 PASS 且相对 nominal
  下降 `<=0.03`。evaluator/robustness 单测 `16/16` 通过。
- C004 明确 `REJECT for freeze`；运行时 recent-veto-only 逻辑保留。下一候选只做通用的探测
  周期内移与归一化走廊收紧，再重跑有效邻域。没有 holdout/full 运行。

## 2026-11-11T13:25:34+08:00 C004 nominal PASS 与 provenance 接口补齐

- 同一 C004 产物经提交 `007f201ff998ec8b3d5f21ee134d406cc81d6870` 的 evaluator
  重算后 development 全门槛 PASS：帧 188 唯一重捕，有效观测 mean IoU `0.762083`，
  all-visible 诊断 mean IoU `0.741046`，搜索段 max speed/acceleration
  `370.984454/9331.818646`，均低于真值派生上限。报告 JSON/Markdown SHA-256 分别为
  `daad50c1a5be7361ba7d1f2b695d9a10a7ca0cf6ba66fc493159cdbf89a096d9`、
  `0336d518f7946a825a413820adc7e98ba87787e30d97223a8e2f218b54075f2c`。
- Task 8 聚合器要求报告内含源码、配置、模型身份；evaluator 新增成组 provenance 参数，拒绝
  缺项、非 40 位 commit 或非 64 位 SHA-256。evaluator/robustness 单测合计 `15/15` 与
  Python 语法检查通过。下一步重签 nominal 报告并执行真实扰动矩阵；当前仍未冻结。

## 2026-11-11T13:23:03+08:00 C004 重捕成功与 evaluator 语义分层修正

- C004 源提交 `0e19decf14d6fc674e78aa4c6249e12b70ba85b0` 已部署到 RK3588
  `/home/neardi/Project/SOT/occ-candidate-0e19dec`；源码归档 SHA-256
  `4eb692d72dae7f74093677b54313c7f0a7447ce3ba12eb253aa934c571f68082`。板端原生
  Debug、`-Werror=switch`、CTest `19/19` 通过；可执行文件 SHA-256
  `89e61bd1fb938be513f3b941af2e4354a682d952856de3566710b27119f28fa2`。
- 严格 `--max-frames 189` 的 `runs/C004-development` 运行完成，schema-10 board verifier
  PASS。帧 172--180 均为 `SEARCHING,false,MOTION_SEARCH`，控制连续；唯一两击重捕在帧
  188，随后帧 189 仍跟踪同一目标。旧 evaluator 报告显示重捕门槛、遮挡语义、连续控制均
  PASS，但把重捕前 `observation_valid=false` 的可见搜索预测当成视觉观测，并把原子
  `SEARCHING -> TRACKING` 切换计入同一运动段，因此其 FAIL 不能作为 schema-10 合同结论。
- evaluator 已用合成回归修正语义分层：搜索预测继续受估计轨迹、搜索段运动连续性和控制门槛
  约束；严格 IoU/中心误差只评价 `observation_valid=true`；原子重捕切换开启新的连续性段，
  但重捕观测仍必须具有真值重叠并经过 identity proxy。新增回归连同 evaluator 单测 `10/10`
  通过；尚待把这一提交精确传板并重算 C004 报告。
- 证据边界：运行程序、视频帧和 bbox 标签均未读取 190 以后；一次诊断命令的 `sed` 范围误显
  示了 `annotation_status.csv` 的 190--193 行状态文本。未运行 holdout/full evaluator，也未据此
  调整运行时代码或参数，但严格的“190 后元数据完全未暴露”边界已不再成立，后续冻结记录必须
  保留这一限制。

## 2026-11-11T13:11:43+08:00 C003 verifier PASS 与 C004 recent-veto-only 修正

- C003 在 RK3588 `/home/neardi/Project/SOT/occ-candidate-2389df0` 原生 Debug 构建、
  `-Werror=switch` 和 CTest `19/19` 通过；二进制 SHA-256
  `b2cad65b27fc18171ce36fadf8bdb63d4a970f16205ed934e3a4f17a01fc46da`。严格 0--189
  运行后，board verifier 首次 PASS，verify log SHA-256
  `f6564fb4dcfed79e3726f98d87cb3fb20a5bdbfa551023f269537cc304c585bd`。
- evaluator 仍与 C002 同一 FAIL：无重捕且 visible/identity gate 失败。逐字段证据显示窄走廊已把
  两批 position-valid/queued 数量从 3 降为 1；frame 187 唯一候选 center ratio `0.016328`，
  anchor 可靠，recent score `0.748582` 只达 weak，但两模板 IoU `0.911284`。当前实现错误要求
  recent 也必须 reliable，导致合法 anchor 无法建立 shadow first hit。
- C004 按既定证据政策修正为“anchor 唯一授权、recent 只否决”：可靠 anchor 必需；recent
  不存在或 weak 时不能授权也不能否决，只有 recent 自身 reliable 且与 anchor 几何冲突才 veto。
  detector identity/class/ambiguity、400 ms 最小搜索龄、0.02 corridor、两击一致性和第一击零提交
  均保持。
- 合成 detector 测试把第一击 recent 降为 weak 且保持高 IoU，确认仍只生成 shadow hint；第二击
  才原子提交。主机 CTest `19/19`、鲁棒性检查器单测 `4/4`、Python 语法检查和
  `git diff --check` 均通过。C003/C004 结果已只追加到 calibration ledger。
- C004 尚未提交、部署或运行；没有 holdout 证据，nominal 即使通过也尚未满足鲁棒性矩阵。

## 2026-11-11T13:07:57+08:00 C002 安全搜索结果与 C003 成熟窄走廊候选

- C002 在 RK3588 继续严格 `--max-frames 189`；二进制/config SHA-256 分别为
  `980545c9baeeceba21a9d0c10e7036c5eb3c73505c46b5c5adce694d1585cfd1`、
  `bb3513919fd4cc78ae3ae78b38057882ffa96d14bf22429a7f3120fd54e51901`。板端原生
  `-Werror=switch` 构建及 CTest `19/19` 通过，运行 SUMMARY mean runtime `9.338 ms`、
  mean pipeline `112.215 ms`、end-to-end `8.911 FPS`。
- evaluator development 仍 FAIL，但安全失败形态符合预期：full-occlusion semantics、
  continuous control、prediction continuity 全部 PASS，错误的 tracker-only 重捕已消失；两个
  detector batch 都保留三个同类候选，ambiguity gate 全部拒绝，因此 190 前无重捕。visible mean
  IoU `0.732761`；JSON SHA-256
  `f6585be2c7faf8b21000f0a99220771773020b08752abba0dcc41d236d8760cc`。
- C002 还暴露 verifier 不应要求 `hypothesis_source=MOTION`：搜索帧可以正在探测 detector ROI，
  但对外控制仍明确来自 `MOTION_SEARCH`。新增 DETECTOR-hypothesis 的合法 fixture 先 RED，再移除
  该错误耦合；phase/source/confidence/control_valid 仍全部必需。
- C003 将按整帧对角线归一化的 detector center gate 从 `0.15` 收紧为 `0.02`。前者在
  1920x1080 上约等于 330 px，而目标宽约 44 px；后者形成约一目标宽的通用 motion corridor。
  同时新增默认 0、候选 400 ms 的 `search_reacquisition_min_age_ms`，阻止短暂遮挡过渡立即种下
  重捕候选，且必须小于等于 motion confidence 期。
- parser、identity、metadata、verifier 与合成 controller 测试覆盖该最小搜索龄：龄前可靠观测
  不能产生 hint，龄后仍需两击才提交。C003 配置 SHA-256
  `89fbc1d23b2be17c8d6938f5f53383ec4b26a59383f4343dce80603335715706`；聚焦 4/4
  CTest 通过。后续即使 nominal 成功，也必须对两个实数门做 ±10% 邻域，不能直接冻结。
- C003 尚未提交、部署或运行；仍无 holdout 证据。

## 2026-11-11T13:01:15+08:00 C001 development 失败与 C002 detector-required 候选

- C001 在 RK3588 `/home/neardi/Project/SOT/occ-candidate-cbd38a5/runs/C001-development`
  严格以 `--max-frames 189` 完成，SUMMARY `tracked_frames=189`、mean runtime `9.148 ms`、
  mean pipeline `111.934 ms`、end-to-end `8.934 FPS`；没有读取 190--270。二进制 SHA-256
  `f58f22bb9e2bb6ba5821841d1402d3869e9978097d921d7bfb0cd5933199439d`，输入和所有
  RK3588 ONNX 派生模型/detector 哈希在运行前复核与 C000 一致。
- evaluator development 明确 FAIL：continuous control PASS，但 tracker-only shadow candidate
  在 171、183、186 三次错误重捕；full-occlusion semantics 于 172--173 失败，visible mean IoU
  `0.731970`，182--189 可见框与 183 以后 identity proxy 失败，运动速度/加速度也越过真值参考。
  JSON/Markdown SHA-256 为 `4aeb6e574db8c3fbbb466e176f9a373b559f725fefe1c419ad10bd1d9f290771`、
  `e616374ebb4b9432d2e5c31c1ffea14b6333ea1a5624e99637160034cb5dac15`。
- 同一真实输出暴露 verifier 将 `motion_control_authorized` 仍限定为旧
  `OCCLUSION_HOLD`，从而错误拒绝 frame 169 的 `UNCERTAIN / SEARCHING / MOTION_SEARCH`。
  新增 valid motion-search fixture 先复现 RED，再改为依据 schema 10 phase/source/confidence
  授权；旧 HOLD 的 age/budget 约束仍保留。
- C002 增加通用 `search_reacquisition_requires_detector=true`，默认 false 保持兼容；搜索态的
  非 detector 观测不再落入 legacy ordinary recovery，只能维持 motion control。detector 仍只是
  class/generation/position 安全的搜索种子，最终授权继续要求 anchor/recent 与两击几何一致，
  detector 本身不能直接控制。两击提交的 hint 同时明确记录 `used=true`。
- 配置解析、identity、metadata、verifier 与候选文件已同步，新候选配置 SHA-256
  `bb3513919fd4cc78ae3ae78b38057882ffa96d14bf22429a7f3120fd54e51901`；定向
  `controller_support|long_term_controller|long_term_runtime|tracking_log|verify_board_schema`
  为 `5/5` 通过。C001/C002 证据已只追加写入 calibration ledger。
- C002 尚未提交、部署或运行；没有 holdout 证据，不能宣称已重捕或已标定。

## 2026-11-11T12:53:27+08:00 development-only 鲁棒性聚合器与 RK3588 首次构建审计

- 在 `codex/occ-video-1-tracking`、Task 8 起点
  `2403c8618dc5f4234e399165e9168c5687aeb8e7` 新增纯报告聚合器
  `scripts/check_occ_tracking_robustness.py`。它不执行 tracker 或选择参数，只接受 schema 1
  candidate manifest 与已命名 evaluator reports；结果提交可用
  `git log -1 --format=%H -- scripts/check_occ_tracking_robustness.py` 查询。
- 聚合器要求每个实数参数 `-10%/nominal/+10%`、每个可调计数 `-1/nominal/+1`、初始框中心
  `±2 px`、尺度 `±5%`、变时间戳和 detector delay/drop/generation gap 全部齐备；所有报告必须
  严格为 development `0--189`、身份哈希匹配、所有 evaluator gate PASS、重捕 `<190`，且相对
  nominal 的可见 mean IoU 最大下降不超过 `0.03`。confirmation 至少两击、detector 不直接
  控制、generation/stale 隔离与异常 fail-closed 作为固定安全审计，不允许向下扰动。
- 单元测试先因脚本不存在 4/4 RED；实现后覆盖缺邻域、nominal-only 刀刃、错误 scope/holdout、
  重复报告、source/config/model 哈希不匹配、安全降级和 IoU 崩落，最终 4/4 PASS，
  `py_compile` 通过。完整主机 CTest `19/19`、0 失败、总时间 `4.24 sec`。
- 已把 `2403c86` 精确归档到 RK3588 `/home/neardi/Project/SOT/occ-candidate-2403c86`，归档两端
  SHA-256 `dc97c84662778a15ff85d36c17b73c1a4069dbfbd6cbf80bef17dcf1b7b4525a`；models、
  releases、data 均只链接到现有 ONNX 派生 RK3588 工件，复核哈希与 C000 一致。原生 Debug
  构建二进制为 `bb596ef7cb95c38232b724f3c4fb5d462d2ac8c0e48d1d2d25b0f5162211f207`，板端
  CTest `19/19`、0 失败、`6.40 sec`。
- 板端 `-Werror=switch` 复现发现 Task 6 新增的 `kSearchControlPrediction`/`kSearchHint` 未进入
  `main.cpp` 颜色 switch；根因是共享 overlay enum 已扩展而 Debug app 映射未同步。当前最小
  修复为两种语义增加独立颜色，避免主要 Debug 功能落到红色 fallback。修复提交后必须重新
  打包、原生构建和复跑 `-Werror=switch`，当前旧二进制不得用于 C001 数据运行。
- 尚未执行任何 0--189 candidate run，也没有读取 190--270；本条不是标定、鲁棒平台或板端
  功能通过结论。

## 2026-11-11T12:47:34+08:00 通用可见光遮挡候选配置与只追加标定账本

- 在 `codex/occ-video-1-tracking`、Task 7 起点
  `15f32fe8d52fee0b53e2c15d961b87f1c006ccd5` 新增通用
  `config/visible-occlusion-calibrated.conf`；结果提交可用
  `git log -1 --format=%H -- rk3566/nanotrack_v3/config/visible-occlusion-calibrated.conf` 查询。
- C001 保留 `visible-default.conf` 的已有视觉、遮挡、detector 和两击确认阈值，只显式启用
  `motion_search_enabled=true`，初始通用物理边界为 `2500 ms` 置信期、`1.0 diagonal/s`
  速度、`2.0 diagonal/s^2` 加速度、`0.50/s` 尺度变化和 `100 ms` probe 间隔；配置中没有
  视频名、帧号、真值坐标、桥梁或场景特例。`confirmation_required=2`、window 3 保持不变。
- 原 `visible-default.conf` 未修改，SHA-256 仍为
  `54dcc973feed11ad3e9e7838c8f35e86b0d587d5aa143863a625c849fa5dae19`；新候选为
  `495d5a0bc74d077cb333ae25affc512005b701fc432552f0994ed55607aab6c9`，controller identity
  测试确认两者不同且默认配置继续关闭 motion search。
- 新建只追加 `docs/evaluation/occ-video-1/calibration-ledger.md`，先冻结 C000 失败基线的源码、
  配置、RK3588 ONNX 派生模型、detector、输入与输出报告哈希，再登记 C001 的 parent、理由、
  变更参数和严格 `--max-frames 189` 待执行命令。C001 明确标为 `PENDING_NOT_RUN`；未知二进制、
  板端目录和报告写为 `PENDING`，没有从历史证据猜测。
- TDD RED 时两个测试分别因候选文件不存在失败；新增文件后，聚焦 `controller_support|cli`
  为 `2/2` 通过。新鲜完整构建及 CTest 为 `19/19` 通过、`0` 失败、总时间 `4.51 sec`，
  `git diff --check` 无输出。
- 本里程碑没有运行 C001、没有读取 190--270 holdout，也没有改变或重新生成 RK3588 模型；
  `calibrated=true` 仅表示候选配置接口，不能解释为真值标定已经完成。下一步 Task 8 先实现
  robustness manifest 校验器，再部署并只用 0--189 做候选与参数邻域验证。

## 2026-11-11T12:43:43+08:00 schema 10 搜索控制证据契约与 fail-closed 校验

- 在 `codex/occ-video-1-tracking`、Task 6 起点
  `936a5e77510786ec6631d94939f236492ae3e643` 将 tracking CSV 与 session metadata
  升级为 schema 10；结果提交可用
  `git log -1 --format=%H -- rk3566/nanotrack_v3/include/tracking_log.hpp` 查询。
- 每行现在直接序列化 controller/runtime 的 `tracking_phase`、`observation_valid`、
  `control_source`、真实 tracking/display/control 三类框、接受路径、搜索年龄与置信状态、
  速度/加速度/尺度速度，以及 shadow hint 的 source frame/timestamp、generation、projected
  bbox、确认计数、使用状态和拒绝原因；没有从旧 mode 或显示框反推这些语义。
- overlay 明确把搜索期云台框显示为 `SEARCHING / MOTION_SEARCH PREDICTION`，与
  `TRACKING / OBSERVATION` 分开；telemetry 以显式 phase/source 产生 `search_enter`、
  `search_expired`、`hint_first_hit`、`hint_reject` 和两击 `reacquired` 事件。
- 板端 verifier 现在要求 schema 10、连续帧/时间戳、三类有限且在画面内的框、通用运动搜索
  配置 identity、搜索控制置信状态和完整 hint 诊断；会拒绝 SEARCHING 冒充 observation、
  detector 直接授权、无置信 motion control、单击重捕、partial hint 以及 metadata/CSV schema
  不一致。旧模型/manifest/hash/runtime 检查继续保留。
- 新鲜验证命令重新配置并构建 `/tmp/nanotrack-occ-plan`，完整 CTest 为 `19/19` 通过、
  `0` 失败、总时间 `3.65 sec`；`git diff --check` 无输出。定向
  `tracking_log|debug_telemetry|interactive_debug_overlay|verify_board_schema` 为 `4/4` 通过。
- 本里程碑仍只有主机合成证据，没有改 ONNX/RKNN 模型，也没有同步或运行 RK3588/RK3566；
  不能宣称 `occ_video_1` 已达成 190 帧前重捕、阈值已由真值标定或真实云台已验收。下一步是
  Task 7 形成少量通用可见光候选配置，再在 0--189 development 段进行候选标定与扰动筛选。

## 2026-11-11T12:33:39+08:00 两击一致性重捕与模板 probation 原子提交

- 在 `codex/occ-video-1-tracking`、Task 5 起点
  `6b8c9f950b77b3b95ff3227d46d1c7d746fe7d9f` 将 tracker-seeded 与
  detector-seeded 重捕统一到一个 shadow confirmation candidate；结果提交可用
  `git log -1 --format=%H -- rk3566/nanotrack_v3/src/long_term_controller.cpp` 查询。
- 搜索中只有 anchor 可靠观测能建立/确认候选；recent 可以通过模板一致性门或否决，但
  recent-only 永远不能授权。第二击必须在 `confirmation_window` 内、identity/source 一致且满足
  IoU、中心、面积和宽高比门；同类邻近歧义、远框、模板冲突、窗口过期和 generation/source
  变化均不会提交。
- 第一击继续发布 motion control，不修改 committed bbox、motion 或模板；第二个合法击中才
  使用真实确认 tracker bbox 一次性重置 motion，清空 hint/候选队列，发布
  `TRACKING / OBSERVATION` 和单帧 `reacquired=true`，路径标记为
  `search_reacquisition`。后续普通跟踪不会重复发出 reacquired pulse。
- 原子提交帧只 `STAGE_RECENT`，不替换 anchor；后续普通可靠帧累计
  `recent_probation_frames` 后才 `COMMIT_RECENT`，期间 anchor/recent 不一致则
  `ROLLBACK_RECENT`，但 anchor 跟踪可继续。runtime 测试确认第一击零 bbox/template commit，
  第二击恰好一次 bbox commit 与一次 recent stage。
- RED 阶段首先因缺少 `kSearchReacquisition` 编译失败；GREEN 后聚焦
  `long_term_controller|long_term_runtime|long_term_replay` 为 `3/3` 通过。完整构建及 CTest
  为 `19/19` 通过、`0` 失败、总时间 `3.22 sec`，`git diff --check` 无输出。
- 本里程碑仍未在 RK3588/RK3566 或真实云台上运行；尚缺 schema 10 遥测、通用配置候选标定、
  冻结 holdout 与板端三次复跑，不能宣称 190 帧前重捕门已通过。

## 2026-11-11T12:27:57+08:00 SEARCHING detector 调度与 shadow hint 隔离

- 在 `codex/occ-video-1-tracking`、Task 4 起点
  `422cab44043786c755fb81a36063e55e3aa795a3` 完成候选路径的 search-wide detector
  调度：目标类别允许恢复且 detector ready 时，从第一个已进入 SEARCHING 的帧开始按
  `detector_interval_ms` 毫秒节流提交全画面请求，不再等待 ASSISTED/LOST；旧默认关闭候选时
  仍保留原调度与 coast 行为。结果提交可用
  `git log -1 --format=%H -- rk3566/nanotrack_v3/include/long_term_types.hpp` 查询。
- 异步候选保留 source frame/timestamp、generation 和 candidate identity，并先用 detector
  请求时与当前帧的有界 motion 位移投影到当前走廊，再执行类别、位置、多候选容量与歧义门。
  新增 `SearchHint` 诊断记录 projected bbox、首击帧、确认计数、是否已使用与拒绝原因。
- 一个合法 detector/anchor 首击只生成恰好供下一已处理帧使用的 shadow hint；hint 可改变
  NanoTrack 搜索 ROI，但未接受帧的 `tracking_bbox`/`control_bbox` 继续来自 motion，且不会
  correct/initialize motion、提交 bbox 或更新模板。第二击在本 Task 仅增加诊断或标记冲突，
  原子重捕提交留给 Task 5；中止帧、冲突、数值异常都会清除 hint/active candidate。
- runtime 测试确认 detector poll 异常仍保留 `MOTION_SEARCH` 控制，且 bbox commit、recent
  stage/commit/rollback 均不发生。既有 stale、frame/generation、class、position、capacity、
  same-class ambiguity 和 legacy HOLD 拒绝顺序继续由原测试覆盖。
- RED 阶段因 projected bbox、SearchHint 与 search-wide 调度接口缺失编译失败；GREEN 后聚焦
  `long_term_controller|long_term_runtime` 为 `2/2` 通过。完整构建及 CTest 为 `19/19`
  通过、`0` 失败、总时间 `3.48 sec`，`git diff --check` 无输出。
- 本里程碑仍仅为主机合成验证；没有板端同步、构建或视频运行，不能表述为 detector 已在
  RK3588 上帮助重捕或 `occ_video_1` 已通过。下一步是 Task 5 的两击一致性原子提交与 recent
  template probation。

## 2026-11-11T12:20:56+08:00 通用时间制有界 motion search 主机实现

- 在 `codex/occ-video-1-tracking`、Task 3 起点
  `339d5e0a50fef15af8f9666f2664678d35d91e99` 以 TDD 增加默认关闭的
  `motion_search_enabled`，以及毫秒置信期、按画面对角线归一化的速度/加速度上限、每秒尺度
  变化上限和搜索探测间隔。配置解析、验证和 identity 均覆盖这些字段；结果提交可用
  `git log -1 --format=%H -- rk3566/nanotrack_v3/src/motion_estimator.cpp` 查询。
- motion estimator 只从已接受视觉观测修正速度、加速度和尺度速度，支持 16/33/67 ms
  变帧率与丢帧时间步，所有导数有界、预测框有限且裁进画面。公开 snapshot 给出运动导数、
  search age 和时间置信状态；时间倒退、未初始化及非有限状态继续 fail closed。
- 候选开关启用后，第一个未接受观测立即进入 `SEARCHING`，保留最近接受尺度并以 motion
  prediction 同时发布 `tracking_bbox`/`control_bbox`，在置信期内明确发布
  `observation_valid=false`、`control_valid=true`、`control_source=MOTION_SEARCH`；详细模式在
  UNCERTAIN/LOCAL/ASSISTED 间变化不会中断授权。置信期后仍是搜索态但撤销控制；两次一致的
  NanoTrack 视觉确认恢复 `OBSERVATION` 控制。高质量但满足遮挡缩框证据的观测不会被提交，
  也不会走旧的固定帧数 coast 授权。
- 每个搜索 episode 只递增一次 detector eligibility generation，并清空确认、候选和旧请求；
  重复拒绝不会重复递增。旧默认 `motion_search_enabled=false`，因此既有 RK3566 配置的普通拒绝
  与 frame-count coast 行为保持不变。
- RED 阶段三个聚焦目标因新增字段和接口不存在而编译失败；GREEN 后命令
  `ctest --test-dir /tmp/nanotrack-occ-plan -R 'controller_support|long_term_controller|long_term_replay' --output-on-failure`
  为 `3/3` 通过，随后完整构建及 CTest 为 `19/19` 通过、`0` 失败、总时间 `3.29 sec`，
  `git diff --check` 无输出。
- 本里程碑仅是主机合成状态矩阵证据，未同步、编译或运行 RK3588/RK3566，未改变 ONNX/RKNN
  模型或工件哈希，也不能表述为 `occ_video_1` 已通过、阈值已完成真值标定或真实云台验收。
  下一步仍需实现搜索期 detector shadow hint、两击重捕、schema 10，再按冻结 evaluator 在
  RK3588 完整复跑。

## 2026-11-11T12:13:38+08:00 观测有效性与控制来源外部语义契约

- 在 `codex/occ-video-1-tracking`、Task 2 起点
  `73efebb0130e6c94ee097e2c4613e2b8179dc59f` 以 TDD 新增
  `TrackingPhase {TRACKING, SEARCHING, INVALID}`、
  `ControlSource {NONE, OBSERVATION, MOTION_SEARCH}`，并把 `tracking_phase`、
  `observation_valid`、`control_source` 放入 `SearchPlan` 和 `ControllerOutput`。所有完成的
  controller 输出在返回前经过同一语义校验器；结果提交以
  `git log -1 --format=%H -- rk3566/nanotrack_v3/include/long_term_types.hpp` 查询。
- 直接接受观测现在明确发布 `TRACKING / observation_valid=true / OBSERVATION`，且
  `tracking_bbox==control_bbox`，display smoothing 继续独立；既有授权 coast 明确发布
  `SEARCHING / observation_valid=false / MOTION_SEARCH`。旧默认配置下的普通拒绝暂时显式为
  `SEARCHING / false / NONE / control_valid=false`，没有在 Task 3 的 feature gate 和有界运动置信期
  完成前静默切换 RK3566 默认控制行为。
- RED 阶段 `test_long_term_controller` 因枚举、字段和校验接口不存在而编译失败；GREEN 后聚焦
  `tracker_math`、`long_term_controller`、`long_term_runtime` 3/3 通过，完整 RKNN-independent
  构建及 CTest `19/19` 通过，`git diff --check` 通过。本里程碑只建立共享 core 语义；尚未写
  schema 10 日志字段，也尚未使普通未接受帧持续驱动云台。下一步由 Task 3 在通用候选配置
  显式启用时间制有界 motion search。

## 2026-11-11T12:09:22+08:00 `occ_video_1` evaluator 与失败基线冻结

- 在分支 `codex/occ-video-1-tracking`、实现起点
  `e51a1d2d63c34077b4489e74675dcef03aba5681` 完成实施计划 Task 1；结果提交以
  `git log -1 --format=%H -- scripts/evaluate_occ_video_1.py` 查询。本里程碑新增独立
  `scripts/evaluate_occ_video_1.py`，runtime 不读取它或真值。evaluator 严格核对 canonical
  video/metadata/events/status/label-manifest 哈希，将 169--181 排除普通 IoU，分层报告可见框、
  估计轨迹、完全遮挡语义、控制连续性、重捕和身份代理；合法输入但硬门失败 exit 1，输入或
  身份错误 exit 2。schema 9 仅允许复现失败基线，候选 PASS 必须使用带明确
  `tracking_phase`、`observation_valid`、`control_source` 和 tracking box 的 schema 10。
- TDD 首轮在生产脚本不存在时 8/8 预期失败；实现后聚焦套件最终 9/9 通过。真实 RK3588
  baseline CSV 只读复制到 `/tmp/occ-baseline-full-20261111-bboxes.csv`，SHA-256
  `b2ba9d7328f0554d1806a36f817ece8a42a5cdd0e4275b9cae44f1b87271eb6f`，与板端记录一致；
  evaluator exit 1 并精确复现普通可见 mean IoU `0.5785649329`、0--168
  `0.6311449519`、182--270 `0.4787219754`，无合法重捕且遮挡/控制/schema/可见框/身份代理门
  失败。紧凑证据在 `docs/evaluation/occ-video-1/baseline-201991a.{json,md}`；完整可再生报告
  SHA-256 `6ed8b993bd825b38dc033515a286e51e6283906f7e1449689d6fc9128ee8ce6a`。
- 系统 `python3` 全量 discover 因缺 `cv2` 等既有环境依赖只运行 79 项并产生 16 个导入错误；
  改用项目 NanoTrack venv 后实际运行 286 项，除本工作树已知绝对主工作树路径/缺本地 venv/
  缺历史 smoke report 的 `test_nanotrack_three_stage_configs` 5 failures + 1 error 外，其余通过。
  本任务新增套件独立全绿；这些既有 linked-worktree 测试边界未被改写或冒充全套通过。
- 本里程碑没有修改 C++ runtime、配置阈值、模型、协调方 `STATUS.md`/契约或板端部署；原基线
  目录仍缺 `manifest.json`，因此继续是诊断 FAIL，不是 board verifier PASS。下一步按计划 Task 2
  先以失败测试引入 observation/control 来源的外部语义契约。

## 2026-11-11T11:18:21+08:00 `occ_video_1` 方案 2 获批并形成实施计划

- 用户已批准
  `docs/superpowers/specs/2026-11-11-occ-video-1-closed-loop-tracking-design.md` 的最终设计，包含
  SEARCHING 期间持续运动预测与云台控制、190 帧前两阶段重捕、detector shadow hint、模板
  probation 以及避免单视频过拟合的泛化护栏。批准不等于算法或板端验收已经完成。
- 实施计划写入
  `docs/superpowers/plans/2026-11-11-occ-video-1-closed-loop-tracking.md`，按 evaluator、外部语义、
  时间制运动搜索、搜索期 detector、两次确认重捕、schema 10、通用可见光候选配置、0--189
  标定/扰动平台、冻结 holdout 和 RK3588 三次完整复跑九个任务执行。计划明确第一次暴露
  190--270 前必须提交冻结记录，若盲测结果促成修改则该段永久降级为 regression。
- 本里程碑仍只提交设计/计划/任务进度，没有修改 runtime 算法、阈值、模型或协调方专属
  `STATUS.md`/`integration-contract.yaml`，也没有产生新的主机测试或开发板运行证据。起点为
  `codex/occ-video-1-tracking@98ee995e0ef67f51bfb6114754eb3f43d7e0e909`；结果提交以
  `git log -1 --format=%H -- docs/progress/board-validation.md` 查询。

## 2026-11-11T10:53:22+08:00 `occ_video_1` 完整基线与方案 2 设计待批准

- 新任务在隔离工作树
  `/home/u20/.codex/worktrees/rk3588-debug-deploy/single-object-tracking` 的独立分支
  `codex/occ-video-1-tracking` 开始，输入基线为已推送 RK3588 Debug 提交
  `201991a92bba95b542c97505c37053d1c4fe1da8`。本里程碑只包含诊断与设计，尚未修改算法；
  结果提交以 `git log -1 --format=%H -- docs/progress/board-validation.md` 查询。
- 用户确认完全遮挡不算永久丢失：172--180 帧仍输出运动预测框并持续驱动云台；用户选择
  “真值 evaluator + 状态机/运动/detector/NanoTrack/模板闭环重构”的方案 2，并把正确重捕
  硬门定为第 190 帧之前。设计在
  `docs/superpowers/specs/2026-11-11-occ-video-1-closed-loop-tracking-design.md`，仍需用户批准后
  才能写实现计划或代码。
- 在 RK3588 `192.168.1.102` 对当前候选运行完整 271 帧基线，实际核心命令为：

```bash
cd /home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f
stdbuf -oL -eL ./build/nanotrack_v3 \
  --video data/occ_video_1/1.mp4 --bbox 992,582,37,39 --model-dir models \
  --sensor visible --controller-config config/visible-default.conf --max-frames 270 \
  --detector-model releases/yolov8n-visdrone9-rk3588-fp16-v1/models/yolov8n-visdrone9-rk3588-fp16.rknn \
  --detector-manifest releases/yolov8n-visdrone9-rk3588-fp16-v1/models/detector-manifest.json \
  --detector-labels releases/yolov8n-visdrone9-rk3588-fp16-v1/models/visdrone9.txt \
  --detector-class-id 2 --detector-conf 0.25 --detector-nms 0.45 \
  --output-video runs/occ-baseline-full-20261111/output.avi \
  --output-boxes runs/occ-baseline-full-20261111/bboxes.csv \
  --session-metadata runs/occ-baseline-full-20261111/session-metadata.json \
  > runs/occ-baseline-full-20261111/run.log 2>&1
```

- 运行 exit 0，SUMMARY `tracked_frames=270`、mean runtime `9.196 ms`、runtime FPS
  `108.744`、mean pipeline `111.414 ms`、end-to-end FPS `8.976`。按 frozen truth 排除
  169--181 后，可评价 258 帧 mean IoU `0.579`；0--168 为 `0.631`，182--270 为
  `0.479`。遮挡状态从未触发、detector 从未请求；174--178 和 180--184
  `control_valid=0`；182 帧 IoU `0.303`，185 帧才恢复普通接受且 `reacquired=0`。
- 证据目录
  `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/runs/occ-baseline-full-20261111`；
  `bboxes.csv`、`output.avi`、`run.log`、`session-metadata.json` SHA-256 依次为
  `b2ba9d7328f0554d1806a36f817ece8a42a5cdd0e4275b9cae44f1b87271eb6f`、
  `0002caa118473ee41e2c015e29728445f157ac2b2f70996d179670fe8c2906e4`、
  `511e02d7e6aca3eac432ceb472bd54fb7c964ea1ef31d362b3fe632260c138cf`、
  `08f34c2452ad379eed26cd2e14e7c8cfd58960bc2ad450ff2b74e65005c8f9a8`。
  本次诊断目录未生成 `manifest.json`，故 `verify_board.sh` 明确报
  `missing or empty artifact`；它是失败基线，不计正式 board PASS。
- 独立 builder 审查的五项歧义已写入设计：一次 detector hit 只允许 shadow search hint；
  改动进入共享 core 但只由独立 Debug 配置启用；identity 自动代理门后仍需人工视频签署；
  本轮是离线帧序功能验收而非 30 Hz 实时门；最终运行必须同时通过 evaluator、run manifest 和
  `verify_board.sh`。未修改协调方专属 `STATUS.md` 或 `integration-contract.yaml`。
- 用户随后明确要求避免对单视频过拟合，把本视频视为典型遮挡场景并保留泛化能力。设计因此
  删除 `occ_video_1` 专用配置和允许过拟合的表述，改用通用
  `visible-occlusion-calibrated.conf` 候选；增加 190--270 冻结 holdout、合成状态机矩阵、
  参数 `±10%`/计数 `±1` 稳定平台、初始框中心 `±2 px`/尺度 `±5%` 扰动和时序/异步扰动门。
  `occ_video_1` 明确禁止进入任何后续模型训练或训练期选择。

## 2026-11-10T19:21:14+08:00 RK3588 独立 Debug 迁移与实板运行

- 本轮按用户确认的“方案 1”在隔离工作树
  `/home/u20/.codex/worktrees/rk3588-debug-deploy/single-object-tracking` 执行；主线输入为
  `main@1bf657ff730891ae0bea48340a046cbcbeaa87d4`，实现链为
  `3c7801897372f63f7b94755806ea2c53f4e03648`（转换目标参数化）、
  `493a3f8429bec0e85bb48fd15598218fb38ab6b2`（RK3588 Debug 配置与校验器）、
  `b1dc8450b3a679e9f41a22a3d9e3af25a527de39`（C++ runtime 接受 RK3588 detector
  manifest）和 `cb2d0fab019bf44edd55b3f947506b8b389bf19f`（校验当前 SUMMARY 格式）。
  工作树为 detached HEAD；本条进度的结果提交以
  `git log -1 --format=%H -- docs/progress/board-validation.md` 查询。未修改协调方专属的
  `docs/progress/STATUS.md` 或 `docs/progress/integration-contract.yaml`。
- 目标板最终地址为 `192.168.1.102`，hostname `LKD3588`，硬件
  `Rockchip RK3588 NEARDI LKD3588 Board`，AArch64 Ubuntu 22.04、Linux 6.1.84，
  GCC 11.4.0、CMake 3.22.1、OpenCV 4.5.4；RKNPU driver `0.9.8`。系统
  `/usr/lib/librknnrt.so` SHA-256 为
  `0f1c5db6c649c8705759308aa56ecffc3295c7eb465f9ec9170882f44004aa8d`，实际运行日志
  API 为 `2.1.0 (967d001cc8@2024-08-07T19:28:19)`。编译头从已连接 RK3566 的
  `/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/include/rknn_api.h` 取得，SHA-256
  `c48e11a6f41b451a5fd1e4ad774ea60252d3d94f78bee9b21ea3d21b21deba9a`；只使用该头
  编译，未覆盖 RK3588 系统 runtime。

### ONNX 到 RK3588 模型身份

- 严格未复制 RK3566 RKNN。NanoTrack 作者仓库 HEAD
  `248663fde6bf7c40190cf10ee396d5662919ecd3`，权重 SHA-256
  `4f0d25108385a67bdec5fa01ca37c74acb72343b53996dd645fe18b86e512e69`；确定性导出 seed
  `20260929`、opset 14，ONNX checker 和 ORT parity 均通过。计划指定的旧
  `nanotrack` venv 为 Torch 1.7.1、最高稳定 opset 12，实际会报
  `Unsupported ONNX opset version: 14`；因此实际导出使用与仓库历史 opset-14 制品一致的
  `/home/u20/single-object-tracking/.venvs/rknn-convert/bin/python`（Torch 2.4.0、ONNX
  1.16.1、ORT 1.23.2），未降低 opset 或跳过数值校验。
- 实际转换核心命令为：

```bash
/home/u20/single-object-tracking/.venvs/rknn-convert/bin/python \
  rk3566/nanotrack_v3/tools/export_v3.py \
  --repo /home/u20/single-object-tracking/repos/siamtrackers/NanoTrack \
  --config /home/u20/single-object-tracking/repos/siamtrackers/NanoTrack/models/config/configv3.yaml \
  --weight /home/u20/single-object-tracking/repos/siamtrackers/NanoTrack/models/pretrained/nanotrackv3.pth \
  --output-dir /home/u20/single-object-tracking/artifacts/rk3588-debug-deploy/nanotrack \
  --seed 20260929 --opset 14 --atol 1e-4 --rtol 1e-4

/home/u20/single-object-tracking/.venvs/rknn-convert/bin/python \
  rk3566/nanotrack_v3/tools/convert_rknn.py \
  --model-dir /home/u20/single-object-tracking/artifacts/rk3588-debug-deploy/nanotrack \
  --source-commit 248663fde6bf7c40190cf10ee396d5662919ecd3 \
  --weight-sha256 4f0d25108385a67bdec5fa01ca37c74acb72343b53996dd645fe18b86e512e69 \
  --target rk3588

/home/u20/single-object-tracking/.venvs/rknn-convert/bin/python \
  rk3566/nanotrack_v3/tools/convert_yolov8_rk3566.py \
  --onnx /home/u20/single-object-tracking/artifacts/yolov8-visdrone9/export/yolov8n-visdrone9-rkopt.onnx \
  --output /home/u20/single-object-tracking/artifacts/rk3588-debug-deploy/detector/yolov8n-visdrone9-rk3588-fp16.rknn \
  --manifest /home/u20/single-object-tracking/artifacts/rk3588-debug-deploy/detector/detector-manifest.json \
  --labels /home/u20/single-object-tracking/artifacts/rk3588-debug-deploy/detector/visdrone9.txt \
  --source-url local://yolov8n-visdrone9-rkopt.onnx \
  --source-repository https://github.com/airockchip/ultralytics_yolov8 \
  --source-commit 4674fe6e003dfbc5f2250d3b39dd31faaf7a9877 --target rk3588 \
  --output-count 9 \
  --training-checkpoint /home/u20/single-object-tracking/artifacts/yolov8-visdrone9/export/yolov8n-visdrone9-best.pt \
  --training-repository https://github.com/airockchip/ultralytics_yolov8 \
  --training-commit 4674fe6e003dfbc5f2250d3b39dd31faaf7a9877 \
  --training-config /home/u20/single-object-tracking/artifacts/yolov8-visdrone9/runs/visdrone9-full/args.yaml \
  --dataset-audit /home/u20/single-object-tracking/artifacts/yolov8-visdrone9/dataset-audit.json
```

- 新生成的 RK3588 template/search/head SHA-256 依次为
  `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55`、
  `d3cb0e3879df6a1272adb59bf501324f1f8f55b58309d3c4da4a731b600973ee`、
  `4da379f54093d79f02775e90bfea33ebee0837c6fc73a324499279b4c57807c5`；tracker
  manifest 为 `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`，
  `provenance.target_platform=rk3588`。VisDrone9 RK3588 model/manifest/labels 依次为
  `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c`、
  `ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9`、
  `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`；detector
  manifest 为 RK3588、FP16/no quantization、9 outputs、9 classes，源 RKOPT ONNX SHA-256
  `b2948072f01da7fe3b686392a47fe742380c98775c4149111527b7ddf26a5cc8`。主机模型包
  `/home/u20/single-object-tracking/artifacts/rk3588-debug-deploy/SHA256SUMS` 全部通过；审计还确认
  新包文件名不含 `rk3566`，且没有复制 tracked RK3566 RKNN 路径。

### 原生构建与实板运行

- 隔离部署根为
  `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f`，创建前确认不存在。canonical
  输入 `data/occ_video_1/1.mp4` SHA-256
  `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`，1920x1080、
  30 FPS、271 帧；顶层数据清单 SHA-256
  `872483add3bb9ae3ddc4172b689e224ccf3e65b51bd0ab07268ee02957c3254f`。
  source archive `inputs/source-cb2d0fa.tar.gz` SHA-256
  `171d587d0abc0fa91d4587bc7ad9200bbced198f69118470afd1c89f8fe25be6`。后续源码更新解包
  显式排除 `models/*`，避免源码仓库中的历史 manifest 覆盖生成模型。
- 板端实际构建命令：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
  -DENABLE_RKNN_RUNTIME=ON \
  -DRKNN_INCLUDE_DIR=/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/include \
  -DRKNN_LIBRARY=/usr/lib/librknnrt.so
cmake --build build -- -j2
ctest --test-dir build --output-on-failure
```

  原生 Debug 构建成功；最新完整 CTest `19/19` 通过。`build/nanotrack_v3` 和
  `build/yolo_detector_smoke` SHA-256 分别为
  `64c3839cc4666b48d160e3d7482237979fabf2b96ec51b80888cb7658021008a`、
  `2437181e5d77817137f1af362ddfbdc2181ad488f4ed4d667e4b08c407560e1e`；`ldd`
  确认二者使用板载 `librknnrt.so`，无缺失依赖。完整配置、构建、CTest 和 ldd 日志均留在部署根。
- 以下命令均在
  `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f` 执行；这是本轮保留证据所用的完整
  detector、bounded、verifier 和交互应用参数（交互键盘输入由临时 ctypes/XTest helper 等价执行
  `Space`、等待 4 秒、`Q`，其结果保留在 `runs/interactive-debug/x11-input.log`）：

```bash
ffmpeg -v error -y -i data/occ_video_1/1.mp4 -frames:v 1 \
  runs/detector-smoke/frame0.png
stdbuf -oL -eL ./build/yolo_detector_smoke \
  --model releases/yolov8n-visdrone9-rk3588-fp16-v1/models/yolov8n-visdrone9-rk3588-fp16.rknn \
  --manifest releases/yolov8n-visdrone9-rk3588-fp16-v1/models/detector-manifest.json \
  --labels releases/yolov8n-visdrone9-rk3588-fp16-v1/models/visdrone9.txt \
  --image runs/detector-smoke/frame0.png --conf 0.25 --nms 0.45 --class-id 2 \
  --output-image runs/detector-smoke/annotated.jpg \
  --output-csv runs/detector-smoke/detections.csv \
  > runs/detector-smoke/run.log 2>&1

stdbuf -oL -eL ./build/nanotrack_v3 \
  --video data/occ_video_1/1.mp4 --bbox 992,582,37,39 --model-dir models \
  --sensor visible --controller-config config/visible-default.conf --max-frames 60 \
  --detector-model releases/yolov8n-visdrone9-rk3588-fp16-v1/models/yolov8n-visdrone9-rk3588-fp16.rknn \
  --detector-manifest releases/yolov8n-visdrone9-rk3588-fp16-v1/models/detector-manifest.json \
  --detector-labels releases/yolov8n-visdrone9-rk3588-fp16-v1/models/visdrone9.txt \
  --detector-class-id 2 --detector-conf 0.25 --detector-nms 0.45 \
  --output-video runs/occ-smoke/output.avi --output-boxes runs/occ-smoke/bboxes.csv \
  --session-metadata runs/occ-smoke/session-metadata.json \
  > runs/occ-smoke/run.log 2>&1

EXPECTED_RKNN_TARGET=rk3588 scripts/verify_board.sh \
  runs/occ-smoke models 61 1920 1080 \
  releases/yolov8n-visdrone9-rk3588-fp16-v1/models/yolov8n-visdrone9-rk3588-fp16.rknn \
  releases/yolov8n-visdrone9-rk3588-fp16-v1/models/detector-manifest.json \
  releases/yolov8n-visdrone9-rk3588-fp16-v1/models/visdrone9.txt \
  > runs/occ-smoke/verify.log 2>&1

timeout --signal=INT 30s env DISPLAY=:0 XAUTHORITY=/home/neardi/.Xauthority \
  stdbuf -oL -eL ./build/nanotrack_v3 \
  --config config/rk3588-interactive-debug.conf --bbox 992,582,37,39 \
  --max-frames 120 --output-boxes runs/interactive-debug/bboxes.csv \
  --session-metadata runs/interactive-debug/session-metadata.json \
  > runs/interactive-debug/run.log 2>&1
```

- detector 单帧 smoke 使用 canonical frame 0、class 2 `car`、conf/NMS `0.25/0.45`。
  模型成功初始化并报告 `target=rk3588`、API 2.1.0、driver 0.9.8、9 outputs；推理结果
  `raw=346`、保留上限 100 个 car 候选，preprocess/inference/postprocess/total 为
  `8.745/53.909/12.492/75.146 ms`。这证明模型加载和推理完成，不是 detector 精度或阈值
  质量结论。证据在 `runs/detector-smoke/`；`run.log`、`detections.csv`、
  `annotated.jpg` SHA-256 依次为
  `cb4351742e38787f13e7887e47f884852ee4441893612d1c288ec9ef16661c1c`、
  `22d6a9e758ce791d2cb4f875053b5527c0efa3e5bf2b58f7339f692a6c5018d5`、
  `10b793e67844d041c67b04ca9543a1a3eb925c0041722def7da92bd1634c9c46`。
- bounded canonical smoke 使用 bbox `992,582,37,39`、`--max-frames 60`、class 2 和上述模型。
  初始化帧加 60 个跟踪帧，共 61 行数据/61 帧视频；SUMMARY 为
  `tracked_frames=60`、mean runtime `8.489 ms`、runtime FPS `117.799`、mean pipeline
  `113.422 ms`、end-to-end FPS `8.817`。执行上述完整
  `EXPECTED_RKNN_TARGET=rk3588 scripts/verify_board.sh` 命令得到
  `PASS board artifacts output_frames=61 tracked_frames=60`。61 行中 detector direct-control
  行为 0；本短段稳定跟踪中 detector request/complete/reject/error 均为 0，因此不证明 detector
  重捕。证据在 `runs/occ-smoke/`；log/CSV/metadata/video/verify SHA-256 依次为
  `cf043fa92c8c546e499ee0d2b43c6da9e92b47a8fcaf1a7cfbb45468cc9301b1`、
  `f8bdfbe3d84d793f6acac176de845e4edebd3cc36f5b476dc29276ae4abf0fe8`、
  `5515a66da7125d89ed126845e7023dc58a5ec20a050321c0029344c471a748a6`、
  `0c6c852c045d9d517095e94db7beade7d48bdde94bc963949ee0efe799c2be41`、
  `34296832c1129c5694da72a7f307e3d46bcf7d1aa0b6d9dc391da36e6cc55bab`。
- X11 Debug 在 active `:0` 会话运行 `--config config/rk3588-interactive-debug.conf`，显式同一 bbox，
  通过 XTest helper 发送 Space 播放、4 秒后 Q 退出；应用与输入 helper 均 exit 0。日志记录
  `interactive_ready`、`action=play_pause`、30/60/90/120 帧结构化 `frame_status`、
  `frame_limit_reached`、`action=quit` 和 `shutdown reason=user_quit`，最终 120 tracked frames。
  证据在 `runs/interactive-debug/`；log/CSV/metadata SHA-256 依次为
  `2cc348f9e0d5a74ec14759dea97340e5c2582ef4a07bed6c344093ecee75ada4`、
  `e18aef2ba86ad8fef9e4fa6d3fa1c076255190ec3ddc167b1ab4898a4a15c4d4`、
  `20c3d1d9aaa33b82c452d4d5054f4bb998c8ff5f6371f66a1ef919329d079c77`。
  总清单 `runs/EVIDENCE-SHA256SUMS` 含 40 项；环境快照在 `runs/BOARD-ENVIRONMENT.txt`。
- 首次 detector smoke 因 C++ runtime 仍硬编码 RK3566 manifest 而失败，原日志保留为
  `runs/detector-smoke/run-initial-rk3566-hardcode-failure.log`，随后由 `b1dc845` 修复并复测。
  二次源码归档解包曾只覆盖 tracker manifest 为 RK3566（RKNN 文件实测仍为新 RK3588 哈希），
  该次不一致运行保留于 `runs/occ-smoke-invalid-rk3566-manifest/`，不计入通过证据；恢复并逐文件
  `cmp` RK3588 包后重新运行，最终 `runs/occ-smoke/` 才是有效证据。

### 证据边界与 Handoff

- 本轮只证明在指定 LKD3588 上：四个从 ONNX 新生成的 RK3588 模型身份一致、原生 Debug 构建、
  detector 单图推理、canonical 视频 60 帧集成运行、板端 schema/safety 校验和真实 X11 交互播放/退出
  均通过。它不满足当前 integration contract 的 RK3566 board gate，也不构成 detector promotion、
  production handoff、生产交叉编译、长时稳定性、遮挡重现质量、阈值/性能校准或真实吊舱验收。
  `functional_pass_promotion_held` 和既有 RK3566 `latest_handoff.available=false` 不因本轮改变。
- 请求协调对话决定是否在 `integration-contract.yaml` 中把 RK3588 建立为与 RK3566 分离、明确命名的
  Debug target，并审核提交链 `3c78018..cb2d0fa` 后再决定是否集成。若建立新目标，还需分别冻结
  RK3588 runtime/API/driver、四模型 manifest/hash、canonical 路径及独立验收门；不得用本轮实验结果
  替换既有 RK3566 合同身份。

## 2026-11-08T22:52:30+08:00 统一非训练测试数据板端落盘核验

- 协调基线为 `main@9d0085f63e5e332db2142bfff71bfcef5f20b8c7`；任务工作树
  `/home/u20/single-object-tracking/.worktrees/board-validation-progress-sync`、分支
  `codex/board-validation-progress-sync` 在核验前干净并快进到该提交。本轮只核验用户已上传的
  数据并更新本任务文件，没有运行算法、修改源码、模型、公共 `STATUS.md` 或
  `integration-contract.yaml`。
- 板端实际路径为 `/home/cat/Project/SOT/data/occ_video_1`。在真实 RK3566 板端执行
  `sha256sum 1.mp4 annotation_metadata.json occlusion_events.json annotation_status.csv labels.sha256 checksums.sha256`，
  得到：视频 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`、
  元数据 `5641650435a1980694f982cf1fa6715ba959b1515938cee383c8db8ec4aeb076`、
  遮挡事件 `a43f016c96b34c8a0ec6125dcb944ba4fa07a79d99a46ce696bbb29841ded256`、
  逐帧状态 `e44fd23514d6871e72d628ad4843fc90bb4246f306322ee297c8a2aa75818036`、
  标签清单 `9ec01854884126350f93e1af3a4aee002d38ce7c2d50c71c78de0f1a8dc0a648`、
  顶层清单 `872483add3bb9ae3ddc4172b689e224ccf3e65b51bd0ab07268ee02957c3254f`，
  与 schema v2 集成契约逐项一致。
- 板端继续执行 `sha256sum -c --quiet --strict checksums.sha256` 和
  `sha256sum -c --quiet --strict labels.sha256`，两者均退出成功；`labels.sha256` 为
  271 项，`labels/` 实际 271 个 `frame_*.txt`，共 271 行有效 YOLO 记录，均为
  `class_id=0` 且每行 5 字段。`annotation_status.csv` 为连续 frame 0～270 的 271 条
  状态记录；169～181 的来源均为 `bidirectional_interpolation`。
- 板端 `ffprobe` 实测 `1.mp4` 为 1920×1080、`30/1` FPS、271 帧。解析元数据与事件记录
  确认目标为完整 `car`，169～171 部分遮挡、172～180 完全遮挡、181 首次可见、182
  首次可靠重现、184 稳定重现，遮挡物为人行天桥。169～181 是人工确认估计框，
  `ordinary_iou_metrics_allowed=false`，后续普通 IoU 统计不得包含这些帧。

### Handoff

- 请求协调对话采纳板端实际路径
  `/home/cat/Project/SOT/data/occ_video_1` 及上述哈希，补全公共契约的
  `test_data.canonical_dataset.target_environment_path`。后续新增开发板非训练算法证据只使用
  该数据；`sot-v1.mp4` 和 `girl_dance.mp4` 只保留历史输入身份，不再新增验证结论。
- 本轮仅证明冻结数据已完整落盘并与契约一致，不证明算法运行、遮挡恢复、精度、性能、
  阈值标定、生产交叉编译或吊舱验收。任务仍保持
  `functional_pass_promotion_held`，`development_board_validation.latest_handoff.available=false`。

## 2026-10-31T10:43:00+08:00 最终源码事件脉冲板端复测

- 被测源码 `codex/adaptive-occlusion-coast@d589215f1c0319f2686d6364edfec86a67a0c63e`，工作树干净；相对 `e3ec19615a0d931544fa9567f288be62a0935551` 的 `git diff --binary e3ec196 HEAD -- rk3566/nanotrack_v3` SHA-256 `64a0f8ab085a4e067618944b9e3899b0e7cedbf6df392d5e72b971efd57b01c7`。本进度提交结果身份见 Git 日志。
- 已校验源码 bundle 两端 SHA-256 `25d601866477794fe6d06223c09a22aff0f66db20ec79ba04d13ae6d8f444623`；新建独立板端 worktree `/home/cat/Project/SOT/adaptive-coast-d589215`，干净 detached HEAD `d589215f1c0319f2686d6364edfec86a67a0c63e`，源码子树对象 `4956f404de2ff6e4b63649191c09861ecb4caf83`。原工作区和此前证据未修改。本次复算模型、manifest、labels 和两视频 SHA-256，均与下两节记录相同；控制配置 `visible-default.conf` SHA-256 `2010c6cd16553595844a12f103f53412942289262d866b9fadba26b2821337ed`，Debug 配置 SHA-256 `e3fe5f927ba3a47d2829dce600c01634f7f2f405576865c2edea22e0256493f8`。板端时间 `2026-10-31T10:38:10+08:00`；Linux 4.19.232 AArch64，动态链接 OpenCV 4.10、RKNN runtime 2.3.2，运行日志 API 2.3.2、driver 0.8.2。
- 新证据根 `E=/home/cat/Project/SOT/validation-adaptive-coast-d589215`，源码 `S=/home/cat/Project/SOT/adaptive-coast-d589215/rk3566/nanotrack_v3`；`A`、`R` 与下两节相同。执行 `cmake -S "$S" -B "$E/build" -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=ON -DOpenCV_DIR=/home/cat/ThirdParty/OpenCV4100/lib/cmake/opencv4 -DRKNN_INCLUDE_DIR=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/include -DRKNN_LIBRARY=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/aarch64/librknnrt.so`、`cmake --build "$E/build" -- -j2`、`(cd "$E/build" && ctest --output-on-failure)`：CTest `17/17`、15.74 秒。二进制 SHA-256 `1d09aa599db01c663762f4185a9d7b9a36d2ed6c6c0bbd88a40fbba4ad0e0886`；`configure.log`、`build.log`、`ctest.log` 依次 `bfba39142461f7f1a5338c4de643441dd222e45f7f87fd82ca85be62c48796b5`、`a1c1cc90dc9833a484068d84cbc87d6f6e63ef909bc06ecc785be783ca5b7468`、`bdcf7286abba9ebbadd3cb80b7f8e30e60cf94c015f3739ab8c4729f197ccdd6`。

两次非交互实际命令与两节后的完整命令相同，仅 `E` 和 `S` 换成本节新目录；视频、ROI、模型、配置、帧数、阈值、尺寸不变，均 exit 0。为直接观察仅在交互模式输出的结构化事件，另执行：

```bash
timeout --signal=INT 90s env DISPLAY=:0 stdbuf -oL -eL "$E/build/nanotrack_v3" --interactive --video "$A/data/girl_dance.mp4" --bbox 196,160,184,310 --model-dir "$A/models" --controller-config "$S/config/visible-default.conf" --detector-model "$R/yolov8n-visdrone9-rk3566-fp16.rknn" --detector-manifest "$R/detector-manifest.json" --detector-labels "$R/visdrone9.txt" --max-frames 200 --output-boxes "$E/girl-interactive.csv" --session-metadata "$E/girl-interactive.json" > "$E/girl-interactive.log" 2>&1
```

交互窗口初始化后由 `/tmp/adaptive_xplay.py` 使用 X11/XTest 发送空格播放，达到 200 帧上限后发送 q 正常退出，exit 0；最终辅助脚本 SHA-256 `dba4ccf1bbeaf045b41084450c1664c768ca876a6798656502eab2c68d4b466a`。视频证据来自非交互运行；交互运行保存 CSV、metadata、原始结构化日志。

`girl_dance` 的触发/探测/控制轨迹不变：34 帧 suspicion_start，39 帧 `GRADUAL_WIDTH_PEAK` age 1 且运动控制有效；43 帧 age 5 SEARCH 命中一次，44 帧 age 6 CONFIRM 被质量或运动几何拒绝，48 帧 age 10 SEARCH 同样拒绝；50 帧 age 12，51 帧 LOCAL_RECOVERY 且控制无效。交互结构化日志中该次 suspicion_start 在 34 帧仅一次，51 帧 LOCAL_RECOVERY 的 `occlusion_suspicion_started=0`，没有重复事件；该帧仍保留 `FULL_STABLE_MEDIAN`、参考宽高 `227.833588/278.244293` 和 `GRADUAL_WIDTH_PEAK`。整段共有三次 suspicion_start（34、54、124），后两次出现在恢复为 TRACKING 之后，是后续新的 suspicion 起点；不能声称整段只有一次。全部 201 行 hypothesis 为 MOTION、恢复 detector 请求为零，未见 probe 或 detector 直接授权控制；成功 early recovery 未观察。

`tracking-test-v1` 同样 120 帧，初始类别确立 request/complete/error `1/1/0`，仍 UNKNOWN/NO_MATCH；121 行含 35 个 ASSISTED_RECOVERY 行，恢复 request/submit/complete `0/0/0`，候选队列、锁均为零，hypothesis 全为 MOTION，未见 detector 控制。无真值视频只支持定性状态/控制观察。

`girl.{avi,csv,json,log}` SHA-256 依次为 `aee8ea7743978bb3495f497e2efca8f19d2d5e22f6882f8c3e1dffe25cf38b4a`、`0d20556c753a09794f1cc763ec0ee0bcaf15000d0e5f3a4ac2edb219b32ee23d`、`c6b7126db37cf8c80971f25864ed305a156c8155ab062b7b27354c89c0019ba7`、`083987093b3b3924574a8c1c7cb7303523d8c282b988a8f4998f7d716cca612c`；`girl-interactive.{csv,json,log}` 依次为 `dc4dde141340764e579bb0d431e73bf4d9c3ac4642c4d9d66abdb829f2211e8c`、`c7d5819be9573dcdbd4a95cfd4021a0bc91ab7ab44b22745d50cb73a06abe370`、`5429d883c779c39699e6bdb0fef42f82c53564f3faeb11cddc9d3f7adba4e8d7`；`tracking.{avi,csv,json,log}` 依次为 `1d1d369364bd623d8a98d393f19108103c8356bee42dc3ae44859b0504604d93`、`617b555f612cd3ee42556a418770dbf03d5fa617faae05add47f69646a5b1607`、`847c1ff270d144bc75ae4ba0d2a8b59d25b205c0425e627728b7b9b1b4883da4`、`001cf43552312b7df7277f2c526a2530c1e9ae7487dc588fdf82815fd6f906ec`。

### Handoff

请求协调方采纳本次精确源码板端证据；promotion 继续 held、`latest_handoff.available=false`。detector 正例确立及确认重捕、成功 early recovery、长稳及其他晋级门未闭环；没有真实吊舱验收。

## 2026-10-31T10:29:51+08:00 UNKNOWN 恢复请求门控修复的板端复测

- 复测源码 `codex/adaptive-occlusion-coast@e3ec19615a0d931544fa9567f288be62a0935551`，父提交为上次证据 `9f38674`；相对上次被测源码 `3890c50bc6aff067027d63f0140d798f783d472e` 的 `git diff --binary 3890c50 e3ec196 -- rk3566/nanotrack_v3` SHA-256 `1638b4d3dcdcc855b44f2f9a3dae92374c5d469dd3eb3b30190b893fc7fd2972`。本进度提交的结果身份见 Git 日志。
- 板端独立工作树 `/home/cat/Project/SOT/adaptive-coast-e3ec196` 干净，HEAD 精确为 `e3ec19615a0d931544fa9567f288be62a0935551`，源码子树对象 `4fbd95d4c6c192449e6427a5558397c6be45a52f`；传输 bundle 两端 SHA-256 `eebaeefe98b9a654792e7e630a5cfc2aa475541d24f9523f1941e201e4b2fa64`。原有 checkout 和上次证据目录未修改。新证据根 `E=/home/cat/Project/SOT/validation-adaptive-coast-e3ec196`，源码 `S=/home/cat/Project/SOT/adaptive-coast-e3ec196/rk3566/nanotrack_v3`；`A`、`R`、模型及两视频路径与下节相同，所有模型、manifest、labels 和视频 SHA-256 均于本次复核并与下节一致。`$S/config/visible-default.conf` 为 `2010c6cd16553595844a12f103f53412942289262d866b9fadba26b2821337ed`；Debug 配置为 `e3fe5f927ba3a47d2829dce600c01634f7f2f405576865c2edea22e0256493f8`，本次未传 Debug 配置。
- 板端时间已核对，Linux 4.19.232 AArch64；实际动态链接 OpenCV 4.10 和 RKNN runtime 2.3.2，运行日志 API 2.3.2、driver 0.8.2。执行 `cmake -S "$S" -B "$E/build" -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=ON -DOpenCV_DIR=/home/cat/ThirdParty/OpenCV4100/lib/cmake/opencv4 -DRKNN_INCLUDE_DIR=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/include -DRKNN_LIBRARY=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/aarch64/librknnrt.so`、`cmake --build "$E/build" -- -j2`、`(cd "$E/build" && ctest --output-on-failure)`；完整 CTest `17/17`、15.53 秒。`$E/build/nanotrack_v3` SHA-256 `9c533b18fba5e0f25b56d1c2f48ecf3ede16382e8e675382f44c54e1af83d508`；`configure.log`、`build.log`、`ctest.log` 分别为 `47689096a5c49cb9b8a46010d15fcf6828abb2d43565f2fc5645bb427758b9b4`、`777378efb93222698a54207383f810851819b88908269cad228941ba6c6d7830`、`180d7ff6c69acf98eed4404e4bf0e33b286d306bae2976deb1e5dd17843cfd78`。

复测两条实际命令与下节两条完全相同，除 `E` 和 `S` 使用本节新目录；均 exit 0，输入、ROI、模型、配置、阈值、帧数和输出尺寸不变。非交互输出视频截图保留在新证据目录，不是 GUI 截屏。

- `tracking-test-v1`：初始分类独立请求/完成/错误为 `1/1/0`，六个候选均未匹配，结果仍 UNKNOWN/NO_MATCH、恢复关闭。121 行含初始化，状态 TRACKING 52、UNCERTAIN 10、LOCAL_RECOVERY 24、ASSISTED_RECOVERY 35；恢复 detector 计划请求/实际提交/完成均 `0/0/0`（metadata 恢复 request/complete/error 为 `0/0/0`），旧版同场景提交过 3 次。全部行候选队列 0、candidate ID 空、locked 0、hypothesis MOTION；69 个非控制有效帧中包括全部辅助恢复帧，无 detector 候选激活或 detector 直接控制。该样本复测支持旧 UNKNOWN 下额外请求问题已解决；不代表所有板端场景或 detector 正例恢复通过。无真值视频只作为定性观察。
- `girl_dance`：200 跟踪帧、初始化分类请求/完成 `1/1`、UNKNOWN，恢复 detector 请求/完成 `0/0`。第 34 帧 suspicion 开始；39 帧 `GRADUAL_WIDTH_PEAK` age 1 且运动控制有效；43 帧 age 5 SEARCH 命中一次，44 帧 age 6 CONFIRM 被质量或运动几何拒绝，48 帧 age 10 SEARCH 被拒绝；第 50 帧 age 12，51 帧 LOCAL_RECOVERY 且控制无效。全 201 行均为 MOTION，HOLD/UNCERTAIN 无 detector 请求，未见 probe 直接授权控制。与上轮观察的滑行节奏一致；成功 early recovery 仍未观察。

`tracking.{avi,csv,json,log}` SHA-256 依次为 `1d1d369364bd623d8a98d393f19108103c8356bee42dc3ae44859b0504604d93`、`6429112e5bee74086115123661894227dab7a38ca971875930c3e465b5e8d603`、`0fbef4559311b13983582f7d0604e0d410d86063774bdec8cfe0684eb86040c4`、`ad6929735b9f31a85ecc3ec20de63628f28253b126df6decb4abb975a1461229`；`girl.{avi,csv,json,log}` 依次为 `aee8ea7743978bb3495f497e2efca8f19d2d5e22f6882f8c3e1dffe25cf38b4a`、`57a0d294fa8a15126ff8dd876c429e5249d7f1aa79eb83526a36478b3bee9ab8`、`47aeeb8206789a7460e81b75244417ee84502b6e81aa259fee475ac63e60fa10`、`efe077d9eebefd4f896fe22d3dbd4f413e7455084a5ee3d1b7efdd5324fc8643`。`tracking-initial.png` 为 `e348eff93a1ccb09ca6c9c366f7c8dbe80d85d172d3a2bad85684bb3ec2a33be`；`girl-observation-01.png` 至 `05.png` 依次为 `19b3bb71b46afe492b895755db45ef35b5421b1b9b49728bc4ce222fddc74ef5`、`5a166c086b67fb64eb76248b2955942007a1af5df9650600b22660c133eb6156`、`fe7659b5509b33417ce300ea36bb52320c6310b6e673795acab8db7124b42787`、`c4380d238a8517edb1975a0f341649c32f75d1c90160c36a8b0f5404cf320dde`、`80533a8ad85ad2d91374ccec238a564a7a6ff16e03b405ed7cd98967b28e8321`。

### Handoff

请求协调方采纳本次旧问题复测证据。仍保持 promotion held 和 `latest_handoff.available=false`：detector 正例确立、detector 确认重捕、成功 early recovery、长稳及其他晋级门尚未闭环；没有真实吊舱验收。

## 2026-10-31T10:17:30+08:00 自适应遮挡滑行板端验证：恢复请求问题待处理

- 源码工作树 `/home/u20/single-object-tracking/.worktrees/adaptive-occlusion-coast`，分支 `codex/adaptive-occlusion-coast`，验证基线 `3890c50bc6aff067027d63f0140d798f783d472e`；与 `main` 的 merge-base 为 `b827c0a6d8916e9c0832c748e30722f6f2fe5563`。`git diff <merge-base> HEAD --binary` 的 SHA-256 为 `632162a38530bacaf541179e3a6f6f8920dfb6825d850680a60c807681580fd9`。本进度提交以该基线为父提交；结果提交身份见 Git 日志。
- 新建主机构建 `/tmp/nanotrack-adaptive-coast-final`：`cmake -S rk3566/nanotrack_v3 -B /tmp/nanotrack-adaptive-coast-final -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF`、`cmake --build /tmp/nanotrack-adaptive-coast-final -- -j2`、`ctest --test-dir /tmp/nanotrack-adaptive-coast-final --output-on-failure` 均成功，CTest `17/17`、1.63 秒；`git diff --check` 成功。主机测试不是板端场景验收。
- 板端既有 `/home/cat/Project/SOT/nanotrack_v3_longterm` 有修改的 `config/interactive-debug.conf` 和未跟踪 `cmd.txt`，未改动。将提交打包为 `/tmp/nanotrack-adaptive-3890c50.bundle`，两端 SHA-256 均为 `a8bbd83e3a19a0f16662994b7c28b24d4583b9616bf66130b66081e839f5d7a9`；独立裸库 `/home/cat/Project/SOT/adaptive-coast-3890c50.git` 的 worktree `/home/cat/Project/SOT/adaptive-coast-3890c50` 为干净 detached HEAD `3890c50bc6aff067027d63f0140d798f783d472e`，源码子树对象 `ec83cf87b265557b49800be729a42ba05aca1e1d` 与主机一致。板端时间 `2026-10-31T10:12:29+08:00`，Linux 4.19.232 AArch64；实际加载 OpenCV 4.10 和 RKNN API 2.3.2、driver 0.8.2。
- 板端构建及证据根目录 `E=/home/cat/Project/SOT/validation-adaptive-coast-3890c50`；源码目录 `S=/home/cat/Project/SOT/adaptive-coast-3890c50/rk3566/nanotrack_v3`。执行 `cmake -S "$S" -B "$E/build" -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=ON -DOpenCV_DIR=/home/cat/ThirdParty/OpenCV4100/lib/cmake/opencv4 -DRKNN_INCLUDE_DIR=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/include -DRKNN_LIBRARY=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/aarch64/librknnrt.so`、`cmake --build "$E/build" -- -j2`、`(cd "$E/build" && ctest --output-on-failure)`；成功，板端 CTest `17/17`、16.01 秒。`$E/build/nanotrack_v3` SHA-256 `7d96500d2832c87c033b8e1afdd95e381d7b71c11c8618540cf80a48b9d2d3c8`；`configure.log`、`build.log`、`ctest.log` SHA-256 分别为 `a7645ce858f8db6463f998e5c9ee150ca6e27ece8fd145c5cce402ae3bb48ae0`、`94a2aed5a4740944b0ccb29390c53440db542a6fc1392390285bf57a519cb89c`、`63cd9c5ec6d283711b0bde6d65d5eab4e4b7c58c8a29546ae4ea7d3d647a91ed`。
- 输入根 `A=/home/cat/Project/SOT/nanotrack_v3_longterm`，`R=$A/releases/yolov8n-visdrone9-rk3566-fp16-v1/models`。实测 `$A/models/{template_backbone.rknn,search_backbone.rknn,head.rknn,manifest.json}` SHA-256 依次为 `38b60334e8e0f0999bfb7ff9e943090546461f9307682653685237e24b2a7709`、`92125f1ef6254dade5786bb999faa5b9d9412ceb2b7987f6dbba82f959b7ef6a`、`2065bfca503cca9197e67de1a754efad930dfed4d2fc73881c77d9e850dcb600`、`441e93f2092afe94bac663f27adb1fe99848357356efd67c7473b0b506f854b2`；`$R/{yolov8n-visdrone9-rk3566-fp16.rknn,detector-manifest.json,visdrone9.txt}` 依次为 `3698bc9bea1f43a4bac2826a09e03be0439fddfbc5c5604d8ed0f92b36881df0`、`05e6d8c7c210b7f9d85b8fb712aa00b014f310e46e85f171ce1158afc90ba63e`、`41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`，与已记录身份一致。`$A/data/girl_dance.mp4` 为 `8090fcd5090adb62f21e79e799f0b0871d9c610d8cf8087b9fcdd3c7e9c2e9b9`；`/home/cat/Project/SOT/data/tracking-test-v1.mp4` 为 `aa30402f63cf9fdf01f96a5907ad1010ecdd09098ff0e8435815d9c3ddb82d07`。提交内 `$S/config/interactive-debug.conf` 为 `e3fe5f927ba3a47d2829dce600c01634f7f2f405576865c2edea22e0256493f8`；实际使用的 `$S/config/visible-default.conf` 为 `2010c6cd16553595844a12f103f53412942289262d866b9fadba26b2821337ed`，不同于历史旧配置，原因是本源码提交新增 coast 参数。

### 场景命令与观察

两次均使用下列非交互方式、源码默认 detector 类别和阈值（-1、0.25、0.45），未传 Debug 配置；显式传入上述模型和控制配置。视频、CSV、metadata、日志和提取截图保留在 `$E`，均为真实板端运行，未改阈值：

```bash
timeout --signal=INT 180s stdbuf -oL -eL "$E/build/nanotrack_v3" --video "$A/data/girl_dance.mp4" --bbox 196,160,184,310 --model-dir "$A/models" --controller-config "$S/config/visible-default.conf" --detector-model "$R/yolov8n-visdrone9-rk3566-fp16.rknn" --detector-manifest "$R/detector-manifest.json" --detector-labels "$R/visdrone9.txt" --max-frames 200 --output-size 428x240 --output-video "$E/girl.avi" --output-boxes "$E/girl.csv" --session-metadata "$E/girl.json" > "$E/girl.log" 2>&1
timeout --signal=INT 240s stdbuf -oL -eL "$E/build/nanotrack_v3" --video /home/cat/Project/SOT/data/tracking-test-v1.mp4 --bbox 1395,1075,240,125 --model-dir "$A/models" --controller-config "$S/config/visible-default.conf" --detector-model "$R/yolov8n-visdrone9-rk3566-fp16.rknn" --detector-manifest "$R/detector-manifest.json" --detector-labels "$R/visdrone9.txt" --max-frames 120 --output-size 960x540 --output-video "$E/tracking.avi" --output-boxes "$E/tracking.csv" --session-metadata "$E/tracking.json" > "$E/tracking.log" 2>&1
```

两次均 exit 0。`girl_dance` 200 跟踪帧：第 34 帧 suspicion 开始；第 39 帧触发 `GRADUAL_WIDTH_PEAK`，age 1，`motion_control_authorized=1`、`control_valid=1`。第 43 帧 age 5 SEARCH 命中一次，第 44 帧 age 6 CONFIRM 因 `QUALITY_OR_MOTION_GEOMETRY` 拒绝，第 48 帧 age 10 SEARCH 同样拒绝；第 50 帧 age 12，51 帧进入 LOCAL_RECOVERY 且控制无效，52 帧 MOTION 重捕。该次没有成功的 early probe recovery；12 个 HOLD 帧及两个 UNCERTAIN 帧均无 detector 请求，所有 201 行 hypothesis 均为 MOTION，未见 detector 或 probe 直接授权控制。初始化目标类别 UNKNOWN/NO_MATCH，故不能证明 detector 确认重捕。截图 `$E/girl-observation-01.png` 至 `05.png` 分别取输出帧 39、43、44、48、51；这是输出视频截图，不是交互 GUI 截屏。

路口视频首帧 `$E/tracking-first.png` SHA-256 `f7a460dc17e165adb2e8aba72498c448f0ab2fc8b6df75499cc1fccdbf054088`；人工检查后选取前景深色轿车 ROI `1395,1075,240,125`，保存的初始框截图 `$E/tracking-initial.png` SHA-256 `e348eff93a1ccb09ca6c9c366f7c8dbe80d85d172d3a2bad85684bb3ec2a33be`。120 跟踪帧中 TRACKING 52、UNCERTAIN 10、LOCAL_RECOVERY 24、ASSISTED_RECOVERY 35（计数含初始化行）；未触发 HOLD 或 coast probe。此视频无真值，以上仅为定性状态/控制观察，不是精度、身份、恢复率或阈值校准证据。

**待处理：UNKNOWN 下仍提交恢复 detector 请求。** 路口视频初始化 frame 0 为 UNKNOWN/NO_MATCH、`detector_recovery_enabled=0`；但 ASSISTED_RECOVERY 的 frame 86、101、117 有 `detector_requested=1`、worker `submitted`，87、102、118 分别完成，过滤候选数 8、9、10，批状态 `ACCEPTED`。这不是初始化诊断：源码 `long_term_controller.cpp` 的 `begin_frame` 请求条件未检查 target class，`long_term_runtime.cpp` 随后实际提交；`ingest_detector_batch` 对 UNKNOWN 候选拒绝入队。全 121 行 `detector_candidates_queued=0`、candidate ID 为空、locked=0、hypothesis_source=MOTION；上述提交与完成帧 `control_valid=0`，未观察到 detector 候选激活或直接控制。该请求仍消耗资源，且与 UNKNOWN 关闭 detector 辅助恢复的要求存在冲突，须作为阻塞项由源码 owner 修复或明确裁决；不将本轮记为功能全通过。

`girl.{avi,csv,json,log}` SHA-256 依次 `aee8ea7743978bb3495f497e2efca8f19d2d5e22f6882f8c3e1dffe25cf38b4a`、`14e65e17d80c075cf47dd37f6d4f72ba19c16528fb6d58846f39bdded8636afc`、`6caaf6f4a520ad36254dada95f68d439126798e79d6f274cf1257b7431be18f7`、`48aa466ef62e62f4bf43b74decacc1738f5cfea93d392692ea9c8b8d049e62fa`；`tracking.{avi,csv,json,log}` 依次 `1d1d369364bd623d8a98d393f19108103c8356bee42dc3ae44859b0504604d93`、`3be6ea849dca076bf9804426eaa0f00df2de23ea890bd3220bb8307b765f138a`、`225886a840ae974adcb6e4ab3a9a0b5b8ed671b81619ff2f46aac4db1c6236c0`、`a438364e1f96a736679bccb0bd1d11f67d5f3ad1547617371f3f6228cdd0f879`。

### Handoff

保留 `functional_pass_promotion_held` 的历史状态，不把本次源码候选标为功能全通过；`development_board_validation.latest_handoff.available=false`。请求协调方采纳本证据并安排 UNKNOWN 时请求门控问题的修复与板端复测。正例 detector 确立、detector 确认重捕、成功 early probe recovery、长稳及其他 promotion 门仍未通过；未执行真实吊舱测试。

- 任务状态：`functional_pass_promotion_held`
- 最近核对：`2026-10-26T19:53:37+08:00`
- 核对基线：`main@bb297504070b3dfcf2fd4b4da05ebb1503e309db`
- 任务工作树：`/home/u20/single-object-tracking/.worktrees/debug-runtime-config`
- 任务分支：`codex/debug-runtime-config`
- 责任边界：在交叉编译之前，使用具备完整编译环境的真实 RK3566 开发板，对交互式 Debug 版本执行原生编译、调试、加载/推理、集成视频、控制安全、时延、资源和稳定性测试。

本任务是开发阶段的板端验证，不消费交叉编译生产产物。交叉编译只在本任务冻结并移交最新可用结果后进行；真实吊舱测试由人完成。

## 2026-10-22 首次协调复核

- 主工作树 `/home/u20/single-object-tracking` 在 `main` 上，`HEAD` 正好是协调基线
  `29b083e4e6ced2fd46eb7f9ecf821287b4cc401a`；首次只读检查时没有已暂存、
  未暂存或未跟踪修改，`HEAD...29b083e` 的左右提交数为 `0/0`，因此没有执行
  merge、reset、覆盖或文件搬运。
- 本任务从该基线创建独立工作树和分支。历史控制器源提交
  `1552b7d14f67006768757dff039327253e3a60cb` 是当前基线祖先；交互式 Debug
  分支当前停在 `b98b1335c3d263f542fd52656c7d7c4d58b2bb77`，但它不是协调基线祖先，
  尚不能把两者合称为一份已集成、可移交的源码身份。
- `integration-contract.yaml` 锁定的 VisDrone9 RKNN/manifest/labels 大文件不在
  Git 工作树中，符合“大文件不入库”规则；本轮只能核对契约和历史报告中的哈希，
  不能把路径记录当作当前文件哈希复验。
- 尝试使用当前约定命令
  `ssh -p 10022 cat@127.0.0.1 'printf connected'` 连接开发板，实际返回
  `ssh: connect to host 127.0.0.1 port 10022: Connection refused`。用户随后恢复
  隧道；复测时 TCP 已成功连接到 `127.0.0.1:10022`，但 12 秒内未收到 SSH
  banner，返回 `Connection timed out during banner exchange`。这证明 Linux 端
  reverse-forward listener 已存在，但不能证明 Mac 到 `192.168.103.172:22` 的
  后半段可达。因此本轮仍未重新读取板端 Git、二进制、模型、运行时或保留工件，
  也没有生成新的板端 PASS。
- 对协调基线执行 RKNN-independent 主机回归：临时目录配置
  `cmake -S rk3566/nanotrack_v3 -B <tmp> -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF`，
  随后构建并运行 `ctest --test-dir <tmp> --output-on-failure`，结果为 `12/12`
  通过。这仅证明当前主线的主机逻辑基线可构建和测试，不是开发板证据。

本轮实际任务状态仍为 `functional_pass_promotion_held`。此前板端功能和控制安全
证据继续作为历史保留证据，但在反向隧道恢复并重新核对工件身份前，不升级为
“当前已复验”；`development_board_validation.latest_handoff.available=false` 仍然正确。

## 2026-10-22 隧道恢复后的当前板端复核

- 用户接通 Mac 与开发板的网线后，`ssh -p 10022 cat@127.0.0.1` 恢复。板端为
  LubanCat RK3566、AArch64、Debian 10、Linux `4.19.232`。原生工具链为 GCC
  `8.3.0`、CMake `3.13.4`、Git `2.20.1`；二进制动态链接
  `/home/cat/ThirdParty/OpenCV4100/lib/libopencv_world.so.410` 和 RKNN 2.3.2
  `librknnrt.so`，`ldd` 未报告缺失依赖。
- 板端仓库 `/home/cat/Project/SOT/nanotrack_v3_longterm` 位于
  `codex/rk3566-interactive-debug@d9f200ce26ade0f7664a9c853230f277c28da47b`，
  tracked tree 干净，仅有用户未跟踪文件 `cmd.txt`，本轮未修改。该板端仓库使用
  独立 Git 历史；逐文件对象比较表明，除板端额外的根 `.gitignore` 外，其 tracked
  源码与共享仓库 `b98b1335c3d263f542fd52656c7d7c4d58b2bb77:rk3566/nanotrack_v3`
  一致。
- 当前 Debug 二进制
  `/home/cat/Project/SOT/nanotrack_v3_longterm/build-interactive-debug-keyfix/nanotrack_v3`
  是 AArch64 PIE，SHA-256 为
  `c4436db4a7f2491483d5e66e4ce22a98feb710ae72dfe5203cb73793d043f8e9`。
  现有构建目录执行 `ctest --output-on-failure` 为 `13/13` 通过。
- 当前 NanoTrack 模型 SHA-256：template
  `38b60334e8e0f0999bfb7ff9e943090546461f9307682653685237e24b2a7709`、search
  `92125f1ef6254dade5786bb999faa5b9d9412ceb2b7987f6dbba82f959b7ef6a`、head
  `2065bfca503cca9197e67de1a754efad930dfed4d2fc73881c77d9e850dcb600`；
  tracker manifest 为
  `441e93f2092afe94bac663f27adb1fe99848357356efd67c7473b0b506f854b2`。
- VisDrone9 RKNN/manifest/labels 的板端 SHA-256 分别为
  `3698bc9bea1f43a4bac2826a09e03be0439fddfbc5c5604d8ed0f92b36881df0`、
  `05e6d8c7c210b7f9d85b8fb712aa00b014f310e46e85f171ce1158afc90ba63e`、
  `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`，
  与 `integration-contract.yaml` 完全一致；板端 manifest verifier 报告
  `target=rk3566 outputs=9 classes=9`。
- 使用 release 中样本 `0000330_00801_d_0000804.jpg` 执行当前 VisDrone9 person
  smoke：RKNN API `2.3.2 (429f97ae6b@2025-04-09T09:09:27)`、driver `0.8.2`，
  9 个输出张量与 manifest 一致；`raw=19`、person candidates `7`，preprocess /
  inference / postprocess / total 为 `26.833 / 177.154 / 23.459 / 227.446 ms`。
  输出位于 `/tmp/board-validation-detector-smoke.C97Y8A`，CSV SHA-256
  `499051ec07bbe03ecc6c7dbfa97f1df87c9882495fc52da9e0da51f72e4eee06`，
  标注图 SHA-256
  `3037104c175564fb411fab182381a255d9be9dc5aa54038c51de2d260821b7b9`；
  CSV 与历史 person-filter 证据一致。这是 `detector_function` 复核，不是精度或恢复验收。
- 用当前 Debug 二进制、上述 NanoTrack/VisDrone9 工件、class 0 和
  `girl_dance.mp4`（SHA-256
  `8090fcd5090adb62f21e79e799f0b0871d9c610d8cf8087b9fcdd3c7e9c2e9b9`）
  执行 3 tracked-frame 非交互烟测。结果 `mean_runtime_ms=33.154`、
  `mean_pipeline_ms=67.102`、`control_valid=true` 且无错误；4 帧 428x240 AVI、5 行
  CSV 和 metadata 保留在 `/tmp/board-validation-integrated-smoke.PFYuzG`，SHA-256
  分别为 `f0776f9ae6bf189e2595ce0d9c5ff03fcb8f5afddfd9a3eca16af3786c0bad51`、
  `17711c767032272324a3675cdbb0dae1f33a8b152491bf056301641b0a2f914a`、
  `938e3bcfa74ced584a8e907f7184b81237d3d3aa8d080be0b2bd9d63917e9737`。
  由于稳定
  跟踪期间 detector request/complete 为 `0/0`，此结果只证明 `model_load` 和短程
  `integrated_function`，不证明 detector 队列、重捕或长时稳定性。
- 板卡重启后 RTC 仍为 `2019-02-14`；首次登录时系统时间也为 2019，随后 `ntpd`
  已把系统时间纠正为 2026，且 `ntpq -pn` 有选中 peer，但 `timedatectl` 仍显示
  `System clock synchronized: no`、RTC 仍为 2019。任何原生增量构建前必须先确认
  系统时间已纠正，避免未来时间戳导致陈旧对象未重编译。

本轮板端实际验证命令的核心部分为：

```bash
cd /home/cat/Project/SOT/nanotrack_v3_longterm/build-interactive-debug-keyfix
ctest --output-on-failure

/home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/build/yolo_detector_smoke \
  --model /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/yolov8n-visdrone9-rk3566-fp16.rknn \
  --manifest /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/detector-manifest.json \
  --labels /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/visdrone9.txt \
  --image /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/samples/0000330_00801_d_0000804.jpg \
  --conf 0.25 --nms 0.45 --class-id 0 \
  --output-image /tmp/board-validation-detector-smoke.C97Y8A/person.jpg \
  --output-csv /tmp/board-validation-detector-smoke.C97Y8A/person.csv

/home/cat/Project/SOT/nanotrack_v3_longterm/build-interactive-debug-keyfix/nanotrack_v3 \
  --video /home/cat/Project/SOT/nanotrack_v3_longterm/data/girl_dance.mp4 \
  --bbox 244,161,74,70 \
  --model-dir /home/cat/Project/SOT/nanotrack_v3_longterm/models \
  --output-video /tmp/board-validation-integrated-smoke.PFYuzG/output.avi \
  --output-boxes /tmp/board-validation-integrated-smoke.PFYuzG/bboxes.csv \
  --max-frames 3 --output-size 428x240 --sensor visible \
  --controller-config /home/cat/Project/SOT/nanotrack_v3_longterm/config/visible-default.conf \
  --session-metadata /tmp/board-validation-integrated-smoke.PFYuzG/session-metadata.json \
  --detector-model /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/yolov8n-visdrone9-rk3566-fp16.rknn \
  --detector-manifest /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/detector-manifest.json \
  --detector-labels /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/visdrone9.txt \
  --detector-class-id 0 --detector-conf 0.25 --detector-nms 0.45
```

当前 live 复核确认了现有 Debug 产物的工件身份、模型加载、detector 单图功能、
短程集成功能和 13 项测试；没有改变 `functional_pass_promotion_held` 裁决，也没有
产生可供交叉编译消费的 latest handoff。

## 2026-10-23 Debug 运行配置板端验证

本轮只改造交互式 Debug 的启动参数入口，不修改跟踪、重捕、模板更新或控制授权
逻辑。经用户确认，采用独立的严格 `key=value` 运行配置和
`nanotrack_v3 --config FILE`，没有引入 YAML 依赖；原命令行继续兼容，显式 CLI
值覆盖配置值，配置中的相对路径以配置文件目录为基准。detector 仍只是候选来源，
不能直接产生 `control_valid=true`。

### 源码与板端身份

- 当前协调基线为
  `main@bb297504070b3dfcf2fd4b4da05ebb1503e309db`。独立分支
  `codex/debug-runtime-config` 在该基线上显式合并既有交互 Debug 候选
  `b98b1335c3d263f542fd52656c7d7c4d58b2bb77`；设计/计划提交为
  `f5cedeb2414473bb6bbf736b8026308688bcec24`，实现提交为
  `eb50d6788c477733697703d6b1e8b9f6e4800cd1`。
- 板端仓库使用独立历史。实现 patch SHA-256 为
  `863e2cd0fd37fd956d928faebea0ffbd3a8877b71f64f70b7951832560d7d4f4`；首次
  `git am --3way` 形成 `a45d563e7e18134400b66313b2db8d7c8f9f5f41` 时，三个新增文件因共享
  仓库子目录前缀被放入嵌套路径。未使用 reset；随后以可追溯提交
  `41aa1ffe703416ea442afa4582d5346d9475f254` 将它们移动到板端仓库根目录。
  最终 tracked 状态干净，仍只有既有用户文件 `cmd.txt` 未跟踪，本轮未修改。
- 共享提交子树与板端 `HEAD` 的 55 个构建相关条目（`CMakeLists.txt`、
  `config/`、`include/`、`src/`、C++ 测试、board schema 测试和 verifier）
  `git ls-tree` 指纹均为
  `9dc0ccb607672b0863f7805c1790db153c880ad2c68d95125cdfeb6ed8e840eb`。
  两个仓库的完整 tracked tree 不相同：板端独立历史不包含主线后增的训练文档、
  Python 训练工具和相应测试；因此这里只声明板端构建/测试输入等价，不声明完整
  仓库树等价。

### 配置、工件和运行环境

- 板端配置为
  `/home/cat/Project/SOT/nanotrack_v3_longterm/config/interactive-debug.conf`，
  SHA-256
  `9e57e3a7e8a13212b46a8b21daba08836c0ed41287f3cb76a73562b57e7551f1`。
  它启用交互模式、可见光 controller 和 VisDrone9 person candidate，不设置
  `bbox`，也不设置 video/CSV/metadata 输出路径；因此启动后暂停等待鼠标框选，
  调试输出默认不保存。
- 输入 `data/girl_dance.mp4` SHA-256 为
  `8090fcd5090adb62f21e79e799f0b0871d9c610d8cf8087b9fcdd3c7e9c2e9b9`。
  NanoTrack template/search/head/manifest SHA-256 依次为
  `38b60334e8e0f0999bfb7ff9e943090546461f9307682653685237e24b2a7709`、
  `92125f1ef6254dade5786bb999faa5b9d9412ceb2b7987f6dbba82f959b7ef6a`、
  `2065bfca503cca9197e67de1a754efad930dfed4d2fc73881c77d9e850dcb600`、
  `441e93f2092afe94bac663f27adb1fe99848357356efd67c7473b0b506f854b2`。
- VisDrone9 RKNN/manifest/labels SHA-256 依次为
  `3698bc9bea1f43a4bac2826a09e03be0439fddfbc5c5604d8ed0f92b36881df0`、
  `05e6d8c7c210b7f9d85b8fb712aa00b014f310e46e85f171ce1158afc90ba63e`、
  `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`，
  与公共契约一致；本配置使用候选不等于 canonical promotion。
- 本轮板端系统时间为 `2026-10-23T15:13:21+08:00` 且 NTP 同步为 yes；环境仍为
  LubanCat RK3566/AArch64、Linux `4.19.232`、GCC `8.3.0`、CMake `3.13.4`、
  Git `2.20.1`。后续重启后仍需重新检查 RTC/NTP。

### 原生编译与实测结果

首次新建的 `build-interactive-debug-config` 未显式设置 `OpenCV_DIR`，CMake 自动
选择系统 OpenCV 3.2；虽然编译和 13 项测试可通过，实际启动在 OpenCV 3.2 解析
detector JSON manifest 时失败。该目录是无效诊断产物，不能作为本轮结果。根因经
cache、`ldd` 和 manifest 首字节对照确认；manifest 本身以合法 JSON `{` 开头。

有效 clean build 使用另一个全新目录并显式绑定既有 OpenCV 4.10：

```bash
cmake -S /home/cat/Project/SOT/nanotrack_v3_longterm \
  -B /home/cat/Project/SOT/nanotrack_v3_longterm/build-interactive-debug-config-opencv4100 \
  -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=ON \
  -DOpenCV_DIR=/home/cat/ThirdParty/OpenCV4100/lib/cmake/opencv4 \
  -DRKNN_INCLUDE_DIR=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/include \
  -DRKNN_LIBRARY=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/aarch64/librknnrt.so
cmake --build /home/cat/Project/SOT/nanotrack_v3_longterm/build-interactive-debug-config-opencv4100 -- -j2
cd /home/cat/Project/SOT/nanotrack_v3_longterm/build-interactive-debug-config-opencv4100
ctest --output-on-failure
```

观察结果：原生编译成功，CTest `13/13` 通过；有效 AArch64 PIE 为
`/home/cat/Project/SOT/nanotrack_v3_longterm/build-interactive-debug-config-opencv4100/nanotrack_v3`，
SHA-256
`e469aa57a209c415f083a1149824d95b6dcd62c3381df1901f4c5a31acf7f33a`。
`ldd` 确认链接
`/home/cat/ThirdParty/OpenCV4100/lib/libopencv_world.so.410` 和 RKNN 2.3.2
`librknnrt.so`，未观察到缺失依赖。

实际简化启动烟测为：

```bash
cd /home/cat/Project/SOT/nanotrack_v3_longterm
timeout --signal=INT 15s env DISPLAY=:0 \
  ./build-interactive-debug-config-opencv4100/nanotrack_v3 \
  --config config/interactive-debug.conf
```

程序 15 秒内没有配置、路径、模型、manifest 或 GUI 启动错误，最终由 `timeout`
按预期结束并报告 `CONFIG_SMOKE_EXIT=124`。这证明实际板端二进制能够消费单一配置
参数并保持在交互运行状态；结合板端 `test_cli`，确认 shipped config 从任意 cwd
解析、默认无输出且等待鼠标 ROI。该有界启动没有人工框选、没有执行跟踪帧，也没有
证明 detector 推理、遮挡重捕、长时稳定性或跟踪质量。

### 本轮裁决与 Handoff

本轮层级为 `native_build + config_parse + interactive_startup` 功能通过，不改变总体
`functional_pass_promotion_held`。共享实现提交尚未合入 `main`，公共契约也尚未把
它锁定为下一轮 source candidate；因此
`development_board_validation.latest_handoff.available` 必须继续为 `false`，交叉编译
任务不得消费本结果。建议协调对话审查并集成
`eb50d6788c477733697703d6b1e8b9f6e4800cd1` 后，再决定是否将其设为下一轮板端候选。

## 2026-10-23 结构化 Debug 遥测板端验证

- 时间：`2026-10-23T22:39:00+08:00`。
- 共享分支/工作树：`codex/debug-runtime-config` /
  `/home/u20/single-object-tracking/.worktrees/debug-runtime-config`。
- 本功能基线为 `fb15ba310fa13a26741a22120bacf19b3a3d8aea`；纯遥测组件提交为
  `9289f07f528641ee014f8d1aa94e871acdea0ada`，交互主循环接入提交为
  `d868636f8332a58f7523ad0b941f258a7b07ffc5`。本进度文件的结果提交以
  `git log -1 --format=%H -- docs/progress/board-validation.md` 查询，避免在提交前虚构
  自身 commit ID。
- 共享源码到板端的 patch SHA-256 为
  `599fc21942fdef26319eb9d4f7538d62acf00999618b5c25cc73dee1d63b5dfa`。
  patch 只含 8 个 CMake/C++/测试文件，不含模型、视频、日志或构建产物。

### 板端隔离与源码身份

原板端 checkout `/home/cat/Project/SOT/nanotrack_v3_longterm` 在同步前已有用户状态：
`config/interactive-debug.conf` 将输入改为
`/home/cat/Project/SOT/nanotrack_v3_longterm/data/1.mp4`，另有未跟踪
`cmd.txt`。首次在该 checkout 应用源码并构建后，CTest 为 `13/14`，唯一失败是
`cli` 的 shipped-config 路径断言；根因就是上述配置修改，不是遥测代码。没有覆盖
或提交这两个用户文件；已精确反向应用本任务 patch，使原 checkout 恢复为仅这两项
用户状态。

正式验证改在板端独立工作树
`/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry`、分支
`codex/rk3566-debug-telemetry` 中进行，基线为
`41aa1ffe703416ea442afa4582d5346d9475f254`，板端结果提交为
`0f155ab7dcde33582534f1d87e61a3453167b825`，tracked 状态干净。该提交只增加
结构化终端遥测及其测试，不修改 `long_term_controller.cpp`、阈值、搜索计划、
模板决策或 detector 控制授权逻辑。

### 原生构建、测试和工件

板端系统时间已由 NTP 同步；环境为 LubanCat RK3566/AArch64、Linux
`4.19.232`、GCC `8.3.0`、CMake `3.13.4`、Git `2.20.1`。干净构建命令为：

```bash
cd /home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry
cmake -S . -B build-board \
  -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=ON \
  -DOpenCV_DIR=/home/cat/ThirdParty/OpenCV4100/lib/cmake/opencv4 \
  -DRKNN_INCLUDE_DIR=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/include \
  -DRKNN_LIBRARY=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/aarch64/librknnrt.so
cmake --build build-board -- -j2
(cd build-board && ctest --output-on-failure)
```

观察结果：原生编译成功，CTest `14/14` 通过，其中新增
`debug_telemetry` 测试覆盖首次状态、状态/控制变化、单步强制心跳、连续播放每
30 帧心跳、detector request/batch/error、候选 ID/区域缺失、同类干扰、锁定和
2/3 确认、重捕来源、模板动作及重新初始化 reset。有效 AArch64 PIE 为
`/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/build-board/nanotrack_v3`，
SHA-256 为
`11c70e81ac2beb6305c923bee5bbff61f0969d1d82e16548d9f7393b017f04dd`。
`ldd` 绑定 `/home/cat/ThirdParty/OpenCV4100/lib/libopencv_world.so.410` 与
`/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/aarch64/librknnrt.so`，没有
`not found`。

### 交互烟测

使用 canonical `config/interactive-debug.conf`，并通过显式 CLI 覆盖把大工件指向
原板端 checkout；没有把大文件复制到新工作树。核心启动参数为：

```bash
cd /home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry
timeout --signal=INT 35s env DISPLAY=:0 stdbuf -oL -eL \
  ./build-board/nanotrack_v3 --config config/interactive-debug.conf \
  --video /home/cat/Project/SOT/nanotrack_v3_longterm/data/girl_dance.mp4 \
  --bbox 244,161,74,70 \
  --model-dir /home/cat/Project/SOT/nanotrack_v3_longterm/models \
  --controller-config /home/cat/Project/SOT/nanotrack_v3_longterm/config/visible-default.conf \
  --detector-model /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/yolov8n-visdrone9-rk3566-fp16.rknn \
  --detector-manifest /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/detector-manifest.json \
  --detector-labels /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/visdrone9.txt \
  --max-frames 40
```

板端没有 `xdotool`，因此使用只存在 `/tmp` 的 ctypes/XTest 脚本向现有 X11
窗口依次发送 `R`、`Esc`、`N`、`Space`、`Space`、`Q`；未安装系统包。
脚本 SHA-256 为
`522a09537f16534e94bd44ceec1ae83ccfaccb89250fe05d2ae64eb245e283ec`。
日志 `/tmp/nanotrack-debug-telemetry-smoke.log` 共 47 行，SHA-256 为
`a4ced575095a1e4207dedeb22fab17ac09c36e525fc6b15aaa008f363661384d`。

实际观察到：

- 启动时打印视频、分辨率/FPS、模型目录、detector 开关/类别/模式、标定状态和
  `video=disabled boxes=disabled metadata=disabled`；调试默认仍不保存输出。
- `R -> Esc` 打印 `select_roi`、`roi_selection_started` 和
  `roi_selection_cancelled`；`N` 在第 1 帧立即打印完整 `frame_status`。
- 连续播放在第 30 帧打印周期心跳，并分别在第 30/33 帧打印
  `STAGE_RECENT`/`COMMIT_RECENT`；第 40 帧达到限制后由 `Q` 正常退出，退出原因为
  `user_quit`。
- 日志错误扫描为空；最终摘要为 40 tracked frames，mean runtime
  `31.909 ms`、mean pipeline `37.131 ms`。这是短程 Debug 烟测时序，不是正式
  性能基准或长稳资源结论。

本次沿用的 `girl_dance.mp4`、NanoTrack template/search/head/manifest 和
VisDrone9 detector RKNN/manifest/labels SHA-256 依次为
`8090fcd5090adb62f21e79e799f0b0871d9c610d8cf8087b9fcdd3c7e9c2e9b9`、
`38b60334e8e0f0999bfb7ff9e943090546461f9307682653685237e24b2a7709`、
`92125f1ef6254dade5786bb999faa5b9d9412ceb2b7987f6dbba82f959b7ef6a`、
`2065bfca503cca9197e67de1a754efad930dfed4d2fc73881c77d9e850dcb600`、
`441e93f2092afe94bac663f27adb1fe99848357356efd67c7473b0b506f854b2`、
`3698bc9bea1f43a4bac2826a09e03be0439fddfbc5c5604d8ed0f92b36881df0`、
`05e6d8c7c210b7f9d85b8fb712aa00b014f310e46e85f171ce1158afc90ba63e`、
`41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`。
RKNN API 为 `2.3.2`，driver 为 `0.8.2`，detector 实际加载 9 个输出。

### 裁决与 Handoff

本轮新增层级为 `native_build + interactive_debug_telemetry_smoke` 功能通过，且没有
改变 detector 候选不能直接产生 `control_valid=true` 的安全约束。稳定跟踪期间
没有触发 detector request、丢失或重捕，因此本结果不证明 detector 队列、同类
干扰、2/3 重捕、遮挡恢复、长时稳定、内存平台化或真实吊舱验收。

协调对话需要审查并集成 `codex/debug-runtime-config` 的最新结果提交，并在公共契约
中决定是否把它锁定为下一轮交互 Debug source candidate；这不是发布板端 latest
handoff。`development_board_validation.latest_handoff.available` 必须继续为
`false`，总体裁决仍为 `functional_pass_promotion_held`。

## 已观察证据

- VisDrone9 detector-only 验收：工件身份、manifest、模型加载、真实 tensor 布局、20 图推理、类别过滤和单进程资源检查为 `PASS`。
- 集成运行保留了人员、车辆、detector-disabled 和 1001 tracked-frame stability 证据。
- 所有保留运行的 detector 请求最终完成，最大队列 gap 不超过 1；未观察到 detector/RKNN error。
- 所有 `LOST` 行均保持 `control_valid=false`；detector-disabled 路径计数为零。
- 主要证据：
  `rk3566/nanotrack_v3/docs/visdrone9-detector-board-verification.md`。

## 开发板交付门结果

- 1001-frame stability 的 warm RSS 约为 111 MiB 且线程数稳定，但 VmHWM 在每个 30 秒采样点持续增长到 `160556 kB`。
- 人员运行有 detector hypothesis，但 confirmed reacquisition 为 0。
- 车辆运行的 4 次 reacquisition 都来自 motion，而不是 detector。
- 固定阈值下，VisDrone9 人员 usable hypotheses 少于 COCO 基线。
- 因此当前结论是“功能和控制安全通过，正式 promotion hold”；不是长时重捕产品验收通过。

## Blockers

- 协调基线尚未集成交互式 Debug 分支，也未在公共契约中锁定下一轮板端验证要使用
  的精确 source commit、NanoTrack 模型身份和 detector 选择。
- 板端 RTC 在重启后仍回退到 2019；NTP 纠正前执行增量构建存在时钟偏移风险。
- 隔离或修复视频 writer/buffer/allocator 内存路径，并证明 RSS 与 VmHWM 平台化。
- 用相同输入、时长、采样协议完成 COCO/VisDrone9 资源对比。
- 使用标注的遮挡/可靠重现数据证明 detector-confirmed recovery，而不是只统计候选或 motion reacquisition。
- 完成可见光/红外阈值标定和同类目标身份风险评估。

## Next action

1. 请协调对话先锁定下一轮板端验证的 source commit、NanoTrack 模型和 detector
   组合；在此之前不开始新的 promotion run，也不发布 latest handoff。
2. 锁定后先确认板端系统时间已由 NTP 纠正，再在完整原生环境中 clean-build
   交互式 Debug 版本，依次复测模型加载、推理、
   集成视频、控制安全、运行/端到端时延、同协议资源平台化及标注遮挡重捕。
3. 下一轮至少保留源 commit、板端原生编译二进制、模型/manifest/labels/输入哈希、
   完整命令、资源采样、verifier 结果和分层裁决。

## Handoff

任何 `PASS` 都必须注明层级：`model_load`、`detector_function`、`integrated_function`、`stability`、`recovery_quality` 或 `development_board_acceptance`。低层级通过不得升级表述为高层级通过。

当某一组合达到预定开发板门槛后，本任务应向协调对话移交精确 source commit、模型/manifest/labels 哈希、配置、板端验证证据和裁决。协调对话将其标记为最新 handoff，供交叉编译任务获取。此 handoff 不代表真实吊舱已通过测试。

当前需要协调对话更新公共契约的内容是“下一轮候选锁定”，不是发布通过的 handoff：
请明确批准的交互式 Debug source commit、NanoTrack 模型身份和 COCO/VisDrone9 detector
选择。`latest_handoff.available` 在新的真实板端验收达到门槛前必须继续保持 `false`。

## 2026-10-26 Debug 窗口 720p 初始尺寸验证

- 时间：`2026-10-26T19:53:37+08:00`。共享工作树/分支为
  `/home/u20/single-object-tracking/.worktrees/debug-runtime-config` /
  `codex/debug-runtime-config`，任务基线为
  `2f2f3fb57022d42855f20eca940eaafb3103720f`；结果提交可用
  `git log -1 --format=%H -- rk3566/nanotrack_v3/src/main.cpp` 查询。
- 变更仅在交互式 Debug 分支的 `cv::WINDOW_NORMAL` 窗口创建后调用
  `cv::resizeWindow(..., 1280, 720)`。输入帧、NanoTrack/detector 输入、框坐标、
  保存输出和生产 `ai_alg` 均未缩放或修改；窗口启动后仍允许用户调整大小。
- 主机侧先增加尺寸断言并观察到因常量不存在而编译失败，随后实现转绿；
  `test_interactive_control` 通过，RKNN-independent CTest 为 `14/14` 通过，
  `git diff --check` 通过。该主机构建目录不含 RKNN 可执行目标，因此完整程序由板端
  原生环境编译验证。
- 板端隔离工作树为
  `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry`，分支
  `codex/rk3566-debug-telemetry`，基线
  `0f155ab7dcde33582534f1d87e61a3453167b825`，结果提交
  `8802eabc4a56d3311de72e450d73967f218ab827`。系统时间为
  `2026-10-26T19:51:53+08:00` 且 NTP 已同步；`cmake --build build-board -- -j2`
  原生编译成功。
- 板端新二进制
  `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/build-board/nanotrack_v3`
  SHA-256 为
  `dbf1804ff2dda7a452d87149980a73dbd967e76744e7314f457a8192dc50fb71`。
  使用 `DISPLAY=:0`、`girl_dance.mp4`、既有 NanoTrack 模型和 VisDrone9 detector
  做有界启动后，`xwininfo -name "NanoTrack V3 RK3566 Debug"` 实测窗口内容区为
  `Width: 1280`、`Height: 720`；程序日志仍报告源视频和输出尺寸为 `856x480`，
  证明本次只改变窗口初始显示尺寸。启动日志和窗口查询分别保留在
  `/tmp/nanotrack-window-720p.log`、`/tmp/nanotrack-window-720p.xwininfo`，SHA-256
  分别为
  `e84814050a5698d4e0a896987c83a0c82e343cdc9260abb53ca70d0c3439403a`、
  `3cf58780fd121382d9e20b38ad63a537d7f9a9ef2ef4cf40fedaf81021e54c20`。
- 本次有界启动绑定的输入、NanoTrack template/search/head/manifest 和 VisDrone9
  detector RKNN/manifest/labels SHA-256 依次为
  `8090fcd5090adb62f21e79e799f0b0871d9c610d8cf8087b9fcdd3c7e9c2e9b9`、
  `38b60334e8e0f0999bfb7ff9e943090546461f9307682653685237e24b2a7709`、
  `92125f1ef6254dade5786bb999faa5b9d9412ceb2b7987f6dbba82f959b7ef6a`、
  `2065bfca503cca9197e67de1a754efad930dfed4d2fc73881c77d9e850dcb600`、
  `441e93f2092afe94bac663f27adb1fe99848357356efd67c7473b0b506f854b2`、
  `3698bc9bea1f43a4bac2826a09e03be0439fddfbc5c5604d8ed0f92b36881df0`、
  `05e6d8c7c210b7f9d85b8fb712aa00b014f310e46e85f171ce1158afc90ba63e`、
  `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`。
  二进制绑定 OpenCV 4.10 `libopencv_world.so.410`；启动日志实测 RKNN API
  `2.3.2 (429f97ae6b@2025-04-09T09:09:27)`、driver `0.8.2`。
- 实际启动、窗口查询和停止命令如下。程序由 `timeout` 设置 25 秒硬边界；查询到
  窗口后向本轮启动的进程发送 `SIGINT`，没有让其继续执行长时跟踪：

```bash
cd /home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry
timeout --signal=INT 25s env DISPLAY=:0 stdbuf -oL -eL \
  ./build-board/nanotrack_v3 --config config/interactive-debug.conf \
  --video /home/cat/Project/SOT/nanotrack_v3_longterm/data/girl_dance.mp4 \
  --bbox 244,161,74,70 \
  --model-dir /home/cat/Project/SOT/nanotrack_v3_longterm/models \
  --controller-config /home/cat/Project/SOT/nanotrack_v3_longterm/config/visible-default.conf \
  --detector-model /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/yolov8n-visdrone9-rk3566-fp16.rknn \
  --detector-manifest /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/detector-manifest.json \
  --detector-labels /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/visdrone9.txt \
  --max-frames 1 >/tmp/nanotrack-window-720p.log 2>&1 &
app_pid=$!
found=0
attempt=0
while [ "$attempt" -lt 15 ]; do
  if xwininfo -display :0 -name "NanoTrack V3 RK3566 Debug" \
      >/tmp/nanotrack-window-720p.xwininfo 2>/dev/null; then
    found=1
    break
  fi
  attempt=$((attempt + 1))
  sleep 1
done
test "$found" -eq 1
kill -INT "$app_pid"
wait "$app_pid" || true
```

- 板端 `interactive_control` 及其他 12 项测试通过；全量 CTest 为 `13/14`，唯一
  失败是 `cli` 对发行配置视频路径的断言。根因已由重复运行和差异核对确认：本轮
  开始前用户已把 `config/interactive-debug.conf` 的 `video` 从
  `../data/girl_dance.mp4` 改为 `/home/cat/Project/SOT/data/1.mp4`。该用户修改未被
  覆盖、暂存或提交，因此不能把本轮板端结果表述为全量 CTest 通过；它与窗口尺寸
  代码无重叠，窗口尺寸聚焦测试和实测均通过。

### 裁决与 Handoff

本轮仅形成 `native_build + interactive_window_geometry_smoke` 功能通过，不增加跟踪、
重捕、资源、长稳或真实吊舱证据，不改变 detector 不能直接产生
`control_valid=true` 的安全约束。协调对话可集成本次 Debug 功能提交和板端证据，
但无需修改 `integration-contract.yaml`；总体状态仍为
`functional_pass_promotion_held`，
`development_board_validation.latest_handoff.available` 继续为 `false`。

## 后续更新模板

```text
时间：<ISO 8601 + timezone>
板卡/运行时：<board, OS, RKNN API, driver, OpenCV>
集成身份：<source commit, executable/model/manifest/labels/input SHA-256>
命令：<exact board command>
结果：<frames, request/complete/error, reacquisition source, safety>
性能边界：<runtime vs end-to-end>
资源：<RSS, VmHWM, threads, temperature, duration>
verifier：<command -> observed result>
裁决/阻塞：<layered pass/fail/risk>
Handoff：<requested contract change or none>
```

## 2026-10-23 Debug 日志字段参考文档

- 时间：`2026-10-23T23:33:06+08:00`。
- 分支/工作树：`codex/debug-runtime-config` /
  `/home/u20/single-object-tracking/.worktrees/debug-runtime-config`。
- 基线：`e91af4c7d4943cd3ed8e4aba7d9d644cf891b4d2`；结果提交可用
  `git log -1 --format=%H -- RK3566_NANOTRACK_LONGTERM_CODE_FLOW.zh-CN.md`
  查询，避免在提交前记录不存在的 commit ID。
- 变更：在 `RK3566_NANOTRACK_LONGTERM_CODE_FLOW.zh-CN.md` 新增交互式 Debug
  日志章节，覆盖 `main.cpp` 和 `debug_telemetry.cpp` 当前输出的全部 20 种
  `DEBUG_EVENT`，说明触发条件、字段含义、源结构、枚举、打印节拍和排障顺序。
- 关键边界：明确这些日志只属于独立交互式 Debug 程序，不属于 ViewLink
  `ai_alg` 生产 ABI；再次明确 detector batch、候选框、可靠质量或候选锁定均不能
  直接产生 `control_valid=true`，必须经过 NanoTrack 验证和多帧确认。
- 一致性检查：从 `src/main.cpp` 的 `DEBUG_EVENT event=` 和
  `src/debug_telemetry.cpp` 的 `event_stream()` 提取事件名，与文档反查，观察结果为
  `documented_events=20 required_fields=17`；`git diff --check` 通过；现有主机
  CTest 回归仍为 `14/14` 通过。独立审查后又按源码修正了 2/3 确认示例、候选 ID
  重置范围、`FULL_OBJECT` 探针数量和选项结构类型名；针对这四点的 6 项文档断言
  由失败转为通过。
- 证据边界：本轮只更新说明文档，没有修改或重新编译程序，没有执行新的开发板、
  重捕、资源、长稳或吊舱测试；此前板端二进制、模型身份和验收结论均不改变。

### Handoff

协调对话只需集成本次文档提交；不需要修改 `integration-contract.yaml`，不改变
`functional_pass_promotion_held` 和
`development_board_validation.latest_handoff.available=false`。

## 2026-10-26 detector 位置硬门与 Debug 分色框简单板端验证

- 时间：`2026-10-26T21:25:05+08:00`。共享工作树/分支为
  `/home/u20/single-object-tracking/.worktrees/debug-runtime-config` /
  `codex/debug-runtime-config`，实现基线为
  `7d2fad2bbe4ebe67efe2f67aee492d755dbb3bf6`；实现提交为
  `c07f85080e1af02b53e9fa77f2b8bb0a5503b60e`、
  `5055e75af57c2a99e1d92a4eff13440e04f90759`、`1451537`、`02aef3c`。
  板端独立工作树/分支为
  `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry` /
  `codex/rk3566-debug-telemetry`，结果提交为
  `d845f056d41d5b1ab108b72375dbde59aab61f62`。
- 同步前板端已有用户修改 `config/interactive-debug.conf`（视频路径为
  `/home/cat/Project/SOT/data/1.mp4`）和未跟踪 `log.txt`；两者均未覆盖、未暂存、
  未提交。板端系统时间与 RTC 已正确到 2026，系统报告时钟已同步。
- 新增严格位置门：候选中心到本帧运动预测中心的距离除以画面对角线不得超过
  `detector_candidate_max_center_ratio=0.15`；`ASSISTED_RECOVERY` 与 `LOST`
  不放宽，通过者按距离升序、置信度降序排序。新增 `POSITION_REJECTED`、位置通过/
  拒绝计数和 schema 6 字段，且 detector 仍不能直接产生 `control_valid=true`。
- 交互窗口区分青色 motion、紫色 detector candidate、橙色 detector probe、蓝色
  anchor、黄色 recent、绿色 `ACCEPTED CTRL` 与红色 `REJECTED NO_CTRL`。第一次
  板端截图发现动态标签遮挡，随后通过共享提交 `02aef3c` 改为右侧固定纵向标签并
  重新原生链接。非交互输出视频的原单框渲染未改变。

主机 clean build 命令：

```bash
cmake -S rk3566/nanotrack_v3 -B /tmp/nanotrack-position-gate-clean-build \
  -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF
cmake --build /tmp/nanotrack-position-gate-clean-build -- -j2
ctest --test-dir /tmp/nanotrack-position-gate-clean-build --output-on-failure
```

结果 `15/15` 通过。新增 `interactive_debug_overlay` 测试覆盖六类框的绘制顺序、
`FULL_OBJECT` 候选/probe 同坐标仍保留不同标签，以及未确认假设不得生成
`ACCEPTED CTRL`。

板端为 LubanCat RK3566/AArch64、Linux `4.19.232`、GCC `8.3.0`、CMake
`3.13.4`、OpenCV 4.10、RKNN API `2.3.2` / driver `0.8.2`。原生命令：

```bash
cd /home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry
cmake -S . -B build-board -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=ON
cmake --build build-board -- -j2
cd build-board
ctest --output-on-failure
ctest --output-on-failure -E '^cli$'
```

完整 RKNN/OpenCV 可执行程序构建成功。全量 CTest 为 `14/15`；唯一失败的 `cli`
读取了必须保留的用户绝对视频路径覆盖，导致 shipped-profile 相对路径断言失败。
没有为凑通过而覆盖用户文件。排除这项已解释的本地配置差异，其余板端测试
`14/14` 通过；干净共享工作树的同一 `cli` 已包含在主机 `15/15` 中。

板端有界交互命令：

```bash
DISPLAY=:0 ./build-board/nanotrack_v3 \
  --config config/interactive-debug.conf \
  --video /home/cat/Project/SOT/nanotrack_v3_longterm/data/girl_dance.mp4 \
  --bbox 244,161,74,70 --max-frames 200
```

通过 X11 发送一次空格开始播放，帧 200 自动暂停后截图，再发送 `Q` 正常退出。日志
在帧 129 进入 `ASSISTED_RECOVERY`，帧 189 进入 `LOST`；帧 176 实际记录：

```text
DEBUG_EVENT event=detector_batch frame=176 source_frame=174 age_ms=67 raw=1 filtered=1 position_valid=0 position_rejected=1 status=POSITION_REJECTED total_ms=196.923
```

该候选没有后续 `detector_verify` 或 `reacquired`，不可靠状态始终
`control_valid=0`。帧 200 截图显示青色 `MOTION PRED`、蓝色 `NT ANCHOR`、黄色
`NT RECENT` 和红色 `REJECTED NO_CTRL`，标签不再互相遮挡；被位置门拒绝的候选
没有伪装成活动假设或绿色控制框。由于本次真实候选被硬门拒绝，截图不包含紫色/
橙色实例；它们及 `FULL_OBJECT` 重合情况由板端通过的纯数据 overlay 测试覆盖，
不能表述为七种颜色均完成了人工视觉验收。

| 工件 | 路径 | SHA-256 |
|---|---|---|
| Debug 可执行程序 | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/build-board/nanotrack_v3` | `30855451e2e7050ad208e70fb459e43cf61e459b962cfba8ac680111514c13ac` |
| template RKNN | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/models/template_backbone.rknn` | `38b60334e8e0f0999bfb7ff9e943090546461f9307682653685237e24b2a7709` |
| search RKNN | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/models/search_backbone.rknn` | `92125f1ef6254dade5786bb999faa5b9d9412ceb2b7987f6dbba82f959b7ef6a` |
| head RKNN | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/models/head.rknn` | `2065bfca503cca9197e67de1a754efad930dfed4d2fc73881c77d9e850dcb600` |
| tracker manifest | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/models/manifest.json` | `441e93f2092afe94bac663f27adb1fe99848357356efd67c7473b0b506f854b2` |
| VisDrone9 RKNN | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/yolov8n-visdrone9-rk3566-fp16.rknn` | `3698bc9bea1f43a4bac2826a09e03be0439fddfbc5c5604d8ed0f92b36881df0` |
| detector manifest | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/detector-manifest.json` | `05e6d8c7c210b7f9d85b8fb712aa00b014f310e46e85f171ce1158afc90ba63e` |
| VisDrone9 labels | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/visdrone9.txt` | `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992` |
| 输入视频 | `/home/cat/Project/SOT/nanotrack_v3_longterm/data/girl_dance.mp4` | `8090fcd5090adb62f21e79e799f0b0871d9c610d8cf8087b9fcdd3c7e9c2e9b9` |
| 帧 200 截图 | `/tmp/nanotrack-position-gate-debug-v2.png` | `53c8ace17b620759536e2f42a11b2b76b488073aca04684c36d60ebaad709efe` |
| 200 帧 Debug 日志 | `/tmp/nanotrack-position-gate-debug-v2.log` | `f70331b9f028b2ad361cc0dcd9debac6c3d32fe527f270f8fa466d9329e9805d` |

本轮只达到 `integrated_function_short_smoke` 与 Debug 可观测性验证，不是长时重捕
质量、阈值标定、资源平台化、完整七色人工视觉验收、真实吊舱或开发板发布门通过。
任务状态继续为 `functional_pass_promotion_held`，`latest_handoff.available=false`。

### Handoff

请协调对话在集成本任务提交后，把下一轮共享源码候选更新到该集成结果，并继续保持
detector 不能直接授权控制、VisDrone9 promotion hold 和 latest handoff 不可用；
是否把严格 `0.15` Debug 策略锁定为下一轮板端候选仍由协调者决定。

## 2026-10-27 独立审查修正：取消位置门前截断

- 时间：`2026-10-27T00:51:25+08:00`；分支/工作树仍为
  `codex/debug-runtime-config` /
  `/home/u20/single-object-tracking/.worktrees/debug-runtime-config`，审查起点为
  `59187142454acf9773423e42c8d829e3ba24a348`。
- 独立审查发现实际 RKNN detector 链路此前会在 YOLO 后处理中先按置信度截断为
  `detector_max_candidates=3`，三个远处高置信框可能在控制器位置门之前挤掉近处
  候选。这不满足“位置过滤、距离排序之后再截断”的完整顺序。
- 修正后 detector 请求允许后处理保留全部经过置信度过滤和 NMS 的候选，最终数量
  限制只在控制器完成 `0.15` 位置硬门和距离优先排序后执行。新增控制器请求与真实
  YOLO 后处理回归，覆盖第四个较低置信候选不会被上游提前丢弃。
- 同一审查指出 `FULL_OBJECT` 下候选框与 probe 框重合时，等线宽会使后画的橙色
  probe 覆盖紫色 candidate。现改为紫色 3 像素粗框、橙色 1 像素细框，并由纯数据
  overlay 测试锁定线宽关系。
- TDD 证据：新增断言先观察到控制器请求仍为 `3` 而失败、overlay 缺少
  `line_thickness` 而编译失败；实现后
  `long_term_controller`、`yolov8_postprocess`、`interactive_debug_overlay` 三项
  聚焦测试 `3/3` 通过。修正共享提交为
  `a6c23be`；主机重新构建后完整 CTest `15/15` 通过。
- 板端在保留用户修改 `config/interactive-debug.conf` 和未跟踪 `log.txt` 的前提下，
  应用同一源代码修正并原生重编译完整 RKNN/OpenCV 程序；板端独立提交为
  `5e620b1`。命令
  `cmake --build build-board -- -j2` 成功，随后
  `ctest --output-on-failure -E '^cli$'` 为 `14/14` 通过。这里排除的 `cli` 仍仅因
  已记录的用户视频路径覆盖，不把结果表述为板端全量 `15/15`。
- 新 Debug 可执行程序路径为
  `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/build-board/nanotrack_v3`，
  SHA-256 为
  `2b6a3836fb2ff46b38fa21a71a4c1610cab4914bef3f2ae101568a28531c229c`。
  本次复测是原生编译和测试级验证，没有再次运行视频，也没有新增重捕性能或人工
  七色视觉验收证据。
- 安全边界不变：detector 候选仍只是假设，不能直接产生 `control_valid=true`；
  NanoTrack 双模板、几何门和 2/3 确认规则均未绕过。任务状态仍为
  `functional_pass_promotion_held`，`latest_handoff.available=false`。

## 2026-10-28 遮挡等待状态与 Detector 全候选显示设计

- 时间：`2026-10-28T18:51:42+08:00`。
- 分支/工作树：`codex/debug-runtime-config` /
  `/home/u20/single-object-tracking/.worktrees/debug-runtime-config`；设计基线为
  `afc5f4942a6020b50058ecccd349cf1dc87288c2`。
- 与用户确认采用独立 `OCCLUSION_HOLD` 状态。默认触发条件为连续 2 帧非可靠且
  宽或高相对最近可靠尺度中位数缩小到 `0.70` 以下；HOLD 帧数通过
  `occlusion_hold_frames` 配置，默认 12。
- 触发帧仍执行 NanoTrack；随后完整 N 帧不运行 tracker、不发起 detector，只以
  冻结宽高的 motion 框预测中心且保持 `control_valid=false`。同一次 loss episode
  只允许触发一次，仅在 `TRACKING/UNCERTAIN/LOCAL_RECOVERY` 早期阶段允许触发。
- Detector batch 到达时计划显示全部经过类别阈值和 NMS 的候选，并区分位置门通过、
  位置拒绝、非法、最终入队和活动候选；所有原始 detector 框仍不能直接授权控制。
- 设计文档：
  `docs/superpowers/specs/2026-10-28-occlusion-hold-detector-debug-design.md`。
  独立实现者审查已用于消除 HOLD 首尾帧、重复触发、晚到 detector batch 和现有
  motion/detector 重捕规则边界的歧义。
- 用户批准设计后形成实施计划：
  `docs/superpowers/plans/2026-10-28-occlusion-hold-detector-debug.md`。计划按配置/类型、
  motion 尺度冻结、控制器状态、runtime 与 detector generation、全候选可视化/日志、
  主机与开发板验证六个可独立审查的任务执行，全部要求先写失败测试。
- 本次只形成设计，没有修改程序、编译开发板、运行视频或产生新的模型/二进制哈希。
  当前状态继续为 `functional_pass_promotion_held`，
  `development_board_validation.latest_handoff.available=false`。

### Handoff

协调对话暂不需要修改公共契约；待实现、主机测试和真实 RK3566 简单验证形成提交后，
再请求集成新的 Debug 源码候选。Detector 直接控制禁令和人工吊舱边界保持不变。

## 2026-10-28 OCCLUSION_HOLD 整体审查与板前主机门

- 时间：`2026-10-28T23:27:15+08:00`；工作树/分支为
  `/home/u20/single-object-tracking/.worktrees/debug-runtime-config` /
  `codex/debug-runtime-config`，审查和修正基线为
  `6e533c2785b1389c3613be38788468554f4133e4`。本节与相关测试、README
  修正同一提交，结果提交可用
  `git log -1 --format=%H -- docs/progress/board-validation.md` 查询。
- 整体功能审查范围为 `4f1a046..6e533c2`；审查门结论为没有未解决的
  Critical/Important 项。三个测试覆盖类 Minor 与三个 README 文案类 Minor 已在
  板端同步前处理，未因审查修改生产控制逻辑。
- 新增直接回归覆盖：HOLD 退出而尚未重捕时运动预测宽高仍保持冻结；活动 detector
  候选正在 NanoTrack 验证时不能进入 HOLD；HOLD 触发会清除在触发帧到达、通过位置门
  的晚到 detector 候选。此提交当时关于“清除旧 motion 确认”的公开行为断言并不能
  单独证明清理发生，因为固定 2/3 确认窗口会在 HOLD 结束前自然淘汰旧确认；该证据缺口
  由下方 `2026-10-28 HOLD 确认清理测试证据修正` 补齐。
- README 同步把控制无效状态列表补入 `OCCLUSION_HOLD`，把遗留
  `schema-v5` 改为当前 `schema-v7`，并收窄颜色说明：只有无拒绝原因的合法原始假设为
  紫色，位置/容量/stale/frame/HOLD 拒绝均为灰色，非法几何只显示深红文字。
- TDD/聚焦证据：新增候选清理断言首次运行时发现测试错误地读取触发帧末尾队列为
  `1`，实际设计语义是候选先经批次接收和位置门、随后由 HOLD 立即作废并把最终队列
  清为 `0`；断言改为同时验证 `ACCEPTED` 批次、位置通过计数、
  `HOLD_INVALIDATED` 诊断和最终零队列。随后
  `ctest -R '^(long_term_controller|long_term_replay)$'` 为 `2/2` 通过。
- 主机命令：

  ```bash
  cmake -S rk3566/nanotrack_v3 \
    -B /tmp/nanotrack-occlusion-hold-final-build \
    -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF
  cmake --build /tmp/nanotrack-occlusion-hold-final-build -- -j2
  ctest --test-dir /tmp/nanotrack-occlusion-hold-final-build --output-on-failure
  git diff --check 4f1a046..HEAD
  git diff --check
  ```

  观察结果：配置和完整构建成功，完整主机 CTest 为 `15/15` 通过、`0` 失败，
  总测试时间 `1.13 sec`；两项 whitespace 检查均无输出。
- 证据边界：本节仅证明 x86_64 主机、`ENABLE_RKNN_RUNTIME=OFF` 的构建/测试及
  源码审查状态；尚未连接 RK3566、未进行原生 RKNN 构建、模型加载、
  `girl_dance` 流程观察、二进制/模型/视频哈希或资源测量。不得把本节表述为板端验证、
  重捕性能、真实吊舱或发布门通过。
- 安全边界保持不变：detector 原始候选、HOLD motion 框和 WEAK 观测都不能直接产生
  `control_valid=true`；`latest_handoff.available=false`。板端仅允许同步本节对应的精确
  已提交源码，必须保留板端用户配置、日志、模型、视频和构建目录。

### Handoff

当前不请求协调对话修改 `STATUS.md` 或 `integration-contract.yaml`。完成真实 RK3566
原生构建、CTest 和有界 `girl_dance` 观察后，再在本文件追加板端工件身份、观察项、
未观察项和最终 Debug 分支提交。

## 2026-10-28 OCCLUSION_HOLD RK3566 原生构建与有界视频观察

- 时间：`2026-10-28T23:35:55+08:00`。共享源码提交为
  `578613dbe11a67c4241363e02e6554755258c178`，工作树/分支为
  `/home/u20/single-object-tracking/.worktrees/debug-runtime-config` /
  `codex/debug-runtime-config`。板端为 `lubancat`、Linux `4.19.232`，源码工作树/分支为
  `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry` /
  `codex/rk3566-debug-telemetry`，同步前提交 `5e620b1`，同步后的审计提交为
  `7ba0da0e0359c1ead4bd00656366a4cfbd463d69`。
- 同步前板端仍只有用户修改的 `config/interactive-debug.conf` 和未跟踪 `log.txt`；前者
  把视频改为 `/home/cat/Project/SOT/data/1.mp4`。二者均未覆盖、暂存或提交。系统时间为
  `2026-10-28T23:28:19+08:00`，时区 `Asia/Shanghai`，时钟已同步；根分区可用约
  `15G`。
- 板端源码仓库没有配置远程，且其仓库根对应共享仓库的 `rk3566/nanotrack_v3` 子目录。
  因此使用以下可审计同步方式：从两个精确共享提交生成相对路径补丁，传到板端 `/tmp`，
  校验 SHA-256，先执行 `git apply --check`，再应用并只暂存补丁列出的 31 个文件：

  ```bash
  git diff --relative=rk3566/nanotrack_v3 --binary \
    a6c23be..578613d -- rk3566/nanotrack_v3 \
    > /tmp/nanotrack-occlusion-hold-578613d.patch
  sha256sum /tmp/nanotrack-occlusion-hold-578613d.patch
  git apply --check /tmp/nanotrack-occlusion-hold-578613d.patch
  git apply /tmp/nanotrack-occlusion-hold-578613d.patch
  ```

  主机和板端补丁 SHA-256 均为
  `a481f5a69700bcb15a0935f626d009734ce27f59ce4627f6a6158e1cdb79b75d`；
  板端应用检查成功。提交后 `git status --short` 仍只显示上述用户配置和 `log.txt`。
- 板端工具链为 GCC `8.3.0`、CMake `3.13.4`、OpenCV `4.10.0`。首次按最简命令配置
  失败，明确提示必须设置 `RKNN_INCLUDE_DIR` 和 `RKNN_LIBRARY`；未改源码。随后复用
  板端既有验证路径，在新的独立构建目录完成原生配置和全量链接：

  ```bash
  cmake -S . -B build-board-occlusion-hold-578613d \
    -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=ON \
    -DOpenCV_DIR=/home/cat/ThirdParty/OpenCV4100/lib/cmake/opencv4 \
    -DRKNN_INCLUDE_DIR=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/include \
    -DRKNN_LIBRARY=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/aarch64/librknnrt.so
  cmake --build build-board-occlusion-hold-578613d -- -j2
  cd build-board-occlusion-hold-578613d
  ctest --output-on-failure
  ctest --output-on-failure -E '^cli$'
  ```

  构建成功。完整 CTest 为 `14/15`，唯一失败仍是用户绝对视频路径导致的既有 `cli`
  shipped-profile 断言；没有覆盖用户配置。排除该已解释差异后其余 `14/14` 全部通过，
  包括新增 HOLD/controller/runtime/overlay/telemetry/schema-7 回归。
- 有界板端命令：

  ```bash
  DISPLAY=:0 ./build-board-occlusion-hold-578613d/nanotrack_v3 \
    --config config/interactive-debug.conf \
    --video /home/cat/Project/SOT/nanotrack_v3_longterm/data/girl_dance.mp4 \
    --bbox 244,161,74,70 --max-frames 400
  ```

  通过本机 X11 向精确窗口发送 Space 开始连续播放，处理 307 帧后保留窗口 XWD 并发送
  Q 正常退出；因此这是 307 帧有界观察，不宣称达到配置上限 400。日志记录 RKNN API
  `2.3.2 (429f97ae6b@2025-04-09T09:09:27)`、driver `0.8.2`，最终摘要为
  `tracked_frames=307`、`mean_tracker_ms=45.596`、`mean_runtime_ms=45.846`、
  `runtime_fps=21.812`、`mean_pipeline_ms=50.029`、`end_to_end_fps=19.989`；这是单次
  Debug 观察值，不是性能门通过。
- 本次 `girl_dance` 自然流程没有触发 `occlusion_hold_enter/tick/exit`，三类事件计数均
  为 `0`。因此“触发帧仍有 anchor/recent、随后恰好 12 帧跳过推理、冻结尺度、
  `control_valid=0`、之后恢复搜索”在真实视频中均标记为**未观察到**，不使用人工伪造
  或单元测试代替板端视频证据。对应状态机边界仅有板端通过的自动化测试证据。
- Detector 在该 307 帧运行中完成 `12` 个 batch，记录 `7` 条逐候选事件。例如帧 237
  的 batch 对 `source_frame=234`、`age_ms=100` 的两个 filtered 候选逐一记录 bbox、
  class/confidence、center ratio、`eligible=1`、`position_pass=0`、`queued=0` 和
  `POSITION_REJECTED`；随后 batch 汇总为 `raw=10 filtered=2 position_rejected=2`。
  运行中没有 `reacquired` 事件；候选出现后的周期状态均保持
  `ASSISTED_RECOVERY/LOST`、`control_valid=0`。这证明本次返回的候选未直接授权控制，
  但由于所有候选均被位置门拒绝，未观察到 PASS/queued/active-probe 实例，也未在候选
  完成帧人工截图验证全部颜色；这些显示分支只有板端通过的纯数据 overlay 测试证据。

| 工件 | 板端绝对路径 | SHA-256 |
|---|---|---|
| Debug 可执行程序 | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/build-board-occlusion-hold-578613d/nanotrack_v3` | `62fe954c00fd4cce96e55db62a1f1687e037b53c5045070eef333dc02cf2e3f2` |
| template RKNN | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/models/template_backbone.rknn` | `38b60334e8e0f0999bfb7ff9e943090546461f9307682653685237e24b2a7709` |
| search RKNN | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/models/search_backbone.rknn` | `92125f1ef6254dade5786bb999faa5b9d9412ceb2b7987f6dbba82f959b7ef6a` |
| head RKNN | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/models/head.rknn` | `2065bfca503cca9197e67de1a754efad930dfed4d2fc73881c77d9e850dcb600` |
| tracker manifest | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/models/manifest.json` | `441e93f2092afe94bac663f27adb1fe99848357356efd67c7473b0b506f854b2` |
| VisDrone9 RKNN | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/yolov8n-visdrone9-rk3566-fp16.rknn` | `3698bc9bea1f43a4bac2826a09e03be0439fddfbc5c5604d8ed0f92b36881df0` |
| detector manifest | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/detector-manifest.json` | `05e6d8c7c210b7f9d85b8fb712aa00b014f310e46e85f171ce1158afc90ba63e` |
| VisDrone9 labels | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/visdrone9.txt` | `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992` |
| visible controller config | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/config/visible-default.conf` | `b9a128b78063056173626af1c7c7d2c580a59b295c45a4afc0ef8e2e04bff95b` |
| preserved user Debug config | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/config/interactive-debug.conf` | `c07da92f912f91d5c57d42f005017a309b874445e728079d3740148b9584ab6e` |
| 输入视频 | `/home/cat/Project/SOT/nanotrack_v3_longterm/data/girl_dance.mp4` | `8090fcd5090adb62f21e79e799f0b0871d9c610d8cf8087b9fcdd3c7e9c2e9b9` |
| 307 帧 Debug 日志 | `/tmp/nanotrack-occlusion-hold-578613d-girl-dance.log` | `9118fab42e1d512617689afb88efcaeae621994e49f61a415482158d88dd4bc0` |
| 退出前窗口 XWD | `/tmp/nanotrack-occlusion-hold-578613d-frame-limit.xwd` | `dff2ed7e978ee665e2151ae2bbc7519a56956e45fb3347e1f96d1d6b710519a5` |
| 同步补丁 | `/tmp/nanotrack-occlusion-hold-578613d.patch` | `a481f5a69700bcb15a0935f626d009734ce27f59ce4627f6a6158e1cdb79b75d` |

- 裁决：本轮达到“精确提交同步 + RK3566 原生完整链接 + 除已解释 CLI 本地配置差异外
  其余测试通过 + 无标注有界 detector 诊断观察”。不达到真实视频 HOLD 时序观察、
  detector PASS/入队/活动探针人工视觉验收、标注重捕率、资源长稳、发布门或真实吊舱
  验收。任务状态继续为 `functional_pass_promotion_held`，
  `development_board_validation.latest_handoff.available=false`。

### Handoff

协调对话可采纳本节锁定的共享源码提交和板端工件身份，但不应把它发布为公共
`latest_handoff`。公共契约需要协调者评估是否仅更新 Debug 源码候选；VisDrone9
promotion hold、detector 直接控制禁令、交叉编译前板端门和人工吊舱边界均保持不变。

## 2026-10-28 HOLD 确认清理测试证据修正

- 时间：`2026-10-28T23:41:18+08:00`；修正基线为
  `bc6103ddf1c327cc6c88b3121784de6a71fa31cc`。独立复核指出，上述旧测试在 HOLD 前
  已通过重捕路径清空确认，不能证明 HOLD 入口自身执行了 `confirmations_.clear()`。
- 新测试把 `occlusion_trigger_consecutive_unreliable` 设为 `3`：先以模板冲突留下恰好
  一个 motion 确认，再输入两个 WEAK 缩小框，第二个 WEAK 触发 HOLD。测试在触发前验证
  待确认数为 `1`，在 HOLD 入口后验证为 `0`；HOLD 结束后的第一个一致可靠观测只能建立
  新的第一次确认，保持 `accepted=false`、`control_valid=false`。
- 由于当前确认策略固定为 2/3，旧确认按帧号在 HOLD 结束前也会自然过期；仅验证退出后
  未接受不足以证明入口清理。为验证新断言的敏感性，临时且仅在主机工作树中禁用 HOLD
  入口的 `confirmations_.clear()`，重新构建运行 `test_long_term_controller`，观察到：

  ```text
  FAIL: HOLD entry clears the pending motion confirmation immediately
  1 test(s) failed
  ```

  随后立即恢复原生产代码；`git diff -- src/long_term_controller.cpp` 无输出。恢复后聚焦
  `long_term_controller|long_term_replay` 为 `2/2` 通过，完整主机 CTest 为 `15/15`
  通过、`0` 失败、总时间 `0.55 sec`，`git diff --check` 无输出。
- 本次提交只增加/修正测试与证据文档，不改变生产决策或板端二进制对应的生产源码。
  真实 RK3566 构建和 307 帧视频观察发生在本测试补强之前，本轮没有重新同步、编译或
  运行开发板；板端提交仍为 `7ba0da0e0359c1ead4bd00656366a4cfbd463d69`，此前二进制
  SHA-256 `62fe954c00fd4cce96e55db62a1f1687e037b53c5045070eef333dc02cf2e3f2`
  和全部板端证据边界不变，不能把新增主机测试表述为板端复测。
- 安全与发布裁决不变：detector/HOLD/WEAK 不能直接授权控制，
  `functional_pass_promotion_held` 与 `latest_handoff.available=false` 保持不变。

### Handoff

协调对话应采用本修正后的测试覆盖和证据措辞；公共契约无需因测试补强改变工件身份或
发布裁决。若未来要求在板端执行这个新增测试，必须另行形成真实板端复测证据。

## 2026-10-30T01:34:14+08:00 目标类别初始化 RK3566 板端验证

- 共享分支/工作树：`codex/debug-target-class-init` /
  `/home/u20/single-object-tracking/.worktrees/debug-target-class-init`；输入源码
  `85c7ba592577915576be018093524a8711915ce2`。本节结果提交可用
  `git log -1 --format=%H -- docs/progress/board-validation.md` 查询，不在提交前虚构自身哈希。
- 独立板端工作树 `/home/cat/Project/SOT/nanotrack_v3_longterm-target-class-85c7ba5`
  位于 `75d44431887f29f6813329a8acb102c9d9bed594`，复核时 tracked/untracked
  状态均干净。共享源码补丁 `/tmp/nanotrack-target-class-85c7ba5.patch` 的 SHA-256
  为 `93467e875b3def3ba4aedfc0062f6c482681892083e14101bea6956881acd64d`；
  前序部署核对的构建相关对象指纹匹配，本轮又复核了补丁哈希与板端 HEAD。
- 板卡为 LubanCat RK3566/AArch64、Debian 10、Linux `4.19.232`、GCC `8.3.0`、
  CMake `3.13.4`，OpenCV `4.10.0`；实际运行日志报告 RKNN API
  `2.3.2 (429f97ae6b@2025-04-09T09:09:27)`、driver `0.8.2`。
  `ldd` 指向 `/home/cat/ThirdParty/OpenCV4100/lib/libopencv_world.so.410` 和
  `/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/aarch64/librknnrt.so`，没有缺失依赖。

### 输入与输出身份

路径均在板端。`A=/home/cat/Project/SOT/nanotrack_v3_longterm`，
`B=/home/cat/Project/SOT/nanotrack_v3_longterm-target-class-85c7ba5`，
`R=$A/releases/yolov8n-visdrone9-rk3566-fp16-v1/models`。

| 工件 | 路径 | SHA-256 |
| --- | --- | --- |
| 原生可执行文件 | `$B/build-board-target-class-85c7ba5/nanotrack_v3` | `28ffadf45d023ea66bb99ec30e7a7b7efb755d6d0ad1edc4336940b19368d243` |
| NanoTrack template | `$A/models/template_backbone.rknn` | `38b60334e8e0f0999bfb7ff9e943090546461f9307682653685237e24b2a7709` |
| NanoTrack search | `$A/models/search_backbone.rknn` | `92125f1ef6254dade5786bb999faa5b9d9412ceb2b7987f6dbba82f959b7ef6a` |
| NanoTrack head | `$A/models/head.rknn` | `2065bfca503cca9197e67de1a754efad930dfed4d2fc73881c77d9e850dcb600` |
| NanoTrack manifest | `$A/models/manifest.json` | `441e93f2092afe94bac663f27adb1fe99848357356efd67c7473b0b506f854b2` |
| VisDrone9 detector RKNN | `$R/yolov8n-visdrone9-rk3566-fp16.rknn` | `3698bc9bea1f43a4bac2826a09e03be0439fddfbc5c5604d8ed0f92b36881df0` |
| Detector manifest | `$R/detector-manifest.json` | `05e6d8c7c210b7f9d85b8fb712aa00b014f310e46e85f171ce1158afc90ba63e` |
| Detector labels | `$R/visdrone9.txt` | `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992` |
| 输入视频 | `$A/data/girl_dance.mp4` | `8090fcd5090adb62f21e79e799f0b0871d9c610d8cf8087b9fcdd3c7e9c2e9b9` |
| Debug 配置 | `$B/config/interactive-debug.conf` | `e3fe5f927ba3a47d2829dce600c01634f7f2f405576865c2edea22e0256493f8` |
| 可见光控制配置 | `$B/config/visible-default.conf` | `b9a128b78063056173626af1c7c7d2c580a59b295c45a4afc0ef8e2e04bff95b` |

### 原生构建、CTest 与场景命令

前序代理在新的板端构建目录执行以下原生配置/构建；保留原始日志为
`/tmp/nanotrack-target-class-configure.log`、`/tmp/nanotrack-target-class-build.log`、
`/tmp/nanotrack-target-class-ctest.log`，其 SHA-256 分别为
`68557c3c81af073eac3527862eeb766f15c4d3b8a8f0d948ed2a426219296984`、
`6cb11a61d3fa9938bc957421b752fc65379022ef44622c9e5c49b842b563a947`、
`34604e9e286bbf61537d176b305b5dfe9db1f4755294382d766ce403b60d06fc`。

```bash
cmake -S "$B" -B "$B/build-board-target-class-85c7ba5" \
  -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=ON \
  -DOpenCV_DIR=/home/cat/ThirdParty/OpenCV4100/lib/cmake/opencv4 \
  -DRKNN_INCLUDE_DIR=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/include \
  -DRKNN_LIBRARY=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/aarch64/librknnrt.so
cmake --build "$B/build-board-target-class-85c7ba5" -- -j2
(cd "$B/build-board-target-class-85c7ba5" && ctest --output-on-failure)
```

配置/构建完成，原始 CTest `16/16` 通过、`5.18 s`；本轮从构建目录重跑
`ctest --output-on-failure`，`16/16` 通过、`5.10 s`。板端 CTest 3.13 不支持
`--test-dir`；一次从 `/home/cat` 使用该参数得到 `No tests were found`，未计入测试结果。

场景脚本 `/tmp/target_class_board_scenarios.py` SHA-256
`9153936f9c06fd14bebb9c210ba1681623119558c0fc0eb8dc0e1a53c2ff3dcb`，
GUI 辅助脚本 `/tmp/target_class_board_gui.py` SHA-256
`3558a257d7863e0a03b0acc38f0301274393d67c1b657504925b37955c2b47ac`。
实际命令为 `python3 /tmp/target_class_board_scenarios.py {full,local,background,reselect}`；
脚本启动上述可执行文件，传入 `--config $B/config/interactive-debug.conf`、
`--video $A/data/girl_dance.mp4`、`--model-dir $A/models`、
`--controller-config $B/config/visible-default.conf` 和三个 `$R` detector 工件；
各场景完整命令保留在
`/home/cat/Project/SOT/validation-target-class-85c7ba5/{full,local,background,reselect}.command`。
前三个命令使用 `--max-frames 3` 并通过 GUI 单帧步进，重选命令使用
`--max-frames 210`；脚本对等待和进程退出设有上限。所有场景 exit 0。

| 场景 | ROI（原视频坐标） | 板端观察 |
| --- | --- | --- |
| 人物全身大框 | `196,160,184,310` | `UNKNOWN/NO_MATCH`，仅一个 car 候选，非 person；恢复关闭 |
| 人物局部框 | `244,161,74,70` | `UNKNOWN/NO_MATCH`，仍仅 car 候选；恢复关闭 |
| 背景框 | `30,70,100,100` | `UNKNOWN/NO_MATCH`；恢复关闭 |
| 同进程重选 | 全身→局部→背景→全身 | 初始化与 request ID 均为 1–4，四次完成、零错误；四次均 UNKNOWN，随后跟踪 210 帧 |

在默认 detector confidence `0.25` 下，初始化 batch 原始数 `6`，过滤后仅一框：
`car(2)`、confidence `0.363`、bbox `484.21,163.14,98.73,175.73`，
与左侧人物全身 ROI 的覆盖率 `0`。用于诊断的有界命令额外覆盖
`--detector-conf 0.05`，日志 `$E/full-conf005.log`（`E` 为下述证据目录）记录左侧
人物对应的 `truck(4)` 候选 confidence `0.162` 被选中、右侧有两个低置信度 person
候选；`timeout --signal=INT 12s` 退出码 `124`，没有 metadata 文件。
这既不是默认配置结果，也不是正确的 person 确立或阈值优化通过。

诊断命令（`E=/home/cat/Project/SOT/validation-target-class-85c7ba5`）：

```bash
timeout --signal=INT 12s env DISPLAY=:0 stdbuf -oL -eL \
  "$B/build-board-target-class-85c7ba5/nanotrack_v3" \
  --config "$B/config/interactive-debug.conf" --video "$A/data/girl_dance.mp4" \
  --bbox 196,160,184,310 --model-dir "$A/models" \
  --controller-config "$B/config/visible-default.conf" \
  --detector-model "$R/yolov8n-visdrone9-rk3566-fp16.rknn" \
  --detector-manifest "$R/detector-manifest.json" --detector-labels "$R/visdrone9.txt" \
  --detector-conf 0.05 --max-frames 1 --session-metadata "$E/full-conf005.json" \
  > "$E/full-conf005.log" 2>&1
```

证据目录为 `/home/cat/Project/SOT/validation-target-class-85c7ba5`。
`full/local/background/reselect.log` SHA-256 依次为
`db5581161e5f7dde8736b7f732b7af96dbeca725237717cd8900fbbdef7fc9c`、
`3a23962e92eb971a26c6ec7c1a0a686550473069279f131ebab2643339e160bc`、
`5e12cea76d00490fecd7cb327241a454b27d91ca49e5d5c0357be8d77edf339f`、
`1a79b7b7e209f908f14c037f465b5440d8ba3e9a785930380821cdfb720b3e97`；
对应 `.json/.csv/.xwd` 也保留。`reselect.csv` 为 212 行（表头、唯一 frame 0、
210 跟踪帧），未重复写入初始化帧；`reselect.json` 的初始化 detector
`request_count=4, complete_count=4, error_count=0`，恢复 detector
`request_count=0, complete_count=0`。跟踪期出现 `control_valid=1` 的记录来自
NanoTrack `MOTION` 路径，未观察到 detector-only 控制授权；没有实际触发 detector
辅助恢复，也没有可观察的旧 batch 污染或新 eligibility generation 遥测。

另一次无预置 bbox 的 GUI 运行使用 `timeout --signal=INT 45s`，在
`interactive_ready initialized=0` 时截取 `prepaint-before-roi.xwd`，随后选取
`196,160,184,310` 并截取 `prepaint-after-roi.xwd`。前者可见首帧、操作提示及
`NO_TARGET`；后者可见 `UNKNOWN — DET RECOVERY OFF`、灰色拒绝 car 候选和绿色
tracker 控制框。白色初始化框的边缘被绿色框覆盖，无法从此截图独立确认白框层叠
效果；未观察到成功选择的候选。两个截图 SHA-256 分别为
`39e2ca4cf221fc9c05fdf88ffafe278b464dfe7f4c561506b68b31e5e51548f5`、
`3ac57070626ead0ab19a1c38d03b10ab31bdfa1eff26823ea1bb012e64f9d95e`；
`prepaint.log` SHA-256 为
`134037f819800ccba563b261cdb83cce2fa1fb2ab87f17994a64c037f465b544`。
GUI 启动命令与场景命令使用相同输入，使用
`timeout --signal=INT 45s`、不传 `--bbox`，显式设置
`--max-frames 1 --output-boxes $E/prepaint.csv --session-metadata $E/prepaint.json`。
随后依次运行
`python3 /tmp/target_class_board_gui.py capture $E/prepaint-before-roi.xwd`、
`python3 /tmp/target_class_board_gui.py roi 196,160,184,310`、
`python3 /tmp/target_class_board_gui.py capture $E/prepaint-after-roi.xwd`、
`python3 /tmp/target_class_board_gui.py key q` 并等待正常退出。

### 裁决、边界与 Handoff

本轮 **原生构建、板端 schema/CTest、模型加载和实际初始化推理路径通过**；
`girl_dance` 正例目标类别确立 **失败**：人物全身/局部均 UNKNOWN，默认阈值下
仅检出 car，降低阈值会把左侧人物误判为 truck。UNKNOWN 时恢复关闭、同进程
request ID 隔离与初始化帧不重复写出已观察。板端仅有 `girl_dance` 与
`sot-v1` 系列视频，未确认合法车辆样本，车辆全身/局部场景记为 **未观察**。
没有成功确立 person 或 vehicle，detector 辅助恢复未开启/未触发，不能以
`control_valid=1` 的 tracker 帧替代该门槛；亦无长稳、性能晋级或真实吊舱验收。
总体任务状态仍为 `functional_pass_promotion_held`，公共契约中的
`development_board_validation.latest_handoff.available=false` 必须保持。

请求协调对话采纳本证据提交，保持交叉编译 handoff 不可用，并决定后续 detector
类别误判/样本覆盖的独立修复与重测；不得把本次原生编译成功写成 target-class
功能通过或生产交付批准。完整任务报告在工作树
`.superpowers/sdd/2026-10-29-target-class-establishment/task-9-report.md`（不纳入 Git）。
## 2026-11-11T15:17:45+08:00 C010 post-holdout PARTIAL FAIL

- 分支 `codex/occ-video-1-tracking`，源码提交
  `033dc9058dfc4bf86d46fc74b1b472f689c6922b`，板端根目录
  `/home/neardi/Project/SOT/occ-candidate-033dc90`。源码归档、配置、板端可执行 SHA-256
  分别为 `82d41764a70fecbd6b48cf89571d05fb6c6f0d699d133e125e1347732690009b`、
  `83f1382e57942d44692c687c388133639a2cc94c91de3eff9a09ca688895d274`、
  `5802f441e8907007f54d1cacfa9fa4e0ba362f44786daa12689adde97daf7f51`。
- RK3588 原生 Debug、RKNN、`-Werror=switch` 构建成功，CTest `19/19`；完整运行
  verifier PASS，视频 1920x1080/271 帧，CSV 271 行。运行证据保留于
  `/home/neardi/Project/SOT/occ-candidate-033dc90/runs/C010-full-run1`。
- full evaluator FAIL，仅 `reacquisition` 门失败：帧 200 由 `template_bridge`
  正常接受，但 anchor 的 score-only 低谷持续到 201；单帧预算耗尽后 201--216
  再次 SEARCHING，重捕获帧为 `[185,217]`。其余门全部通过，valid-observation
  mean IoU `0.734841`，all-visible mean IoU `0.721368`。
- 证据边界：这是 C009 揭盲后的迭代，不是盲测；没有执行 C010 重复运行，也没有
  声称云台实机验收。下一步为通用、有界的 C011 两帧桥接设计，不降低可靠性或评估阈值。

## 2026-11-11T15:31:46+08:00 C012 canonical board acceptance PASS

- 分支 `codex/occ-video-1-tracking`，最终源码提交
  `e865c78679247cb45ddbdbf2fb466e49930d5d12`，板端根目录
  `/home/neardi/Project/SOT/occ-candidate-e865c78`。源码归档、配置、可执行 SHA-256
  分别为 `64896748bbedbe16a8bcea762087e3060db0f0a0ba4b7a64cbc113b56755ef69`、
  `82ce1e035e202f10a46f1aa2a21f4ed42d79839dca01b0629e1848c90919432c`、
  `e2a44c74bf6410dc79b5d07a5c134e27cd0096cdca16a81c67d2f27df521ab4d`。
- C011 两帧预算 full FAIL（重捕获 `[185,218]`），三帧诊断仅将失败平移到 203，
  因此停止按帧追参。C012 改为与帧率无关的 200 ms 短时滞回，不改变 tracker
  reliability、geometry 或 evaluator 阈值。
- RK3588 原生 Debug/RKNN/`-Werror=switch` 构建成功，CTest `19/19`。三次完整
  0--270 运行均 verifier PASS、full evaluator PASS，语义签名完全一致：唯一重捕获
  帧 185，模板桥接帧 200--206，190 以后无 SEARCHING；valid-observation mean IoU
  `0.732253`，all-visible mean IoU `0.725693`。
- 最终审查补充了 abort / motion prediction exception 立即撤销 bridge 的 fail-closed
  负例；修复后重新构建并重新执行三次。三次 evaluator JSON SHA-256 均为
  `8748b91ca59a8a91ecb013099d3aa43cb7868b1227c6123a5008c4f16f530aaf`；
  verifier log 均为
  `86808013d246f358f354b6101ef4e84470bb2cd29dfb95fa89482d31d735f02e`。
  49 帧复核视频保留在 run1，SHA-256
  `a3e76846fbf5b256a52226f21d041deec8b102c458fe3810c1f3410eb2f221c6`。
- 证据边界：`PASS_CANONICAL_BOARD_ACCEPTANCE` 仅表示 RK3588 上 canonical
  `occ_video_1` 后揭盲三次验收通过；人工复核视频状态仍为 PENDING，未声称真实云台
  pod 验收。

## 2026-11-12T10:48:45+08:00 C012 main 基线集成门禁

- 用户明确批准 C012 作为不可退化的效果基线。`main@1bf657ff730891ae0bea48340a046cbcbeaa87d4`
  是 C012 最终修复提交 `e865c78679247cb45ddbdbf2fb466e49930d5d12` 的直接祖先，且没有
  main-only 提交；集成分支 `codex/c012-main-integration` 从该 C012 提交创建，不包含其后的
  recent-safe/unified-candidates 实验提交。运行时基线仍由既有 C012 RK3588 证据约束，不把
  主机构建或 Git 快进改写成新的板端验收。
- 新增 `docs/evaluation/occ-video-1/c012-effect-baseline.json` 和
  `scripts/check_c012_effect_baseline.py`。门禁直接读取两个候选运行 CSV：car001 必须使用
  `992,582,37,39`、覆盖 0--270、首次重捕获不晚于 185、190 后无 SEARCHING 且 0--270
  `control_valid=1`；car002 必须使用 `970,517,38,38`，60--80 路牌区间逐帧保持
  TRACKING、accepted 和 control_valid。任一条件失败均禁止替换 C012。
- 实际保留的 C012 car001/car002 CSV SHA-256 分别为
  `e94d3afc3eaff438900f488b0eb9901d9ef97e6d4a1a94330831acde3918936a`、
  `ff46e033c1e900832970fa4558164842004ea9adbc02073e5a2e64717b281f20`；新门禁对两者输出
  PASS，结果 JSON SHA-256
  `b042f585b54775ae1429e1afeff835216c591a24c1b3564784587ea29085126c`。car001 为完整 271
  帧正式 C012 运行；car002 为用户交互运行保留的 242 帧 CSV，本次只冻结已有证据覆盖的
  60--80 路牌区间，不外推为完整 car002 验收。
- 新门禁单元测试 5/5 通过，覆盖晚重捕、190 后再次搜索、控制失效、car002 第 70 帧丢失、
  ROI 不一致、帧缺口与短运行负例。独立 RKNN-off Debug 构建完成、CTest 19/19 通过；
  C012 门禁、occ evaluator 与 robustness checker Python 回归合计 24/24 通过，
  `git diff --check` 通过。
- **Handoff**：请求协调者将 C012 精确历史快进到 main，并以
  `rk3588-c012-baseline` 固定 `e865c78679247cb45ddbdbf2fb466e49930d5d12`。未来候选必须在
  完全相同 ROI 和输入身份下通过上述双目标门禁；编译成功、配置可加载或单目标通过均不能
  替代该门禁。正式生产 handoff 与人工吊舱验收状态保持不变；未修改 `STATUS.md` 或
  `integration-contract.yaml`。
## 2026-11-11T16:10:00+08:00 RK3588 interactive one-command profile startup PASS

- 分支 `codex/occ-video-1-tracking`，任务基线
  `10c311e4576e67036ff30eb2538e9298aab5abfc`，配置/测试/文档提交
  `b3a7e4a`。本机配置为
  `rk3566/nanotrack_v3/config/rk3588-interactive-debug.conf`，板端部署路径为
  `/home/neardi/Project/SOT/occ-candidate-e865c78/rk3588-interactive-debug.conf`；两端
  SHA-256 均为
  `e7daafa4373f676b3e9b10984e48fa2ffe46cc6d9ec1c422dbe3d4e77e88544a`。
- 主机侧先修改 CLI 测试形成 3 个预期失败断言，再更新配置；聚焦 `cli` 测试通过，
  全量 CTest `19/19` 通过，`git diff --check` 通过。配置保留
  `interactive=true` 且不预置 bbox，绑定 C012 已验收的 RK3588 NanoTrack 模型、
  `visible-occlusion-calibrated.conf` 和 RK3588 VisDrone9 detector release；固定输出为
  `/tmp/rk3588-interactive-debug.avi`、`.csv`、`.json`。
- 板端在 `/home/neardi/Project/SOT/occ-candidate-e865c78` 执行
  `DISPLAY=:0 timeout --signal=INT --kill-after=2s 15s stdbuf -oL -eL ./build/nanotrack_v3 --config rk3588-interactive-debug.conf`。
  日志确认 RKNN API `2.1.0`、driver `0.9.8`、detector `target=rk3588 outputs=9`，
  随后出现 `runtime_ready`、三个输出均 enabled，以及
  `interactive_ready playing=0 initialized=0`；冒烟结束后无残留 `nanotrack_v3`
  进程。日志保留于板端 `/tmp/rk3588-interactive-config-smoke.log`。
- 证据边界：本次只验证新配置通过既有二进制加载并进入等待鼠标选框状态；未人工画框、
  未运行完整视频、未生成新的算法验收结论，也未声称真实云台 pod 验收。C012 的
  canonical acceptance 及其源码/可执行身份不因本次运行配置部署而改变。SSH 登录环境
  的 `DISPLAY` 为空，本次冒烟因此显式使用 `DISPLAY=:0`；用户给出的原样两行命令应在
  RK3588 图形桌面终端（已继承 `DISPLAY`）执行，不能把 SSH 冒烟误记为字面命令验收。

## 2026-11-11T22:21:04+08:00 occ_video_1 car_002 271-frame draft generated

- 分支 `codex/occ-video-1-tracking`，本任务执行基线
  `8545f97ac9539231e936dfcc2a322d8916e5f143`，候选工具提交 `1ccff5d`，分层草案打包器
  提交 `8545f97`，review contact-sheet 纳入包与哈希的修复提交
  `5d186f8705b5f8a1bababe51a11ffdd43a080935`，独立审查后的 provenance/validator/
  staged-replace 加固提交 `b1d9dc2`、`d33e848`、`e069bf6` 与 `db20a75`。这是主机侧标注草案生成，不是
  RK3588 新运行或板端验收。
- 原视频 `/home/u20/single-object-tracking/data/occ_video_1/1.mp4` SHA-256
  `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`，严格解码为
  1920x1080、30/1 fps、271 帧。用于辅助定位运行行为的保留证据 CSV/log/JSON/AVI
  SHA-256 依次为
  `ab5f05f2b278ae0216f404f8fee8130b53a396a582a361f0cd00e07fc13ae79f`、
  `269b80673b436db70e2f1d16be56898e69f64b78b114068a128fed7cba6d78b0`、
  `c588f6867043256266a838e2c95517f31bd334355dfbf402e6d53028ae6a8088`、
  `7146e5719363ff4a5611d15b758f67b6e3abc09a5997fd30c6c3358c8d2cce61`；运行证据只作
  诊断参考，不充当真值。
- 工作输入位于
  `artifacts/ground_truth/occ_video_1_car_002_work/`：`anchors.csv`、`candidates.csv`、
  `review-policy.csv` SHA-256 分别为
  `9a618cb492bf5d47b6d6688feb8cf2af7780b6c030438c78c1d027da377cf994`、
  `39c23fa800287843d5e20f3e528c796b7d44bb6315c6c10652c4c82465ca3821`、
  `e9bfa7ab37394b299ce5dc9c482878dc759b5936f84dd6ff78f1f847e6c4f3ac`。
  `distractors.json` SHA-256 为
  `dfcde77592f879ca85924c6155f2f0ffc49c5f1a1925ea6d77c50ac677723f23`。
  候选 review AVI SHA-256
  `ca40b981acb8dd8c4fae41cd75b157dcd77041105eb4e64baa63d81eb5233a67`，解码为
  1920x1080、30/1 fps、271 帧；三张 contact sheet 已逐张查看，SHA-256 为
  `dc3976f3da8d7be84740227b450e52d24e43acf4ce70736a737640e6c5ed2762`、
  `607796268c2b5031c857c6d34d264dd11aa52eaac1e3984541a57c0c79170116`、
  `4968b176b57957a2a8e212b06fa004a0421ba535e79ec6b0cf63439f2b982b94`。
- 草案位于 `artifacts/ground_truth/occ_video_1_car_002_draft/`，
  `checksums.sha256` SHA-256
  `be57dbaee44cfda1e2f925ea08f24a76edbf968c1159076a8c2c7f39303e1e27`，
  `annotation_metadata.json` SHA-256
  `2df9f8ccdb5c546133cac0e7e2bc26111a6aae3635938b9ba23a0ae4f7ce28b2`；连续两次重建
  获得相同两项哈希。
  三层均恰有 271 个连续文件：正式 `labels/` 非空 0，未确认候选
  `candidate_labels/` 非空 252，非可观测估计 `estimated_labels/` 非空 4；
  `frames.jsonl`、`annotation_status.csv` 均为 271 帧，`review_manifest.csv` 有 267 条
  待确认记录。状态计数为 assistant candidate 252、needs human review 15、estimated
  non-observable 4、manual confirmed 0。
- 身份复核结论：目标是桥前初始化白车，桥后为绿色车左侧白车；双向候选在 0--210、
  230--270 连续跟随该车。211--212 与 217--229 为高优先级人工复核区间；213--216
  标为 full occlusion，仅保存低置信度运动估计。运行在 259 帧取得的 detector 候选及
  262 帧恢复对应另一辆同类白车，已以 `car_002_distractor_001` 连续记录 259--264 帧
  （并保留 270 帧参照），不能作为目标真值。
- 实际生成命令为
  `/tmp/nanotrack-car002-tools/generate_bidirectional_candidates --video ... --anchors ...`
  与 `python3 rk3566/nanotrack_v3/tools/build_occ_car002_draft.py build ... --review-dir ...`；
  `validate --require-confirmed false` PASS，`validate --require-confirmed true` 按预期以
  `unresolved review rows: 267` FAIL。三个 layer manifest 与总 `checksums.sha256` 均经
  `sha256sum -c --quiet` PASS；Python 聚焦测试 8/8、annotation-tools CTest 20/20、
  既有主机 CTest 19/19 PASS，`git diff --check` PASS。
- 独立终审指出的四项问题已处理：人工确认必须通过独立 `confirmed_*` 框且保留原候选；
  validator 重新检查几何、枚举、派生字段、估计依据、逐层 YOLO 一致性、状态计数、
  metadata/video/episode 身份、状态与可见率关系、干扰物类型/ID 及 checksum 完整覆盖；
  独立 `validate` 默认绑定 canonical `occ_video_1` 固定 SHA-256，包内即使改绑到另一存在
  文件并提供其正确哈希也会被拒绝；
  `--replace-draft` 先在同级 staging 完整构建并通过
  validator，再以可恢复备份交换（不声称单次系统调用原子交换）；错误同类车补齐
  259--264 逐帧记录。测试覆盖多类语义篡改后重算哈希仍必须失败、结构无效 staging
  不得替换旧包，以及非法输入不得破坏旧草案。
- 证据边界：这是完整覆盖 0--270 的 assistant draft，不是人工确认真值。正式层为空，
  在用户逐帧确认并生成新版本/新哈希前，不授权 IoU、阈值标定、算法准确率、canonical
  dataset 晋级、板端验收或云台 pod 验收；没有修改 `car_001`、`STATUS.md` 或
  `integration-contract.yaml`。

## 2026-11-11T23:33:23+08:00 occ_video_1 car_002 confirmed package generated

- 分支 `codex/occ-video-1-tracking`，任务基线
  `100afb18d371707d9700b40a8f3931d5c22a931c`，结果代码与本记录位于同一 Git 提交。
  用户明确“整体接受”：接受 0--210、230--270 的候选框；确认遮挡区间；确认
  259--264 的候选框仍是正确目标，并与既有运行在 264 帧跳到错误车辆的负面证据严格
  区分。原 assistant draft 未作为本轮生成目的地，也未被重建或覆盖。
- 确认策略输入为
  `artifacts/ground_truth/occ_video_1_car_002_work/review-policy-confirmed.csv`，SHA-256
  `2fd9a8959ddb141292680fc4c5c30f7a0c3a682358788a0def55880eacde75a5`。0--210、
  230--270 共 252 帧使用 `manual_confirmed_visible`，候选框晋级到正式 `labels/`，同时
  保留同框的 candidate 层和候选来源；211--212、217--229 共 15 帧使用
  `manual_confirmed_no_bbox`，只确认遮挡状态与目标身份，正式层和候选层都明确为空；
  213--216 共 4 帧保留 `estimated_non_observable`，估计层不晋级为正式真值。
- 独立确认包位于
  `artifacts/ground_truth/occ_video_1_car_002_confirmed/`。`checksums.sha256` 与
  `annotation_metadata.json` SHA-256 分别为
  `4540ff280903a3e32d8b439f54ddca4497e855095da68ebdbe8fbea044371811`、
  `ddbd8846cd12466d82a833f2f335160a6e6dd1f6189963d268db1b1c54d03a63`；正式、候选、
  估计 manifest SHA-256 分别为
  `2f0e78ab1123cb035c87efa06171f1057c108f38b06fcc5c1ed3a3ec63fdbe98`、
  `c6d1093c16fd17c02c9b8ce1743a85bbc5316dacef76d08e368093a976a6c11e`、
  `62a2877bfc38bf4482e3e1cca1e81551272a677f81d932918723e557db3d24e4`。
  三层仍各有 271 个连续文件，非空计数依次为 252、252、4；
  `review_manifest.csv` 只有表头，未决计数为 0。连续两次生成得到相同 metadata 与总清单
  哈希，`validate --require-confirmed true` 及确认包 `sha256sum -c` 均 PASS。
- 打包器新增 confirmed-package 模式及严格语义：只有零未决项才可写入
  `draft=false`、`formal_evaluation_authorized=true`；`manual_confirmed_no_bbox` 必须身份
  certain、质量 unusable，且 formal/candidate/estimate 三种框都为空。实际执行 Python
  单元测试 10/10、含 annotation tool 的 CTest 20/20、既有主机 CTest 19/19，全部通过；
  `git diff --check` 通过。
- 复核旧 draft 时发现其
  `review_contact_sheets/contact-sheet-000.jpg` 当前缺失，旧总清单仍期望其 SHA-256
  `dc3976f3da8d7be84740227b450e52d24e43acf4ce70736a737640e6c5ed2762`，因此旧 draft
  当前独立校验失败。工作输入和新 confirmed 包都含有该文件且哈希匹配；为遵守“保留原
  draft、不覆盖”的边界，本轮未修改或修复旧 draft，并把此漂移作为外部证据异常记录。
- 证据边界：`formal_evaluation_authorized=true` 只授权新 confirmed 包中 252 个非空正式
  框参与 ordinary IoU；211--229 中无正式框的帧不能被当成零框误计 IoU，213--216 的
  估计框也必须排除。该确认不自动完成阈值标定、canonical dataset 晋级、RK3588 新运行、
  板端验收或真实云台 pod 验收；未修改协调者所有的 `STATUS.md` 与
  `integration-contract.yaml`。

## 2026-11-11T23:44:37+08:00 dual ground-truth sequences installed

- 分支 `codex/occ-video-1-tracking`，任务基线
  `4d38c28f31f125479bcc24c92231584a9cdde3e1`。按用户要求，将第二目标 confirmed 包从
  `artifacts/ground_truth/occ_video_1_car_002_confirmed/` 移至真实 canonical 数据目录
  `/home/u20/single-object-tracking/data/occ_video_1/ground_truth/car_002/`；原 artifacts
  confirmed 路径现已不存在。共享视频仍只有根目录 `1.mp4` 一份，不复制视频。
- 新增 `ground_truth/index.json` 与 `ground_truth/README.md`，SHA-256 分别为
  `42d08ea98642f81640842dd166ff8ba3518668441d430ce9931f99d1dc544177`、
  `2b2b9e10e7d70cecffc693f7044cde6a61128e8517188bf897bdcae6578dc6b9`；既有第一目标
  标注以逐文件相同的镜像放入 `ground_truth/car_001/`，根目录旧路径继续保留以兼容现有
  公共契约和旧命令。镜像 metadata/status/events/labels manifest 哈希仍分别为
  `5641650435a1980694f982cf1fa6715ba959b1515938cee383c8db8ec4aeb076`、
  `e44fd23514d6871e72d628ad4843fc90bb4246f306322ee297c8a2aa75818036`、
  `a43f016c96b34c8a0ec6125dcb944ba4fa07a79d99a46ce696bbb29841ded256`、
  `9ec01854884126350f93e1af3a4aee002d38ce7c2d50c71c78de0f1a8dc0a648`，271 项标签
  `sha256sum -c` 全部通过。
- 第二目标迁移后 `checksums.sha256` 仍为
  `4540ff280903a3e32d8b439f54ddca4497e855095da68ebdbe8fbea044371811`，严格 confirmed
  validator PASS；metadata/status/events/formal manifest/estimate manifest 哈希分别为
  `ddbd8846cd12466d82a833f2f335160a6e6dd1f6189963d268db1b1c54d03a63`、
  `212525642591c124320e93369f8748d82cc53092628beff051507f930946dec1`、
  `efc0652335fcf19c766def88bf50bb8defab1441fdc8fbb412b85d02135c31db`、
  `2f0e78ab1123cb035c87efa06171f1057c108f38b06fcc5c1ed3a3ec63fdbe98`、
  `62a2877bfc38bf4482e3e1cca1e81551272a677f81d932918723e557db3d24e4`。
- `scripts/evaluate_occ_video_1.py` 新增
  `--target-id {car_001,car_002}`，默认仍为 `car_001`。第二序列读取空正式标签、独立估计层
  与新遮挡事件：252 帧允许 ordinary IoU、4 帧仅作估计轨迹参考、15 帧无 bbox 且不参与
  几何评分。第一序列继续按 `source` 排除 169--181 共 13 个插值帧，避免被同一 CSV 的
  `review_status` 覆盖。双序列身份加载分别得到 271 帧；合成完美运行对两序列 full evaluator
  均为全部 gate PASS。Python evaluator 单元测试 16/16 通过。
- 后续同一运行 CSV 可分别执行：
  `python3 scripts/evaluate_occ_video_1.py --dataset-root /home/u20/single-object-tracking/data/occ_video_1 --target-id car_001 ...`
  与将 target 改为 `car_002` 的第二次评估。一次运行的初始化目标必须与所选真值目标一致，
  不能用初始化 `car_001` 的 CSV 对 `car_002` 结论作有效性声明。
- Handoff：请求协调对话把双序列目录、索引和上述新哈希纳入
  `integration-contract.yaml`，并决定板端数据复制路径；本任务按所有权规则未直接修改
  `STATUS.md` 或公共契约。此次仅完成主机数据安装和 evaluator 消费能力，不是新的 RK3588
  算法运行、阈值标定、板端验收或吊舱验收。

## 2026-11-12T02:38:54+08:00 RS001 recent/safe RK3588 原生验证 HOLD

- 分支 `codex/occ-video-1-tracking`，工作树
  `/home/u20/.codex/worktrees/rk3588-debug-deploy/single-object-tracking`，Task 6 基线
  `68b527659e8be39304ab805c9a6f85219c2af5ce`。板端源码先提交为
  `e027006339f0cdafb0b8d38c4dfa5e14abe66593`，证据与本条记录同提交
  （`test: validate recent-safe recovery on RK3588`）。
  `git archive` 两端 SHA-256 均为
  `fc8deb4fd4f96d28ab27be34feaf2133e6e8dc75937380b8304b913220af080e`；
  板端逐文件比对 314 个归档文件内容与执行权限通过。
- 实际板为 `LKD3588`，Linux 6.1.84 aarch64。独立候选根
  `/home/neardi/Project/SOT/occ-recent-safe-rs001`。RKNN API
  `2.1.0 (967d001cc8@2024-08-07T19:28:19)`、driver `0.9.8`；
  运行前用同一提交的 RKNN loader 逐一查询三份 tracker 模型和 detector。
  `build/nanotrack_v3` SHA-256
  `c4161c0a448351f4a6d84be60bc00ce8c7c6e98ed28134efb5069d7562821304`，
  `visible-recent-safe-v1.conf` SHA-256
  `7b038eed1dd052de5879bedbf465a2ded3cc79d5a6f0b662e39d9fd06fc15c34`（`calibrated=false`）。
  原生 CMake 使用 Debug、`ENABLE_RKNN_RUNTIME=ON`、`BUILD_TESTING=ON`、
  `-Werror=switch`，`cmake --build build --parallel 4` 和
  `ctest --test-dir build --output-on-failure` 通过 22/22（13.85 s）。
  主机 RKNN-off CTest 22/22、evaluator Python 19/19、语法检查和 `git diff --check` 通过。
- 两次各读取独立 ROI 和真值，`car_001=992,582,37,39` 使用 canonical 标注，
  `car_002=967,513,42,42` 使用已确认包（252 帧正式可观测框）。
  两次均运行 0–270 帧，1920×1080、30/1 fps，runtime exit 0，
  schema v11 verifier PASS，271 行 CSV 与实际 CLI 发布的 271 行 JSONL companion 均完整。
  模型、标注、输出哈希、完整 evaluator 门槛、源码与运行命令记录在
  [RS001 评估记录](../evaluation/occ-video-1/recent-safe-candidate.json)；
  大工件留在板端 `runs/car_001/` 与 `runs/car_002/`，每个目录有 `SHA256SUMS`。
- `car_001` full evaluator **FAIL**。172–173 帧未满足完全遮挡语义，174 帧开始搜索，
  `[182,190)` 内及整段均无重捕获，248–270 帧控制无效。最后可信帧 172，
  到首次失去控制经过 2534 ms。有效观测 mean IoU 0.764074（168 帧），
  全部可见帧 mean IoU 0.602621。候选冲突 76 帧。
- `car_002` full evaluator **FAIL**。70 帧进入搜索后无重捕获，145–270 帧控制无效；
  213–216 帧完全遮挡语义和估计轨迹连续性均失败，`[230,271)` 无重捕获。
  最后可信帧 69，到首次失去控制经过 2533 ms。有效观测 mean IoU 0.910586（70 帧），
  全部可见帧 mean IoU 0.302287。候选冲突 143 帧。两目标 identity proxy 均 PASS，
  其结论限于有权使用的可见观测，两次均未提交新恢复身份。
- recent/safe/staged 物理 head 调用分别为 `270/96/14` 与 `270/200/6`，
  safe 复用各 0 次，任何帧最多 2 次，未观察到模板事务应用失败。
  runtime p50/p95 分别为 7.82/9.97 ms 和 8.71/10.55 ms，pipeline p50/p95 为
  109.90/113.42 ms 和 111.02/113.78 ms。板端缺少 `/usr/bin/time`，以约 100 ms
  间隔采样进程 `/proc/<PID>/status`，观察到 VmHWM 237260 / 235476 kB。
- **Handoff**：RS001 保持 HOLD 和 `calibrated=false`。两次均按原配置一次运行，
  C012 图形启动仍使用原路径与配置；其二进制、交互配置和控制器配置 SHA-256
  前后一致。需复查 RS001 时，在候选根执行
  `cat runs/car_001/commands.txt runs/car_002/commands.txt` 查阅准确诊断命令，
  新诊断使用新的输出路径。证据限 RK3588 原生 Debug 的两目标算法结果。
  请求协调对话继续保留正式 handoff 门槛，生产交叉编译与人工吊舱验收未启动。

## 2026-11-12T02:53:15+08:00 RS001 v11 evaluator 复核与重新评分

- 在 `codex/occ-video-1-tracking` 的既有证据提交
  `fcab231098371b4e659282b1472b7faf243934e5` 上修复 evaluator。板端运行源码仍为
  `e027006339f0cdafb0b8d38c4dfa5e14abe66593`，重新评分脚本提交为
  `eb344708429949356c9f0161253bb326e0c16502`，脚本 SHA-256
  `57bdd5d6824c5517adaeeac1910152b52dbc1721589983cea548c6c4f5f1b7f4`。
  v11 重捕获必须来自可信跟踪观测和已应用的恢复阶段；`LOST` 禁止控制和观测声明；
  完全遮挡门直接核对观测缺失、可信标记和已获授权的运动预测。新增负例先失败，修复后
  evaluator Python 23/23、完整主机 CTest 22/22、Python 语法与 Git diff 检查通过。
- 用该提交的脚本在 RS001 分别重新评估保留的 `runs/car_001/bboxes.csv` 和
  `runs/car_002/bboxes.csv`，沿用各自正式真值及 session metadata。两次仍为 **FAIL**，
  每项 gate 的布尔结果和失败帧与原报告一致，均无重捕获或无效重捕获声明。
  更新后的 JSON SHA-256 分别为
  `b338b8b8ce5967d377bbdf15751a8ab36b63023151758e162937a34a88ddee19` 和
  `3e37c58e1387769203353dd81dcebbacf441ec55a269522b13deb1426fbd1846`，
  Markdown SHA-256 分别为
  `0de2acb648f2aa4fd42e2ffdcc583392bf28031b7a882b29402edfa4ebb011ae` 和
  `b2673e732693c3f43e15caf4abe9723ae638d0440e5808dc4c3d53aced6c21b0`。
  新 `SHA256SUMS` 均通过复核，非 evaluator 文件逐项哈希与旧清单一致；原报告及清单保留于
  两个 run 的 `evaluator-original-e027006/`。准确命令、原始及更新后的哈希见
  [RS001 评估记录](../evaluation/occ-video-1/recent-safe-candidate.json)。
- **Handoff**：保持 RS001 **HOLD**。本次仅对保留 CSV 重新评分，新增推理运行 0 次。

## 2026-11-12T09:56:47+08:00 latest HEAD RK3588 双目标视频运行与回传

- 分支 `codex/occ-video-1-tracking`，运行源码提交与任务基线均为
  `3ec5d56c72567b4e3eb6cab89cda149f51ce77f3`。源码通过 `git archive` 部署，归档在
  主机与板端核验的 SHA-256 均为
  `5d523e03c7d13a70b4065e9f450f8c79f1b0627d2f7a652610a8a68ba7114cc1`；独立板端根为
  `/home/neardi/Project/SOT/occ-latest-3ec5d56`，未覆盖 C012 或 RS001。
- 实际板为 `LKD3588`、Linux 6.1.84 aarch64，RKNN API
  `2.1.0 (967d001cc8@2024-08-07T19:28:19)`、driver `0.9.8`。原生 Debug 构建启用
  `ENABLE_RKNN_RUNTIME=ON`、`BUILD_TESTING=ON` 和 `-Werror=switch`，CTest 22/22 通过；
  新 `build/nanotrack_v3` SHA-256 为
  `8b30b04ce84dcfe4b4209a9aff7cf94a21697b78c35d2422fb2cee9396aebd38`。
- 输入仍为 `data/occ_video_1/1.mp4`，SHA-256
  `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；控制器配置
  `visible-recent-safe-v1.conf` SHA-256
  `7b038eed1dd052de5879bedbf465a2ded3cc79d5a6f0b662e39d9fd06fc15c34`
  （`calibrated=false`）。tracker manifest 和 RK3588 detector SHA-256 分别为
  `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`、
  `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c`。
- `car_001=992,582,37,39` 与 `car_002=967,513,42,42` 分别独立运行 0--270 帧，
  runtime 均 exit 0；schema v11 `verify_board.sh` 均 PASS，CSV 均为 271 数据行，输出视频
  均经 `ffprobe` 确认为 1920x1080、30/1 fps、271 帧。板端完整结果保留在
  `runs/car_001/` 与 `runs/car_002/`。
- 两段视频已回传至 `/home/u20/occ-video-1-latest-3ec5d56-results/`，并在 u20 端重新
  解码计数和复核哈希。`car_001-3ec5d56.avi` SHA-256 为
  `7d0438ea9da9dc08f5f7effa5d0084668c9c97091b8e818e4e2a5e11eb5aa1fd`；
  `car_002-3ec5d56.avi` SHA-256 为
  `dee808467725707228f5eda6045f21aa8388425bc3099855189acab28fc0e0ef`。
- **证据边界**：本次按用户要求完成最新程序的两目标单次运行、结构校验和视频交付，未执行
  full evaluator，也不改变 RS001 HOLD、`calibrated=false`、正式 handoff 门槛或人工吊舱
  验收状态；未修改 `STATUS.md` 或 `integration-contract.yaml`。

## 2026-11-12T13:07:25+08:00 recent-safe v2 阶段 1：单指标瞬降板端验证

- 分支 `codex/recent-safe-occlusion-recovery-v2`，任务基线
  `ef0c02130fba4aa900f85daf3341943bd408e634`，本阶段源码提交
  `c3d27bea3087f42ae60b8aa7e891bbc9a61fe605`。改动只调整 recent 观测判定：恰好一个
  响应指标失败时发布 tentative/SUSPECTED 观测，不更新可信运动、响应基线或模板；尺寸
  塌缩和多个绝对响应指标失败仍直接进入 SEARCH。主机 RKNN-off CTest 22/22、C012
  门禁 Python 单测 5/5 和 `git diff --check` 通过。
- 源码以 `git archive` 部署到实际板 `LKD3588` 的独立目录
  `/home/neardi/Project/SOT/occ-recent-safe-v2-c3d27be`，未覆盖 C012、RS001 或 latest。
  `source.tar` SHA-256 为
  `42741709a2dc50150c558cd4452b95dafbaa7eadfcbb1814b4613784aba915b3`。
  板端原生 Debug 构建启用 `ENABLE_RKNN_RUNTIME=ON`、`BUILD_TESTING=ON` 和
  `-Werror=switch`，CTest 22/22 通过；`build/nanotrack_v3` SHA-256 为
  `668aa05e6a7981f428afd3591d7436862575864afb39e13499dace806639ce52`。
- 使用固定 C012 A/B 初始框 `car_002=970,517,38,38`，同一视频、tracker 模型、detector
  和 `visible-recent-safe-v1.conf` 单次运行 0--270 帧。视频、配置、detector SHA-256
  分别为 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`、
  `7b038eed1dd052de5879bedbf465a2ded3cc79d5a6f0b662e39d9fd06fc15c34`、
  `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c`；三份 tracker
  RKNN SHA-256 记录在板端 `provenance/car002-inputs.sha256`。runtime exit 0，schema v11
  verifier PASS，CSV 和 1920x1080 AVI 均为 271 帧。
- 60--80 帧没有 SEARCH 且全部 `control_valid=1`。第 70 帧从 RS001 的直接 SEARCH 变为
  一帧 SUSPECTED，仍输出 NanoTrack 观测框并保持 observation control；第 71 帧恢复
  TRACKING。第 70 帧框为 `894.354431,618.288696,36.756348,37.964905`，与 C012 同帧
  `896.088928,618.415588,37,38` 接近。新 CSV、AVI 和 session metadata SHA-256 分别为
  `69fd729a37415b8c9071b4e9527c367e0b791e8fc13fcf4017399c1578b2fa67`、
  `6350da6ec20043f9bce4cce36dbefad108f4eb5745182d3812b12b741a6a9abd`、
  `a054f00d3d02b25042fb730a594299401a79705773a11319b91619d42f26bda7`；完整大工件留在
  板端 `runs/car_002/`。
- **证据边界 / Handoff**：阶段 1 的遮挡判定目标在 car002 上达到，但现有 C012 晋级脚本
  要求 60--80 每帧均 `tracking_phase=TRACKING` 且 `accepted=1`，因此会在有意冻结可信
  状态的第 70 帧报告失败。此处不修改或放宽协调者批准的门禁，不宣称候选已达到 C012
  全部晋级条件；C012 继续作为默认效果基线。尚未实施阶段 2 重捕获候选关联、阶段 3
  持续搜索控制，也未完成 car001 新运行、生产交叉编译或人工吊舱验收。

## 2026-11-12T13:38:53+08:00 recent-safe v2 三阶段 RK3588 双目标验证 HOLD

- 分支 `codex/recent-safe-occlusion-recovery-v2`。阶段 2 候选关联提交
  `d36ae22a200522314f7b8e0a4362e04e0cd97bc3`，恢复 probation 提交
  `5ce180dd4e14b04e9328150fd87bc38ca58e0c33`、`b18bb8241e39fc0f13886564d6e843eb584f0d0f`，
  阶段 3 持续有界搜索控制提交
  `b53866785d4a50855f3c2abf08ccb89f2aa69b9d`。后续实验
  `ffe8bd9a466b13de641a62b51928903e118759a7` 试图让持续单指标下降快速进入 SEARCH，
  但板端双目标均出现多次正常段误 SEARCH/误重捕获，已由
  `b8aa1006b66843a37f711e147ce4f92632279317` 明确撤销；当前分支树与已验证的
  `b538667` 相同。未修改阈值、配置或 ROI 扰动策略。
- 保留候选源码部署根为
  `/home/neardi/Project/SOT/occ-recent-safe-v2-b538667`，`source.tar` SHA-256
  `ed2f4f61014a4ef107eabe44d7a2d02b57ee2a42dc6a42f2582cedf2b36c8ed2`。
  RK3588 原生 Debug CTest 22/22 通过；主机 RKNN-off CTest 22/22、C012 门禁单测 5/5
  与 `git diff --check` 通过。板端 `build/nanotrack_v3` SHA-256 为
  `c97945d117dc6d611f222477308dfe4a7a07d2067b0c306872c091d3b557c3c2`；配置 SHA-256
  仍为 `7b038eed1dd052de5879bedbf465a2ded3cc79d5a6f0b662e39d9fd06fc15c34`
  （`calibrated=false`）。视频、detector 和三份 tracker 模型哈希均与 RS001 相同，精确清单
  位于板端 `provenance/final-inputs.sha256`。
- 最终固定 A/B ROI 运行：`car_001=992,582,37,39`、`car_002=970,517,38,38`，各
  0--270 帧。两次 runtime exit 0、schema v11 verifier PASS、AVI 1920x1080 共 271 帧；
  两次均全程 `control_valid=1` 且无 `LOST`。car001 在 185 帧重捕获，186 帧只进入一帧
  SUSPECTED，187--189 由 staged 承接并提交 recent，190--270 无 SEARCH。car002 在
  60--80 无 SEARCH，第 70 帧为单帧 SUSPECTED 且 observation control 有效；后续搜索在
  运动置信度衰减后仍保持有界 `MOTION_SEARCH` 控制。
- car001 CSV/AVI/session metadata SHA-256 分别为
  `82e23ad08e80d976ed072187ea1e0e2344a220595f2a925e3e33ce302792c89a`、
  `4f40eeb1b35b5a23d4814213a6bdeabde6587d7b5ea4f72a481bf7d4a23c0a89`、
  `e5db2fd2225e580c00a0ece936aa747780cd081ce6d2e5532809060e1cb39944`；car002 对应为
  `2c03b7de9803922e83fc75ab46e09ba08211005c7f8ff6d09eebb607abda427e`、
  `cde0c9267243483c1319b7c65454b761d1cb55575a7abdd54d3043b780b6eb75`、
  `49dbc0385e5d8da53a34e2264f72a379520a1d5448c2a7882bb8416843a8c07b`。
  大工件和完整日志保留在板端 `runs/car_001/`、`runs/car_002/`。
- full evaluator：car001 **PASS**，有效观测 mean IoU `0.752494`，重捕获恰为 185，
  evaluator JSON SHA-256
  `7b945e043c0c9c5f6c485b5f20beb5f00e361f7aa77c12cd2a75a804a5a67c3e`；car002
  可见框、连续控制、identity proxy、预测连续性和 235 帧重捕获均 PASS，但 213--216
  完全遮挡仍声明观测，`full_occlusion_semantics` FAIL，JSON SHA-256
  `886b4aead8f8595bd721c57ae155159969c8cea09ae51b7531d3757dc62101bf`。
  现有 C012 双目标脚本另因 car002 第 70 帧有意为 `accepted=0` 的 SUSPECTED 状态而在该
  单帧 FAIL；视觉框和控制未中断，但未经协调者批准不放宽该门禁。
- **结论 / Handoff**：相对 RS001，car001 的永久身份冲突、恢复后立即回滚及搜索控制失效
  已修复；car002 路牌段不再丢失，且后段可重捕获。候选仍为 **HOLD**，C012 保持正式效果
  基线和默认部署。未解决项是 car002 完全遮挡的提前识别，以及 C012 门禁是否应允许
  `SUSPECTED + observation control + accepted=0` 的单帧冻结语义；不得用已否决的单指标
  快速升级规则解决。未启动生产交叉编译或人工吊舱验收，未修改协调者所有的
  `STATUS.md` 或 `integration-contract.yaml`。
