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
