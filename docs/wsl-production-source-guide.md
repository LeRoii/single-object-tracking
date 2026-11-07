# WSL 生产源码修改指南

本文记录 `2026-11-07` 实际核对到的 WSL 生产工程。它用于帮助你自行定位、修改和编译吊舱版 NanoTrack；路径、哈希和功能说明均来自当前 WSL 文件，不以聊天记录为准。

## 1. 最新源码和产物在哪里

登录 WSL 后进入：

```bash
ssh -p 2222 wjm@100.104.145.88
cd /home/wjm/viewpro_gimbal_zp/rk3566/gimbal_tracker
```

关键目录：

| 用途 | 当前路径 |
| --- | --- |
| 生产算法源码 | `/home/wjm/viewpro_gimbal_zp/rk3566/gimbal_tracker/ai_alg` |
| NanoTrack/长期控制源码 | `ai_alg/nanotrack` |
| 宿主 C 接口适配层 | `ai_alg/source/native-lib_impl.cpp` |
| A6 构建入口 | `run.sh` |
| 当前 A6 程序 | `build_A6/viewlink_app` |
| 当前算法静态库 | `build_A6/ai_alg/libAI.a` |

当前生产核心源码来自跟踪仓库提交 `f5023b38b27911ad2d68d3ade2b1ef07670cdd8a`，但 WSL 工程本身**不是 Git 仓库**。WSL 里的 `native-lib_impl.cpp` 还包含生产适配修改，所以直接改 WSL 前必须先备份。

当前身份：

| 文件 | SHA-256 |
| --- | --- |
| `ai_alg/source/native-lib_impl.cpp` | `6cc6261a9a82d93ef571e790f2793a7d6a2a7258765c52b8478a2acb24bcb79d` |
| `ai_alg/nanotrack/config/visible-default.conf` | `54dcc973feed11ad3e9e7838c8f35e86b0d587d5aa143863a625c849fa5dae19` |
| `ai_alg/nanotrack/source/long_term_controller.cpp` | `b9a1d875f4fae6d386c0c88da805dc338692f04454f2bb7dcf7846af91a57e2a` |
| `ai_alg/nanotrack/source/nanotrack_v3.cpp` | `b6c5da83c280a823111a4116be728ff7e9ac6e673ccffd75116faa4280dcce8c` |
| `build_A6/viewlink_app` | `221c00e32ed0b59f4ea4c9e27b122dd43248cb9d7147ede2823f412dd77725ad` |
| `build_A6/ai_alg/libAI.a` | `d7d8bbb91228babc0efe01b846e4433f53eafad65081e55ab8e0f54ee1205941` |

不要把以下位置当作最新源码或程序：

- `ai_alg/backup-*`、`ai_alg.backup-*`：历史备份，只用于回退和比较。
- `ai_alg/source/native-lib_stub.cpp`：桩实现，当前没有参加 `libAI.a` 编译。
- `deployment_b827/`：混合了多轮历史日志、模型和程序；其中 `bin/viewlink_app` 不是当前最新程序，`config/visible-default.conf` 也缺少最新中心平滑参数。
- `ai_alg/nanotrack/source/main.cpp`、CLI 和调试界面源码：保留在目录中，但当前生产 `ai_alg/CMakeLists.txt` 没有编译它们。

## 2. 各关键功能对应哪个文件

### 对外接口、图像和输出框

| 功能 | 源码位置 | 关键入口 |
| --- | --- | --- |
| 宿主 ABI、结构体、命令枚举 | `ai_alg/include/native-lib.h` | `AI_TRACK_INFO`、`TRACK_RESULT`、`do_tracker_detector()` |
| 模型和运行时初始化 | `ai_alg/source/native-lib_impl.cpp` | `tracker_detector_init()` |
| 每帧跟踪入口 | `ai_alg/source/native-lib_impl.cpp` | `do_tracker_detector()` |
| 输入分辨率检查 | 同上 | `valid_frame_contract()`；当前只接受 A6、可见光、`1920x1080` |
| RGB 转 BGR 和固定步长 | 同上 | `make_bgr_view()`；当前固定 `1920x1080`、RGB 三通道、stride `5760` |
| 输出框写入和诊断日志 | 同上 | `set_frame_outputs()` |
| 初始化帧截图 | `ai_alg/source/init_frame_snapshot.cpp` | `save_init_frame_snapshot()` |

当前输出字段实际含义：

| 字段 | 当前内容 |
| --- | --- |
| `rect[0]` | 真实最终跟踪框，用于宿主控制；原“固定中心框”赋值仍以注释保留 |
| `rect[1]` | 真实最终跟踪框 |
| `rect[2]` | 运动预测框 |
| `rect[3]` | 扩大的搜索区域 |
| `rect[4]` | detector 候选区域，没有候选时为零框 |
| `rect_show` | 真实最终跟踪框 |

修改 `AI_TRACK_INFO`、`RECT_SIZE` 或字段顺序会改变跨模块 ABI。宿主是预编译库，除非同时拿到并修改宿主源码，否则不要改变这些结构体布局。

### 阈值、框平滑和长期跟踪

| 想改的功能 | 首选文件 | 关键位置 |
| --- | --- | --- |
| Reliable/Weak 阈值 | `ai_alg/nanotrack/config/visible-default.conf` | `reliable_*`、`weak_*` |
| 中心位置平滑 | 同上 | `accepted_center_smoothing_alpha`、`accepted_center_max_correction_ratio` |
| 宽高/尺度平滑 | 同上 | `accepted_scale_*` |
| 搜索区域大小 | 同上 | `search_tracking`、`search_uncertain`、`search_local_*`、`search_assisted`、`search_lost` |
| 丢失和恢复时间 | 同上 | `uncertain_ms`、`local_recovery_ms`、`assisted_recovery_ms` |
| 2/3 帧确认 | 同上 | `confirmation_window`、`confirmation_required` |
| 遮挡 coast/HOLD | 同上 | `occlusion_*` |
| detector 恢复 | 同上 | `detector_*` |
| 配置字段定义、读取和合法性检查 | `ai_alg/nanotrack/include/controller_config.hpp`、`ai_alg/nanotrack/source/controller_config.cpp` | `ControllerConfig`、`load_controller_config()`、`validate_controller_config()` |
| 质量分类 | `ai_alg/nanotrack/source/long_term_controller.cpp` | `evaluate()` |
| 搜索尺度状态机 | 同上 | `search_scale_for()`、`begin_frame()` |
| 尺度稳定 | 同上 | `stabilize_accepted_bbox()` |
| 中心稳定 | 同上 | `stabilize_accepted_center()` |
| 接受框、更新运动和模板 | 同上 | `accept_observation()`、`finish_frame()` |
| 运动预测 | `ai_alg/nanotrack/source/motion_estimator.cpp` | `MotionEstimator` |
| 遮挡证据 | `ai_alg/nanotrack/source/occlusion_evidence.cpp` | `OcclusionEvidence` |

若只是修改**已有配置项的值**，源码侧只需改 `ai_alg/nanotrack/config/visible-default.conf`，但实机运行前仍要把它复制到板端实际配置路径。若要新增配置项，需要同时修改：

1. `ai_alg/nanotrack/include/controller_config.hpp` 中的字段；
2. `ai_alg/nanotrack/source/controller_config.cpp` 中的解析、默认值、校验和配置指纹；
3. `ai_alg/nanotrack/config/visible-default.conf`；
4. 实际部署到板端的运行配置。

### NanoTrack 网络推理和原始框

| 功能 | 源码位置 | 关键入口 |
| --- | --- | --- |
| 三个 RKNN 模型加载 | `ai_alg/nanotrack/source/nanotrack_v3.cpp` | `NanoTrackV3::NanoTrackV3()` |
| 模板裁剪 | 同上 | `extract_template_feature()`；输出 `127x127` |
| 搜索裁剪和搜索扩大 | 同上 | `NanoTrackV3::infer()`；输出 `255x255`，乘 `plan.search_scale` |
| 分类、回归解码和 PSR/APCE | `ai_alg/nanotrack/source/tracker_math.cpp` | `try_decode_head()` |
| 网络固有参数 | `ai_alg/nanotrack/source/nanotrack_v3.cpp` | `penalty_k`、`window_influence`、`learning_rate` |
| 原始框相对初始框的尺寸限制 | `ai_alg/nanotrack/source/nanotrack_v3.cpp`、`ai_alg/nanotrack/source/tracker_math.cpp` | `constrain_bbox_dimensions()` |
| 最终接受框回写给下一帧 | `ai_alg/nanotrack/source/nanotrack_v3.cpp` | `commit_bbox()` |
| RKNN 运行时封装 | `ai_alg/nanotrack/source/rknn_model.cpp` | `RknnModel` |

### detector 和目标类别

| 功能 | 源码位置 |
| --- | --- |
| detector RKNN 加载、预处理和输出解释 | `ai_alg/nanotrack/source/yolo_rknn_engine.cpp` |
| 异步请求、结果轮询和旧请求隔离 | `ai_alg/nanotrack/source/yolo_detector_adapter.cpp` |
| YOLOv8 后处理/NMS | `ai_alg/nanotrack/source/yolov8_postprocess.cpp` |
| 初始化目标类别匹配 | `ai_alg/nanotrack/source/target_class.cpp` |
| detector 默认置信度 `0.25` 和 NMS `0.45` | `ai_alg/source/native-lib_impl.cpp` 的 `tracker_detector_init()` |

detector 只能提供恢复候选；最终仍需 NanoTrack 验证后才允许 `control_valid=true`，不要把 detector 框直接写到 `rect[0]`。

### 配置和模型路径

`ai_alg/source/native-lib_impl.cpp` 中的 `kDeploymentConfig` 当前固定为：

```text
/app/nanotrack_b827_candidate/config/production-deployment.conf
```

这个板端文件再指定 NanoTrack 三模型目录、可见光控制配置、detector 模型、manifest 和 labels。WSL 中对应的打包参考文件是：

```text
deployment_b827/config/production-deployment.conf
```

注意：配置文件不会嵌入二进制。只改 WSL 的 `visible-default.conf` 并重新编译，并不会自动改变板端正在读取的配置；还必须复制正确配置到板端 `production-deployment.conf` 指定的位置，并重新启动程序使其重新加载。

### 宿主和伺服控制边界

顶层 `src/main_pro.cpp` 只有 `start_stream()`。`do_tracker_detector()` 的宿主调用、伺服误差计算和 yaw/pitch 命令位于 `A6_libs/libviewlink.a` 等预编译库中，这个 WSL 工程没有对应源码。

因此本工程可以修改算法输出的 `rect[0]`，但不能直接修改宿主如何把框换算成伺服速度。如果以后要改死区、PID、零速度清除或 yaw/pitch 命令，必须先取得宿主库源码。

## 3. 自己修改和编译的最短流程

先备份准备修改的文件，例如：

```bash
cd /home/wjm/viewpro_gimbal_zp/rk3566/gimbal_tracker
cp -a ai_alg/source/native-lib_impl.cpp \
  ai_alg/source/native-lib_impl.cpp.backup-before-my-change
cp -a ai_alg/nanotrack/config/visible-default.conf \
  ai_alg/nanotrack/config/visible-default.conf.backup-before-my-change
```

修改后执行 A6 交叉编译：

```bash
cd /home/wjm/viewpro_gimbal_zp/rk3566/gimbal_tracker
BUILD_JOBS=4 ./run.sh --platform A6
sha256sum build_A6/viewlink_app build_A6/ai_alg/libAI.a
```

成功产物是：

```text
build_A6/viewlink_app
build_A6/ai_alg/libAI.a
```

建议每次记录：修改前后文件哈希、编译日志、程序哈希和实际使用的配置哈希。不要只靠文件时间判断版本。

如果修改的是核心算法，最好把同样的修改同步到有 Git 和主机测试的仓库源码：

```text
/home/u20/single-object-tracking/.worktrees/cross-compile-main-b827/rk3566/nanotrack_v3
```

先在 Git 工作树运行回归测试并提交，再把精确提交中的生产文件同步到 WSL。WSL 当前只有生产构建链，不能替代完整的主机单元测试。

## 4. 部署和实机测试注意事项

- 不要直接覆盖板端 `/app/bin/viewlink_app`；先放到新的独立候选目录。
- 上传后在 WSL 和板端分别执行 `sha256sum`，两边必须一致。
- 启动前确认候选读取的是预期模型和配置，而不是历史 `/app` 目录。
- `rect[0]` 当前是真实目标框，程序启动后可能立即驱动吊舱。必须先确认吊舱周围无人、无遮挡且可以自由转动。
- 编译成功只证明生产构建链可用，不等于模型加载、跟踪效果或吊舱验收通过。

当前真实 `rect[0]` 候选已隔离部署在：

```text
/app/nanotrack_f5023b3_real_rect0_20261107/bin/viewlink_app
```

该候选 SHA-256 为 `221c00e32ed0b59f4ea4c9e27b122dd43248cb9d7147ede2823f412dd77725ad`。截至本文核对时它尚未启动，正式 `/app/bin/viewlink_app` 也未被覆盖。
