# RK3566 开发板验证任务进度

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
