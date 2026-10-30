# 生产交叉编译任务进度

- 任务状态：`candidate_cross_compile_succeeded_handoff_unapproved`
- 最近核对：`2026-10-30T17:55:23+08:00`
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

## 完成条件

- `development_board_validation.latest_handoff.available=true`；
- 本地取得的 commit 和工件哈希与 handoff 完全一致；
- 对该结果执行交叉编译并观察到成功退出；
- 将命令、结果和生产产物路径写入本文件。

达到以上条件只能表述为“最新板端结果已成功交叉编译”，不能表述为“已部署”或“吊舱测试通过”。

## Next action

1. 将本次候选编译记录和任务 commit 交给协调对话，但保持 `latest_handoff.available=false`，直到开发板验证任务发布达到门槛的精确 handoff。
2. handoff 发布后核对 source commit、模型/manifest/labels、配置、runtime 身份、证据路径和裁决。
3. 若批准身份与本次候选输入完全一致，协调对话可决定如何采纳候选编译证据；任何一项不同都必须重新执行 A6 交叉编译，不能沿用本节哈希。
4. 真实吊舱部署和测试仍由人完成；没有明确人工反馈不得声明吊舱验收通过。

## Handoff

- 请协调对话采纳本次候选编译记录：`main@b827c0a6d8916e9c0832c748e30722f6f2fe5563` 已在现有 WSL 环境中生成 AArch64 `viewlink_app`，其路径和 SHA-256 见上文。
- 公共契约当前没有“用户授权的未批准 handoff 候选编译”字段。协调对话如更新公共状态，应明确区分候选编译成功与批准 handoff 编译成功；在 handoff 未发布前保持 `latest_result_access_confirmed=false`，不得仅据本节把 `development_board_validation.latest_handoff.available` 改为 `true`。
- 需要协调对话决定是否增加/记录 candidate compile 状态，并处理 RKNN runtime `1.5.2` 与板端/模型 `2.3.2` 的身份差异；真实吊舱部署和测试仍由人完成。

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
