# 生产交叉编译任务进度

- 任务状态：`candidate_cross_compile_succeeded_handoff_unapproved`
- 最近核对：`2026-10-31T16:24:12+08:00`
- 核对分支/工作树：`codex/cross-compile-main-b827` / `/home/u20/single-object-tracking/.worktrees/cross-compile-main-b827`
- 核对基线：`main@b827c0a6d8916e9c0832c748e30722f6f2fe5563`
- 责任边界：确认能够取得协调对话指定的最新开发板验证结果，并使用现有交叉编译流程成功生成生产产物。

## 前置条件

开发板验证在本任务之前完成。开发板具备完整原生编译环境；交互式 Debug 版本用于板端开发、调试和测试。只有开发板任务形成明确 handoff 后，交叉编译任务才消费该结果。

“最新结果”不以文件时间戳判断，而是由协调对话锁定的以下身份共同确定：

- 已通过相应开发板门槛的源代码 commit；
- 对应模型、manifest 和 labels 路径及 SHA-256；
- 必需配置和接口版本；
- 开发板验证记录路径及裁决。

## 本任务只需确认

1. 能取得上述精确 commit 和工件，而不是旧工作树、旧板端目录或未提交副本。
2. 交叉编译命令针对这份最新结果执行并返回成功。
3. 记录生产产物路径；如流程方便，同时记录产物 SHA-256，便于人工部署时核对。

## 不属于本任务

- 不负责算法、模型或交互式 Debug 功能开发。
- 不负责开发板原生编译、调试或板端验收。
- 不要求重新建立完整工具链/sysroot provenance，只使用现有交叉编译环境完成编译确认。
- 不负责把产物部署到真实吊舱。
- 不负责真实吊舱测试，也不得声明吊舱测试通过。

## 2026-10-22 首次同步与重新核对

### 实际检查结果

- 当前基线已包含协调规则提交 `29b083e4e6ced2fd46eb7f9ecf821287b4cc401a`；同步前主工作树位于 `main`、HEAD 正好等于该提交且工作树干净，因此没有执行 merge、rebase、reset 或文件覆盖。
- 本任务按仓库规则在独立分支和工作树中重新核对；开始核对时该工作树 HEAD 同样为 `29b083e4e6ced2fd46eb7f9ecf821287b4cc401a`，无未提交修改。
- 已依次读取根目录 `AGENTS.md`、`docs/progress/STATUS.md`、`docs/progress/integration-contract.yaml` 和本文件；同时按上游依赖读取 `docs/progress/board-validation.md`。
- `integration-contract.yaml` 的 SHA-256 为 `b9698f121c36f8db6eb57b715ccb8ffe12b8a26d3952c6702f905b4dae9f49ac`。其中 `development_board_validation.latest_handoff.available=false`，`source_commit`、`model_artifacts`、`configuration`、`evidence` 和 `verdict` 全部为 `null`。
- `board-validation.md` 的 SHA-256 为 `94546ad2efd0038e30bb66ec83abd15fb60ddbffc373dfaf6b5fd87ee308dfb4`；其当前裁决为 `functional_pass_promotion_held`，明确记录 detector-confirmed reacquisition、内存平台化及可见光/红外标定等门槛尚未通过。
- 因协调者尚未发布可消费的板端 handoff，本任务目前没有被授权消费的源 commit、模型/manifest/labels 哈希、配置或板端裁决，故不能把任何旧工作树、旧 WSL 源码或历史 `viewlink_app` 产物当作本轮输入。
- 本轮没有执行交叉编译。此处的阻塞发生在输入身份确认之前，不是编译器或代码构建失败。

### 实际执行的核对命令

```bash
git status --short --branch
git branch --show-current
git rev-parse HEAD
git branch --all --contains 29b083e4e6ced2fd46eb7f9ecf821287b4cc401a
git show --no-patch --format='%H%n%P%n%D%n%s' 29b083e4e6ced2fd46eb7f9ecf821287b4cc401a
sha256sum docs/progress/integration-contract.yaml \
  docs/progress/board-validation.md \
  docs/progress/cross-compile.md
```

观察结果：主工作树同步前为干净的 `main@29b083e4e6ced2fd46eb7f9ecf821287b4cc401a`；任务工作树创建后为干净的 `codex/cross-compile-progress-sync@29b083e4e6ced2fd46eb7f9ecf821287b4cc401a`；协调契约没有可消费 handoff。

## 当前阻塞与证据边界

- **阻塞项：** 等待开发板验证任务形成达到协调者要求的结果，并由协调对话把精确身份写入 `development_board_validation.latest_handoff`。
- **证据边界：** 当前只能证明生产交叉编译任务已经同步协调基线、读取当前契约并识别到输入未就绪；不能证明已经取得最新板端结果，不能证明本轮交叉编译成功，也不能证明部署或真实吊舱验收。
- **不采用的历史证据：** 早期 A6 构建成功、旧二进制路径或旧对话中的哈希不属于当前协调者锁定的 handoff，除非协调对话以后明确把完全相同的身份发布为最新 handoff。

## 2026-10-30 用户授权的候选生产交叉编译

### 输入身份与裁决

- 时间：`2026-10-30T17:55:23+08:00`。
- 分支/工作树：`codex/cross-compile-main-b827` / `/home/u20/single-object-tracking/.worktrees/cross-compile-main-b827`。
- 精确仓库核心源基线：`main@b827c0a6d8916e9c0832c748e30722f6f2fe5563`；开始时工作树干净。WSL 的生产 C ABI adapter 和 `ai_alg/CMakeLists.txt` 不在该仓库 commit 中，是本轮基于备份版本完成的生产集成文件，其最终哈希在下文单独锁定。
- 用户明确授权在公共契约尚未发布 handoff 的情况下，对当前 `main` 和保留的 canonical COCO detector 执行一次候选生产交叉编译。该授权只允许验证生产编译链，不改变仓库规定的开发板/真实吊舱验收边界。
- 当前 `docs/progress/integration-contract.yaml` SHA-256 为 `3d52ec1e0741620e38bb48810bbd1b8f1fb7d89df0c7989d208afecee87504a6`，其中 `development_board_validation.latest_handoff.available=false`；`docs/progress/board-validation.md` SHA-256 为 `840756c8f8230ff5c170f14ed1a5428e5d239b403155f7050db30fe84267dcc1`。因此本节结果是候选编译，不是协调者批准的板端 handoff 消费结果。

canonical COCO detector 的部署身份如下；RKNN 哈希来自跟踪仓库中的 manifest 声明，本轮交叉编译环境没有复制或执行模型，不能把声明哈希提升为本轮模型加载证据：

| 工件 | 生产路径/来源 | SHA-256 |
| --- | --- | --- |
| COCO RKNN | `/app/model/yolov8n-rk3566-fp16.rknn`；由 manifest 声明 | `4d53a9b1e12996556294be8e099fe2dc0815d5c9cfdc327edfe350832baf8cf6` |
| detector manifest | `/app/model/detector-manifest.json`；仓库 `rk3566/nanotrack_v3/models/detector-manifest.json` | `8adcb1026803d688a7b85c55977d533eac6c15070ac4260bedb09d1da49d6061` |
| COCO80 labels | `/app/model/coco80.txt`；仓库 `rk3566/nanotrack_v3/models/coco80.txt` | `bd17f1ee35d5f3c862a4894605855abbb9dda4b0621fdb0ac4c2c8c7bb7e730a` |
| production controller config | `/app/model/visible-default.conf`；仓库 `rk3566/nanotrack_v3/config/visible-default.conf` | `b9a128b78063056173626af1c7c7d2c580a59b295c45a4afc0ef8e2e04bff95b` |

### 同步、备份与生产边界

- WSL 工程：`/home/wjm/viewpro_gimbal_zp/rk3566/gimbal_tracker`。
- 修改前将 `ai_alg` 完整复制为 `/home/wjm/viewpro_gimbal_zp/rk3566/gimbal_tracker/ai_alg.backup-20261030-before-main-b827`；原目录和备份各有 1273 个文件，`diff -qr` 无输出。备份中的 `source/native-lib_impl.cpp` SHA-256 为 `8f814a3bc282b3783e7d290f40abee7503cb3465d8a095fd2b8f8b665d1aec04`，`CMakeLists.txt` SHA-256 为 `75cfd954fdda7e2b1a87f2254ef7f568cdbf7a6a61b3bd1296f4d6a5811d682c`。
- 通过 SSH 复用连接和 `scp` 只同步生产白名单：16 个头文件、13 个核心源文件、`visible-default.conf`、生产 C ABI adapter 和 `ai_alg/CMakeLists.txt`，合计 32 个文件。本地基线与 WSL 落盘文件逐文件 SHA-256 对比无差异。
- 最终 WSL `ai_alg/source/native-lib_impl.cpp` SHA-256 为 `08735f05ea5e631ca459fce7cc164428d69ac77caf2a349f3b66ae2d76c5391b`；最终 `ai_alg/CMakeLists.txt` SHA-256 为 `2dc1029fc7b92d75fc028bb8531552f05d76eeffa505d6bfb0a3adc6105d8dae`。
- 纳入的 Debug 分支生产改进包括位置门控、门控后候选上限、`OCCLUSION_HOLD`、单帧异常终止保护和目标类别初始化；adapter 使用 `YoloRknnEngine::labels().size()` 配置自动类别建立。
- 未同步 `main.cpp`、CLI/运行时参数解析、HighGUI、键鼠交互、逐帧播放、调试覆盖层、调试遥测、视频/CSV/metadata 输出和 tracking log。生产源目录扫描以及最终归档成员/符号扫描均未发现这些对象或引用。
- C ABI、RGB888 `1920x1080`、stride `5760`、`STATE_INIT_TRACK` 当帧输出、正常/不确定/丢失均返回 `true`、丢失期输出预测框等既有 adapter 语义保持不变；本轮没有在开发板上执行这些运行时行为。

### 实际命令与观察结果

主机侧先对精确源基线执行 RKNN-independent 回归：

```bash
cmake -S rk3566/nanotrack_v3 -B /tmp/nanotrack-main-b827-host \
  -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF
cmake --build /tmp/nanotrack-main-b827-host -j4
ctest --test-dir /tmp/nanotrack-main-b827-host --output-on-failure
```

观察结果：`16/16` 通过。

WSL 中只执行 A6 生产构建：

```bash
cd /home/wjm/viewpro_gimbal_zp/rk3566/gimbal_tracker
BUILD_JOBS=4 ./run.sh --platform A6
```

观察结果：退出码 `0`；CMake 重新配置；GNU AArch64 C++ `10.3.0`；`target_class.cpp.o` 和其余生产核心进入 `libAI.a`；`viewlink_app` 链接成功。

| 产物 | 大小/时间（Asia/Shanghai） | SHA-256 |
| --- | --- | --- |
| `/home/wjm/viewpro_gimbal_zp/rk3566/gimbal_tracker/build_A6/viewlink_app` | 32394856 bytes；`2026-10-30 17:53:28 +0800` | `768e65e635c3c78ff9d61f8963e8131ec5cb897c301fefcaffc31d17003da988` |
| `/home/wjm/viewpro_gimbal_zp/rk3566/gimbal_tracker/build_A6/ai_alg/libAI.a` | 513498 bytes；`2026-10-30 17:53:27 +0800` | `8050dd530d3ce03538b740386c8e5c8fd2ec525888ed19e0dab8a550ead7da20` |

`file` 将 `viewlink_app` 识别为 ARM aarch64 ELF。`libAI.a` 含 14 个预期对象并导出真实的 `tracker_detector_init`、`do_tracker_detector`、`sr_model_init`、`sr_infer_frame` C 符号；无 stub、CLI、main、HighGUI、交互或调试对象/符号。

### 风险、阻塞和证据边界

- 交叉环境链接的 `A6_libs/librknnrt.so` SHA-256 为 `135f497b820ae161d36f756c26b949419d271918e915e71663d424e5c1d799e7`，可见版本字符串为 RKNN runtime `1.5.2`；canonical COCO manifest 记录模型由 toolkit `2.3.2` 转换，而最新板端验证使用 RKNN API `2.3.2`。本轮只完成链接，没有模型加载/推理，故旧链接库与 2.3.2 模型/部署 runtime 的兼容性仍是明确风险。
- 开发板最新记录仍为 `functional_pass_promotion_held`，且公共契约 `latest_handoff.available=false`。本次候选构建不绕过该门槛，不把 `b827c0a` 宣称为协调者批准的生产 handoff。
- 没有把产物部署到开发板或真实吊舱，没有执行板端模型加载、推理、跟踪、长期恢复、内存平台或场景验收；不能声明开发板或吊舱验收通过。
- 下一步由协调对话决定是否更新公共契约并锁定可消费 handoff；若以后批准的源 commit、模型、配置或 runtime 身份不同，必须重新交叉编译。人工可使用上述路径和 SHA-256 取得候选产物进行后续受控部署。

## 2026-10-30 实际测试板候选目录暂存

- 时间：`2026-10-30T18:19:53+08:00`。
- 经 Windows/WSL 连接 `root@192.168.2.119`，板端身份为 `rk3566_rk3568`、AArch64、Buildroot 2021.11、Linux 5.10.198；只执行文件检查和候选目录暂存，没有启动程序或执行板端测试。
- 新建隔离目录 `/app/nanotrack_b827_candidate`，没有覆盖 `/app/bin/viewlink_app`、`/app/viewlink_appnt3` 或 `/app/model`。原程序在操作后 SHA-256 仍分别为 `a60b61a34cdaf0b43d27726a5ce311e9e995a8d34bb874657c944eabd86fb357` 和 `55d5d709ac08ac7dd119e7c6fe01b96263afd1eeb0f7d36318d2152f330c3598`。
- 已暂存并在板端复核：
  - `/app/nanotrack_b827_candidate/bin/viewlink_app`：`768e65e635c3c78ff9d61f8963e8131ec5cb897c301fefcaffc31d17003da988`；
  - `model/coco80.txt`：`bd17f1ee35d5f3c862a4894605855abbb9dda4b0621fdb0ac4c2c8c7bb7e730a`；
  - `model/detector-manifest.json`：`8adcb1026803d688a7b85c55977d533eac6c15070ac4260bedb09d1da49d6061`；
  - `model/visible-default.conf`：`b9a128b78063056173626af1c7c7d2c580a59b295c45a4afc0ef8e2e04bff95b`。
- 板卡、WSL、Windows 用户目录和当前仓库均未找到四个必需 RKNN 文件，旧的反向隧道 `127.0.0.1:10022` 也未在线。当前明确缺少：
  - `template_backbone.rknn`，期望 SHA-256 `38b60334e8e0f0999bfb7ff9e943090546461f9307682653685237e24b2a7709`；
  - `search_backbone.rknn`，期望 SHA-256 `92125f1ef6254dade5786bb999faa5b9d9412ceb2b7987f6dbba82f959b7ef6a`；
  - `head.rknn`，期望 SHA-256 `2065bfca503cca9197e67de1a754efad930dfed4d2fc73881c77d9e850dcb600`；
  - `yolov8n-rk3566-fp16.rknn`，manifest 声明 SHA-256 `4d53a9b1e12996556294be8e099fe2dc0815d5c9cfdc327edfe350832baf8cf6`。
- 当前候选 binary 仍使用编译期绝对路径 `/app/model`，不会自动读取新目录下的 `model/`。在四个 RKNN 工件取得前不启动；取得后还需要在“重新编译为 `/app/nanotrack_b827_candidate/model`”和“将模型安装到既有 `/app/model`”之间确认部署方式。前者保持候选目录隔离且不影响现有模型，建议优先采用。

## 2026-10-30 生产部署配置解析器（Task 1，仅主机验证）

- 时间：`2026-10-30T18:39:38+08:00`。
- 分支/工作树：`codex/cross-compile-main-b827` / `/home/u20/single-object-tracking/.worktrees/cross-compile-main-b827`；输入基线 `28ac6307f698ae1abd0efac8f62de5e3cf5753b4`，开始时工作树干净。本节结果提交可用 `git log -1 --format=%H -- rk3566/nanotrack_v3/src/production_deployment_config.cpp` 查询。
- 输入需求：`.superpowers/sdd/2026-10-30-rk3566-production-deployment-config/task-1-brief.md`。新增 `rk3566/nanotrack_v3/include/production_deployment_config.hpp`、`src/production_deployment_config.cpp` 和 `tests/test_production_deployment_config.cpp`，修改同目录 `CMakeLists.txt`。
- 新接口要求恰好五个路径键，允许空行、`#` 注释和首尾空白，拒绝未知/重复/缺失/空值及非 `key=value` 行；加载时同时检查 tracker 三个 RKNN 文件和 controller/detector/manifest/labels 均为普通文件。相对路径按进程当前目录解释，无隐式默认路径。
- 先写测试并实际执行 RED：下述 configure/build/focused 命令退出 `2`，原因是 `production_deployment_config.hpp` 尚不存在。实现后重复同一命令，退出 `0`、focused `1/1`；完整 CTest `17/17`、零失败。测试程序含 12 个行为用例，必需缺失键、空值、重复键和各缺失工件逐项覆盖；使用隔离临时目录及占位文件，不使用真实模型。

```bash
cmake -S rk3566/nanotrack_v3 -B /tmp/nanotrack-deployment-config -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF && cmake --build /tmp/nanotrack-deployment-config -j4 && ctest --test-dir /tmp/nanotrack-deployment-config -R production_deployment_config --output-on-failure
ctest --test-dir /tmp/nanotrack-deployment-config --output-on-failure
git diff --check
```

- 主机测试输出 `/tmp/nanotrack-deployment-config/test_production_deployment_config` SHA-256：`6ea2a77c6021c29f8f2e75ce7853fb598f46fa3e63b704129732fff063227fce`；主机库 `/tmp/nanotrack-deployment-config/libnanotrack_production_deployment_config.a` SHA-256：`d85c18660bf862f22ae2d2b58eeb0b0e7ad45a353bc06f79518ca97139cf4a40`。生成物留在 Git 外。
- 边界：本任务只验证配置语法和文件存在/类型，不验证模型内容、哈希或 runtime 兼容性；未交叉编译、未连接板卡、未启动模型或吊舱。公共契约 `latest_handoff.available=false` 和现有候选编译裁决保持不变。
- Handoff：后续生产 adapter 应消费本提交的严格解析/验证接口；下一步按计划实现配置选择与 WSL adapter 集成。协调对话无需因本次主机测试调整板端批准状态。

## 2026-10-30 配置化 A6 候选构建暂存（Task 2，连接中断）

- 时间：`2026-10-30T18:57:18+08:00`；分支/工作树：`codex/cross-compile-main-b827` / `/home/u20/single-object-tracking/.worktrees/cross-compile-main-b827`；输入 HEAD 为 Task 1 commit `ebbfec37fba05283240a598cbc15b79a08be4aac`。
- 先对 WSL 当前 adapter 和 CMake 做只读快照，确认原 SHA-256 分别为 `08735f05ea5e631ca459fce7cc164428d69ac77caf2a349f3b66ae2d76c5391b` 和 `2dc1029fc7b92d75fc028bb8531552f05d76eeffa505d6bfb0a3adc6105d8dae`。修改前建立可恢复备份 `/home/wjm/viewpro_gimbal_zp/rk3566/gimbal_tracker/ai_alg.backup-20261030-before-deployment-config`，备份文件哈希与上述原文件一致。
- 本地静态接入检查对原快照得到预期 RED：`5/16` 通过、11 项失败，失败项正是旧五个硬编码路径、未调用严格 parser、五路径未接入及 CMake 未包含 parser source。补丁后得到 GREEN：`18/18` 通过；adapter 除 parser include、唯一固定 `kDeploymentConfig=/app/nanotrack_b827_candidate/config/production-deployment.conf`、五路径接入外保持原字节行为，CMake 只新增 `production_deployment_config.cpp`。
- 经 WSL 临时目录逐项 `sha256sum -c` 成功后，已经落盘并再次核对以下文件：
  - `ai_alg/source/native-lib_impl.cpp`：`687293fc0ebef2630d878a5e68f30a4a2d7663aeb7495081bc90bc934cfb4ca0`；
  - `ai_alg/CMakeLists.txt`：`0d763471acf50160e3fdc3c176c2e22a9141870bb473923f29adc213731c693b`；
  - `ai_alg/nanotrack/include/production_deployment_config.hpp`：`f6c3035decc01e0b0428d07d300dbb3f3b7edc00f9a4fdbac570a106726d51e6`；
  - `ai_alg/nanotrack/source/production_deployment_config.cpp`：`1c644b15d392c5b0e2711352d3dd044ce66e1b2a5a35aa28849d5d76e6e02cd9`；
  - `deployment_b827/config/production-deployment.conf`：`220b77f494b7c577f9dc2e0306f3fe1ce2ad1a0c684ed93b9bbd61f9b458e82c`。
- 随后向 WSL 临时目录传输三个 NanoTrack RKNN、VisDrone9 RKNN/manifest/labels 和 controller config 时，`100.104.145.88:2222` 连接中断。断线后 `tailscale ping -c 3 100.104.145.88` 无响应，SSH 和 TCP 2222 均超时；本机 `tailscale0` 路由仍存在。由于无法重新进入 WSL，临时工件目录的大小和 SHA、最终 `deployment_b827` 工件身份均**未确认**，不得把传输动作视为成功。
- 本节没有执行 `BUILD_JOBS=4 ./run.sh --platform A6`，没有产生新的 `viewlink_app`/`libAI.a` 身份，也没有连接开发板或启动候选程序。Task 2 当前状态为 `blocked_on_wsl_reachability_after_adapter_staging`；恢复连接后的第一步必须先核对上述五个已安装文件和临时工件目录，再补传/校验七个工件，然后才允许运行 A6 构建。
- RKNN 风险保持不变：交叉环境历史 runtime 为 `1.5.2`，临时 VisDrone9 工件及板端历史环境为 `2.3.2`；即使后续编译成功，也不能据此声明模型加载、推理、开发板或吊舱验收。

## 2026-10-31 配置化 A6 候选构建完成（Task 2）

- 时间：`2026-10-31T15:46:59+08:00`；分支/工作树：`codex/cross-compile-main-b827` / `/home/u20/single-object-tracking/.worktrees/cross-compile-main-b827`；输入 HEAD 为 `e976d8083f5d53f5bb7e4ac5fa7c2cea0f24917c`，包含 Task 1 parser commit `ebbfec37fba05283240a598cbc15b79a08be4aac` 和中断暂存记录。
- WSL 恢复后首先重新核对前节五个已安装文件，SHA-256 未变化。断线留下的 RKNN 半文件没有被采用；改用 `rsync --partial --append-verify` 续传，然后在临时目录和最终 `deployment_b827` 目录分别核对工件身份。

| 部署工件 | WSL 候选路径 | SHA-256 |
| --- | --- | --- |
| NanoTrack template | `deployment_b827/model/template_backbone.rknn` | `38b60334e8e0f0999bfb7ff9e943090546461f9307682653685237e24b2a7709` |
| NanoTrack search | `deployment_b827/model/search_backbone.rknn` | `92125f1ef6254dade5786bb999faa5b9d9412ceb2b7987f6dbba82f959b7ef6a` |
| NanoTrack head | `deployment_b827/model/head.rknn` | `2065bfca503cca9197e67de1a754efad930dfed4d2fc73881c77d9e850dcb600` |
| temporary VisDrone9 RKNN | `deployment_b827/model/yolov8n-visdrone9-rk3566-fp16.rknn` | `3698bc9bea1f43a4bac2826a09e03be0439fddfbc5c5604d8ed0f92b36881df0` |
| VisDrone9 manifest | `deployment_b827/config/detector-manifest.json` | `05e6d8c7c210b7f9d85b8fb712aa00b014f310e46e85f171ce1158afc90ba63e` |
| VisDrone9 labels | `deployment_b827/config/visdrone9.txt` | `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992` |
| controller config | `deployment_b827/config/visible-default.conf` | `b9a128b78063056173626af1c7c7d2c580a59b295c45a4afc0ef8e2e04bff95b` |
| deployment config | `deployment_b827/config/production-deployment.conf` | `220b77f494b7c577f9dc2e0306f3fe1ce2ad1a0c684ed93b9bbd61f9b458e82c` |

- `production-deployment.conf` 只指向 `/app/nanotrack_b827_candidate` 下的上述 tracker/controller/detector/manifest/labels 路径。adapter 只固定该配置文件自身的位置；未知/重复/缺失/空键、缺文件或模型初始化异常均沿现有 `catch` 使 `tracker_detector_init()` 返回 `false`，没有 fallback。
- 实际执行：

```bash
cd /home/wjm/viewpro_gimbal_zp/rk3566/gimbal_tracker
BUILD_JOBS=4 ./run.sh --platform A6
```

- 观察结果：退出码 `0`；CMake 重新配置；GNU AArch64 C++ 工具链完成 `production_deployment_config.cpp.o`、`target_class.cpp.o` 及其余生产对象编译，`libAI.a` 和 `viewlink_app` 链接成功。完整日志位于 WSL `deployment_b827/build-A6-configurable.log`，SHA-256 `e9d0e480dd82ced5d0f4121401ee7150a3c7fb362e878300358a7cebdd8262e5`。

| 产物 | 大小/时间（Asia/Shanghai） | SHA-256 |
| --- | --- | --- |
| `build_A6/viewlink_app` | 32404424 bytes；`2026-10-31 15:46:31 +0800` | `80554ffe9bde5c0ea0a729c56902d657d548214018eee84a445f3203c22f4692` |
| `build_A6/ai_alg/libAI.a` | 551452 bytes；`2026-10-31 15:46:30 +0800` | `4eb1fd42f5063b327cb72545a2747319305061852b143b184930a4aebf0af76f` |

- `file` 将新程序识别为 ARM aarch64 ELF。`libAI.a` 含 `production_deployment_config.cpp.o`、`target_class.cpp.o` 和真实 `tracker_detector_init`、`do_tracker_detector`、`sr_model_init`、`sr_infer_frame` C 符号；归档成员没有 `main/debug/highgui/telemetry/tracking_log/video/csv` 对象，字符串扫描确认唯一部署配置绑定为 `/app/nanotrack_b827_candidate/config/production-deployment.conf`。
- 新二进制已复制到 WSL `deployment_b827/bin/viewlink_app` 并复核为同一 SHA。被替换的旧候选二进制保存在 `deployment_b827.backup-20261031-old-binary/viewlink_app`，SHA-256 `768e65e635c3c78ff9d61f8963e8131ec5cb897c301fefcaffc31d17003da988`。没有修改 WSL 工程外的现有板端程序或 `/app/model`。
- 证据边界：Task 2 只证明锁定工件已取得并且配置化生产代码在现有 A6 环境成功交叉编译。VisDrone9 仅是临时功能候选，不是 COCO80 或最终 detector 验收；本节未执行板端模型加载、推理、跟踪/长期恢复、内存平台或真实吊舱测试。交叉链接 runtime `1.5.2` 与模型/历史板端环境 `2.3.2` 的兼容风险仍需板端启动暴露，编译成功不消除该风险。
- 下一步：按 Task 3 对板端执行只读 preflight，在不覆盖 `/app/bin/viewlink_app`、`/app/viewlink_appnt3`、`/app/model` 的前提下，以临时目录复制精确白名单，并做有界前台启动；这仍不是开发板验收或吊舱验收。

## 2026-10-31 板端隔离暂存 preflight（Task 3，因现有进程停止）

- 时间：`2026-10-31T15:57:25+08:00`；输入 HEAD `e3dae4ad090937c3a40f419046dce9c1678f879d`。通过 WSL 连接测试板 `root@192.168.2.119`，只执行只读 preflight；没有复制、改名、删除、停止或启动任何板端程序。
- 板端身份仍为 `rk3566_rk3568`、AArch64、Buildroot 2021.11、Linux 5.10.198；NPU driver 报告 `v0.9.3`。板端系统时钟错误地显示为 `2017-08-04`，因此后续证据时间必须同时使用根代理时间和板端原始时间，不能依赖板端 mtime 排序。
- `/app` 所在根文件系统可用约 26,980,744 KiB，`/tmp` 可用约 1,902,712 KiB，空间和 inode 足够。`sha256sum/wc/od/awk/tr/find/sort/cmp/readlink/tar/mv/mkdir/chmod/sleep/kill/timeout` 可见；`file/readelf/ldd` 未在 `command -v` 输出中出现，不能把缺少这些工具误写成依赖正常。
- 受保护程序仍未变化：`/app/bin/viewlink_app` SHA-256 `a60b61a34cdaf0b43d27726a5ce311e9e995a8d34bb874657c944eabd86fb357`；`/app/viewlink_appnt3` SHA-256 `55d5d709ac08ac7dd119e7c6fe01b96263afd1eeb0f7d36318d2152f330c3598`。现有旧候选目录仍只有早期 binary 和 COCO/config 文件，本轮未修改。
- 安全阻塞：`ps` 和 `pidof viewlink_app` 均确认 `/app/bin/viewlink_app` 正在运行，PID `784`，同时存在 `app_watchdog.sh` PID `785`。按“不得启动第二实例、不得终止既有程序、必须确认 watchdog 不会自动拉起”的约束，Task 3 在复制前停止；没有为了继续 smoke 而 kill 生产进程或修改 watchdog。
- 下一步需要人工先停止现有 `viewlink_app` 及其自动拉起机制，或者明确授权根代理对这两个确切进程/机制执行受控停用。恢复后必须从完整只读 preflight 重新开始，不能沿用 PID 或进程空闲结论；随后才可复制九文件白名单、保留旧候选并执行有界前台 smoke。
- 证据边界：当前只证明测试板可达、身份/空间/保护文件已核对，并识别出并发冲突。没有形成新候选暂存、模型加载、启动、跟踪、重检测、开发板验收或吊舱验收证据。

## 2026-10-31 板端隔离候选发布与跟踪指令 smoke（Task 3）

- 时间：`2026-10-31T16:24:12+08:00`；分支/工作树：`codex/cross-compile-main-b827` / `/home/u20/single-object-tracking/.worktrees/cross-compile-main-b827`；输入 HEAD `7a865280b261cacd253e0665c43e326fadd9656c`。用户人工停止原 `viewlink_app` 和 `app_watchdog.sh` 后重新执行完整 preflight，确认二者均不在运行；未由本任务终止或修改生产进程、自启动或 watchdog。
- 板端证据目录为 `/tmp/codex_nanotrack_20261031T161155`。板端系统时钟仍错误地显示为 `2017-08-04`；本节时间采用根代理的 Asia/Shanghai 时间，板端文件 mtime 仅作为原始信息保留。
- 从 WSL `deployment_b827` 只复制 Task 2 锁定的九文件白名单到 `/app/nanotrack_b827_candidate.staging-20261031T161155-codex`。白名单 SHA-256 为 `77fd1d3a56ebb67b575aa352afea0e4a9dfa267903db4645fa501b7c952f27e1`，只读核验脚本 SHA-256 为 `aa1ff2bb7aba956a766ebf3694f05b6248037e93c5ff3f68f1802f67f9fbe95c`。staging 的九项 size/SHA、AArch64 ELF header、五个配置路径和“无额外文件/目录”检查全部通过。
- 旧候选目录未删除，保留为 `/app/nanotrack_b827_candidate.backup-20261031T161155-codex`；已验证 staging 以同文件系统 rename 发布为 `/app/nanotrack_b827_candidate`。发布后再次执行 `sh /tmp/codex_nanotrack_20261031T161155/verify-candidate.sh /app/nanotrack_b827_candidate`，九文件哈希全部匹配 Task 2，程序仍为 `80554ffe9bde5c0ea0a729c56902d657d548214018eee84a445f3203c22f4692`。
- 动态库只对本次进程设置 `LD_LIBRARY_PATH=/app/lib`，没有修改系统环境、启动脚本或 `/app/lib`。实际 loader 检查显示应用依赖可解析；`librknnrt.so` 实际来自 `/lib/librknnrt.so`，SHA-256 `62f849a06a522a190b96d14da4339910bdd228048ad9b3adfeb12f959cba8b8d`，RKNN API `1.6.0 (9a7b5d24c@2023-12-13T17:31:11)`，NPU driver `0.9.3`。这消除了“本板 runtime 不能加载本候选四个模型”的启动风险，但不改变模型质量或场景验收边界。

实际有界启动形式为：

```sh
cd /app
timeout -s TERM -k 2 <seconds> env LD_LIBRARY_PATH=/app/lib \
  /app/nanotrack_b827_candidate/bin/viewlink_app
```

- 10 秒启动 smoke 中，三个 NanoTrack RKNN 和 VisDrone9 RKNN/manifest/labels 均报告上述 API/driver 身份并完成初始化；stdout/stderr SHA-256 分别为 `b8e54b36c1ea9442411d3d7aee7d3eccfa50d33689c2f3995a35807b1ab4d539`、`9bf009dee6ac9012c4a7fe37a701184f8a00a79be5d67c9ffa2afb933420d9cf`。进程不响应 TERM，随后由本次 `timeout -k 2` 的 KILL 边界结束，观察到状态 `137`，退出后无残留。
- 首个 90 秒窗口没有在日志中观察到跟踪初始化事件，不能证明当时指令已到达。用户随后明确要求再次启动并在第二个 120 秒窗口内确认“已下发”。该窗口 stdout `/tmp/codex_nanotrack_20261031T161155/stdout.trackcmd2.log` 为 725091 bytes、SHA-256 `d19ebd92e8780c60e0777ea68429838ddab88f368faadcedf80ab6778a6ae0ab`；stderr 为 5638 bytes、SHA-256 `702176188399a36d939495a8b6961ba8470d23afd746cbacbcc60cd04cceb60a`。
- 第二个窗口再次成功初始化四个模型，并实际记录 `TRACK_INIT: ... init_box=(940,519,64,64)`；紧随其后的首帧记录为 `DO_TRACK first frame: rect[0]=(942,506,58,64), ... use init_box=(940,519,64,64)`。日志没有 `track init failed`，说明本候选在正确进程库路径下收到了该次上位机指令，完成任务初始化，并由 `do_tracker_detector()` 返回了有效首帧框。
- 后续两次 `DO_TRACK reinit` 分别使用 `(1137,479,64,64)` 和 `(729,558,64,64)`；二者之前均出现 `do_track: new click during inference, drop result`。因此只把它们记录为后续新点击/框选触发的重初始化，不把它们宣称为 YOLO 自动重检测或长期恢复证据。
- 120 秒窗口同样在 TERM 后由 KILL 边界结束，控制终端明确记录 `Killed`，退出后无 `viewlink_app` 或 `app_watchdog` 残留。本窗口的 `exit.trackcmd2.txt` 内容为 `0`，经复核是多层 SSH 引号导致的错误状态捕获，不能作为程序退出码；本节仅采用控制终端的 KILL 观察和退出后进程检查，不把该文件提升为有效退出证据。
- stderr 中仍有缺失 `/mnt/sdcard/user_custom.xml`、thermal sysfs、`XDG_RUNTIME_DIR` 和串口设备的板级应用告警；它们没有阻止本次模型初始化和跟踪首帧返回，但本任务未修复或验证这些非算法外设路径。stdout 中大量 `rk-debug_err layinfo->numLayer=..., mInitSucee=0` 也被原样保留，未据此声明显示/叠加链正常。
- 退出后重新生成受保护文件和 `/app/model` 的 tree/SHA/symlink 快照；四份 `cmp` 均返回 `0`。受保护程序仍为 `/app/bin/viewlink_app` SHA-256 `a60b61a34cdaf0b43d27726a5ce311e9e995a8d34bb874657c944eabd86fb357` 和 `/app/viewlink_appnt3` SHA-256 `55d5d709ac08ac7dd119e7c6fe01b96263afd1eeb0f7d36318d2152f330c3598`；`/app/model` 未改变，也没有新增 symlink。
- 证据边界：本节是隔离候选的模型加载和一条人工下发跟踪指令 smoke，证明当前二进制/配置/四模型能够在这块板上启动并返回首帧框；它不是开发板验证任务的完整复测，不证明检测器自动重检测、长期恢复效果、内存平台、场景指标或真实吊舱验收。公共契约仍为 `latest_handoff.available=false`，VisDrone9 仍只是临时功能候选；真实吊舱部署和测试仍须人工完成。
- Handoff：请协调对话采纳“配置化 A6 交叉编译成功 + 隔离板端模型加载/单次跟踪初始化 smoke 成功”的精确候选证据，但不要据此把公共契约改成批准的开发板 handoff，也不要宣称吊舱验收。若协调者锁定的最终 source/model/config 身份变化，必须重新交叉编译。

## 完成条件

- `development_board_validation.latest_handoff.available=true`；
- 本地取得的 commit 和工件哈希与 handoff 完全一致；
- 对该结果执行交叉编译并观察到成功退出；
- 将命令、结果和生产产物路径写入本文件。

## 2026-10-31 零伺服偏移诊断版交叉编译与隔离上传

- 时间：`2026-10-31T23:50:00+08:00`；分支/工作树：`codex/cross-compile-main-b827` / `/home/u20/single-object-tracking/.worktrees/cross-compile-main-b827`；开始时 HEAD 为 `98a196fa7d4dca5480a081fe39775bda01101d7c`。
- 用户确认的诊断边界是：上位机继续读取真实 `rect_show`；宿主伺服控制使用的 `track_box_result.rect[0]` 在初始化帧及所有后续跟踪帧始终以 1920x1080 图像中心为中心，使其整数中心偏移严格为 `(0,0)`；`rect[1..4]`、NanoTrack 推理、尺寸保护、长期状态机、质量判定、检测器和模型配置不变。
- 保留上一轮已由人工观察确认“框高不再缩到 1”的尺寸保护：输出宽高限制为初始框对应维度的 `0.75..1.33`，且裁剪到图像边缘。该保护的主机单测与本轮完整回归一并执行。
- 新增适配层诊断日志：每帧记录 `mode/quality/accepted/control_valid`、真实/控制/最终/预测框、三类中心偏移、选中观测的 raw score、penalized peak、PSR、APCE、peak margin、中心残差、面积和宽高比变化；另逐条记录 anchor/recent observation 的框及五项响应指标。

本轮先完成控制框纯函数红绿验证：缺少 `centered_control_box` 时测试按预期编译失败；实现后，奇数尺寸、偶数尺寸和 1x1 边缘小框均保持原宽高、输入框不被修改，并得到整数中心 `(960,540)`。适配层合同检查确认只在初始化和正常输出各调用一次居中函数，`rect_show` 与 `rect[1]` 均使用真实框且没有被控制框替换。

主机侧重新建立干净的 RKNN-independent 构建目录并执行：

```bash
cmake -S rk3566/nanotrack_v3 -B /tmp/nanotrack-zero-control-host \
  -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF
cmake --build /tmp/nanotrack-zero-control-host -j4
ctest --test-dir /tmp/nanotrack-zero-control-host --output-on-failure
```

观察结果：`17/17` 测试通过，零失败；`git diff --check` 通过。原计划引用的 `/tmp/nanotrack-scale-guard` 已被环境清理，因此使用上述全新目录重新配置，而不是复用旧测试结果。

同步到 WSL 的适配层源码和帮助头文件 SHA-256 分别为：

- `/home/wjm/viewpro_gimbal_zp/rk3566/gimbal_tracker/ai_alg/source/native-lib_impl.cpp`：`2b56081633ad6a1084a875bb7444a70745f667cf20aea23a37bb7a8610a553fe`；
- `/home/wjm/viewpro_gimbal_zp/rk3566/gimbal_tracker/ai_alg/include/control_output_safety.hpp`：`bee1882c3523e3e30ceb6b292b9ec3b2705d76db1dd0f2d15c639fb2f4e8cd30`。

实际交叉编译命令：

```bash
cd /home/wjm/viewpro_gimbal_zp/rk3566/gimbal_tracker
BUILD_JOBS=4 ./run.sh --platform A6
```

观察结果：退出码 `0`，重新编译适配层及 NanoTrack 相关对象，成功链接 `libAI.a` 和 `viewlink_app`。新工件身份为：

| 工件 | 大小 | SHA-256 |
| --- | ---: | --- |
| `build_A6/viewlink_app` | 32,408,600 bytes | `e68509753b886e184a6b0be875129939ecfba2377d98452822311ff938415062` |
| `build_A6/ai_alg/libAI.a` | 554,748 bytes | `cb6440e45f1bb0b3528810a4155bb658badbdee942bbabd59a4ab30bd6083f0c` |

`file` 将程序识别为 ARM aarch64 ELF；字符串复核可见 `AI_TRACK_DIAG state=INIT/DO_TRACK`、`selected_pscore` 和 `AI_TRACK_OBS`。新程序与上一轮诊断程序 `3298c835d468e1876d9638ee88f9829dd753d97f76042b9cd7c602ff763bfb43` 不同。

板端只执行隔离上传，没有启动程序。新路径为：

```text
/app/nanotrack_b827_zero_control_diag_20261031/bin/viewlink_app
```

传输先落到同目录 `viewlink_app.staging`，板端 SHA-256 匹配后改为权限 `755` 并原子重命名。最终板端大小仍为 32,408,600 bytes、SHA-256 仍为 `e68509753b886e184a6b0be875129939ecfba2377d98452822311ff938415062`，暂存文件不存在，进程检查输出 `ZERO_CONTROL_DIAG_NOT_RUNNING`。

上传前后受保护程序保持不变：

- `/app/bin/viewlink_app`：`a60b61a34cdaf0b43d27726a5ce311e9e995a8d34bb874657c944eabd86fb357`；
- `/app/nanotrack_b827_candidate/bin/viewlink_app`：`80554ffe9bde5c0ea0a729c56902d657d548214018eee84a445f3203c22f4692`。

- 证据边界：本节证明控制/显示通道分离通过纯函数和适配层合同检查、主机 17/17 回归通过、A6 交叉编译成功且新工件已隔离上传。由于新程序尚未启动，本节不证明上位机实际显示、伺服静止、质量门判断、模型运行或真实吊舱验收；这些结论必须以后续人工下发指令的实际观察为准。
- 下一步：按用户指令启动该隔离程序时，进程环境需包含 `LD_LIBRARY_PATH=/app/lib:/usr/lib`，保留 stdout/stderr 日志，并由用户确认“真实框可见且伺服不动”；不得据此升级公共开发板 handoff 或吊舱验收状态。

## 2026-11-01 自适应遮挡滑行板端改动同步、A6 构建与诊断启动

- 时间：`2026-11-01T00:15:30+08:00`；分支/工作树：`codex/cross-compile-main-b827` / `/home/u20/single-object-tracking/.worktrees/cross-compile-main-b827`。开发板验证分支最新提交为 `codex/adaptive-occlusion-coast@617bef92986741c4702e01a2727cd5e3e5ed9367`；其中三次末端板上修正对应共享提交 `3890c50bc6aff067027d63f0140d798f783d472e`、`e3ec19615a0d931544fa9567f288be62a0935551`、`d589215f1c0319f2686d6364edfec86a67a0c63e`，开发板独立工作树均为 clean。该上游分支合入本诊断分支后的 merge commit 为 `433deff6d93611d38dcc884f3ef18ad55d12a1f6`。
- 合并仅在 `rk3566/nanotrack_v3/CMakeLists.txt` 出现一处源列表相邻冲突：保留本分支 `production_deployment_config` 测试，同时保留上游 `occlusion_evidence` 测试和源文件。合并前主机基线 CTest `17/17` 通过；合并后新建 `/tmp/codex_postmerge_host_build` 并重新配置、构建、执行 CTest，结果为 `18/18` 通过，零失败。
- WSL 工程 `/home/wjm/viewpro_gimbal_zp/rk3566/gimbal_tracker` 不是 Git checkout。同步前备份位于 `ai_alg/backup-before-adaptive-coast-433deff`，其中原 `CMakeLists.txt` SHA-256 为 `0d763471acf50160e3fdc3c176c2e22a9141870bb473923f29adc213731c693b`。只将合并提交的 `include/`、`src/`、`config/` 分别同步到 WSL `ai_alg/nanotrack/include/`、`source/`、`config/`，并在 WSL AI 源列表中增加 `occlusion_evidence.cpp`；保留既有零控制诊断 `native-lib_impl.cpp`。同步后的三组目录执行 checksum dry-run 无文件差异，WSL `ai_alg/CMakeLists.txt` SHA-256 为 `55aabecdd167459b4ebd03822e5af0d7e2f07e23083b89d4b2025bfc7e7f3279`。
- 实际交叉编译命令仍为 `BUILD_JOBS=4 ./run.sh --platform A6`，退出码 `0`；日志观察到新增 `occlusion_evidence.cpp.o` 编译，随后 `libAI.a` 和 `viewlink_app` 链接成功。新程序 `/home/wjm/viewpro_gimbal_zp/rk3566/gimbal_tracker/build_A6/viewlink_app` 为 ARM aarch64 ELF，SHA-256 为 `313a5e767f84cf73a24133859564300a05a0d60abc7d52be4cc0ad613a37bb05`。
- 新诊断目录为 `/app/nanotrack_433deff_adaptive_zero_control_20261101`；板上程序 SHA-256 与 WSL 完全一致。新 `visible-default.conf` SHA-256 为 `2010c6cd16553595844a12f103f53412942289262d866b9fadba26b2821337ed`，包含自适应遮挡证据/确认参数；旧共享配置已保留为 `/app/nanotrack_b827_candidate/config/visible-default.conf.pre-433deff`，SHA-256 `b9a128b78063056173626af1c7c7d2c580a59b295c45a4afc0ef8e2e04bff95b`。
- 首次切换时旧 PID `30016` 短暂处于不可中断 `D` 状态并继续占用 `/dev/video0`，导致新实例持续出现 `VIDIOC_STREAMON`/格式设置失败。停止新实例后旧实例恢复为可终止状态；结束已核验的旧进程并确认视频设备释放，再以 `LD_LIBRARY_PATH=/app/lib:/usr/lib` 启动新版本。最终仅有新 PID `16804`，启动后六秒内摄像头相关错误计数为 `0`；日志位于新目录 `log/stdout.log` 和 `log/stderr.log`。
- 证据边界：本节证明已取得并合并最新自适应遮挡滑行源码、主机回归通过、A6 交叉编译成功、工件/配置哈希匹配并在测试板上无摄像头冲突地启动。尚未收到本次人工跟踪反馈，不能宣称 `quality/accepted`、框稳定、伺服静止、长期恢复或真实吊舱验收通过；公共契约仍是 `latest_handoff.available=false`，本结果是用户授权的诊断候选，不是协调者批准的生产 handoff。
- 下一步：由用户通过上位机执行多次约十秒的稳定目标跟踪后取消；保留日志并分析 `quality`、`accepted`、框尺寸和状态转换。任何人工结果只按实际观察记录，不提升为未达到的发布门。

### 现场绿屏恢复与六次稳定目标测试日志

- 首次无摄像头冲突启动后，用户明确反馈上位机为绿屏。对比上一版正常日志确认：采集、VPSS 和七路 MPP encoder 均已初始化，但新实例两次 `eglInitialize()` 返回 `EGL_NOT_INITIALIZED (0x3001)`，持续输出 `mInitSucee=0`；上一版同阶段则成功识别 `OpenGL ES 3.2`、ARM、`Mali-G52`。绿屏日志保留为新目录 `log/stdout.green-no-xdg.log` 和 `log/stderr.green-no-xdg.log`。
- 单变量重启仅增加 `XDG_RUNTIME_DIR=/var/run`，同时保留 `LD_LIBRARY_PATH=/app/lib:/usr/lib`。新 PID `19190` 的两套图形实例均成功识别 `Mali-G52`，`eglInitialize failed`、`mInitSucee=0` 和摄像头启动错误计数均为 `0`；用户随后明确反馈画面已恢复。该结果只证明启动环境修正，不是算法或吊舱验收。
- 画面恢复后的日志基线为 stdout `400` 行、stderr `63` 行，二者均无 `TRACK_INIT` 或 `AI_TRACK_DIAG`。用户完成测试后，从该基线之后观察到六次初始化，初始框依次为 `(837,472,64,64)`、`(889,315,64,64)`、`(1576,731,64,64)`、`(522,510,64,64)`、`(1507,464,64,64)`、`(305,183,64,64)`。
- 六次 `DO_TRACK` 帧数依次为 `119/106/86/99/57/109`。前五次 `RELIABLE=0`、`accepted=0`；第六次只有第 `474` 帧为 `RELIABLE + accepted=true + control_valid=true`，下一帧即因 PSR `5.528008 < reliable_psr 6.0` 降为 `WEAK + accepted=false`，再下一帧 PSR `4.558636` 降为 `REJECTED`。六次的 `RELIABLE/WEAK/REJECTED` 计数分别为 `0/3/116`、`0/3/103`、`0/0/86`、`0/0/99`、`0/8/49`、`1/1/107`。
- 前五次首帧拒绝分别由峰值间隔低于 weak 门（`0.019654`、`0.024922`）、PSR 低于 weak 门（`4.532691`、`4.893508`）以及峰值间隔低于 weak 门（`0.019521`）直接解释；对应 raw score 仍为 `0.999413/0.889625/0.957912/0.896523/0.999833`。这证明 `accepted` 与当前“仅可靠观测可接受”的代码规则一致，但当前 PSR/peak-margin 门或其度量对这些稳定目标不适用，不能把现象归因于 raw score 低。
- 六次状态机均在没有稳定可靠历史的情况下按 `UNCERTAIN -> LOCAL_RECOVERY -> ASSISTED_RECOVERY -> LOST` 推进，`OCCLUSION_HOLD=0`。因此本次同步的自适应遮挡滑行没有机会工作；它不能修复发生在其前面的质量门拒绝。所有逐帧 `control_err=(0,0)`，但这不推翻宿主在初始化/首帧覆盖算法控制框的既有反汇编证据，也不能单凭日志声明伺服静止。
- 前五次最终框保持 `64x64`；第六次最终框范围为宽 `68..80`、高 `40..58`。这再次表明 raw NanoTrack 尺寸保护不覆盖长期状态机的 motion 预测框，`accepted=false` 后预测高度仍会低于初始框的 `0.75` 下限。本轮未修改阈值、PSR计算、accepted策略、motion estimator 或宿主控制代码。
- 证据边界：本节是用户人工触发、日志可复核的六次功能诊断；没有标注真值，不能形成精度、稳定率、重捕率或吊舱验收结论。下一步应由算法/板端验证任务先选择单变量方向：验证 PSR/peak-margin 计算，或用有标注稳定序列标定门限；交叉编译任务不应直接猜测阈值并继续叠加修复。

达到以上条件只能表述为“最新板端结果已成功交叉编译”，不能表述为“已部署”或“吊舱测试通过”。

## Next action

1. 将本次候选编译记录和任务 commit 交给协调对话，但保持 `latest_handoff.available=false`，直到开发板验证任务发布达到门槛的精确 handoff。
2. handoff 发布后核对 source commit、模型/manifest/labels、配置、runtime 身份、证据路径和裁决。
3. 若批准身份与本次候选输入完全一致，协调对话可决定如何采纳候选编译证据；任何一项不同都必须重新执行 A6 交叉编译，不能沿用本节哈希。
4. 真实吊舱部署和测试仍由人完成；没有明确人工反馈不得声明吊舱验收通过。

## Handoff

- 请协调对话采纳本次候选编译记录：`main@b827c0a6d8916e9c0832c748e30722f6f2fe5563` 已在现有 WSL 环境中生成 AArch64 `viewlink_app`，其路径和 SHA-256 见上文。
- 公共契约当前没有“用户授权的未批准 handoff 候选编译”字段。协调对话如更新公共状态，应明确区分候选编译成功与批准 handoff 编译成功；在 handoff 未发布前保持 `latest_result_access_confirmed=false`，不得仅据本节把 `development_board_validation.latest_handoff.available` 改为 `true`。
- 需要协调对话决定是否增加/记录 candidate compile 状态，并准确保留三方身份：交叉环境链接库可见 runtime `1.5.2`、本次实际测试板部署 runtime API `1.6.0` / driver `0.9.3`、临时 VisDrone9 模型 toolkit `2.3.2`。本候选四模型已在该测试板加载，但这不等于最终 runtime 兼容矩阵、模型质量或吊舱验收；真实吊舱部署和测试仍由人完成。

## 后续更新模板

```text
时间：<ISO 8601 + timezone>
分支/工作树：<branch> / <absolute path>
最新板端 handoff：<source commit, model/manifest/labels SHA-256>
取得结果：<path or checkout, matched yes/no>
交叉编译命令：<exact command>
编译结果：<exit code and concise observed result>
生产产物：<path, optional SHA-256>
下一步：<hand to human for pod deployment/test>
```

## 2026-11-01 NanoTrack 初始化输入帧快照诊断（本地实现通过，等待 WSL 恢复）

- 时间：`2026-11-01T13:09:28+08:00`。
- 分支/工作树：`codex/cross-compile-main-b827` / `/home/u20/single-object-tracking/.worktrees/cross-compile-main-b827`。
- 本轮起始 HEAD：`2b200a4b38782af6592501d8953e9a0917d541b9`。
- 用户确认的设计：每次 `STATE_INIT_TRACK` 在 RGB 转 BGR 后、调用 NanoTrack 初始化前，仅保存一张未标注完整 PNG；保存失败只记录日志，不改变跟踪、框或伺服输出语义。设计和实施计划分别为
  `docs/superpowers/specs/2026-11-01-init-frame-snapshot-design.md`（SHA-256
  `eee60536bde29b6c4cb9245b3dac88f130e38471f2348fbed6f71e6f7717183e`）和
  `docs/superpowers/plans/2026-11-01-init-frame-snapshot.md`（SHA-256
  `0e85a12b18b01cde32aa7587c578a7194b2af290845290c6eaf4ec0df3f1a7c1`）。
- 按 TDD 先观察到 RED：快照测试因 `init_frame_snapshot.hpp` 不存在编译失败，适配器顺序测试因找不到 `save_init_frame_snapshot` 失败。最小实现后，2x2 BGR PNG 真实写入/读回像素一致；不存在父目录时返回 `false`、提供错误文本且不抛异常；适配器调用位于 `make_bgr_view()` 后、`LongTermRuntime::initialize()` 前且仅出现一次。
- 本地诊断暂存文件 SHA-256：`init_frame_snapshot.hpp`
  `6fb2bcb7944ac43d9619511dba25c7dff5b5bf25318bcefb7704b2642561af0f`，
  `init_frame_snapshot.cpp`
  `192e4bc053dd574f11c25cc0d16b3e89c868dec8ea82c2ade48a98b92da2ca59`，
  接入后的 `native-lib_impl.cpp`
  `f4ae4df28d4f0bced7fff953bad17609fad2826a668214945dbe27a0b5fcf0f4`。
  这些适配器暂存文件位于本计划隔离的 git-ignored 执行目录；在 WSL 恢复后必须按上述哈希同步并复核，不能仅凭文件名消费。
- RKNN-independent 基线和实现后回归均为 CTest `18/18` 通过；快照聚焦测试和适配器调用顺序测试也通过。
- 外部阻塞：`ssh -o BatchMode=yes -o ConnectTimeout=8 wjm@100.104.145.88 true` 在 TCP 连接阶段超时，尚未进入 SSH 认证。故本轮没有写入 WSL、没有执行 `BUILD_JOBS=4 ./run.sh --platform A6`、没有生成或部署新的 A6 二进制，也没有在吊舱上生成 PNG。现有板端程序和目录未被本轮修改。
- 下一步：Windows/WSL 的 Tailscale SSH 恢复后，将上述三份经测试文件及 CMake 源文件项同步到 `/home/wjm/viewpro_gimbal_zp/rk3566/gimbal_tracker/ai_alg/` 并逐项核对 SHA-256；随后完成 A6 编译、隔离部署，在用户下发一次初始化后读取 `/app/nanotrack_433deff_init_frame_diag_20261101/log/nanotrack_init_input.png`，核对尺寸、颜色、内容和初始化框日志。

### 证据边界

当前只证明诊断保存逻辑在主机 OpenCV 环境中按预期工作，并且现有 RKNN-independent 回归未被破坏；不能声明 WSL 源码已更新、A6 交叉编译成功、板端图片已生成、输入图像正确、跟踪质量改善或真实吊舱验收通过。

## 2026-11-01 NanoTrack 初始化输入帧快照诊断完成

- 时间：`2026-11-01T14:04:41+08:00`；分支/工作树：`codex/cross-compile-main-b827` / `/home/u20/single-object-tracking/.worktrees/cross-compile-main-b827`；实施前基线为 `2b200a4b38782af6592501d8953e9a0917d541b9`，设计/计划提交为 `05f21b2bd29a9d62842a89e6027ce930ad1ed638`。
- WSL 实际通过 `100.104.145.88:2222` 恢复连接。写入前备份位于 `/home/wjm/viewpro_gimbal_zp/rk3566/gimbal_tracker/ai_alg/backup-before-init-frame-snapshot-05f21b2/`；只在现有适配层的 RGB→BGR 后、`LongTermRuntime::initialize()` 前增加无控制副作用的 PNG 保存，并在 `ai_alg/CMakeLists.txt` 增加对应源文件和 `opencv_imgcodecs`。
- 第一次 A6 编译暴露 `cv::MatStep` 直接传给 `%zu` 的告警；停止部署后先增加 `frame_step_bytes()` 聚焦 RED 测试，再以 `frame.step[0]` 返回 `std::size_t` 修正。最终主机快照测试、适配器顺序测试和 RKNN-independent CTest `18/18` 均通过。
- 最终同步文件 SHA-256：`include/init_frame_snapshot.hpp` 为 `76266b222847868b9c44907dc6aea2da041c1f976abb15718601c4976d7b8e55`；`source/init_frame_snapshot.cpp` 为 `39afbed46eae4438fd1daf688541aa197df97303ac222e1abef923a76e579095`；`source/native-lib_impl.cpp` 为 `39fec2569be78682621c4d67f8c23e17593b6c00c88225ae26c7182a1b11191a`；`ai_alg/CMakeLists.txt` 为 `bef7585ef5ab3cf905861f1ddeb2eb51f5ef109fd054cb5be5250a3e16889e3c`。
- 实际执行 `cd /home/wjm/viewpro_gimbal_zp/rk3566/gimbal_tracker && BUILD_JOBS=4 ./run.sh --platform A6`，最终退出码 `0` 且无编译告警。`build_A6/viewlink_app` 为 ARM aarch64 ELF、34,888,216 bytes、SHA-256 `9daea988453a3ae03d83dff8467da293be0139e357c73200f3c90216a0531bff`；`build_A6/ai_alg/libAI.a` 为 588,638 bytes、SHA-256 `0d812eb62c867ad54c7922d17d54db36a9cb0897b8d109d85db49c80842f1496`。
- 诊断程序隔离部署到 `/app/nanotrack_433deff_init_frame_diag_20261101`，板端程序哈希与 WSL 一致；受保护的 `/app/bin/viewlink_app` 仍为 `a60b61a34cdaf0b43d27726a5ce311e9e995a8d34bb874657c944eabd86fb357`。以 `XDG_RUNTIME_DIR=/var/run LD_LIBRARY_PATH=/app/lib:/usr/lib` 启动，进程 PID `6047` 持续运行，启动日志没有 `eglInitialize failed`、`mInitSucee=0` 或 `track init failed`。
- 用户下发后实际发生两次 `STATE_INIT_TRACK`：第一次在诊断 frame 1，第二次在 frame 59，因此固定文件名被第二次初始化覆盖，不是同一条日志重复打印。两次初始框均为 `(1047,484,64,64)`。
- 最终快照 `/app/nanotrack_433deff_init_frame_diag_20261101/log/nanotrack_init_input.png` 为 2,018,708 bytes、SHA-256 `3aae880f8d7701010b8fa80f13acb5d8c0463a0bdc16c1fab84dad5a6d2db4d9`。本地 `file` 识别为 `1920 x 1080, 8-bit/color RGB, non-interlaced`；适配层日志为 `width=1920 height=1080 channels=3 step=5760`。人工查看显示当前室内真实画面，色彩自然，无绿屏、行错位或明显通道交换；初始框坐标位于画面中央偏右的杯子/把手区域。由此可确认进入 NanoTrack 初始化前的完整 BGR 帧内容和内存步长正确。
- 同一日志中第一次初始化后的 57 个跟踪帧统计为 `quality0/quality1/quality2=0/3/54`、`accepted1/accepted0=0/57`。枚举 `0/1/2` 对应 `Reliable/Weak/Rejected`。实际配置 `/app/nanotrack_b827_candidate/config/visible-default.conf` SHA-256 为 `2010c6cd16553595844a12f103f53412942289262d866b9fadba26b2821337ed`，要求 `reliable_psr=6.0`；本次 PSR 约为 `4.5..5.9`，因此没有观测达到 Reliable，而控制器只接受 Reliable 观测。这把 `accepted=false` 定位到质量门/度量层，不支持“输入帧格式错误”假设。

### 证据边界与 Handoff

- 本节证明诊断代码通过主机回归和 A6 交叉编译，并在测试板取得了可目视复核的初始化输入帧；它不证明门限应如何修改，也不证明跟踪质量、重捕、伺服控制或真实吊舱验收。
- 本轮没有修改 PSR/APCE/peak-margin 计算、可靠门限、accepted 策略、长期状态机或控制输出。下一步应由算法/开发板验证任务基于有标注稳定序列核对 PSR 度量或标定门限；交叉编译任务只在协调者锁定新的精确源码/配置身份后重新构建。
- 公共契约仍应保持 `development_board_validation.latest_handoff.available=false`；协调对话可采纳本节的“初始化输入正确、拒绝发生在质量门之后”诊断证据，但不得据此宣称开发板或吊舱验收通过。

## 2026-11-03 可见光 PSR 4.5/4.5 候选验证与主线同步

- 时间：`2026-11-03T23:54:32+08:00`；分支/工作树：`codex/cross-compile-main-b827` / `/home/u20/single-object-tracking/.worktrees/cross-compile-main-b827`；本轮起始 HEAD：`6ae23ee40a468067dd325ccb17809a01a5ff8d24`。
- 用户在同一测试板上分别完成两轮静止目标人工点选。旧门限轮为 `reliable_psr=6.0`、`weak_psr=5.0`，共 6 次初始化、817 个跟踪帧，`RELIABLE/WEAK/REJECTED=439/28/350`、`accepted=431`；其中 3/6 组从未进入接受跟踪。冻结日志为 `/app/nanotrack_433deff_init_frame_diag_20261101/log/{stdout,stderr}.threshold-20261103.log`，SHA-256 分别为 `a82072441fe06b9677af0769dc15fdc28a9c71bccfb889b0c7050ab7b361b2dd`、`819ced0e04cd0bb3641e9add93bffdcc742076a6d9344bc6de63d8bd29b7ae9a`。
- 单变量候选仅把可见光 PSR 门限改为 `4.5/4.5`；板端原配置保留为 `/app/nanotrack_b827_candidate/config/visible-default.conf.pre-psr45-20261103`，SHA-256 `2010c6cd16553595844a12f103f53412942289262d866b9fadba26b2821337ed`。实际候选配置 SHA-256 为 `fb6e0ab48a5a2ae57873a53ae8cccae7a5c8a03ffd9e347dc9f966922580c15b`。两轮均使用同一诊断程序 `/app/nanotrack_433deff_init_frame_diag_20261101/bin/viewlink_app`，SHA-256 `9daea988453a3ae03d83dff8467da293be0139e357c73200f3c90216a0531bff`。
- 新门限轮同样为 6 次初始化，共 882 个跟踪帧，`RELIABLE/WEAK/REJECTED=857/11/14`、`accepted=783`；六组均进入接受跟踪，最长连续未接受为 4 帧，状态帧分布 `TRACKING/UNCERTAIN/LOCAL_RECOVERY/ASSISTED_RECOVERY/LOST=783/98/1/0/0`。仅 1 帧 PSR 低于 4.5；其余拒绝主要由瞬时 peak margin 触发，均未推进到 assisted recovery 或 lost。冻结日志为 `/app/nanotrack_433deff_init_frame_diag_20261101/log/{stdout,stderr}.psr45-20261103.log`，SHA-256 分别为 `5e223405d930183ee05afb662b2481468d40090a2bced147db6e369762a4f85e`、`efe8b29274c66fb46f8b6bff7bcc91a3bc9985efabfba3fda7186137a03930a4`；测试程序已停止。
- 两轮目标不同，因此上述差异不是同目标 A/B 因果证明；日志没有标注真值，也不能证明框始终落在正确目标上。用户只确认所选目标均为静止且按预期不应跟丢。该结果支持将 `4.5/4.5` 保留为候选，但不构成精度、稳定率、重捕率、开发板正式验收或真实吊舱验收。
- 主线同步同时修改随包 `rk3566/nanotrack_v3/config/visible-default.conf` 与 `default_controller_config("visible")`，避免内置默认仍为 `6.0/5.0` 或出现 `reliable_psr < weak_psr` 的非法组合；红外默认和其余质量门均未修改。测试先按 TDD 得到预期 RED：`controller_support` 对内置默认和随包配置共报 4 项旧值失败；最小实现后聚焦测试通过，随后执行 `cmake --build /tmp/nanotrack-psr45-host-20261103 -j4` 和 `ctest --test-dir /tmp/nanotrack-psr45-host-20261103 --output-on-failure`，结果 `18/18` 通过、零失败。同步后的随包配置 SHA-256 为 `fb6e0ab48a5a2ae57873a53ae8cccae7a5c8a03ffd9e347dc9f966922580c15b`。
- 本轮没有修改协调对话拥有的 `docs/progress/STATUS.md` 或 `docs/progress/integration-contract.yaml`，没有执行 A6 生产交叉编译、生产部署或真实吊舱验收。生产交叉编译仍须等待协调者批准的开发板 handoff；本提交只同步经本轮功能诊断支持的可见光 PSR 候选门限及其回归测试。

## 2026-11-04 静止目标阈值分层与控制器尺度稳定候选

- 时间：`2026-11-04T21:59:03+08:00`；分支/工作树：`codex/cross-compile-main-b827` / `/home/u20/single-object-tracking/.worktrees/cross-compile-main-b827`；实施前 HEAD：`7b87042b75e886cf2427f89d521078da59c660eb`。用户先批准本地源码修改和主机验证；随后于 `2026-11-04T22:05:44+08:00` 明确要求继续实机测试，授权本候选提交、A6 交叉编译、隔离部署和测试进程启动。提交前主机构建成功、CTest `18/18` 通过；本节后续继续记录交叉编译与实机启动的真实结果，不以授权本身代替完成证据。
- 诊断输入为十次人工确认的静止目标测试，共 1878 个 `DO_TRACK` 帧。冻结日志 `/tmp/stdout.scale-static-20261104.log`、`/tmp/stderr.scale-static-20261104.log` 的 SHA-256 分别为 `7dff6ee141452021acbfae1246108c4b33d64df068df2a21490c1f16ac2864ba`、`2e02db905df70a6efedb0a0a76151b04a478e4cafd7f027a45d872021fbf4023`。用户补充确认第十次恢复的是同一目标，且本次目标不属于 detector 支持类别；因此同目标恢复和无 detector 候选不记为异常。
- 排除人工报告跟丢的第七次后，48 次从 accepted 进入 non-accepted 的起点中，47 次涉及 `reliable_peak_margin < 0.06`，只有 1 次主要由 `PSR < 4.5` 触发。第七次恢复窗口存在约 `4.30..4.47` 的 PSR。由此把可见光候选分层改为 `reliable_psr=4.2`、`weak_psr=4.0`、`reliable_peak_margin=0.04`、`weak_peak_margin=0.02`；`calibrated=false` 保持不变，raw score、APCE、几何门、模板确认和红外配置不变。
- 根因链路为：非可靠帧触发搜索倍率从 `1.0` 扩到 `1.5` 或更大；更大原图区域仍缩放到固定 `255x255`，改变目标采样、响应峰和尺度回归；此前恢复观察的宽高又会直接进入最终框和下一帧参考，形成锯齿。候选新增 `max_accepted_scale_step_ratio=0.05`：每个 accepted 帧的宽、高分别相对最后 accepted 控制框最多变化正负 5%，中心仍采用可靠观察中心；同一稳定框进入 `ControllerOutput`、运动修正/重初始化、控制器当前状态和既有 `commit_bbox()` 路径。值 `1.0` 明确定义为兼容旧行为的“不限制”哨兵，故 generic/infrared 不改变行为。
- 普通 non-accepted 路径现在立即调用既有 `MotionEstimator::freeze_scale()`，把宽高恢复到最后 accepted 框并清零尺度速度；中心继续按运动模型预测。扩大搜索仍用于定位，但未确认观察的原始尺度不再污染最终框。两次一致 Reliable 的三帧窗口确认规则保持不变；确认重捕后，观察中心立即恢复，尺度仍通过 5% 限制后统一写回。
- 设计和计划分别为 `docs/superpowers/specs/2026-11-04-visible-threshold-scale-stabilization-design.md`、`docs/superpowers/plans/2026-11-04-visible-threshold-scale-stabilization.md`。TDD RED 实际观察到：新配置字段不存在导致编译失败；未限制实现产生 7 项接受尺度断言失败；普通丢失产生 6 项冻结/恢复断言失败；`1.0` 兼容语义产生 1 项断言失败。最小实现后，配置/元数据聚焦测试 `2/2` 通过，控制器/运行时聚焦测试 `2/2` 通过。最终自审又发现板端证据校验器未强制新元数据字段；新增缺失字段和 `1.01` 越界负例后先观察到 `verify_board_schema` 错误放行并失败，再补充 `scripts/verify_board.sh` 的 `(0,1]` 校验，聚焦测试恢复通过。
- 在 `/tmp/nanotrack-visible-scale-stabilization` 执行 `cmake -S rk3566/nanotrack_v3 -B /tmp/nanotrack-visible-scale-stabilization -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF`、`cmake --build /tmp/nanotrack-visible-scale-stabilization -- -j2`、`ctest --test-dir /tmp/nanotrack-visible-scale-stabilization --output-on-failure`；配置、构建成功，CTest `18/18` 通过、零失败，`git diff --check` 成功。修改后的随包 `rk3566/nanotrack_v3/config/visible-default.conf` SHA-256 为 `b2a333ca0cfe8173d63f5cfa71f24304e7cabd08d7656d3ada71b902b32372bf`。

### 证据边界与下一步

- 当前只证明候选配置、控制器尺度限制、丢失冻结和运行时提交一致性通过 RKNN-independent 主机测试。十次测试只有正样本人工语义，没有相似干扰、遮挡和错误重捕负样本，故不得把候选标为正式阈值标定完成。
- 截至本地候选提交前，尚未执行 A6 交叉编译、板端/吊舱部署或修改远程配置，也没有启动或停止远程进程。用户已授权继续实机测试；下一步按隔离候选方式交叉编译、核验工件哈希、部署并启动，由人进行同协议静止目标与负样本测试，比较 false-loss 起点、逐帧宽高变化、身份保持和错误接受。
- 本轮没有修改协调对话拥有的 `docs/progress/STATUS.md`、`docs/progress/integration-contract.yaml` 或开发板任务拥有的 `docs/progress/board-validation.md`。公共契约仍应保持 `development_board_validation.latest_handoff.available=false`，真实吊舱验收只能由人报告。

### A6 交叉编译、隔离部署与实机测试启动

- 时间：`2026-11-04T22:15:03+08:00`；输入源码提交：`7dabe87d52e2126421196667f7b261e8151c6491`。提交前重新执行主机构建与完整 CTest，结果仍为 `18/18` 通过、零失败。
- WSL 工程为 `/home/wjm/viewpro_gimbal_zp/rk3566/gimbal_tracker`。同步前将 8 个 NanoTrack 源码/配置输入保存到 `ai_alg/backup-before-visible-scale-7dabe87/`；同步包 `/tmp/nanotrack-visible-scale-7dabe87.tgz` 为 30,459 bytes，SHA-256 `a28716e66080d094b31d35e8f85a5803b6c128c8de23b81f39e91451c08133f9`。解包后逐项哈希与本地提交一致，再更新 WSL `ai_alg/nanotrack/{include,source,config}` 和 `deployment_b827/config/visible-default.conf`；既有快照/中心控制框诊断适配器未改动。
- 实际在 WSL 工程根目录执行 `BUILD_JOBS=4 ./run.sh --platform A6`，退出码 `0`，重新编译 `controller_config.cpp.o`、`long_term_controller.cpp.o` 等生产对象并成功链接 `libAI.a` 和 `viewlink_app`。完整日志 `deployment_b827/build-A6-visible-scale-7dabe87.log` 为 2,525 bytes，SHA-256 `2bc4d508ab6b9dfec9971cd90d5c797016341e8ded8783508219d3f12286b65f`。

| A6 工件 | 大小 | SHA-256 |
| --- | ---: | --- |
| `build_A6/viewlink_app` | 34,888,416 bytes | `a86c13ad278f06f140cf2a7cf27b8b1ced677a93556eb73c76c78ea6fda29c0b` |
| `build_A6/ai_alg/libAI.a` | 590,426 bytes | `f17c2899d7397b98ac833f4ec95766a0dbfd009cb45c1dd555bccf66bf3315b9` |

- `file` 将程序识别为 ARM aarch64 ELF；`libAI.a` 包含生产 `controller_config`、`long_term_controller`、`production_deployment_config`、`native-lib_impl` 等对象，二进制字符串可见 `max_accepted_scale_step_ratio`、逐帧 `AI_TRACK_DIAG` 格式和固定部署配置路径。
- 板端启动前确认无 `viewlink_app` 或 `app_watchdog` 进程。新程序隔离部署到 `/app/nanotrack_7dabe87_visible_scale_20261104/bin/viewlink_app`，板端大小和 SHA-256 与 WSL 完全一致。受保护的 `/app/bin/viewlink_app` 仍为 `a60b61a34cdaf0b43d27726a5ce311e9e995a8d34bb874657c944eabd86fb357`，未被覆盖。
- 运行配置 `/app/nanotrack_b827_candidate/config/visible-default.conf` 由上一轮 `4.5/4.5` 版本原子替换为本轮配置，当前 SHA-256 为 `b2a333ca0cfe8173d63f5cfa71f24304e7cabd08d7656d3ada71b902b32372bf`；旧配置保存在同目录 `visible-default.conf.pre-visible-scale-7dabe87-20261104`，SHA-256 `fb6e0ab48a5a2ae57873a53ae8cccae7a5c8a03ffd9e347dc9f966922580c15b`。
- 实际以 `XDG_RUNTIME_DIR=/var/run`、`LD_LIBRARY_PATH=/app/lib:/usr/lib` 后台启动隔离程序；启动 PID `17854`，stdout/stderr 分别为 `/app/nanotrack_7dabe87_visible_scale_20261104/log/stdout.visible-scale-20261104.log` 和 `stderr.visible-scale-20261104.log`。启动后复核进程存活，三个 NanoTrack RKNN 与 VisDrone9 RKNN 均报告 API `1.6.0 (9a7b5d24c@2023-12-13T17:31:11)`、driver `0.9.3` 并完成初始化。既有缺失 `user_custom.xml` 和 thermal sysfs 告警仍存在，但未阻止本次进程和模型初始化。
- 当前仅达到“候选源码提交、A6 交叉编译、隔离部署和模型启动成功”。实机点选尚未开始，不能据此评价阈值、尺度稳定、身份保持、伺服行为或吊舱验收；日志仍在增长，必须在用户完成后冻结并重新计算最终大小/SHA-256，再做逐次统计分析。

### 十一次实机点选冻结日志与分析

- 时间：`2026-11-04T22:29:58+08:00`。用户报告测试完成，并特别要求关注第 5 次；第 6 次末尾目标已超出画面。停止前核对 PID `17854` 的可执行路径为本节隔离候选；程序不响应 TERM，等待 10 秒后以 KILL 结束，随后确认无 `viewlink_app` 残留。
- 冻结 stdout 为 32,867 bytes、SHA-256 `f712859acd84d3c6e2693ac07b523f39c6ae7f09fc560a67b2601457eeea0f1c`；冻结 stderr 为 1,655,106 bytes、SHA-256 `94d4a168bee0f11ef7fc32a3c4bb62a44ff48dfbff1582cda00553631b5abb3a`。板端原路径位于 `/app/nanotrack_7dabe87_visible_scale_20261104/log/`，本地复核副本为 `/tmp/{stdout,stderr}.visible-scale-20261104.log`。
- 日志实际包含 11 次 `TRACK_INIT` 和 2,209 个 `DO_TRACK` 帧。全部帧为 `Reliable/Weak/Rejected=2191/6/12`、`accepted=2171`；状态为 `TRACKING/UNCERTAIN/LOCAL_RECOVERY=2171/36/2`，没有进入 `ASSISTED_RECOVERY` 或 `LOST`。以第 6 次原始框首次触及上边界的 frame 1198 作为出画分析边界并排除其后 19 帧，剩余画面内 2,190 帧为 `Reliable=2177`、`accepted=2160`，没有 `LOST`。
- 第 5 次为 frame 782..986，共 205 帧，`Reliable/Rejected=203/2`、`accepted=201`，最长连续未接受 3 帧，没有进入本地恢复或丢失。frame 783/784 的 PSR 分别为 `6.34/5.23`，拒绝原因是 peak margin 仅 `0.0035/0.0077`；frame 785/786 两次可靠观察后恢复接受。frame 957 是 anchor margin `0.0323` 与 recent margin `0.6118` 的模板分歧导致的一帧保守拒绝，frame 958 即恢复。原始框开头出现 `59x56 -> 67x65 -> 72x62 -> 73x78`，稳定输出仅为约 `60x61 -> 61x61 -> 61x61 -> 64x64`，说明 5% 限制压住了突变，但不能消除响应本身的中心/尺度波动。
- 第 6 次在 frame 988..1197 的画面内部分共 210 帧，全部判为 Reliable，209 帧 accepted；只有 frame 1195 因模板分歧保守未接受一帧。frame 1198 起原始框 `y=0`，与用户报告的末尾出画相符：后续 19 帧仍有 11 帧 accepted，最终宽度由约 60 累计增长到 85。frame 1207/1208 因 PSR `4.19/4.02` 为 Weak；frame 1209/1210 的 PSR `4.268/4.299` 在新门限下为 Reliable，frame 1211 的 PSR `4.499` 使确认成立，输出中心相对预测框跳变约 78 像素。frame 1212..1216 又在 Rejected/Reliable/Accepted 间摆动。这是出画边界处的错误恢复风险，不能算作普通静止目标跟踪失败，但证明低 PSR 门限和现有 1.5 倍中心残差门不足以单独阻止边界伪峰确认。
- 完全画面内的逐帧尺寸仍有高频小幅变化：最终宽度在 56.6% 的相邻帧发生变化，变化中位数 1 像素、P95 为 2 像素、最大 4 像素，方向反转 866 次；最终高度仅 6.2% 的相邻帧变化。对应原始宽度有 70.3% 的相邻帧变化、P95 为 5 像素、最大 12 像素。故本轮限制显著降低单帧尺度突变和高度抖动，但不是低通或死区，不能把“宽度快速来回变化”标记为解决。
- 其余画面内区间中，第 10 次开头是质量波动最大的一组：4 个未接受片段、最长 4 帧、一次进入 `LOCAL_RECOVERY`；PSR 始终为 `5.57..6.89`，主要原因仍是 peak margin `0.004..0.031`，不是 PSR 4.2 门限。继续无条件降低 peak-margin 门会把明显多峰响应直接标为 Reliable，不由本轮正样本日志支持。
- 本轮建议保持当前阈值作为候选，不继续下调；下一轮代码设计应分别处理两个结构问题：对 accepted 尺度增加小变化死区/时间平滑以消除 1..2 像素高频反转；对连续边界接触/部分出画增加可见性门，并限制恢复中心相对运动预测的跳变，避免仅靠两个彼此一致的边界伪峰恢复控制。未经用户再次确认，本节没有修改算法或配置；除冻结并停止本轮测试进程外，只更新本任务进度记录。

## 2026-11-04 可见光尺度死区/平滑与普通恢复中心门本地主机实现

- 时间：`2026-11-04T23:05:20+08:00`；分支/工作树：`codex/cross-compile-main-b827` / `/home/u20/single-object-tracking/.worktrees/cross-compile-main-b827`；实施前 HEAD：`6e4791d72063bb9ab624b6e9fb29e89bf4bc007d`。配置契约提交为 `c8c8ef6aeccbd9d3fa227e0bb216d8ebeb5ff103`，控制器行为提交为 `1f2608aa97bd7d8f1e300ab9eab11ce20b8b0ac4`；本节的元数据、证据校验和进度记录随最终实现提交一并形成。
- 用户确认的设计保持可见光质量门 `reliable_psr/weak_psr=4.2/4.0`、peak margin `0.04/0.02` 和外层尺度上限 `max_accepted_scale_step_ratio=0.05`；新增 `accepted_scale_deadband_ratio=0.02`、`accepted_scale_smoothing_alpha=0.10` 和 `max_reacquisition_center_distance=0.75`。通用/红外有效兼容值为 `0/1/0`，其中已有 `scale_step=1` 继续作为完全不限制的哨兵。随包可见光配置 SHA-256 为 `431487f2758c82bdbd0842017ef33619ea94e37da622aaeb0f89a5814eb96a8e`。
- accepted 宽高现在分别处理：相对最后 accepted 尺寸不超过 2% 时保持；超过时先把原始观察限制在正负 5%，再向限制后目标移动 10%，因此可见光每个 accepted 帧的单维最大变化为 0.5%。中心仍取原始可靠观察中心并由既有 `centered_box` 保证框合法；持续真实尺度变化会逐步收敛，进入 2% 死区后停止。non-accepted 尺度冻结、运行时 `commit_bbox()` 使用最终控制器框的既有语义保持不变。
- 普通 motion 恢复（含模板分歧触发的确认）只有在已计算的归一化中心残差不超过固定 `0.75` 时才进入两次确认队列，门限不再随搜索倍率放大；失败会清空普通确认队列。detector 确认和 `OCCLUSION_HOLD` probe 确认保持原有独立几何门，未修改。按用户边界，本轮没有新增画面边缘、部分可见或出画处理。
- 设计文档 `docs/superpowers/specs/2026-11-04-visible-scale-smoothing-recovery-gate-design.md` SHA-256 为 `17fb2b109036e86ae1f353f8e28baf4431e70ad3b98ccbf142557f5e98237028`；实施计划 `docs/superpowers/plans/2026-11-04-visible-scale-smoothing-recovery-gate.md` SHA-256 为 `fc097baab8f9572d317c49dcfd2e335f59038636270a0bbe79a65b23bfd79d38`。
- TDD RED 依次观察到：配置测试因三个字段和 identity API 不存在而编译失败；旧控制器产生 deadband、0.5% 平滑步长、普通恢复队列清理和 runtime commit 共 21 项断言失败；元数据测试因三个字段不存在而编译失败。最小实现后，配置聚焦测试 `1/1`、控制器/运行时/遮挡聚焦测试 `3/3`、元数据/板端 schema 聚焦测试 `2/2` 均通过。
- 实际主机命令为 `cmake -S rk3566/nanotrack_v3 -B /tmp/nanotrack-visible-scale-recovery-host -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF`、`cmake --build /tmp/nanotrack-visible-scale-recovery-host -j4`、`ctest --test-dir /tmp/nanotrack-visible-scale-recovery-host --output-on-failure` 和 `git diff --check`。实施前基线和实现后回归均为 CTest `18/18` 通过、零失败；实现后 `git diff --check` 无输出。session metadata 与板端 verifier 在既有 schema version `9` 下新增并强制三个有效设置，缺失、非有限和越界负例均被测试覆盖。
- 独立最终代码审查复现了浮点边界缺陷：accepted 宽度为 `60` 时，字面上正负 2% 的 `61.2F/58.8F` 会因两边独立舍入而错误进入平滑。新增这两个精确边界及 `61.2002F/58.7998F` 边界外回归，先观察到聚焦测试 2 项失败；随后改为先计算同一 float deadband allowance，再构造包含式上下界。修正后聚焦测试 `1/1`、完整 CTest `18/18` 通过，`git diff --check` 无输出。

### 证据边界与下一步

- 本节只证明 RKNN-independent 主机逻辑、配置解析、控制器状态、运行时提交、元数据序列化和板端证据 schema 回归通过。没有执行 A6 生产交叉编译，没有写入 WSL 或开发板/吊舱，没有复制或部署二进制/配置，没有停止或启动远程进程，也没有产生新的板端或真实吊舱测试结果。
- 本轮未修改协调对话拥有的 `docs/progress/STATUS.md`、`docs/progress/integration-contract.yaml` 或开发板任务拥有的 `docs/progress/board-validation.md`。公共契约和真实吊舱人工验收边界保持不变；任何交叉编译、隔离部署、进程操作和下一轮实机点选都需要用户另行明确授权。

## 2026-11-05 尺度死区/平滑候选 A6 编译、吊舱复测与高度抖动分析

- 时间：`2026-11-05T21:24:48+08:00`；分支/工作树：`codex/cross-compile-main-b827` / `/home/u20/single-object-tracking/.worktrees/cross-compile-main-b827`；生产输入提交：`4c4511001bad4f28a552e3005a3cad6617256148`。操作前工作树干净，用户明确授权交叉编译、隔离部署、启动测试并在完成后冻结日志。
- 同步包 `/tmp/nanotrack-scale-smooth-4c45110-vj4ibu.tgz` 为 22,012 bytes、SHA-256 `200372e454a8dee75f16e47bb9c3254d97c635acd2ea7d1b1a8655c0e73029c2`。只同步生产使用的 `controller_config.hpp`、`tracking_log.hpp`、`controller_config.cpp`、`long_term_controller.cpp`、`tracking_log.cpp` 和 `visible-default.conf`；WSL 写入前备份位于 `/home/wjm/viewpro_gimbal_zp/rk3566/gimbal_tracker/ai_alg/backup-before-scale-smooth-4c45110-20261105/`，没有同步测试或主机调试入口。
- WSL 工程 `/home/wjm/viewpro_gimbal_zp/rk3566/gimbal_tracker` 实际执行 `BUILD_JOBS=4 ./run.sh --platform A6`，退出码 `0`；日志 `deployment_b827/build-A6-scale-smooth-4c45110.log` 为 2,525 bytes、SHA-256 `0258d737a24901245146d83f49c54611150ea1c622717756f5e3664237e87ca6`。`build_A6/viewlink_app` 为 ARM aarch64 ELF、34,898,056 bytes、SHA-256 `c16c9ad77733a00a5d8a43d3a4879da6c29c097f0a6babd6c283e37f7dfb3cc6`；`build_A6/ai_alg/libAI.a` 为 597,856 bytes、SHA-256 `d60a0775813e0dba4578a1cd1735244b41d5eee7620515de761b412a18943285`。二进制字符串包含 `accepted_scale_deadband_ratio`、`accepted_scale_smoothing_alpha`、`max_reacquisition_center_distance` 和固定生产部署配置路径。
- 新程序隔离部署到 `/app/nanotrack_4c45110_scale_smooth_20261105/bin/viewlink_app`，板端大小/哈希与 WSL 一致。共享运行配置 `/app/nanotrack_b827_candidate/config/visible-default.conf` 更新为 SHA-256 `431487f2758c82bdbd0842017ef33619ea94e37da622aaeb0f89a5814eb96a8e`；上一轮配置保存在 `visible-default.conf.pre-scale-smooth-4c45110-20261105`，SHA-256 `b2a333ca0cfe8173d63f5cfa71f24304e7cabd08d7656d3ada71b902b32372bf`。受保护的 `/app/bin/viewlink_app` 始终保持 SHA-256 `a60b61a34cdaf0b43d27726a5ce311e9e995a8d34bb874657c944eabd86fb357`。
- 以 `XDG_RUNTIME_DIR=/var/run`、`LD_LIBRARY_PATH=/app/lib:/usr/lib` 启动隔离候选，PID 为 `2185`。三个 NanoTrack RKNN 和 VisDrone9 RKNN 均报告 API `1.6.0 (9a7b5d24c@2023-12-13T17:31:11)`、driver `0.9.3` 并完成初始化。用户完成同目标重复点选后，先核对 `/proc/2185/exe` 为本轮隔离程序；TERM 等待 10 秒仍未退出，随后只对该 PID 执行 KILL，并确认无 `viewlink_app` 或 `app_watchdog` 残留。
- 冻结 stdout 为 28,771 bytes、SHA-256 `17f14fd2d19271fcbce93fb055e05bc624daa7e46b5c6575ff0cd998df6b971f`；stderr 为 1,474,217 bytes、SHA-256 `6d8536e5b199a72490e1490b81f6b9917bbed1c14e437cd19572150ca32259f4`。板端原路径为 `/app/nanotrack_4c45110_scale_smooth_20261105/log/`，本地复核副本位于 `/tmp/nanotrack-4c45110-logs/`；板端、WSL 和本地哈希一致。
- 日志实际包含 10 次初始化和 1,967 个 `DO_TRACK` 帧；`Reliable/Weak/Rejected=1897/18/52`、`accepted=1820`，最长连续未接受为 4 帧，状态模式为 `TRACKING/UNCERTAIN/LOCAL_RECOVERY=1820/145/2`，没有进入 assisted recovery 或 lost。52 个 Rejected 全部满足 `selected_margin < 0.02`，18 个 Weak 全部满足 `selected_margin < 0.04`，没有一帧由 PSR 低于 `4.0/4.2` 主导。
- 在同一次初始化内部统计最终控制框：宽度变化率由上一轮 `56.6%` 降为 `33.5%`，变化 P95 由 3 像素降为 1 像素；但高度变化率由上一轮 `6.2%` 升到 `24.3%`，共 475 次，其中 472 次为 1 像素，方向反转 303 次，`48<->49` 往返 102 次。475 次高度变化中，448 次发生在 `Reliable && accepted` 帧，故用户观察到的高度“呼吸”真实存在，且不是降低质量门限可以解决的问题。
- 十组控制框高度都从初始化的 `64` 缓慢收敛到 `48/49`，首次到达不高于 49 像素需要 72..150 帧；上一轮 5% 单帧限制下只需 8..27 帧。当前逻辑对持续偏离死区的原始尺度每个 accepted 帧只移动至多 0.5%，浮点小步累积后在整数输出上形成频繁 1 像素变化和反转；稳定框又由 `LongTermRuntime::process_frame()` 通过 `tracker_.commit_bbox(result.output.bbox)` 写回 NanoTrack，下一帧搜索裁剪和尺度回归随之改变，构成尺度反馈。新日志的 raw 高度变化率为 `45.6%`，与该反馈解释一致，但两轮不是逐帧同步 A/B，不能把全部差异单独归因于本次代码。

### 结论、证据边界与下一步

- 当前候选实现了“限制单帧大跳”，但以更长时间、更高频率的 1 像素高度呼吸为代价；不能标记尺度稳定问题已解决。阈值不是本轮视觉抖动的主因，不建议继续下调 PSR 或 peak-margin 门限。
- 下一版应在保持真实尺度可缓慢变化的前提下，增加尺度观测的时间一致性和整数输出迟滞，例如先对 5 帧左右可靠尺度取中值/要求连续同向证据，再更新浮点尺度，并只在跨过明确像素迟滞边界后改变输出宽高；还需明确区分供 NanoTrack 下一帧裁剪的内部尺度状态和供显示/控制的稳定输出，避免未经验证的反馈耦合。未经用户确认，本轮没有继续修改算法或参数。
- 本轮是人机协同的吊舱功能复测，不是开发板正式 handoff、精度标定或真实吊舱验收。没有修改协调对话拥有的 `docs/progress/STATUS.md`、`docs/progress/integration-contract.yaml` 或开发板任务拥有的 `docs/progress/board-validation.md`。

## 2026-11-05 可见光尺度时间一致性与输出迟滞本地主机实现

- 时间：`2026-11-05T21:48:25+08:00`；分支/工作树：`codex/cross-compile-main-b827` / `/home/u20/single-object-tracking/.worktrees/cross-compile-main-b827`；实施前基线：`95231b10b408db689680c38b8abeeed9bcbefd2c`；配置与证据契约提交：`d1320cb14d225788ed7ed83671c4710e0e2e3901`；控制器行为、测试和本节记录随同一后续提交形成，准确提交号以本节所在 Git 提交为准。
- 用户确认的设计与计划分别为 `docs/superpowers/specs/2026-11-05-visible-scale-temporal-hysteresis-design.md`（SHA-256 `b46658afceee03d575ccdc63690f4f5c3ab68e52d5d36d8164f97df8b71704b9`）和 `docs/superpowers/plans/2026-11-05-visible-scale-temporal-hysteresis.md`（SHA-256 `545e32cad95eceec2abadc1f665b45e0d457a70276d9cbdec01f15eb23052c1f`）。最终随包可见光配置 `rk3566/nanotrack_v3/config/visible-default.conf` SHA-256 为 `dbd412b7df5998fb52b3faa55327754322bb5fcd8a7bdf7c4c0d44127c8871f6`。
- 保持质量门 `reliable_psr/weak_psr=4.2/4.0`、peak margin `0.04/0.02`，以及已有尺度步长/死区/平滑 `0.05/0.02/0.10` 不变。新增并记录可见光有效设置：`accepted_scale_median_window_frames=5`、`accepted_scale_direction_confirm_frames=3`、`accepted_scale_output_hysteresis_px=0.75`；通用和红外兼容值为 `1/1/0`。三个字段进入配置解析、校验、identity、schema-9 session metadata 和板端证据校验。
- 控制器现在为宽、高分别保留 accepted 原始尺度窗口、浮点滤波值、最终发射值、方向和连续确认数。窗口填满前保持尺寸；中值超出既有 2% 死区后，需三个同向中值才按既有正负 5% 外限和 0.10 alpha 更新；滤波值与最终值差小于 0.75 像素时保持，达到边界时每帧最多沿该方向移动一像素。最终框仍是控制器、motion、`LongTermRuntime::commit_bbox()`、适配器输出和日志的唯一共同框，没有增加内部/显示双框。
- 初始化重置全部尺度状态；普通、detector 和遮挡恢复在处理恢复观察前只清空样本/方向证据，保留滤波及最终尺度，因此不会因恢复框直接跳变。non-accepted 和 abort 路径不采样、不更新尺度状态；短间隔保留已有 accepted 历史。`1/1/0` 兼容路径保留原有每帧尺度行为，其中原有 unrestricted `1/0/1` 继续精确输出原始 accepted 尺寸。
- TDD RED 先观察到：新增私有状态尚不存在导致控制器测试编译失败；改为纯行为回归后，旧实现产生 65 个控制器断言失败和 8 个运行时断言失败，具体覆盖窗口前提前更新、`48/49` 高度往返、0.75 像素边界未迟滞、每帧变化超过一像素、恢复跳变和 runtime 高度提交抖动。最小实现后的聚焦命令 `ctest --test-dir /tmp/nanotrack-visible-temporal-hysteresis-host --output-on-failure -R 'long_term_controller|long_term_runtime'` 为 `2/2` 通过。
- 独立整分支审查发现并复现两个重要缺陷：中值进入滤波死区后，最终发射尺寸会停止追赶已有滤波积压；合法 `5/3/0` 配置会因固定一像素步长越过滤波值而反向抖动。新增 600 帧持续增长/缩小收敛测试和零迟滞单调增长测试，先观察到聚焦测试分别出现“长期未收敛”和 8 次反转失败；修正为在死区内继续追赶滤波值，并在迟滞不大于 0.5 像素时采用不越过滤波值的至多一像素步长。选定的 0.75 像素边界仍保持达到边界时移动一整像素。
- 实际主机命令：`cmake -S rk3566/nanotrack_v3 -B /tmp/nanotrack-visible-temporal-hysteresis-host -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF`、`cmake --build /tmp/nanotrack-visible-temporal-hysteresis-host -j4`、`ctest --test-dir /tmp/nanotrack-visible-temporal-hysteresis-host --output-on-failure`、`git diff --check`。修改前基线和实现后完整回归均为 CTest `18/18` 通过、零失败；实现后格式检查无输出。

### 证据边界与下一步

- 本节只证明 RKNN-independent 主机上的配置、时间尺度状态、精确 0.75 像素边界、恢复/失败冻结、运行时最终框提交、元数据和证据 schema 行为。没有写入 WSL，没有执行 A6 交叉编译，没有生成或部署新的二进制/配置，没有连接吊舱，也没有停止或启动任何远程进程。
- 因此“吊舱框抖动已改善”仍未得到实机证据。后续只有在用户另行明确授权后，才能以准确源码提交和配置哈希进行 A6 交叉编译、隔离部署，并由人执行同目标复测；不得以主机 `18/18` 或编译成功替代真实吊舱验收。
- 本轮没有修改协调对话拥有的 `docs/progress/STATUS.md`、`docs/progress/integration-contract.yaml` 或开发板任务拥有的 `docs/progress/board-validation.md`；公共 handoff 与真实吊舱人工验收边界保持不变。

## 2026-11-06 尺度时间一致性候选 A6 编译与吊舱复测

- 时间：`2026-11-06T21:11:31+08:00`；分支/工作树：`codex/cross-compile-main-b827` / `/home/u20/single-object-tracking/.worktrees/cross-compile-main-b827`；生产输入提交：`c6a216f0d8c11961d24590aba437f6d70b02dc88`。操作前工作树干净，用户明确要求再次实机测试，并延续本轮已确认的交叉编译、隔离部署、启动和完成后冻结日志授权。
- 首次同步沿用了上一轮六文件清单，遗漏本轮新增私有状态的 `include/long_term_controller.hpp`。第一次实际执行 `BUILD_JOBS=4 ./run.sh --platform A6` 退出码为 `2`，编译器明确报告 `reset_scale_state`、`DimensionScaleState` 和 `stabilize_accepted_bbox` 声明与实现不一致；失败日志 `deployment_b827/build-A6-temporal-c6a216f.log` 为 8,279 bytes、SHA-256 `46936f00bf445e2df151368db9876cfdf3a4e806cfee589a791228165948f41e`，此时板端尚未写入。
- WSL 原头文件备份到 `ai_alg/backup-before-temporal-c6a216f-20261105/include/long_term_controller.hpp`，SHA-256 为 `834db5f5a5e664990956742c7532a4dd231a267c3869cf1f9806a634030890d2`；补同步后的头文件 SHA-256 为 `5583666e7fbf530170cd09e53260585eb1b40ba1b28467a0a555fb9b56db64c2`。补齐后逐一复核的七个生产输入为：`controller_config.hpp` `e466209de5ee3413550f967331b6f97571aa0a346fee8e81db0a0d6cc72e8854`、`tracking_log.hpp` `3c9ece661b148d083ed8df83f1307fc0e0646c5bdd5280bdb29b7463966308c1`、`long_term_controller.hpp` `5583666e7fbf530170cd09e53260585eb1b40ba1b28467a0a555fb9b56db64c2`、`controller_config.cpp` `dcc4942598489aed1c2162ab883d3cc13eb7ce5b039fa58f2701b9b649627bf9`、`long_term_controller.cpp` `2d14629e40d1e125bd67efa5eef2aaa641a542b731325e4e531524a1f60bb4f2`、`tracking_log.cpp` `a8a809a4032e94a32aa8b70906dacb1472d219b813d5226872f53262bd4fb666`、`visible-default.conf` `dbd412b7df5998fb52b3faa55327754322bb5fcd8a7bdf7c4c0d44127c8871f6`。
- 第二次执行同一 A6 命令退出码为 `0`。重试日志 `deployment_b827/build-A6-temporal-c6a216f-retry.log` 为 2,525 bytes、SHA-256 `ba23131ce009a97c2f0f521f5176f66dd965f1065276a40c00ac174bdd64d222`。`build_A6/viewlink_app` 为 ARM aarch64 ELF、34,903,264 bytes、SHA-256 `90672c847d76ee8ae24fa052ec845e21763ce9f88aac90915c0b23e8018a851e`；`build_A6/ai_alg/libAI.a` 为 612,636 bytes、SHA-256 `4251e6f24d0fb0730d9a2b057a9dbca7f8030af2011f41cd2c713eba4306babe`。二进制字符串包含 `accepted_scale_median_window_frames`、`accepted_scale_direction_confirm_frames` 和 `accepted_scale_output_hysteresis_px`。
- 候选只部署到 `/app/nanotrack_c6a216f_temporal_scale_20261106/bin/viewlink_app`，板端哈希与 WSL 一致。旧共享配置备份为同目录 `config/visible-default.pre-c6a216f.conf`，SHA-256 `431487f2758c82bdbd0842017ef33619ea94e37da622aaeb0f89a5814eb96a8e`；共享测试配置更新为 `dbd412b7df5998fb52b3faa55327754322bb5fcd8a7bdf7c4c0d44127c8871f6`。受保护的 `/app/bin/viewlink_app` 始终保持 SHA-256 `a60b61a34cdaf0b43d27726a5ce311e9e995a8d34bb874657c944eabd86fb357`。
- 以 `XDG_RUNTIME_DIR=/var/run`、`LD_LIBRARY_PATH=/app/lib:/usr/lib` 启动隔离候选，PID 为 `1904`；`/proc/1904/exe` 哈希为本轮候选哈希，RTSP 端口 `554` 由该 PID 监听。三个 NanoTrack RKNN 与 VisDrone9 RKNN 均记录 API `1.6.0 (9a7b5d24c@2023-12-13T17:31:11)`、driver `0.9.3` 并完成初始化。用户报告测试完成后先核验进程路径；TERM 等待 5 秒未退出，随后只对 PID `1904` 执行 KILL，并确认无 `viewlink_app` 或 `app_watchdog` 残留。
- 冻结 stdout 为 28,771 bytes、SHA-256 `fff550912d9bbb2dd670a3f85a0ff7452ace4e108a41edc4045598d6dd04f971`；stderr 为 1,619,421 bytes、SHA-256 `cc6fad3b5b9cc82c54fb8a6ae95ca5448065bafcbc579aa49282d46e621311b9`。板端原路径为 `/app/nanotrack_c6a216f_temporal_scale_20261106/log/`。含日志和旧配置副本的冻结包在板端 `/tmp/nanotrack-c6a216f-test-20261106.tgz`、WSL `deployment_b827/nanotrack-c6a216f-test-20261106.tgz`，大小 255,336 bytes、SHA-256 均为 `f8d17adca7e790e8377c46b52259680e50be15bfd8da0f4e55ca1fa3b9b39ccf`；本地复核副本位于 `/tmp/nanotrack-c6a216f-analysis/`。

### 十一次点选的尺度与质量结果

- 日志实际包含 11 次初始化和 2,161 个 `DO_TRACK` 帧。`Reliable/Weak/Rejected=2088/28/45`，`accepted=1982`；状态模式为 `TRACKING/UNCERTAIN/LOCAL_RECOVERY=1982/178/1`，没有进入 assisted recovery、lost 或 occlusion hold，单次最长连续未接受为 4 帧。全部 28 个 Weak 的 PSR 为 `4.970..7.007`、margin 为 `0.0200..0.0399`；全部 45 个 Rejected 的 PSR 为 `5.892..6.584`、margin 为 `0.0011..0.0196`，因此本轮非 Reliable 帧仍由 peak-margin 门解释，不是 PSR `4.2/4.0` 过高。
- 在同一次初始化内部统计，原始宽度相邻帧变化率为 `71.3%`、P95 `4` 像素、最大 `11` 像素、方向反转 `1,143` 次；原始高度变化率为 `53.6%`、P95 `12` 像素、最大 `21` 像素、方向反转 `840` 次。最终/控制框宽度变化率降为 `6.1%`，高度降为 `6.6%`；两者单帧最大变化均为 `1` 像素，方向反转分别为 `2/1` 次，`48<->49` 往返为 `0`。2,161 帧中最终框与控制框的宽高完全一致，全部 2,172 条诊断的 `control_err=(0,0)`。
- 宽高任一变化的相邻帧为 264/2,150，即 `12.3%`；变化间隔中位数为 4 帧，最长连续变化仅 2 帧。264 次变化中，132 次仅高度减 1、119 次仅宽度减 1、9 次宽高同时减 1，仅有 4 次正向变化。故本轮主要行为是初始 `64x64` 向 tracker 持续支持的约 `49..55` 像素尺度单向缓慢收敛，而不是上一轮 `48/49` 高频来回呼吸。
- 与上一轮相同目标复测的非同步对照相比，最终宽度变化率由 `33.5%` 降至 `6.1%`，最终高度由 `24.3%` 降至 `6.6%`，高度方向反转由 `303` 次降至 `1` 次，`48<->49` 往返由 `102` 次降至 `0`。两轮不是逐帧同步 A/B，不能将比例差异解释为精度提升，但本轮日志直接证明时间一致性和输出迟滞已消除绝大多数最终框尺度反转。
- 第 5 次是本轮质量波动最大的区间之一：190 帧中 `Reliable/Weak/Rejected=162/13/15`、accepted 132，出现一次 local recovery；最终高度只出现一次 `64->65` 后降至 `60`，宽度仅两次反转。第 3、9、11 次也有较多 uncertain 帧，但十一组均未 lost。当前日志不支持继续下调 PSR 或 peak-margin 来解决框抖动；尺度抖动主因是原始回归噪声，已由本轮时间滤波显著隔离。

### 证据边界与下一步

- 本节证明准确提交 `c6a216f` 已在既有 A6 环境交叉编译、以隔离候选在真实吊舱上运行，并取得用户人工点选后的逐帧日志。`2026-11-06T21:14:56+08:00`，用户在查看本轮实际画面后明确反馈“是的，明显稳定”，回答的是整体框、特别是高度相对上一轮是否明显稳定；该人工观感与日志中尺度反转和高频呼吸显著减少相互印证，但仍不等同于跟踪精度、长期稳定率或吊舱整体验收通过。
- 共享配置当前仍是本轮测试配置，但板上没有运行中的 `viewlink_app` 或 `app_watchdog`；正式 `/app/bin/viewlink_app` 未被替换。若继续优化，应先由用户评价本轮视觉观感；在没有新的主观问题或标注误差证据前，不建议继续增加平滑或降低质量门。
- 本轮只修改本任务拥有的 `docs/progress/cross-compile.md`，没有修改 `docs/progress/STATUS.md`、`docs/progress/integration-contract.yaml` 或 `docs/progress/board-validation.md`。公共 handoff 状态和“真实吊舱测试只能由人验收”的边界保持不变。

## 2026-11-06 新小目标位置抖动复测与只读诊断

- 时间：`2026-11-06T21:31:45+08:00`；分支/工作树：`codex/cross-compile-main-b827` / `/home/u20/single-object-tracking/.worktrees/cross-compile-main-b827`；测试源码/程序仍为 `c6a216f0d8c11961d24590aba437f6d70b02dc88` / `90672c847d76ee8ae24fa052ec845e21763ce9f88aac90915c0b23e8018a851e`，配置仍为 `dbd412b7df5998fb52b3faa55327754322bb5fcd8a7bdf7c4c0d44127c8871f6`，没有重新编译或修改参数。启动前无 `viewlink_app`/`app_watchdog`，受保护程序仍为 `a60b61a34cdaf0b43d27726a5ce311e9e995a8d34bb874657c944eabd86fb357`。
- 同一隔离候选以 PID `7534` 启动，`/proc/7534/exe` 和 RTSP `554` 监听均核验为该候选。用户说明第 1 次目标是柜门把手，后续目标是所附画面白框圈住的椅子扶手末端，第 8 次目标被遮挡、该次丢失属于预期；用户主观反馈本轮“框的位置抖得比较厉害”。用户提供的参考图为 1920x1440 JPEG、SHA-256 `a24ad95517cb319d5665ae113c90079fdcc30d508ca41c0438ec1d56171c45dc`，显示该目标尺寸小、局部纹理弱，附近有扶手和椅背强边缘。
- 完成后先核验 PID 路径；TERM 等待 5 秒仍未退出，只对 PID `7534` 执行 KILL，并确认无残留进程。冻结 stdout 为 41,059 bytes、SHA-256 `96e46cf3e19efb2e8836e927a2bf73bd9062b06287efc618a050151b0f917938`；stderr 为 1,977,824 bytes、SHA-256 `9abe8f10599b152287489870066ccbabb5d7ffcd8534eed5669ea3319550f714`。板端日志位于 `/app/nanotrack_c6a216f_temporal_scale_20261106/log/{stdout,stderr}.new-target-20261106.log`。冻结包在板端 `/tmp/nanotrack-c6a216f-new-target-20261106.tgz`、WSL `deployment_b827/nanotrack-c6a216f-new-target-20261106.tgz`，大小 322,326 bytes、SHA-256 均为 `352db3b62724eb08e1d266a328b26aee14641e32ead3f789fdf3917b125d8afe`；本地复核副本位于 `/tmp/nanotrack-c6a216f-new-target-analysis/`。
- 日志实际包含 12 次初始化和 2,637 个跟踪帧。第 1 段柜门把手只有 4 帧 accepted，随后进入 local/assisted/lost，lost 84 帧。按用户说明排除第 8 段遮挡；日志第 10 段另有一次独立失败，120 帧中仅 1 帧 accepted、lost 81 帧，frame 2078 起从 Weak 转为 Rejected，frame 2079 原始中心跳到约 `(1538.5,962.5)`、PSR `3.245`、中心残差 `2.028`，随后未恢复。若用户计数与日志初始化一一对应，这不是已声明的第 8 次遮挡，需要后续单独复核当时画面条件。
- 排除第 1、8、10 段后，其余九段小目标共 2,156 帧，`Reliable/Weak/Rejected=2107/24/25`、accepted 1,934；模式为 `TRACKING/UNCERTAIN=1934/222`，没有进入 local recovery 或 lost。大量 Reliable 帧仍处于 uncertain，符合双模板位置不一致触发确认而非质量阈值拒绝。
- 同一程序上一轮目标的最终框中心相邻位移 P50/P95/最大为 `3.61/10.77/23.77` 像素，高频二阶变化 P95 为 `14.55` 像素。本轮排除第 1、8、10 段后的对应位置指标约为 `6.0/20.25/46.62` 像素和 `29.83` 像素，均约翻倍。accepted 且模板选择可唯一判定的帧中，最终中心与被选观测中心差不超过 `0.71` 像素，证明观测位置抖动基本原样进入最终框。
- 两模板中心距离在本轮成功小目标段的 P50/P95/最大为 `7.19/24.52/44.28` 像素，上一轮为 `2.03/8.00/46.48` 像素；模板 IoU 低于当前 `0.30` agreement 门的比例由 `0.14%` 升到 `4.13%`。可识别的连续模板切换 257 次，切换帧中心位移 P95 为 `29.26` 像素；同模板连续帧仍有 `18.10` 像素，说明模板切换会放大抖动，但小目标自身定位峰噪声也是独立来源。accepted 连续帧中心跳变不小于 15 像素共有 276 次，上一轮只有 11 次。
- 尺度稳定修改仍按预期工作：排除第 1、8 段后，最终宽/高变化率为 `9.0%/7.7%`，单帧最大变化均为 1 像素，`48<->49` 往返为 0。因此本轮问题是中心位置而非宽高。源码也与日志一致：`stabilize_accepted_bbox()` 只对宽高做时间处理，中心直接取 `observation`；`accept_observation()` 将该框写入 motion/current，runtime 又把 accepted 最终框提交给 NanoTrack 下一帧使用。

### 诊断结论与边界

- 当前证据支持的根因是：新目标小且纹理/边缘歧义更强，anchor/recent 两模板定位中心明显分歧；现有控制器逐帧按质量/分数选择观测后，对 accepted 中心没有时间滤波或迟滞，所选峰值直接成为最终框并反馈给下一帧，所以用户看到明显位置抖动。PSR/peak-margin 对单张响应图的可靠性判断不能表达双模板中心分歧，继续降低或提高现有质量门都不是位置抖动的直接修复。
- 最小后续方向应保持现有尺度和质量参数不变，先为 accepted 中心增加基于 motion prediction 的单一时间稳定层，并用“交替位置噪声应抑制、真实匀速移动应缓慢跟随、non-accepted/恢复路径不错误推进”的主机失败测试约束；先处理选择后的共同输出可同时覆盖同模板噪声和模板切换放大，避免同时改模板选择和中心滤波而无法归因。未经用户明确确认，本节没有修改代码、配置、阈值或远程状态。
- 这是无位置真值的人工功能诊断，能够证明输出中心高频变化与用户观感一致，不能给出像素精度或断言第 8 次遮挡行为已正确实现。板上当前没有运行中的候选或正式程序。
